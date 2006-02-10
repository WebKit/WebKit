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
 */

#include "cairo-surface-fallback-private.h"
#include "cairo-clip-private.h"

typedef struct {
    cairo_surface_t *dst;
    cairo_rectangle_t extents;
    cairo_image_surface_t *image;
    cairo_rectangle_t image_rect;
    void *image_extra;
} fallback_state_t;

/**
 * _fallback_init:
 * 
 * Acquire destination image surface needed for an image-based
 * fallback.
 * 
 * Return value: CAIRO_INT_STATUS_NOTHING_TO_DO if the extents are not
 * visible, CAIRO_STATUS_SUCCESS if some portion is visible and all
 * went well, or some error status otherwise.
 **/
static cairo_int_status_t
_fallback_init (fallback_state_t *state,
		cairo_surface_t  *dst,
		int               x,
		int               y,
		int               width,
		int               height)
{
    cairo_status_t status;

    state->extents.x = x;
    state->extents.y = y;
    state->extents.width = width;
    state->extents.height = height;
    
    state->dst = dst;

    status = _cairo_surface_acquire_dest_image (dst, &state->extents,
						&state->image, &state->image_rect,
						&state->image_extra);
    if (status)
	return status;

    /* XXX: This NULL value tucked away in state->image is a rather
     * ugly interface. Cleaner would be to push the
     * CAIRO_INT_STATUS_NOTHING_TO_DO value down into
     * _cairo_surface_acquire_dest_image and its backend
     * counterparts. */
    if (state->image == NULL)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    return CAIRO_STATUS_SUCCESS;
}

static void
_fallback_fini (fallback_state_t *state)
{
    _cairo_surface_release_dest_image (state->dst, &state->extents,
				       state->image, &state->image_rect,
				       state->image_extra);
}

typedef cairo_status_t (*cairo_draw_func_t) (void                    *closure,
					     cairo_operator_t         op,
					     cairo_pattern_t         *src,
					     cairo_surface_t         *dst,
					     int                      dst_x,
					     int                      dst_y,
					     const cairo_rectangle_t *extents);

static cairo_status_t
_create_composite_mask_pattern (cairo_surface_pattern_t *mask_pattern,
				cairo_clip_t            *clip,
				cairo_draw_func_t        draw_func,
				void                    *draw_closure,
				cairo_surface_t         *dst,
				const cairo_rectangle_t *extents)
{
    cairo_surface_t *mask;
    cairo_status_t status;
    
    mask = cairo_surface_create_similar (dst,
					 CAIRO_CONTENT_ALPHA,
					 extents->width,
					 extents->height);
    if (mask->status)
	return CAIRO_STATUS_NO_MEMORY;
    
    status = (*draw_func) (draw_closure, CAIRO_OPERATOR_ADD,
			   NULL, mask,
			   extents->x, extents->y,
			   extents);
    if (status)
	goto CLEANUP_SURFACE;

    if (clip && clip->surface)
	status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_IN,
						 mask,
						 extents->x, extents->y,
						 extents);
    if (status)
	goto CLEANUP_SURFACE;
    
    _cairo_pattern_init_for_surface (mask_pattern, mask);

 CLEANUP_SURFACE:
    cairo_surface_destroy (mask);

    return status;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_clip_and_composite_with_mask (cairo_clip_t            *clip,
			       cairo_operator_t         op,
			       cairo_pattern_t         *src,
			       cairo_draw_func_t        draw_func,
			       void                    *draw_closure,
			       cairo_surface_t         *dst,
			       const cairo_rectangle_t *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_status_t status;

    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (status)
	return status;
	
    status = _cairo_surface_composite (op,
				       src, &mask_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&mask_pattern.base);

    return status;
}

/* Handles compositing with a clip surface when we have to do the operation
 * in two pieces and combine them together.
 */
static cairo_status_t
_clip_and_composite_combine (cairo_clip_t            *clip,
			     cairo_operator_t         op,
			     cairo_pattern_t         *src,
			     cairo_draw_func_t        draw_func,
			     void                    *draw_closure,
			     cairo_surface_t         *dst,
			     const cairo_rectangle_t *extents)
{
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t dst_pattern;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_status_t status;

    /* We'd be better off here creating a surface identical in format
     * to dst, but we have no way of getting that information.
     * A CAIRO_CONTENT_CLONE or something might be useful.
     * cairo_surface_create_similar() also unnecessarily clears the surface.
     */
    intermediate = cairo_surface_create_similar (dst,
						 CAIRO_CONTENT_COLOR_ALPHA,
						 extents->width,
						 extents->height);
    if (intermediate->status)
	return CAIRO_STATUS_NO_MEMORY;

    /* Initialize the intermediate surface from the destination surface
     */
    _cairo_pattern_init_for_surface (&dst_pattern, dst);

    /* Set a translation on dst_pattern equivalent to the surface
     * device offset, to make sure it's in the right place when
     * composited.
     */
    if (dst->device_x_offset != 0.0 ||
        dst->device_y_offset != 0.0 ||
        dst->device_x_scale != 1.0 ||
        dst->device_y_scale != 1.0)
    {
	cairo_matrix_t txmat;
        cairo_matrix_init_scale (&txmat, dst->device_x_scale, dst->device_y_scale);
        cairo_matrix_translate (&txmat, dst->device_x_offset, dst->device_y_offset);
	cairo_pattern_set_matrix ((cairo_pattern_t*) &dst_pattern, &txmat);
    }

    status = _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
				       &dst_pattern.base, NULL, intermediate,
				       extents->x,     extents->y,
				       0,              0,
				       0,              0,
				       extents->width, extents->height);

    _cairo_pattern_fini (&dst_pattern.base);

    if (status)
	goto CLEANUP_SURFACE;

    status = (*draw_func) (draw_closure, op,
			   src, intermediate,
			   extents->x, extents->y,
			   extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Combine that with the clip
     */
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_DEST_IN,
					     intermediate,
					     extents->x, extents->y,					     
					     extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Punch the clip out of the destination
     */
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_DEST_OUT,
					     dst,
					     0, 0,
					     extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Now add the two results together
     */
    _cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
    
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       &intermediate_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&intermediate_pattern.base);
    
 CLEANUP_SURFACE:
    cairo_surface_destroy (intermediate);

    return status;
}

/* Handles compositing for CAIRO_OPERATOR_SOURCE, which is special; it's
 * defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
 */
static cairo_status_t
_clip_and_composite_source (cairo_clip_t            *clip,
			    cairo_pattern_t         *src,
			    cairo_draw_func_t        draw_func,
			    void                    *draw_closure,
			    cairo_surface_t         *dst,
			    const cairo_rectangle_t *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_status_t status;

    /* Create a surface that is mask IN clip
     */
    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (status)
	return status;
    
    /* Compute dest' = dest OUT (mask IN clip)
     */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_OUT,
				       &mask_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    if (status)
	goto CLEANUP_MASK_PATTERN;

    /* Now compute (src IN (mask IN clip)) ADD dest'
     */
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       src, &mask_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

 CLEANUP_MASK_PATTERN:
    _cairo_pattern_fini (&mask_pattern.base);
    return status;
}

static int
_cairo_rectangle_empty (const cairo_rectangle_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/**
 * _clip_and_composite:
 * @clip: a #cairo_clip_t
 * @op: the operator to draw with
 * @src: source pattern
 * @draw_func: function that can be called to draw with the mask onto a surface.
 * @draw_closure: data to pass to @draw_func.
 * @dst: destination surface
 * @extents: rectangle holding a bounding box for the operation; this
 *           rectangle will be used as the size for the temporary
 *           surface.
 *
 * When there is a surface clip, we typically need to create an intermediate
 * surface. This function handles the logic of creating a temporary surface
 * drawing to it, then compositing the result onto the target surface.
 *
 * @draw_func is to called to draw the mask; it will be called no more
 * than once.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if the drawing succeeded.
 **/
static cairo_status_t
_clip_and_composite (cairo_clip_t            *clip,
		     cairo_operator_t         op,
		     cairo_pattern_t         *src,
		     cairo_draw_func_t        draw_func,
		     void                    *draw_closure,
		     cairo_surface_t         *dst,
		     const cairo_rectangle_t *extents)
{
    cairo_pattern_union_t solid_pattern;
    cairo_status_t status;

    if (_cairo_rectangle_empty (extents))
	/* Nothing to do */
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_pattern_init_solid (&solid_pattern.solid, CAIRO_COLOR_WHITE);
	src = &solid_pattern.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if ((clip && clip->surface) || op == CAIRO_OPERATOR_SOURCE)
    {
	if (op == CAIRO_OPERATOR_SOURCE)
	    status = _clip_and_composite_source (clip,
						 src,
						 draw_func, draw_closure,
						 dst, extents);
	else if (_cairo_operator_bounded_by_mask (op))
	    status = _clip_and_composite_with_mask (clip, op,
						    src,
						    draw_func, draw_closure,
						    dst, extents);
	else
	    status = _clip_and_composite_combine (clip, op,
						  src,
						  draw_func, draw_closure,
						  dst, extents);
    }
    else
    {
	status = (*draw_func) (draw_closure, op,
			       src, dst,
			       0, 0,
			       extents);
    }

    if (src == &solid_pattern.base)
	_cairo_pattern_fini (&solid_pattern.base);

    return status;
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_clip_t      *clip,
			cairo_pattern_t   *src,
			cairo_operator_t   op,
			cairo_surface_t   *dst,
			pixman_region16_t *trap_region,
			cairo_rectangle_t *extents)
{
    cairo_status_t status;
    cairo_pattern_union_t solid_pattern;
    cairo_pattern_union_t mask;
    int num_rects = pixman_region_num_rects (trap_region);
    unsigned int clip_serial;
    cairo_surface_t *clip_surface = clip ? clip->surface : NULL;

    if (clip_surface && op == CAIRO_OPERATOR_CLEAR) {
	_cairo_pattern_init_solid (&solid_pattern.solid, CAIRO_COLOR_WHITE);
	src = &solid_pattern.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;
    
    if (num_rects > 1) {
      if (_cairo_surface_get_clip_mode (dst) != CAIRO_CLIP_MODE_REGION)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	
	clip_serial = _cairo_surface_allocate_clip_serial (dst);
	status = _cairo_surface_set_clip_region (dst,
						 trap_region,
						 clip_serial);
	if (status)
	    return status;
    }
    
    if (clip_surface)
	_cairo_pattern_init_for_surface (&mask.surface, clip_surface);
	
    status = _cairo_surface_composite (op,
				       src,
				       clip_surface ? &mask.base : NULL,
				       dst,
				       extents->x, extents->y,
				       extents->x - (clip_surface ? clip->surface_rect.x : 0),
				       extents->y - (clip_surface ? clip->surface_rect.y : 0),
				       extents->x, extents->y,
				       extents->width, extents->height);

    if (clip_surface)
      _cairo_pattern_fini (&mask.base);

    if (src == &solid_pattern.base)
	_cairo_pattern_fini (&solid_pattern.base);

    return status;
}

typedef struct {
    cairo_traps_t *traps;
    cairo_antialias_t antialias;
} cairo_composite_traps_info_t;

static cairo_status_t
_composite_traps_draw_func (void                    *closure,
			    cairo_operator_t         op,
			    cairo_pattern_t         *src,
			    cairo_surface_t         *dst,
			    int                      dst_x,
			    int                      dst_y,
			    const cairo_rectangle_t *extents)
{
    cairo_composite_traps_info_t *info = closure;
    cairo_pattern_union_t pattern;
    cairo_status_t status;
    
    if (dst_x != 0 || dst_y != 0)
	_cairo_traps_translate (info->traps, - dst_x, - dst_y);

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    if (!src)
	src = &pattern.base;
    
    status = _cairo_surface_composite_trapezoids (op,
						  src, dst, info->antialias,
						  extents->x,         extents->y,
						  extents->x - dst_x, extents->y - dst_y,
						  extents->width,     extents->height,
						  info->traps->traps,
						  info->traps->num_traps);
    _cairo_pattern_fini (&pattern.base);

    return status;
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_clip_and_composite_trapezoids (cairo_pattern_t *src,
				cairo_operator_t op,
				cairo_surface_t *dst,
				cairo_traps_t *traps,
				cairo_clip_t *clip,
				cairo_antialias_t antialias)
{
    cairo_status_t status;
    pixman_region16_t *trap_region;
    pixman_region16_t *clear_region = NULL;
    cairo_rectangle_t extents;
    cairo_composite_traps_info_t traps_info;
    
    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_traps_extract_region (traps, &trap_region);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_mask (op))
    {
	if (trap_region) {
	    status = _cairo_clip_intersect_to_region (clip, trap_region);
	    _cairo_region_extents_rectangle (trap_region, &extents);
	} else {
	    cairo_box_t trap_extents;
	    _cairo_traps_extents (traps, &trap_extents);
	    _cairo_box_round_to_rectangle (&trap_extents, &extents);
	    status = _cairo_clip_intersect_to_rectangle (clip, &extents);
	}
    }
    else
    {
	cairo_surface_t *clip_surface = clip ? clip->surface : NULL;
	
	status = _cairo_surface_get_extents (dst, &extents);
	if (status)
	    return status;
	
	if (trap_region && !clip_surface) {
	    /* If we optimize drawing with an unbounded operator to
	     * _cairo_surface_fill_rectangles() or to drawing with a
	     * clip region, then we have an additional region to clear.
	     */
	    status = _cairo_surface_get_extents (dst, &extents);
	    if (status)
		return status;
	    
	    clear_region = _cairo_region_create_from_rectangle (&extents);
	    status = _cairo_clip_intersect_to_region (clip, clear_region);
	    if (status)
		return status;
	    
	    _cairo_region_extents_rectangle (clear_region,  &extents);
	    
	    if (pixman_region_subtract (clear_region, clear_region, trap_region) != PIXMAN_REGION_STATUS_SUCCESS)
		return CAIRO_STATUS_NO_MEMORY;
	    
	    if (!pixman_region_not_empty (clear_region)) {
		pixman_region_destroy (clear_region);
		clear_region = NULL;
	    }
	} else {
	    status = _cairo_clip_intersect_to_rectangle (clip, &extents);
	    if (status)
		return status;
	}
    }
	
    if (status)
	goto out;
    
    if (trap_region)
    {
	cairo_surface_t *clip_surface = clip ? clip->surface : NULL;
	
	if ((src->type == CAIRO_PATTERN_SOLID || op == CAIRO_OPERATOR_CLEAR) &&
	    !clip_surface)
	{
	    const cairo_color_t *color;

	    if (op == CAIRO_OPERATOR_CLEAR)
		color = CAIRO_COLOR_TRANSPARENT;
	    else
		color = &((cairo_solid_pattern_t *)src)->color;
	  
	    /* Solid rectangles special case */
	    status = _cairo_surface_fill_region (dst, op, color, trap_region);
	    if (!status && clear_region)
		status = _cairo_surface_fill_region (dst, CAIRO_OPERATOR_CLEAR,
						     CAIRO_COLOR_TRANSPARENT,
						     clear_region);

	    goto out;
	}

	if ((_cairo_operator_bounded_by_mask (op) && op != CAIRO_OPERATOR_SOURCE) ||
	    !clip_surface)
	{
	    /* For a simple rectangle, we can just use composite(), for more
	     * rectangles, we have to set a clip region. The cost of rasterizing
	     * trapezoids is pretty high for most backends currently, so it's
	     * worthwhile even if a region is needed.
	     *
	     * If we have a clip surface, we set it as the mask; this only works
	     * for bounded operators other than SOURCE; for unbounded operators,
	     * clip and mask cannot be interchanged. For SOURCE, the operator
	     * as implemented by the backends is different in it's handling
	     * of the mask then what we want.
	     *
	     * CAIRO_INT_STATUS_UNSUPPORTED will be returned if the region has
	     * more than rectangle and the destination doesn't support clip
	     * regions. In that case, we fall through.
	     */
	    status = _composite_trap_region (clip, src, op, dst,
					     trap_region, &extents);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    {
		if (!status && clear_region)
		    status = _cairo_surface_fill_region (dst, CAIRO_OPERATOR_CLEAR,
							 CAIRO_COLOR_TRANSPARENT,
							 clear_region);
		goto out;
	    }
	}
    }

    traps_info.traps = traps;
    traps_info.antialias = antialias;

    status = _clip_and_composite (clip, op, src,
				  _composite_traps_draw_func, &traps_info,
				  dst, &extents);

 out:
    if (trap_region)
	pixman_region_destroy (trap_region);
    if (clear_region)
	pixman_region_destroy (clear_region);
    
    return status;
}

cairo_status_t
_cairo_surface_fallback_paint (cairo_surface_t	*surface,
			       cairo_operator_t	 op,
			       cairo_pattern_t	*source)
{
    cairo_status_t status;
    cairo_rectangle_t extents;
    cairo_box_t box;
    cairo_traps_t traps;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }
    
    status = _cairo_clip_intersect_to_rectangle (surface->clip, &extents);
    if (status)
	return status;

    box.p1.x = _cairo_fixed_from_int (extents.x);
    box.p1.y = _cairo_fixed_from_int (extents.y);
    box.p2.x = _cairo_fixed_from_int (extents.x + extents.width);
    box.p2.y = _cairo_fixed_from_int (extents.y + extents.height);

    status = _cairo_traps_init_box (&traps, &box);
    if (status)
	return status;
    
    _clip_and_composite_trapezoids (source,
				    op,
				    surface,
				    &traps,
				    surface->clip,
				    CAIRO_ANTIALIAS_NONE);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_surface_mask_draw_func (void                    *closure,
			       cairo_operator_t         op,
			       cairo_pattern_t         *src,
			       cairo_surface_t         *dst,
			       int                      dst_x,
			       int                      dst_y,
			       const cairo_rectangle_t *extents)
{
    cairo_pattern_t *mask = closure;

    if (src)
	return _cairo_surface_composite (op,
					 src, mask, dst,
					 extents->x,         extents->y,
					 extents->x,         extents->y,
					 extents->x - dst_x, extents->y - dst_y,
					 extents->width,     extents->height);
    else
	return _cairo_surface_composite (op,
					 mask, NULL, dst,
					 extents->x,         extents->y,
					 0,                  0, /* unused */
					 extents->x - dst_x, extents->y - dst_y,
					 extents->width,     extents->height);
}

cairo_status_t
_cairo_surface_fallback_mask (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_pattern_t		*mask)
{
    cairo_status_t status;
    cairo_rectangle_t extents, source_extents, mask_extents;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }
    
    if (_cairo_operator_bounded_by_mask (op)) {
	status = _cairo_pattern_get_extents (mask, &mask_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &mask_extents);
    }

    status = _cairo_clip_intersect_to_rectangle (surface->clip, &extents);
    if (status)
	return status;

    status = _clip_and_composite (surface->clip, op,
				  source,
				  _cairo_surface_mask_draw_func,
				  mask,
				  surface,
				  &extents);

    return status;
}

cairo_status_t
_cairo_surface_fallback_stroke (cairo_surface_t		*surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				cairo_path_fixed_t	*path,
				cairo_stroke_style_t	*stroke_style,
				cairo_matrix_t		*ctm,
				cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias)
{
    cairo_status_t status;
    cairo_traps_t traps;
    
    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path,
						stroke_style,
						ctm, ctm_inverse,
						tolerance,
						&traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _clip_and_composite_trapezoids (source,
				    op,
				    surface,
				    &traps,
				    surface->clip,
				    antialias);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_fallback_fill (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias)
{
    cairo_status_t status;
    cairo_traps_t traps;

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      fill_rule,
					      tolerance,
					      &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    status = _clip_and_composite_trapezoids (source,
					     op,
					     surface,
					     &traps,
					     surface->clip,
					     antialias);

    _cairo_traps_fini (&traps);

    return status;
}

typedef struct {
    cairo_scaled_font_t *font;
    const cairo_glyph_t *glyphs;
    int num_glyphs;
} cairo_show_glyphs_info_t;

static cairo_status_t
_cairo_surface_old_show_glyphs_draw_func (void                    *closure,
					  cairo_operator_t         op,
					  cairo_pattern_t         *src,
					  cairo_surface_t         *dst,
					  int                      dst_x,
					  int                      dst_y,
					  const cairo_rectangle_t *extents)
{
    cairo_show_glyphs_info_t *glyph_info = closure;
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    /* Modifying the glyph array is fine because we know that this function
     * will be called only once, and we've already made a copy of the
     * glyphs in the wrapper.
     */
    if (dst_x != 0 || dst_y != 0) {
	int i;
	
	for (i = 0; i < glyph_info->num_glyphs; ++i)
	{
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].x -= dst_x;
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].y -= dst_y;
	}
    }

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    if (!src)
	src = &pattern.base;
    
    status = _cairo_surface_old_show_glyphs (glyph_info->font, op, src, 
					     dst,
					     extents->x, extents->y,
					     extents->x - dst_x,
					     extents->y - dst_y,
					     extents->width,
					     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs);

    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;
    
    status = _cairo_scaled_font_show_glyphs (glyph_info->font, 
					     op, 
					     src, dst,
					     extents->x,         extents->y,
					     extents->x - dst_x + dst->device_x_offset,
					     extents->y - dst_y + dst->device_y_offset,
					     extents->width,     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs);

    if (src == &pattern.base)
	_cairo_pattern_fini (&pattern.base);

    return status;
}

cairo_status_t
_cairo_surface_fallback_show_glyphs (cairo_surface_t		*surface,
				     cairo_operator_t		 op,
				     cairo_pattern_t		*source,
				     const cairo_glyph_t	*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font)
{
    cairo_status_t status;
    cairo_rectangle_t extents, glyph_extents;
    cairo_show_glyphs_info_t glyph_info;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_mask (op)) {
	status = _cairo_scaled_font_glyph_device_extents (scaled_font,
							  glyphs, 
							  num_glyphs, 
							  &glyph_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &glyph_extents);
    }
    
    status = _cairo_clip_intersect_to_rectangle (surface->clip, &extents);
    if (status)
	return status;
    
    glyph_info.font = scaled_font;
    glyph_info.glyphs = glyphs;
    glyph_info.num_glyphs = num_glyphs;
    
    status = _clip_and_composite (surface->clip,
				  op,
				  source,
				  _cairo_surface_old_show_glyphs_draw_func,
				  &glyph_info,
				  surface,
				  &extents);
    
    return status;
}

cairo_surface_t *
_cairo_surface_fallback_snapshot (cairo_surface_t *surface)
{
    cairo_surface_t *snapshot;
    cairo_status_t status;
    cairo_pattern_union_t pattern;
    cairo_image_surface_t *image;
    void *image_extra;

    status = _cairo_surface_acquire_source_image (surface,
						  &image, &image_extra);
    if (status != CAIRO_STATUS_SUCCESS)
	return (cairo_surface_t *) &_cairo_surface_nil;

    snapshot = cairo_image_surface_create (image->format,
					   image->width,
					   image->height);
    if (cairo_surface_status (snapshot))
	return snapshot;

    _cairo_pattern_init_for_surface (&pattern.surface, &image->base);

    _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
			      &pattern.base,
			      NULL,
			      snapshot,
			      0, 0,
			      0, 0,
			      0, 0,
			      image->width,
			      image->height);

    _cairo_pattern_fini (&pattern.base);

    _cairo_surface_release_source_image (surface,
					 image, &image_extra);

    snapshot->device_x_offset = surface->device_x_offset;
    snapshot->device_y_offset = surface->device_y_offset;
    snapshot->device_x_scale = surface->device_x_scale;
    snapshot->device_y_scale = surface->device_y_scale;

    snapshot->is_snapshot = TRUE;

    return snapshot;
}

cairo_status_t
_cairo_surface_fallback_composite (cairo_operator_t	op,
				   cairo_pattern_t	*src,
				   cairo_pattern_t	*mask,
				   cairo_surface_t	*dst,
				   int			src_x,
				   int			src_y,
				   int			mask_x,
				   int			mask_y,
				   int			dst_x,
				   int			dst_y,
				   unsigned int		width,
				   unsigned int		height)
{
    fallback_state_t state;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (status) {
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return CAIRO_STATUS_SUCCESS;
	return status;
    }

    /* We know this will never fail with the image backend; but
     * instead of calling into it directly, we call
     * _cairo_surface_composite so that we get the correct device
     * offset handling.
     */
    status = _cairo_surface_composite (op, src, mask,
				       &state.image->base,
				       src_x, src_y, mask_x, mask_y,
				       dst_x - state.image_rect.x,
				       dst_y - state.image_rect.y,
				       width, height);
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_fill_rectangles (cairo_surface_t	*surface,
					 cairo_operator_t	 op,
					 const cairo_color_t	*color,
					 cairo_rectangle_t	*rects,
					 int			 num_rects)
{
    fallback_state_t state;
    cairo_rectangle_t *offset_rects = NULL;
    cairo_status_t status;
    int x1, y1, x2, y2;
    int i;

    assert (! surface->is_snapshot);

    if (num_rects <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* Compute the bounds of the rectangles, so that we know what area of the
     * destination surface to fetch
     */
    x1 = rects[0].x;
    y1 = rects[0].y;
    x2 = rects[0].x + rects[0].width;
    y2 = rects[0].y + rects[0].height;
    
    for (i = 1; i < num_rects; i++) {
	if (rects[i].x < x1)
	    x1 = rects[i].x;
	if (rects[i].y < y1)
	    y1 = rects[i].y;

	if (rects[i].x + rects[i].width > x2)
	    x2 = rects[i].x + rects[i].width;
	if (rects[i].y + rects[i].height > y2)
	    y2 = rects[i].y + rects[i].height;
    }

    status = _fallback_init (&state, surface, x1, y1, x2 - x1, y2 - y1);
    if (status) {
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return CAIRO_STATUS_SUCCESS;
	return status;
    }

    /* If the fetched image isn't at 0,0, we need to offset the rectangles */
    
    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_rects = malloc (sizeof (cairo_rectangle_t) * num_rects);
	if (offset_rects == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto DONE;
	}

	for (i = 0; i < num_rects; i++) {
	    offset_rects[i].x = rects[i].x - state.image_rect.x;
	    offset_rects[i].y = rects[i].y - state.image_rect.y;
	    offset_rects[i].width = rects[i].width;
	    offset_rects[i].height = rects[i].height;
	}

	rects = offset_rects;
    }

    status = _cairo_surface_fill_rectangles (&state.image->base,
					     op, color,
					     rects, num_rects);

    free (offset_rects);

 DONE:
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_composite_trapezoids (cairo_operator_t		op,
					      cairo_pattern_t	       *pattern,
					      cairo_surface_t	       *dst,
					      cairo_antialias_t		antialias,
					      int			src_x,
					      int			src_y,
					      int			dst_x,
					      int			dst_y,
					      unsigned int		width,
					      unsigned int		height,
					      cairo_trapezoid_t	       *traps,
					      int			num_traps)
{
    fallback_state_t state;
    cairo_trapezoid_t *offset_traps = NULL;
    cairo_status_t status;
    int i;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (status) {
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return CAIRO_STATUS_SUCCESS;
	return status;
    }

    /* If the destination image isn't at 0,0, we need to offset the trapezoids */
    
    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_traps = malloc (sizeof (cairo_trapezoid_t) * num_traps);
	if (!offset_traps) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto DONE;
	}

	_cairo_trapezoid_array_translate_and_scale (offset_traps, traps, num_traps,
                                                    - state.image_rect.x, - state.image_rect.y,
                                                    1.0, 1.0);
	traps = offset_traps;
    }

    _cairo_surface_composite_trapezoids (op, pattern,
					 &state.image->base,
					 antialias,
					 src_x, src_y,
					 dst_x - state.image_rect.x,
					 dst_y - state.image_rect.y,
					 width, height, traps, num_traps);
    if (offset_traps)
	free (offset_traps);

 DONE:
    _fallback_fini (&state);
    
    return status;
}
