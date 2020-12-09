/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGL) && USE(ANGLE)

#include "ANGLEHeaders.h"
#include "GraphicsTypesGL.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class ScopedRestoreTextureBinding {
    WTF_MAKE_NONCOPYABLE(ScopedRestoreTextureBinding);
public:
    ScopedRestoreTextureBinding(GCGLenum bindingPointQuery, GCGLenum bindingPoint, bool condition = true)
    {
        ASSERT(bindingPoint != static_cast<GCGLenum>(0u));
        if (condition) {
            m_bindingPoint = bindingPoint;
            gl::GetIntegerv(bindingPointQuery, reinterpret_cast<GLint*>(&m_bindingValue));
        }
    }

    ~ScopedRestoreTextureBinding()
    {
        if (m_bindingPoint)
            gl::BindTexture(m_bindingPoint, m_bindingValue);
    }

private:
    GLenum m_bindingPoint { 0 };
    GLuint m_bindingValue { 0u };
};

}

#endif
