/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

namespace WebCore {

class WebGLRenderingContext final : public WebGLRenderingContextBase {
public:
    WebGLRenderingContext(HTMLCanvasElement*, GraphicsContext3D::Attributes);
    WebGLRenderingContext(HTMLCanvasElement*, PassRefPtr<GraphicsContext3D>, GraphicsContext3D::Attributes);

private:
    bool isWebGL1() const final { return true; }

    WebGLExtension* getExtension(const String&) final;
    WebGLGetInfo getParameter(GC3Denum pname) final;
    Vector<String> getSupportedExtensions() final;

    WebGLGetInfo getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname) final;
    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) final;
    bool validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment) final;
    void hint(GC3Denum target, GC3Denum mode) final;
    void clear(GC3Dbitfield mask) final;
    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border) final;
    void texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, const void* pixels) final;
    void texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image*, GraphicsContext3D::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha) final;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&&) final;
    ExceptionOr<void> texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Optional<TexImageSource>&&) final;

    GC3Dint getMaxDrawBuffers() final;
    GC3Dint getMaxColorAttachments() final;
    void initializeVertexArrayObjects() final;
    bool validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired) final;
    bool validateBlendEquation(const char* functionName, GC3Denum mode) final;
    bool validateTexFuncParameters(const char* functionName, TexFuncValidationFunctionType, GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type) final;
    bool validateTexFuncFormatAndType(const char* functionName, GC3Denum internalformat, GC3Denum format, GC3Denum type, GC3Dint level) final;
    bool validateTexFuncData(const char* functionName, GC3Dint level, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, ArrayBufferView* pixels, NullDisposition) final;
    bool validateCapability(const char* functionName, GC3Denum cap) final;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGLRenderingContext, isWebGL1())
