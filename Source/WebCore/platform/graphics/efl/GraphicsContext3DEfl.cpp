/*
    Copyright (C) 2011 Samsung Electronics

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

#if ENABLE(WEBGL)

#include "GraphicsContext3DInternal.h"

#include "ImageData.h"
#include "NotImplemented.h"

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, RenderStyle renderStyle)
{
    bool renderDirectlyToEvasGLObject = (renderStyle == RenderDirectlyToHostWindow);

    OwnPtr<GraphicsContext3DInternal> internal = GraphicsContext3DInternal::create(attrs, hostWindow, renderDirectlyToEvasGLObject);
    if (!internal)
        return 0;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderDirectlyToEvasGLObject));
    context->m_internal = internal.release();
    return context.release();
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_isResourceSafe(false)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_internal->platformGraphicsContext3D();
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
    m_internal->makeContextCurrent();
    return true;
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_internal->isGLES2Compliant();
}

void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    m_internal->activeTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    m_internal->attachShader(program, shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    m_internal->bindAttribLocation(program, index, name);
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    m_internal->bindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    m_internal->bindFramebuffer(target, buffer);
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    m_internal->bindRenderbuffer(target, renderbuffer);
}

void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    m_internal->bindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    m_internal->blendColor(red, green, blue, alpha);
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    m_internal->blendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    m_internal->blendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3D::blendFunc(GC3Denum srcFactor, GC3Denum dstFactor)
{
    m_internal->blendFunc(srcFactor, dstFactor);
}

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    m_internal->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    m_internal->bufferData(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    m_internal->bufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    m_internal->bufferSubData(target, offset, size, data);
}

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    return m_internal->checkFramebufferStatus(target);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    m_internal->clear(mask);
}

void GraphicsContext3D::clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    m_internal->clearColor(red, green, blue, alpha);
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    m_internal->clearDepth(depth);
}

void GraphicsContext3D::clearStencil(GC3Dint clearValue)
{
    m_internal->clearStencil(clearValue);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    m_internal->colorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    m_internal->compileShader(shader);
}

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    m_internal->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    m_internal->copyTexSubImage2D(target, level, xOffset, yOffset, x, y, width, height);
}

void GraphicsContext3D::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data)
{
    // FIXME: Add support for compressedTexImage2D.
    // m_internal->compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void GraphicsContext3D::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data)
{
    // FIXME: Add support for compressedTexSubImage2D.
    // m_internal->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    m_internal->cullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    m_internal->depthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    m_internal->depthMask(flag);
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    m_internal->depthRange(zNear, zFar);
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    m_internal->detachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    m_internal->disable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    m_internal->disableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    m_internal->drawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    m_internal->drawElements(mode, count, type, offset);
}

void GraphicsContext3D::enable(GC3Denum cap)
{
    m_internal->enable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    m_internal->enableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    m_internal->finish();
}

void GraphicsContext3D::flush()
{
    m_internal->flush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbufferTarget, Platform3DObject buffer)
{
    m_internal->framebufferRenderbuffer(target, attachment, renderbufferTarget, buffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum texTarget, Platform3DObject texture, GC3Dint level)
{
    m_internal->framebufferTexture2D(target, attachment, texTarget, texture, level);
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    m_internal->frontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    m_internal->generateMipmap(target);
}

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    return m_internal->getActiveAttrib(program, index, info);
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    return m_internal->getActiveUniform(program, index, info);
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
{
    m_internal->getAttachedShaders(program, maxCount, count, shaders);
}

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_internal->getAttribLocation(program, name);
}

void GraphicsContext3D::getBooleanv(GC3Denum paramName, GC3Dboolean* value)
{
    m_internal->getBooleanv(paramName, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getBufferParameteriv(target, paramName, value);
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    return m_internal->getContextAttributes();
}

GC3Denum GraphicsContext3D::getError()
{
    return m_internal->getError();
}

void GraphicsContext3D::getFloatv(GC3Denum paramName, GC3Dfloat* value)
{
    m_internal->getFloatv(paramName, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getFramebufferAttachmentParameteriv(target, attachment, paramName, value);
}

void GraphicsContext3D::getIntegerv(GC3Denum paramName, GC3Dint* value)
{
    m_internal->getIntegerv(paramName, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getProgramiv(program, paramName, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    return m_internal->getProgramInfoLog(program);
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getRenderbufferParameteriv(target, paramName, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getShaderiv(shader, paramName, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    return m_internal->getShaderInfoLog(shader);
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    return m_internal->getShaderSource(shader);
}

String GraphicsContext3D::getString(GC3Denum name)
{
    return m_internal->getString(name);
}

void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum paramName, GC3Dfloat* value)
{
    m_internal->getTexParameterfv(target, paramName, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getTexParameteriv(target, paramName, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    m_internal->getUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    m_internal->getUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_internal->getUniformLocation(program, name);
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum paramName, GC3Dfloat* value)
{
    m_internal->getVertexAttribfv(index, paramName, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum paramName, GC3Dint* value)
{
    m_internal->getVertexAttribiv(index, paramName, value);
}

long GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum paramName)
{
    return m_internal->getVertexAttribOffset(index, paramName);
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    m_internal->hint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject obj)
{
    return m_internal->isBuffer(obj);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    return m_internal->isEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject obj)
{
    return m_internal->isFramebuffer(obj);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject obj)
{
    return m_internal->isProgram(obj);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject obj)
{
    return m_internal->isRenderbuffer(obj);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject obj)
{
    return m_internal->isShader(obj);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject obj)
{
    return m_internal->isTexture(obj);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    m_internal->lineWidth(width);
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    m_internal->linkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum paramName, GC3Dint param)
{
    m_internal->pixelStorei(paramName, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    m_internal->polygonOffset(factor, units);
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    m_internal->readPixels(x, y, width, height, format, type, data);
}

void GraphicsContext3D::releaseShaderCompiler()
{
    notImplemented();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    m_internal->renderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    m_internal->sampleCoverage(value, invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    m_internal->scissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject program, const String& string)
{
    m_internal->shaderSource(program, string);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    m_internal->stencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    m_internal->stencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    m_internal->stencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    m_internal->stencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    m_internal->stencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    m_internal->stencilOpSeparate(face, fail, zfail, zpass);
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    return m_internal->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum paramName, GC3Dfloat param)
{
    m_internal->texParameterf(target, paramName, param);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum paramName, GC3Dint param)
{
    m_internal->texParameteri(target, paramName, param);
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_internal->texSubImage2D(target, level, xOffset, yOffset, width, height, format, type, pixels);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat x)
{
    m_internal->uniform1f(location, x);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_internal->uniform1fv(location, size, v);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint x)
{
    m_internal->uniform1i(location, x);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_internal->uniform1iv(location, size, v);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat x, float y)
{
    m_internal->uniform2f(location, x, y);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_internal->uniform2fv(location, size, v);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint x, GC3Dint y)
{
    m_internal->uniform2i(location, x, y);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_internal->uniform2iv(location, size, v);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    m_internal->uniform3f(location, x, y, z);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_internal->uniform3fv(location, size, v);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    m_internal->uniform3i(location, x, y, z);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_internal->uniform3iv(location, size, v);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    m_internal->uniform4f(location, x, y, z, w);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_internal->uniform4fv(location, size, v);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    m_internal->uniform4i(location, x, y, z, w);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_internal->uniform4iv(location, size, v);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    m_internal->uniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    m_internal->uniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    m_internal->uniformMatrix4fv(location, size, transpose, value);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    m_internal->useProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    m_internal->validateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat x)
{
    m_internal->vertexAttrib1f(index, x);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, GC3Dfloat* values)
{
    m_internal->vertexAttrib1fv(index, values);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y)
{
    m_internal->vertexAttrib2f(index, x, y);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, GC3Dfloat* values)
{
    m_internal->vertexAttrib2fv(index, values);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    m_internal->vertexAttrib3f(index, x, y, z);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, GC3Dfloat* values)
{
    m_internal->vertexAttrib3fv(index, values);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    m_internal->vertexAttrib4f(index, x, y, z, w);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, GC3Dfloat* values)
{
    m_internal->vertexAttrib4fv(index, values);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    m_internal->vertexAttribPointer(index, size, type, normalized, stride, offset);
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    m_internal->viewport(x, y, width, height);
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
    // DrawingBuffer support only implemented in Chromium ports.
    ASSERT(!drawingBuffer);
    notImplemented();
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    // DrawingBuffer support only implemented in Chromium ports.
    ASSERT(!drawingBuffer);
    notImplemented();
    RefPtr<ImageData> imageData = ImageData::create(IntSize(1, 1));
    return imageData.release();
}

Platform3DObject GraphicsContext3D::createBuffer()
{
    return m_internal->createBuffer();
}

Platform3DObject GraphicsContext3D::createFramebuffer()
{
    return m_internal->createFramebuffer();
}

Platform3DObject GraphicsContext3D::createProgram()
{
    return m_internal->createProgram();
}

Platform3DObject GraphicsContext3D::createRenderbuffer()
{
    return m_internal->createRenderbuffer();
}

Platform3DObject GraphicsContext3D::createShader(GC3Denum type)
{
    return m_internal->createShader(type);
}

Platform3DObject GraphicsContext3D::createTexture()
{
    return m_internal->createTexture();
}

void GraphicsContext3D::deleteBuffer(Platform3DObject buffer)
{
    m_internal->deleteBuffer(buffer);
}

void GraphicsContext3D::deleteFramebuffer(Platform3DObject buffer)
{
    m_internal->deleteFramebuffer(buffer);
}

void GraphicsContext3D::deleteProgram(Platform3DObject program)
{
    m_internal->deleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(Platform3DObject buffer)
{
    m_internal->deleteRenderbuffer(buffer);
}

void GraphicsContext3D::deleteShader(Platform3DObject shader)
{
    m_internal->deleteShader(shader);
}

void GraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    m_internal->deleteTexture(texture);
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    m_internal->synthesizeGLError(error);
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    return m_internal->getExtensions();
}

IntSize GraphicsContext3D::getInternalFramebufferSize()
{
    notImplemented();
    return IntSize();
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
    notImplemented();
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>)
{
    notImplemented();
}

bool GraphicsContext3D::getImageData(Image* image, GC3Denum format, GC3Denum type, bool premultiplyAlpha,
                                     bool ignoreGammaAndColorProfile, Vector<uint8_t>& outputVector)
{
    notImplemented();
    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
