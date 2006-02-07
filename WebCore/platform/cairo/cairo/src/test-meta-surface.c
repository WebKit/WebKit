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
 * test suite to help exercise the meta-surface paths in cairo.
 *
 * The defining feature of this backend is that it uses a meta surface
 * to record all operations, and then replays everything to an image
 * surface.
 *
 * It's possible that this code might serve as a good starting point
 * for someone working on bringing up a new meta-surface-based
 * backend.
 */

#include "test-meta-surface.h"

#include "cairoint.h"
#include "cairo-meta-surface-private.h"

typedef struct _test_meta_surface {
    cairo_surface_t base;

    /* This is a cairo_meta_surface to record all operations. */
    cairo_surface_t *meta;

    /* And this is a cairo_image_surface to hold the final result. */
    cairo_surface_t *image;

    cairo_bool_t image_reflects_meta;
} test_meta_surface_t;

const cairo_private cairo_surface_backend_t test_meta_surface_backend;

static cairo_int_status_t
_test_meta_surface_show_page (void *abstract_surface);

cairo_surface_t *
_test_meta_surface_create (cairo_content_t	content,
			   int		 	width,
			   int		 	height)
{
    test_meta_surface_t *surface;

    surface = malloc (sizeof (test_meta_surface_t));
    if (surface == NULL)
	goto FAIL;

    _cairo_surface_init (&surface->base, &test_meta_surface_backend);

    surface->meta = _cairo_meta_surface_create (content, width, height);
    if (cairo_surface_status (surface->meta))
	goto FAIL_CLEANUP_SURFACE;

    surface->image = _cairo_image_surface_create_with_content (content,
							       width, height);
    if (cairo_surface_status (surface->image))
	goto FAIL_CLEANUP_META;

    surface->image_reflects_meta = FALSE;

    return &surface->base;

  FAIL_CLEANUP_META:
    cairo_surface_destroy (surface->meta);
  FAIL_CLEANUP_SURFACE:
    free (surface);
  FAIL:
    _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return (cairo_surface_t*) &_cairo_surface_nil;
}

static cairo_status_t
_test_meta_surface_finish (void *abstract_surface)
{
    test_meta_surface_t *surface = abstract_surface;

    cairo_surface_destroy (surface->meta);
    cairo_surface_destroy (surface->image);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_test_meta_surface_acquire_source_image (void		  *abstract_surface,
					 cairo_image_surface_t	**image_out,
					 void			**image_extra)
{
    test_meta_surface_t *surface = abstract_surface;

    if (! surface->image_reflects_meta)
	_test_meta_surface_show_page (abstract_surface);

    return _cairo_surface_acquire_source_image (surface->image,
						image_out, image_extra);
}

static void
_test_meta_surface_release_source_image (void			*abstract_surface,
					 cairo_image_surface_t	*image,
					 void		  	*image_extra)
{
    test_meta_surface_t *surface = abstract_surface;

    _cairo_surface_release_source_image (surface->image,
					 image, image_extra);
}

static cairo_int_status_t
_test_meta_surface_show_page (void *abstract_surface)
{
    test_meta_surface_t *surface = abstract_surface;

    if (surface->image_reflects_meta)
	return CAIRO_STATUS_SUCCESS;

    _cairo_meta_surface_replay (surface->meta, surface->image);

    surface->image_reflects_meta = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_test_meta_surface_intersect_clip_path (void			*abstract_surface,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	 fill_rule,
					double			 tolerance,
					cairo_antialias_t	 antialias)
{
    test_meta_surface_t *surface = abstract_surface;

    return _cairo_surface_intersect_clip_path (surface->meta,
					       path, fill_rule,
					       tolerance, antialias);
}

static cairo_int_status_t
_test_meta_surface_get_extents (void			*abstract_surface,
				cairo_rectangle_t	*rectangle)
{
    test_meta_surface_t *surface = abstract_surface;

    surface->image_reflects_meta = FALSE;

    return _cairo_surface_get_extents (surface->image, rectangle);
}

static cairo_int_status_t
_test_meta_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source)
{
    test_meta_surface_t *surface = abstract_surface;

    surface->image_reflects_meta = FALSE;

    return _cairo_surface_paint (surface->meta, op, source);
}

static cairo_int_status_t
_test_meta_surface_mask (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_pattern_t	*mask)
{
    test_meta_surface_t *surface = abstract_surface;

    surface->image_reflects_meta = FALSE;

    return _cairo_surface_mask (surface->meta, op, source, mask);
}

static cairo_int_status_t
_test_meta_surface_stroke (void			*abstract_surface,
			   cairo_operator_t	 op,
			   cairo_pattern_t	*source,
			   cairo_path_fixed_t	*path,
			   cairo_stroke_style_t	*style,
			   cairo_matrix_t	*ctm,
			   cairo_matrix_t	*ctm_inverse,
			   double		 tolerance,
			   cairo_antialias_t	 antialias)
{
    test_meta_surface_t *surface = abstract_surface;

    surface->image_reflects_meta = FALSE;

    return _cairo_surface_stroke (surface->meta, op, source,
				  path, style,
				  ctm, ctm_inverse,
				  tolerance, antialias);
}

static cairo_int_status_t
_test_meta_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias)
{
    test_meta_surface_t *surface = abstract_surface;

    surface->image_reflects_meta = FALSE;

    return _cairo_surface_fill (surface->meta, op, source,
				path, fill_rule,
				tolerance, antialias);
}

static cairo_int_status_t
_test_meta_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				const cairo_glyph_t	*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font)
{
    test_meta_surface_t *surface = abstract_surface;

    surface->image_reflects_meta = FALSE;

    return _cairo_surface_show_glyphs (surface->meta, op, source,
				       glyphs, num_glyphs,
				       scaled_font);
}

static cairo_surface_t *
_test_meta_surface_snapshot (void *abstract_other)
{
    test_meta_surface_t *other = abstract_other;

    /* XXX: Just making a snapshot of other->meta is what we really
     * want. But this currently triggers a bug somewhere (the "mask"
     * test from the test suite segfaults).
     *
     * For now, we'll create a new image surface and replay onto
     * that. It would be tempting to replay into other->image and then
     * return a snapshot of that, but that will cause the self-copy
     * test to fail, (since our replay will be affected by a clip that
     * should not have any effect on the use of the resulting snapshot
     * as a source).
     */

#if 0
    return _cairo_surface_snapshot (other->meta);
#else
    cairo_rectangle_t extents;
    cairo_surface_t *surface;

    _cairo_surface_get_extents (other->image, &extents);

    surface = cairo_surface_create_similar (other->image,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    extents.width,
					    extents.height);

    _cairo_meta_surface_replay (other->meta, surface);

    return surface;
#endif
}

const cairo_surface_backend_t test_meta_surface_backend = {
    NULL, /* create_similar */
    _test_meta_surface_finish,
    _test_meta_surface_acquire_source_image,
    _test_meta_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    _test_meta_surface_show_page,
    NULL, /* set_clip_region */
    _test_meta_surface_intersect_clip_path,
    _test_meta_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    _test_meta_surface_paint,
    _test_meta_surface_mask,
    _test_meta_surface_stroke,
    _test_meta_surface_fill,
    _test_meta_surface_show_glyphs,
    _test_meta_surface_snapshot
};
