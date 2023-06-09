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

#if USE(GBM)
#include "AcceleratedBackingStoreDMABufMessages.h"
#include "AcceleratedSurfaceDMABufMessages.h"
#include "ShareableBitmap.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/DMABufFormat.h>
#include <WebCore/GBMDevice.h>
#include <WebCore/PlatformDisplay.h>
#include <array>
#include <epoxy/egl.h>
#include <gbm.h>
#include <wtf/SafeStrerror.h>

namespace WebKit {

std::unique_ptr<AcceleratedSurfaceDMABuf> AcceleratedSurfaceDMABuf::create(WebPage& webPage, Client& client)
{
    return std::unique_ptr<AcceleratedSurfaceDMABuf>(new AcceleratedSurfaceDMABuf(webPage, client));
}

AcceleratedSurfaceDMABuf::AcceleratedSurfaceDMABuf(WebPage& webPage, Client& client)
    : AcceleratedSurface(webPage, client)
    , m_isSoftwareRast(WebCore::PlatformDisplay::sharedDisplayForCompositing().type() == WebCore::PlatformDisplay::Type::Surfaceless)
{
}

AcceleratedSurfaceDMABuf::~AcceleratedSurfaceDMABuf()
{
}

AcceleratedSurfaceDMABuf::RenderTarget::RenderTarget(WebCore::PageIdentifier pageID, const WebCore::IntSize& size)
    : m_pageID(pageID)
{
    std::array<unsigned, 3> renderbuffers;
    glGenRenderbuffers(3, renderbuffers.data());
    m_backColorBuffer = renderbuffers[0];
    m_frontColorBuffer = renderbuffers[1];
    m_depthStencilBuffer = renderbuffers[2];
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());
}

AcceleratedSurfaceDMABuf::RenderTarget::~RenderTarget()
{
    if (m_backColorBuffer)
        glDeleteRenderbuffers(1, &m_backColorBuffer);
    if (m_frontColorBuffer)
        glDeleteRenderbuffers(1, &m_frontColorBuffer);
    if (m_depthStencilBuffer)
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTarget::willRenderFrame() const
{
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_backColorBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
}

void AcceleratedSurfaceDMABuf::RenderTarget::swap()
{
    std::swap(m_backColorBuffer, m_frontColorBuffer);
}

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetEGLImage::create(WebCore::PageIdentifier pageID, const WebCore::IntSize& size)
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
        auto* bo = gbm_bo_create(device, size.width(), size.height(), uint32_t(WebCore::DMABufFormat::FourCC::ARGB8888), 0);
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
    if (!backImage.second || !frontImage.second) {
        WTFLogAlways("Failed to create EGL images for DMABufs with file descriptors %d and %d", backImage.first.value(), frontImage.first.value());
        if (backImage.second)
            display.destroyEGLImage(backImage.second);
        if (frontImage.second)
            display.destroyEGLImage(frontImage.second);
        return nullptr;
    }

    return makeUnique<RenderTargetEGLImage>(pageID, size, backImage.second, frontImage.second, WTFMove(backImage.first), WTFMove(frontImage.first), metadata.format, metadata.offset, metadata.stride, metadata.modifier);
}

AcceleratedSurfaceDMABuf::RenderTargetEGLImage::RenderTargetEGLImage(WebCore::PageIdentifier pageID, const WebCore::IntSize& size, EGLImage backImage, EGLImage frontImage, UnixFileDescriptor&& backFD, UnixFileDescriptor&& frontFD, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier)
    : RenderTarget(pageID, size)
    , m_backImage(backImage)
    , m_frontImage(frontImage)
{
    glBindRenderbuffer(GL_RENDERBUFFER, m_backColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_backImage);

    glBindRenderbuffer(GL_RENDERBUFFER, m_frontColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_frontImage);

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Configure(WTFMove(backFD), WTFMove(frontFD),
        size, format, offset, stride, modifier), pageID);
}

AcceleratedSurfaceDMABuf::RenderTargetEGLImage::~RenderTargetEGLImage()
{
    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    if (m_backImage)
        display.destroyEGLImage(m_backImage);
    if (m_frontImage)
        display.destroyEGLImage(m_frontImage);
}

void AcceleratedSurfaceDMABuf::RenderTargetEGLImage::swap()
{
    std::swap(m_backImage, m_frontImage);
    RenderTarget::swap();
}

std::unique_ptr<AcceleratedSurfaceDMABuf::RenderTarget> AcceleratedSurfaceDMABuf::RenderTargetSHMImage::create(WebCore::PageIdentifier pageID, const WebCore::IntSize& size)
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

    return makeUnique<RenderTargetSHMImage>(pageID, size, Ref { *backBuffer }, WTFMove(*backBufferHandle), Ref { *frontBuffer }, WTFMove(*frontBufferHandle));
}

AcceleratedSurfaceDMABuf::RenderTargetSHMImage::RenderTargetSHMImage(WebCore::PageIdentifier pageID, const WebCore::IntSize& size, Ref<ShareableBitmap>&& backBitmap, ShareableBitmap::Handle&& backBitmapHandle, Ref<ShareableBitmap>&& frontBitmap, ShareableBitmap::Handle&& frontBitmapHandle)
    : RenderTarget(pageID, size)
    , m_backBitmap(WTFMove(backBitmap))
    , m_frontBitmap(WTFMove(frontBitmap))
{
    glBindRenderbuffer(GL_RENDERBUFFER, m_backColorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());

    glBindRenderbuffer(GL_RENDERBUFFER, m_frontColorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size.width(), size.height());

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::ConfigureSHM(WTFMove(backBitmapHandle), WTFMove(frontBitmapHandle)), m_pageID);
}

void AcceleratedSurfaceDMABuf::RenderTargetSHMImage::didRenderFrame() const
{
    glReadPixels(0, 0, m_backBitmap->size().width(), m_backBitmap->size().height(), GL_BGRA, GL_UNSIGNED_BYTE, m_backBitmap->data());
}

void AcceleratedSurfaceDMABuf::RenderTargetSHMImage::swap()
{
    std::swap(m_backBitmap, m_frontBitmap);
    RenderTarget::swap();
}

void AcceleratedSurfaceDMABuf::didCreateCompositingRunLoop(RunLoop& runLoop)
{
    WebProcess::singleton().parentProcessConnection()->addMessageReceiver(runLoop, *this, Messages::AcceleratedSurfaceDMABuf::messageReceiverName(), m_webPage.identifier().toUInt64());
}

void AcceleratedSurfaceDMABuf::willDestroyCompositingRunLoop()
{
    WebProcess::singleton().parentProcessConnection()->removeMessageReceiver(Messages::AcceleratedSurfaceDMABuf::messageReceiverName(), m_webPage.identifier().toUInt64());
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
    return m_webPage.identifier().toUInt64();
}

void AcceleratedSurfaceDMABuf::clientResize(const WebCore::IntSize& size)
{
    m_target = nullptr;
    if (size.isEmpty())
        return;
    m_target = m_isSoftwareRast ? RenderTargetSHMImage::create(m_webPage.identifier(), size) : RenderTargetEGLImage::create(m_webPage.identifier(), size);
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
    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Frame(), m_webPage.identifier());
}

void AcceleratedSurfaceDMABuf::frameDone()
{
    if (m_target)
        m_target->swap();
    m_client.frameComplete();
}

} // namespace WebKit

#endif // USE(GBM)
