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

/*
 * Locale-specific 1-byte character versions
 * See utf-8.c for UTF-8 versions
 */

#include "reldap.h"

#include <ac/stdlib.h>
#include <ac/string.h>
#include <ac/time.h>
#include <ac/ctype.h>

#include "ldap-int.h"


#if defined ( HAVE_STRSPN )
#define int_strspn strspn
#else
static int int_strspn( const char *str, const char *delim )
{
	int pos;
	const char *p=delim;

	for( pos=0; (*str) ; pos++,str++) {
		if (*str!=*p) {
			for( p=delim; (*p) ; p++ ) {
				if (*str==*p) {
					break;
				}
		  	}
		}

		if (*p=='\0') {
			return pos;
		}
	}
	return pos;
}
#endif

#if defined( HAVE_STRPBRK )
#define int_strpbrk strpbrk
#else
static const char *(int_strpbrk)( const char *str, const char *accept )
{
	const char *p;

	for( ; (*str) ; str++ ) {
		for( p=accept; (*p) ; p++) {
			if (*str==*p) {
				return str;
			}
		}
	}

	return NULL;
}
#endif

char *(ldap_pvt_strtok)( char *str, const char *delim, char **pos )
{
	char *p;

	if (pos==NULL) {
		return NULL;
	}

	if (str==NULL) {
		if (*pos==NULL) {
			return NULL;
		}

		str=*pos;
	}

	/* skip any initial delimiters */
	str += int_strspn( str, delim );
	if (*str == '\0') {
		return NULL;
	}

	p = (char*) int_strpbrk( str, delim );
	if (p==NULL) {
		*pos = NULL;

	} else {
		*p ='\0';
		*pos = p+1;
	}

	return str;
}

char *
ldap_pvt_str2upper( char *str )
{
	char    *s;

	/* to upper */
	if ( str ) {
		for ( s = str; *s; s++ ) {
			*s = TOUPPER( (unsigned char) *s );
		}
	}

	return( str );
}

struct berval *
ldap_pvt_str2upperbv( char *str, struct berval *bv )
{
	char    *s = NULL;

	assert( bv != NULL );

	/* to upper */
	if ( str ) {
		for ( s = str; *s; s++ ) {
			*s = TOUPPER( (unsigned char) *s );
		}
	}

	bv->bv_val = str;
	bv->bv_len = (ber_len_t)(s - str);

	return( bv );
}

char *
ldap_pvt_str2lower( char *str )
{
	char    *s;

	/* to lower */
	if ( str ) {
		for ( s = str; *s; s++ ) {
			*s = TOLOWER( (unsigned char) *s );
		}
	}

	return( str );
}

struct berval *
ldap_pvt_str2lowerbv( char *str, struct berval *bv )
{
	char    *s = NULL;

	assert( bv != NULL );

	/* to lower */
	if ( str ) {
		for ( s = str; *s; s++ ) {
			*s = TOLOWER( (unsigned char) *s );
		}
	}

	bv->bv_val = str;
	bv->bv_len = (ber_len_t)(s - str);

	return( bv );
}
