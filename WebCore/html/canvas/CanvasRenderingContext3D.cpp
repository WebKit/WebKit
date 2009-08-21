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

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "CanvasRenderingContext3D.h"
#include "CanvasBuffer.h"
#include "CanvasFramebuffer.h"
#include "CanvasProgram.h"
#include "CanvasRenderbuffer.h"
#include "CanvasTexture.h"
#include "CanvasShader.h"
#include "HTMLCanvasElement.h"
#include "RenderBox.h"
#include "RenderLayer.h"

namespace WebCore {

CanvasRenderingContext3D::CanvasRenderingContext3D(HTMLCanvasElement* canvas)
    : CanvasRenderingContext(canvas)
    , m_needsUpdate(true)
{
    m_context.reshape(m_canvas->width(), m_canvas->height());
}

CanvasRenderingContext3D::~CanvasRenderingContext3D()
{
}

void CanvasRenderingContext3D::markContextChanged()
{
    if (m_canvas->renderBox() && m_canvas->renderBox()->hasLayer())
        m_canvas->renderBox()->layer()->rendererContentChanged();
}

void CanvasRenderingContext3D::reshape(int width, int height)
{
    if (m_needsUpdate) {
        if (m_canvas->renderBox() && m_canvas->renderBox()->hasLayer())
            m_canvas->renderBox()->layer()->rendererContentChanged();
        m_needsUpdate = false;
    }
    
    m_context.reshape(width, height);
}

void CanvasRenderingContext3D::glActiveTexture(unsigned long texture)
{
    m_context.glActiveTexture(texture);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glAttachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;
    m_context.glAttachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBindAttribLocation(CanvasProgram* program, unsigned long index, const String& name)
{
    if (!program)
        return;
    m_context.glBindAttribLocation(program, index, name);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBindBuffer(unsigned long target, CanvasBuffer* buffer)
{
    m_context.glBindBuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glBindFramebuffer(unsigned long target, CanvasFramebuffer* buffer)
{
    m_context.glBindFramebuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBindRenderbuffer(unsigned long target, CanvasRenderbuffer* renderbuffer)
{
    m_context.glBindRenderbuffer(target, renderbuffer);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glBindTexture(unsigned target, CanvasTexture* texture)
{
    m_context.glBindTexture(target, texture);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBlendColor(double red, double green, double blue, double alpha)
{
    m_context.glBlendColor(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBlendEquation( unsigned long mode )
{
    m_context.glBlendEquation(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBlendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    m_context.glBlendEquationSeparate(modeRGB, modeAlpha);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glBlendFunc(unsigned long sfactor, unsigned long dfactor)
{
    m_context.glBlendFunc(sfactor, dfactor);
    cleanupAfterGraphicsCall(false);
}       

void CanvasRenderingContext3D::glBlendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    m_context.glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBufferData(unsigned long target, CanvasNumberArray* array, unsigned long usage)
{
    m_context.glBufferData(target, array, usage);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glBufferSubData(unsigned long target, long offset, CanvasNumberArray* array)
{
    if (!array || !array->data().size())
        return;
        
    m_context.glBufferSubData(target, offset, array);
    cleanupAfterGraphicsCall(false);
}

unsigned long CanvasRenderingContext3D::glCheckFramebufferStatus(CanvasFramebuffer* framebuffer)
{
    return m_context.glCheckFramebufferStatus(framebuffer);
}

void CanvasRenderingContext3D::glClearColor(double r, double g, double b, double a)
{
    if (isnan(r))
        r = 0;
    if (isnan(g))
        g = 0;
    if (isnan(b))
        b = 0;
    if (isnan(a))
        a = 1;
    m_context.glClearColor(r, g, b, a);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glClear(unsigned long mask)
{
    m_context.glClear(mask);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::glClearDepth(double depth)
{
    m_context.glClearDepth(depth);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glClearStencil(long s)
{
    m_context.glClearStencil(s);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glColorMask(bool red, bool green, bool blue, bool alpha)
{
    m_context.glColorMask(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glCompileShader(CanvasShader* shader)
{
    m_context.glCompileShader(shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glCopyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    m_context.glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glCopyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    m_context.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glCullFace(unsigned long mode)
{
    m_context.glCullFace(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glDepthFunc(unsigned long func)
{
    m_context.glDepthFunc(func);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glDepthMask(bool flag)
{
    m_context.glDepthMask(flag);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glDepthRange(double zNear, double zFar)
{
    m_context.glDepthRange(zNear, zFar);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glDetachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;

    m_context.glDetachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glDisable(unsigned long cap)
{
    m_context.glDisable(cap);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glDisableVertexAttribArray(unsigned long index)
{
    m_context.glDisableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glDrawArrays(unsigned long mode, long first, unsigned long count)
{
    m_context.glDrawArrays(mode, first, count);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::glDrawElements(unsigned long mode, unsigned long count, unsigned long type, void* array)
{
    m_context.glDrawElements(mode, count, type, array);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::glEnable(unsigned long cap)
{
    m_context.glEnable(cap);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glEnableVertexAttribArray(unsigned long index)
{
    m_context.glEnableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glFinish()
{
    m_context.glFinish();
    cleanupAfterGraphicsCall(true);
}


void CanvasRenderingContext3D::glFlush()
{
    m_context.glFlush();
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::glFramebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer* buffer)
{
    if (!buffer)
        return;
        
    m_context.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glFramebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture* texture, long level)
{
    if (!texture)
        return;
        
    m_context.glFramebufferTexture2D(target, attachment, textarget, texture, level);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glFrontFace(unsigned long mode)
{
    m_context.glFrontFace(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glGenerateMipmap(unsigned long target)
{
    m_context.glGenerateMipmap(target);
    cleanupAfterGraphicsCall(false);
}

int CanvasRenderingContext3D::glGetAttribLocation(CanvasProgram* program, const String& name)
{
    return m_context.glGetAttribLocation(program, name);
}

unsigned long CanvasRenderingContext3D::glGetError()
{
    return m_context.glGetError();
}

String CanvasRenderingContext3D::glGetString(unsigned long name)
{
    return m_context.glGetString(name);
}

void CanvasRenderingContext3D::glHint(unsigned long target, unsigned long mode)
{
    m_context.glHint(target, mode);
    cleanupAfterGraphicsCall(false);
}

bool CanvasRenderingContext3D::glIsBuffer(CanvasBuffer* buffer)
{
    if (!buffer)
        return false;
        
    return m_context.glIsBuffer(buffer);
}

bool CanvasRenderingContext3D::glIsEnabled(unsigned long cap)
{
    return m_context.glIsEnabled(cap);
}

bool CanvasRenderingContext3D::glIsFramebuffer(CanvasFramebuffer* framebuffer)
{
    return m_context.glIsFramebuffer(framebuffer);
}

bool CanvasRenderingContext3D::glIsProgram(CanvasProgram* program)
{
    return m_context.glIsProgram(program);
}

bool CanvasRenderingContext3D::glIsRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    return m_context.glIsRenderbuffer(renderbuffer);
}

bool CanvasRenderingContext3D::glIsShader(CanvasShader* shader)
{
    return m_context.glIsShader(shader);
}

bool CanvasRenderingContext3D::glIsTexture(CanvasTexture* texture)
{
    return m_context.glIsTexture(texture);
}

void CanvasRenderingContext3D::glLineWidth(double width)
{
    m_context.glLineWidth((float) width);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glLinkProgram(CanvasProgram* program)
{
    if (!program)
        return;
        
    m_context.glLinkProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glPixelStorei(unsigned long pname, long param)
{
    m_context.glPixelStorei(pname, param);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glPolygonOffset(double factor, double units)
{
    m_context.glPolygonOffset((float) factor, (float) units);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glReleaseShaderCompiler()
{
    m_context.glReleaseShaderCompiler();
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glRenderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    m_context.glRenderbufferStorage(target, internalformat, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glSampleCoverage(double value, bool invert)
{
    m_context.glSampleCoverage((float) value, invert);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glScissor(long x, long y, unsigned long width, unsigned long height)
{
    m_context.glScissor(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glShaderSource(CanvasShader* shader, const String& string)
{
    m_context.glShaderSource(shader, string);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glStencilFunc(unsigned long func, long ref, unsigned long mask)
{
    m_context.glStencilFunc(func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glStencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    m_context.glStencilFuncSeparate(face, func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glStencilMask(unsigned long mask)
{
    m_context.glStencilMask(mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glStencilMaskSeparate(unsigned long face, unsigned long mask)
{
    m_context.glStencilMaskSeparate(face, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glStencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context.glStencilOp(fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glStencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context.glStencilOpSeparate(face, fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glTexParameter(unsigned target, unsigned pname, CanvasNumberArray* array)
{
    m_context.glTexParameter(target, pname,array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glTexParameter(unsigned target, unsigned pname, double value)
{
    m_context.glTexParameter(target, pname, value);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, CanvasNumberArray* array)
{
    m_context.glUniform(location, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, float v0)
{
    m_context.glUniform(location, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, float v0, float v1)
{
    m_context.glUniform(location, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, float v0, float v1, float v2)
{
    m_context.glUniform(location, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, float v0, float v1, float v2, float v3)
{
    m_context.glUniform(location, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, int v0)
{
    m_context.glUniform(location, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, int v0, int v1)
{
    m_context.glUniform(location, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, int v0, int v1, int v2)
{
    m_context.glUniform(location, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniform(long location, int v0, int v1, int v2, int v3)
{
    m_context.glUniform(location, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniformMatrix(long location, long count, bool transpose, CanvasNumberArray*array)
{
    m_context.glUniformMatrix(location, count, transpose, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniformMatrix(long location, bool transpose, const Vector<WebKitCSSMatrix*>& array)
{
    m_context.glUniformMatrix(location, transpose, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUniformMatrix(long location, bool transpose, const WebKitCSSMatrix* matrix)
{
    m_context.glUniformMatrix(location, transpose, matrix);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glUseProgram(CanvasProgram* program)
{
    m_context.glUseProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glValidateProgram(CanvasProgram* program)
{
    m_context.glValidateProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glVertexAttrib(unsigned long indx, float v0)
{
    m_context.glVertexAttrib(indx, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glVertexAttrib(unsigned long indx, float v0, float v1)
{
    m_context.glVertexAttrib(indx, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glVertexAttrib(unsigned long indx, float v0, float v1, float v2)
{
    m_context.glVertexAttrib(indx, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glVertexAttrib(unsigned long indx, float v0, float v1, float v2, float v3)
{
    m_context.glVertexAttrib(indx, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glVertexAttrib(unsigned long indx, CanvasNumberArray* array)
{
    m_context.glVertexAttrib(indx, array);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::glVertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized, unsigned long stride, CanvasNumberArray* array)
{
    m_context.glVertexAttribPointer(indx, size, type, normalized, stride, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::glViewport(long x, long y, unsigned long width, unsigned long height)
{
    if (isnan(x))
        x = 0;
    if (isnan(y))
        y = 0;
    if (isnan(width))
        width = 100;
    if (isnan(height))
        height = 100;
    m_context.glViewport(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

// Non-GL functions
PassRefPtr<CanvasBuffer> CanvasRenderingContext3D::createBuffer()
{
    return m_context.createBuffer();
}
        
PassRefPtr<CanvasFramebuffer> CanvasRenderingContext3D::createFramebuffer()
{
    return m_context.createFramebuffer();
}

PassRefPtr<CanvasTexture> CanvasRenderingContext3D::createTexture()
{
    return m_context.createTexture();
}

PassRefPtr<CanvasProgram> CanvasRenderingContext3D::createProgram()
{
    return m_context.createProgram();
}

PassRefPtr<CanvasRenderbuffer> CanvasRenderingContext3D::createRenderbuffer()
{
    return m_context.createRenderbuffer();
}

PassRefPtr<CanvasShader> CanvasRenderingContext3D::createShader(unsigned long type)
{
    return m_context.createShader(type);
}

void CanvasRenderingContext3D::deleteBuffer(CanvasBuffer* buffer)
{
    m_context.deleteBuffer(buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::deleteFramebuffer(CanvasFramebuffer* framebuffer)
{
    m_context.deleteFramebuffer(framebuffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::deleteProgram(CanvasProgram* program)
{
    m_context.deleteProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::deleteRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    m_context.deleteRenderbuffer(renderbuffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::deleteShader(CanvasShader* shader)
{
    m_context.deleteShader(shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::deleteTexture(CanvasTexture* texture)
{
    m_context.deleteTexture(texture);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::get(unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.get(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getBufferParameter(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getBufferParameter(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getFramebufferAttachmentParameter(unsigned long target, unsigned long attachment, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getFramebufferAttachmentParameter(target, attachment, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getProgram(CanvasProgram* program, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getProgram(program, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

String CanvasRenderingContext3D::glGetProgramInfoLog(CanvasProgram* program)
{
    String s = m_context.glGetProgramInfoLog(program);
    cleanupAfterGraphicsCall(false);
    return s;
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getRenderbufferParameter(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getRenderbufferParameter(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getShader(CanvasShader* shader, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getShader(shader, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

String CanvasRenderingContext3D::glGetShaderInfoLog(CanvasShader* shader)
{
    String s = m_context.glGetShaderInfoLog(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}

String CanvasRenderingContext3D::glGetShaderSource(CanvasShader* shader)
{
    String s = m_context.glGetShaderSource(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}


PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getTexParameter(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getTexParameter(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getUniform(CanvasProgram* program, long location, long size)
{
    RefPtr<CanvasNumberArray> array = m_context.getUniform(program, location, size);
    cleanupAfterGraphicsCall(false);
    return array;
}

long CanvasRenderingContext3D::getUniformLocation(CanvasProgram* program, const String& name)
{
    return m_context.getUniformLocation(program, name);
}

PassRefPtr<CanvasNumberArray> CanvasRenderingContext3D::getVertexAttrib(unsigned long index, unsigned long pname)
{
    RefPtr<CanvasNumberArray> array = m_context.getVertexAttrib(index, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}


void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, HTMLImageElement* image, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context.texImage2D(target, level, image);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context.texImage2D(target, level, canvas);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLImageElement* image, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context.texSubImage2D(target, level, xoff, yoff, width, height, image);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context.texSubImage2D(target, level, xoff, yoff, width, height, canvas);
    cleanupAfterGraphicsCall(false);
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)

