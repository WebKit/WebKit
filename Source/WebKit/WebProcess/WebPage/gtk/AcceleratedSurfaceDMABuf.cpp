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

#include "AcceleratedBackingStoreDMABufMessages.h"
#include "AcceleratedSurfaceDMABufMessages.h"
#include "ShareableBitmap.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/DMABufFormat.h>
#include <WebCore/PlatformDisplay.h>
#include <array>
#include <epoxy/egl.h>
#include <wtf/SafeStrerror.h>

#if USE(GBM)
#include <WebCore/GBMDevice.h>
#include <gbm.h>
#endif

namespace WebKit {

static uint64_t generateID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

std::unique_ptr<AcceleratedSurfaceDMABuf> AcceleratedSurfaceDMABuf::create(WebPage& webPage, Client& client)
{
    return std::unique_ptr<AcceleratedSurfaceDMABuf>(new AcceleratedSurfaceDMABuf(webPage, client));
}

AcceleratedSurfaceDMABuf::AcceleratedSurfaceDMABuf(WebPage& webPage, Client& client)
    : AcceleratedSurface(webPage, client)
    , m_id(generateID())
{
}

AcceleratedSurfaceDMABuf::~AcceleratedSurfaceDMABuf()
{
}

AcceleratedSurfaceDMABuf::RenderTarget::RenderTarget(const WebCore::IntSize& size)
{
    glGenRenderbuffers(1, &m_depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());
}

AcceleratedSurfaceDMABuf::RenderTarget::~RenderTarget()
{
    if (m_depthStencilBuffer)
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTarget::willRenderFrame() const
{
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
}

AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::RenderTargetColorBuffer(const WebCore::IntSize& size)
    : RenderTarget(size)
{
    std::array<unsigned, 3> renderbuffers;
    glGenRenderbuffers(3, renderbuffers.data());
    m_backColorBuffer = renderbuffers[0];
    m_frontColorBuffer = renderbuffers[1];
    m_displayColorBuffer = renderbuffers[2];
}

AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::~RenderTargetColorBuffer()
{
    if (m_backColorBuffer)
        glDeleteRenderbuffers(1, &m_backColorBuffer);
    if (m_frontColorBuffer)
        glDeleteRenderbuffers(1, &m_frontColorBuffer);
    if (m_displayColorBuffer)
        glDeleteRenderbuffers(1, &m_displayColorBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::willRenderFrame() const
{
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_backColorBuffer);
    RenderTarget::willRenderFrame();
}

void AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::didRenderFrame()
{
    std::swap(m_backColorBuffer, m_frontColorBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTargetColorBuffer::didDisplayFrame()
{
    std::swap(m_frontColorBuffer, m_displayColorBuffer);
}

#if USE(GBM)
std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetEGLImage::create(uint64_t surfaceID, const WebCore::IntSize& size)
{
    struct {
        uint32_t format;
        uint32_t offset;
        uint32_t stride;
        uint64_t modifier;
    } metadata;

    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    auto createImage = [&]() -> std::pair<UnixFileDescriptor, EGLImage> {
        auto* device = WebCore::GBMDevice::singleton().device();
        if (!device) {
            WTFLogAlways("Failed to create GBM buffer of size %dx%d: no GBM device found", size.width(), size.height());
            return { };
        }
        auto* bo = gbm_bo_create(device, size.width(), size.height(), uint32_t(WebCore::DMABufFormat::FourCC::ARGB8888), GBM_BO_USE_RENDERING);
        if (!bo) {
            WTFLogAlways("Failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
            return { };
        }

        UnixFileDescriptor fd { gbm_bo_get_fd(bo), UnixFileDescriptor::Adopt };
        metadata.format = gbm_bo_get_format(bo);
        metadata.offset = gbm_bo_get_offset(bo, 0);
        metadata.stride = gbm_bo_get_stride(bo);
        metadata.modifier = gbm_bo_get_modifier(bo);

        Vector<EGLAttrib> attributes = {
            EGL_WIDTH, static_cast<EGLAttrib>(gbm_bo_get_width(bo)),
            EGL_HEIGHT, static_cast<EGLAttrib>(gbm_bo_get_height(bo)),
            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(metadata.format),
            EGL_DMA_BUF_PLANE0_FD_EXT, fd.value(),
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLAttrib>(metadata.offset),
            EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLAttrib>(metadata.stride),
        };
        if (metadata.modifier != uint64_t(WebCore::DMABufFormat::Modifier::Invalid) && display.eglExtensions().EXT_image_dma_buf_import_modifiers) {
            std::array<EGLAttrib, 4> modifierAttributes {
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLAttrib>(metadata.modifier >> 32),
                EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLAttrib>(metadata.modifier & 0xffffffff),
            };
            attributes.append(std::span<const EGLAttrib> { modifierAttributes });
        }
        attributes.append(EGL_NONE);

        EGLImage image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
        gbm_bo_destroy(bo);

        return { WTFMove(fd), image };
    };

    auto backImage = createImage();
    auto frontImage = createImage();
    auto displayImage = createImage();
    if (!backImage.second || !frontImage.second || !displayImage.second) {
        WTFLogAlways("Failed to create EGL images for DMABufs with file descriptors %d, %d and %d", backImage.first.value(), frontImage.first.value(), displayImage.first.value());
        if (backImage.second)
            display.destroyEGLImage(backImage.second);
        if (frontImage.second)
            display.destroyEGLImage(frontImage.second);
        if (displayImage.second)
            display.destroyEGLImage(displayImage.second);
        return nullptr;
    }

    return makeUnique<RenderTargetEGLImage>(surfaceID, size, backImage.second, WTFMove(backImage.first), frontImage.second, WTFMove(frontImage.first), displayImage.second, WTFMove(displayImage.first), metadata.format, metadata.offset, metadata.stride, metadata.modifier);
}

AcceleratedSurfaceDMABuf::RenderTargetEGLImage::RenderTargetEGLImage(uint64_t surfaceID, const WebCore::IntSize& size, EGLImage backImage, UnixFileDescriptor&& backFD, EGLImage frontImage, UnixFileDescriptor&& frontFD, EGLImage displayImage, UnixFileDescriptor&& displayFD, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier)
    : RenderTargetColorBuffer(size)
    , m_backImage(backImage)
    , m_frontImage(frontImage)
    , m_displayImage(displayImage)
{
    glBindRenderbuffer(GL_RENDERBUFFER, m_backColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_backImage);

    glBindRenderbuffer(GL_RENDERBUFFER, m_frontColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_frontImage);

    glBindRenderbuffer(GL_RENDERBUFFER, m_displayColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_displayImage);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Configure(WTFMove(backFD), WTFMove(frontFD), WTFMove(displayFD),
        size, format, offset, stride, modifier), surfaceID);
}

AcceleratedSurfaceDMABuf::RenderTargetEGLImage::~RenderTargetEGLImage()
{
    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    if (m_backImage)
        display.destroyEGLImage(m_backImage);
    if (m_frontImage)
        display.destroyEGLImage(m_frontImage);
    if (m_displayImage)
        display.destroyEGLImage(m_displayImage);
}

void AcceleratedSurfaceDMABuf::RenderTargetEGLImage::didRenderFrame()
{
    std::swap(m_backImage, m_frontImage);
    RenderTargetColorBuffer::didRenderFrame();
}

void AcceleratedSurfaceDMABuf::RenderTargetEGLImage::didDisplayFrame()
{
    std::swap(m_frontImage, m_displayImage);
    RenderTargetColorBuffer::didDisplayFrame();
}
#endif

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetSHMImage::create(uint64_t surfaceID, const WebCore::IntSize& size)
{
    auto backBuffer = ShareableBitmap::create({ size });
    if (!backBuffer) {
        WTFLogAlways("Failed to allocate shared memory buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    auto frontBuffer = ShareableBitmap::create({ size });
    if (!frontBuffer) {
        WTFLogAlways("Failed to allocate shared memory buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    auto displayBuffer = ShareableBitmap::create({ size });
    if (!displayBuffer) {
        WTFLogAlways("Failed to allocate shared memory buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    auto backBufferHandle = backBuffer->createReadOnlyHandle();
    if (!backBufferHandle) {
        WTFLogAlways("Failed to create handle for shared memory buffer");
        return nullptr;
    }

    auto frontBufferHandle = frontBuffer->createReadOnlyHandle();
    if (!frontBufferHandle) {
        WTFLogAlways("Failed to create handle for shared memory buffer");
        return nullptr;
    }

    auto displayBufferHandle = displayBuffer->createReadOnlyHandle();
    if (!displayBufferHandle) {
        WTFLogAlways("Failed to create handle for shared memory buffer");
        return nullptr;
    }

    return makeUnique<RenderTargetSHMImage>(surfaceID, size, Ref { *backBuffer }, WTFMove(*backBufferHandle), Ref { *frontBuffer }, WTFMove(*frontBufferHandle), Ref { *displayBuffer }, WTFMove(*displayBufferHandle));
}

AcceleratedSurfaceDMABuf::RenderTargetSHMImage::RenderTargetSHMImage(uint64_t surfaceID, const WebCore::IntSize& size, Ref<ShareableBitmap>&& backBitmap, ShareableBitmap::Handle&& backBitmapHandle, Ref<ShareableBitmap>&& frontBitmap, ShareableBitmap::Handle&& frontBitmapHandle, Ref<ShareableBitmap>&& displayBitmap, ShareableBitmap::Handle&& displayBitmapHandle)
    : RenderTargetColorBuffer(size)
    , m_backBitmap(WTFMove(backBitmap))
    , m_frontBitmap(WTFMove(frontBitmap))
    , m_displayBitmap(WTFMove(displayBitmap))
{
    glBindRenderbuffer(GL_RENDERBUFFER, m_backColorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());

    glBindRenderbuffer(GL_RENDERBUFFER, m_frontColorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());

    glBindRenderbuffer(GL_RENDERBUFFER, m_displayColorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::ConfigureSHM(WTFMove(backBitmapHandle), WTFMove(frontBitmapHandle), WTFMove(displayBitmapHandle)), surfaceID);
}

void AcceleratedSurfaceDMABuf::RenderTargetSHMImage::didRenderFrame()
{
    glReadPixels(0, 0, m_backBitmap->size().width(), m_backBitmap->size().height(), GL_BGRA, GL_UNSIGNED_BYTE, m_backBitmap->data());
    std::swap(m_backBitmap, m_frontBitmap);
    RenderTargetColorBuffer::didRenderFrame();
}

void AcceleratedSurfaceDMABuf::RenderTargetSHMImage::didDisplayFrame()
{
    std::swap(m_frontBitmap, m_displayBitmap);
    RenderTargetColorBuffer::didDisplayFrame();
}

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetTexture::create(uint64_t surfaceID, const WebCore::IntSize& size)
{
    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    std::array<unsigned, 3> textures;
    glGenTextures(3, textures.data());

    struct {
        uint32_t format;
        uint32_t offset;
        uint32_t stride;
        uint64_t modifier;
    } metadata;

    auto exportTexture = [&](unsigned texture) -> UnixFileDescriptor {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        auto image = display.createEGLImage(eglGetCurrentContext(), EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)texture, { });
        if (!image) {
            WTFLogAlways("Failed to create EGL image for texture");
            return { };
        }

        int fourcc;
        uint64_t modifier;
        if (!eglExportDMABUFImageQueryMESA(display.eglDisplay(), image, &fourcc, nullptr, &modifier)) {
            WTFLogAlways("eglExportDMABUFImageQueryMESA failed");
            display.destroyEGLImage(image);
            return { };
        }

        int fd, stride, offset;
        if (!eglExportDMABUFImageMESA(display.eglDisplay(), image, &fd, &stride, &offset)) {
            WTFLogAlways("eglExportDMABUFImageMESA failed");
            display.destroyEGLImage(image);
            return { };
        }

        display.destroyEGLImage(image);

        metadata.format = fourcc;
        metadata.offset = offset;
        metadata.stride = stride;
        metadata.modifier = modifier;
        return UnixFileDescriptor(fd, UnixFileDescriptor::Adopt);
    };

    auto backFD = exportTexture(textures[0]);
    auto frontFD = exportTexture(textures[1]);
    auto displayFD = exportTexture(textures[2]);
    if (!backFD || !frontFD || !displayFD) {
        glDeleteTextures(3, textures.data());
        return nullptr;
    }

    return makeUnique<RenderTargetTexture>(surfaceID, size, WTFMove(backFD), textures[0], WTFMove(frontFD), textures[1], WTFMove(displayFD), textures[2], metadata.format, metadata.offset, metadata.stride, metadata.modifier);
}

AcceleratedSurfaceDMABuf::RenderTargetTexture::RenderTargetTexture(uint64_t surfaceID, const WebCore::IntSize& size, UnixFileDescriptor&& backFD, unsigned backTexture, UnixFileDescriptor&& frontFD, unsigned frontTexture, UnixFileDescriptor&& displayFD, unsigned displayTexture, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier)
    : RenderTarget(size)
    , m_backTexture(backTexture)
    , m_frontTexture(frontTexture)
    , m_displayTexture(displayTexture)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Configure(WTFMove(backFD), WTFMove(frontFD), WTFMove(displayFD),
        size, format, offset, stride, modifier), surfaceID);
}

AcceleratedSurfaceDMABuf::RenderTargetTexture::~RenderTargetTexture()
{
    if (m_backTexture)
        glDeleteTextures(1, &m_backTexture);
    if (m_frontTexture)
        glDeleteTextures(1, &m_frontTexture);
    if (m_displayTexture)
        glDeleteTextures(1, &m_displayTexture);
}

void AcceleratedSurfaceDMABuf::RenderTargetTexture::willRenderFrame() const
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_backTexture, 0);
    RenderTarget::willRenderFrame();
}

void AcceleratedSurfaceDMABuf::RenderTargetTexture::didRenderFrame()
{
    std::swap(m_backTexture, m_frontTexture);
}

void AcceleratedSurfaceDMABuf::RenderTargetTexture::didDisplayFrame()
{
    std::swap(m_frontTexture, m_displayTexture);
}

void AcceleratedSurfaceDMABuf::didCreateCompositingRunLoop(RunLoop& runLoop)
{
    WebProcess::singleton().parentProcessConnection()->addMessageReceiver(runLoop, *this, Messages::AcceleratedSurfaceDMABuf::messageReceiverName(), m_id);
}

void AcceleratedSurfaceDMABuf::willDestroyCompositingRunLoop()
{
    WebProcess::singleton().parentProcessConnection()->removeMessageReceiver(Messages::AcceleratedSurfaceDMABuf::messageReceiverName(), m_id);
}

void AcceleratedSurfaceDMABuf::didCreateGLContext()
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void AcceleratedSurfaceDMABuf::willDestroyGLContext()
{
    m_target = nullptr;

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
    m_target = nullptr;
    if (size.isEmpty())
        return;

    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    switch (display.type()) {
    case WebCore::PlatformDisplay::Type::Surfaceless:
        if (display.eglExtensions().MESA_image_dma_buf_export && WebProcess::singleton().dmaBufRendererBufferMode().contains(DMABufRendererBufferMode::Hardware))
            m_target = RenderTargetTexture::create(m_id, size);
        else
            m_target = RenderTargetSHMImage::create(m_id, size);
        break;
#if USE(GBM)
    case WebCore::PlatformDisplay::Type::GBM:
        if (display.eglExtensions().EXT_image_dma_buf_import)
            m_target = RenderTargetEGLImage::create(m_id, size);
        else
            m_target = RenderTargetSHMImage::create(m_id, size);
        break;
#endif
    default:
        break;
    }
}

void AcceleratedSurfaceDMABuf::willRenderFrame()
{
    if (!m_target)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_target->willRenderFrame();
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        WTFLogAlways("AcceleratedSurfaceDMABuf was unable to construct a complete framebuffer");
}

void AcceleratedSurfaceDMABuf::didRenderFrame()
{
    if (!m_target)
        return;

    glFlush();

    m_target->didRenderFrame();
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Frame(), m_id);
}

void AcceleratedSurfaceDMABuf::frameDone()
{
    if (m_target)
        m_target->didDisplayFrame();
    m_client.frameComplete();
}

} // namespace WebKit
