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

#include "ScrollbarTheme.h"
#include "ScrollbarThemeComposite.h"
#include "cc/CCQuadCuller.h"
#include "cc/CCTextureDrawQuad.h"

namespace WebCore {

PassOwnPtr<CCScrollbarLayerImpl> CCScrollbarLayerImpl::create(int id)
{
    return adoptPtr(new CCScrollbarLayerImpl(id));
}

CCScrollbarLayerImpl::CCScrollbarLayerImpl(int id)
    : CCLayerImpl(id)
    , m_scrollLayer(0)
    , m_scrollbar(this)
    , m_backTrackTextureId(0)
    , m_foreTrackTextureId(0)
    , m_thumbTextureId(0)
{
}

namespace {

FloatRect toUVRect(const IntRect& r, const IntRect& bounds)
{
    ASSERT(bounds.contains(r));
    return FloatRect(static_cast<float>(r.x()) / bounds.width(), static_cast<float>(r.y()) / bounds.height(),
                     static_cast<float>(r.width()) / bounds.width(), static_cast<float>(r.height()) / bounds.height());
}

}

void CCScrollbarLayerImpl::appendQuads(CCQuadCuller& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    ScrollbarThemeComposite* theme = static_cast<ScrollbarThemeComposite*>(ScrollbarTheme::theme());
    if (!theme)
        return;

    bool premultipledAlpha = false;
    bool flipped = false;
    FloatRect uvRect(0, 0, 1, 1);
    IntRect boundsRect(IntPoint(), contentBounds());

    IntRect thumbRect, backTrackRect, foreTrackRect;
    theme->splitTrack(&m_scrollbar, theme->trackRect(&m_scrollbar), backTrackRect, thumbRect, foreTrackRect);

    if (m_thumbTextureId && theme->hasThumb(&m_scrollbar) && !thumbRect.isEmpty()) {
        OwnPtr<CCTextureDrawQuad> quad = CCTextureDrawQuad::create(sharedQuadState, thumbRect, m_thumbTextureId, premultipledAlpha, uvRect, flipped);
        quad->setNeedsBlending();
        quadList.append(quad.release());
    }

    if (!m_backTrackTextureId)
        return;

    // We only paint the track in two parts if we were given a texture for the forward track part.
    if (m_foreTrackTextureId && !foreTrackRect.isEmpty())
        quadList.append(CCTextureDrawQuad::create(sharedQuadState, foreTrackRect, m_foreTrackTextureId, premultipledAlpha, toUVRect(foreTrackRect, boundsRect), flipped));

    // Order matters here: since the back track texture is being drawn to the entire contents rect, we must append it after the thumb and
    // fore track quads. The back track texture contains (and displays) the buttons.
    if (!boundsRect.isEmpty())
        quadList.append(CCTextureDrawQuad::create(sharedQuadState, boundsRect, m_backTrackTextureId, premultipledAlpha, uvRect, flipped));
}


int CCScrollbarLayerImpl::CCScrollbar::x() const
{
    return frameRect().x();
}

int CCScrollbarLayerImpl::CCScrollbar::y() const
{
    return frameRect().y();
}

int CCScrollbarLayerImpl::CCScrollbar::width() const
{
    return frameRect().width();
}

int CCScrollbarLayerImpl::CCScrollbar::height() const
{
    return frameRect().height();
}

IntSize CCScrollbarLayerImpl::CCScrollbar::size() const
{
    return frameRect().size();
}

IntPoint CCScrollbarLayerImpl::CCScrollbar::location() const
{
    return frameRect().location();
}

ScrollView* CCScrollbarLayerImpl::CCScrollbar::parent() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

ScrollView* CCScrollbarLayerImpl::CCScrollbar::root() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void CCScrollbarLayerImpl::CCScrollbar::setFrameRect(const IntRect&)
{
    ASSERT_NOT_REACHED();
}

IntRect CCScrollbarLayerImpl::CCScrollbar::frameRect() const
{
    return IntRect(IntPoint(), m_owner->contentBounds());
}

void CCScrollbarLayerImpl::CCScrollbar::invalidate()
{
    invalidateRect(frameRect());
}

void CCScrollbarLayerImpl::CCScrollbar::invalidateRect(const IntRect&)
{
    ASSERT_NOT_REACHED();
}

ScrollbarOverlayStyle CCScrollbarLayerImpl::CCScrollbar::scrollbarOverlayStyle() const
{
    return m_owner->m_scrollbarOverlayStyle;
}

void CCScrollbarLayerImpl::CCScrollbar::getTickmarks(Vector<IntRect>& tickmarks) const
{
    tickmarks = m_owner->m_tickmarks;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollableAreaActive() const
{
    return m_owner->m_isScrollableAreaActive;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollViewScrollbar() const
{
    return m_owner->m_isScrollViewScrollbar;
}

IntPoint CCScrollbarLayerImpl::CCScrollbar::convertFromContainingWindow(const IntPoint& windowPoint)
{
    ASSERT_NOT_REACHED();
    return windowPoint;
}

bool CCScrollbarLayerImpl::CCScrollbar::isCustomScrollbar() const
{
    return false;
}

ScrollbarOrientation CCScrollbarLayerImpl::CCScrollbar::orientation() const
{
    return m_owner->m_orientation;
}

int CCScrollbarLayerImpl::CCScrollbar::value() const
{
    if (!m_owner->m_scrollLayer)
        return 0;
    if (orientation() == HorizontalScrollbar)
        return m_owner->m_scrollLayer->scrollPosition().x() + m_owner->m_scrollLayer->scrollDelta().width();
    return m_owner->m_scrollLayer->scrollPosition().y() + m_owner->m_scrollLayer->scrollDelta().height();
}

float CCScrollbarLayerImpl::CCScrollbar::currentPos() const
{
    return value();
}

int CCScrollbarLayerImpl::CCScrollbar::visibleSize() const
{
    return totalSize() - maximum();
}

int CCScrollbarLayerImpl::CCScrollbar::totalSize() const
{
    if (!m_owner->m_scrollLayer || !m_owner->m_scrollLayer->children().size())
        return 0;
    // Copy & paste from CCLayerTreeHostImpl...
    // FIXME: Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    if (orientation() == HorizontalScrollbar)
        return m_owner->m_scrollLayer->children()[0]->bounds().width();
    return m_owner->m_scrollLayer->children()[0]->bounds().height();
}

int CCScrollbarLayerImpl::CCScrollbar::maximum() const
{
    if (!m_owner->m_scrollLayer)
        return 0;
    if (orientation() == HorizontalScrollbar)
        return m_owner->m_scrollLayer->maxScrollPosition().width();
    return m_owner->m_scrollLayer->maxScrollPosition().height();
}

ScrollbarControlSize CCScrollbarLayerImpl::CCScrollbar::controlSize() const
{
    return m_owner->m_controlSize;
}

int CCScrollbarLayerImpl::CCScrollbar::lineStep() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

int CCScrollbarLayerImpl::CCScrollbar::pageStep() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::pressedPart() const
{
    return m_owner->m_pressedPart;
}

ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::hoveredPart() const
{
    return m_owner->m_hoveredPart;
}

void CCScrollbarLayerImpl::CCScrollbar::styleChanged()
{
}

bool CCScrollbarLayerImpl::CCScrollbar::enabled() const
{
    return m_owner->m_enabled;
}

void CCScrollbarLayerImpl::CCScrollbar::setEnabled(bool)
{
    ASSERT_NOT_REACHED();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
