/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedSurface.h"

#if USE(COORDINATED_GRAPHICS)
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/GLContext.h>
#include <WebCore/GLFence.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/Region.h>
#include <WebCore/Settings.h>
#include <WebCore/ShareableBitmap.h>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
#include "AcceleratedBackingStoreMessages.h"
#include "AcceleratedSurfaceMessages.h"
#include <epoxy/egl.h>
#endif

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

#if USE(GBM)
#include <WebCore/DRMDeviceManager.h>
#include <WebCore/GBMVersioning.h>
#include <drm_fourcc.h>
#endif

#if USE(WPE_RENDERER)
#include <WebCore/PlatformDisplayLibWPE.h>
#include <wpe/wpe-egl.h>
#include <wtf/UniStdExtras.h>
#endif

#if OS(ANDROID)
#include <WebCore/BufferFormatAndroid.h>
#include <android/hardware_buffer.h>
#include <drm/drm_fourcc.h>
#include <wtf/android/RefPtrAndroid.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedSurface);

static inline bool isColorOpaque(AcceleratedSurface::ColorComponents color)
{
    return color[3] == WebCore::AlphaTraits<float>::opaque;
}

static uint64_t generateID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

Ref<AcceleratedSurface> AcceleratedSurface::create(WebPage& webPage, Function<void()>&& frameCompleteHandler, RenderingPurpose renderingPurpose)
{
    return adoptRef(*new AcceleratedSurface(webPage, WTF::move(frameCompleteHandler), renderingPurpose));
}

static bool useExplicitSync()
{
    auto& display = PlatformDisplay::sharedDisplay();
    auto& extensions = display.eglExtensions();
    return extensions.ANDROID_native_fence_sync && (display.eglCheckVersion(1, 5) || extensions.KHR_fence_sync);
}

AcceleratedSurface::AcceleratedSurface(WebPage& webPage, Function<void()>&& frameCompleteHandler, RenderingPurpose renderingPurpose)
    : m_webPage(webPage)
    , m_frameCompleteHandler(WTF::move(frameCompleteHandler))
    , m_id(generateID())
    , m_swapChain(m_id, renderingPurpose, webPage.corePage()->settings().hardwareAccelerationEnabled())
    , m_isVisible(webPage.activityState().contains(ActivityState::IsVisible))
    , m_useExplicitSync(usesGL() && useExplicitSync())
{
    auto color = webPage.backgroundColor();
    m_backgroundColor = color ? color->toResolvedColorComponentsInColorSpace(WebCore::ColorSpace::SRGB) : white;

#if (PLATFORM(GTK) || ENABLE(WPE_PLATFORM)) && (USE(GBM) || OS(ANDROID))
    if (m_swapChain.type() == SwapChain::Type::EGLImage)
        m_swapChain.setupBufferFormat(m_webPage->preferredBufferFormats(), isColorOpaque(m_backgroundColor));
#endif
#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        m_swapChain.initialize(webPage);
#endif
}

AcceleratedSurface::~AcceleratedSurface() = default;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedSurface::RenderTarget);

static uint64_t generateTargetID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

AcceleratedSurface::RenderTarget::RenderTarget(uint64_t surfaceID)
    : m_id(generateTargetID())
    , m_surfaceID(surfaceID)
{
}

AcceleratedSurface::RenderTarget::~RenderTarget() = default;

#if ENABLE(DAMAGE_TRACKING)
void AcceleratedSurface::RenderTarget::addDamage(const std::optional<Damage>& damage)
{
    if (!m_damage)
        return;

    if (damage)
        m_damage->add(*damage);
    else
        m_damage = std::nullopt;
}
#endif

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedSurface::RenderTargetShareableBuffer);

AcceleratedSurface::RenderTargetShareableBuffer::RenderTargetShareableBuffer(uint64_t surfaceID, const IntSize& size)
    : RenderTarget(surfaceID)
    , m_initialSize(size)
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenRenderbuffers(1, &m_depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
}

AcceleratedSurface::RenderTargetShareableBuffer::~RenderTargetShareableBuffer()
{
    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);

    if (m_depthStencilBuffer)
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidDestroyBuffer(m_id), m_surfaceID);
}

void AcceleratedSurface::RenderTargetShareableBuffer::didRenderFrame(Vector<IntRect, 1>&& damageRects)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::Frame(m_id, WTF::move(damageRects), WTF::move(m_renderingFenceFD)), m_surfaceID);
}

#if USE(SKIA)
SkSurface* AcceleratedSurface::RenderTargetShareableBuffer::skiaSurface()
{
    if (!m_skiaSurface) {
        auto* skiaGLContext = PlatformDisplay::sharedDisplay().skiaGLContext();
        if (!skiaGLContext)
            return nullptr;

        int stencilBits;
        glGetIntegerv(GL_STENCIL_BITS, &stencilBits);

        const int sampleCount = 0; // 0 == no MSAA.
        GrGLFramebufferInfo fbInfo;
        fbInfo.fFBOID = m_fbo;
        fbInfo.fFormat = GL_RGBA8;
        GrBackendRenderTarget renderTargetSkia = GrBackendRenderTargets::MakeGL(
            m_initialSize.width(),
            m_initialSize.height(),
            sampleCount,
            stencilBits,
            fbInfo
        );
        if (!skiaGLContext->makeContextCurrent())
            return nullptr;

        auto skiaSurface = SkSurfaces::WrapBackendRenderTarget(
            PlatformDisplay::sharedDisplay().skiaGrContext(),
            renderTargetSkia,
            GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
            SkColorType::kRGBA_8888_SkColorType,
            nullptr,
            nullptr
        );
        if (!skiaSurface)
            return nullptr;

        if (auto* canvas = skiaSurface->getCanvas()) {
            // Fresh buffer should default to non-opaque white.
            canvas->clear(SK_ColorWHITE);
            m_skiaSurface = WTF::move(skiaSurface);
        }
    }
    return m_skiaSurface.get();
}
#endif

void AcceleratedSurface::RenderTargetShareableBuffer::willRenderFrame()
{
    if (m_releaseFenceFD) {
        if (auto fence = GLFence::importFD(PlatformDisplay::sharedDisplay().glDisplay(), WTF::move(m_releaseFenceFD)))
            fence->serverWait();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        WTFLogAlways("AcceleratedSurface was unable to construct a complete framebuffer");
}

std::unique_ptr<GLFence> AcceleratedSurface::RenderTargetShareableBuffer::createRenderingFence(bool useExplicitSync) const
{
    auto& display = PlatformDisplay::sharedDisplay().glDisplay();
    if (useExplicitSync && supportsExplicitSync()) {
        if (auto fence = GLFence::createExportable(display))
            return fence;
    }
    return GLFence::create(display);
}

void AcceleratedSurface::RenderTargetShareableBuffer::sync(bool useExplicitSync)
{
#if USE(SKIA)
    if (m_skiaSurface) {
        PlatformDisplay::sharedDisplay().skiaGrContext()->flushAndSubmit(
            m_skiaSurface.get(),
            GLFence::isSupported(PlatformDisplay::sharedDisplay().glDisplay()) ? GrSyncCpu::kNo : GrSyncCpu::kYes
        );
    }
#endif

    if (auto fence = createRenderingFence(useExplicitSync)) {
        m_renderingFenceFD = fence->exportFD();
        if (!m_renderingFenceFD)
            fence->clientWait();
    } else
        glFlush();
}

void AcceleratedSurface::RenderTargetShareableBuffer::setReleaseFenceFD(UnixFileDescriptor&& releaseFence)
{
    m_releaseFenceFD = WTF::move(releaseFence);
}

#if USE(GBM)
std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetEGLImage::create(uint64_t surfaceID, const IntSize& size, const BufferFormat& bufferFormat)
{
    if (!bufferFormat.fourcc) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: no valid format found", size.width(), size.height());
        return nullptr;
    }

    auto gbmDevice = bufferFormat.gbmDevice;
    if (!gbmDevice) {
        gbmDevice = DRMDeviceManager::singleton().mainGBMDevice(bufferFormat.usage == RendererBufferFormat::Usage::Scanout ?
            DRMDeviceManager::NodeType::Primary : DRMDeviceManager::NodeType::Render);
    }
    if (!gbmDevice) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: no GBM device found", size.width(), size.height());
        return nullptr;
    }

    struct gbm_bo* bo = nullptr;
    uint32_t flags = bufferFormat.usage == RendererBufferFormat::Usage::Scanout ? GBM_BO_USE_SCANOUT : GBM_BO_USE_RENDERING;
    bool disableModifiers = bufferFormat.modifiers.size() == 1 && bufferFormat.modifiers[0] == DRM_FORMAT_MOD_INVALID;
    if (!disableModifiers && !bufferFormat.modifiers.isEmpty())
        bo = gbm_bo_create_with_modifiers2(gbmDevice->device(), size.width(), size.height(), bufferFormat.fourcc, bufferFormat.modifiers.span().data(), bufferFormat.modifiers.size(), flags);

    if (!bo) {
        if (bufferFormat.usage == RendererBufferFormat::Usage::Mapping)
            flags |= GBM_BO_USE_LINEAR;
        bo = gbm_bo_create(gbmDevice->device(), size.width(), size.height(), bufferFormat.fourcc, flags);
    }

    if (!bo) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
        return nullptr;
    }

    Vector<UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;
    uint32_t format = gbm_bo_get_format(bo);
    int planeCount = gbm_bo_get_plane_count(bo);
    uint64_t modifier = disableModifiers ? DRM_FORMAT_MOD_INVALID : gbm_bo_get_modifier(bo);

    Vector<EGLAttrib> attributes = {
        EGL_WIDTH, static_cast<EGLAttrib>(gbm_bo_get_width(bo)),
        EGL_HEIGHT, static_cast<EGLAttrib>(gbm_bo_get_height(bo)),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(format),
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    fds.append(UnixFileDescriptor { gbm_bo_get_fd_for_plane(bo, planeIndex), UnixFileDescriptor::Adopt }); \
    offsets.append(gbm_bo_get_offset(bo, planeIndex)); \
    strides.append(gbm_bo_get_stride_for_plane(bo, planeIndex)); \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, fds.last().value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLAttrib>(offsets.last()), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLAttrib>(strides.last()) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (modifier != DRM_FORMAT_MOD_INVALID) { \
        std::array<EGLAttrib, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLAttrib>(modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLAttrib>(modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLAttrib> { modifierAttributes }); \
    } \
    }

    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBS

    attributes.append(EGL_NONE);

    auto& display = PlatformDisplay::sharedDisplay();
    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    gbm_bo_destroy(bo);

    if (!image) {
        WTFLogAlways("Failed to create EGL image for DMABufs with size %dx%d", size.width(), size.height());
        return nullptr;
    }

    return makeUnique<RenderTargetEGLImage>(surfaceID, size, image, format, WTF::move(fds), WTF::move(offsets), WTF::move(strides), modifier, bufferFormat.usage);
}

AcceleratedSurface::RenderTargetEGLImage::RenderTargetEGLImage(uint64_t surfaceID, const IntSize& size, EGLImage image, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, RendererBufferFormat::Usage usage)
    : RenderTargetShareableBuffer(surfaceID, size)
    , m_image(image)
{
    initializeColorBuffer();
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidCreateDMABufBuffer(m_id, size, format, WTF::move(fds), WTF::move(offsets), WTF::move(strides), modifier, usage), surfaceID);
}
#endif // USE(GBM)

#if OS(ANDROID)
static uint64_t usageToAHardwareBufferUsage(RendererBufferFormat::Usage usage)
{
    switch (usage) {
    case RendererBufferFormat::Usage::Rendering:
        return AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    case RendererBufferFormat::Usage::Mapping:
        return AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;
    case RendererBufferFormat::Usage::Scanout:
        // FIXME(297316): Add the AHARDWAREBUFFER_USAGE_CPU_READ_RARELY flag to allow using AHardwareBuffer_lock()
        return AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_FRONT_BUFFER | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;
    }
}

std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetEGLImage::create(uint64_t surfaceID, const WebCore::IntSize& size, const BufferFormat& bufferFormat)
{
    const auto hardwareBufferFormat = toAHardwareBufferFormat(bufferFormat.fourcc);
    if (!hardwareBufferFormat) {
        LOG_ERROR("Failed to create AHardwareBuffer of size %dx%d: no valid format found (FourCC=%s)",
            size.width(), size.height(), FourCC(bufferFormat.fourcc).string().data());
        return nullptr;
    }

    AHardwareBuffer_Desc description = { };
    description.width = size.width();
    description.height = size.height();
    description.layers = 1;
    description.format = hardwareBufferFormat.value();
    description.usage = usageToAHardwareBufferUsage(bufferFormat.usage);

    AHardwareBuffer* hardwareBufferPtr { nullptr };
    int result = AHardwareBuffer_allocate(&description, &hardwareBufferPtr);
    if (result) {
        LOG_ERROR("Failed to create AHardwareBuffer of size %dx%d, format %s, error code: %d",
            size.width(), size.height(), FourCC(bufferFormat.fourcc).string().data(), result);
        return nullptr;
    }
    auto hardwareBuffer = adoptRef(hardwareBufferPtr);

    const Vector<EGLAttrib> attributes { EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE };

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    auto clientBuffer = eglGetNativeClientBufferANDROID(hardwareBuffer.get());
    if (!clientBuffer) {
        LOG_ERROR("Failed to create client buffer for AHarwareBuffer of size %dx%d, format %s. EGL error: %#04x",
            size.width(), size.height(), FourCC(bufferFormat.fourcc).string().data(), eglGetError());
        return nullptr;
    }

    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attributes);
    if (image == EGL_NO_IMAGE) {
        LOG_ERROR("Failed to bind AHardwareBuffer to an EGLImage. This is typically caused by "
            "a version mismatch between the gralloc implementation and the OpenGL/EGL driver. "
            "Please contact your GPU vendor to resolve this problem. EGL error: %#04x", eglGetError());
        return nullptr;
    }

    return makeUnique<RenderTargetEGLImage>(surfaceID, size, image, WTF::move(hardwareBuffer));
}

AcceleratedSurface::RenderTargetEGLImage::RenderTargetEGLImage(uint64_t surfaceID, const WebCore::IntSize& size, EGLImage image, RefPtr<AHardwareBuffer>&& hardwareBuffer)
    : RenderTargetShareableBuffer(surfaceID, size)
    , m_image(image)
{
    initializeColorBuffer();
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidCreateAndroidBuffer(m_id, WTF::move(hardwareBuffer)), surfaceID);
}
#endif // OS(ANDROID)

#if USE(GBM) || OS(ANDROID)
void AcceleratedSurface::RenderTargetEGLImage::initializeColorBuffer()
{
    glGenRenderbuffers(1, &m_colorBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_colorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_image);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorBuffer);
}

AcceleratedSurface::RenderTargetEGLImage::~RenderTargetEGLImage()
{
    if (m_colorBuffer)
        glDeleteRenderbuffers(1, &m_colorBuffer);

    if (m_image)
        PlatformDisplay::sharedDisplay().destroyEGLImage(m_image);
}
#endif // USE(GBM) || OS(ANDROID)

std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetSHMImage::create(uint64_t surfaceID, const IntSize& size)
{
    RefPtr buffer = ShareableBitmap::create({ size });
    if (!buffer) {
        WTFLogAlways("Failed to allocate shared memory buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    auto bufferHandle = buffer->createReadOnlyHandle();
    if (!bufferHandle) {
        WTFLogAlways("Failed to create handle for shared memory buffer");
        return nullptr;
    }

    return makeUnique<RenderTargetSHMImage>(surfaceID, size, Ref { *buffer }, WTF::move(*bufferHandle));
}

AcceleratedSurface::RenderTargetSHMImage::RenderTargetSHMImage(uint64_t surfaceID, const IntSize& size, Ref<ShareableBitmap>&& bitmap, ShareableBitmap::Handle&& bitmapHandle)
    : RenderTargetShareableBuffer(surfaceID, size)
    , m_bitmap(WTF::move(bitmap))
{
    glGenRenderbuffers(1, &m_colorBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_colorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorBuffer);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidCreateSHMBuffer(m_id, WTF::move(bitmapHandle)), surfaceID);
}

AcceleratedSurface::RenderTargetSHMImage::~RenderTargetSHMImage()
{
    if (m_colorBuffer)
        glDeleteRenderbuffers(1, &m_colorBuffer);
}

void AcceleratedSurface::RenderTargetSHMImage::didRenderFrame(Vector<IntRect, 1>&& damageRects)
{
    glReadPixels(0, 0, m_bitmap->size().width(), m_bitmap->size().height(), GL_BGRA, GL_UNSIGNED_BYTE, m_bitmap->mutableSpan().data());
    RenderTargetShareableBuffer::didRenderFrame(WTF::move(damageRects));
}

std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetSHMImageWithoutGL::create(uint64_t surfaceID, const IntSize& size)
{
    RefPtr buffer = ShareableBitmap::create({ size });
    if (!buffer) {
        LOG_ERROR("Failed to allocate shared memory buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    auto bufferHandle = buffer->createReadOnlyHandle();
    if (!bufferHandle) {
        LOG_ERROR("Failed to create handle for shared memory buffer");
        return nullptr;
    }

    return makeUnique<RenderTargetSHMImageWithoutGL>(surfaceID, size, Ref { *buffer }, WTF::move(*bufferHandle));
}

AcceleratedSurface::RenderTargetSHMImageWithoutGL::RenderTargetSHMImageWithoutGL(uint64_t surfaceID, const IntSize& size, Ref<ShareableBitmap>&& bitmap, ShareableBitmap::Handle&& bitmapHandle)
    : RenderTarget(surfaceID)
    , m_initialSize(size)
    , m_bitmap(WTF::move(bitmap))
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidCreateSHMBuffer(m_id, WTF::move(bitmapHandle)), surfaceID);
}

AcceleratedSurface::RenderTargetSHMImageWithoutGL::~RenderTargetSHMImageWithoutGL()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidDestroyBuffer(m_id), m_surfaceID);
}

#if USE(SKIA)
SkSurface* AcceleratedSurface::RenderTargetSHMImageWithoutGL::skiaSurface()
{
    if (!m_skiaSurface)
        m_skiaSurface = m_bitmap->createSurface();
    return m_skiaSurface.get();
}
#endif

void AcceleratedSurface::RenderTargetSHMImageWithoutGL::didRenderFrame(Vector<IntRect, 1>&& damageRects)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::Frame(m_id, WTF::move(damageRects), UnixFileDescriptor()), m_surfaceID);
}

std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetTexture::create(uint64_t surfaceID, const IntSize& size)
{
    unsigned texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    auto& display = PlatformDisplay::sharedDisplay();
    auto image = display.createEGLImage(eglGetCurrentContext(), EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)texture, { });
    if (!image) {
        glDeleteTextures(1, &texture);
        WTFLogAlways("Failed to create EGL image for texture");
        return nullptr;
    }

    int fourcc, planeCount;
    uint64_t modifier;
    if (!eglExportDMABUFImageQueryMESA(display.eglDisplay(), image, &fourcc, &planeCount, &modifier)) {
        WTFLogAlways("eglExportDMABUFImageQueryMESA failed");
        display.destroyEGLImage(image);
        glDeleteTextures(1, &texture);
        return nullptr;
    }

    Vector<int> fdsOut(planeCount);
    Vector<int> stridesOut(planeCount);
    Vector<int> offsetsOut(planeCount);
    if (!eglExportDMABUFImageMESA(display.eglDisplay(), image, fdsOut.mutableSpan().data(), stridesOut.mutableSpan().data(), offsetsOut.mutableSpan().data())) {
        WTFLogAlways("eglExportDMABUFImageMESA failed");
        display.destroyEGLImage(image);
        glDeleteTextures(1, &texture);
        return nullptr;
    }

    display.destroyEGLImage(image);

    Vector<UnixFileDescriptor> fds = fdsOut.map([](int fd) {
        return UnixFileDescriptor(fd, UnixFileDescriptor::Adopt);
    });
    Vector<uint32_t> strides = stridesOut.map([](int stride) {
        return static_cast<uint32_t>(stride);
    });
    Vector<uint32_t> offsets = offsetsOut.map([](int offset) {
        return static_cast<uint32_t>(offset);
    });

    return makeUnique<RenderTargetTexture>(surfaceID, size, texture, fourcc, WTF::move(fds), WTF::move(offsets), WTF::move(strides), modifier);
}

AcceleratedSurface::RenderTargetTexture::RenderTargetTexture(uint64_t surfaceID, const IntSize& size, unsigned texture, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
    : RenderTargetShareableBuffer(surfaceID, size)
    , m_texture(texture)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidCreateDMABufBuffer(m_id, size, format, WTF::move(fds), WTF::move(offsets), WTF::move(strides), modifier, RendererBufferFormat::Usage::Rendering), surfaceID);
}

AcceleratedSurface::RenderTargetTexture::~RenderTargetTexture()
{
    if (m_texture)
        glDeleteTextures(1, &m_texture);
}
#endif // PLATFORM(GTK) || ENABLE(WPE_PLATFORM)

#if USE(WPE_RENDERER)
std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetWPEBackend::create(uint64_t surfaceID, const IntSize& initialSize, UnixFileDescriptor&& hostFD, const AcceleratedSurface& surface)
{
    return makeUnique<RenderTargetWPEBackend>(surfaceID, initialSize, WTF::move(hostFD), surface);
}

AcceleratedSurface::RenderTargetWPEBackend::RenderTargetWPEBackend(uint64_t surfaceID, const IntSize& initialSize, UnixFileDescriptor&& hostFD, const AcceleratedSurface& surface)
    : RenderTarget(surfaceID)
    , m_backend(wpe_renderer_backend_egl_target_create(hostFD.release()))
{
    static struct wpe_renderer_backend_egl_target_client s_client = {
        // frame_complete
        [](void* data) {
            auto& surface = *reinterpret_cast<AcceleratedSurface*>(data);
            surface.frameDone();
        },
        // padding
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    wpe_renderer_backend_egl_target_set_client(m_backend, &s_client, const_cast<AcceleratedSurface*>(&surface));
    wpe_renderer_backend_egl_target_initialize(m_backend, downcast<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()).backend(),
        std::max(1, initialSize.width()), std::max(1, initialSize.height()));
}

AcceleratedSurface::RenderTargetWPEBackend::~RenderTargetWPEBackend()
{
#if WPE_CHECK_VERSION(1, 9, 1)
    // libwpe 1.9.1 introduced an additional ::deinitialize function, which
    // may be called some time before destruction. As there is no better place
    // to invoke it at the moment, do it right before destroying the object.
    wpe_renderer_backend_egl_target_deinitialize(m_backend);
#endif

    wpe_renderer_backend_egl_target_destroy(m_backend);
}

uint64_t AcceleratedSurface::RenderTargetWPEBackend::window() const
{
    // EGLNativeWindowType changes depending on the EGL implementation: reinterpret_cast works
    // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
    // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
    // a plain C cast expression in this one instance works in all cases.
    static_assert(sizeof(EGLNativeWindowType) <= sizeof(uint64_t), "EGLNativeWindowType must not be longer than 64 bits.");
    return (uint64_t)wpe_renderer_backend_egl_target_get_native_window(m_backend);
}

void AcceleratedSurface::RenderTargetWPEBackend::resize(const IntSize& size)
{
    wpe_renderer_backend_egl_target_resize(m_backend, std::max(1, size.width()), std::max(1, size.height()));
}

void AcceleratedSurface::RenderTargetWPEBackend::willRenderFrame()
{
    wpe_renderer_backend_egl_target_frame_will_render(m_backend);
}

void AcceleratedSurface::RenderTargetWPEBackend::didRenderFrame(Vector<IntRect, 1>&&)
{
    wpe_renderer_backend_egl_target_frame_rendered(m_backend);
}
#endif

AcceleratedSurface::SwapChain::SwapChain(uint64_t surfaceID, RenderingPurpose renderingPurpose, bool useHardwareBuffersForFrameRendering)
    : m_surfaceID(surfaceID)
{
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    if (renderingPurpose == RenderingPurpose::NonComposited && !useHardwareBuffersForFrameRendering) {
        m_type = Type::SharedMemoryWithoutGL;
        return;
    }
#endif

    auto& display = PlatformDisplay::sharedDisplay();
    switch (display.type()) {
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    case PlatformDisplay::Type::Surfaceless:
        if (display.eglExtensions().MESA_image_dma_buf_export && WebProcess::singleton().rendererBufferTransportMode().contains(RendererBufferTransportMode::Hardware))
            m_type = Type::Texture;
        else
            m_type = Type::SharedMemory;
        break;
#if USE(GBM)
    case PlatformDisplay::Type::GBM:
        if (useHardwareBuffersForFrameRendering && display.eglExtensions().EXT_image_dma_buf_import)
            m_type = Type::EGLImage;
        else
            m_type = renderingPurpose == RenderingPurpose::Composited ? Type::SharedMemory : Type::SharedMemoryWithoutGL;
        break;
#endif
#if OS(ANDROID)
    case PlatformDisplay::Type::Android:
        m_type = Type::EGLImage;
        break;
#endif
#endif // PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
#if USE(WPE_RENDERER)
    case PlatformDisplay::Type::WPE:
        m_type = Type::WPEBackend;
        break;
#endif
#if PLATFORM(GTK) || OS(ANDROID)
    case PlatformDisplay::Type::Default:
        break;
#endif // PLATFORM(GTK) || OS(ANDROID)
    }
}

#if (PLATFORM(GTK) || ENABLE(WPE_PLATFORM)) && (USE(GBM) || OS(ANDROID))
void AcceleratedSurface::SwapChain::setupBufferFormat(const Vector<RendererBufferFormat>& preferredFormats, bool isOpaque)
{
    auto isOpaqueFormat = [](FourCC fourcc) -> bool {
        return fourcc != DRM_FORMAT_ARGB8888
            && fourcc != DRM_FORMAT_RGBA8888
            && fourcc != DRM_FORMAT_ABGR8888
            && fourcc != DRM_FORMAT_BGRA8888
            && fourcc != DRM_FORMAT_ARGB2101010
            && fourcc != DRM_FORMAT_ABGR2101010
            && fourcc != DRM_FORMAT_ARGB16161616F
            && fourcc != DRM_FORMAT_ABGR16161616F;
    };

    // The preferred formats vector is sorted by usage, but all formats for the same usage has the same priority.
    Locker locker { m_bufferFormatLock };
    BufferFormat newBufferFormat;
    const auto& supportedFormats = PlatformDisplay::sharedDisplay().bufferFormats();
    for (const auto& bufferFormat : preferredFormats) {

        auto matchesOpacity = false;
        for (const auto& format : supportedFormats) {
            auto index = bufferFormat.formats.findIf([&](const auto& item) {
                return format.fourcc == item.fourcc;
            });
            if (index != notFound) {
                const auto& preferredFormat = bufferFormat.formats[index];

                matchesOpacity = isOpaqueFormat(preferredFormat.fourcc) == isOpaque;
                if (!matchesOpacity && newBufferFormat.fourcc)
                    continue;

                newBufferFormat.usage = bufferFormat.usage;
                newBufferFormat.fourcc = preferredFormat.fourcc;
#if USE(GBM)
                newBufferFormat.drmDevice = bufferFormat.drmDevice;
                if (preferredFormat.modifiers[0] == DRM_FORMAT_MOD_INVALID)
                    newBufferFormat.modifiers = preferredFormat.modifiers;
                else {
                    newBufferFormat.modifiers = WTF::compactMap(preferredFormat.modifiers, [&format](uint64_t modifier) -> std::optional<uint64_t> {
                        if (format.modifiers.contains(modifier))
                            return modifier;
                        return std::nullopt;
                    });
                }
#endif // USE(GBM)

                if (matchesOpacity)
                    break;
            }
        }

        if (newBufferFormat.fourcc && matchesOpacity)
            break;
    }

    if (!newBufferFormat.fourcc || newBufferFormat == m_bufferFormat)
        return;

#if USE(GBM)
    if (!newBufferFormat.drmDevice.isNull()) {
        if (newBufferFormat.drmDevice == m_bufferFormat.drmDevice && newBufferFormat.usage == m_bufferFormat.usage)
            newBufferFormat.gbmDevice = m_bufferFormat.gbmDevice;
        else {
            auto nodeType = newBufferFormat.usage == RendererBufferFormat::Usage::Scanout ? DRMDeviceManager::NodeType::Primary : DRMDeviceManager::NodeType::Render;
            newBufferFormat.gbmDevice = DRMDeviceManager::singleton().gbmDevice(newBufferFormat.drmDevice, nodeType);
        }
    }
#endif // USE(GBM)

    m_bufferFormat = WTF::move(newBufferFormat);
    m_bufferFormatChanged = true;
}
#endif // USE(GBM) || OS(ANDROID)

bool AcceleratedSurface::SwapChain::resize(const IntSize& size)
{
    if (m_size == size)
        return false;

    m_size = size;
#if USE(WPE_RENDERER)
    if (m_type == Type::WPEBackend) {
        static_cast<RenderTargetWPEBackend*>(m_lockedTargets[0].get())->resize(m_size);
        return true;
    }
#endif
    reset();
    return true;
}

std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::SwapChain::createTarget() const
{
    switch (m_type) {
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
#if USE(GBM) || OS(ANDROID)
    case Type::EGLImage:
        return RenderTargetEGLImage::create(m_surfaceID, m_size, m_bufferFormat);
#endif
    case Type::Texture:
        return RenderTargetTexture::create(m_surfaceID, m_size);
    case Type::SharedMemory:
        return RenderTargetSHMImage::create(m_surfaceID, m_size);
    case Type::SharedMemoryWithoutGL:
        return RenderTargetSHMImageWithoutGL::create(m_surfaceID, m_size);
#endif
#if USE(WPE_RENDERER)
    case Type::WPEBackend:
        ASSERT_NOT_REACHED();
        break;
#endif
    case Type::Invalid:
        break;
    }
#if !(PLATFORM(GTK) || ENABLE(WPE_PLATFORM))
    UNUSED_PARAM(m_surfaceID);
#endif
    return nullptr;
}

AcceleratedSurface::RenderTarget* AcceleratedSurface::SwapChain::nextTarget()
{
#if USE(WPE_RENDERER)
    if (m_type == Type::WPEBackend)
        return m_lockedTargets[0].get();
#endif

#if (PLATFORM(GTK) || ENABLE(WPE_PLATFORM)) && (USE(GBM) || OS(ANDROID))
    if (m_type == Type::EGLImage) {
        Locker locker { m_bufferFormatLock };
        if (m_bufferFormatChanged) {
            reset();
            m_bufferFormatChanged = false;
        }
    }
#endif

    if (m_freeTargets.isEmpty()) {
        ASSERT(m_lockedTargets.size() < s_maximumBuffers);

        if (m_lockedTargets.isEmpty()) [[unlikely]] {
            // Initial setup.
#if ENABLE(WPE_PLATFORM)
            WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidChangeBufferConfiguration(s_initialBuffers), m_surfaceID);
#endif
            for (unsigned i = 0; i < s_initialBuffers; ++i)
                m_freeTargets.append(createTarget());
        } else {
            // Additional buffers creted on demand.
#if ENABLE(WPE_PLATFORM)
            WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidChangeBufferConfiguration(m_lockedTargets.size() + 1), m_surfaceID);
#endif
            m_lockedTargets.insert(0, createTarget());
            return m_lockedTargets[0].get();
        }
    }

    auto target = m_freeTargets.takeLast();
    m_lockedTargets.insert(0, WTF::move(target));
    return m_lockedTargets[0].get();
}

void AcceleratedSurface::SwapChain::releaseTarget(uint64_t targetID, UnixFileDescriptor&& releaseFence)
{
#if USE(WPE_RENDERER)
    ASSERT(m_type != Type::WPEBackend);
#endif

    auto index = m_lockedTargets.reverseFindIf([targetID](const auto& item) {
        return item->id() == targetID;
    });
    if (index != notFound) {
        m_lockedTargets[index]->setReleaseFenceFD(WTF::move(releaseFence));
        m_freeTargets.insert(0, WTF::move(m_lockedTargets[index]));
        m_lockedTargets.removeAt(index);
    }
}

void AcceleratedSurface::SwapChain::reset()
{
#if ENABLE(WPE_PLATFORM)
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidChangeBufferConfiguration(0), m_surfaceID);
#endif

    m_lockedTargets.clear();
    m_freeTargets.clear();
}

void AcceleratedSurface::SwapChain::releaseUnusedBuffers()
{
#if USE(WPE_RENDERER)
    ASSERT(m_type != Type::WPEBackend);
#endif

#if ENABLE(WPE_PLATFORM)
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStore::DidChangeBufferConfiguration(m_lockedTargets.size()), m_surfaceID);
#endif

    m_freeTargets.clear();
}

#if USE(WPE_RENDERER)
void AcceleratedSurface::SwapChain::initialize(WebPage& webPage)
{
    ASSERT(m_type == Type::WPEBackend);
    m_hostFD = webPage.hostFileDescriptor();
    m_initialSize = webPage.size();
    m_initialSize.scale(webPage.deviceScaleFactor());
}

uint64_t AcceleratedSurface::SwapChain::initializeTarget(const AcceleratedSurface& surface)
{
    ASSERT(m_type == Type::WPEBackend);
    auto target = RenderTargetWPEBackend::create(m_surfaceID, m_initialSize, WTF::move(m_hostFD), surface);
    auto window = static_cast<RenderTargetWPEBackend*>(target.get())->window();
    m_lockedTargets.append(WTF::move(target));
    return window;
}
#endif

#if ENABLE(DAMAGE_TRACKING)
void AcceleratedSurface::SwapChain::addDamage(const std::optional<Damage>& damage)
{
    for (auto& renderTarget : m_freeTargets)
        renderTarget->addDamage(damage);
    for (auto& renderTarget : m_lockedTargets)
        renderTarget->addDamage(damage);
}
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
void AcceleratedSurface::preferredBufferFormatsDidChange()
{
    if (m_swapChain.type() != SwapChain::Type::EGLImage)
        return;

    m_swapChain.setupBufferFormat(m_webPage->preferredBufferFormats(), isColorOpaque(m_backgroundColor));
}
#endif

void AcceleratedSurface::visibilityDidChange(bool isVisible)
{
    if (m_isVisible == isVisible)
        return;

    m_isVisible = isVisible;
    if (!m_releaseUnusedBuffersTimer)
        return;

    if (m_isVisible)
        m_releaseUnusedBuffersTimer->stop();
    else {
        static const Seconds releaseUnusedBuffersDelay = 10_s;
        m_releaseUnusedBuffersTimer->startOneShot(releaseUnusedBuffersDelay);
    }
}

void AcceleratedSurface::backgroundColorDidChange()
{
    ASSERT(RunLoop::isMain());
    const auto& color = m_webPage->backgroundColor();

    bool wasOpaque = isColorOpaque(m_backgroundColor);
    m_backgroundColor = color ? color->toResolvedColorComponentsInColorSpace(WebCore::ColorSpace::SRGB) : white;
    bool isOpaque = isColorOpaque(m_backgroundColor);

    if (isOpaque == wasOpaque)
        return;

#if (PLATFORM(GTK) || ENABLE(WPE_PLATFORM)) && (USE(GBM) || OS(ANDROID))
    if (m_swapChain.type() == SwapChain::Type::EGLImage)
        m_swapChain.setupBufferFormat(m_webPage->preferredBufferFormats(), isOpaque);
#endif
}

void AcceleratedSurface::releaseUnusedBuffersTimerFired()
{
    m_swapChain.releaseUnusedBuffers();
}

void AcceleratedSurface::didCreateCompositingRunLoop(RunLoop& runLoop)
{
#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        return;
#endif

    m_releaseUnusedBuffersTimer = makeUnique<RunLoop::Timer>(runLoop, "AcceleratedSurface::ReleaseUnusedBuffersTimer"_s, this, &AcceleratedSurface::releaseUnusedBuffersTimerFired);
#if USE(GLIB_EVENT_LOOP)
    m_releaseUnusedBuffersTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    WebProcess::singleton().parentProcessConnection()->addMessageReceiver(runLoop, *this, Messages::AcceleratedSurface::messageReceiverName(), m_id);
#endif
}

void AcceleratedSurface::willDestroyCompositingRunLoop()
{
    m_frameCompleteHandler = nullptr;

#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        return;
#endif

    m_releaseUnusedBuffersTimer = nullptr;
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    WebProcess::singleton().parentProcessConnection()->removeMessageReceiver(Messages::AcceleratedSurface::messageReceiverName(), m_id);
#endif
}

void AcceleratedSurface::willDestroyGLContext()
{
    m_swapChain.reset();
}

uint64_t AcceleratedSurface::surfaceID() const
{
    return m_id;
}

uint64_t AcceleratedSurface::window() const
{
#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        return const_cast<SwapChain*>(&m_swapChain)->initializeTarget(*this);
#endif
    return 0;
}

#if USE(SKIA)
SkCanvas* AcceleratedSurface::canvas()
{
    if (auto* surface = m_target ? m_target->skiaSurface() : nullptr)
        return surface->getCanvas();
    return nullptr;
}
#endif

void AcceleratedSurface::willRenderFrame(const IntSize& size)
{
    bool sizeDidChange = m_swapChain.resize(size);

    m_target = m_swapChain.nextTarget();
    if (m_target)
        m_target->willRenderFrame();

    if (sizeDidChange)
        glViewport(0, 0, size.width(), size.height());
}

void AcceleratedSurface::clear(const OptionSet<WebCore::CompositionReason>& reasons)
{
    ASSERT(!RunLoop::isMain());
    auto backgroundColor = m_backgroundColor.load();
    if (!isColorOpaque(backgroundColor)) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (reasons.contains(CompositionReason::AsyncScrolling)) {
        auto [r, g, b, a] = backgroundColor;
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void AcceleratedSurface::didRenderFrame()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    TraceScope traceScope(WaitForCompositionCompletionStart, WaitForCompositionCompletionEnd);
#endif

    if (!m_target)
        return;

    m_target->sync(m_useExplicitSync);

    Vector<IntRect, 1> damageRects;
#if ENABLE(DAMAGE_TRACKING)
    // For now, we use bounding box damage for render target damage, as its only 2 consumers so far
    // (CoordinatedBackingStore & ThreadedCompositor) only fetch bounds. Thus having damage with
    // better resolution is pointless as the bounds are the same in such case.
    m_target->setDamage(Damage(m_swapChain.size(), Damage::Mode::BoundingBox));
    if (m_frameDamage) {
        damageRects = m_frameDamage->rects();
        m_frameDamage = std::nullopt;
    }
#endif

    m_target->didRenderFrame(WTF::move(damageRects));
}

#if ENABLE(DAMAGE_TRACKING)
void AcceleratedSurface::setFrameDamage(Damage&& damage)
{
    if (!damage.isEmpty())
        m_frameDamage = WTF::move(damage);
    else
        m_frameDamage = std::nullopt;
}

const std::optional<Damage>& AcceleratedSurface::renderTargetDamage()
{
    m_swapChain.addDamage(m_frameDamage);
    ASSERT(m_target);
    return m_target->damage();
}
#endif

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
void AcceleratedSurface::releaseBuffer(uint64_t targetID, UnixFileDescriptor&& releaseFence)
{
#if USE(WPE_RENDERER)
    ASSERT(m_swapChain.type() != SwapChain::Type::WPEBackend);
#endif
    m_swapChain.releaseTarget(targetID, WTF::move(releaseFence));
}
#endif

void AcceleratedSurface::frameDone()
{
    if (m_frameCompleteHandler)
        m_frameCompleteHandler();
    m_target = nullptr;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
