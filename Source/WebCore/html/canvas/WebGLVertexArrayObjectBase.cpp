/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "WebGLVertexArrayObjectBase.h"

#if ENABLE(WEBGL)

#include "WebGLRenderingContextBase.h"

namespace WebCore {

WebGLVertexArrayObjectBase::WebGLVertexArrayObjectBase(WebGLRenderingContextBase& context, Type type)
    : WebGLContextObject(context)
    , m_type(type)
{
    m_vertexAttribState.resize(context.getMaxVertexAttribs());
}

void WebGLVertexArrayObjectBase::setElementArrayBuffer(WebGLBuffer* buffer)
{
    if (buffer)
        buffer->onAttached();
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context()->graphicsContextGL());
    m_boundElementArrayBuffer = buffer;
    
}

void WebGLVertexArrayObjectBase::setVertexAttribState(GCGLuint index, GCGLsizei bytesPerElement, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset, WebGLBuffer* buffer)
{
    GCGLsizei validatedStride = stride ? stride : bytesPerElement;
    
    auto& state = m_vertexAttribState[index];
    
    if (buffer)
        buffer->onAttached();
    if (state.bufferBinding)
        state.bufferBinding->onDetached(context()->graphicsContextGL());
    
    state.bufferBinding = buffer;
    state.bytesPerElement = bytesPerElement;
    state.size = size;
    state.type = type;
    state.normalized = normalized;
    state.stride = validatedStride;
    state.originalStride = stride;
    state.offset = offset;
}

void WebGLVertexArrayObjectBase::unbindBuffer(WebGLBuffer& buffer)
{
    if (m_boundElementArrayBuffer == &buffer) {
        m_boundElementArrayBuffer->onDetached(context()->graphicsContextGL());
        m_boundElementArrayBuffer = nullptr;
    }
    
    for (size_t i = 0; i < m_vertexAttribState.size(); ++i) {
        auto& state = m_vertexAttribState[i];
        if (state.bufferBinding == &buffer) {
            buffer.onDetached(context()->graphicsContextGL());
            
#if !USE(ANGLE)
            if (!i && !context()->isGLES2Compliant()) {
                state.bufferBinding = context()->m_vertexAttrib0Buffer;
                state.bufferBinding->onAttached();
                state.bytesPerElement = 0;
                state.size = 4;
                state.type = GraphicsContextGL::FLOAT;
                state.normalized = false;
                state.stride = 16;
                state.originalStride = 0;
                state.offset = 0;
            } else
#endif
                state.bufferBinding = nullptr;
        }
    }
}

void WebGLVertexArrayObjectBase::setVertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    m_vertexAttribState[index].divisor = divisor;
}
    
}

#endif // ENABLE(WEBGL)
