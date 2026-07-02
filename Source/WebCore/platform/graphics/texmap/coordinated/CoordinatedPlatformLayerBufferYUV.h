/*
 * Copyright (C) 2015, 2024 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBuffer.h"

namespace WebCore {

class BitmapTexture;

class CoordinatedPlatformLayerBufferYUV final : public CoordinatedPlatformLayerBuffer {
public:
    enum class Format : uint8_t { AYUV, NV12, NV21, P010, YUV420, YVU420, YUV444, YUV411, YUV422, A420 };
    enum class YuvToRgbColorSpace : uint8_t { Bt601, Bt709, Bt2020, Smpte240M };
    enum class TransferFunction : uint8_t { Bt709, Pq };
    static std::unique_ptr<CoordinatedPlatformLayerBufferYUV> create(Format, unsigned planeCount, std::array<unsigned, 4>&& planes, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace, TransferFunction, const IntSize&, OptionSet<TextureMapperFlags>, std::unique_ptr<GLFence>&&);
    static std::unique_ptr<CoordinatedPlatformLayerBufferYUV> create(Format, unsigned planeCount, Vector<RefPtr<BitmapTexture>, 4>&& textures, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace, TransferFunction, const IntSize&, OptionSet<TextureMapperFlags>, std::unique_ptr<GLFence>&&);
    CoordinatedPlatformLayerBufferYUV(Format, unsigned planeCount, std::array<unsigned, 4>&& planes, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace, TransferFunction, const IntSize&, OptionSet<TextureMapperFlags>, std::unique_ptr<GLFence>&&);
    CoordinatedPlatformLayerBufferYUV(Format, unsigned planeCount, Vector<RefPtr<BitmapTexture>, 4>&& textures, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace, TransferFunction, const IntSize&, OptionSet<TextureMapperFlags>, std::unique_ptr<GLFence>&&);
    virtual ~CoordinatedPlatformLayerBufferYUV();

private:
    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0) override;

#if USE(SKIA)
    sk_sp<SkImage> skiaImage() override;
#endif

    Format m_format { Format::AYUV };
    unsigned m_planeCount { 0 };
    Vector<RefPtr<BitmapTexture>, 4> m_textures;
    std::array<unsigned, 4> m_planes;
    std::array<unsigned, 4> m_yuvPlane;
    std::array<unsigned, 4> m_yuvPlaneOffset;
    YuvToRgbColorSpace m_yuvToRgbColorSpace { YuvToRgbColorSpace::Bt601 };
    TransferFunction m_transferFunction { TransferFunction::Bt709 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_COORDINATED_PLATFORM_LAYER_BUFFER_TYPE(CoordinatedPlatformLayerBufferYUV, Type::YUV)

#endif // USE(COORDINATED_GRAPHICS)
