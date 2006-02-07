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

#define Mask(n)	((n) == 32 ? 0xffffffff : ((1 << (n))-1))

pixman_format_t *
pixman_format_create (pixman_format_name_t name)
{
    switch (name) {
    case PIXMAN_FORMAT_NAME_ARGB32:
	return pixman_format_create_masks (32,
				    0xff000000,
				    0x00ff0000,
				    0x0000ff00,
				    0x000000ff);
    case PIXMAN_FORMAT_NAME_RGB24:
	return pixman_format_create_masks (32,
				    0x0,
				    0xff0000,
				    0x00ff00,
				    0x0000ff);
    case PIXMAN_FORMAT_NAME_A8:
	return pixman_format_create_masks (8, 0xff,
				    0, 0, 0);
    case PIXMAN_FORMAT_NAME_A1:
	return pixman_format_create_masks (1, 0x1,
				    0, 0, 0);
    }

    return NULL;
}

/* XXX: There's some nonsense going on here. The macros above help
   pixman_format_create_masks to encode a format into an int, while
   immediately afterwards pixman_format_init goes through the effort of
   decoding it. This should all be disentagled, (it's probably
   possible to just eliminate the encoding macros altogether). */
pixman_format_t *
pixman_format_create_masks (int bpp,
		     int alpha_mask,
		     int red_mask,
		     int green_mask,
		     int blue_mask)
{
    int type;
    int format_code;
    pixman_format_t *format;

    if (red_mask == 0 && green_mask == 0 && blue_mask == 0)
	type = PICT_TYPE_A;
    else if (red_mask > blue_mask)
	type = PICT_TYPE_ARGB;
    else
	type = PICT_TYPE_ABGR;

    format_code = PICT_FORMAT (bpp, type,
			       _FbOnes (alpha_mask),
			       _FbOnes (red_mask),
			       _FbOnes (green_mask),
			       _FbOnes (blue_mask));

    format = malloc (sizeof (pixman_format_t));
    if (format == NULL)
	return NULL;

    pixman_format_init (format, format_code);

    return format;
}

void
pixman_format_init (pixman_format_t *format, int format_code)
{
    memset (format, 0, sizeof (pixman_format_t));
    
/* XXX: What do we want to lodge in here?
    format->id = FakeClientID (0);
*/
    format->format_code = format_code;

    switch (PICT_FORMAT_TYPE(format_code)) {
    case PICT_TYPE_ARGB:
	
	format->alphaMask = Mask(PICT_FORMAT_A(format_code));
	if (format->alphaMask)
	    format->alpha = (PICT_FORMAT_R(format_code) +
			     PICT_FORMAT_G(format_code) +
			     PICT_FORMAT_B(format_code));
	
	format->redMask = Mask(PICT_FORMAT_R(format_code));
	format->red = (PICT_FORMAT_G(format_code) + 
		       PICT_FORMAT_B(format_code));
	
	format->greenMask = Mask(PICT_FORMAT_G(format_code));
	format->green = PICT_FORMAT_B(format_code);
	
	format->blueMask = Mask(PICT_FORMAT_B(format_code));
	format->blue = 0;
	break;
	
    case PICT_TYPE_ABGR:
	
	format->alphaMask = Mask(PICT_FORMAT_A(format_code));
	if (format->alphaMask)
	    format->alpha = (PICT_FORMAT_B(format_code) +
			     PICT_FORMAT_G(format_code) +
			     PICT_FORMAT_R(format_code));
	
	format->blueMask = Mask(PICT_FORMAT_B(format_code));
	format->blue = (PICT_FORMAT_G(format_code) + 
			PICT_FORMAT_R(format_code));
	
	format->greenMask = Mask(PICT_FORMAT_G(format_code));
	format->green = PICT_FORMAT_R(format_code);
	
	format->redMask = Mask(PICT_FORMAT_R(format_code));
	format->red = 0;
	break;
	
    case PICT_TYPE_A:
	
	format->alpha = 0;
	format->alphaMask = Mask(PICT_FORMAT_A(format_code));

	/* remaining fields already set to zero */
	break;
    }

    format->depth = _FbOnes ((format->alphaMask << format->alpha) |
			     (format->redMask << format->red) |
			     (format->blueMask << format->blue) |
			     (format->greenMask << format->green));
}
slim_hidden_def(pixman_format_init);

void
pixman_format_destroy (pixman_format_t *format)
{
    free (format);
}

void
pixman_format_get_masks (pixman_format_t *format,
                         int *bpp,
                         int *alpha_mask,
                         int *red_mask,
                         int *green_mask,
                         int *blue_mask)
{
    *bpp = PICT_FORMAT_BPP (format->format_code);

    if (format->alphaMask)
	*alpha_mask = format->alphaMask << format->alpha;
    else
	*alpha_mask = 0;

    if (format->redMask)
	*red_mask = format->redMask << format->red;
    else
	*red_mask = 0;

    if (format->greenMask)
	*green_mask = format->greenMask << format->green;
    else
	*green_mask = 0;

    if (format->blueMask)
	*blue_mask = format->blueMask << format->blue;
    else
	*blue_mask = 0;
}
