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

#ifndef WebGLRenderingContext_h
#define WebGLRenderingContext_h

#include "WebGLRenderingContextBase.h"

namespace WebCore {

class WebGLRenderingContext final : public WebGLRenderingContextBase {
public:
    WebGLRenderingContext(HTMLCanvasElement*, GraphicsContext3D::Attributes);
    WebGLRenderingContext(HTMLCanvasElement*, PassRefPtr<GraphicsContext3D>, GraphicsContext3D::Attributes);
    bool isWebGL1() const override { return true; }
    
    WebGLExtension* getExtension(const String&) override;
    WebGLGetInfo getParameter(GC3Denum pname, ExceptionCode&) override;
    Vector<String> getSupportedExtensions() override;

    WebGLGetInfo getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname, ExceptionCode&) override;
    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) override;
    bool validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment) override;
    void hint(GC3Denum target, GC3Denum mode) override;
    void clear(GC3Dbitfield mask) override;
    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border) override;
    void texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, const void* pixels, ExceptionCode&) override;
    void texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image*, GraphicsContext3D::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha, ExceptionCode&) override;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Dsizei width, GC3Dsizei height,
        GC3Denum format, GC3Denum type, ArrayBufferView*, ExceptionCode&) override;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, ImageData*, ExceptionCode&) override;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, HTMLImageElement*, ExceptionCode&) override;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, HTMLCanvasElement*, ExceptionCode&) override;
#if ENABLE(VIDEO)
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, HTMLVideoElement*, ExceptionCode&) override;
#endif

protected:
    GC3Dint getMaxDrawBuffers() override;
    GC3Dint getMaxColorAttachments() override;
    void initializeVertexArrayObjects() override;
    bool validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired) override;
    bool validateBlendEquation(const char* functionName, GC3Denum mode) override;
    bool validateTexFuncParameters(const char* functionName,
        TexFuncValidationFunctionType,
        GC3Denum target, GC3Dint level,
        GC3Denum internalformat,
        GC3Dsizei width, GC3Dsizei height, GC3Dint border,
        GC3Denum format, GC3Denum type) override;
    bool validateTexFuncFormatAndType(const char* functionName, GC3Denum internalformat, GC3Denum format, GC3Denum type, GC3Dint level) override;
    bool validateTexFuncData(const char* functionName, GC3Dint level,
        GC3Dsizei width, GC3Dsizei height,
        GC3Denum internalformat, GC3Denum format, GC3Denum type,
        ArrayBufferView* pixels,
        NullDisposition) override;
    bool validateCapability(const char* functionName, GC3Denum cap) override;
};
    
} // namespace WebCore

#endif
