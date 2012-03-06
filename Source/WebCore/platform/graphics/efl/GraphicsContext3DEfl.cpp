/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(WEBGL) || USE(ACCELERATED_COMPOSITING)

#include "GraphicsContext3DPrivate.h"

#include "ImageData.h"
#include "NotImplemented.h"

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, RenderStyle renderStyle)
{
    bool renderDirectlyToEvasGLObject = (renderStyle == RenderDirectlyToHostWindow);

    OwnPtr<GraphicsContext3DPrivate> internal = GraphicsContext3DPrivate::create(attrs, hostWindow, renderDirectlyToEvasGLObject);
    if (!internal)
        return 0;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderDirectlyToEvasGLObject));
    context->m_private = internal.release();
    return context.release();
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_private->platformGraphicsContext3D();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    notImplemented();
    return 0;
}
#endif

bool GraphicsContext3D::makeContextCurrent()
{
    return m_private->makeContextCurrent();
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_private->isGLES2Compliant();
}

void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    m_private->activeTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    m_private->attachShader(program, shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    m_private->bindAttribLocation(program, index, name);
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    m_private->bindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    m_private->bindFramebuffer(target, buffer);
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    m_private->bindRenderbuffer(target, renderbuffer);
}

void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    m_private->bindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    m_private->blendColor(red, green, blue, alpha);
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    m_private->blendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    m_private->blendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3D::blendFunc(GC3Denum srcFactor, GC3Denum dstFactor)
{
    m_private->blendFunc(srcFactor, dstFactor);
}

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    m_private->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    m_private->bufferData(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    m_private->bufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    m_private->bufferSubData(target, offset, size, data);
}

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    return m_private->checkFramebufferStatus(target);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    m_private->clear(mask);
}

void GraphicsContext3D::clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    m_private->clearColor(red, green, blue, alpha);
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    m_private->clearDepth(depth);
}

void GraphicsContext3D::clearStencil(GC3Dint clearValue)
{
    m_private->clearStencil(clearValue);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    m_private->colorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    m_private->compileShader(shader);
}

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    m_private->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    m_private->copyTexSubImage2D(target, level, xOffset, yOffset, x, y, width, height);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    m_private->cullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    m_private->depthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    m_private->depthMask(flag);
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    m_private->depthRange(zNear, zFar);
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    m_private->detachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    m_private->disable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    m_private->disableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    m_private->drawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    m_private->drawElements(mode, count, type, offset);
}

void GraphicsContext3D::enable(GC3Denum cap)
{
    m_private->enable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    m_private->enableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    m_private->finish();
}

void GraphicsContext3D::flush()
{
    m_private->flush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbufferTarget, Platform3DObject buffer)
{
    m_private->framebufferRenderbuffer(target, attachment, renderbufferTarget, buffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum texTarget, Platform3DObject texture, GC3Dint level)
{
    m_private->framebufferTexture2D(target, attachment, texTarget, texture, level);
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    m_private->frontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    m_private->generateMipmap(target);
}

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    return m_private->getActiveAttrib(program, index, info);
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    return m_private->getActiveUniform(program, index, info);
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
{
    m_private->getAttachedShaders(program, maxCount, count, shaders);
}

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_private->getAttribLocation(program, name);
}

void GraphicsContext3D::getBooleanv(GC3Denum paramName, GC3Dboolean* value)
{
    m_private->getBooleanv(paramName, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    m_private->getBufferParameteriv(target, paramName, value);
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    return m_private->getContextAttributes();
}

GC3Denum GraphicsContext3D::getError()
{
    return m_private->getError();
}

void GraphicsContext3D::getFloatv(GC3Denum paramName, GC3Dfloat* value)
{
    m_private->getFloatv(paramName, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum paramName, GC3Dint* value)
{
    m_private->getFramebufferAttachmentParameteriv(target, attachment, paramName, value);
}

void GraphicsContext3D::getIntegerv(GC3Denum paramName, GC3Dint* value)
{
    m_private->getIntegerv(paramName, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum paramName, GC3Dint* value)
{
    m_private->getProgramiv(program, paramName, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    return m_private->getProgramInfoLog(program);
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    m_private->getRenderbufferParameteriv(target, paramName, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum paramName, GC3Dint* value)
{
    m_private->getShaderiv(shader, paramName, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    return m_private->getShaderInfoLog(shader);
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    return m_private->getShaderSource(shader);
}

String GraphicsContext3D::getString(GC3Denum name)
{
    return m_private->getString(name);
}

void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum paramName, GC3Dfloat* value)
{
    m_private->getTexParameterfv(target, paramName, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    m_private->getTexParameteriv(target, paramName, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    m_private->getUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    m_private->getUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_private->getUniformLocation(program, name);
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum paramName, GC3Dfloat* value)
{
    m_private->getVertexAttribfv(index, paramName, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum paramName, GC3Dint* value)
{
    m_private->getVertexAttribiv(index, paramName, value);
}

long GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum paramName)
{
    return m_private->getVertexAttribOffset(index, paramName);
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    m_private->hint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject obj)
{
    return m_private->isBuffer(obj);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    return m_private->isEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject obj)
{
    return m_private->isFramebuffer(obj);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject obj)
{
    return m_private->isProgram(obj);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject obj)
{
    return m_private->isRenderbuffer(obj);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject obj)
{
    return m_private->isShader(obj);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject obj)
{
    return m_private->isTexture(obj);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    m_private->lineWidth(width);
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    m_private->linkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum paramName, GC3Dint param)
{
    m_private->pixelStorei(paramName, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    m_private->polygonOffset(factor, units);
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    m_private->readPixels(x, y, width, height, format, type, data);
}

void GraphicsContext3D::releaseShaderCompiler()
{
    notImplemented();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    m_private->renderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    m_private->sampleCoverage(value, invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    m_private->scissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject program, const String& string)
{
    m_private->shaderSource(program, string);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    m_private->stencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    m_private->stencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    m_private->stencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    m_private->stencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    m_private->stencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    m_private->stencilOpSeparate(face, fail, zfail, zpass);
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    return m_private->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum paramName, GC3Dfloat param)
{
    m_private->texParameterf(target, paramName, param);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum paramName, GC3Dint param)
{
    m_private->texParameteri(target, paramName, param);
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_private->texSubImage2D(target, level, xOffset, yOffset, width, height, format, type, pixels);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat x)
{
    m_private->uniform1f(location, x);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GGC3Dsizei size, C3Dfloat* v)
{
    m_private->uniform1fv(location, size, v);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint x)
{
    m_private->uniform1i(location, x);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GGC3Dsizei size, C3Dint* v)
{
    m_private->uniform1iv(location, size, v);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat x, float y)
{
    m_private->uniform2f(location, x, y);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    m_private->uniform2fv(location, size, v);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint x, GC3Dint y)
{
    m_private->uniform2i(location, x, y);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    m_private->uniform2iv(location, size, v);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    m_private->uniform3f(location, x, y, z);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    m_private->uniform3fv(location, size, v);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    m_private->uniform3i(location, x, y, z);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    m_private->uniform3iv(location, size, v);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    m_private->uniform4f(location, x, y, z, w);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    m_private->uniform4fv(location, size, v);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    m_private->uniform4i(location, x, y, z, w);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    m_private->uniform4iv(location, size, v);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    m_private->uniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    m_private->uniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    m_private->uniformMatrix4fv(location, size, transpose, value);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    m_private->useProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    m_private->validateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat x)
{
    m_private->vertexAttrib1f(index, x);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, GC3Dfloat* values)
{
    m_private->vertexAttrib1fv(index, values);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y)
{
    m_private->vertexAttrib2f(index, x, y);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, GC3Dfloat* values)
{
    m_private->vertexAttrib2fv(index, values);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    m_private->vertexAttrib3f(index, x, y, z);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, GC3Dfloat* values)
{
    m_private->vertexAttrib3fv(index, values);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    m_private->vertexAttrib4f(index, x, y, z, w);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, GC3Dfloat* values)
{
    m_private->vertexAttrib4fv(index, values);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    m_private->vertexAttribPointer(index, size, type, normalized, stride, offset);
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    m_private->viewport(x, y, width, height);
}

void GraphicsContext3D::reshape(int width, int height)
{
    notImplemented();
}

void GraphicsContext3D::markContextChanged()
{
    notImplemented();
}

void GraphicsContext3D::markLayerComposited()
{
    notImplemented();
}

bool GraphicsContext3D::layerComposited() const
{
    notImplemented();
    return false;
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context, DrawingBuffer* drawingBuffer)
{
    notImplemented();
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    notImplemented();
    return 0;
}

bool GraphicsContext3D::paintCompositedResultsToCanvas(CanvasRenderingContext*)
{
    return false;
}

Platform3DObject GraphicsContext3D::createBuffer()
{
    return m_private->createBuffer();
}

Platform3DObject GraphicsContext3D::createFramebuffer()
{
    return m_private->createFramebuffer();
}

Platform3DObject GraphicsContext3D::createProgram()
{
    return m_private->createProgram();
}

Platform3DObject GraphicsContext3D::createRenderbuffer()
{
    return m_private->createRenderbuffer();
}

Platform3DObject GraphicsContext3D::createShader(GC3Denum type)
{
    return m_private->createShader(type);
}

Platform3DObject GraphicsContext3D::createTexture()
{
    return m_private->createTexture();
}

void GraphicsContext3D::deleteBuffer(Platform3DObject buffer)
{
    m_private->deleteBuffer(buffer);
}

void GraphicsContext3D::deleteFramebuffer(Platform3DObject buffer)
{
    m_private->deleteFramebuffer(buffer);
}

void GraphicsContext3D::deleteProgram(Platform3DObject program)
{
    m_private->deleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(Platform3DObject buffer)
{
    m_private->deleteRenderbuffer(buffer);
}

void GraphicsContext3D::deleteShader(Platform3DObject shader)
{
    m_private->deleteShader(shader);
}

void GraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    m_private->deleteTexture(texture);
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    m_private->synthesizeGLError(error);
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    return m_private->getExtensions();
}

IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    notImplemented();
    return IntSize();
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
    notImplemented();
}

bool GraphicsContext3D::getImageData(Image* image, GC3Denum format, GC3Denum type, bool premultiplyAlpha,
                                     bool ignoreGammaAndColorProfile, Vector<uint8_t>& outputVector)
{
    notImplemented();
    return false;
}

void GraphicsContext3D::validateAttributes()
{
    notImplemented();
}

void GraphicsContext3D::readRenderingResults(unsigned char* pixels, int pixelsSize)
{
    notImplemented();
}

bool GraphicsContext3D::reshapeFBOs(const IntSize&)
{
    notImplemented();
}

void GraphicsContext3D::resolveMultisamplingIfNecessary(const IntRect&)
{
    notImplemented();
}

bool GraphicsContext3D::isResourceSafe()
{
    notImplemented();
    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
