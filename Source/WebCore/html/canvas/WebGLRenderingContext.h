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

#include "WebGLRenderingContextBase.h"
#include <memory>

#if ENABLE(WEBGL)

namespace WebCore {

class WebGLRenderingContext final : public WebGLRenderingContextBase {
public:
    static std::unique_ptr<WebGLRenderingContext> create(CanvasBase&, GraphicsContext3DAttributes);
    static std::unique_ptr<WebGLRenderingContext> create(CanvasBase&, Ref<GraphicsContext3D>&&, GraphicsContext3DAttributes);

    bool isWebGL1() const final { return true; }

    WebGLExtension* getExtension(const String&) final;
    WebGLAny getParameter(GC3Denum pname) final;
    Optional<Vector<String>> getSupportedExtensions() final;

    WebGLAny getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname) final;
    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) final;
    bool validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment) final;
    void hint(GC3Denum target, GC3Denum mode) final;
    void clear(GC3Dbitfield mask) final;

    GC3Dint getMaxDrawBuffers() final;
    GC3Dint getMaxColorAttachments() final;
    void initializeVertexArrayObjects() final;
    bool validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired) final;
    bool validateBlendEquation(const char* functionName, GC3Denum mode) final;
    bool validateCapability(const char* functionName, GC3Denum cap) final;

private:
    WebGLRenderingContext(CanvasBase&, GraphicsContext3DAttributes);
    WebGLRenderingContext(CanvasBase&, Ref<GraphicsContext3D>&&, GraphicsContext3DAttributes);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGLRenderingContext, isWebGL1())

#endif
