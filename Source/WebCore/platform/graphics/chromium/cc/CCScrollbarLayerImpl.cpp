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

#if USE(ACCELERATED_COMPOSITING)

#include "CCScrollbarLayerImpl.h"

#include "CCQuadSink.h"
#include "CCScrollbarAnimationController.h"
#include "CCTextureDrawQuad.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace WebCore {

PassOwnPtr<CCScrollbarLayerImpl> CCScrollbarLayerImpl::create(int id)
{
    return adoptPtr(new CCScrollbarLayerImpl(id));
}

CCScrollbarLayerImpl::CCScrollbarLayerImpl(int id)
    : CCLayerImpl(id)
    , m_scrollbar(this)
    , m_backTrackResourceId(0)
    , m_foreTrackResourceId(0)
    , m_thumbResourceId(0)
    , m_scrollbarOverlayStyle(WebScrollbar::ScrollbarOverlayStyleDefault)
    , m_orientation(WebScrollbar::Horizontal)
    , m_controlSize(WebScrollbar::RegularScrollbar)
    , m_pressedPart(WebScrollbar::NoPart)
    , m_hoveredPart(WebScrollbar::NoPart)
    , m_isScrollableAreaActive(false)
    , m_isScrollViewScrollbar(false)
    , m_enabled(false)
    , m_isCustomScrollbar(false)
    , m_isOverlayScrollbar(false)
{
}

void CCScrollbarLayerImpl::setScrollbarGeometry(PassOwnPtr<WebKit::WebScrollbarThemeGeometry> geometry)
{
    m_geometry = geometry;
}

void CCScrollbarLayerImpl::setScrollbarData(const WebScrollbar* scrollbar)
{
    m_scrollbarOverlayStyle = scrollbar->scrollbarOverlayStyle();
    m_orientation = scrollbar->orientation();
    m_controlSize = scrollbar->controlSize();
    m_pressedPart = scrollbar->pressedPart();
    m_hoveredPart = scrollbar->hoveredPart();
    m_isScrollableAreaActive = scrollbar->isScrollableAreaActive();
    m_isScrollViewScrollbar = scrollbar->isScrollViewScrollbar();
    m_enabled = scrollbar->enabled();
    m_isCustomScrollbar = scrollbar->isCustomScrollbar();
    m_isOverlayScrollbar = scrollbar->isOverlay();

    scrollbar->getTickmarks(m_tickmarks);
}

static FloatRect toUVRect(const WebRect& r, const IntRect& bounds)
{
    return FloatRect(static_cast<float>(r.x) / bounds.width(), static_cast<float>(r.y) / bounds.height(),
                     static_cast<float>(r.width) / bounds.width(), static_cast<float>(r.height) / bounds.height());
}

void CCScrollbarLayerImpl::appendQuads(CCQuadSink& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    bool premultipledAlpha = false;
    bool flipped = false;
    FloatRect uvRect(0, 0, 1, 1);
    IntRect boundsRect(IntPoint(), contentBounds());

    WebRect thumbRect, backTrackRect, foreTrackRect;
    m_geometry->splitTrack(&m_scrollbar, m_geometry->trackRect(&m_scrollbar), backTrackRect, thumbRect, foreTrackRect);
    if (!m_geometry->hasThumb(&m_scrollbar))
        thumbRect = WebRect();

    if (m_thumbResourceId && !thumbRect.isEmpty()) {
        OwnPtr<CCTextureDrawQuad> quad = CCTextureDrawQuad::create(sharedQuadState, IntRect(thumbRect.x, thumbRect.y, thumbRect.width, thumbRect.height), m_thumbResourceId, premultipledAlpha, uvRect, flipped);
        quad->setNeedsBlending();
        quadList.append(quad.release());
    }

    if (!m_backTrackResourceId)
        return;

    // We only paint the track in two parts if we were given a texture for the forward track part.
    if (m_foreTrackResourceId && !foreTrackRect.isEmpty())
        quadList.append(CCTextureDrawQuad::create(sharedQuadState, IntRect(foreTrackRect.x, foreTrackRect.y, foreTrackRect.width, foreTrackRect.height), m_foreTrackResourceId, premultipledAlpha, toUVRect(foreTrackRect, boundsRect), flipped));

    // Order matters here: since the back track texture is being drawn to the entire contents rect, we must append it after the thumb and
    // fore track quads. The back track texture contains (and displays) the buttons.
    if (!boundsRect.isEmpty())
        quadList.append(CCTextureDrawQuad::create(sharedQuadState, IntRect(boundsRect), m_backTrackResourceId, premultipledAlpha, uvRect, flipped));
}

void CCScrollbarLayerImpl::didLoseContext()
{
    m_backTrackResourceId = 0;
    m_foreTrackResourceId = 0;
    m_thumbResourceId = 0;
}

bool CCScrollbarLayerImpl::CCScrollbar::isOverlay() const
{
    return m_owner->m_isOverlayScrollbar;
}

int CCScrollbarLayerImpl::CCScrollbar::value() const
{
    return m_owner->m_currentPos;
}

WebKit::WebPoint CCScrollbarLayerImpl::CCScrollbar::location() const
{
    return WebKit::WebPoint();
}

WebKit::WebSize CCScrollbarLayerImpl::CCScrollbar::size() const
{
    return WebKit::WebSize(m_owner->contentBounds().width(), m_owner->contentBounds().height());
}

bool CCScrollbarLayerImpl::CCScrollbar::enabled() const
{
    return m_owner->m_enabled;
}

int CCScrollbarLayerImpl::CCScrollbar::maximum() const
{
    return m_owner->m_maximum;
}

int CCScrollbarLayerImpl::CCScrollbar::totalSize() const
{
    return m_owner->m_totalSize;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollViewScrollbar() const
{
    return m_owner->m_isScrollViewScrollbar;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollableAreaActive() const
{
    return m_owner->m_isScrollableAreaActive;
}

void CCScrollbarLayerImpl::CCScrollbar::getTickmarks(WebKit::WebVector<WebRect>& tickmarks) const
{
    tickmarks = m_owner->m_tickmarks;
}

WebScrollbar::ScrollbarControlSize CCScrollbarLayerImpl::CCScrollbar::controlSize() const
{
    return m_owner->m_controlSize;
}

WebScrollbar::ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::pressedPart() const
{
    return m_owner->m_pressedPart;
}

WebScrollbar::ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::hoveredPart() const
{
    return m_owner->m_hoveredPart;
}

WebScrollbar::ScrollbarOverlayStyle CCScrollbarLayerImpl::CCScrollbar::scrollbarOverlayStyle() const
{
    return m_owner->m_scrollbarOverlayStyle;
}

WebScrollbar::Orientation CCScrollbarLayerImpl::CCScrollbar::orientation() const
{
    return m_owner->m_orientation;
}

bool CCScrollbarLayerImpl::CCScrollbar::isCustomScrollbar() const
{
    return m_owner->m_isCustomScrollbar;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
