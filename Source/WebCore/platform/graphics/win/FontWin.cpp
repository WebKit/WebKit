/*
 * Copyright (C) 2006-2008, 2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FontCascade.h"

#include "Font.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LayoutRect.h"
#include "Logging.h"
#include "TextRun.h"
#include "UniscribeController.h"
#include <wtf/MathExtras.h>


namespace WebCore {

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

void FontCascade::adjustSelectionRectForComplexText(const TextRun& run, LayoutRect& selectionRect, unsigned from, unsigned to) const
{
    UniscribeController it(this, run);
    it.advance(from);
    float beforeWidth = it.runWidthSoFar();
    it.advance(to);
    float afterWidth = it.runWidthSoFar();

    if (run.rtl()) {
        it.advance(run.length());
        selectionRect.move(it.runWidthSoFar() - afterWidth, 0);
    } else
        selectionRect.move(beforeWidth, 0);
    selectionRect.setWidth(afterWidth - beforeWidth);
}

float FontCascade::getGlyphsAndAdvancesForComplexText(const TextRun& run, unsigned from, unsigned to, GlyphBuffer& glyphBuffer, ForTextEmphasisOrNot forTextEmphasis) const
{
    if (forTextEmphasis) {
        // FIXME: Add forTextEmphasis paremeter to UniscribeController and use it.
        LOG_ERROR("Not implemented for text emphasis.");
        return 0;
    }

    UniscribeController controller(this, run);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to, &glyphBuffer);

    if (glyphBuffer.isEmpty())
        return 0;

    float afterWidth = controller.runWidthSoFar();

    if (run.rtl()) {
        controller.advance(run.length());
        return controller.runWidthSoFar() - afterWidth;
    }
    return beforeWidth;
}

float FontCascade::floatWidthForComplexText(const TextRun& run, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    UniscribeController controller(this, run, fallbackFonts);
    controller.advance(run.length());
    if (glyphOverflow) {
        glyphOverflow->top = std::max<int>(glyphOverflow->top, ceilf(-controller.minGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().ascent()));
        glyphOverflow->bottom = std::max<int>(glyphOverflow->bottom, ceilf(controller.maxGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0  : fontMetrics().descent()));
        glyphOverflow->left = std::max<int>(0, ceilf(-controller.minGlyphBoundingBoxX()));
        glyphOverflow->right = std::max<int>(0, ceilf(controller.maxGlyphBoundingBoxX() - controller.runWidthSoFar()));
    }
    return controller.runWidthSoFar();
}

int FontCascade::offsetForPositionForComplexText(const TextRun& run, float xFloat, bool includePartialGlyphs) const
{
    // FIXME: This truncation is not a problem for HTML, but only affects SVG, which passes floating-point numbers
    // to FontCascade::offsetForPosition(). Bug http://webkit.org/b/40673 tracks fixing this problem.
    int x = static_cast<int>(xFloat);

    UniscribeController controller(this, run);
    return controller.offsetForPosition(x, includePartialGlyphs);
}

}
