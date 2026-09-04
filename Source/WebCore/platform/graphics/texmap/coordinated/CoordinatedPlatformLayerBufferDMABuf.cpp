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
#include "GLContext.h"
#include "PlatformDisplay.h"
#include <drm_fourcc.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(TEXTURE_MAPPER)
#include "TextureMapper.h"
#endif

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkYUVAInfo.h>
#include <skia/gpu/ganesh/GrYUVABackendTextures.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/private/chromium/GrPromiseImageTexture.h>
#include <skia/private/chromium/SkImageChromium.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

#if USE(TEXTURE_MAPPER)
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

#else

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTF::move(dmabuf), flags, WTF::move(fence), threadSafeGrContext);
}

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, UnixFileDescriptor&& fenceFD, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTF::move(dmabuf), flags, WTF::move(fenceFD), threadSafeGrContext);
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, WTF::move(fence))
    , m_dmabuf(WTF::move(dmabuf))
{
    createSkiaImageIfNeeded(threadSafeGrContext);
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, UnixFileDescriptor&& fenceFD, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, nullptr)
    , m_dmabuf(WTF::move(dmabuf))
    , m_fenceFD(WTF::move(fenceFD))
{
    createSkiaImageIfNeeded(threadSafeGrContext);
}
#endif

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

static std::unique_ptr<CoordinatedPlatformLayerBuffer> importYUV(const Ref<DMABufBuffer>& dmabuf, OptionSet<TextureMapperFlags> flags)
{
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

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importDMABuf() const
{
    const auto& attributes = m_dmabuf->attributes();
    if (formatIsYUV(attributes.fourcc.value))
        return importYUV(m_dmabuf, m_flags);

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = importToTexture(attributes.size, attributes, textureFlags);
    return texture ? CoordinatedPlatformLayerBufferRGB::create(texture.releaseNonNull(), m_flags, nullptr) : nullptr;
}

#if USE(TEXTURE_MAPPER)
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

#else

class PromiseDMABufImageContext final : public ThreadSafeRefCounted<PromiseDMABufImageContext> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PromiseDMABufImageContext);
public:
    static Ref<PromiseDMABufImageContext> create(Ref<DMABufBuffer>&& buffer, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& glFence, WTF::UnixFileDescriptor&& fd)
    {
        return adoptRef(*new PromiseDMABufImageContext(WTF::move(buffer), flags, WTF::move(glFence), WTF::move(fd)));
    }

    ~PromiseDMABufImageContext() = default;

    sk_sp<GrPromiseImageTexture> promiseImageTexture()
    {
        if (!makeContextCurrentAndWaitForContentsIfNeeded())
            return nullptr;

        const auto& attributes = m_dmabuf->attributes();
        if (!m_dmabuf->buffer()) {
            std::unique_ptr<CoordinatedPlatformLayerBuffer> buffer;
            OptionSet<BitmapTexture::Flags> textureFlags;
            if (m_flags.contains(TextureMapperFlags::ShouldBlend))
                textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
            if (auto texture = importToTexture(attributes.size, attributes, textureFlags))
                buffer = CoordinatedPlatformLayerBufferRGB::create(texture.releaseNonNull(), { }, nullptr);
            m_dmabuf->setBuffer(WTF::move(buffer));
        }

        auto* buffer = m_dmabuf->buffer();
        if (!is<CoordinatedPlatformLayerBufferRGB>(buffer))
            return nullptr;

        return createPromiseImageTexture(downcast<CoordinatedPlatformLayerBufferRGB>(*buffer).textureID(), GL_RGBA8, attributes.size);
    }

    sk_sp<GrPromiseImageTexture> promiseImageTextureForPlane(size_t planeIndex, unsigned planeFormat, const IntSize& planeSize)
    {
        if (!makeContextCurrentAndWaitForContentsIfNeeded())
            return nullptr;

        if (!m_dmabuf->buffer())
            m_dmabuf->setBuffer(importYUV(m_dmabuf, m_flags));

        auto* buffer = m_dmabuf->buffer();
        if (!is<CoordinatedPlatformLayerBufferYUV>(buffer))
            return nullptr;

        auto texture = downcast<CoordinatedPlatformLayerBufferYUV>(*buffer).texture(planeIndex);
        if (!texture)
            return nullptr;

        return createPromiseImageTexture(texture->id(), planeFormat, planeSize);
    }

private:
    PromiseDMABufImageContext(Ref<DMABufBuffer>&& buffer, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& glFence, WTF::UnixFileDescriptor&& fd)
        : m_dmabuf(WTF::move(buffer))
        , m_flags(flags)
        , m_fence(WTF::move(glFence))
        , m_fenceFD(WTF::move(fd))
    {
    }

    bool makeContextCurrentAndWaitForContentsIfNeeded()
    {
        auto& display = PlatformDisplay::sharedDisplay();
        auto* glContext = display.skiaGLContext();
        if (!glContext || !glContext->makeContextCurrent())
            return false;

        if (auto glFence = WTF::move(m_fence))
            glFence->serverWait();
        else if (m_fenceFD) {
            if (auto glFence = GLFence::importFD(display.glDisplay(), WTF::move(m_fenceFD)))
                glFence->serverWait();
        }

        return true;
    }

    sk_sp<GrPromiseImageTexture> createPromiseImageTexture(unsigned id, unsigned format, const IntSize& size)
    {
        GrGLTextureInfo externalTexture;
        externalTexture.fTarget = GL_TEXTURE_2D;
        externalTexture.fID = id;
        externalTexture.fFormat = format;
        auto backendTexture = GrBackendTextures::MakeGL(size.width(), size.height(), skgpu::Mipmapped::kNo, externalTexture);
        return GrPromiseImageTexture::Make(backendTexture);
    }

    const Ref<DMABufBuffer> m_dmabuf;
    OptionSet<TextureMapperFlags> m_flags;
    std::unique_ptr<GLFence> m_fence;
    WTF::UnixFileDescriptor m_fenceFD;
};

struct PromiseDMABufYUVPlaneContext {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(PromiseDMABufYUVPlaneContext);

    PromiseDMABufYUVPlaneContext(Ref<PromiseDMABufImageContext>&& dmabufContext, size_t planeIndex, unsigned glFormat, int width, int height)
        : context(WTF::move(dmabufContext))
        , index(planeIndex)
        , format(glFormat)
        , size(width, height)
    {
    }

    Ref<PromiseDMABufImageContext> context;
    size_t index { 0 };
    unsigned format { 0 };
    IntSize size;
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(PromiseDMABufYUVPlaneContext);

void CoordinatedPlatformLayerBufferDMABuf::createSkiaImageIfNeeded(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    if (!threadSafeGrContext)
        return;

    auto backendFormat = threadSafeGrContext->defaultBackendFormat(kRGBA_8888_SkColorType, GrRenderable::kYes);
    ASSERT(backendFormat.isValid());

    Ref context = PromiseDMABufImageContext::create(m_dmabuf.copyRef(), m_flags, WTF::move(m_fence), WTF::move(m_fenceFD));

    const auto& attributes = m_dmabuf->attributes();
    if (formatIsYUV(attributes.fourcc.value)) {
        auto planeConfig = SkYUVAInfo::PlaneConfig::kUnknown;
        auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
        std::array<GrBackendFormat, 4> backendFormats;
        std::array<PromiseDMABufYUVPlaneContext*, 4> planeContexts;

        auto setPlaneFormatAndContext = [&](size_t index, unsigned glFormat, int width, int height) {
            backendFormats[index] = GrBackendFormats::MakeGL(glFormat, GL_TEXTURE_2D);
            planeContexts[index] = makeUnique<PromiseDMABufYUVPlaneContext>(context.copyRef(), index, glFormat, width, height).release();
        };

        auto format = yuvFormatFromDRMFourcc(attributes.fourcc.value);
        switch (format) {
        case CoordinatedPlatformLayerBufferYUV::Format::AYUV:
            planeConfig = SkYUVAInfo::PlaneConfig::kYUVA;
            subsampling = SkYUVAInfo::Subsampling::k444;
            setPlaneFormatAndContext(0, GL_RGBA8, attributes.size.width(), attributes.size.height());
            break;
        case CoordinatedPlatformLayerBufferYUV::Format::NV12:
        case CoordinatedPlatformLayerBufferYUV::Format::NV21:
        case CoordinatedPlatformLayerBufferYUV::Format::P010: {
            planeConfig = format == CoordinatedPlatformLayerBufferYUV::Format::NV21 ? SkYUVAInfo::PlaneConfig::kY_VU : SkYUVAInfo::PlaneConfig::kY_UV;
            subsampling = SkYUVAInfo::Subsampling::k420;
            setPlaneFormatAndContext(0, format == CoordinatedPlatformLayerBufferYUV::Format::P010 ? GL_R16 : GL_R8, attributes.size.width(), attributes.size.height());
            setPlaneFormatAndContext(1, format == CoordinatedPlatformLayerBufferYUV::Format::P010 ? GL_RG16 : GL_RG8, attributes.size.width() / 2, attributes.size.height() / 2);
            break;
        }
        case CoordinatedPlatformLayerBufferYUV::Format::YUV420:
        case CoordinatedPlatformLayerBufferYUV::Format::YVU420:
            planeConfig = format == CoordinatedPlatformLayerBufferYUV::Format::YVU420 ? SkYUVAInfo::PlaneConfig::kY_V_U : SkYUVAInfo::PlaneConfig::kY_U_V;
            subsampling = SkYUVAInfo::Subsampling::k420;
            setPlaneFormatAndContext(0, GL_R8, attributes.size.width(), attributes.size.height());
            setPlaneFormatAndContext(1, GL_R8, attributes.size.width() / 2, attributes.size.height() / 2);
            setPlaneFormatAndContext(2, GL_R8, attributes.size.width() / 2, attributes.size.height() / 2);
            break;
        case CoordinatedPlatformLayerBufferYUV::Format::YUV444:
            planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
            subsampling = SkYUVAInfo::Subsampling::k444;
            setPlaneFormatAndContext(0, GL_R8, attributes.size.width(), attributes.size.height());
            setPlaneFormatAndContext(1, GL_R8, attributes.size.width(), attributes.size.height());
            setPlaneFormatAndContext(2, GL_R8, attributes.size.width(), attributes.size.height());
            break;
        case CoordinatedPlatformLayerBufferYUV::Format::YUV411:
            planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
            subsampling = SkYUVAInfo::Subsampling::k411;
            setPlaneFormatAndContext(0, GL_R8, attributes.size.width(), attributes.size.height());
            setPlaneFormatAndContext(1, GL_R8, attributes.size.width() / 4, attributes.size.height());
            setPlaneFormatAndContext(2, GL_R8, attributes.size.width() / 4, attributes.size.height());
            break;
        case CoordinatedPlatformLayerBufferYUV::Format::YUV422:
            planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
            subsampling = SkYUVAInfo::Subsampling::k422;
            setPlaneFormatAndContext(0, GL_R8, attributes.size.width(), attributes.size.height());
            setPlaneFormatAndContext(1, GL_R8, attributes.size.width() / 2, attributes.size.height());
            setPlaneFormatAndContext(2, GL_R8, attributes.size.width() / 2, attributes.size.height());
            break;
        case CoordinatedPlatformLayerBufferYUV::Format::A420:
            // Not supported in DMA-BUF.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        if (planeConfig == SkYUVAInfo::PlaneConfig::kUnknown) {
            LOG_ERROR("Failed to create Skia image for DMA-BUF YUV video buffer: unknown plane configuration");
            return;
        }

        SkYUVColorSpace yuvaColorSpace = [&] {
            switch (m_dmabuf->colorSpace().value_or(DMABufBuffer::ColorSpace::Bt601)) {
            case DMABufBuffer::ColorSpace::Bt601:
                return kRec601_Limited_SkYUVColorSpace;
            case DMABufBuffer::ColorSpace::Bt709:
                return kRec709_Full_SkYUVColorSpace;
            case DMABufBuffer::ColorSpace::Bt2020:
                return kBT2020_8bit_Full_SkYUVColorSpace;
            case DMABufBuffer::ColorSpace::Smpte240M:
                return kSMPTE240_Full_SkYUVColorSpace;
            }
            RELEASE_ASSERT_NOT_REACHED();
        }();

        SkYUVAInfo info(SkISize::Make(m_size.width(), m_size.height()), planeConfig, subsampling, yuvaColorSpace);
        auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
        GrYUVABackendTextureInfo yuvaBackendTexturesInfo(info, backendFormats.data(), skgpu::Mipmapped::kNo, origin);
        if (!yuvaBackendTexturesInfo.isValid()) {
            LOG_ERROR("Failed to create Skia image for DMA-BUF YUV video buffer: invalid backend texture information");
            return;
        }

        sk_sp<SkColorSpace> colorSpace = [&] {
            switch (m_dmabuf->transferFunction().value_or(DMABufBuffer::TransferFunction::Bt709)) {
            case DMABufBuffer::TransferFunction::Bt709:
                return SkColorSpace::MakeRGB(SkNamedTransferFn::kRec709, SkNamedGamut::kSRGB);
            case DMABufBuffer::TransferFunction::Pq:
                return SkColorSpace::MakeRGB(SkNamedTransferFn::kPQ, SkNamedGamut::kRec2020);
            }
            RELEASE_ASSERT_NOT_REACHED();
        }();

        m_image = SkImages::PromiseTextureFromYUVA(threadSafeGrContext, yuvaBackendTexturesInfo, colorSpace,
            +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
                auto& planeContext = *static_cast<PromiseDMABufYUVPlaneContext*>(userData);
                return planeContext.context->promiseImageTextureForPlane(planeContext.index, planeContext.format, planeContext.size);
            },
            +[](void* userData) {
                std::unique_ptr<PromiseDMABufYUVPlaneContext> planeContext(static_cast<PromiseDMABufYUVPlaneContext*>(userData));
            }, reinterpret_cast<void**>(planeContexts.data()));
    } else {
        auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
        auto alphaType = m_flags.contains(TextureMapperFlags::ShouldBlend) ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
        m_image = SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, SkISize::Make(attributes.size.width(), attributes.size.height()), skgpu::Mipmapped::kNo,
            origin, kRGBA_8888_SkColorType, alphaType, SkColorSpace::MakeSRGB(),
            +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
                auto& context = *static_cast<PromiseDMABufImageContext*>(userData);
                return context.promiseImageTexture();
            },
            +[](void* userData) {
                Ref context = adoptRef(*static_cast<PromiseDMABufImageContext*>(userData));
            }, &context.leakRef());
    }
}

sk_sp<SkImage> CoordinatedPlatformLayerBufferDMABuf::skiaImage()
{
    if (m_image)
        return m_image;

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
