/*
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

#ifndef ProgramBinding_h
#define ProgramBinding_h

#if USE(ACCELERATED_COMPOSITING)

#include "PlatformString.h"
#include "TraceEvent.h"

namespace WebCore {

class GraphicsContext3D;

class ProgramBindingBase {
public:
    ProgramBindingBase();
    ~ProgramBindingBase();

    void init(GraphicsContext3D*, const String& vertexShader, const String& fragmentShader);
    void cleanup(GraphicsContext3D*);

    unsigned program() const { ASSERT(m_initialized); return m_program; }
    bool initialized() const { return m_initialized; }

protected:

    unsigned loadShader(GraphicsContext3D*, unsigned type, const String& shaderSource);
    unsigned createShaderProgram(GraphicsContext3D*, const String& vertexShaderSource, const String& fragmentShaderSource);

    unsigned m_program;
    bool m_initialized;
};

template<class VertexShader, class FragmentShader>
class ProgramBinding : public ProgramBindingBase {
public:
    explicit ProgramBinding(GraphicsContext3D* context)
    {
        ProgramBindingBase::init(context, m_vertexShader.getShaderString(), m_fragmentShader.getShaderString());
    }

    void initialize(GraphicsContext3D* context)
    {
        ASSERT(context);
        ASSERT(m_program);
        ASSERT(!m_initialized);

        m_vertexShader.init(context, m_program);
        m_fragmentShader.init(context, m_program);
        m_initialized = true;
    }

    const VertexShader& vertexShader() const { return m_vertexShader; }
    const FragmentShader& fragmentShader() const { return m_fragmentShader; }

private:

    VertexShader m_vertexShader;
    FragmentShader m_fragmentShader;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
