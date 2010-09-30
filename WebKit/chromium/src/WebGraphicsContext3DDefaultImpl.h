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

#ifndef WebGraphicsContext3DDefaultImpl_h
#define WebGraphicsContext3DDefaultImpl_h

#if ENABLE(3D_CANVAS)

#include "GLSLANG/ShaderLang.h"
#include "WebGraphicsContext3D.h"

#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/OwnPtr.h>

#if !PLATFORM(CG)
#define FLIP_FRAMEBUFFER_VERTICALLY
#endif
namespace gfx {
class GLContext;
}

namespace WebKit {

// The default implementation of WebGL. In Chromium, using this class
// requires the sandbox to be disabled, which is strongly discouraged.
// It is provided for support of test_shell and any Chromium ports
// where an in-renderer WebGL implementation would be helpful.

class WebGraphicsContext3DDefaultImpl : public WebGraphicsContext3D {
public:
    WebGraphicsContext3DDefaultImpl();
    virtual ~WebGraphicsContext3DDefaultImpl();

    //----------------------------------------------------------------------
    // WebGraphicsContext3D methods
    virtual bool initialize(WebGraphicsContext3D::Attributes attributes, WebView*, bool);
    virtual bool makeContextCurrent();

    virtual int width();
    virtual int height();

    virtual int sizeInBytes(int type);

    virtual bool isGLES2Compliant();
    virtual bool isGLES2NPOTStrict();
    virtual bool isErrorGeneratedOnOutOfBoundsAccesses();

    virtual void reshape(int width, int height);

    virtual bool readBackFramebuffer(unsigned char* pixels, size_t bufferSize);

    virtual unsigned int getPlatformTextureId();
    virtual void prepareTexture();

    virtual void synthesizeGLError(unsigned long error);
    virtual bool supportsBGRA();
    virtual bool supportsMapSubCHROMIUM();
    virtual void* mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access);
    virtual void unmapBufferSubDataCHROMIUM(const void*);
    virtual void* mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access);
    virtual void unmapTexSubImage2DCHROMIUM(const void*);
    virtual bool supportsCopyTextureToParentTextureCHROMIUM();
    virtual void copyTextureToParentTextureCHROMIUM(unsigned texture, unsigned parentTexture);

    virtual void activeTexture(unsigned long texture);
    virtual void attachShader(WebGLId program, WebGLId shader);
    virtual void bindAttribLocation(WebGLId program, unsigned long index, const char* name);
    virtual void bindBuffer(unsigned long target, WebGLId buffer);
    virtual void bindFramebuffer(unsigned long target, WebGLId framebuffer);
    virtual void bindRenderbuffer(unsigned long target, WebGLId renderbuffer);
    virtual void bindTexture(unsigned long target, WebGLId texture);
    virtual void blendColor(double red, double green, double blue, double alpha);
    virtual void blendEquation(unsigned long mode);
    virtual void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
    virtual void blendFunc(unsigned long sfactor, unsigned long dfactor);
    virtual void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);

    virtual void bufferData(unsigned long target, int size, const void* data, unsigned long usage);
    virtual void bufferSubData(unsigned long target, long offset, int size, const void* data);

    virtual unsigned long checkFramebufferStatus(unsigned long target);
    virtual void clear(unsigned long mask);
    virtual void clearColor(double red, double green, double blue, double alpha);
    virtual void clearDepth(double depth);
    virtual void clearStencil(long s);
    virtual void colorMask(bool red, bool green, bool blue, bool alpha);
    virtual void compileShader(WebGLId shader);

    virtual void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
    virtual void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);
    virtual void cullFace(unsigned long mode);
    virtual void depthFunc(unsigned long func);
    virtual void depthMask(bool flag);
    virtual void depthRange(double zNear, double zFar);
    virtual void detachShader(WebGLId program, WebGLId shader);
    virtual void disable(unsigned long cap);
    virtual void disableVertexAttribArray(unsigned long index);
    virtual void drawArrays(unsigned long mode, long first, long count);
    virtual void drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset);

    virtual void enable(unsigned long cap);
    virtual void enableVertexAttribArray(unsigned long index);
    virtual void finish();
    virtual void flush();
    virtual void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLId renderbuffer);
    virtual void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLId texture, long level);
    virtual void frontFace(unsigned long mode);
    virtual void generateMipmap(unsigned long target);

    virtual bool getActiveAttrib(WebGLId program, unsigned long index, ActiveInfo&);
    virtual bool getActiveUniform(WebGLId program, unsigned long index, ActiveInfo&);

    virtual void getAttachedShaders(WebGLId program, int maxCount, int* count, unsigned int* shaders);

    virtual int  getAttribLocation(WebGLId program, const char* name);

    virtual void getBooleanv(unsigned long pname, unsigned char* value);

    virtual void getBufferParameteriv(unsigned long target, unsigned long pname, int* value);

    virtual Attributes getContextAttributes();

    virtual unsigned long getError();

    virtual void getFloatv(unsigned long pname, float* value);

    virtual void getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname, int* value);

    virtual void getIntegerv(unsigned long pname, int* value);

    virtual void getProgramiv(WebGLId program, unsigned long pname, int* value);

    virtual WebString getProgramInfoLog(WebGLId program);

    virtual void getRenderbufferParameteriv(unsigned long target, unsigned long pname, int* value);

    virtual void getShaderiv(WebGLId shader, unsigned long pname, int* value);

    virtual WebString getShaderInfoLog(WebGLId shader);

    // TBD
    // void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

    virtual WebString getShaderSource(WebGLId shader);
    virtual WebString getString(unsigned long name);

    virtual void getTexParameterfv(unsigned long target, unsigned long pname, float* value);
    virtual void getTexParameteriv(unsigned long target, unsigned long pname, int* value);

    virtual void getUniformfv(WebGLId program, long location, float* value);
    virtual void getUniformiv(WebGLId program, long location, int* value);

    virtual long getUniformLocation(WebGLId program, const char* name);

    virtual void getVertexAttribfv(unsigned long index, unsigned long pname, float* value);
    virtual void getVertexAttribiv(unsigned long index, unsigned long pname, int* value);

    virtual long getVertexAttribOffset(unsigned long index, unsigned long pname);

    virtual void hint(unsigned long target, unsigned long mode);
    virtual bool isBuffer(WebGLId buffer);
    virtual bool isEnabled(unsigned long cap);
    virtual bool isFramebuffer(WebGLId framebuffer);
    virtual bool isProgram(WebGLId program);
    virtual bool isRenderbuffer(WebGLId renderbuffer);
    virtual bool isShader(WebGLId shader);
    virtual bool isTexture(WebGLId texture);
    virtual void lineWidth(double);
    virtual void linkProgram(WebGLId program);
    virtual void pixelStorei(unsigned long pname, long param);
    virtual void polygonOffset(double factor, double units);

    virtual void readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels);

    virtual void releaseShaderCompiler();
    virtual void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
    virtual void sampleCoverage(double value, bool invert);
    virtual void scissor(long x, long y, unsigned long width, unsigned long height);
    virtual void shaderSource(WebGLId shader, const char* string);
    virtual void stencilFunc(unsigned long func, long ref, unsigned long mask);
    virtual void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
    virtual void stencilMask(unsigned long mask);
    virtual void stencilMaskSeparate(unsigned long face, unsigned long mask);
    virtual void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
    virtual void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);

    virtual void texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, const void* pixels);

    virtual void texParameterf(unsigned target, unsigned pname, float param);
    virtual void texParameteri(unsigned target, unsigned pname, int param);

    virtual void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, const void* pixels);

    virtual void uniform1f(long location, float x);
    virtual void uniform1fv(long location, int count, float* v);
    virtual void uniform1i(long location, int x);
    virtual void uniform1iv(long location, int count, int* v);
    virtual void uniform2f(long location, float x, float y);
    virtual void uniform2fv(long location, int count, float* v);
    virtual void uniform2i(long location, int x, int y);
    virtual void uniform2iv(long location, int count, int* v);
    virtual void uniform3f(long location, float x, float y, float z);
    virtual void uniform3fv(long location, int count, float* v);
    virtual void uniform3i(long location, int x, int y, int z);
    virtual void uniform3iv(long location, int count, int* v);
    virtual void uniform4f(long location, float x, float y, float z, float w);
    virtual void uniform4fv(long location, int count, float* v);
    virtual void uniform4i(long location, int x, int y, int z, int w);
    virtual void uniform4iv(long location, int count, int* v);
    virtual void uniformMatrix2fv(long location, int count, bool transpose, const float* value);
    virtual void uniformMatrix3fv(long location, int count, bool transpose, const float* value);
    virtual void uniformMatrix4fv(long location, int count, bool transpose, const float* value);

    virtual void useProgram(WebGLId program);
    virtual void validateProgram(WebGLId program);

    virtual void vertexAttrib1f(unsigned long indx, float x);
    virtual void vertexAttrib1fv(unsigned long indx, const float* values);
    virtual void vertexAttrib2f(unsigned long indx, float x, float y);
    virtual void vertexAttrib2fv(unsigned long indx, const float* values);
    virtual void vertexAttrib3f(unsigned long indx, float x, float y, float z);
    virtual void vertexAttrib3fv(unsigned long indx, const float* values);
    virtual void vertexAttrib4f(unsigned long indx, float x, float y, float z, float w);
    virtual void vertexAttrib4fv(unsigned long indx, const float* values);
    virtual void vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                     unsigned long stride, unsigned long offset);

    virtual void viewport(long x, long y, unsigned long width, unsigned long height);

    // Support for buffer creation and deletion
    virtual unsigned createBuffer();
    virtual unsigned createFramebuffer();
    virtual unsigned createProgram();
    virtual unsigned createRenderbuffer();
    virtual unsigned createShader(unsigned long);
    virtual unsigned createTexture();

    virtual void deleteBuffer(unsigned);
    virtual void deleteFramebuffer(unsigned);
    virtual void deleteProgram(unsigned);
    virtual void deleteRenderbuffer(unsigned);
    virtual void deleteShader(unsigned);
    virtual void deleteTexture(unsigned);

private:
    WebGraphicsContext3D::Attributes m_attributes;
    bool m_initialized;
    bool m_renderDirectlyToWebView;

    unsigned int m_texture;
    unsigned int m_fbo;
    unsigned int m_depthStencilBuffer;
    unsigned int m_cachedWidth, m_cachedHeight;

    // For multisampling
    unsigned int m_multisampleFBO;
    unsigned int m_multisampleDepthStencilBuffer;
    unsigned int m_multisampleColorBuffer;

    // For tracking which FBO is bound
    unsigned int m_boundFBO;

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    unsigned char* m_scanline;
    void flipVertically(unsigned char* framebuffer,
                        unsigned int width,
                        unsigned int height);
#endif

    // Take into account the user's requested context creation attributes, in
    // particular stencil and antialias, and determine which could or could
    // not be honored based on the capabilities of the OpenGL implementation.
    void validateAttributes();

    // Resolve the given rectangle of the multisampled framebuffer if necessary.
    void resolveMultisampledFramebuffer(unsigned x, unsigned y, unsigned width, unsigned height);

    // Note: we aren't currently using this information, but we will
    // need to in order to verify that all enabled vertex arrays have
    // a valid buffer bound -- to avoid crashes on certain cards.
    unsigned int m_boundArrayBuffer;
    struct VertexAttribPointerState {
        VertexAttribPointerState();

        bool enabled;
        unsigned long buffer;
        unsigned long indx;
        int size;
        int type;
        bool normalized;
        unsigned long stride;
        unsigned long offset;
    };

    enum {
        NumTrackedPointerStates = 2
    };
    VertexAttribPointerState m_vertexAttribPointerState[NumTrackedPointerStates];

    // Errors raised by synthesizeGLError().
    ListHashSet<unsigned long> m_syntheticErrors;

    OwnPtr<gfx::GLContext> m_glContext;

    // ANGLE related.
    struct ShaderSourceEntry {
        ShaderSourceEntry()
                : type(0)
                , source(0)
                , log(0)
                , translatedSource(0)
                , isValid(false)
        {
        }

        ~ShaderSourceEntry()
        {
            if (source)
                fastFree(source);
            if (log)
                fastFree(log);
            if (translatedSource)
                fastFree(translatedSource);
        }

        unsigned long type;
        char* source;
        char* log;
        char* translatedSource;
        bool isValid;
    };

    bool angleCreateCompilers();
    void angleDestroyCompilers();
    bool angleValidateShaderSource(ShaderSourceEntry& entry);

    typedef HashMap<WebGLId, ShaderSourceEntry> ShaderSourceMap;
    ShaderSourceMap m_shaderSourceMap;

    ShHandle m_fragmentCompiler;
    ShHandle m_vertexCompiler;
};

} // namespace WebKit

#endif // ENABLE(3D_CANVAS)

#endif
