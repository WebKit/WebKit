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
#include <public/WebScrollableLayer.h>

#include "LayerChromium.h"
#include "Region.h"

namespace WebKit {

void WebScrollableLayer::setScrollPosition(WebPoint position)
{
    m_private->setScrollPosition(position);
}

void WebScrollableLayer::setScrollable(bool scrollable)
{
    m_private->setScrollable(scrollable);
}

void WebScrollableLayer::setHaveWheelEventHandlers(bool haveWheelEventHandlers)
{
    m_private->setHaveWheelEventHandlers(haveWheelEventHandlers);
}

void WebScrollableLayer::setShouldScrollOnMainThread(bool shouldScrollOnMainThread)
{
    m_private->setShouldScrollOnMainThread(shouldScrollOnMainThread);
}

void WebScrollableLayer::setNonFastScrollableRegion(const WebVector<WebRect>& rects)
{
    WebCore::Region region;
    for (size_t i = 0; i < rects.size(); ++i) {
        WebCore::IntRect rect = rects[i];
        region.unite(rect);
    }
    m_private->setNonFastScrollableRegion(region);

}

void WebScrollableLayer::setIsContainerForFixedPositionLayers(bool enable)
{
    m_private->setIsContainerForFixedPositionLayers(enable);
}

void WebScrollableLayer::setFixedToContainerLayer(bool enable)
{
    m_private->setFixedToContainerLayer(enable);
}

} // namespace WebKit
