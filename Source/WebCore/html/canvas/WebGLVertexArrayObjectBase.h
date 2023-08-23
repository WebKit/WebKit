/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include "GraphicsContextGL.h"
#include "WebGLBuffer.h"
#include "WebGLObject.h"
#include <optional>

namespace JSC {
class AbstractSlotVisitor;
}

namespace WTF {
class AbstractLocker;
}

namespace WebCore {

class WebCoreOpaqueRoot;

class WebGLVertexArrayObjectBase : public WebGLObject {
public:
    enum class Type { Default, User };

    // Cached values for vertex attrib range checks
    struct VertexAttribState {
        bool isBound() const { return bufferBinding && bufferBinding->object(); }
        bool validateBinding() const { return !enabled || isBound(); }

        bool enabled { false };
        WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::ARRAY_BUFFER> bufferBinding;
        GCGLsizei bytesPerElement { 0 };
        GCGLint size { 4 };
        GCGLenum type { GraphicsContextGL::FLOAT };
        bool normalized { false };
        GCGLsizei stride { 16 };
        GCGLsizei originalStride { 0 };
        GCGLintptr offset { 0 };
        GCGLuint divisor { 0 };
        bool isInteger { false };
    };

    bool isDefaultObject() const { return m_type == Type::Default; }

    void didBind() { m_hasEverBeenBound = true; }

    WebGLBuffer* getElementArrayBuffer() const { return m_boundElementArrayBuffer.get(); }
    void setElementArrayBuffer(const AbstractLocker&, WebGLBuffer*);

    void setVertexAttribEnabled(int index, bool flag);
    const VertexAttribState& getVertexAttribState(int index) { return m_vertexAttribState[index]; }
    void setVertexAttribState(const AbstractLocker&, GCGLuint, GCGLsizei, GCGLint, GCGLenum, GCGLboolean, GCGLsizei, GCGLintptr, bool, WebGLBuffer*);
    bool hasArrayBuffer(WebGLBuffer* buffer) { return m_vertexAttribState.containsIf([&](auto& item) { return item.bufferBinding == buffer; }); }
    void unbindBuffer(const AbstractLocker&, WebGLBuffer&);

    void setVertexAttribDivisor(GCGLuint index, GCGLuint divisor);

    void addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor&);

    bool areAllEnabledAttribBuffersBound();

    bool isUsable() const { return object() && !isDeleted(); }
    bool isInitialized() const { return m_hasEverBeenBound; }

protected:
    WebGLVertexArrayObjectBase(WebGLRenderingContextBase&, PlatformGLObject, Type);
    void deleteObjectImpl(const AbstractLocker&, GraphicsContextGL*, PlatformGLObject) override = 0;

    Type m_type;
    bool m_hasEverBeenBound { false };
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::ELEMENT_ARRAY_BUFFER> m_boundElementArrayBuffer;
    Vector<VertexAttribState> m_vertexAttribState;
    std::optional<bool> m_allEnabledAttribBuffersBoundCache;
};

WebCoreOpaqueRoot root(WebGLVertexArrayObjectBase*);

} // namespace WebCore

#endif
