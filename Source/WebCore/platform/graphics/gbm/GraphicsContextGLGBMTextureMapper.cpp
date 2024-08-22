/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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
#include "GraphicsContextGLGBMTextureMapper.h"

#if USE(ANGLE_GBM)

#include "GraphicsLayerContentsDisplayDelegateTextureMapper.h"
#include "NicosiaGCGLANGLELayer.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerProxyDMABuf.h"

namespace WebCore {

RefPtr<GraphicsContextGLGBMTextureMapper> GraphicsContextGLGBMTextureMapper::create(GraphicsContextGLAttributes&& attributes)
{
    auto context = adoptRef(*new GraphicsContextGLGBMTextureMapper(WTFMove(attributes)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLGBMTextureMapper::GraphicsContextGLGBMTextureMapper(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLGBM(WTFMove(attributes))
{ }

GraphicsContextGLGBMTextureMapper::~GraphicsContextGLGBMTextureMapper()
{
    if (m_layerContentsDisplayDelegate)
        static_cast<GraphicsLayerContentsDisplayDelegateTextureMapper*>(m_layerContentsDisplayDelegate.get())->proxy().setSwapBuffersFunction(nullptr);
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLGBMTextureMapper::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.copyRef();
}

bool GraphicsContextGLGBMTextureMapper::platformInitialize()
{
    auto proxy = TextureMapperPlatformLayerProxyDMABuf::create(TextureMapperPlatformLayerProxy::ContentType::WebGL);
    proxy->setSwapBuffersFunction([this](TextureMapperPlatformLayerProxy& proxy) mutable {
        auto bo = WTFMove(m_swapchain.displayBO);
        if (!bo)
            return;

        Locker locker { proxy.lock() };
        OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
        if (contextAttributes().alpha)
            flags.add(TextureMapperFlags::ShouldBlend);

        downcast<TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(
            DMABufObject(reinterpret_cast<uintptr_t>(m_swapchain.swapchain.get()) + bo->handle()),
            [&](auto&& object) {
                return bo->createDMABufObject(object.handle);
            }, flags, WTFMove(m_frameFence));
    });
    m_layerContentsDisplayDelegate = GraphicsLayerContentsDisplayDelegateTextureMapper::create(WTFMove(proxy));

    return GraphicsContextGLGBM::platformInitialize();
}

} // namespace WebCore

#endif // USE(ANGLE_GBM)
