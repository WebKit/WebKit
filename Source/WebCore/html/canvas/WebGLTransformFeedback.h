/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#include "WebGLObject.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {
class AbstractSlotVisitor;
}

namespace WTF {
class AbstractLocker;
}

namespace WebCore {

class WebGL2RenderingContext;
class WebGLBuffer;
class WebGLProgram;

class WebGLTransformFeedback final : public WebGLObject {
public:
    virtual ~WebGLTransformFeedback();

    static RefPtr<WebGLTransformFeedback> create(WebGL2RenderingContext&);

    bool isActive() const { return m_active; }
    bool isPaused() const { return m_paused; }

    void setActive(bool active) { m_active = active; }
    void setPaused(bool paused) { m_paused = paused; }

    // These are the indexed bind points for transform feedback buffers.
    // Returns false if index is out of range and the caller should
    // synthesize a GL error.
    void setBoundIndexedTransformFeedbackBuffer(const AbstractLocker&, GCGLuint index, WebGLBuffer*);
    bool getBoundIndexedTransformFeedbackBuffer(GCGLuint index, WebGLBuffer** outBuffer);
    bool hasBoundIndexedTransformFeedbackBuffer(const WebGLBuffer* buffer) { return m_boundIndexedTransformFeedbackBuffers.contains(buffer); }

    bool validateProgramForResume(WebGLProgram*) const;

    void didBind() { m_hasEverBeenBound = true; }

    WebGLProgram* program() const { return m_program.get(); }
    void setProgram(const AbstractLocker&, WebGLProgram&);

    void unbindBuffer(const AbstractLocker&, WebGLBuffer&);

    bool hasEnoughBuffers(GCGLuint numRequired) const;

    void addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor&);

    bool isUsable() const { return object() && !isDeleted(); }
    bool isInitialized() const { return m_hasEverBeenBound; }

private:
    WebGLTransformFeedback(WebGL2RenderingContext&, PlatformGLObject);

    void deleteObjectImpl(const AbstractLocker&, GraphicsContextGL*, PlatformGLObject) override;

    bool m_active { false };
    bool m_paused { false };
    bool m_hasEverBeenBound { false };
    unsigned m_programLinkCount { 0 };
    Vector<WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER>> m_boundIndexedTransformFeedbackBuffers;
    RefPtr<WebGLProgram> m_program;
};

} // namespace WebCore

#endif // ENABLE(WEBGL)
