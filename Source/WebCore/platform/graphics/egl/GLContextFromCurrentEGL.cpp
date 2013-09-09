/*
 * Copyright (C) 2012-2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GLContextFromCurrentEGL.h"

#include "OpenGLShims.h"

namespace WebCore {

PassOwnPtr<GLContextFromCurrentEGL> GLContextFromCurrentEGL::createFromCurrentGLContext()
{
    if (!initialize())
        return nullptr;
    return adoptPtr(new GLContextFromCurrentEGL());
}

GLContextFromCurrentEGL::GLContextFromCurrentEGL()
    : m_display(eglGetCurrentDisplay())
    , m_surface(eglGetCurrentSurface(EGL_DRAW))
    , m_context(eglGetCurrentContext())
{
}

bool GLContextFromCurrentEGL::makeContextCurrent()
{
    GLContext::makeContextCurrent();
    return eglMakeCurrent(m_display, m_surface, m_surface, m_context);
}

bool GLContextFromCurrentEGL::initialize()
{
    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    return success;
}

} // namespace WebCore
