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

#ifndef WebGLRenderingContext_h
#define WebGLRenderingContext_h

#include "CanvasRenderingContext.h"
#include "ExceptionCode.h"
#include "WebGLFloatArray.h"
#include "WebGLIntArray.h"
#include "WebGLUnsignedByteArray.h"
#include "GraphicsContext3D.h"
#include "PlatformString.h"

namespace WebCore {

class WebGLActiveInfo;
class WebGLBuffer;
class WebGLFramebuffer;
class CanvasObject;
class WebGLProgram;
class WebGLRenderbuffer;
class WebGLShader;
class WebGLTexture;
class HTMLImageElement;
class HTMLVideoElement;
class ImageData;
class WebKitCSSMatrix;

    class WebGLRenderingContext : public CanvasRenderingContext {
    public:
        static PassOwnPtr<WebGLRenderingContext> create(HTMLCanvasElement*);
        virtual ~WebGLRenderingContext();

        virtual bool is3d() const { return true; }

        // Helper to return the size in bytes of OpenGL data types
        // like GL_FLOAT, GL_INT, etc.
        int sizeInBytes(int type, ExceptionCode& ec);

        void activeTexture(unsigned long texture);
        void attachShader(WebGLProgram*, WebGLShader*, ExceptionCode& ec);
        void bindAttribLocation(WebGLProgram*, unsigned long index, const String& name, ExceptionCode& ec);
        void bindBuffer(unsigned long target, WebGLBuffer*, ExceptionCode& ec);
        void bindFramebuffer(unsigned long target, WebGLFramebuffer*, ExceptionCode& ec);
        void bindRenderbuffer(unsigned long target, WebGLRenderbuffer*, ExceptionCode& ec);
        void bindTexture(unsigned long target, WebGLTexture*, ExceptionCode& ec);
        void blendColor(double red, double green, double blue, double alpha);
        void blendEquation(unsigned long mode);
        void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
        void blendFunc(unsigned long sfactor, unsigned long dfactor);
        void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);

        void bufferData(unsigned long target, int size, unsigned long usage);
        void bufferData(unsigned long target, WebGLArray* data, unsigned long usage);
        void bufferSubData(unsigned long target, long offset, WebGLArray* data);

        unsigned long checkFramebufferStatus(unsigned long target);
        void clear(unsigned long mask);
        void clearColor(double red, double green, double blue, double alpha);
        void clearDepth(double);
        void clearStencil(long);
        void colorMask(bool red, bool green, bool blue, bool alpha);
        void compileShader(WebGLShader*, ExceptionCode& ec);
        
        //void compressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
        //void compressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);
        
        void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
        void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);

        PassRefPtr<WebGLBuffer> createBuffer();
        PassRefPtr<WebGLFramebuffer> createFramebuffer();
        PassRefPtr<WebGLProgram> createProgram();
        PassRefPtr<WebGLRenderbuffer> createRenderbuffer();
        PassRefPtr<WebGLShader> createShader(unsigned long type);
        PassRefPtr<WebGLTexture> createTexture();

        void cullFace(unsigned long mode);

        void deleteBuffer(WebGLBuffer*);
        void deleteFramebuffer(WebGLFramebuffer*);
        void deleteProgram(WebGLProgram*);
        void deleteRenderbuffer(WebGLRenderbuffer*);
        void deleteShader(WebGLShader*);
        void deleteTexture(WebGLTexture*);
        
        void depthFunc(unsigned long);
        void depthMask(bool);
        void depthRange(double zNear, double zFar);
        void detachShader(WebGLProgram*, WebGLShader*, ExceptionCode& ec);
        void disable(unsigned long cap);
        void disableVertexAttribArray(unsigned long index);
        void drawArrays(unsigned long mode, long first, long count);
        void drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset);

        void enable(unsigned long cap);
        void enableVertexAttribArray(unsigned long index);
        void finish();
        void flush();
        void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLRenderbuffer*, ExceptionCode& ec);
        void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLTexture*, long level, ExceptionCode& ec);
        void frontFace(unsigned long mode);
        void generateMipmap(unsigned long target);

        PassRefPtr<WebGLActiveInfo> getActiveAttrib(WebGLProgram*, unsigned long index, ExceptionCode&);
        PassRefPtr<WebGLActiveInfo> getActiveUniform(WebGLProgram*, unsigned long index, ExceptionCode&);

        int  getAttribLocation(WebGLProgram*, const String& name);

        bool getBoolean(unsigned long pname);
        PassRefPtr<WebGLUnsignedByteArray> getBooleanv(unsigned long pname);
        int getBufferParameteri(unsigned long target, unsigned long pname);
        PassRefPtr<WebGLIntArray> getBufferParameteriv(unsigned long target, unsigned long pname);

        unsigned long getError();

        float getFloat(unsigned long pname);
        PassRefPtr<WebGLFloatArray> getFloatv(unsigned long pname);
        int getFramebufferAttachmentParameteri(unsigned long target, unsigned long attachment, unsigned long pname);
        PassRefPtr<WebGLIntArray> getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname);
        int getInteger(unsigned long pname);
        PassRefPtr<WebGLIntArray> getIntegerv(unsigned long pname);
        int getProgrami(WebGLProgram*, unsigned long pname, ExceptionCode& ec);
        PassRefPtr<WebGLIntArray> getProgramiv(WebGLProgram*, unsigned long pname, ExceptionCode& ec);
        String getProgramInfoLog(WebGLProgram*, ExceptionCode& ec);
        int getRenderbufferParameteri(unsigned long target, unsigned long pname);
        PassRefPtr<WebGLIntArray> getRenderbufferParameteriv(unsigned long target, unsigned long pname);
        int getShaderi(WebGLShader*, unsigned long pname, ExceptionCode& ec);
        PassRefPtr<WebGLIntArray> getShaderiv(WebGLShader*, unsigned long pname, ExceptionCode& ec);

        String getShaderInfoLog(WebGLShader*, ExceptionCode& ec);

        // TBD
        // void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

        String getShaderSource(WebGLShader*, ExceptionCode& ec);
        String getString(unsigned long name);

        float getTexParameterf(unsigned long target, unsigned long pname);
        PassRefPtr<WebGLFloatArray> getTexParameterfv(unsigned long target, unsigned long pname);
        int getTexParameteri(unsigned long target, unsigned long pname);
        PassRefPtr<WebGLIntArray> getTexParameteriv(unsigned long target, unsigned long pname);

        float getUniformf(WebGLProgram* program, long location, ExceptionCode& ec);
        PassRefPtr<WebGLFloatArray> getUniformfv(WebGLProgram* program, long location, ExceptionCode& ec);
        long getUniformi(WebGLProgram* program, long location, ExceptionCode& ec);
        PassRefPtr<WebGLIntArray> getUniformiv(WebGLProgram* program, long location, ExceptionCode& ec);

        long getUniformLocation(WebGLProgram*, const String& name, ExceptionCode& ec);

        float getVertexAttribf(unsigned long index, unsigned long pname);
        PassRefPtr<WebGLFloatArray> getVertexAttribfv(unsigned long index, unsigned long pname);
        long getVertexAttribi(unsigned long index, unsigned long pname);
        PassRefPtr<WebGLIntArray> getVertexAttribiv(unsigned long index, unsigned long pname);

        long getVertexAttribOffset(unsigned long index, unsigned long pname);

        void hint(unsigned long target, unsigned long mode);
        bool isBuffer(WebGLBuffer*);
        bool isEnabled(unsigned long cap);
        bool isFramebuffer(WebGLFramebuffer*);
        bool isProgram(WebGLProgram*);
        bool isRenderbuffer(WebGLRenderbuffer*);
        bool isShader(WebGLShader*);
        bool isTexture(WebGLTexture*);
        void lineWidth(double);
        void linkProgram(WebGLProgram*, ExceptionCode& ec);
        void pixelStorei(unsigned long pname, long param);
        void polygonOffset(double factor, double units);
        
        PassRefPtr<WebGLArray> readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type);
        
        void releaseShaderCompiler();
        void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
        void sampleCoverage(double value, bool invert);
        void scissor(long x, long y, unsigned long width, unsigned long height);
        void shaderSource(WebGLShader*, const String&, ExceptionCode& ec);
        void stencilFunc(unsigned long func, long ref, unsigned long mask);
        void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
        void stencilMask(unsigned long);
        void stencilMaskSeparate(unsigned long face, unsigned long mask);
        void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
        void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);

        void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                        unsigned width, unsigned height, unsigned border,
                        unsigned format, unsigned type, WebGLArray* pixels, ExceptionCode&);
        void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                        unsigned width, unsigned height, unsigned border,
                        unsigned format, unsigned type, ImageData* pixels, ExceptionCode&);
        void texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                        bool flipY, bool premultiplyAlpha, ExceptionCode&);
        void texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                        bool flipY, bool premultiplyAlpha, ExceptionCode&);
        void texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                        bool flipY, bool premultiplyAlpha, ExceptionCode&);

        void texParameterf(unsigned target, unsigned pname, float param);
        void texParameteri(unsigned target, unsigned pname, int param);

        void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned width, unsigned height,
                           unsigned format, unsigned type, WebGLArray* pixels, ExceptionCode&);
        void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned width, unsigned height,
                           unsigned format, unsigned type, ImageData* pixels, ExceptionCode&);
        void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned width, unsigned height, HTMLImageElement* image,
                           bool flipY, bool premultiplyAlpha, ExceptionCode&);
        void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned width, unsigned height, HTMLCanvasElement* canvas,
                           bool flipY, bool premultiplyAlpha, ExceptionCode&);
        void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned width, unsigned height, HTMLVideoElement* video,
                           bool flipY, bool premultiplyAlpha, ExceptionCode&);

        void uniform1f(long location, float x);
        void uniform1fv(long location, WebGLFloatArray* v);
        void uniform1fv(long location, float* v, int size);
        void uniform1i(long location, int x);
        void uniform1iv(long location, WebGLIntArray* v);
        void uniform1iv(long location, int* v, int size);
        void uniform2f(long location, float x, float y);
        void uniform2fv(long location, WebGLFloatArray* v);
        void uniform2fv(long location, float* v, int size);
        void uniform2i(long location, int x, int y);
        void uniform2iv(long location, WebGLIntArray* v);
        void uniform2iv(long location, int* v, int size);
        void uniform3f(long location, float x, float y, float z);
        void uniform3fv(long location, WebGLFloatArray* v);
        void uniform3fv(long location, float* v, int size);
        void uniform3i(long location, int x, int y, int z);
        void uniform3iv(long location, WebGLIntArray* v);
        void uniform3iv(long location, int* v, int size);
        void uniform4f(long location, float x, float y, float z, float w);
        void uniform4fv(long location, WebGLFloatArray* v);
        void uniform4fv(long location, float* v, int size);
        void uniform4i(long location, int x, int y, int z, int w);
        void uniform4iv(long location, WebGLIntArray* v);
        void uniform4iv(long location, int* v, int size);
        void uniformMatrix2fv(long location, bool transpose, WebGLFloatArray* value);
        void uniformMatrix2fv(long location, bool transpose, float* value, int size);
        void uniformMatrix3fv(long location, bool transpose, WebGLFloatArray* value);
        void uniformMatrix3fv(long location, bool transpose, float* value, int size);
        void uniformMatrix4fv(long location, bool transpose, WebGLFloatArray* value);
        void uniformMatrix4fv(long location, bool transpose, float* value, int size);

        void useProgram(WebGLProgram*);
        void validateProgram(WebGLProgram*);

        void vertexAttrib1f(unsigned long indx, float x);
        void vertexAttrib1fv(unsigned long indx, WebGLFloatArray* values);
        void vertexAttrib1fv(unsigned long indx, float* values, int size);
        void vertexAttrib2f(unsigned long indx, float x, float y);
        void vertexAttrib2fv(unsigned long indx, WebGLFloatArray* values);
        void vertexAttrib2fv(unsigned long indx, float* values, int size);
        void vertexAttrib3f(unsigned long indx, float x, float y, float z);
        void vertexAttrib3fv(unsigned long indx, WebGLFloatArray* values);
        void vertexAttrib3fv(unsigned long indx, float* values, int size);
        void vertexAttrib4f(unsigned long indx, float x, float y, float z, float w);
        void vertexAttrib4fv(unsigned long indx, WebGLFloatArray* values);
        void vertexAttrib4fv(unsigned long indx, float* values, int size);
        void vertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized,
                                 unsigned long stride, unsigned long offset);

        void viewport(long x, long y, unsigned long width, unsigned long height);

        GraphicsContext3D* graphicsContext3D() const { return m_context.get(); }
    
        void reshape(int width, int height);

        // Helpers for notification about paint events.
        void beginPaint();
        void endPaint();
        
        void removeObject(CanvasObject*);
        
    private:
        friend class CanvasObject;

        WebGLRenderingContext(HTMLCanvasElement*, PassOwnPtr<GraphicsContext3D>);

        void addObject(CanvasObject*);
        void detachAndRemoveAllObjects();

        void markContextChanged();
        void cleanupAfterGraphicsCall(bool changed)
        {
            m_context->checkError();
            if (changed)
                markContextChanged();
        }
        
        OwnPtr<GraphicsContext3D> m_context;
        bool m_needsUpdate;
        bool m_markedCanvasDirty;
        // FIXME: I think this is broken -- it does not increment any
        // reference counts, so may refer to destroyed objects.
        HashSet<CanvasObject*> m_canvasObjects;
    };

} // namespace WebCore

#endif
