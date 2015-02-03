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

#include "EXTTextureFilterAnisotropic.h"
#include "Extensions3D.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "WebGLActiveInfo.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
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
    switch (target) {
    case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER_START:
    case GraphicsContext3D::UNIFORM_BUFFER_BINDING:
    case GraphicsContext3D::UNIFORM_BUFFER_SIZE:
    case GraphicsContext3D::UNIFORM_BUFFER_START:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getIndexedParameter", "parameter name not yet supported");
        return WebGLGetInfo();
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getIndexedParameter", "invalid parameter name");
        return WebGLGetInfo();
            
    }
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

WebGLExtension* WebGL2RenderingContext::getExtension(const String& name)
{
    if (isContextLostOrPending())
        return nullptr;

    if (equalIgnoringCase(name, "WEBKIT_EXT_texture_filter_anisotropic")
        && m_context->getExtensions()->supports("GL_EXT_texture_filter_anisotropic")) {
        if (!m_extTextureFilterAnisotropic) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_texture_filter_anisotropic");
            m_extTextureFilterAnisotropic = std::make_unique<EXTTextureFilterAnisotropic>(this);
        }
        return m_extTextureFilterAnisotropic.get();
    }
    if (equalIgnoringCase(name, "OES_texture_float")
        && m_context->getExtensions()->supports("GL_OES_texture_float")) {
        if (!m_oesTextureFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float");
            m_oesTextureFloat = std::make_unique<OESTextureFloat>(this);
        }
        return m_oesTextureFloat.get();
    }
    if (equalIgnoringCase(name, "OES_texture_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_float_linear")) {
        if (!m_oesTextureFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float_linear");
            m_oesTextureFloatLinear = std::make_unique<OESTextureFloatLinear>(this);
        }
        return m_oesTextureFloatLinear.get();
    }
    if (equalIgnoringCase(name, "OES_texture_half_float")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float")) {
        if (!m_oesTextureHalfFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float");
            m_oesTextureHalfFloat = std::make_unique<OESTextureHalfFloat>(this);
        }
        return m_oesTextureHalfFloat.get();
    }
    if (equalIgnoringCase(name, "OES_texture_half_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float_linear")) {
        if (!m_oesTextureHalfFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float_linear");
            m_oesTextureHalfFloatLinear = std::make_unique<OESTextureHalfFloatLinear>(this);
        }
        return m_oesTextureHalfFloatLinear.get();
    }
    if ((equalIgnoringCase(name, "WEBKIT_WEBGL_compressed_texture_atc"))
        && WebGLCompressedTextureATC::supported(this)) {
        if (!m_webglCompressedTextureATC)
            m_webglCompressedTextureATC = std::make_unique<WebGLCompressedTextureATC>(this);
        return m_webglCompressedTextureATC.get();
    }
    if ((equalIgnoringCase(name, "WEBKIT_WEBGL_compressed_texture_pvrtc"))
        && WebGLCompressedTexturePVRTC::supported(this)) {
        if (!m_webglCompressedTexturePVRTC)
            m_webglCompressedTexturePVRTC = std::make_unique<WebGLCompressedTexturePVRTC>(this);
        return m_webglCompressedTexturePVRTC.get();
    }
    if (equalIgnoringCase(name, "WEBGL_compressed_texture_s3tc")
        && WebGLCompressedTextureS3TC::supported(this)) {
        if (!m_webglCompressedTextureS3TC)
            m_webglCompressedTextureS3TC = std::make_unique<WebGLCompressedTextureS3TC>(this);
        return m_webglCompressedTextureS3TC.get();
    }
    if (equalIgnoringCase(name, "WEBGL_depth_texture")
        && WebGLDepthTexture::supported(graphicsContext3D())) {
        if (!m_webglDepthTexture) {
            m_context->getExtensions()->ensureEnabled("GL_CHROMIUM_depth_texture");
            m_webglDepthTexture = std::make_unique<WebGLDepthTexture>(this);
        }
        return m_webglDepthTexture.get();
    }
    if (allowPrivilegedExtensions()) {
        if (equalIgnoringCase(name, "WEBGL_debug_renderer_info")) {
            if (!m_webglDebugRendererInfo)
                m_webglDebugRendererInfo = std::make_unique<WebGLDebugRendererInfo>(this);
            return m_webglDebugRendererInfo.get();
        }
        if (equalIgnoringCase(name, "WEBGL_debug_shaders")
            && m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source")) {
            if (!m_webglDebugShaders)
                m_webglDebugShaders = std::make_unique<WebGLDebugShaders>(this);
            return m_webglDebugShaders.get();
        }
    }
    
    return nullptr;
}

Vector<String> WebGL2RenderingContext::getSupportedExtensions()
{
    Vector<String> result;
    
    if (m_isPendingPolicyResolution)
        return result;

    if (m_context->getExtensions()->supports("GL_OES_texture_float"))
        result.append("OES_texture_float");
    if (m_context->getExtensions()->supports("GL_OES_texture_float_linear"))
        result.append("OES_texture_float_linear");
    if (m_context->getExtensions()->supports("GL_OES_texture_half_float"))
        result.append("OES_texture_half_float");
    if (m_context->getExtensions()->supports("GL_OES_texture_half_float_linear"))
        result.append("OES_texture_half_float_linear");
    if (m_context->getExtensions()->supports("GL_EXT_texture_filter_anisotropic"))
        result.append("WEBKIT_EXT_texture_filter_anisotropic");
    if (WebGLCompressedTextureATC::supported(this))
        result.append("WEBKIT_WEBGL_compressed_texture_atc");
    if (WebGLCompressedTexturePVRTC::supported(this))
        result.append("WEBKIT_WEBGL_compressed_texture_pvrtc");
    if (WebGLCompressedTextureS3TC::supported(this))
        result.append("WEBGL_compressed_texture_s3tc");
    if (WebGLDepthTexture::supported(graphicsContext3D()))
        result.append("WEBGL_depth_texture");
    
    if (allowPrivilegedExtensions()) {
        if (m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source"))
            result.append("WEBGL_debug_shaders");
        result.append("WEBGL_debug_renderer_info");
    }
    
    return result;
}

WebGLGetInfo WebGL2RenderingContext::getParameter(GC3Denum pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (isContextLostOrPending())
        return WebGLGetInfo();
    const int intZero = 0;
    switch (pname) {
    case GraphicsContext3D::ACTIVE_TEXTURE:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::ALPHA_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundArrayBuffer));
    case GraphicsContext3D::BLEND:
        return getBooleanParameter(pname);
    case GraphicsContext3D::BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::BLEND_DST_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_DST_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_EQUATION_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_EQUATION_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_SRC_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_SRC_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLUE_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GraphicsContext3D::COMPRESSED_TEXTURE_FORMATS:
        return WebGLGetInfo(Uint32Array::create(m_compressedTextureFormats.data(), m_compressedTextureFormats.size()));
    case GraphicsContext3D::CULL_FACE:
        return getBooleanParameter(pname);
    case GraphicsContext3D::CULL_FACE_MODE:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::CURRENT_PROGRAM:
        return WebGLGetInfo(PassRefPtr<WebGLProgram>(m_currentProgram));
    case GraphicsContext3D::DEPTH_BITS:
        if (!m_framebufferBinding && !m_attributes.depth)
            return WebGLGetInfo(intZero);
        return getIntParameter(pname);
    case GraphicsContext3D::DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GraphicsContext3D::DEPTH_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::DEPTH_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GraphicsContext3D::DITHER:
        return getBooleanParameter(pname);
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundVertexArrayObject->getElementArrayBuffer()));
    case GraphicsContext3D::FRAMEBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLFramebuffer>(m_framebufferBinding));
    case GraphicsContext3D::FRONT_FACE:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::GENERATE_MIPMAP_HINT:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::GREEN_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::IMPLEMENTATION_COLOR_READ_FORMAT:
        return getIntParameter(pname);
    case GraphicsContext3D::IMPLEMENTATION_COLOR_READ_TYPE:
        return getIntParameter(pname);
    case GraphicsContext3D::LINE_WIDTH:
        return getFloatParameter(pname);
    case GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_CUBE_MAP_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_RENDERBUFFER_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VARYING_VECTORS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_ATTRIBS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContext3D::NUM_SHADER_BINARY_FORMATS:
        return getIntParameter(pname);
    case GraphicsContext3D::PACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GraphicsContext3D::RED_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::RENDERBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(m_renderbufferBinding));
    case GraphicsContext3D::RENDERER:
        return WebGLGetInfo(String("WebKit WebGL"));
    case GraphicsContext3D::SAMPLE_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GraphicsContext3D::SAMPLES:
        return getIntParameter(pname);
    case GraphicsContext3D::SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContext3D::SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SHADING_LANGUAGE_VERSION:
        return WebGLGetInfo("WebGL GLSL ES 1.0 (" + m_context->getString(GraphicsContext3D::SHADING_LANGUAGE_VERSION) + ")");
    case GraphicsContext3D::STENCIL_BACK_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_REF:
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BITS:
        if (!m_framebufferBinding && !m_attributes.stencil)
            return WebGLGetInfo(intZero);
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_CLEAR_VALUE:
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_REF:
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::STENCIL_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::SUBPIXEL_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::TEXTURE_BINDING_2D:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].texture2DBinding));
    case GraphicsContext3D::TEXTURE_BINDING_CUBE_MAP:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].textureCubeMapBinding));
    case GraphicsContext3D::UNPACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_FLIP_Y_WEBGL:
        return WebGLGetInfo(m_unpackFlipY);
    case GraphicsContext3D::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return WebGLGetInfo(m_unpackPremultiplyAlpha);
    case GraphicsContext3D::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return WebGLGetInfo(m_unpackColorspaceConversion);
    case GraphicsContext3D::VENDOR:
        return WebGLGetInfo(String("WebKit"));
    case GraphicsContext3D::VERSION:
        return WebGLGetInfo("WebGL 2.0 (" + m_context->getString(GraphicsContext3D::VERSION) + ")");
    case GraphicsContext3D::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo)
            return WebGLGetInfo(m_context->getString(GraphicsContext3D::RENDERER));
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return WebGLGetInfo();
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo)
            return WebGLGetInfo(m_context->getString(GraphicsContext3D::VENDOR));
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(Extensions3D::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return WebGLGetInfo();
    case GraphicsContext3D::FRAGMENT_SHADER_DERIVATIVE_HINT:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_3D_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_ARRAY_TEXTURE_LAYERS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_COLOR_ATTACHMENTS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        return getInt64Parameter(pname);
    case GraphicsContext3D::MAX_COMBINED_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        return getInt64Parameter(pname);
    case GraphicsContext3D::MAX_DRAW_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_ELEMENT_INDEX:
        return getInt64Parameter(pname);
    case GraphicsContext3D::MAX_ELEMENTS_INDICES:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_ELEMENTS_VERTICES:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_FRAGMENT_UNIFORM_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_FRAGMENT_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_PROGRAM_TEXEL_OFFSET:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_SAMPLES:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_SERVER_WAIT_TIMEOUT:
        return getInt64Parameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_LOD_BIAS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_UNIFORM_BLOCK_SIZE:
        return getInt64Parameter(pname);
    case GraphicsContext3D::MAX_UNIFORM_BUFFER_BINDINGS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VARYING_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_OUTPUT_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_UNIFORM_COMPONENTS: 
        return getIntParameter(pname);                            
    case GraphicsContext3D::MIN_PROGRAM_TEXEL_OFFSET:
        return getIntParameter(pname);
    case GraphicsContext3D::PACK_ROW_LENGTH:
        return getIntParameter(pname);
    case GraphicsContext3D::PACK_SKIP_PIXELS:
        return getIntParameter(pname);
    case GraphicsContext3D::PACK_SKIP_ROWS:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_IMAGE_HEIGHT:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_ROW_LENGTH:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_SKIP_IMAGES:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_SKIP_PIXELS:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_SKIP_ROWS:
        return getIntParameter(pname);
    case GraphicsContext3D::RASTERIZER_DISCARD:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SAMPLE_ALPHA_TO_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContext3D::TRANSFORM_FEEDBACK_ACTIVE:
        return getBooleanParameter(pname);
    case GraphicsContext3D::TRANSFORM_FEEDBACK_PAUSED:
        return getBooleanParameter(pname);
    case GraphicsContext3D::UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContext3D::COPY_READ_BUFFER:
    case GraphicsContext3D::COPY_WRITE_BUFFER:
    case GraphicsContext3D::DRAW_BUFFER0:
    case GraphicsContext3D::DRAW_BUFFER1:
    case GraphicsContext3D::DRAW_BUFFER2:
    case GraphicsContext3D::DRAW_BUFFER3:
    case GraphicsContext3D::DRAW_BUFFER4:
    case GraphicsContext3D::DRAW_BUFFER5:
    case GraphicsContext3D::DRAW_BUFFER6:
    case GraphicsContext3D::DRAW_BUFFER7:
    case GraphicsContext3D::DRAW_BUFFER8:
    case GraphicsContext3D::DRAW_BUFFER9:
    case GraphicsContext3D::DRAW_BUFFER10:
    case GraphicsContext3D::DRAW_BUFFER11:
    case GraphicsContext3D::DRAW_BUFFER12:
    case GraphicsContext3D::DRAW_BUFFER13:
    case GraphicsContext3D::DRAW_BUFFER14:
    case GraphicsContext3D::DRAW_BUFFER15:
    case GraphicsContext3D::PIXEL_PACK_BUFFER_BINDING:   
    case GraphicsContext3D::PIXEL_UNPACK_BUFFER_BINDING:
    case GraphicsContext3D::READ_BUFFER:
    case GraphicsContext3D::SAMPLER_BINDING:
    case GraphicsContext3D::TEXTURE_BINDING_2D_ARRAY:
    case GraphicsContext3D::TEXTURE_BINDING_3D:
    case GraphicsContext3D::READ_FRAMEBUFFER_BINDING:
    case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GraphicsContext3D::UNIFORM_BUFFER_BINDING:
    case GraphicsContext3D::VERTEX_ARRAY_BINDING:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "parameter name not yet supported");
        return WebGLGetInfo();
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

bool WebGL2RenderingContext::validateCapability(const char* functionName, GC3Denum cap)
{
    switch (cap) {
    case GraphicsContext3D::BLEND:
    case GraphicsContext3D::CULL_FACE:
    case GraphicsContext3D::DEPTH_TEST:
    case GraphicsContext3D::DITHER:
    case GraphicsContext3D::POLYGON_OFFSET_FILL:
    case GraphicsContext3D::SAMPLE_ALPHA_TO_COVERAGE:
    case GraphicsContext3D::SAMPLE_COVERAGE:
    case GraphicsContext3D::SCISSOR_TEST:
    case GraphicsContext3D::STENCIL_TEST:
    case GraphicsContext3D::RASTERIZER_DISCARD:
        return true;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid capability");
        return false;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
