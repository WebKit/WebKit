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

#include "TransformationMatrix.h"
#include "ChromiumBridge.h"
#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"
#include "SkiaFontWin.h"
#include "SkiaUtils.h"
#include "UniscribeHelperTextRun.h"

#include "skia/ext/platform_canvas_win.h"
#include "skia/ext/skia_utils_win.h"  // FIXME: remove this dependency.

#include <windows.h>

namespace WebCore {

static bool windowsCanHandleTextDrawing(GraphicsContext* context)
{
    // Check for non-translation transforms. Sometimes zooms will look better in
    // Skia, and sometimes better in Windows. The main problem is that zooming
    // in using Skia will show you the hinted outlines for the smaller size,
    // which look weird. All else being equal, it's better to use Windows' text
    // drawing, so we don't check for zooms.
    const TransformationMatrix& matrix = context->getCTM();
    if (matrix.b() != 0 ||  // Y skew
        matrix.c() != 0)    // X skew
        return false;

    // Check for stroke effects.
    if (context->platformContext()->getTextDrawingMode() != cTextFill)
        return false;

    // Check for shadow effects.
    if (context->platformContext()->getDrawLooper())
        return false;

    return true;
}

// Skia equivalents to Windows text drawing functions. They
// will get the outlines from Windows and draw then using Skia using the given
// parameters in the paint arguments. This allows more complex effects and
// transforms to be drawn than Windows allows.
//
// These functions will be significantly slower than Windows GDI, and the text
// will look different (no ClearType), so use only when necessary.
//
// When you call a Skia* text drawing function, various glyph outlines will be
// cached. As a result, you should call SkiaWinOutlineCache::removePathsForFont 
// when the font is destroyed so that the cache does not outlive the font (since
// the HFONTs are recycled).

// Analog of the Windows GDI function DrawText, except using the given SkPaint
// attributes for the text. See above for more.
//
// Returns true of the text was drawn successfully. False indicates an error
// from Windows.
static bool skiaDrawText(HFONT hfont,
                  SkCanvas* canvas,
                  const SkPoint& point,
                  SkPaint* paint,
                  const WORD* glyphs,
                  const int* advances,
                  int numGlyphs)
{
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, hfont);

    canvas->save();
    canvas->translate(point.fX, point.fY);

    for (int i = 0; i < numGlyphs; i++) {
        const SkPath* path = SkiaWinOutlineCache::lookupOrCreatePathForGlyph(dc, hfont, glyphs[i]);
        if (!path)
            return false;
        canvas->drawPath(*path, *paint);
        canvas->translate(advances[i], 0);
    }

    canvas->restore();

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
    return true;
}

static bool paintSkiaText(PlatformContextSkia* platformContext,
                          HFONT hfont,
                          int numGlyphs,
                          const WORD* glyphs,
                          const int* advances,
                          const SkPoint& origin)
{
    int textMode = platformContext->getTextDrawingMode();

    // Filling (if necessary). This is the common case.
    SkPaint paint;
    platformContext->setupPaintForFilling(&paint);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    bool didFill = false;
    if ((textMode & cTextFill) && SkColorGetA(paint.getColor())) {
        if (!skiaDrawText(hfont, platformContext->canvas(), origin, &paint, &glyphs[0], &advances[0], numGlyphs))
            return false;
        didFill = true;
    }

    // Stroking on top (if necessary).
    if ((textMode & WebCore::cTextStroke)
        && platformContext->getStrokeStyle() != NoStroke
        && platformContext->getStrokeThickness() > 0) {

        paint.reset();
        platformContext->setupPaintForStroking(&paint, 0, 0);
        paint.setFlags(SkPaint::kAntiAlias_Flag);
        if (didFill) {
            // If there is a shadow and we filled above, there will already be
            // a shadow. We don't want to draw it again or it will be too dark
            // and it will go on top of the fill.
            //
            // Note that this isn't strictly correct, since the stroke could be
            // very thick and the shadow wouldn't account for this. The "right"
            // thing would be to draw to a new layer and then draw that layer
            // with a shadow. But this is a lot of extra work for something
            // that isn't normally an issue.
            paint.setLooper(0)->safeUnref();
        }

        if (!skiaDrawText(hfont, platformContext->canvas(), origin, &paint, &glyphs[0], &advances[0], numGlyphs))
            return false;
    }
    return true;
}

void Font::drawGlyphs(GraphicsContext* graphicsContext,
                      const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,
                      int from,
                      int numGlyphs,
                      const FloatPoint& point) const
{
    PlatformGraphicsContext* context = graphicsContext->platformContext();

    // Max buffer length passed to the underlying windows API.
    const int kMaxBufferLength = 1024;
    // Default size for the buffer. It should be enough for most of cases.
    const int kDefaultBufferLength = 256;

    SkColor color = context->fillColor();
    unsigned char alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha && context->getStrokeStyle() == NoStroke)
        return;

    // Set up our graphics context.
    HDC hdc = context->canvas()->beginPlatformPaint();
    HGDIOBJ oldFont = SelectObject(hdc, font->platformData().hfont());

    // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
    // Enforce non-transparent color.
    color = SkColorSetRGB(SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
    SetTextColor(hdc, skia::SkColorToCOLORREF(color));
    SetBkMode(hdc, TRANSPARENT);

    // Windows needs the characters and the advances in nice contiguous
    // buffers, which we build here.
    Vector<WORD, kDefaultBufferLength> glyphs;
    Vector<int, kDefaultBufferLength> advances;

    // Compute the coordinate. The 'origin' represents the baseline, so we need
    // to move it up to the top of the bounding square.
    int x = static_cast<int>(point.x());
    int lineTop = static_cast<int>(point.y()) - font->ascent();

    bool canUseGDI = windowsCanHandleTextDrawing(graphicsContext);

    // We draw the glyphs in chunks to avoid having to do a heap allocation for
    // the arrays of characters and advances. Since ExtTextOut is the
    // lowest-level text output function on Windows, there should be little
    // penalty for splitting up the text. On the other hand, the buffer cannot
    // be bigger than 4094 or the function will fail.
    int glyphIndex = 0;
    while (glyphIndex < numGlyphs) {
        // how many chars will be in this chunk?
        int curLen = std::min(kMaxBufferLength, numGlyphs - glyphIndex);

        glyphs.resize(curLen);
        advances.resize(curLen);

        int curWidth = 0;
        for (int i = 0; i < curLen; ++i, ++glyphIndex) {
            glyphs[i] = glyphBuffer.glyphAt(from + glyphIndex);
            advances[i] = static_cast<int>(glyphBuffer.advanceAt(from + glyphIndex));
            curWidth += advances[i];
        }

        bool success = false;
        for (int executions = 0; executions < 2; ++executions) {
            if (canUseGDI)
                success = !!ExtTextOut(hdc, x, lineTop, ETO_GLYPH_INDEX, 0, reinterpret_cast<const wchar_t*>(&glyphs[0]), curLen, &advances[0]);
            else {
                // Skia's text draing origin is the baseline, like WebKit, not
                // the top, like Windows.
                SkPoint origin = { x, point.y() };
                success = paintSkiaText(context, font->platformData().hfont(), numGlyphs, reinterpret_cast<const WORD*>(&glyphs[0]), &advances[0], origin);
            }

            if (!success && executions == 0) {
                // Ask the browser to load the font for us and retry.
                ChromiumBridge::ensureFontLoaded(font->platformData().hfont());
                continue;
            }
            break;
        }

        ASSERT(success);

        x += curWidth;
    }

    SelectObject(hdc, oldFont);
    context->canvas()->endPlatformPaint();
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const IntPoint& point,
                                            int h,
                                            int from,
                                            int to) const
{
    UniscribeHelperTextRun state(run, *this);
    float left = static_cast<float>(point.x() + state.CharacterToX(from));
    float right = static_cast<float>(point.x() + state.CharacterToX(to));

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

    SkColor color = context->fillColor();
    unsigned char alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha)
        return;

    HDC hdc = context->canvas()->beginPlatformPaint();

    // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
    // Enforce non-transparent color.
    color = SkColorSetRGB(SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
    SetTextColor(hdc, skia::SkColorToCOLORREF(color));
    SetBkMode(hdc, TRANSPARENT);

    // Uniscribe counts the coordinates from the upper left, while WebKit uses
    // the baseline, so we have to subtract off the ascent.
    state.Draw(hdc, static_cast<int>(point.x()), static_cast<int>(point.y() - ascent()), from, to);
    context->canvas()->endPlatformPaint();
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    UniscribeHelperTextRun state(run, *this);
    return static_cast<float>(state.Width());
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x,
                                          bool includePartialGlyphs) const
{
    // Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.
    UniscribeHelperTextRun state(run, *this);
    int char_index = state.XToCharacter(x);

    // XToCharacter will return -1 if the position is before the first
    // character (we get called like this sometimes).
    if (char_index < 0)
      char_index = 0;
    return char_index;
}

} // namespace WebCore
