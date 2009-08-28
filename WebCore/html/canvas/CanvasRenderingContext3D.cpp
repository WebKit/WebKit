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
    detachAndRemoveAllObjects();
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

void CanvasRenderingContext3D::activeTexture(unsigned long texture)
{
    m_context.activeTexture(texture);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::attachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;
    m_context.attachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bindAttribLocation(CanvasProgram* program, unsigned long index, const String& name)
{
    if (!program)
        return;
    m_context.bindAttribLocation(program, index, name);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bindBuffer(unsigned long target, CanvasBuffer* buffer)
{
    m_context.bindBuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::bindFramebuffer(unsigned long target, CanvasFramebuffer* buffer)
{
    m_context.bindFramebuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bindRenderbuffer(unsigned long target, CanvasRenderbuffer* renderbuffer)
{
    m_context.bindRenderbuffer(target, renderbuffer);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::bindTexture(unsigned target, CanvasTexture* texture)
{
    m_context.bindTexture(target, texture);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::blendColor(double red, double green, double blue, double alpha)
{
    m_context.blendColor(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::blendEquation( unsigned long mode )
{
    m_context.blendEquation(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    m_context.blendEquationSeparate(modeRGB, modeAlpha);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::blendFunc(unsigned long sfactor, unsigned long dfactor)
{
    m_context.blendFunc(sfactor, dfactor);
    cleanupAfterGraphicsCall(false);
}       

void CanvasRenderingContext3D::blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    m_context.blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bufferData(unsigned long target, CanvasNumberArray* array, unsigned long usage)
{
    m_context.bufferData(target, array, usage);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bufferSubData(unsigned long target, long offset, CanvasNumberArray* array)
{
    if (!array || !array->data().size())
        return;
        
    m_context.bufferSubData(target, offset, array);
    cleanupAfterGraphicsCall(false);
}

unsigned long CanvasRenderingContext3D::checkFramebufferStatus(CanvasFramebuffer* framebuffer)
{
    return m_context.checkFramebufferStatus(framebuffer);
}

void CanvasRenderingContext3D::clearColor(double r, double g, double b, double a)
{
    if (isnan(r))
        r = 0;
    if (isnan(g))
        g = 0;
    if (isnan(b))
        b = 0;
    if (isnan(a))
        a = 1;
    m_context.clearColor(r, g, b, a);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::clear(unsigned long mask)
{
    m_context.clear(mask);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::clearDepth(double depth)
{
    m_context.clearDepth(depth);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::clearStencil(long s)
{
    m_context.clearStencil(s);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::colorMask(bool red, bool green, bool blue, bool alpha)
{
    m_context.colorMask(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::compileShader(CanvasShader* shader)
{
    m_context.compileShader(shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    m_context.copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    m_context.copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::cullFace(unsigned long mode)
{
    m_context.cullFace(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::depthFunc(unsigned long func)
{
    m_context.depthFunc(func);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::depthMask(bool flag)
{
    m_context.depthMask(flag);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::depthRange(double zNear, double zFar)
{
    m_context.depthRange(zNear, zFar);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::detachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;

    m_context.detachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::disable(unsigned long cap)
{
    m_context.disable(cap);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::disableVertexAttribArray(unsigned long index)
{
    m_context.disableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::drawArrays(unsigned long mode, long first, unsigned long count)
{
    m_context.drawArrays(mode, first, count);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::drawElements(unsigned long mode, unsigned long count, unsigned long type, void* array)
{
    m_context.drawElements(mode, count, type, array);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::enable(unsigned long cap)
{
    m_context.enable(cap);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::enableVertexAttribArray(unsigned long index)
{
    m_context.enableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::finish()
{
    m_context.finish();
    cleanupAfterGraphicsCall(true);
}


void CanvasRenderingContext3D::flush()
{
    m_context.flush();
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer* buffer)
{
    if (!buffer)
        return;
        
    m_context.framebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture* texture, long level)
{
    if (!texture)
        return;
        
    m_context.framebufferTexture2D(target, attachment, textarget, texture, level);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::frontFace(unsigned long mode)
{
    m_context.frontFace(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::generateMipmap(unsigned long target)
{
    m_context.generateMipmap(target);
    cleanupAfterGraphicsCall(false);
}

int CanvasRenderingContext3D::getAttribLocation(CanvasProgram* program, const String& name)
{
    return m_context.getAttribLocation(program, name);
}

unsigned long CanvasRenderingContext3D::getError()
{
    return m_context.getError();
}

String CanvasRenderingContext3D::getString(unsigned long name)
{
    return m_context.getString(name);
}

void CanvasRenderingContext3D::hint(unsigned long target, unsigned long mode)
{
    m_context.hint(target, mode);
    cleanupAfterGraphicsCall(false);
}

bool CanvasRenderingContext3D::isBuffer(CanvasBuffer* buffer)
{
    if (!buffer)
        return false;
        
    return m_context.isBuffer(buffer);
}

bool CanvasRenderingContext3D::isEnabled(unsigned long cap)
{
    return m_context.isEnabled(cap);
}

bool CanvasRenderingContext3D::isFramebuffer(CanvasFramebuffer* framebuffer)
{
    return m_context.isFramebuffer(framebuffer);
}

bool CanvasRenderingContext3D::isProgram(CanvasProgram* program)
{
    return m_context.isProgram(program);
}

bool CanvasRenderingContext3D::isRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    return m_context.isRenderbuffer(renderbuffer);
}

bool CanvasRenderingContext3D::isShader(CanvasShader* shader)
{
    return m_context.isShader(shader);
}

bool CanvasRenderingContext3D::isTexture(CanvasTexture* texture)
{
    return m_context.isTexture(texture);
}

void CanvasRenderingContext3D::lineWidth(double width)
{
    m_context.lineWidth((float) width);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::linkProgram(CanvasProgram* program)
{
    if (!program)
        return;
        
    m_context.linkProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::pixelStorei(unsigned long pname, long param)
{
    m_context.pixelStorei(pname, param);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::polygonOffset(double factor, double units)
{
    m_context.polygonOffset((float) factor, (float) units);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::releaseShaderCompiler()
{
    m_context.releaseShaderCompiler();
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    m_context.renderbufferStorage(target, internalformat, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::sampleCoverage(double value, bool invert)
{
    m_context.sampleCoverage((float) value, invert);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::scissor(long x, long y, unsigned long width, unsigned long height)
{
    m_context.scissor(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::shaderSource(CanvasShader* shader, const String& string)
{
    m_context.shaderSource(shader, string);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilFunc(unsigned long func, long ref, unsigned long mask)
{
    m_context.stencilFunc(func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    m_context.stencilFuncSeparate(face, func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilMask(unsigned long mask)
{
    m_context.stencilMask(mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilMaskSeparate(unsigned long face, unsigned long mask)
{
    m_context.stencilMaskSeparate(face, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context.stencilOp(fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context.stencilOpSeparate(face, fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::texParameter(unsigned target, unsigned pname, CanvasNumberArray* array)
{
    m_context.texParameter(target, pname,array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texParameter(unsigned target, unsigned pname, double value)
{
    m_context.texParameter(target, pname, value);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, CanvasNumberArray* array)
{
    m_context.uniform(location, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, float v0)
{
    m_context.uniform(location, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, float v0, float v1)
{
    m_context.uniform(location, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, float v0, float v1, float v2)
{
    m_context.uniform(location, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, float v0, float v1, float v2, float v3)
{
    m_context.uniform(location, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, int v0)
{
    m_context.uniform(location, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, int v0, int v1)
{
    m_context.uniform(location, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, int v0, int v1, int v2)
{
    m_context.uniform(location, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform(long location, int v0, int v1, int v2, int v3)
{
    m_context.uniform(location, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix(long location, long count, bool transpose, CanvasNumberArray*array)
{
    m_context.uniformMatrix(location, count, transpose, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix(long location, bool transpose, const Vector<WebKitCSSMatrix*>& array)
{
    m_context.uniformMatrix(location, transpose, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix(long location, bool transpose, const WebKitCSSMatrix* matrix)
{
    m_context.uniformMatrix(location, transpose, matrix);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::useProgram(CanvasProgram* program)
{
    m_context.useProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::validateProgram(CanvasProgram* program)
{
    m_context.validateProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib(unsigned long indx, float v0)
{
    m_context.vertexAttrib(indx, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib(unsigned long indx, float v0, float v1)
{
    m_context.vertexAttrib(indx, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib(unsigned long indx, float v0, float v1, float v2)
{
    m_context.vertexAttrib(indx, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib(unsigned long indx, float v0, float v1, float v2, float v3)
{
    m_context.vertexAttrib(indx, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib(unsigned long indx, CanvasNumberArray* array)
{
    m_context.vertexAttrib(indx, array);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::vertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized, unsigned long stride, CanvasNumberArray* array)
{
    m_context.vertexAttribPointer(indx, size, type, normalized, stride, array);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::viewport(long x, long y, unsigned long width, unsigned long height)
{
    if (isnan(x))
        x = 0;
    if (isnan(y))
        y = 0;
    if (isnan(width))
        width = 100;
    if (isnan(height))
        height = 100;
    m_context.viewport(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

// Non-GL functions
PassRefPtr<CanvasBuffer> CanvasRenderingContext3D::createBuffer()
{
    RefPtr<CanvasBuffer> o = CanvasBuffer::create(this);
    addObject(o.get());
    return o;
}
        
PassRefPtr<CanvasFramebuffer> CanvasRenderingContext3D::createFramebuffer()
{
    RefPtr<CanvasFramebuffer> o = CanvasFramebuffer::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<CanvasTexture> CanvasRenderingContext3D::createTexture()
{
    RefPtr<CanvasTexture> o = CanvasTexture::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<CanvasProgram> CanvasRenderingContext3D::createProgram()
{
    RefPtr<CanvasProgram> o = CanvasProgram::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<CanvasRenderbuffer> CanvasRenderingContext3D::createRenderbuffer()
{
    RefPtr<CanvasRenderbuffer> o = CanvasRenderbuffer::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<CanvasShader> CanvasRenderingContext3D::createShader(unsigned long type)
{
    // FIXME: Need to include GL_ constants for internal use
    // FIXME: Need to do param checking and throw exception if an illegal value is passed in
    GraphicsContext3D::ShaderType shaderType = GraphicsContext3D::VERTEX_SHADER;
    if (type == 0x8B30) // GL_FRAGMENT_SHADER
        shaderType = GraphicsContext3D::FRAGMENT_SHADER;
        
    RefPtr<CanvasShader> o = CanvasShader::create(this, shaderType);
    addObject(o.get());
    return o;
}

void CanvasRenderingContext3D::deleteBuffer(CanvasBuffer* buffer)
{
    if (!buffer)
        return;
    
    buffer->deleteObject();
}

void CanvasRenderingContext3D::deleteFramebuffer(CanvasFramebuffer* framebuffer)
{
    if (!framebuffer)
        return;
    
    framebuffer->deleteObject();
}

void CanvasRenderingContext3D::deleteProgram(CanvasProgram* program)
{
    if (!program)
        return;
    
    program->deleteObject();
}

void CanvasRenderingContext3D::deleteRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    if (!renderbuffer)
        return;
    
    renderbuffer->deleteObject();
}

void CanvasRenderingContext3D::deleteShader(CanvasShader* shader)
{
    if (!shader)
        return;
    
    shader->deleteObject();
}

void CanvasRenderingContext3D::deleteTexture(CanvasTexture* texture)
{
    if (!texture)
        return;
    
    texture->deleteObject();
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

String CanvasRenderingContext3D::getProgramInfoLog(CanvasProgram* program)
{
    String s = m_context.getProgramInfoLog(program);
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

String CanvasRenderingContext3D::getShaderInfoLog(CanvasShader* shader)
{
    String s = m_context.getShaderInfoLog(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}

String CanvasRenderingContext3D::getShaderSource(CanvasShader* shader)
{
    String s = m_context.getShaderSource(shader);
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

void CanvasRenderingContext3D::removeObject(CanvasObject* object)
{
    m_canvasObjects.remove(object);
}

void CanvasRenderingContext3D::addObject(CanvasObject* object)
{
    removeObject(object);
    m_canvasObjects.add(object);
}

void CanvasRenderingContext3D::detachAndRemoveAllObjects()
{
    HashSet<CanvasObject*>::iterator pend = m_canvasObjects.end();
    for (HashSet<CanvasObject*>::iterator it = m_canvasObjects.begin(); it != pend; ++it)
        (*it)->detachContext();
        
    m_canvasObjects.clear();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)

