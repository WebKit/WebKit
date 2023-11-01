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

#pragma once

#include "TextureMapperPlatformLayerProxy.h"

#if USE(COORDINATED_GRAPHICS) && USE(TEXTURE_MAPPER_DMABUF)

#include "DMABufFormat.h"
#include "DMABufObject.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayer.h"
#include <cstdint>
#include <memory>
#include <wtf/OptionSet.h>

namespace WebCore {

class TextureMapper;

class TextureMapperPlatformLayerProxyDMABuf final : public TextureMapperPlatformLayerProxy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextureMapperPlatformLayerProxyDMABuf();
    virtual ~TextureMapperPlatformLayerProxyDMABuf();

    bool isDMABufBased() const override { return true; }

    WEBCORE_EXPORT void activateOnCompositingThread(Compositor*, TextureMapperLayer*) override;
    WEBCORE_EXPORT void invalidate() override;
    WEBCORE_EXPORT void swapBuffer() override;

    class DMABufLayer : public ThreadSafeRefCounted<DMABufLayer>, public TextureMapperPlatformLayer {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit DMABufLayer(DMABufObject&&, OptionSet<TextureMapperFlags> = { });
        virtual ~DMABufLayer();

        void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = { }, float opacity = 1.0) final;

        void release()
        {
            m_object.releaseFlag.release();
        }

    private:
        friend class TextureMapperPlatformLayerProxyDMABuf;

        struct EGLImageData;
        static std::unique_ptr<EGLImageData> createEGLImageData(DMABufObject&);

        DMABufObject m_object;
        std::unique_ptr<EGLImageData> m_imageData;
        OptionSet<TextureMapperFlags> m_flags;

        static constexpr unsigned c_maximumAge { 16 };
        unsigned m_age { 0 };
    };

    template<typename F>
    void pushDMABuf(DMABufObject&& dmabufObject, const F& constructor, OptionSet<TextureMapperFlags> flags = { })
    {
        ASSERT(m_lock.isHeld());

        auto result = m_layers.ensure(dmabufObject.handle,
            [&] {
                return adoptRef(*new DMABufLayer(constructor(WTFMove(dmabufObject)), flags));
            });
        pushDMABuf(result.iterator->value.copyRef());
    }

private:
    void pushDMABuf(Ref<DMABufLayer>&&);

#if ASSERT_ENABLED
    RefPtr<Thread> m_compositorThread;
#endif

    using LayerMap = HashMap<uintptr_t, Ref<DMABufLayer>, WTF::DefaultHash<uintptr_t>, WTF::UnsignedWithZeroKeyHashTraits<uintptr_t>>;
    LayerMap m_layers;

    RefPtr<DMABufLayer> m_pendingLayer;
    RefPtr<DMABufLayer> m_committedLayer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_TEXTUREMAPPER_PLATFORMLAYERPROXY(TextureMapperPlatformLayerProxyDMABuf, isDMABufBased());

#endif // USE(COORDINATED_GRAPHICS) && USE(TEXTURE_MAPPER_DMABUF)
