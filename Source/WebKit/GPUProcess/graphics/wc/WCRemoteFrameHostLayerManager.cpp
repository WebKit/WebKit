/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
#include "WCRemoteFrameHostLayerManager.h"

#if USE(GRAPHICS_LAYER_WC)

#include <WebCore/BitmapTexture.h>
#include <WebCore/TextureMapperPlatformLayer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class WCRemoteFrameHostLayer final : public WebCore::TextureMapperPlatformLayer {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WCRemoteFrameHostLayer);
public:
    void paintToTextureMapper(WebCore::TextureMapper& textureMapper, const WebCore::FloatRect& targetRect, const WebCore::TransformationMatrix& modelViewMatrix, float opacity) override
    {
        if (m_texture)
            textureMapper.drawTexture(*m_texture, targetRect, modelViewMatrix, opacity);
    }

    void setTexture(RefPtr<WebCore::BitmapTexture> texture)
    {
        m_texture = texture;
    }

private:
    RefPtr<WebCore::BitmapTexture> m_texture;
};

class WCRemoteFrameHostLayerManager::RemoteFrameHostLayerData final {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WCRemoteFrameHostLayerManager);
public:
    RemoteFrameHostLayerData(WebCore::ProcessIdentifier webProcessIdentifier)
        : m_webProcessIdentifier(webProcessIdentifier)
    {
    }

    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }
    void setWebProcessIdentifier(WebCore::ProcessIdentifier webProcessIdentifier) { m_webProcessIdentifier = webProcessIdentifier; }
    WCRemoteFrameHostLayer& layer() { return m_layer; }

private:
    // A web process identifier currently owning the texture. Remote frame process or frame host process.
    WebCore::ProcessIdentifier m_webProcessIdentifier;
    WCRemoteFrameHostLayer m_layer;
};

WCRemoteFrameHostLayerManager& WCRemoteFrameHostLayerManager::singleton()
{
    static NeverDestroyed<WCRemoteFrameHostLayerManager> manager;
    return manager;
}

WebCore::TextureMapperPlatformLayer* WCRemoteFrameHostLayerManager::acquireRemoteFrameHostLayer(WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier, WebCore::ProcessIdentifier webProcessIdentifier)
{
    auto& data = *m_layers.ensure(layerHostingContextIdentifier, [&] {
        return makeUnique<RemoteFrameHostLayerData>(webProcessIdentifier);
    }).iterator->value;
    // Transfer the data ownership to the frame host process.
    data.setWebProcessIdentifier(webProcessIdentifier);
    return &data.layer();
}

void WCRemoteFrameHostLayerManager::releaseRemoteFrameHostLayer(WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier)
{
    m_layers.remove(layerHostingContextIdentifier);
}

void WCRemoteFrameHostLayerManager::updateTexture(WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier, WebCore::ProcessIdentifier webProcessIdentifier, RefPtr<WebCore::BitmapTexture> texture)
{
    m_layers.ensure(layerHostingContextIdentifier, [&] {
        // Create a new data if the frame host didn't create it yet. The initial owner is the remote frame process.
        return makeUnique<RemoteFrameHostLayerData>(webProcessIdentifier);
    }).iterator->value->layer().setTexture(WTFMove(texture));
}

void WCRemoteFrameHostLayerManager::removeAllLayersForProcess(WebCore::ProcessIdentifier webProcessIdentifier)
{
    m_layers.removeIf([&](auto& iterator) {
        return iterator.value->webProcessIdentifier() == webProcessIdentifier;
    });
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
