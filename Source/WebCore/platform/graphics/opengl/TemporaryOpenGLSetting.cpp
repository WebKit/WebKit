/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(OPENGL) || USE(OPENGL_ES)
#include "TemporaryOpenGLSetting.h"

#if USE(LIBEPOXY)
#include "EpoxyShims.h"

#elif PLATFORM(COCOA)

#if USE(OPENGL_ES)
#include <OpenGLES/ES2/gl.h>
#elif USE(OPENGL)
#include <OpenGL/gl.h>
#endif

#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>
#include "OpenGLESShims.h"

#elif PLATFORM(GTK) || PLATFORM(WIN)
#include "OpenGLShims.h"
#endif

namespace WebCore {

TemporaryOpenGLSetting::TemporaryOpenGLSetting(GLenum capability, GLenum scopedState)
    : m_capability(capability)
    , m_scopedState(scopedState)
{
    m_originalState = ::glIsEnabled(m_capability);
    if (m_originalState == m_scopedState)
        return;

    if (GL_TRUE == m_scopedState)
        ::glEnable(m_capability);
    else
        ::glDisable(m_capability);
}

TemporaryOpenGLSetting::~TemporaryOpenGLSetting()
{
    if (m_originalState == m_scopedState)
        return;

    if (GL_TRUE == m_originalState)
        ::glEnable(m_capability);
    else
        ::glDisable(m_capability);
}

}

#endif // USE(OPENGL) || USE(OPENGL_ES)
