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
#include "CoordinatedPlatformLayerBufferDMABuf.h"

#if USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "CoordinatedPlatformLayerBufferYUV.h"
#include "DMABufBuffer.h"
#include "PlatformDisplay.h"
#include "TextureMapper.h"
#include <drm_fourcc.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTF::move(dmabuf), flags, WTF::move(fence));
}

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, UnixFileDescriptor&& fenceFD)
{
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTF::move(dmabuf), flags, WTF::move(fenceFD));
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, WTF::move(fence))
    , m_dmabuf(WTF::move(dmabuf))
{
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, UnixFileDescriptor&& fenceFD)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, nullptr)
    , m_dmabuf(WTF::move(dmabuf))
    , m_fenceFD(WTF::move(fenceFD))
{
}

CoordinatedPlatformLayerBufferDMABuf::~CoordinatedPlatformLayerBufferDMABuf() = default;

static RefPtr<BitmapTexture> importToTexture(const IntSize& textureSize, const DMABufBuffer::Attributes& dmaBufAttributes, OptionSet<BitmapTexture::Flags> textureFlags)
{
    auto& display = PlatformDisplay::sharedDisplay();
    auto image = DMABufBuffer::createEGLImage(display.glDisplay(), dmaBufAttributes);
    if (!image)
        return nullptr;

    auto texture = BitmapTexturePool::singleton().createTextureForImage(image, textureSize, textureFlags);
    display.destroyEGLImage(image);
    return texture;
}

static bool formatIsYUV(uint32_t fourcc)
{
    switch (fourcc) {
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_YUV411:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_AYUV:
    case DRM_FORMAT_P010:
        return true;
    }

    return false;
}

struct YUVPlaneInfo {
    uint32_t fourcc;
    unsigned index;
    unsigned offset;
    IntSize subsampling;
};

static const HashMap<uint32_t, Vector<YUVPlaneInfo>>& yuvFormatPlaneInfo()
{
    static NeverDestroyed<HashMap<uint32_t, Vector<YUVPlaneInfo>>> yuvFormatsMap = [] {
        HashMap<uint32_t, Vector<YUVPlaneInfo>> map;
        // 1 plane formats.
        map.set(DRM_FORMAT_AYUV, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_ABGR8888, 0, 0, { 1, 1 } },
        });

        // 2 plane formats.
        map.set(DRM_FORMAT_NV12, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_GR88, 1, 0, { 2, 2 } },
        });
        map.set(DRM_FORMAT_NV21, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_GR88, 1, 1, { 2, 2 } },
        });
        map.set(DRM_FORMAT_P010, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R16, 0, 0, { 1, 1 } },
            { DRM_FORMAT_GR1616, 1, 0, { 2, 2 } },
        });

        // 3 plane formats.
        map.set(DRM_FORMAT_YUV420, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_R8, 1, 0, { 2, 2 } },
            { DRM_FORMAT_R8, 2, 0, { 2, 2 } },
        });
        map.set(DRM_FORMAT_YVU420, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_R8, 2, 0, { 2, 2 } },
            { DRM_FORMAT_R8, 1, 0, { 2, 2 } },
        });
        map.set(DRM_FORMAT_YUV444, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_R8, 1, 0, { 1, 1 } },
            { DRM_FORMAT_R8, 2, 0, { 1, 1 } },
        });
        map.set(DRM_FORMAT_YUV411, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_R8, 1, 0, { 4, 1 } },
            { DRM_FORMAT_R8, 2, 0, { 4, 1 } },
        });
        map.set(DRM_FORMAT_YUV422, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, 0, 0, { 1, 1 } },
            { DRM_FORMAT_R8, 1, 0, { 2, 1 } },
            { DRM_FORMAT_R8, 2, 0, { 2, 1 } },
        });
        return map;
    }();
    return yuvFormatsMap;
}

static CoordinatedPlatformLayerBufferYUV::Format yuvFormatFromDRMFourcc(uint32_t fourcc)
{
    switch (fourcc) {
    case DRM_FORMAT_AYUV:
        return CoordinatedPlatformLayerBufferYUV::Format::AYUV;
    case DRM_FORMAT_NV12:
        return CoordinatedPlatformLayerBufferYUV::Format::NV12;
    case DRM_FORMAT_P010:
        return CoordinatedPlatformLayerBufferYUV::Format::P010;
    case DRM_FORMAT_YUV420:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV420;
    case DRM_FORMAT_YVU420:
        return CoordinatedPlatformLayerBufferYUV::Format::YVU420;
    case DRM_FORMAT_YUV444:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV444;
    case DRM_FORMAT_YUV411:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV411;
    case DRM_FORMAT_YUV422:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV422;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importYUV() const
{
    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

    Vector<RefPtr<BitmapTexture>, 4> textures;
    std::array<unsigned, 4> yuvPlane;
    std::array<unsigned, 4> yuvPlaneOffset;

    const auto& attributes = m_dmabuf->attributes();
    const auto& iter = yuvFormatPlaneInfo().find(attributes.fourcc.value);
    if (iter == yuvFormatPlaneInfo().end())
        return nullptr;

    const auto& planeInfo = iter->value;
    for (unsigned i = 0; i < planeInfo.size(); ++i) {
        const auto& plane = planeInfo[i];
        IntSize adjustedSize { attributes.size.width() / plane.subsampling.width(), attributes.size.height() / plane.subsampling.height() };
        auto planeFds = Vector<UnixFileDescriptor>::from(attributes.fds[i].borrow());
        DMABufBuffer::Attributes planeAttributes { adjustedSize, plane.fourcc, WTF::move(planeFds), { attributes.offsets[i] }, { attributes.strides[i] }, attributes.modifier };
        auto texture = importToTexture(attributes.size, planeAttributes, textureFlags);
        if (!texture)
            return nullptr;
        textures.append(WTF::move(texture));
        yuvPlane[i] = plane.index;
        yuvPlaneOffset[i] = plane.offset;
    }

    if (textures.isEmpty())
        return nullptr;

    auto format = yuvFormatFromDRMFourcc(attributes.fourcc.value);

    CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace yuvToRgbColorSpace;
    switch (m_dmabuf->colorSpace().value_or(DMABufBuffer::ColorSpace::Bt601)) {
    case DMABufBuffer::ColorSpace::Bt601:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt601;
        break;
    case DMABufBuffer::ColorSpace::Bt709:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt709;
        break;
    case DMABufBuffer::ColorSpace::Bt2020:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt2020;
        break;
    case DMABufBuffer::ColorSpace::Smpte240M:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Smpte240M;
        break;
    }

    CoordinatedPlatformLayerBufferYUV::TransferFunction transferFunction;
    switch (m_dmabuf->transferFunction().value_or(DMABufBuffer::TransferFunction::Bt709)) {
    case DMABufBuffer::TransferFunction::Bt709:
        transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Bt709;
        break;
    case DMABufBuffer::TransferFunction::Pq:
        transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Pq;
        break;
    }

    unsigned numberOfPlanes = textures.size();
    return CoordinatedPlatformLayerBufferYUV::create(format, numberOfPlanes, WTF::move(textures), WTF::move(yuvPlane), WTF::move(yuvPlaneOffset), yuvToRgbColorSpace, transferFunction, m_size, m_flags, nullptr);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importDMABuf() const
{
    const auto& attributes = m_dmabuf->attributes();
    if (formatIsYUV(attributes.fourcc.value))
        return importYUV();

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = importToTexture(attributes.size, attributes, textureFlags);
    return texture ? CoordinatedPlatformLayerBufferRGB::create(texture.releaseNonNull(), m_flags, nullptr) : nullptr;
}

void CoordinatedPlatformLayerBufferDMABuf::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    if (m_fenceFD) {
        if (auto fence = GLFence::importFD(PlatformDisplay::sharedDisplay().glDisplay(), WTF::move(m_fenceFD)))
            fence->serverWait();
    }

    if (!m_dmabuf->buffer())
        m_dmabuf->setBuffer(importDMABuf());

    if (auto* buffer = m_dmabuf->buffer())
        buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

#if USE(SKIA)
sk_sp<SkImage> CoordinatedPlatformLayerBufferDMABuf::skiaImage()
{
    waitForContentsIfNeeded();

    if (m_fenceFD) {
        if (auto fence = GLFence::importFD(PlatformDisplay::sharedDisplay().glDisplay(), WTF::move(m_fenceFD)))
            fence->serverWait();
    }

    if (!m_dmabuf->buffer())
        m_dmabuf->setBuffer(importDMABuf());

    if (auto* buffer = m_dmabuf->buffer())
        return buffer->skiaImage();

    return nullptr;
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
