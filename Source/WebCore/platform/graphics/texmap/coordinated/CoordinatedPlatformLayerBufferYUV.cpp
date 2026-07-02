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
#include "BitmapTexturePool.h"
#include "PlatformDisplay.h"
#include "TextureMapper.h"

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkYUVAInfo.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/GrYUVABackendTextures.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferYUV> CoordinatedPlatformLayerBufferYUV::create(Format format, unsigned planeCount, std::array<unsigned, 4>&& planes, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, TransferFunction transferFunction, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferYUV>(format, planeCount, WTF::move(planes), WTF::move(yuvPlane), WTF::move(yuvPlaneOffset), yuvToRgbColorSpace, transferFunction, size, flags, WTF::move(fence));
}

std::unique_ptr<CoordinatedPlatformLayerBufferYUV> CoordinatedPlatformLayerBufferYUV::create(Format format, unsigned planeCount, Vector<RefPtr<BitmapTexture>, 4>&& textures, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, TransferFunction transferFunction, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferYUV>(format, planeCount, WTF::move(textures), WTF::move(yuvPlane), WTF::move(yuvPlaneOffset), yuvToRgbColorSpace, transferFunction, size, flags, WTF::move(fence));
}

CoordinatedPlatformLayerBufferYUV::CoordinatedPlatformLayerBufferYUV(Format format, unsigned planeCount, std::array<unsigned, 4>&& planes, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, TransferFunction transferFunction, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::YUV, size, flags, WTF::move(fence))
    , m_format(format)
    , m_planeCount(planeCount)
    , m_planes(WTF::move(planes))
    , m_yuvPlane(WTF::move(yuvPlane))
    , m_yuvPlaneOffset(WTF::move(yuvPlaneOffset))
    , m_yuvToRgbColorSpace(yuvToRgbColorSpace)
    , m_transferFunction(transferFunction)
{
}

CoordinatedPlatformLayerBufferYUV::CoordinatedPlatformLayerBufferYUV(Format format, unsigned planeCount, Vector<RefPtr<BitmapTexture>, 4>&& textures, std::array<unsigned, 4>&& yuvPlane, std::array<unsigned, 4>&& yuvPlaneOffset, YuvToRgbColorSpace yuvToRgbColorSpace, TransferFunction transferFunction, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::YUV, size, flags, WTF::move(fence))
    , m_format(format)
    , m_planeCount(planeCount)
    , m_textures(WTF::move(textures))
    , m_yuvPlane(WTF::move(yuvPlane))
    , m_yuvPlaneOffset(WTF::move(yuvPlaneOffset))
    , m_yuvToRgbColorSpace(yuvToRgbColorSpace)
    , m_transferFunction(transferFunction)
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
        case YuvToRgbColorSpace::Bt601:
            return s_bt601ConversionMatrix;
        case YuvToRgbColorSpace::Bt709:
            return s_bt709ConversionMatrix;
        case YuvToRgbColorSpace::Bt2020:
            return s_bt2020ConversionMatrix;
        case YuvToRgbColorSpace::Smpte240M:
            return s_smpte240MConversionMatrix;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    TextureMapper::TransferFunction textureMapperTransferFunction;
    switch (m_transferFunction) {
    case TransferFunction::Bt709:
        textureMapperTransferFunction = TextureMapper::TransferFunction::Bt709;
        break;
    case TransferFunction::Pq:
        textureMapperTransferFunction = TextureMapper::TransferFunction::Pq;
        break;
    }

    switch (m_planeCount) {
    case 1:
        RELEASE_ASSERT(m_format == Format::AYUV);
        ASSERT(m_yuvPlane[0] == m_yuvPlane[1] && m_yuvPlane[1] == m_yuvPlane[2]);
        ASSERT(m_yuvPlaneOffset[0] == 2 && m_yuvPlaneOffset[1] == 1 && !m_yuvPlaneOffset[2]);
        textureMapper.drawTexturePackedYUV(m_planes[m_yuvPlane[0]], yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity, textureMapperTransferFunction);
        break;
    case 2:
        ASSERT(!m_yuvPlaneOffset[0]);
        textureMapper.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { m_planes[m_yuvPlane[0]], m_planes[m_yuvPlane[1]] }, !!m_yuvPlaneOffset[1],
            yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity, textureMapperTransferFunction);
        break;
    case 3:
        ASSERT(!m_yuvPlaneOffset[0] && !m_yuvPlaneOffset[1] && !m_yuvPlaneOffset[2]);
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { m_planes[m_yuvPlane[0]], m_planes[m_yuvPlane[1]], m_planes[m_yuvPlane[2]] },
            yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity, std::nullopt, textureMapperTransferFunction);
        break;
    case 4:
        ASSERT(!m_yuvPlaneOffset[0] && !m_yuvPlaneOffset[1] && !m_yuvPlaneOffset[2]);
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { m_planes[m_yuvPlane[0]], m_planes[m_yuvPlane[1]], m_planes[m_yuvPlane[2]] },
            yuvToRgbMatrix, m_flags, targetRect, modelViewMatrix, opacity, m_planes[m_yuvPlane[3]], textureMapperTransferFunction);
        break;
    }
}

#if USE(SKIA)
sk_sp<SkImage> CoordinatedPlatformLayerBufferYUV::skiaImage()
{
    waitForContentsIfNeeded();

    auto planeConfig = SkYUVAInfo::PlaneConfig::kUnknown;
    auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
    GrGLTextureInfo externalTexture = { GL_TEXTURE_2D, 0, 0, skgpu::Protected::kNo };
    std::array<GrBackendTexture, 4> backendTextures;

    switch (m_planeCount) {
    case 1:
        RELEASE_ASSERT(m_format == Format::AYUV);
        planeConfig = SkYUVAInfo::PlaneConfig::kYUVA;
        subsampling = SkYUVAInfo::Subsampling::k444;

        externalTexture.fID = m_planes[m_yuvPlane[0]];
        externalTexture.fFormat = GL_RGBA8;
        backendTextures[0] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
        break;
    case 2:
        RELEASE_ASSERT(m_format == Format::NV12 || m_format == Format::NV21 || m_format == Format::P010);
        planeConfig = m_format == Format::NV21 ? SkYUVAInfo::PlaneConfig::kY_VU : SkYUVAInfo::PlaneConfig::kY_UV;
        subsampling = SkYUVAInfo::Subsampling::k420;

        externalTexture.fID = m_planes[m_yuvPlane[0]];
        externalTexture.fFormat = m_format == Format::P010 ? GL_R16 : GL_R8;
        backendTextures[0] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);

        externalTexture.fID = m_planes[m_yuvPlane[1]];
        externalTexture.fFormat = m_format == Format::P010 ? GL_RG16 : GL_RG8;
        backendTextures[1] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height() / 2, skgpu::Mipmapped::kNo, externalTexture);
        break;
    case 3:
        planeConfig = m_format == Format::YVU420 ? SkYUVAInfo::PlaneConfig::kY_V_U : SkYUVAInfo::PlaneConfig::kY_U_V;

        externalTexture.fID = m_planes[m_yuvPlane[0]];
        externalTexture.fFormat = GL_R8;
        backendTextures[0] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);

        switch (m_format) {
        case Format::YUV420:
        case Format::YVU420:
            subsampling = SkYUVAInfo::Subsampling::k420;

            externalTexture.fID = m_planes[m_yuvPlane[1]];
            backendTextures[1] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height() / 2, skgpu::Mipmapped::kNo, externalTexture);
            externalTexture.fID = m_planes[m_yuvPlane[2]];
            backendTextures[2] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height() / 2, skgpu::Mipmapped::kNo, externalTexture);
            break;
        case Format::YUV444:
            subsampling = SkYUVAInfo::Subsampling::k444;

            externalTexture.fID = m_planes[m_yuvPlane[1]];
            backendTextures[1] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
            externalTexture.fID = m_planes[m_yuvPlane[2]];
            backendTextures[2] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
            break;
        case Format::YUV411:
            subsampling = SkYUVAInfo::Subsampling::k411;

            externalTexture.fID = m_planes[m_yuvPlane[1]];
            backendTextures[1] = GrBackendTextures::MakeGL(m_size.width() / 4, m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
            externalTexture.fID = m_planes[m_yuvPlane[2]];
            backendTextures[2] = GrBackendTextures::MakeGL(m_size.width() / 4, m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
            break;
        case Format::YUV422:
            subsampling = SkYUVAInfo::Subsampling::k422;

            externalTexture.fID = m_planes[m_yuvPlane[1]];
            backendTextures[1] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
            externalTexture.fID = m_planes[m_yuvPlane[2]];
            backendTextures[2] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    case 4:
        RELEASE_ASSERT(m_format == Format::A420);
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V_A;
        subsampling = SkYUVAInfo::Subsampling::k420;
        externalTexture.fFormat = GL_R8;
        externalTexture.fID = m_planes[m_yuvPlane[0]];
        backendTextures[0] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
        externalTexture.fID = m_planes[m_yuvPlane[1]];
        backendTextures[1] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height() / 2, skgpu::Mipmapped::kNo, externalTexture);
        externalTexture.fID = m_planes[m_yuvPlane[2]];
        backendTextures[2] = GrBackendTextures::MakeGL(m_size.width() / 2, m_size.height() / 2, skgpu::Mipmapped::kNo, externalTexture);
        externalTexture.fID = m_planes[m_yuvPlane[3]];
        backendTextures[3] = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
        break;
    }

    if (planeConfig == SkYUVAInfo::PlaneConfig::kUnknown)
        return nullptr;

    SkYUVColorSpace yuvaColorSpace = [&] {
        switch (m_yuvToRgbColorSpace) {
        case YuvToRgbColorSpace::Bt601:
            return kRec601_Limited_SkYUVColorSpace;
        case YuvToRgbColorSpace::Bt709:
            return kRec709_Full_SkYUVColorSpace;
        case YuvToRgbColorSpace::Bt2020:
            return kBT2020_8bit_Full_SkYUVColorSpace;
        case YuvToRgbColorSpace::Smpte240M:
            return kSMPTE240_Full_SkYUVColorSpace;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    SkYUVAInfo info(SkISize::Make(m_size.width(), m_size.height()), planeConfig, subsampling, yuvaColorSpace);
    GrYUVABackendTextures yuvaBackendTextures(info, backendTextures.data(), kTopLeft_GrSurfaceOrigin);
    if (!yuvaBackendTextures.isValid())
        return nullptr;

    sk_sp<SkColorSpace> colorSpace = [&] {
        switch (m_transferFunction) {
        case TransferFunction::Bt709:
            return SkColorSpace::MakeRGB(SkNamedTransferFn::kRec709, SkNamedGamut::kSRGB);
        case TransferFunction::Pq:
            return SkColorSpace::MakeRGB(SkNamedTransferFn::kPQ, SkNamedGamut::kRec2020);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    ASSERT(grContext);
    return SkImages::TextureFromYUVATextures(grContext, yuvaBackendTextures, colorSpace);
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
