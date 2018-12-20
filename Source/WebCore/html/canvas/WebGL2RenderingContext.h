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

#include "WebGLRenderingContextBase.h"
#include <memory>

namespace WebCore {

class WebGLQuery;
class WebGLSampler;
class WebGLSync;
class WebGLTransformFeedback;
class WebGLVertexArrayObject;

class WebGL2RenderingContext final : public WebGLRenderingContextBase {
public:
    static std::unique_ptr<WebGL2RenderingContext> create(CanvasBase&, GraphicsContext3DAttributes);
    static std::unique_ptr<WebGL2RenderingContext> create(CanvasBase&, Ref<GraphicsContext3D>&&, GraphicsContext3DAttributes);

    // Buffer objects
    using WebGLRenderingContextBase::bufferData;
    using WebGLRenderingContextBase::bufferSubData;
    void bufferData(GC3Denum target, const ArrayBufferView& data, GC3Denum usage, GC3Duint srcOffset, GC3Duint length);
    void bufferSubData(GC3Denum target, long long offset, const ArrayBufferView& data, GC3Duint srcOffset, GC3Duint length);
    void copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dint64 readOffset, GC3Dint64 writeOffset, GC3Dint64 size);
    void getBufferSubData(GC3Denum target, long long srcByteOffset, RefPtr<ArrayBufferView>&& dstData, GC3Duint dstOffset = 0, GC3Duint length = 0);
    
    // Framebuffer objects
    WebGLAny getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname) final;
    void blitFramebuffer(GC3Dint srcX0, GC3Dint srcY0, GC3Dint srcX1, GC3Dint srcY1, GC3Dint dstX0, GC3Dint dstY0, GC3Dint dstX1, GC3Dint dstY1, GC3Dbitfield mask, GC3Denum filter);
    void framebufferTextureLayer(GC3Denum target, GC3Denum attachment, WebGLTexture*, GC3Dint level, GC3Dint layer);
    WebGLAny getInternalformatParameter(GC3Denum target, GC3Denum internalformat, GC3Denum pname);
    void invalidateFramebuffer(GC3Denum target, const Vector<GC3Denum>& attachments);
    void invalidateSubFramebuffer(GC3Denum target, const Vector<GC3Denum>& attachments, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void readBuffer(GC3Denum src);
    
    // Renderbuffer objects
    void renderbufferStorageMultisample(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);
    
    // Texture objects
    void texStorage2D(GC3Denum target, GC3Dsizei levels, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height);
    void texStorage3D(GC3Denum target, GC3Dsizei levels, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth);

#if ENABLE(VIDEO)
    using TexImageSource = WTF::Variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>, RefPtr<HTMLVideoElement>>;
#else
    using TexImageSource = WTF::Variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>>;
#endif

    using WebGLRenderingContextBase::texImage2D;
    void texImage2D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint64 pboOffset);
    void texImage2D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, TexImageSource&&);
    void texImage2D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&& srcData, GC3Duint srcOffset);

    void texImage3D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint64 pboOffset);
    void texImage3D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Denum format, GC3Denum type, TexImageSource&&);
    void texImage3D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&& pixels);
    void texImage3D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&& srcData, GC3Duint srcOffset);

    using WebGLRenderingContextBase::texSubImage2D;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dint64 pboOffset);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, TexImageSource&&);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&& srcData, GC3Duint srcOffset);

    void texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, GC3Denum type, GC3Dint64 pboOffset);
    void texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&& pixels, GC3Duint srcOffset);
    void texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, GC3Denum type, TexImageSource&&);

    void copyTexSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, GC3Dint64 offset);
    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, ArrayBufferView& data, GC3Duint, GC3Duint);
    void compressedTexImage3D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Dsizei imageSize, GC3Dint64 offset);
    void compressedTexImage3D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, ArrayBufferView& srcData, GC3Duint srcOffset, GC3Duint srcLengthOverride);

    void compressedTexSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, GC3Dsizei imageSize, GC3Dint64 offset);
    void compressedTexSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, ArrayBufferView& data, GC3Duint srcOffset, GC3Duint srcLengthOverride);

    // Programs and shaders
    GC3Dint getFragDataLocation(WebGLProgram&, const String& name);

    // Uniforms and attributes
    using Uint32List = TypedList<Uint32Array, uint32_t>;
    using Float32List = TypedList<Float32Array, float>;
    void uniform1ui(WebGLUniformLocation*, GC3Duint v0);
    void uniform2ui(WebGLUniformLocation*, GC3Duint v0, GC3Duint v1);
    void uniform3ui(WebGLUniformLocation*, GC3Duint v0, GC3Duint v1, GC3Duint v2);
    void uniform4ui(WebGLUniformLocation*, GC3Duint v0, GC3Duint v1, GC3Duint v2, GC3Duint v3);
    void uniform1uiv(WebGLUniformLocation*, Uint32List&& data, GC3Duint srcOffset, GC3Duint srcLength);
    void uniform2uiv(WebGLUniformLocation*, Uint32List&& data, GC3Duint srcOffset, GC3Duint srcLength);
    void uniform3uiv(WebGLUniformLocation*, Uint32List&& data, GC3Duint srcOffset, GC3Duint srcLength);
    void uniform4uiv(WebGLUniformLocation*, Uint32List&& data, GC3Duint srcOffset, GC3Duint srcLength);
    void uniformMatrix2x3fv(WebGLUniformLocation*, GC3Dboolean transpose, Float32List&& value, GC3Duint srcOffset, GC3Duint srcLength);
    void uniformMatrix3x2fv(WebGLUniformLocation*, GC3Dboolean transpose, Float32List&& value, GC3Duint srcOffset, GC3Duint srcLength);
    void uniformMatrix2x4fv(WebGLUniformLocation*, GC3Dboolean transpose, Float32List&& value, GC3Duint srcOffset, GC3Duint srcLength);
    void uniformMatrix4x2fv(WebGLUniformLocation*, GC3Dboolean transpose, Float32List&& value, GC3Duint srcOffset, GC3Duint srcLength);
    void uniformMatrix3x4fv(WebGLUniformLocation*, GC3Dboolean transpose, Float32List&& value, GC3Duint srcOffset, GC3Duint srcLength);
    void uniformMatrix4x3fv(WebGLUniformLocation*, GC3Dboolean transpose, Float32List&& value, GC3Duint srcOffset, GC3Duint srcLength);
    void vertexAttribI4i(GC3Duint index, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w);
    void vertexAttribI4iv(GC3Duint index, Int32List&& v);
    void vertexAttribI4ui(GC3Duint index, GC3Duint x, GC3Duint y, GC3Duint z, GC3Duint w);
    void vertexAttribI4uiv(GC3Duint index, Uint32List&& v);
    void vertexAttribIPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dsizei stride, GC3Dint64 offset);
    
    // Writing to the drawing buffer
    void clear(GC3Dbitfield mask) final;
    void vertexAttribDivisor(GC3Duint index, GC3Duint divisor);
    void drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei instanceCount);
    void drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dint64 offset, GC3Dsizei instanceCount);
    void drawRangeElements(GC3Denum mode, GC3Duint start, GC3Duint end, GC3Dsizei count, GC3Denum type, GC3Dint64 offset);
    
    // Multiple render targets
    void drawBuffers(const Vector<GC3Denum>& buffers);
    void clearBufferiv(GC3Denum buffer, GC3Dint drawbuffer, Int32List&& values, GC3Duint srcOffset);
    void clearBufferuiv(GC3Denum buffer, GC3Dint drawbuffer, Uint32List&& values, GC3Duint srcOffset);
    void clearBufferfv(GC3Denum buffer, GC3Dint drawbuffer, Float32List&& values, GC3Duint srcOffset);
    void clearBufferfi(GC3Denum buffer, GC3Dint drawbuffer, GC3Dfloat depth, GC3Dint stencil);
    
    // Query objects
    RefPtr<WebGLQuery> createQuery();
    void deleteQuery(WebGLQuery*);
    GC3Dboolean isQuery(WebGLQuery*);
    void beginQuery(GC3Denum target, WebGLQuery&);
    void endQuery(GC3Denum target);
    RefPtr<WebGLQuery> getQuery(GC3Denum target, GC3Denum pname);
    WebGLAny getQueryParameter(WebGLQuery&, GC3Denum pname);
    
    // Sampler objects
    RefPtr<WebGLSampler> createSampler();
    void deleteSampler(WebGLSampler*);
    GC3Dboolean isSampler(WebGLSampler*);
    void bindSampler(GC3Duint unit, WebGLSampler*);
    void samplerParameteri(WebGLSampler&, GC3Denum pname, GC3Dint param);
    void samplerParameterf(WebGLSampler&, GC3Denum pname, GC3Dfloat param);
    WebGLAny getSamplerParameter(WebGLSampler&, GC3Denum pname);
    
    // Sync objects
    RefPtr<WebGLSync> fenceSync(GC3Denum condition, GC3Dbitfield flags);
    GC3Dboolean isSync(WebGLSync*);
    void deleteSync(WebGLSync*);
    GC3Denum clientWaitSync(WebGLSync&, GC3Dbitfield flags, GC3Duint64 timeout);
    void waitSync(WebGLSync&, GC3Dbitfield flags, GC3Dint64 timeout);
    WebGLAny getSyncParameter(WebGLSync&, GC3Denum pname);
    
    // Transform feedback
    RefPtr<WebGLTransformFeedback> createTransformFeedback();
    void deleteTransformFeedback(WebGLTransformFeedback* id);
    GC3Dboolean isTransformFeedback(WebGLTransformFeedback* id);
    void bindTransformFeedback(GC3Denum target, WebGLTransformFeedback* id);
    void beginTransformFeedback(GC3Denum primitiveMode);
    void endTransformFeedback();
    void transformFeedbackVaryings(WebGLProgram&, const Vector<String>& varyings, GC3Denum bufferMode);
    RefPtr<WebGLActiveInfo> getTransformFeedbackVarying(WebGLProgram&, GC3Duint index);
    void pauseTransformFeedback();
    void resumeTransformFeedback();
    
    // Uniform buffer objects and transform feedback buffers
    void bindBufferBase(GC3Denum target, GC3Duint index, WebGLBuffer*);
    void bindBufferRange(GC3Denum target, GC3Duint index, WebGLBuffer*, GC3Dint64 offset, GC3Dint64 size);
    WebGLAny getIndexedParameter(GC3Denum target, GC3Duint index);
    Optional<Vector<GC3Duint>> getUniformIndices(WebGLProgram&, const Vector<String>& uniformNames);
    WebGLAny getActiveUniforms(WebGLProgram&, const Vector<GC3Duint>& uniformIndices, GC3Denum pname);
    GC3Duint getUniformBlockIndex(WebGLProgram&, const String& uniformBlockName);
    WebGLAny getActiveUniformBlockParameter(WebGLProgram&, GC3Duint uniformBlockIndex, GC3Denum pname);
    WebGLAny getActiveUniformBlockName(WebGLProgram&, GC3Duint uniformBlockIndex);
    void uniformBlockBinding(WebGLProgram&, GC3Duint uniformBlockIndex, GC3Duint uniformBlockBinding);
    
    // Vertex array objects
    RefPtr<WebGLVertexArrayObject> createVertexArray();
    void deleteVertexArray(WebGLVertexArrayObject* vertexArray);
    GC3Dboolean isVertexArray(WebGLVertexArrayObject* vertexArray);
    void bindVertexArray(WebGLVertexArrayObject* vertexArray);
    
    WebGLExtension* getExtension(const String&) final;
    Optional<Vector<String>> getSupportedExtensions() final;
    WebGLAny getParameter(GC3Denum pname) final;

    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) final;
    void hint(GC3Denum target, GC3Denum mode) final;

private:
    WebGL2RenderingContext(CanvasBase&, GraphicsContext3DAttributes);
    WebGL2RenderingContext(CanvasBase&, Ref<GraphicsContext3D>&&, GraphicsContext3DAttributes);

    bool isWebGL2() const final { return true; }

    void initializeVertexArrayObjects() final;
    GC3Dint getMaxDrawBuffers() final;
    GC3Dint getMaxColorAttachments() final;
    bool validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired) final;
    bool validateBlendEquation(const char* functionName, GC3Denum mode) final;
    bool validateCapability(const char* functionName, GC3Denum cap) final;
    bool validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment) final;
    bool validateFramebufferTarget(const char* functionName, GC3Denum target);
    bool validateNonDefaultFramebufferAttachment(const char* functionName, GC3Denum attachment);
    
    GC3Denum baseInternalFormatFromInternalFormat(GC3Denum internalformat);
    bool isIntegerFormat(GC3Denum internalformat);
    void initializeShaderExtensions();

    bool validateTexStorageFuncParameters(GC3Denum target, GC3Dsizei levels, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, const char* functionName);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGL2RenderingContext, isWebGL2())

#endif // WEBGL2
