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

#include "cairoint.h"
#include "cairo-private.h"

#include "cairo-arc-private.h"
#include "cairo-path-data-private.h"

#define CAIRO_TOLERANCE_MINIMUM	0.0002 /* We're limited by 16 bits of sub-pixel precision */

static const cairo_t cairo_nil = {
  (unsigned int)-1,		/* ref_count */
  CAIRO_STATUS_NO_MEMORY,	/* status */
  { 				/* path */
    NULL, NULL,			/* op_buf_head, op_buf_tail */
    NULL, NULL,			/* arg_buf_head, arg_buf_tail */
    { 0, 0 },			/* last_move_point */
    { 0, 0 },			/* current point */
    FALSE,			/* has_current_point */
  },
  NULL				/* gstate */
};

#include <assert.h>

/* This has to be updated whenever cairo_status_t is extended.  That's
 * a bit of a pain, but it should be easy to always catch as long as
 * one adds a new test case to test a trigger of the new status value.
 */
#define CAIRO_STATUS_LAST_STATUS CAIRO_STATUS_INVALID_DASH

/**
 * _cairo_error:
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 * 
 * Checks that status is an error status, but does nothing else.
 *
 * All assignments of an error status to any user-visible object
 * within the cairo application should result in a call to
 * _cairo_error().
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
void
_cairo_error (cairo_status_t status)
{
    assert (status > CAIRO_STATUS_SUCCESS &&
	    status <= CAIRO_STATUS_LAST_STATUS);
}

/**
 * _cairo_set_error:
 * @cr: a cairo context
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 * 
 * Sets cr->status to @status and calls _cairo_error;
 *
 * All assignments of an error status to cr->status should happen
 * through _cairo_set_error() or else _cairo_error() should be
 * called immediately after the assignment.
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
static void
_cairo_set_error (cairo_t *cr, cairo_status_t status)
{
    /* Don't overwrite an existing error. This preserves the first
     * error, which is the most significant. It also avoids attempting
     * to write to read-only data (eg. from a nil cairo_t). */
    if (cr->status == CAIRO_STATUS_SUCCESS)
	cr->status = status;

    _cairo_error (status);
}

/**
 * cairo_version:
 * 
 * Returns the version of the cairo library encoded in a single
 * integer as per CAIRO_VERSION_ENCODE. The encoding ensures that
 * later versions compare greater than earlier versions.
 *
 * A run-time comparison to check that cairo's version is greater than
 * or equal to version X.Y.Z could be performed as follows:
 *
 * <informalexample><programlisting>
 * if (cairo_version() >= CAIRO_VERSION_ENCODE(X,Y,Z)) {...}
 * </programlisting></informalexample>
 *
 * See also cairo_version_string() as well as the compile-time
 * equivalents %CAIRO_VERSION and %CAIRO_VERSION_STRING.
 * 
 * Return value: the encoded version.
 **/
int
cairo_version (void)
{
    return CAIRO_VERSION;
}

/**
 * cairo_version_string:
 * 
 * Returns the version of the cairo library as a human-readable string
 * of the form "X.Y.Z".
 *
 * See also cairo_version() as well as the compile-time equivalents
 * %CAIRO_VERSION_STRING and %CAIRO_VERSION.
 * 
 * Return value: a string containing the version.
 **/
const char*
cairo_version_string (void)
{
    return CAIRO_VERSION_STRING;
}

/**
 * cairo_create:
 * @target: target surface for the context
 * 
 * Creates a new #cairo_t with all graphics state parameters set to
 * default values and with @target as a target surface. The target
 * surface should be constructed with a backend-specific function such
 * as cairo_image_surface_create() (or any other
 * <literal>cairo_&lt;backend&gt;_surface_create</literal> variant).
 *
 * This function references @target, so you can immediately
 * call cairo_surface_destroy() on it if you don't need to
 * maintain a separate reference to it.
 * 
 * Return value: a newly allocated #cairo_t with a reference
 *  count of 1. The initial reference count should be released
 *  with cairo_destroy() when you are done using the #cairo_t.
 *  This function never returns %NULL. If memory cannot be
 *  allocated, a special #cairo_t object will be returned on
 *  which cairo_status() returns %CAIRO_STATUS_NO_MEMORY.
 *  You can use this object normally, but no drawing will
 *  be done.
 **/
cairo_t *
cairo_create (cairo_surface_t *target)
{
    cairo_t *cr;

    cr = malloc (sizeof (cairo_t));
    if (cr == NULL)
	return (cairo_t *) &cairo_nil;

    cr->ref_count = 1;

    cr->status = CAIRO_STATUS_SUCCESS;

    _cairo_path_fixed_init (&cr->path);

    if (target == NULL) {
	cr->gstate = NULL;
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return cr;
    }

    cr->gstate = _cairo_gstate_create (target);
    if (cr->gstate == NULL)
	_cairo_set_error (cr, CAIRO_STATUS_NO_MEMORY);

    return cr;
}

/**
 * cairo_reference:
 * @cr: a #cairo_t
 * 
 * Increases the reference count on @cr by one. This prevents
 * @cr from being destroyed until a matching call to cairo_destroy() 
 * is made.
 *
 * Return value: the referenced #cairo_t.
 **/
cairo_t *
cairo_reference (cairo_t *cr)
{
    if (cr->ref_count == (unsigned int)-1)
	return cr;

    assert (cr->ref_count > 0);
    
    cr->ref_count++;

    return cr;
}

/**
 * cairo_destroy:
 * @cr: a #cairo_t
 * 
 * Decreases the reference count on @cr by one. If the result
 * is zero, then @cr and all associated resources are freed.
 * See cairo_reference().
 **/
void
cairo_destroy (cairo_t *cr)
{
    if (cr->ref_count == (unsigned int)-1)
	return;

    assert (cr->ref_count > 0);
    
    cr->ref_count--;
    if (cr->ref_count)
	return;

    while (cr->gstate) {
	cairo_gstate_t *tmp = cr->gstate;
	cr->gstate = tmp->next;

	_cairo_gstate_destroy (tmp);
    }

    _cairo_path_fixed_fini (&cr->path);

    free (cr);
}

/**
 * cairo_save:
 * @cr: a #cairo_t
 * 
 * Makes a copy of the current state of @cr and saves it
 * on an internal stack of saved states for @cr. When
 * cairo_restore() is called, @cr will be restored to
 * the saved state. Multiple calls to cairo_save() and
 * cairo_restore() can be nested; each call to cairo_restore()
 * restores the state from the matching paired cairo_save().
 *
 * It isn't necessary to clear all saved states before
 * a #cairo_t is freed. If the reference count of a #cairo_t
 * drops to zero in response to a call to cairo_destroy(),
 * any saved states will be freed along with the #cairo_t.
 **/
void
cairo_save (cairo_t *cr)
{
    cairo_gstate_t *top;

    if (cr->status)
	return;

    top = _cairo_gstate_clone (cr->gstate);

    if (top == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NO_MEMORY);
	return;
    }

    top->next = cr->gstate;
    cr->gstate = top;
}
slim_hidden_def(cairo_save);

/**
 * cairo_restore:
 * @cr: a #cairo_t
 * 
 * Restores @cr to the state saved by a preceding call to
 * cairo_save() and removes that state from the stack of
 * saved states.
 **/
void
cairo_restore (cairo_t *cr)
{
    cairo_gstate_t *top;

    if (cr->status)
	return;

    top = cr->gstate;
    cr->gstate = top->next;

    _cairo_gstate_destroy (top);

    if (cr->gstate == NULL)
	_cairo_set_error (cr, CAIRO_STATUS_INVALID_RESTORE);
}
slim_hidden_def(cairo_restore);

/**
 * moz_cairo_set_target:
 * @cr: a #cairo_t
 * @target: a #cairo_surface_t
 *
 * Change the destination surface of rendering to @cr to @target.
 * @target must not be %NULL, or an error will be set on @cr.
 */
void
moz_cairo_set_target (cairo_t *cr, cairo_surface_t *target)
{
    if (cr->status)
        return;

    if (target == NULL) {
        _cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
        return;
    }

    _moz_cairo_gstate_set_target (cr->gstate, target);
}
slim_hidden_def(moz_cairo_set_target);

/**
 * cairo_push_group:
 * @cr: a cairo context
 *
 * Pushes a CAIRO_CONTENT_COLOR_ALPHA temporary surface onto
 * the rendering stack, redirecting all rendering into it.
 * See cairo_push_group_with_content().
 */

void
cairo_push_group (cairo_t *cr)
{
    cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
}
slim_hidden_def(cairo_push_group);

/**
 * cairo_push_group_with_content:
 * @cr: a cairo context
 * @content: a %cairo_content_t indicating the type of group that
 *           will be created
 *
 * Pushes a temporary surface onto the rendering stack, redirecting
 * all rendering into it.  The surface dimensions are the size of
 * the current clipping bounding box.  Initially, this surface
 * is painted with CAIRO_OPERATOR_CLEAR.
 *
 * cairo_push_group() calls cairo_save() so that any changes to the
 * graphics state will not be visible after cairo_pop_group() or
 * cairo_pop_group_with_alpha().  See cairo_pop_group() and
 * cairo_pop_group_with_alpha().
 */

void
cairo_push_group_with_content (cairo_t *cr, cairo_content_t content)
{
    cairo_status_t status;
    cairo_rectangle_t extents;
    cairo_surface_t *group_surface = NULL;

    /* Get the extents that we'll use in creating our new group surface */
    _cairo_surface_get_extents (_cairo_gstate_get_target (cr->gstate), &extents);
    status = _cairo_clip_intersect_to_rectangle (_cairo_gstate_get_clip (cr->gstate), &extents);
    if (status != CAIRO_STATUS_SUCCESS)
	goto bail;

    group_surface = cairo_surface_create_similar (_cairo_gstate_get_target (cr->gstate),
						  content,
						  extents.width,
						  extents.height);
    status = cairo_surface_status (group_surface);
    if (status)
	goto bail;

    /* Set device offsets on the new surface so that logically it appears at
     * the same location on the parent surface. */
    cairo_surface_set_device_offset (group_surface, -extents.x, -extents.y);

    /* create a new gstate for the redirect */
    cairo_save (cr);
    if (cr->status)
	goto bail;

    _cairo_gstate_redirect_target (cr->gstate, group_surface);

bail:
    cairo_surface_destroy (group_surface);
    if (status)
	_cairo_set_error (cr, status);
}
slim_hidden_def(cairo_push_group_with_content);

cairo_pattern_t *
cairo_pop_group (cairo_t *cr)
{
    cairo_surface_t *group_surface, *parent_target;
    cairo_pattern_t *group_pattern = NULL;
    cairo_matrix_t group_matrix;

    /* Grab the active surfaces */
    group_surface = _cairo_gstate_get_target (cr->gstate);
    parent_target = _cairo_gstate_get_parent_target (cr->gstate);

    /* Verify that we are at the right nesting level */
    if (parent_target == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_INVALID_POP_GROUP);
	return NULL;
    }

    /* We need to save group_surface before we restore; we don't need
     * to reference parent_target and original_target, since the
     * gstate will still hold refs to them once we restore. */
    cairo_surface_reference (group_surface);

    cairo_restore (cr);

    if (cr->status)
	goto done;

    group_pattern = cairo_pattern_create_for_surface (group_surface);
    if (!group_pattern) {
        cr->status = CAIRO_STATUS_NO_MEMORY;
        goto done;
    }

    _cairo_gstate_get_matrix (cr->gstate, &group_matrix);
    cairo_pattern_set_matrix (group_pattern, &group_matrix);
done:
    cairo_surface_destroy (group_surface);

    return group_pattern;
}
slim_hidden_def(cairo_pop_group);

void
cairo_pop_group_to_source (cairo_t *cr)
{
    cairo_pattern_t *group_pattern;

    group_pattern = cairo_pop_group (cr);
    if (!group_pattern)
        return;

    cairo_set_source (cr, group_pattern);
    cairo_pattern_destroy (group_pattern);
}
slim_hidden_def(cairo_pop_group_to_source);

/**
 * cairo_set_operator:
 * @cr: a #cairo_t
 * @op: a compositing operator, specified as a #cairo_operator_t
 * 
 * Sets the compositing operator to be used for all drawing
 * operations. See #cairo_operator_t for details on the semantics of
 * each available compositing operator.
 *
 * XXX: I'd also like to direct the reader's attention to some
 * (not-yet-written) section on cairo's imaging model. How would I do
 * that if such a section existed? (cworth).
 **/
void
cairo_set_operator (cairo_t *cr, cairo_operator_t op)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_operator (cr->gstate, op);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_source_rgb
 * @cr: a cairo context
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 * 
 * Sets the source pattern within @cr to an opaque color. This opaque
 * color will then be used for any subsequent drawing operation until
 * a new source pattern is set.
 *
 * The color components are floating point numbers in the range 0 to
 * 1. If the values passed in are outside that range, they will be
 * clamped.
 **/
void
cairo_set_source_rgb (cairo_t *cr, double red, double green, double blue)
{
    cairo_pattern_t *pattern;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_rgb (red, green, blue);
    cairo_set_source (cr, pattern);
    cairo_pattern_destroy (pattern);
}

/**
 * cairo_set_source_rgba:
 * @cr: a cairo context
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 * @alpha: alpha component of color
 * 
 * Sets the source pattern within @cr to a translucent color. This
 * color will then be used for any subsequent drawing operation until
 * a new source pattern is set.
 *
 * The color and alpha components are floating point numbers in the
 * range 0 to 1. If the values passed in are outside that range, they
 * will be clamped.
 **/
void
cairo_set_source_rgba (cairo_t *cr,
		       double red, double green, double blue,
		       double alpha)
{
    cairo_pattern_t *pattern;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_rgba (red, green, blue, alpha);
    cairo_set_source (cr, pattern);
    cairo_pattern_destroy (pattern);
}

/**
 * cairo_set_source_surface:
 * @cr: a cairo context
 * @surface: a surface to be used to set the source pattern
 * @x: User-space X coordinate for surface origin
 * @y: User-space Y coordinate for surface origin
 * 
 * This is a convenience function for creating a pattern from @surface
 * and setting it as the source in @cr with cairo_set_source().
 *
 * The @x and @y parameters give the user-space coordinate at which
 * the surface origin should appear. (The surface origin is its
 * upper-left corner before any transformation has been applied.) The
 * @x and @y patterns are negated and then set as translation values
 * in the pattern matrix.
 *
 * Other than the initial translation pattern matrix, as described
 * above, all other pattern attributes, (such as its extend mode), are
 * set to the default values as in cairo_pattern_create_for_surface().
 * The resulting pattern can be queried with cairo_get_source() so
 * that these attributes can be modified if desired, (eg. to create a
 * repeating pattern with cairo_pattern_set_extend()).
 **/
void
cairo_set_source_surface (cairo_t	  *cr,
			  cairo_surface_t *surface,
			  double	   x,
			  double	   y)
{
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_for_surface (surface);

    cairo_matrix_init_translate (&matrix, -x, -y);
    cairo_pattern_set_matrix (pattern, &matrix);

    cairo_set_source (cr, pattern);
    cairo_pattern_destroy (pattern);
}

/**
 * cairo_set_source
 * @cr: a cairo context
 * @source: a #cairo_pattern_t to be used as the source for
 * subsequent drawing operations.
 * 
 * Sets the source pattern within @cr to @source. This pattern
 * will then be used for any subsequent drawing operation until a new
 * source pattern is set.
 *
 * Note: The pattern's transformation matrix will be locked to the
 * user space in effect at the time of cairo_set_source(). This means
 * that further modifications of the current transformation matrix
 * will not affect the source pattern. See cairo_pattern_set_matrix().
 *
 * XXX: I'd also like to direct the reader's attention to some
 * (not-yet-written) section on cairo's imaging model. How would I do
 * that if such a section existed? (cworth).
 **/
void
cairo_set_source (cairo_t *cr, cairo_pattern_t *source)
{
    if (cr->status)
	return;

    if (source == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (source->status) {
	_cairo_set_error (cr, source->status);
	return;
    }

    cr->status = _cairo_gstate_set_source (cr->gstate, source);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_source:
 * @cr: a cairo context
 * 
 * Gets the current source pattern for @cr.
 * 
 * Return value: the current source pattern. This object is owned by
 * cairo. To keep a reference to it, you must call
 * cairo_pattern_reference().
 **/
cairo_pattern_t *
cairo_get_source (cairo_t *cr)
{
    if (cr->status)
	return (cairo_pattern_t*) &cairo_pattern_nil.base;

    return _cairo_gstate_get_source (cr->gstate);
}

/**
 * cairo_set_tolerance:
 * @cr: a #cairo_t
 * @tolerance: the tolerance, in device units (typically pixels)
 * 
 * Sets the tolerance used when converting paths into trapezoids.
 * Curved segments of the path will be subdivided until the maximum
 * deviation between the original path and the polygonal approximation
 * is less than @tolerance. The default value is 0.1. A larger
 * value will give better performance, a smaller value, better
 * appearance. (Reducing the value from the default value of 0.1
 * is unlikely to improve appearance significantly.)
 **/
void
cairo_set_tolerance (cairo_t *cr, double tolerance)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&tolerance, CAIRO_TOLERANCE_MINIMUM, tolerance);

    cr->status = _cairo_gstate_set_tolerance (cr->gstate, tolerance);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_antialias:
 * @cr: a #cairo_t
 * @antialias: the new antialiasing mode
 * 
 * Set the antialiasing mode of the rasterizer used for drawing shapes.
 * This value is a hint, and a particular backend may or may not support
 * a particular value.  At the current time, no backend supports
 * %CAIRO_ANTIALIAS_SUBPIXEL when drawing shapes.
 *
 * Note that this option does not affect text rendering, instead see
 * cairo_font_options_set_antialias().
 **/
void
cairo_set_antialias (cairo_t *cr, cairo_antialias_t antialias)
{
    if (cr->status) 
	return;

    cr->status = _cairo_gstate_set_antialias (cr->gstate, antialias);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_fill_rule:
 * @cr: a #cairo_t
 * @fill_rule: a fill rule, specified as a #cairo_fill_rule_t
 * 
 * Set the current fill rule within the cairo context. The fill rule
 * is used to determine which regions are inside or outside a complex
 * (potentially self-intersecting) path. The current fill rule affects
 * both cairo_fill and cairo_clip. See #cairo_fill_rule_t for details
 * on the semantics of each available fill rule.
 **/
void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_fill_rule (cr->gstate, fill_rule);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_line_width:
 * @cr: a #cairo_t
 * @width: a line width, as a user-space value
 * 
 * Sets the current line width within the cairo context. The line
 * width specifies the diameter of a pen that is circular in
 * user-space.
 *
 * As with the other stroke parameters, the current line cap style is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 **/
void
cairo_set_line_width (cairo_t *cr, double width)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&width, 0.0, width);

    cr->status = _cairo_gstate_set_line_width (cr->gstate, width);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_line_cap:
 * @cr: a cairo context, as a #cairo_t
 * @line_cap: a line cap style, as a #cairo_line_cap_t
 * 
 * Sets the current line cap style within the cairo context. See
 * #cairo_line_cap_t for details about how the available line cap
 * styles are drawn.
 *
 * As with the other stroke parameters, the current line cap style is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 **/
void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_cap (cr->gstate, line_cap);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_line_join:
 * @cr: a cairo context, as a #cairo_t
 * @line_join: a line joint style, as a #cairo_line_join_t
 *
 * Sets the current line join style within the cairo context. See
 * #cairo_line_join_t for details about how the available line join
 * styles are drawn.
 *
 * As with the other stroke parameters, the current line join style is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 **/
void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_join (cr->gstate, line_join);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_dash:
 * @cr: a cairo context
 * @dashes: an array specifying alternate lengths of on and off po
 * @num_dashes: the length of the dashes array
 * @offset: an offset into the dash pattern at which the stroke should start
 * 
 * Sets the dash pattern to be used by cairo_stroke(). A dash pattern
 * is specified by @dashes, an array of positive values. Each value
 * provides the user-space length of altenate "on" and "off" portions
 * of the stroke. The @offset specifies an offset into the pattern at
 * which the stroke begins.
 *
 * If @num_dashes is 0 dashing is disabled.
 *
 * If @num_dashes is 1 a symmetric pattern is assumed with alternating
 * on and off portions of the size specified by the single value in
 * @dashes.
 *
 * If any value in @dashes is negative, or if all values are 0, then
 * @cairo_t will be put into an error state with a status of
 * #CAIRO_STATUS_INVALID_DASH.
 **/
void
cairo_set_dash (cairo_t	*cr,
		double	*dashes,
		int	 num_dashes,
		double	 offset)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_dash (cr->gstate,
					 dashes, num_dashes, offset);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

void
cairo_set_miter_limit (cairo_t *cr, double limit)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_miter_limit (cr->gstate, limit);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}


/**
 * cairo_translate:
 * @cr: a cairo context
 * @tx: amount to translate in the X direction
 * @ty: amount to translate in the Y direction
 * 
 * Modifies the current transformation matrix (CTM) by translating the
 * user-space origin by (@tx, @ty). This offset is interpreted as a
 * user-space coordinate according to the CTM in place before the new
 * call to cairo_translate. In other words, the translation of the
 * user-space origin takes place after any existing transformation.
 **/
void
cairo_translate (cairo_t *cr, double tx, double ty)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_translate (cr->gstate, tx, ty);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_scale:
 * @cr: a cairo context
 * @sx: scale factor for the X dimension
 * @sy: scale factor for the Y dimension
 * 
 * Modifies the current transformation matrix (CTM) by scaling the X
 * and Y user-space axes by @sx and @sy respectively. The scaling of
 * the axes takes place after any existing transformation of user
 * space.
 **/
void
cairo_scale (cairo_t *cr, double sx, double sy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_scale (cr->gstate, sx, sy);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}


/**
 * cairo_rotate:
 * @cr: a cairo context
 * @angle: angle (in radians) by which the user-space axes will be
 * rotated
 * 
 * Modifies the current transformation matrix (CTM) by rotating the
 * user-space axes by @angle radians. The rotation of the axes takes
 * places after any existing transformation of user space. The
 * rotation direction for positive angles is from the positive X axis
 * toward the positive Y axis.
 **/
void
cairo_rotate (cairo_t *cr, double angle)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rotate (cr->gstate, angle);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_transform:
 * @cr: a cairo context
 * @matrix: a transformation to be applied to the user-space axes
 * 
 * Modifies the current transformation matrix (CTM) by applying
 * @matrix as an additional transformation. The new transformation of
 * user space takes place after any existing transformation.
 **/
void
cairo_transform (cairo_t	      *cr,
		 const cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform (cr->gstate, matrix);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_matrix:
 * @cr: a cairo context
 * @matrix: a transformation matrix from user space to device space
 * 
 * Modifies the current transformation matrix (CTM) by setting it
 * equal to @matrix.
 **/
void
cairo_set_matrix (cairo_t	       *cr,
		  const cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_matrix (cr->gstate, matrix);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_identity_matrix:
 * @cr: a cairo context
 * 
 * Resets the current transformation matrix (CTM) by setting it equal
 * to the identity matrix. That is, the user-space and device-space
 * axes will be aligned and one user-space unit will transform to one
 * device-space unit.
 **/
void
cairo_identity_matrix (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_identity_matrix (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_user_to_device:
 * @cr: a cairo context
 * @x: X value of coordinate (in/out parameter)
 * @y: Y value of coordinate (in/out parameter)
 * 
 * Transform a coordinate from user space to device space by
 * multiplying the given point by the current transformation matrix
 * (CTM).
 **/
void
cairo_user_to_device (cairo_t *cr, double *x, double *y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_user_to_device (cr->gstate, x, y);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_user_to_device_distance:
 * @cr: a cairo context
 * @dx: X component of a distance vector (in/out parameter)
 * @dy: Y component of a distance vector (in/out parameter)
 * 
 * Transform a distance vector from user space to device space. This
 * function is similar to cairo_user_to_device() except that the
 * translation components of the CTM will be ignored when transforming
 * (@dx,@dy).
 **/
void
cairo_user_to_device_distance (cairo_t *cr, double *dx, double *dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_user_to_device_distance (cr->gstate, dx, dy);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_device_to_user:
 * @cr: a cairo
 * @x: X value of coordinate (in/out parameter)
 * @y: Y value of coordinate (in/out parameter)
 * 
 * Transform a coordinate from device space to user space by
 * multiplying the given point by the inverse of the current
 * transformation matrix (CTM).
 **/
void
cairo_device_to_user (cairo_t *cr, double *x, double *y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_device_to_user (cr->gstate, x, y);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_device_to_user_distance:
 * @cr: a cairo context
 * @dx: X component of a distance vector (in/out parameter)
 * @dy: Y component of a distance vector (in/out parameter)
 * 
 * Transform a distance vector from device space to user space. This
 * function is similar to cairo_device_to_user() except that the
 * translation components of the inverse CTM will be ignored when
 * transforming (@dx,@dy).
 **/
void
cairo_device_to_user_distance (cairo_t *cr, double *dx, double *dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_device_to_user_distance (cr->gstate, dx, dy);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_new_path:
 * @cr: a cairo context
 *
 * Clears the current path. After this call there will be no current
 * point.
 **/
void
cairo_new_path (cairo_t *cr)
{
    if (cr->status)
	return;

    _cairo_path_fixed_fini (&cr->path);
}
slim_hidden_def(cairo_new_path);

/**
 * cairo_move_to:
 * @cr: a cairo context
 * @x: the X coordinate of the new position
 * @y: the Y coordinate of the new position
 *
 * If the current subpath is not empty, begin a new subpath. After
 * this call the current point will be (@x, @y).
 **/
void
cairo_move_to (cairo_t *cr, double x, double y)
{
    cairo_fixed_t x_fixed, y_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_backend (cr->gstate, &x, &y);
    x_fixed = _cairo_fixed_from_double (x);
    y_fixed = _cairo_fixed_from_double (y);

    cr->status = _cairo_path_fixed_move_to (&cr->path, x_fixed, y_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_move_to);

/**
 * cairo_line_to:
 * @cr: a cairo context
 * @x: the X coordinate of the end of the new line
 * @y: the Y coordinate of the end of the new line
 *
 * Adds a line to the path from the current point to position (@x, @y)
 * in user-space coordinates. After this call the current point
 * will be (@x, @y).
 **/
void
cairo_line_to (cairo_t *cr, double x, double y)
{
    cairo_fixed_t x_fixed, y_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_backend (cr->gstate, &x, &y);
    x_fixed = _cairo_fixed_from_double (x);
    y_fixed = _cairo_fixed_from_double (y);

    cr->status = _cairo_path_fixed_line_to (&cr->path, x_fixed, y_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_curve_to:
 * @cr: a cairo context
 * @x1: the X coordinate of the first control point
 * @y1: the Y coordinate of the first control point
 * @x2: the X coordinate of the second control point
 * @y2: the Y coordinate of the second control point
 * @x3: the X coordinate of the end of the curve
 * @y3: the Y coordinate of the end of the curve
 *
 * Adds a cubic Bézier spline to the path from the current point to
 * position (@x3, @y3) in user-space coordinates, using (@x1, @y1) and
 * (@x2, @y2) as the control points. After this call the current point
 * will be (@x3, @y3).
 **/
void
cairo_curve_to (cairo_t *cr,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
    cairo_fixed_t x1_fixed, y1_fixed;
    cairo_fixed_t x2_fixed, y2_fixed;
    cairo_fixed_t x3_fixed, y3_fixed;
	
    if (cr->status)
	return;

    _cairo_gstate_user_to_backend (cr->gstate, &x1, &y1);
    _cairo_gstate_user_to_backend (cr->gstate, &x2, &y2);
    _cairo_gstate_user_to_backend (cr->gstate, &x3, &y3);

    x1_fixed = _cairo_fixed_from_double (x1);
    y1_fixed = _cairo_fixed_from_double (y1);

    x2_fixed = _cairo_fixed_from_double (x2);
    y2_fixed = _cairo_fixed_from_double (y2);

    x3_fixed = _cairo_fixed_from_double (x3);
    y3_fixed = _cairo_fixed_from_double (y3);

    cr->status = _cairo_path_fixed_curve_to (&cr->path,
					     x1_fixed, y1_fixed,
					     x2_fixed, y2_fixed,
					     x3_fixed, y3_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_arc:
 * @cr: a cairo context
 * @xc: X position of the center of the arc
 * @yc: Y position of the center of the arc
 * @radius: the radius of the arc
 * @angle1: the start angle, in radians
 * @angle2: the end angle, in radians
 * 
 * Adds a circular arc of the given @radius to the current path.  The
 * arc is centered at (@xc, @yc), begins at @angle1 and proceeds in
 * the direction of increasing angles to end at @angle2. If @angle2 is
 * less than @angle1 it will be progressively increased by 2*M_PI
 * until it is greater than @angle1.
 *
 * If there is a current point, an initial line segment will be added
 * to the path to connect the current point to the beginning of the
 * arc.
 *
 * Angles are measured in radians. An angle of 0.0 is in the direction
 * of the positive X axis (in user-space). An angle of %M_PI/2.0 radians
 * (90 degrees) is in the direction of the positive Y axis (in
 * user-space). Angles increase in the direction from the positive X
 * axis toward the positive Y axis. So with the default transformation
 * matrix, angles increase in a clockwise direction.
 *
 * (To convert from degrees to radians, use <literal>degrees * (M_PI /
 * 180.)</literal>.)
 *
 * This function gives the arc in the direction of increasing angles;
 * see cairo_arc_negative() to get the arc in the direction of
 * decreasing angles.
 *
 * The arc is circular in user-space. To achieve an elliptical arc,
 * you can scale the current transformation matrix by different
 * amounts in the X and Y directions. For example, to draw an ellipse
 * in the box given by @x, @y, @width, @height:
 *
 * <informalexample><programlisting>
 * cairo_save (cr);
 * cairo_translate (x + width / 2., y + height / 2.);
 * cairo_scale (1. / (height / 2.), 1. / (width / 2.));
 * cairo_arc (cr, 0., 0., 1., 0., 2 * M_PI);
 * cairo_restore (cr);
 * </programlisting></informalexample>
 **/
void
cairo_arc (cairo_t *cr,
	   double xc, double yc,
	   double radius,
	   double angle1, double angle2)
{
    if (cr->status)
	return;

    /* Do nothing, successfully, if radius is <= 0 */
    if (radius <= 0.0)
	return;

    while (angle2 < angle1)
	angle2 += 2 * M_PI;

    cairo_line_to (cr,
		   xc + radius * cos (angle1),
		   yc + radius * sin (angle1));

    _cairo_arc_path (cr, xc, yc, radius,
		     angle1, angle2);
}

/**
 * cairo_arc_negative:
 * @cr: a cairo context
 * @xc: X position of the center of the arc
 * @yc: Y position of the center of the arc
 * @radius: the radius of the arc
 * @angle1: the start angle, in radians
 * @angle2: the end angle, in radians
 * 
 * Adds a circular arc of the given @radius to the current path.  The
 * arc is centered at (@xc, @yc), begins at @angle1 and proceeds in
 * the direction of decreasing angles to end at @angle2. If @angle2 is
 * greater than @angle1 it will be progressively decreased by 2*M_PI
 * until it is greater than @angle1.
 *
 * See cairo_arc() for more details. This function differs only in the
 * direction of the arc between the two angles.
 **/
void
cairo_arc_negative (cairo_t *cr,
		    double xc, double yc,
		    double radius,
		    double angle1, double angle2)
{
    if (cr->status)
	return;

    /* Do nothing, successfully, if radius is <= 0 */
    if (radius <= 0.0)
	return;

    while (angle2 > angle1)
	angle2 -= 2 * M_PI;

    cairo_line_to (cr,
		   xc + radius * cos (angle1),
		   yc + radius * sin (angle1));

     _cairo_arc_path_negative (cr, xc, yc, radius,
			       angle1, angle2);
}

/* XXX: NYI
void
cairo_arc_to (cairo_t *cr,
	      double x1, double y1,
	      double x2, double y2,
	      double radius)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc_to (cr->gstate,
				       x1, y1,
				       x2, y2,
				       radius);
}
*/

/**
 * cairo_rel_move_to:
 * @cr: a cairo context
 * @dx: the X offset
 * @dy: the Y offset
 *
 * If the current subpath is not empty, begin a new subpath. After
 * this call the current point will offset by (@x, @y).
 *
 * Given a current point of (x, y), cairo_rel_move_to(@cr, @dx, @dy)
 * is logically equivalent to cairo_move_to (@cr, x + @dx, y + @dy).
 **/
void
cairo_rel_move_to (cairo_t *cr, double dx, double dy)
{
    cairo_fixed_t dx_fixed, dy_fixed;

    if (cr->status) 
	return;

    _cairo_gstate_user_to_device_distance (cr->gstate, &dx, &dy);
    dx_fixed = _cairo_fixed_from_double (dx);
    dy_fixed = _cairo_fixed_from_double (dy);

    cr->status = _cairo_path_fixed_rel_move_to (&cr->path, dx_fixed, dy_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_rel_line_to:
 * @cr: a cairo context
 * @dx: the X offset to the end of the new line
 * @dy: the Y offset to the end of the new line
 *
 * Relative-coordinate version of cairo_line_to(). Adds a line to the
 * path from the current point to a point that is offset from the
 * current point by (@dx, @dy) in user space. After this call the
 * current point will be offset by (@dx, @dy).
 *
 * Given a current point of (x, y), cairo_rel_line_to(@cr, @dx, @dy)
 * is logically equivalent to cairo_line_to (@cr, x + @dx, y + @dy).
 **/
void
cairo_rel_line_to (cairo_t *cr, double dx, double dy)
{
    cairo_fixed_t dx_fixed, dy_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_device_distance (cr->gstate, &dx, &dy);
    dx_fixed = _cairo_fixed_from_double (dx);
    dy_fixed = _cairo_fixed_from_double (dy);

    cr->status = _cairo_path_fixed_rel_line_to (&cr->path, dx_fixed, dy_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_rel_line_to);

/**
 * cairo_rel_curve_to:
 * @cr: a cairo context
 * @dx1: the X offset to the first control point
 * @dy1: the Y offset to the first control point
 * @dx2: the X offset to the second control point
 * @dy2: the Y offset to the second control point
 * @dx3: the X offset to the end of the curve
 * @dy3: the Y offset to the end of the curve
 *
 * Relative-coordinate version of cairo_curve_to(). All offsets are
 * relative to the current point. Adds a cubic Bézier spline to the
 * path from the current point to a point offset from the current
 * point by (@dx3, @dy3), using points offset by (@dx1, @dy1) and
 * (@dx2, @dy2) as the control points. After this call the current
 * point will be offset by (@dx3, @dy3).
 *
 * Given a current point of (x, y), cairo_rel_curve_to (@cr, @dx1,
 * @dy1, @dx2, @dy2, @dx3, @dy3) is logically equivalent to
 * cairo_curve_to (@cr, x + @dx1, y + @dy1, x + @dx2, y + @dy2, x +
 * @dx3, y + @dy3).
 **/
void
cairo_rel_curve_to (cairo_t *cr,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3)
{
    cairo_fixed_t dx1_fixed, dy1_fixed;
    cairo_fixed_t dx2_fixed, dy2_fixed;
    cairo_fixed_t dx3_fixed, dy3_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_device_distance (cr->gstate, &dx1, &dy1);
    _cairo_gstate_user_to_device_distance (cr->gstate, &dx2, &dy2);
    _cairo_gstate_user_to_device_distance (cr->gstate, &dx3, &dy3);

    dx1_fixed = _cairo_fixed_from_double (dx1);
    dy1_fixed = _cairo_fixed_from_double (dy1);

    dx2_fixed = _cairo_fixed_from_double (dx2);
    dy2_fixed = _cairo_fixed_from_double (dy2);

    dx3_fixed = _cairo_fixed_from_double (dx3);
    dy3_fixed = _cairo_fixed_from_double (dy3);

    cr->status = _cairo_path_fixed_rel_curve_to (&cr->path,
						 dx1_fixed, dy1_fixed,
						 dx2_fixed, dy2_fixed,
						 dx3_fixed, dy3_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_rectangle:
 * @cr: a cairo context
 * @x: the X coordinate of the top left corner of the rectangle
 * @y: the Y coordinate to the top left corner of the rectangle
 * @width: the width of the rectangle
 * @height: the height of the rectangle
 *
 * Adds a closed-subpath rectangle of the given size to the current
 * path at position (@x, @y) in user-space coordinates.
 *
 * This function is logically equivalent to:
 * <informalexample><programlisting>
 * cairo_move_to (cr, x, y);
 * cairo_rel_line_to (cr, width, 0);
 * cairo_rel_line_to (cr, 0, height);
 * cairo_rel_line_to (cr, -width, 0);
 * cairo_close_path (cr);
 * </programlisting></informalexample>
 **/
void
cairo_rectangle (cairo_t *cr,
		 double x, double y,
		 double width, double height)
{
    if (cr->status)
	return;

    cairo_move_to (cr, x, y);
    cairo_rel_line_to (cr, width, 0);
    cairo_rel_line_to (cr, 0, height);
    cairo_rel_line_to (cr, -width, 0);
    cairo_close_path (cr);
}

/* XXX: NYI
void
cairo_stroke_to_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke_path (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
*/

/**
 * cairo_close_path:
 * @cr: a cairo context
 * 
 * Adds a line segment to the path from the current point to the
 * beginning of the current subpath, (the most recent point passed to
 * cairo_move_to()), and closes this subpath.
 *
 * The behavior of cairo_close_path() is distinct from simply calling
 * cairo_line_to() with the equivalent coordinate in the case of
 * stroking. When a closed subpath is stroked, there are no caps on
 * the ends of the subpath. Instead, their is a line join connecting
 * the final and initial segments of the subpath.
 **/
void
cairo_close_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_path_fixed_close_path (&cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_close_path);

/**
 * cairo_paint:
 * @cr: a cairo context
 * 
 * A drawing operator that paints the current source everywhere within
 * the current clip region.
 **/
void
cairo_paint (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_paint (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_paint_with_alpha:
 * @cr: a cairo context
 * @alpha: alpha value, between 0 (transparent) and 1 (opaque)
 * 
 * A drawing operator that paints the current source everywhere within
 * the current clip region using a mask of constant alpha value
 * @alpha. The effect is similar to cairo_paint(), but the drawing
 * is faded out using the alpha value.
 **/
void
cairo_paint_with_alpha (cairo_t *cr,
			double   alpha)
{
    cairo_color_t color;
    cairo_pattern_union_t pattern;
  
    if (cr->status)
	return;

    if (CAIRO_ALPHA_IS_OPAQUE (alpha)) {
	cairo_paint (cr);
	return;
    }

    if (CAIRO_ALPHA_IS_ZERO (alpha)) {
	return;
    }

    _cairo_color_init_rgba (&color, 1., 1., 1., alpha);
    _cairo_pattern_init_solid (&pattern.solid, &color);

    cr->status = _cairo_gstate_mask (cr->gstate, &pattern.base);
    if (cr->status)
	_cairo_set_error (cr, cr->status);

    _cairo_pattern_fini (&pattern.base);
}

/**
 * cairo_mask:
 * @cr: a cairo context
 * @pattern: a #cairo_pattern_t
 *
 * A drawing operator that paints the current source
 * using the alpha channel of @pattern as a mask. (Opaque
 * areas of @mask are painted with the source, transparent
 * areas are not painted.)
 */
void
cairo_mask (cairo_t         *cr,
	    cairo_pattern_t *pattern)
{
    if (cr->status)
	return;

    if (pattern == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }
    
    if (pattern->status) {
	_cairo_set_error (cr, pattern->status);
	return;
    }

    cr->status = _cairo_gstate_mask (cr->gstate, pattern);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_mask_surface:
 * @cr: a cairo context
 * @surface: a #cairo_surface_t
 * @surface_x: X coordinate at which to place the origin of @surface
 * @surface_y: Y coordinate at which to place the origin of @surface
 *
 * A drawing operator that paints the current source
 * using the alpha channel of @surface as a mask. (Opaque
 * areas of @surface are painted with the source, transparent
 * areas are not painted.) 
 */
void
cairo_mask_surface (cairo_t         *cr,
		    cairo_surface_t *surface,
		    double           surface_x,
		    double           surface_y)
{
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_for_surface (surface);

    cairo_matrix_init_translate (&matrix, - surface_x, - surface_y);
    cairo_pattern_set_matrix (pattern, &matrix);

    cairo_mask (cr, pattern);
    
    cairo_pattern_destroy (pattern);
}

/**
 * cairo_stroke:
 * @cr: a cairo context
 * 
 * A drawing operator that strokes the current path according to the
 * current line width, line join, line cap, and dash settings. After
 * cairo_stroke, the current path will be cleared from the cairo
 * context. See cairo_set_line_width(), cairo_set_line_join(),
 * cairo_set_line_cap(), cairo_set_dash(), and
 * cairo_stroke_preserve().
 **/
void
cairo_stroke (cairo_t *cr)
{
    cairo_stroke_preserve (cr);

    cairo_new_path (cr);
}

/**
 * cairo_stroke_preserve:
 * @cr: a cairo context
 * 
 * A drawing operator that strokes the current path according to the
 * current line width, line join, line cap, and dash settings. Unlike
 * cairo_stroke(), cairo_stroke_preserve preserves the path within the
 * cairo context.
 *
 * See cairo_set_line_width(), cairo_set_line_join(),
 * cairo_set_line_cap(), cairo_set_dash(), and
 * cairo_stroke_preserve().
 **/
void
cairo_stroke_preserve (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke (cr->gstate, &cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_stroke_preserve);

/**
 * cairo_fill:
 * @cr: a cairo context
 * 
 * A drawing operator that fills the current path according to the
 * current fill rule, (each sub-path is implicitly closed before being
 * filled). After cairo_fill, the current path will be cleared from
 * the cairo context. See cairo_set_fill_rule() and
 * cairo_fill_preserve().
 **/
void
cairo_fill (cairo_t *cr)
{
    cairo_fill_preserve (cr);

    cairo_new_path (cr);
}

/**
 * cairo_fill_preserve:
 * @cr: a cairo context
 * 
 * A drawing operator that fills the current path according to the
 * current fill rule, (each sub-path is implicitly closed before being
 * filled). Unlike cairo_fill(), cairo_fill_preserve preserves the
 * path within the cairo context.
 *
 * See cairo_set_fill_rule() and cairo_fill().
 **/
void
cairo_fill_preserve (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_fill (cr->gstate, &cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_fill_preserve);

void
cairo_copy_page (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_copy_page (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

void
cairo_show_page (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_page (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

cairo_bool_t
cairo_in_stroke (cairo_t *cr, double x, double y)
{
    int inside;

    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_stroke (cr->gstate,
					  &cr->path,
					  x, y, &inside);
    if (cr->status)
	return 0;

    return inside;
}

cairo_bool_t
cairo_in_fill (cairo_t *cr, double x, double y)
{
    int inside;

    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_fill (cr->gstate,
					&cr->path,
					x, y, &inside);
    if (cr->status) {
	_cairo_set_error (cr, cr->status);
	return 0;
    }

    return inside;
}

void
cairo_stroke_extents (cairo_t *cr,
                      double *x1, double *y1, double *x2, double *y2)
{
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_stroke_extents (cr->gstate,
					       &cr->path,
					       x1, y1, x2, y2);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

void
cairo_fill_extents (cairo_t *cr,
                    double *x1, double *y1, double *x2, double *y2)
{
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_fill_extents (cr->gstate,
					     &cr->path,
					     x1, y1, x2, y2);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_clip:
 * @cr: a cairo context
 * 
 * Establishes a new clip region by intersecting the current clip
 * region with the current path as it would be filled by cairo_fill()
 * and according to the current fill rule (see cairo_set_fill_rule()).
 *
 * After cairo_clip, the current path will be cleared from the cairo
 * context.
 *
 * The current clip region affects all drawing operations by
 * effectively masking out any changes to the surface that are outside
 * the current clip region.
 *
 * Calling cairo_clip() can only make the clip region smaller, never
 * larger. But the current clip is part of the graphics state, so a
 * temporary restriction of the clip region can be achieved by
 * calling cairo_clip() within a cairo_save()/cairo_restore()
 * pair. The only other means of increasing the size of the clip
 * region is cairo_reset_clip().
 **/
void
cairo_clip (cairo_t *cr)
{
    cairo_clip_preserve (cr);

    cairo_new_path (cr);
}

/**
 * cairo_clip_preserve:
 * @cr: a cairo context
 * 
 * Establishes a new clip region by intersecting the current clip
 * region with the current path as it would be filled by cairo_fill()
 * and according to the current fill rule (see cairo_set_fill_rule()).
 *
 * Unlike cairo_clip(), cairo_clip_preserve preserves the path within
 * the cairo context.
 *
 * The current clip region affects all drawing operations by
 * effectively masking out any changes to the surface that are outside
 * the current clip region.
 *
 * Calling cairo_clip() can only make the clip region smaller, never
 * larger. But the current clip is part of the graphics state, so a
 * temporary restriction of the clip region can be achieved by
 * calling cairo_clip() within a cairo_save()/cairo_restore()
 * pair. The only other means of increasing the size of the clip
 * region is cairo_reset_clip().
 **/
void
cairo_clip_preserve (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_clip (cr->gstate, &cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_clip_preserve);

/**
 * cairo_reset_clip:
 * @cr: a cairo context
 * 
 * Reset the current clip region to its original, unrestricted
 * state. That is, set the clip region to an infinitely large shape
 * containing the target surface. Equivalently, if infinity is too
 * hard to grasp, one can imagine the clip region being reset to the
 * exact bounds of the target surface.
 *
 * Note that code meant to be reusable should not call
 * cairo_reset_clip() as it will cause results unexpected by
 * higher-level code which calls cairo_clip(). Consider using
 * cairo_save() and cairo_restore() around cairo_clip() as a more
 * robust means of temporarily restricting the clip region.
 **/
void
cairo_reset_clip (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_reset_clip (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_select_font_face:
 * @cr: a #cairo_t
 * @family: a font family name, encoded in UTF-8
 * @slant: the slant for the font
 * @weight: the weight for the font
 * 
 * Selects a family and style of font from a simplified description as
 * a family name, slant and weight. This function is meant to be used
 * only for applications with simple font needs: Cairo doesn't provide
 * for operations such as listing all available fonts on the system,
 * and it is expected that most applications will need to use a more
 * comprehensive font handling and text layout library in addition to
 * cairo.
 **/
void
cairo_select_font_face (cairo_t              *cr, 
			const char           *family, 
			cairo_font_slant_t    slant, 
			cairo_font_weight_t   weight)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_select_font_face (cr->gstate, family, slant, weight);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_font_face:
 * @cr: a #cairo_t
 * 
 * Gets the current font face for a #cairo_t.
 *
 * Return value: the current font object. Can return %NULL
 *   on out-of-memory or if the context is already in
 *   an error state. This object is owned by cairo. To keep
 *   a reference to it, you must call cairo_font_face_reference().
 **/
cairo_font_face_t *
cairo_get_font_face (cairo_t *cr)
{
    cairo_font_face_t *font_face;

    if (cr->status)
	return (cairo_font_face_t*) &_cairo_font_face_nil;

    cr->status = _cairo_gstate_get_font_face (cr->gstate, &font_face);
    if (cr->status) {
	_cairo_set_error (cr, cr->status);
	return (cairo_font_face_t*) &_cairo_font_face_nil;
    }

    return font_face;
}

/**
 * cairo_font_extents:
 * @cr: a #cairo_t
 * @extents: a #cairo_font_extents_t object into which the results
 * will be stored.
 * 
 * Gets the font extents for the currently selected font.
 **/
void
cairo_font_extents (cairo_t              *cr, 
		    cairo_font_extents_t *extents)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_get_font_extents (cr->gstate, extents);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_font_face:
 * @cr: a #cairo_t
 * @font_face: a #cairo_font_face_t, or %NULL to restore to the default font
 *
 * Replaces the current #cairo_font_face_t object in the #cairo_t with
 * @font_face. The replaced font face in the #cairo_t will be
 * destroyed if there are no other references to it.
 **/
void
cairo_set_font_face (cairo_t           *cr,
		     cairo_font_face_t *font_face)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_face (cr->gstate, font_face);  
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_font_size:
 * @cr: a #cairo_t
 * @size: the new font size, in user space units
 * 
 * Sets the current font matrix to a scale by a factor of @size, replacing
 * any font matrix previously set with cairo_set_font_size() or
 * cairo_set_font_matrix(). This results in a font size of @size user space
 * units. (More precisely, this matrix will result in the font's
 * em-square being a @size by @size square in user space.)
 **/
void
cairo_set_font_size (cairo_t *cr, double size)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_size (cr->gstate, size);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_font_matrix
 * @cr: a #cairo_t
 * @matrix: a #cairo_matrix_t describing a transform to be applied to
 * the current font.
 *
 * Sets the current font matrix to @matrix. The font matrix gives a
 * transformation from the design space of the font (in this space,
 * the em-square is 1 unit by 1 unit) to user space. Normally, a
 * simple scale is used (see cairo_set_font_size()), but a more
 * complex font matrix can be used to shear the font
 * or stretch it unequally along the two axes
 **/
void
cairo_set_font_matrix (cairo_t		    *cr,
		       const cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_matrix (cr->gstate, matrix);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_font_matrix
 * @cr: a #cairo_t
 * @matrix: return value for the matrix
 *
 * Stores the current font matrix into @matrix. See
 * cairo_set_font_matrix().
 **/
void
cairo_get_font_matrix (cairo_t *cr, cairo_matrix_t *matrix)
{
    _cairo_gstate_get_font_matrix (cr->gstate, matrix);
}

/**
 * cairo_set_font_options:
 * @cr: a #cairo_t
 * @options: font options to use
 * 
 * Sets a set of custom font rendering options for the #cairo_t.
 * Rendering options are derived by merging these options with the
 * options derived from underlying surface; if the value in @options
 * has a default value (like %CAIRO_ANTIALIAS_DEFAULT), then the value
 * from the surface is used.
 **/
void
cairo_set_font_options (cairo_t                    *cr,
			const cairo_font_options_t *options)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_options (cr->gstate, options);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_font_options:
 * @cr: a #cairo_t
 * @options: a #cairo_font_options_t object into which to store
 *   the retrieved options. All existing values are overwritten
 * 
 * Retrieves font rendering options set via #cairo_set_font_options.
 * Note that the returned options do not include any options derived
 * from the underlying surface; they are literally the options
 * passed to cairo_set_font_options().
 **/
void
cairo_get_font_options (cairo_t              *cr,
			cairo_font_options_t *options)
{
    _cairo_gstate_get_font_options (cr->gstate, options);
}

/**
 * cairo_text_extents:
 * @cr: a #cairo_t
 * @utf8: a string of text, encoded in UTF-8
 * @extents: a #cairo_text_extents_t object into which the results
 * will be stored
 * 
 * Gets the extents for a string of text. The extents describe a
 * user-space rectangle that encloses the "inked" portion of the text,
 * (as it would be drawn by cairo_show_text()). Additionally, the
 * x_advance and y_advance values indicate the amount by which the
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
cairo_text_extents (cairo_t              *cr,
		    const char		 *utf8,
		    cairo_text_extents_t *extents)
{
    cairo_glyph_t *glyphs = NULL;
    int num_glyphs;
    double x, y;

    if (cr->status)
	return;

    if (utf8 == NULL) {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;
	return;
    }

    cairo_get_current_point (cr, &x, &y);

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8,
					       x, y,
					       &glyphs, &num_glyphs);

    if (cr->status) {
	if (glyphs)
	    free (glyphs);
	_cairo_set_error (cr, cr->status);
	return;
    }
	
    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, num_glyphs, extents);
    if (glyphs)
	free (glyphs);

    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_glyph_extents:
 * @cr: a #cairo_t
 * @glyphs: an array of #cairo_glyph_t objects
 * @num_glyphs: the number of elements in @glyphs
 * @extents: a #cairo_text_extents_t object into which the results
 * will be stored
 * 
 * Gets the extents for an array of glyphs. The extents describe a
 * user-space rectangle that encloses the "inked" portion of the
 * glyphs, (as they would be drawn by cairo_show_glyphs()).
 * Additionally, the x_advance and y_advance values indicate the
 * amount by which the current point would be advanced by
 * cairo_show_glyphs.
 * 
 * Note that whitespace glyphs do not contribute to the size of the
 * rectangle (extents.width and extents.height).
 **/
void
cairo_glyph_extents (cairo_t                *cr,
		     cairo_glyph_t          *glyphs, 
		     int                    num_glyphs,
		     cairo_text_extents_t   *extents)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, num_glyphs,
					      extents);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_show_text:
 * @cr: a cairo context
 * @utf8: a string of text encoded in UTF-8
 * 
 * A drawing operator that generates the shape from a string of UTF-8
 * characters, rendered according to the current font_face, font_size
 * (font_matrix), and font_options.
 *
 * This function first computes a set of glyphs for the string of
 * text. The first glyph is placed so that its origin is at the
 * current point. The origin of each subsequent glyph is offset from
 * that of the previous glyph by the advance values of the previous
 * glyph.
 *
 * After this call the current point is moved to the origin of where
 * the next glyph would be placed in this same progression. That is,
 * the current point will be at the origin of the final glyph offset
 * by its advance values. This allows for easy display of a single
 * logical string with multiple calls to cairo_show_text().
 *
 * NOTE: The cairo_show_text() function call is part of what the cairo
 * designers call the "toy" text API. It is convenient for short demos
 * and simple programs, but it is not expected to be adequate for the
 * most serious of text-using applications. See cairo_show_glyphs()
 * for the "real" text display API in cairo.
 **/
void
cairo_show_text (cairo_t *cr, const char *utf8)
{
    cairo_text_extents_t extents;
    cairo_glyph_t *glyphs = NULL, *last_glyph;
    int num_glyphs;
    double x, y;

    if (cr->status)
	return;

    if (utf8 == NULL)
	return;

    cairo_get_current_point (cr, &x, &y);

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8,
					       x, y,
					       &glyphs, &num_glyphs);
    if (cr->status)
	goto BAIL;

    if (num_glyphs == 0)
	return;

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, num_glyphs);
    if (cr->status)
	goto BAIL;

    last_glyph = &glyphs[num_glyphs - 1];
    cr->status = _cairo_gstate_glyph_extents (cr->gstate,
					      last_glyph, 1,
					      &extents);
    if (cr->status)
	goto BAIL;

    x = last_glyph->x + extents.x_advance;
    y = last_glyph->y + extents.y_advance;
    cairo_move_to (cr, x, y);

 BAIL:
    if (glyphs)
	free (glyphs);

    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

void
cairo_show_glyphs (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, num_glyphs);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

void
cairo_text_path  (cairo_t *cr, const char *utf8)
{
    cairo_glyph_t *glyphs = NULL;
    int num_glyphs;
    double x, y;

    if (cr->status)
	return;

    cairo_get_current_point (cr, &x, &y);

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8,
					       x, y,
					       &glyphs, &num_glyphs);

    if (cr->status) {
	if (glyphs)
	    free (glyphs);
	_cairo_set_error (cr, cr->status);
	return;
    }

    cr->status = _cairo_gstate_glyph_path (cr->gstate,
					   glyphs, num_glyphs,
					   &cr->path);
    if (glyphs)
	free (glyphs);

    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

void
cairo_glyph_path (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_path (cr->gstate,
					   glyphs, num_glyphs,
					   &cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_operator:
 * @cr: a cairo context
 * 
 * Gets the current compositing operator for a cairo context.
 * 
 * Return value: the current compositing operator.
 **/
cairo_operator_t
cairo_get_operator (cairo_t *cr)
{
    return _cairo_gstate_get_operator (cr->gstate);
}

/**
 * cairo_get_tolerance:
 * @cr: a cairo context
 * 
 * Gets the current tolerance value, as set by cairo_set_tolerance().
 * 
 * Return value: the current tolerance value.
 **/
double
cairo_get_tolerance (cairo_t *cr)
{
    return _cairo_gstate_get_tolerance (cr->gstate);
}

/**
 * cairo_get_antialias:
 * @cr: a cairo context
 * 
 * Gets the current shape antialiasing mode, as set by cairo_set_shape_antialias().
 * 
 * Return value: the current shape antialiasing mode.
 **/
cairo_antialias_t
cairo_get_antialias (cairo_t *cr)
{
    return _cairo_gstate_get_antialias (cr->gstate);
}

/**
 * cairo_get_current_point:
 * @cr: a cairo context
 * @x: return value for X coordinate of the current point
 * @y: return value for Y coordinate of the current point
 * 
 * Gets the current point of the current path, which is
 * conceptually the final point reached by the path so far.
 *
 * The current point is returned in the user-space coordinate
 * system. If there is no defined current point then @x and @y will
 * both be set to 0.0.
 *
 * Most path construction functions alter the current point. See the
 * following for details on how they affect the current point:
 *
 * cairo_new_path(), cairo_move_to(), cairo_line_to(),
 * cairo_curve_to(), cairo_arc(), cairo_rel_move_to(),
 * cairo_rel_line_to(), cairo_rel_curve_to(), cairo_arc(),
 * cairo_text_path(), cairo_stroke_to_path()
 **/
void
cairo_get_current_point (cairo_t *cr, double *x_ret, double *y_ret)
{
    cairo_status_t status;
    cairo_fixed_t x_fixed, y_fixed;
    double x, y;

    status = _cairo_path_fixed_get_current_point (&cr->path, &x_fixed, &y_fixed);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	x = 0.0;
	y = 0.0;
    } else {
	x = _cairo_fixed_to_double (x_fixed);
	y = _cairo_fixed_to_double (y_fixed);
	_cairo_gstate_backend_to_user (cr->gstate, &x, &y);
    }

    if (x_ret)
	*x_ret = x;
    if (y_ret)
	*y_ret = y;
}
slim_hidden_def(cairo_get_current_point);

/**
 * cairo_get_fill_rule:
 * @cr: a cairo context
 * 
 * Gets the current fill rule, as set by cairo_set_fill_rule().
 * 
 * Return value: the current fill rule.
 **/
cairo_fill_rule_t
cairo_get_fill_rule (cairo_t *cr)
{
    return _cairo_gstate_get_fill_rule (cr->gstate);
}

/**
 * cairo_get_line_width:
 * @cr: a cairo context
 * 
 * Gets the current line width, as set by cairo_set_line_width().
 * 
 * Return value: the current line width, in user-space units.
 **/
double
cairo_get_line_width (cairo_t *cr)
{
    return _cairo_gstate_get_line_width (cr->gstate);
}

/**
 * cairo_get_line_cap:
 * @cr: a cairo context
 * 
 * Gets the current line cap style, as set by cairo_set_line_cap().
 * 
 * Return value: the current line cap style.
 **/
cairo_line_cap_t
cairo_get_line_cap (cairo_t *cr)
{
    return _cairo_gstate_get_line_cap (cr->gstate);
}

/**
 * cairo_get_line_join:
 * @cr: a cairo context
 * 
 * Gets the current line join style, as set by cairo_set_line_join().
 * 
 * Return value: the current line join style.
 **/
cairo_line_join_t
cairo_get_line_join (cairo_t *cr)
{
    return _cairo_gstate_get_line_join (cr->gstate);
}

/**
 * cairo_get_miter_limit:
 * @cr: a cairo context
 * 
 * Gets the current miter limit, as set by cairo_set_miter_limit().
 * 
 * Return value: the current miter limit.
 **/
double
cairo_get_miter_limit (cairo_t *cr)
{
    return _cairo_gstate_get_miter_limit (cr->gstate);
}

/**
 * cairo_get_matrix:
 * @cr: a cairo context
 * @matrix: return value for the matrix
 *
 * Stores the current transformation matrix (CTM) into @matrix.
 **/
void
cairo_get_matrix (cairo_t *cr, cairo_matrix_t *matrix)
{
    _cairo_gstate_get_matrix (cr->gstate, matrix);
}

/**
 * cairo_get_target:
 * @cr: a cairo context
 * 
 * Gets the target surface for the cairo context as passed to
 * cairo_create().
 *
 * This function will always return a valid pointer, but the result
 * can be a "nil" surface if @cr is already in an error state,
 * (ie. cairo_status() <literal>!=</literal> %CAIRO_STATUS_SUCCESS).
 * A nil surface is indicated by cairo_surface_status()
 * <literal>!=</literal> %CAIRO_STATUS_SUCCESS.
 * 
 * Return value: the target surface. This object is owned by cairo. To
 * keep a reference to it, you must call cairo_surface_reference().
 **/
cairo_surface_t *
cairo_get_target (cairo_t *cr)
{
    if (cr->status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    return _cairo_gstate_get_original_target (cr->gstate);
}

/**
 * cairo_get_group_target:
 * @cr: a cairo context
 * 
 * Gets the target surface for the current transparency group
 * started by the last cairo_push_group() call on the cairo
 * context.
 *
 * This function may return NULL if there is no transparency
 * group on the target.
 * 
 * Return value: the target group surface, or NULL if none.  This
 * object is owned by cairo. To keep a reference to it, you must call
 * cairo_surface_reference().
 **/
cairo_surface_t *
cairo_get_group_target (cairo_t *cr)
{
    if (cr->status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    return _cairo_gstate_get_target (cr->gstate);
}

/**
 * cairo_copy_path:
 * @cr: a cairo context
 * 
 * Creates a copy of the current path and returns it to the user as a
 * #cairo_path_t. See #cairo_path_data_t for hints on how to iterate
 * over the returned data structure.
 * 
 * This function will always return a valid pointer, but the result
 * will have no data (<literal>data==NULL</literal> and
 * <literal>num_data==0</literal>), if either of the following
 * conditions hold:
 *
 * <orderedlist>
 * <listitem>If there is insufficient memory to copy the path.</listitem>
 * <listitem>If @cr is already in an error state.</listitem>
 * </orderedlist>
 *
 * In either case, <literal>path->status</literal> will be set to
 * %CAIRO_STATUS_NO_MEMORY (regardless of what the error status in
 * @cr might have been).
 *
 * Return value: the copy of the current path. The caller owns the
 * returned object and should call cairo_path_destroy() when finished
 * with it.
 **/
cairo_path_t *
cairo_copy_path (cairo_t *cr)
{
    if (cr->status)
	return (cairo_path_t*) &_cairo_path_nil;

    return _cairo_path_data_create (&cr->path, cr->gstate);
}

/**
 * cairo_copy_path_flat:
 * @cr: a cairo context
 * 
 * Gets a flattened copy of the current path and returns it to the
 * user as a #cairo_path_t. See #cairo_path_data_t for hints on
 * how to iterate over the returned data structure.
 *
 * This function is like cairo_copy_path() except that any curves
 * in the path will be approximated with piecewise-linear
 * approximations, (accurate to within the current tolerance
 * value). That is, the result is guaranteed to not have any elements
 * of type %CAIRO_PATH_CURVE_TO which will instead be replaced by a
 * series of %CAIRO_PATH_LINE_TO elements.
 *
 * This function will always return a valid pointer, but the result
 * will have no data (<literal>data==NULL</literal> and
 * <literal>num_data==0</literal>), if either of the following
 * conditions hold:
 *
 * <orderedlist>
 * <listitem>If there is insufficient memory to copy the path. In this
 *     case <literal>path->status</literal> will be set to
 *     %CAIRO_STATUS_NO_MEMORY.</listitem>
 * <listitem>If @cr is already in an error state. In this case
 *    <literal>path->status</literal> will contain the same status that
 *    would be returned by cairo_status().</listitem>
 * </orderedlist>
 * 
 * Return value: the copy of the current path. The caller owns the
 * returned object and should call cairo_path_destroy() when finished
 * with it.
 **/
cairo_path_t *
cairo_copy_path_flat (cairo_t *cr)
{
    if (cr->status)
	return (cairo_path_t*) &_cairo_path_nil;
    else
	return _cairo_path_data_create_flat (&cr->path, cr->gstate);
}

/**
 * cairo_append_path:
 * @cr: a cairo context
 * @path: path to be appended
 * 
 * Append the @path onto the current path. The @path may be either the
 * return value from one of cairo_copy_path() or
 * cairo_copy_path_flat() or it may be constructed manually.  See
 * #cairo_path_t for details on how the path data structure should be
 * initialized, and note that <literal>path->status</literal> must be
 * initialized to %CAIRO_STATUS_SUCCESS.
 **/
void
cairo_append_path (cairo_t	*cr,
		   cairo_path_t *path)
{
    if (cr->status)
	return;

    if (path == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (path->status) {
	if (path->status <= CAIRO_STATUS_LAST_STATUS)
	    _cairo_set_error (cr, path->status);
	else
	    _cairo_set_error (cr, CAIRO_STATUS_INVALID_STATUS);
	return;
    }

    if (path->data == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    cr->status = _cairo_path_data_append_to_context (path, cr);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_status:
 * @cr: a cairo context
 *
 * Checks whether an error has previously occurred for this context.
 *
 * Returns the current status of this context, see #cairo_status_t
 **/
cairo_status_t
cairo_status (cairo_t *cr)
{
    return cr->status;
}

/**
 * cairo_status_to_string:
 * @status: a cairo status
 *
 * Provides a human-readable description of a #cairo_status_t.
 *
 * Returns a string representation of the status
 */
const char *
cairo_status_to_string (cairo_status_t status)
{
    switch (status) {
    case CAIRO_STATUS_SUCCESS:
	return "success";
    case CAIRO_STATUS_NO_MEMORY:
	return "out of memory";
    case CAIRO_STATUS_INVALID_RESTORE:
	return "cairo_restore without matching cairo_save";
    case CAIRO_STATUS_INVALID_POP_GROUP:
	return "cairo_pop_group without matching cairo_push_group";
    case CAIRO_STATUS_NO_CURRENT_POINT:
	return "no current point defined";
    case CAIRO_STATUS_INVALID_MATRIX:
	return "invalid matrix (not invertible)";
    case CAIRO_STATUS_INVALID_STATUS:
	return "invalid value for an input cairo_status_t";
    case CAIRO_STATUS_NULL_POINTER:
	return "NULL pointer";
    case CAIRO_STATUS_INVALID_STRING:
	return "input string not valid UTF-8";
    case CAIRO_STATUS_INVALID_PATH_DATA:
	return "input path data not valid";
    case CAIRO_STATUS_READ_ERROR:
	return "error while reading from input stream";
    case CAIRO_STATUS_WRITE_ERROR:
	return "error while writing to output stream";
    case CAIRO_STATUS_SURFACE_FINISHED:
	return "the target surface has been finished";
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH:
	return "the surface type is not appropriate for the operation";
    case CAIRO_STATUS_PATTERN_TYPE_MISMATCH:
	return "the pattern type is not appropriate for the operation";
    case CAIRO_STATUS_INVALID_CONTENT:
	return "invalid value for an input cairo_content_t";
    case CAIRO_STATUS_INVALID_FORMAT:
	return "invalid value for an input cairo_format_t";
    case CAIRO_STATUS_INVALID_VISUAL:
	return "invalid value for an input Visual*";
    case CAIRO_STATUS_FILE_NOT_FOUND:
	return "file not found";
    case CAIRO_STATUS_INVALID_DASH:
	return "invalid value for a dash setting";
    }

    return "<unknown error status>";
}

void
_cairo_restrict_value (double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}
