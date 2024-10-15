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

#include "WebGLRenderingContextBase.h"
#include <memory>
#include <optional>

namespace WebCore {

#if ENABLE(WEB_CODECS)
class WebCodecsVideoFrame;
#endif
class WebGLQuery;
class WebGLSampler;
class WebGLSync;
class WebGLTransformFeedback;
class WebGLVertexArrayObject;

class WebGL2RenderingContext final : public WebGLRenderingContextBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebGL2RenderingContext);
public:
    static std::unique_ptr<WebGL2RenderingContext> create(CanvasBase&, WebGLContextAttributes&&);

    ~WebGL2RenderingContext();

    // Context state
    void pixelStorei(GCGLenum pname, GCGLint param) override;

    // Buffer objects
    using WebGLRenderingContextBase::bufferData;
    using WebGLRenderingContextBase::bufferSubData;
    void bufferData(GCGLenum target, const ArrayBufferView& data, GCGLenum usage, GCGLuint srcOffset, GCGLuint length);
    void bufferSubData(GCGLenum target, long long offset, const ArrayBufferView& data, GCGLuint srcOffset, GCGLuint length);
    void copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLint64 readOffset, GCGLint64 writeOffset, GCGLint64 size);
    void getBufferSubData(GCGLenum target, long long srcByteOffset, RefPtr<ArrayBufferView>&& dstData, GCGLuint dstOffset = 0, GCGLuint length = 0);
    
    // Framebuffer objects
    WebGLAny getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname) final;
    void bindFramebuffer(GCGLenum target, WebGLFramebuffer*) final;
    void blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter);
    void deleteFramebuffer(WebGLFramebuffer*) final;
    void framebufferTextureLayer(GCGLenum target, GCGLenum attachment, WebGLTexture*, GCGLint level, GCGLint layer);
    WebGLAny getInternalformatParameter(GCGLenum target, GCGLenum internalformat, GCGLenum pname);
    void invalidateFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments);
    void invalidateSubFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height);
    void readBuffer(GCGLenum src);
    
    // Renderbuffer objects
    void renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height);
    
    // Texture objects
    WebGLAny getTexParameter(GCGLenum target, GCGLenum pname) final;
    void texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height);
    void texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLsizei depth);

    // Must override the WebGL 1.0 signatures to add extra validation.
    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&&) override;
    ExceptionOr<void> texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, std::optional<TexImageSource>) override;
    // WebGL 2.0 entry points:
    void texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset);
    ExceptionOr<void> texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, TexImageSource&&);
    void texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset);

    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint64 pboOffset);
    ExceptionOr<void> texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, TexImageSource&&);
    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels);
    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset);

    // Must override the WebGL 1.0 signature to add extra validation.
    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&&) override;
    ExceptionOr<void> texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, std::optional<TexImageSource>&&) override;
    // WebGL 2.0 entry points:
    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr pboOffset);
    ExceptionOr<void> texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, TexImageSource&&);
    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset);

    void texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLint64 pboOffset);
    void texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels, GCGLuint srcOffset);
    ExceptionOr<void> texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, TexImageSource&&);

    void copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height);

    // Must override the WebGL 1.0 signature in order to add extra validation.
    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData);
    // WebGL 2.0 entry points:
    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLint64 offset);
    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride);
    void compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLint64 offset);
    void compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride);

    // Must override the WebGL 1.0 signature in order to add extra validation.
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& srcData);
    // WebGL 2.0 entry points:
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset);
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride);

    void compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLint64 offset);
    void compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride);

    // Programs and shaders
    GCGLint getFragDataLocation(WebGLProgram&, const String& name);

    // Uniforms and attributes
    using Uint32List = TypedList<Uint32Array, uint32_t>;
    using Float32List = TypedList<Float32Array, float>;
    void uniform1ui(const WebGLUniformLocation*, GCGLuint v0);
    void uniform2ui(const WebGLUniformLocation*, GCGLuint v0, GCGLuint v1);
    void uniform3ui(const WebGLUniformLocation*, GCGLuint v0, GCGLuint v1, GCGLuint v2);
    void uniform4ui(const WebGLUniformLocation*, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3);
    void uniform1uiv(const WebGLUniformLocation*, Uint32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform2uiv(const WebGLUniformLocation*, Uint32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform3uiv(const WebGLUniformLocation*, Uint32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform4uiv(const WebGLUniformLocation*, Uint32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix2x3fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& value, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix3x2fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& value, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix2x4fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& value, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix4x2fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& value, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix3x4fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& value, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix4x3fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& value, GCGLuint srcOffset, GCGLuint srcLength);
    void vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w);
    void vertexAttribI4iv(GCGLuint index, Int32List&& v);
    void vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w);
    void vertexAttribI4uiv(GCGLuint index, Uint32List&& v);
    void vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLint64 offset);

    using WebGLRenderingContextBase::uniform1fv;
    using WebGLRenderingContextBase::uniform2fv;
    using WebGLRenderingContextBase::uniform3fv;
    using WebGLRenderingContextBase::uniform4fv;
    void uniform1fv(const WebGLUniformLocation*, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform2fv(const WebGLUniformLocation*, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform3fv(const WebGLUniformLocation*, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform4fv(const WebGLUniformLocation*, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);

    using WebGLRenderingContextBase::uniform1iv;
    using WebGLRenderingContextBase::uniform2iv;
    using WebGLRenderingContextBase::uniform3iv;
    using WebGLRenderingContextBase::uniform4iv;
    void uniform1iv(const WebGLUniformLocation*, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform2iv(const WebGLUniformLocation*, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform3iv(const WebGLUniformLocation*, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniform4iv(const WebGLUniformLocation*, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength);

    using WebGLRenderingContextBase::uniformMatrix2fv;
    using WebGLRenderingContextBase::uniformMatrix3fv;
    using WebGLRenderingContextBase::uniformMatrix4fv;
    void uniformMatrix2fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix3fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);
    void uniformMatrix4fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength);

    // Writing to the drawing buffer
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor);
    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount);
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLint64 offset, GCGLsizei instanceCount);
    void drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLint64 offset);
    
    // Multiple render targets
    void drawBuffers(const Vector<GCGLenum>& buffers);
    void clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, Int32List&& values, GCGLuint srcOffset);
    void clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, Uint32List&& values, GCGLuint srcOffset);
    void clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, Float32List&& values, GCGLuint srcOffset);
    void clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil);
    
    // Query objects
    RefPtr<WebGLQuery> createQuery();
    void deleteQuery(WebGLQuery*);
    GCGLboolean isQuery(WebGLQuery*);
    void beginQuery(GCGLenum target, WebGLQuery&);
    void endQuery(GCGLenum target);
    WebGLAny getQuery(GCGLenum target, GCGLenum pname);
    WebGLAny getQueryParameter(WebGLQuery&, GCGLenum pname);
    
    // Sampler objects
    RefPtr<WebGLSampler> createSampler();
    void deleteSampler(WebGLSampler*);
    GCGLboolean isSampler(WebGLSampler*);
    void bindSampler(GCGLuint unit, WebGLSampler*);
    void samplerParameteri(WebGLSampler&, GCGLenum pname, GCGLint param);
    void samplerParameterf(WebGLSampler&, GCGLenum pname, GCGLfloat param);
    WebGLAny getSamplerParameter(WebGLSampler&, GCGLenum pname);
    
    // Sync objects
    RefPtr<WebGLSync> fenceSync(GCGLenum condition, GCGLbitfield flags);
    GCGLboolean isSync(WebGLSync*);
    void deleteSync(WebGLSync*);
    GCGLenum clientWaitSync(WebGLSync&, GCGLbitfield flags, GCGLuint64 timeout);
    void waitSync(WebGLSync&, GCGLbitfield flags, GCGLint64 timeout);
    WebGLAny getSyncParameter(WebGLSync&, GCGLenum pname);
    
    // Transform feedback
    RefPtr<WebGLTransformFeedback> createTransformFeedback();
    void deleteTransformFeedback(WebGLTransformFeedback* id);
    GCGLboolean isTransformFeedback(WebGLTransformFeedback* id);
    void bindTransformFeedback(GCGLenum target, WebGLTransformFeedback* id);
    void beginTransformFeedback(GCGLenum primitiveMode);
    void endTransformFeedback();
    void transformFeedbackVaryings(WebGLProgram&, const Vector<String>& varyings, GCGLenum bufferMode);
    RefPtr<WebGLActiveInfo> getTransformFeedbackVarying(WebGLProgram&, GCGLuint index);
    void pauseTransformFeedback();
    void resumeTransformFeedback();
    
    // Uniform buffer objects and transform feedback buffers
    void bindBufferBase(GCGLenum target, GCGLuint index, WebGLBuffer*);
    void bindBufferRange(GCGLenum target, GCGLuint index, WebGLBuffer*, GCGLint64 offset, GCGLint64 size);
    WebGLAny getIndexedParameter(GCGLenum target, GCGLuint index);
    std::optional<Vector<GCGLuint>> getUniformIndices(WebGLProgram&, const Vector<String>& uniformNames);
    WebGLAny getActiveUniforms(WebGLProgram&, const Vector<GCGLuint>& uniformIndices, GCGLenum pname);
    GCGLuint getUniformBlockIndex(WebGLProgram&, const String& uniformBlockName);
    WebGLAny getActiveUniformBlockParameter(WebGLProgram&, GCGLuint uniformBlockIndex, GCGLenum pname);
    WebGLAny getActiveUniformBlockName(WebGLProgram&, GCGLuint uniformBlockIndex);
    void uniformBlockBinding(WebGLProgram&, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding);
    
    // Vertex array objects
    RefPtr<WebGLVertexArrayObject> createVertexArray();
    void deleteVertexArray(WebGLVertexArrayObject* vertexArray);
    GCGLboolean isVertexArray(WebGLVertexArrayObject* vertexArray);
    void bindVertexArray(WebGLVertexArrayObject* vertexArray);
    
    std::optional<WebGLExtensionAny> getExtension(const String&) final;
    std::optional<Vector<String>> getSupportedExtensions() final;
    WebGLAny getParameter(GCGLenum pname) final;

    // Must override the WebGL 1.0 signature in order to add extra validation.
    void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels) override;
    void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset);
    void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, ArrayBufferView& dstData, GCGLuint dstOffset);

    GCGLuint maxTransformFeedbackSeparateAttribs() const;

    bool checkAndTranslateAttachments(ASCIILiteral functionName, GCGLenum, Vector<GCGLenum>&);

    void addMembersToOpaqueRoots(JSC::AbstractSlotVisitor&) override;

    bool isTransformFeedbackActiveAndNotPaused();

private:
    using WebGLRenderingContextBase::WebGLRenderingContextBase;
    void initializeContextState() final;

    RefPtr<ArrayBufferView> arrayBufferViewSliceFactory(ASCIILiteral functionName, const ArrayBufferView& data, unsigned startByte, unsigned bytelength);
    RefPtr<ArrayBufferView> sliceArrayBufferView(ASCIILiteral functionName, const ArrayBufferView& data, GCGLuint srcOffset, GCGLuint length);

    long long getInt64Parameter(GCGLenum) final;
    Vector<bool> getIndexedBooleanArrayParameter(GCGLenum pname, GCGLuint index);

    void initializeDefaultObjects() final;
    bool validateBufferTarget(ASCIILiteral functionName, GCGLenum target) final;
    bool validateBufferTargetCompatibility(ASCIILiteral, GCGLenum, WebGLBuffer*);
    WebGLBuffer* validateBufferDataParameters(ASCIILiteral functionName, GCGLenum target, GCGLenum usage) final;
    WebGLBuffer* validateBufferDataTarget(ASCIILiteral functionName, GCGLenum target) final;
    bool validateAndCacheBufferBinding(const AbstractLocker&, ASCIILiteral functionName, GCGLenum target, WebGLBuffer*) final;
    GCGLint maxDrawBuffers() final;
    GCGLint maxColorAttachments() final;
    bool validateCapability(ASCIILiteral functionName, GCGLenum cap) final;
    template<typename T, typename TypedArrayType>
    std::optional<std::span<const T>> validateClearBuffer(ASCIILiteral functionName, GCGLenum buffer, TypedList<TypedArrayType, T>& values, GCGLuint srcOffset);
    bool validateFramebufferTarget(GCGLenum target) final;
    WebGLFramebuffer* getFramebufferBinding(GCGLenum target) final;
    bool validateNonDefaultFramebufferAttachment(ASCIILiteral functionName, GCGLenum attachment);
    enum ActiveQueryKey { SamplesPassed = 0, PrimitivesWritten = 1, TimeElapsed = 2, NumKeys = 3 };
    std::optional<ActiveQueryKey> validateQueryTarget(ASCIILiteral functionName, GCGLenum target);
    void renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, ASCIILiteral functionName) final;
    void renderbufferStorageHelper(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height);

    bool setIndexedBufferBinding(ASCIILiteral functionName, GCGLenum target, GCGLuint index, WebGLBuffer*);

    IntRect getTextureSourceSubRectangle(GCGLsizei width, GCGLsizei height);

    RefPtr<WebGLTexture> validateTexImageBinding(TexImageFunctionID, GCGLenum) final;

    // Helper function to check texture 3D target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null. Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTexture3DBinding(ASCIILiteral functionName, GCGLenum target);

    // Helper function to check immutable texture 2D target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null. Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTextureStorage2DBinding(ASCIILiteral functionName, GCGLenum target);

    bool validateTexFuncLayer(ASCIILiteral functionName, GCGLenum texTarget, GCGLint layer);
    GCGLint maxTextureLevelForTarget(GCGLenum target) final;

    void uncacheDeletedBuffer(const AbstractLocker&, WebGLBuffer*) final;

    enum class ClearBufferCaller : uint8_t {
        ClearBufferiv,
        ClearBufferuiv,
        ClearBufferfv,
        ClearBufferfi
    };
    void updateBuffersToAutoClear(ClearBufferCaller, GCGLenum buffer, GCGLint drawbuffer);

    WebGLBindingPoint<WebGLFramebuffer> m_readFramebufferBinding;
    WebGLBindingPoint<WebGLTransformFeedback> m_boundTransformFeedback;
    RefPtr<WebGLTransformFeedback> m_defaultTransformFeedback;

    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::COPY_READ_BUFFER> m_boundCopyReadBuffer;
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::COPY_WRITE_BUFFER> m_boundCopyWriteBuffer;
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::PIXEL_PACK_BUFFER> m_boundPixelPackBuffer;
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::PIXEL_UNPACK_BUFFER> m_boundPixelUnpackBuffer;
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER> m_boundTransformFeedbackBuffer;
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::UNIFORM_BUFFER> m_boundUniformBuffer;
    Vector<WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::UNIFORM_BUFFER>> m_boundIndexedUniformBuffers;

    RefPtr<WebGLQuery> m_activeQueries[ActiveQueryKey::NumKeys];

    Vector<RefPtr<WebGLSampler>> m_boundSamplers;

    GCGLint m_uniformBufferOffsetAlignment { 0 };
    GCGLuint m_maxTransformFeedbackSeparateAttribs { 0 };
    GCGLint m_max3DTextureSize { 0 };
    GCGLint m_max3DTextureLevel { 0 };
    GCGLint m_maxArrayTextureLayers { 0 };

    friend class ScopedWebGLRestoreFramebuffer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGL2RenderingContext, isWebGL2())

#endif // WEBGL
