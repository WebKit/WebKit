/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"
#include "cairo-clip-private.h"

static cairo_clip_path_t *
_cairo_clip_path_reference (cairo_clip_path_t *clip_path);

static void
_cairo_clip_path_destroy (cairo_clip_path_t *clip_path);

void
_cairo_clip_init (cairo_clip_t *clip, cairo_surface_t *target)
{
    clip->mode = _cairo_surface_get_clip_mode (target);

    clip->surface = NULL;
    clip->surface_rect.x = 0;
    clip->surface_rect.y = 0;
    clip->surface_rect.width = 0;
    clip->surface_rect.height = 0;

    clip->serial = 0;

    clip->region = NULL;

    clip->path = NULL;
}

void
_cairo_clip_fini (cairo_clip_t *clip)
{
    cairo_surface_destroy (clip->surface);
    clip->surface = NULL;

    clip->serial = 0;

    if (clip->region)
	pixman_region_destroy (clip->region);
    clip->region = NULL;

    _cairo_clip_path_destroy (clip->path);
    clip->path = NULL;
}

void
_cairo_clip_init_copy (cairo_clip_t *clip, cairo_clip_t *other)
{
    clip->mode = other->mode;

    clip->surface = cairo_surface_reference (other->surface);
    clip->surface_rect = other->surface_rect;

    clip->serial = other->serial;

    if (other->region == NULL) {
	clip->region = other->region;
    } else {
	clip->region = pixman_region_create ();
	pixman_region_copy (clip->region, other->region);
    }

    clip->path = _cairo_clip_path_reference (other->path);
}

cairo_status_t
_cairo_clip_reset (cairo_clip_t *clip)
{
    /* destroy any existing clip-region artifacts */
    cairo_surface_destroy (clip->surface);
    clip->surface = NULL;

    clip->serial = 0;

    if (clip->region)
	pixman_region_destroy (clip->region);
    clip->region = NULL;

    _cairo_clip_path_destroy (clip->path);
    clip->path = NULL;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_clip_intersect_to_rectangle (cairo_clip_t      *clip,
				    cairo_rectangle_t *rectangle)
{
    if (!clip)
	return CAIRO_STATUS_SUCCESS;

    if (clip->path) {
	/* Intersect path extents here. */
    }

    if (clip->region) {
	pixman_region16_t *intersection;
	cairo_status_t status = CAIRO_STATUS_SUCCESS;
	pixman_region_status_t pixman_status;
	
	intersection = _cairo_region_create_from_rectangle (rectangle);
	if (intersection == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	
	pixman_status = pixman_region_intersect (intersection,
					  clip->region,
					  intersection);
	if (pixman_status == PIXMAN_REGION_STATUS_SUCCESS)
	    _cairo_region_extents_rectangle (intersection, rectangle);
	else
	    status = CAIRO_STATUS_NO_MEMORY;

	pixman_region_destroy (intersection);

	if (status)
	    return status;
    }

    if (clip->surface)
	_cairo_rectangle_intersect (rectangle, &clip->surface_rect);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_clip_intersect_to_region (cairo_clip_t      *clip,
				 pixman_region16_t *region)
{
    if (!clip)
	return CAIRO_STATUS_SUCCESS;

    if (clip->path) {
	/* Intersect clip path into region. */
    }

    if (clip->region)
	pixman_region_intersect (region, clip->region, region);

    if (clip->surface) {
	pixman_region16_t *clip_rect;
	pixman_region_status_t pixman_status;
	cairo_status_t status = CAIRO_STATUS_SUCCESS;
    
	clip_rect = _cairo_region_create_from_rectangle (&clip->surface_rect);
	if (clip_rect == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	
	pixman_status = pixman_region_intersect (region,
						 clip_rect,
						 region);
	if (pixman_status != PIXMAN_REGION_STATUS_SUCCESS)
	    status = CAIRO_STATUS_NO_MEMORY;

	pixman_region_destroy (clip_rect);

	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Combines the region of clip->surface given by extents in
 * device backend coordinates into the given temporary surface,
 * which has its origin at dst_x, dst_y in backend coordinates
 */
cairo_status_t
_cairo_clip_combine_to_surface (cairo_clip_t            *clip,
				cairo_operator_t         op,
				cairo_surface_t         *dst,
				int                      dst_x,
				int                      dst_y,
				const cairo_rectangle_t *extents)
{
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    _cairo_pattern_init_for_surface (&pattern.surface, clip->surface);
    
    status = _cairo_surface_composite (op,
				       &pattern.base,
				       NULL,
				       dst,
				       extents->x - clip->surface_rect.x,
				       extents->y - clip->surface_rect.y, 
				       0, 0,
				       extents->x - dst_x,
				       extents->y - dst_y,
				       extents->width, extents->height);
    
    _cairo_pattern_fini (&pattern.base);

    return status;
}

static cairo_status_t
_cairo_clip_intersect_path (cairo_clip_t       *clip,
			    cairo_path_fixed_t *path,
			    cairo_fill_rule_t   fill_rule,
			    double              tolerance,
			    cairo_antialias_t   antialias,
			    cairo_surface_t    *target)
{
    cairo_clip_path_t *clip_path;
    cairo_status_t status;

    if (clip->mode != CAIRO_CLIP_MODE_PATH)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    clip_path = malloc (sizeof (cairo_clip_path_t));
    if (clip_path == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    status = _cairo_path_fixed_init_copy (&clip_path->path, path);
    if (status)
	return status;

    clip_path->ref_count = 1;
    clip_path->fill_rule = fill_rule;
    clip_path->tolerance = tolerance;
    clip_path->antialias = antialias;
    clip_path->prev = clip->path;
    clip->path = clip_path;
    clip->serial = _cairo_surface_allocate_clip_serial (target);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_clip_path_t *
_cairo_clip_path_reference (cairo_clip_path_t *clip_path)
{
    if (clip_path == NULL)
	return NULL;

    clip_path->ref_count++;

    return clip_path;
}

static void
_cairo_clip_path_destroy (cairo_clip_path_t *clip_path)
{
    if (clip_path == NULL)
	return;

    clip_path->ref_count--;
    if (clip_path->ref_count)
	return;

    _cairo_path_fixed_fini (&clip_path->path);
    _cairo_clip_path_destroy (clip_path->prev);
    free (clip_path);
}

static cairo_status_t
_cairo_clip_intersect_region (cairo_clip_t    *clip,
			      cairo_traps_t   *traps,
			      cairo_surface_t *target)
{
    pixman_region16_t *region;
    cairo_status_t status;

    if (clip->mode != CAIRO_CLIP_MODE_REGION)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    status = _cairo_traps_extract_region (traps, &region);
    if (status)
	return status;
	
    if (region == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = CAIRO_STATUS_SUCCESS;
    if (clip->region == NULL) {
	clip->region = region;
    } else {
	pixman_region16_t *intersection = pixman_region_create();
    
	if (pixman_region_intersect (intersection, 
				     clip->region, region)
	    == PIXMAN_REGION_STATUS_SUCCESS) {
	    pixman_region_destroy (clip->region);
	    clip->region = intersection;
	} else {		
	    status = CAIRO_STATUS_NO_MEMORY;
	}
	pixman_region_destroy (region);
    }

    clip->serial = _cairo_surface_allocate_clip_serial (target);

    return status;
}

static cairo_status_t
_cairo_clip_intersect_mask (cairo_clip_t      *clip,
			    cairo_traps_t     *traps,
			    cairo_antialias_t antialias,
			    cairo_surface_t   *target)
{
    cairo_pattern_union_t pattern;
    cairo_box_t extents;
    cairo_rectangle_t surface_rect, target_rect;
    cairo_surface_t *surface;
    cairo_status_t status;

    /* Represent the clip as a mask surface.  We create a new surface
     * the size of the intersection of the old mask surface and the
     * extents of the new clip path. */

    _cairo_traps_extents (traps, &extents);
    _cairo_box_round_to_rectangle (&extents, &surface_rect);

    if (clip->surface != NULL)
	_cairo_rectangle_intersect (&surface_rect, &clip->surface_rect);

    /* Intersect with the target surface rectangle so we don't use
     * more memory and time than we need to. */

    status = _cairo_surface_get_extents (target, &target_rect);
    if (!status)
	_cairo_rectangle_intersect (&surface_rect, &target_rect);

    surface = _cairo_surface_create_similar_solid (target,
						   CAIRO_CONTENT_ALPHA,
						   surface_rect.width,
						   surface_rect.height,
						   CAIRO_COLOR_WHITE);
    if (surface->status)
	return CAIRO_STATUS_NO_MEMORY;

    /* Render the new clipping path into the new mask surface. */

    _cairo_traps_translate (traps, -surface_rect.x, -surface_rect.y);
    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
    status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_IN,
						  &pattern.base,
						  surface,
						  antialias,
						  0, 0,
						  0, 0,
						  surface_rect.width,
						  surface_rect.height,
						  traps->traps,
						  traps->num_traps);

    _cairo_pattern_fini (&pattern.base);

    if (status) {
	cairo_surface_destroy (surface);
	return status;
    }

    /* If there was a clip surface already, combine it with the new
     * mask surface using the IN operator, so we get the intersection
     * of the old and new clipping paths. */

    if (clip->surface != NULL) {
	_cairo_pattern_init_for_surface (&pattern.surface, clip->surface);

	status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
					   &pattern.base,
					   NULL,
					   surface,
					   surface_rect.x - clip->surface_rect.x,
					   surface_rect.y - clip->surface_rect.y,
					   0, 0,
					   0, 0,
					   surface_rect.width,
					   surface_rect.height);

	_cairo_pattern_fini (&pattern.base);

	if (status) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	cairo_surface_destroy (clip->surface);
    }

    clip->surface = surface;
    clip->surface_rect = surface_rect;
    clip->serial = _cairo_surface_allocate_clip_serial (target);

    return status;
}

cairo_status_t
_cairo_clip_clip (cairo_clip_t       *clip,
		  cairo_path_fixed_t *path,
		  cairo_fill_rule_t   fill_rule,
		  double              tolerance,
		  cairo_antialias_t   antialias,
		  cairo_surface_t    *target)
{
    cairo_status_t status;
    cairo_traps_t traps;
    
    status = _cairo_clip_intersect_path (clip,
					 path, fill_rule, tolerance,
					 antialias, target);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (path,
					      fill_rule,
					      tolerance,
					      &traps);
    if (status)
	goto bail;

    status = _cairo_clip_intersect_region (clip, &traps, target);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	goto bail;

    status = _cairo_clip_intersect_mask (clip, &traps, antialias, target);
	
 bail:
    _cairo_traps_fini (&traps);

    return status;
}
