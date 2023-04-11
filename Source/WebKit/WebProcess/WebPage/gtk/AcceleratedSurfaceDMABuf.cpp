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
{
}

AcceleratedSurfaceDMABuf::~AcceleratedSurfaceDMABuf()
{
}

void AcceleratedSurfaceDMABuf::didCreateGLContext()
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void AcceleratedSurfaceDMABuf::willDestroyGLContext()
{
    auto& display = WebCore::PlatformDisplay::sharedDisplayForCompositing();

    if (m_back.image)
        display.destroyEGLImage(m_back.image);
    if (m_back.colorBuffer)
        glDeleteRenderbuffers(1, &m_back.colorBuffer);
    m_back = { };

    if (m_front.image)
        display.destroyEGLImage(m_front.image);
    if (m_front.colorBuffer)
        glDeleteRenderbuffers(1, &m_front.colorBuffer);
    m_front = { };

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

    if (m_back.image)
        display.destroyEGLImage(m_back.image);
    if (m_back.colorBuffer)
        glDeleteRenderbuffers(1, &m_back.colorBuffer);
    m_back = { };

    if (m_front.image)
        display.destroyEGLImage(m_front.image);
    if (m_front.colorBuffer)
        glDeleteRenderbuffers(1, &m_front.colorBuffer);
    m_front = { };

    if (m_depthStencilBuffer) {
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
        m_depthStencilBuffer = 0;
    }

    if (size.isEmpty())
        return;

    struct {
        uint32_t format;
        uint32_t offset;
        uint32_t stride;
        uint64_t modifier;
    } metadata;

    auto createImage = [&]() -> std::pair<UnixFileDescriptor, EGLImage> {
        auto* bo = gbm_bo_create(WebCore::GBMDevice::singleton().device(), size.width(), size.height(), uint32_t(WebCore::DMABufFormat::FourCC::ARGB8888), 0);
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
            attributes.append(Span<const EGLAttrib> { modifierAttributes });
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
        return;
    }

    m_back.image = backImage.second;
    m_front.image = frontImage.second;

    std::array<unsigned, 3> renderbuffers;
    glGenRenderbuffers(3, renderbuffers.data());

    m_back.colorBuffer = renderbuffers[0];
    glBindRenderbuffer(GL_RENDERBUFFER, m_back.colorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_back.image);

    m_front.colorBuffer = renderbuffers[1];
    glBindRenderbuffer(GL_RENDERBUFFER, m_front.colorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_front.image);

    m_depthStencilBuffer = renderbuffers[2];
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());

    WebProcess::singleton().parentProcessConnection()->send(Messages::AcceleratedBackingStoreDMABuf::Configure(WTFMove(backImage.first), WTFMove(frontImage.first),
        size, metadata.format, metadata.offset, metadata.stride, metadata.modifier), m_webPage.identifier());
}

void AcceleratedSurfaceDMABuf::willRenderFrame()
{
    if (!m_back.image)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_back.colorBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        WTFLogAlways("AcceleratedSurfaceDMABuf was unable to construct a complete framebuffer");
}

void AcceleratedSurfaceDMABuf::didRenderFrame()
{
    if (!m_back.image)
        return;

    glFlush();
    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::AcceleratedBackingStoreDMABuf::Frame(), [this, weakThis = WeakPtr { *this }, runLoop = Ref { RunLoop::current() }]() mutable {
        // FIXME: it would be great if there was an option to send replies to the current run loop directly.
        runLoop->dispatch([this, weakThis = WTFMove(weakThis)] {
            if (!weakThis)
                return;
            std::swap(m_back, m_front);
            m_client.frameComplete();
        });
    }, m_webPage.identifier());
}

} // namespace WebKit

#endif // USE(GBM)
