/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/GraphicsLayerCA.h>
#include <WebCore/PlatformLayer.h>

namespace WebKit {

class RemoteLayerTreeContext;

class GraphicsLayerCARemote final : public WebCore::GraphicsLayerCA {
public:
    GraphicsLayerCARemote(Type layerType, WebCore::GraphicsLayerClient&, RemoteLayerTreeContext&);
    virtual ~GraphicsLayerCARemote();

    bool filtersCanBeComposited(const WebCore::FilterOperations& filters) override;

    void moveToContext(RemoteLayerTreeContext&);
    void clearContext() { m_context = nullptr; }

private:
    bool isGraphicsLayerCARemote() const override { return true; }

    Ref<WebCore::PlatformCALayer> createPlatformCALayer(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner) override;
    Ref<WebCore::PlatformCALayer> createPlatformCALayer(PlatformLayer*, WebCore::PlatformCALayerClient* owner) override;
#if ENABLE(MODEL_ELEMENT)
    Ref<WebCore::PlatformCALayer> createPlatformCALayer(Ref<WebCore::Model>, WebCore::PlatformCALayerClient* owner) override;
#endif
    Ref<WebCore::PlatformCAAnimation> createPlatformCAAnimation(WebCore::PlatformCAAnimation::AnimationType, const String& keyPath) override;

    // PlatformCALayerRemote can't currently proxy directly composited image contents, so opt out of this optimization.
    bool shouldDirectlyCompositeImage(WebCore::Image*) const override { return false; }

    WebCore::Color pageTiledBackingBorderColor() const override;

    RefPtr<WebCore::GraphicsLayerAsyncContentsDisplayDelegate> createAsyncContentsDisplayDelegate() final;

    RemoteLayerTreeContext* m_context;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebKit::GraphicsLayerCARemote, isGraphicsLayerCARemote())
