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

#ifndef CanvasRenderingContext3D_h
#define CanvasRenderingContext3D_h

#include "CanvasRenderingContext.h"
#include "ExceptionCode.h"
#include "CanvasNumberArray.h"
#include "GraphicsContext3D.h"
#include "PlatformString.h"

namespace WebCore {

class CanvasBuffer;
class CanvasFramebuffer;
class CanvasObject;
class CanvasProgram;
class CanvasRenderbuffer;
class CanvasShader;
class CanvasTexture;
class HTMLImageElement;
class WebKitCSSMatrix;

    class CanvasRenderingContext3D : public CanvasRenderingContext {
    public:
        CanvasRenderingContext3D(HTMLCanvasElement*);
        ~CanvasRenderingContext3D();

        virtual bool is3d() const { return true; }

        void activeTexture(unsigned long texture);
        void attachShader(CanvasProgram*, CanvasShader*);
        void bindAttribLocation(CanvasProgram*, unsigned long index, const String& name);
        void bindBuffer(unsigned long target, CanvasBuffer*);
        void bindFramebuffer(unsigned long target, CanvasFramebuffer*);
        void bindRenderbuffer(unsigned long target, CanvasRenderbuffer*);
        void bindTexture(unsigned target, CanvasTexture*);
        void blendColor(double red, double green, double blue, double alpha);
        void blendEquation( unsigned long mode );
        void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
        void blendFunc(unsigned long sfactor, unsigned long dfactor);
        void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);
        void bufferData(unsigned long target, CanvasNumberArray*, unsigned long usage);
        void bufferSubData(unsigned long target, long offset, CanvasNumberArray*);
        unsigned long checkFramebufferStatus(CanvasFramebuffer*);
        void clear(unsigned long mask);
        void clearColor(double red, double green, double blue, double alpha);
        void clearDepth(double);
        void clearStencil(long);
        void colorMask(bool red, bool green, bool blue, bool alpha);
        void compileShader(CanvasShader*);
        
        //void compressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
        //void compressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);
        
        void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
        void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);
        void cullFace(unsigned long mode);
        void depthFunc(unsigned long);
        void depthMask(bool);
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
        
        //void glReadPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels);
        
        void releaseShaderCompiler();
        void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
        void sampleCoverage(double value, bool invert);
        void scissor(long x, long y, unsigned long width, unsigned long height);
        void shaderSource(CanvasShader*, const String&);
        void stencilFunc(unsigned long func, long ref, unsigned long mask);
        void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
        void stencilMask(unsigned long);
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

        // Non-GL functions
        PassRefPtr<CanvasBuffer> createBuffer();
        PassRefPtr<CanvasFramebuffer> createFramebuffer();
        PassRefPtr<CanvasProgram> createProgram();                     // replaces glCreateProgram
        PassRefPtr<CanvasRenderbuffer> createRenderbuffer();
        PassRefPtr<CanvasShader> createShader(unsigned long type);  // replaces glCreateShader
        PassRefPtr<CanvasTexture> createTexture();

        void deleteBuffer(CanvasBuffer*);                // replaces glDeleteBuffers
        void deleteFramebuffer(CanvasFramebuffer*);      // replaces glDeleteFramebuffers
        void deleteProgram(CanvasProgram*);              // replaces glDeleteProgram
        void deleteRenderbuffer(CanvasRenderbuffer*);    // replaces glDeleteRenderbuffers
        void deleteShader(CanvasShader*);                // replaces glDeleteShader
        void deleteTexture(CanvasTexture*);              // replaces glDeleteTextures
        
        PassRefPtr<CanvasNumberArray> get(unsigned long pname);      // replaces glGetBooleanv
        PassRefPtr<CanvasNumberArray> getBufferParameter(unsigned long target, unsigned long pname); // replaces glGetBufferParameteriv
        PassRefPtr<CanvasNumberArray> getFramebufferAttachmentParameter(unsigned long target, unsigned long attachment, unsigned long pname); // replaces glGetFramebufferAttachmentParameteriv
        PassRefPtr<CanvasNumberArray> getProgram(CanvasProgram*, unsigned long pname); // replaces glGetProgramiv
        String getProgramInfoLog(CanvasProgram*); // replaces glGetProgramInfoLog
        PassRefPtr<CanvasNumberArray> getRenderbufferParameter(unsigned long target, unsigned long pname); // replaces glGetRenderbufferParameteriv
        PassRefPtr<CanvasNumberArray> getShader(CanvasShader*, unsigned long pname); // replaces glGetShaderiv
        String getShaderInfoLog(CanvasShader*);
        String getShaderSource(CanvasShader*);

        PassRefPtr<CanvasNumberArray> getTexParameter(unsigned long target, unsigned long pname); // replaces glGetTexParameterfv
        PassRefPtr<CanvasNumberArray> getUniform(CanvasProgram*, long location, long size); // replaces glGetUniformfv
        long getUniformLocation(CanvasProgram*, const String& name); // replaces glGetUniformLocation
        PassRefPtr<CanvasNumberArray> getVertexAttrib(unsigned long index, unsigned long pname); // replaces glGetVertexAttribfv
        
        void texImage2D(unsigned target, unsigned level, HTMLImageElement* image, ExceptionCode&);
        void texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas, ExceptionCode&);
        void texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLImageElement* image, ExceptionCode&);
        void texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLCanvasElement* canvas, ExceptionCode&);

        GraphicsContext3D* graphicsContext3D() { return &m_context; }
    
        void reshape(int width, int height);
        
        void removeObject(CanvasObject*);
        
    private:
        void addObject(CanvasObject*);
        void detachAndRemoveAllObjects();

        void markContextChanged();
        void cleanupAfterGraphicsCall(bool changed)
        {
            m_context.checkError();
            if (changed)
                markContextChanged();
        }
        
        GraphicsContext3D m_context;
        bool m_needsUpdate;
        HashSet<CanvasObject*> m_canvasObjects;
    };

} // namespace WebCore

#endif
