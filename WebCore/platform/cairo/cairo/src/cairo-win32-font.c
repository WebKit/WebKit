/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 */

#include <string.h>
#include <stdio.h>
#include "cairoint.h"
#include "cairo-win32-private.h"

#ifndef SPI_GETFONTSMOOTHINGTYPE 
#define SPI_GETFONTSMOOTHINGTYPE 0x200a
#endif
#ifndef FE_FONTSMOOTHINGCLEARTYPE
#define FE_FONTSMOOTHINGCLEARTYPE 2
#endif
#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif
#ifndef TT_PRIM_CSPLINE
#define TT_PRIM_CSPLINE 3
#endif

const cairo_scaled_font_backend_t cairo_win32_scaled_font_backend;

typedef struct {
    cairo_scaled_font_t base;

    LOGFONTW logfont;

    BYTE quality;

    /* We do drawing and metrics computation in a "logical space" which
     * is similar to font space, except that it is scaled by a factor
     * of the (desired font size) * (WIN32_FONT_LOGICAL_SCALE). The multiplication
     * by WIN32_FONT_LOGICAL_SCALE allows for sub-pixel precision.
     */
    double logical_scale;

    /* The size we should actually request the font at from Windows; differs
     * from the logical_scale because it is quantized for orthogonal
     * transformations
     */
    double logical_size;

    /* Transformations from device <=> logical space
     */
    cairo_matrix_t logical_to_device;
    cairo_matrix_t device_to_logical;

    /* We special case combinations of 90-degree-rotations, scales and
     * flips ... that is transformations that take the axes to the
     * axes. If preserve_axes is true, then swap_axes/swap_x/swap_y
     * encode the 8 possibilities for orientation (4 rotation angles with
     * and without a flip), and scale_x, scale_y the scale components.
     */
    cairo_bool_t preserve_axes;
    cairo_bool_t swap_axes;
    cairo_bool_t swap_x;
    cairo_bool_t swap_y;
    double x_scale;
    double y_scale;
  
    /* The size of the design unit of the font
     */
    int em_square;

    HFONT scaled_hfont;
    HFONT unscaled_hfont;

    cairo_bool_t delete_scaled_hfont;
} cairo_win32_scaled_font_t;

static cairo_status_t 
_cairo_win32_scaled_font_set_metrics (cairo_win32_scaled_font_t *scaled_font);

static cairo_status_t 
_cairo_win32_scaled_font_init_glyph_metrics (cairo_win32_scaled_font_t *scaled_font,
					     cairo_scaled_glyph_t      *scaled_glyph);

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_path (cairo_win32_scaled_font_t *scaled_font,
					  cairo_scaled_glyph_t      *scaled_glyph);

#define NEARLY_ZERO(d) (fabs(d) < (1. / 65536.))

static void
_compute_transform (cairo_win32_scaled_font_t *scaled_font,
		    cairo_matrix_t            *sc)
{
    cairo_status_t status;

    if (NEARLY_ZERO (sc->yx) && NEARLY_ZERO (sc->xy)) {
	scaled_font->preserve_axes = TRUE;
	scaled_font->x_scale = sc->xx;
	scaled_font->swap_x = (sc->xx < 0);
	scaled_font->y_scale = sc->yy;
	scaled_font->swap_y = (sc->yy < 0);
	scaled_font->swap_axes = FALSE;
	
    } else if (NEARLY_ZERO (sc->xx) && NEARLY_ZERO (sc->yy)) {
	scaled_font->preserve_axes = TRUE;
	scaled_font->x_scale = sc->yx;
	scaled_font->swap_x = (sc->yx < 0);
	scaled_font->y_scale = sc->xy;
	scaled_font->swap_y = (sc->xy < 0);
	scaled_font->swap_axes = TRUE;

    } else {
	scaled_font->preserve_axes = FALSE;
	scaled_font->swap_x = scaled_font->swap_y = scaled_font->swap_axes = FALSE;
    }

    if (scaled_font->preserve_axes) {
	if (scaled_font->swap_x)
	    scaled_font->x_scale = - scaled_font->x_scale;
	if (scaled_font->swap_y)
	    scaled_font->y_scale = - scaled_font->y_scale;
	
	scaled_font->logical_scale = WIN32_FONT_LOGICAL_SCALE * scaled_font->y_scale;
	scaled_font->logical_size = WIN32_FONT_LOGICAL_SCALE * floor (scaled_font->y_scale + 0.5);
    }

    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values.
     */
    cairo_matrix_init (&scaled_font->logical_to_device,
		       sc->xx, sc->yx, sc->xy, sc->yy, 0, 0);

    if (!scaled_font->preserve_axes) {
	_cairo_matrix_compute_scale_factors (&scaled_font->logical_to_device,
					     &scaled_font->x_scale, &scaled_font->y_scale,
					     TRUE);	/* XXX: Handle vertical text */

	scaled_font->logical_size = floor (WIN32_FONT_LOGICAL_SCALE * scaled_font->y_scale + 0.5);
	scaled_font->logical_scale = WIN32_FONT_LOGICAL_SCALE * scaled_font->y_scale;
    }

    cairo_matrix_scale (&scaled_font->logical_to_device,
			1.0 / scaled_font->logical_scale, 1.0 / scaled_font->logical_scale);

    scaled_font->device_to_logical = scaled_font->logical_to_device;
    
    status = cairo_matrix_invert (&scaled_font->device_to_logical);
    if (status)
	cairo_matrix_init_identity (&scaled_font->device_to_logical);
}

static cairo_bool_t
_have_cleartype_quality (void)
{
    OSVERSIONINFO version_info;
    
    version_info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
    
    if (!GetVersionEx (&version_info)) {
	_cairo_win32_print_gdi_error ("_have_cleartype_quality");
	return FALSE;
    }
    
    return (version_info.dwMajorVersion > 5 ||
	    (version_info.dwMajorVersion == 5 &&
	     version_info.dwMinorVersion >= 1));	/* XP or newer */
}


static BYTE
_get_system_quality (void)
{
    BOOL font_smoothing;
    UINT smoothing_type;

    if (!SystemParametersInfo (SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
	_cairo_win32_print_gdi_error ("_get_system_quality");
	return DEFAULT_QUALITY;
    }

    if (font_smoothing) {
	if (_have_cleartype_quality ()) {
	    if (!SystemParametersInfo (SPI_GETFONTSMOOTHINGTYPE,
				       0, &smoothing_type, 0)) {
		_cairo_win32_print_gdi_error ("_get_system_quality");
		return DEFAULT_QUALITY;
	    }

	    if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	}

	return ANTIALIASED_QUALITY;
    } else {
	return DEFAULT_QUALITY;
    }
}

static cairo_scaled_font_t *
_win32_scaled_font_create (LOGFONTW                   *logfont,
			   HFONT                      hfont,
			   cairo_font_face_t	      *font_face,
			   const cairo_matrix_t       *font_matrix,
			   const cairo_matrix_t       *ctm,
			   const cairo_font_options_t *options)			  
{
    cairo_win32_scaled_font_t *f;
    cairo_matrix_t scale;
    cairo_status_t status;

    f = malloc (sizeof(cairo_win32_scaled_font_t));
    if (f == NULL)
	return NULL;

    f->logfont = *logfont;

    /* We don't have any control over the hinting style or subpixel
     * order in the Win32 font API, so we ignore those parts of
     * cairo_font_options_t. We use the 'antialias' field to set
     * the 'quality'.
     * 
     * XXX: The other option we could pay attention to, but don't
     *      here is the hint_metrics options.
     */
    if (options->antialias == CAIRO_ANTIALIAS_DEFAULT)
	f->quality = _get_system_quality ();
    else {
	switch (options->antialias) {
	case CAIRO_ANTIALIAS_NONE:
	    f->quality = NONANTIALIASED_QUALITY;
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    f->quality = ANTIALIASED_QUALITY;
	    break;
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    if (_have_cleartype_quality ())
		f->quality = CLEARTYPE_QUALITY;
	    else
		f->quality = ANTIALIASED_QUALITY;
	    break;
	case CAIRO_ANTIALIAS_DEFAULT:
	    ASSERT_NOT_REACHED;
	}
    }
    
    f->em_square = 0;
    f->scaled_hfont = hfont;
    f->unscaled_hfont = NULL;

    /* don't delete the hfont if it was passed in to us */
    f->delete_scaled_hfont = !hfont;

    cairo_matrix_multiply (&scale, font_matrix, ctm);
    _compute_transform (f, &scale);

    _cairo_scaled_font_init (&f->base, font_face,
			     font_matrix, ctm, options,
			     &cairo_win32_scaled_font_backend);

    status = _cairo_win32_scaled_font_set_metrics (f);
    if (status) {
	cairo_scaled_font_destroy (&f->base);
	return NULL;
    }

    return &f->base;
}

static cairo_status_t
_win32_scaled_font_set_world_transform (cairo_win32_scaled_font_t *scaled_font,
					HDC                        hdc)
{
    XFORM xform;
    
    xform.eM11 = scaled_font->logical_to_device.xx;
    xform.eM21 = scaled_font->logical_to_device.xy;
    xform.eM12 = scaled_font->logical_to_device.yx;
    xform.eM22 = scaled_font->logical_to_device.yy;
    xform.eDx = scaled_font->logical_to_device.x0;
    xform.eDy = scaled_font->logical_to_device.y0;

    if (!SetWorldTransform (hdc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_scaled_font_set_world_transform");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_win32_scaled_font_set_identity_transform (HDC hdc)
{
    if (!ModifyWorldTransform (hdc, NULL, MWT_IDENTITY))
	return _cairo_win32_print_gdi_error ("_win32_scaled_font_set_identity_transform");

    return CAIRO_STATUS_SUCCESS;
}

static HDC
_get_global_font_dc (void)
{
    static HDC hdc;

    if (!hdc) {
	hdc = CreateCompatibleDC (NULL);
	if (!hdc) {
	    _cairo_win32_print_gdi_error ("_get_global_font_dc");
	    return NULL;
	}

	if (!SetGraphicsMode (hdc, GM_ADVANCED)) {
	    _cairo_win32_print_gdi_error ("_get_global_font_dc");
	    DeleteDC (hdc);
	    return NULL;
	}
    }

    return hdc;
}

static HFONT
_win32_scaled_font_get_scaled_hfont (cairo_win32_scaled_font_t *scaled_font)
{
    if (!scaled_font->scaled_hfont) {
	LOGFONTW logfont = scaled_font->logfont;
	logfont.lfHeight = -scaled_font->logical_size;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfQuality = scaled_font->quality;

	scaled_font->scaled_hfont = CreateFontIndirectW (&logfont);
	if (!scaled_font->scaled_hfont) {
	    _cairo_win32_print_gdi_error ("_win32_scaled_font_get_scaled_hfont");
	    return NULL;
	}
    }

    return scaled_font->scaled_hfont;
}

static HFONT
_win32_scaled_font_get_unscaled_hfont (cairo_win32_scaled_font_t *scaled_font,
				       HDC                        hdc)
{
    if (!scaled_font->unscaled_hfont) {
	OUTLINETEXTMETRIC *otm;
	unsigned int otm_size;
	HFONT scaled_hfont;
	LOGFONTW logfont;

	scaled_hfont = _win32_scaled_font_get_scaled_hfont (scaled_font);
	if (!scaled_hfont)
	    return NULL;

	if (!SelectObject (hdc, scaled_hfont)) {
	    _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:SelectObject");
	    return NULL;
	}

	otm_size = GetOutlineTextMetrics (hdc, 0, NULL);
	if (!otm_size) {
	    _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:GetOutlineTextMetrics");
	    return NULL;
	}
	
	otm = malloc (otm_size);
	if (!otm)
	    return NULL;

	if (!GetOutlineTextMetrics (hdc, otm_size, otm)) {
	    _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:GetOutlineTextMetrics");
	    free (otm);
	    return NULL;
	}

	scaled_font->em_square = otm->otmEMSquare;
	free (otm);
	
	logfont = scaled_font->logfont;
	logfont.lfHeight = -scaled_font->em_square;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfQuality = scaled_font->quality;
	
	scaled_font->unscaled_hfont = CreateFontIndirectW (&logfont);
	if (!scaled_font->unscaled_hfont) {
	    _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:CreateIndirect");
	    return NULL;
	}
    }

    return scaled_font->unscaled_hfont;
}

static cairo_status_t
_cairo_win32_scaled_font_select_unscaled_font (cairo_scaled_font_t *scaled_font,
					       HDC                  hdc)
{
    cairo_status_t status;
    HFONT hfont;
    HFONT old_hfont = NULL;

    hfont = _win32_scaled_font_get_unscaled_hfont ((cairo_win32_scaled_font_t *)scaled_font, hdc);
    if (!hfont)
	return CAIRO_STATUS_NO_MEMORY;

    old_hfont = SelectObject (hdc, hfont);
    if (!old_hfont)
	return _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_select_unscaled_font");

    status = _win32_scaled_font_set_identity_transform (hdc);
    if (status) {
	SelectObject (hdc, old_hfont);
	return status;
    }

    SetMapMode (hdc, MM_TEXT);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_scaled_font_done_unscaled_font (cairo_scaled_font_t *scaled_font)
{
}

/* implement the font backend interface */

static cairo_status_t
_cairo_win32_scaled_font_create_toy (cairo_toy_font_face_t *toy_face,
				     const cairo_matrix_t        *font_matrix,
				     const cairo_matrix_t        *ctm,
				     const cairo_font_options_t  *options,
				     cairo_scaled_font_t        **scaled_font_out)
{
    LOGFONTW logfont;
    cairo_scaled_font_t *scaled_font;
    uint16_t *face_name;
    int face_name_len;
    cairo_status_t status;

    status = _cairo_utf8_to_utf16 (toy_face->family, -1,
				   &face_name, &face_name_len);
    if (status)
	return status;

    if (face_name_len > LF_FACESIZE - 1) {
	free (face_name);
	return CAIRO_STATUS_INVALID_STRING;
    }

    memcpy (logfont.lfFaceName, face_name, sizeof (uint16_t) * (face_name_len + 1));
    free (face_name);

    logfont.lfHeight = 0;	/* filled in later */
    logfont.lfWidth = 0;	/* filled in later */
    logfont.lfEscapement = 0;	/* filled in later */
    logfont.lfOrientation = 0;	/* filled in later */

    switch (toy_face->weight) {
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
	logfont.lfWeight = FW_NORMAL;
	break;
    case CAIRO_FONT_WEIGHT_BOLD:
	logfont.lfWeight = FW_BOLD;
	break;
    }

    switch (toy_face->slant) {
    case CAIRO_FONT_SLANT_NORMAL:
    default:
	logfont.lfItalic = FALSE;
	break;
    case CAIRO_FONT_SLANT_ITALIC:
    case CAIRO_FONT_SLANT_OBLIQUE:
	logfont.lfItalic = TRUE;
	break;
    }

    logfont.lfUnderline = FALSE;
    logfont.lfStrikeOut = FALSE;
    /* The docs for LOGFONT discourage using this, since the
     * interpretation is locale-specific, but it's not clear what
     * would be a better alternative.
     */
    logfont.lfCharSet = DEFAULT_CHARSET; 
    logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    logfont.lfQuality = DEFAULT_QUALITY; /* filled in later */
    logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    if (!logfont.lfFaceName)
	return CAIRO_STATUS_NO_MEMORY;
    
    scaled_font = _win32_scaled_font_create (&logfont, NULL, &toy_face->base,
					     font_matrix, ctm, options);
    if (!scaled_font)
	return CAIRO_STATUS_NO_MEMORY;

    *scaled_font_out = scaled_font;

    return CAIRO_STATUS_SUCCESS;
}

static void 
_cairo_win32_scaled_font_fini (void *abstract_font)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;

    if (scaled_font == NULL)
	return;

    if (scaled_font->scaled_hfont && scaled_font->delete_scaled_hfont)
	DeleteObject (scaled_font->scaled_hfont);

    if (scaled_font->unscaled_hfont)
	DeleteObject (scaled_font->unscaled_hfont);
}

static cairo_int_status_t 
_cairo_win32_scaled_font_text_to_glyphs (void		*abstract_font,
					 double		x,
					 double		y,
					 const char	*utf8,
					 cairo_glyph_t **glyphs, 
					 int		*num_glyphs)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    uint16_t *utf16;
    int n16;
    GCP_RESULTSW gcp_results;
    unsigned int buffer_size, i;
    WCHAR *glyph_indices = NULL;
    int *dx = NULL;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    double x_pos, y_pos;
    double x_incr, y_incr;
    HDC hdc = NULL;

    /* Compute a vector in user space along the baseline of length one logical space unit */
    x_incr = 1;
    y_incr = 0;
    cairo_matrix_transform_distance (&scaled_font->base.font_matrix, &x_incr, &y_incr);
    x_incr /= scaled_font->logical_scale;
    y_incr /= scaled_font->logical_scale;
    
    status = _cairo_utf8_to_utf16 (utf8, -1, &utf16, &n16);
    if (status)
	return status;

    gcp_results.lStructSize = sizeof (GCP_RESULTS);
    gcp_results.lpOutString = NULL;
    gcp_results.lpOrder = NULL;
    gcp_results.lpCaretPos = NULL;
    gcp_results.lpClass = NULL;
    
    buffer_size = MAX (n16 * 1.2, 16);		/* Initially guess number of chars plus a few */
    if (buffer_size > INT_MAX) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL1;
    }
    
    hdc = _get_global_font_dc ();
    if (!hdc) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL1;
    }

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
	goto FAIL1;
    
    while (TRUE) {
	if (glyph_indices) {
	    free (glyph_indices);
	    glyph_indices = NULL;
	}
	if (dx) {
	    free (dx);
	    dx = NULL;
	}
	
	glyph_indices = malloc (sizeof (WCHAR) * buffer_size);
	dx = malloc (sizeof (int) * buffer_size);
	if (!glyph_indices || !dx) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL2;
	}

	gcp_results.nGlyphs = buffer_size;
	gcp_results.lpDx = dx;
	gcp_results.lpGlyphs = glyph_indices;

	if (!GetCharacterPlacementW (hdc, utf16, n16,
				     0,
				     &gcp_results, 
				     GCP_DIACRITIC | GCP_LIGATE | GCP_GLYPHSHAPE | GCP_REORDER)) {
	    status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_text_to_glyphs");
	    goto FAIL2;
	}

	if (gcp_results.lpDx && gcp_results.lpGlyphs)
	    break;

	/* Too small a buffer, try again */
	
	buffer_size *= 1.5;
	if (buffer_size > INT_MAX) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL2;
	}
    }

    *num_glyphs = gcp_results.nGlyphs;
    *glyphs = malloc (sizeof (cairo_glyph_t) * gcp_results.nGlyphs);
    if (!*glyphs) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL2;
    }

    x_pos = x;
    y_pos = y;
    
    for (i = 0; i < gcp_results.nGlyphs; i++) {
	(*glyphs)[i].index = glyph_indices[i];
	(*glyphs)[i].x = x_pos ;
	(*glyphs)[i].y = y_pos;

	x_pos += x_incr * dx[i];
	y_pos += y_incr * dx[i];
    }

 FAIL2:
    if (glyph_indices)
	free (glyph_indices);
    if (dx)
	free (dx);
   
    cairo_win32_scaled_font_done_font (&scaled_font->base);
    
 FAIL1:
    free (utf16);
   
    return status;
}

static cairo_status_t 
_cairo_win32_scaled_font_set_metrics (cairo_win32_scaled_font_t *scaled_font)
{
    cairo_status_t status;
    cairo_font_extents_t extents;

    TEXTMETRIC metrics;
    HDC hdc;

    hdc = _get_global_font_dc ();
    if (!hdc)
	return CAIRO_STATUS_NO_MEMORY;

    if (scaled_font->preserve_axes) {
	/* For 90-degree rotations (including 0), we get the metrics
	 * from the GDI in logical space, then convert back to font space
	 */
	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;
	GetTextMetrics (hdc, &metrics);
	cairo_win32_scaled_font_done_font (&scaled_font->base);

	extents.ascent = metrics.tmAscent / scaled_font->logical_scale;
	extents.descent = metrics.tmDescent / scaled_font->logical_scale;

	extents.height = (metrics.tmHeight + metrics.tmExternalLeading) / scaled_font->logical_scale;
	extents.max_x_advance = metrics.tmMaxCharWidth / scaled_font->logical_scale;
	extents.max_y_advance = 0;

    } else {
	/* For all other transformations, we use the design metrics
	 * of the font. The GDI results from GetTextMetrics() on a
	 * transformed font are inexplicably large and we want to
	 * avoid them.
	 */
	status = _cairo_win32_scaled_font_select_unscaled_font (&scaled_font->base, hdc);
	if (status)
	    return status;
	GetTextMetrics (hdc, &metrics);
	_cairo_win32_scaled_font_done_unscaled_font (&scaled_font->base);

	extents.ascent = (double)metrics.tmAscent / scaled_font->em_square;
	extents.descent = metrics.tmDescent * scaled_font->em_square;
	extents.height = (double)(metrics.tmHeight + metrics.tmExternalLeading) / scaled_font->em_square;
	extents.max_x_advance = (double)(metrics.tmMaxCharWidth) / scaled_font->em_square;
	extents.max_y_advance = 0;
	
    }

    _cairo_scaled_font_set_metrics (&scaled_font->base, &extents);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_scaled_font_init_glyph_metrics (cairo_win32_scaled_font_t *scaled_font,
					     cairo_scaled_glyph_t      *scaled_glyph)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    GLYPHMETRICS metrics;
    cairo_status_t status;
    cairo_text_extents_t extents;
    HDC hdc;

    hdc = _get_global_font_dc ();
    if (!hdc)
	return CAIRO_STATUS_NO_MEMORY;

    if (scaled_font->preserve_axes) {
	/* If we aren't rotating / skewing the axes, then we get the metrics
	 * from the GDI in device space and convert to font space.
	 */
	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;
	if (GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
			      GGO_METRICS | GGO_GLYPH_INDEX,
			      &metrics, 0, NULL, &matrix) == GDI_ERROR) {
	  status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_init_glyph_metrics:GetGlyphOutlineW");
	  memset (&metrics, 0, sizeof (GLYPHMETRICS));
	}
	cairo_win32_scaled_font_done_font (&scaled_font->base);

	if (scaled_font->swap_axes) {
	    extents.x_bearing = - metrics.gmptGlyphOrigin.y / scaled_font->y_scale;
	    extents.y_bearing = metrics.gmptGlyphOrigin.x / scaled_font->x_scale;
	    extents.width = metrics.gmBlackBoxY / scaled_font->y_scale;
	    extents.height = metrics.gmBlackBoxX / scaled_font->x_scale;
	    extents.x_advance = metrics.gmCellIncY / scaled_font->x_scale;
	    extents.y_advance = metrics.gmCellIncX / scaled_font->y_scale;
	} else {
	    extents.x_bearing = metrics.gmptGlyphOrigin.x / scaled_font->x_scale;
	    extents.y_bearing = - metrics.gmptGlyphOrigin.y / scaled_font->y_scale;
	    extents.width = metrics.gmBlackBoxX / scaled_font->x_scale;
	    extents.height = metrics.gmBlackBoxY / scaled_font->y_scale;
	    extents.x_advance = metrics.gmCellIncX / scaled_font->x_scale;
	    extents.y_advance = metrics.gmCellIncY / scaled_font->y_scale;
	}

	if (scaled_font->swap_x) {
	    extents.x_bearing = (- extents.x_bearing - extents.width);
	    extents.x_advance = - extents.x_advance;
	}

	if (scaled_font->swap_y) {
	    extents.y_bearing = (- extents.y_bearing - extents.height);
	    extents.y_advance = - extents.y_advance;
	}
	
    } else {
	/* For all other transformations, we use the design metrics
	 * of the font.
	 */
	status = _cairo_win32_scaled_font_select_unscaled_font (&scaled_font->base, hdc);
	if (GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
			      GGO_METRICS | GGO_GLYPH_INDEX,
			      &metrics, 0, NULL, &matrix) == GDI_ERROR) {
	  status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_init_glyph_metrics:GetGlyphOutlineW");
	  memset (&metrics, 0, sizeof (GLYPHMETRICS));
	}
	_cairo_win32_scaled_font_done_unscaled_font (&scaled_font->base);

	extents.x_bearing = (double)metrics.gmptGlyphOrigin.x / scaled_font->em_square;
	extents.y_bearing = - (double)metrics.gmptGlyphOrigin.y / scaled_font->em_square;
	extents.width = (double)metrics.gmBlackBoxX / scaled_font->em_square;
	extents.height = (double)metrics.gmBlackBoxY / scaled_font->em_square;
	extents.x_advance = (double)metrics.gmCellIncX / scaled_font->em_square;
	extents.y_advance = (double)metrics.gmCellIncY / scaled_font->em_square;
    }

    _cairo_scaled_glyph_set_metrics (scaled_glyph,
				     &scaled_font->base,
				     &extents);

    return CAIRO_STATUS_SUCCESS;
}

/* Not currently used code, but may be useful in the future if we add
 * back the capability to the scaled font backend interface to get the
 * actual device space bbox rather than computing it from the
 * font-space metrics.
 */
#if 0
static cairo_status_t 
_cairo_win32_scaled_font_glyph_bbox (void		 *abstract_font,
				     const cairo_glyph_t *glyphs,
				     int                  num_glyphs,
				     cairo_box_t         *bbox)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    if (num_glyphs > 0) {
	HDC hdc = _get_global_font_dc ();
	GLYPHMETRICS metrics;
	cairo_status_t status;
	int i;

	if (!hdc)
	    return CAIRO_STATUS_NO_MEMORY;

	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;

	for (i = 0; i < num_glyphs; i++) {
	    int x = floor (0.5 + glyphs[i].x);
	    int y = floor (0.5 + glyphs[i].y);

	    GetGlyphOutlineW (hdc, glyphs[i].index, GGO_METRICS | GGO_GLYPH_INDEX,
			     &metrics, 0, NULL, &matrix);

	    if (i == 0 || x1 > x + metrics.gmptGlyphOrigin.x)
		x1 = x + metrics.gmptGlyphOrigin.x;
	    if (i == 0 || y1 > y - metrics.gmptGlyphOrigin.y)
		y1 = y - metrics.gmptGlyphOrigin.y;
	    if (i == 0 || x2 < x + metrics.gmptGlyphOrigin.x + (int)metrics.gmBlackBoxX)
		x2 = x + metrics.gmptGlyphOrigin.x + (int)metrics.gmBlackBoxX;
	    if (i == 0 || y2 < y - metrics.gmptGlyphOrigin.y + (int)metrics.gmBlackBoxY)
		y2 = y - metrics.gmptGlyphOrigin.y + (int)metrics.gmBlackBoxY;
	}

	cairo_win32_scaled_font_done_font (&scaled_font->base);
    }

    bbox->p1.x = _cairo_fixed_from_int (x1);
    bbox->p1.y = _cairo_fixed_from_int (y1);
    bbox->p2.x = _cairo_fixed_from_int (x2);
    bbox->p2.y = _cairo_fixed_from_int (y2);

    return CAIRO_STATUS_SUCCESS;
}
#endif

typedef struct {
    cairo_win32_scaled_font_t *scaled_font;
    HDC hdc;
    
    cairo_array_t glyphs;
    cairo_array_t dx;

    int start_x;
    int last_x;
    int last_y;
} cairo_glyph_state_t;

static void
_start_glyphs (cairo_glyph_state_t        *state,
	       cairo_win32_scaled_font_t  *scaled_font,
	       HDC                         hdc)
{
    state->hdc = hdc;
    state->scaled_font = scaled_font;

    _cairo_array_init (&state->glyphs, sizeof (WCHAR));
    _cairo_array_init (&state->dx, sizeof (int));
}

static cairo_status_t
_flush_glyphs (cairo_glyph_state_t *state)
{
    cairo_status_t status;
    int dx = 0;
    WCHAR * elements;
    int * dx_elements;

    status = _cairo_array_append (&state->dx, &dx);
    if (status)
	return status;
    
    elements = _cairo_array_index (&state->glyphs, 0);
    dx_elements = _cairo_array_index (&state->dx, 0);
    if (!ExtTextOutW (state->hdc,
		      state->start_x, state->last_y,
		      ETO_GLYPH_INDEX,
		      NULL,
		      elements,
		      state->glyphs.num_elements,
		      dx_elements)) {
	return _cairo_win32_print_gdi_error ("_flush_glyphs");
    }
    
    _cairo_array_truncate (&state->glyphs, 0);
    _cairo_array_truncate (&state->dx, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_add_glyph (cairo_glyph_state_t *state,
	    unsigned long        index,
	    double               device_x,
	    double               device_y)
{
    cairo_status_t status;
    double user_x = device_x;
    double user_y = device_y;
    WCHAR glyph_index = index;
    int logical_x, logical_y;

    cairo_matrix_transform_point (&state->scaled_font->device_to_logical, &user_x, &user_y);

    logical_x = floor (user_x + 0.5);
    logical_y = floor (user_y + 0.5);

    if (state->glyphs.num_elements > 0) {
	int dx;
	
	if (logical_y != state->last_y) {
	    status = _flush_glyphs (state);
	    if (status)
		return status;
	    state->start_x = logical_x;
	}
	
	dx = logical_x - state->last_x;
	status = _cairo_array_append (&state->dx, &dx);
	if (status)
	    return status;
    } else {
	state->start_x = logical_x;
    }

    state->last_x = logical_x;
    state->last_y = logical_y;
    
    status = _cairo_array_append (&state->glyphs, &glyph_index);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static void
_finish_glyphs (cairo_glyph_state_t *state)
{
    _flush_glyphs (state);

    _cairo_array_fini (&state->glyphs);
    _cairo_array_fini (&state->dx);
}

static cairo_status_t
_draw_glyphs_on_surface (cairo_win32_surface_t     *surface,
			 cairo_win32_scaled_font_t *scaled_font,
			 COLORREF                   color,
			 int                        x_offset,
			 int                        y_offset,
			 const cairo_glyph_t       *glyphs,
			 int                 	    num_glyphs)
{
    cairo_glyph_state_t state;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int i;

    if (!SaveDC (surface->dc))
	return _cairo_win32_print_gdi_error ("_draw_glyphs_on_surface:SaveDC");

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, surface->dc);
    if (status)
	goto FAIL1;

    SetTextColor (surface->dc, color);
    SetTextAlign (surface->dc, TA_BASELINE | TA_LEFT);
    SetBkMode (surface->dc, TRANSPARENT);
    
    _start_glyphs (&state, scaled_font, surface->dc);

    for (i = 0; i < num_glyphs; i++) {
	status = _add_glyph (&state, glyphs[i].index,
			     glyphs[i].x - x_offset, glyphs[i].y - y_offset);
	if (status)
	    goto FAIL2;
    }

 FAIL2:
    _finish_glyphs (&state);
    cairo_win32_scaled_font_done_font (&scaled_font->base);
 FAIL1:
    RestoreDC (surface->dc, -1);
    
    return status;
}
		
/* Duplicate the green channel of a 4-channel mask in the alpha channel, then
 * invert the whole mask.
 */
static void
_compute_argb32_mask_alpha (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image = (cairo_image_surface_t *)mask_surface->image;
    int i, j;

    for (i = 0; i < image->height; i++) {
	uint32_t *p = (uint32_t *) (image->data + i * image->stride);
	for (j = 0; j < image->width; j++) {
	    *p = 0xffffffff ^ (*p | ((*p & 0x0000ff00) << 16));
	    p++;
	}
    }
}

/* Invert a mask
 */
static void
_invert_argb32_mask (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image = (cairo_image_surface_t *)mask_surface->image;
    int i, j;

    for (i = 0; i < image->height; i++) {
	uint32_t *p = (uint32_t *) (image->data + i * image->stride);
	for (j = 0; j < image->width; j++) {
	    *p = 0xffffffff ^ *p;
	    p++;
	}
    }
}

/* Compute an alpha-mask from a monochrome RGB24 image
 */
static cairo_surface_t *
_compute_a8_mask (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image24 = (cairo_image_surface_t *)mask_surface->image;
    cairo_image_surface_t *image8;
    int i, j;

    image8 = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8,
								  image24->width, image24->height);
    if (image8->base.status)
	return NULL;

    for (i = 0; i < image24->height; i++) {
	uint32_t *p = (uint32_t *) (image24->data + i * image24->stride);
	unsigned char *q = (unsigned char *) (image8->data + i * image8->stride);
	
	for (j = 0; j < image24->width; j++) {
	    *q = 255 - ((*p & 0x0000ff00) >> 8);
	    p++;
	    q++;
	}
    }

    return &image8->base;
}


static cairo_status_t
_cairo_win32_scaled_font_glyph_init (void		       *abstract_font,
				     cairo_scaled_glyph_t      *scaled_glyph,
				     cairo_scaled_glyph_info_t  info)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    cairo_status_t status;

    if ((info & CAIRO_SCALED_GLYPH_INFO_METRICS) != 0) {
	status = _cairo_win32_scaled_font_init_glyph_metrics (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_SURFACE) != 0) {
	ASSERT_NOT_REACHED;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_PATH) != 0) {
	status = _cairo_win32_scaled_font_init_glyph_path (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t 
_cairo_win32_scaled_font_show_glyphs (void		       *abstract_font,
				      cairo_operator_t    	op,
				      cairo_pattern_t          *pattern,
				      cairo_surface_t          *generic_surface,
				      int                 	source_x,
				      int                 	source_y,
				      int			dest_x,
				      int			dest_y,
				      unsigned int		width,
				      unsigned int		height,
				      const cairo_glyph_t      *glyphs,
				      int                 	num_glyphs)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)generic_surface;
    cairo_status_t status;

    if (width == 0 || height == 0)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_is_win32 (generic_surface) &&
	surface->format == CAIRO_FORMAT_RGB24 &&
	op == CAIRO_OPERATOR_OVER &&
	_cairo_pattern_is_opaque_solid (pattern)) {

	cairo_solid_pattern_t *solid_pattern = (cairo_solid_pattern_t *)pattern;

	/* When compositing OVER on a GDI-understood surface, with a
	 * solid opaque color, we can just call ExtTextOut directly.
	 */
	COLORREF new_color;
	
	new_color = RGB (((int)solid_pattern->color.red_short) >> 8,
			 ((int)solid_pattern->color.green_short) >> 8,
			 ((int)solid_pattern->color.blue_short) >> 8);

	status = _draw_glyphs_on_surface (surface, scaled_font, new_color,
					  0, 0,
					  glyphs, num_glyphs);
	
	return status;
    } else {
	/* Otherwise, we need to draw using software fallbacks. We create a mask 
	 * surface by drawing the the glyphs onto a DIB, black-on-white then
	 * inverting. GDI outputs gamma-corrected images so inverted black-on-white
	 * is very different from white-on-black. We favor the more common
	 * case where the final output is dark-on-light.
	 */
	cairo_win32_surface_t *tmp_surface;
	cairo_surface_t *mask_surface;
	cairo_surface_pattern_t mask;
	RECT r;

	tmp_surface = (cairo_win32_surface_t *)cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32, width, height);
	if (tmp_surface->base.status)
	    return CAIRO_STATUS_NO_MEMORY;

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;
	FillRect (tmp_surface->dc, &r, GetStockObject (WHITE_BRUSH));

	_draw_glyphs_on_surface (tmp_surface, scaled_font, RGB (0, 0, 0),
				 dest_x, dest_y,
				 glyphs, num_glyphs);

	if (scaled_font->quality == CLEARTYPE_QUALITY) {
	    /* For ClearType, we need a 4-channel mask. If we are compositing on
	     * a surface with alpha, we need to compute the alpha channel of
	     * the mask (we just copy the green channel). But for a destination
	     * surface without alpha the alpha channel of the mask is ignored
	     */

	    if (surface->format != CAIRO_FORMAT_RGB24)
		_compute_argb32_mask_alpha (tmp_surface);
	    else
		_invert_argb32_mask (tmp_surface);
	    
	    mask_surface = &tmp_surface->base;

	    /* XXX: Hacky, should expose this in cairo_image_surface */
	    pixman_image_set_component_alpha (((cairo_image_surface_t *)tmp_surface->image)->pixman_image, TRUE);
	    
	} else {
	    mask_surface = _compute_a8_mask (tmp_surface);
	    cairo_surface_destroy (&tmp_surface->base);
	    if (!mask_surface)
		return CAIRO_STATUS_NO_MEMORY;
	}

	/* For op == OVER, no-cleartype, a possible optimization here is to
	 * draw onto an intermediate ARGB32 surface and alpha-blend that with the
	 * destination
	 */
	_cairo_pattern_init_for_surface (&mask, mask_surface);

	status = _cairo_surface_composite (op, pattern, 
					   &mask.base,
					   &surface->base,
					   source_x, source_y,
					   0, 0,
					   dest_x, dest_y,
					   width, height);

	_cairo_pattern_fini (&mask.base);
	
	cairo_surface_destroy (mask_surface);

	return status;
    }
}

static cairo_fixed_t
_cairo_fixed_from_FIXED (FIXED f)
{
    return *((cairo_fixed_t *)&f);
}

static cairo_status_t 
_cairo_win32_scaled_font_init_glyph_path (cairo_win32_scaled_font_t *scaled_font,
					  cairo_scaled_glyph_t      *scaled_glyph)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, -1 } };
    cairo_status_t status;
    GLYPHMETRICS metrics;
    HDC hdc;
    DWORD bytesGlyph;
    unsigned char *buffer, *ptr;
    cairo_path_fixed_t *path;

    hdc = _get_global_font_dc ();
    if (!hdc)
        return CAIRO_STATUS_NO_MEMORY;

    path = _cairo_path_fixed_create ();
    if (!path)
	return CAIRO_STATUS_NO_MEMORY;

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
        goto CLEANUP_PATH;
    
    bytesGlyph = GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
				   GGO_NATIVE | GGO_GLYPH_INDEX,
				   &metrics, 0, NULL, &matrix);
    
    if (bytesGlyph == GDI_ERROR) {
	status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_glyph_path");
	goto CLEANUP_FONT;
    }

    ptr = buffer = malloc (bytesGlyph);
    
    if (!buffer) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_FONT;
    }

    if (GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
			  GGO_NATIVE | GGO_GLYPH_INDEX,
			  &metrics, bytesGlyph, buffer, &matrix) == GDI_ERROR) {
	status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_glyph_path");
	free (buffer);
	goto CLEANUP_FONT;
    }
    
    while (ptr < buffer + bytesGlyph) {
	TTPOLYGONHEADER *header = (TTPOLYGONHEADER *)ptr;
	unsigned char *endPoly = ptr + header->cb;
	
	ptr += sizeof (TTPOLYGONHEADER);
	
	_cairo_path_fixed_move_to (path,
				   _cairo_fixed_from_FIXED (header->pfxStart.x),
				   _cairo_fixed_from_FIXED (header->pfxStart.y));

	while (ptr < endPoly) {
	    TTPOLYCURVE *curve = (TTPOLYCURVE *)ptr;
	    POINTFX *points = curve->apfx;
	    int i;
	    switch (curve->wType) {
	    case TT_PRIM_LINE:
		for (i = 0; i < curve->cpfx; i++) {
		    _cairo_path_fixed_line_to (path,
					       _cairo_fixed_from_FIXED (points[i].x),
					       _cairo_fixed_from_FIXED (points[i].y));
		}
		break;
	    case TT_PRIM_QSPLINE:
		for (i = 0; i < curve->cpfx - 1; i++) {
		    cairo_fixed_t p1x, p1y, p2x, p2y, cx, cy, c1x, c1y, c2x, c2y;
		    _cairo_path_fixed_get_current_point (path, &p1x, &p1y);
		    cx = _cairo_fixed_from_FIXED (points[i].x);
		    cy = _cairo_fixed_from_FIXED (points[i].y);
		    
		    if (i + 1 == curve->cpfx - 1) {
			p2x = _cairo_fixed_from_FIXED (points[i + 1].x);
			p2y = _cairo_fixed_from_FIXED (points[i + 1].y);
		    } else {
			/* records with more than one curve use interpolation for
			   control points, per http://support.microsoft.com/kb/q87115/ */
			p2x = (cx + _cairo_fixed_from_FIXED (points[i + 1].x)) / 2;
			p2y = (cy + _cairo_fixed_from_FIXED (points[i + 1].y)) / 2;
		    }
		    
		    c1x = 2 * cx / 3 + p1x / 3;
		    c1y = 2 * cy / 3 + p1y / 3;
		    c2x = 2 * cx / 3 + p2x / 3;
		    c2y = 2 * cy / 3 + p2y / 3;
		    
		    _cairo_path_fixed_curve_to (path, c1x, c1y, c2x, c2y, p2x, p2y);
		}
		break;
	    case TT_PRIM_CSPLINE:
		for (i = 0; i < curve->cpfx - 2; i += 2) {
		    _cairo_path_fixed_curve_to (path,
						_cairo_fixed_from_FIXED (points[i].x),
						_cairo_fixed_from_FIXED (points[i].y),
						_cairo_fixed_from_FIXED (points[i + 1].x),
						_cairo_fixed_from_FIXED (points[i + 1].y),
						_cairo_fixed_from_FIXED (points[i + 2].x),
						_cairo_fixed_from_FIXED (points[i + 2].y));
		}
		break;
	    }
	    ptr += sizeof(TTPOLYCURVE) + sizeof (POINTFX) * (curve->cpfx - 1);
	}
	_cairo_path_fixed_close_path (path);
    }
    free(buffer);

CLEANUP_FONT:

    _cairo_scaled_glyph_set_path (scaled_glyph,
				  &scaled_font->base,
				  path);

    cairo_win32_scaled_font_done_font (&scaled_font->base);

 CLEANUP_PATH:

    if (status != CAIRO_STATUS_SUCCESS)
	_cairo_path_fixed_destroy (path);

    return status;
}

const cairo_scaled_font_backend_t cairo_win32_scaled_font_backend = {
    CAIRO_FONT_TYPE_WIN32,
    _cairo_win32_scaled_font_create_toy,
    _cairo_win32_scaled_font_fini,
    _cairo_win32_scaled_font_glyph_init,
    _cairo_win32_scaled_font_text_to_glyphs,
    NULL,			/* ucs4_to_index */
    _cairo_win32_scaled_font_show_glyphs,
};

/* cairo_win32_font_face_t */

typedef struct _cairo_win32_font_face cairo_win32_font_face_t;

struct _cairo_win32_font_face {
    cairo_font_face_t base;
    LOGFONTW logfont;
    HFONT hfont;
};

/* implement the platform-specific interface */

static void
_cairo_win32_font_face_destroy (void *abstract_face)
{
}

static cairo_status_t
_cairo_win32_font_face_scaled_font_create (void			*abstract_face,
					   const cairo_matrix_t	*font_matrix,
					   const cairo_matrix_t	*ctm,
					   const cairo_font_options_t *options,
					   cairo_scaled_font_t **font)
{
    cairo_win32_font_face_t *font_face = abstract_face;

    *font = _win32_scaled_font_create (&font_face->logfont,
				       font_face->hfont,
				       &font_face->base,
				       font_matrix, ctm, options);
    if (*font)
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_STATUS_NO_MEMORY;
}

static const cairo_font_face_backend_t _cairo_win32_font_face_backend = {
    CAIRO_FONT_TYPE_WIN32,
    _cairo_win32_font_face_destroy,
    _cairo_win32_font_face_scaled_font_create
};

/**
 * cairo_win32_font_face_create_for_logfontw:
 * @logfont: A #LOGFONTW structure specifying the font to use.
 *   The lfHeight, lfWidth, lfOrientation and lfEscapement
 *   fields of this structure are ignored.
 * 
 * Creates a new font for the Win32 font backend based on a
 * #LOGFONT. This font can then be used with
 * cairo_set_font_face() or cairo_font_create(). The #cairo_scaled_font_t
 * returned from cairo_font_create() is also for the Win32 backend
 * and can be used with functions such as cairo_win32_scaled_font_select_font().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 **/
cairo_font_face_t *
cairo_win32_font_face_create_for_logfontw (LOGFONTW *logfont)
{
    cairo_win32_font_face_t *font_face;

    font_face = malloc (sizeof (cairo_win32_font_face_t));
    if (!font_face) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_font_face_t *)&_cairo_font_face_nil;
    }
    
    font_face->logfont = *logfont;
    font_face->hfont = NULL;
    
    _cairo_font_face_init (&font_face->base, &_cairo_win32_font_face_backend);

    return &font_face->base;
}


cairo_font_face_t *
cairo_win32_font_face_create_for_hfont (HFONT font)
{
    cairo_win32_font_face_t *font_face;

    font_face = malloc (sizeof (cairo_win32_font_face_t));
    if (!font_face) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_font_face_t *)&_cairo_font_face_nil;
    }
    
    font_face->hfont = font;
    
    _cairo_font_face_init (&font_face->base, &_cairo_win32_font_face_backend);

    return &font_face->base;
}

/**
 * cairo_win32_scaled_font_select_font:
 * @scaled_font: A #cairo_scaled_font_t from the Win32 font backend. Such an
 *   object can be created with cairo_win32_scaled_font_create_for_logfontw().
 * @hdc: a device context
 *
 * Selects the font into the given device context and changes the
 * map mode and world transformation of the device context to match
 * that of the font. This function is intended for use when using
 * layout APIs such as Uniscribe to do text layout with the
 * cairo font. After finishing using the device context, you must call
 * cairo_win32_scaled_font_done_font() to release any resources allocated
 * by this function.
 *
 * See cairo_win32_scaled_font_get_metrics_factor() for converting logical
 * coordinates from the device context to font space.
 *
 * Normally, calls to SaveDC() and RestoreDC() would be made around
 * the use of this function to preserve the original graphics state.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if the operation succeeded.
 *   otherwise an error such as %CAIRO_STATUS_NO_MEMORY and
 *   the device context is unchanged.
 **/
cairo_status_t
cairo_win32_scaled_font_select_font (cairo_scaled_font_t *scaled_font,
				     HDC                  hdc)
{
    cairo_status_t status;
    HFONT hfont;
    HFONT old_hfont = NULL;
    int old_mode;

    if (scaled_font->status)
	return scaled_font->status;

    hfont = _win32_scaled_font_get_scaled_hfont ((cairo_win32_scaled_font_t *)scaled_font);
    if (!hfont)
	return CAIRO_STATUS_NO_MEMORY;

    old_hfont = SelectObject (hdc, hfont);
    // FIXME: This error is occurring and killing all subsequent text drawing.
    // Probably my fault, but comment it out until we're certain.
    //if (!old_hfont)
    //	return _cairo_win32_print_gdi_error ("cairo_win32_scaled_font_select_font:SelectObject");

    old_mode = SetGraphicsMode (hdc, GM_ADVANCED);
    if (!old_mode) {
	status = _cairo_win32_print_gdi_error ("cairo_win32_scaled_font_select_font:SetGraphicsMode");
	SelectObject (hdc, old_hfont);
	return status;
    }

    status = _win32_scaled_font_set_world_transform ((cairo_win32_scaled_font_t *)scaled_font, hdc);
    if (status) {
	SetGraphicsMode (hdc, old_mode);
	SelectObject (hdc, old_hfont);
	return status;
    }

    SetMapMode (hdc, MM_TEXT);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_win32_scaled_font_done_font:
 * @scaled_font: A #cairo_scaled_font_t from the Win32 font backend.
 * 
 * Releases any resources allocated by cairo_win32_scaled_font_select_font()
 **/
void
cairo_win32_scaled_font_done_font (cairo_scaled_font_t *scaled_font)
{
}

/**
 * cairo_win32_scaled_font_get_metrics_factor:
 * @scaled_font: a #cairo_scaled_font_t from the Win32 font backend
 * 
 * Gets a scale factor between logical coordinates in the coordinate
 * space used by cairo_win32_scaled_font_select_font() (that is, the
 * coordinate system used by the Windows functions to return metrics) and
 * font space coordinates.
 * 
 * Return value: factor to multiply logical units by to get font space
 *               coordinates.
 **/
double
cairo_win32_scaled_font_get_metrics_factor (cairo_scaled_font_t *scaled_font)
{
    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_WIN32)
	return 1.0;

    return 1. / ((cairo_win32_scaled_font_t *)scaled_font)->logical_scale;
}

void
cairo_win32_scaled_font_get_logical_to_device (cairo_scaled_font_t *scaled_font,
					       cairo_matrix_t *logical_to_device)
{
    cairo_win32_scaled_font_t *win_font = (cairo_win32_scaled_font_t *)scaled_font;
    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_WIN32)
	return;

    logical_to_device->xx = win_font->logical_to_device.xx;
    logical_to_device->yx = win_font->logical_to_device.yx;
    logical_to_device->xy = win_font->logical_to_device.xy;
    logical_to_device->yy = win_font->logical_to_device.yy;
    logical_to_device->x0 = win_font->logical_to_device.x0;
    logical_to_device->y0 = win_font->logical_to_device.y0;
}

void
cairo_win32_scaled_font_get_device_to_logical (cairo_scaled_font_t *scaled_font,
					       cairo_matrix_t *device_to_logical)
{
    cairo_win32_scaled_font_t *win_font = (cairo_win32_scaled_font_t *)scaled_font;
    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_WIN32)
	return;

    device_to_logical->xx = win_font->device_to_logical.xx;
    device_to_logical->yx = win_font->device_to_logical.yx;
    device_to_logical->xy = win_font->device_to_logical.xy;
    device_to_logical->yy = win_font->device_to_logical.yy;
    device_to_logical->x0 = win_font->device_to_logical.x0;
    device_to_logical->y0 = win_font->device_to_logical.y0;
}
