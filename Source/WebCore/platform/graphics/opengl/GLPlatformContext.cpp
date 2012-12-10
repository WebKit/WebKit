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
#include "GLPlatformContext.h"

#if USE(ACCELERATED_COMPOSITING)

#if HAVE(GLX)
#include "GLXContext.h"
#endif

#include "NotImplemented.h"

namespace WebCore {

static PFNGLGETGRAPHICSRESETSTATUSARBPROC glGetGraphicsResetStatusARB = 0;
static GLPlatformContext* m_currentContext = 0;

PassOwnPtr<GLPlatformContext> GLPlatformContext::createContext(GraphicsContext3D::RenderStyle renderStyle)
{
    if (!initializeOpenGLShims())
        return nullptr;

    if (!glGetGraphicsResetStatusARB) {
#if HAVE(GLX)
        glGetGraphicsResetStatusARB = reinterpret_cast<PFNGLGETGRAPHICSRESETSTATUSARBPROC>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glGetGraphicsResetStatusARB")));
#endif
    }

    switch (renderStyle) {
    case GraphicsContext3D::RenderOffscreen:
        if (OwnPtr<GLPlatformContext> glxContext = GLPlatformContext::createOffScreenContext())
            return glxContext.release();
        break;
    case GraphicsContext3D::RenderToCurrentGLContext:
        if (OwnPtr<GLPlatformContext> glxContext = GLPlatformContext::createCurrentContextWrapper())
            return glxContext.release();
        break;
    case GraphicsContext3D::RenderDirectlyToHostWindow:
        ASSERT_NOT_REACHED();
        break;
    }

    return nullptr;
}

PassOwnPtr<GLPlatformContext> GLPlatformContext::createOffScreenContext()
{
#if HAVE(GLX)
    OwnPtr<GLPlatformContext> glxContext = adoptPtr(new GLXOffScreenContext());
    return glxContext.release();
#endif

    return nullptr;
}

PassOwnPtr<GLPlatformContext> GLPlatformContext::createCurrentContextWrapper()
{
#if HAVE(GLX)
    OwnPtr<GLPlatformContext> glxContext = adoptPtr(new GLXCurrentContextWrapper());
    return glxContext.release();
#endif

    return nullptr;
}

GLPlatformContext::GLPlatformContext()
    : m_contextHandle(0)
    , m_resetLostContext(false)
{
}

GLPlatformContext::~GLPlatformContext()
{
    if (this == m_currentContext)
        m_currentContext = 0;
}

bool GLPlatformContext::makeCurrent(GLPlatformSurface* surface)
{
    m_contextLost = false;

    if (isCurrentContext())
        return true;

    m_currentContext = 0;

    if (!surface || (surface && !surface->handle()))
        platformReleaseCurrent();
    else if (platformMakeCurrent(surface))
        m_currentContext = this;

    if (m_resetLostContext && glGetGraphicsResetStatusARB) {
        GLenum status = glGetGraphicsResetStatusARB();

        switch (status) {
        case PLATFORMCONTEXT_NO_ERROR:
            break;
        case PLATFORMCONTEXT_GUILTY_CONTEXT_RESET:
            m_contextLost = true;
            break;
        case PLATFORMCONTEXT_INNOCENT_CONTEXT_RESET:
            break;
        case PLATFORMCONTEXT_UNKNOWN_CONTEXT_RESET:
            m_contextLost = true;
            break;
        default:
            break;
        }
    }

    return m_currentContext;
}

bool GLPlatformContext::isValid() const
{
    return !m_contextLost;
}

void GLPlatformContext::releaseCurrent()
{
    if (!isCurrentContext())
        return;

    m_currentContext = 0;
    platformReleaseCurrent();
}

PlatformContext GLPlatformContext::handle() const
{
    return m_contextHandle;
}

bool GLPlatformContext::isCurrentContext() const
{
    return true;
}

bool GLPlatformContext::initialize(GLPlatformSurface*)
{
    return true;
}

GLPlatformContext* GLPlatformContext::getCurrent()
{
    return m_currentContext;
}

bool GLPlatformContext::platformMakeCurrent(GLPlatformSurface*)
{
    return true;
}

void GLPlatformContext::platformReleaseCurrent()
{
    notImplemented();
}

void GLPlatformContext::destroy()
{
    m_contextHandle = 0;
    m_resetLostContext = false;

    if (this == m_currentContext)
        m_currentContext = 0;
}

} // namespace WebCore

#endif
