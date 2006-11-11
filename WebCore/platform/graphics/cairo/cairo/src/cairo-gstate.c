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

#include <stdlib.h>

#include "cairoint.h"

#include "cairo-clip-private.h"
#include "cairo-gstate-private.h"

static cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target);

static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other);

static void
_cairo_gstate_fini (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate);

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate);

/**
 * _cairo_gstate_create:
 * @target: a #cairo_surface_t, not NULL
 *
 * Create a new #cairo_gstate_t to draw to target with all graphics
 * state parameters set to defaults. gstate->next will be set to NULL
 * and may be used by the caller to chain #cairo_gstate_t objects
 * together.
 *
 * Return value: a new #cairo_gstate_t or NULL if there is
 * insufficient memory.
 **/
cairo_gstate_t *
_cairo_gstate_create (cairo_surface_t *target)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    assert (target != NULL);

    gstate = malloc (sizeof (cairo_gstate_t));
    if (gstate == NULL)
	return NULL;

    status = _cairo_gstate_init (gstate, target);
    if (status) {
	free (gstate);
	return NULL;		
    }

    return gstate;
}

static cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target)
{
    gstate->op = CAIRO_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;
    gstate->antialias = CAIRO_ANTIALIAS_DEFAULT;

    _cairo_stroke_style_init (&gstate->stroke_style);

    gstate->fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;

    gstate->font_face = NULL;
    gstate->scaled_font = NULL;

    cairo_matrix_init_scale (&gstate->font_matrix,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE, 
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE);

    _cairo_font_options_init_default (&gstate->font_options);
    
    _cairo_clip_init (&gstate->clip, target);

    gstate->target = cairo_surface_reference (target);
    gstate->parent_target = NULL;
    gstate->original_target = cairo_surface_reference (target);

    _cairo_gstate_identity_matrix (gstate);
    gstate->source_ctm_inverse = gstate->ctm_inverse;

    gstate->source = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK);
    if (gstate->source->status)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_init_copy:
 *
 * Initialize @gstate by performing a deep copy of state fields from
 * @other. Note that gstate->next is not copied but is set to NULL by
 * this function.
 **/
static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other)
{
    cairo_status_t status;
    
    gstate->op = other->op;

    gstate->tolerance = other->tolerance;
    gstate->antialias = other->antialias;

    status = _cairo_stroke_style_init_copy (&gstate->stroke_style,
					    &other->stroke_style);
    if (status)
	return status;

    gstate->fill_rule = other->fill_rule;

    gstate->font_face = cairo_font_face_reference (other->font_face);
    gstate->scaled_font = cairo_scaled_font_reference (other->scaled_font);

    gstate->font_matrix = other->font_matrix;

    _cairo_font_options_init_copy (&gstate->font_options , &other->font_options);

    _cairo_clip_init_copy (&gstate->clip, &other->clip);

    gstate->target = cairo_surface_reference (other->target);
    /* parent_target is always set to NULL; it's only ever set by redirect_target */
    gstate->parent_target = NULL;
    gstate->original_target = cairo_surface_reference (other->original_target);

    gstate->ctm = other->ctm;
    gstate->ctm_inverse = other->ctm_inverse;
    gstate->source_ctm_inverse = other->source_ctm_inverse;

    gstate->source = cairo_pattern_reference (other->source);

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    _cairo_stroke_style_fini (&gstate->stroke_style);

    cairo_font_face_destroy (gstate->font_face);
    gstate->font_face = NULL;

    cairo_scaled_font_destroy (gstate->scaled_font);
    gstate->scaled_font = NULL;

    _cairo_clip_fini (&gstate->clip);

    cairo_surface_destroy (gstate->target);
    gstate->target = NULL;

    cairo_surface_destroy (gstate->parent_target);
    gstate->parent_target = NULL;

    cairo_surface_destroy (gstate->original_target);
    gstate->target = NULL;

    cairo_pattern_destroy (gstate->source);
    gstate->source = NULL;
}

void
_cairo_gstate_destroy (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return;

    _cairo_gstate_fini (gstate);
    free (gstate);
}

/**
 * _cairo_gstate_clone:
 * @other: a #cairo_gstate_t to be copied, not NULL.
 *
 * Create a new #cairo_gstate_t setting all graphics state parameters
 * to the same values as contained in @other. gstate->next will be set
 * to NULL and may be used by the caller to chain cairo_gstate_t
 * objects together.
 *
 * Return value: a new cairo_gstate_t or NULL if there is insufficient
 * memory.
 **/
cairo_gstate_t*
_cairo_gstate_clone (cairo_gstate_t *other)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    assert (other != NULL);

    gstate = malloc (sizeof (cairo_gstate_t));
    if (gstate == NULL)
	return NULL;

    status = _cairo_gstate_init_copy (gstate, other);
    if (status) {
	free (gstate);
	return NULL;
    }

    return gstate;
}

static cairo_status_t
_cairo_gstate_recursive_apply_clip_path (cairo_gstate_t *gstate,
					 cairo_clip_path_t *cpath)
{
    cairo_status_t status;

    if (cpath == NULL)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_gstate_recursive_apply_clip_path (gstate, cpath->prev);
    if (status)
	return status;

    return _cairo_clip_clip (&gstate->clip,
			     &cpath->path,
			     cpath->fill_rule,
			     cpath->tolerance,
			     cpath->antialias,
			     gstate->target);
}

/**
 * _cairo_gstate_redirect_target:
 * @gstate: a #cairo_gstate_t
 * @child: the new child target
 *
 * Redirect @gstate rendering to a "child" target. The original
 * "parent" target with which the gstate was created will not be
 * affected. See _cairo_gstate_get_target().
 *
 * Unless the redirected target has the same device offsets as the
 * original #cairo_t target, the clip will be INVALID after this call,
 * and the caller should either recreate or reset the clip.
 **/
void
_cairo_gstate_redirect_target (cairo_gstate_t *gstate, cairo_surface_t *child)
{
    /* If this gstate is already redirected, this is an error; we need a
     * new gstate to be able to redirect */
    assert (gstate->parent_target == NULL);

    /* Set up our new parent_target based on our current target;
     * gstate->parent_target will take the ref that is held by gstate->target
     */
    cairo_surface_destroy (gstate->parent_target);
    gstate->parent_target = gstate->target;

    /* Now set up our new target; we overwrite gstate->target directly,
     * since its ref is now owned by gstate->parent_target */
    gstate->target = cairo_surface_reference (child);

    _cairo_clip_fini (&gstate->clip);
    _cairo_clip_init_deep_copy (&gstate->clip, &gstate->next->clip, child);

    /* The clip is in surface backend coordinates for the previous target;
     * translate it into the child's backend coordinates. */
    _cairo_clip_translate (&gstate->clip,
                           _cairo_fixed_from_double (child->device_x_offset - gstate->parent_target->device_x_offset),
                           _cairo_fixed_from_double (child->device_y_offset - gstate->parent_target->device_y_offset));
}

/**
 * _cairo_gstate_is_redirected
 * @gstate: a #cairo_gstate_t
 *
 * Return value: TRUE if the gstate is redirected to a traget
 * different than the original, FALSE otherwise.
 **/
cairo_bool_t
_cairo_gstate_is_redirected (cairo_gstate_t *gstate)
{
    return (gstate->target != gstate->original_target);
}

/**
 * _cairo_gstate_get_target:
 * @gstate: a #cairo_gstate_t
 *
 * Return the current drawing target; if drawing is not redirected,
 * this will be the same as _cairo_gstate_get_original_target().
 *
 * Return value: the current target surface
 **/
cairo_surface_t *
_cairo_gstate_get_target (cairo_gstate_t *gstate)
{
    return gstate->target;
}

/**
 * _cairo_gstate_get_parent_target:
 * @gstate: a #cairo_gstate_t
 *
 * Return the parent surface of the current drawing target surface;
 * if this particular gstate isn't a redirect gstate, this will return NULL.
 **/
cairo_surface_t *
_cairo_gstate_get_parent_target (cairo_gstate_t *gstate)
{
    return gstate->parent_target;
}

/**
 * _cairo_gstate_get_original_target:
 * @gstate: a #cairo_gstate_t
 *
 * Return the original target with which @gstate was created. This
 * function always returns the original target independent of any
 * child target that may have been set with
 * _cairo_gstate_redirect_target.
 *
 * Return value: the original target surface
 **/
cairo_surface_t *
_cairo_gstate_get_original_target (cairo_gstate_t *gstate)
{
    return gstate->original_target;
}

/**
 * _cairo_gstate_get_target_offsets_from_original
 * @gstate: a #cairo_gstate_t
 * @dx: device offset from gstate original target
 * @dy: device offset from gstate original target
 *
 * Return the device offsets in dx, dy for the current group target
 * from the original target at the top of the gstate chain.
 **/
void
_cairo_gstate_get_target_offsets_from_original (cairo_gstate_t *gstate,
                                                double *dx,
                                                double *dy)
{
    /* Because the device offsets for the current group target are
     * always relative to its parent, we have to walk up the gstate
     * stack to figure out the actual device offsets. */

    double x = 0.0, y = 0.0;
    double prevx = 0.0, prevy = 0.0;
    while (gstate) {
        if (gstate->parent_target) {
            /* The device offset on a group surface is relative to its
             * parent; we need to recover the offset to the actual
             * top-level surface origin.  So we increase the offsets
             * by the difference between the previous (child) and the
             * current (parent).  We only check for
             * gstate->parent_target to catch the actual redirection
             * levels; we then use the target field in the gstate,
             * which is the actual group target at that point.*/
            x += (prevx - gstate->target->device_x_offset);
            y += (prevy - gstate->target->device_y_offset);

            prevx = gstate->target->device_x_offset;
            prevy = gstate->target->device_y_offset;
        }
        gstate = gstate->next;
    }

    if (dx)
        *dx = x;
    if (dy)
        *dy = y;
}

/**
 * _cairo_gstate_get_clip:
 * @gstate: a #cairo_gstate_t
 *
 * Return value: a pointer to the gstate's cairo_clip_t structure.
 */
cairo_clip_t *
_cairo_gstate_get_clip (cairo_gstate_t *gstate)
{
    return &gstate->clip;
}

cairo_status_t
_cairo_gstate_set_source (cairo_gstate_t  *gstate,
			  cairo_pattern_t *source)
{
    if (source->status)
	return source->status;

    cairo_pattern_reference (source);
    cairo_pattern_destroy (gstate->source);
    gstate->source = source;
    gstate->source_ctm_inverse = gstate->ctm_inverse;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_pattern_t *
_cairo_gstate_get_source (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->source;
}

cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t op)
{
    gstate->op = op;

    return CAIRO_STATUS_SUCCESS;
}

cairo_operator_t
_cairo_gstate_get_operator (cairo_gstate_t *gstate)
{
    return gstate->op;
}

cairo_status_t
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance)
{
    gstate->tolerance = tolerance;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_tolerance (cairo_gstate_t *gstate)
{
    return gstate->tolerance;
}

cairo_status_t
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule)
{
    gstate->fill_rule = fill_rule;

    return CAIRO_STATUS_SUCCESS;
}

cairo_fill_rule_t
_cairo_gstate_get_fill_rule (cairo_gstate_t *gstate)
{
    return gstate->fill_rule;
}

cairo_status_t
_cairo_gstate_set_line_width (cairo_gstate_t *gstate, double width)
{
    gstate->stroke_style.line_width = width;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_line_width (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_width;
}

cairo_status_t
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap)
{
    gstate->stroke_style.line_cap = line_cap;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_cap_t
_cairo_gstate_get_line_cap (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_cap;
}

cairo_status_t
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join)
{
    gstate->stroke_style.line_join = line_join;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_join_t
_cairo_gstate_get_line_join (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_join;
}

cairo_status_t
_cairo_gstate_set_dash (cairo_gstate_t *gstate, double *dash, int num_dashes, double offset)
{
    int i;
    double dash_total;

    if (gstate->stroke_style.dash)
	free (gstate->stroke_style.dash);
    
    gstate->stroke_style.num_dashes = num_dashes;

    if (gstate->stroke_style.num_dashes == 0) {
	gstate->stroke_style.dash = NULL;
	gstate->stroke_style.dash_offset = 0.0;
	return CAIRO_STATUS_SUCCESS;
    }

    gstate->stroke_style.dash = malloc (gstate->stroke_style.num_dashes * sizeof (double));
    if (gstate->stroke_style.dash == NULL) {
	gstate->stroke_style.num_dashes = 0;
	return CAIRO_STATUS_NO_MEMORY;
    }

    memcpy (gstate->stroke_style.dash, dash, gstate->stroke_style.num_dashes * sizeof (double));
    
    dash_total = 0.0;
    for (i = 0; i < gstate->stroke_style.num_dashes; i++) {
	if (gstate->stroke_style.dash[i] < 0)
	    return CAIRO_STATUS_INVALID_DASH;
	dash_total += gstate->stroke_style.dash[i];
    }

    if (dash_total == 0.0)
	return CAIRO_STATUS_INVALID_DASH;

    /* A single dash value indicate symmetric repeating, so the total
     * is twice as long. */
    if (gstate->stroke_style.num_dashes == 1)
	dash_total *= 2;

    /* The dashing code doesn't like a negative offset, so we compute
     * the equivalent positive offset. */
    if (offset < 0)
	offset += ceil (-offset / dash_total + 0.5) * dash_total;

    gstate->stroke_style.dash_offset = offset;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit)
{
    gstate->stroke_style.miter_limit = limit;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_miter_limit (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.miter_limit;
}

void
_cairo_gstate_get_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix)
{
    *matrix = gstate->ctm;
}

cairo_status_t
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_translate (&tmp, tx, ty);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_translate (&tmp, -tx, -ty);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy)
{
    cairo_matrix_t tmp;

    if (sx == 0 || sy == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_scale (&tmp, sx, sy);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_scale (&tmp, 1/sx, 1/sy);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_rotate (&tmp, angle);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_rotate (&tmp, -angle);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform (cairo_gstate_t	      *gstate,
			 const cairo_matrix_t *matrix)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    tmp = *matrix;
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_invert (&tmp);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_matrix (cairo_gstate_t       *gstate,
			  const cairo_matrix_t *matrix)
{
    cairo_status_t status;

    _cairo_gstate_unset_scaled_font (gstate);
    
    gstate->ctm = *matrix;

    gstate->ctm_inverse = *matrix;
    status = cairo_matrix_invert (&gstate->ctm_inverse);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate)
{
    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_identity (&gstate->ctm);
    cairo_matrix_init_identity (&gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm_inverse, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_user_to_backend (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);
}

void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);
}

/* XXX: NYI 
cairo_status_t
_cairo_gstate_stroke_to_path (cairo_gstate_t *gstate)
{
    cairo_status_t status;

    _cairo_pen_init (&gstate);
    return CAIRO_STATUS_SUCCESS;
}
*/

static void
_cairo_gstate_copy_transformed_pattern (cairo_gstate_t  *gstate,
					cairo_pattern_t *pattern,
					cairo_pattern_t *original,
					cairo_matrix_t  *ctm_inverse)
{
    cairo_surface_pattern_t *surface_pattern;
    cairo_surface_t *surface;
    cairo_matrix_t offset_matrix;

    _cairo_pattern_init_copy (pattern, original);
    _cairo_pattern_transform (pattern, ctm_inverse);

    if (cairo_pattern_get_type (original) == CAIRO_PATTERN_TYPE_SURFACE) {
        surface_pattern = (cairo_surface_pattern_t *) original;
        surface = surface_pattern->surface;
        if (_cairo_surface_has_device_offset_or_scale (surface)) {
            cairo_matrix_init_translate (&offset_matrix,
                                         surface->device_x_offset,
                                         surface->device_y_offset);
            _cairo_pattern_transform (pattern, &offset_matrix);
        }
    }

}

static void
_cairo_gstate_copy_transformed_source (cairo_gstate_t  *gstate,
				       cairo_pattern_t *pattern)
{
    _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					    gstate->source,
					    &gstate->source_ctm_inverse);
}

static void
_cairo_gstate_copy_transformed_mask (cairo_gstate_t  *gstate,
				     cairo_pattern_t *pattern,
				     cairo_pattern_t *mask)
{
    _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					    mask,
					    &gstate->ctm_inverse);
}

cairo_status_t
_cairo_gstate_paint (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &pattern.base);

    status = _cairo_surface_paint (gstate->target,
				   gstate->op,
				   &pattern.base);

    _cairo_pattern_fini (&pattern.base);

    return status;
}

/**
 * _cairo_operator_bounded_by_mask:
 * @op: a #cairo_operator_t
 * 
 * A bounded operator is one where mask pixel
 * of zero results in no effect on the destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, draw trapezoids with an unbounded operator, the effect
 * extends past the bounding box of the trapezoids.
 *
 * Return value: %TRUE if the operator is bounded by the mask operand
 **/
cairo_bool_t
_cairo_operator_bounded_by_mask (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }
    
    ASSERT_NOT_REACHED;
    return FALSE;
}

/**
 * _cairo_operator_bounded_by_source:
 * @op: a #cairo_operator_t
 * 
 * A bounded operator is one where source pixels of zero
 * (in all four components, r, g, b and a) effect no change
 * in the resulting destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, copy a surface with the SOURCE operator, the effect
 * extends past the bounding box of the source surface.
 *
 * Return value: %TRUE if the operator is bounded by the source operand
 **/
cairo_bool_t
_cairo_operator_bounded_by_source (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }
    
    ASSERT_NOT_REACHED;
    return FALSE;
}

cairo_status_t
_cairo_gstate_mask (cairo_gstate_t  *gstate,
		    cairo_pattern_t *mask)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern, mask_pattern;

    if (mask->status)
	return mask->status;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);
    _cairo_gstate_copy_transformed_mask (gstate, &mask_pattern.base, mask);

    status = _cairo_surface_mask (gstate->target,
				  gstate->op,
				  &source_pattern.base,
				  &mask_pattern.base);

    _cairo_pattern_fini (&source_pattern.base);
    _cairo_pattern_fini (&mask_pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern;

    if (gstate->source->status)
	return gstate->source->status;

    if (gstate->stroke_style.line_width <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);

    status = _cairo_surface_stroke (gstate->target,
				    gstate->op,
				    &source_pattern.base,
				    path,
				    &gstate->stroke_style,
				    &gstate->ctm,
				    &gstate->ctm_inverse,
				    gstate->tolerance,
				    gstate->antialias);

    _cairo_pattern_fini (&source_pattern.base);
    
    return status;

}

cairo_status_t
_cairo_gstate_in_stroke (cairo_gstate_t	    *gstate,
			 cairo_path_fixed_t *path,
			 double		     x,
			 double		     y,
			 cairo_bool_t	    *inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path,
						&gstate->stroke_style,
						&gstate->ctm,
						&gstate->ctm_inverse,
						gstate->tolerance,
						&traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

/* XXX We currently have a confusing mix of boxes and rectangles as
 * exemplified by this function.  A cairo_box_t is a rectangular area
 * represented by the coordinates of the upper left and lower right
 * corners, expressed in fixed point numbers.  A cairo_rectangle_t is
 * also a rectangular area, but represented by the upper left corner
 * and the width and the height, as integer numbers.
 *
 * This function converts a cairo_box_t to a cairo_rectangle_t by
 * increasing the area to the nearest integer coordinates.  We should
 * standardize on cairo_rectangle_t and cairo_rectangle_fixed_t, and
 * this function could be renamed to the more reasonable
 * _cairo_rectangle_fixed_round.
 */

void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

void
_cairo_rectangle_intersect (cairo_rectangle_t *dest, cairo_rectangle_t *src)
{
    int x1, y1, x2, y2;

    x1 = MAX (dest->x, src->x);
    y1 = MAX (dest->y, src->y);
    x2 = MIN (dest->x + dest->width, src->x + src->width);
    y2 = MIN (dest->y + dest->height, src->y + src->height);

    if (x1 >= x2 || y1 >= y2) {
	dest->x = 0;
	dest->y = 0;
	dest->width = 0;
	dest->height = 0;
    } else {
	dest->x = x1;
	dest->y = y1;
	dest->width = x2 - x1;
	dest->height = y2 - y1;
    }	
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;

    if (gstate->source->status)
	return gstate->source->status;
    
    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;
  
    _cairo_gstate_copy_transformed_source (gstate, &pattern.base);

    status = _cairo_surface_fill (gstate->target,
				  gstate->op,
				  &pattern.base,
				  path,
				  gstate->fill_rule,
				  gstate->tolerance,
				  gstate->antialias);

    _cairo_pattern_fini (&pattern.base);
    
    return status;
}

cairo_status_t
_cairo_gstate_in_fill (cairo_gstate_t	  *gstate,
		       cairo_path_fixed_t *path,
		       double		   x,
		       double		   y,
		       cairo_bool_t	  *inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);
    
BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_copy_page (cairo_gstate_t *gstate)
{
    cairo_int_status_t status;

    status = _cairo_surface_copy_page (gstate->target);

    /* It's fine if some surfaces just don't support this. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_STATUS_SUCCESS;

    return status;
}

cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate)
{
    cairo_int_status_t status;

    status = _cairo_surface_show_page (gstate->target);

    /* It's fine if some surfaces just don't support this. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_STATUS_SUCCESS;

    return status;
}

cairo_status_t
_cairo_gstate_stroke_extents (cairo_gstate_t	 *gstate,
			      cairo_path_fixed_t *path,
                              double *x1, double *y1,
			      double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_stroke_to_traps (path,
						&gstate->stroke_style,
						&gstate->ctm,
						&gstate->ctm_inverse,
						gstate->tolerance,
						&traps);
    if (status)
	goto BAIL;

    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_fill_extents (cairo_gstate_t     *gstate,
			    cairo_path_fixed_t *path,
                            double *x1, double *y1,
			    double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status)
	goto BAIL;
  
    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_reset_clip (cairo_gstate_t *gstate)
{
    return _cairo_clip_reset (&gstate->clip);
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    return _cairo_clip_clip (&gstate->clip,
			     path, gstate->fill_rule, gstate->tolerance,
			     gstate->antialias, gstate->target);
}

cairo_bool_t
_cairo_gstate_has_clip (cairo_gstate_t *gstate)
{
   return _cairo_clip_has_clip (&gstate->clip);
}

cairo_bool_t
_cairo_gstate_extract_clip_rectangles (cairo_gstate_t *gstate,
                                       int max_rectangles,
                                       cairo_clip_rect_t *rectangles_out,
                                       int *num_rectangles_out)
{
    return _cairo_clip_extract_rectangles (&gstate->clip,
                                           max_rectangles,
                                           rectangles_out,
                                           num_rectangles_out);
}

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate)
{
    if (gstate->scaled_font) {
	cairo_scaled_font_destroy (gstate->scaled_font);
	gstate->scaled_font = NULL;
    }
}

cairo_status_t
_cairo_gstate_select_font_face (cairo_gstate_t       *gstate, 
				const char           *family, 
				cairo_font_slant_t    slant, 
				cairo_font_weight_t   weight)
{
    cairo_font_face_t *font_face;

    font_face = _cairo_toy_font_face_create (family, slant, weight);
    if (font_face->status)
	return font_face->status;

    _cairo_gstate_set_font_face (gstate, font_face);
    cairo_font_face_destroy (font_face);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_size (cairo_gstate_t *gstate, 
			     double          size)
{
    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_scale (&gstate->font_matrix, size, size);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_matrix (cairo_gstate_t	    *gstate, 
			       const cairo_matrix_t *matrix)
{
    _cairo_gstate_unset_scaled_font (gstate);

    gstate->font_matrix = *matrix;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_get_font_matrix (cairo_gstate_t *gstate,
			       cairo_matrix_t *matrix)
{
    *matrix = gstate->font_matrix;
}

cairo_status_t
_cairo_gstate_set_font_options (cairo_gstate_t             *gstate,
				const cairo_font_options_t *options)
{
    _cairo_gstate_unset_scaled_font (gstate);

    gstate->font_options = *options;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_get_font_options (cairo_gstate_t       *gstate,
				cairo_font_options_t *options)
{
    *options = gstate->font_options;
}

cairo_status_t
_cairo_gstate_get_font_face (cairo_gstate_t     *gstate,
			     cairo_font_face_t **font_face)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;
    
    *font_face = gstate->font_face;

    return CAIRO_STATUS_SUCCESS;
}

/* 
 * Like everything else in this file, fonts involve Too Many Coordinate Spaces;
 * it is easy to get confused about what's going on.
 *
 * The user's view
 * ---------------
 *
 * Users ask for things in user space. When cairo starts, a user space unit
 * is about 1/96 inch, which is similar to (but importantly different from)
 * the normal "point" units most users think in terms of. When a user
 * selects a font, its scale is set to "one user unit". The user can then
 * independently scale the user coordinate system *or* the font matrix, in
 * order to adjust the rendered size of the font.
 *
 * Metrics are returned in user space, whether they are obtained from
 * the currently selected font in a  #cairo_t or from a #cairo_scaled_font_t
 * which is a font specialized to a particular scale matrix, CTM, and target
 * surface. 
 *
 * The font's view
 * ---------------
 *
 * Fonts are designed and stored (in say .ttf files) in "font space", which
 * describes an "EM Square" (a design tile) and has some abstract number
 * such as 1000, 1024, or 2048 units per "EM". This is basically an
 * uninteresting space for us, but we need to remember that it exists.
 *
 * Font resources (from libraries or operating systems) render themselves
 * to a particular device. Since they do not want to make most programmers
 * worry about the font design space, the scaling API is simplified to
 * involve just telling the font the required pixel size of the EM square
 * (that is, in device space).
 *
 *
 * Cairo's gstate view
 * -------------------
 *
 * In addition to the CTM and CTM inverse, we keep a matrix in the gstate
 * called the "font matrix" which describes the user's most recent
 * font-scaling or font-transforming request. This is kept in terms of an
 * abstract scale factor, composed with the CTM and used to set the font's
 * pixel size. So if the user asks to "scale the font by 12", the matrix
 * is:
 *
 *   [ 12.0, 0.0, 0.0, 12.0, 0.0, 0.0 ]
 *
 * It is an affine matrix, like all cairo matrices, but its tx and ty
 * components are always set to zero; we don't permit "nudging" fonts
 * around.
 *
 * In order to perform any action on a font, we must build an object
 * called a cairo_font_scale_t; this contains the central 2x2 matrix 
 * resulting from "font matrix * CTM".
 *  
 * We pass this to the font when making requests of it, which causes it to
 * reply for a particular [user request, device] combination, under the CTM
 * (to accomodate the "zoom in" == "bigger fonts" issue above).
 *
 * The other terms in our communication with the font are therefore in
 * device space. When we ask it to perform text->glyph conversion, it will
 * produce a glyph string in device space. Glyph vectors we pass to it for
 * measuring or rendering should be in device space. The metrics which we
 * get back from the font will be in device space. The contents of the
 * global glyph image cache will be in device space.
 *
 *
 * Cairo's public view
 * -------------------
 *
 * Since the values entering and leaving via public API calls are in user
 * space, the gstate functions typically need to multiply arguments by the
 * CTM (for user-input glyph vectors), and return values by the CTM inverse
 * (for font responses such as metrics or glyph vectors).
 *
 */

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate)
{
    if (!gstate->font_face) {
	cairo_font_face_t *font_face;

	font_face = _cairo_toy_font_face_create (CAIRO_FONT_FAMILY_DEFAULT,
						 CAIRO_FONT_SLANT_DEFAULT,
						 CAIRO_FONT_WEIGHT_DEFAULT);
	if (font_face->status)
	    return font_face->status;
	else
	    gstate->font_face = font_face;
    }
    
    return CAIRO_STATUS_SUCCESS;
}
    
static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_font_options_t options;
    
    if (gstate->scaled_font)
	return CAIRO_STATUS_SUCCESS;
    
    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    cairo_surface_get_font_options (gstate->target, &options);
    cairo_font_options_merge (&options, &gstate->font_options);
    
    gstate->scaled_font = cairo_scaled_font_create (gstate->font_face,
						    &gstate->font_matrix,
						    &gstate->ctm,
						    &options);
    
    if (!gstate->scaled_font)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_get_font_extents (cairo_gstate_t *gstate, 
				cairo_font_extents_t *extents)
{
    cairo_status_t status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_extents (gstate->scaled_font, extents);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_text_to_glyphs (cairo_gstate_t *gstate, 
			      const char     *utf8,
			      double	      x,
			      double	      y,
			      cairo_glyph_t **glyphs,
			      int	     *num_glyphs)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    status = _cairo_scaled_font_text_to_glyphs (gstate->scaled_font, x, y,
						utf8, glyphs, num_glyphs);

    if (status || !glyphs || !num_glyphs || !(*glyphs) || !(num_glyphs))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_face (cairo_gstate_t    *gstate, 
			     cairo_font_face_t *font_face)
{
    if (font_face && font_face->status)
	return font_face->status;
    
    if (font_face != gstate->font_face) {
	cairo_font_face_destroy (gstate->font_face);
	gstate->font_face = cairo_font_face_reference (font_face);
    }

    _cairo_gstate_unset_scaled_font (gstate);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     cairo_glyph_t *glyphs, 
			     int num_glyphs,
			     cairo_text_extents_t *extents)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_glyph_extents (gstate->scaled_font,
				     glyphs, num_glyphs,
				     extents);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate, 
			   cairo_glyph_t *glyphs, 
			   int num_glyphs)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern;
    cairo_glyph_t *transformed_glyphs;
    int i;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &transformed_glyphs[i].x, 
				       &transformed_glyphs[i].y);
    }

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);

    status = _cairo_surface_show_glyphs (gstate->target,
					 gstate->op,
					 &source_pattern.base,
					 transformed_glyphs,
					 num_glyphs,
					 gstate->scaled_font);

    _cairo_pattern_fini (&source_pattern.base);
    free (transformed_glyphs);

    return status;
}

cairo_status_t
_cairo_gstate_glyph_path (cairo_gstate_t     *gstate,
			  cairo_glyph_t	     *glyphs, 
			  int		      num_glyphs,
			  cairo_path_fixed_t *path)
{
    cairo_status_t status;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &(transformed_glyphs[i].x), 
				       &(transformed_glyphs[i].y));
    }

    status = _cairo_scaled_font_glyph_path (gstate->scaled_font,
					    transformed_glyphs, num_glyphs,
					    path);

    free (transformed_glyphs);
    return status;
}

cairo_status_t
_cairo_gstate_set_antialias (cairo_gstate_t *gstate,
			     cairo_antialias_t antialias)
{
    gstate->antialias = antialias;

    return CAIRO_STATUS_SUCCESS;
}

cairo_antialias_t
_cairo_gstate_get_antialias (cairo_gstate_t *gstate)
{
    return gstate->antialias;
}
