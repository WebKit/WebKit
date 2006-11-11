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

#include "pixman-xserver-compat.h"

static void
FbPixelsInit (FbPixels *pixels, pixman_bits_t *buf, int width, int height, int depth, int bpp, int stride);

static unsigned int
pixman_bits_per_pixel (unsigned int depth);

static unsigned int
pixman_bits_per_pixel (unsigned int depth)
{
    if (depth > 8)
	if (depth > 16)
	    return 32;
	else
	    return 16;
    else
	if (depth > 4)
	    return 8;
	else if (depth > 1)
	    return 4;
	else
	    return 1;
}

FbPixels *
FbPixelsCreate (int width, int height, int depth)
{
    FbPixels		*pixels;
    pixman_bits_t		*buf;
    unsigned int	buf_size;
    unsigned int	bpp;
    unsigned int	stride;
    unsigned int	adjust;
    unsigned int	base;

    bpp = pixman_bits_per_pixel (depth);
    stride = ((width * bpp + FB_MASK) >> FB_SHIFT) * sizeof (pixman_bits_t);
    buf_size = height * stride;
    base = sizeof (FbPixels);
    adjust = 0;
    if (base & 7)
	adjust = 8 - (base & 7);
    buf_size += adjust;

    pixels = malloc(base + buf_size);
    if (!pixels)
	return NULL;

    buf = (pixman_bits_t *) ((char *)pixels + base + adjust);

    FbPixelsInit (pixels, buf, width, height, depth, bpp, stride);

    return pixels;
}

FbPixels *
FbPixelsCreateForData (pixman_bits_t *data, int width, int height, int depth, int bpp, int stride)
{
    FbPixels *pixels;

    pixels = malloc (sizeof (FbPixels));
    if (pixels == NULL)
	return NULL;

    FbPixelsInit (pixels, data, width, height, depth, bpp, stride);

    return pixels;
}

static void
FbPixelsInit (FbPixels *pixels, pixman_bits_t *buf, int width, int height, int depth, int bpp, int stride)
{
    pixels->data = buf;
    pixels->width = width;
    pixels->height = height;
    pixels->depth = depth;
    pixels->bpp = bpp;
    pixels->stride = stride;
    pixels->x = 0;
    pixels->y = 0;
    pixels->refcnt = 1;
}

void
FbPixelsDestroy (FbPixels *pixels)
{
    if(--pixels->refcnt)
	return;

    free(pixels);
}

