/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaGCGLANGLELayer.h"

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#include "GraphicsContextGLTextureMapperANGLE.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"
#include <epoxy/gl.h>

#if USE(ANGLE_GBM)
#include "GraphicsContextGLGBM.h"
#include "TextureMapperPlatformLayerProxyDMABuf.h"
#endif

namespace Nicosia {

using namespace WebCore;

void GCGLANGLELayer::swapBuffersIfNeeded()
{
    auto& proxy = m_contentLayer->proxy();

#if USE(ANGLE_GBM)
    if (is<TextureMapperPlatformLayerProxyDMABuf>(proxy)) {
        auto& swapchain = static_cast<GraphicsContextGLGBM&>(m_context).swapchain();
        auto bo = WTFMove(swapchain.displayBO);
        if (bo) {
            Locker locker { proxy.lock() };

            OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
            if (m_context.contextAttributes().alpha)
                flags.add(TextureMapperFlags::ShouldBlend);

            downcast<TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(
                DMABufObject(reinterpret_cast<uintptr_t>(swapchain.swapchain.get()) + bo->handle()),
                [&](auto&& object) {
                    return bo->createDMABufObject(object.handle);
                }, flags);
        }
        return;
    }
    ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
#endif

    OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
    GLint colorFormat;
    if (m_context.contextAttributes().alpha) {
        flags.add(TextureMapperFlags::ShouldBlend);
        colorFormat = GL_RGBA;
    } else
        colorFormat = GL_RGB;

    auto fboSize = m_context.getInternalFramebufferSize();
    Locker locker { proxy.lock() };
    auto layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(static_cast<GraphicsContextGLTextureMapperANGLE&>(m_context).m_compositorTextureID, fboSize, flags, colorFormat);
    downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
}

GCGLANGLELayer::GCGLANGLELayer(GraphicsContextGLTextureMapperANGLE& context)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(*this, adoptRef(*new TextureMapperPlatformLayerProxyGL)))
{
}

#if USE(ANGLE_GBM)
GCGLANGLELayer::GCGLANGLELayer(GraphicsContextGLGBM& context)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(*this, adoptRef(*new TextureMapperPlatformLayerProxyDMABuf)))
{
}
#endif

GCGLANGLELayer::~GCGLANGLELayer()
{
    m_contentLayer->invalidateClient();
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
