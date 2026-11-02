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

#if USE(COORDINATED_GRAPHICS) && USE(TEXTURE_MAPPER) && USE(GBM)
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayerBufferExternalOES.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "CoordinatedPlatformLayerBufferYUV.h"
#include "DMABufBuffer.h"
#include "DRMDeviceManager.h"
#include "GBMDevice.h"
#include "GBMVersioning.h"
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include "TextureMapper.h"
#include <cerrno>
#include <drm_fourcc.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

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

static RefPtr<BitmapTexture> importToTexture(const IntSize& textureSize, const DMABufBuffer::Attributes& dmaBufAttributes, OptionSet<BitmapTexture::Flags> textureFlags, std::optional<DMABufBuffer::ColorSpace> colorSpace = std::nullopt, std::optional<DMABufBuffer::SampleRange> sampleRange = std::nullopt)
{
    auto& display = PlatformDisplay::sharedDisplay();
    EGLImage image = EGL_NO_IMAGE;
    if (colorSpace && sampleRange) {
        textureFlags.add(BitmapTexture::Flags::ExternalOESRenderTarget);
#if USE(GSTREAMER)
        image = DMABufBuffer::createEGLImageForQualcommVideoFrame(display.glDisplay(), dmaBufAttributes, *colorSpace, *sampleRange);
#endif
    } else
        image = DMABufBuffer::createEGLImage(display.glDisplay(), dmaBufAttributes);
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
    case DRM_FORMAT_NV21:
        return CoordinatedPlatformLayerBufferYUV::Format::NV21;
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

static unsigned bytesPerSampleForPlaneFourcc(uint32_t fourcc)
{
    switch (fourcc) {
    case DRM_FORMAT_R8:
        return 1;
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R16:
        return 2;
    case DRM_FORMAT_GR1616:
    case DRM_FORMAT_ABGR8888:
        return 4;
    }
    return 0;
}

// A plane whose byte stride is a multiple of 16 but not of the GPU's stricter standalone
// alignment is rejected when imported on its own, while the decoder keeps exporting that
// stride for the stream. Remember the rejected {format, modifier, plane, stride} so later
// frames skip the doomed import and repack straight away, and the GPU error is logged once.
struct YUVPlaneImportFailure {
    uint32_t fourcc;
    uint64_t modifier;
    unsigned planeIndex;
    uint32_t stride;

    friend bool operator==(const YUVPlaneImportFailure&, const YUVPlaneImportFailure&) = default;
};

static Lock s_yuvPlaneImportFailuresLock;
static Vector<YUVPlaneImportFailure>& yuvPlaneImportFailures() WTF_REQUIRES_LOCK(s_yuvPlaneImportFailuresLock)
{
    static NeverDestroyed<Vector<YUVPlaneImportFailure>> failures;
    return failures;
}

static bool yuvPlaneImportKnownToFail(const YUVPlaneImportFailure& key)
{
    Locker locker { s_yuvPlaneImportFailuresLock };
    return yuvPlaneImportFailures().contains(key);
}

static void markYUVPlaneImportFailed(const YUVPlaneImportFailure& key)
{
    Locker locker { s_yuvPlaneImportFailuresLock };
    if (!yuvPlaneImportFailures().contains(key))
        yuvPlaneImportFailures().append(key);
}

static RefPtr<BitmapTexture> importRepackedPlane(const IntSize& textureSize, const IntSize& planeSize, uint32_t planeFourcc, std::span<const uint8_t> sourceData, unsigned sourceStride, OptionSet<BitmapTexture::Flags> textureFlags)
{
    unsigned bytesPerSample = bytesPerSampleForPlaneFourcc(planeFourcc);
    if (!bytesPerSample || planeSize.isEmpty() || !sourceStride || sourceData.empty())
        return nullptr;

    RefPtr gbmDevice = DRMDeviceManager::singleton().mainGBMDevice(DRMDeviceManager::NodeType::Render);
    if (!gbmDevice || !gbmDevice->device())
        return nullptr;

    auto* bo = gbm_bo_create(gbmDevice->device(), planeSize.width(), planeSize.height(), planeFourcc, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
    if (!bo)
        return nullptr;

    auto attributes = DMABufBufferAttributes::fromGBMBufferObject(bo);
    if (!attributes || attributes->fds.isEmpty() || attributes->strides.isEmpty()) {
        gbm_bo_destroy(bo);
        return nullptr;
    }

    // The buffer object is allocated at the plane's visible dimensions but its row stride is
    // padded to the GPU's alignment. Import it with the visible width and height so the sampler
    // reads the picture, not the padding; the padded stride stays the pitch only.
    attributes->size = planeSize;

    uint32_t mappedStride = 0;
    void* mapData = nullptr;
    void* mapped = gbm_bo_map(bo, 0, 0, planeSize.width(), planeSize.height(), GBM_BO_TRANSFER_WRITE, &mappedStride, &mapData);
    if (!mapped || mapped == MAP_FAILED) {
        gbm_bo_destroy(bo);
        return nullptr;
    }

    int syncFD = attributes->fds[0].value();
    auto dmaBufSync = [&](uint64_t flags) {
        struct dma_buf_sync sync;
        zeroBytes(sync);
        sync.flags = flags;
        unsigned counter = 0;
        int result;
        do {
            result = ioctl(syncFD, DMA_BUF_IOCTL_SYNC, &sync);
        } while (result == -1 && (errno == EAGAIN || errno == EINTR) && counter++ < 10);
    };

    // Copy the full visible plane height. The source span reported by GStreamer can describe
    // fewer rows than the plane holds, so read directly from the plane base for the visible
    // height: the decoded buffer always contains at least the visible rows, and leaving any
    // trailing row unwritten renders as a green band.
    const unsigned rows = planeSize.height();
    const unsigned validBytesPerRow = std::min<unsigned>(planeSize.width() * bytesPerSample, std::min<unsigned>(sourceStride, mappedStride));
    auto source = unsafeMakeSpan(sourceData.data(), static_cast<size_t>(rows) * sourceStride);

    dmaBufSync(DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE);
    auto dst = unsafeMakeSpan(static_cast<uint8_t*>(mapped), static_cast<size_t>(mappedStride) * rows);
    for (unsigned row = 0; row < rows; ++row)
        memcpySpan(dst.subspan(static_cast<size_t>(row) * mappedStride, validBytesPerRow), source.subspan(static_cast<size_t>(row) * sourceStride, validBytesPerRow));
    dmaBufSync(DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE);
    gbm_bo_unmap(bo, mapData);

    auto texture = importToTexture(textureSize, *attributes, textureFlags);
    gbm_bo_destroy(bo);
    return texture;
}

static std::unique_ptr<CoordinatedPlatformLayerBuffer> importYUV(const Ref<DMABufBuffer>& dmabuf, OptionSet<TextureMapperFlags> flags, const CoordinatedPlatformLayerBufferDMABuf::YUVPlaneSourceFunction& planeSource, bool& containsCopy)
{
    containsCopy = false;
    OptionSet<BitmapTexture::Flags> textureFlags;
    if (flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

    Vector<RefPtr<BitmapTexture>, 4> textures;
    std::array<unsigned, 4> yuvPlane;
    std::array<unsigned, 4> yuvPlaneOffset;

    const auto& attributes = dmabuf->attributes();
    const auto& iter = yuvFormatPlaneInfo().find(attributes.fourcc.value);
    if (iter == yuvFormatPlaneInfo().end())
        return nullptr;

    const auto& planeInfo = iter->value;
    for (unsigned i = 0; i < planeInfo.size(); ++i) {
        const auto& plane = planeInfo[i];
        IntSize adjustedSize { attributes.size.width() / plane.subsampling.width(), attributes.size.height() / plane.subsampling.height() };

        YUVPlaneImportFailure failureKey { plane.fourcc, attributes.modifier, i, attributes.strides[i] };
        bool knownToFail = yuvPlaneImportKnownToFail(failureKey);

        RefPtr<BitmapTexture> texture;
        if (!knownToFail) {
            auto planeFds = Vector<UnixFileDescriptor>::from(attributes.fds[i].borrow());
            DMABufBuffer::Attributes planeAttributes { adjustedSize, plane.fourcc, WTF::move(planeFds), { attributes.offsets[i] }, { attributes.strides[i] }, attributes.modifier };
            texture = importToTexture(attributes.size, planeAttributes, textureFlags);
            if (!texture)
                markYUVPlaneImportFailed(failureKey);
        }

        if (!texture) {
            if (!planeSource)
                return nullptr;
            unsigned sourceStride = 0;
            auto sourceData = planeSource(i, sourceStride);
            texture = importRepackedPlane(attributes.size, adjustedSize, plane.fourcc, sourceData, sourceStride, textureFlags);
            if (texture)
                containsCopy = true;
        }
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
    switch (dmabuf->colorSpace().value_or(DMABufBuffer::ColorSpace::Bt601)) {
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
    switch (dmabuf->transferFunction().value_or(DMABufBuffer::TransferFunction::Bt709)) {
    case DMABufBuffer::TransferFunction::Bt709:
        transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Bt709;
        break;
    case DMABufBuffer::TransferFunction::Pq:
        transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Pq;
        break;
    }

    unsigned numberOfPlanes = textures.size();
    return CoordinatedPlatformLayerBufferYUV::create(format, numberOfPlanes, WTF::move(textures), WTF::move(yuvPlane), WTF::move(yuvPlaneOffset),
        yuvToRgbColorSpace, transferFunction, attributes.size, flags, nullptr);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importDMABuf(bool& containsCopy) const
{
    containsCopy = false;
    bool importWithHints = m_dmabuf->colorSpace() && m_dmabuf->sampleRange();
    const auto& attributes = m_dmabuf->attributes();
    if (!importWithHints && formatIsYUV(attributes.fourcc.value))
        return importYUV(m_dmabuf, m_flags, m_yuvPlaneSource, containsCopy);

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = importToTexture(attributes.size, attributes, textureFlags, m_dmabuf->colorSpace(), m_dmabuf->sampleRange());
    if (!texture)
        return nullptr;

    if (importWithHints)
        return CoordinatedPlatformLayerBufferExternalOES::create(texture.releaseNonNull(), m_flags, nullptr);
    return CoordinatedPlatformLayerBufferRGB::create(texture.releaseNonNull(), m_flags, nullptr);
}

void CoordinatedPlatformLayerBufferDMABuf::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    if (m_fenceFD) {
        if (auto fence = GLFence::importFD(PlatformDisplay::sharedDisplay().glDisplay(), WTF::move(m_fenceFD)))
            fence->serverWait();
    }

    // A zero-copy import aliases the dma-buf, so it is cached on the shared DMABufBuffer and
    // re-sampled as the decoder rewrites the recurring pooled buffer. A copied import is a
    // one-time snapshot of this frame's content, so it is held per layer buffer (one per
    // GstSample) and never shared, otherwise the pool's buffers would replay stale frames.
    if (!m_copiedBuffer && !m_dmabuf->buffer()) {
        bool containsCopy = false;
        auto buffer = importDMABuf(containsCopy);
        if (containsCopy)
            m_copiedBuffer = WTF::move(buffer);
        else
            m_dmabuf->setBuffer(WTF::move(buffer));
    }

    auto* buffer = m_copiedBuffer ? m_copiedBuffer.get() : m_dmabuf->buffer();
    if (buffer)
        buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(TEXTURE_MAPPER) && USE(GBM)
