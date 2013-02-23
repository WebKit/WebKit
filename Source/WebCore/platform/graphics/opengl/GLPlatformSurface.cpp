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
#include "GLPlatformSurface.h"

#if USE(ACCELERATED_COMPOSITING)

#if HAVE(GLX)
#include "GLXSurface.h"
#endif

#if USE(EGL)
#include "EGLSurface.h"
#endif

#include "NotImplemented.h"

namespace WebCore {

PassOwnPtr<GLPlatformSurface> GLPlatformSurface::createOffScreenSurface(SurfaceAttributes attributes)
{
#if USE(GLX)
    OwnPtr<GLPlatformSurface> surface = adoptPtr(new GLXOffScreenSurface(attributes));

    if (surface->drawable())
        return surface.release();
#else
    UNUSED_PARAM(attributes);
#endif

    return nullptr;
}

PassOwnPtr<GLPlatformSurface> GLPlatformSurface::createTransportSurface(SurfaceAttributes attributes)
{
#if USE(GLX)
    OwnPtr<GLPlatformSurface> surface = adoptPtr(new GLXTransportSurface(attributes));
#elif USE(EGL)
    OwnPtr<GLPlatformSurface> surface = adoptPtr(new EGLWindowTransportSurface(attributes));
#endif

    if (surface && surface->handle() && surface->drawable())
        return surface.release();

    return nullptr;
}

GLPlatformSurface::GLPlatformSurface(SurfaceAttributes)
    : m_sharedDisplay(0)
    , m_drawable(0)
    , m_bufferHandle(0)
{
}

GLPlatformSurface::~GLPlatformSurface()
{
}

PlatformBufferHandle GLPlatformSurface::handle() const
{
    return m_bufferHandle;
}

PlatformDrawable GLPlatformSurface::drawable() const
{
    return m_drawable;
}

const IntRect& GLPlatformSurface::geometry() const
{
    return m_rect;
}

PlatformDisplay GLPlatformSurface::sharedDisplay() const
{
    return m_sharedDisplay;
}

PlatformSurfaceConfig GLPlatformSurface::configuration()
{
    return 0;
}

void GLPlatformSurface::swapBuffers()
{
    notImplemented();
}

void GLPlatformSurface::updateContents(const uint32_t)
{
}

void GLPlatformSurface::setGeometry(const IntRect&)
{
}

void GLPlatformSurface::destroy()
{
}

GLPlatformSurface::SurfaceAttributes GLPlatformSurface::attributes() const
{
    return GLPlatformSurface::Default;
}

}

#endif
