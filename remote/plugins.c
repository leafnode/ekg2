/* $Id: plugins.c 4551 2008-08-29 19:17:22Z wiechu $ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz (deli@rzepaknet.us>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <dlfcn.h>

#include "commands.h"
#include "debug.h"
#include "dynstuff.h"
#include "plugins.h"
#include "stuff.h"
#include "vars.h"
#include "themes.h"
#include "xmalloc.h"
#include "dynstuff_inline.h"

#include "queries.h"

#if !defined(va_copy) && defined(__va_copy)
#define va_copy(DST,SRC) __va_copy(DST,SRC)
#endif

static LIST_ADD_COMPARE(plugin_register_compare, plugin_t *) { return data2->prio - data1->prio; }

DYNSTUFF_LIST_DECLARE_SORTED_NF(plugins, plugin_t, plugin_register_compare,
	static __DYNSTUFF_LIST_ADD_SORTED,		/* plugins_add() */
	__DYNSTUFF_NOUNLINK)

EXPORTNOT list_t watches = NULL;
static query_t *queries[QUERY_EXTERNAL+1];

EXPORTNOT plugin_t *ui_plugin = NULL;
plugin_t *plugins = NULL;

static int ekg2_dlclose(void *plugin) {
	return dlclose(plugin);
}

static void *ekg2_dlopen(const char *name) {
	void *tmp = dlopen(name, RTLD_NOW);

	if (!tmp) {
		debug_warn("[plugin] could not be loaded: %s %s\n", name, dlerror());
	} else {
		debug_ok("[plugin] loaded: %s\n", name);
	}
	return tmp;
}

static void *ekg2_dlsym(void *plugin, char *name) {
	void *tmp;
	const char *error;

	dlerror();			/* Clear any existing error */
	tmp = dlsym(plugin, name);	/* Look for symbol */

	/* Be POSIX like, if dlerror() returns smth, even if dlsym() successful return pointer. Then report error.
	 * man 3 dlsym */
	if ((error = dlerror())) {
		debug_error("[plugin] plugin: %x symbol: %s error: %s\n", plugin, name, error);
		return NULL;
	}

	return tmp;
}

EXPORTNOT void plugin_load(const char *name)
{
#ifdef SHARED_LIBS
	char *lib = NULL;
	char *env_ekg_plugins_path = NULL;
	char *init = NULL;
#endif

	void *plugin = NULL;
	int (*plugin_init)() = NULL;

	if (ui_plugin) {
		debug_error("plugin_load() but already ui plugin!\n");
		return;
	}

#ifdef SHARED_LIBS
	if ((env_ekg_plugins_path = getenv("EKG_PLUGINS_PATH"))) {
                lib = saprintf("%s/%s.so", env_ekg_plugins_path, name);
		plugin = ekg2_dlopen(lib);

                if (!plugin) {
                        xfree(lib);
                        lib = saprintf("%s/%s/.libs/%s.so", env_ekg_plugins_path, name, name);
			plugin = ekg2_dlopen(lib);
		}
        }

	/* Note: usunalem testowanie, plugins/%s/.libs/%s.so
	 * 	                oraz, ../plugins/%s/.libs/%s.so
	 *
	 * Jak nie znajdziemy pluga, to trzeba powiedziec userowi zeby ustawil EKG_PLUGINS_PATH na katalog instalacyjny ekg2/plugins
	 */

	if (!plugin) {
		xfree(lib);
		lib = saprintf("%s/%s.so", PLUGINDIR, name);
		plugin = ekg2_dlopen(lib);
	}

	if (!plugin)
		return;
#endif

#ifdef STATIC_LIBS
/* first let's try to load static plugin... */
	// extern int readline_plugin_init(int prio);
	// extern int ncurses_plugin_init(int prio);
	extern int gtk_plugin_init(int prio);

	debug("searching for name: %s in STATICLIBS: %s\n", name, STATIC_LIBS);

	if (!strcmp(name, "gtk")) 
		plugin_init = &gtk_plugin_init;
#endif

#ifdef SHARED_LIBS
	if (!plugin_init) {
/* than if we don't have static plugin... let's try to load it dynamicly */
		init = saprintf("%s_plugin_init", name);

		if (!(plugin_init = ekg2_dlsym(plugin, init))) {
			dlclose(plugin);
			xfree(init);
			return;
		}
		xfree(init);
	}
#endif
	if (!plugin_init)
		return;

	if (plugin_init(0) == -1) {		/* prio:

						   -254 is default one,
						      0 is default one for ui-plugins
						  */
		debug_error("plugin_load() plugin_init() failed.\n");
		dlclose(plugin);
		return;
	}

	if (!ui_plugin) {
		debug_error("plugin_load() plugin_find(%s) not found.\n", name);
		/* It's FATAL */
		return;
	}

	ui_plugin->__dl = plugin;
	if (ui_plugin->theme_init)
		ui_plugin->theme_init();

	query_emit_id(ui_plugin, SET_VARS_DEFAULT);
}

EXPORTNOT plugin_t *remote_plugin_load(const char *name, int prio) {
	plugin_t *pl = xmalloc(sizeof(plugin_t));

	pl->name = xstrdup(name);
	pl->pclass = PLUGIN_ANY;		/* XXX */
	pl->prio = prio;

	/* p->destroy() mial czyscic, zeby nie bylo memleaka   
	   HEHE, tylko p->destroy() nie dostaje parametru ktory plugin...   
	   dlatego w remote_plugins_destroy() jest taki kod jaki jest. */

	plugins_add(pl);
	return pl;
}

EXPORTNOT void remote_plugins_destroy() {
	plugin_t *p;

	for (p = plugins; p; ) {
		plugin_t *next = p->next;

		if (p->destroy != NULL) {
			debug_error("remote_plugins_destroy() but p->destroy != NULL\n");
			p->destroy();
		} else {
			int i;

			/* generic unloader, for remote plugins */
			xfree(p->name);

			if (p->params) {
				for (i = 0; p->params[i].key; i++)
					xfree(p->params[i].key);
				xfree(p->params);
			}

			xfree(p);
		}
		p = next;
	}
}

plugin_t *plugin_find(const char *name)
{
	plugin_t *p;

	for (p = plugins; p; p = p->next) {
		if (!xstrcmp(p->name, name))
			return p;
	}

	return NULL;
}

int plugin_register(plugin_t *p, int prio) {
	if (ui_plugin) {
		debug_error("plugin_register() but already ui plugin!\n");
		return 1;
	}

	if (p->pclass != PLUGIN_UI)
		debug_error("plugin_register() but not ui plugin!\n");

	p->prio = prio;
	ui_plugin = p;

	return 0;
}

EXPORTNOT void plugin_unload(plugin_t *p) {
	struct timer *t;
	query_t **ll;
	variable_t *v;
	command_t *c;
	list_t l;

	p->destroy();

	if (p == ui_plugin)
		ui_plugin = NULL;
	else
		debug_error("plugin_unload() but no ui_plugin!\n");

	/* plugin_unregister() */

	/* XXX think about sequence of unloading....: currently: watches, timers, sessions, queries, variables, commands */

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->plugin == p)
			watch_free(w);
	}

	for (t = timers; t; t = t->next) {
		if (t->plugin == p)
			t = timers_removei(t);
	}

	for (ll = queries; ll <= &queries[QUERY_EXTERNAL]; ll++) {
		query_t *q;

		for (q = *ll; q; ) {
			query_t *next = q->next;

			if (q->plugin == p)
				LIST_REMOVE2(&queries[q->id >= QUERY_EXTERNAL ? QUERY_EXTERNAL : q->id], q, NULL);

			q = next;
		}
	}

	for (v = variables; v; v = v->next) {
		if (v->plugin == p) 
			v = variables_removei(v);
	}

	for (c = commands; c; c = c->next) {
		if (c->__plugin == p)
			c = commands_removei(c);
	}

	if (p->__dl)
		ekg2_dlclose(p->__dl);
}

int plugin_unregister(plugin_t *p) {
	if (p != ui_plugin) {
		debug_error("plugin_unregister() but p != plugin_ui\n");
		return -1;
	}
	/* it does nothing */

	return 0;
}

query_t *query_connect_id(plugin_t *plugin, const int id, query_handler_func_t *handler, void *data) {
	query_t *q = xmalloc(sizeof(query_t));

	q->id		= id;
	q->plugin	= plugin;
	q->handler	= handler;
	q->data		= data;

	return LIST_ADD2(&queries[id >= QUERY_EXTERNAL ? QUERY_EXTERNAL : id], q);
}

EXPORTNOT void queries_destroy() {
	query_t **ll;

	for (ll = queries; ll <= &queries[QUERY_EXTERNAL]; ll++) {
		LIST_DESTROY2(*ll, NULL);
		*ll = NULL;
	}
}

int query_emit_id(plugin_t *plugin, const int id, ...) {
	static int nested = 0;

	int result = -2;
	va_list ap;
	query_t *q;

	if (id >= QUERY_EXTERNAL) {
		debug_error("query_emit_id() shouldn't happen!\n");	/* XXX */
		return -2;
	}

	if (nested >= 32) {
		/*
		   if (nested == 33)
			   debug("too many nested queries. exiting to avoid deadlock\n");
	 	  */
		return -1;
	}

	nested++;

	va_start(ap, id);
	for (q = queries[id]; q; q = q->next) {
		if ((!plugin || (plugin == q->plugin))) {
			int (*handler)(void *data, va_list ap) = q->handler;
			va_list ap_plugin;
			int bresult;

			/*
			 * pc and amd64: va_arg remove var from va_list when you use va_arg, 
			 * so we must keep orig va_list for next plugins
			 */
			va_copy(ap_plugin, ap);
			bresult = handler(q->data, ap_plugin);
			va_end(ap_plugin);

			if (bresult == -1) {
				result = -1;
				break;
			} else
				result = 0;
		}
	}
	va_end(ap);

	nested--;
	return result;
}

static watch_t *watch_find(plugin_t *plugin, int fd, watch_type_t type) {
	list_t l;
	
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->plugin == plugin && w->fd == fd && (w->type == type) && !(w->removed > 0))
			return w;
	}

	return NULL;
}

static LIST_FREE_ITEM(watch_free_data, watch_t *) {
	data->removed = 2;	/* to avoid situation: when handler of watch, execute watch_free() on this watch... stupid */

	if (data->buf) {
		int (*handler)(int, int, const char *, void *) = data->handler;
		string_free(data->buf, 1);
		/* DO WE WANT TO SEND ALL  IN BUFOR TO FD ? IF IT'S WATCH_WRITE_LINE? or parse all data if it's WATCH_READ_LINE? mmh. XXX */
		if (handler)
			handler(1, data->fd, NULL, data->data);
	} else {
		int (*handler)(int, int, int, void *) = data->handler;
		if (handler)
			handler(1, data->fd, data->type, data->data);
	}
}

EXPORTNOT void watch_free(watch_t *w) {
	if (!w)
		return;

	if (w->removed == 2)
		return;

	if (w->removed == -1 || w->removed == 1) { /* watch is running.. we cannot remove it */
		w->removed = 1;
		return;
	}

	if (w->type == WATCH_WRITE && w->buf && !w->handler && w->plugin) {	/* XXX */
		debug_error("[INTERNAL_DEBUG] WATCH_LINE_WRITE must be removed by plugin, manually (settype to WATCH_NONE and than call watch_free()\n");
		return;
	}

	watch_free_data(w);
	list_remove_safe(&watches, w);

	ekg_watches_removed++;
	debug("watch_free() REMOVED WATCH, watches removed this loop: %d oldwatch: 0x%x\n", ekg_watches_removed, w);
}

EXPORTNOT void watch_handle(watch_t *w) {
	int res = 0;

	if (w->buf && w->type == WATCH_WRITE && (w->buf->len == 0 || w->transfer_limit == -1))	/* transfer limit turned on, don't send anythink... */
		return;

	w->removed = -1;

	if (w->buf) {
		int (*handler)(int, int, const char *, void *) = w->handler;

		if (w->type == WATCH_READ) {
			char buf[1024], *tmp;
			int ret;

			ret = read(w->fd, buf, sizeof(buf));

			/* debug_io("[watch_handle_read] fd: %d read: %d\n", w->fd, ret); */

			if (ret > 0)
				string_append_raw(w->buf, buf, ret);

			if (ret == 0 || (ret == -1 && errno != EAGAIN))
				string_append_c(w->buf, '\n');

			while ((tmp = strchr(w->buf->str, '\n'))) {
				size_t strlen = tmp - w->buf->str;		/* get len of str from begining to \n char */
				char *line = xstrndup(w->buf->str, strlen);	/* strndup() str with len == strlen */

				/* we strndup() str with len == strlen, so we don't need to call xstrlen() */
				if (strlen > 1 && line[strlen - 1] == '\r')
					line[strlen - 1] = 0;

				res = handler(0, w->fd, line, w->data);
				xfree(line);

				if (res == -1)
					break;

				string_remove(w->buf, strlen + 1);
			}

			/* je�li koniec strumienia, lub nie jest to ci�g�e przegl�danie,
			 * zwolnij pami�� i usu� z listy */
			if (ret == 0 || (ret == -1 && errno != EAGAIN))
				res = -1;

		} else if (w->type == WATCH_WRITE) {
			int res = -1;
			int len = w->buf->len;

			debug_io("[watch_handle_write] fd: %d in queue: %d bytes.... ", w->fd, len);

			if (handler) {
				res = handler(0, w->fd, w->buf->str, w->data);
			} else {
				res = write(w->fd, w->buf->str, len);

				if (res == -1 && errno == EAGAIN)
					res = 0;
			}

			debug_io(" ... wrote:%d bytes (handler: 0x%x) ", res, handler);

			if (res == -1) {
				debug("Error: %s %d\n", strerror(errno), errno);
				goto cleanup;
			}

			if (res > len) {
				debug_error("watch_write(): handler returned bad value, 0x%x vs 0x%x\n", res, len);
				res = len;
			} else if (res < 0) {
				debug_error("watch_write(): handler returned negative value other than -1.. XXX\n");
				res = 0;
			}

			string_remove(w->buf, res);
			debug_io("left: %d bytes\n", w->buf->len);
		}
	} else {
		int (*handler)(int, int, int, void *) = w->handler;

		res = handler(0, w->fd, w->type, w->data);
	}

cleanup:
	if (res == -1 || w->removed == 1) {
		w->removed = 0;
		watch_free(w);
		return;
	} 

	w->removed = 0;
}

EXPORTNOT int watch_write(watch_t *w, const char *buf, int len) {
	int was_empty;

	if (!buf || len <= 0 || w->type != WATCH_WRITE || !w->buf)
		return -1;

	was_empty = !w->buf->len;
	string_append_raw(w->buf, buf, len);

	if (was_empty) 
		watch_handle(w); /* let's try to write somethink ? */
	return 0;
}

EXPORTNOT void watches_destroy() {
	list_t l;

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		watch_free(w);
	}
	list_destroy(watches); watches = NULL;
}

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, watcher_handler_func_t *handler, void *data) {
	watch_t *w	= xmalloc(sizeof(watch_t));
	w->plugin	= plugin;
	w->fd		= fd;
	w->type		= type;

	if (w->type == WATCH_READ_LINE) {
		w->type = WATCH_READ;
		w->buf = string_init(NULL);
	} else if (w->type == WATCH_WRITE_LINE) {
		w->type = WATCH_WRITE;
		w->buf = string_init(NULL);
	}
	
	w->handler = handler;
	w->data    = data;

	list_add_beginning(&watches, w);
	return w;
}

int watch_remove(plugin_t *plugin, int fd, watch_type_t type) {
	int res = -1;
	watch_t *w;
/* XXX, here can be deadlock feel warned. */
	while ((w = watch_find(plugin, fd, type))) {
		watch_free(w);
		res = 0;
	}

	return res;
}

int plugin_abi_version(int plugin_abi_ver, const char * plugin_name) {
	if (plugin_abi_ver == 4729)
		return 1;

	if (EKG_ABI_VER == plugin_abi_ver)
		return 1;

	debug_error("ABI versions mismatch.  %s_plugin ABI ver. %d,  core ABI ver. %d\n", plugin_name, plugin_abi_ver, EKG_ABI_VER);
	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
