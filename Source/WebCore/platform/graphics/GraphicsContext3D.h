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

#include "ANGLEWebKitBridge.h"
#include "GraphicsContext3DBase.h"
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
class Extensions3D;
#if USE(ANGLE)
class Extensions3DANGLE;
#elif !PLATFORM(COCOA) && USE(OPENGL_ES)
class Extensions3DOpenGLES;
#elif USE(OPENGL) || (PLATFORM(COCOA) && USE(OPENGL_ES))
class Extensions3DOpenGL;
#endif
class HostWindow;
class ImageBuffer;
class ImageData;
#if USE(TEXTURE_MAPPER)
class TextureMapperGC3DPlatformLayer;
#endif

typedef WTF::HashMap<CString, uint64_t> ShaderNameHash;

class GraphicsContext3DPrivate;

class GraphicsContext3D : public GraphicsContext3DBase {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void didComposite() = 0;
        virtual void forceContextLost() = 0;
        virtual void recycleContext() = 0;
        virtual void dispatchContextChangedNotification() = 0;
    };

    static RefPtr<GraphicsContext3D> create(GraphicsContext3DAttributes, HostWindow*, Destination = Destination::Offscreen);
    virtual ~GraphicsContext3D();

#if PLATFORM(COCOA)
    static Ref<GraphicsContext3D> createShared(GraphicsContext3D& sharedContext);
#endif

#if PLATFORM(COCOA)
    PlatformGraphicsContext3D platformGraphicsContext3D() const override { return m_contextObj; }
    Platform3DObject platformTexture() const override { return m_texture; }
    CALayer* platformLayer() const override { return reinterpret_cast<CALayer*>(m_webGLLayer.get()); }
#if USE(ANGLE)
    PlatformGraphicsContext3DDisplay platformDisplay() const { return m_displayObj; }
    PlatformGraphicsContext3DConfig platformConfig() const { return m_configObj; }
#endif // USE(ANGLE)
#else
    PlatformGraphicsContext3D platformGraphicsContext3D() const override;
    Platform3DObject platformTexture() const override;
    PlatformLayer* platformLayer() const override;
#endif

    bool makeContextCurrent();

    void addClient(Client& client) { m_clients.add(&client); }
    void removeClient(Client& client) { m_clients.remove(&client); }

    // With multisampling on, blit from multisampleFBO to regular FBO.
    void prepareTexture();

    // Equivalent to ::glTexImage2D(). Allows pixels==0 with no allocation.
    void texImage2DDirect(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels);

    // Get an attribute location without checking the name -> mangledname mapping.
    int getAttribLocationDirect(Platform3DObject program, const String& name);

    // Compile a shader without going through ANGLE.
    void compileShaderDirect(Platform3DObject);

    // Helper to texImage2D with pixel==0 case: pixels are initialized to 0.
    // Return true if no GL error is synthesized.
    // By default, alignment is 4, the OpenGL default setting.
    bool texImage2DResourceSafe(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint alignment = 4);

    bool isGLES2Compliant() const;

    //----------------------------------------------------------------------
    // Helpers for texture uploading and pixel readback.
    //

    // Computes the components per pixel and bytes per component
    // for the given format and type combination. Returns false if
    // either was an invalid enum.
    static bool computeFormatAndTypeParameters(GC3Denum format,
                                               GC3Denum type,
                                               unsigned int* componentsPerPixel,
                                               unsigned int* bytesPerComponent);

    // Computes the image size in bytes. If paddingInBytes is not null, padding
    // is also calculated in return. Returns NO_ERROR if succeed, otherwise
    // return the suggested GL error indicating the cause of the failure:
    //   INVALID_VALUE if width/height is negative or overflow happens.
    //   INVALID_ENUM if format/type is illegal.
    static GC3Denum computeImageSizeInBytes(GC3Denum format,
                                     GC3Denum type,
                                     GC3Dsizei width,
                                     GC3Dsizei height,
                                     GC3Dint alignment,
                                     unsigned int* imageSizeInBytes,
                                     unsigned int* paddingInBytes);

    static bool possibleFormatAndTypeForInternalFormat(GC3Denum internalFormat, GC3Denum& format, GC3Denum& type);

    // Extracts the contents of the given ImageData into the passed Vector,
    // packing the pixel data according to the given format and type,
    // and obeying the flipY and premultiplyAlpha flags. Returns true
    // upon success.
    static bool extractImageData(ImageData*,
                          GC3Denum format,
                          GC3Denum type,
                          bool flipY,
                          bool premultiplyAlpha,
                          Vector<uint8_t>& data);

    // Helper function which extracts the user-supplied texture
    // data, applying the flipY and premultiplyAlpha parameters.
    // If the data is not tightly packed according to the passed
    // unpackAlignment, the output data will be tightly packed.
    // Returns true if successful, false if any error occurred.
    static bool extractTextureData(unsigned int width, unsigned int height,
                            GC3Denum format, GC3Denum type,
                            unsigned int unpackAlignment,
                            bool flipY, bool premultiplyAlpha,
                            const void* pixels,
                            Vector<uint8_t>& data);

    //----------------------------------------------------------------------
    // Entry points for WebGL.
    //

    void activeTexture(GC3Denum texture) override;
    void attachShader(Platform3DObject program, Platform3DObject shader) override;
    void bindAttribLocation(Platform3DObject, GC3Duint index, const String& name) override;
    void bindBuffer(GC3Denum target, Platform3DObject) override;
    void bindFramebuffer(GC3Denum target, Platform3DObject) override;
    void bindRenderbuffer(GC3Denum target, Platform3DObject) override;
    void bindTexture(GC3Denum target, Platform3DObject) override;
    void blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha) override;
    void blendEquation(GC3Denum mode) override;
    void blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha) override;
    void blendFunc(GC3Denum sfactor, GC3Denum dfactor) override;
    void blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha) override;

    void bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage) override;
    void bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage) override;
    void bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data) override;

    void* mapBufferRange(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr length, GC3Dbitfield access) override;
    GC3Dboolean unmapBuffer(GC3Denum target) override;
    void copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dintptr readOffset, GC3Dintptr writeOffset, GC3Dsizeiptr) override;

    void getInternalformativ(GC3Denum target, GC3Denum internalformat, GC3Denum pname, GC3Dsizei bufSize, GC3Dint* params) override;
    void renderbufferStorageMultisample(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) override;

    void texStorage2D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) override;
    void texStorage3D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth) override;

    void getActiveUniforms(Platform3DObject program, const Vector<GC3Duint>& uniformIndices, GC3Denum pname, Vector<GC3Dint>& params) override;

    GC3Denum checkFramebufferStatus(GC3Denum target) override;
    void clear(GC3Dbitfield mask) override;
    void clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha) override;
    void clearDepth(GC3Dclampf depth) override;
    void clearStencil(GC3Dint s) override;
    void colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha) override;
    void compileShader(Platform3DObject) override;

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data) override;
    void compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data) override;
    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border) override;
    void copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height) override;
    void cullFace(GC3Denum mode) override;
    void depthFunc(GC3Denum func) override;
    void depthMask(GC3Dboolean flag) override;
    void depthRange(GC3Dclampf zNear, GC3Dclampf zFar) override;
    void detachShader(Platform3DObject, Platform3DObject) override;
    void disable(GC3Denum cap) override;
    void disableVertexAttribArray(GC3Duint index) override;
    void drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count) override;
    void drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset) override;

    void enable(GC3Denum cap) override;
    void enableVertexAttribArray(GC3Duint index) override;
    void finish() override;
    void flush() override;
    void framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject) override;
    void framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject, GC3Dint level) override;
    void frontFace(GC3Denum mode) override;
    void generateMipmap(GC3Denum target) override;

    bool getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo&) override;
    bool getActiveAttribImpl(Platform3DObject program, GC3Duint index, ActiveInfo&);
    bool getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo&) override;
    bool getActiveUniformImpl(Platform3DObject program, GC3Duint index, ActiveInfo&);
    void getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders) override;
    GC3Dint getAttribLocation(Platform3DObject, const String& name) override;
    void getBooleanv(GC3Denum pname, GC3Dboolean* value) override;
    void getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value) override;
    GC3Denum getError() override;
    void getFloatv(GC3Denum pname, GC3Dfloat* value) override;
    void getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum pname, GC3Dint* value) override;
    void getIntegerv(GC3Denum pname, GC3Dint* value) override;
    void getInteger64v(GC3Denum pname, GC3Dint64* value) override;
    void getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value) override;
#if !USE(ANGLE)
    void getNonBuiltInActiveSymbolCount(Platform3DObject program, GC3Denum pname, GC3Dint* value);
#endif // !USE(ANGLE)
    String getProgramInfoLog(Platform3DObject) override;
    String getUnmangledInfoLog(Platform3DObject[2], GC3Dsizei, const String&);
    void getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value) override;
    void getShaderiv(Platform3DObject, GC3Denum pname, GC3Dint* value) override;
    String getShaderInfoLog(Platform3DObject) override;
    void getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision) override;
    String getShaderSource(Platform3DObject) override;
    String getString(GC3Denum name) override;
    void getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value) override;
    void getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value) override;
    void getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value) override;
    void getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value) override;
    GC3Dint getUniformLocation(Platform3DObject, const String& name) override;
    void getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value) override;
    void getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value) override;
    GC3Dsizeiptr getVertexAttribOffset(GC3Duint index, GC3Denum pname) override;

    void hint(GC3Denum target, GC3Denum mode) override;
    GC3Dboolean isBuffer(Platform3DObject) override;
    GC3Dboolean isEnabled(GC3Denum cap) override;
    GC3Dboolean isFramebuffer(Platform3DObject) override;
    GC3Dboolean isProgram(Platform3DObject) override;
    GC3Dboolean isRenderbuffer(Platform3DObject) override;
    GC3Dboolean isShader(Platform3DObject) override;
    GC3Dboolean isTexture(Platform3DObject) override;
    void lineWidth(GC3Dfloat) override;
    void linkProgram(Platform3DObject) override;
    void pixelStorei(GC3Denum pname, GC3Dint param) override;
    void polygonOffset(GC3Dfloat factor, GC3Dfloat units) override;

    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data) override;

    void releaseShaderCompiler();

    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) override;
    void sampleCoverage(GC3Dclampf value, GC3Dboolean invert) override;
    void scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height) override;
    void shaderSource(Platform3DObject, const String& string) override;
    void stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask) override;
    void stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask) override;
    void stencilMask(GC3Duint mask) override;
    void stencilMaskSeparate(GC3Denum face, GC3Duint mask) override;
    void stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass) override;
    void stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass) override;

    bool texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels) override;
    void texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param) override;
    void texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param) override;
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels) override;

    void uniform1f(GC3Dint location, GC3Dfloat x) override;
    void uniform1fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v) override;
    void uniform1i(GC3Dint location, GC3Dint x) override;
    void uniform1iv(GC3Dint location, GC3Dsizei, const GC3Dint* v) override;
    void uniform2f(GC3Dint location, GC3Dfloat x, GC3Dfloat y) override;
    void uniform2fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v) override;
    void uniform2i(GC3Dint location, GC3Dint x, GC3Dint y) override;
    void uniform2iv(GC3Dint location, GC3Dsizei, const GC3Dint* v) override;
    void uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z) override;
    void uniform3fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v) override;
    void uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z) override;
    void uniform3iv(GC3Dint location, GC3Dsizei, const GC3Dint* v) override;
    void uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w) override;
    void uniform4fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v) override;
    void uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w) override;
    void uniform4iv(GC3Dint location, GC3Dsizei, const GC3Dint* v) override;
    void uniformMatrix2fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, const GC3Dfloat* value) override;
    void uniformMatrix3fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, const GC3Dfloat* value) override;
    void uniformMatrix4fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, const GC3Dfloat* value) override;

    void useProgram(Platform3DObject) override;
    void validateProgram(Platform3DObject) override;
#if !USE(ANGLE)
    bool checkVaryingsPacking(Platform3DObject vertexShader, Platform3DObject fragmentShader) const;
    bool precisionsMatch(Platform3DObject vertexShader, Platform3DObject fragmentShader) const;
#endif

    void vertexAttrib1f(GC3Duint index, GC3Dfloat x) override;
    void vertexAttrib1fv(GC3Duint index, const GC3Dfloat* values) override;
    void vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y) override;
    void vertexAttrib2fv(GC3Duint index, const GC3Dfloat* values) override;
    void vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z) override;
    void vertexAttrib3fv(GC3Duint index, const GC3Dfloat* values) override;
    void vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w) override;
    void vertexAttrib4fv(GC3Duint index, const GC3Dfloat* values) override;
    void vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset) override;

    void viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height) override;

    void reshape(int width, int height);

    void drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount) override;
    void drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset, GC3Dsizei primcount) override;
    void vertexAttribDivisor(GC3Duint index, GC3Duint divisor) override;

    // VertexArrayOject calls
    Platform3DObject createVertexArray() override;
    void deleteVertexArray(Platform3DObject) override;
    GC3Dboolean isVertexArray(Platform3DObject) override;
    void bindVertexArray(Platform3DObject) override;

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
    void primitiveRestartIndex(GC3Duint) override;
#endif

#if PLATFORM(COCOA)
    bool texImageIOSurface2D(GC3Denum target, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, IOSurfaceRef, GC3Duint plane);

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

    GraphicsContext3DPowerPreference powerPreferenceUsedForCreation() const { return m_powerPreferenceUsedForCreation; }

    // Support for buffer creation and deletion
    Platform3DObject createBuffer() override;
    Platform3DObject createFramebuffer() override;
    Platform3DObject createProgram() override;
    Platform3DObject createRenderbuffer() override;
    Platform3DObject createShader(GC3Denum) override;
    Platform3DObject createTexture() override;

    void deleteBuffer(Platform3DObject) override;
    void deleteFramebuffer(Platform3DObject) override;
    void deleteProgram(Platform3DObject) override;
    void deleteRenderbuffer(Platform3DObject) override;
    void deleteShader(Platform3DObject) override;
    void deleteTexture(Platform3DObject) override;

    // Synthesizes an OpenGL error which will be returned from a
    // later call to getError. This is used to emulate OpenGL ES
    // 2.0 behavior on the desktop and to enforce additional error
    // checking mandated by WebGL.
    //
    // Per the behavior of glGetError, this stores at most one
    // instance of any given error, and returns them from calls to
    // getError in the order they were added.
    void synthesizeGLError(GC3Denum error);

    // Read real OpenGL errors, and move them to the synthetic
    // error list. Return true if at least one error is moved.
    bool moveErrorsToSyntheticErrorList();

    // Support for extensions. Returns a non-null object, though not
    // all methods it contains may necessarily be supported on the
    // current hardware. Must call Extensions3D::supports() to
    // determine this.
    Extensions3D& getExtensions() override;

    IntSize getInternalFramebufferSize() const;

    // Packs the contents of the given Image which is passed in |pixels| into the passed Vector
    // according to the given format and type, and obeying the flipY and AlphaOp flags.
    // Returns true upon success.
    static bool packImageData(Image*, const void* pixels, GC3Denum format, GC3Denum type, bool flipY, AlphaOp, DataFormat sourceFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, Vector<uint8_t>& data);

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

    GC3Denum activeTextureUnit() const { return m_state.activeTextureUnit; }
    GC3Denum currentBoundTexture() const { return m_state.currentBoundTexture(); }
    GC3Denum currentBoundTarget() const { return m_state.currentBoundTarget(); }
    unsigned textureSeed(GC3Duint texture) { return m_state.textureSeedCount.count(texture); }

#if PLATFORM(MAC)
    using PlatformDisplayID = uint32_t;
    void screenDidChange(PlatformDisplayID);
#endif

private:
    GraphicsContext3D(GraphicsContext3DAttributes, HostWindow*, Destination = Destination::Offscreen, GraphicsContext3D* sharedContext = nullptr);

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
    void setRenderbufferStorageFromDrawable(GC3Dsizei width, GC3Dsizei height);
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
    PlatformGraphicsContext3D m_contextObj { nullptr };
#if USE(ANGLE)
    PlatformGraphicsContext3DDisplay m_displayObj { nullptr };
    PlatformGraphicsContext3DConfig m_configObj { nullptr };
#endif // USE(ANGLE)
#endif // PLATFORM(COCOA)

#if PLATFORM(WIN) && USE(CA)
    RefPtr<PlatformCALayer> m_webGLLayer;
#endif

    typedef HashMap<String, sh::ShaderVariable> ShaderSymbolMap;

    struct ShaderSourceEntry {
        GC3Denum type;
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
    // This is bad, and it also relies on the fact we never reuse Platform3DObject numbers.
    typedef HashMap<Platform3DObject, ShaderSourceEntry> ShaderSourceMap;
    ShaderSourceMap m_shaderSourceMap;

    typedef HashMap<Platform3DObject, std::pair<Platform3DObject, Platform3DObject>> LinkedShaderMap;
    LinkedShaderMap m_linkedShaderMap;

    struct ActiveShaderSymbolCounts {
        Vector<GC3Dint> filteredToActualAttributeIndexMap;
        Vector<GC3Dint> filteredToActualUniformIndexMap;

        ActiveShaderSymbolCounts()
        {
        }

        GC3Dint countForType(GC3Denum activeType)
        {
            ASSERT(activeType == ACTIVE_ATTRIBUTES || activeType == ACTIVE_UNIFORMS);
            if (activeType == ACTIVE_ATTRIBUTES)
                return filteredToActualAttributeIndexMap.size();

            return filteredToActualUniformIndexMap.size();
        }
    };
    typedef HashMap<Platform3DObject, ActiveShaderSymbolCounts> ShaderProgramSymbolCountMap;
    ShaderProgramSymbolCountMap m_shaderProgramSymbolCountMap;

    typedef HashMap<String, String> HashedSymbolMap;
    HashedSymbolMap m_possiblyUnusedAttributeMap;

    String mappedSymbolName(Platform3DObject program, ANGLEShaderSymbolType, const String& name);
    String mappedSymbolName(Platform3DObject shaders[2], size_t count, const String& name);
    String originalSymbolName(Platform3DObject program, ANGLEShaderSymbolType, const String& name);
    Optional<String> mappedSymbolInShaderSourceMap(Platform3DObject shader, ANGLEShaderSymbolType, const String& name);
    Optional<String> originalSymbolInShaderSourceMap(Platform3DObject shader, ANGLEShaderSymbolType, const String& name);

    std::unique_ptr<ShaderNameHash> nameHashMapForShaders;
#endif // !USE(ANGLE)

#if USE(ANGLE)
    friend class Extensions3DANGLE;
    std::unique_ptr<Extensions3DANGLE> m_extensions;
#elif !PLATFORM(COCOA) && USE(OPENGL_ES)
    friend class Extensions3DOpenGLES;
    friend class Extensions3DOpenGLCommon;
    std::unique_ptr<Extensions3DOpenGLES> m_extensions;
#elif USE(OPENGL) || (PLATFORM(COCOA) && USE(OPENGL_ES))
    friend class Extensions3DOpenGL;
    friend class Extensions3DOpenGLCommon;
    std::unique_ptr<Extensions3DOpenGL> m_extensions;
#endif

    GraphicsContext3DPowerPreference m_powerPreferenceUsedForCreation { GraphicsContext3DPowerPreference::Default };
    Vector<Vector<float>> m_vertexArray;

#if !USE(ANGLE)
    ANGLEWebKitBridge m_compiler;
#endif

    GC3Duint m_texture { 0 };
    GC3Duint m_fbo { 0 };
#if USE(COORDINATED_GRAPHICS)
    GC3Duint m_compositorTexture { 0 };
    GC3Duint m_intermediateTexture { 0 };
#endif

    GC3Duint m_depthBuffer { 0 };
    GC3Duint m_stencilBuffer { 0 };
    GC3Duint m_depthStencilBuffer { 0 };

    bool m_layerComposited { false };
    GC3Duint m_internalColorFormat { 0 };

#if USE(ANGLE) && PLATFORM(COCOA)
    PlatformGraphicsContext3DSurface m_pbuffer;
#endif

    struct GraphicsContext3DState {
        GC3Duint boundFBO { 0 };
        GC3Denum activeTextureUnit { GraphicsContext3D::TEXTURE0 };

        using BoundTextureMap = HashMap<GC3Denum,
            std::pair<GC3Duint, GC3Denum>,
            WTF::IntHash<GC3Denum>, 
            WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>,
            WTF::PairHashTraits<WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>, WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>>
        >;
        BoundTextureMap boundTextureMap;
        GC3Duint currentBoundTexture() const { return boundTexture(activeTextureUnit); }
        GC3Duint boundTexture(GC3Denum textureUnit) const
        {
            auto iterator = boundTextureMap.find(textureUnit);
            if (iterator != boundTextureMap.end())
                return iterator->value.first;
            return 0;
        }

        GC3Duint currentBoundTarget() const { return boundTarget(activeTextureUnit); }
        GC3Denum boundTarget(GC3Denum textureUnit) const
        {
            auto iterator = boundTextureMap.find(textureUnit);
            if (iterator != boundTextureMap.end())
                return iterator->value.second;
            return 0;
        }

        void setBoundTexture(GC3Denum textureUnit, GC3Duint texture, GC3Denum target)
        {
            boundTextureMap.set(textureUnit, std::make_pair(texture, target));
        }

        using TextureSeedCount = HashCountedSet<GC3Duint, WTF::IntHash<GC3Duint>, WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>>;
        TextureSeedCount textureSeedCount;
    };

    GraphicsContext3DState m_state;

    // For multisampling
    GC3Duint m_multisampleFBO { 0 };
    GC3Duint m_multisampleDepthStencilBuffer { 0 };
    GC3Duint m_multisampleColorBuffer { 0 };

    // Errors raised by synthesizeGLError().
    ListHashSet<GC3Denum> m_syntheticErrors;

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)
    friend class Nicosia::GC3DLayer;
    std::unique_ptr<Nicosia::GC3DLayer> m_nicosiaLayer;
#elif USE(TEXTURE_MAPPER)
    friend class TextureMapperGC3DPlatformLayer;
    std::unique_ptr<TextureMapperGC3DPlatformLayer> m_texmapLayer;
#else
    friend class GraphicsContext3DPrivate;
    std::unique_ptr<GraphicsContext3DPrivate> m_private;
#endif

    HashSet<Client*> m_clients;

    bool m_isForWebGL2 { false };
    bool m_usingCoreProfile { false };

    unsigned m_statusCheckCount { 0 };
    bool m_failNextStatusCheck { false };

#if USE(CAIRO)
    Platform3DObject m_vao { 0 };
#endif

#if PLATFORM(COCOA) && (USE(OPENGL) || USE(ANGLE))
    bool m_hasSwitchedToHighPerformanceGPU { false };
#endif
};

} // namespace WebCore

#endif
