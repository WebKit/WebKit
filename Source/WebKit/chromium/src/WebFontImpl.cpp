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
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "PlatformContextSkia.h"
#include "WebFloatPoint.h"
#include "WebFloatRect.h"
#include "WebFontDescription.h"
#include "WebRect.h"
#include "WebTextRun.h"

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
    return m_font.ascent();
}

int WebFontImpl::descent() const
{
    return m_font.descent();
}

int WebFontImpl::height() const
{
    return m_font.height();
}

int WebFontImpl::lineSpacing() const
{
    return m_font.lineSpacing();
}

float WebFontImpl::xHeight() const
{
    return m_font.xHeight();
}

void WebFontImpl::drawText(WebCanvas* canvas, const WebTextRun& run, const WebFloatPoint& leftBaseline,
                           WebColor color, const WebRect& clip, bool canvasIsOpaque,
                           int from, int to) const
{
    // FIXME hook canvasIsOpaque up to the platform-specific indicators for
    // whether subpixel AA can be used for this draw. On Windows, this is
    // PlatformContextSkia::setDrawingToImageBuffer.
#if WEBKIT_USING_CG
    GraphicsContext gc(canvas);
#elif WEBKIT_USING_SKIA
    PlatformContextSkia context(canvas);
    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia.
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#else
    notImplemented();
#endif

    gc.save();
    gc.setFillColor(color, ColorSpaceDeviceRGB);
    gc.clip(WebCore::FloatRect(clip));
    m_font.drawText(&gc, run, leftBaseline, from, to);
    gc.restore();
}

int WebFontImpl::calculateWidth(const WebTextRun& run) const
{
    return m_font.width(run, 0);
}

int WebFontImpl::offsetForPosition(const WebTextRun& run, float position) const
{
    return m_font.offsetForPosition(run, position, true);
}

WebFloatRect WebFontImpl::selectionRectForText(const WebTextRun& run, const WebFloatPoint& leftBaseline, int height, int from, int to) const
{
    return m_font.selectionRectForText(run, leftBaseline, height, from, to);
}

} // namespace WebKit
