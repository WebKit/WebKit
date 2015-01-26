/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)
#include "WebGL2RenderingContext.h"

#include "WebGLActiveInfo.h"
#include "WebGLQuery.h"
#include "WebGLSampler.h"
#include "WebGLSync.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArrayObject.h"

namespace WebCore {

WebGL2RenderingContext::WebGL2RenderingContext(HTMLCanvasElement* passedCanvas, GraphicsContext3D::Attributes attributes)
    : WebGLRenderingContextBase(passedCanvas, attributes)
{
}

WebGL2RenderingContext::WebGL2RenderingContext(HTMLCanvasElement* passedCanvas, PassRefPtr<GraphicsContext3D> context,
    GraphicsContext3D::Attributes attributes) : WebGLRenderingContextBase(passedCanvas, context, attributes)
{
}

void WebGL2RenderingContext::copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dint64 readOffset, GC3Dint64 writeOffset, GC3Dint64 size)
{
    UNUSED_PARAM(readTarget);
    UNUSED_PARAM(writeTarget);
    UNUSED_PARAM(readOffset);
    UNUSED_PARAM(writeOffset);
    UNUSED_PARAM(size);
}

void WebGL2RenderingContext::getBufferSubData(GC3Denum target, GC3Dint64 offset, ArrayBufferView* returnedData)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(returnedData);
}

void WebGL2RenderingContext::getBufferSubData(GC3Denum target, GC3Dint64 offset, ArrayBuffer* returnedData)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(returnedData);
}

WebGLGetInfo WebGL2RenderingContext::getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachment);
    UNUSED_PARAM(pname);
    return WebGLGetInfo();
}

void WebGL2RenderingContext::blitFramebuffer(GC3Dint srcX0, GC3Dint srcY0, GC3Dint srcX1, GC3Dint srcY1, GC3Dint dstX0, GC3Dint dstY0, GC3Dint dstX1, GC3Dint dstY1, GC3Dbitfield mask, GC3Denum filter)
{
    UNUSED_PARAM(srcX0);
    UNUSED_PARAM(srcX1);
    UNUSED_PARAM(srcY0);
    UNUSED_PARAM(srcY1);
    UNUSED_PARAM(dstX0);
    UNUSED_PARAM(dstX1);
    UNUSED_PARAM(dstY0);
    UNUSED_PARAM(dstY1);
    UNUSED_PARAM(mask);
    UNUSED_PARAM(filter);
}

void WebGL2RenderingContext::framebufferTextureLayer(GC3Denum target, GC3Denum attachment, GC3Duint texture, GC3Dint level, GC3Dint layer)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachment);
    UNUSED_PARAM(texture);
    UNUSED_PARAM(level);
    UNUSED_PARAM(layer);
}

WebGLGetInfo WebGL2RenderingContext::getInternalformatParameter(GC3Denum target, GC3Denum internalformat, GC3Denum pname)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(pname);
    return WebGLGetInfo();
}

void WebGL2RenderingContext::invalidateFramebuffer(GC3Denum target, Vector<GC3Denum> attachments)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachments);
}

void WebGL2RenderingContext::invalidateSubFramebuffer(GC3Denum target, Vector<GC3Denum> attachments, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachments);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void WebGL2RenderingContext::readBuffer(GC3Denum src)
{
    UNUSED_PARAM(src);
}

void WebGL2RenderingContext::renderbufferStorageMultisample(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(samples);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void WebGL2RenderingContext::texStorage2D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(levels);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void WebGL2RenderingContext::texStorage3D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(levels);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
}

void WebGL2RenderingContext::texImage3D(GC3Denum target, GC3Dint level, GC3Dint internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Denum format, GC3Denum type, ArrayBufferView* pixels)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pixels);
}

void WebGL2RenderingContext::texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, GC3Denum type, ArrayBufferView* pixels)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pixels);
}

void WebGL2RenderingContext::texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Denum format, GC3Denum type, ImageData* source)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(source);
}

void WebGL2RenderingContext::texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Denum format, GC3Denum type, HTMLImageElement* source)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(source);
}

void WebGL2RenderingContext::texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Denum format, GC3Denum type, HTMLCanvasElement* source)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(source);
}

void WebGL2RenderingContext::texSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Denum format, GC3Denum type, HTMLVideoElement* source)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(source);
}

void WebGL2RenderingContext::copyTexSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void WebGL2RenderingContext::compressedTexImage3D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Dint border, GC3Dsizei imageSize, ArrayBufferView* data)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(data);
}

void WebGL2RenderingContext::compressedTexSubImage3D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint zoffset, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth, GC3Denum format, GC3Dsizei imageSize, ArrayBufferView* data)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(format);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(data);
}

GC3Dint WebGL2RenderingContext::getFragDataLocation(WebGLProgram* program, String name)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(name);
    return 0;
}

void WebGL2RenderingContext::uniform1ui(WebGLUniformLocation* location, GC3Duint v0)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
}

void WebGL2RenderingContext::uniform2ui(WebGLUniformLocation* location, GC3Duint v0, GC3Duint v1)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
    UNUSED_PARAM(v1);
}

void WebGL2RenderingContext::uniform3ui(WebGLUniformLocation* location, GC3Duint v0, GC3Duint v1, GC3Duint v2)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
    UNUSED_PARAM(v1);
    UNUSED_PARAM(v2);
}

void WebGL2RenderingContext::uniform4ui(WebGLUniformLocation* location, GC3Duint v0, GC3Duint v1, GC3Duint v2, GC3Duint v3)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
    UNUSED_PARAM(v1);
    UNUSED_PARAM(v2);
    UNUSED_PARAM(v3);
}

void WebGL2RenderingContext::uniform1uiv(WebGLUniformLocation* location, Uint32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniform2uiv(WebGLUniformLocation* location, Uint32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniform3uiv(WebGLUniformLocation* location, Uint32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniform4uiv(WebGLUniformLocation* location, Uint32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniformMatrix2x3fv(WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniformMatrix3x2fv(WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniformMatrix2x4fv(WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniformMatrix4x2fv(WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniformMatrix3x4fv(WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::uniformMatrix4x3fv(WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* value)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::vertexAttribI4i(GC3Duint index, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
    UNUSED_PARAM(w);
}

void WebGL2RenderingContext::vertexAttribI4iv(GC3Duint index, Int32Array* v)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(v);
}

void WebGL2RenderingContext::vertexAttribI4ui(GC3Duint index, GC3Duint x, GC3Duint y, GC3Duint z, GC3Duint w)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
    UNUSED_PARAM(w);
}

void WebGL2RenderingContext::vertexAttribI4uiv(GC3Duint index, Uint32Array* v)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(v);
}

void WebGL2RenderingContext::vertexAttribIPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dsizei stride, GC3Dint64 offset)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(size);
    UNUSED_PARAM(type);
    UNUSED_PARAM(stride);
    UNUSED_PARAM(offset);
}


void WebGL2RenderingContext::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(divisor);
}

void WebGL2RenderingContext::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei instanceCount)
{
    UNUSED_PARAM(mode);
    UNUSED_PARAM(first);
    UNUSED_PARAM(count);
    UNUSED_PARAM(instanceCount);
}

void WebGL2RenderingContext::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dint64 offset, GC3Dsizei instanceCount)
{
    UNUSED_PARAM(mode);
    UNUSED_PARAM(count);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(instanceCount);
}

void WebGL2RenderingContext::drawRangeElements(GC3Denum mode, GC3Duint start, GC3Duint end, GC3Dsizei count, GC3Denum type, GC3Dint64 offset)
{
    UNUSED_PARAM(mode);
    UNUSED_PARAM(start);
    UNUSED_PARAM(end);
    UNUSED_PARAM(count);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);
}

void WebGL2RenderingContext::drawBuffers(Vector<GC3Denum> buffers)
{
    UNUSED_PARAM(buffers);
}

void WebGL2RenderingContext::clearBufferiv(GC3Denum buffer, GC3Dint drawbuffer, Int32Array* value)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::clearBufferuiv(GC3Denum buffer, GC3Dint drawbuffer, Uint32Array* value)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::clearBufferfv(GC3Denum buffer, GC3Dint drawbuffer, Float32Array* value)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(value);
}

void WebGL2RenderingContext::clearBufferfi(GC3Denum buffer, GC3Dint drawbuffer, GC3Dfloat depth, GC3Dint stencil)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(stencil);
}

PassRefPtr<WebGLQuery> WebGL2RenderingContext::createQuery()
{
    return nullptr;
}

void WebGL2RenderingContext::deleteQuery(WebGLQuery* query)
{
    UNUSED_PARAM(query);
}

GC3Dboolean WebGL2RenderingContext::isQuery(WebGLQuery* query)
{
    UNUSED_PARAM(query);
    return false;
}

void WebGL2RenderingContext::beginQuery(GC3Denum target, WebGLQuery* query)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(query);
}

void WebGL2RenderingContext::endQuery(GC3Denum target)
{
    UNUSED_PARAM(target);
}

PassRefPtr<WebGLQuery> WebGL2RenderingContext::getQuery(GC3Denum target, GC3Denum pname)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(pname);
    return nullptr;
}

WebGLGetInfo WebGL2RenderingContext::getQueryParameter(WebGLQuery* query, GC3Denum pname)
{
    UNUSED_PARAM(query);
    UNUSED_PARAM(pname);
    return WebGLGetInfo();
}

PassRefPtr<WebGLSampler> WebGL2RenderingContext::createSampler()
{
    return nullptr;
}

void WebGL2RenderingContext::deleteSampler(WebGLSampler* sampler)
{
    UNUSED_PARAM(sampler);
}

GC3Dboolean WebGL2RenderingContext::isSampler(WebGLSampler* sampler)
{
    UNUSED_PARAM(sampler);
    return false;
}

void WebGL2RenderingContext::bindSampler(GC3Duint unit, WebGLSampler* sampler)
{
    UNUSED_PARAM(unit);
    UNUSED_PARAM(sampler);
}

void WebGL2RenderingContext::samplerParameteri(WebGLSampler* sampler, GC3Denum pname, GC3Dint param)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(param);
}

void WebGL2RenderingContext::samplerParameterf(WebGLSampler* sampler, GC3Denum pname, GC3Dfloat param)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(param);
}

WebGLGetInfo WebGL2RenderingContext::getSamplerParameter(WebGLSampler* sampler, GC3Denum pname)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    return WebGLGetInfo();
}

PassRefPtr<WebGLSync> WebGL2RenderingContext::fenceSync(GC3Denum condition, GC3Dbitfield flags)
{
    UNUSED_PARAM(condition);
    UNUSED_PARAM(flags);
    return nullptr;
}

GC3Dboolean WebGL2RenderingContext::isSync(WebGLSync* sync)
{
    UNUSED_PARAM(sync);
    return false;
}

void WebGL2RenderingContext::deleteSync(WebGLSync* sync)
{
    UNUSED_PARAM(sync);
}

GC3Denum WebGL2RenderingContext::clientWaitSync(WebGLSync* sync, GC3Dbitfield flags, GC3Duint64 timeout)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(timeout);
    return 0;
}

void WebGL2RenderingContext::waitSync(WebGLSync* sync, GC3Dbitfield flags, GC3Duint64 timeout)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(timeout);
}

WebGLGetInfo WebGL2RenderingContext::getSyncParameter(WebGLSync* sync, GC3Denum pname)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(pname);
    return WebGLGetInfo();
}

PassRefPtr<WebGLTransformFeedback> WebGL2RenderingContext::createTransformFeedback()
{
    return nullptr;
}

void WebGL2RenderingContext::deleteTransformFeedback(WebGLTransformFeedback* id)
{
    UNUSED_PARAM(id);
}

GC3Dboolean WebGL2RenderingContext::isTransformFeedback(WebGLTransformFeedback* id)
{
    UNUSED_PARAM(id);
    return false;
}

void WebGL2RenderingContext::bindTransformFeedback(GC3Denum target, WebGLTransformFeedback* id)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(id);
}

void WebGL2RenderingContext::beginTransformFeedback(GC3Denum primitiveMode)
{
    UNUSED_PARAM(primitiveMode);
}

void WebGL2RenderingContext::endTransformFeedback()
{
}

void WebGL2RenderingContext::transformFeedbackVaryings(WebGLProgram* program, Vector<String> varyings, GC3Denum bufferMode)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(varyings);
    UNUSED_PARAM(bufferMode);
}

PassRefPtr<WebGLActiveInfo> WebGL2RenderingContext::getTransformFeedbackVarying(WebGLProgram* program, GC3Duint index)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(index);
    return nullptr;
}

void WebGL2RenderingContext::pauseTransformFeedback()
{
}

void WebGL2RenderingContext::resumeTransformFeedback()
{
}

void WebGL2RenderingContext::bindBufferBase(GC3Denum target, GC3Duint index, WebGLBuffer* buffer)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(index);
    UNUSED_PARAM(buffer);
}

void WebGL2RenderingContext::bindBufferRange(GC3Denum target, GC3Duint index, WebGLBuffer* buffer, GC3Dint64 offset, GC3Dint64 size)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(index);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

WebGLGetInfo WebGL2RenderingContext::getIndexedParameter(GC3Denum target, GC3Duint index)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(index);
    return WebGLGetInfo();
}

Uint32Array* WebGL2RenderingContext::getUniformIndices(WebGLProgram* program, Vector<String> uniformNames)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformNames);
    return nullptr;
}

Int32Array* WebGL2RenderingContext::getActiveUniforms(WebGLProgram* program, Uint32Array* uniformIndices, GC3Denum pname)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformIndices);
    UNUSED_PARAM(pname);
    return nullptr;
}

GC3Duint WebGL2RenderingContext::getUniformBlockIndex(WebGLProgram* program, String uniformBlockName)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockName);
    return 0;
}

WebGLGetInfo WebGL2RenderingContext::getActiveUniformBlockParameter(WebGLProgram* program, GC3Duint uniformBlockIndex, GC3Denum pname)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(pname);
    return WebGLGetInfo();
}

WebGLGetInfo WebGL2RenderingContext::getActiveUniformBlockName(WebGLProgram* program, GC3Duint uniformBlockIndex)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    return WebGLGetInfo();
}

void WebGL2RenderingContext::uniformBlockBinding(WebGLProgram* program, GC3Duint uniformBlockIndex, GC3Duint uniformBlockBinding)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(uniformBlockBinding);
}

PassRefPtr<WebGLVertexArrayObject> WebGL2RenderingContext::createVertexArray()
{
    return nullptr;
}

void WebGL2RenderingContext::deleteVertexArray(WebGLVertexArrayObject* vertexArray)
{
    UNUSED_PARAM(vertexArray);
}

GC3Dboolean WebGL2RenderingContext::isVertexArray(WebGLVertexArrayObject* vertexArray)
{
    UNUSED_PARAM(vertexArray);
    return false;
}

void WebGL2RenderingContext::bindVertexArray(WebGLVertexArrayObject* vertexArray)
{
    UNUSED_PARAM(vertexArray);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
