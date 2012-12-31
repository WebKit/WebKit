/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "EGLSurface.h"

#if USE(EGL) && USE(GRAPHICS_SURFACE)

namespace WebCore {

EGLWindowTransportSurface::EGLWindowTransportSurface()
    : GLPlatformSurface()
{
    m_nativeResource = adoptPtr(new NativeOffScreenWindow());
    m_configSelector = adoptPtr(new EGLConfigSelector(m_nativeResource->nativeSharedDisplay()));
    m_sharedDisplay = m_configSelector->display();

    if (m_sharedDisplay == EGL_NO_DISPLAY) {
        m_configSelector = nullptr;
        m_nativeResource = nullptr;
        return;
    }

    EGLConfig config = m_configSelector->surfaceContextConfig();

    if (!config) {
        destroy();
        return;
    }

    EGLint visualId = m_configSelector->nativeVisualId(config);

    if (visualId == -1) {
        destroy();
        return;
    }

    if (!m_nativeResource->setVisualId(visualId)) {
        destroy();
        return;
    }

    m_nativeResource->createOffscreenWindow(&m_bufferHandle);

    if (!m_bufferHandle) {
        destroy();
        return;
    }

    m_drawable = eglCreateWindowSurface(m_sharedDisplay, m_configSelector->surfaceContextConfig(), (EGLNativeWindowType)m_bufferHandle, 0);

    if (m_drawable == EGL_NO_SURFACE) {
        LOG_ERROR("Failed to create EGL surface(%d).", eglGetError());
        destroy();
    }
}

EGLWindowTransportSurface::~EGLWindowTransportSurface()
{
}

PlatformSurfaceConfig EGLWindowTransportSurface::configuration()
{
    return m_configSelector->surfaceContextConfig();
}

void EGLWindowTransportSurface::swapBuffers()
{
    if (!m_drawable)
        return;

    EGLBoolean success = eglBindAPI(eglAPIVersion);

    if (success != EGL_TRUE) {
        LOG_ERROR("Failed to set EGL API(%d).", eglGetError());
        destroy();
    }

    success = eglSwapBuffers(m_sharedDisplay, m_drawable);

    if (success != EGL_TRUE) {
        LOG_ERROR("Failed to SwapBuffers(%d).", eglGetError());
        destroy();
    }
}

void EGLWindowTransportSurface::destroy()
{
    m_nativeResource->destroyWindow(m_bufferHandle);
    freeEGLResources();
    m_nativeResource = nullptr;
    m_configSelector = nullptr;
    m_bufferHandle = 0;
}

void EGLWindowTransportSurface::freeEGLResources()
{
    if (m_drawable == EGL_NO_SURFACE || m_sharedDisplay == EGL_NO_DISPLAY)
        return;

    EGLBoolean eglResult = eglBindAPI(eglAPIVersion);

    if (eglResult != EGL_TRUE) {
        LOG_ERROR("Failed to set EGL API(%d).", eglGetError());
        return;
    }

    eglDestroySurface(m_sharedDisplay, m_drawable);
    m_drawable = EGL_NO_SURFACE;
}

void EGLWindowTransportSurface::setGeometry(const IntRect& newRect)
{
    GLPlatformSurface::setGeometry(newRect);
    m_nativeResource->reSizeWindow(newRect, m_bufferHandle);
}

}

#endif
