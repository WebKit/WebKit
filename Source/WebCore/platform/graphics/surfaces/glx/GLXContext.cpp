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
#include "GLXContext.h"

#if USE(ACCELERATED_COMPOSITING) && USE(GLX)

namespace WebCore {

typedef GLXContext (*GLXCREATECONTEXTATTRIBSARBPROC)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
static GLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = 0;

static int Attribs[] = {
    GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
    GLX_LOSE_CONTEXT_ON_RESET_ARB,
    0 };

static void initializeARBExtensions(Display* display)
{
    static bool initialized = false;
    if (initialized)
        return;

    initialized = true;
    if (GLPlatformContext::supportsGLXExtension(display, "GLX_ARB_create_context_robustness"))
        glXCreateContextAttribsARB = reinterpret_cast<GLXCREATECONTEXTATTRIBSARBPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
}

GLXOffScreenContext::GLXOffScreenContext()
    : GLPlatformContext()
    , m_display(0)
{
}

bool GLXOffScreenContext::initialize(GLPlatformSurface* surface)
{
    if (!surface)
        return false;

    m_display = surface->sharedDisplay();
    if (!m_display)
        return false;

    GLXFBConfig config = surface->configuration();

    if (config) {
        initializeARBExtensions(m_display);

        if (glXCreateContextAttribsARB)
            m_contextHandle = glXCreateContextAttribsARB(m_display, config, 0, true, Attribs);

        if (m_contextHandle) {
            // The GLX_ARB_create_context_robustness spec requires that a context created with
            // GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB bit set must also support GL_ARB_robustness or
            // a version of OpenGL incorporating equivalent functionality.
            // The spec also defines similar requirements for attribute GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB.
            if (platformMakeCurrent(surface) && GLPlatformContext::supportsGLExtension("GL_ARB_robustness"))
                m_resetLostContext = true;
            else
                glXDestroyContext(m_display, m_contextHandle);
        }

        if (!m_contextHandle)
            m_contextHandle = glXCreateNewContext(m_display, config, GLX_RGBA_TYPE, 0, true);

        if (m_contextHandle)
            return true;
    }

    return false;
}

GLXOffScreenContext::~GLXOffScreenContext()
{
}

bool GLXOffScreenContext::isCurrentContext() const
{
    return m_contextHandle == glXGetCurrentContext();
}

bool GLXOffScreenContext::platformMakeCurrent(GLPlatformSurface* surface)
{
    return glXMakeCurrent(surface->sharedDisplay(), surface->handle(), m_contextHandle);
}

void GLXOffScreenContext::platformReleaseCurrent()
{
    glXMakeCurrent(m_display, 0, 0);
}

void GLXOffScreenContext::freeResources()
{
    if (m_contextHandle)
        glXDestroyContext(m_display, m_contextHandle);

    m_contextHandle = 0;
    m_display = 0;
}

void GLXOffScreenContext::destroy()
{
    freeResources();
    GLPlatformContext::destroy();
}

}

#endif
