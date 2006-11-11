/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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

void
_cairo_slope_init (cairo_slope_t *slope, cairo_point_t *a, cairo_point_t *b)
{
    slope->dx = b->x - a->x;
    slope->dy = b->y - a->y;
}

/* Compare two slopes. Slope angles begin at 0 in the direction of the
   positive X axis and increase in the direction of the positive Y
   axis.

   WARNING: This function only gives correct results if the angular
   difference between a and b is less than PI.

   <  0 => a less positive than b
   == 0 => a equal to be
   >  0 => a more positive than b
*/
int
_cairo_slope_compare (cairo_slope_t *a, cairo_slope_t *b)
{
    cairo_fixed_48_16_t diff;

    diff = ((cairo_fixed_48_16_t) a->dy * (cairo_fixed_48_16_t) b->dx 
	    - (cairo_fixed_48_16_t) b->dy * (cairo_fixed_48_16_t) a->dx);

    if (diff > 0)
	return 1;
    if (diff < 0)
	return -1;

    if (a->dx == 0 && a->dy == 0)
	return 1;
    if (b->dx == 0 && b->dy ==0)
	return -1;

    return 0;
}

/* XXX: It might be cleaner to move away from usage of
   _cairo_slope_clockwise/_cairo_slope_counter_clockwise in favor of
   directly using _cairo_slope_compare.
*/

/* Is a clockwise of b?
 *
 * NOTE: The strict equality here is not significant in and of itself,
 * but there are functions up above that are sensitive to it,
 * (cf. _cairo_pen_find_active_cw_vertex_index).
 */
int
_cairo_slope_clockwise (cairo_slope_t *a, cairo_slope_t *b)
{
    return _cairo_slope_compare (a, b) < 0;
}

int
_cairo_slope_counter_clockwise (cairo_slope_t *a, cairo_slope_t *b)
{
    return ! _cairo_slope_clockwise (a, b);
}




