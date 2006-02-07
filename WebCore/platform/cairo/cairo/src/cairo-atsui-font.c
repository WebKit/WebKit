/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Calum Robinson
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
 * The Initial Developer of the Original Code is Calum Robinson
 *
 * Contributor(s):
 *  Calum Robinson <calumr@mac.com>
 */

#include <stdlib.h>
#include <math.h>
#include "cairo-atsui.h"
#include "cairoint.h"
#include "cairo.h"
#include "cairo-quartz-private.h"

/*
 * FixedToFloat/FloatToFixed are 10.3+ SDK items - include definitions
 * here so we can use older SDKs.
 */
#ifndef FixedToFloat
#define fixed1              ((Fixed) 0x00010000L)
#define FixedToFloat(a)     ((float)(a) / fixed1)
#define FloatToFixed(a)     ((Fixed)((float)(a) * fixed1))
#endif

typedef struct _cairo_atsui_font_face cairo_atsui_font_face_t;
typedef struct _cairo_atsui_font cairo_atsui_font_t;

static cairo_status_t _cairo_atsui_font_create_scaled (cairo_font_face_t *font_face,
						       ATSUFontID font_id,
						       ATSUStyle style,
						       const cairo_matrix_t *font_matrix,
						       const cairo_matrix_t *ctm,
						       const cairo_font_options_t *options,
						       cairo_scaled_font_t **font_out);

struct _cairo_atsui_font {
    cairo_scaled_font_t base;

    ATSUStyle style;
    ATSUStyle unscaled_style;
    ATSUFontID fontID;
};


struct _cairo_atsui_font_face {
  cairo_font_face_t base;
  ATSUFontID font_id;
};

static void
_cairo_atsui_font_face_destroy (void *abstract_face)
{
}

static cairo_status_t
_cairo_atsui_font_face_scaled_font_create (void	*abstract_face,
					   const cairo_matrix_t	*font_matrix,
					   const cairo_matrix_t	*ctm,
					   const cairo_font_options_t *options,
					   cairo_scaled_font_t **font)
{
    cairo_atsui_font_face_t *font_face = abstract_face;
    OSStatus err;
    ATSUAttributeTag styleTags[] = { kATSUFontTag };
    ATSUAttributeValuePtr styleValues[] = { &font_face->font_id };
    ByteCount styleSizes[] = {  sizeof(ATSUFontID) };
    ATSUStyle style;

    err = ATSUCreateStyle (&style);
    err = ATSUSetAttributes(style,
                            sizeof(styleTags) / sizeof(styleTags[0]),
                            styleTags, styleSizes, styleValues);

    return _cairo_atsui_font_create_scaled (&font_face->base, font_face->font_id, style,
					    font_matrix, ctm, options, font);
}

static const cairo_font_face_backend_t _cairo_atsui_font_face_backend = {
    _cairo_atsui_font_face_destroy,
    _cairo_atsui_font_face_scaled_font_create
};

cairo_public cairo_font_face_t *
cairo_atsui_font_face_create_for_atsu_font_id (ATSUFontID font_id)
{
  cairo_atsui_font_face_t *font_face;

  font_face = malloc (sizeof (cairo_atsui_font_face_t));
  if (!font_face) {
    _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return (cairo_font_face_t *)&_cairo_font_face_nil;
  }

  font_face->font_id = font_id;

    _cairo_font_face_init (&font_face->base, &_cairo_atsui_font_face_backend);

    return &font_face->base;
}



static CGAffineTransform
CGAffineTransformMakeWithCairoFontScale(const cairo_matrix_t *scale)
{
    return CGAffineTransformMake(scale->xx, scale->yx,
                                 scale->xy, scale->yy,
                                 0, 0);
}

static ATSUStyle
CreateSizedCopyOfStyle(ATSUStyle inStyle, const cairo_matrix_t *scale)
{
    ATSUStyle style;
    OSStatus err;

    // Set the style's size
    CGAffineTransform theTransform =
        CGAffineTransformMakeWithCairoFontScale(scale);
    Fixed theSize =
        FloatToFixed(CGSizeApplyAffineTransform
                     (CGSizeMake(1.0, 1.0), theTransform).height);
    const ATSUAttributeTag theFontStyleTags[] = { kATSUSizeTag };
    const ByteCount theFontStyleSizes[] = { sizeof(Fixed) };
    ATSUAttributeValuePtr theFontStyleValues[] = { &theSize };

    err = ATSUCreateAndCopyStyle(inStyle, &style);

    err = ATSUSetAttributes(style,
                            sizeof(theFontStyleTags) /
                            sizeof(ATSUAttributeTag), theFontStyleTags,
                            theFontStyleSizes, theFontStyleValues);

    return style;
}

static cairo_status_t
_cairo_atsui_font_set_metrics (cairo_atsui_font_t *font)
{
    ATSFontRef atsFont;
    ATSFontMetrics metrics;
    OSStatus err;

    atsFont = FMGetATSFontRefFromFont(font->fontID);

    if (atsFont) {
        err =
            ATSFontGetHorizontalMetrics(atsFont, kATSOptionFlagsDefault,
                                        &metrics);

        if (err == noErr) {
	    	cairo_font_extents_t extents;
	    
            extents.ascent = metrics.ascent;
            extents.descent = -metrics.descent;
            extents.height = metrics.capHeight;
            extents.max_x_advance = metrics.maxAdvanceWidth;

            // The FT backend doesn't handle max_y_advance either, so we'll ignore it for now. 
            extents.max_y_advance = 0.0;

	    	_cairo_scaled_font_set_metrics (&font->base, &extents);

            return CAIRO_STATUS_SUCCESS;
        }
    }

    return CAIRO_STATUS_NULL_POINTER;
}

static cairo_status_t
_cairo_atsui_font_create_scaled (cairo_font_face_t *font_face,
				 ATSUFontID font_id,
				 ATSUStyle style,
				 const cairo_matrix_t *font_matrix,
				 const cairo_matrix_t *ctm,
				 const cairo_font_options_t *options,
				 cairo_scaled_font_t **font_out)
{
    cairo_atsui_font_t *font = NULL;
    OSStatus err;
    cairo_status_t status;

    font = malloc(sizeof(cairo_atsui_font_t));

    _cairo_scaled_font_init(&font->base, font_face, font_matrix, ctm, options,
			    &cairo_atsui_scaled_font_backend);

    font->style = CreateSizedCopyOfStyle(style, &font->base.scale);

    Fixed theSize = FloatToFixed(1.0);
    const ATSUAttributeTag theFontStyleTags[] = { kATSUSizeTag };
    const ByteCount theFontStyleSizes[] = { sizeof(Fixed) };
    ATSUAttributeValuePtr theFontStyleValues[] = { &theSize };
    err = ATSUSetAttributes(style,
                            sizeof(theFontStyleTags) /
                            sizeof(ATSUAttributeTag), theFontStyleTags,
                            theFontStyleSizes, theFontStyleValues);

    font->unscaled_style = style;

    font->fontID = font_id;

    *font_out = &font->base;

    status = _cairo_atsui_font_set_metrics (font);
    if (status) {
	cairo_scaled_font_destroy (&font->base);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_atsui_font_create_toy(cairo_toy_font_face_t *toy_face,
			     const cairo_matrix_t *font_matrix,
			     const cairo_matrix_t *ctm,
			     const cairo_font_options_t *options,
			     cairo_scaled_font_t **font_out)
{
    ATSUStyle style;
    ATSUFontID fontID;
    OSStatus err;
    Boolean isItalic, isBold;
    const char *family = toy_face->family;

    err = ATSUCreateStyle(&style);

    switch (toy_face->weight) {
    case CAIRO_FONT_WEIGHT_BOLD:
        isBold = true;
        break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
        isBold = false;
        break;
    }

    switch (toy_face->slant) {
    case CAIRO_FONT_SLANT_ITALIC:
        isItalic = true;
        break;
    case CAIRO_FONT_SLANT_OBLIQUE:
        isItalic = false;
        break;
    case CAIRO_FONT_SLANT_NORMAL:
    default:
        isItalic = false;
        break;
    }

    err = ATSUFindFontFromName(family, strlen(family),
                               kFontFamilyName,
                               kFontNoPlatformCode,
                               kFontRomanScript,
                               kFontNoLanguageCode, &fontID);

    if (err != noErr) {
	// couldn't get the font - remap css names and try again

	if (!strcmp(family, "serif"))
	    family = "Times";
	else if (!strcmp(family, "sans-serif"))
	    family = "Helvetica";
	else if (!strcmp(family, "cursive"))
	    family = "Apple Chancery";
	else if (!strcmp(family, "fantasy"))
	    family = "Gadget";
	else if (!strcmp(family, "monospace"))
	    family = "Courier";
	else // anything else - return error instead?
	    family = "Courier";

	err = ATSUFindFontFromName(family, strlen(family),
				   kFontFamilyName,
				   kFontNoPlatformCode,
				   kFontRomanScript,
				   kFontNoLanguageCode, &fontID);
    }

    ATSUAttributeTag styleTags[] =
        { kATSUQDItalicTag, kATSUQDBoldfaceTag, kATSUFontTag };
    ATSUAttributeValuePtr styleValues[] = { &isItalic, &isBold, &fontID };
    ByteCount styleSizes[] =
        { sizeof(Boolean), sizeof(Boolean), sizeof(ATSUFontID) };

    err = ATSUSetAttributes(style,
                            sizeof(styleTags) / sizeof(styleTags[0]),
                            styleTags, styleSizes, styleValues);


    return _cairo_atsui_font_create_scaled (&toy_face->base, fontID, style,
					    font_matrix, ctm, options, font_out);
}

static void
_cairo_atsui_font_fini(void *abstract_font)
{
    cairo_atsui_font_t *font = abstract_font;

    if (font == NULL)
        return;

    if (font->style)
        ATSUDisposeStyle(font->style);
    if (font->unscaled_style)
        ATSUDisposeStyle(font->unscaled_style);
}

static cairo_status_t
_cairo_atsui_font_init_glyph_metrics (cairo_atsui_font_t *font,
				      cairo_scaled_glyph_t *scaled_glyph)
{
   cairo_text_extents_t extents;
   OSStatus err;
   GlyphID theGlyph = _cairo_scaled_glyph_index (scaled_glyph);
   ATSGlyphIdealMetrics metricsH, metricsV;
   ATSUStyle style;
   ATSUVerticalCharacterType verticalType = kATSUStronglyVertical;
   const ATSUAttributeTag theTag[] = { kATSUVerticalCharacterTag };
   const ByteCount theSizes[] = { sizeof(verticalType) };
   ATSUAttributeValuePtr theValues[] = { &verticalType };

   ATSUCreateAndCopyStyle(font->unscaled_style, &style);

   err = ATSUGlyphGetIdealMetrics(style,
				  1, &theGlyph, 0, &metricsH);
   err = ATSUSetAttributes(style, 1, theTag, theSizes, theValues);
   err = ATSUGlyphGetIdealMetrics(style,
				  1, &theGlyph, 0, &metricsV);

   extents.x_bearing = metricsH.sideBearing.x;
   extents.y_bearing = metricsV.advance.y;
   extents.width = 
      metricsH.advance.x - metricsH.sideBearing.x - metricsH.otherSideBearing.x;
   extents.height = 
     -metricsV.advance.y - metricsV.sideBearing.y - metricsV.otherSideBearing.y;
   extents.x_advance = metricsH.advance.x;
   extents.y_advance = 0;

  _cairo_scaled_glyph_set_metrics (scaled_glyph,
				   &font->base,
				   &extents);

  return CAIRO_STATUS_SUCCESS;
}

static OSStatus 
_move_to (const Float32Point *point,
	  void *callback_data)
{
    cairo_path_fixed_t *path = callback_data;

    _cairo_path_fixed_close_path (path);
    _cairo_path_fixed_move_to (path,
			       _cairo_fixed_from_double(point->x),
			       _cairo_fixed_from_double(point->y));

    return noErr;
}

static OSStatus 
_line_to (const Float32Point *point,
	  void *callback_data)
{
    cairo_path_fixed_t *path = callback_data;

    _cairo_path_fixed_line_to (path,
			       _cairo_fixed_from_double(point->x),
			       _cairo_fixed_from_double(point->y));
    
    return noErr;
}

static OSStatus
_curve_to (const Float32Point *point1,
	   const Float32Point *point2,
	   const Float32Point *point3,
	   void *callback_data)
{
    cairo_path_fixed_t *path = callback_data;

    _cairo_path_fixed_curve_to (path,
				_cairo_fixed_from_double(point1->x),
				_cairo_fixed_from_double(point1->y),
				_cairo_fixed_from_double(point2->x),
				_cairo_fixed_from_double(point2->y),
				_cairo_fixed_from_double(point3->x),
				_cairo_fixed_from_double(point3->y));
    
    return noErr;
}

static OSStatus
_close_path (void *callback_data)

{
    cairo_path_fixed_t *path = callback_data;

    _cairo_path_fixed_close_path (path);

    return noErr;
}

static cairo_status_t 
_cairo_atsui_scaled_font_init_glyph_path (cairo_atsui_font_t *scaled_font,
					  cairo_scaled_glyph_t *scaled_glyph)
{
    static ATSCubicMoveToUPP moveProc = NULL;
    static ATSCubicLineToUPP lineProc = NULL;
    static ATSCubicCurveToUPP curveProc = NULL;
    static ATSCubicClosePathUPP closePathProc = NULL;
    OSStatus err;
    cairo_path_fixed_t *path;

    path = _cairo_path_fixed_create ();
    if (!path)
	return CAIRO_STATUS_NO_MEMORY;

    if (moveProc == NULL) {
        moveProc = NewATSCubicMoveToUPP(_move_to);
        lineProc = NewATSCubicLineToUPP(_line_to);
        curveProc = NewATSCubicCurveToUPP(_curve_to);
        closePathProc = NewATSCubicClosePathUPP(_close_path);
    }

    err = ATSUGlyphGetCubicPaths(scaled_font->style,
				 _cairo_scaled_glyph_index (scaled_glyph),
				 moveProc,
				 lineProc,
				 curveProc,
				 closePathProc, (void *)path, &err);

    _cairo_scaled_glyph_set_path (scaled_glyph, &scaled_font->base, path);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_atsui_font_scaled_glyph_init (void			*abstract_font,
				     cairo_scaled_glyph_t	*scaled_glyph,
				     cairo_scaled_glyph_info_t	 info)
{
    cairo_atsui_font_t *scaled_font = abstract_font;
    cairo_status_t status;

    if ((info & CAIRO_SCALED_GLYPH_INFO_METRICS) != 0) {
      status = _cairo_atsui_font_init_glyph_metrics (scaled_font, scaled_glyph);
      if (status)
	return status;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_PATH) != 0) {
	status = _cairo_atsui_scaled_font_init_glyph_path (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t 
_cairo_atsui_font_text_to_glyphs (void		*abstract_font,
				  double	 x,
				  double	 y,
				  const char	*utf8,
				  cairo_glyph_t **glyphs, 
				  int		*num_glyphs)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    uint16_t *utf16;
    int n16;
    OSStatus err;
    ATSUTextLayout textLayout;
    ATSLayoutRecord *layoutRecords;
    cairo_atsui_font_t *font = abstract_font;
    ItemCount glyphCount;
    int i;

    status = _cairo_utf8_to_utf16 ((unsigned char *)utf8, -1, &utf16, &n16);
    if (status)
	return status;

    err = ATSUCreateTextLayout(&textLayout);

    err = ATSUSetTextPointerLocation(textLayout, utf16, 0, n16, n16);

    // Set the style for all of the text
    err = ATSUSetRunStyle(textLayout,
			  font->style, kATSUFromTextBeginning, kATSUToTextEnd);

    err = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(textLayout,
							0,
							kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
							(void *)&layoutRecords,
							&glyphCount);

    *num_glyphs = glyphCount - 1;
    *glyphs = 
	(cairo_glyph_t *) malloc(*num_glyphs * (sizeof (cairo_glyph_t)));
    if (*glyphs == NULL) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    for (i = 0; i < *num_glyphs; i++) {
	(*glyphs)[i].index = layoutRecords[i].glyphID;
	(*glyphs)[i].x = x + FixedToFloat(layoutRecords[i].realPos);
	(*glyphs)[i].y = y;
    }

    free (utf16);

    ATSUDirectReleaseLayoutDataArrayPtr(NULL, 
					kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
					(void *) &layoutRecords);
    ATSUDisposeTextLayout(textLayout);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t 
_cairo_atsui_font_old_show_glyphs (void		       *abstract_font,
				   cairo_operator_t    	op,
				   cairo_pattern_t     *pattern,
				   cairo_surface_t     *generic_surface,
				   int                 	source_x,
				   int                 	source_y,
				   int			dest_x,
				   int			dest_y,
				   unsigned int		width,
				   unsigned int		height,
				   const cairo_glyph_t *glyphs,
				   int                 	num_glyphs)
{
    cairo_atsui_font_t *font = abstract_font;
    CGContextRef myBitmapContext;
    CGColorSpaceRef colorSpace;
    cairo_image_surface_t *destImageSurface;
    int i;
    void *extra = NULL;

    cairo_rectangle_t rect = {dest_x, dest_y, width, height};
    _cairo_surface_acquire_dest_image(generic_surface,
				      &rect,
				      &destImageSurface,
				      &rect,
				      &extra);

    // Create a CGBitmapContext for the dest surface for drawing into
    colorSpace = CGColorSpaceCreateDeviceRGB();

    myBitmapContext = CGBitmapContextCreate(destImageSurface->data,
                                            destImageSurface->width,
                                            destImageSurface->height,
                                            destImageSurface->depth / 4,
                                            destImageSurface->stride,
                                            colorSpace,
                                            kCGImageAlphaPremultipliedFirst);
    CGContextTranslateCTM(myBitmapContext, 0, destImageSurface->height);
    CGContextScaleCTM(myBitmapContext, 1.0f, -1.0f);

    ATSFontRef atsFont = FMGetATSFontRefFromFont(font->fontID);
    CGFontRef cgFont = CGFontCreateWithPlatformFont(&atsFont);

    CGContextSetFont(myBitmapContext, cgFont);

    CGAffineTransform textTransform =
        CGAffineTransformMakeWithCairoFontScale(&font->base.scale);

    textTransform = CGAffineTransformScale(textTransform, 1.0f, -1.0f);

    CGContextSetFontSize(myBitmapContext, 1.0);
    CGContextSetTextMatrix(myBitmapContext, textTransform);

    if (pattern->type == CAIRO_PATTERN_SOLID &&
	_cairo_pattern_is_opaque_solid(pattern))
    {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *)pattern;
	CGContextSetRGBFillColor(myBitmapContext,
				 solid->color.red,
				 solid->color.green,
				 solid->color.blue, 1.0f);
    } else {
	CGContextSetRGBFillColor(myBitmapContext, 0.0f, 0.0f, 0.0f, 0.0f);
    }

	if (_cairo_surface_is_quartz (generic_surface)) {
		cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *)generic_surface;
		if (surface->clip_region) {
			pixman_box16_t *boxes = pixman_region_rects (surface->clip_region);
			int num_boxes = pixman_region_num_rects (surface->clip_region);
			CGRect stack_rects[10];
			CGRect *rects;
			int i;
			
			if (num_boxes > 10)
				rects = malloc (sizeof (CGRect) * num_boxes);
			else
				rects = stack_rects;
				
			for (i = 0; i < num_boxes; i++) {
				rects[i].origin.x = boxes[i].x1;
				rects[i].origin.y = boxes[i].y1;
				rects[i].size.width = boxes[i].x2 - boxes[i].x1;
				rects[i].size.height = boxes[i].y2 - boxes[i].y1;
			}
			
			CGContextClipToRects (myBitmapContext, rects, num_boxes);
			
			if (rects != stack_rects)
				free(rects);
		}
	} else {
		/* XXX: Need to get the text clipped */
	}
	
    // TODO - bold and italic text
    //
    // We could draw the text using ATSUI and get bold, italics
    // etc. for free, but ATSUI does a lot of text layout work
    // that we don't really need...

	
    for (i = 0; i < num_glyphs; i++) {
        CGGlyph theGlyph = glyphs[i].index;
		
        CGContextShowGlyphsAtPoint(myBitmapContext,
				   glyphs[i].x,
                                   glyphs[i].y,
                                   &theGlyph, 1);
    }


    CGColorSpaceRelease(colorSpace);
    CGContextRelease(myBitmapContext);

    _cairo_surface_release_dest_image(generic_surface,
				      &rect,
				      destImageSurface,
				      &rect,
				      extra);

    return CAIRO_STATUS_SUCCESS;
}

const cairo_scaled_font_backend_t cairo_atsui_scaled_font_backend = {
    _cairo_atsui_font_create_toy,
    _cairo_atsui_font_fini,
    _cairo_atsui_font_scaled_glyph_init,
    _cairo_atsui_font_text_to_glyphs,
    NULL, /* ucs4_to_index */
    _cairo_atsui_font_old_show_glyphs,
};

