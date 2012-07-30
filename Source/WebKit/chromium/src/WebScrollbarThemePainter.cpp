/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <public/WebScrollbarThemePainter.h>

#include "PlatformContextSkia.h"
#include "ScrollbarThemeComposite.h"
#include "WebScrollbarThemeClientImpl.h"
#include <public/WebRect.h>
#include <public/WebScrollbar.h>

using namespace WebCore;

namespace WebKit {

void WebScrollbarThemePainter::assign(const WebScrollbarThemePainter& painter)
{
    // This is a pointer to a static object, so no ownership transferral.
    m_theme = painter.m_theme;
}

void WebScrollbarThemePainter::paintScrollbarBackground(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    SkRect clip = SkRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
    canvas->clipRect(clip);

    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintScrollbarBackground(&context, &client);
}

void WebScrollbarThemePainter::paintTrackBackground(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintTrackBackground(&context, &client, IntRect(rect));
}

void WebScrollbarThemePainter::paintBackTrackPart(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintTrackPiece(&context, &client, IntRect(rect), WebCore::BackTrackPart);
}

void WebScrollbarThemePainter::paintForwardTrackPart(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintTrackPiece(&context, &client, IntRect(rect), WebCore::ForwardTrackPart);
}

void WebScrollbarThemePainter::paintBackButtonStart(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintButton(&context, &client, IntRect(rect), WebCore::BackButtonStartPart);
}

void WebScrollbarThemePainter::paintBackButtonEnd(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintButton(&context, &client, IntRect(rect), WebCore::BackButtonEndPart);
}

void WebScrollbarThemePainter::paintForwardButtonStart(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintButton(&context, &client, IntRect(rect), WebCore::ForwardButtonStartPart);
}

void WebScrollbarThemePainter::paintForwardButtonEnd(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintButton(&context, &client, IntRect(rect), WebCore::ForwardButtonEndPart);
}

void WebScrollbarThemePainter::paintTickmarks(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintTickmarks(&context, &client, IntRect(rect));
}

void WebScrollbarThemePainter::paintThumb(WebCanvas* canvas, WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    PlatformContextSkia platformContext(canvas);
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext context(&platformContext);
    m_theme->paintThumb(&context, &client, IntRect(rect));
}

WebScrollbarThemePainter::WebScrollbarThemePainter(WebCore::ScrollbarThemeComposite* theme)
    : m_theme(theme)
{
}

} // namespace WebKit
