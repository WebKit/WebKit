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
#include "CairoUtilities.h"
#include "ContextShadow.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "SimpleFontData.h"

namespace WebCore {

static void prepareContextForGlyphDrawing(cairo_t* context, const SimpleFontData* font, const FloatPoint& point)
{
    static const float syntheticObliqueSkew = -tanf(14 * acosf(0) / 90);
    cairo_set_scaled_font(context, font->platformData().scaledFont());
    if (font->platformData().syntheticOblique()) {
        cairo_matrix_t mat = {1, 0, syntheticObliqueSkew, 1, point.x(), point.y()};
        cairo_transform(context, &mat);
    } else
        cairo_translate(context, point.x(), point.y());
}

static void drawGlyphsToContext(cairo_t* context, const SimpleFontData* font, GlyphBufferGlyph* glyphs, int numGlyphs)
{
    cairo_show_glyphs(context, glyphs, numGlyphs);
    if (font->syntheticBoldOffset()) {
        // We could use cairo_save/cairo_restore here, but two translations are likely faster.
        cairo_translate(context, font->syntheticBoldOffset(), 0);
        cairo_show_glyphs(context, glyphs, numGlyphs);
        cairo_translate(context, -font->syntheticBoldOffset(), 0);
    }
}

static void drawGlyphsShadow(GraphicsContext* graphicsContext, cairo_t* context, const FloatPoint& point, const SimpleFontData* font, GlyphBufferGlyph* glyphs, int numGlyphs)
{
    ContextShadow* shadow = graphicsContext->contextShadow();
    ASSERT(shadow);

    if (!(graphicsContext->textDrawingMode() & cTextFill) || shadow->m_type == ContextShadow::NoShadow)
        return;

    if (shadow->m_type == ContextShadow::SolidShadow) {
        // Optimize non-blurry shadows, by just drawing text without the ContextShadow.
        cairo_save(context);
        cairo_translate(context, shadow->m_offset.width(), shadow->m_offset.height());
        setSourceRGBAFromColor(context, shadow->m_color);
        prepareContextForGlyphDrawing(context, font, point);
        cairo_show_glyphs(context, glyphs, numGlyphs);
        cairo_restore(context);
        return;
    }

    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(font->platformData().scaledFont(), glyphs, numGlyphs, &extents);
    FloatRect fontExtentsRect(point.x(), point.y() - extents.height, extents.width, extents.height);
    cairo_t* shadowContext = shadow->beginShadowLayer(context, fontExtentsRect);
    if (shadowContext) {
        prepareContextForGlyphDrawing(shadowContext, font, point);
        drawGlyphsToContext(shadowContext, font, glyphs, numGlyphs);
        shadow->endShadowLayer(context);
    }
}

void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer,
                      int from, int numGlyphs, const FloatPoint& point) const
{
    GlyphBufferGlyph* glyphs = (GlyphBufferGlyph*)glyphBuffer.glyphs(from);

    float offset = 0.0f;
    for (int i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = 0.0f;
        offset += glyphBuffer.advanceAt(from + i);
    }

    cairo_t* cr = context->platformContext();
    drawGlyphsShadow(context, cr, point, font, glyphs, numGlyphs);

    cairo_save(cr);
    prepareContextForGlyphDrawing(cr, font, point);
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
            context->fillColor().getRGBA(red, green, blue, alpha);
            cairo_set_source_rgba(cr, red, green, blue, alpha * context->getAlpha());
        }
        drawGlyphsToContext(cr, font, glyphs, numGlyphs);
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
            float red, green, blue, alpha;
            context->strokeColor().getRGBA(red, green, blue, alpha);
            cairo_set_source_rgba(cr, red, green, blue, alpha * context->getAlpha());
        }
        cairo_glyph_path(cr, glyphs, numGlyphs);
        cairo_set_line_width(cr, context->strokeThickness());
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

}
