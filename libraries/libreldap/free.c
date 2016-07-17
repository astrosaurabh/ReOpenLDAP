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
/* Portions Copyright (c) 1994 The Regents of the University of Michigan.
 * All rights reserved.
 */

/*
 *  free.c - some free routines are included here to avoid having to
 *           link in lots of extra code when not using certain features
 */

#include "reldap.h"

#include <stdio.h>
#include <ac/stdlib.h>

#include <ac/string.h>
#include <ac/time.h>

#include "ldap-int.h"

/*
 * C-API deallocator
 */
void
ldap_memfree( void *p )
{
	LDAP_FREE( p );
}

void
ldap_memvfree( void **v )
{
	LDAP_VFREE( v );
}

void *
ldap_memalloc( ber_len_t s )
{
	return LDAP_MALLOC( s );
}

void *
ldap_memcalloc( ber_len_t n, ber_len_t s )
{
	return LDAP_CALLOC( n, s );
}

void *
ldap_memrealloc( void* p, ber_len_t s )
{
	return LDAP_REALLOC( p, s );
}

char *
ldap_strdup( LDAP_CONST char *p )
{
	return LDAP_STRDUP( p );
}

/*
 * free a null-terminated array of pointers to mod structures. the
 * structures are freed, not the array itself, unless the freemods
 * flag is set.
 */

void
ldap_mods_free( LDAPMod **mods, int freemods )
{
	int	i;

	if ( mods == NULL )
		return;

	for ( i = 0; mods[i] != NULL; i++ ) {
		if ( mods[i]->mod_op & LDAP_MOD_BVALUES ) {
			if( mods[i]->mod_bvalues != NULL )
				ber_bvecfree( mods[i]->mod_bvalues );

		} else if( mods[i]->mod_values != NULL ) {
			LDAP_VFREE( mods[i]->mod_values );
		}

		if ( mods[i]->mod_type != NULL ) {
			LDAP_FREE( mods[i]->mod_type );
		}

		LDAP_FREE( (char *) mods[i] );
	}

	if ( freemods ) {
		LDAP_FREE( (char *) mods );
	}
}
