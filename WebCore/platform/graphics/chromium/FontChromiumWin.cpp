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

#include "ChromiumBridge.h"
#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"
#include "SkiaFontWin.h"
#include "SkiaUtils.h"
#include "TransparencyWin.h"
#include "UniscribeHelperTextRun.h"

#include "skia/ext/platform_canvas_win.h"
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
    ~TransparencyAwareFontPainter();

protected:
    // Called by our subclass' constructor to initialize GDI if necessary. This
    // is a separate function so it can be called after the subclass finishes
    // construction (it calls virtual functions).
    void init();

    virtual IntRect estimateTextBounds() = 0;

    // Use the context from the transparency helper when drawing with GDI. It
    // may point to a temporary one.
    GraphicsContext* m_graphicsContext;
    PlatformGraphicsContext* m_platformContext;

    FloatPoint m_point;

    // Set when Windows can handle the type of drawing we're doing.
    bool m_useGDI;

    // These members are valid only when m_useGDI is set.
    HDC m_hdc;
    TransparencyWin m_transparency;

private:
    // Call when we're using GDI mode to initialize the TransparencyWin to help
    // us draw GDI text.
    void initializeForGDI();

    bool m_createdTransparencyLayer;  // We created a layer to give the font some alpha.
};

TransparencyAwareFontPainter::TransparencyAwareFontPainter(GraphicsContext* context,
                                                           const FloatPoint& point)
    : m_graphicsContext(context)
    , m_platformContext(context->platformContext())
    , m_point(point)
    , m_useGDI(windowsCanHandleTextDrawing(context))
    , m_hdc(0)
    , m_createdTransparencyLayer(false)
{
}

void TransparencyAwareFontPainter::init()
{
    if (m_useGDI)
        initializeForGDI();
}

void TransparencyAwareFontPainter::initializeForGDI()
{
    SkColor color = m_platformContext->effectiveFillColor();
    if (SkColorGetA(color) != 0xFF) {
        // When the font has some transparency, apply it by creating a new
        // transparency layer with that opacity applied.
        m_createdTransparencyLayer = true;
        m_graphicsContext->beginTransparencyLayer(SkColorGetA(color) / 255.0f);
        // The color should be opaque now.
        color = SkColorSetRGB(SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
    }

    TransparencyWin::LayerMode layerMode;
    IntRect layerRect;
    if (m_platformContext->isDrawingToImageBuffer()) {
        // Assume if we're drawing to an image buffer that the background
        // is not opaque and we have to undo ClearType. We may want to
        // enhance this to actually check, since it will often be opaque
        // and we could do ClearType in that case.
        layerMode = TransparencyWin::TextComposite;
        layerRect = estimateTextBounds();

        // The transparency helper requires that we draw text in black in
        // this mode and it will apply the color.
        m_transparency.setTextCompositeColor(color);
        color = SkColorSetRGB(0, 0, 0);
    } else if (canvasHasMultipleLayers(m_platformContext->canvas())) {
        // When we're drawing a web page, we know the background is opaque,
        // but if we're drawing to a layer, we still need extra work.
        layerMode = TransparencyWin::OpaqueCompositeLayer;
        layerRect = estimateTextBounds();
    } else {
        // Common case of drawing onto the bottom layer of a web page: we
        // know everything is opaque so don't need to do anything special.
        layerMode = TransparencyWin::NoLayer;
    }
    m_transparency.init(m_graphicsContext, layerMode, TransparencyWin::KeepTransform, layerRect);

    // Set up the DC, using the one from the transparency helper.
    m_hdc = m_transparency.platformContext()->canvas()->beginPlatformPaint();
    SetTextColor(m_hdc, skia::SkColorToCOLORREF(color));
    SetBkMode(m_hdc, TRANSPARENT);
}

TransparencyAwareFontPainter::~TransparencyAwareFontPainter()
{
    if (!m_useGDI)
        return;  // Nothing to do.
    m_transparency.composite();
    if (m_createdTransparencyLayer)
        m_graphicsContext->endTransparencyLayer();
    m_platformContext->canvas()->endPlatformPaint();
}

// Specialization for simple GlyphBuffer painting.
class TransparencyAwareGlyphPainter : public TransparencyAwareFontPainter {
 public:
    TransparencyAwareGlyphPainter(GraphicsContext*,
                                  const SimpleFontData*,
                                  const GlyphBuffer&,
                                  int from, int numGlyphs,
                                  const FloatPoint&);
    ~TransparencyAwareGlyphPainter();

    // Draws the partial string of glyphs, starting at |startAdvance| to the
    // left of m_point. We express it this way so that if we're using the Skia
    // drawing path we can use floating-point positioning, even though we have
    // to use integer positioning in the GDI path.
    bool drawGlyphs(int numGlyphs, const WORD* glyphs, const int* advances, int startAdvance) const;

 private:
    virtual IntRect estimateTextBounds();

    const SimpleFontData* m_font;
    const GlyphBuffer& m_glyphBuffer;
    int m_from;
    int m_numGlyphs;

    // When m_useGdi is set, this stores the previous HFONT selected into the
    // m_hdc so we can restore it.
    HGDIOBJ m_oldFont;  // For restoring the DC to its original state.
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
    , m_oldFont(0)
{
    init();

    m_oldFont = ::SelectObject(m_hdc, m_font->platformData().hfont());
}

TransparencyAwareGlyphPainter::~TransparencyAwareGlyphPainter()
{
    if (m_useGDI)
        ::SelectObject(m_hdc, m_oldFont);
}


// Estimates the bounding box of the given text. This is copied from
// FontCGWin.cpp, it is possible, but a lot more work, to get the precide
// bounds.
IntRect TransparencyAwareGlyphPainter::estimateTextBounds()
{
    int totalWidth = 0;
    for (int i = 0; i < m_numGlyphs; i++)
        totalWidth += lroundf(m_glyphBuffer.advanceAt(m_from + i));

    return IntRect(m_point.x() - (m_font->ascent() + m_font->descent()) / 2,
                   m_point.y() - m_font->ascent() - m_font->lineGap(),
                   totalWidth + m_font->ascent() + m_font->descent(),
                   m_font->lineSpacing()); 
}

bool TransparencyAwareGlyphPainter::drawGlyphs(int numGlyphs,
                                               const WORD* glyphs,
                                               const int* advances,
                                               int startAdvance) const
{
    if (!m_useGDI) {
        SkPoint origin = m_point;
        origin.fX += startAdvance;
        return paintSkiaText(m_graphicsContext, m_font->platformData().hfont(),
                             numGlyphs, glyphs, advances, 0, &origin);
    }

    // Windows' origin is the top-left of the bounding box, so we have
    // to subtract off the font ascent to get it.
    int x = lroundf(m_point.x() + startAdvance);
    int y = lroundf(m_point.y() - m_font->ascent());

    // If there is a non-blur shadow and both the fill color and shadow color 
    // are opaque, handle without skia. 
    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    if (m_graphicsContext->getShadow(shadowSize, shadowBlur, shadowColor)) {
        // If there is a shadow and this code is reached, windowsCanHandleDrawTextShadow()
        // will have already returned true during the ctor initiatization of m_useGDI
        ASSERT(shadowColor.alpha() == 255);
        ASSERT(m_graphicsContext->fillColor().alpha() == 255);
        ASSERT(shadowBlur == 0);
        COLORREF textColor = skia::SkColorToCOLORREF(SkColorSetARGB(255, shadowColor.red(), shadowColor.green(), shadowColor.blue()));
        COLORREF savedTextColor = GetTextColor(m_hdc);
        SetTextColor(m_hdc, textColor);
        ExtTextOut(m_hdc, x + shadowSize.width(), y + shadowSize.height(), ETO_GLYPH_INDEX, 0, reinterpret_cast<const wchar_t*>(&glyphs[0]), numGlyphs, &advances[0]);
        SetTextColor(m_hdc, savedTextColor); 
    }
    
    return !!ExtTextOut(m_hdc, x, y, ETO_GLYPH_INDEX, 0, reinterpret_cast<const wchar_t*>(&glyphs[0]), numGlyphs, &advances[0]);
}

class TransparencyAwareUniscribePainter : public TransparencyAwareFontPainter {
 public:
    TransparencyAwareUniscribePainter(GraphicsContext*,
                                      const Font*,
                                      const TextRun&,
                                      int from, int to,
                                      const FloatPoint&);
    ~TransparencyAwareUniscribePainter();

    // Uniscibe will draw directly into our buffer, so we need to expose our DC.
    HDC hdc() const { return m_hdc; }

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
    init();
}

TransparencyAwareUniscribePainter::~TransparencyAwareUniscribePainter()
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
    return IntRect(left - (m_font->ascent() + m_font->descent()) / 2,
                   m_point.y() - m_font->ascent() - m_font->lineGap(),
                   (right - left) + m_font->ascent() + m_font->descent(),
                   m_font->lineSpacing());
}

}  // namespace

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
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
    if (!alpha && graphicsContext->platformContext()->getStrokeStyle() == NoStroke)
        return;

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
    int curAdvance = 0;  // How far from the left the current chunk is.
    while (glyphIndex < numGlyphs) {
        // How many chars will be in this chunk?
        int curLen = std::min(kMaxBufferLength, numGlyphs - glyphIndex);
        glyphs.resize(curLen);
        advances.resize(curLen);

        int curWidth = 0;
        for (int i = 0; i < curLen; ++i, ++glyphIndex) {
            glyphs[i] = glyphBuffer.glyphAt(from + glyphIndex);
            advances[i] = static_cast<int>(glyphBuffer.advanceAt(from + glyphIndex));
            curWidth += advances[i];
        }

        // Actually draw the glyphs (with retry on failure).
        bool success = false;
        for (int executions = 0; executions < 2; ++executions) {
            success = painter.drawGlyphs(curLen, &glyphs[0], &advances[0], curAdvance);
            if (!success && executions == 0) {
                // Ask the browser to load the font for us and retry.
                ChromiumBridge::ensureFontLoaded(font->platformData().hfont());
                continue;
            }
            break;
        }

        ASSERT(success);
        curAdvance += curWidth;
    }
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const IntPoint& point,
                                            int h,
                                            int from,
                                            int to) const
{
    UniscribeHelperTextRun state(run, *this);
    float left = static_cast<float>(point.x() + state.characterToX(from));
    float right = static_cast<float>(point.x() + state.characterToX(to));

    // If the text is RTL, left will actually be after right.
    if (left < right)
        return FloatRect(left, static_cast<float>(point.y()),
                       right - left, static_cast<float>(h));

    return FloatRect(right, static_cast<float>(point.y()),
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

    TransparencyAwareUniscribePainter painter(graphicsContext, this, run, from, to, point);

    HDC hdc = painter.hdc();

    // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
    // Enforce non-transparent color.
    color = SkColorSetRGB(SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
    SetTextColor(hdc, skia::SkColorToCOLORREF(color));
    SetBkMode(hdc, TRANSPARENT);

    // If there is a non-blur shadow and both the fill color and shadow color 
    // are opaque, handle without skia. 
    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    if (graphicsContext->getShadow(shadowSize, shadowBlur, shadowColor) && windowsCanHandleDrawTextShadow(graphicsContext)) {
        COLORREF textColor = skia::SkColorToCOLORREF(SkColorSetARGB(255, shadowColor.red(), shadowColor.green(), shadowColor.blue()));
        COLORREF savedTextColor = GetTextColor(hdc);
        SetTextColor(hdc, textColor);
        state.draw(graphicsContext, hdc, static_cast<int>(point.x()) + shadowSize.width(),
                   static_cast<int>(point.y() - ascent()) + shadowSize.height(), from, to);
        SetTextColor(hdc, savedTextColor); 
    }

    // Uniscribe counts the coordinates from the upper left, while WebKit uses
    // the baseline, so we have to subtract off the ascent.
    state.draw(graphicsContext, hdc, static_cast<int>(point.x()),
               static_cast<int>(point.y() - ascent()), from, to);

    context->canvas()->endPlatformPaint();
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */) const
{
    UniscribeHelperTextRun state(run, *this);
    return static_cast<float>(state.width());
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x,
                                          bool includePartialGlyphs) const
{
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
