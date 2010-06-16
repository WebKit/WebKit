/*
 * Copyright (C) 2007 Kevin Ollivier.  All rights reserved.
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

#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

#if OS(WINDOWS)
#include "UniscribeController.h"
typedef WebCore::UniscribeController ComplexTextController;
#endif

#if OS(DARWIN)
#include "mac/ComplexTextController.h"
#endif

#include <wx/dcclient.h>
#include "fontprops.h"
#include "non-kerned-drawing.h"

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
#if OS(WINDOWS) || OS(DARWIN)
    return true;
#else
    return false;
#endif
}

void Font::drawGlyphs(GraphicsContext* graphicsContext, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, 
                      int from, int numGlyphs, const FloatPoint& point) const
{
    // prepare DC
    Color color = graphicsContext->fillColor();

    // We can't use wx drawing methods on Win/Linux because they automatically kern text
    // so we've created a function with platform dependent drawing implementations that
    // will hopefully be folded into wx once the API has solidified.
    // see platform/wx/wxcode/<platform> for the implementations.
    drawTextWithSpacing(graphicsContext, font, color, glyphBuffer, from, numGlyphs, point);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const FloatPoint& point, int h, int from, int to) const
{
#if OS(WINDOWS) || OS(DARWIN)
    ComplexTextController it(this, run);
    it.advance(from);
    float beforeWidth = it.runWidthSoFar();
    it.advance(to);
    float afterWidth = it.runWidthSoFar();

    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (run.rtl()) {
#if OS(WINDOWS)
        it.advance(run.length());
        float totalWidth = it.runWidthSoFar();
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
#else
        float totalWidth = it.totalWidth();
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
#endif
    } 
    
    return FloatRect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);
#else
    notImplemented();
    return FloatRect();
#endif
}

void Font::drawComplexText(GraphicsContext* context, const TextRun& run, const FloatPoint& point, int from, int to) const
{
#if OS(WINDOWS) || OS(DARWIN)
    // This glyph buffer holds our glyphs + advances + font data for each glyph.
    GlyphBuffer glyphBuffer;

    float startX = point.x();
    ComplexTextController controller(this, run);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    float afterWidth = controller.runWidthSoFar();

    if (run.rtl()) {
#if OS(WINDOWS)
        controller.advance(run.length());
        startX += controller.runWidthSoFar() - afterWidth;
#else
        startX += controller.totalWidth() + controller.finalRoundingWidth() - afterWidth;
        for (int i = 0, end = glyphBuffer.size() - 1; i < glyphBuffer.size() / 2; ++i, --end)
            glyphBuffer.swap(i, end);
#endif
    } else
        startX += beforeWidth;

    // Draw the glyph buffer now at the starting point returned in startX.
    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, glyphBuffer, run, startPoint);
#else
    notImplemented();
#endif
}


float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow*) const
{
#if OS(WINDOWS) || OS(DARWIN)
    ComplexTextController controller(this, run, fallbackFonts);
#if OS(WINDOWS)
    controller.advance(run.length());
    return controller.runWidthSoFar();
#else
    return controller.totalWidth();
#endif
#else
    notImplemented();
    return 0;
#endif
}

int Font::offsetForPositionForComplexText(const TextRun& run, float x, bool includePartialGlyphs) const
{
#if OS(WINDOWS) || OS(DARWIN)
    ComplexTextController controller(this, run);
    return controller.offsetForPosition(x, includePartialGlyphs);
#else
    notImplemented();
    return 0;
#endif
}
}
