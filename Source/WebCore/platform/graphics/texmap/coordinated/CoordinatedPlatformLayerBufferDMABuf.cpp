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
#include "BitmapTexture.h"
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
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTFMove(dmabuf), flags, WTFMove(fence));
}

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, UnixFileDescriptor&& fenceFD)
{
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTFMove(dmabuf), flags, WTFMove(fenceFD));
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, WTFMove(fence))
    , m_dmabuf(WTFMove(dmabuf))
{
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, UnixFileDescriptor&& fenceFD)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, nullptr)
    , m_dmabuf(WTFMove(dmabuf))
    , m_fenceFD(WTFMove(fenceFD))
{
}

CoordinatedPlatformLayerBufferDMABuf::~CoordinatedPlatformLayerBufferDMABuf() = default;

static RefPtr<BitmapTexture> importToTexture(const IntSize& size, const IntSize& subsampling, uint32_t fourcc, const Vector<int>& fds, const Vector<uint32_t>& offsets, const Vector<uint32_t>& strides, uint64_t modifier, OptionSet<BitmapTexture::Flags> textureFlags, TextureMapper& textureMapper)
{
    auto& display = PlatformDisplay::sharedDisplay();
    Vector<EGLAttrib> attributes = {
        EGL_WIDTH, size.width() / subsampling.width(),
        EGL_HEIGHT, size.height() / subsampling.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(fourcc)
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, fds[planeIndex], \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLAttrib>(offsets[planeIndex]), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLAttrib>(strides[planeIndex]) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (modifier != DRM_FORMAT_MOD_INVALID && display.eglExtensions().EXT_image_dma_buf_import_modifiers) { \
        std::array<EGLAttrib, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLAttrib>(modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLAttrib>(modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLAttrib> { modifierAttributes }); \
    } \
    }

    auto planeCount = fds.size();
    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBUTES

    attributes.append(EGL_NONE);

    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!image)
        return nullptr;

    auto texture = textureMapper.acquireTextureFromPool(size, textureFlags);
    glBindTexture(GL_TEXTURE_2D, texture->id());
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
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

static const UncheckedKeyHashMap<uint32_t, Vector<YUVPlaneInfo>>& yuvFormatPlaneInfo()
{
    static NeverDestroyed<UncheckedKeyHashMap<uint32_t, Vector<YUVPlaneInfo>>> yuvFormatsMap = [] {
        UncheckedKeyHashMap<uint32_t, Vector<YUVPlaneInfo>> map;
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

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importYUV(TextureMapper& textureMapper) const
{
    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

    Vector<RefPtr<BitmapTexture>, 4> textures;
    std::array<unsigned, 4> yuvPlane;
    std::array<unsigned, 4> yuvPlaneOffset;

    const auto& attributes = m_dmabuf->attributes();
    const auto& iter = yuvFormatPlaneInfo().find(attributes.fourcc);
    if (iter == yuvFormatPlaneInfo().end())
        return nullptr;

    const auto& planeInfo = iter->value;
    for (unsigned i = 0; i < planeInfo.size(); ++i) {
        const auto& plane = planeInfo[i];
        auto texture = importToTexture(attributes.size, plane.subsampling, plane.fourcc, { attributes.fds[i].value() }, { attributes.offsets[i] }, { attributes.strides[i] }, attributes.modifier, textureFlags, textureMapper);
        if (!texture)
            return nullptr;
        textures.append(WTFMove(texture));
        yuvPlane[i] = plane.index;
        yuvPlaneOffset[i] = plane.offset;
    }

    if (textures.isEmpty())
        return nullptr;

    CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace yuvToRgbColorSpace;
    switch (m_dmabuf->colorSpace().value_or(DMABufBuffer::ColorSpace::BT601)) {
    case DMABufBuffer::ColorSpace::BT601:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::BT601;
        break;
    case DMABufBuffer::ColorSpace::BT709:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::BT709;
        break;
    case DMABufBuffer::ColorSpace::BT2020:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::BT2020;
        break;
    case DMABufBuffer::ColorSpace::SMPTE240M:
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::SMPTE240M;
        break;
    }

    unsigned numberOfPlanes = textures.size();
    return CoordinatedPlatformLayerBufferYUV::create(numberOfPlanes, WTFMove(textures), WTFMove(yuvPlane), WTFMove(yuvPlaneOffset), yuvToRgbColorSpace, m_size, m_flags, nullptr);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importDMABuf(TextureMapper& textureMapper) const
{
    const auto& attributes = m_dmabuf->attributes();
    if (formatIsYUV(attributes.fourcc))
        return importYUV(textureMapper);

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    Vector<int> fds = attributes.fds.map<Vector<int>>([] (const UnixFileDescriptor& fd) {
        return fd.value();
    });
    auto texture = importToTexture(attributes.size, { 1, 1 }, attributes.fourcc, fds, attributes.offsets, attributes.strides, attributes.modifier, textureFlags, textureMapper);
    return texture ? CoordinatedPlatformLayerBufferRGB::create(texture.releaseNonNull(), m_flags, nullptr) : nullptr;
}

void CoordinatedPlatformLayerBufferDMABuf::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    if (m_fenceFD) {
        if (auto fence = GLFence::importFD(WTFMove(m_fenceFD)))
            fence->serverWait();
    }

    if (!m_dmabuf->buffer())
        m_dmabuf->setBuffer(importDMABuf(textureMapper));

    if (auto* buffer = m_dmabuf->buffer())
        buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
