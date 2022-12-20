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

#if ENABLE(WEBGL)

#include "ANGLEUtilities.h"
#include "GraphicsContextGL.h"
#include "GraphicsContextGLState.h"
#include <memory>
#include <wtf/ListHashSet.h>

#if PLATFORM(COCOA)
#include "GraphicsContextGLIOSurfaceSwapChain.h"
#include "IOSurface.h"
#include "ProcessIdentity.h"
#endif

namespace WebCore {

// Base class for GraphicsContextGL contexts that use ANGLE.
class WEBCORE_EXPORT GraphicsContextGLANGLE : public GraphicsContextGL {
public:
    virtual ~GraphicsContextGLANGLE();

    GCGLDisplay platformDisplay() const;
    GCGLConfig platformConfig() const;
    GCGLenum drawingBufferTextureTarget();
    static GCGLenum drawingBufferTextureTargetQueryForDrawingTarget(GCGLenum drawingTarget);
    static GCGLint EGLDrawingBufferTextureTargetForDrawingTarget(GCGLenum drawingTarget);
    enum class ReleaseThreadResourceBehavior {
        // Releases current context after GraphicsContextGLANGLE calls done in the thread.
        ReleaseCurrentContext,
        // Releases all thread resources after GraphicsContextGLANGLE calls done in the thread.
        ReleaseThreadResources,
        // Releases all global state. Should be used only after all depending objects have
        // been released.
        TerminateAndReleaseThreadResources
    };
    static bool releaseThreadResources(ReleaseThreadResourceBehavior);

    // Get an attribute location without checking the name -> mangledname mapping.
    int getAttribLocationDirect(PlatformGLObject program, const String& name);

    // Compile a shader without going through ANGLE.
    void compileShaderDirect(PlatformGLObject);

    // Equivalent to ::glTexImage2D(). Allows pixels==0 with no allocation.
    void texImage2DDirect(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels);

    // GraphicsContextGL overrides.
    bool isGLES2Compliant() const final;
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
    void bufferData(GCGLenum target, GCGLsizeiptr, GCGLenum usage) final;
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
    bool getActiveAttrib(PlatformGLObject program, GCGLuint index, GraphicsContextGLActiveInfo&) final;
    bool getActiveAttribImpl(PlatformGLObject program, GCGLuint index, GraphicsContextGLActiveInfo&);
    bool getActiveUniform(PlatformGLObject program, GCGLuint index, GraphicsContextGLActiveInfo&) final;
    bool getActiveUniformImpl(PlatformGLObject program, GCGLuint index, GraphicsContextGLActiveInfo&);
    void getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders);
    GCGLint getAttribLocation(PlatformGLObject, const String& name) final;
    void getBooleanv(GCGLenum pname, GCGLSpan<GCGLboolean> value) final;
    GCGLint getBufferParameteri(GCGLenum target, GCGLenum pname) final;
    GCGLenum getError() final;
    void getFloatv(GCGLenum pname, GCGLSpan<GCGLfloat> value) final;
    GCGLint getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname) final;
    void getIntegerv(GCGLenum pname, GCGLSpan<GCGLint> value) final;
    void getIntegeri_v(GCGLenum pname, GCGLuint index, GCGLSpan<GCGLint, 4> value) final; // NOLINT
    GCGLint64 getInteger64(GCGLenum pname) final;
    GCGLint64 getInteger64i(GCGLenum pname, GCGLuint index) final;
    GCGLint getProgrami(PlatformGLObject program, GCGLenum pname) final;
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
    void shaderSource(PlatformGLObject, const String&) final;
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
    PlatformGLObject createVertexArray() final;
    void deleteVertexArray(PlatformGLObject) final;
    GCGLboolean isVertexArray(PlatformGLObject) final;
    void bindVertexArray(PlatformGLObject) final;
    void copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr) final;
    void getBufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<GCGLvoid> data) final;
    void blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter) final;
    void framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer) final;
    void invalidateFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments) final;
    void invalidateSubFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) final;
    void readBuffer(GCGLenum src) final;
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
    GCGLuint getQueryObjectui(PlatformGLObject query, GCGLenum pname) final;
    PlatformGLObject createSampler() final;
    void deleteSampler(PlatformGLObject sampler) final;
    GCGLboolean isSampler(PlatformGLObject sampler) final;
    void bindSampler(GCGLuint unit, PlatformGLObject sampler) final;
    void samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param) final;
    void samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param) final;
    GCGLfloat getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname) final;
    GCGLint getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname) final;
    GCGLsync fenceSync(GCGLenum condition, GCGLbitfield flags) final;
    GCGLboolean isSync(GCGLsync) final;
    void deleteSync(GCGLsync) final;
    GCGLenum clientWaitSync(GCGLsync, GCGLbitfield flags, GCGLuint64 timeout) final;
    void waitSync(GCGLsync, GCGLbitfield flags, GCGLint64 timeout) final;
    GCGLint getSynci(GCGLsync, GCGLenum pname) final;
    PlatformGLObject createTransformFeedback() final;
    void deleteTransformFeedback(PlatformGLObject id) final;
    GCGLboolean isTransformFeedback(PlatformGLObject id) final;
    void bindTransformFeedback(GCGLenum target, PlatformGLObject id) final;
    void beginTransformFeedback(GCGLenum primitiveMode) final;
    void endTransformFeedback() final;
    void transformFeedbackVaryings(PlatformGLObject program, const Vector<String>& varyings, GCGLenum bufferMode) final;
    void getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index, GraphicsContextGLActiveInfo&) final;
    void pauseTransformFeedback() final;
    void resumeTransformFeedback() final;
    void bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer) final;
    void bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr) final;
    Vector<GCGLuint> getUniformIndices(PlatformGLObject program, const Vector<String>& uniformNames) final;
    Vector<GCGLint> getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname) final;
    GCGLuint getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName) final;
    String getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex) final;
    void uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding) final;
    void getActiveUniformBlockiv(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLSpan<GCGLint> params) final;
    void multiDrawArraysANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei> firstsAndCounts) final;
    void multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei> firstsCountsAndInstanceCounts) final;
    void multiDrawElementsANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei> countsAndOffsets, GCGLenum type) final;
    void multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei> countsOffsetsAndInstanceCounts, GCGLenum type) final;
    bool supportsExtension(const String&) override;
    void ensureExtensionEnabled(const String&) override;
    bool isExtensionEnabled(const String&) override;
    void drawBuffersEXT(GCGLSpan<const GCGLenum>) override;
    String getTranslatedShaderSourceANGLE(PlatformGLObject) override;
    void enableiOES(GCGLenum target, GCGLuint index) final;
    void disableiOES(GCGLenum target, GCGLuint index) final;
    void blendEquationiOES(GCGLuint buf, GCGLenum mode) final;
    void blendEquationSeparateiOES(GCGLuint buf, GCGLenum modeRGB, GCGLenum modeAlpha) final;
    void blendFunciOES(GCGLuint buf, GCGLenum src, GCGLenum dst) final;
    void blendFuncSeparateiOES(GCGLuint buf, GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha) final;
    void colorMaskiOES(GCGLuint buf, GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha) final;
    void drawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount, GCGLuint baseInstance) final;
    void drawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei instanceCount, GCGLint baseVertex, GCGLuint baseInstance) final;
    void multiDrawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei, const GCGLuint> firstsCountsInstanceCountsAndBaseInstances) final;
    void multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei, const GCGLint, const GCGLuint> countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, GCGLenum type) final;
    void provokingVertexANGLE(GCGLenum provokeMode) final;

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
    void simulateEventForTesting(SimulatedEventForTesting) override;
    void paintRenderingResultsToCanvas(ImageBuffer&) final;
    RefPtr<PixelBuffer> paintRenderingResultsToPixelBuffer() final;
    void paintCompositedResultsToCanvas(ImageBuffer&) final;

    RefPtr<PixelBuffer> readRenderingResultsForPainting();
    RefPtr<PixelBuffer> readCompositedResultsForPainting();
    // Returns true on success.
    bool readnPixelsWithStatus(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data);
protected:
    GraphicsContextGLANGLE(GraphicsContextGLAttributes);

    // Called once by all the public entry points that eventually call OpenGL.
    bool makeContextCurrent() WARN_UNUSED_RETURN;

    // Initializes the instance. Returns false if the instance should not be used.
    bool initialize();
    // Called first by initialize(). Subclasses should override to instantiate the platform specific bits of EGLContext.
    // FIXME: Currently platforms do not share the context creation. They should.
    virtual bool platformInitializeContext();
    // Called last by initialize(). Subclasses should override to instantiate platform specific state that depend on
    // the shared state.
    virtual bool platformInitialize();

    // Take into account the user's requested context creation attributes,
    // in particular stencil and antialias, and determine which could or
    // could not be honored based on the capabilities of the OpenGL
    // implementation.
    void validateDepthStencil(ASCIILiteral packedDepthStencilExtension);
    void validateAttributes();

    bool readnPixelsImpl(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLsizei* length, GCGLsizei* columns, GCGLsizei* rows, GCGLvoid* data, bool readingToPixelBufferObject);

    // Did the most recent drawing operation leave the GPU in an acceptable state?
    void checkGPUStatus();

    RefPtr<PixelBuffer> readRenderingResults();
    RefPtr<PixelBuffer> readCompositedResults();
    RefPtr<PixelBuffer> readPixelsForPaintResults();

    bool reshapeFBOs(const IntSize&);
    virtual void prepareTexture();
    void resolveMultisamplingIfNecessary(const IntRect& = IntRect());
    void attachDepthAndStencilBufferIfNeeded(GCGLuint internalDepthStencilFormat, int width, int height);
#if PLATFORM(COCOA)
    static bool makeCurrent(GCGLDisplay, GCGLContext);
#endif
    virtual bool reshapeDisplayBufferBacking() = 0;

    // Returns false if context should be lost due to timeout.
    bool waitAndUpdateOldestFrame() WARN_UNUSED_RETURN;

    // Platform specific behavior for releaseResources();
    static void platformReleaseThreadResources();

    virtual void invalidateKnownTextureContent(GCGLuint);

    // Only for non-WebGL 2.0 contexts.
    GCGLenum adjustWebGL1TextureInternalFormat(GCGLenum internalformat, GCGLenum format, GCGLenum type);

    HashSet<String> m_availableExtensions;
    HashSet<String> m_requestableExtensions;
    HashSet<String> m_enabledExtensions;
    bool m_webglColorBufferFloatRGB { false };
    bool m_webglColorBufferFloatRGBA { false };
    GCGLuint m_texture { 0 };
    GCGLuint m_fbo { 0 };
    GCGLuint m_depthStencilBuffer { 0 };
    GCGLuint m_internalColorFormat { 0 };
    GCGLuint m_internalDepthStencilFormat { 0 };
    GCGLuint m_multisampleFBO { 0 };
    GCGLuint m_multisampleDepthStencilBuffer { 0 };
    GCGLuint m_multisampleColorBuffer { 0 };
    // For preserveDrawingBuffer:true without multisampling.
    GCGLuint m_preserveDrawingBufferTexture { 0 };
    // Attaches m_texture when m_preserveDrawingBufferTexture is non-zero.
    GCGLuint m_preserveDrawingBufferFBO { 0 };
    // Queried at display startup.
    GCGLint m_drawingBufferTextureTarget { -1 };
    // Errors raised by synthesizeGLError().
    ListHashSet<GCGLenum> m_syntheticErrors;
    bool m_isForWebGL2 { false };
    unsigned m_statusCheckCount { 0 };
    bool m_failNextStatusCheck { false };
    bool m_useFenceSyncForDisplayRateLimit = false;
    static constexpr size_t maxPendingFrames = 3;
    size_t m_oldestFrameCompletionFence { 0 };
    ScopedGLFence m_frameCompletionFences[maxPendingFrames];
    GraphicsContextGLState m_state;

    GCGLDisplay m_displayObj { nullptr };
    GCGLContext m_contextObj { nullptr };
    GCGLConfig m_configObj { nullptr };

#if PLATFORM(COCOA)
    // FIXME: Move these to GraphicsContextGLCocoa.
    GraphicsContextGLIOSurfaceSwapChain m_swapChain;
    // Backing store for the the buffer which is eventually used for display.
    // When preserveDrawingBuffer == false, this is the drawing buffer backing store.
    // When preserveDrawingBuffer == true, this is blitted to during display prepare.
    std::unique_ptr<IOSurface> m_displayBufferBacking;
    void* m_displayBufferPbuffer { nullptr };
#endif
};

}

#endif
