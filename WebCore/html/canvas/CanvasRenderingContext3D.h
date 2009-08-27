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

        void glActiveTexture(unsigned long texture);
        void glAttachShader(CanvasProgram*, CanvasShader*);
        void glBindAttribLocation(CanvasProgram*, unsigned long index, const String& name);
        void glBindBuffer(unsigned long target, CanvasBuffer*);
        void glBindFramebuffer(unsigned long target, CanvasFramebuffer*);
        void glBindRenderbuffer(unsigned long target, CanvasRenderbuffer*);
        void glBindTexture(unsigned target, CanvasTexture*);
        void glBlendColor(double red, double green, double blue, double alpha);
        void glBlendEquation( unsigned long mode );
        void glBlendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
        void glBlendFunc(unsigned long sfactor, unsigned long dfactor);
        void glBlendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);
        void glBufferData(unsigned long target, CanvasNumberArray*, unsigned long usage);
        void glBufferSubData(unsigned long target, long offset, CanvasNumberArray*);
        unsigned long glCheckFramebufferStatus(CanvasFramebuffer*);
        void glClear(unsigned long mask);
        void glClearColor(double red, double green, double blue, double alpha);
        void glClearDepth(double);
        void glClearStencil(long);
        void glColorMask(bool red, bool green, bool blue, bool alpha);
        void glCompileShader(CanvasShader*);
        
        //void glCompressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
        //void glCompressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);
        
        void glCopyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
        void glCopyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);
        void glCullFace(unsigned long mode);
        void glDepthFunc(unsigned long);
        void glDepthMask(bool);
        void glDepthRange(double zNear, double zFar);
        void glDetachShader(CanvasProgram*, CanvasShader*);
        void glDisable(unsigned long cap);
        void glDisableVertexAttribArray(unsigned long index);
        void glDrawArrays(unsigned long mode, long first, unsigned long count);
        void glDrawElements(unsigned long mode, unsigned long count, unsigned long type, void* array);
        void glEnable(unsigned long cap);
        void glEnableVertexAttribArray(unsigned long index);
        void glFinish();
        void glFlush();
        void glFramebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer*);
        void glFramebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture*, long level);
        void glFrontFace(unsigned long mode);
        void glGenerateMipmap(unsigned long target);
        
        int  glGetAttribLocation(CanvasProgram*, const String& name);
        unsigned long glGetError();
        String glGetString(unsigned long name);
        void glHint(unsigned long target, unsigned long mode);
        bool glIsBuffer(CanvasBuffer*);
        bool glIsEnabled(unsigned long cap);
        bool glIsFramebuffer(CanvasFramebuffer*);
        bool glIsProgram(CanvasProgram*);
        bool glIsRenderbuffer(CanvasRenderbuffer*);
        bool glIsShader(CanvasShader*);
        bool glIsTexture(CanvasTexture*);
        void glLineWidth(double);
        void glLinkProgram(CanvasProgram*);
        void glPixelStorei(unsigned long pname, long param);
        void glPolygonOffset(double factor, double units);
        
        //void glReadPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels);
        
        void glReleaseShaderCompiler();
        void glRenderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
        void glSampleCoverage(double value, bool invert);
        void glScissor(long x, long y, unsigned long width, unsigned long height);
        void glShaderSource(CanvasShader*, const String&);
        void glStencilFunc(unsigned long func, long ref, unsigned long mask);
        void glStencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
        void glStencilMask(unsigned long);
        void glStencilMaskSeparate(unsigned long face, unsigned long mask);
        void glStencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
        void glStencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);
        void glTexParameter(unsigned target, unsigned pname, CanvasNumberArray*);
        void glTexParameter(unsigned target, unsigned pname, double value);
        void glUniform(long location, float v0);
        void glUniform(long location, float v0, float v1);
        void glUniform(long location, float v0, float v1, float v2);
        void glUniform(long location, float v0, float v1, float v2, float v3);
        void glUniform(long location, int v0);
        void glUniform(long location, int v0, int v1);
        void glUniform(long location, int v0, int v1, int v2);
        void glUniform(long location, int v0, int v1, int v2, int v3);
        void glUniform(long location, CanvasNumberArray*);
        void glUniformMatrix(long location, long count, bool transpose, CanvasNumberArray*);
        void glUniformMatrix(long location, bool transpose, const Vector<WebKitCSSMatrix*>&);
        void glUniformMatrix(long location, bool transpose, const WebKitCSSMatrix*);
        void glUseProgram(CanvasProgram*);
        void glValidateProgram(CanvasProgram*);
        void glVertexAttrib(unsigned long indx, float v0);
        void glVertexAttrib(unsigned long indx, float v0, float v1);
        void glVertexAttrib(unsigned long indx, float v0, float v1, float v2);
        void glVertexAttrib(unsigned long indx, float v0, float v1, float v2, float v3);
        void glVertexAttrib(unsigned long indx, CanvasNumberArray*);
        void glVertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized, unsigned long stride, CanvasNumberArray*);
        void glViewport(long x, long y, unsigned long width, unsigned long height);

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
        String glGetProgramInfoLog(CanvasProgram*); // replaces glGetProgramInfoLog
        PassRefPtr<CanvasNumberArray> getRenderbufferParameter(unsigned long target, unsigned long pname); // replaces glGetRenderbufferParameteriv
        PassRefPtr<CanvasNumberArray> getShader(CanvasShader*, unsigned long pname); // replaces glGetShaderiv
        String glGetShaderInfoLog(CanvasShader*);
        String glGetShaderSource(CanvasShader*);

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
