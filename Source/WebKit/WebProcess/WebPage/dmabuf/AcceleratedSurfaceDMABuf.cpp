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
#include "AcceleratedSurfaceDMABuf.h"

#if (PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM)))

#include "AcceleratedBackingStoreDMABufMessages.h"
#include "AcceleratedSurfaceDMABufMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/GLFence.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/Region.h>
#include <WebCore/ShareableBitmap.h>
#include <array>
#include <epoxy/egl.h>
#include <fcntl.h>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(GBM)
#include <WebCore/DRMDeviceManager.h>
#include <WebCore/GBMVersioning.h>
#include <drm_fourcc.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {

static uint64_t generateID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

std::unique_ptr<AcceleratedSurfaceDMABuf> AcceleratedSurfaceDMABuf::create(WebPage& webPage, Function<void()>&& frameCompleteHandler)
{
    return std::unique_ptr<AcceleratedSurfaceDMABuf>(new AcceleratedSurfaceDMABuf(webPage, WTFMove(frameCompleteHandler)));
}

static bool useExplicitSync()
{
    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    auto& extensions = display.eglExtensions();
    return extensions.ANDROID_native_fence_sync && (display.eglCheckVersion(1, 5) || extensions.KHR_fence_sync);
}

AcceleratedSurfaceDMABuf::AcceleratedSurfaceDMABuf(WebPage& webPage, Function<void()>&& frameCompleteHandler)
    : AcceleratedSurface(webPage, WTFMove(frameCompleteHandler))
    , m_id(generateID())
    , m_swapChain(m_id)
    , m_isVisible(webPage.activityState().contains(WebCore::ActivityState::IsVisible))
    , m_useExplicitSync(useExplicitSync())
{
#if USE(GBM)
    if (m_swapChain.type() == SwapChain::Type::EGLImage)
        m_swapChain.setupBufferFormat(m_webPage->preferredBufferFormats(), m_isOpaque);
#endif
}

AcceleratedSurfaceDMABuf::~AcceleratedSurfaceDMABuf()
{
}

static uint64_t generateTargetID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(AcceleratedSurfaceDMABufRenderTarget, AcceleratedSurfaceDMABuf::RenderTarget);

AcceleratedSurfaceDMABuf::RenderTarget::RenderTarget(uint64_t surfaceID, const WebCore::IntSize& size)
    : m_id(generateTargetID())
    , m_surfaceID(surfaceID)
{
    glGenRenderbuffers(1, &m_depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());
}

AcceleratedSurfaceDMABuf::RenderTarget::~RenderTarget()
{
    if (m_depthStencilBuffer)
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::DidDestroyBuffer(m_id), m_surfaceID);
}

void AcceleratedSurfaceDMABuf::RenderTarget::addDamage(const WebCore::Damage& damage)
{
    if (!m_damage.isInvalid())
        m_damage.add(damage);
}

std::unique_ptr<WebCore::GLFence> AcceleratedSurfaceDMABuf::RenderTarget::createRenderingFence(bool useExplicitSync) const
{
    if (useExplicitSync && supportsExplicitSync()) {
        if (auto fence = WebCore::GLFence::createExportable())
            return fence;
    }
    return WebCore::GLFence::create();
}

void AcceleratedSurfaceDMABuf::RenderTarget::willRenderFrame() const
{
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTarget::setReleaseFenceFD(UnixFileDescriptor&& releaseFence)
{
    m_releaseFenceFD = WTFMove(releaseFence);
}

void AcceleratedSurfaceDMABuf::RenderTarget::waitRelease()
{
    if (!m_releaseFenceFD)
        return;

    if (auto fence = WebCore::GLFence::importFD(WTFMove(m_releaseFenceFD)))
        fence->serverWait();
}

AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::RenderTargetColorBuffer(uint64_t surfaceID, const WebCore::IntSize& size)
    : RenderTarget(surfaceID, size)
{
    glGenRenderbuffers(1, &m_colorBuffer);
}

AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::~RenderTargetColorBuffer()
{
    if (m_colorBuffer)
        glDeleteRenderbuffers(1, &m_colorBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::willRenderFrame() const
{
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorBuffer);
    RenderTarget::willRenderFrame();
}

#if USE(GBM)
std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetEGLImage::create(uint64_t surfaceID, const WebCore::IntSize& size, const BufferFormat& dmabufFormat)
{
    if (!dmabufFormat.fourcc) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: no valid format found", size.width(), size.height());
        return nullptr;
    }

    auto* device = dmabufFormat.drmDeviceNode ? dmabufFormat.drmDeviceNode->gbmDevice() : nullptr;
    if (!device) {
        device = WebCore::DRMDeviceManager::singleton().mainGBMDeviceNode(dmabufFormat.usage == DMABufRendererBufferFormat::Usage::Scanout ?
            WebCore::DRMDeviceManager::NodeType::Primary : WebCore::DRMDeviceManager::NodeType::Render);
    }
    if (!device) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: no GBM device found", size.width(), size.height());
        return nullptr;
    }

    struct gbm_bo* bo = nullptr;
    uint32_t flags = dmabufFormat.usage == DMABufRendererBufferFormat::Usage::Scanout ? GBM_BO_USE_SCANOUT : GBM_BO_USE_RENDERING;
    bool disableModifiers = dmabufFormat.modifiers.size() == 1 && dmabufFormat.modifiers[0] == DRM_FORMAT_MOD_INVALID;
    if (!disableModifiers && !dmabufFormat.modifiers.isEmpty())
        bo = gbm_bo_create_with_modifiers2(device, size.width(), size.height(), dmabufFormat.fourcc, dmabufFormat.modifiers.data(), dmabufFormat.modifiers.size(), flags);

    if (!bo) {
        if (dmabufFormat.usage == DMABufRendererBufferFormat::Usage::Mapping)
            flags |= GBM_BO_USE_LINEAR;
        bo = gbm_bo_create(device, size.width(), size.height(), dmabufFormat.fourcc, flags);
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

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    gbm_bo_destroy(bo);

    if (!image) {
        WTFLogAlways("Failed to create EGL image for DMABufs with size %dx%d", size.width(), size.height());
        return nullptr;
    }

    return makeUnique<RenderTargetEGLImage>(surfaceID, size, image, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier, dmabufFormat.usage);
}

AcceleratedSurfaceDMABuf::RenderTargetEGLImage::RenderTargetEGLImage(uint64_t surfaceID, const WebCore::IntSize& size, EGLImage image, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, DMABufRendererBufferFormat::Usage usage)
    : RenderTargetColorBuffer(surfaceID, size)
    , m_image(image)
{
    glBindRenderbuffer(GL_RENDERBUFFER, m_colorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_image);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::DidCreateBuffer(m_id, size, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier, usage), surfaceID);
}

AcceleratedSurfaceDMABuf::RenderTargetEGLImage::~RenderTargetEGLImage()
{
    if (!m_image)
        return;

    WebCore::PlatformDisplay::sharedDisplay().destroyEGLImage(m_image);
}
#endif

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetSHMImage::create(uint64_t surfaceID, const WebCore::IntSize& size)
{
    RefPtr buffer = WebCore::ShareableBitmap::create({ size });
    if (!buffer) {
        WTFLogAlways("Failed to allocate shared memory buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    auto bufferHandle = buffer->createReadOnlyHandle();
    if (!bufferHandle) {
        WTFLogAlways("Failed to create handle for shared memory buffer");
        return nullptr;
    }

    return makeUnique<RenderTargetSHMImage>(surfaceID, size, Ref { *buffer }, WTFMove(*bufferHandle));
}

AcceleratedSurfaceDMABuf::RenderTargetSHMImage::RenderTargetSHMImage(uint64_t surfaceID, const WebCore::IntSize& size, Ref<WebCore::ShareableBitmap>&& bitmap, WebCore::ShareableBitmap::Handle&& bitmapHandle)
    : RenderTargetColorBuffer(surfaceID, size)
    , m_bitmap(WTFMove(bitmap))
{
    glBindRenderbuffer(GL_RENDERBUFFER, m_colorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::DidCreateBufferSHM(m_id, WTFMove(bitmapHandle)), surfaceID);
}

void AcceleratedSurfaceDMABuf::RenderTargetSHMImage::didRenderFrame()
{
    RenderTarget::didRenderFrame();
    glReadPixels(0, 0, m_bitmap->size().width(), m_bitmap->size().height(), GL_BGRA, GL_UNSIGNED_BYTE, m_bitmap->mutableSpan().data());
}

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetTexture::create(uint64_t surfaceID, const WebCore::IntSize& size)
{
    unsigned texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
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
    if (!eglExportDMABUFImageMESA(display.eglDisplay(), image, fdsOut.data(), stridesOut.data(), offsetsOut.data())) {
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

    return makeUnique<RenderTargetTexture>(surfaceID, size, texture, fourcc, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier);
}

AcceleratedSurfaceDMABuf::RenderTargetTexture::RenderTargetTexture(uint64_t surfaceID, const WebCore::IntSize& size, unsigned texture, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
    : RenderTarget(surfaceID, size)
    , m_texture(texture)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::DidCreateBuffer(m_id, size, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier, DMABufRendererBufferFormat::Usage::Rendering), surfaceID);
}

AcceleratedSurfaceDMABuf::RenderTargetTexture::~RenderTargetTexture()
{
    if (m_texture)
        glDeleteTextures(1, &m_texture);
}

void AcceleratedSurfaceDMABuf::RenderTargetTexture::willRenderFrame() const
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    RenderTarget::willRenderFrame();
}

AcceleratedSurfaceDMABuf::SwapChain::SwapChain(uint64_t surfaceID)
    : m_surfaceID(surfaceID)
{
    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    switch (display.type()) {
    case WebCore::PlatformDisplay::Type::Surfaceless:
        if (display.eglExtensions().MESA_image_dma_buf_export && WebProcess::singleton().dmaBufRendererBufferMode().contains(DMABufRendererBufferMode::Hardware))
            m_type = Type::Texture;
        else
            m_type = Type::SharedMemory;
        break;
#if USE(GBM)
    case WebCore::PlatformDisplay::Type::GBM:
        if (display.eglExtensions().EXT_image_dma_buf_import)
            m_type = Type::EGLImage;
        else
            m_type = Type::SharedMemory;
        break;
#endif
    default:
        break;
    }
}

#if USE(GBM)
void AcceleratedSurfaceDMABuf::SwapChain::setupBufferFormat(const Vector<DMABufRendererBufferFormat>& preferredFormats, bool isOpaque)
{
    auto isOpaqueFormat = [](uint32_t fourcc) -> bool {
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
    Locker locker { m_dmabufFormatLock };
    BufferFormat dmabufFormat;
    const auto& supportedFormats = WebCore::PlatformDisplay::sharedDisplay().dmabufFormats();
    for (const auto& bufferFormat : preferredFormats) {
        for (const auto& format : supportedFormats) {
            auto index = bufferFormat.formats.findIf([&](const auto& item) {
                return format.fourcc == item.fourcc;
            });
            if (index != notFound) {
                const auto& preferredFormat = bufferFormat.formats[index];

                bool matchesOpacity = isOpaqueFormat(preferredFormat.fourcc) == isOpaque;
                if (!matchesOpacity && dmabufFormat.fourcc)
                    continue;

                dmabufFormat.usage = bufferFormat.usage;
                dmabufFormat.drmDevice = bufferFormat.drmDevice;
                dmabufFormat.fourcc = preferredFormat.fourcc;
                if (preferredFormat.modifiers[0] == DRM_FORMAT_MOD_INVALID)
                    dmabufFormat.modifiers = preferredFormat.modifiers;
                else {
                    dmabufFormat.modifiers = WTF::compactMap(preferredFormat.modifiers, [&format](uint64_t modifier) -> std::optional<uint64_t> {
                        if (format.modifiers.contains(modifier))
                            return modifier;
                        return std::nullopt;
                    });
                }

                if (matchesOpacity)
                    break;
            }
        }

        if (dmabufFormat.fourcc)
            break;
    }

    if (!dmabufFormat.fourcc || dmabufFormat == m_dmabufFormat)
        return;

    if (!dmabufFormat.drmDevice.isNull()) {
        if (dmabufFormat.drmDevice == m_dmabufFormat.drmDevice)
            dmabufFormat.drmDeviceNode = m_dmabufFormat.drmDeviceNode;
        else
            dmabufFormat.drmDeviceNode = WebCore::DRMDeviceManager::singleton().deviceNode(dmabufFormat.drmDevice);
    }

    m_dmabufFormat = WTFMove(dmabufFormat);
    m_dmabufFormatChanged = true;
}
#endif

void AcceleratedSurfaceDMABuf::SwapChain::resize(const WebCore::IntSize& size)
{
    if (m_size == size)
        return;

    m_size = size;
    reset();
}

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::SwapChain::createTarget() const
{
    switch (m_type) {
#if USE(GBM)
    case Type::EGLImage:
        return RenderTargetEGLImage::create(m_surfaceID, m_size, m_dmabufFormat);
#endif
    case Type::Texture:
        return RenderTargetTexture::create(m_surfaceID, m_size);
    case Type::SharedMemory:
        return RenderTargetSHMImage::create(m_surfaceID, m_size);
    case Type::Invalid:
        break;
    }
    return nullptr;
}

AcceleratedSurfaceDMABuf::RenderTarget* AcceleratedSurfaceDMABuf::SwapChain::nextTarget()
{
#if USE(GBM)
    Locker locker { m_dmabufFormatLock };
    if (m_dmabufFormatChanged) {
        reset();
        m_dmabufFormatChanged = false;
    }
#endif

    if (m_freeTargets.isEmpty()) {
        ASSERT(m_lockedTargets.size() < s_maximumBuffers);
        m_lockedTargets.insert(0, createTarget());
        return m_lockedTargets[0].get();
    }

    auto target = m_freeTargets.takeLast();
    m_lockedTargets.insert(0, WTFMove(target));
    return m_lockedTargets[0].get();
}

void AcceleratedSurfaceDMABuf::SwapChain::releaseTarget(uint64_t targetID, UnixFileDescriptor&& releaseFence)
{
    auto index = m_lockedTargets.reverseFindIf([targetID](const auto& item) {
        return item->id() == targetID;
    });
    if (index != notFound) {
        m_lockedTargets[index]->setReleaseFenceFD(WTFMove(releaseFence));
        m_freeTargets.insert(0, WTFMove(m_lockedTargets[index]));
        m_lockedTargets.remove(index);
    }
}

void AcceleratedSurfaceDMABuf::SwapChain::reset()
{
    m_lockedTargets.clear();
    m_freeTargets.clear();
}

void AcceleratedSurfaceDMABuf::SwapChain::releaseUnusedBuffers()
{
    m_freeTargets.clear();
}

void AcceleratedSurfaceDMABuf::SwapChain::addDamage(const WebCore::Damage& damage)
{
    for (auto& renderTarget : m_freeTargets)
        renderTarget->addDamage(damage);
    for (auto& renderTarget : m_lockedTargets)
        renderTarget->addDamage(damage);
}

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
void AcceleratedSurfaceDMABuf::preferredBufferFormatsDidChange()
{
    if (m_swapChain.type() != SwapChain::Type::EGLImage)
        return;

    m_swapChain.setupBufferFormat(m_webPage->preferredBufferFormats(), m_isOpaque);
}
#endif

void AcceleratedSurfaceDMABuf::visibilityDidChange(bool isVisible)
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

bool AcceleratedSurfaceDMABuf::backgroundColorDidChange()
{
    if (!AcceleratedSurface::backgroundColorDidChange())
        return false;

#if USE(GBM)
    if (m_swapChain.type() == SwapChain::Type::EGLImage)
        m_swapChain.setupBufferFormat(m_webPage->preferredBufferFormats(), m_isOpaque);
#endif

    return true;
}

void AcceleratedSurfaceDMABuf::releaseUnusedBuffersTimerFired()
{
    m_swapChain.releaseUnusedBuffers();
}

void AcceleratedSurfaceDMABuf::didCreateCompositingRunLoop(RunLoop& runLoop)
{
    m_releaseUnusedBuffersTimer = makeUnique<RunLoop::Timer>(runLoop, this, &AcceleratedSurfaceDMABuf::releaseUnusedBuffersTimerFired);
#if USE(GLIB_EVENT_LOOP)
    m_releaseUnusedBuffersTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
    WebProcess::singleton().parentProcessConnection()->addMessageReceiver(runLoop, *this, Messages::AcceleratedSurfaceDMABuf::messageReceiverName(), m_id);
}

void AcceleratedSurfaceDMABuf::willDestroyCompositingRunLoop()
{
    m_releaseUnusedBuffersTimer = nullptr;
    WebProcess::singleton().parentProcessConnection()->removeMessageReceiver(Messages::AcceleratedSurfaceDMABuf::messageReceiverName(), m_id);
}

void AcceleratedSurfaceDMABuf::didCreateGLContext()
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void AcceleratedSurfaceDMABuf::willDestroyGLContext()
{
    m_swapChain.reset();

    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
}

uint64_t AcceleratedSurfaceDMABuf::surfaceID() const
{
    return m_id;
}

void AcceleratedSurfaceDMABuf::clientResize(const WebCore::IntSize& size)
{
    m_swapChain.resize(size);
}

void AcceleratedSurfaceDMABuf::willRenderFrame()
{
    m_target = m_swapChain.nextTarget();
    if (!m_target)
        return;

    m_target->waitRelease();

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    m_target->willRenderFrame();
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        WTFLogAlways("AcceleratedSurfaceDMABuf was unable to construct a complete framebuffer");
}

void AcceleratedSurfaceDMABuf::didRenderFrame(WebCore::Region&& damage)
{
    TraceScope traceScope(WaitForCompositionCompletionStart, WaitForCompositionCompletionEnd);

    if (!m_target)
        return;

    UnixFileDescriptor renderingFence;
    if (auto fence = m_target->createRenderingFence(m_useExplicitSync)) {
        renderingFence = fence->exportFD();
        if (!renderingFence)
            fence->clientWait();
    } else
        glFlush();

    m_target->didRenderFrame();
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Frame(m_target->id(), WTFMove(damage), WTFMove(renderingFence)), m_id);
}

const WebCore::Damage& AcceleratedSurfaceDMABuf::addDamage(const WebCore::Damage& damage)
{
    m_swapChain.addDamage(damage);
    return m_target ? m_target->damage() : WebCore::Damage::invalid();
}

void AcceleratedSurfaceDMABuf::releaseBuffer(uint64_t targetID, UnixFileDescriptor&& releaseFence)
{
    m_swapChain.releaseTarget(targetID, WTFMove(releaseFence));
}

void AcceleratedSurfaceDMABuf::frameDone()
{
    frameComplete();
    m_target = nullptr;
}

} // namespace WebKit

#endif // (PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM)))
