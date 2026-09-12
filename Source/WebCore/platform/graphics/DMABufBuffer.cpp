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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DMABufBuffer.h"

#if USE(GBM)
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayerBuffer.h"
#include "GBMVersioning.h"
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include <atomic>
#include <drm_fourcc.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrYUVABackendTextures.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/private/chromium/GrPromiseImageTexture.h>
#include <skia/private/chromium/SkImageChromium.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace WebCore {

static uint64_t generateID()
{
    static std::atomic<uint64_t> id;
    return ++id;
}

DMABufBuffer::DMABufBuffer(Attributes&& attributes)
    : m_id(generateID())
    , m_attributes(WTF::move(attributes))
{
}

DMABufBuffer::DMABufBuffer(uint64_t id, Attributes&& attributes)
    : m_id(id)
    , m_attributes(WTF::move(attributes))
{
}

DMABufBuffer::~DMABufBuffer() = default;

std::optional<DMABufBuffer::Attributes> DMABufBuffer::takeAttributes()
{
    if (m_attributes.fds.isEmpty())
        return std::nullopt;

    return DMABufBuffer::Attributes { WTF::move(m_attributes.size), std::exchange(m_attributes.fourcc, 0), WTF::move(m_attributes.fds), WTF::move(m_attributes.offsets), WTF::move(m_attributes.strides), std::exchange(m_attributes.modifier, 0) };
}

std::optional<DMABufBufferAttributes> DMABufBufferAttributes::fromGBMBufferObject(struct gbm_bo* bo, EnableModifiers enableModifiers)
{
    if (!bo)
        return std::nullopt;

    int planeCount = gbm_bo_get_plane_count(bo);
    if (planeCount <= 0)
        return std::nullopt;

    DMABufBufferAttributes attributes;
    attributes.size = { static_cast<int>(gbm_bo_get_width(bo)), static_cast<int>(gbm_bo_get_height(bo)) };
    attributes.fourcc = gbm_bo_get_format(bo);
    attributes.modifier = enableModifiers == EnableModifiers::Yes ? gbm_bo_get_modifier(bo) : DRM_FORMAT_MOD_INVALID;

    for (int i = 0; i < planeCount; ++i) {
        int fd = gbm_bo_get_fd_for_plane(bo, i);
        if (fd < 0) {
            LOG_ERROR("DMABufBufferAttributes::fromGBMBufferObject(), failed to export dma-buf for plane %d", i);
            return std::nullopt;
        }
        attributes.fds.append(UnixFileDescriptor { fd, UnixFileDescriptor::Adopt });
        attributes.offsets.append(gbm_bo_get_offset(bo, i));
        attributes.strides.append(gbm_bo_get_stride_for_plane(bo, i));
    }

    return attributes;
}

EGLImage DMABufBuffer::createEGLImage(GLDisplay& display) const
{
    return createEGLImage(display, m_attributes);
}

static std::optional<Vector<EGLAttrib>> buildEGLAttributesForDMABuf(const DMABufBuffer::Attributes& dmaBufAttributes, DMABufBuffer::Attributes::EnableModifiers enableModifiers, std::optional<DMABufBuffer::ColorSpace> colorSpace = std::nullopt, std::optional<DMABufBuffer::SampleRange> sampleRange = std::nullopt)
{
    auto planeCount = dmaBufAttributes.fds.size();
    if (!planeCount || planeCount > DMABufBuffer::Attributes::maxPlaneCountForEGLImage)
        return std::nullopt;

    bool hasModifiers = dmaBufAttributes.modifier != DRM_FORMAT_MOD_INVALID && enableModifiers == DMABufBuffer::Attributes::EnableModifiers::Yes;
    bool hasColorSpaceHint = colorSpace && sampleRange;

    // 6 base attributes + per-plane (6 + optional 4 modifier) + 4 optional color space and sample range + EGL_NONE terminator.
    static constexpr unsigned baseAttributeCount = 6;
    static constexpr unsigned planeAttributeCount = 6;
    static constexpr unsigned modifierAttributeCount = 4;
    static constexpr unsigned colorSpaceAttributeCount = 4;

    Vector<EGLAttrib> eglAttributes;
    eglAttributes.reserveInitialCapacity(baseAttributeCount + planeCount * (planeAttributeCount + (hasModifiers ? modifierAttributeCount : 0)) + (hasColorSpaceHint ? colorSpaceAttributeCount : 0) + 1);

    eglAttributes.appendList<EGLAttrib>({
        EGL_WIDTH, static_cast<EGLAttrib>(dmaBufAttributes.size.width()),
        EGL_HEIGHT, static_cast<EGLAttrib>(dmaBufAttributes.size.height()),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(dmaBufAttributes.fourcc.value)
    });

    static constexpr std::array planeAttributeNames = {
        std::array { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT },
    };

    for (size_t i = 0; i < planeCount; ++i) {
        const auto& names = planeAttributeNames[i];
        eglAttributes.appendList<EGLAttrib>({
            names[0], static_cast<EGLAttrib>(dmaBufAttributes.fds[i].value()),
            names[1], static_cast<EGLAttrib>(dmaBufAttributes.offsets[i]),
            names[2], static_cast<EGLAttrib>(dmaBufAttributes.strides[i])
        });

        if (hasModifiers) {
            eglAttributes.appendList<EGLAttrib>({
                names[3], static_cast<EGLAttrib>(dmaBufAttributes.modifier >> 32),
                names[4], static_cast<EGLAttrib>(dmaBufAttributes.modifier & 0xffffffff)
            });
        }
    }

    if (hasColorSpaceHint) {
        auto colorSpaceHint = [&]() -> EGLAttrib {
            switch (*colorSpace) {
            case DMABufBuffer::ColorSpace::Bt601:
                return EGL_ITU_REC601_EXT;
            case DMABufBuffer::ColorSpace::Bt709:
                return EGL_ITU_REC709_EXT;
            case DMABufBuffer::ColorSpace::Bt2020:
                return EGL_ITU_REC2020_EXT;
            case DMABufBuffer::ColorSpace::Smpte240M:
                // EGL has no token for it.
                break;
            }
            RELEASE_ASSERT_NOT_REACHED();
        }();

        eglAttributes.appendList<EGLAttrib>({
            EGL_YUV_COLOR_SPACE_HINT_EXT, colorSpaceHint,
            EGL_SAMPLE_RANGE_HINT_EXT, *sampleRange == DMABufBuffer::SampleRange::Full ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT,
        });
    }

    eglAttributes.append(EGL_NONE);
    return eglAttributes;
}

EGLImage DMABufBuffer::createEGLImage(GLDisplay& display, const Attributes& dmaBufAttributes, std::optional<ColorSpace> colorSpace, std::optional<SampleRange> sampleRange)
{
    auto enableModifiers = display.extensions().EXT_image_dma_buf_import_modifiers
        ? DMABufBuffer::Attributes::EnableModifiers::Yes : DMABufBuffer::Attributes::EnableModifiers::No;
    auto eglAttributes = buildEGLAttributesForDMABuf(dmaBufAttributes, enableModifiers, colorSpace, sampleRange);
    if (!eglAttributes)
        return EGL_NO_IMAGE;
    auto eglImage = display.createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, *eglAttributes);
    if (!eglImage && colorSpace && sampleRange) [[unlikely]] {
        auto eglAttributes = buildEGLAttributesForDMABuf(dmaBufAttributes, enableModifiers);
        eglImage = display.createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, *eglAttributes);
    }
    return eglImage;
}

std::optional<Vector<EGLint>> DMABufBuffer::buildEGLImageAttributes(const Attributes& dmaBufAttributes, Attributes::EnableModifiers enableModifiers)
{
    auto eglAttributes = buildEGLAttributesForDMABuf(dmaBufAttributes, enableModifiers);
    if (!eglAttributes)
        return std::nullopt;
    return eglAttributes->map<Vector<EGLint>>([](EGLAttrib value) {
        return static_cast<EGLint>(value);
    });
}

#if USE(TEXTURE_MAPPER)
void DMABufBuffer::setBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer)
{
    m_buffer = WTF::move(buffer);
}

#else

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
    unsigned glFormat;
    IntSize subsampling;
};

static const HashMap<uint32_t, Vector<YUVPlaneInfo>>& yuvFormatPlaneInfo()
{
    static NeverDestroyed<HashMap<uint32_t, Vector<YUVPlaneInfo>>> yuvFormatsMap = [] {
        HashMap<uint32_t, Vector<YUVPlaneInfo>> map;
        // 1 plane formats.
        map.set(DRM_FORMAT_AYUV, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_ABGR8888, GL_RGBA8, { 1, 1 } },
        });

        // 2 plane formats.
        map.set(DRM_FORMAT_NV12, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_GR88, GL_RG8, { 2, 2 } },
        });
        map.set(DRM_FORMAT_NV21, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_GR88, GL_RG8, { 2, 2 } },
        });
        map.set(DRM_FORMAT_P010, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R16, GL_R16, { 1, 1 } },
            { DRM_FORMAT_GR1616, GL_RG16, { 2, 2 } },
        });

        // 3 plane formats.
        map.set(DRM_FORMAT_YUV420, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 2, 2 } },
            { DRM_FORMAT_R8, GL_R8, { 2, 2 } },
        });
        map.set(DRM_FORMAT_YVU420, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 2, 2 } },
            { DRM_FORMAT_R8, GL_R8, { 2, 2 } },
        });
        map.set(DRM_FORMAT_YUV444, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
        });
        map.set(DRM_FORMAT_YUV411, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 4, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 4, 1 } },
        });
        map.set(DRM_FORMAT_YUV422, Vector<YUVPlaneInfo> {
            { DRM_FORMAT_R8, GL_R8, { 1, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 2, 1 } },
            { DRM_FORMAT_R8, GL_R8, { 2, 1 } },
        });
        return map;
    }();
    return yuvFormatsMap;
}

bool DMABufBuffer::importIfNeeded()
{
    if (!m_importedTextures.isEmpty())
        return true;

    // Color space hint is used for Qualcomm video decoder, that exposes
    // a single fd for two planes using external EOS texture.
    bool isSinglePlaneExternalTexture = m_colorSpace && m_sampleRange;

    auto& display = PlatformDisplay::sharedDisplay();
    auto importPlane = [&](const Attributes& planeAttributes, unsigned glFormat) {
        auto eglImage = createEGLImage(display.glDisplay(), planeAttributes, isSinglePlaneExternalTexture ? m_colorSpace : std::nullopt, isSinglePlaneExternalTexture ? m_sampleRange : std::nullopt);
        if (!eglImage)
            return false;

        Ref texture = BitmapTexturePool::singleton().createTextureForImage(eglImage, planeAttributes.size, { });
        m_importedTextures.append(texture);
        display.destroyEGLImage(eglImage);

        GrGLTextureInfo externalTexture;
        externalTexture.fTarget = isSinglePlaneExternalTexture ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
        externalTexture.fID = texture->id();
        externalTexture.fFormat = glFormat;
        m_importedBackendTextures.append(GrBackendTextures::MakeGL(texture->size().width(), texture->size().height(), skgpu::Mipmapped::kNo, externalTexture));
        return true;
    };

    if (!isSinglePlaneExternalTexture && formatIsYUV(m_attributes.fourcc.value)) {
        const auto& iter = yuvFormatPlaneInfo().find(m_attributes.fourcc.value);
        if (iter == yuvFormatPlaneInfo().end())
            return false;

        const auto& planeInfo = iter->value;
        for (unsigned i = 0; i < planeInfo.size(); ++i) {
            const auto& plane = planeInfo[i];
            IntSize planeSize { m_attributes.size.width() / plane.subsampling.width(), m_attributes.size.height() / plane.subsampling.height() };
            auto planeFds = Vector<UnixFileDescriptor>::from(m_attributes.fds[i].borrow());
            Attributes planeAttributes { planeSize, plane.fourcc, WTF::move(planeFds), { m_attributes.offsets[i] }, { m_attributes.strides[i] }, m_attributes.modifier };
            if (!importPlane(planeAttributes, plane.glFormat)) {
                LOG_ERROR("Failed to import DMA-BUF YUV buffer: could not create an EGL image for plane %u", i);
                m_importedTextures.clear();
                m_importedBackendTextures.clear();
                return false;
            }
        }
    } else {
        if (!importPlane(m_attributes, GL_RGBA8)) {
            LOG_ERROR("Failed to import DMA-BUF YUV buffer: could not create an EGL image");
            m_importedTextures.clear();
            m_importedBackendTextures.clear();
            return false;
        }
    }

    return true;
}

const GrBackendTexture& DMABufBuffer::backendTexture(size_t planeIndex) const
{
    RELEASE_ASSERT(planeIndex < m_importedBackendTextures.size());
    return m_importedBackendTextures[planeIndex];
}

SkYUVAInfo DMABufBuffer::yuvaInfo() const
{
    auto planeConfig = SkYUVAInfo::PlaneConfig::kUnknown;
    auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
    switch (m_attributes.fourcc.value) {
    case DRM_FORMAT_AYUV:
        planeConfig = SkYUVAInfo::PlaneConfig::kYUVA;
        subsampling = SkYUVAInfo::Subsampling::k444;
        break;
    case DRM_FORMAT_NV21:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_VU;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_P010:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_UV;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case DRM_FORMAT_YUV420:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case DRM_FORMAT_YVU420:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_V_U;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case DRM_FORMAT_YUV444:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k444;
        break;
    case DRM_FORMAT_YUV411:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k411;
        break;
    case DRM_FORMAT_YUV422:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k422;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    SkYUVColorSpace yuvaColorSpace = [&] {
        switch (m_colorSpace.value_or(DMABufBuffer::ColorSpace::Bt601)) {
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

    return SkYUVAInfo(SkISize::Make(m_attributes.size.width(), m_attributes.size.height()), planeConfig, subsampling, yuvaColorSpace);
}

sk_sp<SkColorSpace> DMABufBuffer::skiaColorSpace() const
{
    switch (m_transferFunction.value_or(DMABufBuffer::TransferFunction::Bt709)) {
    case DMABufBuffer::TransferFunction::Bt709:
        return SkColorSpace::MakeRGB(SkNamedTransferFn::kRec709, SkNamedGamut::kSRGB);
    case DMABufBuffer::TransferFunction::Pq:
        return SkColorSpace::MakeRGB(SkNamedTransferFn::kPQ, SkNamedGamut::kRec2020);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

sk_sp<SkImage> DMABufBuffer::createImage(SkColorType colorType, SkAlphaType alphaType, GrSurfaceOrigin origin)
{
    if (!importIfNeeded())
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    ASSERT(grContext);

    if (!formatIsYUV(m_attributes.fourcc.value)) {
        ref();
        return SkImages::BorrowTextureFrom(grContext, backendTexture(0), origin, colorType, alphaType, SkColorSpace::MakeSRGB(), +[](void* userData) {
            static_cast<DMABufBuffer*>(userData)->deref();
        }, this);
    }

    SkYUVAInfo info = yuvaInfo();
    GrYUVABackendTextures yuvaBackendTextures(info, m_importedBackendTextures.span().data(), origin);
    if (!yuvaBackendTextures.isValid()) {
        LOG_ERROR("Failed to create Skia image for DMA-BUF YUV video buffer: invalid backend texture information");
        return nullptr;
    }

    ref();
    return SkImages::TextureFromYUVATextures(grContext, yuvaBackendTextures, skiaColorSpace(), +[](void* userData) {
        static_cast<DMABufBuffer*>(userData)->deref();
    }, this);
}

class PromiseDMABufImageContext final : public ThreadSafeRefCounted<PromiseDMABufImageContext> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PromiseDMABufImageContext);
public:
    static Ref<PromiseDMABufImageContext> create(Ref<DMABufBuffer>&& buffer, std::unique_ptr<GLFence>&& glFence, WTF::UnixFileDescriptor&& fenceFD)
    {
        return adoptRef(*new PromiseDMABufImageContext(WTF::move(buffer), WTF::move(glFence), WTF::move(fenceFD)));
    }

    ~PromiseDMABufImageContext() = default;

    sk_sp<GrPromiseImageTexture> promiseImageTexture(size_t planeIndex)
    {
        auto& display = PlatformDisplay::sharedDisplay();
        auto* glContext = display.skiaGLContext();
        if (!glContext || !glContext->makeContextCurrent())
            return nullptr;

        if (auto glFence = WTF::move(m_fence))
            glFence->serverWait();
        else if (m_fenceFD) {
            if (auto glFence = GLFence::importFD(display.glDisplay(), WTF::move(m_fenceFD)))
                glFence->serverWait();
        }

        if (!m_dmabuf->importIfNeeded())
            return nullptr;

        return GrPromiseImageTexture::Make(m_dmabuf->backendTexture(planeIndex));
    }

private:
    PromiseDMABufImageContext(Ref<DMABufBuffer>&& buffer, std::unique_ptr<GLFence>&& glFence, WTF::UnixFileDescriptor&& fenceFD)
        : m_dmabuf(WTF::move(buffer))
        , m_fence(WTF::move(glFence))
        , m_fenceFD(WTF::move(fenceFD))
    {
    }

    const Ref<DMABufBuffer> m_dmabuf;
    std::unique_ptr<GLFence> m_fence;
    WTF::UnixFileDescriptor m_fenceFD;
};

struct PromiseDMABufYUVPlaneContext {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(PromiseDMABufYUVPlaneContext);

    PromiseDMABufYUVPlaneContext(Ref<PromiseDMABufImageContext>&& dmabufContext, size_t planeIndex)
        : context(WTF::move(dmabufContext))
        , index(planeIndex)
    {
    }

    Ref<PromiseDMABufImageContext> context;
    size_t index { 0 };
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(PromiseDMABufYUVPlaneContext);

sk_sp<SkImage> DMABufBuffer::createPromiseImage(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, SkColorType colorType, SkAlphaType alphaType, GrSurfaceOrigin origin, std::unique_ptr<GLFence>&& glFence, WTF::UnixFileDescriptor&& fenceFD)
{
    ASSERT(threadSafeGrContext);

    if (!formatIsYUV(m_attributes.fourcc.value)) {
        Ref context = PromiseDMABufImageContext::create(Ref { *this }, WTF::move(glFence), WTF::move(fenceFD));

        auto backendFormat = threadSafeGrContext->defaultBackendFormat(colorType, GrRenderable::kYes);
        ASSERT(backendFormat.isValid());

        return SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, SkISize::Make(m_attributes.size.width(), m_attributes.size.height()), skgpu::Mipmapped::kNo,
            origin, colorType, alphaType, SkColorSpace::MakeSRGB(),
            +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
                auto& context = *static_cast<PromiseDMABufImageContext*>(userData);
                return context.promiseImageTexture(0);
            },
            +[](void* userData) {
                Ref context = adoptRef(*static_cast<PromiseDMABufImageContext*>(userData));
            }, &context.leakRef());
    }

    const auto& iter = yuvFormatPlaneInfo().find(m_attributes.fourcc.value);
    if (iter == yuvFormatPlaneInfo().end()) {
        LOG_ERROR("Failed to create Skia image for DMA-BUF YUV buffer: unknown plane configuration");
        return nullptr;
    }

    Ref context = PromiseDMABufImageContext::create(Ref { *this }, WTF::move(glFence), WTF::move(fenceFD));
    const auto& planeInfo = iter->value;
    std::array<GrBackendFormat, 4> backendFormats;
    for (unsigned i = 0; i < planeInfo.size(); ++i)
        backendFormats[i] = GrBackendFormats::MakeGL(planeInfo[i].glFormat, GL_TEXTURE_2D);

    SkYUVAInfo info = yuvaInfo();
    GrYUVABackendTextureInfo yuvaBackendTexturesInfo(info, backendFormats.data(), skgpu::Mipmapped::kNo, origin);
    if (!yuvaBackendTexturesInfo.isValid()) {
        LOG_ERROR("Failed to create Skia image for DMA-BUF YUV video buffer: invalid backend texture information");
        return nullptr;
    }

    std::array<PromiseDMABufYUVPlaneContext*, 4> planeContexts;
    for (unsigned i = 0; i < planeInfo.size(); ++i)
        planeContexts[i] = makeUnique<PromiseDMABufYUVPlaneContext>(context.copyRef(), i).release();

    return SkImages::PromiseTextureFromYUVA(threadSafeGrContext, yuvaBackendTexturesInfo, skiaColorSpace(),
        +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
            auto& planeContext = *static_cast<PromiseDMABufYUVPlaneContext*>(userData);
            return planeContext.context->promiseImageTexture(planeContext.index);
        },
        +[](void* userData) {
            std::unique_ptr<PromiseDMABufYUVPlaneContext> planeContext(static_cast<PromiseDMABufYUVPlaneContext*>(userData));
        }, reinterpret_cast<void**>(planeContexts.data()));
}

#if USE(GSTREAMER)
sk_sp<SkImage> DMABufBuffer::createPromiseImageForQualcommVideoFrame(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, SkColorType colorType, SkAlphaType alphaType, GrSurfaceOrigin origin)
{
    ASSERT(threadSafeGrContext);

    Ref context = PromiseDMABufImageContext::create(Ref { *this }, nullptr, { });
    auto backendFormat = GrBackendFormats::MakeGLExternal();
    ASSERT(backendFormat.isValid());

    return SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, SkISize::Make(m_attributes.size.width(), m_attributes.size.height()), skgpu::Mipmapped::kNo,
        origin, colorType, alphaType, SkColorSpace::MakeSRGB(),
        +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
            auto& context = *static_cast<PromiseDMABufImageContext*>(userData);
            return context.promiseImageTexture(0);
        },
        +[](void* userData) {
            Ref context = adoptRef(*static_cast<PromiseDMABufImageContext*>(userData));
        }, &context.leakRef());
}
#endif
#endif

} // namespace WebCore

#endif // USE(GBM)
