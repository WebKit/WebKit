/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#include "EGLXSurface.h"

#if PLATFORM(X11) && USE(EGL) && USE(GRAPHICS_SURFACE)

#include "EGLConfigSelector.h"

namespace WebCore {

EGLWindowTransportSurface::EGLWindowTransportSurface(const IntSize& size, GLPlatformSurface::SurfaceAttributes attributes)
    : EGLTransportSurface(size, attributes)
{
    if (!m_configSelector)
        return;

    if (!m_configSelector->surfaceContextConfig()) {
        destroy();
        return;
    }

    EGLint visualId = m_configSelector->nativeVisualId(m_configSelector->surfaceContextConfig());

    if (visualId == -1) {
        destroy();
        return;
    }

    NativeWrapper::createOffScreenWindow(&m_bufferHandle, visualId, m_configSelector->attributes() & GLPlatformSurface::SupportAlpha, size);

    if (!m_bufferHandle) {
        destroy();
        return;
    }

    m_drawable = eglCreateWindowSurface(m_sharedDisplay, m_configSelector->surfaceContextConfig(), static_cast<EGLNativeWindowType>(m_bufferHandle), 0);

    if (m_drawable == EGL_NO_SURFACE) {
        LOG_ERROR("Failed to create EGL surface(%d).", eglGetError());
        destroy();
    }
}

EGLWindowTransportSurface::~EGLWindowTransportSurface()
{
}

void EGLWindowTransportSurface::swapBuffers()
{
    if (!eglSwapBuffers(m_sharedDisplay, m_drawable))
        LOG_ERROR("Failed to SwapBuffers(%d).", eglGetError());
}

void EGLWindowTransportSurface::destroy()
{
    EGLTransportSurface::destroy();

    if (m_bufferHandle) {
        NativeWrapper::destroyWindow(m_bufferHandle);
        m_bufferHandle = 0;
    }
}

EGLPixmapSurface::EGLPixmapSurface(GLPlatformSurface::SurfaceAttributes surfaceAttributes)
    : EGLOffScreenSurface(surfaceAttributes)
{
    if (!m_configSelector)
        return;

    EGLConfig config = m_configSelector->pixmapContextConfig();

    if (!config) {
        destroy();
        return;
    }

    EGLint visualId = m_configSelector->nativeVisualId(config);

    if (visualId == -1) {
        destroy();
        return;
    }

    NativePixmap pixmap;
    NativeWrapper::createPixmap(&pixmap, visualId, m_configSelector->attributes() & GLPlatformSurface::SupportAlpha);
    m_bufferHandle = pixmap;

    if (!m_bufferHandle) {
        destroy();
        return;
    }

    m_drawable = eglCreatePixmapSurface(m_sharedDisplay, config, static_cast<EGLNativePixmapType>(m_bufferHandle), 0);

    if (m_drawable == EGL_NO_SURFACE) {
        LOG_ERROR("Failed to create EGL surface(%d).", eglGetError());
        destroy();
    }
}

EGLPixmapSurface::~EGLPixmapSurface()
{
}

void EGLPixmapSurface::destroy()
{
    EGLOffScreenSurface::destroy();

    if (m_bufferHandle) {
        NativeWrapper::destroyPixmap(m_bufferHandle);
        m_bufferHandle = 0;
    }
}

}

#endif
