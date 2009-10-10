/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef GraphicsContext3D_h
#define GraphicsContext3D_h

#include "PlatformString.h"

#include <wtf/Noncopyable.h>

#if PLATFORM(MAC)
#include <OpenGL/OpenGL.h>

typedef void* PlatformGraphicsContext3D;
const  PlatformGraphicsContext3D NullPlatformGraphicsContext3D = 0;
typedef GLuint Platform3DObject;
const Platform3DObject NullPlatform3DObject = 0;
#else
typedef void* PlatformGraphicsContext3D;
const  PlatformGraphicsContext3D NullPlatformGraphicsContext3D = 0;
typedef int Platform3DObject;
const Platform3DObject NullPlatform3DObject = 0;
#endif

namespace WebCore {
    class CanvasActiveInfo;
    class CanvasArray;
    class CanvasBuffer;
    class CanvasUnsignedByteArray;
    class CanvasFloatArray;
    class CanvasFramebuffer;
    class CanvasIntArray;
    class CanvasProgram;
    class CanvasRenderbuffer;
    class CanvasRenderingContext3D;
    class CanvasShader;
    class CanvasTexture;
    class HTMLCanvasElement;
    class HTMLImageElement;
    class HTMLVideoElement;
    class ImageData;
    class WebKitCSSMatrix;

    struct ActiveInfo {
        String name;
        unsigned type;
        int size;
    };

    // FIXME: ideally this would be used on all platforms.
#if PLATFORM(CHROMIUM)
    class GraphicsContext3DInternal;
#endif

    class GraphicsContext3D : Noncopyable {
    public:
        enum ShaderType { FRAGMENT_SHADER, VERTEX_SHADER };
    
        GraphicsContext3D();
        virtual ~GraphicsContext3D();

#if PLATFORM(MAC)
        PlatformGraphicsContext3D platformGraphicsContext3D() const { return m_contextObj; }
        Platform3DObject platformTexture() const { return m_texture; }
#elif PLATFORM(CHROMIUM)
        PlatformGraphicsContext3D platformGraphicsContext3D() const;
        Platform3DObject platformTexture() const;
#else
        PlatformGraphicsContext3D platformGraphicsContext3D() const { return NullPlatformGraphicsContext3D; }
        Platform3DObject platformTexture() const { return NullPlatform3DObject; }
#endif
        void checkError() const;
        
        void makeContextCurrent();
        
        // Helper to return the size in bytes of OpenGL data types
        // like GL_FLOAT, GL_INT, etc.
        int sizeInBytes(int type);

        void activeTexture(unsigned long texture);
        void attachShader(CanvasProgram* program, CanvasShader* shader);
        void bindAttribLocation(CanvasProgram*, unsigned long index, const String& name);
        void bindBuffer(unsigned long target, CanvasBuffer*);
        void bindFramebuffer(unsigned long target, CanvasFramebuffer*);
        void bindRenderbuffer(unsigned long target, CanvasRenderbuffer*);
        void bindTexture(unsigned long target, CanvasTexture* texture);
        void blendColor(double red, double green, double blue, double alpha);
        void blendEquation(unsigned long mode);
        void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
        void blendFunc(unsigned long sfactor, unsigned long dfactor);
        void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);

        void bufferData(unsigned long target, int size, unsigned long usage);
        void bufferData(unsigned long target, CanvasArray* data, unsigned long usage);
        void bufferSubData(unsigned long target, long offset, CanvasArray* data);

        unsigned long checkFramebufferStatus(unsigned long target);
        void clear(unsigned long mask);
        void clearColor(double red, double green, double blue, double alpha);
        void clearDepth(double depth);
        void clearStencil(long s);
        void colorMask(bool red, bool green, bool blue, bool alpha);
        void compileShader(CanvasShader*);
        
        //void compressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
        //void compressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);
        
        void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
        void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);
        void cullFace(unsigned long mode);
        void depthFunc(unsigned long func);
        void depthMask(bool flag);
        void depthRange(double zNear, double zFar);
        void detachShader(CanvasProgram*, CanvasShader*);
        void disable(unsigned long cap);
        void disableVertexAttribArray(unsigned long index);
        void drawArrays(unsigned long mode, long first, long count);
        void drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset);

        void enable(unsigned long cap);
        void enableVertexAttribArray(unsigned long index);
        void finish();
        void flush();
        void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer*);
        void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture*, long level);
        void frontFace(unsigned long mode);
        void generateMipmap(unsigned long target);

        bool getActiveAttrib(CanvasProgram* program, unsigned long index, ActiveInfo&);
        bool getActiveUniform(CanvasProgram* program, unsigned long index, ActiveInfo&);

        int  getAttribLocation(CanvasProgram*, const String& name);

        bool getBoolean(unsigned long pname);
        PassRefPtr<CanvasUnsignedByteArray> getBooleanv(unsigned long pname);
        int getBufferParameteri(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasIntArray> getBufferParameteriv(unsigned long target, unsigned long pname);

        unsigned long getError();

        float getFloat(unsigned long pname);
        PassRefPtr<CanvasFloatArray> getFloatv(unsigned long pname);
        int getFramebufferAttachmentParameteri(unsigned long target, unsigned long attachment, unsigned long pname);
        PassRefPtr<CanvasIntArray> getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname);
        int getInteger(unsigned long pname);
        PassRefPtr<CanvasIntArray> getIntegerv(unsigned long pname);
        int getProgrami(CanvasProgram*, unsigned long pname);
        PassRefPtr<CanvasIntArray> getProgramiv(CanvasProgram*, unsigned long pname);
        String getProgramInfoLog(CanvasProgram*);
        int getRenderbufferParameteri(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasIntArray> getRenderbufferParameteriv(unsigned long target, unsigned long pname);
        int getShaderi(CanvasShader*, unsigned long pname);
        PassRefPtr<CanvasIntArray> getShaderiv(CanvasShader*, unsigned long pname);

        String getShaderInfoLog(CanvasShader*);

        // TBD
        // void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

        String getShaderSource(CanvasShader*);
        String getString(unsigned long name);
        
        float getTexParameterf(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasFloatArray> getTexParameterfv(unsigned long target, unsigned long pname);
        int getTexParameteri(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasIntArray> getTexParameteriv(unsigned long target, unsigned long pname);

        float getUniformf(CanvasProgram* program, long location);
        PassRefPtr<CanvasFloatArray> getUniformfv(CanvasProgram* program, long location);
        int getUniformi(CanvasProgram* program, long location);
        PassRefPtr<CanvasIntArray> getUniformiv(CanvasProgram* program, long location);

        long getUniformLocation(CanvasProgram*, const String& name);

        float getVertexAttribf(unsigned long index, unsigned long pname);
        PassRefPtr<CanvasFloatArray> getVertexAttribfv(unsigned long index, unsigned long pname);
        int getVertexAttribi(unsigned long index, unsigned long pname);
        PassRefPtr<CanvasIntArray> getVertexAttribiv(unsigned long index, unsigned long pname);
        
        long getVertexAttribOffset(unsigned long index, unsigned long pname);

        void hint(unsigned long target, unsigned long mode);
        bool isBuffer(CanvasBuffer*);
        bool isEnabled(unsigned long cap);
        bool isFramebuffer(CanvasFramebuffer*);
        bool isProgram(CanvasProgram*);
        bool isRenderbuffer(CanvasRenderbuffer*);
        bool isShader(CanvasShader*);
        bool isTexture(CanvasTexture*);
        void lineWidth(double);
        void linkProgram(CanvasProgram*);
        void pixelStorei(unsigned long pname, long param);
        void polygonOffset(double factor, double units);
        
        // TBD
        //void readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels);
        
        void releaseShaderCompiler();
        void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
        void sampleCoverage(double value, bool invert);
        void scissor(long x, long y, unsigned long width, unsigned long height);
        void shaderSource(CanvasShader*, const String& string);
        void stencilFunc(unsigned long func, long ref, unsigned long mask);
        void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
        void stencilMask(unsigned long mask);
        void stencilMaskSeparate(unsigned long face, unsigned long mask);
        void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
        void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);

        // These next several functions return an error code (0 if no errors) rather than using an ExceptionCode.
        // Currently they return -1 on any error.
        int texImage2D(unsigned target, unsigned level, unsigned internalformat,
                       unsigned width, unsigned height, unsigned border,
                       unsigned format, unsigned type, CanvasArray* pixels);
        int texImage2D(unsigned target, unsigned level, unsigned internalformat,
                       unsigned width, unsigned height, unsigned border,
                       unsigned format, unsigned type, ImageData* pixels);
        int texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                       bool flipY, bool premultiplyAlpha);
        int texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                       bool flipY, bool premultiplyAlpha);
        int texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                       bool flipY, bool premultiplyAlpha);

        void texParameterf(unsigned target, unsigned pname, float param);
        void texParameteri(unsigned target, unsigned pname, int param);

        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                          unsigned width, unsigned height,
                          unsigned format, unsigned type, CanvasArray* pixels);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                          unsigned width, unsigned height,
                          unsigned format, unsigned type, ImageData* pixels);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                          unsigned width, unsigned height, HTMLImageElement* image,
                          bool flipY, bool premultiplyAlpha);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                          unsigned width, unsigned height, HTMLCanvasElement* canvas,
                          bool flipY, bool premultiplyAlpha);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                          unsigned width, unsigned height, HTMLVideoElement* video,
                          bool flipY, bool premultiplyAlpha);

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

        void useProgram(CanvasProgram*);
        void validateProgram(CanvasProgram*);

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

        void reshape(int width, int height);
        
        // Helpers for notification about paint events
        void beginPaint(CanvasRenderingContext3D* context);
        void endPaint();

        // Support for buffer creation and deletion
        unsigned createBuffer();
        unsigned createFramebuffer();
        unsigned createProgram();
        unsigned createRenderbuffer();
        unsigned createShader(ShaderType);
        unsigned createTexture();
        
        void deleteBuffer(unsigned);
        void deleteFramebuffer(unsigned);
        void deleteProgram(unsigned);
        void deleteRenderbuffer(unsigned);
        void deleteShader(unsigned);
        void deleteTexture(unsigned);        
        
    private:        
        int m_currentWidth, m_currentHeight;
        
#if PLATFORM(MAC)
        Vector<Vector<float> > m_vertexArray;
        
        CGLContextObj m_contextObj;
        GLuint m_texture;
        GLuint m_fbo;
        GLuint m_depthBuffer;
#endif        

        // FIXME: ideally this would be used on all platforms.
#if PLATFORM(CHROMIUM)
        friend class GraphicsContext3DInternal;
        OwnPtr<GraphicsContext3DInternal> m_internal;
#endif
    };

} // namespace WebCore

#endif // GraphicsContext3D_h

