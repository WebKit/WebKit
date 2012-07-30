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

#include <public/WebScrollbarThemeGeometry.h>

#include "ScrollbarThemeComposite.h"
#include "WebScrollbarThemeClientImpl.h"
#include <public/WebRect.h>
#include <public/WebScrollbar.h>

using namespace WebCore;

namespace WebKit {

void WebScrollbarThemeGeometry::assign(const WebScrollbarThemeGeometry& geometry)
{
    // This is a pointer to a static object, so no ownership transferral.
    m_theme = geometry.m_theme;
}

int WebScrollbarThemeGeometry::thumbPosition(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->thumbPosition(&client);
}

int WebScrollbarThemeGeometry::thumbLength(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->thumbLength(&client);
}

int WebScrollbarThemeGeometry::trackPosition(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->trackPosition(&client);
}

int WebScrollbarThemeGeometry::trackLength(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->trackLength(&client);
}

bool WebScrollbarThemeGeometry::hasButtons(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->hasButtons(&client);
}

bool WebScrollbarThemeGeometry::hasThumb(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->hasThumb(&client);
}

WebRect WebScrollbarThemeGeometry::trackRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->trackRect(&client);
}

WebRect WebScrollbarThemeGeometry::thumbRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->thumbRect(&client);
}

int WebScrollbarThemeGeometry::minimumThumbLength(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->minimumThumbLength(&client);
}

int WebScrollbarThemeGeometry::scrollbarThickness(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->scrollbarThickness(client.controlSize());
}

WebRect WebScrollbarThemeGeometry::backButtonStartRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->backButtonRect(&client, BackButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometry::backButtonEndRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->backButtonRect(&client, BackButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometry::forwardButtonStartRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->forwardButtonRect(&client, ForwardButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometry::forwardButtonEndRect(WebScrollbar* scrollbar)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->forwardButtonRect(&client, ForwardButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometry::constrainTrackRectToTrackPieces(WebScrollbar* scrollbar, const WebRect& rect)
{
    WebScrollbarThemeClientImpl client(scrollbar);
    return m_theme->constrainTrackRectToTrackPieces(&client, IntRect(rect));
}

void WebScrollbarThemeGeometry::splitTrack(WebScrollbar* scrollbar, const WebRect& webTrack, WebRect& webStartTrack, WebRect& webThumb, WebRect& webEndTrack)
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

WebScrollbarThemeGeometry::WebScrollbarThemeGeometry(WebCore::ScrollbarThemeComposite* theme)
    : m_theme(theme)
{
}

} // namespace WebKit
