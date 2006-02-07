/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc.
 * Copyright © 2005 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Keith Packard <keithp@keithp.com>
 *	Graydon Hoare <graydon@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 */

#ifndef CAIRO_HASH_PRIVATE_H
#define CAIRO_HASH_PRIVATE_H

/* XXX: I'd like this file to be self-contained in terms of
 * includeability, but that's not really possible with the current
 * monolithic cairoint.h. So, for now, just include cairoint.h instead
 * if you want to include this file. */

typedef struct _cairo_hash_table cairo_hash_table_t;

/**
 * cairo_hash_entry_t:
 *
 * A #cairo_hash_entry_t contains both a key and a value for
 * cairo_hash_table_t. User-derived types for cairo_hash_entry_t must
 * be type-compatible with this structure (eg. they must have an
 * unsigned long as the first parameter. The easiest way to get this
 * is to use:
 *
 * 	typedef _my_entry {
 *	    cairo_hash_entry_t base;
 *	    ... Remainder of key and value fields here ..
 *	} my_entry_t;
 *
 * which then allows a pointer to my_entry_t to be passed to any of
 * the cairo_hash_table functions as follows without requiring a cast:
 *
 *	_cairo_hash_table_insert (hash_table, &my_entry->base);
 *
 * IMPORTANT: The caller is reponsible for initializing
 * my_entry->base.hash with a hash code derived from the key. The
 * essential property of the hash code is that keys_equal must never
 * return TRUE for two keys that have different hashes. The best hash
 * code will reduce the frequency of two keys with the same code for
 * which keys_equal returns FALSE.
 *
 * Which parts of the entry make up the "key" and which part make up
 * the value are entirely up to the caller, (as determined by the
 * computation going into base.hash as well as the keys_equal
 * function). A few of the cairo_hash_table functions accept an entry
 * which will be used exclusively as a "key", (indicated by a
 * parameter name of key). In these cases, the value-related fields of
 * the entry need not be initialized if so desired.
 **/
typedef struct _cairo_hash_entry {
    unsigned long hash;
} cairo_hash_entry_t;

typedef cairo_bool_t
(*cairo_hash_keys_equal_func_t) (void *key_a, void *key_b);

typedef cairo_bool_t
(*cairo_hash_predicate_func_t) (void *entry);

typedef void
(*cairo_hash_callback_func_t) (void *entry,
			       void *closure);

cairo_private cairo_hash_table_t *
_cairo_hash_table_create (cairo_hash_keys_equal_func_t keys_equal);

cairo_private void
_cairo_hash_table_destroy (cairo_hash_table_t *hash_table);

cairo_private cairo_bool_t
_cairo_hash_table_lookup (cairo_hash_table_t  *hash_table,
			  cairo_hash_entry_t  *key,
			  cairo_hash_entry_t **entry_return);

cairo_private void *
_cairo_hash_table_random_entry (cairo_hash_table_t	   *hash_table,
				cairo_hash_predicate_func_t predicate);

cairo_private cairo_status_t
_cairo_hash_table_insert (cairo_hash_table_t *hash_table,
			  cairo_hash_entry_t *entry);

cairo_private void
_cairo_hash_table_remove (cairo_hash_table_t *hash_table,
			  cairo_hash_entry_t *key);

cairo_private void
_cairo_hash_table_foreach (cairo_hash_table_t 	      *hash_table,
			   cairo_hash_callback_func_t  hash_callback,
			   void			      *closure);

#endif
