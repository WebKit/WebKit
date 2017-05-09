/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "DrawingAreaProxyWPE.h"

#include "DrawingAreaMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

DrawingAreaProxyWPE::DrawingAreaProxyWPE(WebPageProxy& page)
    : DrawingAreaProxy(DrawingAreaTypeWPE, page)
{
}

DrawingAreaProxyWPE::~DrawingAreaProxyWPE()
{
}

void DrawingAreaProxyWPE::deviceScaleFactorDidChange()
{
    notImplemented();
}

void DrawingAreaProxyWPE::sizeDidChange()
{
    if (m_webPageProxy.isValid())
        m_webPageProxy.process().send(Messages::DrawingArea::UpdateBackingStoreState(0, false, m_webPageProxy.deviceScaleFactor(), m_size, m_scrollOffset), m_webPageProxy.pageID());
}

void DrawingAreaProxyWPE::dispatchAfterEnsuringDrawing(std::function<void(CallbackBase::Error)> callbackFunction)
{
    if (!m_webPageProxy.isValid()) {
        callbackFunction(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    RunLoop::main().dispatch(
        [callbackFunction] {
            callbackFunction(CallbackBase::Error::None);
        });
}

void DrawingAreaProxyWPE::update(uint64_t backingStoreStateID, const UpdateInfo&)
{
    notImplemented();
}

void DrawingAreaProxyWPE::didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo&, const LayerTreeContext&)
{
    notImplemented();
}

void DrawingAreaProxyWPE::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    ASSERT(!backingStoreStateID);

    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.enterAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyWPE::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&)
{
    notImplemented();
}

void DrawingAreaProxyWPE::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&)
{
    notImplemented();
}

} // namespace WebKit
