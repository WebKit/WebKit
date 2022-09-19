/*
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

#pragma once

#if USE(GRAPHICS_LAYER_WC)

#include "BackingStore.h"
#include "DrawingAreaProxy.h"

namespace WebCore {
class Region;
}

namespace WebKit {

class DrawingAreaProxyWC final : public DrawingAreaProxy {
public:
    DrawingAreaProxyWC(WebPageProxy&, WebProcessProxy&);

    void paint(BackingStore::PlatformGraphicsContext, const WebCore::IntRect&, WebCore::Region& unpaintedRegion);

private:
    // DrawingAreaProxy
    void deviceScaleFactorDidChange() override { }
    void sizeDidChange() override;
    void dispatchAfterEnsuringDrawing(WTF::Function<void(CallbackBase::Error)>&&) override;
    // message handers
    void update(uint64_t, const UpdateInfo&) override;
    void enterAcceleratedCompositingMode(uint64_t, const LayerTreeContext&) override;

    void incorporateUpdate(const UpdateInfo&);
    void discardBackingStore();

    uint64_t m_currentBackingStoreStateID { 0 };
    std::optional<BackingStore> m_backingStore;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
