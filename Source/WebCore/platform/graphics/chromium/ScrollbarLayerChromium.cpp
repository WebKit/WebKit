/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ScrollbarLayerChromium.h"

#include "Scrollbar.h"
#include "cc/CCScrollbarLayerImpl.h"

namespace WebCore {

PassOwnPtr<CCLayerImpl> ScrollbarLayerChromium::createCCLayerImpl()
{
    return CCScrollbarLayerImpl::create(id());
}

PassRefPtr<ScrollbarLayerChromium> ScrollbarLayerChromium::create(Scrollbar* scrollbar, int scrollLayerId)
{
    return adoptRef(new ScrollbarLayerChromium(scrollbar, scrollLayerId));
}

ScrollbarLayerChromium::ScrollbarLayerChromium(Scrollbar* scrollbar, int scrollLayerId)
    : m_scrollbar(scrollbar)
    , m_scrollLayerId(scrollLayerId)
    , m_scrollbarOverlayStyle(scrollbar->scrollbarOverlayStyle())
    , m_isScrollableAreaActive(scrollbar->isScrollableAreaActive())
    , m_isScrollViewScrollbar(scrollbar->isScrollViewScrollbar())
    , m_orientation(scrollbar->orientation())
    , m_controlSize(scrollbar->controlSize())
{
}

void ScrollbarLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCScrollbarLayerImpl* scrollbarLayer = static_cast<CCScrollbarLayerImpl*>(layer);

    scrollbarLayer->setScrollbarOverlayStyle(m_scrollbarOverlayStyle);

    Vector<IntRect> tickmarks;
    m_scrollbar->getTickmarks(tickmarks);
    scrollbarLayer->setTickmarks(tickmarks);

    scrollbarLayer->setIsScrollableAreaActive(m_isScrollableAreaActive);
    scrollbarLayer->setIsScrollViewScrollbar(m_isScrollViewScrollbar);

    scrollbarLayer->setOrientation(m_orientation);

    scrollbarLayer->setControlSize(m_controlSize);

    scrollbarLayer->setPressedPart(m_scrollbar->pressedPart());
    scrollbarLayer->setHoveredPart(m_scrollbar->hoveredPart());

    scrollbarLayer->setEnabled(m_scrollbar->enabled());
}

}
#endif // USE(ACCELERATED_COMPOSITING)
