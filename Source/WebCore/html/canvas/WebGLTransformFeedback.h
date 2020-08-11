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

#pragma once

#if ENABLE(WEBGL2)

#include "WebGL2RenderingContext.h"
#include "WebGLSharedObject.h"

namespace JSC {
class SlotVisitor;
}

namespace WebCore {

class WebGLTransformFeedback final : public WebGLSharedObject {
public:
    virtual ~WebGLTransformFeedback();

    static Ref<WebGLTransformFeedback> create(WebGL2RenderingContext&);
    
    bool isActive() const { return m_active; }
    bool isPaused() const { return m_paused; }
    
    void setActive(bool active) { m_active = active; }
    void setPaused(bool paused) { m_paused = paused; }
    
    // These are the indexed bind points for transform feedback buffers.
    // Returns false if index is out of range and the caller should
    // synthesize a GL error.
    void setBoundIndexedTransformFeedbackBuffer(GCGLuint index, WebGLBuffer*);
    bool getBoundIndexedTransformFeedbackBuffer(GCGLuint index, WebGLBuffer** outBuffer);
    
    bool validateProgramForResume(WebGLProgram*) const;

    bool hasEverBeenBound() const { return object() && m_hasEverBeenBound; }
    void setHasEverBeenBound() { m_hasEverBeenBound = true; }
    
    WebGLProgram* program() const { return m_program.get(); }
    void setProgram(WebGLProgram&);
    
    void unbindBuffer(WebGLBuffer&);
    
    bool hasEnoughBuffers(GCGLuint numRequired) const;

    void visitReferencedJSWrappers(JSC::SlotVisitor&);

private:
    WebGLTransformFeedback(WebGL2RenderingContext&);

    void deleteObjectImpl(GraphicsContextGLOpenGL*, PlatformGLObject) override;
    
    bool m_active { false };
    bool m_paused { false };
    bool m_hasEverBeenBound { false };
    unsigned m_programLinkCount { 0 };
    Vector<RefPtr<WebGLBuffer>> m_boundIndexedTransformFeedbackBuffers;
    RefPtr<WebGLProgram> m_program;
};

} // namespace WebCore

#endif
