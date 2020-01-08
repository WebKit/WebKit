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

#pragma once

#include "GraphicsTypesGL.h"

#if ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || USE(OPENGL_ES))

#include <wtf/Noncopyable.h>

namespace WebCore {

// TemporaryOpenGLSetting<> is useful for temporarily disabling (or enabling) a particular OpenGL
// feature with a particular scope. A TemporaryOpenGLSetting<> object returns the flag to its original
// value upon destruction, making it an alternative to checking, clearing, and resetting each flag
// at all of a block's exit points.
//
// Based on WTF::SetForScope<>

class TemporaryOpenGLSetting {
    WTF_MAKE_NONCOPYABLE(TemporaryOpenGLSetting);
public:
    TemporaryOpenGLSetting(GCGLenum capability, GCGLenum scopedState);
    ~TemporaryOpenGLSetting();

private:
    const GCGLenum m_capability;
    const GCGLenum m_scopedState;
    GCGLenum m_originalState;
};

}

using WebCore::TemporaryOpenGLSetting;

#endif
