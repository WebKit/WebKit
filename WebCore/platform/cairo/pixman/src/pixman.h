#ifndef _PIXMAN_H_
#define _PIXMAN_H_


/* pixman.h - a merge of pixregion.h and ic.h */


/* from pixregion.h */


/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* libic.h */

/*
 * Copyright © 1998 Keith Packard
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


#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#elif defined (__OpenBSD__) || defined (_AIX) || defined (__osf__)
# include <inttypes.h>
#elif defined (_MSC_VER)
  typedef __int8 int8_t;
  typedef unsigned __int8 uint8_t;
  typedef __int16 int16_t;
  typedef unsigned __int16 uint16_t;
  typedef __int32 int32_t;
  typedef unsigned __int32 uint32_t;
  typedef __int64 int64_t;
  typedef unsigned __int64 uint64_t;
#else
# include <stdint.h>
#endif

#include "pixman-remap.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* pixregion.h */

typedef struct pixman_region16 pixman_region16_t;

typedef struct pixman_box16 {
    short x1, y1, x2, y2;
} pixman_box16_t;

typedef enum {
    PIXMAN_REGION_STATUS_FAILURE,
    PIXMAN_REGION_STATUS_SUCCESS
} pixman_region_status_t;

/* creation/destruction */

pixman_region16_t *
pixman_region_create (void);

pixman_region16_t *
pixman_region_create_simple (pixman_box16_t *extents);

void
pixman_region_destroy (pixman_region16_t *region);

/* manipulation */

void
pixman_region_translate (pixman_region16_t *region, int x, int y);

pixman_region_status_t
pixman_region_copy (pixman_region16_t *dest, pixman_region16_t *source);

pixman_region_status_t
pixman_region_intersect (pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_region16_t *reg2);

pixman_region_status_t
pixman_region_union (pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_region16_t *reg2);

pixman_region_status_t
pixman_region_union_rect(pixman_region16_t *dest, pixman_region16_t *source,
			 int x, int y, unsigned int width, unsigned int height);

pixman_region_status_t
pixman_region_subtract (pixman_region16_t *regD, pixman_region16_t *regM, pixman_region16_t *regS);

pixman_region_status_t
pixman_region_inverse (pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_box16_t *invRect);

/* XXX: Need to fix this so it doesn't depend on an X data structure
pixman_region16_t *
RectsTopixman_region16_t (int nrects, xRectanglePtr prect, int ctype);
*/

/* querying */

/* XXX: These should proably be combined: pixman_region_get_rects? */
int
pixman_region_num_rects (pixman_region16_t *region);

pixman_box16_t *
pixman_region_rects (pixman_region16_t *region);

/* XXX: Change to an enum */
#define rgnOUT 0
#define rgnIN  1
#define rgnPART 2

int
pixman_region_contains_point (pixman_region16_t *region, int x, int y, pixman_box16_t *box);

int
pixman_region_contains_rectangle (pixman_region16_t *pixman_region16_t, pixman_box16_t *prect);

int
pixman_region_not_empty (pixman_region16_t *region);

pixman_box16_t *
pixman_region_extents (pixman_region16_t *region);

/* mucking around */

/* WARNING: calling pixman_region_append may leave dest as an invalid
   region. Follow-up with pixman_region_validate to fix it up. */
pixman_region_status_t
pixman_region_append (pixman_region16_t *dest, pixman_region16_t *region);

pixman_region_status_t
pixman_region_validate (pixman_region16_t *badreg, int *pOverlap);

/* Unclassified functionality
 * XXX: Do all of these need to be exported?
 */

void
pixman_region_reset (pixman_region16_t *region, pixman_box16_t *pBox);

void
pixman_region_empty (pixman_region16_t *region);


/* ic.h */


/* icformat.c */
typedef enum pixman_operator {
    PIXMAN_OPERATOR_CLEAR,
    PIXMAN_OPERATOR_SRC,
    PIXMAN_OPERATOR_DST,
    PIXMAN_OPERATOR_OVER,
    PIXMAN_OPERATOR_OVER_REVERSE,
    PIXMAN_OPERATOR_IN,
    PIXMAN_OPERATOR_IN_REVERSE,
    PIXMAN_OPERATOR_OUT,
    PIXMAN_OPERATOR_OUT_REVERSE,
    PIXMAN_OPERATOR_ATOP,
    PIXMAN_OPERATOR_ATOP_REVERSE,
    PIXMAN_OPERATOR_XOR,
    PIXMAN_OPERATOR_ADD,
    PIXMAN_OPERATOR_SATURATE
} pixman_operator_t;

typedef enum pixman_format_name {
    PIXMAN_FORMAT_NAME_ARGB32,
    PIXMAN_FORMAT_NAME_RGB24,
    PIXMAN_FORMAT_NAME_A8,
    PIXMAN_FORMAT_NAME_A1
} pixman_format_name_t;

typedef struct pixman_format pixman_format_t;

pixman_format_t *
pixman_format_create (pixman_format_name_t name);

pixman_format_t *
pixman_format_create_masks (int bpp,
			    int alpha_mask,
			    int red_mask,
			    int green_mask,
			    int blue_mask);

void
pixman_format_destroy (pixman_format_t *format);

void
pixman_format_get_masks (pixman_format_t *format,
                         int *bpp,
                         int *alpha_mask,
                         int *red_mask,
                         int *green_mask,
                         int *blue_mask);

/* icimage.c */

typedef struct pixman_image pixman_image_t;

pixman_image_t *
pixman_image_create (pixman_format_t	*format,
		     int	width,
		     int	height);

/*
 * This single define controls the basic size of data manipulated
 * by this software; it must be log2(sizeof (pixman_bits_t) * 8)
 */

/* We use a 32-bit size on all platforms, (even those with native 64
 * bit types). This is consistent with the code currently in the X
 * server, so it goes through much more well-tested code paths, (we
 * saw rendering bugs when we tried IC_SHIFT==6 and uint64_t for
 * pixman_bits_t on 64-bit platofrms). In addition, Keith says that
 * his testing indicates that using 32-bits everywhere is a
 * performance win in any case, (presumably due to 32-bit datapaths
 * between the processor and the video card).
*/
#ifndef IC_SHIFT
#define IC_SHIFT 5
#define FB_SHIFT IC_SHIFT
typedef uint32_t pixman_bits_t;
#endif

pixman_image_t *
pixman_image_create_for_data (pixman_bits_t *data,
			      pixman_format_t *format,
			      int width, int height,
			      int bpp, int stride);

void
pixman_image_destroy (pixman_image_t *image);

int
pixman_image_set_clip_region (pixman_image_t	*image,
			      pixman_region16_t	*region);

typedef int32_t pixman_fixed16_16_t;

typedef struct pixman_point_fixed {
    pixman_fixed16_16_t  x, y;
} pixman_point_fixed_t;

typedef struct pixman_line_fixed {
    pixman_point_fixed_t	p1, p2;
} pixman_line_fixed_t;

/* XXX: It's goofy that pixman_rectangle_t has integers while all the other
   datatypes have fixed-point values. (Though by design,
   pixman_fill_rectangles is designed to fill only whole pixels) */
typedef struct pixman_rectangle {
    short x, y;
    unsigned short width, height;
} pixman_rectangle_t;

typedef struct pixman_triangle {
    pixman_point_fixed_t	p1, p2, p3;
} pixman_triangle_t;

typedef struct pixman_trapezoid {
    pixman_fixed16_16_t  top, bottom;
    pixman_line_fixed_t	left, right;
} pixman_trapezoid_t;

typedef struct pixman_vector {
    pixman_fixed16_16_t    vector[3];
} pixman_vector_t;

typedef struct pixman_transform {
    pixman_fixed16_16_t  matrix[3][3];
} pixman_transform_t;

typedef struct pixman_color {
    unsigned short   red;
    unsigned short   green;
    unsigned short   blue;
    unsigned short   alpha;
} pixman_color_t;

typedef struct _pixman_gradient_stop {
    pixman_fixed16_16_t x;
    pixman_color_t      color;
} pixman_gradient_stop_t;

typedef struct _pixman_circle {
    pixman_fixed16_16_t x;
    pixman_fixed16_16_t y;
    pixman_fixed16_16_t radius;
} pixman_circle_t;

typedef struct pixman_linear_gradient {
    pixman_point_fixed_t p1;
    pixman_point_fixed_t p2;
} pixman_linear_gradient_t;

typedef struct pixman_radial_gradient {
    pixman_circle_t inner;
    pixman_circle_t outer;
} pixman_radial_gradient_t;

typedef enum {
    PIXMAN_FILTER_FAST,
    PIXMAN_FILTER_GOOD,
    PIXMAN_FILTER_BEST,
    PIXMAN_FILTER_NEAREST,
    PIXMAN_FILTER_BILINEAR
} pixman_filter_t;

void
pixman_image_set_component_alpha (pixman_image_t *image,
				  int		 component_alpha);

int
pixman_image_set_transform (pixman_image_t	*image,
			    pixman_transform_t	*transform);

/* Don't blame me, blame XRender */
typedef enum {
    PIXMAN_REPEAT_NONE,
    PIXMAN_REPEAT_NORMAL,
    PIXMAN_REPEAT_PAD,
    PIXMAN_REPEAT_REFLECT
} pixman_repeat_t;

void
pixman_image_set_repeat (pixman_image_t		*image,
			 pixman_repeat_t	repeat);

void
pixman_image_set_filter (pixman_image_t		*image,
			 pixman_filter_t	filter);

int
pixman_image_get_width (pixman_image_t	*image);

int
pixman_image_get_height (pixman_image_t	*image);

int
pixman_image_get_stride (pixman_image_t	*image);

int
pixman_image_get_depth (pixman_image_t	*image);

pixman_format_t *
pixman_image_get_format (pixman_image_t	*image);

pixman_bits_t *
pixman_image_get_data (pixman_image_t	*image);

pixman_image_t *
pixman_image_create_linear_gradient (const pixman_linear_gradient_t *gradient,
				     const pixman_gradient_stop_t   *stops,
				     int			    n_stops);

pixman_image_t *
pixman_image_create_radial_gradient (const pixman_radial_gradient_t *gradient,
				     const pixman_gradient_stop_t   *stops,
				     int			    n_stops);

/* iccolor.c */

void
pixman_color_to_pixel (const pixman_format_t	*format,
		       const pixman_color_t	*color,
		       pixman_bits_t		*pixel);

void
pixman_pixel_to_color (const pixman_format_t	*format,
		       pixman_bits_t		pixel,
		       pixman_color_t		*color);

/* icrect.c */

void
pixman_fill_rectangle (pixman_operator_t	op,
		       pixman_image_t		*dst,
		       const pixman_color_t	*color,
		       int			x,
		       int			y,
		       unsigned int		width,
		       unsigned int		height);

void
pixman_fill_rectangles (pixman_operator_t		op,
			pixman_image_t			*dst,
			const pixman_color_t		*color,
			const pixman_rectangle_t	*rects,
			int				nRects);

/* ictrap.c */

void
pixman_composite_trapezoids (pixman_operator_t		op,
			     pixman_image_t		*src,
			     pixman_image_t		*dst,
			     int			xSrc,
			     int			ySrc,
			     const pixman_trapezoid_t *traps,
			     int			ntrap);

void
pixman_add_trapezoids (pixman_image_t		*dst,
		       int			x_off,
		       int			y_off,
		       const pixman_trapezoid_t	*traps,
		       int			ntraps);

/* ictri.c */

void
pixman_composite_triangles (pixman_operator_t		op,
			    pixman_image_t		*src,
			    pixman_image_t		*dst,
			    int				xSrc,
			    int				ySrc,
			    const pixman_triangle_t	*tris,
			    int				ntris);

void
pixman_composite_tri_strip (pixman_operator_t		op,
			    pixman_image_t		*src,
			    pixman_image_t		*dst,
			    int				xSrc,
			    int				ySrc,
			    const pixman_point_fixed_t	*points,
			    int				npoints);


void
pixman_composite_tri_fan (pixman_operator_t		op,
			  pixman_image_t		*src,
			  pixman_image_t		*dst,
			  int				xSrc,
			  int				ySrc,
			  const pixman_point_fixed_t	*points,
			  int				npoints);

/* ic.c */

void
pixman_composite (pixman_operator_t	op,
		  pixman_image_t	*iSrc,
		  pixman_image_t	*iMask,
		  pixman_image_t	*iDst,
		  int      		xSrc,
		  int      		ySrc,
		  int      		xMask,
		  int      		yMask,
		  int      		xDst,
		  int      		yDst,
		  int			width,
		  int			height);



#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _PIXMAN_H_ */
