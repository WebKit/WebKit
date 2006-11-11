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

#define _GNU_SOURCE
#include <stdlib.h>

#include "cairoint.h"

static void
_cairo_matrix_scalar_multiply (cairo_matrix_t *matrix, double scalar);

static void
_cairo_matrix_compute_adjoint (cairo_matrix_t *matrix);

/**
 * cairo_matrix_init_identity:
 * @matrix: a #cairo_matrix_t
 * 
 * Modifies @matrix to be an identity transformation.
 **/
void
cairo_matrix_init_identity (cairo_matrix_t *matrix)
{
    cairo_matrix_init (matrix,
		       1, 0,
		       0, 1,
		       0, 0);
}
slim_hidden_def(cairo_matrix_init_identity);

/**
 * cairo_matrix_init:
 * @matrix: a cairo_matrix_t
 * @xx: xx component of the affine transformation
 * @yx: yx component of the affine transformation
 * @xy: xy component of the affine transformation
 * @yy: yy component of the affine transformation
 * @x0: X translation component of the affine transformation
 * @y0: Y translation component of the affine transformation
 * 
 * Sets @matrix to be the affine transformation given by
 * @xx, @yx, @xy, @yy, @x0, @y0. The transformation is given
 * by:
 * <programlisting>
 *  x_new = xx * x + xy * y + x0;
 *  y_new = yx * x + yy * y + y0;
 * </programlisting>
 **/
void
cairo_matrix_init (cairo_matrix_t *matrix,
		   double xx, double yx,

		   double xy, double yy,
		   double x0, double y0)
{
    matrix->xx = xx; matrix->yx = yx;
    matrix->xy = xy; matrix->yy = yy;
    matrix->x0 = x0; matrix->y0 = y0;
}
slim_hidden_def(cairo_matrix_init);

/**
 * _cairo_matrix_get_affine:
 * @matrix: a @cairo_matrix_t
 * @xx: location to store xx component of matrix
 * @yx: location to store yx component of matrix
 * @xy: location to store xy component of matrix
 * @yy: location to store yy component of matrix
 * @x0: location to store x0 (X-translation component) of matrix, or %NULL
 * @y0: location to store y0 (Y-translation component) of matrix, or %NULL
 * 
 * Gets the matrix values for the affine tranformation that @matrix represents.
 * See cairo_matrix_init().
 *
 *
 * This function is a leftover from the old public API, but is still
 * mildly useful as an internal means for getting at the matrix
 * members in a positional way. For example, when reassigning to some
 * external matrix type, or when renaming members to more meaningful
 * names (such as a,b,c,d,e,f) for particular manipulations.
 **/
void
_cairo_matrix_get_affine (const cairo_matrix_t *matrix,
			  double *xx, double *yx,
			  double *xy, double *yy,
			  double *x0, double *y0)
{
    *xx  = matrix->xx;
    *yx  = matrix->yx;

    *xy  = matrix->xy;
    *yy  = matrix->yy;

    if (x0)
	*x0 = matrix->x0;
    if (y0)
	*y0 = matrix->y0;
}

/**
 * cairo_matrix_init_translate:
 * @matrix: a cairo_matrix_t
 * @tx: amount to translate in the X direction
 * @ty: amount to translate in the Y direction
 * 
 * Initializes @matrix to a transformation that translates by @tx and
 * @ty in the X and Y dimensions, respectively.
 **/
void
cairo_matrix_init_translate (cairo_matrix_t *matrix,
			     double tx, double ty)
{
    cairo_matrix_init (matrix,
		       1, 0,
		       0, 1,
		       tx, ty);
}
slim_hidden_def(cairo_matrix_init_translate);

/**
 * cairo_matrix_translate:
 * @matrix: a cairo_matrix_t
 * @tx: amount to translate in the X direction
 * @ty: amount to translate in the Y direction
 * 
 * Applies a translation by @tx, @ty to the transformation in
 * @matrix. The effect of the new transformation is to first translate
 * the coordinates by @tx and @ty, then apply the original transformation
 * to the coordinates.
 **/
void
cairo_matrix_translate (cairo_matrix_t *matrix, double tx, double ty)
{
    cairo_matrix_t tmp;

    cairo_matrix_init_translate (&tmp, tx, ty);

    cairo_matrix_multiply (matrix, &tmp, matrix);
}

/**
 * cairo_matrix_init_scale:
 * @matrix: a cairo_matrix_t
 * @sx: scale factor in the X direction
 * @sy: scale factor in the Y direction
 * 
 * Initializes @matrix to a transformation that scales by @sx and @sy
 * in the X and Y dimensions, respectively.
 **/
void
cairo_matrix_init_scale (cairo_matrix_t *matrix,
			 double sx, double sy)
{
    cairo_matrix_init (matrix,
		       sx,  0,
		       0, sy,
		       0, 0);
}
slim_hidden_def(cairo_matrix_init_scale);

/**
 * cairo_matrix_scale:
 * @matrix: a #cairo_matrix_t
 * @sx: scale factor in the X direction
 * @sy: scale factor in the Y direction
 * 
 * Applies scaling by @tx, @ty to the transformation in @matrix. The
 * effect of the new transformation is to first scale the coordinates
 * by @sx and @sy, then apply the original transformation to the coordinates.
 **/
void
cairo_matrix_scale (cairo_matrix_t *matrix, double sx, double sy)
{
    cairo_matrix_t tmp;

    cairo_matrix_init_scale (&tmp, sx, sy);

    cairo_matrix_multiply (matrix, &tmp, matrix);
}
slim_hidden_def(cairo_matrix_scale);

/**
 * cairo_matrix_init_rotate:
 * @matrix: a cairo_matrix_t
 * @radians: angle of rotation, in radians. The direction of rotation
 * is defined such that positive angles rotate in the direction from
 * the positive X axis toward the positive Y axis. With the default
 * axis orientation of cairo, positive angles rotate in a clockwise
 * direction.
 * 
 * Initialized @matrix to a transformation that rotates by @radians.
 **/
void
cairo_matrix_init_rotate (cairo_matrix_t *matrix,
			  double radians)
{
    double  s;
    double  c;

    s = sin (radians);
    c = cos (radians);

    cairo_matrix_init (matrix,
		       c, s,
		       -s, c,
		       0, 0);
}
slim_hidden_def(cairo_matrix_init_rotate);

/**
 * cairo_matrix_rotate:
 * @matrix: a @cairo_matrix_t
 * @radians: angle of rotation, in radians. The direction of rotation
 * is defined such that positive angles rotate in the direction from
 * the positive X axis toward the positive Y axis. With the default
 * axis orientation of cairo, positive angles rotate in a clockwise
 * direction.
 * 
 * Applies rotation by @radians to the transformation in
 * @matrix. The effect of the new transformation is to first rotate the
 * coordinates by @radians, then apply the original transformation
 * to the coordinates.
 **/
void
cairo_matrix_rotate (cairo_matrix_t *matrix, double radians)
{
    cairo_matrix_t tmp;

    cairo_matrix_init_rotate (&tmp, radians);

    cairo_matrix_multiply (matrix, &tmp, matrix);
}

/**
 * cairo_matrix_multiply:
 * @result: a @cairo_matrix_t in which to store the result
 * @a: a @cairo_matrix_t
 * @b: a @cairo_matrix_t
 * 
 * Multiplies the affine transformations in @a and @b together
 * and stores the result in @result. The effect of the resulting
 * transformation is to first apply the transformation in @a to the
 * coordinates and then apply the transformation in @b to the
 * coordinates.
 *
 * It is allowable for @result to be identical to either @a or @b.
 **/
/*
 * XXX: The ordering of the arguments to this function corresponds
 *      to [row_vector]*A*B. If we want to use column vectors instead,
 *      then we need to switch the two arguments and fix up all
 *      uses.
 */
void
cairo_matrix_multiply (cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b)
{
    cairo_matrix_t r;

    r.xx = a->xx * b->xx + a->yx * b->xy;
    r.yx = a->xx * b->yx + a->yx * b->yy;

    r.xy = a->xy * b->xx + a->yy * b->xy;
    r.yy = a->xy * b->yx + a->yy * b->yy;

    r.x0 = a->x0 * b->xx + a->y0 * b->xy + b->x0;
    r.y0 = a->x0 * b->yx + a->y0 * b->yy + b->y0;

    *result = r;
}
slim_hidden_def(cairo_matrix_multiply);

/**
 * cairo_matrix_transform_distance:
 * @matrix: a @cairo_matrix_t
 * @dx: X component of a distance vector. An in/out parameter
 * @dy: Y component of a distance vector. An in/out parameter
 * 
 * Transforms the distance vector (@dx,@dy) by @matrix. This is
 * similar to cairo_matrix_transform() except that the translation
 * components of the transformation are ignored. The calculation of
 * the returned vector is as follows:
 *
 * <programlisting>
 * dx2 = dx1 * a + dy1 * c;
 * dy2 = dx1 * b + dy1 * d;
 * </programlisting>
 *
 * Affine transformations are position invariant, so the same vector
 * always transforms to the same vector. If (@x1,@y1) transforms
 * to (@x2,@y2) then (@x1+@dx1,@y1+@dy1) will transform to
 * (@x1+@dx2,@y1+@dy2) for all values of @x1 and @x2.
 **/
void
cairo_matrix_transform_distance (const cairo_matrix_t *matrix, double *dx, double *dy)
{
    double new_x, new_y;

    new_x = (matrix->xx * *dx + matrix->xy * *dy);
    new_y = (matrix->yx * *dx + matrix->yy * *dy);

    *dx = new_x;
    *dy = new_y;
}
slim_hidden_def(cairo_matrix_transform_distance);

/**
 * cairo_matrix_transform_point:
 * @matrix: a @cairo_matrix_t
 * @x: X position. An in/out parameter
 * @y: Y position. An in/out parameter
 * 
 * Transforms the point (@x, @y) by @matrix.
 **/
void
cairo_matrix_transform_point (const cairo_matrix_t *matrix, double *x, double *y)
{
    cairo_matrix_transform_distance (matrix, x, y);

    *x += matrix->x0;
    *y += matrix->y0;
}
slim_hidden_def(cairo_matrix_transform_point);

void
_cairo_matrix_transform_bounding_box (const cairo_matrix_t *matrix,
				      double *x, double *y,
				      double *width, double *height)
{
    int i;
    double quad_x[4], quad_y[4];
    double dx1, dy1;
    double dx2, dy2;
    double min_x, max_x;
    double min_y, max_y;

    quad_x[0] = *x;
    quad_y[0] = *y;
    cairo_matrix_transform_point (matrix, &quad_x[0], &quad_y[0]);

    dx1 = *width;
    dy1 = 0;
    cairo_matrix_transform_distance (matrix, &dx1, &dy1);
    quad_x[1] = quad_x[0] + dx1;
    quad_y[1] = quad_y[0] + dy1;

    dx2 = 0;
    dy2 = *height;
    cairo_matrix_transform_distance (matrix, &dx2, &dy2);
    quad_x[2] = quad_x[0] + dx2;
    quad_y[2] = quad_y[0] + dy2;

    quad_x[3] = quad_x[0] + dx1 + dx2;
    quad_y[3] = quad_y[0] + dy1 + dy2;

    min_x = max_x = quad_x[0];
    min_y = max_y = quad_y[0];

    for (i=1; i < 4; i++) {
	if (quad_x[i] < min_x)
	    min_x = quad_x[i];
	if (quad_x[i] > max_x)
	    max_x = quad_x[i];

	if (quad_y[i] < min_y)
	    min_y = quad_y[i];
	if (quad_y[i] > max_y)
	    max_y = quad_y[i];
    }

    *x = min_x;
    *y = min_y;
    *width = max_x - min_x;
    *height = max_y - min_y;
}

static void
_cairo_matrix_scalar_multiply (cairo_matrix_t *matrix, double scalar)
{
    matrix->xx *= scalar;
    matrix->yx *= scalar;

    matrix->xy *= scalar;
    matrix->yy *= scalar;

    matrix->x0 *= scalar;
    matrix->y0 *= scalar;
}

/* This function isn't a correct adjoint in that the implicit 1 in the
   homogeneous result should actually be ad-bc instead. But, since this
   adjoint is only used in the computation of the inverse, which
   divides by det (A)=ad-bc anyway, everything works out in the end. */
static void
_cairo_matrix_compute_adjoint (cairo_matrix_t *matrix)
{
    /* adj (A) = transpose (C:cofactor (A,i,j)) */
    double a, b, c, d, tx, ty;

    _cairo_matrix_get_affine (matrix,
			      &a,  &b,
			      &c,  &d,
			      &tx, &ty);

    cairo_matrix_init (matrix,
		       d, -b,
		       -c, a,
		       c*ty - d*tx, b*tx - a*ty);
}

/**
 * cairo_matrix_invert:
 * @matrix: a @cairo_matrix_t
 * 
 * Changes @matrix to be the inverse of it's original value. Not
 * all transformation matrices have inverses; if the matrix
 * collapses points together (it is <firstterm>degenerate</firstterm>),
 * then it has no inverse and this function will fail.
 * 
 * Returns: If @matrix has an inverse, modifies @matrix to
 *  be the inverse matrix and returns %CAIRO_STATUS_SUCCESS. Otherwise,
 *  returns %CAIRO_STATUS_INVALID_MATRIX.
 **/
cairo_status_t
cairo_matrix_invert (cairo_matrix_t *matrix)
{
    /* inv (A) = 1/det (A) * adj (A) */
    double det;

    _cairo_matrix_compute_determinant (matrix, &det);
    
    if (det == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_matrix_compute_adjoint (matrix);
    _cairo_matrix_scalar_multiply (matrix, 1 / det);

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_invert);

void
_cairo_matrix_compute_determinant (const cairo_matrix_t *matrix,
				   double		*det)
{
    double a, b, c, d;

    a = matrix->xx; b = matrix->yx;
    c = matrix->xy; d = matrix->yy;

    *det = a*d - b*c;
}

/* Compute the amount that each basis vector is scaled by. */
cairo_status_t
_cairo_matrix_compute_scale_factors (const cairo_matrix_t *matrix,
				     double *sx, double *sy, int x_major)
{
    double det;

    _cairo_matrix_compute_determinant (matrix, &det);

    if (det == 0)
    {
	*sx = *sy = 0;
    }
    else
    {
	double x = x_major != 0;
	double y = x == 0;
	double major, minor;
	
	cairo_matrix_transform_distance (matrix, &x, &y);
	major = sqrt(x*x + y*y);
	/*
	 * ignore mirroring
	 */
	if (det < 0)
	    det = -det;
	if (major)
	    minor = det / major;
	else 
	    minor = 0.0;
	if (x_major)
	{
	    *sx = major;
	    *sy = minor;
	}
	else
	{
	    *sx = minor;
	    *sy = major;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t 
_cairo_matrix_is_integer_translation(const cairo_matrix_t *m,
				     int *itx, int *ity)
{
    cairo_bool_t is_integer_translation;
    cairo_fixed_t x0_fixed, y0_fixed;

    x0_fixed = _cairo_fixed_from_double (m->x0);
    y0_fixed = _cairo_fixed_from_double (m->y0);

    is_integer_translation = ((m->xx == 1.0) &&
			      (m->yx == 0.0) &&
			      (m->xy == 0.0) &&
			      (m->yy == 1.0) &&
			      (_cairo_fixed_is_integer(x0_fixed)) &&
			      (_cairo_fixed_is_integer(y0_fixed)));

    if (! is_integer_translation)
	return FALSE;

    if (itx)
	*itx = _cairo_fixed_integer_part(x0_fixed);
    if (ity)
	*ity = _cairo_fixed_integer_part(y0_fixed);

    return TRUE;
}

/*
  A circle in user space is transformed into an ellipse in device space.

  The following is a derivation of a formula to calculate the length of the
  major axis for this ellipse; this is useful for error bounds calculations.
  
  Thanks to Walter Brisken <wbrisken@aoc.nrao.edu> for this derivation:
  
  1.  First some notation:
  
  All capital letters represent vectors in two dimensions.  A prime ' 
  represents a transformed coordinate.  Matrices are written in underlined
  form, ie _R_.  Lowercase letters represent scalar real values.
  
  2.  The question has been posed:  What is the maximum expansion factor 
  achieved by the linear transformation
  
  X' = X _R_
  
  where _R_ is a real-valued 2x2 matrix with entries:
  
  _R_ = [a b]
        [c d]  .
  
  In other words, what is the maximum radius, MAX[ |X'| ], reached for any 
  X on the unit circle ( |X| = 1 ) ?
  
  
  3.  Some useful formulae
  
  (A) through (C) below are standard double-angle formulae.  (D) is a lesser
  known result and is derived below:
  
  (A)  sin²(θ) = (1 - cos(2*θ))/2
  (B)  cos²(θ) = (1 + cos(2*θ))/2
  (C)  sin(θ)*cos(θ) = sin(2*θ)/2
  (D)  MAX[a*cos(θ) + b*sin(θ)] = sqrt(a² + b²)
  
  Proof of (D):
  
  find the maximum of the function by setting the derivative to zero:
  
       -a*sin(θ)+b*cos(θ) = 0
  
  From this it follows that 
  
       tan(θ) = b/a 
  
  and hence 
  
       sin(θ) = b/sqrt(a² + b²)
  
  and 
  
       cos(θ) = a/sqrt(a² + b²)
  
  Thus the maximum value is
  
       MAX[a*cos(θ) + b*sin(θ)] = (a² + b²)/sqrt(a² + b²)
                                   = sqrt(a² + b²)
  
  
  4.  Derivation of maximum expansion
  
  To find MAX[ |X'| ] we search brute force method using calculus.  The unit
  circle on which X is constrained is to be parameterized by t:
  
       X(θ) = (cos(θ), sin(θ))
  
  Thus 
  
       X'(θ) = X(θ) * _R_ = (cos(θ), sin(θ)) * [a b]
                                               [c d]
             = (a*cos(θ) + c*sin(θ), b*cos(θ) + d*sin(θ)).
  
  Define 
  
       r(θ) = |X'(θ)|
  
  Thus
  
       r²(θ) = (a*cos(θ) + c*sin(θ))² + (b*cos(θ) + d*sin(θ))²
             = (a² + b²)*cos²(θ) + (c² + d²)*sin²(θ) 
                 + 2*(a*c + b*d)*cos(θ)*sin(θ) 
  
  Now apply the double angle formulae (A) to (C) from above:
  
       r²(θ) = (a² + b² + c² + d²)/2 
	     + (a² + b² - c² - d²)*cos(2*θ)/2
  	     + (a*c + b*d)*sin(2*θ)
             = f + g*cos(φ) + h*sin(φ)
  
  Where
  
       f = (a² + b² + c² + d²)/2
       g = (a² + b² - c² - d²)/2
       h = (a*c + d*d)
       φ = 2*θ
  
  It is clear that MAX[ |X'| ] = sqrt(MAX[ r² ]).  Here we determine MAX[ r² ]
  using (D) from above:
  
       MAX[ r² ] = f + sqrt(g² + h²)
  
  And finally

       MAX[ |X'| ] = sqrt( f + sqrt(g² + h²) )

  Which is the solution to this problem.


  Walter Brisken
  2004/10/08

  (Note that the minor axis length is at the minimum of the above solution,
  which is just sqrt ( f - sqrt(g² + h²) ) given the symmetry of (D)).
*/

/* determine the length of the major axis of a circle of the given radius
   after applying the transformation matrix. */
double
_cairo_matrix_transformed_circle_major_axis (cairo_matrix_t *matrix, double radius)
{
    double  a, b, c, d, f, g, h, i, j;

    _cairo_matrix_get_affine (matrix,
                              &a, &b,
                              &c, &d,
                              NULL, NULL);

    i = a*a + b*b;
    j = c*c + d*d;

    f = 0.5 * (i + j);
    g = 0.5 * (i - j);
    h = a*c + b*d;

    return radius * sqrt (f + sqrt (g*g+h*h));

    /*
     * we don't need the minor axis length, which is
     * double min = radius * sqrt (f - sqrt (g*g+h*h));
     */
}

void
_cairo_matrix_to_pixman_matrix (const cairo_matrix_t	*matrix,
				pixman_transform_t	*pixman_transform)
{
    pixman_transform->matrix[0][0] = _cairo_fixed_from_double (matrix->xx);
    pixman_transform->matrix[0][1] = _cairo_fixed_from_double (matrix->xy);
    pixman_transform->matrix[0][2] = _cairo_fixed_from_double (matrix->x0);

    pixman_transform->matrix[1][0] = _cairo_fixed_from_double (matrix->yx);
    pixman_transform->matrix[1][1] = _cairo_fixed_from_double (matrix->yy);
    pixman_transform->matrix[1][2] = _cairo_fixed_from_double (matrix->y0);

    pixman_transform->matrix[2][0] = 0;
    pixman_transform->matrix[2][1] = 0;
    pixman_transform->matrix[2][2] = _cairo_fixed_from_double (1);
}
