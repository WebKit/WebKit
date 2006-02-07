/*
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#include "icint.h"

#ifdef ICINT_NEED_IC_ONES
/* Fall back on HACKMEM 169.  */
int
_FbOnes (unsigned long mask)
{
    register unsigned long y;

    y = (mask >> 1) &033333333333;
    y = mask - y - ((y >>1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
}
#endif

void
pixman_color_to_pixel (const pixman_format_t	*format,
		const pixman_color_t	*color,
		pixman_bits_t		*pixel)
{
    uint32_t	    r, g, b, a;

    r = color->red >> (16 - _FbOnes (format->redMask));
    g = color->green >> (16 - _FbOnes (format->greenMask));
    b = color->blue >> (16 - _FbOnes (format->blueMask));
    a = color->alpha >> (16 - _FbOnes (format->alphaMask));
    r = r << format->red;
    g = g << format->green;
    b = b << format->blue;
    a = a << format->alpha;
    *pixel = r|g|b|a;
}
slim_hidden_def(pixman_color_to_pixel);

static uint16_t
FbFillColor (uint32_t pixel, int bits)
{
    while (bits < 16)
    {
	pixel |= pixel << bits;
	bits <<= 1;
    }
    return (uint16_t) pixel;
}

void
pixman_pixel_to_color (const pixman_format_t	*format,
		const pixman_bits_t	pixel,
		pixman_color_t		*color)
{
    uint32_t	    r, g, b, a;

    r = (pixel >> format->red) & format->redMask;
    g = (pixel >> format->green) & format->greenMask;
    b = (pixel >> format->blue) & format->blueMask;
    a = (pixel >> format->alpha) & format->alphaMask;
    color->red = FbFillColor (r, _FbOnes (format->redMask));
    color->green = FbFillColor (r, _FbOnes (format->greenMask));
    color->blue = FbFillColor (r, _FbOnes (format->blueMask));
    color->alpha = FbFillColor (r, _FbOnes (format->alphaMask));
}
