/*
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "icint.h"

static void
pixman_point_fixed_bounds (int npoint, const pixman_point_fixed_t *points, pixman_box16_t *bounds)
{
    bounds->x1 = xFixedToInt (points->x);
    bounds->x2 = xFixedToInt (xFixedCeil (points->x));
    bounds->y1 = xFixedToInt (points->y);
    bounds->y2 = xFixedToInt (xFixedCeil (points->y));
    points++;
    npoint--;
    while (npoint-- > 0)
    {
	int	x1 = xFixedToInt (points->x);
	int	x2 = xFixedToInt (xFixedCeil (points->x));
	int	y1 = xFixedToInt (points->y);
	int	y2 = xFixedToInt (xFixedCeil (points->y));

	if (x1 < bounds->x1)
	    bounds->x1 = x1;
	else if (x2 > bounds->x2)
	    bounds->x2 = x2;
	if (y1 < bounds->y1)
	    bounds->y1 = y1;
	else if (y2 > bounds->y2)
	    bounds->y2 = y2;
	points++;
    }
}

static void
pixman_triangle_bounds (int ntri, const pixman_triangle_t *tris, pixman_box16_t *bounds)
{
    pixman_point_fixed_bounds (ntri * 3, (pixman_point_fixed_t *) tris, bounds);
}

static void
FbRasterizeTriangle (pixman_image_t		*image,
		     const pixman_triangle_t	*tri,
		     int		x_off,
		     int		y_off)
{
    const pixman_point_fixed_t	*top, *left, *right, *t;
    pixman_trapezoid_t		trap[2];

    top = &tri->p1;
    left = &tri->p2;
    right = &tri->p3;
    if (left->y < top->y) {
	t = left; left = top; top = t;
    }
    if (right->y < top->y) {
	t = right; right = top; top = t;
    }
    /* XXX: This code is broken, left and right must be determined by
       comparing the angles of the two edges, (eg. we can only compare
       X coordinates if we've already intersected each edge with the
       same Y coordinate) */
    if (right->x < left->x) {
	t = right; right = left; left = t;
    }

    /*
     * Two cases:
     *
     *		+		+
     *	       / \             / \
     *	      /   \           /   \
     *	     /     +         +     \
     *      /    --           --    \
     *     /   --               --   \
     *    / ---                   --- \
     *	 +--                         --+
     */
    
    trap[0].top = top->y;
    
    trap[0].left.p1.x = top->x;
    trap[0].left.p1.y = trap[0].top;
    trap[0].left.p2.x = left->x;
    trap[0].left.p2.y = left->y;
    
    trap[0].right.p1 = trap[0].left.p1;
    trap[0].right.p2.x = right->x;
    trap[0].right.p2.y = right->y;
    
    if (right->y < left->y)
    {
	trap[0].bottom = trap[0].right.p2.y;

	trap[1].top = trap[0].bottom;
	trap[1].bottom = trap[0].left.p2.y;
	
	trap[1].left = trap[0].left;
	trap[1].right.p1 = trap[0].right.p2;
	trap[1].right.p2 = trap[0].left.p2;
    }
    else
    {
	trap[0].bottom = trap[0].left.p2.y;
	
	trap[1].top = trap[0].bottom;
	trap[1].bottom = trap[0].right.p2.y;
	
	trap[1].right = trap[0].right;
	trap[1].left.p1 = trap[0].left.p2;
	trap[1].left.p2 = trap[0].right.p2;
    }
    if (trap[0].top != trap[0].bottom)
	fbRasterizeTrapezoid (image, &trap[0], x_off, y_off);
    if (trap[1].top != trap[1].bottom)
	fbRasterizeTrapezoid (image, &trap[1], x_off, y_off);
}

void
pixman_composite_triangles (pixman_operator_t	op,
		      pixman_image_t		*src,
		      pixman_image_t		*dst,
		      int		xSrc,
		      int		ySrc,
		      const pixman_triangle_t	*tris,
		      int		ntris)
{
    pixman_box16_t	bounds;
    pixman_image_t		*image = NULL;
    int		xDst, yDst;
    int		xRel, yRel;
    pixman_format_t	*format;
    
    xDst = tris[0].p1.x >> 16;
    yDst = tris[0].p1.y >> 16;

    format = pixman_format_create (PIXMAN_FORMAT_NAME_A8);
    
    if (format)
    {
	pixman_triangle_bounds (ntris, tris, &bounds);
	if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
	    return;
	image = FbCreateAlphaPicture (dst,
				      format,
				      bounds.x2 - bounds.x1,
				      bounds.y2 - bounds.y1);
	if (!image)
	    return;
    }
    for (; ntris; ntris--, tris++)
    {
	if (!format)
	{
	    pixman_triangle_bounds (1, tris, &bounds);
	    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
		continue;
	    image = FbCreateAlphaPicture (dst,
					  format,
					  bounds.x2 - bounds.x1,
					  bounds.y2 - bounds.y1);
	    if (!image)
		break;
	}
	FbRasterizeTriangle (image, tris, -bounds.x1, -bounds.y1);
	if (!format)
	{
	    xRel = bounds.x1 + xSrc - xDst;
	    yRel = bounds.y1 + ySrc - yDst;
	    pixman_composite (op, src, image, dst,
			 xRel, yRel, 0, 0, bounds.x1, bounds.y1,
			 bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	    pixman_image_destroy (image);
	}
	/* XXX adjust xSrc and ySrc */
    }
    if (format)
    {
	xRel = bounds.x1 + xSrc - xDst;
	yRel = bounds.y1 + ySrc - yDst;
	pixman_composite (op, src, image, dst,
		     xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		     bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	pixman_image_destroy (image);
    }

    pixman_format_destroy (format);
}

void
pixman_composite_tri_strip (pixman_operator_t		op,
		     pixman_image_t		*src,
		     pixman_image_t		*dst,
		     int		xSrc,
		     int		ySrc,
		     const pixman_point_fixed_t	*points,
		     int		npoints)
{
    pixman_triangle_t		tri;
    pixman_box16_t	bounds;
    pixman_image_t		*image = NULL;
    int		xDst, yDst;
    int		xRel, yRel;
    pixman_format_t	*format;
    
    xDst = points[0].x >> 16;
    yDst = points[0].y >> 16;

    format = pixman_format_create (PIXMAN_FORMAT_NAME_A8);
    
    if (npoints < 3)
	return;
    if (format)
    {
	pixman_point_fixed_bounds (npoints, points, &bounds);
	if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
	    return;
	image = FbCreateAlphaPicture (dst,
				      format,
				      bounds.x2 - bounds.x1,
				      bounds.y2 - bounds.y1);
	if (!image)
	    return;
    }
    for (; npoints >= 3; npoints--, points++)
    {
	tri.p1 = points[0];
	tri.p2 = points[1];
	tri.p3 = points[2];
	if (!format)
	{
	    pixman_triangle_bounds (1, &tri, &bounds);
	    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
		continue;
	    image = FbCreateAlphaPicture (dst,
					  format, 
					  bounds.x2 - bounds.x1,
					  bounds.y2 - bounds.y1);
	    if (!image)
		continue;
	}
	FbRasterizeTriangle (image, &tri, -bounds.x1, -bounds.y1);
	if (!format)
	{
	    xRel = bounds.x1 + xSrc - xDst;
	    yRel = bounds.y1 + ySrc - yDst;
	    pixman_composite (op, src, image, dst,
			 xRel, yRel, 0, 0, bounds.x1, bounds.y1,
			 bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	    pixman_image_destroy (image);
	}
    }
    if (format)
    {
	xRel = bounds.x1 + xSrc - xDst;
	yRel = bounds.y1 + ySrc - yDst;
	pixman_composite (op, src, image, dst,
		     xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		     bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	pixman_image_destroy (image);
    }

    pixman_format_destroy (format);
}

void
pixman_composite_tri_fan (pixman_operator_t		op,
		   pixman_image_t		*src,
		   pixman_image_t		*dst,
		   int			xSrc,
		   int			ySrc,
		   const pixman_point_fixed_t	*points,
		   int			npoints)
{
    pixman_triangle_t		tri;
    pixman_box16_t	bounds;
    pixman_image_t		*image = NULL;
    const pixman_point_fixed_t	*first;
    int		xDst, yDst;
    int		xRel, yRel;
    pixman_format_t	*format;
    
    xDst = points[0].x >> 16;
    yDst = points[0].y >> 16;

    format = pixman_format_create (PIXMAN_FORMAT_NAME_A8);
    
    if (npoints < 3)
	return;
    if (format)
    {
	pixman_point_fixed_bounds (npoints, points, &bounds);
	if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
	    return;
	image = FbCreateAlphaPicture (dst,
				      format,
				      bounds.x2 - bounds.x1,
				      bounds.y2 - bounds.y1);
	if (!image)
	    return;
    }
    first = points++;
    npoints--;
    for (; npoints >= 2; npoints--, points++)
    {
	tri.p1 = *first;
	tri.p2 = points[0];
	tri.p3 = points[1];
	if (!format)
	{
	    pixman_triangle_bounds (1, &tri, &bounds);
	    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
		continue;
	    image = FbCreateAlphaPicture (dst,
					  format, 
					  bounds.x2 - bounds.x1,
					  bounds.y2 - bounds.y1);
	    if (!image)
		continue;
	}
	FbRasterizeTriangle (image, &tri, -bounds.x1, -bounds.y1);
	if (!format)
	{
	    xRel = bounds.x1 + xSrc - xDst;
	    yRel = bounds.y1 + ySrc - yDst;
	    pixman_composite (op, src, image, dst,
			 xRel, yRel, 0, 0, bounds.x1, bounds.y1,
			 bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	    pixman_image_destroy (image);
	}
    }
    if (format)
    {
	xRel = bounds.x1 + xSrc - xDst;
	yRel = bounds.y1 + ySrc - yDst;
	pixman_composite (op, src, image, dst,
		     xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		     bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	pixman_image_destroy (image);
    }

    pixman_format_destroy (format);
}

