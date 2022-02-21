/*
 * Copyright (C) 2009, 2014-2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL) && !USE(ANGLE)

#include "ANGLEWebKitBridge.h"
#include "ExtensionsGLOpenGLCommon.h"
#include "GraphicsContextGL.h"
#include "GraphicsContextGLState.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/UniqueRef.h>

#if USE(NICOSIA)
namespace Nicosia {
class GCGLLayer;
}
#endif

namespace WebCore {
#if USE(OPENGL_ES)
class ExtensionsGLOpenGLES;
#else
class ExtensionsGLOpenGL;
#endif
class HostWindow;
class ImageBuffer;
class MediaPlayer;
class PixelBuffer;
#if USE(TEXTURE_MAPPER)
class TextureMapperGCGLPlatformLayer;
#endif

typedef HashMap<CString, uint64_t> ShaderNameHash;

// Base class for GraphicsContextGL contexts that use OpenGL or OpenGL ES.
class WEBCORE_EXPORT GraphicsContextGLOpenGL : public GraphicsContextGL
{
public:
    virtual ~GraphicsContextGLOpenGL();

    // With multisampling on, blit from multisampleFBO to regular FBO.
    void prepareTexture();

    // Get an attribute location without checking the name -> mangledname mapping.
    int getAttribLocationDirect(PlatformGLObject program, const String& name);

    // Compile a shader without going through ANGLE.
    void compileShaderDirect(PlatformGLObject);

    // Equivalent to ::glTexImage2D(). Allows pixels==0 with no allocation.
    void texImage2DDirect(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels);

    bool texImage2DResourceSafe(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLint alignment = 4);

    // GraphicsContextGL overrides.
    bool isGLES2Compliant() const final;
    //----------------------------------------------------------------------
    // Entry points for WebGL.
    //

    void activeTexture(GCGLenum texture) final;
    void attachShader(PlatformGLObject program, PlatformGLObject shader) final;
    void bindAttribLocation(PlatformGLObject, GCGLuint index, const String& name) final;
    void bindBuffer(GCGLenum target, PlatformGLObject) final;
    void bindFramebuffer(GCGLenum target, PlatformGLObject) final;
    void bindRenderbuffer(GCGLenum target, PlatformGLObject) final;
    void bindTexture(GCGLenum target, PlatformGLObject) final;
    void blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) final;
    void blendEquation(GCGLenum mode) final;
    void blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha) final;
    void blendFunc(GCGLenum sfactor, GCGLenum dfactor) final;
    void blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha) final;

    void bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage) final;
    void bufferData(GCGLenum target, GCGLSpan<const GCGLvoid> data, GCGLenum usage) final;
    void bufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<const GCGLvoid> data) final;

    GCGLenum checkFramebufferStatus(GCGLenum target) final;
    void clear(GCGLbitfield mask) final;
    void clearColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) final;
    void clearDepth(GCGLclampf depth) final;
    void clearStencil(GCGLint s) final;
    void colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha) final;
    void compileShader(PlatformGLObject) final;

    void copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border) final;
    void copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) final;
    void cullFace(GCGLenum mode) final;
    void depthFunc(GCGLenum func) final;
    void depthMask(GCGLboolean flag) final;
    void depthRange(GCGLclampf zNear, GCGLclampf zFar) final;
    void detachShader(PlatformGLObject, PlatformGLObject) final;
    void disable(GCGLenum cap) final;
    void disableVertexAttribArray(GCGLuint index) final;
    void drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count) final;
    void drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset) final;

    void enable(GCGLenum cap) final;
    void enableVertexAttribArray(GCGLuint index) final;
    void finish() final;
    void flush() final;
    void framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject) final;
    void framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject, GCGLint level) final;
    void frontFace(GCGLenum mode) final;
    void generateMipmap(GCGLenum target) final;

    bool getActiveAttrib(PlatformGLObject program, GCGLuint index, ActiveInfo&) final;
    bool getActiveAttribImpl(PlatformGLObject program, GCGLuint index, ActiveInfo&);
    bool getActiveUniform(PlatformGLObject program, GCGLuint index, ActiveInfo&) final;
    bool getActiveUniformImpl(PlatformGLObject program, GCGLuint index, ActiveInfo&);
    void getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders);
    GCGLint getAttribLocation(PlatformGLObject, const String& name) final;
    void getBooleanv(GCGLenum pname, GCGLSpan<GCGLboolean> value) final;
    GCGLint getBufferParameteri(GCGLenum target, GCGLenum pname) final;
    GCGLenum getError() final;
    void getFloatv(GCGLenum pname, GCGLSpan<GCGLfloat> value) final;
    GCGLint getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname) final;
    void getIntegerv(GCGLenum pname, GCGLSpan<GCGLint> value) final;
    GCGLint64 getInteger64(GCGLenum pname) final;
    GCGLint64 getInteger64i(GCGLenum pname, GCGLuint index) final;
    GCGLint getProgrami(PlatformGLObject program, GCGLenum pname) final;
    void getNonBuiltInActiveSymbolCount(PlatformGLObject program, GCGLenum pname, GCGLint* value);
    String getProgramInfoLog(PlatformGLObject) final;
    String getUnmangledInfoLog(PlatformGLObject[2], GCGLsizei, const String&);
    GCGLint getRenderbufferParameteri(GCGLenum target, GCGLenum pname) final;
    GCGLint getShaderi(PlatformGLObject, GCGLenum pname) final;
    String getShaderInfoLog(PlatformGLObject) final;
    void getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLSpan<GCGLint, 2> range, GCGLint* precision) final;
    String getShaderSource(PlatformGLObject) final;
    String getString(GCGLenum name) final;
    GCGLfloat getTexParameterf(GCGLenum target, GCGLenum pname) final;
    GCGLint getTexParameteri(GCGLenum target, GCGLenum pname) final;
    void getUniformfv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLfloat> value) final;
    void getUniformiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLint> value) final;
    void getUniformuiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLuint> value) final;
    GCGLint getUniformLocation(PlatformGLObject, const String& name) final;
    GCGLsizeiptr getVertexAttribOffset(GCGLuint index, GCGLenum pname) final;

    void hint(GCGLenum target, GCGLenum mode) final;
    GCGLboolean isBuffer(PlatformGLObject) final;
    GCGLboolean isEnabled(GCGLenum cap) final;
    GCGLboolean isFramebuffer(PlatformGLObject) final;
    GCGLboolean isProgram(PlatformGLObject) final;
    GCGLboolean isRenderbuffer(PlatformGLObject) final;
    GCGLboolean isShader(PlatformGLObject) final;
    GCGLboolean isTexture(PlatformGLObject) final;
    void lineWidth(GCGLfloat) final;
    void linkProgram(PlatformGLObject) final;
    void pixelStorei(GCGLenum pname, GCGLint param) final;
    void polygonOffset(GCGLfloat factor, GCGLfloat units) final;

    void readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data) final;
    void readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset) final;

    void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) final;
    void sampleCoverage(GCGLclampf value, GCGLboolean invert) final;
    void scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) final;
    void shaderSource(PlatformGLObject, const String& string) final;
    void stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask) final;
    void stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask) final;
    void stencilMask(GCGLuint mask) final;
    void stencilMaskSeparate(GCGLenum face, GCGLuint mask) final;
    void stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass) final;
    void stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass) final;

    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels) final;
    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset) final;
    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels) final;
    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset) final;
    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data) final;
    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLintptr offset) final;
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data) final;
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset) final;

    void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param) final;
    void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param) final;

    void uniform1f(GCGLint location, GCGLfloat x) final;
    void uniform1fv(GCGLint location, GCGLSpan<const GCGLfloat> v) final;
    void uniform1i(GCGLint location, GCGLint x) final;
    void uniform1iv(GCGLint location, GCGLSpan<const GCGLint> v) final;
    void uniform2f(GCGLint location, GCGLfloat x, GCGLfloat y) final;
    void uniform2fv(GCGLint location, GCGLSpan<const GCGLfloat> v) final;
    void uniform2i(GCGLint location, GCGLint x, GCGLint y) final;
    void uniform2iv(GCGLint location, GCGLSpan<const GCGLint> v) final;
    void uniform3f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z) final;
    void uniform3fv(GCGLint location, GCGLSpan<const GCGLfloat> v) final;
    void uniform3i(GCGLint location, GCGLint x, GCGLint y, GCGLint z) final;
    void uniform3iv(GCGLint location, GCGLSpan<const GCGLint> v) final;
    void uniform4f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) final;
    void uniform4fv(GCGLint location, GCGLSpan<const GCGLfloat> v) final;
    void uniform4i(GCGLint location, GCGLint x, GCGLint y, GCGLint z, GCGLint w) final;
    void uniform4iv(GCGLint location, GCGLSpan<const GCGLint> v) final;
    void uniformMatrix2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> value) final;
    void uniformMatrix3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> value) final;
    void uniformMatrix4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> value) final;

    void useProgram(PlatformGLObject) final;
    void validateProgram(PlatformGLObject) final;
    bool checkVaryingsPacking(PlatformGLObject vertexShader, PlatformGLObject fragmentShader) const;
    bool precisionsMatch(PlatformGLObject vertexShader, PlatformGLObject fragmentShader) const;

    void vertexAttrib1f(GCGLuint index, GCGLfloat x) final;
    void vertexAttrib1fv(GCGLuint index, GCGLSpan<const GCGLfloat, 1> values) final;
    void vertexAttrib2f(GCGLuint index, GCGLfloat x, GCGLfloat y) final;
    void vertexAttrib2fv(GCGLuint index, GCGLSpan<const GCGLfloat, 2> values) final;
    void vertexAttrib3f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z) final;
    void vertexAttrib3fv(GCGLuint index, GCGLSpan<const GCGLfloat, 3> values) final;
    void vertexAttrib4f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) final;
    void vertexAttrib4fv(GCGLuint index, GCGLSpan<const GCGLfloat, 4> values) final;
    void vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset) final;

    void viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) final;

    void reshape(int width, int height) final;

    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) final;
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount) final;
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor) final;

    // VertexArrayOject calls
    PlatformGLObject createVertexArray() final;
    void deleteVertexArray(PlatformGLObject) final;
    GCGLboolean isVertexArray(PlatformGLObject) final;
    void bindVertexArray(PlatformGLObject) final;

    // ========== WebGL2 entry points.

    void bufferData(GCGLenum target, const void* data, GCGLenum usage, GCGLuint srcOffset, GCGLuint length);
    void bufferSubData(GCGLenum target, GCGLintptr dstByteOffset, const void* srcData, GCGLuint srcOffset, GCGLuint length);
    void copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size) final;

    void getBufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<GCGLvoid> data) final;

    void blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter) final;
    void framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer) final;
    void invalidateFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments) final;
    void invalidateSubFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) final;
    void readBuffer(GCGLenum src) final;

    // getInternalFormatParameter
    void getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLSpan<GCGLint> data) final;
    void renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) final;

    void texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) final;
    void texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth) final;

    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels) final;
    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset) final;
    void texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels) final;
    void texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLintptr offset) final;
    void copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) final;
    void compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data) final;
    void compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLintptr offset) final;
    void compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data) final;
    void compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset) final;

    GCGLint getFragDataLocation(PlatformGLObject program, const String& name) final;

    void uniform1ui(GCGLint location, GCGLuint v0) final;
    void uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1) final;
    void uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2) final;
    void uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3) final;
    void uniform1uiv(GCGLint location, GCGLSpan<const GCGLuint>) final;
    void uniform2uiv(GCGLint location, GCGLSpan<const GCGLuint>) final;
    void uniform3uiv(GCGLint location, GCGLSpan<const GCGLuint>) final;
    void uniform4uiv(GCGLint location, GCGLSpan<const GCGLuint>) final;
    void uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data) final;
    void uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data) final;
    void uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data) final;
    void uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data) final;
    void uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data) final;
    void uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data) final;
    void vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w) final;
    void vertexAttribI4iv(GCGLuint index, GCGLSpan<const GCGLint, 4> values) final;
    void vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w) final;
    void vertexAttribI4uiv(GCGLuint index, GCGLSpan<const GCGLuint, 4> values) final;
    void vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset) final;

    void drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset) final;

    void drawBuffers(GCGLSpan<const GCGLenum> bufs) final;
    void clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLint> values) final;
    void clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLuint> values) final;
    void clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLfloat> values) final;
    void clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil) final;

    PlatformGLObject createQuery() final;
    void deleteQuery(PlatformGLObject query) final;
    GCGLboolean isQuery(PlatformGLObject query) final;
    void beginQuery(GCGLenum target, PlatformGLObject query) final;
    void endQuery(GCGLenum target) final;
    PlatformGLObject getQuery(GCGLenum target, GCGLenum pname) final;
    // getQueryParameter
    GCGLuint getQueryObjectui(PlatformGLObject query, GCGLenum pname) final;

    PlatformGLObject createSampler() final;
    void deleteSampler(PlatformGLObject sampler) final;
    GCGLboolean isSampler(PlatformGLObject sampler) final;
    void bindSampler(GCGLuint unit, PlatformGLObject sampler) final;
    void samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param) final;
    void samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param) final;
    // getSamplerParameter
    GCGLfloat getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname) final;
    GCGLint getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname) final;

    GCGLsync fenceSync(GCGLenum condition, GCGLbitfield flags) final;
    GCGLboolean isSync(GCGLsync) final;
    void deleteSync(GCGLsync) final;
    GCGLenum clientWaitSync(GCGLsync, GCGLbitfield flags, GCGLuint64 timeout) final;
    void waitSync(GCGLsync, GCGLbitfield flags, GCGLint64 timeout) final;
    // getSyncParameter
    // FIXME - this can be implemented at the WebGL level if we signal the WebGLSync object.
    GCGLint getSynci(GCGLsync, GCGLenum pname) final;

    PlatformGLObject createTransformFeedback() final;
    void deleteTransformFeedback(PlatformGLObject id) final;
    GCGLboolean isTransformFeedback(PlatformGLObject id) final;
    void bindTransformFeedback(GCGLenum target, PlatformGLObject id) final;
    void beginTransformFeedback(GCGLenum primitiveMode) final;
    void endTransformFeedback() final;
    void transformFeedbackVaryings(PlatformGLObject program, const Vector<String>& varyings, GCGLenum bufferMode) final;
    void getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index, ActiveInfo&) final;
    void pauseTransformFeedback() final;
    void resumeTransformFeedback() final;

    void bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer) final;
    void bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size) final;
    // getIndexedParameter -> use getParameter calls above.
    Vector<GCGLuint> getUniformIndices(PlatformGLObject program, const Vector<String>& uniformNames) final;
    Vector<GCGLint> getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname) final;

    GCGLuint getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName) final;
    // getActiveUniformBlockParameter
    String getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex) final;
    void uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding) final;

    void getActiveUniformBlockiv(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLSpan<GCGLint> params) final;

    // GL_ANGLE_multi_draw
    void multiDrawArraysANGLE(GCGLenum mode, GCGLSpan<const GCGLint> firsts, GCGLSpan<const GCGLsizei> counts, GCGLsizei drawcount) override;
    void multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpan<const GCGLint> firsts, GCGLSpan<const GCGLsizei> counts, GCGLSpan<const GCGLsizei> instanceCounts, GCGLsizei drawcount) override;
    void multiDrawElementsANGLE(GCGLenum mode, GCGLSpan<const GCGLsizei> counts, GCGLenum type, GCGLSpan<const GCGLint> offsets, GCGLsizei drawcount) override;
    void multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpan<const GCGLsizei> counts, GCGLenum type, GCGLSpan<const GCGLint> offsets, GCGLSpan<const GCGLsizei> instanceCounts, GCGLsizei drawcount) override;

    bool supportsExtension(const String&) final;
    void ensureExtensionEnabled(const String&) final;
    bool isExtensionEnabled(const String&) final;
    GLint getGraphicsResetStatusARB() final;
    void drawBuffersEXT(GCGLSpan<const GCGLenum>) override;
    String getTranslatedShaderSourceANGLE(PlatformGLObject) final;

    // Helper methods.
    void forceContextLost();
    void recycleContext();

    void dispatchContextChangedNotification();

    void paintRenderingResultsToCanvas(ImageBuffer&) final;
    std::optional<PixelBuffer> paintRenderingResultsToPixelBuffer() final;
    void paintCompositedResultsToCanvas(ImageBuffer&) final;

    std::optional<PixelBuffer> readRenderingResultsForPainting();
    std::optional<PixelBuffer> readCompositedResultsForPainting();

    void setContextVisibility(bool) final;

    // Support for buffer creation and deletion
    PlatformGLObject createBuffer() final;
    PlatformGLObject createFramebuffer() final;
    PlatformGLObject createProgram() final;
    PlatformGLObject createRenderbuffer() final;
    PlatformGLObject createShader(GCGLenum) final;
    PlatformGLObject createTexture() final;

    void deleteBuffer(PlatformGLObject) final;
    void deleteFramebuffer(PlatformGLObject) final;
    void deleteProgram(PlatformGLObject) final;
    void deleteRenderbuffer(PlatformGLObject) final;
    void deleteShader(PlatformGLObject) final;
    void deleteTexture(PlatformGLObject) final;

    void synthesizeGLError(GCGLenum error) final;
    bool moveErrorsToSyntheticErrorList() final;

    // Support for extensions. Returns a non-null object, though not
    // all methods it contains may necessarily be supported on the
    // current hardware. Must call ExtensionsGLOpenGL{ES}::supports() to
    // determine this.
#if USE(OPENGL_ES)
    ExtensionsGLOpenGLES& getExtensions();
#else
    ExtensionsGLOpenGL& getExtensions();
#endif
    void simulateEventForTesting(SimulatedEventForTesting) override;

    void prepareForDisplay() override;

protected:
    GraphicsContextGLOpenGL(GraphicsContextGLAttributes);

    GCGLuint m_texture { 0 };

    // Called once by all the public entry points that eventually call OpenGL.
    // Called once by all the public entry points of ExtensionsGLOpenGL/ExtensionGLOpenGLES that eventually call OpenGL.
    bool makeContextCurrent() WARN_UNUSED_RETURN;

    // Take into account the user's requested context creation attributes,
    // in particular stencil and antialias, and determine which could or
    // could not be honored based on the capabilities of the OpenGL
    // implementation.
    void validateDepthStencil(const char* packedDepthStencilExtension);
    void validateAttributes();

    void readnPixelsImpl(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLsizei* length, GCGLsizei* columns, GCGLsizei* rows, GCGLvoid* data, bool readingToPixelBufferObject);

    // Did the most recent drawing operation leave the GPU in an acceptable state?
    void checkGPUStatus();

    std::optional<PixelBuffer> readRenderingResults();
    std::optional<PixelBuffer> readCompositedResults();
    std::optional<PixelBuffer> readPixelsForPaintResults();

    bool reshapeFBOs(const IntSize&);
    void prepareTextureImpl();
    void resolveMultisamplingIfNecessary(const IntRect& = IntRect());
    void attachDepthAndStencilBufferIfNeeded(GCGLuint internalDepthStencilFormat, int width, int height);


    typedef HashMap<String, UniqueRef<sh::ShaderVariable>> ShaderSymbolMap;

    struct ShaderSourceEntry {
        GCGLenum type;
        String source;
        String translatedSource;
        String log;
        bool isValid;
        ShaderSymbolMap attributeMap;
        ShaderSymbolMap uniformMap;
        ShaderSymbolMap varyingMap;
        ShaderSourceEntry()
            : type(VERTEX_SHADER)
            , isValid(false)
        {
        }

        ShaderSymbolMap& symbolMap(enum ANGLEShaderSymbolType symbolType)
        {
            ASSERT(symbolType == SHADER_SYMBOL_TYPE_ATTRIBUTE || symbolType == SHADER_SYMBOL_TYPE_UNIFORM || symbolType == SHADER_SYMBOL_TYPE_VARYING);
            if (symbolType == SHADER_SYMBOL_TYPE_ATTRIBUTE)
                return attributeMap;
            if (symbolType == SHADER_SYMBOL_TYPE_VARYING)
                return varyingMap;
            return uniformMap;
        }
    };

    // FIXME: Shaders are never removed from this map, even if they and their program are deleted.
    // This is bad, and it also relies on the fact we never reuse PlatformGLObject numbers.
    typedef HashMap<PlatformGLObject, ShaderSourceEntry> ShaderSourceMap;
    ShaderSourceMap m_shaderSourceMap;

    typedef HashMap<PlatformGLObject, std::pair<PlatformGLObject, PlatformGLObject>> LinkedShaderMap;
    LinkedShaderMap m_linkedShaderMap;

    struct ActiveShaderSymbolCounts {
        Vector<GCGLint> filteredToActualAttributeIndexMap;
        Vector<GCGLint> filteredToActualUniformIndexMap;

        ActiveShaderSymbolCounts()
        {
        }

        GCGLint countForType(GCGLenum activeType)
        {
            ASSERT(activeType == ACTIVE_ATTRIBUTES || activeType == ACTIVE_UNIFORMS);
            if (activeType == ACTIVE_ATTRIBUTES)
                return filteredToActualAttributeIndexMap.size();

            return filteredToActualUniformIndexMap.size();
        }
    };
    typedef HashMap<PlatformGLObject, ActiveShaderSymbolCounts> ShaderProgramSymbolCountMap;
    ShaderProgramSymbolCountMap m_shaderProgramSymbolCountMap;

    typedef HashMap<String, String> HashedSymbolMap;
    HashedSymbolMap m_possiblyUnusedAttributeMap;

    String mappedSymbolName(PlatformGLObject program, ANGLEShaderSymbolType, const String& name);
    String mappedSymbolName(PlatformGLObject shaders[2], size_t count, const String& name);
    String originalSymbolName(PlatformGLObject program, ANGLEShaderSymbolType, const String& name);
    std::optional<String> mappedSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType, const String& name);
    std::optional<String> originalSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType, const String& name);

    std::unique_ptr<ShaderNameHash> nameHashMapForShaders;

    friend class ExtensionsGLOpenGLCommon;
#if USE(OPENGL_ES)
    friend class ExtensionsGLOpenGLES;
    std::unique_ptr<ExtensionsGLOpenGLES> m_extensions;
#else
    friend class ExtensionsGLOpenGL;
    std::unique_ptr<ExtensionsGLOpenGL> m_extensions;
#endif

    Vector<Vector<float>> m_vertexArray;

    ANGLEWebKitBridge m_compiler;

    GCGLuint m_fbo { 0 };
#if USE(COORDINATED_GRAPHICS)
    GCGLuint m_compositorTexture { 0 };
    GCGLuint m_intermediateTexture { 0 };
#endif

#if USE(OPENGL_ES)
    GCGLuint m_depthBuffer { 0 };
    GCGLuint m_stencilBuffer { 0 };
#endif
    GCGLuint m_depthStencilBuffer { 0 };

    GCGLuint m_internalColorFormat { 0 };


    GraphicsContextGLState m_state;

    // For multisampling
    GCGLuint m_multisampleFBO { 0 };
    GCGLuint m_multisampleDepthStencilBuffer { 0 };
    GCGLuint m_multisampleColorBuffer { 0 };

    // Errors raised by synthesizeGLError().
    ListHashSet<GCGLenum> m_syntheticErrors;

#if USE(NICOSIA)
    friend class Nicosia::GCGLLayer;
    std::unique_ptr<Nicosia::GCGLLayer> m_nicosiaLayer;
#elif USE(TEXTURE_MAPPER)
    friend class TextureMapperGCGLPlatformLayer;
    std::unique_ptr<TextureMapperGCGLPlatformLayer> m_texmapLayer;
#endif

    bool m_isForWebGL2 { false };
    bool m_usingCoreProfile { false };

    unsigned m_statusCheckCount { 0 };
    bool m_failNextStatusCheck { false };

#if USE(CAIRO)
    PlatformGLObject m_vao { 0 };
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && !USE(ANGLE)
