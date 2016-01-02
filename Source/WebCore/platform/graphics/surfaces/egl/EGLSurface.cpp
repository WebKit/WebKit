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

#include "EGLConfigSelector.h"
#include "EGLHelper.h"
#include "GLPlatformContext.h"

#if PLATFORM(X11)
#include "EGLXSurface.h"
#endif

namespace WebCore {

std::unique_ptr<GLTransportSurface> EGLTransportSurface::createTransportSurface(const IntSize& size, SurfaceAttributes attributes)
{
    std::unique_ptr<GLTransportSurface> surface;
#if PLATFORM(X11)
    surface = std::make_unique<EGLWindowTransportSurface>(size, attributes);
#else
    UNUSED_PARAM(size);
    UNUSED_PARAM(attributes);
#endif

    if (surface)
        return WTFMove(surface);

    return nullptr;
}

std::unique_ptr<GLTransportSurfaceClient> EGLTransportSurface::createTransportSurfaceClient(const PlatformBufferHandle handle, const IntSize& size, bool hasAlpha)
{
    EGLHelper::resolveEGLBindings();
    std::unique_ptr<GLTransportSurfaceClient> client;
#if PLATFORM(X11)
    client = std::make_unique<EGLXTransportSurfaceClient>(handle, size, hasAlpha);
#else
    UNUSED_PARAM(handle);
    UNUSED_PARAM(size);
    UNUSED_PARAM(hasAlpha);
#endif

    if (client)
        return WTFMove(client);

    return nullptr;
}

EGLTransportSurface::EGLTransportSurface(const IntSize& size, SurfaceAttributes attributes)
    : GLTransportSurface(size, attributes)
{
    if (EGLHelper::eglDisplay() == EGL_NO_DISPLAY)
        return;

    m_configSelector = std::make_unique<EGLConfigSelector>(attributes);
}

GLPlatformSurface::SurfaceAttributes EGLTransportSurface::attributes() const
{
    return m_configSelector->attributes();
}

bool EGLTransportSurface::isCurrentDrawable() const
{
    return m_drawable == eglGetCurrentSurface(EGL_DRAW);
}

EGLTransportSurface::~EGLTransportSurface()
{
}

void EGLTransportSurface::destroy()
{
    if (m_drawable == EGL_NO_SURFACE || EGLHelper::eglDisplay() == EGL_NO_DISPLAY)
        return;

    GLTransportSurface::destroy();

    if (m_drawable) {
        eglDestroySurface(EGLHelper::eglDisplay(), m_drawable);
        m_drawable = EGL_NO_SURFACE;
    }

    m_configSelector = nullptr;
}

PlatformSurfaceConfig EGLTransportSurface::configuration()
{
    return m_configSelector->surfaceContextConfig();
}

std::unique_ptr<GLPlatformSurface> EGLOffScreenSurface::createOffScreenSurface(SurfaceAttributes attributes)
{
#if PLATFORM(X11)
    return std::make_unique<EGLPixmapSurface>(attributes);
#else
    UNUSED_PARAM(attributes);
    return nullptr;
#endif
}

EGLOffScreenSurface::EGLOffScreenSurface(SurfaceAttributes surfaceAttributes)
    : GLPlatformSurface(surfaceAttributes)
{
    if (EGLHelper::eglDisplay() == EGL_NO_DISPLAY)
        return;

    m_configSelector = std::make_unique<EGLConfigSelector>(surfaceAttributes);
}

EGLOffScreenSurface::~EGLOffScreenSurface()
{
}

GLPlatformSurface::SurfaceAttributes EGLOffScreenSurface::attributes() const
{
    return m_configSelector->attributes();
}

bool EGLOffScreenSurface::isCurrentDrawable() const
{
    return m_drawable == eglGetCurrentSurface(EGL_DRAW);
}

PlatformSurfaceConfig EGLOffScreenSurface::configuration()
{
    return m_configSelector->pixmapContextConfig();
}

void EGLOffScreenSurface::destroy()
{
    if (EGLHelper::eglDisplay() == EGL_NO_DISPLAY || m_drawable == EGL_NO_SURFACE)
        return;

    if (m_drawable) {
        eglDestroySurface(EGLHelper::eglDisplay(), m_drawable);
        m_drawable = EGL_NO_SURFACE;
    }

    m_configSelector = nullptr;
}

}

#endif
