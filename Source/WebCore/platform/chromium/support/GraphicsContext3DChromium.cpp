/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "GraphicsContext3D.h"

#include "DrawingBuffer.h"
#include "GraphicsContext3DPrivate.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include <public/Platform.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes, HostWindow*, GraphicsContext3D::RenderStyle)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
    m_private->setContextLostCallback(nullptr);
    m_private->setErrorMessageCallback(nullptr);
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
{
    m_private->setContextLostCallback(callback);
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback> callback)
{
    m_private->setErrorMessageCallback(callback);
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow*, GraphicsContext3D::RenderStyle renderStyle)
{
    ASSERT(renderStyle != GraphicsContext3D::RenderDirectlyToHostWindow);

    WebKit::WebGraphicsContext3D::Attributes webAttributes;
    webAttributes.alpha = attrs.alpha;
    webAttributes.depth = attrs.depth;
    webAttributes.stencil = attrs.stencil;
    webAttributes.antialias = attrs.antialias;
    webAttributes.premultipliedAlpha = attrs.premultipliedAlpha;
    webAttributes.noExtensions = attrs.noExtensions;
    webAttributes.shareResources = attrs.shareResources;
    webAttributes.preferDiscreteGPU = attrs.preferDiscreteGPU;
    webAttributes.topDocumentURL = attrs.topDocumentURL.string();

    OwnPtr<WebKit::WebGraphicsContext3D> webContext = adoptPtr(WebKit::Platform::current()->createOffscreenGraphicsContext3D(webAttributes));
    if (!webContext)
        return 0;

    return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(webContext.release(), attrs.preserveDrawingBuffer);
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_private->webContext();
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_private->webContext()->getPlatformTextureId();
}

GrContext* GraphicsContext3D::grContext()
{
    return m_private->grContext();
}

PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return 0;
}

// Macros to assist in delegating from GraphicsContext3D to
// WebGraphicsContext3D.

#define DELEGATE_TO_WEBCONTEXT(name) \
void GraphicsContext3D::name() \
{ \
    m_private->webContext()->name(); \
}

#define DELEGATE_TO_WEBCONTEXT_R(name, rt)           \
rt GraphicsContext3D::name() \
{ \
    return m_private->webContext()->name(); \
}

#define DELEGATE_TO_WEBCONTEXT_1(name, t1) \
void GraphicsContext3D::name(t1 a1) \
{ \
    m_private->webContext()->name(a1); \
}

#define DELEGATE_TO_WEBCONTEXT_1R(name, t1, rt)    \
rt GraphicsContext3D::name(t1 a1) \
{ \
    return m_private->webContext()->name(a1); \
}

#define DELEGATE_TO_WEBCONTEXT_2(name, t1, t2) \
void GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    m_private->webContext()->name(a1, a2); \
}

#define DELEGATE_TO_WEBCONTEXT_2R(name, t1, t2, rt)  \
rt GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    return m_private->webContext()->name(a1, a2); \
}

#define DELEGATE_TO_WEBCONTEXT_3(name, t1, t2, t3)   \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)    \
{ \
    m_private->webContext()->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_WEBCONTEXT_3R(name, t1, t2, t3, rt)   \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)    \
{ \
    return m_private->webContext()->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_WEBCONTEXT_4(name, t1, t2, t3, t4)    \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    m_private->webContext()->name(a1, a2, a3, a4);              \
}

#define DELEGATE_TO_WEBCONTEXT_4R(name, t1, t2, t3, t4, rt)    \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4);           \
}

#define DELEGATE_TO_WEBCONTEXT_5(name, t1, t2, t3, t4, t5)      \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_WEBCONTEXT_6(name, t1, t2, t3, t4, t5, t6)  \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6);   \
}

#define DELEGATE_TO_WEBCONTEXT_6R(name, t1, t2, t3, t4, t5, t6, rt)  \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_WEBCONTEXT_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_WEBCONTEXT_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_WEBCONTEXT_8(name, t1, t2, t3, t4, t5, t6, t7, t8)       \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7, a8);      \
}

#define DELEGATE_TO_WEBCONTEXT_9(name, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

#define DELEGATE_TO_WEBCONTEXT_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

DELEGATE_TO_WEBCONTEXT_R(makeContextCurrent, bool)
DELEGATE_TO_WEBCONTEXT(prepareTexture)

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_private->webContext()->isGLES2Compliant();
}



bool GraphicsContext3D::isResourceSafe()
{
    return m_private->isResourceSafe();
}

DELEGATE_TO_WEBCONTEXT_1(activeTexture, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(attachShader, Platform3DObject, Platform3DObject)

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    m_private->webContext()->bindAttribLocation(program, index, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_2(bindBuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindFramebuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindRenderbuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindTexture, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_4(blendColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(blendEquation, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(blendEquationSeparate, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(blendFunc, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(blendFuncSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    bufferData(target, size, 0, usage);
}

DELEGATE_TO_WEBCONTEXT_4(bufferData, GC3Denum, GC3Dsizeiptr, const void*, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(bufferSubData, GC3Denum, GC3Dintptr, GC3Dsizeiptr, const void*)

DELEGATE_TO_WEBCONTEXT_1R(checkFramebufferStatus, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(clear, GC3Dbitfield)
DELEGATE_TO_WEBCONTEXT_4(clearColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(clearDepth, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(clearStencil, GC3Dint)
DELEGATE_TO_WEBCONTEXT_4(colorMask, GC3Dboolean, GC3Dboolean, GC3Dboolean, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1(compileShader, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_8(compressedTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, const void*)
DELEGATE_TO_WEBCONTEXT_9(compressedTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Denum, GC3Dsizei, const void*)
DELEGATE_TO_WEBCONTEXT_8(copyTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Dint)
DELEGATE_TO_WEBCONTEXT_8(copyTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_1(cullFace, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(depthFunc, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(depthMask, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_2(depthRange, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(disable, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(disableVertexAttribArray, GC3Duint)
DELEGATE_TO_WEBCONTEXT_3(drawArrays, GC3Denum, GC3Dint, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_4(drawElements, GC3Denum, GC3Dsizei, GC3Denum, GC3Dintptr)

DELEGATE_TO_WEBCONTEXT_1(enable, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(enableVertexAttribArray, GC3Duint)
DELEGATE_TO_WEBCONTEXT(finish)
DELEGATE_TO_WEBCONTEXT(flush)
DELEGATE_TO_WEBCONTEXT_4(framebufferRenderbuffer, GC3Denum, GC3Denum, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_5(framebufferTexture2D, GC3Denum, GC3Denum, GC3Denum, Platform3DObject, GC3Dint)
DELEGATE_TO_WEBCONTEXT_1(frontFace, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(generateMipmap, GC3Denum)

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_private->webContext()->getActiveAttrib(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_private->webContext()->getActiveUniform(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}


DELEGATE_TO_WEBCONTEXT_4(getAttachedShaders, Platform3DObject, GC3Dsizei, GC3Dsizei*, Platform3DObject*)

GC3Dint GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_private->webContext()->getAttribLocation(program, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_2(getBooleanv, GC3Denum, GC3Dboolean*)
DELEGATE_TO_WEBCONTEXT_3(getBufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes = m_private->webContext()->getContextAttributes();
    GraphicsContext3D::Attributes attributes;
    attributes.alpha = webAttributes.alpha;
    attributes.depth = webAttributes.depth;
    attributes.stencil = webAttributes.stencil;
    attributes.antialias = webAttributes.antialias;
    attributes.premultipliedAlpha = webAttributes.premultipliedAlpha;
    attributes.preserveDrawingBuffer = m_private->preserveDrawingBuffer();
    attributes.preferDiscreteGPU = webAttributes.preferDiscreteGPU;
    return attributes;
}

DELEGATE_TO_WEBCONTEXT_R(getError, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(getFloatv, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(getFramebufferAttachmentParameteriv, GC3Denum, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_2(getIntegerv, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getProgramiv, Platform3DObject, GC3Denum, GC3Dint*)

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    return m_private->webContext()->getProgramInfoLog(program);
}


DELEGATE_TO_WEBCONTEXT_3(getRenderbufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getShaderiv, Platform3DObject, GC3Denum, GC3Dint*)

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    return m_private->webContext()->getShaderInfoLog(shader);
}


DELEGATE_TO_WEBCONTEXT_4(getShaderPrecisionFormat, GC3Denum, GC3Denum, GC3Dint*, GC3Dint*)

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    return m_private->webContext()->getShaderSource(shader);
}

String GraphicsContext3D::getString(GC3Denum name)
{
    return m_private->webContext()->getString(name);
}


DELEGATE_TO_WEBCONTEXT_3(getTexParameterfv, GC3Denum, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getTexParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getUniformfv, Platform3DObject, GC3Dint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getUniformiv, Platform3DObject, GC3Dint, GC3Dint*)

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_private->webContext()->getUniformLocation(program, name.utf8().data());
}


DELEGATE_TO_WEBCONTEXT_3(getVertexAttribfv, GC3Duint, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getVertexAttribiv, GC3Duint, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_2R(getVertexAttribOffset, GC3Duint, GC3Denum, GC3Dsizeiptr)

DELEGATE_TO_WEBCONTEXT_2(hint, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1R(isBuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isEnabled, GC3Denum, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isFramebuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isProgram, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isRenderbuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isShader, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isTexture, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1(lineWidth, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_1(linkProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(pixelStorei, GC3Denum, GC3Dint)
DELEGATE_TO_WEBCONTEXT_2(polygonOffset, GC3Dfloat, GC3Dfloat)

DELEGATE_TO_WEBCONTEXT_7(readPixels, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, void*)

DELEGATE_TO_WEBCONTEXT(releaseShaderCompiler)
DELEGATE_TO_WEBCONTEXT_4(renderbufferStorage, GC3Denum, GC3Denum, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_2(sampleCoverage, GC3Dclampf, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_4(scissor, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& string)
{
    m_private->webContext()->shaderSource(shader, string.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_3(stencilFunc, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_WEBCONTEXT_4(stencilFuncSeparate, GC3Denum, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_WEBCONTEXT_1(stencilMask, GC3Duint)
DELEGATE_TO_WEBCONTEXT_2(stencilMaskSeparate, GC3Denum, GC3Duint)
DELEGATE_TO_WEBCONTEXT_3(stencilOp, GC3Denum, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(stencilOpSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_private->webContext()->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

DELEGATE_TO_WEBCONTEXT_3(texParameterf, GC3Denum, GC3Denum, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(texParameteri, GC3Denum, GC3Denum, GC3Dint)

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_private->webContext()->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

DELEGATE_TO_WEBCONTEXT_2(uniform1f, GC3Dint, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform1fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_2(uniform1i, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform1iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(uniform2f, GC3Dint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform2fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(uniform2i, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform2iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_4(uniform3f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform3fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniform3i, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform3iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_5(uniform4f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform4fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_5(uniform4i, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform4iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix2fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix3fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix4fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)

DELEGATE_TO_WEBCONTEXT_1(useProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(validateProgram, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_2(vertexAttrib1f, GC3Duint, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib1fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(vertexAttrib2f, GC3Duint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib2fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(vertexAttrib3f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib3fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_5(vertexAttrib4f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib4fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_6(vertexAttribPointer, GC3Duint, GC3Dint, GC3Denum, GC3Dboolean, GC3Dsizei, GC3Dintptr)

DELEGATE_TO_WEBCONTEXT_4(viewport, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_private->webContext()->width() && height == m_private->webContext()->height())
        return;

    m_private->webContext()->reshape(width, height);
}

void GraphicsContext3D::markContextChanged()
{
    m_private->markContextChanged();
}

bool GraphicsContext3D::layerComposited() const
{
    return m_private->layerComposited();
}

void GraphicsContext3D::markLayerComposited()
{
    m_private->markLayerComposited();
}

namespace {

void getDrawingParameters(DrawingBuffer* drawingBuffer, WebKit::WebGraphicsContext3D* graphicsContext3D,
                          Platform3DObject* frameBufferId, int* width, int* height)
{
    if (drawingBuffer) {
        *frameBufferId = drawingBuffer->framebuffer();
        *width = drawingBuffer->size().width();
        *height = drawingBuffer->size().height();
    } else {
        *frameBufferId = 0;
        *width = graphicsContext3D->width();
        *height = graphicsContext3D->height();
    }
}

} // anonymous namespace

void GraphicsContext3D::paintRenderingResultsToCanvas(ImageBuffer* imageBuffer, DrawingBuffer* drawingBuffer)
{
    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_private->webContext(), &framebufferId, &width, &height);
    m_private->paintFramebufferToCanvas(framebufferId, width, height, !getContextAttributes().premultipliedAlpha, imageBuffer);
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    if (getContextAttributes().premultipliedAlpha)
        return 0;

    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_private->webContext(), &framebufferId, &width, &height);

    RefPtr<ImageData> imageData = ImageData::create(IntSize(width, height));
    unsigned char* pixels = imageData->data()->data();
    size_t bufferSize = 4 * width * height;

    m_private->webContext()->readBackFramebuffer(pixels, bufferSize, framebufferId, width, height);

    for (size_t i = 0; i < bufferSize; i += 4)
        std::swap(pixels[i], pixels[i + 2]);

    return imageData.release();
}

bool GraphicsContext3D::paintCompositedResultsToCanvas(ImageBuffer*)
{
    return false;
}

DELEGATE_TO_WEBCONTEXT_R(createBuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createFramebuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createRenderbuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1R(createShader, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createTexture, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_1(deleteBuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteFramebuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteRenderbuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteShader, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteTexture, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_1(synthesizeGLError, GC3Denum)

Extensions3D* GraphicsContext3D::getExtensions()
{
    return m_private->getExtensions();
}


IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    return IntSize(m_private->webContext()->width(), m_private->webContext()->height());
}

} // namespace WebCore
