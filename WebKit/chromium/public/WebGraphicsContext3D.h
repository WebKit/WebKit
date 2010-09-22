/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGraphicsContext3D_h
#define WebGraphicsContext3D_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebString.h"

namespace WebKit {

// Typedef for server-side objects like OpenGL textures and program objects.
typedef unsigned int WebGLId;

class WebView;

// This interface abstracts the operations performed by the
// GraphicsContext3D in order to implement WebGL. Nearly all of the
// methods exposed on this interface map directly to entry points in
// the OpenGL ES 2.0 API.

class WebGraphicsContext3D : public WebNonCopyable {
public:
    // Return value from getActiveUniform and getActiveAttrib.
    struct ActiveInfo {
        WebString name;
        unsigned type;
        int size;
    };

    // Context creation attributes.
    struct Attributes {
        Attributes()
            : alpha(true)
            , depth(true)
            , stencil(true)
            , antialias(true)
            , premultipliedAlpha(true)
        {
        }

        bool alpha;
        bool depth;
        bool stencil;
        bool antialias;
        bool premultipliedAlpha;
    };

    // This destructor needs to be public so that using classes can destroy instances if initialization fails.
    virtual ~WebGraphicsContext3D() {}

    // Creates a "default" implementation of WebGraphicsContext3D which calls
    // OpenGL directly.
    WEBKIT_API static WebGraphicsContext3D* createDefault();

    // Initializes the graphics context; should be the first operation performed
    // on newly-constructed instances. Returns true on success.
    virtual bool initialize(Attributes, WebView*, bool renderDirectlyToWebView) = 0;

    // Makes the OpenGL context current on the current thread. Returns true on
    // success.
    virtual bool makeContextCurrent() = 0;

    // The size of the region into which this WebGraphicsContext3D is rendering.
    // Returns the last values passed to reshape().
    virtual int width() = 0;
    virtual int height() = 0;

    // Helper to return the size in bytes of OpenGL data types
    // like GL_FLOAT, GL_INT, etc.
    virtual int sizeInBytes(int type) = 0;

    // Resizes the region into which this WebGraphicsContext3D is drawing.
    virtual void reshape(int width, int height) = 0;

    // Query whether it is built on top of compliant GLES2 implementation.
    virtual bool isGLES2Compliant() = 0;
    // Query whether it is built on top of GLES2 NPOT strict implementation.
    virtual bool isGLES2NPOTStrict() = 0;
    // Query whether it is built on top of implementation that generates errors
    // on out-of-bounds buffer accesses.
    virtual bool isErrorGeneratedOnOutOfBoundsAccesses() = 0;

    // Helper for software compositing path. Reads back the frame buffer into
    // the memory region pointed to by "pixels" with size "bufferSize". It is
    // expected that the storage for "pixels" covers (4 * width * height) bytes.
    // Returns true on success.
    virtual bool readBackFramebuffer(unsigned char* pixels, size_t bufferSize) = 0;

    // Returns the id of the texture which is used for storing the contents of
    // the framebuffer associated with this context. This texture is accessible
    // by the gpu-based page compositor.
    virtual unsigned getPlatformTextureId() = 0;

    // Copies the contents of the off-screen render target used by the WebGL
    // context to the corresponding texture used by the compositor.
    virtual void prepareTexture() = 0;

    // Synthesizes an OpenGL error which will be returned from a
    // later call to getError. This is used to emulate OpenGL ES
    // 2.0 behavior on the desktop and to enforce additional error
    // checking mandated by WebGL.
    //
    // Per the behavior of glGetError, this stores at most one
    // instance of any given error, and returns them from calls to
    // getError in the order they were added.
    virtual void synthesizeGLError(unsigned long error) = 0;

    // EXT_texture_format_BGRA8888
    virtual bool supportsBGRA() = 0;

    // GL_CHROMIUM_map_sub
    virtual bool supportsMapSubCHROMIUM() = 0;
    virtual void* mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access) = 0;
    virtual void unmapBufferSubDataCHROMIUM(const void*) = 0;
    virtual void* mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access) = 0;
    virtual void unmapTexSubImage2DCHROMIUM(const void*) = 0;

    // GL_CHROMIUM_copy_texture_to_parent_texture
    virtual bool supportsCopyTextureToParentTextureCHROMIUM() = 0;
    virtual void copyTextureToParentTextureCHROMIUM(unsigned texture, unsigned parentTexture) = 0;

    // The entry points below map directly to the OpenGL ES 2.0 API.
    // See: http://www.khronos.org/registry/gles/
    // and: http://www.khronos.org/opengles/sdk/docs/man/
    virtual void activeTexture(unsigned long texture) = 0;
    virtual void attachShader(WebGLId program, WebGLId shader) = 0;
    virtual void bindAttribLocation(WebGLId program, unsigned long index, const char* name) = 0;
    virtual void bindBuffer(unsigned long target, WebGLId buffer) = 0;
    virtual void bindFramebuffer(unsigned long target, WebGLId framebuffer) = 0;
    virtual void bindRenderbuffer(unsigned long target, WebGLId renderbuffer) = 0;
    virtual void bindTexture(unsigned long target, WebGLId texture) = 0;
    virtual void blendColor(double red, double green, double blue, double alpha) = 0;
    virtual void blendEquation(unsigned long mode) = 0;
    virtual void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha) = 0;
    virtual void blendFunc(unsigned long sfactor, unsigned long dfactor) = 0;
    virtual void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha) = 0;

    virtual void bufferData(unsigned long target, int size, const void* data, unsigned long usage) = 0;
    virtual void bufferSubData(unsigned long target, long offset, int size, const void* data) = 0;

    virtual unsigned long checkFramebufferStatus(unsigned long target) = 0;
    virtual void clear(unsigned long mask) = 0;
    virtual void clearColor(double red, double green, double blue, double alpha) = 0;
    virtual void clearDepth(double depth) = 0;
    virtual void clearStencil(long s) = 0;
    virtual void colorMask(bool red, bool green, bool blue, bool alpha) = 0;
    virtual void compileShader(WebGLId shader) = 0;

    virtual void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border) = 0;
    virtual void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height) = 0;
    virtual void cullFace(unsigned long mode) = 0;
    virtual void depthFunc(unsigned long func) = 0;
    virtual void depthMask(bool flag) = 0;
    virtual void depthRange(double zNear, double zFar) = 0;
    virtual void detachShader(WebGLId program, WebGLId shader) = 0;
    virtual void disable(unsigned long cap) = 0;
    virtual void disableVertexAttribArray(unsigned long index) = 0;
    virtual void drawArrays(unsigned long mode, long first, long count) = 0;
    virtual void drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset) = 0;

    virtual void enable(unsigned long cap) = 0;
    virtual void enableVertexAttribArray(unsigned long index) = 0;
    virtual void finish() = 0;
    virtual void flush() = 0;
    virtual void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLId renderbuffer) = 0;
    virtual void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLId texture, long level) = 0;
    virtual void frontFace(unsigned long mode) = 0;
    virtual void generateMipmap(unsigned long target) = 0;

    virtual bool getActiveAttrib(WebGLId program, unsigned long index, ActiveInfo&) = 0;
    virtual bool getActiveUniform(WebGLId program, unsigned long index, ActiveInfo&) = 0;

    virtual void getAttachedShaders(WebGLId program, int maxCount, int* count, unsigned int* shaders) = 0;

    virtual int  getAttribLocation(WebGLId program, const char* name) = 0;

    virtual void getBooleanv(unsigned long pname, unsigned char* value) = 0;

    virtual void getBufferParameteriv(unsigned long target, unsigned long pname, int* value) = 0;

    virtual Attributes getContextAttributes() = 0;

    virtual unsigned long getError() = 0;

    virtual void getFloatv(unsigned long pname, float* value) = 0;

    virtual void getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname, int* value) = 0;

    virtual void getIntegerv(unsigned long pname, int* value) = 0;

    virtual void getProgramiv(WebGLId program, unsigned long pname, int* value) = 0;

    virtual WebString getProgramInfoLog(WebGLId program) = 0;

    virtual void getRenderbufferParameteriv(unsigned long target, unsigned long pname, int* value) = 0;

    virtual void getShaderiv(WebGLId shader, unsigned long pname, int* value) = 0;

    virtual WebString getShaderInfoLog(WebGLId shader) = 0;

    // TBD
    // void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

    virtual WebString getShaderSource(WebGLId shader) = 0;
    virtual WebString getString(unsigned long name) = 0;

    virtual void getTexParameterfv(unsigned long target, unsigned long pname, float* value) = 0;
    virtual void getTexParameteriv(unsigned long target, unsigned long pname, int* value) = 0;

    virtual void getUniformfv(WebGLId program, long location, float* value) = 0;
    virtual void getUniformiv(WebGLId program, long location, int* value) = 0;

    virtual long getUniformLocation(WebGLId program, const char* name) = 0;

    virtual void getVertexAttribfv(unsigned long index, unsigned long pname, float* value) = 0;
    virtual void getVertexAttribiv(unsigned long index, unsigned long pname, int* value) = 0;

    virtual long getVertexAttribOffset(unsigned long index, unsigned long pname) = 0;

    virtual void hint(unsigned long target, unsigned long mode) = 0;
    virtual bool isBuffer(WebGLId buffer) = 0;
    virtual bool isEnabled(unsigned long cap) = 0;
    virtual bool isFramebuffer(WebGLId framebuffer) = 0;
    virtual bool isProgram(WebGLId program) = 0;
    virtual bool isRenderbuffer(WebGLId renderbuffer) = 0;
    virtual bool isShader(WebGLId shader) = 0;
    virtual bool isTexture(WebGLId texture) = 0;
    virtual void lineWidth(double) = 0;
    virtual void linkProgram(WebGLId program) = 0;
    virtual void pixelStorei(unsigned long pname, long param) = 0;
    virtual void polygonOffset(double factor, double units) = 0;

    virtual void readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels) = 0;

    virtual void releaseShaderCompiler() = 0;
    virtual void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height) = 0;
    virtual void sampleCoverage(double value, bool invert) = 0;
    virtual void scissor(long x, long y, unsigned long width, unsigned long height) = 0;
    virtual void shaderSource(WebGLId shader, const char* string) = 0;
    virtual void stencilFunc(unsigned long func, long ref, unsigned long mask) = 0;
    virtual void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask) = 0;
    virtual void stencilMask(unsigned long mask) = 0;
    virtual void stencilMaskSeparate(unsigned long face, unsigned long mask) = 0;
    virtual void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass) = 0;
    virtual void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass) = 0;

    virtual void texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, const void* pixels) = 0;

    virtual void texParameterf(unsigned target, unsigned pname, float param) = 0;
    virtual void texParameteri(unsigned target, unsigned pname, int param) = 0;

    virtual void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, const void* pixels) = 0;

    virtual void uniform1f(long location, float x) = 0;
    virtual void uniform1fv(long location, int count, float* v) = 0;
    virtual void uniform1i(long location, int x) = 0;
    virtual void uniform1iv(long location, int count, int* v) = 0;
    virtual void uniform2f(long location, float x, float y) = 0;
    virtual void uniform2fv(long location, int count, float* v) = 0;
    virtual void uniform2i(long location, int x, int y) = 0;
    virtual void uniform2iv(long location, int count, int* v) = 0;
    virtual void uniform3f(long location, float x, float y, float z) = 0;
    virtual void uniform3fv(long location, int count, float* v) = 0;
    virtual void uniform3i(long location, int x, int y, int z) = 0;
    virtual void uniform3iv(long location, int count, int* v) = 0;
    virtual void uniform4f(long location, float x, float y, float z, float w) = 0;
    virtual void uniform4fv(long location, int count, float* v) = 0;
    virtual void uniform4i(long location, int x, int y, int z, int w) = 0;
    virtual void uniform4iv(long location, int count, int* v) = 0;
    virtual void uniformMatrix2fv(long location, int count, bool transpose, const float* value) = 0;
    virtual void uniformMatrix3fv(long location, int count, bool transpose, const float* value) = 0;
    virtual void uniformMatrix4fv(long location, int count, bool transpose, const float* value) = 0;

    virtual void useProgram(WebGLId program) = 0;
    virtual void validateProgram(WebGLId program) = 0;

    virtual void vertexAttrib1f(unsigned long indx, float x) = 0;
    virtual void vertexAttrib1fv(unsigned long indx, const float* values) = 0;
    virtual void vertexAttrib2f(unsigned long indx, float x, float y) = 0;
    virtual void vertexAttrib2fv(unsigned long indx, const float* values) = 0;
    virtual void vertexAttrib3f(unsigned long indx, float x, float y, float z) = 0;
    virtual void vertexAttrib3fv(unsigned long indx, const float* values) = 0;
    virtual void vertexAttrib4f(unsigned long indx, float x, float y, float z, float w) = 0;
    virtual void vertexAttrib4fv(unsigned long indx, const float* values) = 0;
    virtual void vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                     unsigned long stride, unsigned long offset) = 0;

    virtual void viewport(long x, long y, unsigned long width, unsigned long height) = 0;

    // Support for buffer creation and deletion.
    virtual unsigned createBuffer() = 0;
    virtual unsigned createFramebuffer() = 0;
    virtual unsigned createProgram() = 0;
    virtual unsigned createRenderbuffer() = 0;
    virtual unsigned createShader(unsigned long) = 0;
    virtual unsigned createTexture() = 0;

    virtual void deleteBuffer(unsigned) = 0;
    virtual void deleteFramebuffer(unsigned) = 0;
    virtual void deleteProgram(unsigned) = 0;
    virtual void deleteRenderbuffer(unsigned) = 0;
    virtual void deleteShader(unsigned) = 0;
    virtual void deleteTexture(unsigned) = 0;
};

} // namespace WebKit

#endif
