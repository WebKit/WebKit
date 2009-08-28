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
    class CanvasBuffer;
    class CanvasFramebuffer;
    class CanvasNumberArray;
    class CanvasProgram;
    class CanvasRenderbuffer;
    class CanvasShader;
    class CanvasTexture;
    class HTMLCanvasElement;
    class HTMLImageElement;
    class WebKitCSSMatrix;
    
    class GraphicsContext3D : Noncopyable {
    public:
        enum ShaderType { FRAGMENT_SHADER, VERTEX_SHADER };
    
        GraphicsContext3D();
        virtual ~GraphicsContext3D();

#if PLATFORM(MAC)
        PlatformGraphicsContext3D platformGraphicsContext3D() const { return m_contextObj; }
        Platform3DObject platformTexture() const { return m_texture; }
#else
        PlatformGraphicsContext3D platformGraphicsContext3D() const { return NullPlatformGraphicsContext3D; }
        Platform3DObject platformTexture() const { return NullPlatform3DObject; }
#endif
        void checkError() const;
        
        virtual void makeContextCurrent();
        
        void activeTexture(unsigned long texture);
        void attachShader(CanvasProgram* program, CanvasShader* shader);
        void bindAttribLocation(CanvasProgram*, unsigned long index, const String& name);
        void bindBuffer(unsigned long target, CanvasBuffer*);
        void bindFramebuffer(unsigned long target, CanvasFramebuffer*);
        void bindRenderbuffer(unsigned long target, CanvasRenderbuffer*);
        void bindTexture(unsigned target, CanvasTexture* texture3D);
        void blendColor(double red, double green, double blue, double alpha);
        void blendEquation(unsigned long mode );
        void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
        void blendFunc(unsigned long sfactor, unsigned long dfactor);
        void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);
        void bufferData(unsigned long target, CanvasNumberArray*, unsigned long usage);
        void bufferSubData(unsigned long target, long offset, CanvasNumberArray*);
        unsigned long checkFramebufferStatus(CanvasFramebuffer*);
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
        void drawArrays(unsigned long mode, long first, unsigned long count);
        void drawElements(unsigned long mode, unsigned long count, unsigned long type, void* array);
        void enable(unsigned long cap);
        void enableVertexAttribArray(unsigned long index);
        void finish();
        void flush();
        void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer*);
        void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture*, long level);
        void frontFace(unsigned long mode);
        void generateMipmap(unsigned long target);
        
        int  getAttribLocation(CanvasProgram*, const String& name);
        unsigned long getError();
        String getString(unsigned long name);
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
        
        //void readPixelslong x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels);
        
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
        void texParameter(unsigned target, unsigned pname, CanvasNumberArray*);
        void texParameter(unsigned target, unsigned pname, double value);
        void uniform(long location, float v0);
        void uniform(long location, float v0, float v1);
        void uniform(long location, float v0, float v1, float v2);
        void uniform(long location, float v0, float v1, float v2, float v3);
        void uniform(long location, int v0);
        void uniform(long location, int v0, int v1);
        void uniform(long location, int v0, int v1, int v2);
        void uniform(long location, int v0, int v1, int v2, int v3);
        void uniform(long location, CanvasNumberArray*);
        void uniformMatrix(long location, long count, bool transpose, CanvasNumberArray*);
        void uniformMatrix(long location, bool transpose, const Vector<WebKitCSSMatrix*>&);
        void uniformMatrix(long location, bool transpose, const WebKitCSSMatrix*);
        void useProgram(CanvasProgram*);
        void validateProgram(CanvasProgram*);
        void vertexAttrib(unsigned long indx, float v0);
        void vertexAttrib(unsigned long indx, float v0, float v1);
        void vertexAttrib(unsigned long indx, float v0, float v1, float v2);
        void vertexAttrib(unsigned long indx, float v0, float v1, float v2, float v3);
        void vertexAttrib(unsigned long indx, CanvasNumberArray*);
        void vertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized, unsigned long stride, CanvasNumberArray*);
        void viewport(long x, long y, unsigned long width, unsigned long height);
        
        PassRefPtr<CanvasNumberArray> get(unsigned long pname);
        PassRefPtr<CanvasNumberArray> getBufferParameter(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasNumberArray> getFramebufferAttachmentParameter(unsigned long target, unsigned long attachment, unsigned long pname);
        PassRefPtr<CanvasNumberArray> getProgram(CanvasProgram*, unsigned long pname);
        String getProgramInfoLog(CanvasProgram*);
        PassRefPtr<CanvasNumberArray> getRenderbufferParameter(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasNumberArray> getShader(CanvasShader*, unsigned long pname);
        String getShaderInfoLog(CanvasShader*);
        String getShaderSource(CanvasShader*);
        
        PassRefPtr<CanvasNumberArray> getTexParameter(unsigned long target, unsigned long pname);
        PassRefPtr<CanvasNumberArray> getUniform(CanvasProgram*, long location, long size);
        long getUniformLocation(CanvasProgram*, const String& name);
        PassRefPtr<CanvasNumberArray> getVertexAttrib(unsigned long index, unsigned long pname);
        
        // These next 4 functions return an error code0 if no errors) rather than using an ExceptionCode.
        // Currently they return -1 on any error.
        int texImage2D(unsigned target, unsigned level, HTMLImageElement* image);
        int texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLImageElement* image);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLCanvasElement* canvas);
                
        void reshape(int width, int height);
        
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
        
        Vector<Vector<float> > m_vertexArray;
        
#if PLATFORM(MAC)
        CGLContextObj m_contextObj;
        GLuint m_texture;
        GLuint m_fbo;
        GLuint m_depthBuffer;
#endif        
    };

} // namespace WebCore

#endif // GraphicsContext3D_h

