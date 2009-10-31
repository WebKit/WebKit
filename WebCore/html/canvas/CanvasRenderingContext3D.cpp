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

#include "CanvasActiveInfo.h"
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

PassOwnPtr<CanvasRenderingContext3D> CanvasRenderingContext3D::create(HTMLCanvasElement* canvas)
{
    OwnPtr<GraphicsContext3D> context(GraphicsContext3D::create());
    if (!context)
        return 0;
        
    return new CanvasRenderingContext3D(canvas, context.release());
}

CanvasRenderingContext3D::CanvasRenderingContext3D(HTMLCanvasElement* passedCanvas, PassOwnPtr<GraphicsContext3D> context)
    : CanvasRenderingContext(passedCanvas)
    , m_context(context)
    , m_needsUpdate(true)
    , m_markedCanvasDirty(false)
{
    ASSERT(m_context);
    m_context->reshape(canvas()->width(), canvas()->height());
}

CanvasRenderingContext3D::~CanvasRenderingContext3D()
{
    detachAndRemoveAllObjects();
}

void CanvasRenderingContext3D::markContextChanged()
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

void CanvasRenderingContext3D::beginPaint()
{
    if (m_markedCanvasDirty) {
        m_context->beginPaint(this);
    }
}

void CanvasRenderingContext3D::endPaint()
{
    if (m_markedCanvasDirty) {
        m_markedCanvasDirty = false;
        m_context->endPaint();
    }
}

void CanvasRenderingContext3D::reshape(int width, int height)
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

int CanvasRenderingContext3D::sizeInBytes(int type, ExceptionCode& ec)
{
    int result = m_context->sizeInBytes(type);
    if (result <= 0) {
        ec = SYNTAX_ERR;
    }
    return result;
}

void CanvasRenderingContext3D::activeTexture(unsigned long texture)
{
    m_context->activeTexture(texture);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::attachShader(CanvasProgram* program, CanvasShader* shader, ExceptionCode& ec)
{
    if (!program || program->context() != this || !shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->attachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bindAttribLocation(CanvasProgram* program, unsigned long index, const String& name, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindAttribLocation(program, index, name);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bindBuffer(unsigned long target, CanvasBuffer* buffer, ExceptionCode& ec)
{
    if (!buffer || buffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindBuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::bindFramebuffer(unsigned long target, CanvasFramebuffer* buffer, ExceptionCode& ec)
{
    if (!buffer || buffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindFramebuffer(target, buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bindRenderbuffer(unsigned long target, CanvasRenderbuffer* renderBuffer, ExceptionCode& ec)
{
    if (!renderBuffer || renderBuffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindRenderbuffer(target, renderBuffer);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::bindTexture(unsigned long target, CanvasTexture* texture, ExceptionCode& ec)
{
    if (!texture || texture->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->bindTexture(target, texture);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::blendColor(double red, double green, double blue, double alpha)
{
    m_context->blendColor(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::blendEquation( unsigned long mode )
{
    m_context->blendEquation(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    m_context->blendEquationSeparate(modeRGB, modeAlpha);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::blendFunc(unsigned long sfactor, unsigned long dfactor)
{
    m_context->blendFunc(sfactor, dfactor);
    cleanupAfterGraphicsCall(false);
}       

void CanvasRenderingContext3D::blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    m_context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bufferData(unsigned long target, int size, unsigned long usage)
{
    m_context->bufferData(target, size, usage);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bufferData(unsigned long target, CanvasArray* data, unsigned long usage)
{
    m_context->bufferData(target, data, usage);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::bufferSubData(unsigned long target, long offset, CanvasArray* data)
{
    m_context->bufferSubData(target, offset, data);
    cleanupAfterGraphicsCall(false);
}

unsigned long CanvasRenderingContext3D::checkFramebufferStatus(unsigned long target)
{
    return m_context->checkFramebufferStatus(target);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::clear(unsigned long mask)
{
    m_context->clear(mask);
    cleanupAfterGraphicsCall(true);
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
    m_context->clearColor(r, g, b, a);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::clearDepth(double depth)
{
    m_context->clearDepth(depth);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::clearStencil(long s)
{
    m_context->clearStencil(s);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::colorMask(bool red, bool green, bool blue, bool alpha)
{
    m_context->colorMask(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::compileShader(CanvasShader* shader, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->compileShader(shader);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

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

void CanvasRenderingContext3D::cullFace(unsigned long mode)
{
    m_context->cullFace(mode);
    cleanupAfterGraphicsCall(false);
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

void CanvasRenderingContext3D::depthFunc(unsigned long func)
{
    m_context->depthFunc(func);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::depthMask(bool flag)
{
    m_context->depthMask(flag);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::depthRange(double zNear, double zFar)
{
    m_context->depthRange(zNear, zFar);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::detachShader(CanvasProgram* program, CanvasShader* shader, ExceptionCode& ec)
{
    if (!program || program->context() != this || !shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->detachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::disable(unsigned long cap)
{
    m_context->disable(cap);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::disableVertexAttribArray(unsigned long index)
{
    m_context->disableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}


void CanvasRenderingContext3D::drawArrays(unsigned long mode, long first, long count)
{
    m_context->drawArrays(mode, first, count);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    m_context->drawElements(mode, count, type, offset);
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::enable(unsigned long cap)
{
    m_context->enable(cap);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::enableVertexAttribArray(unsigned long index)
{
    m_context->enableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::finish()
{
    m_context->finish();
    cleanupAfterGraphicsCall(true);
}


void CanvasRenderingContext3D::flush()
{
    m_context->flush();
    cleanupAfterGraphicsCall(true);
}

void CanvasRenderingContext3D::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer* buffer, ExceptionCode& ec)
{
    if (!buffer || buffer->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }       
    m_context->framebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture* texture, long level, ExceptionCode& ec)
{
    if (!texture || texture->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->framebufferTexture2D(target, attachment, textarget, texture, level);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::frontFace(unsigned long mode)
{
    m_context->frontFace(mode);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::generateMipmap(unsigned long target)
{
    m_context->generateMipmap(target);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<CanvasActiveInfo> CanvasRenderingContext3D::getActiveAttrib(CanvasProgram* program, unsigned long index, ExceptionCode& ec)
{
    ActiveInfo info;
    if (!program || program->context() != this || !m_context->getActiveAttrib(program, index, info)) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    return CanvasActiveInfo::create(info.name, info.type, info.size);
}

PassRefPtr<CanvasActiveInfo> CanvasRenderingContext3D::getActiveUniform(CanvasProgram* program, unsigned long index, ExceptionCode& ec)
{
    ActiveInfo info;
    if (!program || program->context() != this || !m_context->getActiveUniform(program, index, info)) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    return CanvasActiveInfo::create(info.name, info.type, info.size);
}

int CanvasRenderingContext3D::getAttribLocation(CanvasProgram* program, const String& name)
{
    return m_context->getAttribLocation(program, name);
}

bool CanvasRenderingContext3D::getBoolean(unsigned long pname)
{
    bool result = m_context->getBoolean(pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasUnsignedByteArray> CanvasRenderingContext3D::getBooleanv(unsigned long pname)
{
    RefPtr<CanvasUnsignedByteArray> array = m_context->getBooleanv(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int CanvasRenderingContext3D::getBufferParameteri(unsigned long target, unsigned long pname)
{
    int result = m_context->getBufferParameteri(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getBufferParameteriv(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasIntArray> array = m_context->getBufferParameteriv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

unsigned long CanvasRenderingContext3D::getError()
{
    return m_context->getError();
}

float CanvasRenderingContext3D::getFloat(unsigned long pname)
{
    float result = m_context->getFloat(pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasFloatArray> CanvasRenderingContext3D::getFloatv(unsigned long pname)
{
    RefPtr<CanvasFloatArray> array = m_context->getFloatv(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int CanvasRenderingContext3D::getFramebufferAttachmentParameteri(unsigned long target, unsigned long attachment, unsigned long pname)
{
    int result = m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname)
{
    RefPtr<CanvasIntArray> array = m_context->getFramebufferAttachmentParameteriv(target, attachment, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int CanvasRenderingContext3D::getInteger(unsigned long pname)
{
    float result = m_context->getInteger(pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getIntegerv(unsigned long pname)
{
    RefPtr<CanvasIntArray> array = m_context->getIntegerv(pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int CanvasRenderingContext3D::getProgrami(CanvasProgram* program, unsigned long pname, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    int result = m_context->getProgrami(program, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getProgramiv(CanvasProgram* program, unsigned long pname, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<CanvasIntArray> array = m_context->getProgramiv(program, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

String CanvasRenderingContext3D::getProgramInfoLog(CanvasProgram* program, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return "";
    }
    String s = m_context->getProgramInfoLog(program);
    cleanupAfterGraphicsCall(false);
    return s;
}

int CanvasRenderingContext3D::getRenderbufferParameteri(unsigned long target, unsigned long pname)
{
    int result = m_context->getRenderbufferParameteri(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getRenderbufferParameteriv(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasIntArray> array = m_context->getRenderbufferParameteriv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int CanvasRenderingContext3D::getShaderi(CanvasShader* shader, unsigned long pname, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    int result = m_context->getShaderi(shader, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getShaderiv(CanvasShader* shader, unsigned long pname, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<CanvasIntArray> array = m_context->getShaderiv(shader, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

String CanvasRenderingContext3D::getShaderInfoLog(CanvasShader* shader, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return "";
    }
    String s = m_context->getShaderInfoLog(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}

String CanvasRenderingContext3D::getShaderSource(CanvasShader* shader, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return "";
    }
    String s = m_context->getShaderSource(shader);
    cleanupAfterGraphicsCall(false);
    return s;
}

String CanvasRenderingContext3D::getString(unsigned long name)
{
    return m_context->getString(name);
}

float CanvasRenderingContext3D::getTexParameterf(unsigned long target, unsigned long pname)
{
    float result = m_context->getTexParameterf(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasFloatArray> CanvasRenderingContext3D::getTexParameterfv(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasFloatArray> array = m_context->getTexParameterfv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

int CanvasRenderingContext3D::getTexParameteri(unsigned long target, unsigned long pname)
{
    int result = m_context->getTexParameteri(target, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getTexParameteriv(unsigned long target, unsigned long pname)
{
    RefPtr<CanvasIntArray> array = m_context->getTexParameteriv(target, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

float CanvasRenderingContext3D::getUniformf(CanvasProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    float result = m_context->getUniformf(program, location);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasFloatArray> CanvasRenderingContext3D::getUniformfv(CanvasProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<CanvasFloatArray> array = m_context->getUniformfv(program, location);
    cleanupAfterGraphicsCall(false);
    return array;
}

long CanvasRenderingContext3D::getUniformi(CanvasProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    long result = m_context->getUniformi(program, location);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getUniformiv(CanvasProgram* program, long location, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    RefPtr<CanvasIntArray> array = m_context->getUniformiv(program, location);
    cleanupAfterGraphicsCall(false);
    return array;
}

long CanvasRenderingContext3D::getUniformLocation(CanvasProgram* program, const String& name, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    return m_context->getUniformLocation(program, name);
}

float CanvasRenderingContext3D::getVertexAttribf(unsigned long index, unsigned long pname)
{
    float result = m_context->getVertexAttribf(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasFloatArray> CanvasRenderingContext3D::getVertexAttribfv(unsigned long index, unsigned long pname)
{
    RefPtr<CanvasFloatArray> array = m_context->getVertexAttribfv(index, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

long CanvasRenderingContext3D::getVertexAttribi(unsigned long index, unsigned long pname)
{
    long result = m_context->getVertexAttribi(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

PassRefPtr<CanvasIntArray> CanvasRenderingContext3D::getVertexAttribiv(unsigned long index, unsigned long pname)
{
    RefPtr<CanvasIntArray> array = m_context->getVertexAttribiv(index, pname);
    cleanupAfterGraphicsCall(false);
    return array;
}

long CanvasRenderingContext3D::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    long result = m_context->getVertexAttribOffset(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

void CanvasRenderingContext3D::hint(unsigned long target, unsigned long mode)
{
    m_context->hint(target, mode);
    cleanupAfterGraphicsCall(false);
}

bool CanvasRenderingContext3D::isBuffer(CanvasBuffer* buffer)
{
    if (!buffer)
        return false;

    return m_context->isBuffer(buffer);
}

bool CanvasRenderingContext3D::isEnabled(unsigned long cap)
{
    return m_context->isEnabled(cap);
}

bool CanvasRenderingContext3D::isFramebuffer(CanvasFramebuffer* framebuffer)
{
    return m_context->isFramebuffer(framebuffer);
}

bool CanvasRenderingContext3D::isProgram(CanvasProgram* program)
{
    return m_context->isProgram(program);
}

bool CanvasRenderingContext3D::isRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    return m_context->isRenderbuffer(renderbuffer);
}

bool CanvasRenderingContext3D::isShader(CanvasShader* shader)
{
    return m_context->isShader(shader);
}

bool CanvasRenderingContext3D::isTexture(CanvasTexture* texture)
{
    return m_context->isTexture(texture);
}

void CanvasRenderingContext3D::lineWidth(double width)
{
    m_context->lineWidth((float) width);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::linkProgram(CanvasProgram* program, ExceptionCode& ec)
{
    if (!program || program->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
        
    m_context->linkProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::pixelStorei(unsigned long pname, long param)
{
    m_context->pixelStorei(pname, param);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::polygonOffset(double factor, double units)
{
    m_context->polygonOffset((float) factor, (float) units);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<CanvasArray> CanvasRenderingContext3D::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type)
{
    RefPtr<CanvasArray> array = m_context->readPixels(x, y, width, height, format, type);
    cleanupAfterGraphicsCall(false);
    return array;
}

void CanvasRenderingContext3D::releaseShaderCompiler()
{
    m_context->releaseShaderCompiler();
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    m_context->renderbufferStorage(target, internalformat, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::sampleCoverage(double value, bool invert)
{
    m_context->sampleCoverage((float) value, invert);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::scissor(long x, long y, unsigned long width, unsigned long height)
{
    m_context->scissor(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::shaderSource(CanvasShader* shader, const String& string, ExceptionCode& ec)
{
    if (!shader || shader->context() != this) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    m_context->shaderSource(shader, string);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilFunc(unsigned long func, long ref, unsigned long mask)
{
    m_context->stencilFunc(func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    m_context->stencilFuncSeparate(face, func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilMask(unsigned long mask)
{
    m_context->stencilMask(mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilMaskSeparate(unsigned long face, unsigned long mask)
{
    m_context->stencilMaskSeparate(face, mask);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context->stencilOp(fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context->stencilOpSeparate(face, fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                          unsigned width, unsigned height, unsigned border,
                                          unsigned format, unsigned type, CanvasArray* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, internalformat, width, height,
                         border, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                          unsigned width, unsigned height, unsigned border,
                                          unsigned format, unsigned type, ImageData* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, internalformat, width, height,
                         border, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                                          bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, image, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                                          bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, canvas, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                          bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texImage2D(target, level, video, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texParameterf(unsigned target, unsigned pname, float param)
{
    m_context->texParameterf(target, pname, param);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texParameteri(unsigned target, unsigned pname, int param)
{
    m_context->texParameteri(target, pname, param);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height,
                                             unsigned format, unsigned type, CanvasArray* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height,
                                             unsigned format, unsigned type, ImageData* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height, HTMLImageElement* image,
                                             bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, image, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height, HTMLCanvasElement* canvas,
                                             bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, canvas, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                             unsigned width, unsigned height, HTMLVideoElement* video,
                                             bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, video, flipY, premultiplyAlpha);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform1f(long location, float x)
{
    m_context->uniform1f(location, x);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform1fv(long location, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1fv(location, v->data(), v->length());
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform1fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1fv(location, v, size);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform1i(long location, int x)
{
    m_context->uniform1i(location, x);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform1iv(long location, CanvasIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1iv(location, v->data(), v->length());
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform1iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    m_context->uniform1iv(location, v, size);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform2f(long location, float x, float y)
{
    m_context->uniform2f(location, x, y);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform2fv(long location, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2fv(location, v->data(), v->length() / 2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform2fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2fv(location, v, size / 2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform2i(long location, int x, int y)
{
    m_context->uniform2i(location, x, y);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform2iv(long location, CanvasIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2iv(location, v->data(), v->length() / 2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform2iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 2
    m_context->uniform2iv(location, v, size / 2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform3f(long location, float x, float y, float z)
{
    m_context->uniform3f(location, x, y, z);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform3fv(long location, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3fv(location, v->data(), v->length() / 3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform3fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3fv(location, v, size / 3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform3i(long location, int x, int y, int z)
{
    m_context->uniform3i(location, x, y, z);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform3iv(long location, CanvasIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3iv(location, v->data(), v->length() / 3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform3iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 3
    m_context->uniform3iv(location, v, size / 3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform4f(long location, float x, float y, float z, float w)
{
    m_context->uniform4f(location, x, y, z, w);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform4fv(long location, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4fv(location, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform4fv(long location, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4fv(location, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform4i(long location, int x, int y, int z, int w)
{
    m_context->uniform4i(location, x, y, z, w);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform4iv(long location, CanvasIntArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4iv(location, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniform4iv(long location, int* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniform4iv(location, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix2fv(long location, bool transpose, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniformMatrix2fv(location, transpose, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix2fv(long location, bool transpose, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 4
    m_context->uniformMatrix2fv(location, transpose, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix3fv(long location, bool transpose, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 9
    m_context->uniformMatrix3fv(location, transpose, v->data(), v->length() / 9);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix3fv(long location, bool transpose, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 9
    m_context->uniformMatrix3fv(location, transpose, v, size / 9);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix4fv(long location, bool transpose, CanvasFloatArray* v)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 16
    m_context->uniformMatrix4fv(location, transpose, v->data(), v->length() / 16);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::uniformMatrix4fv(long location, bool transpose, float* v, int size)
{
    // FIXME: we need to throw if no array passed in
    if (!v)
        return;
        
    // FIXME: length needs to be a multiple of 16
    m_context->uniformMatrix4fv(location, transpose, v, size / 16);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::useProgram(CanvasProgram* program)
{
    m_context->useProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::validateProgram(CanvasProgram* program)
{
    m_context->validateProgram(program);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib1f(unsigned long indx, float v0)
{
    m_context->vertexAttrib1f(indx, v0);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib1fv(unsigned long indx, CanvasFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib1fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib1fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib1fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib2f(unsigned long indx, float v0, float v1)
{
    m_context->vertexAttrib2f(indx, v0, v1);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib2fv(unsigned long indx, CanvasFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib2fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib2fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib2fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib3f(unsigned long indx, float v0, float v1, float v2)
{
    m_context->vertexAttrib3f(indx, v0, v1, v2);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib3fv(unsigned long indx, CanvasFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib3fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib3fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib3fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib4f(unsigned long indx, float v0, float v1, float v2, float v3)
{
    m_context->vertexAttrib4f(indx, v0, v1, v2, v3);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib4fv(unsigned long indx, CanvasFloatArray* v)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    m_context->vertexAttrib4fv(indx, v->data());
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttrib4fv(unsigned long indx, float* v, int size)
{
    // FIXME: Need to make sure array is big enough for attribute being set
    UNUSED_PARAM(size);
    
    m_context->vertexAttrib4fv(indx, v);
    cleanupAfterGraphicsCall(false);
}

void CanvasRenderingContext3D::vertexAttribPointer(unsigned long indx, long size, unsigned long type, bool normalized, unsigned long stride, unsigned long offset)
{
    m_context->vertexAttribPointer(indx, size, type, normalized, stride, offset);
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
    m_context->viewport(x, y, width, height);
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

