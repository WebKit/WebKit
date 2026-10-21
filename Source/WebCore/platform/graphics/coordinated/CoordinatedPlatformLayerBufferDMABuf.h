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

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "CoordinatedPlatformLayerBuffer.h"
#include <span>
#include <wtf/Function.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if !USE(TEXTURE_MAPPER)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

class DMABufBuffer;
class DMABufBufferAttributes;
class GLFence;

class CoordinatedPlatformLayerBufferDMABuf final : public CoordinatedPlatformLayerBuffer {
public:
#if USE(TEXTURE_MAPPER)
    static std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> create(Ref<DMABufBuffer>&&, OptionSet<TextureMapperFlags>, std::unique_ptr<GLFence>&&);
    static std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> create(Ref<DMABufBuffer>&&, OptionSet<TextureMapperFlags>, WTF::UnixFileDescriptor&&);
    CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&&, OptionSet<TextureMapperFlags>, std::unique_ptr<GLFence>&&);
    CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&&, OptionSet<TextureMapperFlags>, WTF::UnixFileDescriptor&&);
#else
    static std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> create(Ref<DMABufBuffer>&&, AlphaMode, Origin, std::unique_ptr<GLFence>&&, const sk_sp<GrContextThreadSafeProxy>&);
    static std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> create(Ref<DMABufBuffer>&&, AlphaMode, Origin, WTF::UnixFileDescriptor&&, const sk_sp<GrContextThreadSafeProxy>&);
    CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&&, AlphaMode, Origin, std::unique_ptr<GLFence>&&, const sk_sp<GrContextThreadSafeProxy>&);
    CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&&, AlphaMode, Origin, WTF::UnixFileDescriptor&&, const sk_sp<GrContextThreadSafeProxy>&);
#endif
    virtual ~CoordinatedPlatformLayerBufferDMABuf();

#if USE(TEXTURE_MAPPER)
    // Provides CPU-readable rows for a YUV plane whose standalone DMABuf import the GPU
    // rejected, so the plane can be repacked at an accepted stride. Set by the owner that
    // has access to the source frame; invoked only when a plane's zero-copy import fails.
    using YUVPlaneSourceFunction = Function<std::span<const uint8_t>(unsigned planeIndex, unsigned& stride)>;
    void setYUVPlaneSource(YUVPlaneSourceFunction&& source) { m_yuvPlaneSource = WTF::move(source); }
#endif

private:
#if USE(TEXTURE_MAPPER)
    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0) override;
#else
    void initializeSkiaImage(const sk_sp<GrContextThreadSafeProxy>&, Origin, std::unique_ptr<GLFence>&&, WTF::UnixFileDescriptor&&);
    sk_sp<SkImage> skiaImage() override { return m_image; }
#endif

#if USE(TEXTURE_MAPPER)
    std::unique_ptr<CoordinatedPlatformLayerBuffer> importDMABuf(bool& containsCopy) const;
#else
    std::unique_ptr<CoordinatedPlatformLayerBuffer> importDMABuf() const;
#endif

    const Ref<DMABufBuffer> m_dmabuf;
#if USE(TEXTURE_MAPPER)
    YUVPlaneSourceFunction m_yuvPlaneSource;
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_copiedBuffer;
    WTF::UnixFileDescriptor m_fenceFD;
#else
    sk_sp<SkImage> m_image;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_COORDINATED_PLATFORM_LAYER_BUFFER_TYPE(CoordinatedPlatformLayerBufferDMABuf, Type::DMABuf)

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
