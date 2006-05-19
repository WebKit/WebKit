/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 *	Carl Worth <cworth@cworth.org>
 */

/* This isn't a "real" surface, but just something to be used by the
 * test suite to test a mythical backend that uses nothing but
 * fallbacks.
 *
 * The defining feature of this backend is that it has as many NULL
 * backend function entries as possible. The ones that aren't NULL are
 * simply those that must be implemented to have working fallbacks.
 * (Except for create_similar---fallbacks would work fine without
 * that---I implemented it here in order to create as many surfaces as
 * possible of type test_fallback_surface_t during the test suite
 * run).
 *
 * It's possible that this code might serve as a good starting point
 * for someone working on bringing up a new backend, starting with the
 * minimal all-fallbacks approach and working up gradually from
 * there.
 */

#include "test-fallback-surface.h"

#include "cairoint.h"

typedef struct _test_fallback_surface {
    cairo_surface_t base;

    /* This is a cairo_image_surface to hold the actual contents. */
    cairo_surface_t *backing;
} test_fallback_surface_t;

const cairo_private cairo_surface_backend_t test_fallback_surface_backend;

cairo_surface_t *
_test_fallback_surface_create (cairo_content_t	content,
			       int		width,
			       int		height)
{
    test_fallback_surface_t *surface;
    cairo_surface_t *backing;

    backing = _cairo_image_surface_create_with_content (content, width, height);
    if (cairo_surface_status (backing))
	return (cairo_surface_t*) &_cairo_surface_nil;

    surface = malloc (sizeof (test_fallback_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &test_fallback_surface_backend);

    surface->backing = backing;

    return &surface->base;
}

static cairo_surface_t *
_test_fallback_surface_create_similar (void		*abstract_surface,
				       cairo_content_t	 content,
				       int		 width,
				       int		 height)
{
    assert (CAIRO_CONTENT_VALID (content));

    return _test_fallback_surface_create (content,
					  width, height);
}

static cairo_status_t
_test_fallback_surface_finish (void *abstract_surface)
{
    test_fallback_surface_t *surface = abstract_surface;

    cairo_surface_destroy (surface->backing);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_test_fallback_surface_acquire_source_image (void	     *abstract_surface,
					     cairo_image_surface_t **image_out,
					     void		 **image_extra)
{
    test_fallback_surface_t *surface = abstract_surface;

    return _cairo_surface_acquire_source_image (surface->backing,
						image_out, image_extra);
}

static void
_test_fallback_surface_release_source_image (void	     *abstract_surface,
					     cairo_image_surface_t	*image,
					     void		  *image_extra)
{
    test_fallback_surface_t *surface = abstract_surface;

    _cairo_surface_release_source_image (surface->backing,
					 image, image_extra);
}

static cairo_status_t
_test_fallback_surface_acquire_dest_image (void		      *abstract_surface,
					   cairo_rectangle_t	 *interest_rect,
					   cairo_image_surface_t**image_out,
					   cairo_rectangle_t	 *image_rect_out,
					   void			**image_extra)
{
    test_fallback_surface_t *surface = abstract_surface;
    
    return _cairo_surface_acquire_dest_image (surface->backing,
					      interest_rect,
					      image_out,
					      image_rect_out,
					      image_extra);
}

static void
_test_fallback_surface_release_dest_image (void			*abstract_surface,
					   cairo_rectangle_t	*interest_rect,
					   cairo_image_surface_t*image,
					   cairo_rectangle_t	*image_rect,
					   void			*image_extra)
{
    test_fallback_surface_t *surface = abstract_surface;

    _cairo_surface_release_dest_image (surface->backing,
				       interest_rect,
				       image,
				       image_rect,
				       image_extra);
}

static cairo_int_status_t
_test_fallback_surface_get_extents (void		*abstract_surface,
				    cairo_rectangle_t	*rectangle)
{
    test_fallback_surface_t *surface = abstract_surface;

    return _cairo_surface_get_extents (surface->backing, rectangle);
}

const cairo_surface_backend_t test_fallback_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
    _test_fallback_surface_create_similar,
    _test_fallback_surface_finish,
    _test_fallback_surface_acquire_source_image,
    _test_fallback_surface_release_source_image,
    _test_fallback_surface_acquire_dest_image,
    _test_fallback_surface_release_dest_image,
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    NULL, /* intersect_clip_path */
    _test_fallback_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* show_glyphs */
    NULL  /* snapshot */
};
