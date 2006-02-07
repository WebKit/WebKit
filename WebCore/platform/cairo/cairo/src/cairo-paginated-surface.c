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

/* The paginated surface layer exists to provide as much code sharing
 * as possible for the various paginated surface backends in cairo
 * (PostScript, PDF, etc.).
 *
 * The concept is that a surface which uses a paginated surface merely
 * needs to implement backend operations which it can accurately
 * provide, (and return CAIRO_INT_STATUS_UNSUPPORTED or leave backend
 * function pointers NULL otherwise). The paginated surface is the
 * responsible for collecting operations that aren't supported,
 * replaying them against the image surface, and then supplying the
 * resulting images to the target surface.
 *
 * When created, a paginated surface accepts the target surface to
 * which the final drawing will eventually be performed. The paginated
 * surface then uses cairo_meta_surface_t to record all drawing
 * operations up until each show_page operation.
 *
 * At the time of show_page, the paginated surface replays the meta
 * surface against the target surface and maintains regions of the
 * result that will come from the nativ surface and regions that will
 * need to come from image fallbacks. It then replays the necessary
 * portions against image surface and provides those results to the
 * target surface through existing interfaces.
 *
 * This way the target surface is never even aware of any distinction
 * between native drawing operations vs. results that are supplied by
 * image fallbacks. Instead the surface need only implement as much of
 * the surface backend interface as it can do correctly, and let the
 * paginated surface take care of all the messy details.
 */

#include "cairoint.h"

#include "cairo-paginated-surface-private.h"
#include "cairo-meta-surface-private.h"

typedef struct _cairo_paginated_surface {
    cairo_surface_t base;

    cairo_content_t content;

    /* XXX: These shouldn't actually exist. We inherit this ugliness
     * from _cairo_meta_surface_create. The width/height parameters
     * from that function also should not exist. The fix that will
     * allow us to remove all of these is to fix acquire_source_image
     * to pass an interest rectangle. */
    int width;
    int height;

    /* The target surface to hold the final result. */
    cairo_surface_t *target;

    /* A cairo_meta_surface to record all operations. To be replayed
     * against target, and also against image surface as necessary for
     * fallbacks. */
    cairo_surface_t *meta;

} cairo_paginated_surface_t;

const cairo_private cairo_surface_backend_t cairo_paginated_surface_backend;

static cairo_int_status_t
_cairo_paginated_surface_show_page (void *abstract_surface);

cairo_surface_t *
_cairo_paginated_surface_create (cairo_surface_t	*target,
				 cairo_content_t	 content,
				 int			 width,
				 int			 height)
{
    cairo_paginated_surface_t *surface;

    surface = malloc (sizeof (cairo_paginated_surface_t));
    if (surface == NULL)
	goto FAIL;

    _cairo_surface_init (&surface->base, &cairo_paginated_surface_backend);

    surface->content = content;
    surface->width = width;
    surface->height = height;

    surface->target = target;

    surface->meta = _cairo_meta_surface_create (content, width, height);
    if (cairo_surface_status (surface->meta))
	goto FAIL_CLEANUP_SURFACE;

    return &surface->base;

  FAIL_CLEANUP_SURFACE:
    free (surface);
  FAIL:
    _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return (cairo_surface_t*) &_cairo_surface_nil;
}

cairo_bool_t
_cairo_surface_is_paginated (cairo_surface_t *surface)
{
    return surface->backend == &cairo_paginated_surface_backend;
}

cairo_surface_t *
_cairo_paginated_surface_get_target (cairo_surface_t *surface)
{
    cairo_paginated_surface_t *paginated_surface;

    assert (_cairo_surface_is_paginated (surface));

    paginated_surface = (cairo_paginated_surface_t *) surface;

    return paginated_surface->target;
}

static cairo_status_t
_cairo_paginated_surface_finish (void *abstract_surface)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    cairo_surface_destroy (surface->meta);

    cairo_surface_destroy (surface->target);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_paginated_surface_acquire_source_image (void	       *abstract_surface,
					       cairo_image_surface_t **image_out,
					       void		   **image_extra)
{
    cairo_paginated_surface_t *surface = abstract_surface;
    cairo_surface_t *image;
    cairo_rectangle_t extents;

    _cairo_surface_get_extents (surface->target, &extents);

    image = _cairo_image_surface_create_with_content (surface->content,
						      extents.width,
						      extents.height);
    
    _cairo_meta_surface_replay (surface->meta, image);

    *image_out = (cairo_image_surface_t*) image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_paginated_surface_release_source_image (void	  *abstract_surface,
					       cairo_image_surface_t *image,
					       void	       *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static void
_paint_page (cairo_paginated_surface_t *surface)
{
    cairo_surface_t *image;
    cairo_pattern_t *pattern;

    image = _cairo_image_surface_create_with_content (surface->content,
						      surface->width,
						      surface->height);

    _cairo_meta_surface_replay (surface->meta, image);

    pattern = cairo_pattern_create_for_surface (image);

    _cairo_surface_paint (surface->target, CAIRO_OPERATOR_SOURCE, pattern);

    cairo_pattern_destroy (pattern);

    cairo_surface_destroy (image);
}

static cairo_int_status_t
_cairo_paginated_surface_copy_page (void *abstract_surface)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    _paint_page (surface);

    /* XXX: It might make sense to add some suport here for calling
     * _cairo_surface_copy_page on the target surface. It would be an
     * optimization for the output, (so that PostScript could include
     * copypage, for example), but the interaction with image
     * fallbacks gets tricky. For now, we just let the target see a
     * show_page and we implement the copying by simply not destroying
     * the meta-surface. */

    _cairo_surface_show_page (surface->target);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_paginated_surface_show_page (void *abstract_surface)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    _paint_page (surface);

    _cairo_surface_show_page (surface->target);

    cairo_surface_destroy (surface->meta);

    surface->meta = _cairo_meta_surface_create (surface->content,
						surface->width, surface->height);
    if (cairo_surface_status (surface->meta))
	return cairo_surface_status (surface->meta);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_paginated_surface_intersect_clip_path (void	  *abstract_surface,
					      cairo_path_fixed_t *path,
					      cairo_fill_rule_t	  fill_rule,
					      double		  tolerance,
					      cairo_antialias_t	  antialias)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_intersect_clip_path (surface->meta,
					       path, fill_rule,
					       tolerance, antialias);
}

static cairo_int_status_t
_cairo_paginated_surface_get_extents (void	 *abstract_surface,
				      cairo_rectangle_t	*rectangle)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_get_extents (surface->target, rectangle);
}

static cairo_int_status_t
_cairo_paginated_surface_paint (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_paint (surface->meta, op, source);
}

static cairo_int_status_t
_cairo_paginated_surface_mask (void		*abstract_surface,
			       cairo_operator_t	 op,
			       cairo_pattern_t	*source,
			       cairo_pattern_t	*mask)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_mask (surface->meta, op, source, mask);
}

static cairo_int_status_t
_cairo_paginated_surface_stroke (void			*abstract_surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 cairo_stroke_style_t	*style,
				 cairo_matrix_t		*ctm,
				 cairo_matrix_t		*ctm_inverse,
				 double			 tolerance,
				 cairo_antialias_t	 antialias)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_stroke (surface->meta, op, source,
				  path, style,
				  ctm, ctm_inverse,
				  tolerance, antialias);
}

static cairo_int_status_t
_cairo_paginated_surface_fill (void			*abstract_surface,
			       cairo_operator_t		 op,
			       cairo_pattern_t		*source,
			       cairo_path_fixed_t	*path,
			       cairo_fill_rule_t	 fill_rule,
			       double			 tolerance,
			       cairo_antialias_t	 antialias)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_fill (surface->meta, op, source,
				path, fill_rule,
				tolerance, antialias);
}

static cairo_int_status_t
_cairo_paginated_surface_show_glyphs (void			*abstract_surface,
				      cairo_operator_t		 op,
				      cairo_pattern_t		*source,
				      const cairo_glyph_t	*glyphs,
				      int			 num_glyphs,
				      cairo_scaled_font_t	*scaled_font)
{
    cairo_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_show_glyphs (surface->meta, op, source,
				       glyphs, num_glyphs,
				       scaled_font);
}

static cairo_surface_t *
_cairo_paginated_surface_snapshot (void *abstract_other)
{
    cairo_paginated_surface_t *other = abstract_other;

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

    _cairo_surface_get_extents (other->target, &extents);

    surface = _cairo_image_surface_create_with_content (other->content,
							extents.width,
							extents.height);

    _cairo_meta_surface_replay (other->meta, surface);

    return surface;
#endif
}

const cairo_surface_backend_t cairo_paginated_surface_backend = {
    NULL, /* create_similar */
    _cairo_paginated_surface_finish,
    _cairo_paginated_surface_acquire_source_image,
    _cairo_paginated_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    _cairo_paginated_surface_copy_page,
    _cairo_paginated_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_paginated_surface_intersect_clip_path,
    _cairo_paginated_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    _cairo_paginated_surface_paint,
    _cairo_paginated_surface_mask,
    _cairo_paginated_surface_stroke,
    _cairo_paginated_surface_fill,
    _cairo_paginated_surface_show_glyphs,
    _cairo_paginated_surface_snapshot
};
