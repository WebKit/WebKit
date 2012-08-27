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

#include "CCScrollbarGeometryFixedThumb.h"

#include <public/WebRect.h>
#include <public/WebScrollbar.h>

using WebKit::WebRect;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarThemeGeometry;

namespace WebCore {

PassOwnPtr<CCScrollbarGeometryFixedThumb> CCScrollbarGeometryFixedThumb::create(PassOwnPtr<WebScrollbarThemeGeometry> geometry)
{
    return adoptPtr(new CCScrollbarGeometryFixedThumb(geometry));
}

CCScrollbarGeometryFixedThumb::~CCScrollbarGeometryFixedThumb()
{
}

void CCScrollbarGeometryFixedThumb::update(WebScrollbar* scrollbar)
{
    int length = CCScrollbarGeometryStub::thumbLength(scrollbar);

    if (scrollbar->orientation() == WebScrollbar::Horizontal)
        m_thumbSize = IntSize(length, scrollbar->size().height);
    else
        m_thumbSize = IntSize(scrollbar->size().width, length);
}

WebScrollbarThemeGeometry* CCScrollbarGeometryFixedThumb::clone() const
{
    CCScrollbarGeometryFixedThumb* geometry = new CCScrollbarGeometryFixedThumb(adoptPtr(CCScrollbarGeometryStub::clone()));
    geometry->m_thumbSize = m_thumbSize;
    return geometry;
}

int CCScrollbarGeometryFixedThumb::thumbLength(WebScrollbar* scrollbar)
{
    if (scrollbar->orientation() == WebScrollbar::Horizontal)
        return m_thumbSize.width();
    return m_thumbSize.height();
}

int CCScrollbarGeometryFixedThumb::thumbPosition(WebScrollbar* scrollbar)
{
    if (scrollbar->enabled()) {
        float size = scrollbar->maximum();
        if (!size)
            return 1;
        int value = std::min(std::max(0, scrollbar->value()), scrollbar->maximum());
        float pos = (trackLength(scrollbar) - thumbLength(scrollbar)) * value / size;
        return static_cast<int>(floorf((pos < 1 && pos > 0) ? 1 : pos));
    }
    return 0;
}
void CCScrollbarGeometryFixedThumb::splitTrack(WebScrollbar* scrollbar, const WebRect& unconstrainedTrackRect, WebRect& beforeThumbRect, WebRect& thumbRect, WebRect& afterThumbRect)
{
    // This is a reimplementation of ScrollbarThemeComposite::splitTrack.
    // Because the WebScrollbarThemeGeometry functions call down to native
    // ScrollbarThemeComposite code which uses ScrollbarThemeComposite virtual
    // helpers, there's no way to override a helper like thumbLength from
    // the WebScrollbarThemeGeometry level. So, these three functions
    // (splitTrack, thumbPosition, thumbLength) are copied here so that the
    // WebScrollbarThemeGeometry helper functions are used instead and
    // a fixed size thumbLength can be used.

    WebRect trackRect = constrainTrackRectToTrackPieces(scrollbar, unconstrainedTrackRect);
    int thickness = scrollbar->orientation() == WebScrollbar::Horizontal ? scrollbar->size().height : scrollbar->size().width;
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar->orientation() == WebScrollbar::Horizontal) {
        thumbRect = WebRect(trackRect.x + thumbPos, trackRect.y + (trackRect.height - thickness) / 2, thumbLength(scrollbar), thickness);
        beforeThumbRect = WebRect(trackRect.x, trackRect.y, thumbPos + thumbRect.width / 2, trackRect.height);
        afterThumbRect = WebRect(trackRect.x + beforeThumbRect.width, trackRect.y, trackRect.x + trackRect.width - beforeThumbRect.x - beforeThumbRect.width, trackRect.height);
    } else {
        thumbRect = WebRect(trackRect.x + (trackRect.width - thickness) / 2, trackRect.y + thumbPos, thickness, thumbLength(scrollbar));
        beforeThumbRect = WebRect(trackRect.x, trackRect.y, trackRect.width, thumbPos + thumbRect.height / 2);
        afterThumbRect = WebRect(trackRect.x, trackRect.y + beforeThumbRect.height, trackRect.width, trackRect.y + trackRect.height - beforeThumbRect.y - beforeThumbRect.height);
    }
}

CCScrollbarGeometryFixedThumb::CCScrollbarGeometryFixedThumb(PassOwnPtr<WebScrollbarThemeGeometry> geometry)
    : CCScrollbarGeometryStub(geometry)
{
}

}
