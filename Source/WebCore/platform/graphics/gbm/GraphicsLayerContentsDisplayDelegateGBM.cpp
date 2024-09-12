/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "GraphicsLayerContentsDisplayDelegateGBM.h"

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "CoordinatedPlatformLayerBufferDMABuf.h"
#include "DMABufBuffer.h"
#include "GLFence.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerProxyGL.h"

namespace WebCore {

GraphicsLayerContentsDisplayDelegateGBM::GraphicsLayerContentsDisplayDelegateGBM(bool isOpaque)
    : GraphicsLayerContentsDisplayDelegateTextureMapper(TextureMapperPlatformLayerProxyGL::create(TextureMapperPlatformLayerProxy::ContentType::WebGL))
    , m_isOpaque(isOpaque)
{
    m_proxy->setSwapBuffersFunction([this](TextureMapperPlatformLayerProxy& proxy) mutable {
        if (!m_buffer)
            return;

        OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
        if (!m_isOpaque)
            flags.add(TextureMapperFlags::ShouldBlend);

        Locker locker { proxy.lock() };
        auto layerBuffer = CoordinatedPlatformLayerBufferDMABuf::create(Ref { *m_buffer }, flags, WTFMove(m_fence));
        downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
    });
}

GraphicsLayerContentsDisplayDelegateGBM::~GraphicsLayerContentsDisplayDelegateGBM() = default;

void GraphicsLayerContentsDisplayDelegateGBM::setDisplayBuffer(RefPtr<DMABufBuffer>& displayBuffer, std::unique_ptr<GLFence>&& fence)
{
    m_buffer = displayBuffer;
    m_fence = WTFMove(fence);
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
