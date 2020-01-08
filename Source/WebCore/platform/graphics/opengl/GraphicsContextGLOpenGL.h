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

#if ENABLE(GRAPHICS_CONTEXT_GL)

#include "ANGLEWebKitBridge.h"
#include "GraphicsContextGL.h"
#include <memory>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/UniqueArray.h>

#if USE(CA)
#include "PlatformCALayer.h"
#endif

// FIXME: Find a better way to avoid the name confliction for NO_ERROR.
#if PLATFORM(WIN)
#undef NO_ERROR
#elif PLATFORM(GTK)
// This define is from the X11 headers, but it's used below, so we must undefine it.
#undef VERSION
#endif

#if PLATFORM(COCOA)
#if USE(OPENGL_ES)
#include <OpenGLES/ES2/gl.h>
#ifdef __OBJC__
#import <OpenGLES/EAGL.h>
#endif // __OBJC__
#endif // USE(OPENGL_ES)
OBJC_CLASS CALayer;
OBJC_CLASS WebGLLayer;
typedef struct __IOSurface* IOSurfaceRef;
#endif // PLATFORM(COCOA)

#if USE(NICOSIA)
namespace Nicosia {
class GC3DLayer;
}
#endif

namespace WebCore {
class ExtensionsGL;
#if USE(ANGLE)
class ExtensionsGLANGLE;
#elif !PLATFORM(COCOA) && USE(OPENGL_ES)
class ExtensionsGLOpenGLES;
#elif USE(OPENGL) || (PLATFORM(COCOA) && USE(OPENGL_ES))
class ExtensionsGLOpenGL;
#endif
class HostWindow;
class ImageBuffer;
class ImageData;
#if USE(TEXTURE_MAPPER)
class TextureMapperGC3DPlatformLayer;
#endif

typedef WTF::HashMap<CString, uint64_t> ShaderNameHash;

class GraphicsContextGLOpenGLPrivate;

class GraphicsContextGLOpenGL : public GraphicsContextGL {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void didComposite() = 0;
        virtual void forceContextLost() = 0;
        virtual void recycleContext() = 0;
        virtual void dispatchContextChangedNotification() = 0;
    };

    static RefPtr<GraphicsContextGLOpenGL> create(GraphicsContextGLAttributes, HostWindow*, Destination = Destination::Offscreen);
    virtual ~GraphicsContextGLOpenGL();

#if PLATFORM(COCOA)
    static Ref<GraphicsContextGLOpenGL> createShared(GraphicsContextGLOpenGL& sharedContext);
#endif

#if PLATFORM(COCOA)
    PlatformGraphicsContextGL platformGraphicsContextGL() const override { return m_contextObj; }
    PlatformGLObject platformTexture() const override { return m_texture; }
    CALayer* platformLayer() const override { return reinterpret_cast<CALayer*>(m_webGLLayer.get()); }
#if USE(ANGLE)
    PlatformGraphicsContextGLDisplay platformDisplay() const { return m_displayObj; }
    PlatformGraphicsContextGLConfig platformConfig() const { return m_configObj; }
#endif // USE(ANGLE)
#else
    PlatformGraphicsContextGL platformGraphicsContextGL() const override;
    PlatformGLObject platformTexture() const override;
    PlatformLayer* platformLayer() const override;
#endif

    bool makeContextCurrent();

    void addClient(Client& client) { m_clients.add(&client); }
    void removeClient(Client& client) { m_clients.remove(&client); }

    // With multisampling on, blit from multisampleFBO to regular FBO.
    void prepareTexture();

    // Equivalent to ::glTexImage2D(). Allows pixels==0 with no allocation.
    void texImage2DDirect(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels);

    // Get an attribute location without checking the name -> mangledname mapping.
    int getAttribLocationDirect(PlatformGLObject program, const String& name);

    // Compile a shader without going through ANGLE.
    void compileShaderDirect(PlatformGLObject);

    // Helper to texImage2D with pixel==0 case: pixels are initialized to 0.
    // Return true if no GL error is synthesized.
    // By default, alignment is 4, the OpenGL default setting.
    bool texImage2DResourceSafe(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLint alignment = 4);

    bool isGLES2Compliant() const;

    //----------------------------------------------------------------------
    // Helpers for texture uploading and pixel readback.
    //

    // Computes the components per pixel and bytes per component
    // for the given format and type combination. Returns false if
    // either was an invalid enum.
    static bool computeFormatAndTypeParameters(GCGLenum format,
        GCGLenum type,
        unsigned* componentsPerPixel,
        unsigned* bytesPerComponent);

    // Computes the image size in bytes. If paddingInBytes is not null, padding
    // is also calculated in return. Returns NO_ERROR if succeed, otherwise
    // return the suggested GL error indicating the cause of the failure:
    //   INVALID_VALUE if width/height is negative or overflow happens.
    //   INVALID_ENUM if format/type is illegal.
    static GCGLenum computeImageSizeInBytes(GCGLenum format,
        GCGLenum type,
        GCGLsizei width,
        GCGLsizei height,
        GCGLint alignment,
        unsigned* imageSizeInBytes,
        unsigned* paddingInBytes);

    static bool possibleFormatAndTypeForInternalFormat(GCGLenum internalFormat, GCGLenum& format, GCGLenum& type);

    // Extracts the contents of the given ImageData into the passed Vector,
    // packing the pixel data according to the given format and type,
    // and obeying the flipY and premultiplyAlpha flags. Returns true
    // upon success.
    static bool extractImageData(ImageData*,
        GCGLenum format,
        GCGLenum type,
        bool flipY,
        bool premultiplyAlpha,
        Vector<uint8_t>& data);

    // Helper function which extracts the user-supplied texture
    // data, applying the flipY and premultiplyAlpha parameters.
    // If the data is not tightly packed according to the passed
    // unpackAlignment, the output data will be tightly packed.
    // Returns true if successful, false if any error occurred.
    static bool extractTextureData(unsigned width, unsigned height,
        GCGLenum format, GCGLenum type,
        unsigned unpackAlignment,
        bool flipY, bool premultiplyAlpha,
        const void* pixels,
        Vector<uint8_t>& data);

    //----------------------------------------------------------------------
    // Entry points for WebGL.
    //

    void activeTexture(GCGLenum texture) override;
    void attachShader(PlatformGLObject program, PlatformGLObject shader) override;
    void bindAttribLocation(PlatformGLObject, GCGLuint index, const String& name) override;
    void bindBuffer(GCGLenum target, PlatformGLObject) override;
    void bindFramebuffer(GCGLenum target, PlatformGLObject) override;
    void bindRenderbuffer(GCGLenum target, PlatformGLObject) override;
    void bindTexture(GCGLenum target, PlatformGLObject) override;
    void blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) override;
    void blendEquation(GCGLenum mode) override;
    void blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha) override;
    void blendFunc(GCGLenum sfactor, GCGLenum dfactor) override;
    void blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha) override;

    void bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage) override;
    void bufferData(GCGLenum target, GCGLsizeiptr size, const void* data, GCGLenum usage) override;
    void bufferSubData(GCGLenum target, GCGLintptr offset, GCGLsizeiptr size, const void* data) override;

    void* mapBufferRange(GCGLenum target, GCGLintptr offset, GCGLsizeiptr length, GCGLbitfield access) override;
    GCGLboolean unmapBuffer(GCGLenum target) override;
    void copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr) override;

    void getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLsizei bufSize, GCGLint* params) override;
    void renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override;

    void texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override;
    void texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth) override;

    void getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname, Vector<GCGLint>& params) override;

    GCGLenum checkFramebufferStatus(GCGLenum target) override;
    void clear(GCGLbitfield mask) override;
    void clearColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) override;
    void clearDepth(GCGLclampf depth) override;
    void clearStencil(GCGLint s) override;
    void colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha) override;
    void compileShader(PlatformGLObject) override;

    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, const void* data) override;
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, const void* data) override;
    void copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border) override;
    void copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override;
    void cullFace(GCGLenum mode) override;
    void depthFunc(GCGLenum func) override;
    void depthMask(GCGLboolean flag) override;
    void depthRange(GCGLclampf zNear, GCGLclampf zFar) override;
    void detachShader(PlatformGLObject, PlatformGLObject) override;
    void disable(GCGLenum cap) override;
    void disableVertexAttribArray(GCGLuint index) override;
    void drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count) override;
    void drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset) override;

    void enable(GCGLenum cap) override;
    void enableVertexAttribArray(GCGLuint index) override;
    void finish() override;
    void flush() override;
    void framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject) override;
    void framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject, GCGLint level) override;
    void frontFace(GCGLenum mode) override;
    void generateMipmap(GCGLenum target) override;

    bool getActiveAttrib(PlatformGLObject program, GCGLuint index, ActiveInfo&) override;
    bool getActiveAttribImpl(PlatformGLObject program, GCGLuint index, ActiveInfo&);
    bool getActiveUniform(PlatformGLObject program, GCGLuint index, ActiveInfo&) override;
    bool getActiveUniformImpl(PlatformGLObject program, GCGLuint index, ActiveInfo&);
    void getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders) override;
    GCGLint getAttribLocation(PlatformGLObject, const String& name) override;
    void getBooleanv(GCGLenum pname, GCGLboolean* value) override;
    void getBufferParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value) override;
    GCGLenum getError() override;
    void getFloatv(GCGLenum pname, GCGLfloat* value) override;
    void getFramebufferAttachmentParameteriv(GCGLenum target, GCGLenum attachment, GCGLenum pname, GCGLint* value) override;
    void getIntegerv(GCGLenum pname, GCGLint* value) override;
    void getInteger64v(GCGLenum pname, GCGLint64* value) override;
    void getProgramiv(PlatformGLObject program, GCGLenum pname, GCGLint* value) override;
#if !USE(ANGLE)
    void getNonBuiltInActiveSymbolCount(PlatformGLObject program, GCGLenum pname, GCGLint* value);
#endif // !USE(ANGLE)
    String getProgramInfoLog(PlatformGLObject) override;
    String getUnmangledInfoLog(PlatformGLObject[2], GCGLsizei, const String&);
    void getRenderbufferParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value) override;
    void getShaderiv(PlatformGLObject, GCGLenum pname, GCGLint* value) override;
    String getShaderInfoLog(PlatformGLObject) override;
    void getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLint* range, GCGLint* precision) override;
    String getShaderSource(PlatformGLObject) override;
    String getString(GCGLenum name) override;
    void getTexParameterfv(GCGLenum target, GCGLenum pname, GCGLfloat* value) override;
    void getTexParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value) override;
    void getUniformfv(PlatformGLObject program, GCGLint location, GCGLfloat* value) override;
    void getUniformiv(PlatformGLObject program, GCGLint location, GCGLint* value) override;
    GCGLint getUniformLocation(PlatformGLObject, const String& name) override;
    void getVertexAttribfv(GCGLuint index, GCGLenum pname, GCGLfloat* value) override;
    void getVertexAttribiv(GCGLuint index, GCGLenum pname, GCGLint* value) override;
    GCGLsizeiptr getVertexAttribOffset(GCGLuint index, GCGLenum pname) override;

    void hint(GCGLenum target, GCGLenum mode) override;
    GCGLboolean isBuffer(PlatformGLObject) override;
    GCGLboolean isEnabled(GCGLenum cap) override;
    GCGLboolean isFramebuffer(PlatformGLObject) override;
    GCGLboolean isProgram(PlatformGLObject) override;
    GCGLboolean isRenderbuffer(PlatformGLObject) override;
    GCGLboolean isShader(PlatformGLObject) override;
    GCGLboolean isTexture(PlatformGLObject) override;
    void lineWidth(GCGLfloat) override;
    void linkProgram(PlatformGLObject) override;
    void pixelStorei(GCGLenum pname, GCGLint param) override;
    void polygonOffset(GCGLfloat factor, GCGLfloat units) override;

    void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, void* data) override;

    void releaseShaderCompiler();

    void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override;
    void sampleCoverage(GCGLclampf value, GCGLboolean invert) override;
    void scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override;
    void shaderSource(PlatformGLObject, const String& string) override;
    void stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask) override;
    void stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask) override;
    void stencilMask(GCGLuint mask) override;
    void stencilMaskSeparate(GCGLenum face, GCGLuint mask) override;
    void stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass) override;
    void stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass) override;

    bool texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels) override;
    void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param) override;
    void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param) override;
    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, const void* pixels) override;

    void uniform1f(GCGLint location, GCGLfloat x) override;
    void uniform1fv(GCGLint location, GCGLsizei, const GCGLfloat* v) override;
    void uniform1i(GCGLint location, GCGLint x) override;
    void uniform1iv(GCGLint location, GCGLsizei, const GCGLint* v) override;
    void uniform2f(GCGLint location, GCGLfloat x, GCGLfloat y) override;
    void uniform2fv(GCGLint location, GCGLsizei, const GCGLfloat* v) override;
    void uniform2i(GCGLint location, GCGLint x, GCGLint y) override;
    void uniform2iv(GCGLint location, GCGLsizei, const GCGLint* v) override;
    void uniform3f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z) override;
    void uniform3fv(GCGLint location, GCGLsizei, const GCGLfloat* v) override;
    void uniform3i(GCGLint location, GCGLint x, GCGLint y, GCGLint z) override;
    void uniform3iv(GCGLint location, GCGLsizei, const GCGLint* v) override;
    void uniform4f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) override;
    void uniform4fv(GCGLint location, GCGLsizei, const GCGLfloat* v) override;
    void uniform4i(GCGLint location, GCGLint x, GCGLint y, GCGLint z, GCGLint w) override;
    void uniform4iv(GCGLint location, GCGLsizei, const GCGLint* v) override;
    void uniformMatrix2fv(GCGLint location, GCGLsizei, GCGLboolean transpose, const GCGLfloat* value) override;
    void uniformMatrix3fv(GCGLint location, GCGLsizei, GCGLboolean transpose, const GCGLfloat* value) override;
    void uniformMatrix4fv(GCGLint location, GCGLsizei, GCGLboolean transpose, const GCGLfloat* value) override;

    void useProgram(PlatformGLObject) override;
    void validateProgram(PlatformGLObject) override;
#if !USE(ANGLE)
    bool checkVaryingsPacking(PlatformGLObject vertexShader, PlatformGLObject fragmentShader) const;
    bool precisionsMatch(PlatformGLObject vertexShader, PlatformGLObject fragmentShader) const;
#endif

    void vertexAttrib1f(GCGLuint index, GCGLfloat x) override;
    void vertexAttrib1fv(GCGLuint index, const GCGLfloat* values) override;
    void vertexAttrib2f(GCGLuint index, GCGLfloat x, GCGLfloat y) override;
    void vertexAttrib2fv(GCGLuint index, const GCGLfloat* values) override;
    void vertexAttrib3f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z) override;
    void vertexAttrib3fv(GCGLuint index, const GCGLfloat* values) override;
    void vertexAttrib4f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) override;
    void vertexAttrib4fv(GCGLuint index, const GCGLfloat* values) override;
    void vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset) override;

    void viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override;

    void reshape(int width, int height);

    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) override;
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount) override;
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor) override;

    // VertexArrayOject calls
    PlatformGLObject createVertexArray() override;
    void deleteVertexArray(PlatformGLObject) override;
    GCGLboolean isVertexArray(PlatformGLObject) override;
    void bindVertexArray(PlatformGLObject) override;

    void paintToCanvas(const unsigned char* imagePixels, const IntSize& imageSize, const IntSize& canvasSize, GraphicsContext&);

    void markContextChanged();
    void markLayerComposited();
    bool layerComposited() const;
    void forceContextLost();
    void recycleContext();

    void dispatchContextChangedNotification();
    void simulateContextChanged();

    void paintRenderingResultsToCanvas(ImageBuffer*);
    RefPtr<ImageData> paintRenderingResultsToImageData();
    bool paintCompositedResultsToCanvas(ImageBuffer*);

#if USE(OPENGL) && ENABLE(WEBGL2)
    void primitiveRestartIndex(GCGLuint) override;
#endif

#if PLATFORM(COCOA)
    bool texImageIOSurface2D(GCGLenum target, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, IOSurfaceRef, GCGLuint plane);

#if USE(OPENGL_ES)
    void presentRenderbuffer();
#endif

#if USE(OPENGL) || USE(ANGLE)
    void allocateIOSurfaceBackingStore(IntSize);
    void updateFramebufferTextureBackingStoreFromLayer();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void updateCGLContext();
#endif
#endif
#endif // PLATFORM(COCOA)

    void setContextVisibility(bool);

    GraphicsContextGLPowerPreference powerPreferenceUsedForCreation() const { return m_powerPreferenceUsedForCreation; }

    // Support for buffer creation and deletion
    PlatformGLObject createBuffer() override;
    PlatformGLObject createFramebuffer() override;
    PlatformGLObject createProgram() override;
    PlatformGLObject createRenderbuffer() override;
    PlatformGLObject createShader(GCGLenum) override;
    PlatformGLObject createTexture() override;

    void deleteBuffer(PlatformGLObject) override;
    void deleteFramebuffer(PlatformGLObject) override;
    void deleteProgram(PlatformGLObject) override;
    void deleteRenderbuffer(PlatformGLObject) override;
    void deleteShader(PlatformGLObject) override;
    void deleteTexture(PlatformGLObject) override;

    // Synthesizes an OpenGL error which will be returned from a
    // later call to getError. This is used to emulate OpenGL ES
    // 2.0 behavior on the desktop and to enforce additional error
    // checking mandated by WebGL.
    //
    // Per the behavior of glGetError, this stores at most one
    // instance of any given error, and returns them from calls to
    // getError in the order they were added.
    void synthesizeGLError(GCGLenum error);

    // Read real OpenGL errors, and move them to the synthetic
    // error list. Return true if at least one error is moved.
    bool moveErrorsToSyntheticErrorList();

    // Support for extensions. Returns a non-null object, though not
    // all methods it contains may necessarily be supported on the
    // current hardware. Must call ExtensionsGL::supports() to
    // determine this.
    ExtensionsGL& getExtensions() override;

    IntSize getInternalFramebufferSize() const;

    // Packs the contents of the given Image which is passed in |pixels| into the passed Vector
    // according to the given format and type, and obeying the flipY and AlphaOp flags.
    // Returns true upon success.
    static bool packImageData(Image*, const void* pixels, GCGLenum format, GCGLenum type, bool flipY, AlphaOp, DataFormat sourceFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, Vector<uint8_t>& data);

    class ImageExtractor {
    public:
        ImageExtractor(Image*, DOMSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile);

        // Each platform must provide an implementation of this method to deallocate or release resources
        // associated with the image if needed.
        ~ImageExtractor();

        bool extractSucceeded() { return m_extractSucceeded; }
        const void* imagePixelData() { return m_imagePixelData; }
        unsigned imageWidth() { return m_imageWidth; }
        unsigned imageHeight() { return m_imageHeight; }
        DataFormat imageSourceFormat() { return m_imageSourceFormat; }
        AlphaOp imageAlphaOp() { return m_alphaOp; }
        unsigned imageSourceUnpackAlignment() { return m_imageSourceUnpackAlignment; }
        DOMSource imageHtmlDomSource() { return m_imageHtmlDomSource; }
    private:
        // Each platform must provide an implementation of this method.
        // Extracts the image and keeps track of its status, such as width, height, Source Alignment, format and AlphaOp etc,
        // needs to lock the resources or relevant data if needed and returns true upon success
        bool extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile);

#if USE(CAIRO)
        RefPtr<cairo_surface_t> m_imageSurface;
#elif USE(CG)
        RetainPtr<CGImageRef> m_cgImage;
        RetainPtr<CGImageRef> m_decodedImage;
        RetainPtr<CFDataRef> m_pixelData;
        UniqueArray<uint8_t> m_formalizedRGBA8Data;
#endif
        Image* m_image;
        DOMSource m_imageHtmlDomSource;
        bool m_extractSucceeded;
        const void* m_imagePixelData;
        unsigned m_imageWidth;
        unsigned m_imageHeight;
        DataFormat m_imageSourceFormat;
        AlphaOp m_alphaOp;
        unsigned m_imageSourceUnpackAlignment;
    };

    void setFailNextGPUStatusCheck() { m_failNextStatusCheck = true; }

    GCGLenum activeTextureUnit() const { return m_state.activeTextureUnit; }
    GCGLenum currentBoundTexture() const { return m_state.currentBoundTexture(); }
    GCGLenum currentBoundTarget() const { return m_state.currentBoundTarget(); }
    unsigned textureSeed(GCGLuint texture) { return m_state.textureSeedCount.count(texture); }

#if PLATFORM(MAC)
    using PlatformDisplayID = uint32_t;
    void screenDidChange(PlatformDisplayID);
#endif

private:
    GraphicsContextGLOpenGL(GraphicsContextGLAttributes, HostWindow*, Destination = Destination::Offscreen, GraphicsContextGLOpenGL* sharedContext = nullptr);

    // Helper for packImageData/extractImageData/extractTextureData which implement packing of pixel
    // data into the specified OpenGL destination format and type.
    // A sourceUnpackAlignment of zero indicates that the source
    // data is tightly packed. Non-zero values may take a slow path.
    // Destination data will have no gaps between rows.
    static bool packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, unsigned destinationFormat, unsigned destinationType, AlphaOp, void* destinationData, bool flipY);

    // Take into account the user's requested context creation attributes,
    // in particular stencil and antialias, and determine which could or
    // could not be honored based on the capabilities of the OpenGL
    // implementation.
    void validateDepthStencil(const char* packedDepthStencilExtension);
    void validateAttributes();
    
    // Did the most recent drawing operation leave the GPU in an acceptable state?
    void checkGPUStatus();

    // Read rendering results into a pixel array with the same format as the
    // backbuffer.
    void readRenderingResults(unsigned char* pixels, int pixelsSize);
    void readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels);

#if USE(OPENGL_ES)
    void setRenderbufferStorageFromDrawable(GCGLsizei width, GCGLsizei height);
#endif

    bool reshapeFBOs(const IntSize&);
    void resolveMultisamplingIfNecessary(const IntRect& = IntRect());
    void attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height);

#if PLATFORM(COCOA)
    bool allowOfflineRenderers() const;
#endif

    int m_currentWidth { 0 };
    int m_currentHeight { 0 };

#if PLATFORM(COCOA)
    RetainPtr<WebGLLayer> m_webGLLayer;
    PlatformGraphicsContextGL m_contextObj { nullptr };
#if USE(ANGLE)
    PlatformGraphicsContextGLDisplay m_displayObj { nullptr };
    PlatformGraphicsContextGLConfig m_configObj { nullptr };
#endif // USE(ANGLE)
#endif // PLATFORM(COCOA)

#if PLATFORM(WIN) && USE(CA)
    RefPtr<PlatformCALayer> m_webGLLayer;
#endif

    typedef HashMap<String, sh::ShaderVariable> ShaderSymbolMap;

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

#if !USE(ANGLE)
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
    Optional<String> mappedSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType, const String& name);
    Optional<String> originalSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType, const String& name);

    std::unique_ptr<ShaderNameHash> nameHashMapForShaders;
#endif // !USE(ANGLE)

#if USE(ANGLE)
    friend class ExtensionsGLANGLE;
    std::unique_ptr<ExtensionsGLANGLE> m_extensions;
#elif !PLATFORM(COCOA) && USE(OPENGL_ES)
    friend class ExtensionsGLOpenGLES;
    friend class ExtensionsGLOpenGLCommon;
    std::unique_ptr<ExtensionsGLOpenGLES> m_extensions;
#elif USE(OPENGL) || (PLATFORM(COCOA) && USE(OPENGL_ES))
    friend class ExtensionsGLOpenGL;
    friend class ExtensionsGLOpenGLCommon;
    std::unique_ptr<ExtensionsGLOpenGL> m_extensions;
#endif

    GraphicsContextGLPowerPreference m_powerPreferenceUsedForCreation { GraphicsContextGLPowerPreference::Default };
    Vector<Vector<float>> m_vertexArray;

#if !USE(ANGLE)
    ANGLEWebKitBridge m_compiler;
#endif

    GCGLuint m_texture { 0 };
    GCGLuint m_fbo { 0 };
#if USE(COORDINATED_GRAPHICS)
    GCGLuint m_compositorTexture { 0 };
    GCGLuint m_intermediateTexture { 0 };
#endif

    GCGLuint m_depthBuffer { 0 };
    GCGLuint m_stencilBuffer { 0 };
    GCGLuint m_depthStencilBuffer { 0 };

    bool m_layerComposited { false };
    GCGLuint m_internalColorFormat { 0 };

#if USE(ANGLE) && PLATFORM(COCOA)
    PlatformGraphicsContextGLSurface m_pbuffer;
#endif

    struct GraphicsContextGLState {
        GCGLuint boundFBO { 0 };
        GCGLenum activeTextureUnit { GraphicsContextGL::TEXTURE0 };

        using BoundTextureMap = HashMap<GCGLenum,
            std::pair<GCGLuint, GCGLenum>,
            WTF::IntHash<GCGLenum>, 
            WTF::UnsignedWithZeroKeyHashTraits<GCGLuint>,
            WTF::PairHashTraits<WTF::UnsignedWithZeroKeyHashTraits<GCGLuint>, WTF::UnsignedWithZeroKeyHashTraits<GCGLuint>>
        >;
        BoundTextureMap boundTextureMap;
        GCGLuint currentBoundTexture() const { return boundTexture(activeTextureUnit); }
        GCGLuint boundTexture(GCGLenum textureUnit) const
        {
            auto iterator = boundTextureMap.find(textureUnit);
            if (iterator != boundTextureMap.end())
                return iterator->value.first;
            return 0;
        }

        GCGLuint currentBoundTarget() const { return boundTarget(activeTextureUnit); }
        GCGLenum boundTarget(GCGLenum textureUnit) const
        {
            auto iterator = boundTextureMap.find(textureUnit);
            if (iterator != boundTextureMap.end())
                return iterator->value.second;
            return 0;
        }

        void setBoundTexture(GCGLenum textureUnit, GCGLuint texture, GCGLenum target)
        {
            boundTextureMap.set(textureUnit, std::make_pair(texture, target));
        }

        using TextureSeedCount = HashCountedSet<GCGLuint, WTF::IntHash<GCGLuint>, WTF::UnsignedWithZeroKeyHashTraits<GCGLuint>>;
        TextureSeedCount textureSeedCount;
    };

    GraphicsContextGLState m_state;

    // For multisampling
    GCGLuint m_multisampleFBO { 0 };
    GCGLuint m_multisampleDepthStencilBuffer { 0 };
    GCGLuint m_multisampleColorBuffer { 0 };

    // Errors raised by synthesizeGLError().
    ListHashSet<GCGLenum> m_syntheticErrors;

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)
    friend class Nicosia::GC3DLayer;
    std::unique_ptr<Nicosia::GC3DLayer> m_nicosiaLayer;
#elif USE(TEXTURE_MAPPER)
    friend class TextureMapperGC3DPlatformLayer;
    std::unique_ptr<TextureMapperGC3DPlatformLayer> m_texmapLayer;
#elif !PLATFORM(COCOA)
    friend class GraphicsContextGLOpenGLPrivate;
    std::unique_ptr<GraphicsContextGLOpenGLPrivate> m_private;
#endif

    HashSet<Client*> m_clients;

    bool m_isForWebGL2 { false };
    bool m_usingCoreProfile { false };

    unsigned m_statusCheckCount { 0 };
    bool m_failNextStatusCheck { false };

#if USE(CAIRO)
    PlatformGLObject m_vao { 0 };
#endif

#if PLATFORM(COCOA) && (USE(OPENGL) || USE(ANGLE))
    bool m_hasSwitchedToHighPerformanceGPU { false };
#endif
};

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_GL)
