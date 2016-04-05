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

#if ENABLE(WEBGL) && ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"

#include "CachedImage.h"
#include "EXTTextureFilterAnisotropic.h"
#include "Extensions3D.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "RenderBox.h"
#include "WebGLActiveInfo.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLContextAttributes.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLLoseContext.h"
#include "WebGLQuery.h"
#include "WebGLSampler.h"
#include "WebGLSync.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArrayObject.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>

namespace WebCore {

WebGL2RenderingContext::WebGL2RenderingContext(HTMLCanvasElement* passedCanvas, GraphicsContext3D::Attributes attributes)
    : WebGLRenderingContextBase(passedCanvas, attributes)
{
}

WebGL2RenderingContext::WebGL2RenderingContext(HTMLCanvasElement* passedCanvas, PassRefPtr<GraphicsContext3D> context,
    GraphicsContext3D::Attributes attributes) : WebGLRenderingContextBase(passedCanvas, context, attributes)
{
    initializeShaderExtensions();
    initializeVertexArrayObjects();
}

void WebGL2RenderingContext::initializeVertexArrayObjects()
{
    m_defaultVertexArrayObject = WebGLVertexArrayObject::create(this, WebGLVertexArrayObject::VAOTypeDefault);
    addContextObject(m_defaultVertexArrayObject.get());
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
    if (!isGLES2Compliant())
        initVertexAttrib0();
}

void WebGL2RenderingContext::initializeShaderExtensions()
{
    m_context->getExtensions()->ensureEnabled("GL_OES_standard_derivatives");
    m_context->getExtensions()->ensureEnabled("GL_EXT_draw_buffers");
    m_context->getExtensions()->ensureEnabled("GL_EXT_shader_texture_lod");
    m_context->getExtensions()->ensureEnabled("GL_EXT_frag_depth");
}

void WebGL2RenderingContext::copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dint64 readOffset, GC3Dint64 writeOffset, GC3Dint64 size)
{
    UNUSED_PARAM(readTarget);
    UNUSED_PARAM(writeTarget);
    UNUSED_PARAM(readOffset);
    UNUSED_PARAM(writeOffset);
    UNUSED_PARAM(size);
}

void WebGL2RenderingContext::getBufferSubData(GC3Denum target, GC3Dint64 offset, ArrayBuffer* returnedData)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(returnedData);
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

void WebGL2RenderingContext::clear(GC3Dbitfield mask)
{
    if (isContextLostOrPending())
        return;
    if (mask & ~(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clear", "invalid mask");
        return;
    }
    if (m_framebufferBinding && (mask & GraphicsContext3D::COLOR_BUFFER_BIT) && isIntegerFormat(m_framebufferBinding->getColorBufferFormat())) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "clear", "cannot clear an integer buffer");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), !isResourceSafe(), &reason)) {
        synthesizeGLError(GraphicsContext3D::INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
        return;
    }
    if (!clearIfComposited(mask))
        m_context->clear(mask);
    markContextChanged();
}

void WebGL2RenderingContext::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    if (isContextLostOrPending())
        return;
    
    WebGLRenderingContextBase::vertexAttribDivisor(index, divisor);
}

void WebGL2RenderingContext::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei instanceCount)
{
    if (isContextLostOrPending())
        return;

    WebGLRenderingContextBase::drawArraysInstanced(mode, first, count, instanceCount);
}

void WebGL2RenderingContext::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dint64 offset, GC3Dsizei instanceCount)
{
    if (isContextLostOrPending())
        return;

    WebGLRenderingContextBase::drawElementsInstanced(mode, count, type, offset, instanceCount);
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
    if (isContextLost())
        return;
    GC3Dsizei n = buffers.size();
    const GC3Denum* bufs = buffers.data();
    if (!m_framebufferBinding) {
        if (n != 1) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "drawBuffers", "more than one buffer");
            return;
        }
        if (bufs[0] != GraphicsContext3D::BACK && bufs[0] != GraphicsContext3D::NONE) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "drawBuffers", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GC3Denum value = (bufs[0] == GraphicsContext3D::BACK) ? GraphicsContext3D::COLOR_ATTACHMENT0 : GraphicsContext3D::NONE;
        graphicsContext3D()->getExtensions()->drawBuffersEXT(1, &value);
        setBackDrawBuffer(bufs[0]);
    } else {
        if (n > getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "drawBuffers", "more than max draw buffers");
            return;
        }
        for (GC3Dsizei i = 0; i < n; ++i) {
            if (bufs[i] != GraphicsContext3D::NONE && bufs[i] != static_cast<GC3Denum>(GraphicsContext3D::COLOR_ATTACHMENT0 + i)) {
                synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "drawBuffers", "COLOR_ATTACHMENTi or NONE");
                return;
            }
        }
        m_framebufferBinding->drawBuffers(buffers);
    }
}

void WebGL2RenderingContext::clearBufferiv(GC3Denum buffer, GC3Dint drawbuffer, Int32Array* value)
{
    UNUSED_PARAM(value);
    switch (buffer) {
    case GraphicsContext3D::COLOR:
        if (drawbuffer < 0 || drawbuffer >= getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clearBufferiv", "buffer index out of range");
            return;
        }
        // TODO: Call clearBufferiv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContext3D::STENCIL:
        if (drawbuffer) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clearBufferiv", "buffer index must be 0");
            return;
        }
        // TODO: Call clearBufferiv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContext3D::DEPTH:
    case GraphicsContext3D::DEPTH_STENCIL:
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "clearBufferiv", "buffer argument must be COLOR or STENCIL");
        break;
    }
}

void WebGL2RenderingContext::clearBufferuiv(GC3Denum buffer, GC3Dint drawbuffer, Uint32Array* value)
{
    UNUSED_PARAM(value);
    switch (buffer) {
    case GraphicsContext3D::COLOR:
        if (drawbuffer < 0 || drawbuffer >= getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clearBufferuiv", "buffer index out of range");
            return;
        }
        // TODO: Call clearBufferuiv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContext3D::DEPTH:
    case GraphicsContext3D::STENCIL:
    case GraphicsContext3D::DEPTH_STENCIL:
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "clearBufferuiv", "buffer argument must be COLOR");
        break;
    }
}

void WebGL2RenderingContext::clearBufferfv(GC3Denum buffer, GC3Dint drawbuffer, Float32Array* value)
{
    UNUSED_PARAM(value);
    switch (buffer) {
    case GraphicsContext3D::COLOR:
        if (drawbuffer < 0 || drawbuffer >= getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clearBufferfv", "buffer index out of range");
            return;
        }
        // TODO: Call clearBufferfv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContext3D::DEPTH:
        if (drawbuffer) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clearBufferfv", "buffer index must be 0");
            return;
        }
        // TODO: Call clearBufferfv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContext3D::STENCIL:
    case GraphicsContext3D::DEPTH_STENCIL:
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "clearBufferfv", "buffer argument must be COLOR OR DEPTH");
        break;
    }
}

void WebGL2RenderingContext::clearBufferfi(GC3Denum buffer, GC3Dint drawbuffer, GC3Dfloat depth, GC3Dint stencil)
{
    UNUSED_PARAM(depth);
    UNUSED_PARAM(stencil);
    switch (buffer) {
    case GraphicsContext3D::DEPTH_STENCIL:
        if (drawbuffer) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clearBufferfv", "buffer index must be 0");
            return;
        }
        // TODO: Call clearBufferfi, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContext3D::COLOR:
    case GraphicsContext3D::DEPTH:
    case GraphicsContext3D::STENCIL:
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "clearBufferfv", "buffer argument must be DEPTH_STENCIL");
        break;
    }
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
    if (isContextLost())
        return 0;
    
    RefPtr<WebGLVertexArrayObject> o = WebGLVertexArrayObject::create(this, WebGLVertexArrayObject::VAOTypeUser);
    addContextObject(o.get());
    return o.release();
}

void WebGL2RenderingContext::deleteVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (!arrayObject || isContextLost())
        return;
    
    if (arrayObject->isDeleted())
        return;
    
    if (!arrayObject->isDefaultObject() && arrayObject == m_boundVertexArrayObject)
        setBoundVertexArrayObject(0);
    
    arrayObject->deleteObject(graphicsContext3D());
}

GC3Dboolean WebGL2RenderingContext::isVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (!arrayObject || isContextLost())
        return 0;
    
    if (!arrayObject->hasEverBeenBound() || !arrayObject->validate(0, this))
        return 0;
    
    return m_context->isVertexArray(arrayObject->object());
}

void WebGL2RenderingContext::bindVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (isContextLost())
        return;
    
    if (arrayObject && (arrayObject->isDeleted() || !arrayObject->validate(0, this) || !m_contextObjects.contains(arrayObject))) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        m_context->bindVertexArray(arrayObject->object());
        
        arrayObject->setHasEverBeenBound();
        setBoundVertexArrayObject(arrayObject);
    } else {
        m_context->bindVertexArray(m_defaultVertexArrayObject->object());
        setBoundVertexArrayObject(m_defaultVertexArrayObject);
    }
}

WebGLExtension* WebGL2RenderingContext::getExtension(const String& name)
{
    if (isContextLostOrPending())
        return nullptr;

    if ((equalIgnoringASCIICase(name, "EXT_texture_filter_anisotropic") || equalIgnoringASCIICase(name, "WEBKIT_EXT_texture_filter_anisotropic"))
        && m_context->getExtensions()->supports("GL_EXT_texture_filter_anisotropic")) {
        if (!m_extTextureFilterAnisotropic) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_texture_filter_anisotropic");
            m_extTextureFilterAnisotropic = std::make_unique<EXTTextureFilterAnisotropic>(this);
        }
        return m_extTextureFilterAnisotropic.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_float")
        && m_context->getExtensions()->supports("GL_OES_texture_float")) {
        if (!m_oesTextureFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float");
            m_oesTextureFloat = std::make_unique<OESTextureFloat>(this);
        }
        return m_oesTextureFloat.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_float_linear")) {
        if (!m_oesTextureFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float_linear");
            m_oesTextureFloatLinear = std::make_unique<OESTextureFloatLinear>(this);
        }
        return m_oesTextureFloatLinear.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_half_float")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float")) {
        if (!m_oesTextureHalfFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float");
            m_oesTextureHalfFloat = std::make_unique<OESTextureHalfFloat>(this);
        }
        return m_oesTextureHalfFloat.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_half_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float_linear")) {
        if (!m_oesTextureHalfFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float_linear");
            m_oesTextureHalfFloatLinear = std::make_unique<OESTextureHalfFloatLinear>(this);
        }
        return m_oesTextureHalfFloatLinear.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_lose_context")) {
        if (!m_webglLoseContext)
            m_webglLoseContext = std::make_unique<WebGLLoseContext>(this);
        return m_webglLoseContext.get();
    }
    if ((equalIgnoringASCIICase(name, "WEBKIT_WEBGL_compressed_texture_atc"))
        && WebGLCompressedTextureATC::supported(this)) {
        if (!m_webglCompressedTextureATC)
            m_webglCompressedTextureATC = std::make_unique<WebGLCompressedTextureATC>(this);
        return m_webglCompressedTextureATC.get();
    }
    if ((equalIgnoringASCIICase(name, "WEBKIT_WEBGL_compressed_texture_pvrtc"))
        && WebGLCompressedTexturePVRTC::supported(this)) {
        if (!m_webglCompressedTexturePVRTC)
            m_webglCompressedTexturePVRTC = std::make_unique<WebGLCompressedTexturePVRTC>(this);
        return m_webglCompressedTexturePVRTC.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_compressed_texture_s3tc")
        && WebGLCompressedTextureS3TC::supported(this)) {
        if (!m_webglCompressedTextureS3TC)
            m_webglCompressedTextureS3TC = std::make_unique<WebGLCompressedTextureS3TC>(this);
        return m_webglCompressedTextureS3TC.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_depth_texture")
        && WebGLDepthTexture::supported(graphicsContext3D())) {
        if (!m_webglDepthTexture) {
            m_context->getExtensions()->ensureEnabled("GL_CHROMIUM_depth_texture");
            m_webglDepthTexture = std::make_unique<WebGLDepthTexture>(this);
        }
        return m_webglDepthTexture.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_debug_renderer_info")) {
        if (!m_webglDebugRendererInfo)
            m_webglDebugRendererInfo = std::make_unique<WebGLDebugRendererInfo>(this);
        return m_webglDebugRendererInfo.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_debug_shaders")
        && m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source")) {
        if (!m_webglDebugShaders)
            m_webglDebugShaders = std::make_unique<WebGLDebugShaders>(this);
        return m_webglDebugShaders.get();
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
    result.append("WEBGL_lose_context");
    if (m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source"))
        result.append("WEBGL_debug_shaders");
    result.append("WEBGL_debug_renderer_info");

    return result;
}

WebGLGetInfo WebGL2RenderingContext::getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (isContextLostOrPending() || !validateFramebufferFuncParameters("getFramebufferAttachmentParameter", target, attachment))
        return WebGLGetInfo();
    
    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "getFramebufferAttachmentParameter", "no framebuffer bound");
        return WebGLGetInfo();
    }
    
    WebGLSharedObject* object = m_framebufferBinding->getAttachmentObject(attachment);
    if (!object) {
        if (pname == GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return WebGLGetInfo(GraphicsContext3D::NONE);
        // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
        // specifies INVALID_OPERATION.
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
    
    ASSERT(object->isTexture() || object->isRenderbuffer());
    if (object->isTexture()) {
        switch (pname) {
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLGetInfo(GraphicsContext3D::TEXTURE);
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return WebGLGetInfo(PassRefPtr<WebGLTexture>(reinterpret_cast<WebGLTexture*>(object)));
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: {
            GC3Dint value = 0;
            m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
            return WebGLGetInfo(value);
        }
        default:
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for texture attachment");
            return WebGLGetInfo();
        }
    } else {
        switch (pname) {
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLGetInfo(GraphicsContext3D::RENDERBUFFER);
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(reinterpret_cast<WebGLRenderbuffer*>(object)));
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING : {
            WebGLRenderbuffer* renderBuffer = reinterpret_cast<WebGLRenderbuffer*>(object);
            GC3Denum renderBufferFormat = renderBuffer->getInternalFormat();
            if (renderBufferFormat == GraphicsContext3D::SRGB8_ALPHA8
                || renderBufferFormat == GraphicsContext3D::COMPRESSED_SRGB8_ETC2
                || renderBufferFormat == GraphicsContext3D::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
                || renderBufferFormat == GraphicsContext3D::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2) {
                return WebGLGetInfo(GraphicsContext3D::SRGB);
            }
            return WebGLGetInfo(GraphicsContext3D::LINEAR);
        }
        default:
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for renderbuffer attachment");
            return WebGLGetInfo();
        }
    }
}

bool WebGL2RenderingContext::validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment)
{
    if (target != GraphicsContext3D::FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    switch (attachment) {
    case GraphicsContext3D::COLOR_ATTACHMENT0:
    case GraphicsContext3D::DEPTH_ATTACHMENT:
    case GraphicsContext3D::STENCIL_ATTACHMENT:
    case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
        break;
    default:
        if (attachment > GraphicsContext3D::COLOR_ATTACHMENT0
            && attachment < static_cast<GC3Denum>(GraphicsContext3D::COLOR_ATTACHMENT0 + getMaxColorAttachments()))
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid attachment");
        return false;
    }
    return true;
}

GC3Dint WebGL2RenderingContext::getMaxDrawBuffers()
{
    if (!m_maxDrawBuffers)
        m_context->getIntegerv(GraphicsContext3D::MAX_DRAW_BUFFERS, &m_maxDrawBuffers);
    return m_maxDrawBuffers;
}

GC3Dint WebGL2RenderingContext::getMaxColorAttachments()
{
    // DrawBuffers requires MAX_COLOR_ATTACHMENTS == MAX_DRAW_BUFFERS
    if (!m_maxColorAttachments)
        m_context->getIntegerv(GraphicsContext3D::MAX_DRAW_BUFFERS, &m_maxColorAttachments);
    return m_maxColorAttachments;
}

void WebGL2RenderingContext::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    if (isContextLostOrPending())
        return;
    if (target != GraphicsContext3D::RENDERBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "renderbufferStorage", "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "renderbufferStorage", "no bound renderbuffer");
        return;
    }
    if (!validateSize("renderbufferStorage", width, height))
        return;
    switch (internalformat) {
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT32F:
    case GraphicsContext3D::DEPTH_COMPONENT24:
    case GraphicsContext3D::RGBA32I:
    case GraphicsContext3D::RGBA32UI:
    case GraphicsContext3D::RGBA16I:
    case GraphicsContext3D::RGBA16UI:
    case GraphicsContext3D::RGBA8:
    case GraphicsContext3D::RGBA8I:
    case GraphicsContext3D::RGBA8UI:
    case GraphicsContext3D::RGB10_A2:
    case GraphicsContext3D::RGB10_A2UI:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RG32I:
    case GraphicsContext3D::RG32UI:
    case GraphicsContext3D::RG16I:
    case GraphicsContext3D::RG16UI:
    case GraphicsContext3D::RG8:
    case GraphicsContext3D::RG8I:
    case GraphicsContext3D::RG8UI:
    case GraphicsContext3D::R32I:
    case GraphicsContext3D::R32UI:
    case GraphicsContext3D::R16I:
    case GraphicsContext3D::R16UI:
    case GraphicsContext3D::R8:
    case GraphicsContext3D::R8I:
    case GraphicsContext3D::R8UI:
    case GraphicsContext3D::RGB5_A1:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::STENCIL_INDEX8:
    case GraphicsContext3D::SRGB8_ALPHA8:
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContext3D::DEPTH32F_STENCIL8:
    case GraphicsContext3D::DEPTH24_STENCIL8:
        if (!isDepthStencilSupported()) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
            return;
        }
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setIsValid(isDepthStencilSupported());
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
        return;
    }
    applyStencilTest();
}

void WebGL2RenderingContext::hint(GC3Denum target, GC3Denum mode)
{
    if (isContextLostOrPending())
        return;
    bool isValid = false;
    switch (target) {
    case GraphicsContext3D::GENERATE_MIPMAP_HINT:
    case GraphicsContext3D::FRAGMENT_SHADER_DERIVATIVE_HINT:
        isValid = true;
        break;
    }
    if (!isValid) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "hint", "invalid target");
        return;
    }
    m_context->hint(target, mode);
}

void WebGL2RenderingContext::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    if (isContextLostOrPending())
        return;
    RefPtr<WebGLContextAttributes> attributes = getContextAttributes();
    GC3Denum bufferFormat = attributes->alpha() ? GraphicsContext3D::RGBA : GraphicsContext3D::RGB;
    if (!validateTexFuncParameters("copyTexImage2D", CopyTexImage, target, level, internalformat, width, height, border, bufferFormat, GraphicsContext3D::UNSIGNED_BYTE))
        return;
    if (!validateSettableTexFormat("copyTexImage2D", internalformat))
        return;
    WebGLTexture* tex = validateTextureBinding("copyTexImage2D", target, true);
    if (!tex)
        return;
    if (!isTexInternalFormatColorBufferCombinationValid(internalformat, getBoundFramebufferColorFormat())) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "copyTexImage2D", "framebuffer is incompatible format");
        return;
    }
    if (!isGLES2NPOTStrict() && level && WebGLTexture::isNPOT(width, height)) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "copyTexImage2D", "level > 0 not power of 2");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), !isResourceSafe(), &reason)) {
        synthesizeGLError(GraphicsContext3D::INVALID_FRAMEBUFFER_OPERATION, "copyTexImage2D", reason);
        return;
    }
    clearIfComposited();
    if (isResourceSafe())
        m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    else {
        GC3Dint clippedX, clippedY;
        GC3Dsizei clippedWidth, clippedHeight;
        if (clip2D(x, y, width, height, getBoundFramebufferWidth(), getBoundFramebufferHeight(), &clippedX, &clippedY, &clippedWidth, &clippedHeight)) {
            m_context->texImage2DResourceSafe(target, level, internalformat, width, height, border,
                internalformat, GraphicsContext3D::UNSIGNED_BYTE, m_unpackAlignment);
            if (clippedWidth > 0 && clippedHeight > 0) {
                m_context->copyTexSubImage2D(target, level, clippedX - x, clippedY - y,
                    clippedX, clippedY, clippedWidth, clippedHeight);
            }
        } else
            m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    }
    // FIXME: if the framebuffer is not complete, none of the below should be executed.
    tex->setLevelInfo(target, level, internalformat, width, height, GraphicsContext3D::UNSIGNED_BYTE);
}

void WebGL2RenderingContext::texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, const void* pixels, ExceptionCode& ec)
{
    ec = 0;
    ASSERT(!isContextLost());
    if (!validateTexFuncParameters("texSubImage2D", TexSubImage, target, level, internalformat, width, height, 0, format, type))
        return;
    ASSERT(validateSize("texSubImage2D", xoffset, yoffset));
    ASSERT(validateSettableTexFormat("texSubImage2D", format));
    WebGLTexture* tex = validateTextureBinding("texSubImage2D", target, true);
    if (!tex) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT((xoffset + width) >= 0);
    ASSERT((yoffset + height) >= 0);
    ASSERT(tex->getWidth(target, level) >= (xoffset + width));
    ASSERT(tex->getHeight(target, level) >= (yoffset + height));
    ASSERT(tex->getInternalFormat(target, level) == format);
    ASSERT(tex->getType(target, level) == type);
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void WebGL2RenderingContext::texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image* image, GraphicsContext3D::ImageHtmlDomSource domSource, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    Vector<uint8_t> data;
    GraphicsContext3D::ImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GraphicsContext3D::NONE);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "texSubImage2D", "bad image");
        return;
    }
    GraphicsContext3D::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContext3D::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    const void* imagePixelData = imageExtractor.imagePixelData();
    
    bool needConversion = true;
    if (type == GraphicsContext3D::UNSIGNED_BYTE && sourceDataFormat == GraphicsContext3D::DataFormatRGBA8 && format == GraphicsContext3D::RGBA && alphaOp == GraphicsContext3D::AlphaDoNothing && !flipY)
        needConversion = false;
    else {
        if (!m_context->packImageData(image, imagePixelData, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), imageExtractor.imageSourceUnpackAlignment(), data)) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "texImage2D", "bad image data");
            return;
        }
    }
    
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    
    WebGLTexture* tex = validateTextureBinding("texSubImage2D", target, true);
    GC3Denum internalformat = tex->getInternalFormat(target, level);
    texSubImage2DBase(target, level, xoffset, yoffset, image->width(), image->height(), internalformat, format, type,  needConversion ? data.data() : imagePixelData, ec);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGL2RenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, ArrayBufferView* pixels, ExceptionCode& ec)
{
    if (isContextLostOrPending() || !validateTexFuncData("texSubImage2D", level, width, height, GraphicsContext3D::NONE, format, type, pixels, NullNotAllowed) || !validateTexFunc("texSubImage2D", TexSubImage, SourceArrayBufferView, target, level, GraphicsContext3D::NONE, width, height, 0, format, type, xoffset, yoffset))
        return;
    
    void* data = pixels->baseAddress();
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (data && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!m_context->extractTextureData(width, height, format, type,
            m_unpackAlignment,
            m_unpackFlipY, m_unpackPremultiplyAlpha,
            data,
            tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    WebGLTexture* tex = validateTextureBinding("texSubImage2D", target, true);
    GC3Denum internalformat = tex->getInternalFormat(target, level);
    texSubImage2DBase(target, level, xoffset, yoffset, width, height, internalformat, format, type, data, ec);
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGL2RenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, ImageData* pixels, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !pixels || !validateTexFunc("texSubImage2D", TexSubImage, SourceImageData, target, level, GraphicsContext3D::NONE,  pixels->width(), pixels->height(), 0, format, type, xoffset, yoffset))
        return;
    
    Vector<uint8_t> data;
    bool needConversion = true;
    // The data from ImageData is always of format RGBA8.
    // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
    if (format == GraphicsContext3D::RGBA && type == GraphicsContext3D::UNSIGNED_BYTE && !m_unpackFlipY && !m_unpackPremultiplyAlpha)
        needConversion = false;
    else {
        if (!m_context->extractImageData(pixels, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "texSubImage2D", "bad image data");
            return;
        }
    }
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    
    WebGLTexture* tex = validateTextureBinding("texSubImage2D", target, true);
    GC3Denum internalformat = tex->getInternalFormat(target, level);
    texSubImage2DBase(target, level, xoffset, yoffset, pixels->width(), pixels->height(), internalformat, format, type, needConversion ? data.data() : pixels->data()->data(), ec);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGL2RenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, HTMLImageElement* image, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !validateHTMLImageElement("texSubImage2D", image, ec))
        return;
    
    RefPtr<Image> imageForRender = image->cachedImage()->imageForRenderer(image->renderer());
    if (!imageForRender)
        return;

    if (imageForRender->isSVGImage())
        imageForRender = drawImageIntoBuffer(*imageForRender, image->width(), image->height(), 1);
    
    if (!imageForRender || !validateTexFunc("texSubImage2D", TexSubImage, SourceHTMLImageElement, target, level, GraphicsContext3D::NONE, imageForRender->width(), imageForRender->height(), 0, format, type, xoffset, yoffset))
        return;
    
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, imageForRender.get(), GraphicsContext3D::HtmlDomImage, m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

void WebGL2RenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
    GC3Denum format, GC3Denum type, HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !validateHTMLCanvasElement("texSubImage2D", canvas, ec)
        || !validateTexFunc("texSubImage2D", TexSubImage, SourceHTMLCanvasElement, target, level, GraphicsContext3D::NONE, canvas->width(), canvas->height(), 0, format, type, xoffset, yoffset))
        return;
    
    RefPtr<ImageData> imageData = canvas->getImageData();
    if (imageData)
        texSubImage2D(target, level, xoffset, yoffset, format, type, imageData.get(), ec);
    else
        texSubImage2DImpl(target, level, xoffset, yoffset, format, type, canvas->copiedImage(), GraphicsContext3D::HtmlDomCanvas, m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

#if ENABLE(VIDEO)
void WebGL2RenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, HTMLVideoElement* video, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !validateHTMLVideoElement("texSubImage2D", video, ec)
        || !validateTexFunc("texSubImage2D", TexSubImage, SourceHTMLVideoElement, target, level, GraphicsContext3D::NONE, video->videoWidth(), video->videoHeight(), 0, format, type, xoffset, yoffset))
        return;
    
    RefPtr<Image> image = videoFrameToImage(video, ImageBuffer::fastCopyImageMode(), ec);
    if (!image)
        return;
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, image.get(), GraphicsContext3D::HtmlDomVideo, m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}
#endif

bool WebGL2RenderingContext::validateTexFuncParameters(const char* functionName, TexFuncValidationFunctionType functionType, GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type)
{
    if (functionType == CopyTexImage) {
        GC3Denum framebufferInternalFormat = 0;
        WebGLSharedObject* object = m_framebufferBinding->getAttachmentObject(GraphicsContext3D::COLOR_ATTACHMENT0);
        if (object->isTexture()) {
            WebGLTexture* texture = reinterpret_cast<WebGLTexture*>(object);
            framebufferInternalFormat = baseInternalFormatFromInternalFormat(texture->getInternalFormat(GraphicsContext3D::TEXTURE_2D, 0));
        } else if (object->isRenderbuffer()) {
            WebGLRenderbuffer* renderBuffer = reinterpret_cast<WebGLRenderbuffer*>(object);
            framebufferInternalFormat = baseInternalFormatFromInternalFormat(renderBuffer->getInternalFormat());
        }
        
        GC3Denum baseTextureInternalFormat = baseInternalFormatFromInternalFormat(internalformat);
        bool validFormatCombination = true;
        switch (framebufferInternalFormat) {
        case GraphicsContext3D::RED:
            if (baseTextureInternalFormat != GraphicsContext3D::LUMINANCE && baseTextureInternalFormat != GraphicsContext3D::RED)
                validFormatCombination = false;
            break;
        case GraphicsContext3D::RG:
            if (baseTextureInternalFormat != GraphicsContext3D::LUMINANCE && baseTextureInternalFormat != GraphicsContext3D::RED && baseTextureInternalFormat != GraphicsContext3D::RG)
                validFormatCombination = false;
            break;
        case GraphicsContext3D::RGB:
            if (baseTextureInternalFormat != GraphicsContext3D::LUMINANCE && baseTextureInternalFormat != GraphicsContext3D::RED && baseTextureInternalFormat != GraphicsContext3D::RG && baseTextureInternalFormat != GraphicsContext3D::RGB)
                validFormatCombination = false;
            break;
        case GraphicsContext3D::RGBA:
            if (baseTextureInternalFormat != GraphicsContext3D::ALPHA && baseTextureInternalFormat != GraphicsContext3D::LUMINANCE && baseTextureInternalFormat != GraphicsContext3D::LUMINANCE_ALPHA && baseTextureInternalFormat != GraphicsContext3D::RED && baseTextureInternalFormat != GraphicsContext3D::RG && baseTextureInternalFormat != GraphicsContext3D::RGB && baseTextureInternalFormat != GraphicsContext3D::RGBA)
                validFormatCombination = false;
            break;
        case GraphicsContext3D::DEPTH_COMPONENT:
            validFormatCombination = false;
            break;
        case GraphicsContext3D::DEPTH_STENCIL:
            validFormatCombination = false;
            break;
        }
        
        if (!validFormatCombination) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "copyTexImage: invalid combination of framebuffer and texture formats");
            return false;
        }

        ExceptionCode ec;
        bool isSRGB = (getFramebufferAttachmentParameter(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, ec).getInt() == GraphicsContext3D::SRGB);
        if (isSRGB != (framebufferInternalFormat == GraphicsContext3D::SRGB8 || framebufferInternalFormat == GraphicsContext3D::SRGB8_ALPHA8)) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "framebuffer attachment color encoding and internal format do not match");
        return false;
        }
    }
    
    // We absolutely have to validate the format and type combination.
    // The texImage2D entry points taking HTMLImage, etc. will produce
    // temporary data based on this combination, so it must be legal.
    if (!validateTexFuncFormatAndType(functionName, internalformat, format, type, level) || !validateTexFuncLevel(functionName, target, level))
        return false;
    
    if (width < 0 || height < 0) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }
    
    GC3Dint maxTextureSizeForLevel = pow(2.0, m_maxTextureLevel - 1 - level);
    switch (target) {
    case GraphicsContext3D::TEXTURE_2D:
        if (width > maxTextureSizeForLevel || height > maxTextureSizeForLevel) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width or height out of range");
            return false;
        }
        break;
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (functionType != TexSubImage && width != height) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width != height for cube map");
            return false;
        }
        // No need to check height here. For texImage width == height.
        // For texSubImage that will be checked when checking yoffset + height is in range.
        if (width > maxTextureSizeForLevel) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width or height out of range for cube map");
            return false;
        }
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    
    if (border) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "border != 0");
        return false;
    }
    
    return true;
}
    
bool WebGL2RenderingContext::validateTexFuncFormatAndType(const char* functionName, GC3Denum internalformat, GC3Denum format, GC3Denum type, GC3Dint level)
{
    // Verify that a valid format has been provided.
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA_INTEGER:
    case GraphicsContext3D::RG:
    case GraphicsContext3D::RG_INTEGER:
    case GraphicsContext3D::RED_INTEGER:
        break;
    case GraphicsContext3D::DEPTH_STENCIL:
    case GraphicsContext3D::DEPTH_COMPONENT:
        if (m_webglDepthTexture)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "depth texture formats not enabled");
        return false;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture format");
        return false;
    }
    
    // Verify that a valid type has been provided.
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
    case GraphicsContext3D::BYTE:
    case GraphicsContext3D::UNSIGNED_SHORT:
    case GraphicsContext3D::SHORT:
    case GraphicsContext3D::UNSIGNED_INT:
    case GraphicsContext3D::INT:
    case GraphicsContext3D::UNSIGNED_INT_2_10_10_10_REV:
    case GraphicsContext3D::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContext3D::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        break;
    case GraphicsContext3D::FLOAT:
        if (m_oesTextureFloat)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    case GraphicsContext3D::HALF_FLOAT:
    case GraphicsContext3D::HALF_FLOAT_OES:
        if (m_oesTextureHalfFloat)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    case GraphicsContext3D::UNSIGNED_INT_24_8:
    case GraphicsContext3D::FLOAT_32_UNSIGNED_INT_24_8_REV:
        if (isDepthStencilSupported())
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    }
    
    // Veryify that a valid internal format has been provided.
    switch (internalformat) {
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::RGBA32I:
    case GraphicsContext3D::RGBA32UI:
    case GraphicsContext3D::RGBA16I:
    case GraphicsContext3D::RGBA16UI:
    case GraphicsContext3D::RGBA8:
    case GraphicsContext3D::RGBA8I:
    case GraphicsContext3D::RGBA8UI:
    case GraphicsContext3D::RGB10_A2:
    case GraphicsContext3D::RGB10_A2UI:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RG32I:
    case GraphicsContext3D::RG32UI:
    case GraphicsContext3D::RG16I:
    case GraphicsContext3D::RG16UI:
    case GraphicsContext3D::RG8:
    case GraphicsContext3D::RG8I:
    case GraphicsContext3D::RG8UI:
    case GraphicsContext3D::R32I:
    case GraphicsContext3D::R32UI:
    case GraphicsContext3D::R16I:
    case GraphicsContext3D::R16UI:
    case GraphicsContext3D::R8:
    case GraphicsContext3D::R8I:
    case GraphicsContext3D::R8UI:
    case GraphicsContext3D::RGB5_A1:
    case GraphicsContext3D::RGB8:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::RGBA32F:
    case GraphicsContext3D::RGBA16F:
    case GraphicsContext3D::RGBA8_SNORM:
    case GraphicsContext3D::RGB32F:
    case GraphicsContext3D::RGB32I:
    case GraphicsContext3D::RGB32UI:
    case GraphicsContext3D::RGB16F:
    case GraphicsContext3D::RGB16I:
    case GraphicsContext3D::RGB16UI:
    case GraphicsContext3D::RGB8_SNORM:
    case GraphicsContext3D::RGB8I:
    case GraphicsContext3D::RGB8UI:
    case GraphicsContext3D::SRGB8:
    case GraphicsContext3D::SRGB8_ALPHA8:
    case GraphicsContext3D::R11F_G11F_B10F:
    case GraphicsContext3D::RGB9_E5:
    case GraphicsContext3D::RG32F:
    case GraphicsContext3D::RG16F:
    case GraphicsContext3D::RG8_SNORM:
    case GraphicsContext3D::R32F:
    case GraphicsContext3D::R16F:
    case GraphicsContext3D::R8_SNORM:
    case GraphicsContext3D::STENCIL_INDEX8:
        break;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT32F:
    case GraphicsContext3D::DEPTH_COMPONENT24:
        if (m_webglDepthTexture)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "depth texture formats not enabled");
        return false;
    case GraphicsContext3D::DEPTH32F_STENCIL8:
    case GraphicsContext3D::DEPTH24_STENCIL8:
        if (isDepthStencilSupported())
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture internal format");
        return false;
    case GraphicsContext3D::NONE:
        // When calling validateTexFuncFormatAndType with internalformat == GraphicsContext3D::NONE, the intent is
        // only to check for whether or not the format and type are valid types, which we have already done at this point.
        return true;
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture internal format");
        return false;
    }

    // Verify that the combination of format, internalformat and type is supported.
    switch (format) {
    case GraphicsContext3D::RGBA:
        if (type == GraphicsContext3D::UNSIGNED_BYTE
            && (internalformat == GraphicsContext3D::RGBA || internalformat == GraphicsContext3D::RGBA8 || internalformat == GraphicsContext3D::RGB5_A1 || internalformat == GraphicsContext3D::RGBA4 || internalformat == GraphicsContext3D::SRGB8_ALPHA8))
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::RGBA8_SNORM)
            break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4
            && (internalformat == GraphicsContext3D::RGBA || internalformat == GraphicsContext3D::RGBA4))
            break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1
            && (internalformat == GraphicsContext3D::RGBA || internalformat == GraphicsContext3D::RGB5_A1))
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT_2_10_10_10_REV
            && (internalformat == GraphicsContext3D::RGB10_A2 || internalformat == GraphicsContext3D::RGB5_A1))
            break;
        if ((type == GraphicsContext3D::HALF_FLOAT || type == GraphicsContext3D::HALF_FLOAT_OES) && internalformat == GraphicsContext3D::RGBA16F)
            break;
        if (type == GraphicsContext3D::FLOAT
            && (internalformat == GraphicsContext3D::RGBA32F || internalformat == GraphicsContext3D::RGBA16F))
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RGBA_INTEGER:
        if (type == GraphicsContext3D::UNSIGNED_BYTE && internalformat == GraphicsContext3D::RGBA8UI)
        break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::RGBA8I)
        break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT && internalformat == GraphicsContext3D::RGBA16UI)
        break;
        if (type == GraphicsContext3D::SHORT && internalformat == GraphicsContext3D::RGBA16I)
        break;
        if (type == GraphicsContext3D::UNSIGNED_INT && internalformat == GraphicsContext3D::RGBA32UI)
        break;
        if (type == GraphicsContext3D::INT && internalformat == GraphicsContext3D::RGBA32I)
        break;
        if (type == GraphicsContext3D::UNSIGNED_INT_2_10_10_10_REV && internalformat == GraphicsContext3D::RGB10_A2UI)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RGB:
        if (type == GraphicsContext3D::UNSIGNED_BYTE
            && (internalformat == GraphicsContext3D::RGB || internalformat == GraphicsContext3D::RGB8 || internalformat == GraphicsContext3D::RGB565
            || internalformat == GraphicsContext3D::SRGB8))
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::RGB8_SNORM)
            break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT_5_6_5
            && (internalformat == GraphicsContext3D::RGB || internalformat == GraphicsContext3D::RGB565))
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT_10F_11F_11F_REV && internalformat == GraphicsContext3D::R11F_G11F_B10F)
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT_5_9_9_9_REV && internalformat == GraphicsContext3D::RGB9_E5)
            break;
        if ((type == GraphicsContext3D::HALF_FLOAT || type == GraphicsContext3D::HALF_FLOAT_OES)
            && (internalformat == GraphicsContext3D::RGB16F || internalformat == GraphicsContext3D::R11F_G11F_B10F || internalformat == GraphicsContext3D::RGB9_E5))
            break;
        if (type == GraphicsContext3D::FLOAT
            && (internalformat == GraphicsContext3D::RGB32F || internalformat == GraphicsContext3D::RGB16F || internalformat == GraphicsContext3D::R11F_G11F_B10F || internalformat == GraphicsContext3D::RGB9_E5))
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RGB_INTEGER:
        if (type == GraphicsContext3D::UNSIGNED_BYTE && internalformat == GraphicsContext3D::RGB8UI)
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::RGB8I)
            break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT && internalformat == GraphicsContext3D::RGB16UI)
            break;
        if (type == GraphicsContext3D::SHORT && internalformat == GraphicsContext3D::RGB16I)
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT && internalformat == GraphicsContext3D::RGB32UI)
            break;
        if (type == GraphicsContext3D::INT && internalformat == GraphicsContext3D::RGB32I)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RG:
        if (type == GraphicsContext3D::UNSIGNED_BYTE && internalformat == GraphicsContext3D::RG8)
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::RG8_SNORM)
            break;
        if ((type == GraphicsContext3D::HALF_FLOAT || type == GraphicsContext3D::HALF_FLOAT_OES) && internalformat == GraphicsContext3D::RG16F)
            break;
        if (type == GraphicsContext3D::FLOAT
            && (internalformat == GraphicsContext3D::RG32F || internalformat == GraphicsContext3D::RG16F))
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RG_INTEGER:
        if (type == GraphicsContext3D::UNSIGNED_BYTE && internalformat == GraphicsContext3D::RG8UI)
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::RG8I)
            break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT && internalformat == GraphicsContext3D::RG16UI)
            break;
        if (type == GraphicsContext3D::SHORT && internalformat == GraphicsContext3D::RG16I)
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT && internalformat == GraphicsContext3D::RG32UI)
            break;
        if (type == GraphicsContext3D::INT && internalformat == GraphicsContext3D::RG32I)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RED:
        if (type == GraphicsContext3D::UNSIGNED_BYTE && internalformat == GraphicsContext3D::R8)
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::R8_SNORM)
            break;
        if ((type == GraphicsContext3D::HALF_FLOAT || type == GraphicsContext3D::HALF_FLOAT_OES) && internalformat == GraphicsContext3D::R16F)
            break;
        if (type == GraphicsContext3D::FLOAT
            && (internalformat == GraphicsContext3D::R32F || internalformat == GraphicsContext3D::R16F))
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::RED_INTEGER:
        if (type == GraphicsContext3D::UNSIGNED_BYTE && internalformat == GraphicsContext3D::R8UI)
            break;
        if (type == GraphicsContext3D::BYTE && internalformat == GraphicsContext3D::R8I)
            break;
        if (type == GraphicsContext3D::UNSIGNED_SHORT && internalformat == GraphicsContext3D::R16UI)
            break;
        if (type == GraphicsContext3D::SHORT && internalformat == GraphicsContext3D::R16I)
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT && internalformat == GraphicsContext3D::R32UI)
            break;
        if (type == GraphicsContext3D::INT && internalformat == GraphicsContext3D::R32I)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::DEPTH_COMPONENT:
        if (!m_webglDepthTexture) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid format. DEPTH_COMPONENT not enabled");
            return false;
        }
        if (type == GraphicsContext3D::UNSIGNED_SHORT && internalformat == GraphicsContext3D::DEPTH_COMPONENT16)
            break;
        if (type == GraphicsContext3D::UNSIGNED_INT
            && (internalformat == GraphicsContext3D::DEPTH_COMPONENT24 || internalformat == GraphicsContext3D::DEPTH_COMPONENT16))
            break;
        if (type == GraphicsContext3D::FLOAT && internalformat == GraphicsContext3D::DEPTH_COMPONENT32F)
            break;
        if (level > 0) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "level must be 0 for DEPTH_COMPONENT format");
            return false;
        }
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::DEPTH_STENCIL:
        if (!m_webglDepthTexture || !isDepthStencilSupported()) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid format. DEPTH_STENCIL not enabled");
            return false;
        }
        if (type == GraphicsContext3D::UNSIGNED_INT_24_8 && internalformat == GraphicsContext3D::DEPTH24_STENCIL8)
            break;
        if (type == GraphicsContext3D::FLOAT_32_UNSIGNED_INT_24_8_REV && internalformat == GraphicsContext3D::DEPTH32F_STENCIL8)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
        if ((type == GraphicsContext3D::UNSIGNED_BYTE || type == GraphicsContext3D::HALF_FLOAT_OES || type == GraphicsContext3D::HALF_FLOAT || type == GraphicsContext3D::FLOAT) && internalformat == format)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid format, internalformat, and type combination");
        return false;
    default:
        ASSERT_NOT_REACHED();
    }

    return true;
}

bool WebGL2RenderingContext::validateTexFuncData(const char* functionName, GC3Dint level,
    GC3Dsizei width, GC3Dsizei height,
    GC3Denum internalformat, GC3Denum format, GC3Denum type,
    ArrayBufferView* pixels,
    NullDisposition disposition)
{
    if (!pixels) {
        if (disposition == NullAllowed)
            return true;
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "no pixels");
        return false;
    }

    if (!validateTexFuncFormatAndType(functionName, internalformat, format, type, level))
        return false;
    if (!validateSettableTexFormat(functionName, format))
        return false;
    
    switch (type) {
    case GraphicsContext3D::BYTE:
        if (pixels->getType() != JSC::TypeInt8) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type BYTE but ArrayBufferView not Int8Array");
            return false;
        }
        break;
    case GraphicsContext3D::UNSIGNED_BYTE:
        if (pixels->getType() != JSC::TypeUint8) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
            return false;
        }
        break;
    case GraphicsContext3D::SHORT:
        if (pixels->getType() != JSC::TypeInt16) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type SHORT but ArrayBufferView not Int16Array");
            return false;
        }
        break;
    case GraphicsContext3D::UNSIGNED_SHORT:
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        if (pixels->getType() != JSC::TypeUint16) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
            return false;
        }
        break;
    case GraphicsContext3D::INT:
        if (pixels->getType() != JSC::TypeInt32) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type INT but ArrayBufferView not Int32Array");
            return false;
        }
        break;
    case GraphicsContext3D::UNSIGNED_INT:
    case GraphicsContext3D::UNSIGNED_INT_2_10_10_10_REV:
    case GraphicsContext3D::UNSIGNED_INT_10F_11F_11F_REV:
        if (pixels->getType() != JSC::TypeUint32) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type UNSIGNED_INT but ArrayBufferView not Uint32Array");
            return false;
        }
        break;
    case GraphicsContext3D::HALF_FLOAT:
    case GraphicsContext3D::FLOAT:
        if (pixels->getType() != JSC::TypeFloat32) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type FLOAT but ArrayBufferView not Float32Array");
            return false;
        }
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        // As per the specification, ArrayBufferView should be null when
        // OES_texture_half_float is enabled.
        if (pixels) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type HALF_FLOAT_OES but ArrayBufferView is not NULL");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    unsigned totalBytesRequired;
    GC3Denum error = m_context->computeImageSizeInBytes(format, type, width, height, m_unpackAlignment, &totalBytesRequired, 0);
    if (error != GraphicsContext3D::NO_ERROR) {
        synthesizeGLError(error, functionName, "invalid texture dimensions");
        return false;
    }
    if (pixels->byteLength() < totalBytesRequired) {
        if (m_unpackAlignment != 1) {
            m_context->computeImageSizeInBytes(format, type, width, height, 1, &totalBytesRequired, 0);
            if (pixels->byteLength() == totalBytesRequired) {
                synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request with UNPACK_ALIGNMENT > 1");
                return false;
            }
        }
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request");
        return false;
    }
    return true;
}

GC3Denum WebGL2RenderingContext::baseInternalFormatFromInternalFormat(GC3Denum internalformat)
{
    // Handles sized, unsized, and compressed internal formats.
    switch (internalformat) {
    case GraphicsContext3D::R8:
    case GraphicsContext3D::R8_SNORM:
    case GraphicsContext3D::R16F:
    case GraphicsContext3D::R32F:
    case GraphicsContext3D::R8I:
    case GraphicsContext3D::R8UI:
    case GraphicsContext3D::R16I:
    case GraphicsContext3D::R16UI:
    case GraphicsContext3D::R32I:
    case GraphicsContext3D::R32UI:
    case GraphicsContext3D::COMPRESSED_R11_EAC:
    case GraphicsContext3D::COMPRESSED_SIGNED_R11_EAC:
        return GraphicsContext3D::RED;
    case GraphicsContext3D::RG8:
    case GraphicsContext3D::RG8_SNORM:
    case GraphicsContext3D::RG16F:
    case GraphicsContext3D::RG32F:
    case GraphicsContext3D::RG8I:
    case GraphicsContext3D::RG8UI:
    case GraphicsContext3D::RG16I:
    case GraphicsContext3D::RG16UI:
    case GraphicsContext3D::RG32I:
    case GraphicsContext3D::RG32UI:
    case GraphicsContext3D::COMPRESSED_RG11_EAC:
    case GraphicsContext3D::COMPRESSED_SIGNED_RG11_EAC:
        return GraphicsContext3D::RG;
    case GraphicsContext3D::RGB8:
    case GraphicsContext3D::RGB8_SNORM:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::SRGB8:
    case GraphicsContext3D::RGB16F:
    case GraphicsContext3D::RGB32F:
    case GraphicsContext3D::RGB8I:
    case GraphicsContext3D::RGB8UI:
    case GraphicsContext3D::RGB16I:
    case GraphicsContext3D::RGB16UI:
    case GraphicsContext3D::RGB32I:
    case GraphicsContext3D::RGB32UI:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::COMPRESSED_RGB8_ETC2:
    case GraphicsContext3D::COMPRESSED_SRGB8_ETC2:
        return GraphicsContext3D::RGB;
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
    case GraphicsContext3D::RGBA8:
    case GraphicsContext3D::RGBA8_SNORM:
    case GraphicsContext3D::RGB10_A2:
    case GraphicsContext3D::RGB10_A2UI:
    case GraphicsContext3D::SRGB8_ALPHA8:
    case GraphicsContext3D::RGBA16F:
    case GraphicsContext3D::RGBA32F:
    case GraphicsContext3D::RGBA8I:
    case GraphicsContext3D::RGBA8UI:
    case GraphicsContext3D::RGBA16I:
    case GraphicsContext3D::RGBA16UI:
    case GraphicsContext3D::RGBA32I:
    case GraphicsContext3D::RGBA32UI:
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GraphicsContext3D::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GraphicsContext3D::COMPRESSED_RGBA8_ETC2_EAC:
    case GraphicsContext3D::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        return GraphicsContext3D::RGBA;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT24:
    case GraphicsContext3D::DEPTH_COMPONENT32F:
        return GraphicsContext3D::DEPTH_COMPONENT;
    case GraphicsContext3D::DEPTH24_STENCIL8:
    case GraphicsContext3D::DEPTH32F_STENCIL8:
        return GraphicsContext3D::DEPTH_STENCIL;
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::ALPHA:
        return internalformat;
    default:
        ASSERT_NOT_REACHED();
        return GraphicsContext3D::NONE;
    }
}

bool WebGL2RenderingContext::isIntegerFormat(GC3Denum internalformat)
{
    switch (baseInternalFormatFromInternalFormat(internalformat)) {
    case GraphicsContext3D::RED_INTEGER:
    case GraphicsContext3D::RG_INTEGER:
    case GraphicsContext3D::RGB_INTEGER:
    case GraphicsContext3D::RGBA_INTEGER:
        return true;
    }
    return false;
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
        return WebGLGetInfo(Uint32Array::create(m_compressedTextureFormats.data(), m_compressedTextureFormats.size()).release());
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
    case GraphicsContext3D::VERTEX_ARRAY_BINDING: {
        if (!m_boundVertexArrayObject->isDefaultObject())
            return WebGLGetInfo(PassRefPtr<WebGLVertexArrayObject>(static_cast<WebGLVertexArrayObject*>(m_boundVertexArrayObject.get())));
        return WebGLGetInfo();
        }
        break;
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
    case GraphicsContext3D::DRAW_BUFFER15: {
        GC3Dint value = GraphicsContext3D::NONE;
        if (m_framebufferBinding)
            value = m_framebufferBinding->getDrawBuffer(pname);
        else // emulated backbuffer
            value = m_backDrawBuffer;
        return WebGLGetInfo(value);
        }
    case GraphicsContext3D::COPY_READ_BUFFER:
    case GraphicsContext3D::COPY_WRITE_BUFFER:
    case GraphicsContext3D::PIXEL_PACK_BUFFER_BINDING:   
    case GraphicsContext3D::PIXEL_UNPACK_BUFFER_BINDING:
    case GraphicsContext3D::READ_BUFFER:
    case GraphicsContext3D::SAMPLER_BINDING:
    case GraphicsContext3D::TEXTURE_BINDING_2D_ARRAY:
    case GraphicsContext3D::TEXTURE_BINDING_3D:
    case GraphicsContext3D::READ_FRAMEBUFFER_BINDING:
    case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GraphicsContext3D::UNIFORM_BUFFER_BINDING:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "parameter name not yet supported");
        return WebGLGetInfo();
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

bool WebGL2RenderingContext::validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired)
{
    // Performs conservative validation by caching a maximum index of
    // the given type per element array buffer. If all of the bound
    // array buffers have enough elements to satisfy that maximum
    // index, skips the expensive per-draw-call iteration in
    // validateIndexArrayPrecise.
    
    RefPtr<WebGLBuffer> elementArrayBuffer = m_boundVertexArrayObject->getElementArrayBuffer();
    
    if (!elementArrayBuffer)
        return false;
    
    GC3Dsizeiptr numElements = elementArrayBuffer->byteLength();
    // The case count==0 is already dealt with in drawElements before validateIndexArrayConservative.
    if (!numElements)
        return false;
    const ArrayBuffer* buffer = elementArrayBuffer->elementArrayBuffer();
    ASSERT(buffer);
    
    int maxIndex = elementArrayBuffer->getCachedMaxIndex(type);
    if (maxIndex < 0) {
        // Compute the maximum index in the entire buffer for the given type of index.
        switch (type) {
        case GraphicsContext3D::UNSIGNED_BYTE: {
            const GC3Dubyte* p = static_cast<const GC3Dubyte*>(buffer->data());
            for (GC3Dsizeiptr i = 0; i < numElements; i++)
                maxIndex = std::max(maxIndex, static_cast<int>(p[i]));
            break;
        }
        case GraphicsContext3D::UNSIGNED_SHORT: {
            numElements /= sizeof(GC3Dushort);
            const GC3Dushort* p = static_cast<const GC3Dushort*>(buffer->data());
            for (GC3Dsizeiptr i = 0; i < numElements; i++)
                maxIndex = std::max(maxIndex, static_cast<int>(p[i]));
            break;
        }
        case GraphicsContext3D::UNSIGNED_INT: {
            numElements /= sizeof(GC3Duint);
            const GC3Duint* p = static_cast<const GC3Duint*>(buffer->data());
            for (GC3Dsizeiptr i = 0; i < numElements; i++)
                maxIndex = std::max(maxIndex, static_cast<int>(p[i]));
            break;
        }
        default:
            return false;
        }
        elementArrayBuffer->setCachedMaxIndex(type, maxIndex);
    }
    
    if (maxIndex >= 0) {
        // The number of required elements is one more than the maximum
        // index that will be accessed.
        numElementsRequired = maxIndex + 1;
        return true;
    }
    
    return false;
}

bool WebGL2RenderingContext::validateBlendEquation(const char* functionName, GC3Denum mode)
{
    switch (mode) {
    case GraphicsContext3D::FUNC_ADD:
    case GraphicsContext3D::FUNC_SUBTRACT:
    case GraphicsContext3D::FUNC_REVERSE_SUBTRACT:
    case GraphicsContext3D::MIN:
    case GraphicsContext3D::MAX:
        return true;
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid mode");
        return false;
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
