/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#include "WebGLSharedObject.h"
#include <wtf/RefPtr.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

class WebGLBuffer final : public WebGLSharedObject {
public:
    static Ref<WebGLBuffer> create(WebGLRenderingContextBase&);
    virtual ~WebGLBuffer();

    bool associateBufferData(GCGLsizeiptr size);
    bool associateBufferData(JSC::ArrayBuffer*);
    bool associateBufferData(JSC::ArrayBufferView*);
    bool associateBufferSubData(GCGLintptr offset, JSC::ArrayBuffer*);
    bool associateBufferSubData(GCGLintptr offset, JSC::ArrayBufferView*);
    bool associateCopyBufferSubData(const WebGLBuffer& readBuffer, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr);

    void disassociateBufferData();

    GCGLsizeiptr byteLength() const;
    const RefPtr<JSC::ArrayBuffer> elementArrayBuffer() const { return m_elementArrayBuffer; }

    // Gets the cached max index for the given type if one has been set.
    Optional<unsigned> getCachedMaxIndex(GCGLenum type);
    // Sets the cached max index for the given type.
    void setCachedMaxIndex(GCGLenum type, unsigned value);

    GCGLenum getTarget() const { return m_target; }
    void setTarget(GCGLenum);

    GCGLenum arrayBufferOrElementArrayBuffer() const { return m_arrayBufferOrElementArrayBuffer; }

    bool hasEverBeenBound() const { return object() && m_target; }

protected:
    WebGLBuffer(WebGLRenderingContextBase&);

    void deleteObjectImpl(GraphicsContextGLOpenGL*, PlatformGLObject) override;

private:
    GCGLenum m_target { 0 };

    RefPtr<JSC::ArrayBuffer> m_elementArrayBuffer;
    GCGLsizeiptr m_byteLength { 0 };

    GCGLenum m_arrayBufferOrElementArrayBuffer { 0 };

    // Optimization for index validation. For each type of index
    // (i.e., UNSIGNED_SHORT), cache the maximum index in the
    // entire buffer.
    //
    // This is sufficient to eliminate a lot of work upon each
    // draw call as long as all bound array buffers are at least
    // that size.
    struct MaxIndexCacheEntry {
        GCGLenum type;
        unsigned maxIndex;
    };
    // OpenGL ES 2.0 only has two valid index types (UNSIGNED_BYTE
    // and UNSIGNED_SHORT) plus one extension (UNSIGNED_INT).
    MaxIndexCacheEntry m_maxIndexCache[4];
    unsigned m_nextAvailableCacheEntry { 0 };

    // Clears all of the cached max indices.
    void clearCachedMaxIndices();

    // Helper function called by the three associateBufferData().
    bool associateBufferDataImpl(const void* data, GCGLsizeiptr byteLength);
    // Helper function called by the two associateBufferSubData().
    bool associateBufferSubDataImpl(GCGLintptr offset, const void* data, GCGLsizeiptr byteLength);
};

} // namespace WebCore

#endif
