/*
 * Copyright © 2002 Keith Packard
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
 *	Keith R. Packard <keithp@keithp.com>
 *	Carl D. Worth <cworth@cworth.org>
 *
 * 2002-07-15: Converted from XRenderCompositeDoublePoly to cairo_trap. Carl D. Worth
 */

#include "cairoint.h"

/* private functions */

static cairo_status_t
_cairo_traps_grow_by (cairo_traps_t *traps, int additional);

static cairo_status_t
_cairo_traps_add_trap (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
		       cairo_line_t *left, cairo_line_t *right);

static cairo_status_t
_cairo_traps_add_trap_from_points (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
				   cairo_point_t left_p1, cairo_point_t left_p2,
				   cairo_point_t right_p1, cairo_point_t right_p2);

static int
_compare_point_fixed_by_y (const void *av, const void *bv);

static int
_compare_cairo_edge_by_top (const void *av, const void *bv);

static int
_compare_cairo_edge_by_slope (const void *av, const void *bv);

static cairo_fixed_16_16_t
_compute_x (cairo_line_t *line, cairo_fixed_t y);

static int
_line_segs_intersect_ceil (cairo_line_t *left, cairo_line_t *right, cairo_fixed_t *y_ret);

void
_cairo_traps_init (cairo_traps_t *traps)
{
    traps->num_traps = 0;

    traps->traps_size = 0;
    traps->traps = NULL;
    traps->extents.p1.x = traps->extents.p1.y = CAIRO_MAXSHORT << 16;
    traps->extents.p2.x = traps->extents.p2.y = CAIRO_MINSHORT << 16;
}

void
_cairo_traps_fini (cairo_traps_t *traps)
{
    if (traps->traps_size) {
	free (traps->traps);
	traps->traps = NULL;
	traps->traps_size = 0;
	traps->num_traps = 0;
    }
}

/**
 * _cairo_traps_init_box:
 * @traps: a #cairo_traps_t
 * @box: a box that will be converted to a single trapezoid
 *       to store in @traps.
 * 
 * Initializes a cairo_traps_t to contain a single rectangular
 * trapezoid.
 **/
cairo_status_t
_cairo_traps_init_box (cairo_traps_t *traps,
		       cairo_box_t   *box)
{
  cairo_status_t status;
  
  _cairo_traps_init (traps);
  
  status = _cairo_traps_grow_by (traps, 1);
  if (status)
    return status;
  
  traps->num_traps = 1;

  traps->traps[0].top = box->p1.y;
  traps->traps[0].bottom = box->p2.y;
  traps->traps[0].left.p1 = box->p1;
  traps->traps[0].left.p2.x = box->p1.x;
  traps->traps[0].left.p2.y = box->p2.y;
  traps->traps[0].right.p1.x = box->p2.x;
  traps->traps[0].right.p1.y = box->p1.y;
  traps->traps[0].right.p2 = box->p2;

  traps->extents = *box;

  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_traps_add_trap (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
		       cairo_line_t *left, cairo_line_t *right)
{
    cairo_status_t status;
    cairo_trapezoid_t *trap;

    if (top == bottom) {
	return CAIRO_STATUS_SUCCESS;
    }

    if (traps->num_traps >= traps->traps_size) {
	int inc = traps->traps_size ? traps->traps_size : 32;
	status = _cairo_traps_grow_by (traps, inc);
	if (status)
	    return status;
    }

    trap = &traps->traps[traps->num_traps];
    trap->top = top;
    trap->bottom = bottom;
    trap->left = *left;
    trap->right = *right;

    if (top < traps->extents.p1.y)
	traps->extents.p1.y = top;
    if (bottom > traps->extents.p2.y)
	traps->extents.p2.y = bottom;
    /*
     * This isn't generally accurate, but it is close enough for
     * this purpose.  Assuming that the left and right segments always
     * contain the trapezoid vertical extents, these compares will
     * yield a containing box.  Assuming that the points all come from
     * the same figure which will eventually be completely drawn, then
     * the compares will yield the correct overall extents
     */
    if (left->p1.x < traps->extents.p1.x)
	traps->extents.p1.x = left->p1.x;
    if (left->p2.x < traps->extents.p1.x)
	traps->extents.p1.x = left->p2.x;
    
    if (right->p1.x > traps->extents.p2.x)
	traps->extents.p2.x = right->p1.x;
    if (right->p2.x > traps->extents.p2.x)
	traps->extents.p2.x = right->p2.x;
    
    traps->num_traps++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_traps_add_trap_from_points (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
				   cairo_point_t left_p1, cairo_point_t left_p2,
				   cairo_point_t right_p1, cairo_point_t right_p2)
{
    cairo_line_t left;
    cairo_line_t right;

    left.p1 = left_p1;
    left.p2 = left_p2;

    right.p1 = right_p1;
    right.p2 = right_p2;

    return _cairo_traps_add_trap (traps, top, bottom, &left, &right);
}

static cairo_status_t
_cairo_traps_grow_by (cairo_traps_t *traps, int additional)
{
    cairo_trapezoid_t *new_traps;
    int old_size = traps->traps_size;
    int new_size = traps->num_traps + additional;

    if (new_size <= traps->traps_size) {
	return CAIRO_STATUS_SUCCESS;
    }

    traps->traps_size = new_size;
    new_traps = realloc (traps->traps, traps->traps_size * sizeof (cairo_trapezoid_t));

    if (new_traps == NULL) {
	traps->traps_size = old_size;
	return CAIRO_STATUS_NO_MEMORY;
    }

    traps->traps = new_traps;

    return CAIRO_STATUS_SUCCESS;
}

static int
_compare_point_fixed_by_y (const void *av, const void *bv)
{
    const cairo_point_t	*a = av, *b = bv;

    int ret = a->y - b->y;
    if (ret == 0) {
	ret = a->x - b->x;
    }
    return ret;
}

void
_cairo_traps_translate (cairo_traps_t *traps, int x, int y)
{
    cairo_fixed_t xoff, yoff;
    cairo_trapezoid_t *t;
    int i;

    /* Ugh. The cairo_composite/(Render) interface doesn't allow
       an offset for the trapezoids. Need to manually shift all
       the coordinates to align with the offset origin of the
       intermediate surface. */

    xoff = _cairo_fixed_from_int (x);
    yoff = _cairo_fixed_from_int (y);

    for (i = 0, t = traps->traps; i < traps->num_traps; i++, t++) {
	t->top += yoff;
	t->bottom += yoff;
	t->left.p1.x += xoff;
	t->left.p1.y += yoff;
	t->left.p2.x += xoff;
	t->left.p2.y += yoff;
	t->right.p1.x += xoff;
	t->right.p1.y += yoff;
	t->right.p2.x += xoff;
	t->right.p2.y += yoff;
    }
}

void
_cairo_trapezoid_array_translate_and_scale (cairo_trapezoid_t *offset_traps,
                                            cairo_trapezoid_t *src_traps,
                                            int num_traps,
                                            double tx, double ty,
                                            double sx, double sy)
{
    int i;
    cairo_fixed_t xoff = _cairo_fixed_from_double (tx);
    cairo_fixed_t yoff = _cairo_fixed_from_double (ty);

    if (sx == 1.0 && sy == 1.0) {
        for (i = 0; i < num_traps; i++) {
            offset_traps[i].top = src_traps[i].top + yoff;
            offset_traps[i].bottom = src_traps[i].bottom + yoff;
            offset_traps[i].left.p1.x = src_traps[i].left.p1.x + xoff;
            offset_traps[i].left.p1.y = src_traps[i].left.p1.y + yoff;
            offset_traps[i].left.p2.x = src_traps[i].left.p2.x + xoff;
            offset_traps[i].left.p2.y = src_traps[i].left.p2.y + yoff;
            offset_traps[i].right.p1.x = src_traps[i].right.p1.x + xoff;
            offset_traps[i].right.p1.y = src_traps[i].right.p1.y + yoff;
            offset_traps[i].right.p2.x = src_traps[i].right.p2.x + xoff;
            offset_traps[i].right.p2.y = src_traps[i].right.p2.y + yoff;
        }
    } else {
        cairo_fixed_t xsc = _cairo_fixed_from_double (sx);
        cairo_fixed_t ysc = _cairo_fixed_from_double (sy);

        for (i = 0; i < num_traps; i++) {
#define FIXED_MUL(_a, _b) \
            (_cairo_int64_to_int32(_cairo_int64_rsl(_cairo_int32x32_64_mul((_a), (_b)), 16)))

            offset_traps[i].top = FIXED_MUL(src_traps[i].top + yoff, ysc);
            offset_traps[i].bottom = FIXED_MUL(src_traps[i].bottom + yoff, ysc);
            offset_traps[i].left.p1.x = FIXED_MUL(src_traps[i].left.p1.x + xoff, xsc);
            offset_traps[i].left.p1.y = FIXED_MUL(src_traps[i].left.p1.y + yoff, ysc);
            offset_traps[i].left.p2.x = FIXED_MUL(src_traps[i].left.p2.x + xoff, xsc);
            offset_traps[i].left.p2.y = FIXED_MUL(src_traps[i].left.p2.y + yoff, ysc);
            offset_traps[i].right.p1.x = FIXED_MUL(src_traps[i].right.p1.x + xoff, xsc);
            offset_traps[i].right.p1.y = FIXED_MUL(src_traps[i].right.p1.y + yoff, ysc);
            offset_traps[i].right.p2.x = FIXED_MUL(src_traps[i].right.p2.x + xoff, xsc);
            offset_traps[i].right.p2.y = FIXED_MUL(src_traps[i].right.p2.y + yoff, ysc);

#undef FIXED_MUL
        }
    }
}

cairo_status_t
_cairo_traps_tessellate_triangle (cairo_traps_t *traps, cairo_point_t t[3])
{
    cairo_status_t status;
    cairo_line_t line;
    cairo_fixed_16_16_t intersect;
    cairo_point_t tsort[3];

    memcpy (tsort, t, 3 * sizeof (cairo_point_t));
    qsort (tsort, 3, sizeof (cairo_point_t), _compare_point_fixed_by_y);

    /* horizontal top edge requires special handling */
    if (tsort[0].y == tsort[1].y) {
	if (tsort[0].x < tsort[1].x)
	    status = _cairo_traps_add_trap_from_points (traps,
							tsort[1].y, tsort[2].y,
							tsort[0], tsort[2],
							tsort[1], tsort[2]);
	else
	    status = _cairo_traps_add_trap_from_points (traps,
							tsort[1].y, tsort[2].y,
							tsort[1], tsort[2],
							tsort[0], tsort[2]);
	return status;
    }

    line.p1 = tsort[0];
    line.p2 = tsort[1];

    intersect = _compute_x (&line, tsort[2].y);

    if (intersect < tsort[2].x) {
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[0].y, tsort[1].y,
						    tsort[0], tsort[1],
						    tsort[0], tsort[2]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[1].y, tsort[2].y,
						    tsort[1], tsort[2],
						    tsort[0], tsort[2]);
	if (status)
	    return status;
    } else {
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[0].y, tsort[1].y,
						    tsort[0], tsort[2],
						    tsort[0], tsort[1]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[1].y, tsort[2].y,
						    tsort[0], tsort[2],
						    tsort[1], tsort[2]);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Warning: This function reorders the elements of the array provided. */
cairo_status_t
_cairo_traps_tessellate_rectangle (cairo_traps_t *traps, cairo_point_t q[4])
{
    cairo_status_t status;

    qsort (q, 4, sizeof (cairo_point_t), _compare_point_fixed_by_y);

    if (q[1].x > q[2].x) {
	status = _cairo_traps_add_trap_from_points (traps,
						    q[0].y, q[1].y, q[0], q[2], q[0], q[1]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[1].y, q[2].y, q[0], q[2], q[1], q[3]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[2].y, q[3].y, q[2], q[3], q[1], q[3]);
	if (status)
	    return status;
    } else {
	status = _cairo_traps_add_trap_from_points (traps,
						    q[0].y, q[1].y, q[0], q[1], q[0], q[2]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[1].y, q[2].y, q[1], q[3], q[0], q[2]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[2].y, q[3].y, q[1], q[3], q[2], q[3]);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static int
_compare_cairo_edge_by_top (const void *av, const void *bv)
{
    const cairo_edge_t *a = av, *b = bv;

    return a->edge.p1.y - b->edge.p1.y;
}

/* Return value is:
   > 0 if a is "clockwise" from b, (in a mathematical, not a graphical sense)
   == 0 if slope (a) == slope (b)
   < 0 if a is "counter-clockwise" from b
*/
static int
_compare_cairo_edge_by_slope (const void *av, const void *bv)
{
    const cairo_edge_t *a = av, *b = bv;
    cairo_fixed_32_32_t d;

    cairo_fixed_48_16_t a_dx = a->edge.p2.x - a->edge.p1.x;
    cairo_fixed_48_16_t a_dy = a->edge.p2.y - a->edge.p1.y;
    cairo_fixed_48_16_t b_dx = b->edge.p2.x - b->edge.p1.x;
    cairo_fixed_48_16_t b_dy = b->edge.p2.y - b->edge.p1.y;

    d = b_dy * a_dx - a_dy * b_dx;

    if (d > 0)
	return 1;
    else if (d == 0)
	return 0;
    else
	return -1;
}

static int
_compare_cairo_edge_by_current_x_slope (const void *av, const void *bv)
{
    const cairo_edge_t *a = av, *b = bv;
    int ret;

    ret = a->current_x - b->current_x;
    if (ret == 0)
	ret = _compare_cairo_edge_by_slope (a, b);
    return ret;
}

/* XXX: Both _compute_x and _compute_inverse_slope will divide by zero
   for horizontal lines. Now, we "know" that when we are tessellating
   polygons that the polygon data structure discards all horizontal
   edges, but there's nothing here to guarantee that. I suggest the
   following:

   A) Move all of the polygon tessellation code out of xrtraps.c and
      into xrpoly.c, (in order to be in the same module as the code
      discarding horizontal lines).

   OR

   B) Re-implement the line intersection in a way that avoids all
      division by zero. Here's one approach. The only disadvantage
      might be that that there are not meaningful names for all of the
      sub-computations -- just a bunch of determinants. I haven't
      looked at complexity, (both are probably similar and it probably
      doesn't matter much anyway).
 */

/* XXX: Keith's new intersection code is much cleaner, and uses
 * sufficient precision for correctly sorting intersections according
 * to the analysis in Hobby's paper.
 *
 * But, when we enable this code, some things are failing, (eg. the
 * stars in test/fill_rule get filled wrong). This could indicate a
 * bug in one of tree places:
 *
 *	1) The new intersection code in this file
 *
 *	2) cairo_wideint.c (which is only exercised here)
 *
 *      3) In the current tessellator, (where the old intersection
 *	   code, with its mystic increments could be masking the bug).
 *
 * It will likely be easier to revisit this when the new tessellation
 * code is in place. So, for now, we'll simply disable the new
 * intersection code.
 */

#define CAIRO_TRAPS_USE_NEW_INTERSECTION_CODE 0

#if CAIRO_TRAPS_USE_NEW_INTERSECTION_CODE
static const cairo_fixed_32_32_t
_det16_32 (cairo_fixed_16_16_t a,
	   cairo_fixed_16_16_t b,
	   cairo_fixed_16_16_t c,
	   cairo_fixed_16_16_t d)
{
    return _cairo_int64_sub (_cairo_int32x32_64_mul (a, d),
			     _cairo_int32x32_64_mul (b, c));
}

static const cairo_fixed_64_64_t
_det32_64 (cairo_fixed_32_32_t a,
	   cairo_fixed_32_32_t b,
	   cairo_fixed_32_32_t c,
	   cairo_fixed_32_32_t d)
{
    return _cairo_int128_sub (_cairo_int64x64_128_mul (a, d),
			      _cairo_int64x64_128_mul (b, c));
}

static const cairo_fixed_32_32_t
_fixed_16_16_to_fixed_32_32 (cairo_fixed_16_16_t a)
{
    return _cairo_int64_lsl (_cairo_int32_to_int64 (a), 16);
}

static int
_line_segs_intersect_ceil (cairo_line_t *l1, cairo_line_t *l2, cairo_fixed_t *y_intersection)
{
    cairo_fixed_16_16_t	dx1, dx2, dy1, dy2;
    cairo_fixed_32_32_t	den_det;
    cairo_fixed_32_32_t	l1_det, l2_det;
    cairo_fixed_64_64_t num_det;
    cairo_fixed_32_32_t	intersect_32_32;
    cairo_fixed_48_16_t	intersect_48_16;
    cairo_fixed_16_16_t	intersect_16_16;
    cairo_quorem128_t	qr;

    dx1 = l1->p1.x - l1->p2.x;
    dy1 = l1->p1.y - l1->p2.y;
    dx2 = l2->p1.x - l2->p2.x;
    dy2 = l2->p1.y - l2->p2.y;
    den_det = _det16_32 (dx1, dy1,
			 dx2, dy2);
    
    if (_cairo_int64_eq (den_det, _cairo_int32_to_int64(0)))
	return 0;

    l1_det = _det16_32 (l1->p1.x, l1->p1.y,
			l1->p2.x, l1->p2.y);
    l2_det = _det16_32 (l2->p1.x, l2->p1.y,
			l2->p2.x, l2->p2.y);

    
    num_det = _det32_64 (l1_det, _fixed_16_16_to_fixed_32_32 (dy1),
			 l2_det, _fixed_16_16_to_fixed_32_32 (dy2));
    
    /*
     * Ok, this one is a bit tricky in fixed point, the denominator
     * needs to be left with 32-bits of fraction so that the
     * result of the divide ends up with 32-bits of fraction (64 - 32 = 32)
     */
    qr = _cairo_int128_divrem (num_det, _cairo_int64_to_int128 (den_det));
    
    intersect_32_32 = _cairo_int128_to_int64 (qr.quo);
    
    /*
     * Find the ceiling of the quotient -- divrem returns
     * the quotient truncated towards zero, so if the
     * quotient should be positive (num_den and den_det have same sign)
     * bump the quotient up by one.
     */
    
    if (_cairo_int128_ne (qr.rem, _cairo_int32_to_int128 (0)) &&
	(_cairo_int128_ge (num_det, _cairo_int32_to_int128 (0)) ==
	 _cairo_int64_ge (den_det, _cairo_int32_to_int64 (0))))
    {
	intersect_32_32 = _cairo_int64_add (intersect_32_32,
					    _cairo_int32_to_int64 (1));
    }
	
    /* 
     * Now convert from 32.32 to 48.16 and take the ceiling;
     * this requires adding in 15 1 bits and shifting the result
     */

    intersect_32_32 = _cairo_int64_add (intersect_32_32,
					_cairo_int32_to_int64 ((1 << 16) - 1));
    intersect_48_16 = _cairo_int64_rsa (intersect_32_32, 16);
    
    /*
     * And drop the top bits
     */
    intersect_16_16 = _cairo_int64_to_int32 (intersect_48_16);
    
    *y_intersection = intersect_16_16;

    return 1;
}
#endif /* CAIRO_TRAPS_USE_NEW_INTERSECTION_CODE */

static cairo_fixed_16_16_t
_compute_x (cairo_line_t *line, cairo_fixed_t y)
{
    cairo_fixed_16_16_t dx = line->p2.x - line->p1.x;
    cairo_fixed_32_32_t ex = (cairo_fixed_48_16_t) (y - line->p1.y) * (cairo_fixed_48_16_t) dx;
    cairo_fixed_16_16_t dy = line->p2.y - line->p1.y;

    return line->p1.x + (ex / dy);
}

#if ! CAIRO_TRAPS_USE_NEW_INTERSECTION_CODE
static double
_compute_inverse_slope (cairo_line_t *l)
{
    return (_cairo_fixed_to_double (l->p2.x - l->p1.x) / 
	    _cairo_fixed_to_double (l->p2.y - l->p1.y));
}

static double
_compute_x_intercept (cairo_line_t *l, double inverse_slope)
{
    return _cairo_fixed_to_double (l->p1.x) - inverse_slope * _cairo_fixed_to_double (l->p1.y);
}

static int
_line_segs_intersect_ceil (cairo_line_t *l1, cairo_line_t *l2, cairo_fixed_t *y_ret)
{
    /*
     * x = m1y + b1
     * x = m2y + b2
     * m1y + b1 = m2y + b2
     * y * (m1 - m2) = b2 - b1
     * y = (b2 - b1) / (m1 - m2)
     */
    cairo_fixed_16_16_t y_intersect;
    double  m1 = _compute_inverse_slope (l1);
    double  b1 = _compute_x_intercept (l1, m1);
    double  m2 = _compute_inverse_slope (l2);
    double  b2 = _compute_x_intercept (l2, m2);

    if (m1 == m2)
	return 0;

    y_intersect = _cairo_fixed_from_double ((b2 - b1) / (m1 - m2));

    if (m1 < m2) {
	cairo_line_t *t;
	t = l1;
	l1 = l2;
	l2 = t;
    }

    /* Assuming 56 bits of floating point precision, the intersection
       is accurate within one sub-pixel coordinate. We must ensure
       that we return a value that is at or after the intersection. At
       most, we must increment once. */
    if (_compute_x (l2, y_intersect) > _compute_x (l1, y_intersect))
	y_intersect++;
    /* XXX: Hmm... Keith's error calculations said we'd at most be off
       by one sub-pixel. But, I found that the paint-fill-BE-01.svg
       test from the W3C SVG conformance suite definitely requires two
       increments.

       It could be that we need one to overcome the error, and another
       to round up.

       It would be nice to be sure this code is correct, (but we can't
       do the while loop as it will work for way to long on
       exceedingly distant intersections with large errors that we
       really don't care about anyway as they will be ignored by the
       calling function.
    */
    if (_compute_x (l2, y_intersect) > _compute_x (l1, y_intersect))
	y_intersect++;
    /* XXX: hmm... now I found "intersection_killer" inside xrspline.c
       that requires 3 increments. Clearly, we haven't characterized
       this completely yet. */
    if (_compute_x (l2, y_intersect) > _compute_x (l1, y_intersect))
	y_intersect++;
    /* I think I've found the answer to our problems. The insight is
       that everytime we round we are changing the slopes of the
       relevant lines, so we may be introducing new intersections that
       we miss, so everything breaks apart. John Hobby wrote a paper
       on how to fix this:

       [Hobby93c] John D. Hobby, Practical Segment Intersection with
       Finite Precision Output, Computation Geometry Theory and
       Applications, 13(4), 1999.

       Available online (2003-08017):

       http://cm.bell-labs.com/cm/cs/doc/93/2-27.ps.gz

       Now we just need to go off and implement that.
    */

    *y_ret = y_intersect;

    return 1;
}
#endif /* CAIRO_TRAPS_USE_NEW_INTERSECTION_CODE */

/* The algorithm here is pretty simple:

   inactive = [edges]
   y = min_p1_y (inactive)

   while (num_active || num_inactive) {
   	active = all edges containing y

	next_y = min ( min_p2_y (active), min_p1_y (inactive), min_intersection (active) )

	fill_traps (active, y, next_y, fill_rule)

	y = next_y
   }

   The invariants that hold during fill_traps are:

   	All edges in active contain both y and next_y
	No edges in active intersect within y and next_y

   These invariants mean that fill_traps is as simple as sorting the
   active edges, forming a trapezoid between each adjacent pair. Then,
   either the even-odd or winding rule is used to determine whether to
   emit each of these trapezoids.

   Warning: This function obliterates the edges of the polygon provided.
*/
cairo_status_t
_cairo_traps_tessellate_polygon (cairo_traps_t		*traps,
				 cairo_polygon_t	*poly,
				 cairo_fill_rule_t	fill_rule)
{
    cairo_status_t	status;
    int 		i, active, inactive;
    cairo_fixed_t	y, y_next, intersect;
    int			in_out, num_edges = poly->num_edges;
    cairo_edge_t	*edges = poly->edges;

    if (num_edges == 0)
	return CAIRO_STATUS_SUCCESS;

    qsort (edges, num_edges, sizeof (cairo_edge_t), _compare_cairo_edge_by_top);
    
    y = edges[0].edge.p1.y;
    active = 0;
    inactive = 0;
    while (active < num_edges) {
	while (inactive < num_edges && edges[inactive].edge.p1.y <= y)
	    inactive++;

	for (i = active; i < inactive; i++)
	    edges[i].current_x = _compute_x (&edges[i].edge, y);

	qsort (&edges[active], inactive - active,
	       sizeof (cairo_edge_t), _compare_cairo_edge_by_current_x_slope);

	/* find next inflection point */
	y_next = edges[active].edge.p2.y;

	for (i = active; i < inactive; i++) {
	    if (edges[i].edge.p2.y < y_next)
		y_next = edges[i].edge.p2.y;
	    /* check intersect */
	    if (i != inactive - 1 && edges[i].current_x != edges[i+1].current_x)
		if (_line_segs_intersect_ceil (&edges[i].edge, &edges[i+1].edge,
					       &intersect))
		    if (intersect > y && intersect < y_next)
			y_next = intersect;
	}
	/* check next inactive point */
	if (inactive < num_edges && edges[inactive].edge.p1.y < y_next)
	    y_next = edges[inactive].edge.p1.y;

	/* walk the active edges generating trapezoids */
	in_out = 0;
	for (i = active; i < inactive - 1; i++) {
	    if (fill_rule == CAIRO_FILL_RULE_WINDING) {
		if (edges[i].clockWise)
		    in_out++;
		else
		    in_out--;
		if (in_out == 0)
		    continue;
	    } else {
		in_out++;
		if ((in_out & 1) == 0)
		    continue;
	    }
	    status = _cairo_traps_add_trap (traps, y, y_next, &edges[i].edge, &edges[i+1].edge);
	    if (status)
		return status;
	}

	/* delete inactive edges */
	for (i = active; i < inactive; i++) {
	    if (edges[i].edge.p2.y <= y_next) {
		memmove (&edges[active+1], &edges[active], (i - active) * sizeof (cairo_edge_t));
		active++;
	    }
	}

	y = y_next;
    }
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_trap_contains (cairo_trapezoid_t *t, cairo_point_t *pt)
{
    cairo_slope_t slope_left, slope_pt, slope_right;
    
    if (t->top > pt->y)
	return FALSE;
    if (t->bottom < pt->y)
	return FALSE;
    
    _cairo_slope_init (&slope_left, &t->left.p1, &t->left.p2);
    _cairo_slope_init (&slope_pt, &t->left.p1, pt);

    if (_cairo_slope_compare (&slope_left, &slope_pt) < 0)
	return FALSE;

    _cairo_slope_init (&slope_right, &t->right.p1, &t->right.p2);
    _cairo_slope_init (&slope_pt, &t->right.p1, pt);

    if (_cairo_slope_compare (&slope_pt, &slope_right) < 0)
	return FALSE;

    return TRUE;
}

cairo_bool_t
_cairo_traps_contain (cairo_traps_t *traps, double x, double y)
{
    int i;
    cairo_point_t point;

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    for (i = 0; i < traps->num_traps; i++) {
	if (_cairo_trap_contains (&traps->traps[i], &point))
	    return TRUE;
    }

    return FALSE;
}

void
_cairo_traps_extents (cairo_traps_t *traps, cairo_box_t *extents)
{
    *extents = traps->extents;
}

/**
 * _cairo_traps_extract_region:
 * @traps: a #cairo_traps_t
 * @region: on return, %NULL is stored here if the trapezoids aren't
 *          exactly representable as a pixman region, otherwise a
 *          a pointer to such a region, newly allocated.
 *          (free with pixman region destroy)
 * 
 * Determines if a set of trapezoids are exactly representable as a
 * pixman region, and if so creates such a region.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY
 **/
cairo_status_t
_cairo_traps_extract_region (cairo_traps_t      *traps,
			     pixman_region16_t **region)
{
    int i;

    for (i = 0; i < traps->num_traps; i++)
	if (!(traps->traps[i].left.p1.x == traps->traps[i].left.p2.x
	      && traps->traps[i].right.p1.x == traps->traps[i].right.p2.x
	      && _cairo_fixed_is_integer(traps->traps[i].top)
	      && _cairo_fixed_is_integer(traps->traps[i].bottom)
	      && _cairo_fixed_is_integer(traps->traps[i].left.p1.x)
	      && _cairo_fixed_is_integer(traps->traps[i].right.p1.x))) {
	    *region = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    
    *region = pixman_region_create ();

    for (i = 0; i < traps->num_traps; i++) {
	int x = _cairo_fixed_integer_part(traps->traps[i].left.p1.x);
	int y = _cairo_fixed_integer_part(traps->traps[i].top);
	int width = _cairo_fixed_integer_part(traps->traps[i].right.p1.x) - x;
	int height = _cairo_fixed_integer_part(traps->traps[i].bottom) - y;

	/* XXX: Sometimes we get degenerate trapezoids from the tesellator,
	 * if we call pixman_region_union_rect(), it bizarrly fails on such
	 * an empty rectangle, so skip them.
	 */
	if (width == 0 || height == 0)
	  continue;
	
	if (pixman_region_union_rect (*region, *region,
				      x, y, width, height) != PIXMAN_REGION_STATUS_SUCCESS) {
	    pixman_region_destroy (*region);
	    return CAIRO_STATUS_NO_MEMORY;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}
