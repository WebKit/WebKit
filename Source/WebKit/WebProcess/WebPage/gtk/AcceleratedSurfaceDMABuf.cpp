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
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/DMABufFormat.h>
#include <WebCore/GBMDevice.h>
#include <WebCore/PlatformDisplay.h>
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
{
}

AcceleratedSurfaceDMABuf::~AcceleratedSurfaceDMABuf()
{
}

void AcceleratedSurfaceDMABuf::didCreateGLContext()
{
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    clientResize(m_size);
}

void AcceleratedSurfaceDMABuf::willDestroyGLContext()
{
    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    if (m_backImage) {
        display.destroyEGLImage(m_backImage);
        m_backImage = nullptr;
    }

    if (m_frontImage) {
        display.destroyEGLImage(m_frontImage);
        m_frontImage = nullptr;
    }

    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }

    if (m_depthStencilBuffer) {
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
        m_depthStencilBuffer = 0;
    }

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
    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();
    if (m_backImage) {
        display.destroyEGLImage(m_backImage);
        m_backImage = nullptr;
    }

    if (m_frontImage) {
        display.destroyEGLImage(m_frontImage);
        m_frontImage = nullptr;
    }

    if (m_depthStencilBuffer) {
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
        m_depthStencilBuffer = 0;
    }

    if (size.isEmpty())
        return;

    auto* backObject = gbm_bo_create(WebCore::GBMDevice::singleton().device(), size.width(), size.height(), uint32_t(WebCore::DMABufFormat::FourCC::ARGB8888), 0);
    if (!backObject) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
        return;
    }
    UnixFileDescriptor backFD(gbm_bo_get_fd(backObject), UnixFileDescriptor::Adopt);
    Vector<EGLAttrib> attributes = {
        EGL_WIDTH, static_cast<EGLAttrib>(gbm_bo_get_width(backObject)),
        EGL_HEIGHT, static_cast<EGLAttrib>(gbm_bo_get_height(backObject)),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(gbm_bo_get_format(backObject)),
        EGL_DMA_BUF_PLANE0_FD_EXT, backFD.value(),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLAttrib>(gbm_bo_get_offset(backObject, 0)),
        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLAttrib>(gbm_bo_get_stride(backObject)),
        EGL_NONE
    };
    m_backImage = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!m_backImage) {
        WTFLogAlways("Failed to create EGL image for DMABuf with file descriptor: %d", backFD.value());
        gbm_bo_destroy(backObject);
        return;
    }
    auto* frontObject = gbm_bo_create(WebCore::GBMDevice::singleton().device(), size.width(), size.height(), uint32_t(WebCore::DMABufFormat::FourCC::ARGB8888), 0);
    if (!frontObject) {
        WTFLogAlways("Failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
        display.destroyEGLImage(m_backImage);
        m_backImage = nullptr;
        gbm_bo_destroy(backObject);
        return;
    }
    UnixFileDescriptor frontFD(gbm_bo_get_fd(frontObject), UnixFileDescriptor::Adopt);
    attributes[7] = frontFD.value();
    m_frontImage = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!m_frontImage) {
        WTFLogAlways("Failed to create EGL image for DMABuf with file descriptor: %d", frontFD.value());
        gbm_bo_destroy(frontObject);
        display.destroyEGLImage(m_backImage);
        m_backImage = nullptr;
        gbm_bo_destroy(backObject);
        return;
    }

    glGenRenderbuffers(1, &m_depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Configure(WTFMove(backFD), WTFMove(frontFD), gbm_bo_get_format(backObject), gbm_bo_get_offset(backObject, 0), gbm_bo_get_stride(backObject), size), m_webPage.identifier());
    gbm_bo_destroy(backObject);
    gbm_bo_destroy(frontObject);
}

void AcceleratedSurfaceDMABuf::willRenderFrame()
{
    if (!m_backImage)
        return;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_backImage);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
}

void AcceleratedSurfaceDMABuf::didRenderFrame()
{
    if (!m_backImage)
        return;

    glFlush();
    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::AcceleratedBackingStoreDMABuf::Frame(), [this, weakThis = WeakPtr { *this }, runLoop = Ref { RunLoop::current() }]() mutable {
        // FIXME: it would be great if there was an option to send replies to the current run loop directly.
        runLoop->dispatch([this, weakThis = WTFMove(weakThis)] {
            if (!weakThis)
                return;
            std::swap(m_backImage, m_frontImage);
            m_client.frameComplete();
        });
    }, m_webPage.identifier());
}

} // namespace WebKit

#endif // USE(GBM)
