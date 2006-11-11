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

typedef struct cairo_stroker {
    cairo_stroke_style_t	*style;

    cairo_matrix_t *ctm;
    cairo_matrix_t *ctm_inverse;
    double tolerance;

    cairo_traps_t *traps;

    cairo_pen_t	  pen;

    cairo_bool_t has_current_point;
    cairo_point_t current_point;
    cairo_point_t first_point;

    cairo_bool_t has_current_face;
    cairo_stroke_face_t current_face;

    cairo_bool_t has_first_face;
    cairo_stroke_face_t first_face;

    cairo_bool_t dashed;
    int dash_index;
    int dash_on;
    double dash_remain;
} cairo_stroker_t;

/* private functions */
static void
_cairo_stroker_init (cairo_stroker_t		*stroker,
		     cairo_stroke_style_t	*stroke_style,
		     cairo_matrix_t		*ctm,
		     cairo_matrix_t		*ctm_inverse,
		     double			 tolerance,
		     cairo_traps_t		*traps);

static void
_cairo_stroker_fini (cairo_stroker_t *stroker);

static cairo_status_t
_cairo_stroker_move_to (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_stroker_line_to (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_stroker_line_to_dashed (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_stroker_curve_to (void *closure,
			 cairo_point_t *b,
			 cairo_point_t *c,
			 cairo_point_t *d);

static cairo_status_t
_cairo_stroker_curve_to_dashed (void *closure,
				cairo_point_t *b,
				cairo_point_t *c,
				cairo_point_t *d);

static cairo_status_t
_cairo_stroker_close_path (void *closure);

static void
_translate_point (cairo_point_t *point, cairo_point_t *offset);

static int
_cairo_stroker_face_clockwise (cairo_stroke_face_t *in, cairo_stroke_face_t *out);

static cairo_status_t
_cairo_stroker_join (cairo_stroker_t *stroker, cairo_stroke_face_t *in, cairo_stroke_face_t *out);

static void
_cairo_stroker_start_dash (cairo_stroker_t *stroker)
{
    double offset;
    int	on = 1;
    int	i = 0;

    offset = stroker->style->dash_offset;
    while (offset >= stroker->style->dash[i]) {
	offset -= stroker->style->dash[i];
	on = 1-on;
	if (++i == stroker->style->num_dashes)
	    i = 0;
    }
    stroker->dashed = TRUE;
    stroker->dash_index = i;
    stroker->dash_on = on;
    stroker->dash_remain = stroker->style->dash[i] - offset;
}

static void
_cairo_stroker_step_dash (cairo_stroker_t *stroker, double step)
{
    stroker->dash_remain -= step;
    if (stroker->dash_remain <= 0) {
	stroker->dash_index++;
	if (stroker->dash_index == stroker->style->num_dashes)
	    stroker->dash_index = 0;
	stroker->dash_on = 1-stroker->dash_on;
	stroker->dash_remain = stroker->style->dash[stroker->dash_index];
    }
}

static void
_cairo_stroker_init (cairo_stroker_t		*stroker,
		     cairo_stroke_style_t	*stroke_style,
		     cairo_matrix_t		*ctm,
		     cairo_matrix_t		*ctm_inverse,
		     double			 tolerance,
		     cairo_traps_t		*traps)
{
    stroker->style = stroke_style;
    stroker->ctm = ctm;
    stroker->ctm_inverse = ctm_inverse;
    stroker->tolerance = tolerance;
    stroker->traps = traps;

    _cairo_pen_init (&stroker->pen,
		     stroke_style->line_width / 2.0,
		     tolerance, ctm);
    
    stroker->has_current_point = FALSE;
    stroker->has_current_face = FALSE;
    stroker->has_first_face = FALSE;

    if (stroker->style->dash)
	_cairo_stroker_start_dash (stroker);
    else
	stroker->dashed = FALSE;
}

static void
_cairo_stroker_fini (cairo_stroker_t *stroker)
{
    _cairo_pen_fini (&stroker->pen);
}

static void
_translate_point (cairo_point_t *point, cairo_point_t *offset)
{
    point->x += offset->x;
    point->y += offset->y;
}

static int
_cairo_stroker_face_clockwise (cairo_stroke_face_t *in, cairo_stroke_face_t *out)
{
    cairo_slope_t in_slope, out_slope;

    _cairo_slope_init (&in_slope, &in->point, &in->cw);
    _cairo_slope_init (&out_slope, &out->point, &out->cw);

    return _cairo_slope_clockwise (&in_slope, &out_slope);
}

static cairo_status_t
_cairo_stroker_join (cairo_stroker_t *stroker, cairo_stroke_face_t *in, cairo_stroke_face_t *out)
{
    cairo_status_t	status;
    int			clockwise = _cairo_stroker_face_clockwise (out, in);
    cairo_point_t	*inpt, *outpt;

    if (in->cw.x == out->cw.x
	&& in->cw.y == out->cw.y
	&& in->ccw.x == out->ccw.x
	&& in->ccw.y == out->ccw.y) {
	return CAIRO_STATUS_SUCCESS;
    }

    if (clockwise) {
    	inpt = &in->ccw;
    	outpt = &out->ccw;
    } else {
    	inpt = &in->cw;
    	outpt = &out->cw;
    }

    switch (stroker->style->line_join) {
    case CAIRO_LINE_JOIN_ROUND: {
	int i;
	int start, step, stop;
	cairo_point_t tri[3];
	cairo_pen_t *pen = &stroker->pen;

	tri[0] = in->point;
	if (clockwise) {
	    _cairo_pen_find_active_ccw_vertex_index (pen, &in->dev_vector, &start);
	    step = -1;
	    _cairo_pen_find_active_ccw_vertex_index (pen, &out->dev_vector, &stop);
	} else {
	    _cairo_pen_find_active_cw_vertex_index (pen, &in->dev_vector, &start);
	    step = +1;
	    _cairo_pen_find_active_cw_vertex_index (pen, &out->dev_vector, &stop);
	}

	i = start;
	tri[1] = *inpt;
	while (i != stop) {
	    tri[2] = in->point;
	    _translate_point (&tri[2], &pen->vertices[i].point);
	    _cairo_traps_tessellate_triangle (stroker->traps, tri);
	    tri[1] = tri[2];
	    i += step;
	    if (i < 0)
		i = pen->num_vertices - 1;
	    if (i >= pen->num_vertices)
		i = 0;
	}

	tri[2] = *outpt;

	return _cairo_traps_tessellate_triangle (stroker->traps, tri);
    }
    case CAIRO_LINE_JOIN_MITER:
    default: {
	/* dot product of incoming slope vector with outgoing slope vector */
	double	in_dot_out = ((-in->usr_vector.x * out->usr_vector.x)+
			      (-in->usr_vector.y * out->usr_vector.y));
	double	ml = stroker->style->miter_limit;

	/*
	 * Check the miter limit -- lines meeting at an acute angle
	 * can generate long miters, the limit converts them to bevel
	 *
	 * We want to know when the miter is within the miter limit.
	 * That's straightforward to specify:
	 *
	 *	secant (psi / 2) <= ml
	 *
	 * where psi is the angle between in and out
	 *
	 *				secant(psi/2) = 1/sin(psi/2)
	 *	1/sin(psi/2) <= ml
	 *	1 <= ml sin(psi/2)
	 *	1 <= ml² sin²(psi/2)
	 *	2 <= ml² 2 sin²(psi/2)
	 *				2·sin²(psi/2) = 1-cos(psi)
	 *	2 <= ml² (1-cos(psi))
	 *
	 *				in · out = |in| |out| cos (psi)
	 *
	 * in and out are both unit vectors, so:
	 *
	 *				in · out = cos (psi)
	 *
	 *	2 <= ml² (1 - in · out)
	 * 	 
	 */
	if (2 <= ml * ml * (1 - in_dot_out)) {
	    double		x1, y1, x2, y2;
	    double		mx, my;
	    double		dx1, dx2, dy1, dy2;
	    cairo_polygon_t	polygon;
	    cairo_point_t	outer;

	    /* 
	     * we've got the points already transformed to device
	     * space, but need to do some computation with them and
	     * also need to transform the slope from user space to
	     * device space
	     */
	    /* outer point of incoming line face */
	    x1 = _cairo_fixed_to_double (inpt->x);
	    y1 = _cairo_fixed_to_double (inpt->y);
	    dx1 = in->usr_vector.x;
	    dy1 = in->usr_vector.y;
	    cairo_matrix_transform_distance (stroker->ctm, &dx1, &dy1);
	    
	    /* outer point of outgoing line face */
	    x2 = _cairo_fixed_to_double (outpt->x);
	    y2 = _cairo_fixed_to_double (outpt->y);
	    dx2 = out->usr_vector.x;
	    dy2 = out->usr_vector.y;
	    cairo_matrix_transform_distance (stroker->ctm, &dx2, &dy2);
	    
	    /*
	     * Compute the location of the outer corner of the miter.
	     * That's pretty easy -- just the intersection of the two
	     * outer edges.  We've got slopes and points on each
	     * of those edges.  Compute my directly, then compute
	     * mx by using the edge with the larger dy; that avoids
	     * dividing by values close to zero.
	     */
	    my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
		  (dx1 * dy2 - dx2 * dy1));
	    if (fabs (dy1) >= fabs (dy2))
		mx = (my - y1) * dx1 / dy1 + x1;
	    else
		mx = (my - y2) * dx2 / dy2 + x2;
	    
	    /*
	     * Draw the quadrilateral
	     */
	    outer.x = _cairo_fixed_from_double (mx);
	    outer.y = _cairo_fixed_from_double (my);
	    _cairo_polygon_init (&polygon);
	    _cairo_polygon_move_to (&polygon, &in->point);
	    _cairo_polygon_line_to (&polygon, inpt);
	    _cairo_polygon_line_to (&polygon, &outer);
	    _cairo_polygon_line_to (&polygon, outpt);
	    _cairo_polygon_close (&polygon);
	    status = _cairo_traps_tessellate_polygon (stroker->traps,
						      &polygon,
						      CAIRO_FILL_RULE_WINDING);
	    _cairo_polygon_fini (&polygon);

	    return status;
	}
	/* fall through ... */
    }
    case CAIRO_LINE_JOIN_BEVEL: {
	cairo_point_t tri[3];
	tri[0] = in->point;
	tri[1] = *inpt;
	tri[2] = *outpt;

	return _cairo_traps_tessellate_triangle (stroker->traps, tri);
    }
    }
}

static cairo_status_t
_cairo_stroker_add_cap (cairo_stroker_t *stroker, cairo_stroke_face_t *f)
{
    cairo_status_t	    status;

    if (stroker->style->line_cap == CAIRO_LINE_CAP_BUTT)
	return CAIRO_STATUS_SUCCESS;
    
    switch (stroker->style->line_cap) {
    case CAIRO_LINE_CAP_ROUND: {
	int i;
	int start, stop;
	cairo_slope_t slope;
	cairo_point_t tri[3];
	cairo_pen_t *pen = &stroker->pen;

	slope = f->dev_vector;
	_cairo_pen_find_active_cw_vertex_index (pen, &slope, &start);
	slope.dx = -slope.dx;
	slope.dy = -slope.dy;
	_cairo_pen_find_active_cw_vertex_index (pen, &slope, &stop);

	tri[0] = f->point;
	tri[1] = f->cw;
	for (i=start; i != stop; i = (i+1) % pen->num_vertices) {
	    tri[2] = f->point;
	    _translate_point (&tri[2], &pen->vertices[i].point);
	    _cairo_traps_tessellate_triangle (stroker->traps, tri);
	    tri[1] = tri[2];
	}
	tri[2] = f->ccw;

	return _cairo_traps_tessellate_triangle (stroker->traps, tri);
    }
    case CAIRO_LINE_CAP_SQUARE: {
	double dx, dy;
	cairo_slope_t	fvector;
	cairo_point_t	occw, ocw;
	cairo_polygon_t	polygon;

	dx = f->usr_vector.x;
	dy = f->usr_vector.y;
	dx *= stroker->style->line_width / 2.0;
	dy *= stroker->style->line_width / 2.0;
	cairo_matrix_transform_distance (stroker->ctm, &dx, &dy);
	fvector.dx = _cairo_fixed_from_double (dx);
	fvector.dy = _cairo_fixed_from_double (dy);
	occw.x = f->ccw.x + fvector.dx;
	occw.y = f->ccw.y + fvector.dy;
	ocw.x = f->cw.x + fvector.dx;
	ocw.y = f->cw.y + fvector.dy;

	_cairo_polygon_init (&polygon);
	_cairo_polygon_move_to (&polygon, &f->cw);
	_cairo_polygon_line_to (&polygon, &ocw);
	_cairo_polygon_line_to (&polygon, &occw);
	_cairo_polygon_line_to (&polygon, &f->ccw);
	_cairo_polygon_close (&polygon);

	status = _cairo_traps_tessellate_polygon (stroker->traps, &polygon, CAIRO_FILL_RULE_WINDING);
	_cairo_polygon_fini (&polygon);

	return status;
    }
    case CAIRO_LINE_CAP_BUTT:
    default:
	return CAIRO_STATUS_SUCCESS;
    }
}

static cairo_status_t
_cairo_stroker_add_leading_cap (cairo_stroker_t     *stroker,
				cairo_stroke_face_t *face)
{
    cairo_stroke_face_t reversed;
    cairo_point_t t;

    reversed = *face;

    /* The initial cap needs an outward facing vector. Reverse everything */
    reversed.usr_vector.x = -reversed.usr_vector.x;
    reversed.usr_vector.y = -reversed.usr_vector.y;
    reversed.dev_vector.dx = -reversed.dev_vector.dx;
    reversed.dev_vector.dy = -reversed.dev_vector.dy;
    t = reversed.cw;
    reversed.cw = reversed.ccw;
    reversed.ccw = t;

    return _cairo_stroker_add_cap (stroker, &reversed);
}

static cairo_status_t
_cairo_stroker_add_trailing_cap (cairo_stroker_t     *stroker,
				 cairo_stroke_face_t *face)
{
    return _cairo_stroker_add_cap (stroker, face);
}

static cairo_status_t
_cairo_stroker_add_caps (cairo_stroker_t *stroker)
{
    cairo_status_t status;

    if (stroker->has_first_face) {
	status = _cairo_stroker_add_leading_cap (stroker, &stroker->first_face);
	if (status)
	    return status;
    }

    if (stroker->has_current_face) {
	status = _cairo_stroker_add_trailing_cap (stroker, &stroker->current_face);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_compute_face (cairo_point_t *point, cairo_slope_t *slope, cairo_stroker_t *stroker, cairo_stroke_face_t *face)
{
    double mag, det;
    double line_dx, line_dy;
    double face_dx, face_dy;
    cairo_point_double_t usr_vector;
    cairo_point_t offset_ccw, offset_cw;

    line_dx = _cairo_fixed_to_double (slope->dx);
    line_dy = _cairo_fixed_to_double (slope->dy);

    /* faces are normal in user space, not device space */
    cairo_matrix_transform_distance (stroker->ctm_inverse, &line_dx, &line_dy);

    mag = sqrt (line_dx * line_dx + line_dy * line_dy);
    if (mag == 0) {
	/* XXX: Can't compute other face points. Do we want a tag in the face for this case? */
	return;
    }

    /* normalize to unit length */
    line_dx /= mag;
    line_dy /= mag;

    usr_vector.x = line_dx;
    usr_vector.y = line_dy;

    /* 
     * rotate to get a line_width/2 vector along the face, note that
     * the vector must be rotated the right direction in device space,
     * but by 90° in user space. So, the rotation depends on
     * whether the ctm reflects or not, and that can be determined
     * by looking at the determinant of the matrix.
     */
    _cairo_matrix_compute_determinant (stroker->ctm, &det);
    if (det >= 0)
    {
	face_dx = - line_dy * (stroker->style->line_width / 2.0);
	face_dy = line_dx * (stroker->style->line_width / 2.0);
    }
    else
    {
	face_dx = line_dy * (stroker->style->line_width / 2.0);
	face_dy = - line_dx * (stroker->style->line_width / 2.0);
    }

    /* back to device space */
    cairo_matrix_transform_distance (stroker->ctm, &face_dx, &face_dy);

    offset_ccw.x = _cairo_fixed_from_double (face_dx);
    offset_ccw.y = _cairo_fixed_from_double (face_dy);
    offset_cw.x = -offset_ccw.x;
    offset_cw.y = -offset_ccw.y;

    face->ccw = *point;
    _translate_point (&face->ccw, &offset_ccw);

    face->point = *point;

    face->cw = *point;
    _translate_point (&face->cw, &offset_cw);

    face->usr_vector.x = usr_vector.x;
    face->usr_vector.y = usr_vector.y;

    face->dev_vector = *slope;
}

static cairo_status_t
_cairo_stroker_add_sub_edge (cairo_stroker_t *stroker, cairo_point_t *p1, cairo_point_t *p2,
			     cairo_stroke_face_t *start, cairo_stroke_face_t *end)
{
    cairo_status_t status;
    cairo_polygon_t polygon;
    cairo_slope_t slope;

    if (p1->x == p2->x && p1->y == p2->y) {
	/* XXX: Need to rethink how this case should be handled, (both
           here and in _compute_face). The key behavior is that
           degenerate paths should draw as much as possible. */
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_slope_init (&slope, p1, p2);
    _compute_face (p1, &slope, stroker, start);

    /* XXX: This could be optimized slightly by not calling
       _compute_face again but rather  translating the relevant
       fields from start. */
    _compute_face (p2, &slope, stroker, end);

    /* XXX: I should really check the return value of the
       move_to/line_to functions here to catch out of memory
       conditions. But since that would be ugly, I'd prefer to add a
       status flag to the polygon object that I could check only once
       at then end of this sequence, (like we do with cairo_t
       already). */
    _cairo_polygon_init (&polygon);
    _cairo_polygon_move_to (&polygon, &start->cw);
    _cairo_polygon_line_to (&polygon, &start->ccw);
    _cairo_polygon_line_to (&polygon, &end->ccw);
    _cairo_polygon_line_to (&polygon, &end->cw);
    _cairo_polygon_close (&polygon);

    /* XXX: We can't use tessellate_rectangle as the matrix may have
       skewed this into a non-rectangular shape. Perhaps it would be
       worth checking the matrix for skew so that the common case
       could use the faster tessellate_rectangle rather than
       tessellate_polygon? */
    status = _cairo_traps_tessellate_polygon (stroker->traps,
					      &polygon, CAIRO_FILL_RULE_WINDING);

    _cairo_polygon_fini (&polygon);

    return status;
}

static cairo_status_t
_cairo_stroker_move_to (void *closure, cairo_point_t *point)
{
    cairo_status_t status;
    cairo_stroker_t *stroker = closure;

    status = _cairo_stroker_add_caps (stroker);
    if (status)
	return status;

    stroker->first_point = *point;
    stroker->current_point = *point;
    stroker->has_current_point = 1;

    stroker->has_first_face = 0;
    stroker->has_current_face = 0;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_stroker_line_to (void *closure, cairo_point_t *point)
{
    cairo_status_t status;
    cairo_stroker_t *stroker = closure;
    cairo_stroke_face_t start, end;
    cairo_point_t *p1 = &stroker->current_point;
    cairo_point_t *p2 = point;

    if (!stroker->has_current_point)
	return _cairo_stroker_move_to (stroker, point);

    if (p1->x == p2->x && p1->y == p2->y) {
	/* XXX: Need to rethink how this case should be handled, (both
           here and in cairo_stroker_add_sub_edge and in _compute_face). The
           key behavior is that degenerate paths should draw as much
           as possible. */
	return CAIRO_STATUS_SUCCESS;
    }
    
    status = _cairo_stroker_add_sub_edge (stroker, p1, p2, &start, &end);
    if (status)
	return status;

    if (stroker->has_current_face) {
	status = _cairo_stroker_join (stroker, &stroker->current_face, &start);
	if (status)
	    return status;
    } else {
	if (!stroker->has_first_face) {
	    stroker->first_face = start;
	    stroker->has_first_face = 1;
	}
    }
    stroker->current_face = end;
    stroker->has_current_face = 1;

    stroker->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

/*
 * Dashed lines.  Cap each dash end, join around turns when on
 */
static cairo_status_t
_cairo_stroker_line_to_dashed (void *closure, cairo_point_t *point)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_stroker_t *stroker = closure;
    double mag, remain, tmp;
    double dx, dy;
    double dx2, dy2;
    cairo_point_t fd1, fd2;
    int first = 1;
    cairo_stroke_face_t sub_start, sub_end;
    cairo_point_t *p1 = &stroker->current_point;
    cairo_point_t *p2 = point;

    if (!stroker->has_current_point)
	return _cairo_stroker_move_to (stroker, point);
    
    dx = _cairo_fixed_to_double (p2->x - p1->x);
    dy = _cairo_fixed_to_double (p2->y - p1->y);

    cairo_matrix_transform_distance (stroker->ctm_inverse, &dx, &dy);

    mag = sqrt (dx *dx + dy * dy);
    remain = mag;
    fd1 = *p1;
    while (remain) {
	tmp = stroker->dash_remain;
	if (tmp > remain)
	    tmp = remain;
	remain -= tmp;
        dx2 = dx * (mag - remain)/mag;
	dy2 = dy * (mag - remain)/mag;
	cairo_matrix_transform_distance (stroker->ctm, &dx2, &dy2);
	fd2.x = _cairo_fixed_from_double (dx2);
	fd2.y = _cairo_fixed_from_double (dy2);
	fd2.x += p1->x;
	fd2.y += p1->y;
	/*
	 * XXX simplify this case analysis
	 */
	if (stroker->dash_on) {
	    status = _cairo_stroker_add_sub_edge (stroker, &fd1, &fd2, &sub_start, &sub_end);
	    if (status)
		return status;
	    if (!first) {
		/*
		 * Not first dash in this segment, cap start
		 */
		status = _cairo_stroker_add_leading_cap (stroker, &sub_start);
		if (status)
		    return status;
	    } else {
		/*
		 * First in this segment, join to any current_face, else
		 * if at start of sub-path, mark position, else
		 * cap
		 */
		if (stroker->has_current_face) {
		    status = _cairo_stroker_join (stroker, &stroker->current_face, &sub_start);
		    if (status)
			return status;
		} else {
		    if (!stroker->has_first_face) {
			stroker->first_face = sub_start;
			stroker->has_first_face = 1;
		    } else {
			status = _cairo_stroker_add_leading_cap (stroker, &sub_start);
			if (status)
			    return status;
		    }
		}
	    }
	    if (remain) {
		/*
		 * Cap if not at end of segment
		 */
		status = _cairo_stroker_add_trailing_cap (stroker, &sub_end);
		if (status)
		    return status;
	    } else {
		/*
		 * Mark previous line face and fix up next time
		 * through
		 */
		stroker->current_face = sub_end;
		stroker->has_current_face = 1;
	    }
	} else {
	    /*
	     * If starting with off dash, check previous face
	     * and cap if necessary
	     */
	    if (first) {
		if (stroker->has_current_face) {
		    status = _cairo_stroker_add_trailing_cap (stroker, &stroker->current_face);
		    if (status)
			return status;
		}
	    }
	    if (!remain)
		stroker->has_current_face = 0;
	}
	_cairo_stroker_step_dash (stroker, tmp);
	fd1 = fd2;
	first = 0;
    }

    stroker->current_point = *point;

    return status;
}

static cairo_status_t
_cairo_stroker_curve_to (void *closure,
			 cairo_point_t *b,
			 cairo_point_t *c,
			 cairo_point_t *d)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_stroker_t *stroker = closure;
    cairo_spline_t spline;
    cairo_pen_t pen;
    cairo_stroke_face_t start, end;
    cairo_point_t extra_points[4];
    cairo_point_t *a = &stroker->current_point;

    status = _cairo_spline_init (&spline, a, b, c, d);
    if (status == CAIRO_INT_STATUS_DEGENERATE)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_pen_init_copy (&pen, &stroker->pen);
    if (status)
	goto CLEANUP_SPLINE;

    _compute_face (a, &spline.initial_slope, stroker, &start);
    _compute_face (d, &spline.final_slope, stroker, &end);

    if (stroker->has_current_face) {
	status = _cairo_stroker_join (stroker, &stroker->current_face, &start);
	if (status)
	    return status;
    } else {
	if (!stroker->has_first_face) {
	    stroker->first_face = start;
	    stroker->has_first_face = 1;
	}
    }
    stroker->current_face = end;
    stroker->has_current_face = 1;
    
    extra_points[0] = start.cw;
    extra_points[0].x -= start.point.x;
    extra_points[0].y -= start.point.y;
    extra_points[1] = start.ccw;
    extra_points[1].x -= start.point.x;
    extra_points[1].y -= start.point.y;
    extra_points[2] = end.cw;
    extra_points[2].x -= end.point.x;
    extra_points[2].y -= end.point.y;
    extra_points[3] = end.ccw;
    extra_points[3].x -= end.point.x;
    extra_points[3].y -= end.point.y;
    
    status = _cairo_pen_add_points (&pen, extra_points, 4);
    if (status)
	goto CLEANUP_PEN;

    status = _cairo_pen_stroke_spline (&pen, &spline, stroker->tolerance, stroker->traps);
    if (status)
	goto CLEANUP_PEN;

  CLEANUP_PEN:
    _cairo_pen_fini (&pen);
  CLEANUP_SPLINE:
    _cairo_spline_fini (&spline);

    stroker->current_point = *d;

    return status;
}

/* We're using two different algorithms here for dashed and un-dashed
 * splines. The dashed alogorithm uses the existing line dashing
 * code. It's linear in path length, but gets subtly wrong results for
 * self-intersecting paths (an outstanding but for self-intersecting
 * non-curved paths as well). The non-dashed algorithm tessellates a
 * single polygon for the whole curve. It handles the
 * self-intersecting problem, but it's (unsurprisingly) not O(n) and
 * more significantly, it doesn't yet handle dashes.
 *
 * The only reason we're doing split algortihms here is to
 * minimize the impact of fixing the splines-aren't-dashed bug for
 * 1.0.2. Long-term the right answer is to rewrite the whole pile
 * of stroking code so that the entire result is computed as a
 * single polygon that is tessellated, (that is, stroking can be
 * built on top of filling). That will solve the self-intersecting
 * problem. It will also increase the importance of implementing
 * an efficient and more robust tessellator.
 */
static cairo_status_t
_cairo_stroker_curve_to_dashed (void *closure,
				cairo_point_t *b,
				cairo_point_t *c,
				cairo_point_t *d)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_stroker_t *stroker = closure;
    cairo_spline_t spline;
    cairo_point_t *a = &stroker->current_point;
    cairo_line_join_t line_join_save;
    int i;

    status = _cairo_spline_init (&spline, a, b, c, d);
    if (status == CAIRO_INT_STATUS_DEGENERATE)
	return CAIRO_STATUS_SUCCESS;

    /* If the line width is so small that the pen is reduced to a
       single point, then we have nothing to do. */
    if (stroker->pen.num_vertices <= 1)
	goto CLEANUP_SPLINE;

    /* Temporarily modify the stroker to use round joins to guarantee
     * smooth stroked curves. */
    line_join_save = stroker->style->line_join;
    stroker->style->line_join = CAIRO_LINE_JOIN_ROUND;

    status = _cairo_spline_decompose (&spline, stroker->tolerance);
    if (status)
	goto CLEANUP_GSTATE;

    for (i = 1; i < spline.num_points; i++) {
	if (stroker->dashed)
	    status = _cairo_stroker_line_to_dashed (stroker, &spline.points[i]);
	else
	    status = _cairo_stroker_line_to (stroker, &spline.points[i]);
	if (status)
	    break;
    }

  CLEANUP_GSTATE:
    stroker->style->line_join = line_join_save;

  CLEANUP_SPLINE:
    _cairo_spline_fini (&spline);

    return status;
}

static cairo_status_t
_cairo_stroker_close_path (void *closure)
{
    cairo_status_t status;
    cairo_stroker_t *stroker = closure;

    if (stroker->has_current_point) {
	if (stroker->dashed)
	    status = _cairo_stroker_line_to_dashed (stroker, &stroker->first_point);
	else
	    status = _cairo_stroker_line_to (stroker, &stroker->first_point);
	if (status)
	    return status;
    }

    if (stroker->has_first_face && stroker->has_current_face) {
	status = _cairo_stroker_join (stroker, &stroker->current_face, &stroker->first_face);
	if (status)
	    return status;
    }

    stroker->has_first_face = 0;
    stroker->has_current_face = 0;
    stroker->has_current_point = 0;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_stroke_to_traps (cairo_path_fixed_t	*path,
				   cairo_stroke_style_t	*stroke_style,
				   cairo_matrix_t	*ctm,
				   cairo_matrix_t	*ctm_inverse,
				   double		 tolerance,
				   cairo_traps_t	*traps)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_stroker_t stroker;

    _cairo_stroker_init (&stroker, stroke_style,
			 ctm, ctm_inverse, tolerance,
			 traps);

    if (stroker.style->dash)
	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _cairo_stroker_move_to,
					      _cairo_stroker_line_to_dashed,
					      _cairo_stroker_curve_to_dashed,
					      _cairo_stroker_close_path,
					      &stroker);
    else
	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _cairo_stroker_move_to,
					      _cairo_stroker_line_to,
					      _cairo_stroker_curve_to,
					      _cairo_stroker_close_path,
					      &stroker);
    if (status)
	goto BAIL;

    status = _cairo_stroker_add_caps (&stroker);

BAIL:
    _cairo_stroker_fini (&stroker);

    return status;
}
