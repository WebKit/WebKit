/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Font.h"

#include "AffineTransform.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "SimpleFontData.h"

#define SYNTHETIC_OBLIQUE_ANGLE 14

namespace WebCore {

void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer,
                      int from, int numGlyphs, const FloatPoint& point) const
{
    cairo_t* cr = context->platformContext();
    cairo_save(cr);

    cairo_set_scaled_font(cr, font->platformData().scaledFont());

    GlyphBufferGlyph* glyphs = (GlyphBufferGlyph*)glyphBuffer.glyphs(from);

    float offset = 0.0f;
    for (int i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = 0.0f;
        offset += glyphBuffer.advanceAt(from + i);
    }

    Color fillColor = context->fillColor();

    // Synthetic Oblique
    if(font->platformData().syntheticOblique()) {
        cairo_matrix_t mat = {1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, point.x(), point.y()};
        cairo_transform(cr, &mat);
    } else {
        cairo_translate(cr, point.x(), point.y());
    }

    // Text shadow, inspired by FontMac
    FloatSize shadowOffset;
    float shadowBlur = 0;
    Color shadowColor;
    bool hasShadow = context->textDrawingMode() & cTextFill
                     && context->getShadow(shadowOffset, shadowBlur, shadowColor);

    // TODO: Blur support
    if (hasShadow) {
        // Disable graphics context shadows (not yet implemented) and paint them manually
        context->clearShadow();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        cairo_save(cr);

        float red, green, blue, alpha;
        shadowFillColor.getRGBA(red, green, blue, alpha);
        cairo_set_source_rgba(cr, red, green, blue, alpha);

#if ENABLE(FILTERS)
        cairo_text_extents_t extents;
        cairo_scaled_font_glyph_extents(font->platformData().scaledFont(), glyphs, numGlyphs, &extents);

        FloatRect rect(FloatPoint(), FloatSize(extents.width, extents.height));
        IntSize shadowBufferSize;
        FloatRect shadowRect;
        float radius = 0;
        context->calculateShadowBufferDimensions(shadowBufferSize, shadowRect, radius, rect, shadowOffset, shadowBlur);

        // Draw shadow into a new ImageBuffer
        OwnPtr<ImageBuffer> shadowBuffer = ImageBuffer::create(shadowBufferSize);
        GraphicsContext* shadowContext = shadowBuffer->context();
        cairo_t* shadowCr = shadowContext->platformContext();

        cairo_translate(shadowCr, radius, extents.height + radius);

        cairo_set_scaled_font(shadowCr, font->platformData().scaledFont());
        cairo_show_glyphs(shadowCr, glyphs, numGlyphs);
        if (font->syntheticBoldOffset()) {
            cairo_save(shadowCr);
            cairo_translate(shadowCr, font->syntheticBoldOffset(), 0);
            cairo_show_glyphs(shadowCr, glyphs, numGlyphs);
            cairo_restore(shadowCr);
        }
        cairo_translate(cr, 0.0, -extents.height);
        context->applyPlatformShadow(shadowBuffer.release(), shadowColor, shadowRect, radius);
#else
        cairo_translate(cr, shadowOffset.width(), shadowOffset.height());
        cairo_show_glyphs(cr, glyphs, numGlyphs);
        if (font->syntheticBoldOffset()) {
            cairo_save(cr);
            cairo_translate(cr, font->syntheticBoldOffset(), 0);
            cairo_show_glyphs(cr, glyphs, numGlyphs);
            cairo_restore(cr);
        }
#endif

        cairo_restore(cr);
    }

    if (context->textDrawingMode() & cTextFill) {
        if (context->fillGradient()) {
            cairo_set_source(cr, context->fillGradient()->platformGradient());
            if (context->getAlpha() < 1.0f) {
                cairo_push_group(cr);
                cairo_paint_with_alpha(cr, context->getAlpha());
                cairo_pop_group_to_source(cr);
            }
        } else if (context->fillPattern()) {
            AffineTransform affine;
            cairo_pattern_t* pattern = context->fillPattern()->createPlatformPattern(affine);
            cairo_set_source(cr, pattern);
            if (context->getAlpha() < 1.0f) {
                cairo_push_group(cr);
                cairo_paint_with_alpha(cr, context->getAlpha());
                cairo_pop_group_to_source(cr);
            }
            cairo_pattern_destroy(pattern);
        } else {
            float red, green, blue, alpha;
            fillColor.getRGBA(red, green, blue, alpha);
            cairo_set_source_rgba(cr, red, green, blue, alpha * context->getAlpha());
        }
        cairo_show_glyphs(cr, glyphs, numGlyphs);
        if (font->syntheticBoldOffset()) {
            cairo_save(cr);
            cairo_translate(cr, font->syntheticBoldOffset(), 0);
            cairo_show_glyphs(cr, glyphs, numGlyphs);
            cairo_restore(cr);
        }
    }

    // Prevent running into a long computation within cairo. If the stroke width is
    // twice the size of the width of the text we will not ask cairo to stroke
    // the text as even one single stroke would cover the full wdth of the text.
    //  See https://bugs.webkit.org/show_bug.cgi?id=33759.
    if (context->textDrawingMode() & cTextStroke && context->strokeThickness() < 2 * offset) {
        if (context->strokeGradient()) {
            cairo_set_source(cr, context->strokeGradient()->platformGradient());
            if (context->getAlpha() < 1.0f) {
                cairo_push_group(cr);
                cairo_paint_with_alpha(cr, context->getAlpha());
                cairo_pop_group_to_source(cr);
            }
        } else if (context->strokePattern()) {
            AffineTransform affine;
            cairo_pattern_t* pattern = context->strokePattern()->createPlatformPattern(affine);
            cairo_set_source(cr, pattern);
            if (context->getAlpha() < 1.0f) {
                cairo_push_group(cr);
                cairo_paint_with_alpha(cr, context->getAlpha());
                cairo_pop_group_to_source(cr);
            }
            cairo_pattern_destroy(pattern);
        } else {
            Color strokeColor = context->strokeColor();
            float red, green, blue, alpha;
            strokeColor.getRGBA(red, green, blue, alpha);
            cairo_set_source_rgba(cr, red, green, blue, alpha * context->getAlpha());
        } 
        cairo_glyph_path(cr, glyphs, numGlyphs);
        cairo_set_line_width(cr, context->strokeThickness());
        cairo_stroke(cr);
    }

    // Re-enable the platform shadow we disabled earlier
    if (hasShadow)
        context->setShadow(shadowOffset, shadowBlur, shadowColor, DeviceColorSpace);

    cairo_restore(cr);
}

}
