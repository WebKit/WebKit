/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2016-2019 Igalia S.L.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DrawingAreaProxyWC.h"

#if USE(GRAPHICS_LAYER_WC)

#include "DrawingAreaMessages.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include <WebCore/Region.h>

namespace WebKit {

DrawingAreaProxyWC::DrawingAreaProxyWC(WebPageProxy& webPageProxy)
    : DrawingAreaProxy(DrawingAreaType::WC, webPageProxy)
{
}

void DrawingAreaProxyWC::paint(BackingStore::PlatformGraphicsContext context, const WebCore::IntRect& rect, WebCore::Region& unpaintedRegion)
{
    unpaintedRegion = rect;

    if (!m_backingStore)
        return;
    m_backingStore->paint(context, rect);
    unpaintedRegion.subtract(WebCore::IntRect({ }, m_backingStore->size()));
}

void DrawingAreaProxyWC::sizeDidChange()
{
    discardBackingStore();
    m_currentBackingStoreStateID++;
    m_webPageProxy.send(Messages::DrawingArea::UpdateGeometry(m_currentBackingStoreStateID, m_size), m_identifier);
}

void DrawingAreaProxyWC::dispatchAfterEnsuringDrawing(WTF::Function<void(CallbackBase::Error)>&& completionHandler)
{
    completionHandler(CallbackBase::Error::None);
}

void DrawingAreaProxyWC::update(uint64_t backingStoreStateID, const UpdateInfo& updateInfo)
{
    if (backingStoreStateID == m_currentBackingStoreStateID)
        incorporateUpdate(updateInfo);
    m_webPageProxy.send(Messages::DrawingArea::DisplayDidRefresh(), m_identifier);
}

void DrawingAreaProxyWC::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&)
{
    discardBackingStore();
}

void DrawingAreaProxyWC::incorporateUpdate(const UpdateInfo& updateInfo)
{
    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (!m_backingStore)
        m_backingStore.emplace(updateInfo.viewSize, updateInfo.deviceScaleFactor, m_webPageProxy);

    m_backingStore->incorporateUpdate(updateInfo);

    WebCore::Region damageRegion;
    if (updateInfo.scrollRect.isEmpty()) {
        for (const auto& rect : updateInfo.updateRects)
            damageRegion.unite(rect);
    } else
        damageRegion = WebCore::IntRect({ }, m_webPageProxy.viewSize());
    m_webPageProxy.setViewNeedsDisplay(damageRegion);
}

void DrawingAreaProxyWC::discardBackingStore()
{
    m_backingStore = std::nullopt;
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
