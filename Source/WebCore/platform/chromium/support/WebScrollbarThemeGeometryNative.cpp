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

#include "WebScrollbarThemeGeometryNative.h"

#include "ScrollbarThemeComposite.h"
#include "WebScrollbarThemeClientImpl.h"
#include <public/WebScrollbar.h>

using namespace WebCore;

namespace WebKit {

PassOwnPtr<WebKit::WebScrollbarThemeGeometryNative> WebScrollbarThemeGeometryNative::create(WebCore::ScrollbarThemeComposite* theme)
{
    return adoptPtr(new WebScrollbarThemeGeometryNative(theme));
}

WebScrollbarThemeGeometryNative::WebScrollbarThemeGeometryNative(WebCore::ScrollbarThemeComposite* theme)
    : m_theme(theme)
{
}

WebScrollbarThemeGeometryNative* WebScrollbarThemeGeometryNative::clone() const
{
    return new WebScrollbarThemeGeometryNative(m_theme);
}

int WebScrollbarThemeGeometryNative::thumbPosition(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->thumbPosition(&client);
}

int WebScrollbarThemeGeometryNative::thumbLength(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->thumbLength(&client);
}

int WebScrollbarThemeGeometryNative::trackPosition(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->trackPosition(&client);
}

int WebScrollbarThemeGeometryNative::trackLength(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->trackLength(&client);
}

bool WebScrollbarThemeGeometryNative::hasButtons(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->hasButtons(&client);
}

bool WebScrollbarThemeGeometryNative::hasThumb(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->hasThumb(&client);
}

WebRect WebScrollbarThemeGeometryNative::trackRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->trackRect(&client);
}

WebRect WebScrollbarThemeGeometryNative::thumbRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->thumbRect(&client);
}

int WebScrollbarThemeGeometryNative::minimumThumbLength(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->minimumThumbLength(&client);
}

int WebScrollbarThemeGeometryNative::scrollbarThickness(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->scrollbarThickness(client.controlSize());
}

WebRect WebScrollbarThemeGeometryNative::backButtonStartRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->backButtonRect(&client, BackButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometryNative::backButtonEndRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->backButtonRect(&client, BackButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometryNative::forwardButtonStartRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->forwardButtonRect(&client, ForwardButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometryNative::forwardButtonEndRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->forwardButtonRect(&client, ForwardButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometryNative::constrainTrackRectToTrackPieces(WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->constrainTrackRectToTrackPieces(&client, IntRect(rect));
}

void WebScrollbarThemeGeometryNative::splitTrack(WebScrollbar* scrollbar, const WebRect& webTrack, WebRect& webStartTrack, WebRect& webThumb, WebRect& webEndTrack)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    IntRect track(webTrack);
    IntRect startTrack;
    IntRect thumb;
    IntRect endTrack;
    m_theme->splitTrack(&client, track, startTrack, thumb, endTrack);

    webStartTrack = startTrack;
    webThumb = thumb;
    webEndTrack = endTrack;
}

} // namespace WebKit
