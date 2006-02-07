/*
 * Copyright © 2000 Keith Packard
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

typedef void	(*FillFunc) (pixman_image_t *dst,
			     int16_t	     xDst,
			     int16_t	     yDst,
			     uint16_t	     width,
			     uint16_t	     height,
			     pixman_bits_t  *pixel);


static void
pixman_fill_rect_1bpp (pixman_image_t *dst,
		       int16_t	       xDst,
		       int16_t	       yDst,
		       uint16_t	       width,
		       uint16_t	       height,
		       pixman_bits_t  *pixel)
{
    uint32_t value = *pixel ? 0xffffffff : 0;
    char *line;

    line = (char *)dst->pixels->data
	   + yDst * dst->pixels->stride;

    if ((width + xDst - 1) / 32 == xDst / 32) {
        uint32_t mask = 0;
        int pos = xDst / 32;
        int i;

        for (i = xDst; i < width; i++)
#if BITMAP_BIT_ORDER == MSBFirst
            mask |= 1 << (0x1f - i);
#else
            mask |= 1 << i;
#endif

        while (height-- > 0) {
            uint32_t *cur = (uint32_t *) line;
            cur [pos] = (cur [pos] & ~mask) | (value & mask);
	    line += dst->pixels->stride;
        }
    } else {
        uint32_t smask = 0, emask = 0;
        int end = ((xDst + width) / 32);
        int i;

        if (xDst % 32)
            for (i = (xDst % 32); i < 32; i++)
#if BITMAP_BIT_ORDER == MSBFirst
                smask |= 1 << (0x1f - i);
#else
                smask |= 1 << i;
#endif

        if ((width + xDst) % 32)
            for (i = 0; i < (width + xDst) % 32; i++)
#if BITMAP_BIT_ORDER == MSBFirst
                emask |= 1 << (0x1f - i);
#else
                emask |= 1 << i;
#endif

        while (height-- > 0) {
            uint32_t *cur = (uint32_t *) line;
            int start = (xDst / 32);

            if (smask) {
                cur [start] = (cur [start] & ~smask) | (value & smask);
                start++;
            }

            if (emask)
                cur [end] = (cur [end] & ~emask) | (value & emask);

            if (end > start)
                memset (cur + start, value, (end - start) * 4);
	    line += dst->pixels->stride;
        }
    }
}

static void
pixman_fill_rect_8bpp (pixman_image_t *dst,
		       int16_t	       xDst,
		       int16_t	       yDst,
		       uint16_t	       width,
		       uint16_t	       height,
		       pixman_bits_t  *pixel)
{
    int value = (int) (*pixel);
    char *line;

    line = (char *)dst->pixels->data +
	xDst + yDst * dst->pixels->stride;
    while (height-- > 0) {
	memset (line, value, width);
	line += dst->pixels->stride;
    }
}

static void
pixman_fill_rect_32bpp (pixman_image_t *dst,
			int16_t	        xDst,
			int16_t	        yDst, 
			uint16_t	width,
			uint16_t	height,
			pixman_bits_t  *pixel)
{
    uint32_t int_pixel;
    char *line;
    char *data;
    int w;

    line = (char *)dst->pixels->data +
	xDst * 4 + yDst * dst->pixels->stride;
     
    int_pixel = *(uint32_t *)pixel;
    while (height-- > 0) {
	data = line;
	w = width;
	while (w-- > 0) {
	    *(uint32_t *)data = int_pixel;
	    data += 4;
	}
	line += dst->pixels->stride;
    }
}

static void
pixman_fill_rect_general (pixman_image_t *dst,
			  int16_t	  xDst,
			  int16_t	  yDst,
			  uint16_t	  width,
			  uint16_t	  height,
			  pixman_bits_t  *pixel)
{
    int pixel_size;
    char *line;
    char *data;
    int w;

    pixel_size = dst->pixels->bpp >> 3;

    line = (char *)dst->pixels->data +
	xDst * pixel_size + yDst * dst->pixels->stride;
     
    while (height-- > 0) {
	data = line;
	w = width;
	while (w-- > 0) {
	    memcpy (data, pixel, pixel_size);
	    data += pixel_size;
	}
	line += dst->pixels->stride;
    }
}


static void
pixman_color_rects (pixman_image_t	 *dst,
	      pixman_image_t	 *clipPict,
	      pixman_color_t	 *color,
	      int	 nRect,
	      pixman_rectangle_t *rects,
	      int	 xoff,
	      int	 yoff)
{
    pixman_bits_t	pixel;
    pixman_region16_t  *clip;
    pixman_region16_t  *rects_as_region;
    pixman_box16_t     *clipped_rects;
    int	                i, n_clipped_rects;
    FillFunc            func;

    pixman_color_to_pixel (&dst->image_format,
			   color,
			   &pixel);

    /* offset to the right place on the destination image */
    xoff -= dst->pixels->x;
    yoff -= dst->pixels->y;
    
    clip = pixman_region_create();
    pixman_region_union_rect (clip, clip,
			      dst->pixels->x, dst->pixels->y,
			      dst->pixels->width, dst->pixels->height);

    pixman_region_intersect (clip, clip, clipPict->pCompositeClip);
    if (clipPict->alphaMap)
    {
	pixman_region_translate (clip, 
				 -clipPict->alphaOrigin.x,
				 -clipPict->alphaOrigin.y);
	pixman_region_intersect (clip, clip, clipPict->alphaMap->pCompositeClip);
	pixman_region_translate (clip, 
				 clipPict->alphaOrigin.x,
				 clipPict->alphaOrigin.y);
    }

    if (xoff || yoff)
    {
	for (i = 0; i < nRect; i++)
	{
	    rects[i].x -= xoff;
	    rects[i].y -= yoff;
	}
    }

    rects_as_region = pixman_region_create ();
    for (i = 0; i < nRect; i++)
    {
	pixman_region_union_rect (rects_as_region, rects_as_region,
				  rects[i].x, rects[i].y,
				  rects[i].width, rects[i].height);
    }

    pixman_region_intersect (rects_as_region, rects_as_region, clip);
    pixman_region_destroy (clip);

    n_clipped_rects = pixman_region_num_rects (rects_as_region);
    clipped_rects = pixman_region_rects (rects_as_region);

    if (dst->pixels->bpp == 8)
	func = pixman_fill_rect_8bpp;
    else if (dst->pixels->bpp == 32)
	func = pixman_fill_rect_32bpp;
    else if (dst->pixels->bpp == 1)
	func = pixman_fill_rect_1bpp;
    else 
	func = pixman_fill_rect_general;
    
    for (i = 0; i < n_clipped_rects; i++) {
	(*func) (dst,
		 clipped_rects[i].x1, 
		 clipped_rects[i].y1, 
		 clipped_rects[i].x2 - clipped_rects[i].x1, 
		 clipped_rects[i].y2 - clipped_rects[i].y1,
		 &pixel);
    }

    pixman_region_destroy (rects_as_region);

    if (xoff || yoff)
    {
	for (i = 0; i < nRect; i++)
	{
	    rects[i].x += xoff;
	    rects[i].y += yoff;
	}
    }
}

void pixman_fill_rectangle (pixman_operator_t	op,
		      pixman_image_t		*dst,
		      const pixman_color_t	*color,
		      int		x,
		      int		y,
		      unsigned int	width,
		      unsigned int	height)
{
    pixman_rectangle_t rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    pixman_fill_rectangles (op, dst, color, &rect, 1);
}

void
pixman_fill_rectangles (pixman_operator_t		op,
		  pixman_image_t		*dst,
		  const pixman_color_t		*color,
		  const pixman_rectangle_t	*rects,
		  int			nRects)
{
    pixman_color_t color_s = *color;

    if (color_s.alpha == 0xffff)
    {
	if (op == PIXMAN_OPERATOR_OVER)
	    op = PIXMAN_OPERATOR_SRC;
    }
    if (op == PIXMAN_OPERATOR_CLEAR)
	color_s.red = color_s.green = color_s.blue = color_s.alpha = 0;

    if (op == PIXMAN_OPERATOR_SRC || op == PIXMAN_OPERATOR_CLEAR)
    {
      /* We cast away the constness of rects here, because pixman_color_rects
	 temporarily modifies it */
	pixman_color_rects (dst, dst, &color_s, nRects, (pixman_rectangle_t *)rects, 0, 0);
	if (dst->alphaMap)
	    pixman_color_rects (dst->alphaMap, dst,
			  &color_s, nRects, (pixman_rectangle_t *)rects,
			  dst->alphaOrigin.x,
			  dst->alphaOrigin.y);
    }
    else
    {
	pixman_format_t	rgbaFormat;
	FbPixels	*pixels;
	pixman_image_t		*src;
	pixman_bits_t		pixel;

	pixman_format_init (&rgbaFormat, PICT_a8r8g8b8);
	
	pixels = FbPixelsCreate (1, 1, rgbaFormat.depth);
	if (!pixels)
	    goto bail1;
	
	pixman_color_to_pixel (&rgbaFormat, &color_s, &pixel);

	/* XXX: Originally, fb had the following:

	   (*pGC->ops->PolyFillRect) (&pPixmap->drawable, pGC, 1, &one);

	   I haven't checked to see what I might be breaking with a
	   trivial assignment instead.
	*/
	pixels->data[0] = pixel;

	src = pixman_image_createForPixels (pixels, &rgbaFormat);
	if (!src)
	    goto bail2;

	pixman_image_set_repeat (src, PIXMAN_REPEAT_NORMAL);

	while (nRects--)
	{
	    pixman_composite (op, src, NULL, dst, 0, 0, 0, 0, 
			 rects->x,
			 rects->y,
			 rects->width,
			 rects->height);
	    rects++;
	}

	pixman_image_destroy (src);
bail2:
	FbPixelsDestroy (pixels);
bail1:
	;
    }
}
slim_hidden_def(pixman_fill_rectangles);
