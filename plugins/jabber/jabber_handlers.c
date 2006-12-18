/*
 *  (C) Copyright 2003-2006 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Tomasz Torcz <zdzichu@irc.pl>
 *                          Leszek Krupi�ski <leafnode@pld-linux.org>
 *                          Piotr Paw�ow and other libtlen developers (http://libtlen.sourceforge.net/index.php?theme=teary&page=authors)
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

#include "ekg2-config.h"
#include <ekg/win32.h>

#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/utsname.h> /* dla jabber:iq:version */
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#ifndef NO_POSIX_SYSTEM
#include <netdb.h>
#endif

#ifdef HAVE_EXPAT_H
#  include <expat.h>
#endif

#ifdef __sun      /* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif
#include <time.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include <ekg/queries.h>

#include "jabber.h"
#include "jabber_dcc.h"

#define jabberfix(x,a) ((x) ? x : a)
#define STRICT_XMLNS 1

static void jabber_handle_message(xmlnode_t *n, session_t *s, jabber_private_t *j);
static void jabber_handle_presence(xmlnode_t *n, session_t *s);
static void jabber_handle_iq(xmlnode_t *n, jabber_handler_data_t *jdh);
static time_t jabber_try_xdelay(const char *stamp);
static void jabber_session_connected(session_t *s, jabber_handler_data_t *jdh);
int jabber_display_server_features = 1;

static void newmail_common(session_t *s); 
#ifdef JABBER_HAVE_SSL

static char *jabber_ssl_cert_verify(const SSL_SESSION ssl) {
#ifdef JABBER_HAVE_OPENSSL
	X509 *peer_cert = SSL_get_peer_certificate(ssl);
	long ret;

	if (!peer_cert) return _("No peer certificate");

	switch ((ret = SSL_get_verify_result(ssl))) {
		/* 
		 * copied from qssl.cpp - Qt OpenSSL plugin Copyright (C) 2001, 2002  Justin Karneges under LGPL 2.1 
		 */
		case X509_V_OK: 					return NULL;
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT: 		return _("Unable to get issuer certificate");
		case X509_V_ERR_UNABLE_TO_GET_CRL:			return _("Unable to get certificate CRL");
		case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:	return _("Unable to decrypt certificate's signature");
		case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:	return _("Unable to decrypt CRL's signature");
		case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:	return _("Unable to decode issuer public key");
		case X509_V_ERR_CERT_SIGNATURE_FAILURE:			return _("Invalid certificate signature");
		case X509_V_ERR_CRL_SIGNATURE_FAILURE:			return _("Invalid CRL signature");
		case X509_V_ERR_CERT_NOT_YET_VALID:			return _("Certificate not yet valid");
		case X509_V_ERR_CERT_HAS_EXPIRED:			return _("Certificate has expired");
		case X509_V_ERR_CRL_NOT_YET_VALID:			return _("CRL not yet valid");
		case X509_V_ERR_CRL_HAS_EXPIRED:			return _("CRL has expired");
		case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:		return _("Invalid time in certifiate's notBefore field");
		case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:		return _("Invalid time in certificate's notAfter field");
		case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:		return _("Invalid time in CRL's lastUpdate field");
		case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:		return _("Invalid time in CRL's nextUpdate field");
		case X509_V_ERR_OUT_OF_MEM:				return _("Out of memory while checking the certificate chain");
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:		return _("Certificate is self-signed but isn't found in the list of trusted certificates");
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:		return _("Certificate chain ends in a self-signed cert that isn't found in the list of trusted certificates");
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:	return _("Unable to get issuer certificate locally");
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:	return _("Certificate chain contains only one certificate and it's not self-signed");
		case X509_V_ERR_CERT_CHAIN_TOO_LONG:			return _("Certificate chain too long");
		case X509_V_ERR_CERT_REVOKED:				return _("Certificate is revoked");
		case X509_V_ERR_INVALID_CA:				return _("Invalid CA certificate");
		case X509_V_ERR_PATH_LENGTH_EXCEEDED:			return _("Maximum certificate chain length exceeded");
		case X509_V_ERR_INVALID_PURPOSE:			return _("Invalid certificate purpose");
		case X509_V_ERR_CERT_UNTRUSTED:				return _("Certificate not trusted for the required purpose");
		case X509_V_ERR_CERT_REJECTED:				return _("Root CA is marked to reject the specified purpose");
		case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:		return _("Subject issuer mismatch");
		case X509_V_ERR_AKID_SKID_MISMATCH:			return _("Subject Key Identifier doesn't match the Authority Key Identifier");
		case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:		return _("Subject Key Identifier serial number doesn't match the Authority's");
		case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:			return _("Key Usage doesn't include certificate signing");
		default:	debug_error("[jabber] SSL_get_verify_result() unknown retcode: %d\n", ret);
				return "Unknown/Generic SSL_get_verify_result() result";
	}
	return NULL;	/* never here */
#else
	static char buf[100];
	int res;
	unsigned int ret;

	if ((res = gnutls_certificate_verify_peers2(ssl, &ret)) != 0)
		return gnutls_strerror(res);

	buf[0] = 0;
		/* ret is bitmask of gnutls_certificate_status_t */
	if (ret & GNUTLS_CERT_INVALID) 		xstrcat(buf, "Certificate is invalid:");	/* 23b */
	if (ret & GNUTLS_CERT_REVOKED) 		xstrcat(buf, " revoked");			/* 08b */
	if (ret & GNUTLS_CERT_SIGNER_NOT_FOUND)	xstrcat(buf, " signer not found");		/* 17b */
	if (ret & GNUTLS_CERT_SIGNER_NOT_CA)	xstrcat(buf, " signer not a CA");		/* 16b */
/*	if (ret & GNUTLS_CERT_INSECURE_ALGORITHM) xstrcat(buf, " INSECURE ALGO?"); */

	return (buf[0]) ? &buf[0] : NULL;
#endif

/* XXX czy sie dane zgadzaja z j->server */
}

static WATCHER(jabber_handle_connect_tls) /* tymczasowy */
{
        session_t *s = (session_t *) data;
        jabber_private_t *j = session_private_get(s);

	int ret;
	char *certret;

	if (type)
		return 0;

	ret = SSL_HELLO(j->ssl_session);

#ifdef JABBER_HAVE_OPENSSL
	if (ret == -1) {
		ret = SSL_get_error(j->ssl_session, ret);
#else
	{
#endif
		if (SSL_E_AGAIN(ret)) {
			int newfd, direc;
#ifdef JABBER_HAVE_OPENSSL
			direc = (ret != SSL_ERROR_WANT_READ) ? WATCH_WRITE : WATCH_READ;
			newfd = fd;
#else
			direc = gnutls_record_get_direction(j->ssl_session) ? WATCH_WRITE : WATCH_READ;
			newfd = (int) gnutls_transport_get_ptr(j->ssl_session);
#endif
			/* don't create && destroy watch if data is the same... */
			if (newfd == fd && direc == watch) {
				ekg_yield_cpu();
				return 0;
			}

			watch_add(&jabber_plugin, fd, direc, jabber_handle_connect_tls, s);
			ekg_yield_cpu();
			return -1;
		} else {
#ifdef JABBER_HAVE_GNUTLS
			if (ret >= 0) goto gnutls_ok;	/* gnutls was ok */

			SSL_DEINIT(j->ssl_session);
			gnutls_certificate_free_credentials(j->xcred);
			j->using_ssl = 0;	/* XXX, hack, peres has reported that here j->using_ssl can be 1 (how possible?) hack to avoid double free */
#endif
			j->parser = NULL; jabber_handle_disconnect(s, SSL_ERROR(ret), EKG_DISCONNECT_FAILURE);
			return -1;
		}
	}
gnutls_ok:
	
	if ((certret = jabber_ssl_cert_verify(j->ssl_session))) {
		debug_error("[jabber] jabber_ssl_cert_verify() %s retcode = %s\n", s->uid, certret);
		print("generic2", certret);
	}

	// handshake successful
	j->using_ssl = 2;

/* XXX, recv_watch */
/* send_watch */
	j->send_watch->handler	= jabber_handle_write;
	j->send_watch->type	= WATCH_WRITE;

/* reset parser */
	j->parser = jabber_parser_recreate(NULL, XML_GetUserData(j->parser));

/* reinitialize stream */
	watch_write(j->send_watch, 
			"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" version=\"1.0\">", 
			j->server);
	return -1;
}
#endif

static void tlen_handle(xmlnode_t *n, session_t *s) {
	if (!xstrcmp(n->name, "m")) {
		char *type = jabber_attr(n->atts, "tp");
		char *from = jabber_attr(n->atts, "f");
		char *typeadd = jabber_attr(n->atts, "type");

		char *uid = saprintf("tlen:%s", from);

		if (!type || !from || (typeadd && !xstrcmp(typeadd, "error"))) {
			debug_error("tlen_handle() %d %s/%s/%s", __LINE__, type, from, typeadd);

		} else if (!xstrcmp(type, "t") || !xstrcmp(type, "u")) {
			/* typing notification */
			char *session	= xstrdup(session_uid_get(s));
			int stateo	= !xstrcmp(type, "u") ? EKG_XSTATE_TYPING : 0;
			int state	= stateo ? EKG_XSTATE_TYPING : 0;
			
			query_emit_id(NULL, PROTOCOL_XSTATE, &session, &uid, &state, &stateo);
			
			xfree(session);

		} else if (!xstrcmp(type, "a")) {	/* funny thing called alert */
			print_window(uid, s, 0, "tlen_alert", session_name(s), format_user(s, uid));

			if (config_sound_notify_file)
				play_sound(config_sound_notify_file);
			else if (config_beep && config_beep_notify)
				query_emit_id(NULL, UI_BEEP, NULL);
		}
		
		xfree(uid);
	} else if (!xstrcmp(n->name, "n")) {	/* new mail */
		char *from = tlen_decode(jabber_attr(n->atts, "f"));
		char *subj = tlen_decode(jabber_attr(n->atts, "s"));
		
		print("tlen_mail", session_name(s), from, subj);
		newmail_common(s);
		
		xfree(from);
		xfree(subj);

	} else if (!xstrcmp(n->name, "w")) {	/* web message */
		char *from = jabber_attr(n->atts, "f");
		char *mail = jabber_attr(n->atts, "e");
		char *content = n->data;
		string_t body = string_init("");
		
		char *me	= xstrdup(session_uid_get(s));
		char *uid	= xstrdup("webmsg.tlen.pl");
		int class 	= EKG_MSGCLASS_MESSAGE;
		int ekgbeep 	= EKG_TRY_BEEP;
		int secure	= 0;
		char **rcpts 	= NULL;
		char *seq 	= NULL;
		uint32_t *format= NULL;
		time_t sent	= time(NULL);
		char *text;
		
		if (from || mail) {
			string_append(body, "From:");
			if (from) {
				string_append(body, " ");
				string_append(body, from);
			}
			if (mail) {
				string_append(body, " <");
				string_append(body, mail);
				string_append(body, ">");
			}
			string_append(body, "\n");
		}

		if (body->len) string_append(body, "\n");

		string_append(body, content);
		text = tlen_decode(body->str);
		string_free(body, 1);
		
		query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &uid, &rcpts, &text, &format, &sent, &class, &seq, &ekgbeep, &secure);
		
		xfree(me);
		xfree(uid);
		xfree(text);
	} else debug_error("[tlen] what's that: %s ?\n", n->name);
}

void jabber_handle(void *data, xmlnode_t *n)
{
#define CHECK_CONNECT(connecting_, connected_, func) \
	if (j->connecting != connecting_ || s->connected != connected_) {\
		debug_error("[jabber] %s:%d ASSERT_CONNECT j->connecting: %d (shouldbe: %d) s->connected: %d (shouldbe: %d)\n", __FILE__, __LINE__, j->connecting, connecting_, s->connected, connected_);\
		func;\
	}
#if STRICT_XMLNS
#define CHECK_XMLNS(n, xmlns, func) \
	if (xstrcmp(jabber_attr(n->atts, "xmlns"), xmlns)) {\
		debug_error("[jabber] %s:%d ASSERT_XMLNS BAD XMLNS, IS: %s SHOULDBE: %s\n", __FILE__, __LINE__, jabber_attr(n->atts, "xmlns"), xmlns);\
		func;\
	}
#else
#define CHECK_XMLNS(n, xmlns, func)
#endif
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        session_t *s = jdh->session;
        jabber_private_t *j;

        if (!s || !(j = jabber_private(s)) || !n) {
                debug_error("[jabber] jabber_handle() invalid parameters\n");
                return;
        }

        debug_function("[jabber] jabber_handle() <%s>\n", n->name);

        if (!xstrcmp(n->name, "message")) {
		jabber_handle_message(n, s, j);
	} else if (!xstrcmp(n->name, "iq")) {
		jabber_handle_iq(n, jdh);
	} else if (!xstrcmp(n->name, "presence")) {
		jabber_handle_presence(n, s);
	} else if (!xstrcmp(n->name, "stream:features")) {
		int use_sasl = j->connecting == 1 && (session_int_get(s, "use_sasl") == 1);
		int use_fjuczers = 0;	/* bitmaska (& 1 -> session) (& 2 -> bind) */
		xmlnode_t *mech_node = NULL;

		if ((session_int_get(s, "display_server_features") == 1 && jabber_display_server_features == 1) || session_int_get(s, "display_server_features") == 2) {
			if (session_int_get(s, "display_server_features") == 1)
				jabber_display_server_features = 0;

			xmlnode_t *ch;

			print("xmpp_feature_header", session_name(s), j->server, "");

			for (ch = n->children; ch; ch = ch->next) {
				xmlnode_t *another;
#define show_feature(excepted, n, what) \
	if (!xstrcmp(excepted, n->name)) { print("xmpp_feature", session_name(s), j->server, excepted, jabber_attr(n->atts, "xmlns"), what);

#define show_subfeature(excepted, name, n, what) \
	if (!xstrcmp(excepted, n->data)) print("xmpp_feature_sub", session_name(s), j->server, name, excepted, what)

				show_feature("starttls", ch, "/session use_tls") continue; }
				show_feature("mechanisms", ch, "/session use_sasl")
					for (another = ch->children; another; another = another->next) {
						if (!xstrcmp(another->name, "mechanism")) {
							if (0); 
							else show_subfeature("DIGEST-MD5", "SASL", another, "Default");
							else show_subfeature("PLAIN", "SASL", another, "/sesion plaintext_passwd");
							else print("xmpp_feature_sub_unknown", session_name(s), j->server, "SASL", __(another->data), "");
						}
					} continue; 
				}
#if 0
				show_feature("compression", ch, "/session use_compression method1,method2,method3")
					for (another = ch->children; another; another = another->next) {
						if (!xstrcmp(another->name, "method")) {
							if (0); 
							else show_subfeature("zlib", "COMPRESSION", another, "/session use_compression zlib");
							else show_subfeature("lzw", "COMPRESSION", another, "/session use_compression lzw");
							else print("xmpp_feature_sub_unknown", session_name(s), j->server, "COMPRESSION" __(another->data), "");
						}
					} continue;
				}
#endif
				show_feature("session", ch, "Manage session") continue; }
				show_feature("bind", ch, "Bind resource") continue; }
				show_feature("register", ch, "Register account using /register") continue; }

				print("xmpp_feature_unknown", session_name(s), j->server, ch->name, jabber_attr(n->atts, "xmlns"));
#undef show_feature
#undef show_subfeature
			}
			print("xmpp_feature_footer", session_name(s), j->server);
		}

		for (n = n->children; n; n = n->next) {
			if (!xstrcmp(n->name, "starttls")) {
#ifdef JABBER_HAVE_SSL
				CHECK_CONNECT(1, 0, continue)
				CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-tls", continue)

				if (!j->using_ssl && session_int_get(s, "use_tls") == 1 && session_int_get(s, "use_ssl") == 0) {
					debug_function("[jabber] stream:features && TLS! let's rock.\n");

					watch_write(j->send_watch, "<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>");
					return;
				}
#endif
			} else if (!xstrcmp(n->name, "mechanisms") && !mech_node) {		/* faster than xmlnode_find_child */
				CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", continue)
				mech_node = n->children;
			} else if (!xstrcmp(n->name, "session")) {
				CHECK_CONNECT(2, 0, continue)
				CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-session", continue)

				use_fjuczers |= 1;

			} else if (!xstrcmp(n->name, "bind")) {
				CHECK_CONNECT(2, 0, continue)
				CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-bind", continue)
				
				use_fjuczers |= 2;
			} else if (!xstrcmp(n->name, "compression")) {
				const char *tmp		= session_get(s, "use_compression");
				const char *method_comp = NULL;
				char *tmp2;

				xmlnode_t *method;

				if (!tmp) continue;
				CHECK_CONNECT(1, 0, continue)
				CHECK_XMLNS(n, "http://jabber.org/features/compress", continue)

				j->using_compress = JABBER_COMPRESSION_NONE;

				for (method = n->children; method; method = method->next) {
					if (!xstrcmp(method->name, "method")) {
						if (!xstrcmp(method->data, "zlib")) {
#ifdef HAVE_ZLIB
							if ((tmp2 = xstrstr(tmp, "zlib")) && ((tmp2 < method_comp) || (!method_comp)) && 
								(tmp2[4] == ',' || tmp2[4] == '\0')) {
									method_comp = tmp2;			 /* found more preferable method */
									j->using_compress = JABBER_COMPRESSION_ZLIB_INIT;
							}
#else
							debug_error("[jabber] compression... NO ZLIB support\n");
#endif
						} else if (!xstrcmp(method->data, "lzw")) {
							if ((tmp2 = xstrstr(tmp, "zlib")) && ((tmp2 < method_comp) || (!method_comp)) &&
								(tmp2[3] == ',' || tmp2[3] == '\0')) {
/*									method_comp = tmp2 */			/* nieczynne */
/*									j->using_compress = JABBER_COMPRESSION_LZW_INIT; */
							}
							debug_error("[jabber] compression... sorry NO LZW support\n");
						} else	debug_error("[jabber] compression %s\n", __(method->data));

					} else debug_error("[jabber] stream:features/compression %s\n", __(method->name));
				}
				debug_function("[jabber] compression, method: %d\n", j->using_compress);

				if (!j->using_compress) continue;

				if (j->using_compress == JABBER_COMPRESSION_ZLIB_INIT)		method_comp = "zlib";
				else if (j->using_compress == JABBER_COMPRESSION_LZW_INIT)	method_comp = "lzw";
				else {
					debug_error("[jabber] BLAH [%s:%d] %s; %d\n", __FILE__, __LINE__, method_comp, j->using_compress);
					continue;
				}

				watch_write(j->send_watch, 
					"<compress xmlns=\"http://jabber.org/protocol/compress\"><method>%s</method></compress>", method_comp);
				return;
			} else {
				debug_error("[jabber] stream:features %s\n", __(n->name));
			}
		}

		if (j->send_watch) j->send_watch->transfer_limit = -1;

		if (use_fjuczers & 2)	/* bind */
			watch_write(j->send_watch, 
				"<iq type=\"set\" id=\"bind%d\"><bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"><resource>%s</resource></bind></iq>", 
				j->id++, j->resource);

		if (use_fjuczers & 1)	/* session */
			watch_write(j->send_watch, 
				"<iq type=\"set\" id=\"auth\"><session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/></iq>",
				j->id++);

		if (j->connecting == 2 && !(use_fjuczers & 2))	/* STRANGE, BUT MAYBE POSSIBLE? */
			jabber_session_connected(s, jdh);

		JABBER_COMMIT_DATA(j->send_watch);	

		if (use_sasl && mech_node) {
			enum {
				JABBER_SASL_AUTH_UNKNOWN,			/* UNKNOWN */
				JABBER_SASL_AUTH_PLAIN,				/* PLAIN */
				JABBER_SASL_AUTH_DIGEST_MD5,			/* DIGEST-MD5 */
			} auth_type = JABBER_SASL_AUTH_UNKNOWN;

			for (; mech_node; mech_node = mech_node->next) {
				if (!xstrcmp(mech_node->name, "mechanism")) {

					if (!xstrcmp(mech_node->data, "DIGEST-MD5"))	auth_type = JABBER_SASL_AUTH_DIGEST_MD5;
					else if (!xstrcmp(mech_node->data, "PLAIN")) {
						if ((session_int_get(s, "plaintext_passwd"))) {
							auth_type = JABBER_SASL_AUTH_PLAIN;
							break;	/* jesli plaintext jest prefered wychodzimy */
						}
						/* ustaw tylko wtedy gdy nie ma ustawionego, wolimy MD5 */
						if (auth_type == JABBER_SASL_AUTH_UNKNOWN) auth_type = JABBER_SASL_AUTH_PLAIN;

					} else debug_error("[jabber] SASL, unk mechanism: %s\n", __(mech_node->data));
				} else debug_error("[jabber] SASL mechanisms: %s\n", mech_node->name);
			}

			if (auth_type != JABBER_SASL_AUTH_UNKNOWN) 
				j->connecting = 2;

			switch (auth_type) {
				string_t str;
				char *encoded;

				case JABBER_SASL_AUTH_DIGEST_MD5:
					debug_function("[jabber] SASL chosen: JABBER_SASL_AUTH_DIGEST_MD5\n");
					watch_write(j->send_watch, 
						"<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" mechanism=\"DIGEST-MD5\"/>");
					break;
				case JABBER_SASL_AUTH_PLAIN:
					debug_function("[jabber] SASL chosen: JABBER_SASL_AUTH_PLAIN\n");
					str = string_init(NULL);

					string_append_raw(str, "\0", 1);
					string_append_n(str, s->uid+4, xstrchr(s->uid+4, '@')-s->uid-4);
					string_append_raw(str, "\0", 1);
					string_append(str, session_get(s, "password"));

					encoded = base64_encode(str->str, str->len);

					watch_write(j->send_watch,
						"<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" mechanism=\"PLAIN\">%s</auth>", encoded);

					xfree(encoded);
					string_free(str, 1);
					break;
				default:
					debug_error("[jabber] SASL auth_type: UNKNOWN!!!, disconnecting from host.\n");
					j->parser = NULL; jabber_handle_disconnect(s, 
							"We tried to auth using SASL but none of method supported by server we know. "
							"Check __debug window and supported SASL server auth methods and sent them to ekg2 devs. "
							"Temporary you can turn off SASL auth using /session use_sasl 0", EKG_DISCONNECT_FAILURE);
					break;
			}
		}

	} else if (!xstrcmp(n->name, "proceed")) {	/* TLS HERE */
		CHECK_CONNECT(1, 0, return)

		if (!xstrcmp(jabber_attr(n->atts, "xmlns"), "urn:ietf:params:xml:ns:xmpp-tls")) {
#ifdef JABBER_HAVE_SSL
#ifdef JABBER_HAVE_GNUTLS
			const int cert_type_priority[3] = {GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0};
			const int comp_type_priority[3] = {GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0};
			int ret = 0;
#endif

			debug_function("[jabber] proceed urn:ietf:params:xml:ns:xmpp-tls TLS let's rock\n");
#ifdef JABBER_HAVE_OPENSSL
			if (!(j->ssl_session = SSL_new(jabberSslCtx))) {
				print("conn_failed_tls");
				j->parser = NULL; jabber_handle_disconnect(s, NULL, EKG_DISCONNECT_FAILURE);
				return;
			}
			if (SSL_set_fd(j->ssl_session, j->fd) == 0) {
				print("conn_failed_tls");
				SSL_free(j->ssl_session);
				j->ssl_session = NULL;
				j->parser = NULL; jabber_handle_disconnect(s, NULL, EKG_DISCONNECT_FAILURE);
				return;
			}
#else
			gnutls_certificate_allocate_credentials(&(j->xcred));
			/* XXX - ~/.ekg/certs/server.pem */
			gnutls_certificate_set_x509_trust_file(j->xcred, "brak", GNUTLS_X509_FMT_PEM);

			if ((ret = SSL_INIT(j->ssl_session))) {
				print("conn_failed_tls");
				jabber_handle_disconnect(s, SSL_ERROR(ret), EKG_DISCONNECT_FAILURE);
				return;
			}

			gnutls_set_default_priority(j->ssl_session);
			gnutls_certificate_type_set_priority(j->ssl_session, cert_type_priority);
			gnutls_credentials_set(j->ssl_session, GNUTLS_CRD_CERTIFICATE, j->xcred);
			gnutls_compression_set_priority(j->ssl_session, comp_type_priority);

			/* we use read/write instead of recv/send */
			gnutls_transport_set_pull_function(j->ssl_session, (gnutls_pull_func)read);
			gnutls_transport_set_push_function(j->ssl_session, (gnutls_push_func)write);
			SSL_SET_FD(j->ssl_session, j->fd);
#endif
			/* XXX HERE WE SHOULD DISABLE RECV_WATCH && (SEND WATCH TOO?) */
/*			j->send_watch->type = WATCH_NONE; */

			jabber_handle_connect_tls(0, j->fd, WATCH_NONE, s);
#else
			debug_error("[jabber] proceed + urn:ietf:params:xml:ns:xmpp-tls but jabber compilated without ssl support?\n");
#endif
		} else	debug_error("[jabber] proceed what's that xmlns: %s ?\n", jabber_attr(n->atts, "xmlns"));

	} else if (!xstrcmp(n->name, "compressed")) {	/* COMPRESSION HERE */
		CHECK_CONNECT(1, 0, return)
		CHECK_XMLNS(n, "http://jabber.org/protocol/compress", return)

	/* REINITIALIZE STREAM WITH COMPRESSION TURNED ON */
		
		switch (j->using_compress) {
			case JABBER_COMPRESSION_NONE: 		break;
			case JABBER_COMPRESSION_ZLIB_INIT: 	j->using_compress = JABBER_COMPRESSION_ZLIB;	break;
			case JABBER_COMPRESSION_LZW_INIT:	j->using_compress = JABBER_COMPRESSION_LZW;	break;

			default:
				debug_error("[jabber] invalid j->use_compression (%d) state..\n", j->using_compress);
				j->using_compress = JABBER_COMPRESSION_NONE;
		}
		
		if (j->using_compress == JABBER_COMPRESSION_NONE) {
			debug_error("[jabber] j->using_compress == JABBER_COMPRESSION_NONE but, compressed stanza?\n");
			return;
		}

		j->parser = jabber_parser_recreate(NULL, XML_GetUserData(j->parser));
		j->send_watch->handler	= jabber_handle_write;

		watch_write(j->send_watch,
				"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" version=\"1.0\">",
				j->server);
	
	} else if (!xstrcmp(n->name, "success")) {
		CHECK_CONNECT(2, 0, return)
		CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

		j->parser = jabber_parser_recreate(NULL, XML_GetUserData(j->parser));	/* here could be passed j->parser to jabber_parser_recreate() but unfortunetly expat makes SIGSEGV */
		watch_write(j->send_watch, 
				"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" version=\"1.0\">", 
				j->server);

	} else if (!xstrcmp(n->name, "failure")) {
		char *reason;

		CHECK_CONNECT(2, 0, return)
		CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

		reason = n->children ? n->children->name : NULL;
		debug_function("[jabber] failure n->child: 0x%x n->child->name: %s\n", n->children, __(reason));

/* XXX here, think about nice reasons */
		if (!reason)						reason = "(SASL) GENERIC FAILURE";
		else if (!xstrcmp(reason, "temporary-auth-failure"))	reason = "(SASL) TEMPORARY AUTH FAILURE";
		else debug_error("[jabber] UNKNOWN reason: %s\n", reason);

		j->parser = NULL; jabber_handle_disconnect(s, reason, EKG_DISCONNECT_FAILURE);

	} else if (!xstrcmp(n->name, "challenge")) {
		char *data;
		char **arr;
		int i;

		char *realm	= NULL;
		char *rspauth	= NULL;
		char *nonce	= NULL;

		CHECK_CONNECT(2, 0, return)
		CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

		if (!n->data) {
			debug_error("[jabber] challenge, no data.. (XXX?) disconnecting from host.\n");
			return;
		}
	/* decode && parse challenge data */
		data = base64_decode(n->data);
		debug_error("[jabber] PARSING challange (%s): \n", data);
		arr = array_make(data, "=,", 0, 1, 1);	/* maybe we need to change/create another one parser... i'm not sure. please notify me, 
								I'm lazy, sorry */
							/* for chrome.pl and jabber.autocom.pl it works */
		xfree(data);
	
	/* check parsed data... */
		i = 0;
		while (arr[i]) {
			debug_error("[%d] %s: %s\n", i / 2, arr[i], __(arr[i+1]));
			if (!arr[i+1]) {
				debug_error("Parsing var<=>value failed, NULL....\n");
				array_free(arr);
				j->parser = NULL; 
				jabber_handle_disconnect(s, "IE, Current SASL support for ekg2 cannot handle with this data, sorry.", EKG_DISCONNECT_FAILURE);
				return;
			}

			if (!xstrcmp(arr[i], "realm"))		realm	= arr[i+1];
			if (!xstrcmp(arr[i], "rspauth"))	rspauth	= arr[i+1];
			if (!xstrcmp(arr[i], "nonce"))		nonce	= arr[i+1];

			i++;
			if (arr[i]) i++;
		}

		if (rspauth) {
			const char *tmp = session_get(s, "__sasl_excepted");

			if (!xstrcmp(tmp, rspauth)) {
				debug_function("[jabber] KEYS MATCHED, THX FOR USING SASL SUPPORT IN EKG2.\n");
				watch_write(j->send_watch, "<response xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>");
			} else {
				debug_error("[jabber] RSPAUTH BUT KEYS DON'T MATCH!!! IS: %s EXCEPT: %s, DISCONNECTING\n", __(rspauth), __(tmp));
				j->parser = NULL; jabber_handle_disconnect(s, "IE, SASL RSPAUTH DOESN'T MATCH!!", EKG_DISCONNECT_FAILURE);
			}
			session_set(s, "__sasl_excepted", NULL);
		} else {
			char *username = xstrndup(s->uid+4, xstrchr(s->uid+4, '@')-s->uid-4);
			const char *password = session_get(s, "password");

			string_t str = string_init(NULL);	/* text to encode && sent */
			char *encoded;				/* BASE64 response text */

			char tmp_cnonce[32];
			char *cnonce;

			char *xmpp_temp;
			char *auth_resp;

			if (!realm) realm = j->server;

			for (i=0; i < sizeof(tmp_cnonce); i++) tmp_cnonce[i] = (char) (256.0*rand()/(RAND_MAX+1.0));	/* generate random number using high-order bytes man 3 rand() */

			cnonce = base64_encode(tmp_cnonce, sizeof(tmp_cnonce));

			xmpp_temp	= saprintf(":xmpp/%s", realm);
			auth_resp	= jabber_challange_digest(username, password, nonce, cnonce, xmpp_temp, realm);
			session_set(s, "__sasl_excepted", auth_resp);
			xfree(xmpp_temp);

			xmpp_temp	= saprintf("AUTHENTICATE:xmpp/%s", realm);
			auth_resp	= jabber_challange_digest(username, password, nonce, cnonce, xmpp_temp, realm);
			xfree(xmpp_temp);

			string_append(str, "username=\"");	string_append(str, username);	string_append_c(str, '\"');
			string_append(str, ",realm=\"");	string_append(str, realm);	string_append_c(str, '\"');
			string_append(str, ",nonce=\"");	string_append(str, nonce);	string_append_c(str, '\"');
			string_append(str, ",cnonce=\"");	string_append(str, cnonce);	string_append_c(str, '\"');
			string_append(str, ",nc=00000001");
			string_append(str, ",digest-uri=\"xmpp/"); string_append(str, realm);	string_append_c(str, '\"');
			string_append(str, ",qop=auth");
			string_append(str, ",response=");	string_append(str, auth_resp);
			string_append(str, ",charset=utf-8");

			encoded = base64_encode(str->str, str->len);
			watch_write(j->send_watch, "<response xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">%s</response>", encoded);
			xfree(encoded);

			string_free(str, 1);
			xfree(username);
			xfree(cnonce);
		}
		array_free(arr);
	} else if (j->istlen) {
		tlen_handle(n, s);
	} else	debug_error("[jabber] what's that: %s ?\n", n->name);
}

static void jabber_handle_message(xmlnode_t *n, session_t *s, jabber_private_t *j) {
	xmlnode_t *nerr		= xmlnode_find_child(n, "error");
	xmlnode_t *nbody   	= xmlnode_find_child(n, "body");
	xmlnode_t *nsubject	= NULL;
	xmlnode_t *xitem;
	
	const char *from = jabber_attr(n->atts, "from");

	char *juid 	= jabber_unescape(from); /* was tmp */
	char *uid;
	time_t bsent = 0;
	string_t body;
	int new_line = 0;	/* if there was headlines do we need to display seperator between them and body? */
	
	if (j->istlen)	uid = saprintf("tlen:%s", juid);
	else		uid = saprintf("jid:%s", juid);

	xfree(juid);

	if (nerr) {
		char *ecode = jabber_attr(nerr->atts, "code");
		char *etext = jabber_unescape(nerr->data);
		char *recipient = get_nickname(s, uid);

		if (nbody && nbody->data) {
			char *tmp2 = jabber_unescape(nbody->data);
			char *mbody = xstrndup(tmp2, 15);
			xstrtr(mbody, '\n', ' ');

			print("jabber_msg_failed_long", recipient, ecode, etext, mbody);

			xfree(mbody);
			xfree(tmp2);
		} else
			print("jabber_msg_failed", recipient, ecode, etext);

		xfree(etext);
		xfree(uid);
		return;
	} /* <error> */

	if (j->istlen) {	/* disable typing notify, tlen protocol doesn't send info about it (?) XXX */
		char *session 	= xstrdup(session_uid_get(s));
		char *rcpt	= xstrdup(uid);
		int state 	= 0;
		int stateo 	= EKG_XSTATE_TYPING;

		query_emit_id(NULL, PROTOCOL_XSTATE, &session, &rcpt, &state, &stateo);

		xfree(session);
		xfree(rcpt);
	}
	
	body = string_init("");

	for (xitem = n->children; xitem; xitem = xitem->next) {
		if (!xstrcmp(xitem->name, "x")) {
			const char *ns = jabber_attr(xitem->atts, "xmlns");
			
			if (!xstrcmp(ns, "jabber:x:encrypted")) {	/* JEP-0027 */
				char *x_encrypted = xstrdup(xitem->data);
				char *error = NULL;

				if ((x_encrypted = jabber_openpgp(s, uid, JABBER_OPENGPG_DECRYPT, x_encrypted, NULL, &error))) {
					string_append(body, "Encrypted message: ");
					string_append(body,  x_encrypted);
					xfree(x_encrypted);
				} else {
					string_append(body, "Encrypted message but error: ");
					string_append(body, error);
				}
				string_append_c(body, '\n');
				xfree(error);

				new_line = 1;
			} else if (!xstrncmp(ns, "jabber:x:event", 14)) {
				int acktype = 0; /* bitmask: 2 - queued ; 1 - delivered */
				int isack;

				if (xmlnode_find_child(xitem, "delivered"))	acktype |= 1;	/* delivered */
				if (xmlnode_find_child(xitem, "offline"))	acktype	|= 2;	/* queued */
				if (xmlnode_find_child(xitem, "composing"))	acktype |= 4;	/* composing */

				isack = (acktype & 1) || (acktype & 2);

				/* jesli jest body, to mamy do czynienia z prosba o potwierdzenie */
				if (nbody && isack) {
					char *id = jabber_attr(n->atts, "id");
					const char *our_status = session_status_get(s);

					if (j->send_watch) j->send_watch->transfer_limit = -1;

					watch_write(j->send_watch, "<message to=\"%s\"><x xmlns=\"jabber:x:event\">", from);

					if (!xstrcmp(our_status, EKG_STATUS_INVISIBLE)) {
						watch_write(j->send_watch, "<offline/>");
					} else {
						if (acktype & 1)
							watch_write(j->send_watch, "<delivered/>");
						if (acktype & 2)
							watch_write(j->send_watch, "<displayed/>");
					};
					watch_write(j->send_watch, "<id>%s</id></x></message>", id);

					JABBER_COMMIT_DATA(j->send_watch);
				}
				/* je�li body nie ma, to odpowiedz na nasza prosbe */
				if (!nbody && isack) {
					char *__session = xstrdup(session_uid_get(s));
					char *__rcpt	= xstrdup(uid); /* was uid+4 */
					char *__status  = xstrdup(
						(acktype & 1) ? EKG_ACK_DELIVERED : 
						(acktype & 2) ? EKG_ACK_QUEUED : 
						NULL);
					char *__seq	= NULL; /* id ? */
					/* protocol_message_ack; sesja ; uid + 4 ; seq (NULL ? ) ; status - delivered ; queued ) */
					query_emit_id(NULL, PROTOCOL_MESSAGE_ACK, &__session, &__rcpt, &__seq, &__status);
					xfree(__session);
					xfree(__rcpt);
					xfree(__status);
					/* xfree(__seq); */
				}
				
				{
					char *__session = xstrdup(session_uid_get(s));
					char *__rcpt	= xstrdup(uid);
					int  state	= (!nbody && (acktype & 4) ? EKG_XSTATE_TYPING : 0);
					int  stateo	= (!state ? EKG_XSTATE_TYPING : 0);

					query_emit_id(NULL, PROTOCOL_XSTATE, &__session, &__rcpt, &state, &stateo);
					
					xfree(__session);
					xfree(__rcpt);
				}
/* jabber:x:event */	} else if (!xstrncmp(ns, "jabber:x:oob", 12)) {
				xmlnode_t *xurl;
				xmlnode_t *xdesc;

				if ((xurl = xmlnode_find_child(xitem, "url"))) {
					string_append(body, "URL: ");
					string_append(body, xurl->data);
					if ((xdesc = xmlnode_find_child(xitem, "desc"))) {
						string_append(body, " (");
						string_append(body, xdesc->data);
						string_append(body, ")");
					}
					string_append(body, "\n");
					new_line = 1;
				}
/* jabber:x:oob */	} else if (!xstrncmp(ns, "jabber:x:delay", 14)) {
				bsent = jabber_try_xdelay(jabber_attr(xitem->atts, "stamp"));
#if 0		/* XXX, fjuczer? */
				if (nazwa_zmiennej_do_formatowania_czasu) {
					/* some people don't have time in formats... and if we really do like in emails (first headlines than body) so display it too.. */
					stuct tm *tm = localtime(&bsent);
					char buf[100];
					string_append(body, "Sent: ");
					if (!strftime(buf, sizeof(buf), nazwa_zmiennej_do_formatowania_czasu, tm) 
						string_append(body, itoa(bsent);	/* if too long display seconds since the Epoch */
					else	string_append(body, buf);	/* otherwise display formatted time */
					new_line = 1;
				}
#endif
			} else debug_error("[JABBER, MESSAGE]: <x xmlns=%s\n", ns);
/* x */		} else if (!xstrcmp(xitem->name, "subject")) {
			nsubject = xitem;
			if (nsubject->data) {
				string_append(body, "Subject: ");
				string_append(body, nsubject->data);
				string_append(body, "\n");
				new_line = 1;
			}
/* subject */	} else if (!xstrcmp(xitem->name, "body")) {
		} /* XXX, JEP-0085 here */
		else if (!xstrcmp(jabber_attr(xitem->atts, "xmlns"), "http://jabber.org/protocol/chatstates")) {
			if (!xstrcmp(xitem->name, "active"))		{ }
			else if (!xstrcmp(xitem->name, "composing"))	{ } 
			else if (!xstrcmp(xitem->name, "paused"))	{ } 
			else if (!xstrcmp(xitem->name, "inactive"))	{ } 
			else if (!xstrcmp(xitem->name, "gone")) 	{ } 
			else debug_error("[JABBER, MESSAGE]: INVALID CHATSTATE: %s\n", xitem->name);
		} else debug_error("[JABBER, MESSAGE]: <%s\n", xitem->name);
	}
	if (new_line) string_append(body, "\n"); 	/* let's seperate headlines from message */
	if (nbody)    string_append(body, nbody->data);	/* here message */

	if (nbody || nsubject) {
		const char *type = jabber_attr(n->atts, "type");

		char *me	= xstrdup(session_uid_get(s));
		int class 	= EKG_MSGCLASS_CHAT;
		int ekgbeep 	= EKG_TRY_BEEP;
		int secure	= 0;
		char **rcpts 	= NULL;
		char *seq 	= NULL;
		uint32_t *format= NULL;
		time_t sent	= bsent;

		char *text = tlenjabber_unescape(body->str);

		if (!sent) sent = time(NULL);

		debug_function("[jabber,message] type = %s\n", type);
		if (!xstrcmp(type, "groupchat")) {
			char *tuid = xstrrchr(uid, '/');				/* temporary */
			char *uid2 = (tuid) ? xstrndup(uid, tuid-uid) : xstrdup(uid);		/* muc room */
			char *nick = (tuid) ? xstrdup(tuid+1) : NULL;				/* nickname */
			newconference_t *c = newconference_find(s, uid2);
			int isour = (c && !xstrcmp(c->private, nick)) ? 1 : 0;			/* is our message? */
			char *formatted;

		/* jesli (bsent != 0) wtedy mamy do czynienia z backlogiem */

			class	|= EKG_NO_THEMEBIT;
			ekgbeep	= EKG_NO_BEEP;

			formatted = format_string(format_find(isour ? "jabber_muc_send" : "jabber_muc_recv"),
				session_name(s), uid2, nick ? nick : uid2+4, text, "");
			
			debug("[MUC,MESSAGE] uid2:%s uuid:%s message:%s\n", uid2, nick, text);
			query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &uid, &rcpts, &formatted, &format, &sent, &class, &seq, &ekgbeep, &secure);

			xfree(uid2);
			xfree(nick);
			xfree(formatted);
		} else {
			query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &uid, &rcpts, &text, &format, &sent, &class, &seq, &ekgbeep, &secure);
		}

		xfree(me);
		xfree(text);
		array_free(rcpts);
/*
		xfree(seq);
		xfree(format);
*/
	}
	string_free(body, 1);
	xfree(uid);
} /* */

/* idea and some code copyrighted by Marek Marczykowski jid:marmarek@jabberpl.org */

/* handlue <x xmlns=jabber:x:data type=form */
static void jabber_handle_xmldata_form(session_t *s, const char *uid, const char *command, xmlnode_t *form, const char *param) { /* JEP-0004: Data Forms */
	xmlnode_t *node;
	int fieldcount = 0;
/*	const char *FORM_TYPE = NULL; */

	for (node = form; node; node = node->next) {
		if (!xstrcmp(node->name, "title")) {
			char *title = jabber_unescape(node->data);
			print("jabber_form_title", session_name(s), uid, title);
			xfree(title);
		} else if (!xstrcmp(node->name, "instructions")) {
			char *inst = jabber_unescape(node->data);
			print("jabber_form_instructions", session_name(s), uid, inst);
			xfree(inst);
		} else if (!xstrcmp(node->name, "field")) {
			xmlnode_t *child;
			char *label	= jabber_unescape(jabber_attr(node->atts, "label"));
			char *var	= jabber_unescape(jabber_attr(node->atts, "var"));
			char *def_option = NULL;
			string_t sub = NULL;
			int subcount = 0;

			int isreq = 0;	/* -1 - optional; 1 - required */
			
			if (!fieldcount) print("jabber_form_command", session_name(s), uid, command, param);

			for (child = node->children; child; child = child->next) {
				if (!xstrcmp(child->name, "required")) isreq = 1;
				else if (!xstrcmp(child->name, "value")) {
					xfree(def_option); 
					def_option	= jabber_unescape(child->data); 
				} 
				else if (!xstrcmp(child->name, "option")) {
					xmlnode_t *tmp;
					char *opt_value = jabber_unescape( (tmp = xmlnode_find_child(child, "value")) ? tmp->data : NULL);
					char *opt_label = jabber_unescape(jabber_attr(child->atts, "label"));
					char *fritem;

					fritem = format_string(format_find("jabber_form_item_val"), session_name(s), uid, opt_value, opt_label);
					if (!sub)	sub = string_init(fritem);
					else		string_append(sub, fritem);
					xfree(fritem);

/*					print("jabber_form_item_sub", session_name(s), uid, opt_label, opt_value); */
/*					debug("[[option]] [value] %s [label] %s\n", opt_value, opt_label); */

					xfree(opt_value);
					xfree(opt_label);
					subcount++;
					if (!(subcount % 4)) string_append(sub, "\n\t");
				} else debug_error("[jabber] wtf? FIELD->CHILD: %s\n", child->name);
			}

			print("jabber_form_item", session_name(s), uid, label, var, def_option, 
				isreq == -1 ? "X" : isreq == 1 ? "V" : " ");
			if (sub) {
				int len = xstrlen(sub->str);
				if (sub->str[len-1] == '\t' && sub->str[len-2] == '\n') sub->str[len-2] = 0;
				print("jabber_form_item_sub", session_name(s), uid, sub->str);
				string_free(sub, 1);
			}
			fieldcount++;
			xfree(var);
			xfree(label);
		}
	}
	if (!fieldcount) print("jabber_form_command", session_name(s), uid, command);
	print("jabber_form_end", session_name(s), uid, command, param);
}

/* handluje <x xmlns=jabber:x:data type=submit */
static int jabber_handle_xmldata_submit(session_t *s, xmlnode_t *form, const char *FORM_TYPE, int alloc, ...) {
	char **atts	= NULL;
	int valid	= 0;
	int count	= 0;
	char *vatmp;
	va_list ap;

	va_start(ap, alloc);

	if (!alloc) while ((vatmp = va_arg(ap, char *))) { 
		atts		= (char **) xrealloc(atts, sizeof(char *) * (count + 3));
		atts[count]	= xstrdup(vatmp);
		atts[count+1]	= (char *) va_arg(ap, char **);					/* here is char ** */
		atts[count+2]	= NULL;
		count += 2;
	}

	for (; form; form = form->next) {
		if (!xstrcmp(form->name, "field")) {
			int quiet = 0;
			const char *vartype	= jabber_attr(form->atts, "type"); 
			const char *varname	= jabber_attr(form->atts, "var");
			char *value		= jabber_unescape(form->children ? form->children->data : NULL);
			char **tmp; 
							
			if (FORM_TYPE && (!xstrcmp(varname, "FORM_TYPE") && !xstrcmp(vartype, "hidden") && !xstrcmp(value, FORM_TYPE))) { valid = 1; quiet = 1;	}
			if ((tmp = (char **) jabber_attr(atts, varname))) {
				if (!alloc)	{ xfree(*tmp);		*tmp = value; }			/* here is char ** */
				else 		{ xfree((char *) tmp);	 tmp = (char **) value; }	/* here is char * */
				value	= NULL;
			} else if (alloc) {
				atts            = (char **) xrealloc(atts, sizeof(char *) * (count + 3));
				atts[count]     = xstrdup(varname);					
				atts[count+1]	= value;						/* here is char * */
				atts[count+2]	= NULL;
				count += 2;
				value = NULL;
			} else if (!quiet) debug_error("JABBER, RC, FORM_TYPE: %s ATTR NOT IN ATTS: %s (SOMEONE IS DOING MESS WITH FORM_TYPE?)\n", FORM_TYPE, varname);
			xfree(value);
		}
	}
	if (alloc)	(*(va_arg(ap, char ***))) = atts;
	va_end(ap);
	return valid;
}

/* handlue <x xmlns=jabber:x:data type=result */
static void jabber_handle_xmldata_result(session_t *s, xmlnode_t *form, const char *uid) {
	int print_end = 0;
	char **labels = NULL;
	int labels_count = 0;

	for (; form; form = form->next) {
		if (!xstrcmp(form->name, "title")) {
			char *title = jabber_unescape(form->data);
			print("jabber_form_title", session_name(s), uid, title);
			print_end = 1;
			xfree(title);
		} else if (!xstrcmp(form->name, "item")) {
			xmlnode_t *q;
			print("jabber_form_item_beg", session_name(s), uid);
			for (q = form->children; q; q = q->next) {
				if (!xstrcmp(q->name, "field")) {
					xmlnode_t *temp;
					char *var = jabber_attr(q->atts, "var");
					char *tmp = jabber_attr(labels, var);
					char *val = ((temp = xmlnode_find_child(q, "value"))) ? jabber_unescape(temp->data) : NULL;

					print("jabber_form_item_plain", session_name(s), uid, tmp ? tmp : var, var, val);
					xfree(val);
				}
			}
			print("jabber_form_item_end", session_name(s), uid);
		} else if (!xstrcmp(form->name, "reported")) {
			xmlnode_t *q;
			for (q = form->children; q; q = q->next) {
				if (!xstrcmp(q->name, "field")) {
					labels				= (char **) xrealloc(labels, (sizeof(char *) * ((labels_count+1) * 2)) + 1);
					labels[labels_count*2]		= xstrdup(jabber_attr(q->atts, "var"));
					labels[labels_count*2+1]	= jabber_unescape(jabber_attr(q->atts,"label"));
					labels[labels_count*2+2]	= NULL;
					labels_count++;
				}
			}
		} else if (!xstrcmp(form->name, "field")) {
			xmlnode_t *temp;
			char *var	= jabber_attr(form->atts, "var");
			char *label	= jabber_unescape(jabber_attr(form->atts, "label"));
			char *val	= jabber_unescape(((temp = xmlnode_find_child(form, "value"))) ? temp->data : NULL);

			print("jabber_privacy_list_item" /* XXX */, session_name(s), uid, label ? label : var, val);
			xfree(label); xfree(val);
		} else debug_error("jabber_handle_xmldata_result() name: %s\n", form->name);
	}
	if (print_end) print("jabber_form_end", session_name(s), uid, "");
	array_free(labels);
}

static void jabber_handle_iq(xmlnode_t *n, jabber_handler_data_t *jdh) {
	const char *atype= jabber_attr(n->atts, "type");
	const char *id   = jabber_attr(n->atts, "id");
	const char *from = jabber_attr(n->atts, "from");

	session_t *s = jdh->session;
	jabber_private_t *j = jabber_private(s);
	xmlnode_t *q;

	enum {
		JABBER_IQ_TYPE_NONE,
		JABBER_IQ_TYPE_GET,
		JABBER_IQ_TYPE_SET,
		JABBER_IQ_TYPE_RESULT,
		JABBER_IQ_TYPE_ERROR,
	} type = JABBER_IQ_TYPE_NONE;

	if (0);
	else if (!xstrcmp(atype, "get"))	type = JABBER_IQ_TYPE_GET;
	else if (!xstrcmp(atype, "set"))	type = JABBER_IQ_TYPE_SET;
	else if (!xstrcmp(atype, "result"))	type = JABBER_IQ_TYPE_RESULT;
	else if (!xstrcmp(atype, "error"))	type = JABBER_IQ_TYPE_ERROR;

	else if (atype)				debug_error("[jabber] <iq> wtf iq type: %s\n", atype);
	else {					debug_error("[jabber] <iq> without type!\n");
						return;
	}

	if (type == JABBER_IQ_TYPE_ERROR) {
		xmlnode_t *e = xmlnode_find_child(n, "error");
		char *reason = (e) ? jabber_unescape(e->data) : NULL;

		if (!xstrncmp(id, "register", 8)) {
			print("register_failed", jabberfix(reason, "?"));
		} else if (!xstrcmp(id, "auth")) {
			j->parser = NULL; jabber_handle_disconnect(s, reason ? reason : _("Error connecting to Jabber server"), EKG_DISCONNECT_FAILURE);

		} else if (!xstrncmp(id, "passwd", 6)) {
			print("passwd_failed", jabberfix(reason, "?"));
			session_set(s, "__new_password", NULL);
		} else if (!xstrncmp(id, "search", 6)) {
			debug_error("[JABBER] search failed: %s\n", reason);
		}
#if WITH_JABBER_DCC
		else if (!xstrncmp(id, "offer", 5)) {
			char *uin = jabber_unescape(from);
			dcc_t *p = jabber_dcc_find(uin, id, NULL);

			if (p) {
				/* XXX, new theme it's for ip:port */
				print("dcc_error_refused", format_user(s, p->uid));
				dcc_close(p);
			} else {
				/* XXX, possible abuse attempt */
			}
			xfree(uin);
		}
#endif
		else debug_error("[JABBER] GENERIC IQ ERROR: %s\n", reason);

		xfree(reason);
		return;			/* we don't need to go down */
	}

/* thx to Michal Gorny for following code. 
 * handle google mail notify. */
#if GMAIL_MAIL_NOTIFY
	if (type == JABBER_IQ_TYPE_RESULT && (q = xmlnode_find_child(n, "new-mail")) && !xstrcmp(jabber_attr(q->atts, "xmlns"), "google:mail:notify"))
		watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>", j->id++);
	if (type == JABBER_IQ_TYPE_SET && (q = xmlnode_find_child(n, "new-mail")) && !xstrcmp(jabber_attr(q->atts, "xmlns"), "google:mail:notify")) {
		print("gmail_new_mail", session_name(s));
		watch_write(j->send_watch, "<iq type='result' id='%s'/>", jabber_attr(n->atts, "id"));
		if (j->last_gmail_result_time && j->last_gmail_tid)
			watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\" newer-than-time=\"%s\" newer-than-tid=\"%s\" /></iq>", j->id++, j->last_gmail_result_time, j->last_gmail_tid);
		else
			watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>", j->id++);
	}
	if (type == JABBER_IQ_TYPE_RESULT && (q = xmlnode_find_child(n, "mailbox")) && !xstrcmp(jabber_attr(q->atts, "xmlns"), "google:mail:notify")) {
		char *mailcount = jabber_attr(q->atts, "total-matched");
		int tid_set = 0;
		xmlnode_t *child;
		xfree(j->last_gmail_result_time);
		j->last_gmail_result_time = xstrdup(jabber_attr(q->atts, "result-time"));

		print("gmail_count", session_name(s), mailcount);

		for (child = q->children; child; child = child->next) {
			if (!xstrcmp(child->name, "mail-thread-info")) {
				if (!tid_set)
				{
					xfree(j->last_gmail_tid);
					j->last_gmail_tid = xstrdup(jabber_attr(child->atts, "tid"));
				}
				tid_set = 1;
				xmlnode_t *subchild;
				string_t from = string_init(NULL);

				char *amessages = jabber_attr(child->atts, "messages");		/* messages count in thread */
				char *subject = NULL;
				int firstsender = 1;

				for (subchild = child->children; subchild; subchild = subchild->next) {
					if (0) {
					} else if (!xstrcmp(subchild->name, "subject")) {
						if (xstrcmp(subchild->data, "")) {
							xfree(subject);
							subject = jabber_unescape(subchild->data);
						}

					} else if (!xstrcmp(subchild->name, "senders")) {
						xmlnode_t *senders;

						for (senders = subchild->children; senders; senders = senders->next) {
							/* XXX, unescape */
							char *aname = jabber_attr(senders->atts, "name");
							char *amail = jabber_attr(senders->atts, "address");

							if (!firstsender)
								string_append(from, ", ");

							if (aname) {
								char *tmp = saprintf("%s <%s>", aname, amail);
								string_append(from, tmp);
								xfree(tmp);
							} else {
								string_append(from, amail);
							}

							firstsender = 0;
						}
					} else debug_error("[jabber] google:mail:notify/mail-thread-info wtf: %s\n", __(subchild->name));
				}

				print((amessages && atoi(amessages) > 1) ? "gmail_thread" : "gmail_mail", 
						session_name(s), from->str, jabberfix(subject, "(no subject)"), amessages);

				string_free(from, 1);
				xfree(subject);
			} else debug_error("[jabber, iq] google:mail:notify wtf: %s\n", __(child->name));
		}
		if (mailcount && atoi(mailcount)) /* we don't want to beep or send events if no new mail is available */
			newmail_common(s);
	}
#endif

	if (!xstrcmp(id, "auth")) {
		if (type == JABBER_IQ_TYPE_RESULT) {
			jabber_session_connected(s, jdh);
		} else {	/* Can someone look at that, i don't undestand for what is it here... */
			s->last_conn = time(NULL);
			j->connecting = 0;
		}
	}

	if (!xstrncmp(id, "passwd", 6)) {
		if (type == JABBER_IQ_TYPE_RESULT) {
			char *new_passwd = (char *) session_get(s, "__new_password");

			session_set(s, "password", new_passwd);
			session_set(s, "__new_password", NULL);
			wcs_print("passwd");
		} 
		session_set(s, "__new_password", NULL);
	}

#if WITH_JABBER_DCC
	if ((q = xmlnode_find_child(n, "si"))) { /* JEP-0095: Stream Initiation */
/* dj, I think we don't need to unescape rows (tags) when there should be int, do we?  ( <size> <offset> <length>... )*/
		xmlnode_t *p;

		if (type == JABBER_IQ_TYPE_RESULT) {
			char *uin = jabber_unescape(from);
			dcc_t *d;

			if ((d = jabber_dcc_find(uin, id, NULL))) {
				xmlnode_t *node;
				jabber_dcc_t *p = d->priv;
				char *stream_method = NULL;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "feature") && !xstrcmp(jabber_attr(node->atts, "xmlns"), "http://jabber.org/protocol/feature-neg")) {
						xmlnode_t *subnode;
						for (subnode = node->children; subnode; subnode = subnode->next) {
							if (!xstrcmp(subnode->name, "x") && !xstrcmp(jabber_attr(subnode->atts, "xmlns"), "jabber:x:data") && 
								!xstrcmp(jabber_attr(subnode->atts, "type"), "submit")) {
									/* var stream-method == http://jabber.org/protocol/bytestreams */
								jabber_handle_xmldata_submit(s, subnode->children, NULL, 0, "stream-method", &stream_method, NULL);
							}
						}
					}
				}
				if (!xstrcmp(stream_method, "http://jabber.org/protocol/bytestreams")) 	p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS; 
				else debug_error("[JABBER] JEP-0095: ERROR, stream_method XYZ error: %s\n", stream_method);
				xfree(stream_method);

				if (p->protocol == JABBER_DCC_PROTOCOL_BYTESTREAMS) {
					struct jabber_streamhost_item streamhost;
					jabber_dcc_bytestream_t *b;
					list_t l;

					b = p->private.bytestream = xmalloc(sizeof(jabber_dcc_bytestream_t));
					b->validate = JABBER_DCC_PROTOCOL_BYTESTREAMS;

					if (jabber_dcc_ip && jabber_dcc) {
						/* basic streamhost, our ip, default port, our jid. check if we enable it. XXX*/
						streamhost.jid	= saprintf("%s/%s", s->uid+4, j->resource);
						streamhost.ip	= xstrdup(jabber_dcc_ip);
						streamhost.port	= jabber_dcc_port;
						list_add(&(b->streamlist), &streamhost, sizeof(struct jabber_streamhost_item));
					}

/* 		... other, proxy, etc, etc..
					streamhost.ip = ....
					streamhost.port = ....
					list_add(...);
 */

					xfree(p->req);
					p->req = xstrdup(itoa(j->id++));

					watch_write(j->send_watch, "<iq type=\"set\" to=\"%s\" id=\"%s\">"
						"<query xmlns=\"http://jabber.org/protocol/bytestreams\" mode=\"tcp\" sid=\"%s\">", 
						d->uid+4, p->req, p->sid);

					for (l = b->streamlist; l; l = l->next) {
						struct jabber_streamhost_item *item = l->data;
						watch_write(j->send_watch, "<streamhost port=\"%d\" host=\"%s\" jid=\"%s\"/>", item->port, item->ip, item->jid);
					}
					watch_write(j->send_watch, "<fast xmlns=\"http://affinix.com/jabber/stream\"/></query></iq>");

				}
			} else /* XXX */;
		}

		if (type == JABBER_IQ_TYPE_SET && ((p = xmlnode_find_child(q, "file")))) {  /* JEP-0096: File Transfer */
			dcc_t *D;
			char *uin = jabber_unescape(from);
			char *uid;
			char *filename	= jabber_unescape(jabber_attr(p->atts, "name"));
			char *size 	= jabber_attr(p->atts, "size");
			xmlnode_t *range;
			jabber_dcc_t *jdcc;

			uid = saprintf("jid:%s", uin);

			jdcc = xmalloc(sizeof(jabber_dcc_t));
			jdcc->session	= s;
			jdcc->req 	= xstrdup(id);
			jdcc->sid	= jabber_unescape(jabber_attr(q->atts, "id"));
			jdcc->sfd	= -1;

			D = dcc_add(uid, DCC_GET, NULL);
			dcc_filename_set(D, filename);
			dcc_size_set(D, atoi(size));
			dcc_private_set(D, jdcc);
			dcc_close_handler_set(D, jabber_dcc_close_handler);
/* XXX, result
			if ((range = xmlnode_find_child(p, "range"))) {
				char *off = jabber_attr(range->atts, "offset");
				char *len = jabber_attr(range->atts, "length");
				if (off) dcc_offset_set(D, atoi(off));
				if (len) dcc_size_set(D, atoi(len));
			}
*/
			print("dcc_get_offer", format_user(s, uid), filename, size, itoa(dcc_id_get(D))); 

			xfree(uin);
			xfree(uid);
			xfree(filename);
		}
	}
#endif	/* FILETRANSFER */

	if ((q = xmlnode_find_child(n, "command"))) {
		const char *ns		= jabber_attr(q->atts, "xmlns");
		const char *node	= jabber_attr(q->atts, "node");

		char *uid 	= jabber_unescape(from);

		if (type == JABBER_IQ_TYPE_RESULT && !xstrcmp(ns, "http://jabber.org/protocol/commands")) {
			const char *status	= jabber_attr(q->atts, "status");

			if (!xstrcmp(status, "completed")) { 
				print("jabber_remotecontrols_completed", session_name(s), uid, node);
			} else if (!xstrcmp(status, "executing")) {
				const char *set  = node;
				xmlnode_t *child;

				if (!xstrncmp(node, "http://jabber.org/protocol/rc#", 30)) set = node+30;

				for (child = q->children; child; child = child->next) {
					if (	!xstrcmp(child->name, "x") && 
						!xstrcmp(jabber_attr(child->atts, "xmlns"), "jabber:x:data") && 
						!xstrcmp(jabber_attr(child->atts, "type"), "form")) {

						jabber_handle_xmldata_form(s, uid, "control", child->children, node);
					}
				}
			} else debug_error("[JABBER, UNKNOWN STATUS: %s\n", status);
		} else if (type == JABBER_IQ_TYPE_SET && !xstrcmp(ns, "http://jabber.org/protocol/commands") && xstrcmp(jabber_attr(q->atts, "action"), "cancel") /* XXX */) {
			int not_handled = 0;
			int not_allowed = 0;
			int is_valid = 0;
			xmlnode_t *x = xmlnode_find_child(q, "x");
			
//			int iscancel = !xstrcmp(jabber_attr(q->atts, "action"), "cancel");

			if (x) { 
				if (!xstrcmp(jabber_attr(x->atts, "xmlns"), "jabber:x:data") && !xstrcmp(jabber_attr(x->atts, "type"), "submit")); 
				else goto rc_invalid;
			} else { 
					print("jabber_remotecontrols_preparing", session_name(s), uid, node);
			}
			
			/* CHECK IF HE CAN DO IT */
			switch (session_int_get(s, "allow_remote_control"))  {
				case 666: break;
				case 1: if (!xstrncmp(from, s->uid+4, xstrlen(s->uid+4)) /* jesli poczatek jida sie zgadza. */
						&& from[xstrlen(s->uid+4)] == '/' /* i jesli na koncu tej osoby jida, gdzie sie konczy nasz zaczyna sie resource... */) 
						break; /* to jest raczej ok */
				case 2:	/* XXX */
				case 0:
				default:
					not_allowed = 1;
			}
			if (not_allowed) goto rc_forbidden;

#define EXECUTING_HEADER(title, instr, FORM_TYPE) \
	if (j->send_watch) j->send_watch->transfer_limit = -1;	/* read plugins.h for more details */ \
	watch_write(j->send_watch,\
		"<iq to=\"%s\" type=\"result\" id=\"%s\">"\
		"<command xmlns=\"http://jabber.org/protocol/commands\" node=\"%s\" status=\"executing\">"\
		"<x xmlns=\"jabber:x:data\" type=\"form\">"\
		"<title>%s</title>"\
		"<instructions>%s</instructions>"\
		"<field var=\"FORM_TYPE\" type=\"hidden\"><value>%s</value></field>", from, id, node, title, instr, FORM_TYPE)

#define EXECUTING_FIELD_BOOL(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"boolean\"><value>%d</value></field>", name, label, value)

#define EXECUTING_FIELD_STR_MULTI(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"text-multi\"><value>%s</value></field>", name, label, value)

#define EXECUTING_FIELD_STR(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"text-single\"><value>%s</value></field>", name, label, value)

#define EXECUTING_FIELD_STR_INT(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"text-single\"><value>%d</value></field>", name, label, value)

#define EXECUTING_SUBOPTION_STR(label, value) \
	watch_write(j->send_watch, "<option label=\"%s\"><value>%s</value></option>", label, value)

#define EXECUTING_FOOTER() watch_write(j->send_watch, "</x></command></iq>")

			if (!xstrcmp(node, "http://jabber.org/protocol/rc#set-status")) {
				if (!x) {
					char *status = s->status;
					char *descr = jabber_escape(s->descr);
					/* XXX, w/g http://jabber.org/protocol/rc#set-status nie mamy 'avail' tylko 'online' */

					if (!xstrcmp(status, EKG_STATUS_AUTOAWAY))	status = (s->autoaway ? "away" : "online");
					else if (!xstrcmp(status, EKG_STATUS_AVAIL))	status = "online";

					EXECUTING_HEADER("Set Status", "Choose the status and status message", "http://jabber.org/protocol/rc");
					watch_write(j->send_watch, "<field var=\"status\" label=\"Status\" type=\"list-single\"><required/>");
						EXECUTING_SUBOPTION_STR("Chat", EKG_STATUS_FREE_FOR_CHAT);
						EXECUTING_SUBOPTION_STR("Online", "online");			/* EKG_STATUS_AVAIL */
						EXECUTING_SUBOPTION_STR("Away", EKG_STATUS_AWAY);
						EXECUTING_SUBOPTION_STR("Extended Away", EKG_STATUS_XA);
						EXECUTING_SUBOPTION_STR("Do Not Disturb", EKG_STATUS_DND);
						EXECUTING_SUBOPTION_STR("Invisible", EKG_STATUS_INVISIBLE); 
						EXECUTING_SUBOPTION_STR("Offline", EKG_STATUS_NA);
						watch_write(j->send_watch, "<value>%s</value></field>", status);
					EXECUTING_FIELD_STR_INT("status-priority", "Priority", session_int_get(s, "priority"));
					EXECUTING_FIELD_STR_MULTI("status-message", "Message", descr ? descr : "");
					EXECUTING_FOOTER();
					xfree(descr);
				} else {
					char *status	= NULL;
					char *descr	= NULL;
					char *prio	= NULL;
					int priority	= session_int_get(s, "priority");

					is_valid = jabber_handle_xmldata_submit(s, x->children, "http://jabber.org/protocol/rc", 0,
							"status", &status, "status-priority", &prio, "status-message", 
							&descr, "status-priority", &prio, NULL);
					if (is_valid) {
						if (prio) priority = atoi(prio);
						if (!xstrcmp(status, "online")) { xfree(status); status = xstrdup(EKG_STATUS_AVAIL); } 

						if (status)	session_status_set(s, status);
						if (descr)	session_descr_set(s, descr);
						session_int_set(s, "priority", priority);
						print("jabber_remotecontrols_commited_status", session_name(s), uid, status, descr, itoa(priority));
						jabber_write_status(s);
					}
					xfree(status); xfree(descr);
				}
			} else if (!xstrcmp(node, "http://jabber.org/protocol/rc#set-options") || !xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-set-all-options")) {
				int alloptions = !xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-set-all-options");

				if (!x) {
					EXECUTING_HEADER("Set Options", "Set the desired options", alloptions ? "http://ekg2.org/jabber/rc" : "http://jabber.org/protocol/rc");
					if (alloptions) {
						/* send all ekg2 options? *g* :> */
						list_t l;
						for (l = variables; l; l = l->next) {
							variable_t *v = l->data;
							if (v->type == VAR_STR || v->type == VAR_FOREIGN || v->type == VAR_FILE || 
									v->type == VAR_DIR || v->type == VAR_THEME) 
							{
								char *value = jabber_unescape(*(char **) v->ptr);
								EXECUTING_FIELD_STR(v->name, v->name, value ? value : "");
								xfree(value);
							} else if (v->type == VAR_INT || v->type == VAR_BOOL) {
								EXECUTING_FIELD_BOOL(v->name, v->name, *(int *) v->ptr);
							} else { 	/* XXX */
								continue;
							}
						}
					} else {
						EXECUTING_FIELD_BOOL("sounds", "Play sounds", config_beep);
						EXECUTING_FIELD_BOOL("auto-offline", "Automatically Go Offline when Idle", (s->autoaway || s->status == EKG_STATUS_AUTOAWAY));
						EXECUTING_FIELD_BOOL("auto-msg", "Automatically Open New Messages", 0);		/* XXX */
						EXECUTING_FIELD_BOOL("auto-files", "Automatically Accept File Transfers", 0);	/* XXX */
						EXECUTING_FIELD_BOOL("auto-auth", "Automatically Authorize Contacts", 0);	/* XXX */
					}
					EXECUTING_FOOTER();
				} else {
					if (alloptions) {
						char **atts = NULL;
						is_valid = jabber_handle_xmldata_submit(s, x->children, "http://ekg2.org/jabber/rc", 1, &atts, NULL);
						if (is_valid && atts) {
							int i;
							debug_function("[VALID]\n");
							for (i=0; atts[i]; i+=2) {
								debug("[%d] atts: %s %s\n", i, atts[i], atts[i+1]);
								variable_set(atts[i], atts[i+1], 0);
							}
							print("jabber_remotecontrols_executed", session_name(s), uid, node);
						}
						array_free(atts);
					} else { 
						char *sounds 		= NULL;
						char *auto_offline	= NULL;
						char *auto_msg		= NULL;
						char *auto_files	= NULL;
						char *auto_auth		= NULL;
						is_valid = jabber_handle_xmldata_submit(s, x->children, "http://jabber.org/protocol/rc", 0, 
							"sounds", &sounds, "auto-offline", &auto_offline, "auto-msg", &auto_msg, "auto-files", &auto_files, 
							"auto-auth", &auto_auth, NULL);
						/* parse */
						debug_function("[JABBER, RC] sounds: %s [AUTO] off: %s msg: %s files: %s auth: %s\n", sounds, 
							auto_offline, auto_msg, auto_files, auto_auth);
						xfree(sounds); xfree(auto_offline); xfree(auto_msg); xfree(auto_files); xfree(auto_auth);
					} 
				}
			} else if (!xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-manage-plugins")) {
				if (!x) {
					EXECUTING_HEADER("Manage ekg2 plugins", "Do what you want :)", "http://ekg2.org/jabber/rc");
					EXECUTING_FOOTER();
				} else {
					not_handled = 1;
				}
			} else if (!xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-manage-sesions")) {
				if (!x) {
					EXECUTING_HEADER("Manage ekg2 sessions", "Do what you want :)", "http://ekg2.org/jabber/rc");
					EXECUTING_FOOTER();
				} else {
					not_handled = 1;
				}
			} else if (!xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-command-execute")) {
				if (!x) {
					list_t l;
					char *session_cur = jabber_escape(session_current->uid);

					EXECUTING_HEADER("EXECUTE COMMAND IN EKG2", "Do what you want, but be carefull", "http://ekg2.org/jabber/rc");
					watch_write(j->send_watch, 
						"<field label=\"Command name\" type=\"list-single\" var=\"command-name\">");	/* required */

					for (l = commands; l; l = l->next) {
						command_t *c = l->data;
						watch_write(j->send_watch, "<option label=\"%s\"><value>%s</value></option>", c->name, c->name);
					}
					watch_write(j->send_watch,
						"<value>help</value></field>"
						"<field label=\"Params:\" type=\"text-multi\" var=\"params-line\"></field>"
						"<field label=\"Session name (empty for current)\" type=\"list-single\" var=\"session-name\">");
	
					for (l = sessions; l; l = l->next) {
						session_t *s = l->data;
						char *alias	= jabber_escape(s->alias);
						char *uid	= jabber_escape(s->uid);
						watch_write(j->send_watch, "<option label=\"%s\"><value>%s</value></option>", alias ? alias : uid, uid);
						xfree(alias); xfree(uid);
					}
					watch_write(j->send_watch, 
						"<value>%s</value></field>"
						"<field label=\"Window name (leave empty for current)\" type=\"list-single\" var=\"window-name\">", session_cur);

					for (l = windows; l; l = l->next) {
						window_t *w = l->data;
						char *target 	= jabber_escape(window_target(w));
						watch_write(j->send_watch, "<option label=\"%s\"><value>%d</value></option>", target ? target : itoa(w->id), w->id);
						xfree(target);
					} 
					watch_write(j->send_watch, "<value>%d</value></field>", window_current->id);
					EXECUTING_FIELD_BOOL("command-quiet", "Quiet mode? (Won't display *results of command handler* on screen)", 0);
					EXECUTING_FOOTER();
					xfree(session_cur);
				} else {
					char *sessionname	= NULL;
					char *command		= NULL;
					char *params		= NULL;
					char *window		= NULL;
					char *quiet		= NULL;

					int windowid = 1, isquiet = 0;

					is_valid = jabber_handle_xmldata_submit(s, x->children, "http://ekg2.org/jabber/rc", 0, 
							"command-name", &command, "params-line", &params, "session-name", &sessionname,
							"window-name", &window, "command-quiet", &quiet, NULL);
					if (quiet)	isquiet  = atoi(quiet);
					if (window)	windowid = atoi(window);

					if (is_valid) { 
						char *fullcommand = saprintf("/%s %s", command, params ? params : "");
						window_t  *win;
						session_t *ses;
						int ret;

						ses	= session_find(sessionname);
						win	= window_exist(windowid);
	
						if (!ses) ses = session_current;
						if (!win) win = window_current;

						ret = command_exec(win->target, ses, fullcommand, isquiet);
						print("jabber_remotecontrols_commited_command", session_name(s), uid, fullcommand, session_name(ses), win->target, itoa(isquiet));
						xfree(fullcommand);
					}
					xfree(sessionname); xfree(params); xfree(command);
					xfree(window); xfree(quiet);
				}
			} else {
				debug_error("YOU WANT TO CONTROL ME with unknown node? (%s) Sorry I'm not fortune-teller\n", node);
				not_handled = 1;
			}
rc_invalid:
			if (x && !not_handled) {
				if (is_valid) {
					watch_write(j->send_watch, 
						"<iq to=\"%s\" type=\"result\" id=\"%s\">"
						"<command xmlns=\"http://jabber.org/protocol/commands\" node=\"%s\" status=\"completed\"/>"
						"</iq>", from, id, node);
				} else {
					debug_error("JABBER, REMOTECONTROL: He didn't send valid FORM_TYPE!!!! He's bu.\n");
					/* error <bad-request/> ? */
				}
			}
rc_forbidden:
			if (not_handled || not_allowed) {
				watch_write(j->send_watch, 
					"<iq to=\"%s\" type=\"error\" id=\"%s\">"
						"<error type=\"cancel\"><%s xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/></error>"
					"</iq>", from, id,
					not_allowed ? "forbidden" : "item-not-found");
			}
			JABBER_COMMIT_DATA(j->send_watch); /* let's write data to jabber socket */
		}
		xfree(uid);
	}

	/* XXX: temporary hack: roster przychodzi jako typ 'set' (przy dodawaniu), jak
	        i typ "result" (przy za��daniu rostera od serwera) */
	if (type == JABBER_IQ_TYPE_RESULT || type == JABBER_IQ_TYPE_SET) {
		xmlnode_t *q;

		/* First we check if there is vCard... */
		if ((q = xmlnode_find_child(n, "vCard"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrncmp(ns, "vcard-temp", 10)) {
				xmlnode_t *fullname = xmlnode_find_child(q, "FN");
				xmlnode_t *nickname = xmlnode_find_child(q, "NICKNAME");
				xmlnode_t *birthday = xmlnode_find_child(q, "BDAY");
				xmlnode_t *adr  = xmlnode_find_child(q, "ADR");
				xmlnode_t *city = xmlnode_find_child(adr, "LOCALITY");
				xmlnode_t *desc = xmlnode_find_child(q, "DESC");

				char *from_str     = (from)	? jabber_unescape(from) : NULL;
				char *fullname_str = (fullname) ? jabber_unescape(fullname->data) : NULL;
				char *nickname_str = (nickname) ? jabber_unescape(nickname->data) : NULL;
				char *bday_str     = (birthday) ? jabber_unescape(birthday->data) : NULL;
				char *city_str     = (city)	? jabber_unescape(city->data) : NULL;
				char *desc_str     = (desc)	? jabber_unescape(desc->data) : NULL;

				print("jabber_userinfo_response", 
						jabberfix(from_str, _("unknown")), 	jabberfix(fullname_str, _("unknown")),
						jabberfix(nickname_str, _("unknown")),	jabberfix(bday_str, _("unknown")),
						jabberfix(city_str, _("unknown")),	jabberfix(desc_str, _("unknown")));
				xfree(desc_str);
				xfree(city_str);
				xfree(bday_str);
				xfree(nickname_str);
				xfree(fullname_str);
				xfree(from_str);
			}
		}

		if ((q = xmlnode_find_child(n, "query"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");
#if WITH_JABBER_JINGLE
			if (!xstrcmp(ns, "session")) {
				/*   id == still the same through the session */
				/* type == [initiate, candidates, terminate] */

			} else 
#endif
			if (!xstrcmp(ns, "jabber:iq:privacy")) { /* RFC 3921 (10th: privacy) */
				xmlnode_t *active 	= xmlnode_find_child(q, "active");
				xmlnode_t *def		= xmlnode_find_child(q, "default");
				char *activename   	= active ? jabber_attr(active->atts, "name") : NULL;
				char *defaultname 	= def ? jabber_attr(def->atts, "name") : NULL;

				xmlnode_t *node;
				int i = 0;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "list")) {
						if (node->children) {	/* Display all details */
							list_t *lista = NULL;
							xmlnode_t *temp;
							i = -1;

							if (session_int_get(s, "auto_privacylist_sync") != 0) {
								const char *list = session_get(s, "privacy_list");
								if (!list) list = "ekg2";

								if (!xstrcmp(list, jabber_attr(node->atts, "name"))) {
									jabber_privacy_free(j);
									lista = &(j->privacy);
								}
							}

							print("jabber_privacy_item_header", session_name(s), j->server, jabber_attr(node->atts, "name"));

							for (temp=node->children; temp; temp = temp->next) {	
								if (!xstrcmp(temp->name, "item")) {
									/* (we should sort it by order! XXX) (server send like it was set) */
									/* a lot TODO */
									xmlnode_t *iq		= xmlnode_find_child(temp, "iq");
									xmlnode_t *message	= xmlnode_find_child(temp, "message");
									xmlnode_t *presencein	= xmlnode_find_child(temp, "presence-in");
									xmlnode_t *presenceout	= xmlnode_find_child(temp, "presence-out");

									char *value = jabber_attr(temp->atts, "value");
									char *type  = jabber_attr(temp->atts, "type");
									char *action = jabber_attr(temp->atts, "action");
									char *jid, *formated;
									char *formatedx;

									if (!xstrcmp(type, "jid")) 		jid = saprintf("jid:%s", value);
									else if (!xstrcmp(type, "group")) 	jid = saprintf("@%s", value);
									else					jid = xstrdup(value);

									formated = format_string(
										format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), jid);

									formatedx = format_string(
										format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), "*");

									xfree(formatedx);
									formatedx = xstrdup("x");

									if (lista) {
										jabber_iq_privacy_t *item = xmalloc(sizeof(jabber_iq_privacy_t));

										item->type  = xstrdup(type);
										item->value = xstrdup(value);
										item->allow = !xstrcmp(action,"allow");

										if (iq)		item->items |= PRIVACY_LIST_IQ;
										if (message)	item->items |= PRIVACY_LIST_MESSAGE;
										if (presencein)	item->items |= PRIVACY_LIST_PRESENCE_IN;
										if (presenceout)item->items |= PRIVACY_LIST_PRESENCE_OUT;
										item->order = atoi(jabber_attr(temp->atts, "order"));

										list_add_sorted(lista, item, 0, jabber_privacy_add_compare);
									}

									print("jabber_privacy_item", session_name(s), j->server, 
										jabber_attr(temp->atts, "order"), formated, 
										message		? formatedx : " ",
										presencein	? formatedx : " ", 
										presenceout	? formatedx : " ", 
										iq		? formatedx : " ");

									xfree(formated);
									xfree(formatedx);
									xfree(jid);
								}
							}
							{	/* just help... */
								char *allowed = format_string(format_find("jabber_privacy_item_allow"), "jid:allowed - JID ALLOWED");
								char *denied  = format_string(format_find("jabber_privacy_item_deny"), "@denied - GROUP DENIED");
								print("jabber_privacy_item_footer", session_name(s), j->server, allowed, denied);
								xfree(allowed); xfree(denied);
							}
						} else { 		/* Display only name */
							int dn = 0;
							/* rekurencja, ask for this item (?) */
							if (!i) print("jabber_privacy_list_begin", session_name(s), j->server);
							if (i != -1) i++;
							if (active && !xstrcmp(jabber_attr(node->atts, "name"), activename)) {
								print("jabber_privacy_list_item_act", session_name(s), j->server, itoa(i), activename); dn++; }
							if (def && !xstrcmp(jabber_attr(node->atts, "name"), defaultname)) {
								print("jabber_privacy_list_item_def", session_name(s), j->server, itoa(i), defaultname); dn++; }
							if (!dn) print("jabber_privacy_list_item", session_name(s), j->server, itoa(i), jabber_attr(node->atts, "name")); 
						}
					}
				}
				if (i > 0)  print("jabber_privacy_list_end", session_name(s), j->server);
				if (i == 0) print("jabber_privacy_list_noitem", session_name(s), j->server);

			} else if (!xstrcmp(ns, "jabber:iq:private")) {
				xmlnode_t *node;

				for (node = q->children; node; node = node->next) {
					char *lname	= jabber_unescape(node->name);
					char *ns	= jabber_attr(node->atts, "xmlns");
					xmlnode_t *child;

					int config_display = 1;
					int bookmark_display = 1;
					int quiet = 0;

					if (!xstrcmp(node->name, "ekg2")) {
						if (!xstrcmp(ns, "ekg2:prefs") && !xstrncmp(id, "config", 6)) 
							config_display = 0;	/* if it's /jid:config --get (not --display) we don't want to display it */ 
						/* XXX, other */
					}
					if (!xstrcmp(node->name, "storage")) {
						if (!xstrcmp(ns, "storage:bookmarks") && !xstrncmp(id, "config", 6))
							bookmark_display = 0;	/* if it's /jid:bookmark --get (not --display) we don't want to display it (/jid:bookmark --get performed @ connect) */
					}

					if (!config_display || !bookmark_display) quiet = 1;

					if (node->children)	printq("jabber_private_list_header", session_name(s), lname, ns);
					if (!xstrcmp(node->name, "ekg2") && !xstrcmp(ns, "ekg2:prefs")) { 	/* our private struct, containing `full` configuration of ekg2 */
						for (child = node->children; child; child = child->next) {
							char *cname	= jabber_unescape(child->name);
							char *cvalue	= jabber_unescape(child->data);
							if (!xstrcmp(child->name, "plugin") && !xstrcmp(jabber_attr(child->atts, "xmlns"), "ekg2:plugin")) {
								xmlnode_t *temp;
								printq("jabber_private_list_plugin", session_name(s), lname, ns, jabber_attr(child->atts, "name"), jabber_attr(child->atts, "prio"));
								for (temp = child->children; temp; temp = temp->next) {
									char *snname = jabber_unescape(temp->name);
									char *svalue = jabber_unescape(temp->data);
									printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
									xfree(snname);
									xfree(svalue);
								}
							} else if (!xstrcmp(child->name, "session") && !xstrcmp(jabber_attr(child->atts, "xmlns"), "ekg2:session")) {
								xmlnode_t *temp;
								printq("jabber_private_list_session", session_name(s), lname, ns, jabber_attr(child->atts, "uid"));
								for (temp = child->children; temp; temp = temp->next) {
									char *snname = jabber_unescape(temp->name);
									char *svalue = jabber_unescape(temp->data);
									printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
									xfree(snname);
									xfree(svalue);
								}
							} else	printq("jabber_private_list_item", session_name(s), lname, ns, cname, cvalue);
							xfree(cname); xfree(cvalue);
						}
					} else if (!xstrcmp(node->name, "storage") && !xstrcmp(ns, "storage:bookmarks")) { /* JEP-0048: Bookmark Storage */
						/* destroy previously-saved list */
						jabber_bookmarks_free(j);

						/* create new-one */
						for (child = node->children; child; child = child->next) {
							jabber_bookmark_t *book = xmalloc(sizeof(jabber_bookmark_t));

							debug_function("[JABBER:IQ:PRIVATE BOOKMARK item=%s\n", child->name);
							if (!xstrcmp(child->name, "conference")) {
								xmlnode_t *temp;

								book->type	= JABBER_BOOKMARK_CONFERENCE;
								book->private.conf		= xmalloc(sizeof(jabber_bookmark_conference_t));
								book->private.conf->name	= jabber_unescape(jabber_attr(child->atts, "name"));
								book->private.conf->jid		= jabber_unescape(jabber_attr(child->atts, "jid"));
								book->private.conf->autojoin	= !xstrcmp(jabber_attr(child->atts, "autojoin"), "true");

								book->private.conf->nick	= jabber_unescape( (temp = xmlnode_find_child(child, "nick")) ? temp->data : NULL);
								book->private.conf->pass        = jabber_unescape( (temp = xmlnode_find_child(child, "password")) ? temp->data : NULL);

								printq("jabber_bookmark_conf", session_name(s), book->private.conf->name, book->private.conf->jid,
									book->private.conf->autojoin ? "X" : " ", book->private.conf->nick, book->private.conf->pass);

							} else if (!xstrcmp(child->name, "url")) {
								book->type	= JABBER_BOOKMARK_URL;
								book->private.url	= xmalloc(sizeof(jabber_bookmark_url_t));
								book->private.url->name	= jabber_unescape(jabber_attr(child->atts, "name"));
								book->private.url->url	= jabber_unescape(jabber_attr(child->atts, "url"));

								printq("jabber_bookmark_url", session_name(s), book->private.url->name, book->private.url->url);

							} else { debug_error("[JABBER:IQ:PRIVATE:BOOKMARK UNKNOWNITEM=%s\n", child->name); xfree(book); book = NULL; }

							if (book) list_add(&j->bookmarks, book, 0);
						}
					} else {
						/* DISPLAY IT ? w jakim formacie?
						 * + CHILD item=value item2=value2
						 *  - SUBITEM .....
						 *  - SUBITEM........
						 *  + SUBCHILD ......
						 *   - SUBITEM
						 * ? XXX
						 */
					}
					if (node->children)	printq("jabber_private_list_footer", session_name(s), lname, ns);
					else 			printq("jabber_private_list_empty", session_name(s), lname, ns);
					xfree(lname);
				}
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/vacation")) { /* JEP-0109: Vacation Messages */
				xmlnode_t *temp;

				char *message	= jabber_unescape( (temp = xmlnode_find_child(q, "message")) ? temp->data : NULL);
				char *begin	= (temp = xmlnode_find_child(q, "start")) && temp->data ? temp->data : _("begin");
				char *end	= (temp = xmlnode_find_child(q, "end")) && temp->data  ? temp->data : _("never");

				print("jabber_vacation", session_name(s), message, begin, end);

				xfree(message);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				const char *wezel = jabber_attr(q->atts, "node");
				char *uid = jabber_unescape(from);
				xmlnode_t *node;

				print((wezel) ? "jabber_transinfo_begin_node" : "jabber_transinfo_begin", session_name(s), uid, wezel);

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "identity")) {
						char *name	= jabber_unescape(jabber_attr(node->atts, "name"));	/* nazwa */
//						char *cat	= jabber_attr(node->atts, "category");			/* server, gateway, directory */
//						char *type	= jabber_attr(node->atts, "type");			/* typ: im */
						
						if (name) /* jesli nie ma nazwy don't display it. */
							print("jabber_transinfo_identify" /* _server, _gateway... ? */, session_name(s), uid, name);

						xfree(name);
					} else if (!xstrcmp(node->name, "feature")) {
						char *var = jabber_attr(node->atts, "var");
						char *tvar = NULL; /* translated */
						int user_command = 0;

/* dj, jakas glupota... ale ma ktos pomysl zeby to inaczej zrobic?... jeszcze istnieje pytanie czy w ogole jest sens to robic.. */
						if (!xstrcmp(var, "http://jabber.org/protocol/disco#info")) 		tvar = "/jid:transpinfo";
						else if (!xstrcmp(var, "http://jabber.org/protocol/disco#items")) 	tvar = "/jid:transports";
						else if (!xstrcmp(var, "http://jabber.org/protocol/disco"))		tvar = "/jid:transpinfo && /jid:transports";
						else if (!xstrcmp(var, "http://jabber.org/protocol/muc"))		tvar = "/jid:mucpart && /jid:mucjoin";
						else if (!xstrcmp(var, "http://jabber.org/protocol/stats"))		tvar = "/jid:stats";
						else if (!xstrcmp(var, "jabber:iq:register"))		    		tvar = "/jid:register";
						else if (!xstrcmp(var, "jabber:iq:search"))				tvar = "/jid:search";

						else if (!xstrcmp(var, "http://jabber.org/protocol/bytestreams")) { user_command = 1; tvar = "/jid:dcc (PROT: BYTESTREAMS)"; }
						else if (!xstrcmp(var, "http://jabber.org/protocol/si/profile/file-transfer")) { user_command = 1; tvar = "/jid:dcc"; }
						else if (!xstrcmp(var, "http://jabber.org/protocol/commands")) { user_command = 1; tvar = "/jid:control"; } 
						else if (!xstrcmp(var, "jabber:iq:version"))	{ user_command = 1;	tvar = "/jid:ver"; }
						else if (!xstrcmp(var, "jabber:iq:last"))	{ user_command = 1;	tvar = "/jid:lastseen"; }
						else if (!xstrcmp(var, "vcard-temp"))		{ user_command = 1;	tvar = "/jid:change && /jid:userinfo"; }
						else if (!xstrcmp(var, "message"))		{ user_command = 1;	tvar = "/jid:msg"; }

						else if (!xstrcmp(var, "http://jabber.org/protocol/vacation"))	{ user_command = 2;	tvar = "/jid:vacation"; }
						else if (!xstrcmp(var, "presence-invisible"))	{ user_command = 2;	tvar = "/invisible"; } /* we ought use jabber:iq:privacy */
						else if (!xstrcmp(var, "jabber:iq:privacy"))	{ user_command = 2;	tvar = "/jid:privacy"; }
						else if (!xstrcmp(var, "jabber:iq:private"))	{ user_command = 2;	tvar = "/jid:private && /jid:config && /jid:bookmark"; }

						if (tvar)	print(	user_command == 2 ? "jabber_transinfo_comm_not" : 
									user_command == 1 ? "jabber_transinfo_comm_use" : "jabber_transinfo_comm_ser", 

									session_name(s), uid, tvar, var);
						else		print("jabber_transinfo_feature", session_name(s), uid, var, var);
					} else if (!xstrcmp(node->name, "x") && !xstrcmp(jabber_attr(node->atts, "xmlns"), "jabber:x:data") && !xstrcmp(jabber_attr(node->atts, "type"), "result")) {
						jabber_handle_xmldata_result(s, node->children, uid);
					}
				}
				print("jabber_transinfo_end", session_name(s), uid);
				xfree(uid);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				xmlnode_t *item = xmlnode_find_child(q, "item");
				char *uid = jabber_unescape(from);

				int iscontrol = !xstrncmp(id, "control", 7);

				if (item) {
					int i = 1;
					print(iscontrol ? "jabber_remotecontrols_list_begin" : "jabber_transport_list_begin", session_name(s), uid);
					for (; item; item = item->next, i++) {
						char *sjid  = jabber_unescape(jabber_attr(item->atts, "jid"));
						char *sdesc = jabber_unescape(jabber_attr(item->atts, "name"));
						char *snode = jabber_unescape(jabber_attr(item->atts, "node"));
						
						if (!iscontrol)	print(snode ? "jabber_transport_list_item_node" : "jabber_transport_list_item", 
									session_name(s), uid, sjid, snode, sdesc, itoa(i));
						else		print("jabber_remotecontrols_list_item", 
									session_name(s), uid, sjid, snode, sdesc, itoa(i));
						xfree(sdesc);
						xfree(sjid);
						xfree(snode);
					}
					print(iscontrol ? "jabber_remotecontrols_list_end"	: "jabber_transport_list_end", session_name(s), uid);
				} else	print(iscontrol ? "jabber_remotecontrols_list_nolist" : "jabber_transport_list_nolist", session_name(s), uid);
				xfree(uid);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/muc#owner")) {
				xmlnode_t *node;
				int formdone = 0;
				char *uid = jabber_unescape(from);

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(node->atts, "xmlns"))) {
						if (!xstrcmp(jabber_attr(node->atts, "type"), "form")) {
							formdone = 1;
							jabber_handle_xmldata_form(s, uid, "admin", node->children, NULL);
							break;
						} 
					}
				}
//				if (!formdone) ;	// XXX
				xfree(uid);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/muc#admin")) {
				xmlnode_t *node;
				int count = 0;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) {
						char *jid		= jabber_attr(node->atts, "jid");
//						char *aff		= jabber_attr(node->atts, "affiliation");
						xmlnode_t *reason	= xmlnode_find_child(node, "reason");
						char *rsn		= reason ? jabber_unescape(reason->data) : NULL;
						
						print("jabber_muc_banlist", session_name(s), from, jid, rsn ? rsn : "", itoa(++count));
						xfree(rsn);
					}
				}

			} else if (!xstrcmp(ns, "jabber:iq:search")) {
				xmlnode_t *node;
				int rescount = 0;
				char *uid = jabber_unescape(from);
				int formdone = 0;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) rescount++;
				}
				if (rescount > 1) print("jabber_search_begin", session_name(s), uid);

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) {
						xmlnode_t *tmp;
						char *jid 	= jabber_attr(node->atts, "jid");
						char *nickname	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "nick"))  ? tmp->data : NULL);
						char *fn	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "first")) ? tmp->data : NULL);
						char *lastname	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "last"))  ? tmp->data : NULL);
						char *email	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "email")) ? tmp->data : NULL);

						/* idea about displaink user in depend of number of users founded gathered from gg plugin */
						print(rescount > 1 ? "jabber_search_items" : "jabber_search_item", 
							session_name(s), uid, jid, nickname, fn, lastname, email);
						xfree(nickname);
						xfree(fn);
						xfree(lastname);
						xfree(email);
					} else {
						xmlnode_t *reg;
						if (rescount == 0) rescount = -1;
						if (formdone) continue;

						for (reg = q->children; reg; reg = reg->next) {
							if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(reg->atts, "xmlns"))) {
								if (!xstrcmp(jabber_attr(reg->atts, "type"), "form")) {
									formdone = 1;
									jabber_handle_xmldata_form(s, uid, "search", reg->children, "--jabber_x_data");
									break;
								} else if (!xstrcmp(jabber_attr(reg->atts, "type"), "result")) {
									formdone = 1;
									jabber_handle_xmldata_result(s, reg->children, uid);
									break;
								}
							}
						}

						if (!formdone) {
							/* XXX */
						}
					}
				}
				if (rescount > 1) print("jabber_search_end", session_name(s), uid);
				if (rescount == 0) print("search_not_found"); /* not found */
				xfree(uid);
			} else if (!xstrcmp(ns, "jabber:iq:register")) {
				xmlnode_t *reg;
				char *from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
				int done = 0;

				for (reg = q->children; reg; reg = reg->next) {
					if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(reg->atts, "xmlns")) && 
						( !xstrcmp("form", jabber_attr(reg->atts, "type")) || !jabber_attr(reg->atts, "type")))
					{
						done = 1;
						jabber_handle_xmldata_form(s, from_str, "register", reg->children, "--jabber_x_data");
						break;
					}
				}
				if (!done && !q->children) { 
					/* XXX */
					done = 1;
				}
				if (!done) {
					xmlnode_t *instr = xmlnode_find_child(q, "instructions");
					print("jabber_form_title", session_name(s), from_str, from_str);

					if (instr && instr->data) {
						char *instr_str = jabber_unescape(instr->data);
						print("jabber_form_instructions", session_name(s), from_str, instr_str);
						xfree(instr_str);
					}
					print("jabber_form_command", session_name(s), from_str, "register", "");

					for (reg = q->children; reg; reg = reg->next) {
						char *jname, *jdata;
						if (!xstrcmp(reg->name, "instructions")) continue;

						jname = jabber_unescape(reg->name);
						jdata = jabber_unescape(reg->data);
						print("jabber_registration_item", session_name(s), from_str, jname, jdata);
						xfree(jname);
						xfree(jdata);
					}
					print("jabber_form_end", session_name(s), from_str, "register");
				}
				xfree(from_str);
#if WITH_JABBER_DCC
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/bytestreams")) { /* JEP-0065: SOCKS5 Bytestreams */
				char *uid = jabber_unescape(from);		/* jid */
				char *sid = jabber_attr(q->atts, "sid");	/* session id */
				char *smode = jabber_attr(q->atts, "mode"); 	/* tcp, udp */
				dcc_t *d = NULL;

				if (type == JABBER_IQ_TYPE_SET && (d = jabber_dcc_find(uid, NULL, sid))) {
					/* w sumie jak nie mamy nawet tego dcc.. to mozemy kontynuowac ;) */
					/* problem w tym czy user chce ten plik.. etc.. */
					/* i tak to na razie jest jeden wielki hack, trzeba sprawdzac czy to dobry typ dcc. etc, XXX */
					xmlnode_t *node;
					jabber_dcc_t *p = d->priv;
					jabber_dcc_bytestream_t *b = NULL;

					list_t host_list = NULL, l;
					struct jabber_streamhost_item *streamhost;

					if (d->type == DCC_SEND) {
						watch_write(j->send_watch, 
							"<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"406\">Declined</error></iq>", d->uid+4, id);
						return;
					}

					p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS;

					xfree(p->req);
					p->req = xstrdup(id);
		/* XXX, set our streamhost && send them too */
					for (node = q->children; node; node = node->next) {
						if (!xstrcmp(node->name, "streamhost")) {
							struct jabber_streamhost_item *newstreamhost = xmalloc(sizeof(struct jabber_streamhost_item));

							newstreamhost->ip	= xstrdup(jabber_attr(node->atts, "host"));	/* XXX in host can be hostname */
							newstreamhost->port	= atoi(jabber_attr(node->atts, "port"));
							newstreamhost->jid	= xstrdup(jabber_attr(node->atts, "jid"));
							list_add(&host_list, newstreamhost, 0);
						}
					}
					l = host_list;
find_streamhost:
					streamhost = NULL;
					for (; l; l = l->next) {
						struct jabber_streamhost_item *item = l->data;
						struct sockaddr_in sin;
						/* let's search the list for ipv4 address... for now only this we can handle */
						if ((inet_pton(AF_INET, item->ip, &(sin.sin_addr)) > 0)) {
							streamhost = host_list->data;
							break;
						}
					}

					if (streamhost) {
						struct sockaddr_in sin;
						int fd;
						char socks5[4];

						fd = socket(AF_INET, SOCK_STREAM, 0);

						sin.sin_family = AF_INET;
						sin.sin_port	= htons(streamhost->port);
						inet_pton(AF_INET, streamhost->ip, &(sin.sin_addr));

						if (connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) == -1) {
							/* let's try connect once more to another host? */
							debug_error("[jabber] dcc connecting to: %s failed (%s)\n", streamhost->ip, strerror(errno));
							goto find_streamhost;
						}
						p->sfd = fd;

						watch_add(&jabber_plugin, fd, WATCH_READ, jabber_dcc_handle_recv, d);
						
						p->private.bytestream = b = xmalloc(sizeof(jabber_dcc_bytestream_t));
						b->validate	= JABBER_DCC_PROTOCOL_BYTESTREAMS;
						b->step		= SOCKS5_CONNECT;
						b->streamlist	= host_list;
						b->streamhost	= streamhost;

						socks5[0] = 0x05;	/* socks version 5 */
						socks5[1] = 0x02;	/* number of methods */
						socks5[2] = 0x00;	/* no auth */
						socks5[3] = 0x02;	/* username */
						write(fd, (char *) &socks5, sizeof(socks5));
					} else {
						list_t l;

						debug_error("[jabber] We cannot connect to any streamhost with ipv4 address.. sorry, closing connection.\n");

						for (l = host_list; l; l = l->next) {
							struct jabber_streamhost_item *i = l->data;

							xfree(i->jid);
							xfree(i->ip);
						}
						list_destroy(host_list, 1);

						watch_write(j->send_watch, 
							"<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"404\" type=\"cancel\">"
							/* Psi: <error code='404'>Could not connect to given hosts</error> */
								"<item-not-found xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
							"</error></iq>", d->uid+4, id);

						print("dcc_error_refused", format_user(s, d->uid));

						d->active = 1;		/* hack to avoid sending 403 */
						dcc_close(d);		/* zamykamy dcc */
					}
				} else if (type == JABBER_IQ_TYPE_RESULT) {
					xmlnode_t *used = xmlnode_find_child(q, "streamhost-used");
					jabber_dcc_t *p;
					jabber_dcc_bytestream_t *b;
					list_t l;

					if ((d = jabber_dcc_find(uid, id, NULL))) {
						char *usedjid = (used) ? jabber_attr(used->atts, "jid") : NULL;
						watch_t *w;

						p = d->priv;
						b = p->private.bytestream;

						for (l = b->streamlist; l; l = l->next) {
							struct jabber_streamhost_item *item = l->data;
							if (!xstrcmp(item->jid, usedjid)) {
								b->streamhost = item;
							}
						}
						debug_function("[STREAMHOST-USED] stream: 0x%x\n", b->streamhost);
						d->active	= 1;

						w = watch_find(&jabber_plugin, p->sfd, WATCH_NONE);

						if (w && /* w->handler == jabber_dcc_handle_send && */ w->data == d)
							w->type = WATCH_WRITE;
						else {
							debug_error("[jabber] %s:%d WATCH BAD DATA/NOT FOUND, 0x%x 0x%x 0x%x\n", __FILE__, __LINE__, w, w ? w->handler : NULL, w ? w->data : NULL);
							/* XXX, SHOW ERROR ON __STATUS */
							dcc_close(d);
							return;
						}

						{	/* activate stream */
							char buf[1];
							buf[0] = '\r';
							write(p->sfd, &buf[0], 1);
						}
					}
				}
				debug_function("[FILE - BYTESTREAMS] 0x%x\n", d);
#endif
			} else if (!xstrncmp(ns, "jabber:iq:version", 17)) {
				xmlnode_t *name = xmlnode_find_child(q, "name");
				xmlnode_t *version = xmlnode_find_child(q, "version");
				xmlnode_t *os = xmlnode_find_child(q, "os");

				char *from_str	= (from) ?	jabber_unescape(from) : NULL;
				char *name_str	= (name) ?	jabber_unescape(name->data) : NULL;
				char *version_str = (version) ? jabber_unescape(version->data) : NULL;
				char *os_str	= (os) ?	jabber_unescape(os->data) : NULL;

				print("jabber_version_response",
						jabberfix(from_str, "unknown"), jabberfix(name_str, "unknown"), 
						jabberfix(version_str, "unknown"), jabberfix(os_str, "unknown"));
				xfree(os_str);
				xfree(version_str);
				xfree(name_str);
				xfree(from_str);
			} else if (!xstrncmp(ns, "jabber:iq:last", 14)) {
				const char *last = jabber_attr(q->atts, "seconds");
				int seconds = 0;
				char buff[21];
				char *from_str, *lastseen_str;

				seconds = atoi(last);

				if ((seconds>=0) && (seconds < 999 * 24 * 60 * 60  - 1) )
					/* days, hours, minutes, seconds... */
					snprintf (buff, 21, _("%03dd %02dh %02dm %02ds"),seconds / 86400 , \
						(seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60);
				else
					strcpy (buff, _("very long"));

				from_str = (from) ? jabber_unescape(from) : NULL;
				lastseen_str = xstrdup(buff);
				print( (xstrchr(from_str, '/') ? "jabber_lastseen_idle" :	/* if we have resource than display -> user online+idle		*/
					xstrchr(from_str, '@') ? "jabber_lastseen_response" :	/* if we have '@' in jid: than display ->  user logged out	*/
								 "jabber_lastseen_uptime"),	/* otherwise we have server -> server up for: 			*/
					jabberfix(from_str, "unknown"), lastseen_str);
				xfree(lastseen_str);
				xfree(from_str);
			} else if (!xstrncmp(ns, "jabber:iq:roster", 16)) {
				xmlnode_t *item = xmlnode_find_child(q, "item");

				for (; item ; item = item->next) {
					userlist_t *u;
					char *uid;
					if (j->istlen)	uid = saprintf("tlen:%s", jabber_attr(item->atts, "jid"));
					else 		uid = saprintf("jid:%s",jabber_attr(item->atts, "jid"));

					/* je�li element rostera ma subscription = remove to tak naprawde u�ytkownik jest usuwany;
					w przeciwnym wypadku - nalezy go dopisa� do userlisty; dodatkowo, jesli uzytkownika
					mamy ju� w liscie, to przyszla do nas zmiana rostera; usunmy wiec najpierw, a potem
					sprawdzmy, czy warto dodawac :) */
					if (jdh->roster_retrieved && (u = userlist_find(s, uid)) )
						userlist_remove(s, u);

					if (!xstrncmp(jabber_attr(item->atts, "subscription"), "remove", 6)) {
						/* nic nie robimy, bo juz usuniete */
					} else {
						char *nickname 	= jabber_unescape(jabber_attr(item->atts, "name"));
						xmlnode_t *group = xmlnode_find_child(item,"group");
						/* czemu sluzy dodanie usera z nickname uid jesli nie ma nickname ? */
						u = userlist_add(s, uid, nickname ? nickname : uid); 

						if (jabber_attr(item->atts, "subscription"))
							u->authtype = xstrdup(jabber_attr(item->atts, "subscription"));
						
						for (; group ; group = group->next ) {
							char *gname = jabber_unescape(group->data);
							ekg_group_add(u, gname);
							xfree(gname);
						}

						if (jdh->roster_retrieved) {
							command_exec_format(NULL, s, 1, ("/auth --probe %s"), uid);
						}
						xfree(nickname); 
					}
					xfree(uid);
				}; /* for */
				jdh->roster_retrieved = 1;
			} /* jabber:iq:roster */
		} /* if query */
	} /* type == set */

	if (type == JABBER_IQ_TYPE_GET) {
		xmlnode_t *q;

		if ((q = xmlnode_find_child(n, "query"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				if (!xstrcmp(jabber_attr(q->atts, "node"), "http://jabber.org/protocol/commands")) {
					/* XXX, check if $uid can do it */
					watch_write(j->send_watch, 
						"<iq to=\"%s\" type=\"result\" id=\"%s\">"
						"<query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"http://jabber.org/protocol/commands\">"
						"<item jid=\"%s/%s\" name=\"Set Status\" node=\"http://jabber.org/protocol/rc#set-status\"/>"
						"<item jid=\"%s/%s\" name=\"Forward Messages\" node=\"http://jabber.org/protocol/rc#forward\"/>"
						"<item jid=\"%s/%s\" name=\"Set Options\" node=\"http://jabber.org/protocol/rc#set-options\"/>"
						"<item jid=\"%s/%s\" name=\"Set ALL ekg2 Options\" node=\"http://ekg2.org/jabber/rc#ekg-set-all-options\"/>"
						"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-plugins\"/>"
						"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-sessions\"/>"
						"<item jid=\"%s/%s\" name=\"Execute ANY command in ekg2\" node=\"http://ekg2.org/jabber/rc#ekg-command-execute\"/>"
						"</query></iq>", from, id, 
						s->uid+4, j->resource, s->uid+4, j->resource, 
						s->uid+4, j->resource, s->uid+4, j->resource,
						s->uid+4, j->resource, s->uid+4, j->resource,
						s->uid+4, j->resource);
				}
			}
			else if (!xstrcmp(ns, "jabber:iq:last")) {
				/* XXX, buggy cause i think we don't want to do s->activity = time(NULL) in stream handler... just only when we type command or smth? */
				watch_write(j->send_watch, 
					"<iq to=\"%s\" type=\"result\" id=\"%s\">"
					"<query xmlns=\"jabber:iq:last\" seconds=\"%d\">"
					"</query></iq>", from, id, (time(NULL)-s->activity));

			} else if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">"
					"<query xmlns=\"http://jabber.org/protocol/disco#info\">"
					"<feature var=\"http://jabber.org/protocol/commands\"/>"
#if WITH_JABBER_DCC
					"<feature var=\"http://jabber.org/protocol/bytestreams\"/>"
					"<feature var=\"http://jabber.org/protocol/si\"/>"
					"<feature var=\"http://jabber.org/protocol/si/profile/file-transfer\"/>"
#endif
					"</query></iq>", from, id);

			} else if (!xstrncmp(ns, "jabber:iq:version", 17) && id && from) {
				const char *ver_os;
				const char *tmp;

				char *escaped_client_name	= jabber_escape( jabberfix((tmp = session_get(s, "ver_client_name")), DEFAULT_CLIENT_NAME) );
				char *escaped_client_version	= jabber_escape( jabberfix((tmp = session_get(s, "ver_client_version")), VERSION) );
				char *osversion;

				if (!(ver_os = session_get(s, "ver_os"))) {
					struct utsname buf;

					if (uname(&buf) != -1) {
						char *osver = saprintf("%s %s %s", buf.sysname, buf.release, buf.machine);
						osversion = jabber_escape(osver);
						xfree(osver);
					} else {
						osversion = xstrdup(("unknown")); /* uname failed and not ver_os session variable */
					}
				} else {
					osversion = jabber_escape(ver_os);	/* ver_os session variable */
				}

				watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">" 
						"<query xmlns=\"jabber:iq:version\">"
						"<name>%s</name>"
						"<version>%s</version>"
						"<os>%s</os></query></iq>", 
						from, id, 
						escaped_client_name, escaped_client_version, osversion);

				xfree(escaped_client_name);
				xfree(escaped_client_version);
				xfree(osversion);
			} /* jabber:iq:version */
		} /* if query */
	} /* type == get */
} /* iq */

typedef struct {
	char *role;		/* role: */
	char *aff;		/* affiliation: */
} muc_userlist_t;

static inline muc_userlist_t *mucuser_private_get(userlist_t *u) {
	muc_userlist_t *p;
	if (!u) 	return NULL;
	if (u->priv)	return u->priv;

	p = xmalloc(sizeof(muc_userlist_t));
	u->priv = p;
	return p;
}

static inline void mucuser_private_deinit(userlist_t *u) {
	muc_userlist_t *p;
	if (!u || !(p = u->priv)) return;

	xfree(p->role);
	xfree(p->aff);
}

static void jabber_handle_presence(xmlnode_t *n, session_t *s) {
	jabber_private_t *j = jabber_private(s);
	const char *from = jabber_attr(n->atts, "from");
	const char *type = jabber_attr(n->atts, "type");
	char *jid, *uid;
	time_t when = 0;
	xmlnode_t *q;
	int ismuc = 0;
	int istlen = j ? j->istlen : 0;

	int na = !xstrcmp(type, "unavailable");

	jid = jabber_unescape(from);

	if (istlen)	uid = saprintf("tlen:%s", jid);
	else		uid = saprintf("jid:%s", jid);

	xfree(jid);

	if (from && !xstrcmp(type, "subscribe")) {
		print("jabber_auth_subscribe", uid, session_name(s));
		xfree(uid);
		return;
	}

	if (from && !xstrcmp(type, "unsubscribe")) {
		print("jabber_auth_unsubscribe", uid, session_name(s));
		xfree(uid);
		return;
	}

	for (q = n->children; q; q = q->next) {
		char *tmp	= xstrchr(uid, '/');
		char *mucuid	= xstrndup(uid, tmp ? tmp - uid : xstrlen(uid));
		char *ns	= jabber_attr(q->atts, "xmlns");

		if (!xstrcmp(q->name, "x")) {
			if (!xstrcmp(ns, "http://jabber.org/protocol/muc#user")) {
				xmlnode_t *child;

				for (child = q->children; child; child = child->next) {
					if (!xstrcmp(child->name, "status")) {		/* status codes, --- http://www.jabber.org/jeps/jep-0045.html#registrar-statuscodes */
						char *code = jabber_attr(child->atts, "code");
						int codenr = code ? atoi(code) : -1;

						switch (codenr) {
							case 201: print_window(mucuid, s, 0, "jabber_muc_room_created", session_name(s), mucuid);	break;
							case  -1: debug("[jabber, iq, muc#user] codenr: -1 code: %s\n", code);				break;
							default : debug("[jabber, iq, muc#user] XXX codenr: %d code: %s\n", codenr, code);
						}
					} else if (!xstrcmp(child->name, "item")) { /* lista userow */
						char *jid	  = jabber_unescape(jabber_attr(child->atts, "jid"));		/* jid */
						char *role	  = jabber_unescape(jabber_attr(child->atts, "role"));		/* ? */
						char *affiliation = jabber_unescape(jabber_attr(child->atts, "affiliation"));	/* ? */
						char *nickjid	  = NULL;

						newconference_t *c;
						userlist_t *ulist;

						if (!(c = newconference_find(s, mucuid))) {
							debug("[jabber,muc] recved muc#user but conference: %s not found ?\n", mucuid);
							xfree(jid); xfree(role); xfree(affiliation);
							break;
						}
						if (tmp) nickjid = saprintf("jid:%s", tmp + 1);
						else	 nickjid = xstrdup(uid);

						if (na) 	print_window(mucuid, s, 0, "muc_left", session_name(s), nickjid + 4, jid, mucuid+4, "");

						ulist = newconference_member_find(c, nickjid);
						if (ulist && na) { 
							mucuser_private_deinit(ulist); 
							newconference_member_remove(c, ulist); 
							ulist = NULL; 
						} else if (!ulist) {
							ulist = newconference_member_add(c, nickjid, nickjid + 4);
							print_window(mucuid, s, 0, "muc_joined", session_name(s), nickjid + 4, jid, mucuid+4, "", role, affiliation);
						}

						if (ulist) {
							char *tmp = ulist->status;
							ulist->status = xstrdup(EKG_STATUS_AVAIL);
							xfree(tmp);
							
							mucuser_private_deinit(ulist);
							mucuser_private_get(ulist)->role	= xstrdup(role);
							mucuser_private_get(ulist)->aff		= xstrdup(affiliation);
						}
						debug("[MUC, PRESENCE] NEWITEM: %s (%s) ROLE:%s AFF:%s\n", nickjid, __(jid), role, affiliation);
						xfree(nickjid);
						xfree(jid); xfree(role); xfree(affiliation);
					} else {
						debug_error("[MUC, PRESENCE] wtf? child->name: %s\n", child->name);
					}
				}
				ismuc = 1;
			} else if (!xstrcmp(ns, "jabber:x:signed")) {	/* JEP-0027 */
				char *x_signed	= xstrdup(q->data);
				char *x_status	= NULL;
				char *x_key;

				x_key = jabber_openpgp(s, mucuid, JABBER_OPENGPG_VERIFY, "" /* STATUS opisowy */, x_signed, &x_status);
				/* @ x_key KEY, x_status STATUS of verification */
				debug("jabber_openpgp() %s %s\n", __(x_key), __(x_status));
				xfree(x_key);
				xfree(x_status);
			} else if (!xstrncmp(ns, "jabber:x:delay", 14)) {
				when = jabber_try_xdelay(jabber_attr(q->atts, "stamp"));
			} else debug("[JABBER, PRESENCE]: <x xmlns=%s\n", ns);
		}		/* <x> */
		xfree(mucuid);
	}
	if (!ismuc && (!type || ( na || !xstrcmp(type, "error") || !xstrcmp(type, "available")))) {
		xmlnode_t *nshow, *nstatus, *nerr, *temp;
		char *status = NULL, *descr = NULL;
		char *jstatus = NULL;
		char *tmp2;

		int prio = (temp = xmlnode_find_child(n, "priority")) ? atoi(temp->data) : 10;

		if ((nshow = xmlnode_find_child(n, "show"))) {	/* typ */
			jstatus = jabber_unescape(nshow->data);
			if (!xstrcmp(jstatus, "na") || na) {
				status = xstrdup(EKG_STATUS_NA);
				na = 1;
			}
		} else {
			if (na)
				status = xstrdup(EKG_STATUS_NA);
			else	status = xstrdup(EKG_STATUS_AVAIL);
		}

		if ((nerr = xmlnode_find_child(n, "error"))) { /* bledny */
			char *ecode = jabber_attr(nerr->atts, "code");
			char *etext = jabber_unescape(nerr->data);
			descr = saprintf("(%s) %s", ecode, etext);
			xfree(etext);

			xfree(status);
			status = xstrdup(EKG_STATUS_ERROR);
		} 
		if ((nstatus = xmlnode_find_child(n, "status"))) { /* opisowy */
			xfree(descr);
			descr = jabber_unescape(nstatus->data);
		}

		if (status) {
			xfree(jstatus);
		} else if (jstatus && (!xstrcasecmp(jstatus, EKG_STATUS_AWAY)		|| !xstrcasecmp(jstatus, EKG_STATUS_INVISIBLE)	||
					!xstrcasecmp(jstatus, EKG_STATUS_XA)		|| !xstrcasecmp(jstatus, EKG_STATUS_DND) 	|| 
					!xstrcasecmp(jstatus, EKG_STATUS_FREE_FOR_CHAT) || !xstrcasecmp(jstatus, EKG_STATUS_BLOCKED))) {
			status = jstatus;
		} else if (istlen && !xstrcmp(jstatus, "available")) {
			status = xstrdup(EKG_STATUS_AVAIL);
		} else {
			debug_error("[jabber] Unknown presence: %s from %s. Please report!\n", jstatus, uid);
			xfree(jstatus);
			status = xstrdup(EKG_STATUS_AVAIL);
		}

		if ((tmp2 = xstrchr(uid, '/'))) {
			userlist_t *ut;
	
			if ((ut = userlist_find(s, uid))) {
				ekg_resource_t *r;

				if ((r = userlist_resource_find(ut, tmp2+1))) {
					if (na) {				/* if resource went offline remove... */
						userlist_resource_remove(ut, r);
						r = NULL;
					}
				} else r = userlist_resource_add(ut, tmp2+1, prio);

				if (r && r->prio != prio) {
					/* XXX, here resort, stupido */
					r->prio = prio;
				}
			}
		}
		{
			char *session 	= xstrdup(session_uid_get(s));
			char *host 	= NULL;
			int port 	= 0;

			if (!when) when = time(NULL);
			query_emit_id(NULL, PROTOCOL_STATUS, &session, &uid, &status, &descr, &host, &port, &when, NULL);
			
			xfree(session);
/*			xfree(host); */
		}
		xfree(status);
		xfree(descr);
	}
	xfree(uid);
} /* <presence> */

static void jabber_session_connected(session_t *s, jabber_handler_data_t *jdh) {
	jabber_private_t *j = jabber_private(s);
	char *__session = xstrdup(session_uid_get(s));

	session_connected_set(s, 1);
	session_unidle(s);
	j->connecting = 0;
	s->last_conn = time(NULL);

	query_emit_id(NULL, PROTOCOL_CONNECTED, &__session);

	if (session_get(s, "__new_acount")) {
		print("register", __session);
		if (!xstrcmp(session_get(s, "password"), "foo")) print("register_change_passwd", __session, "foo");
		session_set(s, "__new_acount", NULL);
	}

	jdh->roster_retrieved = 0;
	userlist_free(s);
	watch_write(j->send_watch, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
	jabber_write_status(s);

	if (session_int_get(s, "auto_bookmark_sync") != 0) command_exec(NULL, s, ("/jid:bookmark --get"), 1);
	if (session_int_get(s, "auto_privacylist_sync") != 0) {
		const char *list = session_get(s, "privacy_list");

		if (!list) list = "ekg2";
		command_exec_format(NULL, s, 1, ("/jid:privacy --get %s"), 	list);	/* synchronize list */
		command_exec_format(NULL, s, 1, ("/jid:privacy --session %s"), 	list); 	/* set as active */
	}
#if GMAIL_MAIL_NOTIFY
	if (!xstrcmp(j->server, "gmail.com")) {
		watch_write(j->send_watch,
			"<iq type=\"set\" to=\"%s\" id=\"gmail%d\"><usersetting xmlns=\"google:setting\"><mailnotifications value=\"true\"/></usersetting></iq>",
			s->uid+4, j->id++);

		watch_write(j->send_watch,
			"<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>",
			j->id++);
	}
#endif
	xfree(__session);
}

static void newmail_common(session_t *s) { /* maybe inline? */
	if (config_sound_mail_file) 
		play_sound(config_sound_mail_file);
	else if (config_jabber_beep_mail)
		query_emit_id(NULL, UI_BEEP, NULL);
	/* XXX, we NEED to connect to MAIL_COUNT && display info about mail like mail plugin do. */
	/* XXX, emit events */
}

static time_t jabber_try_xdelay(const char *stamp) {
	/* try to parse timestamp */
	if (stamp) {
        	struct tm tm;
       	        memset(&tm, 0, sizeof(tm));
               	sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d",
                        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       	&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
       	        tm.tm_year -= 1900;
               	tm.tm_mon -= 1;
                return mktime(&tm);
        }
	return time(NULL);

}

