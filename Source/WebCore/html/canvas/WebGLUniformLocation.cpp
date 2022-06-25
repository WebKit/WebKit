/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(WEBGL)

#include "WebGLUniformLocation.h"

namespace WebCore {

Ref<WebGLUniformLocation> WebGLUniformLocation::create(WebGLProgram* program, GCGLint location, GCGLenum type)
{
    return adoptRef(*new WebGLUniformLocation(program, location, type));
}

WebGLUniformLocation::WebGLUniformLocation(WebGLProgram* program, GCGLint location, GCGLenum type)
    : m_program(program)
    , m_location(location)
    , m_type(type)
{
    ASSERT(m_program);
    m_linkCount = m_program->getLinkCount();
}

WebGLProgram* WebGLUniformLocation::program() const
{
    // If the program has been linked again, then this UniformLocation is no
    // longer valid.
    if (m_program->getLinkCount() != m_linkCount)
        return 0;
    return m_program.get();
}

GCGLint WebGLUniformLocation::location() const
{
    // If the program has been linked again, then this UniformLocation is no
    // longer valid.
    ASSERT(m_program->getLinkCount() == m_linkCount);
    return m_location;
}
    
GCGLenum WebGLUniformLocation::type() const
{
    // If the program has been linked again, then this UniformLocation is no
    // longer valid.
    ASSERT(m_program->getLinkCount() == m_linkCount);
    return m_type;
}

}

#endif // ENABLE(WEBGL)
