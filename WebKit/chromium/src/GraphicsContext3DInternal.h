/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsContext3DInternal_h
#define GraphicsContext3DInternal_h

#include "GraphicsContext3D.h"
#include <wtf/OwnPtr.h>
#if PLATFORM(SKIA)
#include "SkBitmap.h"
#endif

namespace WebKit {
class WebGraphicsContext3D;
class WebViewImpl;
} // namespace WebKit

namespace WebCore {

#if USE(ACCELERATED_COMPOSITING)
class WebGLLayerChromium;
#endif

class GraphicsContext3DInternal {
public:
    GraphicsContext3DInternal();
    ~GraphicsContext3DInternal();

    bool initialize(GraphicsContext3D::Attributes, HostWindow*);

    PlatformGraphicsContext3D platformGraphicsContext3D() const;
    Platform3DObject platformTexture() const;

    bool makeContextCurrent();

    int sizeInBytes(int type);

    void reshape(int width, int height);

    void paintRenderingResultsToCanvas(CanvasRenderingContext*);
    bool paintsIntoCanvasBuffer() const;

    void prepareTexture();

#if USE(ACCELERATED_COMPOSITING)
    WebGLLayerChromium* platformLayer() const;
#endif
    bool isGLES2Compliant() const;
    bool isGLES2NPOTStrict() const;
    bool isErrorGeneratedOnOutOfBoundsAccesses() const;

    //----------------------------------------------------------------------
    // Entry points for WebGL.
    //
    void activeTexture(unsigned long texture);
    void attachShader(Platform3DObject program, Platform3DObject shader);
    void bindAttribLocation(Platform3DObject, unsigned long index, const String& name);
    void bindBuffer(unsigned long target, Platform3DObject);
    void bindFramebuffer(unsigned long target, Platform3DObject);
    void bindRenderbuffer(unsigned long target, Platform3DObject);
    void bindTexture(unsigned long target, Platform3DObject texture);
    void blendColor(double red, double green, double blue, double alpha);
    void blendEquation(unsigned long mode);
    void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
    void blendFunc(unsigned long sfactor, unsigned long dfactor);
    void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);

    void bufferData(unsigned long target, int size, unsigned long usage);
    void bufferData(unsigned long target, int size, const void* data, unsigned long usage);
    void bufferSubData(unsigned long target, long offset, int size, const void* data);

    unsigned long checkFramebufferStatus(unsigned long target);
    void clear(unsigned long mask);
    void clearColor(double red, double green, double blue, double alpha);
    void clearDepth(double depth);
    void clearStencil(long s);
    void colorMask(bool red, bool green, bool blue, bool alpha);
    void compileShader(Platform3DObject);

    void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
    void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);
    void cullFace(unsigned long mode);
    void depthFunc(unsigned long func);
    void depthMask(bool flag);
    void depthRange(double zNear, double zFar);
    void detachShader(Platform3DObject, Platform3DObject);
    void disable(unsigned long cap);
    void disableVertexAttribArray(unsigned long index);
    void drawArrays(unsigned long mode, long first, long count);
    void drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset);

    void enable(unsigned long cap);
    void enableVertexAttribArray(unsigned long index);
    void finish();
    void flush();
    void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, Platform3DObject);
    void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, Platform3DObject, long level);
    void frontFace(unsigned long mode);
    void generateMipmap(unsigned long target);

    bool getActiveAttrib(Platform3DObject program, unsigned long index, ActiveInfo&);
    bool getActiveUniform(Platform3DObject program, unsigned long index, ActiveInfo&);

    void getAttachedShaders(Platform3DObject program, int maxCount, int* count, unsigned int* shaders);

    int getAttribLocation(Platform3DObject, const String& name);

    void getBooleanv(unsigned long pname, unsigned char* value);

    void getBufferParameteriv(unsigned long target, unsigned long pname, int* value);

    GraphicsContext3D::Attributes getContextAttributes();

    unsigned long getError();

    void getFloatv(unsigned long pname, float* value);

    void getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname, int* value);

    void getIntegerv(unsigned long pname, int* value);

    void getProgramiv(Platform3DObject program, unsigned long pname, int* value);

    String getProgramInfoLog(Platform3DObject);

    void getRenderbufferParameteriv(unsigned long target, unsigned long pname, int* value);

    void getShaderiv(Platform3DObject, unsigned long pname, int* value);

    String getShaderInfoLog(Platform3DObject);

    String getShaderSource(Platform3DObject);
    String getString(unsigned long name);

    void getTexParameterfv(unsigned long target, unsigned long pname, float* value);
    void getTexParameteriv(unsigned long target, unsigned long pname, int* value);

    void getUniformfv(Platform3DObject program, long location, float* value);
    void getUniformiv(Platform3DObject program, long location, int* value);

    long getUniformLocation(Platform3DObject, const String& name);

    void getVertexAttribfv(unsigned long index, unsigned long pname, float* value);
    void getVertexAttribiv(unsigned long index, unsigned long pname, int* value);

    long getVertexAttribOffset(unsigned long index, unsigned long pname);

    void hint(unsigned long target, unsigned long mode);
    bool isBuffer(Platform3DObject);
    bool isEnabled(unsigned long cap);
    bool isFramebuffer(Platform3DObject);
    bool isProgram(Platform3DObject);
    bool isRenderbuffer(Platform3DObject);
    bool isShader(Platform3DObject);
    bool isTexture(Platform3DObject);
    void lineWidth(double);
    void linkProgram(Platform3DObject);
    void pixelStorei(unsigned long pname, long param);
    void polygonOffset(double factor, double units);

    void readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* data);

    void releaseShaderCompiler();
    void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
    void sampleCoverage(double value, bool invert);
    void scissor(long x, long y, unsigned long width, unsigned long height);
    void shaderSource(Platform3DObject, const String& string);
    void stencilFunc(unsigned long func, long ref, unsigned long mask);
    void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
    void stencilMask(unsigned long mask);
    void stencilMaskSeparate(unsigned long face, unsigned long mask);
    void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
    void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);

    // These next several functions return an error code (0 if no errors) rather than using an ExceptionCode.
    // Currently they return -1 on any error.
    int texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels);

    void texParameterf(unsigned target, unsigned pname, float param);
    void texParameteri(unsigned target, unsigned pname, int param);

    int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels);

    void uniform1f(long location, float x);
    void uniform1fv(long location, float* v, int size);
    void uniform1i(long location, int x);
    void uniform1iv(long location, int* v, int size);
    void uniform2f(long location, float x, float y);
    void uniform2fv(long location, float* v, int size);
    void uniform2i(long location, int x, int y);
    void uniform2iv(long location, int* v, int size);
    void uniform3f(long location, float x, float y, float z);
    void uniform3fv(long location, float* v, int size);
    void uniform3i(long location, int x, int y, int z);
    void uniform3iv(long location, int* v, int size);
    void uniform4f(long location, float x, float y, float z, float w);
    void uniform4fv(long location, float* v, int size);
    void uniform4i(long location, int x, int y, int z, int w);
    void uniform4iv(long location, int* v, int size);
    void uniformMatrix2fv(long location, bool transpose, float* value, int size);
    void uniformMatrix3fv(long location, bool transpose, float* value, int size);
    void uniformMatrix4fv(long location, bool transpose, float* value, int size);

    void useProgram(Platform3DObject);
    void validateProgram(Platform3DObject);

    void vertexAttrib1f(unsigned long indx, float x);
    void vertexAttrib1fv(unsigned long indx, float* values);
    void vertexAttrib2f(unsigned long indx, float x, float y);
    void vertexAttrib2fv(unsigned long indx, float* values);
    void vertexAttrib3f(unsigned long indx, float x, float y, float z);
    void vertexAttrib3fv(unsigned long indx, float* values);
    void vertexAttrib4f(unsigned long indx, float x, float y, float z, float w);
    void vertexAttrib4fv(unsigned long indx, float* values);
    void vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                             unsigned long stride, unsigned long offset);

    void viewport(long x, long y, unsigned long width, unsigned long height);

    unsigned createBuffer();
    unsigned createFramebuffer();
    unsigned createProgram();
    unsigned createRenderbuffer();
    unsigned createShader(unsigned long);
    unsigned createTexture();

    void deleteBuffer(unsigned);
    void deleteFramebuffer(unsigned);
    void deleteProgram(unsigned);
    void deleteRenderbuffer(unsigned);
    void deleteShader(unsigned);
    void deleteTexture(unsigned);

    void synthesizeGLError(unsigned long error);

    void swapBuffers();

    // EXT_texture_format_BGRA8888
    bool supportsBGRA();

    // GL_CHROMIUM_map_sub
    bool supportsMapSubCHROMIUM();
    void* mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access);
    void unmapBufferSubDataCHROMIUM(const void*);
    void* mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access);
    void unmapTexSubImage2DCHROMIUM(const void*);

    // GL_CHROMIUM_copy_texture_to_parent_texture
    bool supportsCopyTextureToParentTextureCHROMIUM();
    void copyTextureToParentTextureCHROMIUM(unsigned texture, unsigned parentTexture);

private:
    OwnPtr<WebKit::WebGraphicsContext3D> m_impl;
    WebKit::WebViewImpl* m_webViewImpl;
#if USE(ACCELERATED_COMPOSITING)
    RefPtr<WebGLLayerChromium> m_compositingLayer;
#endif
#if PLATFORM(SKIA)
    // If the width and height of the Canvas's backing store don't
    // match those that we were given in the most recent call to
    // reshape(), then we need an intermediate bitmap to read back the
    // frame buffer into. This seems to happen when CSS styles are
    // used to resize the Canvas.
    SkBitmap m_resizingBitmap;
#endif

#if PLATFORM(CG)
    unsigned char* m_renderOutput;
#endif
};

} // namespace WebCore

#endif // GraphicsContext3D_h
