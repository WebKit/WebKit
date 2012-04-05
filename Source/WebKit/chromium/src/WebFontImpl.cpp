/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebFontImpl.h"

#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "painting/GraphicsContextBuilder.h"
#include "TextRun.h"
#include "platform/WebFloatPoint.h"
#include "platform/WebFloatRect.h"
#include "WebFontDescription.h"
#include "platform/WebRect.h"
#include "WebTextRun.h"

#include <skia/ext/platform_canvas.h>

using namespace WebCore;

namespace WebKit {

WebFont* WebFont::create(const WebFontDescription& desc)
{
    return new WebFontImpl(desc, desc.letterSpacing, desc.wordSpacing);
}

WebFontImpl::WebFontImpl(const FontDescription& desc, short letterSpacing, short wordSpacing)
    : m_font(desc, letterSpacing, wordSpacing)
{
    m_font.update(0);
}

WebFontDescription WebFontImpl::fontDescription() const
{
    return WebFontDescription(m_font.fontDescription(), m_font.letterSpacing(), m_font.wordSpacing());
}

int WebFontImpl::ascent() const
{
    return m_font.fontMetrics().ascent();
}

int WebFontImpl::descent() const
{
    return m_font.fontMetrics().descent();
}

int WebFontImpl::height() const
{
    return m_font.fontMetrics().height();
}

int WebFontImpl::lineSpacing() const
{
    return m_font.fontMetrics().lineSpacing();
}

float WebFontImpl::xHeight() const
{
    return m_font.fontMetrics().xHeight();
}

void WebFontImpl::drawText(WebCanvas* canvas, const WebTextRun& run, const WebFloatPoint& leftBaseline,
                           WebColor color, const WebRect& clip, bool canvasIsOpaque,
                           int from, int to) const
{
    FontCachePurgePreventer fontCachePurgePreventer;

    GraphicsContextBuilder builder(canvas);
    GraphicsContext& gc = builder.context();

#if WEBKIT_USING_SKIA
    gc.platformContext()->setDrawingToImageBuffer(!canvasIsOpaque);
#elif WEBKIT_USING_CG
    // FIXME hook canvasIsOpaque up to the platform-specific indicators for
    // whether subpixel AA can be used for this draw.
#endif

    gc.save();
    gc.setFillColor(color, ColorSpaceDeviceRGB);
    gc.clip(WebCore::FloatRect(clip));
    m_font.drawText(&gc, run, leftBaseline, from, to);
    gc.restore();

#if defined(WIN32)
    if (canvasIsOpaque && SkColorGetA(color) == 0xFF && !canvas->isDrawingToLayer()) {
        // The text drawing logic on Windows ignores the alpha component
        // intentionally, for performance reasons.
        // (Please see TransparencyAwareFontPainter::initializeForGDI in
        // FontChromiumWin.cpp.)
        const SkBitmap& bitmap = canvas->getTopDevice()->accessBitmap(true);
        IntRect textBounds = estimateTextBounds(run, leftBaseline);
        IntRect destRect = gc.getCTM().mapRect(textBounds);
        destRect.intersect(IntRect(0, 0, bitmap.width(), bitmap.height()));
        for (int y = destRect.y(), maxY = destRect.maxY(); y < maxY; y++) {
            uint32_t* row = bitmap.getAddr32(0, y);
            for (int x = destRect.x(), maxX = destRect.maxX(); x < maxX; x++)
                row[x] |= (0xFF << SK_A32_SHIFT);
        }
    }
#endif
}

int WebFontImpl::calculateWidth(const WebTextRun& run) const
{
    FontCachePurgePreventer fontCachePurgePreventer;
    return m_font.width(run, 0);
}

int WebFontImpl::offsetForPosition(const WebTextRun& run, float position) const
{
    FontCachePurgePreventer fontCachePurgePreventer;
    return m_font.offsetForPosition(run, position, true);
}

WebFloatRect WebFontImpl::selectionRectForText(const WebTextRun& run, const WebFloatPoint& leftBaseline, int height, int from, int to) const
{
    FontCachePurgePreventer fontCachePurgePreventer;
    return m_font.selectionRectForText(run, leftBaseline, height, from, to);
}

WebRect WebFontImpl::estimateTextBounds(const WebTextRun& run, const WebFloatPoint& leftBaseline) const
{
    FontCachePurgePreventer fontCachePurgePreventer;
    int totalWidth = m_font.width(run, 0);
    const FontMetrics& fontMetrics = m_font.fontMetrics();
    return WebRect(leftBaseline.x - (fontMetrics.ascent() + fontMetrics.descent()) / 2,
                   leftBaseline.y - fontMetrics.ascent() - fontMetrics.lineGap(),
                   totalWidth + fontMetrics.ascent() + fontMetrics.descent(),
                   fontMetrics.lineSpacing()); 
}

} // namespace WebKit
