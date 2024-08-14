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

#if ENABLE(WEBGL)

#include "WebGLRenderingContextBase.h"
#include <memory>

namespace WebCore {

class WebGLTimerQueryEXT;

class WebGLRenderingContext final : public WebGLRenderingContextBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebGLRenderingContext);
public:
    static std::unique_ptr<WebGLRenderingContext> create(CanvasBase&, WebGLContextAttributes&&);

    ~WebGLRenderingContext();

    bool isWebGL1() const final { return true; }

    std::optional<WebGLExtensionAny> getExtension(const String&) final;
    std::optional<Vector<String>> getSupportedExtensions() final;

    WebGLAny getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname) final;

    long long getInt64Parameter(GCGLenum) final;

    GCGLint maxDrawBuffers() final;
    GCGLint maxColorAttachments() final;
    void initializeDefaultObjects() final;

    void addMembersToOpaqueRoots(JSC::AbstractSlotVisitor&) final;

protected:
    friend class EXTDisjointTimerQuery;

    WebGLBindingPoint<WebGLTimerQueryEXT, GraphicsContextGL::TIME_ELAPSED_EXT> m_activeQuery;

private:
    using WebGLRenderingContextBase::WebGLRenderingContextBase;
};

WebCoreOpaqueRoot root(const WebGLExtension<WebGLRenderingContext>*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGLRenderingContext, isWebGL1())

#endif
