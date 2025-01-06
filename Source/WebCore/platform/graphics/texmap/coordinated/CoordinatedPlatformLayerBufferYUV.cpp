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

#include "config.h"
#include "CoordinatedPlatformLayerBufferYUV.h"

#if USE(COORDINATED_GRAPHICS)
#include "TextureMapper.h"

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferYUV> CoordinatedPlatformLayerBufferYUV::create(unsigned planeCount, std::array<unsigned, 4>&& planes, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferYUV>(planeCount, WTFMove(planes), WTFMove(yuvPlane), WTFMove(yuvPlaneOffset), yuvToRgbColorSpace, size, flags, WTFMove(fence));
}

std::unique_ptr<CoordinatedPlatformLayerBufferYUV> CoordinatedPlatformLayerBufferYUV::create(unsigned planeCount, Vector<RefPtr<BitmapTexture>, 4>&& textures, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferYUV>(planeCount, WTFMove(textures), WTFMove(yuvPlane), WTFMove(yuvPlaneOffset), yuvToRgbColorSpace, size, flags, WTFMove(fence));
}

CoordinatedPlatformLayerBufferYUV::CoordinatedPlatformLayerBufferYUV(unsigned planeCount, std::array<unsigned, 4>&& planes, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::YUV, size, flags, WTFMove(fence))
    , m_planeCount(planeCount)
    , m_planes(WTFMove(planes))
    , m_yuvPlane(WTFMove(yuvPlane))
    , m_yuvPlaneOffset(WTFMove(yuvPlaneOffset))
    , m_yuvToRgbColorSpace(yuvToRgbColorSpace)
{
}

CoordinatedPlatformLayerBufferYUV::CoordinatedPlatformLayerBufferYUV(unsigned planeCount, Vector<RefPtr<BitmapTexture>, 4>&& textures, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::YUV, size, flags, WTFMove(fence))
    , m_planeCount(planeCount)
    , m_textures(WTFMove(textures))
    , m_yuvPlane(WTFMove(yuvPlane))
    , m_yuvPlaneOffset(WTFMove(yuvPlaneOffset))
    , m_yuvToRgbColorSpace(yuvToRgbColorSpace)
{
    for (unsigned i = 0; i < m_textures.size(); ++i)
        m_planes[i] = m_textures[i] ? m_textures[i]->id() : 0;
}

CoordinatedPlatformLayerBufferYUV::~CoordinatedPlatformLayerBufferYUV() = default;

void CoordinatedPlatformLayerBufferYUV::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    static constexpr std::array<GLfloat, 16> s_bt601ConversionMatrix {
        1.164383561643836,  0.0,                1.596026785714286, -0.874202217873451,
        1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146,
        1.164383561643836,  2.017232142857143,  0.0,               -1.085630789302022,
        0.0,                0.0,                0.0,                1.0,
    };
    static constexpr std::array<GLfloat, 16> s_bt709ConversionMatrix {
        1.164383561643836,  0.0,                1.792741071428571, -0.972945075016308,
        1.164383561643836, -0.213248614273730, -0.532909328559444,  0.301482665475862,
        1.164383561643836,  2.112401785714286,  0.0,               -1.133402217873451,
        0.0,                0.0,                0.0,                1.0,
    };
    static constexpr std::array<GLfloat, 16> s_bt2020ConversionMatrix {
        1.164383561643836,  0.0,                1.678674107142857, -0.915687932159165,
        1.164383561643836, -0.187326104219343, -0.650424318505057,  0.347458498519301,
        1.164383561643836,  2.141772321428571,  0.0,               -1.148145075016308,
        0.0,                0.0,                0.0,                1.0,
    };
    static constexpr std::array<GLfloat, 16> s_smpte240MConversionMatrix {
        1.164383561643836,  0.0,                1.793651785714286, -0.973402217873451,
        1.164383561643836, -0.256532845251675, -0.542724809537390,  0.328136638536074,
        1.164383561643836,  2.07984375,         0.0,               -1.117059360730593,
        0.0,                0.0,                0.0,                1.0,
    };

    const std::array<GLfloat, 16>& yuvToRgbMatrix = [&] {
        switch (m_yuvToRgbColorSpace) {
        case YuvToRgbColorSpace::BT601:
            return s_bt601ConversionMatrix;
        case YuvToRgbColorSpace::BT709:
            return s_bt709ConversionMatrix;
        case YuvToRgbColorSpace::BT2020:
            return s_bt2020ConversionMatrix;
        case YuvToRgbColorSpace::SMPTE240M:
            return s_smpte240MConversionMatrix;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    switch (m_planeCount) {
    case 1:
        ASSERT(m_yuvPlane[0] == m_yuvPlane[1] && m_yuvPlane[1] == m_yuvPlane[2]);
        ASSERT(m_yuvPlaneOffset[0] == 2 && m_yuvPlaneOffset[1] == 1 && !m_yuvPlaneOffset[2]);
        textureMapper.drawTexturePackedYUV(m_planes[m_yuvPlane[0]], yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity);
        break;
    case 2:
        ASSERT(!m_yuvPlaneOffset[0]);
        textureMapper.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { m_planes[m_yuvPlane[0]], m_planes[m_yuvPlane[1]] }, !!m_yuvPlaneOffset[1],
            yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity);
        break;
    case 3:
        ASSERT(!m_yuvPlaneOffset[0] && !m_yuvPlaneOffset[1] && !m_yuvPlaneOffset[2]);
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { m_planes[m_yuvPlane[0]], m_planes[m_yuvPlane[1]], m_planes[m_yuvPlane[2]] },
            yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity, std::nullopt);
        break;
    case 4:
        ASSERT(!m_yuvPlaneOffset[0] && !m_yuvPlaneOffset[1] && !m_yuvPlaneOffset[2]);
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { m_planes[m_yuvPlane[0]], m_planes[m_yuvPlane[1]], m_planes[m_yuvPlane[2]] },
            yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity, m_planes[m_yuvPlane[3]]);
        break;
    }
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
