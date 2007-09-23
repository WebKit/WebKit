/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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

#include "FontData.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include <cairo.h>

namespace WebCore {

void Font::drawGlyphs(GraphicsContext* graphicsContext, const FontData* font, const GlyphBuffer& glyphBuffer,
                      int from, int numGlyphs, const FloatPoint& point) const
{
    cairo_t* context = graphicsContext->platformContext();

    // Set the text color to use for drawing.
    float red, green, blue, alpha;
    Color penColor = graphicsContext->fillColor();
    penColor.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(context, red, green, blue, alpha);

    // This was commented out as it made "some text invisible" but seems to work now.
    font->setFont(context);

    GlyphBufferGlyph* glyphs = (GlyphBufferGlyph*) glyphBuffer.glyphs(from);

    float offset = point.x();

    for (int i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = point.y();
        offset += glyphBuffer.advanceAt(from + i);
    }
    cairo_show_glyphs(context, glyphs, numGlyphs);
}

void Font::drawComplexText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&, int from, int to) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun&, const TextStyle&) const
{
    notImplemented();
    return 0.0f;
}

int Font::offsetForPositionForComplexText(const TextRun&, const TextStyle&, int, bool) const
{
    notImplemented();
    return 0;
}

FloatRect Font::selectionRectForComplexText(const TextRun&, const TextStyle&, const IntPoint&, int, int, int) const
{
    notImplemented();
    return FloatRect();
}

}
