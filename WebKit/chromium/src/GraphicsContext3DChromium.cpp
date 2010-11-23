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

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "CachedImage.h"
#include "WebGLLayerChromium.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3DInternal.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "WebGraphicsContext3D.h"
#include "WebGraphicsContext3DDefaultImpl.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebViewImpl.h"

#include <stdio.h>
#include <wtf/FastMalloc.h>
#include <wtf/text/CString.h>

#if PLATFORM(CG)
#include "GraphicsContext.h"
#include "WebGLRenderingContext.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGImage.h>
#endif

// There are two levels of delegation in this file:
//
//   1. GraphicsContext3D delegates to GraphicsContext3DInternal. This is done
//      so that we have some place to store data members common among
//      implementations; GraphicsContext3D only provides us the m_internal
//      pointer. We always delegate to the GraphicsContext3DInternal. While we
//      could sidestep it and go directly to the WebGraphicsContext3D in some
//      cases, it is better for consistency to always delegate through it.
//
//   2. GraphicsContext3DInternal delegates to an implementation of
//      WebGraphicsContext3D. This is done so we have a place to inject an
//      implementation which remotes the OpenGL calls across processes.
//
// The legacy, in-process, implementation uses WebGraphicsContext3DDefaultImpl.

namespace WebCore {

//----------------------------------------------------------------------
// GraphicsContext3DInternal

GraphicsContext3DInternal::GraphicsContext3DInternal()
    : m_webViewImpl(0)
    , m_initializedAvailableExtensions(false)
#if PLATFORM(SKIA)
#elif PLATFORM(CG)
    , m_renderOutput(0)
#else
#error Must port to your platform
#endif
{
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
#if PLATFORM(CG)
    if (m_renderOutput)
        delete[] m_renderOutput;
#endif
}

bool GraphicsContext3DInternal::initialize(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes;
    webAttributes.alpha = attrs.alpha;
    webAttributes.depth = attrs.depth;
    webAttributes.stencil = attrs.stencil;
    webAttributes.antialias = attrs.antialias;
    webAttributes.premultipliedAlpha = attrs.premultipliedAlpha;
    webAttributes.canRecoverFromContextLoss = attrs.canRecoverFromContextLoss;
    WebKit::WebGraphicsContext3D* webContext = WebKit::webKitClient()->createGraphicsContext3D();
    if (!webContext)
        return false;

    Chrome* chrome = static_cast<Chrome*>(hostWindow);
    WebKit::ChromeClientImpl* chromeClientImpl = static_cast<WebKit::ChromeClientImpl*>(chrome->client());

    m_webViewImpl = chromeClientImpl->webView();

    if (!m_webViewImpl)
        return false;
    if (!webContext->initialize(webAttributes, m_webViewImpl, renderDirectlyToHostWindow)) {
        delete webContext;
        return false;
    }
    m_impl.set(webContext);

#if USE(ACCELERATED_COMPOSITING)
    m_compositingLayer = WebGLLayerChromium::create(0);
#endif
    return true;
}

WebKit::WebGraphicsContext3D* GraphicsContext3DInternal::extractWebGraphicsContext3D(GraphicsContext3D* context)
{
    if (!context)
        return 0;
    return context->m_internal->m_impl.get();
}

PlatformGraphicsContext3D GraphicsContext3DInternal::platformGraphicsContext3D() const
{
    return m_impl.get();
}

Platform3DObject GraphicsContext3DInternal::platformTexture() const
{
    return m_impl->getPlatformTextureId();
}

void GraphicsContext3DInternal::prepareTexture()
{
    m_impl->prepareTexture();
}

#if USE(ACCELERATED_COMPOSITING)
WebGLLayerChromium* GraphicsContext3DInternal::platformLayer() const
{
    return m_compositingLayer.get();
}
#endif

void GraphicsContext3DInternal::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    HTMLCanvasElement* canvas = context->canvas();
    ImageBuffer* imageBuffer = canvas->buffer();
    unsigned char* pixels = 0;
#if PLATFORM(SKIA)
    const SkBitmap* canvasBitmap = imageBuffer->context()->platformContext()->bitmap();
    const SkBitmap* readbackBitmap = 0;
    ASSERT(canvasBitmap->config() == SkBitmap::kARGB_8888_Config);
    if (canvasBitmap->width() == m_impl->width() && canvasBitmap->height() == m_impl->height()) {
        // This is the fastest and most common case. We read back
        // directly into the canvas's backing store.
        readbackBitmap = canvasBitmap;
        m_resizingBitmap.reset();
    } else {
        // We need to allocate a temporary bitmap for reading back the
        // pixel data. We will then use Skia to rescale this bitmap to
        // the size of the canvas's backing store.
        if (m_resizingBitmap.width() != m_impl->width() || m_resizingBitmap.height() != m_impl->height()) {
            m_resizingBitmap.setConfig(SkBitmap::kARGB_8888_Config,
                                       m_impl->width(),
                                       m_impl->height());
            if (!m_resizingBitmap.allocPixels())
                return;
        }
        readbackBitmap = &m_resizingBitmap;
    }

    // Read back the frame buffer.
    SkAutoLockPixels bitmapLock(*readbackBitmap);
    pixels = static_cast<unsigned char*>(readbackBitmap->getPixels());
#elif PLATFORM(CG)
    if (m_renderOutput)
        pixels = m_renderOutput;
#else
#error Must port to your platform
#endif

    m_impl->readBackFramebuffer(pixels, 4 * m_impl->width() * m_impl->height());

#if PLATFORM(SKIA)
    if (m_resizingBitmap.readyToDraw()) {
        // We need to draw the resizing bitmap into the canvas's backing store.
        SkCanvas canvas(*canvasBitmap);
        SkRect dst;
        dst.set(SkIntToScalar(0), SkIntToScalar(0), canvasBitmap->width(), canvasBitmap->height());
        canvas.drawBitmapRect(m_resizingBitmap, 0, dst);
    }
#elif PLATFORM(CG)
    if (m_renderOutput && context->is3d()) {
        WebGLRenderingContext* webGLContext = static_cast<WebGLRenderingContext*>(context);
        webGLContext->graphicsContext3D()->paintToCanvas(m_renderOutput, m_impl->width(), m_impl->height(), canvas->width(), canvas->height(), imageBuffer->context()->platformContext());
    }
#else
#error Must port to your platform
#endif
}

bool GraphicsContext3DInternal::paintsIntoCanvasBuffer() const
{
    // If the gpu compositor is on then skip the readback and software rendering path.
    return !m_webViewImpl->isAcceleratedCompositingActive();
}

void GraphicsContext3DInternal::reshape(int width, int height)
{
    if (width == m_impl->width() && height == m_impl->height())
        return;

    m_impl->reshape(width, height);

#if PLATFORM(CG)
    // Need to reallocate the client-side backing store.
    // FIXME: make this more efficient.
    if (m_renderOutput) {
        delete[] m_renderOutput;
        m_renderOutput = 0;
    }
    int rowBytes = width * 4;
    m_renderOutput = new unsigned char[height * rowBytes];
#endif // PLATFORM(CG)
}

bool GraphicsContext3DInternal::isContextLost()
{
    return m_impl->isContextLost();
}

// Macros to assist in delegating from GraphicsContext3DInternal to
// WebGraphicsContext3D.

#define DELEGATE_TO_IMPL(name) \
void GraphicsContext3DInternal::name() \
{ \
    m_impl->name(); \
}

#define DELEGATE_TO_IMPL_R(name, rt)           \
rt GraphicsContext3DInternal::name() \
{ \
    return m_impl->name(); \
}

#define DELEGATE_TO_IMPL_1(name, t1) \
void GraphicsContext3DInternal::name(t1 a1) \
{ \
    m_impl->name(a1); \
}

#define DELEGATE_TO_IMPL_1R(name, t1, rt)    \
rt GraphicsContext3DInternal::name(t1 a1) \
{ \
    return m_impl->name(a1); \
}

#define DELEGATE_TO_IMPL_2(name, t1, t2) \
void GraphicsContext3DInternal::name(t1 a1, t2 a2) \
{ \
    m_impl->name(a1, a2); \
}

#define DELEGATE_TO_IMPL_2R(name, t1, t2, rt)  \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2) \
{ \
    return m_impl->name(a1, a2); \
}

#define DELEGATE_TO_IMPL_3(name, t1, t2, t3)   \
void GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3)    \
{ \
    m_impl->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_IMPL_3R(name, t1, t2, t3, rt)   \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3)    \
{ \
    return m_impl->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_IMPL_4(name, t1, t2, t3, t4)    \
void GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    m_impl->name(a1, a2, a3, a4);              \
}

#define DELEGATE_TO_IMPL_4R(name, t1, t2, t3, t4, rt)       \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4)        \
{ \
    return m_impl->name(a1, a2, a3, a4);           \
}

#define DELEGATE_TO_IMPL_5(name, t1, t2, t3, t4, t5)      \
void GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    m_impl->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_IMPL_5R(name, t1, t2, t3, t4, t5, rt)      \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    return m_impl->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_IMPL_6(name, t1, t2, t3, t4, t5, t6)  \
void GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_IMPL_6R(name, t1, t2, t3, t4, t5, t6, rt)  \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_IMPL_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_IMPL_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_IMPL_8(name, t1, t2, t3, t4, t5, t6, t7, t8)       \
void GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8);      \
}

#define DELEGATE_TO_IMPL_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3DInternal::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

DELEGATE_TO_IMPL_R(makeContextCurrent, bool)
DELEGATE_TO_IMPL_1R(sizeInBytes, int, int)

bool GraphicsContext3DInternal::isGLES2Compliant() const
{
    return m_impl->isGLES2Compliant();
}

DELEGATE_TO_IMPL_1(activeTexture, unsigned long)
DELEGATE_TO_IMPL_2(attachShader, Platform3DObject, Platform3DObject)

void GraphicsContext3DInternal::bindAttribLocation(Platform3DObject program, unsigned long index, const String& name)
{
    m_impl->bindAttribLocation(program, index, name.utf8().data());
}

DELEGATE_TO_IMPL_2(bindBuffer, unsigned long, Platform3DObject)
DELEGATE_TO_IMPL_2(bindFramebuffer, unsigned long, Platform3DObject)
DELEGATE_TO_IMPL_2(bindRenderbuffer, unsigned long, Platform3DObject)
DELEGATE_TO_IMPL_2(bindTexture, unsigned long, Platform3DObject)
DELEGATE_TO_IMPL_4(blendColor, double, double, double, double)
DELEGATE_TO_IMPL_1(blendEquation, unsigned long)
DELEGATE_TO_IMPL_2(blendEquationSeparate, unsigned long, unsigned long)
DELEGATE_TO_IMPL_2(blendFunc, unsigned long, unsigned long)
DELEGATE_TO_IMPL_4(blendFuncSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

void GraphicsContext3DInternal::bufferData(unsigned long target, int size, unsigned long usage)
{
    m_impl->bufferData(target, size, 0, usage);
}

void GraphicsContext3DInternal::bufferData(unsigned long target, int size, const void* data, unsigned long usage)
{
    m_impl->bufferData(target, size, data, usage);
}

void GraphicsContext3DInternal::bufferSubData(unsigned long target, long offset, int size, const void* data)
{
    m_impl->bufferSubData(target, offset, size, data);
}

DELEGATE_TO_IMPL_1R(checkFramebufferStatus, unsigned long, unsigned long)
DELEGATE_TO_IMPL_1(clear, unsigned long)
DELEGATE_TO_IMPL_4(clearColor, double, double, double, double)
DELEGATE_TO_IMPL_1(clearDepth, double)
DELEGATE_TO_IMPL_1(clearStencil, long)
DELEGATE_TO_IMPL_4(colorMask, bool, bool, bool, bool)
DELEGATE_TO_IMPL_1(compileShader, Platform3DObject)

DELEGATE_TO_IMPL_8(copyTexImage2D, unsigned long, long, unsigned long, long, long, unsigned long, unsigned long, long)
DELEGATE_TO_IMPL_8(copyTexSubImage2D, unsigned long, long, long, long, long, long, unsigned long, unsigned long)
DELEGATE_TO_IMPL_1(cullFace, unsigned long)
DELEGATE_TO_IMPL_1(depthFunc, unsigned long)
DELEGATE_TO_IMPL_1(depthMask, bool)
DELEGATE_TO_IMPL_2(depthRange, double, double)
DELEGATE_TO_IMPL_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_IMPL_1(disable, unsigned long)
DELEGATE_TO_IMPL_1(disableVertexAttribArray, unsigned long)
DELEGATE_TO_IMPL_3(drawArrays, unsigned long, long, long)
DELEGATE_TO_IMPL_4(drawElements, unsigned long, unsigned long, unsigned long, long)

DELEGATE_TO_IMPL_1(enable, unsigned long)
DELEGATE_TO_IMPL_1(enableVertexAttribArray, unsigned long)
DELEGATE_TO_IMPL(finish)
DELEGATE_TO_IMPL(flush)
DELEGATE_TO_IMPL_4(framebufferRenderbuffer, unsigned long, unsigned long, unsigned long, Platform3DObject)
DELEGATE_TO_IMPL_5(framebufferTexture2D, unsigned long, unsigned long, unsigned long, Platform3DObject, long)
DELEGATE_TO_IMPL_1(frontFace, unsigned long)
DELEGATE_TO_IMPL_1(generateMipmap, unsigned long)

bool GraphicsContext3DInternal::getActiveAttrib(Platform3DObject program, unsigned long index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_impl->getActiveAttrib(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

bool GraphicsContext3DInternal::getActiveUniform(Platform3DObject program, unsigned long index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_impl->getActiveUniform(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

DELEGATE_TO_IMPL_4(getAttachedShaders, Platform3DObject, int, int*, unsigned int*)

int GraphicsContext3DInternal::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_impl->getAttribLocation(program, name.utf8().data());
}

DELEGATE_TO_IMPL_2(getBooleanv, unsigned long, unsigned char*)

DELEGATE_TO_IMPL_3(getBufferParameteriv, unsigned long, unsigned long, int*)

GraphicsContext3D::Attributes GraphicsContext3DInternal::getContextAttributes()
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes = m_impl->getContextAttributes();
    GraphicsContext3D::Attributes attributes;
    attributes.alpha = webAttributes.alpha;
    attributes.depth = webAttributes.depth;
    attributes.stencil = webAttributes.stencil;
    attributes.antialias = webAttributes.antialias;
    attributes.premultipliedAlpha = webAttributes.premultipliedAlpha;
    return attributes;
}

DELEGATE_TO_IMPL_R(getError, unsigned long)

DELEGATE_TO_IMPL_2(getFloatv, unsigned long, float*)

DELEGATE_TO_IMPL_4(getFramebufferAttachmentParameteriv, unsigned long, unsigned long, unsigned long, int*)

DELEGATE_TO_IMPL_2(getIntegerv, unsigned long, int*)

DELEGATE_TO_IMPL_3(getProgramiv, Platform3DObject, unsigned long, int*)

String GraphicsContext3DInternal::getProgramInfoLog(Platform3DObject program)
{
    return m_impl->getProgramInfoLog(program);
}

DELEGATE_TO_IMPL_3(getRenderbufferParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_IMPL_3(getShaderiv, Platform3DObject, unsigned long, int*)

String GraphicsContext3DInternal::getShaderInfoLog(Platform3DObject shader)
{
    return m_impl->getShaderInfoLog(shader);
}

String GraphicsContext3DInternal::getShaderSource(Platform3DObject shader)
{
    return m_impl->getShaderSource(shader);
}

String GraphicsContext3DInternal::getString(unsigned long name)
{
    return m_impl->getString(name);
}

DELEGATE_TO_IMPL_3(getTexParameterfv, unsigned long, unsigned long, float*)
DELEGATE_TO_IMPL_3(getTexParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_IMPL_3(getUniformfv, Platform3DObject, long, float*)
DELEGATE_TO_IMPL_3(getUniformiv, Platform3DObject, long, int*)

long GraphicsContext3DInternal::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_impl->getUniformLocation(program, name.utf8().data());
}

DELEGATE_TO_IMPL_3(getVertexAttribfv, unsigned long, unsigned long, float*)
DELEGATE_TO_IMPL_3(getVertexAttribiv, unsigned long, unsigned long, int*)

DELEGATE_TO_IMPL_2R(getVertexAttribOffset, unsigned long, unsigned long, long)

DELEGATE_TO_IMPL_2(hint, unsigned long, unsigned long)
DELEGATE_TO_IMPL_1R(isBuffer, Platform3DObject, bool)
DELEGATE_TO_IMPL_1R(isEnabled, unsigned long, bool)
DELEGATE_TO_IMPL_1R(isFramebuffer, Platform3DObject, bool)
DELEGATE_TO_IMPL_1R(isProgram, Platform3DObject, bool)
DELEGATE_TO_IMPL_1R(isRenderbuffer, Platform3DObject, bool)
DELEGATE_TO_IMPL_1R(isShader, Platform3DObject, bool)
DELEGATE_TO_IMPL_1R(isTexture, Platform3DObject, bool)
DELEGATE_TO_IMPL_1(lineWidth, double)
DELEGATE_TO_IMPL_1(linkProgram, Platform3DObject)
DELEGATE_TO_IMPL_2(pixelStorei, unsigned long, long)
DELEGATE_TO_IMPL_2(polygonOffset, double, double)
DELEGATE_TO_IMPL_7(readPixels, long, long, unsigned long, unsigned long, unsigned long, unsigned long, void*)
DELEGATE_TO_IMPL(releaseShaderCompiler)
DELEGATE_TO_IMPL_4(renderbufferStorage, unsigned long, unsigned long, unsigned long, unsigned long)
DELEGATE_TO_IMPL_2(sampleCoverage, double, bool)
DELEGATE_TO_IMPL_4(scissor, long, long, unsigned long, unsigned long)

void GraphicsContext3DInternal::shaderSource(Platform3DObject shader, const String& string)
{
    m_impl->shaderSource(shader, string.utf8().data());
}

DELEGATE_TO_IMPL_3(stencilFunc, unsigned long, long, unsigned long)
DELEGATE_TO_IMPL_4(stencilFuncSeparate, unsigned long, unsigned long, long, unsigned long)
DELEGATE_TO_IMPL_1(stencilMask, unsigned long)
DELEGATE_TO_IMPL_2(stencilMaskSeparate, unsigned long, unsigned long)
DELEGATE_TO_IMPL_3(stencilOp, unsigned long, unsigned long, unsigned long)
DELEGATE_TO_IMPL_4(stencilOpSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

int GraphicsContext3DInternal::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels)
{
    m_impl->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return 0;
}

DELEGATE_TO_IMPL_3(texParameterf, unsigned, unsigned, float)
DELEGATE_TO_IMPL_3(texParameteri, unsigned, unsigned, int)

int GraphicsContext3DInternal::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels)
{
    m_impl->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    return 0;
}

DELEGATE_TO_IMPL_2(uniform1f, long, float)

void GraphicsContext3DInternal::uniform1fv(long location, float* v, int size)
{
    m_impl->uniform1fv(location, size, v);
}

DELEGATE_TO_IMPL_2(uniform1i, long, int)

void GraphicsContext3DInternal::uniform1iv(long location, int* v, int size)
{
    m_impl->uniform1iv(location, size, v);
}

DELEGATE_TO_IMPL_3(uniform2f, long, float, float)

void GraphicsContext3DInternal::uniform2fv(long location, float* v, int size)
{
    m_impl->uniform2fv(location, size, v);
}

DELEGATE_TO_IMPL_3(uniform2i, long, int, int)

void GraphicsContext3DInternal::uniform2iv(long location, int* v, int size)
{
    m_impl->uniform2iv(location, size, v);
}

DELEGATE_TO_IMPL_4(uniform3f, long, float, float, float)

void GraphicsContext3DInternal::uniform3fv(long location, float* v, int size)
{
    m_impl->uniform3fv(location, size, v);
}

DELEGATE_TO_IMPL_4(uniform3i, long, int, int, int)

void GraphicsContext3DInternal::uniform3iv(long location, int* v, int size)
{
    m_impl->uniform3iv(location, size, v);
}

DELEGATE_TO_IMPL_5(uniform4f, long, float, float, float, float)

void GraphicsContext3DInternal::uniform4fv(long location, float* v, int size)
{
    m_impl->uniform4fv(location, size, v);
}

DELEGATE_TO_IMPL_5(uniform4i, long, int, int, int, int)

void GraphicsContext3DInternal::uniform4iv(long location, int* v, int size)
{
    m_impl->uniform4iv(location, size, v);
}

void GraphicsContext3DInternal::uniformMatrix2fv(long location, bool transpose, float* value, int size)
{
    m_impl->uniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3DInternal::uniformMatrix3fv(long location, bool transpose, float* value, int size)
{
    m_impl->uniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3DInternal::uniformMatrix4fv(long location, bool transpose, float* value, int size)
{
    m_impl->uniformMatrix4fv(location, size, transpose, value);
}

DELEGATE_TO_IMPL_1(useProgram, Platform3DObject)
DELEGATE_TO_IMPL_1(validateProgram, Platform3DObject)

DELEGATE_TO_IMPL_2(vertexAttrib1f, unsigned long, float)
DELEGATE_TO_IMPL_2(vertexAttrib1fv, unsigned long, float*)
DELEGATE_TO_IMPL_3(vertexAttrib2f, unsigned long, float, float)
DELEGATE_TO_IMPL_2(vertexAttrib2fv, unsigned long, float*)
DELEGATE_TO_IMPL_4(vertexAttrib3f, unsigned long, float, float, float)
DELEGATE_TO_IMPL_2(vertexAttrib3fv, unsigned long, float*)
DELEGATE_TO_IMPL_5(vertexAttrib4f, unsigned long, float, float, float, float)
DELEGATE_TO_IMPL_2(vertexAttrib4fv, unsigned long, float*)
DELEGATE_TO_IMPL_6(vertexAttribPointer, unsigned long, int, int, bool, unsigned long, unsigned long)

DELEGATE_TO_IMPL_4(viewport, long, long, unsigned long, unsigned long)

DELEGATE_TO_IMPL_R(createBuffer, unsigned)
DELEGATE_TO_IMPL_R(createFramebuffer, unsigned)
DELEGATE_TO_IMPL_R(createProgram, unsigned)
DELEGATE_TO_IMPL_R(createRenderbuffer, unsigned)
DELEGATE_TO_IMPL_1R(createShader, unsigned long, unsigned)
DELEGATE_TO_IMPL_R(createTexture, unsigned)

DELEGATE_TO_IMPL_1(deleteBuffer, unsigned)
DELEGATE_TO_IMPL_1(deleteFramebuffer, unsigned)
DELEGATE_TO_IMPL_1(deleteProgram, unsigned)
DELEGATE_TO_IMPL_1(deleteRenderbuffer, unsigned)
DELEGATE_TO_IMPL_1(deleteShader, unsigned)
DELEGATE_TO_IMPL_1(deleteTexture, unsigned)

DELEGATE_TO_IMPL_1(synthesizeGLError, unsigned long)

Extensions3D* GraphicsContext3DInternal::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DChromium(this));
    return m_extensions.get();
}

bool GraphicsContext3DInternal::supportsExtension(const String& name)
{
    if (!m_initializedAvailableExtensions) {
        String extensionsString = getString(GraphicsContext3D::EXTENSIONS);
        Vector<String> availableExtensions;
        extensionsString.split(" ", availableExtensions);
        for (size_t i = 0; i < availableExtensions.size(); ++i)
            m_availableExtensions.add(availableExtensions[i]);
        m_initializedAvailableExtensions = true;
    }
    return m_availableExtensions.contains(name);
}

DELEGATE_TO_IMPL_4R(mapBufferSubDataCHROMIUM, unsigned, int, int, unsigned, void*)
DELEGATE_TO_IMPL_1(unmapBufferSubDataCHROMIUM, const void*)
DELEGATE_TO_IMPL_9R(mapTexSubImage2DCHROMIUM, unsigned, int, int, int, int, int, unsigned, unsigned, unsigned, void*)
DELEGATE_TO_IMPL_1(unmapTexSubImage2DCHROMIUM, const void*)
DELEGATE_TO_IMPL_2(copyTextureToParentTextureCHROMIUM, unsigned, unsigned)

//----------------------------------------------------------------------
// GraphicsContext3D
//

// Macros to assist in delegating from GraphicsContext3D to
// GraphicsContext3DInternal.

#define DELEGATE_TO_INTERNAL(name) \
void GraphicsContext3D::name() \
{ \
    m_internal->name(); \
}

#define DELEGATE_TO_INTERNAL_R(name, rt)           \
rt GraphicsContext3D::name() \
{ \
    return m_internal->name(); \
}

#define DELEGATE_TO_INTERNAL_1(name, t1) \
void GraphicsContext3D::name(t1 a1) \
{ \
    m_internal->name(a1); \
}

#define DELEGATE_TO_INTERNAL_1R(name, t1, rt)    \
rt GraphicsContext3D::name(t1 a1) \
{ \
    return m_internal->name(a1); \
}

#define DELEGATE_TO_INTERNAL_2(name, t1, t2) \
void GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    m_internal->name(a1, a2); \
}

#define DELEGATE_TO_INTERNAL_2R(name, t1, t2, rt)  \
rt GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    return m_internal->name(a1, a2); \
}

#define DELEGATE_TO_INTERNAL_3(name, t1, t2, t3)   \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)    \
{ \
    m_internal->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_INTERNAL_3R(name, t1, t2, t3, rt)   \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)    \
{ \
    return m_internal->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_INTERNAL_4(name, t1, t2, t3, t4)    \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    m_internal->name(a1, a2, a3, a4);              \
}

#define DELEGATE_TO_INTERNAL_4R(name, t1, t2, t3, t4, rt)    \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    return m_internal->name(a1, a2, a3, a4);           \
}

#define DELEGATE_TO_INTERNAL_5(name, t1, t2, t3, t4, t5)      \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    m_internal->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_INTERNAL_6(name, t1, t2, t3, t4, t5, t6)  \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_internal->name(a1, a2, a3, a4, a5, a6);   \
}

#define DELEGATE_TO_INTERNAL_6R(name, t1, t2, t3, t4, t5, t6, rt)  \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_internal->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_INTERNAL_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_internal->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_INTERNAL_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_internal->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_INTERNAL_8(name, t1, t2, t3, t4, t5, t6, t7, t8)       \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_internal->name(a1, a2, a3, a4, a5, a6, a7, a8);      \
}

#define DELEGATE_TO_INTERNAL_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_internal->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes, HostWindow*, bool)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    OwnPtr<GraphicsContext3DInternal> internal = adoptPtr(new GraphicsContext3DInternal());
    if (!internal->initialize(attrs, hostWindow, renderStyle == RenderDirectlyToHostWindow)) {
        return 0;
    }
    RefPtr<GraphicsContext3D> result = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderStyle == RenderDirectlyToHostWindow));
    result->m_internal = internal.release();
    return result.release();
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_internal->platformGraphicsContext3D();
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_internal->platformTexture();
}

void GraphicsContext3D::prepareTexture()
{
    return m_internal->prepareTexture();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    WebGLLayerChromium* canvasLayer = m_internal->platformLayer();
    canvasLayer->setContext(this);
    return canvasLayer;
}
#endif

DELEGATE_TO_INTERNAL(makeContextCurrent)
DELEGATE_TO_INTERNAL_1R(sizeInBytes, int, int)
DELEGATE_TO_INTERNAL_2(reshape, int, int)

DELEGATE_TO_INTERNAL_1(activeTexture, unsigned long)
DELEGATE_TO_INTERNAL_2(attachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_INTERNAL_3(bindAttribLocation, Platform3DObject, unsigned long, const String&)

DELEGATE_TO_INTERNAL_2(bindBuffer, unsigned long, Platform3DObject)
DELEGATE_TO_INTERNAL_2(bindFramebuffer, unsigned long, Platform3DObject)
DELEGATE_TO_INTERNAL_2(bindRenderbuffer, unsigned long, Platform3DObject)
DELEGATE_TO_INTERNAL_2(bindTexture, unsigned long, Platform3DObject)
DELEGATE_TO_INTERNAL_4(blendColor, double, double, double, double)
DELEGATE_TO_INTERNAL_1(blendEquation, unsigned long)
DELEGATE_TO_INTERNAL_2(blendEquationSeparate, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_2(blendFunc, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_4(blendFuncSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_INTERNAL_3(bufferData, unsigned long, int, unsigned long)
DELEGATE_TO_INTERNAL_4(bufferData, unsigned long, int, const void*, unsigned long)
DELEGATE_TO_INTERNAL_4(bufferSubData, unsigned long, long, int, const void*)

DELEGATE_TO_INTERNAL_1R(checkFramebufferStatus, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_1(clear, unsigned long)
DELEGATE_TO_INTERNAL_4(clearColor, double, double, double, double)
DELEGATE_TO_INTERNAL_1(clearDepth, double)
DELEGATE_TO_INTERNAL_1(clearStencil, long)
DELEGATE_TO_INTERNAL_4(colorMask, bool, bool, bool, bool)
DELEGATE_TO_INTERNAL_1(compileShader, Platform3DObject)

DELEGATE_TO_INTERNAL_8(copyTexImage2D, unsigned long, long, unsigned long, long, long, unsigned long, unsigned long, long)
DELEGATE_TO_INTERNAL_8(copyTexSubImage2D, unsigned long, long, long, long, long, long, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_1(cullFace, unsigned long)
DELEGATE_TO_INTERNAL_1(depthFunc, unsigned long)
DELEGATE_TO_INTERNAL_1(depthMask, bool)
DELEGATE_TO_INTERNAL_2(depthRange, double, double)
DELEGATE_TO_INTERNAL_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_INTERNAL_1(disable, unsigned long)
DELEGATE_TO_INTERNAL_1(disableVertexAttribArray, unsigned long)
DELEGATE_TO_INTERNAL_3(drawArrays, unsigned long, long, long)
DELEGATE_TO_INTERNAL_4(drawElements, unsigned long, unsigned long, unsigned long, long)

DELEGATE_TO_INTERNAL_1(enable, unsigned long)
DELEGATE_TO_INTERNAL_1(enableVertexAttribArray, unsigned long)
DELEGATE_TO_INTERNAL(finish)
DELEGATE_TO_INTERNAL(flush)
DELEGATE_TO_INTERNAL_4(framebufferRenderbuffer, unsigned long, unsigned long, unsigned long, Platform3DObject)
DELEGATE_TO_INTERNAL_5(framebufferTexture2D, unsigned long, unsigned long, unsigned long, Platform3DObject, long)
DELEGATE_TO_INTERNAL_1(frontFace, unsigned long)
DELEGATE_TO_INTERNAL_1(generateMipmap, unsigned long)

DELEGATE_TO_INTERNAL_3R(getActiveAttrib, Platform3DObject, unsigned long, ActiveInfo&, bool)
DELEGATE_TO_INTERNAL_3R(getActiveUniform, Platform3DObject, unsigned long, ActiveInfo&, bool)

DELEGATE_TO_INTERNAL_4(getAttachedShaders, Platform3DObject, int, int*, unsigned int*)

DELEGATE_TO_INTERNAL_2R(getAttribLocation, Platform3DObject, const String&, int)

DELEGATE_TO_INTERNAL_2(getBooleanv, unsigned long, unsigned char*)

DELEGATE_TO_INTERNAL_3(getBufferParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_INTERNAL_R(getContextAttributes, GraphicsContext3D::Attributes)

DELEGATE_TO_INTERNAL_R(getError, unsigned long)

DELEGATE_TO_INTERNAL_2(getFloatv, unsigned long, float*)

DELEGATE_TO_INTERNAL_4(getFramebufferAttachmentParameteriv, unsigned long, unsigned long, unsigned long, int*)

DELEGATE_TO_INTERNAL_2(getIntegerv, unsigned long, int*)

DELEGATE_TO_INTERNAL_3(getProgramiv, Platform3DObject, unsigned long, int*)

DELEGATE_TO_INTERNAL_1R(getProgramInfoLog, Platform3DObject, String)

DELEGATE_TO_INTERNAL_3(getRenderbufferParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_INTERNAL_3(getShaderiv, Platform3DObject, unsigned long, int*)

DELEGATE_TO_INTERNAL_1R(getShaderInfoLog, Platform3DObject, String)

DELEGATE_TO_INTERNAL_1R(getShaderSource, Platform3DObject, String)
DELEGATE_TO_INTERNAL_1R(getString, unsigned long, String)

DELEGATE_TO_INTERNAL_3(getTexParameterfv, unsigned long, unsigned long, float*)
DELEGATE_TO_INTERNAL_3(getTexParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_INTERNAL_3(getUniformfv, Platform3DObject, long, float*)
DELEGATE_TO_INTERNAL_3(getUniformiv, Platform3DObject, long, int*)

DELEGATE_TO_INTERNAL_2R(getUniformLocation, Platform3DObject, const String&, long)

DELEGATE_TO_INTERNAL_3(getVertexAttribfv, unsigned long, unsigned long, float*)
DELEGATE_TO_INTERNAL_3(getVertexAttribiv, unsigned long, unsigned long, int*)

DELEGATE_TO_INTERNAL_2R(getVertexAttribOffset, unsigned long, unsigned long, long)

DELEGATE_TO_INTERNAL_2(hint, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_1R(isBuffer, Platform3DObject, bool)
DELEGATE_TO_INTERNAL_1R(isEnabled, unsigned long, bool)
DELEGATE_TO_INTERNAL_1R(isFramebuffer, Platform3DObject, bool)
DELEGATE_TO_INTERNAL_1R(isProgram, Platform3DObject, bool)
DELEGATE_TO_INTERNAL_1R(isRenderbuffer, Platform3DObject, bool)
DELEGATE_TO_INTERNAL_1R(isShader, Platform3DObject, bool)
DELEGATE_TO_INTERNAL_1R(isTexture, Platform3DObject, bool)
DELEGATE_TO_INTERNAL_1(lineWidth, double)
DELEGATE_TO_INTERNAL_1(linkProgram, Platform3DObject)
DELEGATE_TO_INTERNAL_2(pixelStorei, unsigned long, long)
DELEGATE_TO_INTERNAL_2(polygonOffset, double, double)

DELEGATE_TO_INTERNAL_7(readPixels, long, long, unsigned long, unsigned long, unsigned long, unsigned long, void*)

DELEGATE_TO_INTERNAL(releaseShaderCompiler)
DELEGATE_TO_INTERNAL_4(renderbufferStorage, unsigned long, unsigned long, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_2(sampleCoverage, double, bool)
DELEGATE_TO_INTERNAL_4(scissor, long, long, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_2(shaderSource, Platform3DObject, const String&)
DELEGATE_TO_INTERNAL_3(stencilFunc, unsigned long, long, unsigned long)
DELEGATE_TO_INTERNAL_4(stencilFuncSeparate, unsigned long, unsigned long, long, unsigned long)
DELEGATE_TO_INTERNAL_1(stencilMask, unsigned long)
DELEGATE_TO_INTERNAL_2(stencilMaskSeparate, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_3(stencilOp, unsigned long, unsigned long, unsigned long)
DELEGATE_TO_INTERNAL_4(stencilOpSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_INTERNAL_9R(texImage2D, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, void*, int)
DELEGATE_TO_INTERNAL_3(texParameterf, unsigned, unsigned, float)
DELEGATE_TO_INTERNAL_3(texParameteri, unsigned, unsigned, int)
DELEGATE_TO_INTERNAL_9R(texSubImage2D, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, void*, int)

DELEGATE_TO_INTERNAL_2(uniform1f, long, float)
DELEGATE_TO_INTERNAL_3(uniform1fv, long, float*, int)
DELEGATE_TO_INTERNAL_2(uniform1i, long, int)
DELEGATE_TO_INTERNAL_3(uniform1iv, long, int*, int)
DELEGATE_TO_INTERNAL_3(uniform2f, long, float, float)
DELEGATE_TO_INTERNAL_3(uniform2fv, long, float*, int)
DELEGATE_TO_INTERNAL_3(uniform2i, long, int, int)
DELEGATE_TO_INTERNAL_3(uniform2iv, long, int*, int)
DELEGATE_TO_INTERNAL_4(uniform3f, long, float, float, float)
DELEGATE_TO_INTERNAL_3(uniform3fv, long, float*, int)
DELEGATE_TO_INTERNAL_4(uniform3i, long, int, int, int)
DELEGATE_TO_INTERNAL_3(uniform3iv, long, int*, int)
DELEGATE_TO_INTERNAL_5(uniform4f, long, float, float, float, float)
DELEGATE_TO_INTERNAL_3(uniform4fv, long, float*, int)
DELEGATE_TO_INTERNAL_5(uniform4i, long, int, int, int, int)
DELEGATE_TO_INTERNAL_3(uniform4iv, long, int*, int)
DELEGATE_TO_INTERNAL_4(uniformMatrix2fv, long, bool, float*, int)
DELEGATE_TO_INTERNAL_4(uniformMatrix3fv, long, bool, float*, int)
DELEGATE_TO_INTERNAL_4(uniformMatrix4fv, long, bool, float*, int)

DELEGATE_TO_INTERNAL_1(useProgram, Platform3DObject)
DELEGATE_TO_INTERNAL_1(validateProgram, Platform3DObject)

DELEGATE_TO_INTERNAL_2(vertexAttrib1f, unsigned long, float)
DELEGATE_TO_INTERNAL_2(vertexAttrib1fv, unsigned long, float*)
DELEGATE_TO_INTERNAL_3(vertexAttrib2f, unsigned long, float, float)
DELEGATE_TO_INTERNAL_2(vertexAttrib2fv, unsigned long, float*)
DELEGATE_TO_INTERNAL_4(vertexAttrib3f, unsigned long, float, float, float)
DELEGATE_TO_INTERNAL_2(vertexAttrib3fv, unsigned long, float*)
DELEGATE_TO_INTERNAL_5(vertexAttrib4f, unsigned long, float, float, float, float)
DELEGATE_TO_INTERNAL_2(vertexAttrib4fv, unsigned long, float*)
DELEGATE_TO_INTERNAL_6(vertexAttribPointer, unsigned long, int, int, bool, unsigned long, unsigned long)

DELEGATE_TO_INTERNAL_4(viewport, long, long, unsigned long, unsigned long)

DELEGATE_TO_INTERNAL_1(paintRenderingResultsToCanvas, CanvasRenderingContext*)

bool GraphicsContext3D::paintsIntoCanvasBuffer() const
{
    return m_internal->paintsIntoCanvasBuffer();
}

DELEGATE_TO_INTERNAL_R(createBuffer, unsigned)
DELEGATE_TO_INTERNAL_R(createFramebuffer, unsigned)
DELEGATE_TO_INTERNAL_R(createProgram, unsigned)
DELEGATE_TO_INTERNAL_R(createRenderbuffer, unsigned)
DELEGATE_TO_INTERNAL_1R(createShader, unsigned long, unsigned)
DELEGATE_TO_INTERNAL_R(createTexture, unsigned)

DELEGATE_TO_INTERNAL_1(deleteBuffer, unsigned)
DELEGATE_TO_INTERNAL_1(deleteFramebuffer, unsigned)
DELEGATE_TO_INTERNAL_1(deleteProgram, unsigned)
DELEGATE_TO_INTERNAL_1(deleteRenderbuffer, unsigned)
DELEGATE_TO_INTERNAL_1(deleteShader, unsigned)
DELEGATE_TO_INTERNAL_1(deleteTexture, unsigned)

DELEGATE_TO_INTERNAL_1(synthesizeGLError, unsigned long)
DELEGATE_TO_INTERNAL_R(getExtensions, Extensions3D*)

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_internal->isGLES2Compliant();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
