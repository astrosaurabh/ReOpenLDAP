/* $ReOpenLDAP$ */
/* Copyright (c) 2015,2016 Leonid Yuriev <leo@yuriev.ru>.
 * Copyright (c) 2015,2016 Peter-Service R&D LLC <http://billing.ru/>.
 *
 * This file is part of ReOpenLDAP.
 *
 * ReOpenLDAP is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * ReOpenLDAP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Copyright 1998-2014 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1995 Regents of the University of Michigan.
 * All rights reserved.
 */

#include "reldap.h"

#include <stdio.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <ac/stdlib.h>

#include <ac/param.h>
#include <ac/socket.h>
#include <ac/string.h>
#include <ac/time.h>

#include <ac/unistd.h>

#include "ldap-int.h"
#include "ldap_log.h"

/* Caller must hold the conn_mutex since simultaneous accesses are possible */
int ldap_open_defconn( LDAP *ld )
{
	ld->ld_defconn = ldap_new_connection( ld,
		&ld->ld_options.ldo_defludp, 1, 1, NULL, 0, 0 );

	if( ld->ld_defconn == NULL ) {
		ld->ld_errno = LDAP_SERVER_DOWN;
		return -1;
	}

	++ld->ld_defconn->lconn_refcnt;	/* so it never gets closed/freed */
	return 0;
}

int
ldap_create( LDAP **ldp )
{
	LDAP			*ld;
	struct ldapoptions	*gopts;

	*ldp = NULL;
	/* Get pointer to global option structure */
	if ( (gopts = LDAP_INT_GLOBAL_OPT()) == NULL) {
		return LDAP_NO_MEMORY;
	}

	/* Initialize the global options, if not already done. */
	if( gopts->ldo_valid != LDAP_INITIALIZED ) {
		ldap_int_initialize(gopts, NULL);
		if ( gopts->ldo_valid != LDAP_INITIALIZED )
			return LDAP_LOCAL_ERROR;
	}

	Debug( LDAP_DEBUG_TRACE, "ldap_create\n" );

	if ( (ld = (LDAP *) LDAP_CALLOC( 1, sizeof(LDAP) )) == NULL ) {
		return( LDAP_NO_MEMORY );
	}

	if ( (ld->ldc = (struct ldap_common *) LDAP_CALLOC( 1,
			sizeof(struct ldap_common) )) == NULL ) {
		LDAP_FREE( (char *)ld );
		return( LDAP_NO_MEMORY );
	}
	/* copy the global options */
	LDAP_MUTEX_LOCK( &gopts->ldo_mutex );
	memcpy(&ld->ld_options, gopts, sizeof(ld->ld_options));
#ifdef LDAP_R_COMPILE
	/* Properly initialize the structs mutex */
	ldap_pvt_thread_mutex_init( &(ld->ld_ldopts_mutex) );
#endif
	LDAP_MUTEX_UNLOCK( &gopts->ldo_mutex );

	ld->ld_valid = LDAP_VALID_SESSION;

	/* but not pointers to malloc'ed items */
	ld->ld_options.ldo_sctrls = NULL;
	ld->ld_options.ldo_cctrls = NULL;
	ld->ld_options.ldo_defludp = NULL;
	ld->ld_options.ldo_conn_cbs = NULL;

#ifdef HAVE_CYRUS_SASL
	ld->ld_options.ldo_def_sasl_mech = gopts->ldo_def_sasl_mech
		? LDAP_STRDUP( gopts->ldo_def_sasl_mech ) : NULL;
	ld->ld_options.ldo_def_sasl_realm = gopts->ldo_def_sasl_realm
		? LDAP_STRDUP( gopts->ldo_def_sasl_realm ) : NULL;
	ld->ld_options.ldo_def_sasl_authcid = gopts->ldo_def_sasl_authcid
		? LDAP_STRDUP( gopts->ldo_def_sasl_authcid ) : NULL;
	ld->ld_options.ldo_def_sasl_authzid = gopts->ldo_def_sasl_authzid
		? LDAP_STRDUP( gopts->ldo_def_sasl_authzid ) : NULL;
#endif

#ifdef HAVE_TLS
	/* We explicitly inherit the SSL_CTX, don't need the names/paths. Leave
	 * them empty to allow new SSL_CTX's to be created from scratch.
	 */
	memset( &ld->ld_options.ldo_tls_info, 0,
		sizeof( ld->ld_options.ldo_tls_info ));
	ld->ld_options.ldo_tls_ctx = NULL;
#endif

	if ( gopts->ldo_defludp ) {
		ld->ld_options.ldo_defludp = ldap_url_duplist(gopts->ldo_defludp);

		if ( ld->ld_options.ldo_defludp == NULL ) goto nomem;
	}

	if (( ld->ld_selectinfo = ldap_new_select_info()) == NULL ) goto nomem;

	ld->ld_lberoptions = LBER_USE_DER;

	ld->ld_sb = ber_sockbuf_alloc( );
	if ( ld->ld_sb == NULL ) goto nomem;

#ifdef LDAP_R_COMPILE
	ldap_pvt_thread_mutex_init( &ld->ld_msgid_mutex );
	ldap_pvt_thread_mutex_init( &ld->ld_conn_mutex );
	ldap_pvt_thread_mutex_init( &ld->ld_req_mutex );
	ldap_pvt_thread_mutex_init( &ld->ld_res_mutex );
	ldap_pvt_thread_mutex_init( &ld->ld_abandon_mutex );
	ldap_pvt_thread_mutex_init( &ld->ld_ldcmutex );
#endif
	ld->ld_ldcrefcnt = 1;
	*ldp = ld;
	return LDAP_SUCCESS;

nomem:
	ldap_free_select_info( ld->ld_selectinfo );
	ldap_free_urllist( ld->ld_options.ldo_defludp );
#ifdef HAVE_CYRUS_SASL
	LDAP_FREE( ld->ld_options.ldo_def_sasl_authzid );
	LDAP_FREE( ld->ld_options.ldo_def_sasl_authcid );
	LDAP_FREE( ld->ld_options.ldo_def_sasl_realm );
	LDAP_FREE( ld->ld_options.ldo_def_sasl_mech );
#endif
	LDAP_FREE( (char *)ld );
	return LDAP_NO_MEMORY;
}

/*
 * ldap_init - initialize the LDAP library.  A magic cookie to be used for
 * future communication is returned on success, NULL on failure.
 * "host" may be a space-separated list of hosts or IP addresses
 *
 * Example:
 *	LDAP	*ld;
 *	ld = ldap_init( host, port );
 */
LDAP *
ldap_init( LDAP_CONST char *defhost, int defport )
{
	LDAP *ld;
	int rc;

	rc = ldap_create(&ld);
	if ( rc != LDAP_SUCCESS )
		return NULL;

	if (defport != 0)
		ld->ld_options.ldo_defport = defport;

	if (defhost != NULL) {
		rc = ldap_set_option(ld, LDAP_OPT_HOST_NAME, defhost);
		if ( rc != LDAP_SUCCESS ) {
			ldap_ld_free(ld, 1, NULL, NULL);
			return NULL;
		}
	}

	return( ld );
}

int
ldap_initialize( LDAP **ldp, LDAP_CONST char *url )
{
	int rc;
	LDAP *ld;

	*ldp = NULL;
	rc = ldap_create(&ld);
	if ( rc != LDAP_SUCCESS )
		return rc;

	if (url != NULL) {
		rc = ldap_set_option(ld, LDAP_OPT_URI, url);
		if ( rc != LDAP_SUCCESS ) {
			ldap_ld_free(ld, 1, NULL, NULL);
			return rc;
		}
#ifdef LDAP_CONNECTIONLESS
		if (ldap_is_ldapc_url(url))
			LDAP_IS_UDP(ld) = 1;
#endif
	}

	*ldp = ld;
	return LDAP_SUCCESS;
}

int
ldap_init_fd(
	ber_socket_t fd,
	int proto,
	LDAP_CONST char *url,
	LDAP **ldp
)
{
	int rc;
	LDAP *ld;
	LDAPConn *conn;
#ifdef LDAP_CONNECTIONLESS
	ber_socklen_t	len;
#endif

	*ldp = NULL;
	rc = ldap_create( &ld );
	if( rc != LDAP_SUCCESS )
		return( rc );

	if (url != NULL) {
		rc = ldap_set_option(ld, LDAP_OPT_URI, url);
		if ( rc != LDAP_SUCCESS ) {
			ldap_ld_free(ld, 1, NULL, NULL);
			return rc;
		}
	}

	LDAP_MUTEX_LOCK( &ld->ld_conn_mutex );
	/* Attach the passed socket as the LDAP's connection */
	conn = ldap_new_connection( ld, NULL, 1, 0, NULL, 0, 0 );
	if( conn == NULL ) {
		ldap_unbind_ext( ld, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}
	if( url )
		conn->lconn_server = ldap_url_dup( ld->ld_options.ldo_defludp );
	ber_sockbuf_ctrl( conn->lconn_sb, LBER_SB_OPT_SET_FD, &fd );
	ld->ld_defconn = conn;
	++ld->ld_defconn->lconn_refcnt;	/* so it never gets closed/freed */
	LDAP_MUTEX_UNLOCK( &ld->ld_conn_mutex );

	switch( proto ) {
	case LDAP_PROTO_TCP:
#ifdef LDAP_DEBUG
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
			LBER_SBIOD_LEVEL_PROVIDER, (void *)"tcp_" );
#endif
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_tcp,
			LBER_SBIOD_LEVEL_PROVIDER, NULL );
		break;

#ifdef LDAP_CONNECTIONLESS
	case LDAP_PROTO_UDP:
		LDAP_IS_UDP(ld) = 1;
		if( ld->ld_options.ldo_peer )
			ldap_memfree( ld->ld_options.ldo_peer );
		ld->ld_options.ldo_peer = ldap_memcalloc( 1, sizeof( struct sockaddr_storage ) );
		len = sizeof( struct sockaddr_storage );
		if( getpeername ( fd, ld->ld_options.ldo_peer, &len ) < 0) {
			ldap_unbind_ext( ld, NULL, NULL );
			return( AC_SOCKET_ERROR );
		}
#ifdef LDAP_DEBUG
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
			LBER_SBIOD_LEVEL_PROVIDER, (void *)"udp_" );
#endif
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_udp,
			LBER_SBIOD_LEVEL_PROVIDER, NULL );
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_readahead,
			LBER_SBIOD_LEVEL_PROVIDER, NULL );
		break;
#endif /* LDAP_CONNECTIONLESS */

	case LDAP_PROTO_IPC:
#ifdef LDAP_DEBUG
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
			LBER_SBIOD_LEVEL_PROVIDER, (void *)"ipc_" );
#endif
		ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_fd,
			LBER_SBIOD_LEVEL_PROVIDER, NULL );
		break;

	case LDAP_PROTO_EXT:
		/* caller must supply sockbuf handlers */
		break;

	default:
		ldap_unbind_ext( ld, NULL, NULL );
		return LDAP_PARAM_ERROR;
	}

#ifdef LDAP_DEBUG
	ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
		INT_MAX, (void *)"ldap_" );
#endif

	/* Add the connection to the *LDAP's select pool */
	ldap_mark_select_read( ld, conn->lconn_sb );

	*ldp = ld;
	return LDAP_SUCCESS;
}

/* Protected by ld_conn_mutex */
int
ldap_int_open_connection(
	LDAP *ld,
	LDAPConn *conn,
	LDAPURLDesc *srv,
	int async )
{
	int rc = -1;
	int proto;

	Debug( LDAP_DEBUG_TRACE, "ldap_int_open_connection\n" );

	switch ( proto = ldap_pvt_url_scheme2proto( srv->lud_scheme ) ) {
		case LDAP_PROTO_TCP:
			rc = ldap_connect_to_host( ld, conn->lconn_sb,
				proto, srv, async );

			if ( rc == -1 ) return rc;
#ifdef LDAP_DEBUG
			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
				LBER_SBIOD_LEVEL_PROVIDER, (void *)"tcp_" );
#endif
			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_tcp,
				LBER_SBIOD_LEVEL_PROVIDER, NULL );

			break;

#ifdef LDAP_CONNECTIONLESS
		case LDAP_PROTO_UDP:
			LDAP_IS_UDP(ld) = 1;
			rc = ldap_connect_to_host( ld, conn->lconn_sb,
				proto, srv, async );

			if ( rc == -1 ) return rc;
#ifdef LDAP_DEBUG
			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
				LBER_SBIOD_LEVEL_PROVIDER, (void *)"udp_" );
#endif
			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_udp,
				LBER_SBIOD_LEVEL_PROVIDER, NULL );

			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_readahead,
				LBER_SBIOD_LEVEL_PROVIDER, NULL );

			break;
#endif
		case LDAP_PROTO_IPC:
#ifdef LDAP_PF_LOCAL
			/* only IPC mechanism supported is PF_LOCAL (PF_UNIX) */
			rc = ldap_connect_to_path( ld, conn->lconn_sb,
				srv, async );
			if ( rc == -1 ) return rc;
#ifdef LDAP_DEBUG
			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
				LBER_SBIOD_LEVEL_PROVIDER, (void *)"ipc_" );
#endif
			ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_fd,
				LBER_SBIOD_LEVEL_PROVIDER, NULL );

			break;
#endif /* LDAP_PF_LOCAL */
		default:
			return -1;
			break;
	}

	conn->lconn_created = ldap_now();

#ifdef LDAP_DEBUG
	ber_sockbuf_add_io( conn->lconn_sb, &ber_sockbuf_io_debug,
		INT_MAX, (void *)"ldap_" );
#endif

#ifdef LDAP_CONNECTIONLESS
	if( proto == LDAP_PROTO_UDP ) return 0;
#endif

#ifdef HAVE_TLS
	if (rc == 0 && ( ld->ld_options.ldo_tls_mode == LDAP_OPT_X_TLS_HARD ||
		strcmp( srv->lud_scheme, "ldaps" ) == 0 ))
	{
		++conn->lconn_refcnt;	/* avoid premature free */

		rc = ldap_int_tls_start( ld, conn, srv );

		--conn->lconn_refcnt;

		if (rc != LDAP_SUCCESS) {
			return -1;
		}
	}
#endif

	return( 0 );
}

/*
 * ldap_open_internal_connection - open connection and set file descriptor
 *
 * note: ldap_init_fd() may be preferable
 */

int
ldap_open_internal_connection( LDAP **ldp, ber_socket_t *fdp )
{
	int rc;
	LDAPConn *c;
	LDAPRequest *lr;
	LDAP	*ld;

	rc = ldap_create( &ld );
	if( rc != LDAP_SUCCESS ) {
		*ldp = NULL;
		return( rc );
	}

	/* Make it appear that a search request, msgid 0, was sent */
	lr = (LDAPRequest *)LDAP_CALLOC( 1, sizeof( LDAPRequest ));
	if( lr == NULL ) {
		ldap_unbind_ext( ld, NULL, NULL );
		*ldp = NULL;
		return( LDAP_NO_MEMORY );
	}
	memset(lr, 0, sizeof( LDAPRequest ));
	lr->lr_msgid = 0;
	lr->lr_status = LDAP_REQST_INPROGRESS;
	lr->lr_res_errno = LDAP_SUCCESS;
	/* no mutex lock needed, we just created this ld here */
	ld->ld_requests = lr;

	LDAP_MUTEX_LOCK( &ld->ld_conn_mutex );
	/* Attach the passed socket as the *LDAP's connection */
	c = ldap_new_connection( ld, NULL, 1, 0, NULL, 0, 0 );
	if( c == NULL ) {
		ldap_unbind_ext( ld, NULL, NULL );
		*ldp = NULL;
		LDAP_MUTEX_UNLOCK( &ld->ld_conn_mutex );
		return( LDAP_NO_MEMORY );
	}
	ber_sockbuf_ctrl( c->lconn_sb, LBER_SB_OPT_SET_FD, fdp );
#ifdef LDAP_DEBUG
	ber_sockbuf_add_io( c->lconn_sb, &ber_sockbuf_io_debug,
		LBER_SBIOD_LEVEL_PROVIDER, (void *)"int_" );
#endif
	ber_sockbuf_add_io( c->lconn_sb, &ber_sockbuf_io_tcp,
	  LBER_SBIOD_LEVEL_PROVIDER, NULL );
	ld->ld_defconn = c;
	LDAP_MUTEX_UNLOCK( &ld->ld_conn_mutex );

	/* Add the connection to the *LDAP's select pool */
	ldap_mark_select_read( ld, c->lconn_sb );

	/* Make this connection an LDAP V3 protocol connection */
	rc = LDAP_VERSION3;
	ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &rc );
	*ldp = ld;

	++ld->ld_defconn->lconn_refcnt;	/* so it never gets closed/freed */

	return( LDAP_SUCCESS );
}

LDAP *
ldap_dup( LDAP *old )
{
	LDAP			*ld;

	if ( old == NULL ) {
		return( NULL );
	}

	Debug( LDAP_DEBUG_TRACE, "ldap_dup\n" );

	if ( (ld = (LDAP *) LDAP_CALLOC( 1, sizeof(LDAP) )) == NULL ) {
		return( NULL );
	}

	LDAP_MUTEX_LOCK( &old->ld_ldcmutex );
	ld->ldc = old->ldc;
	old->ld_ldcrefcnt++;
	LDAP_MUTEX_UNLOCK( &old->ld_ldcmutex );
	return ( ld );
}

int
ldap_int_check_async_open( LDAP *ld, ber_socket_t sd )
{
	struct timeval tv = { 0 };
	int rc;

	rc = ldap_int_poll( ld, sd, &tv, 1 );
	switch ( rc ) {
	case 0:
		/* now ready to start tls */
		ld->ld_defconn->lconn_status = LDAP_CONNST_CONNECTED;
		break;

	default:
		ld->ld_errno = LDAP_CONNECT_ERROR;
		return -1;

	case -2:
		/* connect not completed yet */
		ld->ld_errno = LDAP_X_CONNECTING;
		return rc;
	}

#ifdef HAVE_TLS
	if ( ld->ld_options.ldo_tls_mode == LDAP_OPT_X_TLS_HARD ||
		!strcmp( ld->ld_defconn->lconn_server->lud_scheme, "ldaps" )) {

		++ld->ld_defconn->lconn_refcnt;	/* avoid premature free */

		rc = ldap_int_tls_start( ld, ld->ld_defconn, ld->ld_defconn->lconn_server );

		--ld->ld_defconn->lconn_refcnt;
	}
#endif
	return rc;
}
