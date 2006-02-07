/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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

typedef struct cairo_path_bounder {
    int has_point;

    cairo_fixed_t min_x;
    cairo_fixed_t min_y;
    cairo_fixed_t max_x;
    cairo_fixed_t max_y;
} cairo_path_bounder_t;

static void
_cairo_path_bounder_init (cairo_path_bounder_t *bounder);

static void
_cairo_path_bounder_fini (cairo_path_bounder_t *bounder);

static cairo_status_t
_cairo_path_bounder_add_point (cairo_path_bounder_t *bounder, cairo_point_t *point);

static cairo_status_t
_cairo_path_bounder_move_to (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_path_bounder_line_to (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_path_bounder_curve_to (void *closure,
			      cairo_point_t *b,
			      cairo_point_t *c,
			      cairo_point_t *d);

static cairo_status_t
_cairo_path_bounder_close_path (void *closure);

static void
_cairo_path_bounder_init (cairo_path_bounder_t *bounder)
{
    bounder->has_point = 0;
}

static void
_cairo_path_bounder_fini (cairo_path_bounder_t *bounder)
{
    bounder->has_point = 0;
}

static cairo_status_t
_cairo_path_bounder_add_point (cairo_path_bounder_t *bounder, cairo_point_t *point)
{
    if (bounder->has_point) {
	if (point->x < bounder->min_x)
	    bounder->min_x = point->x;
	
	if (point->y < bounder->min_y)
	    bounder->min_y = point->y;
	
	if (point->x > bounder->max_x)
	    bounder->max_x = point->x;
	
	if (point->y > bounder->max_y)
	    bounder->max_y = point->y;
    } else {
	bounder->min_x = point->x;
	bounder->min_y = point->y;
	bounder->max_x = point->x;
	bounder->max_y = point->y;

	bounder->has_point = 1;
    }
	
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_move_to (void *closure, cairo_point_t *point)
{
    cairo_path_bounder_t *bounder = closure;

    _cairo_path_bounder_add_point (bounder, point);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_line_to (void *closure, cairo_point_t *point)
{
    cairo_path_bounder_t *bounder = closure;

    _cairo_path_bounder_add_point (bounder, point);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_curve_to (void *closure,
			      cairo_point_t *b,
			      cairo_point_t *c,
			      cairo_point_t *d)
{
    cairo_path_bounder_t *bounder = closure;

    _cairo_path_bounder_add_point (bounder, b);
    _cairo_path_bounder_add_point (bounder, c);
    _cairo_path_bounder_add_point (bounder, d);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_close_path (void *closure)
{
    return CAIRO_STATUS_SUCCESS;
}

/* XXX: Perhaps this should compute a PixRegion rather than 4 doubles */
cairo_status_t
_cairo_path_fixed_bounds (cairo_path_fixed_t *path,
			  double *x1, double *y1,
			  double *x2, double *y2)
{
    cairo_status_t status;

    cairo_path_bounder_t bounder;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret (path, CAIRO_DIRECTION_FORWARD,
					  _cairo_path_bounder_move_to,
					  _cairo_path_bounder_line_to,
					  _cairo_path_bounder_curve_to,
					  _cairo_path_bounder_close_path,
					  &bounder);
    if (status) {
	*x1 = *y1 = *x2 = *y2 = 0.0;
	_cairo_path_bounder_fini (&bounder);
	return status;
    }

    *x1 = _cairo_fixed_to_double (bounder.min_x);
    *y1 = _cairo_fixed_to_double (bounder.min_y);
    *x2 = _cairo_fixed_to_double (bounder.max_x);
    *y2 = _cairo_fixed_to_double (bounder.max_y);

    _cairo_path_bounder_fini (&bounder);

    return CAIRO_STATUS_SUCCESS;
}
