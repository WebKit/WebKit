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

#include "CCScrollbarGeometryStub.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarThemeGeometry;

namespace WebCore {

CCScrollbarGeometryStub::CCScrollbarGeometryStub(PassOwnPtr<WebScrollbarThemeGeometry> geometry)
    : m_geometry(geometry)
{
}

CCScrollbarGeometryStub::~CCScrollbarGeometryStub()
{
}

WebScrollbarThemeGeometry* CCScrollbarGeometryStub::clone() const
{
    return m_geometry->clone();
}

int CCScrollbarGeometryStub::thumbPosition(WebScrollbar* scrollbar)
{
    return m_geometry->thumbPosition(scrollbar);
}

int CCScrollbarGeometryStub::thumbLength(WebScrollbar* scrollbar)
{
    return m_geometry->thumbLength(scrollbar);
}

int CCScrollbarGeometryStub::trackPosition(WebScrollbar* scrollbar)
{
    return m_geometry->trackPosition(scrollbar);
}

int CCScrollbarGeometryStub::trackLength(WebScrollbar* scrollbar)
{
    return m_geometry->trackLength(scrollbar);
}

bool CCScrollbarGeometryStub::hasButtons(WebScrollbar* scrollbar)
{
    return m_geometry->hasButtons(scrollbar);
}

bool CCScrollbarGeometryStub::hasThumb(WebScrollbar* scrollbar)
{
    return m_geometry->hasThumb(scrollbar);
}

WebRect CCScrollbarGeometryStub::trackRect(WebScrollbar* scrollbar)
{
    return m_geometry->trackRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::thumbRect(WebScrollbar* scrollbar)
{
    return m_geometry->thumbRect(scrollbar);
}

int CCScrollbarGeometryStub::minimumThumbLength(WebScrollbar* scrollbar)
{
    return m_geometry->minimumThumbLength(scrollbar);
}

int CCScrollbarGeometryStub::scrollbarThickness(WebScrollbar* scrollbar)
{
    return m_geometry->scrollbarThickness(scrollbar);
}

WebRect CCScrollbarGeometryStub::backButtonStartRect(WebScrollbar* scrollbar)
{
    return m_geometry->backButtonStartRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::backButtonEndRect(WebScrollbar* scrollbar)
{
    return m_geometry->backButtonEndRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::forwardButtonStartRect(WebScrollbar* scrollbar)
{
    return m_geometry->forwardButtonStartRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::forwardButtonEndRect(WebScrollbar* scrollbar)
{
    return m_geometry->forwardButtonEndRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::constrainTrackRectToTrackPieces(WebScrollbar* scrollbar, const WebRect& rect)
{
    return m_geometry->constrainTrackRectToTrackPieces(scrollbar, rect);
}

void CCScrollbarGeometryStub::splitTrack(WebScrollbar* scrollbar, const WebRect& unconstrainedTrackRect, WebRect& beforeThumbRect, WebRect& thumbRect, WebRect& afterThumbRect)
{
    m_geometry->splitTrack(scrollbar, unconstrainedTrackRect, beforeThumbRect, thumbRect, afterThumbRect);
}

}
