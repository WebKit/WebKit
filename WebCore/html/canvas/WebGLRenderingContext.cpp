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

#include "WebGLRenderingContext.h"

#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLShader.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "RenderBox.h"
#include "RenderLayer.h"

namespace WebCore {

PassOwnPtr<WebGLRenderingContext> WebGLRenderingContext::create(HTMLCanvasElement* canvas)
{
    OwnPtr<GraphicsContext3D> context(GraphicsContext3D::create());
    if (!context)
        return 0;
        
    return new WebGLRenderingContext(canvas, context.release());
}

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, PassOwnPtr<GraphicsContext3D> context)
    : CanvasRenderingContext(passedCanvas)
    , m_context(context)
    , m_needsUpdate(true)
    , m_markedCanvasDirty(false)
{
    ASSERT(m_context);
    m_context->reshape(canvas()->width(), canvas()->height());
}

WebGLRenderingContext::~WebGLRenderingContext()
{
    detachAndRemoveAllObjects();
}

void WebGLRenderingContext::markContextChanged()
{
#if USE(ACCELERATED_COMPOSITING)
    if (canvas()->renderBox() && canvas()->renderBox()->hasLayer()) {
        canvas()->renderBox()->layer()->rendererContentChanged();
    } else {
#endif
        if (!m_markedCanvasDirty) {
            // Make sure the canvas's image buffer is allocated.
            canvas()->buffer();
            canvas()->willDraw(FloatRect(0, 0, canvas()->width(), canvas()->height()));
            m_markedCanvasDirty = true;
        }
#if USE(ACCELERATED_COMPOSITING)
    }
#endif
}

void WebGLRenderingContext::beginPaint()
{
    if (m_markedCanvasDirty) {
        m_context->beginPaint(this);
    }
}

void WebGLRenderingContext::endPaint()
{
    if (m_markedCanvasDirty) {
        m_markedCanvasDirty = false;
        m_context->endPaint();
    }
}

void WebGLRenderingContext::reshape(int width, int height)
{
    if (m_needsUpdate) {
#if USE(ACCELERATED_COMPOSITING)
        if (canvas()->renderBox() && canvas()->renderBox()->hasLayer())
            canvas()->renderBox()->layer()->rendererContentChanged();
#endif
        m_needsUpdate = false;
    }
    
    m_context->reshape(width, height);
}

int WebGLRenderingContext::sizeInBytes(int type, ExceptionCode& ec)
{
    int result = m_context->sizeInBytes(type);
    if (result <= 0) {
        ec = SYNTAX_ERR;
    }
    return result;
}

void WebGLRenderingContext::activeTexture(unsigned long texture)
{
    m_context->activeTexture(texture);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::attachShader(WebGLProgram* program, WebGLShader* shader, ExceptionCode& ec)
{
    if (!program || program->context() != this || !shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->attachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bindAttribLocation(WebGLProgram* program, unsigned long index, const String& name, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindAttribLocation(program, index, name);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bindBuffer(unsigned long target, WebGLBuffer* buffer, ExceptionCode& ec)
{
    if (buffer && buffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindBuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::bindFramebuffer(unsigned long target, WebGLFramebuffer* buffer, ExceptionCode& ec)
{
    if (buffer && buffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindFramebuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bindRenderbuffer(unsigned long target, WebGLRenderbuffer* renderBuffer, ExceptionCode& ec)
{
    if (renderBuffer && renderBuffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindRenderbuffer(target, renderBuffer);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::bindTexture(unsigned long target, WebGLTexture* texture, ExceptionCode& ec)
{
    if (texture && texture->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindTexture(target, texture);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::blendColor(double red, double green, double blue, double alpha)
{
    m_context->blendColor(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::blendEquation( unsigned long mode )
{
    m_context->blendEquation(mode);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    m_context->blendEquationSeparate(modeRGB, modeAlpha);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::blendFunc(unsigned long sfactor, unsigned long dfactor)
{
    m_context->blendFunc(sfactor, dfactor);
    cleanupAfterGraphicsCall(false);
}       

void WebGLRenderingContext::blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    m_context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferData(unsigned long target, int size, unsigned long usage)
{
    m_context->bufferData(target, size, usage);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferData(unsigned long target, WebGLArray* data, unsigned long usage)
{
    m_context->bufferData(target, data, usage);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferSubData(unsigned long target, long offset, WebGLArray* data)
{
    m_context->bufferSubData(target, offset, data);
    cleanupAfterGraphicsCall(false);
}

unsigned long WebGLRenderingContext::checkFramebufferStatus(unsigned long target)
{
    return m_context->checkFramebufferStatus(target);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::clear(unsigned long mask)
{
    m_context->clear(mask);
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::clearColor(double r, double g, double b, double a)
{
    if (isnan(r))
        r = 0;
    if (isnan(g))
        g = 0;
    if (isnan(b))
        b = 0;
    if (isnan(a))
        a = 1;
    m_context->clearColor(r, g, b, a);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::clearDepth(double depth)
{
    m_context->clearDepth(depth);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::clearStencil(long s)
{
    m_context->clearStencil(s);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::colorMask(bool red, bool green, bool blue, bool alpha)
{
    m_context->colorMask(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::compileShader(WebGLShader* shader, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->compileShader(shader);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<WebGLBuffer> WebGLRenderingContext::createBuffer()
{
    RefPtr<WebGLBuffer> o = WebGLBuffer::create(this);
    addObject(o.get());
    return o;
}
        
PassRefPtr<WebGLFramebuffer> WebGLRenderingContext::createFramebuffer()
{
    RefPtr<WebGLFramebuffer> o = WebGLFramebuffer::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLTexture> WebGLRenderingContext::createTexture()
{
    RefPtr<WebGLTexture> o = WebGLTexture::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLProgram> WebGLRenderingContext::createProgram()
{
    RefPtr<WebGLProgram> o = WebGLProgram::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLRenderbuffer> WebGLRenderingContext::createRenderbuffer()
{
    RefPtr<WebGLRenderbuffer> o = WebGLRenderbuffer::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLShader> WebGLRenderingContext::createShader(unsigned long type)
{
    // FIXME: Need to include GL_ constants for internal use
    // FIXME: Need to do param checking and throw exception if an illegal value is passed in
    GraphicsContext3D::ShaderType shaderType = GraphicsContext3D::VERTEX_SHADER;
    if (type == 0x8B30) // GL_FRAGMENT_SHADER
        shaderType = GraphicsContext3D::FRAGMENT_SHADER;
        
    RefPtr<WebGLShader> o = WebGLShader::create(this, shaderType);
    addObject(o.get());
    return o;
}

void WebGLRenderingContext::cullFace(unsigned long mode)
{
    m_context->cullFace(mode);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::deleteBuffer(WebGLBuffer* buffer)
{
    if (!buffer)
        return;
    
    buffer->deleteObject();
}

void WebGLRenderingContext::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer)
        return;
    
    framebuffer->deleteObject();
}

void WebGLRenderingContext::deleteProgram(WebGLProgram* program)
{
    if (!program)
        return;
    
    program->deleteObject();
}

void WebGLRenderingContext::deleteRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer)
        return;
    
    renderbuffer->deleteObject();
}

void WebGLRenderingContext::deleteShader(WebGLShader* shader)
{
    if (!shader)
        return;
    
    shader->deleteObject();
}

void WebGLRenderingContext::deleteTexture(WebGLTexture* texture)
{
    if (!texture)
        return;
    
    texture->deleteObject();
}

void WebGLRenderingContext::depthFunc(unsigned long func)
{
    m_context->depthFunc(func);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::depthMask(bool flag)
{
    m_context->depthMask(flag);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::depthRange(double zNear, double zFar)
{
    m_context->depthRange(zNear, zFar);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::detachShader(WebGLProgram* program, WebGLShader* shader, ExceptionCode& ec)
{
    if (!program || program->context() != this || !shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->detachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::disable(unsigned long cap)
{
    m_context->disable(cap);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::disableVertexAttribArray(unsigned long index)
{
    m_context->disableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::drawArrays(unsigned long mode, long first, long count)
{
    m_context->drawArrays(mode, first, count);
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    m_context->drawElements(mode, count, type, offset);
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::enable(unsigned long cap)
{
    m_context->enable(cap);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::enableVertexAttribArray(unsigned long index)
{
    m_context->enableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::finish()
{
    m_context->finish();
    cleanupAfterGraphicsCall(true);
}


void WebGLRenderingContext::flush()
{
    m_context->flush();
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLRenderbuffer* buffer, ExceptionCode& ec)
{
    if (buffer && buffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }       
    m_context->framebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLTexture* texture, long level, ExceptionCode& ec)
{
    if (texture && texture->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->framebufferTexture2D(target, attachment, textarget, texture, level);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::frontFace(unsigned long mode)
{
    m_context->frontFace(mode);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::generateMipmap(unsigned long target)
{
    m_context->generateMipmap(target);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<WebGLActiveInfo> WebGLRenderingContext::getActiveAttrib(WebGLProgram* program, unsigned long index, ExceptionCode& ec)
{
    ActiveInfo info;
    if (!program || program->context() != this || !m_context->getActiveAttrib(program, index, info)) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

PassRefPtr<WebGLActiveInfo> WebGLRenderingContext::getActiveUniform(WebGLProgram* program, unsigned long index, ExceptionCode& ec)
{
    ActiveInfo info;
    if (!program || program->context() != this || !m_context->getActiveUniform(program, index, info)) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

int WebGLRenderingContext::getAttribLocation(WebGLProgram* program, const String& name)
{
    return m_context->getAttribLocation(program, name);
}

bool WebGLRenderingContext::getBoolean(unsigned long pname)
{
    bool result = m_context->getBoolean(pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLUnsignedByteArray> WebGLRenderingContext::getBooleanv(unsigned long pname)
{
    RefPtr<WebGLUnsignedByteArray> array = m_context->getBooleanv(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int WebGLRenderingContext::getBufferParameteri(unsigned long target, unsigned long pname)
{
    int result = m_context->getBufferParameteri(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getBufferParameteriv(unsigned long target, unsigned long pname)
{
    RefPtr<WebGLIntArray> array = m_context->getBufferParameteriv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

unsigned long WebGLRenderingContext::getError()
{
    return m_context->getError();
}

float WebGLRenderingContext::getFloat(unsigned long pname)
{
    float result = m_context->getFloat(pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLFloatArray> WebGLRenderingContext::getFloatv(unsigned long pname)
{
    RefPtr<WebGLFloatArray> array = m_context->getFloatv(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int WebGLRenderingContext::getFramebufferAttachmentParameteri(unsigned long target, unsigned long attachment, unsigned long pname)
{
    int result = m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname)
{
    RefPtr<WebGLIntArray> array = m_context->getFramebufferAttachmentParameteriv(target, attachment, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int WebGLRenderingContext::getInteger(unsigned long pname)
{
    float result = m_context->getInteger(pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getIntegerv(unsigned long pname)
{
    RefPtr<WebGLIntArray> array = m_context->getIntegerv(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int WebGLRenderingContext::getProgrami(WebGLProgram* program, unsigned long pname, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    int result = m_context->getProgrami(program, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getProgramiv(WebGLProgram* program, unsigned long pname, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<WebGLIntArray> array = m_context->getProgramiv(program, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

String WebGLRenderingContext::getProgramInfoLog(WebGLProgram* program, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return "";
    }
    String s = m_context->getProgramInfoLog(program);
    cleanupAfterGraphicsCall(false);
    return s;
}

int WebGLRenderingContext::getRenderbufferParameteri(unsigned long target, unsigned long pname)
{
    int result = m_context->getRenderbufferParameteri(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getRenderbufferParameteriv(unsigned long target, unsigned long pname)
{
    RefPtr<WebGLIntArray> array = m_context->getRenderbufferParameteriv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int WebGLRenderingContext::getShaderi(WebGLShader* shader, unsigned long pname, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    int result = m_context->getShaderi(shader, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getShaderiv(WebGLShader* shader, unsigned long pname, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<WebGLIntArray> array = m_context->getShaderiv(shader, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

String WebGLRenderingContext::getShaderInfoLog(WebGLShader* shader, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return "";
    }
    String s = m_context->getShaderInfoLog(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}

String WebGLRenderingContext::getShaderSource(WebGLShader* shader, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return "";
    }
    String s = m_context->getShaderSource(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}

String WebGLRenderingContext::getString(unsigned long name)
{
    return m_context->getString(name);
}

float WebGLRenderingContext::getTexParameterf(unsigned long target, unsigned long pname)
{
    float result = m_context->getTexParameterf(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLFloatArray> WebGLRenderingContext::getTexParameterfv(unsigned long target, unsigned long pname)
{
    RefPtr<WebGLFloatArray> array = m_context->getTexParameterfv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int WebGLRenderingContext::getTexParameteri(unsigned long target, unsigned long pname)
{
    int result = m_context->getTexParameteri(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getTexParameteriv(unsigned long target, unsigned long pname)
{
    RefPtr<WebGLIntArray> array = m_context->getTexParameteriv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

float WebGLRenderingContext::getUniformf(WebGLProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    float result = m_context->getUniformf(program, location);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLFloatArray> WebGLRenderingContext::getUniformfv(WebGLProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<WebGLFloatArray> array = m_context->getUniformfv(program, location);
    cleanupAfterGraphicsCall(false);
    return array;
}

long WebGLRenderingContext::getUniformi(WebGLProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    long result = m_context->getUniformi(program, location);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getUniformiv(WebGLProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<WebGLIntArray> array = m_context->getUniformiv(program, location);
    cleanupAfterGraphicsCall(false);
    return array;
}

long WebGLRenderingContext::getUniformLocation(WebGLProgram* program, const String& name, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    return m_context->getUniformLocation(program, name);
}

float WebGLRenderingContext::getVertexAttribf(unsigned long index, unsigned long pname)
{
    float result = m_context->getVertexAttribf(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLFloatArray> WebGLRenderingContext::getVertexAttribfv(unsigned long index, unsigned long pname)
{
    RefPtr<WebGLFloatArray> array = m_context->getVertexAttribfv(index, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

long WebGLRenderingContext::getVertexAttribi(unsigned long index, unsigned long pname)
{
    long result = m_context->getVertexAttribi(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<WebGLIntArray> WebGLRenderingContext::getVertexAttribiv(unsigned long index, unsigned long pname)
{
    RefPtr<WebGLIntArray> array = m_context->getVertexAttribiv(index, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

long WebGLRenderingContext::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    long result = m_context->getVertexAttribOffset(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

void WebGLRenderingContext::hint(unsigned long target, unsigned long mode)
{
    m_context->hint(target, mode);
    cleanupAfterGraphicsCall(false);
}

bool WebGLRenderingContext::isBuffer(WebGLBuffer* buffer)
{
    if (!buffer)
        return false;

    return m_context->isBuffer(buffer);
}

bool WebGLRenderingContext::isEnabled(unsigned long cap)
{
    return m_context->isEnabled(cap);
}

bool WebGLRenderingContext::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    return m_context->isFramebuffer(framebuffer);
}

bool WebGLRenderingContext::isProgram(WebGLProgram* program)
{
    return m_context->isProgram(program);
}

bool WebGLRenderingContext::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    return m_context->isRenderbuffer(renderbuffer);
}

bool WebGLRenderingContext::isShader(WebGLShader* shader)
{
    return m_context->isShader(shader);
}

bool WebGLRenderingContext::isTexture(WebGLTexture* texture)
{
    return m_context->isTexture(texture);
}

void WebGLRenderingContext::lineWidth(double width)
{
    m_context->lineWidth((float) width);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::linkProgram(WebGLProgram* program, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
        
    m_context->linkProgram(program);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::pixelStorei(unsigned long pname, long param)
{
    m_context->pixelStorei(pname, param);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::polygonOffset(double factor, double units)
{
    m_context->polygonOffset((float) factor, (float) units);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<WebGLArray> WebGLRenderingContext::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type)
{
    RefPtr<WebGLArray> array = m_context->readPixels(x, y, width, height, format, type);
    cleanupAfterGraphicsCall(false);
    return array;
}

void WebGLRenderingContext::releaseShaderCompiler()
{
    m_context->releaseShaderCompiler();
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    m_context->renderbufferStorage(target, internalformat, width, height);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::sampleCoverage(double value, bool invert)
{
    m_context->sampleCoverage((float) value, invert);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::scissor(long x, long y, unsigned long width, unsigned long height)
{
    m_context->scissor(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::shaderSource(WebGLShader* shader, const String& string, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->shaderSource(shader, string);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilFunc(unsigned long func, long ref, unsigned long mask)
{
    m_context->stencilFunc(func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    m_context->stencilFuncSeparate(face, func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilMask(unsigned long mask)
{
    m_context->stencilMask(mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilMaskSeparate(unsigned long face, unsigned long mask)
{
    m_context->stencilMaskSeparate(face, mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context->stencilOp(fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context->stencilOpSeparate(face, fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                          unsigned width, unsigned height, unsigned border,
                                          unsigned format, unsigned type, WebGLArray* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, internalformat, width, height,
                         border, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                          unsigned width, unsigned height, unsigned border,
                                          unsigned format, unsigned type, ImageData* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, internalformat, width, height,
                         border, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                                          bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    // FIXME: For now we ignore any errors returned
    m_context->texImage2D(target, level, cachedImage->image(), flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                                          bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    if (!canvas) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    ImageBuffer* buffer = canvas->buffer();
    if (!buffer)
        return;

    // FIXME: For now we ignore any errors returned
    m_context->texImage2D(target, level, buffer->image(), flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                          bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, video, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texParameterf(unsigned target, unsigned pname, float param)
{
    m_context->texParameterf(target, pname, param);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texParameteri(unsigned target, unsigned pname, int param)
{
    m_context->texParameteri(target, pname, param);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height,
                                             unsigned format, unsigned type, WebGLArray* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height,
                                             unsigned format, unsigned type, ImageData* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height, HTMLImageElement* image,
                                             bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    
    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, cachedImage->image(), flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height, HTMLCanvasElement* canvas,
                                             bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    if (!canvas) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    
    ImageBuffer* buffer = canvas->buffer();
    if (!buffer)
        return;
    
    // FIXME: For now we ignore any errors returned
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, buffer->image(), flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height, HTMLVideoElement* video,
                                             bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, video, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1f(long location, float x)
{
    m_context->uniform1f(location, x);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1fv(long location, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1fv(location, v->data(), v->length());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1fv(location, v, size);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1i(long location, int x)
{
    m_context->uniform1i(location, x);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1iv(long location, WebGLIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1iv(location, v->data(), v->length());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1iv(location, v, size);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2f(long location, float x, float y)
{
    m_context->uniform2f(location, x, y);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2fv(long location, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2fv(location, v->data(), v->length() / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2fv(location, v, size / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2i(long location, int x, int y)
{
    m_context->uniform2i(location, x, y);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2iv(long location, WebGLIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2iv(location, v->data(), v->length() / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2iv(location, v, size / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3f(long location, float x, float y, float z)
{
    m_context->uniform3f(location, x, y, z);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3fv(long location, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3fv(location, v->data(), v->length() / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3fv(location, v, size / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3i(long location, int x, int y, int z)
{
    m_context->uniform3i(location, x, y, z);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3iv(long location, WebGLIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3iv(location, v->data(), v->length() / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3iv(location, v, size / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4f(long location, float x, float y, float z, float w)
{
    m_context->uniform4f(location, x, y, z, w);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4fv(long location, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4fv(location, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4fv(location, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4i(long location, int x, int y, int z, int w)
{
    m_context->uniform4i(location, x, y, z, w);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4iv(long location, WebGLIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4iv(location, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4iv(location, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix2fv(long location, bool transpose, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniformMatrix2fv(location, transpose, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix2fv(long location, bool transpose, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniformMatrix2fv(location, transpose, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix3fv(long location, bool transpose, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 9
    m_context->uniformMatrix3fv(location, transpose, v->data(), v->length() / 9);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix3fv(long location, bool transpose, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 9
    m_context->uniformMatrix3fv(location, transpose, v, size / 9);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix4fv(long location, bool transpose, WebGLFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 16
    m_context->uniformMatrix4fv(location, transpose, v->data(), v->length() / 16);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix4fv(long location, bool transpose, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 16
    m_context->uniformMatrix4fv(location, transpose, v, size / 16);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::useProgram(WebGLProgram* program)
{
    m_context->useProgram(program);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::validateProgram(WebGLProgram* program)
{
    m_context->validateProgram(program);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib1f(unsigned long indx, float v0)
{
    m_context->vertexAttrib1f(indx, v0);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib1fv(unsigned long indx, WebGLFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib1fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib1fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib1fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib2f(unsigned long indx, float v0, float v1)
{
    m_context->vertexAttrib2f(indx, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib2fv(unsigned long indx, WebGLFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib2fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib2fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib2fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib3f(unsigned long indx, float v0, float v1, float v2)
{
    m_context->vertexAttrib3f(indx, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib3fv(unsigned long indx, WebGLFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib3fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib3fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib3fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib4f(unsigned long indx, float v0, float v1, float v2, float v3)
{
    m_context->vertexAttrib4f(indx, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib4fv(unsigned long indx, WebGLFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib4fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib4fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib4fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized, unsigned long stride, unsigned long offset)
{
    m_context->vertexAttribPointer(indx, size, type, normalized, stride, offset);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::viewport(long x, long y, unsigned long width, unsigned long height)
{
    if (isnan(x))
        x = 0;
    if (isnan(y))
        y = 0;
    if (isnan(width))
        width = 100;
    if (isnan(height))
        height = 100;
    m_context->viewport(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::removeObject(CanvasObject* object)
{
    m_canvasObjects.remove(object);
}

void WebGLRenderingContext::addObject(CanvasObject* object)
{
    removeObject(object);
    m_canvasObjects.add(object);
}

void WebGLRenderingContext::detachAndRemoveAllObjects()
{
    HashSet<CanvasObject*>::iterator pend = m_canvasObjects.end();
    for (HashSet<CanvasObject*>::iterator it = m_canvasObjects.begin(); it != pend; ++it)
        (*it)->detachContext();
        
    m_canvasObjects.clear();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)

