/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Font.h"

#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "NotImplemented.h"
#include "PlatformBridge.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"
#include "SkiaFontWin.h"
#include "SkiaUtils.h"
#include "TransparencyWin.h"
#include "UniscribeHelperTextRun.h"

#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"  // FIXME: remove this dependency.

#include <windows.h>

namespace WebCore {

namespace {

bool canvasHasMultipleLayers(const SkCanvas* canvas)
{
    SkCanvas::LayerIter iter(const_cast<SkCanvas*>(canvas), false);
    iter.next();  // There is always at least one layer.
    return !iter.done();  // There is > 1 layer if the the iterator can stil advance.
}

class TransparencyAwareFontPainter {
public:
    TransparencyAwareFontPainter(GraphicsContext*, const FloatPoint&);

protected:
    virtual IntRect estimateTextBounds() = 0;

    // Use the context from the transparency helper when drawing with GDI. It
    // may point to a temporary one.
    GraphicsContext* m_graphicsContext;
    PlatformGraphicsContext* m_platformContext;

    FloatPoint m_point;
};

TransparencyAwareFontPainter::TransparencyAwareFontPainter(GraphicsContext* context,
                                                           const FloatPoint& point)
    : m_graphicsContext(context)
    , m_platformContext(context->platformContext())
    , m_point(point)
{
}

// Specialization for simple GlyphBuffer painting.
class TransparencyAwareGlyphPainter : public TransparencyAwareFontPainter {
 public:
    TransparencyAwareGlyphPainter(GraphicsContext*,
                                  const SimpleFontData*,
                                  const GlyphBuffer&,
                                  int from, int numGlyphs,
                                  const FloatPoint&);

    // Draws the partial string of glyphs, starting at |startAdvance| to the
    // left of m_point.
    void drawGlyphs(int numGlyphs, const WORD* glyphs, const int* advances, float startAdvance) const;

 private:
    virtual IntRect estimateTextBounds();

    const SimpleFontData* m_font;
    const GlyphBuffer& m_glyphBuffer;
    int m_from;
    int m_numGlyphs;
};

TransparencyAwareGlyphPainter::TransparencyAwareGlyphPainter(
    GraphicsContext* context,
    const SimpleFontData* font,
    const GlyphBuffer& glyphBuffer,
    int from, int numGlyphs,
    const FloatPoint& point)
    : TransparencyAwareFontPainter(context, point)
    , m_font(font)
    , m_glyphBuffer(glyphBuffer)
    , m_from(from)
    , m_numGlyphs(numGlyphs)
{
}

// Estimates the bounding box of the given text. This is copied from
// FontCGWin.cpp, it is possible, but a lot more work, to get the precide
// bounds.
IntRect TransparencyAwareGlyphPainter::estimateTextBounds()
{
    int totalWidth = 0;
    for (int i = 0; i < m_numGlyphs; i++)
        totalWidth += lroundf(m_glyphBuffer.advanceAt(m_from + i));

    const FontMetrics& fontMetrics = m_font->fontMetrics();
    return IntRect(m_point.x() - (fontMetrics.ascent() + fontMetrics.descent()) / 2,
                   m_point.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
                   totalWidth + fontMetrics.ascent() + fontMetrics.descent(),
                   fontMetrics.lineSpacing()); 
}

void TransparencyAwareGlyphPainter::drawGlyphs(int numGlyphs,
                                               const WORD* glyphs,
                                               const int* advances,
                                               float startAdvance) const
{
    SkPoint origin = m_point;
    origin.fX += SkFloatToScalar(startAdvance);
    paintSkiaText(m_graphicsContext, m_font->platformData().hfont(),
                  numGlyphs, glyphs, advances, 0, &origin);
}

class TransparencyAwareUniscribePainter : public TransparencyAwareFontPainter {
 public:
    TransparencyAwareUniscribePainter(GraphicsContext*,
                                      const Font*,
                                      const TextRun&,
                                      int from, int to,
                                      const FloatPoint&);

 private:
    virtual IntRect estimateTextBounds();

    const Font* m_font;
    const TextRun& m_run;
    int m_from;
    int m_to;
};

TransparencyAwareUniscribePainter::TransparencyAwareUniscribePainter(
    GraphicsContext* context,
    const Font* font,
    const TextRun& run,
    int from, int to,
    const FloatPoint& point)
    : TransparencyAwareFontPainter(context, point)
    , m_font(font)
    , m_run(run)
    , m_from(from)
    , m_to(to)
{
}

IntRect TransparencyAwareUniscribePainter::estimateTextBounds()
{
    // This case really really sucks. There is no convenient way to estimate
    // the bounding box. So we run Uniscribe twice. If we find this happens a
    // lot, the way to fix it is to make the extra layer after the
    // UniscribeHelper has measured the text.
    IntPoint intPoint(lroundf(m_point.x()),
                      lroundf(m_point.y()));

    UniscribeHelperTextRun state(m_run, *m_font);
    int left = lroundf(m_point.x()) + state.characterToX(m_from);
    int right = lroundf(m_point.x()) + state.characterToX(m_to);
    
    // Adjust for RTL script since we just want to know the text bounds.
    if (left > right)
        std::swap(left, right);

    // This algorithm for estimating how much extra space we need (the text may
    // go outside the selection rect) is based roughly on
    // TransparencyAwareGlyphPainter::estimateTextBounds above.
    const FontMetrics& fontMetrics = m_font->fontMetrics();
    return IntRect(left - (fontMetrics.ascent() + fontMetrics.descent()) / 2,
                   m_point.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
                   (right - left) + fontMetrics.ascent() + fontMetrics.descent(),
                   fontMetrics.lineSpacing());
}

}  // namespace

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool Font::canExpandAroundIdeographsInComplexText()
{
    return false;
}

static void drawGlyphsWin(GraphicsContext* graphicsContext,
                          const SimpleFontData* font,
                          const GlyphBuffer& glyphBuffer,
                          int from,
                          int numGlyphs,
                          const FloatPoint& point) {
    TransparencyAwareGlyphPainter painter(graphicsContext, font, glyphBuffer, from, numGlyphs, point);

    // We draw the glyphs in chunks to avoid having to do a heap allocation for
    // the arrays of characters and advances. Since ExtTextOut is the
    // lowest-level text output function on Windows, there should be little
    // penalty for splitting up the text. On the other hand, the buffer cannot
    // be bigger than 4094 or the function will fail.
    const int kMaxBufferLength = 256;
    Vector<WORD, kMaxBufferLength> glyphs;
    Vector<int, kMaxBufferLength> advances;
    int glyphIndex = 0;  // The starting glyph of the current chunk.

    // In order to round all offsets to the correct pixel boundary, this code keeps track of the absolute position
    // of each glyph in floating point units and rounds to integer advances at the last possible moment.

    float horizontalOffset = point.x(); // The floating point offset of the left side of the current glyph.
    int lastHorizontalOffsetRounded = lroundf(horizontalOffset); // The rounded offset of the left side of the last glyph rendered.
    while (glyphIndex < numGlyphs) {
        // How many chars will be in this chunk?
        int curLen = std::min(kMaxBufferLength, numGlyphs - glyphIndex);
        glyphs.resize(curLen);
        advances.resize(curLen);

        float currentWidth = 0;
        for (int i = 0; i < curLen; ++i, ++glyphIndex) {
            glyphs[i] = glyphBuffer.glyphAt(from + glyphIndex);
            horizontalOffset += glyphBuffer.advanceAt(from + glyphIndex);
            advances[i] = lroundf(horizontalOffset) - lastHorizontalOffsetRounded;
            lastHorizontalOffsetRounded += advances[i];
            currentWidth += glyphBuffer.advanceAt(from + glyphIndex);
            
            // Bug 26088 - very large positive or negative runs can fail to
            // render so we clamp the size here. In the specs, negative
            // letter-spacing is implementation-defined, so this should be
            // fine, and it matches Safari's implementation. The call actually
            // seems to crash if kMaxNegativeRun is set to somewhere around
            // -32830, so we give ourselves a little breathing room.
            const int maxNegativeRun = -32768;
            const int maxPositiveRun =  32768;
            if ((currentWidth + advances[i] < maxNegativeRun) || (currentWidth + advances[i] > maxPositiveRun)) 
                advances[i] = 0;
        }

        painter.drawGlyphs(curLen, &glyphs[0], &advances[0], horizontalOffset - point.x() - currentWidth);
    }
}

void Font::drawGlyphs(GraphicsContext* graphicsContext,
                      const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,
                      int from,
                      int numGlyphs,
                      const FloatPoint& point) const
{
    SkColor color = graphicsContext->platformContext()->effectiveFillColor();
    unsigned char alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha && graphicsContext->platformContext()->getStrokeStyle() == NoStroke && !graphicsContext->hasShadow())
        return;

    drawGlyphsWin(graphicsContext, font, glyphBuffer, from, numGlyphs, point);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const FloatPoint& point,
                                            int h,
                                            int from,
                                            int to) const
{
    UniscribeHelperTextRun state(run, *this);
    float left = static_cast<float>(point.x() + state.characterToX(from));
    float right = static_cast<float>(point.x() + state.characterToX(to));

    // If the text is RTL, left will actually be after right.
    if (left < right)
        return FloatRect(left, point.y(),
                       right - left, static_cast<float>(h));

    return FloatRect(right, point.y(),
                     left - right, static_cast<float>(h));
}

void Font::drawComplexText(GraphicsContext* graphicsContext,
                           const TextRun& run,
                           const FloatPoint& point,
                           int from,
                           int to) const
{
    PlatformGraphicsContext* context = graphicsContext->platformContext();
    UniscribeHelperTextRun state(run, *this);

    SkColor color = graphicsContext->platformContext()->effectiveFillColor();
    unsigned char alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha && graphicsContext->platformContext()->getStrokeStyle() == NoStroke)
        return;

    // Uniscribe counts the coordinates from the upper left, while WebKit uses
    // the baseline, so we have to subtract off the ascent.
    HDC hdc = 0;
    state.draw(graphicsContext, hdc, lroundf(point.x()), lroundf(point.y() - fontMetrics().ascent()), from, to);
}

void Font::drawEmphasisMarksForComplexText(GraphicsContext* /* context */, const TextRun& /* run */, const AtomicString& /* mark */, const FloatPoint& /* point */, int /* from */, int /* to */) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */, GlyphOverflow* /* glyphOverflow */) const
{
    UniscribeHelperTextRun state(run, *this);
    return static_cast<float>(state.width());
}

int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
                                          bool includePartialGlyphs) const
{
    // FIXME: This truncation is not a problem for HTML, but only affects SVG, which passes floating-point numbers
    // to Font::offsetForPosition(). Bug http://webkit.org/b/40673 tracks fixing this problem.
    int x = static_cast<int>(xFloat);

    // Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.
    UniscribeHelperTextRun state(run, *this);
    int charIndex = state.xToCharacter(x);

    // XToCharacter will return -1 if the position is before the first
    // character (we get called like this sometimes).
    if (charIndex < 0)
        charIndex = 0;
    return charIndex;
}

} // namespace WebCore
