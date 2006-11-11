/* $Id: cairo-scaled-font.c,v 1.8 2006/04/01 00:36:09 vladimir%pobox.com Exp $
 *
 * Copyright © 2005 Keith Packard
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
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s):
 *      Keith Packard <keithp@keithp.com>
 *	Carl D. Worth <cworth@cworth.org>
 *      Graydon Hoare <graydon@redhat.com>
 *      Owen Taylor <otaylor@redhat.com>
 */

#include "cairoint.h"

static cairo_bool_t
_cairo_scaled_glyph_keys_equal (void *abstract_key_a, void *abstract_key_b)
{
    cairo_scaled_glyph_t *key_a = abstract_key_a;
    cairo_scaled_glyph_t *key_b = abstract_key_b;

    return (_cairo_scaled_glyph_index (key_a) ==
	    _cairo_scaled_glyph_index (key_b));
}

static void
_cairo_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_scaled_font_t	*scaled_font = scaled_glyph->scaled_font;
    const cairo_surface_backend_t *surface_backend = scaled_font->surface_backend;

    if (surface_backend != NULL && surface_backend->scaled_glyph_fini != NULL)
	surface_backend->scaled_glyph_fini (scaled_glyph, scaled_font);
    if (scaled_glyph->surface != NULL)
	cairo_surface_destroy (&scaled_glyph->surface->base);
    if (scaled_glyph->path != NULL)
	_cairo_path_fixed_destroy (scaled_glyph->path);
}
	
static void
_cairo_scaled_glyph_destroy (void *abstract_glyph)
{
    cairo_scaled_glyph_t *scaled_glyph = abstract_glyph;
    _cairo_scaled_glyph_fini (scaled_glyph);
    free (scaled_glyph);
}

static const cairo_scaled_font_t _cairo_scaled_font_nil = {
    { 0 },			/* hash_entry */
    CAIRO_STATUS_NO_MEMORY,	/* status */
    -1,				/* ref_count */
    NULL,			/* font_face */
    { 1., 0., 0., 1., 0, 0},	/* font_matrix */
    { 1., 0., 0., 1., 0, 0},	/* ctm */
    { CAIRO_ANTIALIAS_DEFAULT,	/* options */
      CAIRO_SUBPIXEL_ORDER_DEFAULT,
      CAIRO_HINT_STYLE_DEFAULT,
      CAIRO_HINT_METRICS_DEFAULT} ,
    { 1., 0., 0., 1., 0, 0},	/* scale */
    { 0., 0., 0., 0., 0. },	/* extents */
    NULL,			/* glyphs */
    NULL,			/* surface_backend */
    NULL,			/* surface_private */
    CAIRO_SCALED_FONT_BACKEND_DEFAULT,
};

/**
 * _cairo_scaled_font_set_error:
 * @scaled_font: a scaled_font
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 * 
 * Sets scaled_font->status to @status and calls _cairo_error;
 *
 * All assignments of an error status to scaled_font->status should happen
 * through _cairo_scaled_font_set_error() or else _cairo_error() should be
 * called immediately after the assignment.
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
void
_cairo_scaled_font_set_error (cairo_scaled_font_t *scaled_font,
			      cairo_status_t status)
{
    /* Don't overwrite an existing error. This preserves the first
     * error, which is the most significant. It also avoids attempting
     * to write to read-only data (eg. from a nil scaled_font). */
    if (scaled_font->status == CAIRO_STATUS_SUCCESS)
	scaled_font->status = status;

    _cairo_error (status);
}

/**
 * cairo_scaled_font_get_type:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Return value: The type of @scaled_font. See #cairo_font_type_t.
 **/
cairo_font_type_t
cairo_scaled_font_get_type (cairo_scaled_font_t *scaled_font)
{
    return scaled_font->backend->type;
}

/**
 * cairo_scaled_font_status:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Checks whether an error has previously occurred for this
 * scaled_font.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or another error such as
 *   %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
cairo_scaled_font_status (cairo_scaled_font_t *scaled_font)
{
    return scaled_font->status;
}

/* Here we keep a unique mapping from
 * cairo_font_face_t/matrix/ctm/options => cairo_scaled_font_t.
 *
 * Here are the things that we want to map:
 *
 *  a) All otherwise referenced cairo_scaled_font_t's
 *  b) Some number of not otherwise referenced cairo_scaled_font_t's
 *
 * The implementation uses a hash table which covers (a)
 * completely. Then, for (b) we have an array of otherwise
 * unreferenced fonts (holdovers) which are expired in
 * least-recently-used order.
 *
 * The cairo_scaled_font_create code gets to treat this like a regular
 * hash table. All of the magic for the little holdover cache is in
 * cairo_scaled_font_reference and cairo_scaled_font_destroy.
 */

/* This defines the size of the holdover array ... that is, the number
 * of scaled fonts we keep around even when not otherwise referenced
 */
#define CAIRO_SCALED_FONT_MAX_HOLDOVERS 256
 
typedef struct _cairo_scaled_font_map {
    cairo_hash_table_t *hash_table;
    cairo_scaled_font_t *holdovers[CAIRO_SCALED_FONT_MAX_HOLDOVERS];
    int num_holdovers;
} cairo_scaled_font_map_t;

static cairo_scaled_font_map_t *cairo_scaled_font_map = NULL;

CAIRO_MUTEX_DECLARE (cairo_scaled_font_map_mutex);

static int
_cairo_scaled_font_keys_equal (void *abstract_key_a, void *abstract_key_b);

static cairo_scaled_font_map_t *
_cairo_scaled_font_map_lock (void)
{
    CAIRO_MUTEX_LOCK (cairo_scaled_font_map_mutex);

    if (cairo_scaled_font_map == NULL) {
	cairo_scaled_font_map = malloc (sizeof (cairo_scaled_font_map_t));
	if (cairo_scaled_font_map == NULL)
	    goto CLEANUP_MUTEX_LOCK;

	cairo_scaled_font_map->hash_table =
	    _cairo_hash_table_create (_cairo_scaled_font_keys_equal);

	if (cairo_scaled_font_map->hash_table == NULL)
	    goto CLEANUP_SCALED_FONT_MAP;

	cairo_scaled_font_map->num_holdovers = 0;
    }

    return cairo_scaled_font_map;

 CLEANUP_SCALED_FONT_MAP:
    free (cairo_scaled_font_map);
 CLEANUP_MUTEX_LOCK:
    CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);
    return NULL;
}

static void
_cairo_scaled_font_map_unlock (void)
{
   CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);
}

void
_cairo_scaled_font_map_destroy (void)
{
    int i;
    cairo_scaled_font_map_t *font_map = cairo_scaled_font_map;
    cairo_scaled_font_t *scaled_font;

    if (font_map == NULL)
	return;

    CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);

    for (i = 0; i < font_map->num_holdovers; i++) {
	scaled_font = font_map->holdovers[i];
	/* We should only get here through the reset_static_data path
	 * and there had better not be any active references at that
	 * point. */
	assert (scaled_font->ref_count == 0);
	_cairo_hash_table_remove (font_map->hash_table,
				  &scaled_font->hash_entry);
	_cairo_scaled_font_fini (scaled_font);
	free (scaled_font);
    }

    _cairo_hash_table_destroy (font_map->hash_table);

    free (cairo_scaled_font_map);
    cairo_scaled_font_map = NULL;
}

/* Fowler / Noll / Vo (FNV) Hash (http://www.isthe.com/chongo/tech/comp/fnv/)
 * 
 * Not necessarily better than a lot of other hashes, but should be OK, and
 * well tested with binary data.
 */

#define FNV_32_PRIME ((uint32_t)0x01000193)
#define FNV1_32_INIT ((uint32_t)0x811c9dc5)

static uint32_t
_hash_bytes_fnv (unsigned char *buffer,
		 int            len,
		 uint32_t       hval)
{
    while (len--) {
	hval *= FNV_32_PRIME;
	hval ^= *buffer++;
    }

    return hval;
}

static void
_cairo_scaled_font_init_key (cairo_scaled_font_t        *scaled_font,
			     cairo_font_face_t	        *font_face,
			     const cairo_matrix_t       *font_matrix,
			     const cairo_matrix_t       *ctm,
			     const cairo_font_options_t *options)
{
    uint32_t hash = FNV1_32_INIT;

    scaled_font->status = CAIRO_STATUS_SUCCESS;
    scaled_font->font_face = font_face;
    scaled_font->font_matrix = *font_matrix;
    scaled_font->ctm = *ctm;
    scaled_font->options = *options;

    /* We do a bytewise hash on the font matrices, ignoring the
     * translation values in the ctm */
    hash = _hash_bytes_fnv ((unsigned char *)(&scaled_font->font_matrix.xx),
			    sizeof(cairo_matrix_t), hash);
    hash = _hash_bytes_fnv ((unsigned char *)(&scaled_font->ctm.xx),
			    sizeof(double) * 4, hash);

    hash ^= (unsigned long) scaled_font->font_face;

    hash ^= cairo_font_options_hash (&scaled_font->options);

    scaled_font->hash_entry.hash = hash;
}

static cairo_bool_t
_cairo_scaled_font_keys_equal (void *abstract_key_a, void *abstract_key_b)
{
    cairo_scaled_font_t *key_a = abstract_key_a;
    cairo_scaled_font_t *key_b = abstract_key_b;

    return (key_a->font_face == key_b->font_face &&
	    memcmp ((unsigned char *)(&key_a->font_matrix.xx),
		    (unsigned char *)(&key_b->font_matrix.xx),
		    sizeof(cairo_matrix_t)) == 0 &&
	    memcmp ((unsigned char *)(&key_a->ctm.xx),
		    (unsigned char *)(&key_b->ctm.xx),
		    sizeof(double) * 4) == 0 &&
	    cairo_font_options_equal (&key_a->options, &key_b->options));
}

/*
 * Basic cairo_scaled_font_t object management
 */

cairo_status_t
_cairo_scaled_font_init (cairo_scaled_font_t               *scaled_font, 
			 cairo_font_face_t		   *font_face,
			 const cairo_matrix_t              *font_matrix,
			 const cairo_matrix_t              *ctm,
			 const cairo_font_options_t	   *options,
			 const cairo_scaled_font_backend_t *backend)
{
    scaled_font->ref_count = 1;

    _cairo_scaled_font_init_key (scaled_font, font_face,
				 font_matrix, ctm, options);

    cairo_font_face_reference (font_face);

    cairo_matrix_multiply (&scaled_font->scale,
			   &scaled_font->font_matrix,
			   &scaled_font->ctm);

    scaled_font->glyphs = _cairo_cache_create (_cairo_scaled_glyph_keys_equal,
					       _cairo_scaled_glyph_destroy,
					       256);
    
    scaled_font->surface_backend = NULL;
    scaled_font->surface_private = NULL;
    
    scaled_font->backend = backend;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_scaled_font_set_metrics (cairo_scaled_font_t	    *scaled_font,
				cairo_font_extents_t	    *fs_metrics)
{
    double  font_scale_x, font_scale_y;
    
    _cairo_matrix_compute_scale_factors (&scaled_font->font_matrix,
					 &font_scale_x, &font_scale_y,
					 /* XXX */ 1);
    
    /* 
     * The font responded in unscaled units, scale by the font
     * matrix scale factors to get to user space
     */
    
    scaled_font->extents.ascent = fs_metrics->ascent * font_scale_y;
    scaled_font->extents.descent = fs_metrics->descent * font_scale_y;
    scaled_font->extents.height = fs_metrics->height * font_scale_y;
    scaled_font->extents.max_x_advance = fs_metrics->max_x_advance * font_scale_x;
    scaled_font->extents.max_y_advance = fs_metrics->max_y_advance * font_scale_y;
}

void
_cairo_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    if (scaled_font->font_face != NULL)
	cairo_font_face_destroy (scaled_font->font_face);

    if (scaled_font->glyphs != NULL)
	_cairo_cache_destroy (scaled_font->glyphs);
    
    if (scaled_font->surface_backend != NULL &&
	scaled_font->surface_backend->scaled_font_fini != NULL)
	scaled_font->surface_backend->scaled_font_fini (scaled_font);

    scaled_font->backend->fini (scaled_font);

}

/**
 * cairo_scaled_font_create:
 * @font_face: a #cairo_font_face_t
 * @font_matrix: font space to user space transformation matrix for the
 *       font. In the simplest case of a N point font, this matrix is
 *       just a scale by N, but it can also be used to shear the font
 *       or stretch it unequally along the two axes. See
 *       cairo_set_font_matrix().
 * @ctm: user to device transformation matrix with which the font will
 *       be used.
 * @options: options to use when getting metrics for the font and
 *           rendering with it.
 * 
 * Creates a #cairo_scaled_font_t object from a font face and matrices that
 * describe the size of the font and the environment in which it will
 * be used.
 * 
 * Return value: a newly created #cairo_scaled_font_t. Destroy with
 *  cairo_scaled_font_destroy()
 **/
cairo_scaled_font_t *
cairo_scaled_font_create (cairo_font_face_t          *font_face,
			  const cairo_matrix_t       *font_matrix,
			  const cairo_matrix_t       *ctm,
			  const cairo_font_options_t *options)
{
    cairo_status_t status;
    cairo_scaled_font_map_t *font_map;
    cairo_scaled_font_t key, *scaled_font = NULL;

    if (font_face->status)
	return (cairo_scaled_font_t *)&_cairo_scaled_font_nil;

    font_map = _cairo_scaled_font_map_lock ();
    if (font_map == NULL)
	goto UNWIND;
    
    _cairo_scaled_font_init_key (&key, font_face,
				 font_matrix, ctm, options);

    /* Return existing scaled_font if it exists in the hash table. */
    if (_cairo_hash_table_lookup (font_map->hash_table, &key.hash_entry,
				  (cairo_hash_entry_t**) &scaled_font))
    {
	_cairo_scaled_font_map_unlock ();
	return cairo_scaled_font_reference (scaled_font);
    }

    /* Otherwise create it and insert it into the hash table. */
    status = font_face->backend->scaled_font_create (font_face, font_matrix,
						     ctm, options, &scaled_font);
    if (status)
	goto UNWIND_FONT_MAP_LOCK;

    status = _cairo_hash_table_insert (font_map->hash_table,
				       &scaled_font->hash_entry);
    if (status)
	goto UNWIND_SCALED_FONT_CREATE;

    _cairo_scaled_font_map_unlock ();

    return scaled_font;

UNWIND_SCALED_FONT_CREATE:
    /* We can't call _cairo_scaled_font_destroy here since it expects
     * that the font has already been successfully inserted into the
     * hash table. */
    _cairo_scaled_font_fini (scaled_font);
    free (scaled_font);
UNWIND_FONT_MAP_LOCK:
    _cairo_scaled_font_map_unlock ();
UNWIND:
    return NULL;
}

/**
 * cairo_scaled_font_reference:
 * @scaled_font: a #cairo_scaled_font_t, (may be NULL in which case
 * this function does nothing)
 * 
 * Increases the reference count on @scaled_font by one. This prevents
 * @scaled_font from being destroyed until a matching call to
 * cairo_scaled_font_destroy() is made.
 *
 * Returns: the referenced #cairo_scaled_font_t
 **/
cairo_scaled_font_t *
cairo_scaled_font_reference (cairo_scaled_font_t *scaled_font)
{
    cairo_scaled_font_map_t *font_map;

    if (scaled_font == NULL)
	return NULL;

    if (scaled_font->ref_count == (unsigned int)-1)
	return scaled_font;

    /* We would normally assert (scaled_font->ref_count > 0) here, but
     * we are using ref_count == 0 as a legitimate case for the
     * holdovers array. See below. */

    /* cairo_scaled_font_t objects are cached and shared between
     * threads. This works because these objects are immutable. Except
     * that the reference count is mutable, so we have to do locking
     * around any modification of the reference count. */
    font_map = _cairo_scaled_font_map_lock ();
    {
	/* If the original reference count is 0, then this font must have
	 * been found in font_map->holdovers, (which means this caching is
	 * actually working). So now we remove it from the holdovers
	 * array. */
	if (scaled_font->ref_count == 0) {
	    int i;

	    for (i = 0; i < font_map->num_holdovers; i++)
		if (font_map->holdovers[i] == scaled_font)
		    break;
	    assert (i < font_map->num_holdovers);

	    font_map->num_holdovers--;
	    memmove (&font_map->holdovers[i],
		     &font_map->holdovers[i+1],
		     (font_map->num_holdovers - i) * sizeof (cairo_scaled_font_t*));
	}

	scaled_font->ref_count++;

    }
    _cairo_scaled_font_map_unlock ();

    return scaled_font;
}

/**
 * cairo_scaled_font_destroy:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Decreases the reference count on @font by one. If the result
 * is zero, then @font and all associated resources are freed.
 * See cairo_scaled_font_reference().
 **/
void
cairo_scaled_font_destroy (cairo_scaled_font_t *scaled_font)
{
    cairo_scaled_font_map_t *font_map;

    if (scaled_font == NULL)
	return;

    if (scaled_font->ref_count == (unsigned int)-1)
	return;

    /* cairo_scaled_font_t objects are cached and shared between
     * threads. This works because these objects are immutable. Except
     * that the reference count is mutable, so we have to do locking
     * around any modification of the reference count. */
    font_map = _cairo_scaled_font_map_lock ();
    {
	assert (font_map != NULL);

	assert (scaled_font->ref_count > 0);

	if (--(scaled_font->ref_count) == 0)
	{
	    /* Rather than immediately destroying this object, we put it into
	     * the font_map->holdovers array in case it will get used again
	     * soon. To make room for it, we do actually destroy the
	     * least-recently-used holdover.
	     */
	    if (font_map->num_holdovers == CAIRO_SCALED_FONT_MAX_HOLDOVERS) {
		cairo_scaled_font_t *lru;

		lru = font_map->holdovers[0];
		assert (lru->ref_count == 0);
	
		_cairo_hash_table_remove (font_map->hash_table, &lru->hash_entry);

		_cairo_scaled_font_fini (lru);
		free (lru);
	
		font_map->num_holdovers--;
		memmove (&font_map->holdovers[0],
			 &font_map->holdovers[1],
			 font_map->num_holdovers * sizeof (cairo_scaled_font_t*));
	    }

	    font_map->holdovers[font_map->num_holdovers] = scaled_font;
	    font_map->num_holdovers++;
	}
    }
    _cairo_scaled_font_map_unlock ();
}

/* Public font API follows. */

/**
 * cairo_scaled_font_extents:
 * @scaled_font: a #cairo_scaled_font_t
 * @extents: a #cairo_font_extents_t which to store the retrieved extents.
 * 
 * Gets the metrics for a #cairo_scaled_font_t. 
 **/
void
cairo_scaled_font_extents (cairo_scaled_font_t  *scaled_font,
			   cairo_font_extents_t *extents)
{
    *extents = scaled_font->extents;
}

/**
 * cairo_scaled_font_text_extents:
 * @scaled_font: a #cairo_scaled_font_t
 * @utf8: a string of text, encoded in UTF-8
 * @extents: a #cairo_text_extents_t which to store the retrieved extents.
 *
 * Gets the extents for a string of text. The extents describe a
 * user-space rectangle that encloses the "inked" portion of the text
 * drawn at the origin (0,0) (as it would be drawn by cairo_show_text()
 * if the cairo graphics state were set to the same font_face,
 * font_matrix, ctm, and font_options as @scaled_font).  Additionally,
 * the x_advance and y_advance values indicate the amount by which the
 * current point would be advanced by cairo_show_text().
 *
 * Note that whitespace characters do not directly contribute to the
 * size of the rectangle (extents.width and extents.height). They do
 * contribute indirectly by changing the position of non-whitespace
 * characters. In particular, trailing whitespace characters are
 * likely to not affect the size of the rectangle, though they will
 * affect the x_advance and y_advance values.
 **/
void
cairo_scaled_font_text_extents (cairo_scaled_font_t   *scaled_font,
				const char            *utf8,
				cairo_text_extents_t  *extents)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_glyph_t *glyphs;
    int num_glyphs;

    status = _cairo_scaled_font_text_to_glyphs (scaled_font, 0., 0., utf8, &glyphs, &num_glyphs);
    if (status) {
        _cairo_scaled_font_set_error (scaled_font, status);
        return;
    }
    cairo_scaled_font_glyph_extents (scaled_font, glyphs, num_glyphs, extents);
    free (glyphs);
}

/**
 * cairo_scaled_font_glyph_extents:
 * @scaled_font: a #cairo_scaled_font_t
 * @glyphs: an array of glyph IDs with X and Y offsets.
 * @num_glyphs: the number of glyphs in the @glyphs array
 * @extents: a #cairo_text_extents_t which to store the retrieved extents.
 *
 * Gets the extents for an array of glyphs. The extents describe a
 * user-space rectangle that encloses the "inked" portion of the
 * glyphs, (as they would be drawn by cairo_show_glyphs() if the cairo
 * graphics state were set to the same font_face, font_matrix, ctm,
 * and font_options as @scaled_font).  Additionally, the x_advance and
 * y_advance values indicate the amount by which the current point
 * would be advanced by cairo_show_glyphs.
 *
 * Note that whitespace glyphs do not contribute to the size of the
 * rectangle (extents.width and extents.height).
 **/
void
cairo_scaled_font_glyph_extents (cairo_scaled_font_t   *scaled_font,
				 cairo_glyph_t         *glyphs, 
				 int                    num_glyphs,
				 cairo_text_extents_t  *extents)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int i;
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    double x_pos = 0.0, y_pos = 0.0;

    if (scaled_font->status)
	return;

    if (!num_glyphs) {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;
	
	return;
    }

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t	*scaled_glyph;
	double			left, top, right, bottom;
	
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (status) {
	    _cairo_scaled_font_set_error (scaled_font, status);
	    return;
	}
	
	left = scaled_glyph->metrics.x_bearing + glyphs[i].x;
	right = left + scaled_glyph->metrics.width;
	top = scaled_glyph->metrics.y_bearing + glyphs[i].y;
	bottom = top + scaled_glyph->metrics.height;
	
	if (i == 0) {
	    min_x = left;
	    max_x = right;
	    min_y = top;
	    max_y = bottom;
	} else {
	    if (left < min_x) min_x = left;
	    if (right > max_x) max_x = right;
	    if (top < min_y) min_y = top;
	    if (bottom > max_y) max_y = bottom;
	}
	x_pos = glyphs[i].x + scaled_glyph->metrics.x_advance;
	y_pos = glyphs[i].y + scaled_glyph->metrics.y_advance;
    }

    extents->x_bearing = min_x - glyphs[0].x;
    extents->y_bearing = min_y - glyphs[0].y;
    extents->width = max_x - min_x;
    extents->height = max_y - min_y;
    extents->x_advance = x_pos - glyphs[0].x;
    extents->y_advance = y_pos - glyphs[0].y;
}

cairo_status_t
_cairo_scaled_font_text_to_glyphs (cairo_scaled_font_t *scaled_font,
				   double		x,
				   double		y,
				   const char          *utf8, 
				   cairo_glyph_t      **glyphs, 
				   int 		       *num_glyphs)
{
    size_t i;
    uint32_t *ucs4 = NULL;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *scaled_glyph;

    if (scaled_font->backend->text_to_glyphs) {
	status = scaled_font->backend->text_to_glyphs (scaled_font,
						       x, y, utf8,
						       glyphs, num_glyphs);

        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return status;
    }

    status = _cairo_utf8_to_ucs4 ((unsigned char*)utf8, -1, &ucs4, num_glyphs);
    if (status)
	return status;

    *glyphs = (cairo_glyph_t *) malloc ((*num_glyphs) * (sizeof (cairo_glyph_t)));

    if (*glyphs == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL;
    }

    for (i = 0; i < *num_glyphs; i++) {            
        (*glyphs)[i].index = (*scaled_font->backend->
			      ucs4_to_index) (scaled_font, ucs4[i]);
	(*glyphs)[i].x = x;
	(*glyphs)[i].y = y;
	
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     (*glyphs)[i].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (status) {
	    free (*glyphs);
	    *glyphs = NULL;
	    goto FAIL;
	}
					     
        x += scaled_glyph->metrics.x_advance;
        y += scaled_glyph->metrics.y_advance;
    }

 FAIL:
    free (ucs4);
    
    return status;
}

/*
 * Compute a device-space bounding box for the glyphs.
 */
cairo_status_t
_cairo_scaled_font_glyph_device_extents (cairo_scaled_font_t	*scaled_font,
					 const cairo_glyph_t	*glyphs,
					 int                     num_glyphs,
					 cairo_rectangle_t	*extents)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int i;
    int min_x = CAIRO_MAXSHORT, max_x = CAIRO_MINSHORT;
    int	min_y = CAIRO_MAXSHORT, max_y = CAIRO_MINSHORT;

    if (scaled_font->status)
	return scaled_font->status;

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t	*scaled_glyph;
	int			left, top;
	int			right, bottom;
	int			x, y;
	
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (status) {
	    _cairo_scaled_font_set_error (scaled_font, status);
	    return status;
	}
	
	/* glyph images are snapped to pixel locations */
	x = (int) floor (glyphs[i].x + 0.5);
	y = (int) floor (glyphs[i].y + 0.5);
	
	left   = x + _cairo_fixed_integer_floor(scaled_glyph->bbox.p1.x);
	top    = y + _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y);
	right  = x + _cairo_fixed_integer_ceil(scaled_glyph->bbox.p2.x);
	bottom = y + _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.y);
	
	if (left < min_x) min_x = left;
	if (right > max_x) max_x = right;
	if (top < min_y) min_y = top;
	if (bottom > max_y) max_y = bottom;
    }
    if (min_x < max_x && min_y < max_y) {
	extents->x = min_x;
	extents->width = max_x - min_x;
	extents->y = min_y;
	extents->height = max_y - min_y;
    } else {
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
    }
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_scaled_font_show_glyphs (cairo_scaled_font_t    *scaled_font,
				cairo_operator_t        op,
				cairo_pattern_t        *pattern,
				cairo_surface_t        *surface,
				int                     source_x,
				int                     source_y,
				int			dest_x,
				int			dest_y,
				unsigned int		width,
				unsigned int		height,
				const cairo_glyph_t    *glyphs,
				int                     num_glyphs)
{
    cairo_status_t status;
    cairo_surface_t *mask = NULL;
    int i;

    /* These operators aren't interpreted the same way by the backends;
     * they are implemented in terms of other operators in cairo-gstate.c
     */
    assert (op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_CLEAR);
    
    if (scaled_font->status)
	return scaled_font->status;

    if (scaled_font->backend->show_glyphs != NULL) {
	status = scaled_font->backend->show_glyphs (scaled_font,
						    op, pattern, 
						    surface,
						    source_x, source_y,
						    dest_x, dest_y,
						    width, height,
						    glyphs, num_glyphs);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    /* Font display routine either does not exist or failed. */
    
    status = CAIRO_STATUS_SUCCESS;

    _cairo_cache_freeze (scaled_font->glyphs);

    for (i = 0; i < num_glyphs; i++) {
	int x, y;
	cairo_surface_pattern_t glyph_pattern;
	cairo_image_surface_t *glyph_surface;
	cairo_scaled_glyph_t *scaled_glyph;
	
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);

	if (status)
	    goto CLEANUP_MASK;
	
	glyph_surface = scaled_glyph->surface;

	/* Create the mask using the format from the first glyph */
	if (mask == NULL) {
	    mask = cairo_image_surface_create (glyph_surface->format,
					       width, height);
	    if (mask->status) {
		status = mask->status;
		goto CLEANUP_MASK;
	    }

	    status = _cairo_surface_fill_rectangle (mask,
						    CAIRO_OPERATOR_CLEAR,
						    CAIRO_COLOR_TRANSPARENT,
						    0, 0,
						    width, height);
	    if (status)
		goto CLEANUP_MASK;
	    if (glyph_surface->format == CAIRO_FORMAT_ARGB32)
		pixman_image_set_component_alpha (((cairo_image_surface_t*) mask)->
						  pixman_image, TRUE);

	}
	
	/* round glyph locations to the nearest pixel */
	x = (int) floor (glyphs[i].x + 
			 glyph_surface->base.device_x_offset +
			 0.5);
	y = (int) floor (glyphs[i].y +
			 glyph_surface->base.device_y_offset +
			 0.5);
	
	_cairo_pattern_init_for_surface (&glyph_pattern, &glyph_surface->base);

	status = _cairo_surface_composite (CAIRO_OPERATOR_ADD, 
					   &glyph_pattern.base, 
					   NULL,
					   mask,
					   0, 0,
					   0, 0, 
					   x - dest_x, 
					   y - dest_y, 
					   glyph_surface->width,
					   glyph_surface->height);

	_cairo_pattern_fini (&glyph_pattern.base);
	if (status)
	    break;
    }
    
    if (mask != NULL) {
	cairo_surface_pattern_t mask_pattern;

	_cairo_pattern_init_for_surface (&mask_pattern, mask);
    
	status = _cairo_surface_composite (op, pattern, &mask_pattern.base,
					   surface,
					   source_x, source_y, 
					   0,        0,
					   dest_x,   dest_y,
					   width,    height);
    
	_cairo_pattern_fini (&mask_pattern.base);
    }
	
CLEANUP_MASK:
    _cairo_cache_thaw (scaled_font->glyphs);
    
    if (mask != NULL)
	cairo_surface_destroy (mask);
    return status;
}

typedef struct _cairo_scaled_glyph_path_closure {
    cairo_point_t	    offset;
    cairo_path_fixed_t	    *path;
} cairo_scaled_glyph_path_closure_t;

static cairo_status_t
_scaled_glyph_path_move_to (void *abstract_closure, cairo_point_t *point)
{
    cairo_scaled_glyph_path_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_move_to (closure->path,
				      point->x + closure->offset.x,
				      point->y + closure->offset.y);
}

static cairo_status_t
_scaled_glyph_path_line_to (void *abstract_closure, cairo_point_t *point)
{
    cairo_scaled_glyph_path_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_line_to (closure->path,
				      point->x + closure->offset.x,
				      point->y + closure->offset.y);
}

static cairo_status_t
_scaled_glyph_path_curve_to (void *abstract_closure,
			     cairo_point_t *p0,
			     cairo_point_t *p1,
			     cairo_point_t *p2)
{
    cairo_scaled_glyph_path_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_curve_to (closure->path,
				       p0->x + closure->offset.x,
				       p0->y + closure->offset.y,
				       p1->x + closure->offset.x,
				       p1->y + closure->offset.y,
				       p2->x + closure->offset.x,
				       p2->y + closure->offset.y);
}


static cairo_status_t
_scaled_glyph_path_close_path (void *abstract_closure)
{
    cairo_scaled_glyph_path_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_close_path (closure->path);
}

cairo_status_t
_cairo_scaled_font_glyph_path (cairo_scaled_font_t *scaled_font,
			       cairo_glyph_t	   *glyphs, 
			       int		    num_glyphs,
			       cairo_path_fixed_t  *path)
{
    cairo_status_t status;
    int	i;
    cairo_scaled_glyph_path_closure_t closure;
    
    if (scaled_font->status)
	return scaled_font->status;
    
    closure.path = path;
    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
	
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_PATH,
					     &scaled_glyph);
	if (status)
	    return status;

	closure.offset.x = _cairo_fixed_from_double (glyphs[i].x);
	closure.offset.y = _cairo_fixed_from_double (glyphs[i].y);
	
	status = _cairo_path_fixed_interpret (scaled_glyph->path,
					      CAIRO_DIRECTION_FORWARD,
					      _scaled_glyph_path_move_to,
					      _scaled_glyph_path_line_to,
					      _scaled_glyph_path_curve_to,
					      _scaled_glyph_path_close_path,
					      &closure);
    }
    
    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_scaled_glyph_set_metrics:
 * @scaled_glyph: a #cairo_scaled_glyph_t
 * @scaled_font: a #cairo_scaled_font_t
 * @fs_metrics: a #cairo_text_extents_t in font space
 * 
 * _cairo_scaled_glyph_set_metrics() stores user space metrics
 * for the specified glyph given font space metrics. It is 
 * called by the font backend when initializing a glyph with
 * CAIRO_SCALED_GLYPH_INFO_METRICS.
 **/
void
_cairo_scaled_glyph_set_metrics (cairo_scaled_glyph_t *scaled_glyph,
				 cairo_scaled_font_t *scaled_font,
				 cairo_text_extents_t *fs_metrics)
{
    cairo_bool_t first = TRUE;
    double hm, wm;
    double min_user_x = 0.0, max_user_x = 0.0, min_user_y = 0.0, max_user_y = 0.0;
    double min_device_x = 0.0, max_device_x = 0.0, min_device_y = 0.0, max_device_y = 0.0;
    
    for (hm = 0.0; hm <= 1.0; hm += 1.0)
	for (wm = 0.0; wm <= 1.0; wm += 1.0) {
	    double x, y;

	    /* Transform this corner to user space */
	    x = fs_metrics->x_bearing + fs_metrics->width * wm;
	    y = fs_metrics->y_bearing + fs_metrics->height * hm;
	    cairo_matrix_transform_point (&scaled_font->font_matrix,
					  &x, &y);
	    if (first) {
		min_user_x = max_user_x = x;
		min_user_y = max_user_y = y;
	    } else {
		if (x < min_user_x) min_user_x = x;
		if (x > max_user_x) max_user_x = x;
		if (y < min_user_y) min_user_y = y;
		if (y > max_user_y) max_user_y = y;
	    }

	    /* Transform this corner to device space from glyph origin */
	    x = fs_metrics->x_bearing + fs_metrics->width * wm;
	    y = fs_metrics->y_bearing + fs_metrics->height * hm;
	    cairo_matrix_transform_distance (&scaled_font->scale,
					     &x, &y);
	    
	    if (first) {
		min_device_x = max_device_x = x;
		min_device_y = max_device_y = y;
	    } else {
		if (x < min_device_x) min_device_x = x;
		if (x > max_device_x) max_device_x = x;
		if (y < min_device_y) min_device_y = y;
		if (y > max_device_y) max_device_y = y;
	    }
	    first = FALSE;
	}
    scaled_glyph->metrics.x_bearing = min_user_x;
    scaled_glyph->metrics.y_bearing = min_user_y;
    scaled_glyph->metrics.width = max_user_x - min_user_x;
    scaled_glyph->metrics.height = max_user_y - min_user_y;

    scaled_glyph->metrics.x_advance = fs_metrics->x_advance;
    scaled_glyph->metrics.y_advance = fs_metrics->y_advance;
    cairo_matrix_transform_point (&scaled_font->font_matrix,
				  &scaled_glyph->metrics.x_advance,
				  &scaled_glyph->metrics.y_advance);

    scaled_glyph->bbox.p1.x = _cairo_fixed_from_double (min_device_x);
    scaled_glyph->bbox.p1.y = _cairo_fixed_from_double (min_device_y);
    scaled_glyph->bbox.p2.x = _cairo_fixed_from_double (max_device_x);
    scaled_glyph->bbox.p2.y = _cairo_fixed_from_double (max_device_y);
}

void
_cairo_scaled_glyph_set_surface (cairo_scaled_glyph_t *scaled_glyph,
				 cairo_scaled_font_t *scaled_font,
				 cairo_image_surface_t *surface)
{
    if (scaled_glyph->surface != NULL)
	cairo_surface_destroy (&scaled_glyph->surface->base);
    scaled_glyph->surface = surface;
}

void
_cairo_scaled_glyph_set_path (cairo_scaled_glyph_t *scaled_glyph,
			      cairo_scaled_font_t *scaled_font,
			      cairo_path_fixed_t *path)
{
    if (scaled_glyph->path != NULL)
	_cairo_path_fixed_destroy (scaled_glyph->path);
    scaled_glyph->path = path;
}

/**
 * _cairo_scaled_glyph_lookup:
 * @scaled_font: a #cairo_scaled_font_t
 * @index: the glyph to create
 * @info: a #cairo_scaled_glyph_info_t marking which portions of
 * the glyph should be filled in.
 * @scaled_glyph_ret: a #cairo_scaled_glyph_t * where the glyph
 * is returned.
 * 
 * Returns a glyph with the requested portions filled in. Glyph
 * lookup is cached and glyph will be automatically freed along
 * with the scaled_font so no explicit free is required.
 * @info can be one or more of:
 *  %CAIRO_SCALED_GLYPH_INFO_METRICS - glyph metrics and bounding box
 *  %CAIRO_SCALED_GLYPH_INFO_SURFACE - surface holding glyph image
 *  %CAIRO_SCALED_GLYPH_INFO_PATH - path holding glyph outline in device space
 **/
cairo_status_t
_cairo_scaled_glyph_lookup (cairo_scaled_font_t *scaled_font,
			    unsigned long index,
			    cairo_scaled_glyph_info_t info,
			    cairo_scaled_glyph_t **scaled_glyph_ret)
{
    cairo_status_t		status = CAIRO_STATUS_SUCCESS;
    cairo_cache_entry_t		key;
    cairo_scaled_glyph_t	*scaled_glyph;
    cairo_scaled_glyph_info_t	need_info;
    
    if (scaled_font->status)
	return scaled_font->status;

    CAIRO_MUTEX_LOCK (cairo_scaled_font_map_mutex);

    key.hash = index;
    /*
     * Check cache for glyph
     */
    info |= CAIRO_SCALED_GLYPH_INFO_METRICS;
    if (!_cairo_cache_lookup (scaled_font->glyphs, &key, 
			      (cairo_cache_entry_t **) &scaled_glyph)) 
    {
	/*
	 * On miss, create glyph and insert into cache
	 */
	scaled_glyph = malloc (sizeof (cairo_scaled_glyph_t));
	if (scaled_glyph == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto CLEANUP;
	}
	    
	_cairo_scaled_glyph_set_index(scaled_glyph, index);
	scaled_glyph->cache_entry.size = 1;	/* XXX */
	scaled_glyph->scaled_font = scaled_font;
	scaled_glyph->surface = NULL;
	scaled_glyph->path = NULL;
	scaled_glyph->surface_private = NULL;
	
	/* ask backend to initialize metrics and shape fields */
	status = (*scaled_font->backend->
		  scaled_glyph_init) (scaled_font, scaled_glyph, info);
	if (status)
	    goto CLEANUP;

	status = _cairo_cache_insert (scaled_font->glyphs,
				      &scaled_glyph->cache_entry);
	if (status)
	    goto CLEANUP;
    }
    /*
     * Check and see if the glyph, as provided,
     * already has the requested data and ammend it if not
     */
    need_info = 0;
    if ((info & CAIRO_SCALED_GLYPH_INFO_SURFACE) != 0 && 
	scaled_glyph->surface == NULL)
	need_info |= CAIRO_SCALED_GLYPH_INFO_SURFACE;
    
    if (((info & CAIRO_SCALED_GLYPH_INFO_PATH) != 0 &&
	 scaled_glyph->path == NULL))
	need_info |= CAIRO_SCALED_GLYPH_INFO_PATH;
    
    if (need_info) {
	status = (*scaled_font->backend->
		  scaled_glyph_init) (scaled_font, scaled_glyph, need_info);
	if (status)
	    goto CLEANUP;
    }

  CLEANUP:
    if (status) {
	_cairo_scaled_font_set_error (scaled_font, status);
	if (scaled_glyph)
	    _cairo_scaled_glyph_destroy (scaled_glyph);
	*scaled_glyph_ret = NULL;
    } else {
	*scaled_glyph_ret = scaled_glyph;
    }

    CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);

    return status;
}

/**
 * cairo_scaled_font_get_font_face:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Return value: The #cairo_font_face_t with which @scaled_font was
 * created.
 **/
cairo_font_face_t *
cairo_scaled_font_get_font_face (cairo_scaled_font_t *scaled_font)
{
    if (scaled_font->status)
	return (cairo_font_face_t*) &_cairo_font_face_nil;

    return scaled_font->font_face;
}

/**
 * cairo_scaled_font_get_font_matrix:
 * @scaled_font: a #cairo_scaled_font_t
 * @font_matrix: return value for the matrix
 * 
 * Stores the font matrix with which @scaled_font was created into
 * @matrix.
 **/
void
cairo_scaled_font_get_font_matrix (cairo_scaled_font_t	*scaled_font,
				   cairo_matrix_t	*font_matrix)
{
    if (scaled_font->status) {
	cairo_matrix_init_identity (font_matrix);
	return;
    }

    *font_matrix = scaled_font->font_matrix;
}

/**
 * cairo_scaled_font_get_ctm:
 * @scaled_font: a #cairo_scaled_font_t
 * @ctm: return value for the CTM
 * 
 * Stores the CTM with which @scaled_font was created into @ctm.
 **/
void
cairo_scaled_font_get_ctm (cairo_scaled_font_t	*scaled_font,
			   cairo_matrix_t	*ctm)
{
    if (scaled_font->status) {
	cairo_matrix_init_identity (ctm);
	return;
    }

    *ctm = scaled_font->ctm;
}

/**
 * cairo_scaled_font_get_font_options:
 * @scaled_font: a #cairo_scaled_font_t
 * @options: return value for the font options
 * 
 * Stores the font options with which @scaled_font was created into
 * @ctm.
 **/
void
cairo_scaled_font_get_font_options (cairo_scaled_font_t		*scaled_font,
				    cairo_font_options_t	*options)
{
    if (scaled_font->status) {
	_cairo_font_options_init_default (options);
	return;
    }

    _cairo_font_options_init_copy (options, &scaled_font->options);
}
