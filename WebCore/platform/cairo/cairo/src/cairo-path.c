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

#include "cairo-path-fixed-private.h"

/* private functions */
static cairo_status_t
_cairo_path_fixed_add (cairo_path_fixed_t *path,
		       cairo_path_op_t 	   op,
		       cairo_point_t	  *points,
		       int		   num_points);

static void
_cairo_path_fixed_add_op_buf (cairo_path_fixed_t  *path,
			      cairo_path_op_buf_t *op_buf);

static void
_cairo_path_fixed_add_arg_buf (cairo_path_fixed_t   *path,
			       cairo_path_arg_buf_t *arg_buf);

static cairo_path_op_buf_t *
_cairo_path_op_buf_create (void);

static void
_cairo_path_op_buf_destroy (cairo_path_op_buf_t *op_buf);

static void
_cairo_path_op_buf_add_op (cairo_path_op_buf_t *op_buf,
			   cairo_path_op_t      op);

static cairo_path_arg_buf_t *
_cairo_path_arg_buf_create (void);

static void
_cairo_path_arg_buf_destroy (cairo_path_arg_buf_t *arg_buf);

static void
_cairo_path_arg_buf_add_points (cairo_path_arg_buf_t *arg_buf,
				cairo_point_t	     *points,
				int		      num_points);

void
_cairo_path_fixed_init (cairo_path_fixed_t *path)
{
    path->op_buf_head = NULL;
    path->op_buf_tail = NULL;

    path->arg_buf_head = NULL;
    path->arg_buf_tail = NULL;

    path->current_point.x = 0;
    path->current_point.y = 0;
    path->has_current_point = 0;
    path->last_move_point = path->current_point;
}

cairo_status_t
_cairo_path_fixed_init_copy (cairo_path_fixed_t *path,
			     cairo_path_fixed_t *other)
{
    cairo_path_op_buf_t *op_buf, *other_op_buf;
    cairo_path_arg_buf_t *arg_buf, *other_arg_buf;

    _cairo_path_fixed_init (path);
    path->current_point = other->current_point;
    path->has_current_point = other->has_current_point;
    path->last_move_point = other->last_move_point;

    for (other_op_buf = other->op_buf_head;
	 other_op_buf;
	 other_op_buf = other_op_buf->next)
    {
	op_buf = _cairo_path_op_buf_create ();
	if (op_buf == NULL) {
	    _cairo_path_fixed_fini (path);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	memcpy (op_buf, other_op_buf, sizeof (cairo_path_op_buf_t));
	_cairo_path_fixed_add_op_buf (path, op_buf);
    }

    for (other_arg_buf = other->arg_buf_head;
	 other_arg_buf;
	 other_arg_buf = other_arg_buf->next)
    {
	arg_buf = _cairo_path_arg_buf_create ();
	if (arg_buf == NULL) {
	    _cairo_path_fixed_fini (path);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	memcpy (arg_buf, other_arg_buf, sizeof (cairo_path_arg_buf_t));
	_cairo_path_fixed_add_arg_buf (path, arg_buf);
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_path_fixed_t *
_cairo_path_fixed_create (void)
{
    cairo_path_fixed_t	*path = malloc (sizeof (cairo_path_fixed_t));

    if (!path)
	return NULL;
    _cairo_path_fixed_init (path);
    return path;
}

void
_cairo_path_fixed_fini (cairo_path_fixed_t *path)
{
    cairo_path_op_buf_t *op_buf;
    cairo_path_arg_buf_t *arg_buf;

    while (path->op_buf_head) {
	op_buf = path->op_buf_head;
	path->op_buf_head = op_buf->next;
	_cairo_path_op_buf_destroy (op_buf);
    }
    path->op_buf_tail = NULL;

    while (path->arg_buf_head) {
	arg_buf = path->arg_buf_head;
	path->arg_buf_head = arg_buf->next;
	_cairo_path_arg_buf_destroy (arg_buf);
    }
    path->arg_buf_tail = NULL;

    path->has_current_point = 0;
}

void
_cairo_path_fixed_destroy (cairo_path_fixed_t *path)
{
    _cairo_path_fixed_fini (path);
    free (path);
}

cairo_status_t
_cairo_path_fixed_move_to (cairo_path_fixed_t  *path,
			   cairo_fixed_t	x,
			   cairo_fixed_t	y)
{
    cairo_status_t status;
    cairo_point_t point;

    point.x = x;
    point.y = y;

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_MOVE_TO, &point, 1);
    if (status)
	return status;

    path->current_point = point;
    path->has_current_point = 1;
    path->last_move_point = path->current_point;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_move_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy)
{
    cairo_fixed_t x, y;

    if (!path->has_current_point)
	return CAIRO_STATUS_NO_CURRENT_POINT;

    x = path->current_point.x + dx;
    y = path->current_point.y + dy;

    return _cairo_path_fixed_move_to (path, x, y);
}

cairo_status_t
_cairo_path_fixed_line_to (cairo_path_fixed_t *path,
			   cairo_fixed_t	x,
			   cairo_fixed_t	y)
{
    cairo_status_t status;
    cairo_point_t point;

    point.x = x;
    point.y = y;

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_LINE_TO, &point, 1);
    if (status)
	return status;

    path->current_point = point;
    path->has_current_point = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_line_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy)
{
    cairo_fixed_t x, y;

    if (!path->has_current_point)
	return CAIRO_STATUS_NO_CURRENT_POINT;

    x = path->current_point.x + dx;
    y = path->current_point.y + dy;

    return _cairo_path_fixed_line_to (path, x, y);
}

cairo_status_t
_cairo_path_fixed_curve_to (cairo_path_fixed_t	*path,
			    cairo_fixed_t x0, cairo_fixed_t y0,
			    cairo_fixed_t x1, cairo_fixed_t y1,
			    cairo_fixed_t x2, cairo_fixed_t y2)
{
    cairo_status_t status;
    cairo_point_t point[3];

    point[0].x = x0; point[0].y = y0;
    point[1].x = x1; point[1].y = y1;
    point[2].x = x2; point[2].y = y2;

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CURVE_TO, point, 3);
    if (status)
	return status;

    path->current_point = point[2];
    path->has_current_point = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_curve_to (cairo_path_fixed_t *path,
				cairo_fixed_t dx0, cairo_fixed_t dy0,
				cairo_fixed_t dx1, cairo_fixed_t dy1,
				cairo_fixed_t dx2, cairo_fixed_t dy2)
{
    cairo_fixed_t x0, y0;
    cairo_fixed_t x1, y1;
    cairo_fixed_t x2, y2;

    if (!path->has_current_point)
	return CAIRO_STATUS_NO_CURRENT_POINT;

    x0 = path->current_point.x + dx0;
    y0 = path->current_point.y + dy0;

    x1 = path->current_point.x + dx1;
    y1 = path->current_point.y + dy1;

    x2 = path->current_point.x + dx2;
    y2 = path->current_point.y + dy2;

    return _cairo_path_fixed_curve_to (path,
				       x0, y0,
				       x1, y1,
				       x2, y2);
}

cairo_status_t
_cairo_path_fixed_close_path (cairo_path_fixed_t *path)
{
    cairo_status_t status;

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CLOSE_PATH, NULL, 0);
    if (status)
	return status;

    path->current_point.x = path->last_move_point.x;
    path->current_point.y = path->last_move_point.y;
    path->has_current_point = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_get_current_point (cairo_path_fixed_t *path,
				     cairo_fixed_t	*x,
				     cairo_fixed_t	*y)
{
    if (! path->has_current_point)
	return CAIRO_STATUS_NO_CURRENT_POINT;

    *x = path->current_point.x;
    *y = path->current_point.y;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_fixed_add (cairo_path_fixed_t *path,
		       cairo_path_op_t	   op,
		       cairo_point_t	  *points,
		       int		   num_points)
{
    if (path->op_buf_tail == NULL ||
	path->op_buf_tail->num_ops + 1 > CAIRO_PATH_BUF_SIZE)
    {
	cairo_path_op_buf_t *op_buf;

	op_buf = _cairo_path_op_buf_create ();
	if (op_buf == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	_cairo_path_fixed_add_op_buf (path, op_buf);
    }

    _cairo_path_op_buf_add_op (path->op_buf_tail, op);

    if (path->arg_buf_tail == NULL ||
	path->arg_buf_tail->num_points + num_points > CAIRO_PATH_BUF_SIZE)
    {
	cairo_path_arg_buf_t *arg_buf;

	arg_buf = _cairo_path_arg_buf_create ();

	if (arg_buf == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	_cairo_path_fixed_add_arg_buf (path, arg_buf);
    }

    _cairo_path_arg_buf_add_points (path->arg_buf_tail, points, num_points);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_fixed_add_op_buf (cairo_path_fixed_t  *path,
			      cairo_path_op_buf_t *op_buf)
{
    op_buf->next = NULL;
    op_buf->prev = path->op_buf_tail;

    if (path->op_buf_tail) {
	path->op_buf_tail->next = op_buf;
    } else {
	path->op_buf_head = op_buf;
    }

    path->op_buf_tail = op_buf;
}

static void
_cairo_path_fixed_add_arg_buf (cairo_path_fixed_t   *path,
			       cairo_path_arg_buf_t *arg_buf)
{
    arg_buf->next = NULL;
    arg_buf->prev = path->arg_buf_tail;

    if (path->arg_buf_tail) {
	path->arg_buf_tail->next = arg_buf;
    } else {
	path->arg_buf_head = arg_buf;
    }

    path->arg_buf_tail = arg_buf;
}

static cairo_path_op_buf_t *
_cairo_path_op_buf_create (void)
{
    cairo_path_op_buf_t *op_buf;

    op_buf = malloc (sizeof (cairo_path_op_buf_t));

    if (op_buf) {
	op_buf->num_ops = 0;
	op_buf->next = NULL;
    }

    return op_buf;
}

static void
_cairo_path_op_buf_destroy (cairo_path_op_buf_t *op_buf)
{
    free (op_buf);
}

static void
_cairo_path_op_buf_add_op (cairo_path_op_buf_t *op_buf,
			   cairo_path_op_t	op)
{
    op_buf->op[op_buf->num_ops++] = op;
}

static cairo_path_arg_buf_t *
_cairo_path_arg_buf_create (void)
{
    cairo_path_arg_buf_t *arg_buf;

    arg_buf = malloc (sizeof (cairo_path_arg_buf_t));

    if (arg_buf) {
	arg_buf->num_points = 0;
	arg_buf->next = NULL;
    }

    return arg_buf;
}

static void
_cairo_path_arg_buf_destroy (cairo_path_arg_buf_t *arg_buf)
{
    free (arg_buf);
}

static void
_cairo_path_arg_buf_add_points (cairo_path_arg_buf_t *arg_buf,
				cairo_point_t	     *points,
				int		      num_points)
{
    int i;

    for (i=0; i < num_points; i++) {
	arg_buf->points[arg_buf->num_points++] = points[i];
    }
}

#define CAIRO_PATH_OP_MAX_ARGS 3

static int const num_args[] = 
{
    1, /* cairo_path_move_to */
    1, /* cairo_path_op_line_to */
    3, /* cairo_path_op_curve_to */
    0, /* cairo_path_op_close_path */
};

cairo_status_t
_cairo_path_fixed_interpret (cairo_path_fixed_t			*path,
			     cairo_direction_t			 dir,
			     cairo_path_fixed_move_to_func_t	*move_to,
			     cairo_path_fixed_line_to_func_t	*line_to,
			     cairo_path_fixed_curve_to_func_t	*curve_to,
			     cairo_path_fixed_close_path_func_t	*close_path,
			     void				*closure)
{
    cairo_status_t status;
    int i, arg;
    cairo_path_op_buf_t *op_buf;
    cairo_path_op_t op;
    cairo_path_arg_buf_t *arg_buf = path->arg_buf_head;
    int buf_i = 0;
    cairo_point_t point[CAIRO_PATH_OP_MAX_ARGS];
    cairo_bool_t forward = (dir == CAIRO_DIRECTION_FORWARD);
    int step = forward ? 1 : -1;

    for (op_buf = forward ? path->op_buf_head : path->op_buf_tail;
	 op_buf;
	 op_buf = forward ? op_buf->next : op_buf->prev)
    {
	int start, stop;
	if (forward) {
	    start = 0;
	    stop = op_buf->num_ops;
	} else {
	    start = op_buf->num_ops - 1;
	    stop = -1;
	}

	for (i=start; i != stop; i += step) {
	    op = op_buf->op[i];

	    if (! forward) {
		if (buf_i == 0) {
		    arg_buf = arg_buf->prev;
		    buf_i = arg_buf->num_points;
		}
		buf_i -= num_args[op];
	    }

	    for (arg = 0; arg < num_args[op]; arg++) {
		point[arg] = arg_buf->points[buf_i];
		buf_i++;
		if (buf_i >= arg_buf->num_points) {
		    arg_buf = arg_buf->next;
		    buf_i = 0;
		}
	    }

	    if (! forward) {
		buf_i -= num_args[op];
	    }

	    switch (op) {
	    case CAIRO_PATH_OP_MOVE_TO:
		status = (*move_to) (closure, &point[0]);
		break;
	    case CAIRO_PATH_OP_LINE_TO:
		status = (*line_to) (closure, &point[0]);
		break;
	    case CAIRO_PATH_OP_CURVE_TO:
		status = (*curve_to) (closure, &point[0], &point[1], &point[2]);
		break;
	    case CAIRO_PATH_OP_CLOSE_PATH:
	    default:
		status = (*close_path) (closure);
		break;
	    }
	    if (status)
		return status;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_path_fixed_offset_and_scale (cairo_path_fixed_t *path,
				     cairo_fixed_t offx,
				     cairo_fixed_t offy,
				     cairo_fixed_t scalex,
				     cairo_fixed_t scaley)
{
    cairo_path_arg_buf_t *arg_buf = path->arg_buf_head;
    int i;
    cairo_int64_t i64temp;
    cairo_fixed_t fixedtemp;

    while (arg_buf) {
	 for (i = 0; i < arg_buf->num_points; i++) {
	     /* CAIRO_FIXED_ONE? */
	     if (scalex == 0x00010000) {
		 arg_buf->points[i].x += offx;
	     } else {
		 fixedtemp = arg_buf->points[i].x + offx;
		 i64temp = _cairo_int32x32_64_mul (fixedtemp, scalex);
		 arg_buf->points[i].x = _cairo_int64_to_int32(_cairo_int64_rsl (i64temp, 16));
	     }

	     if (scaley == 0x00010000) {
		 arg_buf->points[i].y += offy;
	     } else {
		 fixedtemp = arg_buf->points[i].y + offy;
		 i64temp = _cairo_int32x32_64_mul (fixedtemp, scaley);
		 arg_buf->points[i].y = _cairo_int64_to_int32(_cairo_int64_rsl (i64temp, 16));
	     }
	 }

	 arg_buf = arg_buf->next;
    }
}

