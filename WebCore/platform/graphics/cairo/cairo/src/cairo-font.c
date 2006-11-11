/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat Inc.
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *      Graydon Hoare <graydon@redhat.com>
 *      Owen Taylor <otaylor@redhat.com>
 */

#include "cairoint.h"

/* Forward declare so we can use it as an arbitrary backend for
 * _cairo_font_face_nil.
 */
static const cairo_font_face_backend_t _cairo_toy_font_face_backend;

/* cairo_font_face_t */

const cairo_font_face_t _cairo_font_face_nil = {
    { 0 },			/* hash_entry */
    CAIRO_STATUS_NO_MEMORY,	/* status */
    -1,		                /* ref_count */
    { 0, 0, 0, NULL },		/* user_data */
    &_cairo_toy_font_face_backend
};

void
_cairo_font_face_init (cairo_font_face_t               *font_face, 
		       const cairo_font_face_backend_t *backend)
{
    font_face->status = CAIRO_STATUS_SUCCESS;
    font_face->ref_count = 1;
    font_face->backend = backend;

    _cairo_user_data_array_init (&font_face->user_data);
}

/**
 * cairo_font_face_reference:
 * @font_face: a #cairo_font_face_t, (may be NULL in which case this
 * function does nothing).
 * 
 * Increases the reference count on @font_face by one. This prevents
 * @font_face from being destroyed until a matching call to
 * cairo_font_face_destroy() is made.
 *
 * Return value: the referenced #cairo_font_face_t.
 **/
cairo_font_face_t *
cairo_font_face_reference (cairo_font_face_t *font_face)
{
    if (font_face == NULL)
	return NULL;

    if (font_face->ref_count == (unsigned int)-1)
	return font_face;

    /* We would normally assert (font_face->ref_count >0) here but we
     * can't get away with that due to the zombie case as documented
     * in _cairo_ft_font_face_destroy. */

    font_face->ref_count++;

    return font_face;
}

/**
 * cairo_font_face_destroy:
 * @font_face: a #cairo_font_face_t
 * 
 * Decreases the reference count on @font_face by one. If the result
 * is zero, then @font_face and all associated resources are freed.
 * See cairo_font_face_reference().
 **/
void
cairo_font_face_destroy (cairo_font_face_t *font_face)
{
    if (font_face == NULL)
	return;

    if (font_face->ref_count == (unsigned int)-1)
	return;

    assert (font_face->ref_count > 0);

    if (--(font_face->ref_count) > 0)
	return;

    font_face->backend->destroy (font_face);

    /* We allow resurrection to deal with some memory management for the
     * FreeType backend where cairo_ft_font_face_t and cairo_ft_unscaled_font_t
     * need to effectively mutually reference each other
     */
    if (font_face->ref_count > 0)
	return;

    _cairo_user_data_array_fini (&font_face->user_data);

    free (font_face);
}

/**
 * cairo_font_face_get_type:
 * @font_face: a #cairo_font_face_t
 * 
 * Return value: The type of @font_face. See #cairo_font_type_t.
 **/
cairo_font_type_t
cairo_font_face_get_type (cairo_font_face_t *font_face)
{
    return font_face->backend->type;
}

/**
 * cairo_font_face_status:
 * @font_face: a #cairo_font_face_t
 * 
 * Checks whether an error has previously occurred for this
 * font face
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or another error such as
 *   %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
cairo_font_face_status (cairo_font_face_t *font_face)
{
    return font_face->status;
}

/**
 * cairo_font_face_get_user_data:
 * @font_face: a #cairo_font_face_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 * 
 * Return user data previously attached to @font_face using the specified
 * key.  If no user data has been attached with the given key this
 * function returns %NULL.
 * 
 * Return value: the user data previously attached or %NULL.
 **/
void *
cairo_font_face_get_user_data (cairo_font_face_t	   *font_face,
			       const cairo_user_data_key_t *key)
{
    return _cairo_user_data_array_get_data (&font_face->user_data,
					    key);
}

/**
 * cairo_font_face_set_user_data:
 * @font_face: a #cairo_font_face_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach to the font face
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * font face is destroyed or when new user data is attached using the
 * same key.
 * 
 * Attach user data to @font_face.  To remove user data from a font face,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 **/
cairo_status_t
cairo_font_face_set_user_data (cairo_font_face_t	   *font_face,
			       const cairo_user_data_key_t *key,
			       void			   *user_data,
			       cairo_destroy_func_t	    destroy)
{
    if (font_face->ref_count == -1)
	return CAIRO_STATUS_NO_MEMORY;
    
    return _cairo_user_data_array_set_data (&font_face->user_data,
					    key, user_data, destroy);
}

static const cairo_font_face_backend_t _cairo_toy_font_face_backend;

static int
_cairo_toy_font_face_keys_equal (void *key_a,
				 void *key_b);

/* We maintain a hash table from family/weight/slant =>
 * cairo_font_face_t for cairo_toy_font_t. The primary purpose of
 * this mapping is to provide unique cairo_font_face_t values so that
 * our cache and mapping from cairo_font_face_t => cairo_scaled_font_t
 * works. Once the corresponding cairo_font_face_t objects fall out of
 * downstream caches, we don't need them in this hash table anymore.
 */

static cairo_hash_table_t *cairo_toy_font_face_hash_table = NULL;

CAIRO_MUTEX_DECLARE (cairo_toy_font_face_hash_table_mutex);

static cairo_hash_table_t *
_cairo_toy_font_face_hash_table_lock (void)
{
    CAIRO_MUTEX_LOCK (cairo_toy_font_face_hash_table_mutex);

    if (cairo_toy_font_face_hash_table == NULL)
    {
	cairo_toy_font_face_hash_table =
	    _cairo_hash_table_create (_cairo_toy_font_face_keys_equal);

	if (cairo_toy_font_face_hash_table == NULL) {
	    CAIRO_MUTEX_UNLOCK (cairo_toy_font_face_hash_table_mutex);
	    return NULL;
	}
    }

    return cairo_toy_font_face_hash_table;
}

static void
_cairo_toy_font_face_hash_table_unlock (void)
{
    CAIRO_MUTEX_UNLOCK (cairo_toy_font_face_hash_table_mutex);
}

/**
 * _cairo_toy_font_face_init_key:
 * 
 * Initialize those portions of cairo_toy_font_face_t needed to use
 * it as a hash table key, including the hash code buried away in
 * font_face->base.hash_entry. No memory allocation is performed here
 * so that no fini call is needed. We do this to make it easier to use
 * an automatic cairo_toy_font_face_t variable as a key.
 **/
static void
_cairo_toy_font_face_init_key (cairo_toy_font_face_t *key,
			       const char	     *family,
			       cairo_font_slant_t     slant,
			       cairo_font_weight_t    weight)
{
    unsigned long hash;

    key->family = family;
    key->owns_family = FALSE;

    key->slant = slant;
    key->weight = weight;

    /* 1607 and 1451 are just a couple of arbitrary primes. */
    hash = _cairo_hash_string (family);
    hash += ((unsigned long) slant) * 1607;
    hash += ((unsigned long) weight) * 1451;
    
    key->base.hash_entry.hash = hash;
}

static cairo_status_t
_cairo_toy_font_face_init (cairo_toy_font_face_t *font_face,
			   const char	         *family,
			   cairo_font_slant_t	  slant,
			   cairo_font_weight_t	  weight)
{
    char *family_copy;

    family_copy = strdup (family);
    if (family_copy == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_toy_font_face_init_key (font_face, family_copy,
				      slant, weight);
    font_face->owns_family = TRUE;

    _cairo_font_face_init (&font_face->base, &_cairo_toy_font_face_backend);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_toy_font_face_fini (cairo_toy_font_face_t *font_face)
{
    /* We assert here that we own font_face->family before casting
     * away the const qualifer. */
    assert (font_face->owns_family);
    free ((char*) font_face->family);
}

static int
_cairo_toy_font_face_keys_equal (void *key_a,
				 void *key_b)
{
    cairo_toy_font_face_t *face_a = key_a;
    cairo_toy_font_face_t *face_b = key_b;

    return (strcmp (face_a->family, face_b->family) == 0 &&
	    face_a->slant == face_b->slant &&
	    face_a->weight == face_b->weight);
}

/**
 * _cairo_toy_font_face_create:
 * @family: a font family name, encoded in UTF-8
 * @slant: the slant for the font
 * @weight: the weight for the font
 * 
 * Creates a font face from a triplet of family, slant, and weight.
 * These font faces are used in implementation of the the #cairo_t "toy"
 * font API.
 * 
 * Return value: a newly created #cairo_font_face_t, destroy with
 *  cairo_font_face_destroy()
 **/
cairo_font_face_t *
_cairo_toy_font_face_create (const char          *family, 
			     cairo_font_slant_t   slant, 
			     cairo_font_weight_t  weight)
{
    cairo_status_t status;
    cairo_toy_font_face_t key, *font_face;
    cairo_hash_table_t *hash_table;

    hash_table = _cairo_toy_font_face_hash_table_lock ();
    if (hash_table == NULL)
	goto UNWIND;

    _cairo_toy_font_face_init_key (&key, family, slant, weight);
    
    /* Return existing font_face if it exists in the hash table. */
    if (_cairo_hash_table_lookup (hash_table,
				  &key.base.hash_entry,
				  (cairo_hash_entry_t **) &font_face))
    {
	_cairo_toy_font_face_hash_table_unlock ();
	return cairo_font_face_reference (&font_face->base);
    }

    /* Otherwise create it and insert into hash table. */
    font_face = malloc (sizeof (cairo_toy_font_face_t));
    if (font_face == NULL)
	goto UNWIND_HASH_TABLE_LOCK;

    status = _cairo_toy_font_face_init (font_face, family, slant, weight);
    if (status)
	goto UNWIND_FONT_FACE_MALLOC;

    status = _cairo_hash_table_insert (hash_table, &font_face->base.hash_entry);
    if (status)
	goto UNWIND_FONT_FACE_INIT;

    _cairo_toy_font_face_hash_table_unlock ();

    return &font_face->base;

 UNWIND_FONT_FACE_INIT:
 UNWIND_FONT_FACE_MALLOC:
    free (font_face);
 UNWIND_HASH_TABLE_LOCK:
    _cairo_toy_font_face_hash_table_unlock ();
 UNWIND:
    return (cairo_font_face_t*) &_cairo_font_face_nil;
}

static void
_cairo_toy_font_face_destroy (void *abstract_face)
{
    cairo_toy_font_face_t *font_face = abstract_face;
    cairo_hash_table_t *hash_table;

    if (font_face == NULL)
	return;

    hash_table = _cairo_toy_font_face_hash_table_lock ();
    /* All created objects must have been mapped in the hash table. */
    assert (hash_table != NULL);

    _cairo_hash_table_remove (hash_table, &font_face->base.hash_entry);
    
    _cairo_toy_font_face_hash_table_unlock ();
    
    _cairo_toy_font_face_fini (font_face);
}

static cairo_status_t
_cairo_toy_font_face_scaled_font_create (void                *abstract_font_face,
					 const cairo_matrix_t       *font_matrix,
					 const cairo_matrix_t       *ctm,
					 const cairo_font_options_t *options,
					 cairo_scaled_font_t	   **scaled_font)
{
    cairo_toy_font_face_t *font_face = abstract_font_face;
    const cairo_scaled_font_backend_t * backend = CAIRO_SCALED_FONT_BACKEND_DEFAULT;

    return backend->create_toy (font_face,
				font_matrix, ctm, options, scaled_font);
}

static const cairo_font_face_backend_t _cairo_toy_font_face_backend = {
    CAIRO_FONT_TYPE_TOY,
    _cairo_toy_font_face_destroy,
    _cairo_toy_font_face_scaled_font_create
};

void
_cairo_unscaled_font_init (cairo_unscaled_font_t               *unscaled_font, 
			   const cairo_unscaled_font_backend_t *backend)
{
    unscaled_font->ref_count = 1;
    unscaled_font->backend = backend;
}

cairo_unscaled_font_t *
_cairo_unscaled_font_reference (cairo_unscaled_font_t *unscaled_font)
{
    if (unscaled_font == NULL)
	return NULL;

    unscaled_font->ref_count++;

    return unscaled_font;
}

void
_cairo_unscaled_font_destroy (cairo_unscaled_font_t *unscaled_font)
{    
    if (unscaled_font == NULL)
	return;

    if (--(unscaled_font->ref_count) > 0)
	return;

    unscaled_font->backend->destroy (unscaled_font);

    free (unscaled_font);
}

void
_cairo_font_reset_static_data (void)
{
    _cairo_scaled_font_map_destroy ();

    CAIRO_MUTEX_LOCK (cairo_toy_font_face_hash_table_mutex);
    _cairo_hash_table_destroy (cairo_toy_font_face_hash_table);
    cairo_toy_font_face_hash_table = NULL;
    CAIRO_MUTEX_UNLOCK (cairo_toy_font_face_hash_table_mutex);
}
