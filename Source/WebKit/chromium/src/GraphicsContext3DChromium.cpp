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

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

#include "CachedImage.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "DrawingBuffer.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3DPrivate.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "WebKit.h"
#include "platform/WebKitPlatformSupport.h"
#include "WebViewImpl.h"
#include "platform/WebGraphicsContext3D.h"

#include <stdio.h>
#include <wtf/FastMalloc.h>
#include <wtf/text/CString.h>

#if USE(CG)
#include "GraphicsContext.h"
#include "WebGLRenderingContext.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGImage.h>
#endif

#if USE(SKIA)
#include "GrContext.h"
#include "GrGLInterface.h"
#endif

// There are two levels of delegation in this file:
//
//   1. GraphicsContext3D delegates to GraphicsContext3DPrivate. This is done
//      so that we have some place to store data members common among
//      implementations; GraphicsContext3D only provides us the m_private
//      pointer. We always delegate to the GraphicsContext3DPrivate. While we
//      could sidestep it and go directly to the WebGraphicsContext3D in some
//      cases, it is better for consistency to always delegate through it.
//
//   2. GraphicsContext3DPrivate delegates to an implementation of
//      WebGraphicsContext3D. This is done so we have a place to inject an
//      implementation which remotes the OpenGL calls across processes.

namespace WebCore {

//----------------------------------------------------------------------
// GraphicsContext3DPrivate

GraphicsContext3DPrivate::GraphicsContext3DPrivate(WebKit::WebViewImpl* webViewImpl, PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, GraphicsContext3D::Attributes attrs)
    : m_impl(webContext)
    , m_webViewImpl(webViewImpl)
    , m_initializedAvailableExtensions(false)
    , m_layerComposited(false)
    , m_preserveDrawingBuffer(attrs.preserveDrawingBuffer)
    , m_resourceSafety(ResourceSafetyUnknown)
#if USE(SKIA)
    , m_grContext(0)
#elif USE(CG)
    , m_renderOutputSize(0)
#else
#error Must port to your platform
#endif
{
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
#if USE(SKIA)
    if (m_grContext) {
        m_grContext->contextDestroyed();
        GrSafeUnref(m_grContext);
    }
#endif
}


PassOwnPtr<GraphicsContext3DPrivate> GraphicsContext3DPrivate::create(WebKit::WebViewImpl* webViewImpl, PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, GraphicsContext3D::Attributes attrs)
{
    return adoptPtr(new GraphicsContext3DPrivate(webViewImpl, webContext, attrs));
}

PassRefPtr<GraphicsContext3D> GraphicsContext3DPrivate::createGraphicsContextFromWebContext(PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle, ThreadUsage threadUsage)
{
    Chrome* chrome = static_cast<Chrome*>(hostWindow);
    WebKit::WebViewImpl* webViewImpl = chrome ? static_cast<WebKit::WebViewImpl*>(chrome->client()->webView()) : 0;

    if (threadUsage == ForUseOnThisThread && !webContext->makeContextCurrent())
        return 0;

    OwnPtr<GraphicsContext3DPrivate> priv = GraphicsContext3DPrivate::create(webViewImpl, webContext, attrs);
    if (!priv)
        return 0;

    bool renderDirectlyToHostWindow = renderStyle == GraphicsContext3D::RenderDirectlyToHostWindow;
    RefPtr<GraphicsContext3D> result = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderDirectlyToHostWindow));
    result->m_private = priv.release();
    return result.release();
}

namespace {

PassRefPtr<GraphicsContext3D> createGraphicsContext(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle, GraphicsContext3DPrivate::ThreadUsage threadUsage)
{
    bool renderDirectlyToHostWindow = renderStyle == GraphicsContext3D::RenderDirectlyToHostWindow;

    WebKit::WebGraphicsContext3D::Attributes webAttributes;
    webAttributes.alpha = attrs.alpha;
    webAttributes.depth = attrs.depth;
    webAttributes.stencil = attrs.stencil;
    webAttributes.antialias = attrs.antialias;
    webAttributes.premultipliedAlpha = attrs.premultipliedAlpha;
    webAttributes.canRecoverFromContextLoss = attrs.canRecoverFromContextLoss;
    webAttributes.noExtensions = attrs.noExtensions;
    webAttributes.shareResources = attrs.shareResources;
    webAttributes.forUseOnAnotherThread = threadUsage == GraphicsContext3DPrivate::ForUseOnAnotherThread;
    OwnPtr<WebKit::WebGraphicsContext3D> webContext = adoptPtr(WebKit::webKitPlatformSupport()->createGraphicsContext3D());
    if (!webContext)
        return 0;

    Chrome* chrome = static_cast<Chrome*>(hostWindow);
    WebKit::WebViewImpl* webViewImpl = chrome ? static_cast<WebKit::WebViewImpl*>(chrome->client()->webView()) : 0;

    if (!webContext->initialize(webAttributes, webViewImpl, renderDirectlyToHostWindow))
        return 0;

    return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(webContext.release(), attrs, hostWindow, renderStyle, threadUsage);
}

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

PassRefPtr<GraphicsContext3D> GraphicsContext3DPrivate::createGraphicsContextForAnotherThread(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    return createGraphicsContext(attrs, hostWindow, renderStyle, ForUseOnAnotherThread);
}

WebKit::WebGraphicsContext3D* GraphicsContext3DPrivate::extractWebGraphicsContext3D(GraphicsContext3D* context)
{
    if (!context)
        return 0;
    return context->m_private->m_impl.get();
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformGraphicsContext3D() const
{
    return m_impl.get();
}

Platform3DObject GraphicsContext3DPrivate::platformTexture() const
{
    ASSERT(m_webViewImpl);
    m_impl->setParentContext(m_webViewImpl->graphicsContext3D());
    return m_impl->getPlatformTextureId();
}

#if USE(SKIA)
GrContext* GraphicsContext3DPrivate::grContext()
{
    // Limit the number of textures we hold in the bitmap->texture cache.
    static const int maxTextureCacheCount = 512;
    // Limit the bytes allocated toward textures in the bitmap->texture cache.
    static const size_t maxTextureCacheBytes = 96 * 1024 * 1024;

    if (!m_grContext) {
        SkAutoTUnref<GrGLInterface> interface(m_impl->createGrGLInterface());
        m_grContext = GrContext::Create(kOpenGL_Shaders_GrEngine, reinterpret_cast<GrPlatform3DContext>(interface.get()));
        if (m_grContext)
            m_grContext->setTextureCacheLimits(maxTextureCacheCount, maxTextureCacheBytes);
    }
    return m_grContext;
}
#endif

void GraphicsContext3DPrivate::prepareTexture()
{
    m_impl->prepareTexture();
}

void GraphicsContext3DPrivate::markContextChanged()
{
    m_layerComposited = false;
}

void GraphicsContext3DPrivate::markLayerComposited()
{
    m_layerComposited = true;
}

bool GraphicsContext3DPrivate::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContext3DPrivate::paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer* imageBuffer)
{
    unsigned char* pixels = 0;
    size_t bufferSize = 4 * width * height;
#if USE(SKIA)
    const SkBitmap* canvasBitmap = imageBuffer->context()->platformContext()->bitmap();
    const SkBitmap* readbackBitmap = 0;
    ASSERT(canvasBitmap->config() == SkBitmap::kARGB_8888_Config);
    if (canvasBitmap->width() == width && canvasBitmap->height() == height) {
        // This is the fastest and most common case. We read back
        // directly into the canvas's backing store.
        readbackBitmap = canvasBitmap;
        m_resizingBitmap.reset();
    } else {
        // We need to allocate a temporary bitmap for reading back the
        // pixel data. We will then use Skia to rescale this bitmap to
        // the size of the canvas's backing store.
        if (m_resizingBitmap.width() != width || m_resizingBitmap.height() != height) {
            m_resizingBitmap.setConfig(SkBitmap::kARGB_8888_Config,
                                       width,
                                       height);
            if (!m_resizingBitmap.allocPixels())
                return;
        }
        readbackBitmap = &m_resizingBitmap;
    }

    // Read back the frame buffer.
    SkAutoLockPixels bitmapLock(*readbackBitmap);
    pixels = static_cast<unsigned char*>(readbackBitmap->getPixels());
#elif USE(CG)
    if (!m_renderOutput || m_renderOutputSize != bufferSize) {
        m_renderOutput = adoptArrayPtr(new unsigned char[bufferSize]);
        m_renderOutputSize = bufferSize;
    }

    pixels = m_renderOutput.get();
#else
#error Must port to your platform
#endif

    m_impl->readBackFramebuffer(pixels, 4 * width * height, framebuffer, width, height);

    if (premultiplyAlpha) {
        for (size_t i = 0; i < bufferSize; i += 4) {
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

#if USE(SKIA)
    readbackBitmap->notifyPixelsChanged();
    if (m_resizingBitmap.readyToDraw()) {
        // We need to draw the resizing bitmap into the canvas's backing store.
        SkCanvas canvas(*canvasBitmap);
        SkRect dst;
        dst.set(SkIntToScalar(0), SkIntToScalar(0), SkIntToScalar(canvasBitmap->width()), SkIntToScalar(canvasBitmap->height()));
        canvas.drawBitmapRect(m_resizingBitmap, 0, dst);
    }
#elif USE(CG)
    GraphicsContext3D::paintToCanvas(pixels, width, height, imageBuffer->width(), imageBuffer->height(), imageBuffer->context()->platformContext());
#else
#error Must port to your platform
#endif
}

void GraphicsContext3DPrivate::paintRenderingResultsToCanvas(CanvasRenderingContext* context, DrawingBuffer* drawingBuffer)
{
    ImageBuffer* imageBuffer = context->canvas()->buffer();
    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_impl.get(), &framebufferId, &width, &height);
    paintFramebufferToCanvas(framebufferId, width, height, !m_impl->getContextAttributes().premultipliedAlpha, imageBuffer);
}

bool GraphicsContext3DPrivate::paintCompositedResultsToCanvas(CanvasRenderingContext* context)
{
    return false;
}

PassRefPtr<ImageData> GraphicsContext3DPrivate::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    if (m_impl->getContextAttributes().premultipliedAlpha)
        return 0;

    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_impl.get(), &framebufferId, &width, &height);

    RefPtr<ImageData> imageData = ImageData::create(IntSize(width, height));
    unsigned char* pixels = imageData->data()->data()->data();
    size_t bufferSize = 4 * width * height;

    m_impl->readBackFramebuffer(pixels, bufferSize, framebufferId, width, height);

    for (size_t i = 0; i < bufferSize; i += 4)
        std::swap(pixels[i], pixels[i + 2]);

    return imageData.release();
}

bool GraphicsContext3DPrivate::paintsIntoCanvasBuffer() const
{
    // If the gpu compositor is on then skip the readback and software rendering path.
    ASSERT(m_webViewImpl);
    return !m_webViewImpl->isAcceleratedCompositingActive();
}

void GraphicsContext3DPrivate::reshape(int width, int height)
{
    if (width == m_impl->width() && height == m_impl->height())
        return;

    m_impl->reshape(width, height);
}

IntSize GraphicsContext3DPrivate::getInternalFramebufferSize() const
{
    return IntSize(m_impl->width(), m_impl->height());
}

bool GraphicsContext3DPrivate::isContextLost()
{
    return m_impl->isContextLost();
}

// Macros to assist in delegating from GraphicsContext3DPrivate to
// WebGraphicsContext3D.

#define DELEGATE_TO_IMPL(name) \
void GraphicsContext3DPrivate::name() \
{ \
    m_impl->name(); \
}

#define DELEGATE_TO_IMPL_R(name, rt)           \
rt GraphicsContext3DPrivate::name() \
{ \
    return m_impl->name(); \
}

#define DELEGATE_TO_IMPL_1(name, t1) \
void GraphicsContext3DPrivate::name(t1 a1) \
{ \
    m_impl->name(a1); \
}

#define DELEGATE_TO_IMPL_1R(name, t1, rt)    \
rt GraphicsContext3DPrivate::name(t1 a1) \
{ \
    return m_impl->name(a1); \
}

#define DELEGATE_TO_IMPL_2(name, t1, t2) \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2) \
{ \
    m_impl->name(a1, a2); \
}

#define DELEGATE_TO_IMPL_2R(name, t1, t2, rt)  \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2) \
{ \
    return m_impl->name(a1, a2); \
}

#define DELEGATE_TO_IMPL_3(name, t1, t2, t3)   \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3)    \
{ \
    m_impl->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_IMPL_3R(name, t1, t2, t3, rt)   \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3)    \
{ \
    return m_impl->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_IMPL_4(name, t1, t2, t3, t4)    \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    m_impl->name(a1, a2, a3, a4);              \
}

#define DELEGATE_TO_IMPL_4R(name, t1, t2, t3, t4, rt)       \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4)        \
{ \
    return m_impl->name(a1, a2, a3, a4);           \
}

#define DELEGATE_TO_IMPL_5(name, t1, t2, t3, t4, t5)      \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    m_impl->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_IMPL_5R(name, t1, t2, t3, t4, t5, rt)      \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    return m_impl->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_IMPL_6(name, t1, t2, t3, t4, t5, t6)  \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_IMPL_6R(name, t1, t2, t3, t4, t5, t6, rt)  \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_IMPL_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_IMPL_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_IMPL_8(name, t1, t2, t3, t4, t5, t6, t7, t8)       \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8);      \
}

#define DELEGATE_TO_IMPL_9(name, t1, t2, t3, t4, t5, t6, t7, t8, t9)       \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);      \
}

#define DELEGATE_TO_IMPL_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

#define DELEGATE_TO_IMPL_10(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) \
void GraphicsContext3DPrivate::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9, t10 a10) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10); \
}

DELEGATE_TO_IMPL_R(makeContextCurrent, bool)

bool GraphicsContext3DPrivate::isGLES2Compliant() const
{
    return m_impl->isGLES2Compliant();
}

DELEGATE_TO_IMPL_1(activeTexture, GC3Denum)
DELEGATE_TO_IMPL_2(attachShader, Platform3DObject, Platform3DObject)

void GraphicsContext3DPrivate::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    m_impl->bindAttribLocation(program, index, name.utf8().data());
}

DELEGATE_TO_IMPL_2(bindBuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_IMPL_2(bindFramebuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_IMPL_2(bindRenderbuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_IMPL_2(bindTexture, GC3Denum, Platform3DObject)
DELEGATE_TO_IMPL_4(blendColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_IMPL_1(blendEquation, GC3Denum)
DELEGATE_TO_IMPL_2(blendEquationSeparate, GC3Denum, GC3Denum)
DELEGATE_TO_IMPL_2(blendFunc, GC3Denum, GC3Denum)
DELEGATE_TO_IMPL_4(blendFuncSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

void GraphicsContext3DPrivate::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    m_impl->bufferData(target, size, 0, usage);
}

void GraphicsContext3DPrivate::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    m_impl->bufferData(target, size, data, usage);
}

void GraphicsContext3DPrivate::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    m_impl->bufferSubData(target, offset, size, data);
}

DELEGATE_TO_IMPL_1R(checkFramebufferStatus, GC3Denum, GC3Denum)
DELEGATE_TO_IMPL_1(clear, GC3Dbitfield)
DELEGATE_TO_IMPL_4(clearColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_IMPL_1(clearDepth, GC3Dclampf)
DELEGATE_TO_IMPL_1(clearStencil, GC3Dint)
DELEGATE_TO_IMPL_4(colorMask, GC3Dboolean, GC3Dboolean, GC3Dboolean, GC3Dboolean)
DELEGATE_TO_IMPL_1(compileShader, Platform3DObject)

DELEGATE_TO_IMPL_8(compressedTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, const void*)
DELEGATE_TO_IMPL_9(compressedTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Denum, GC3Dsizei, const void*)
DELEGATE_TO_IMPL_8(copyTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Dint)
DELEGATE_TO_IMPL_8(copyTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_IMPL_1(cullFace, GC3Denum)
DELEGATE_TO_IMPL_1(depthFunc, GC3Denum)
DELEGATE_TO_IMPL_1(depthMask, GC3Dboolean)
DELEGATE_TO_IMPL_2(depthRange, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_IMPL_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_IMPL_1(disable, GC3Denum)
DELEGATE_TO_IMPL_1(disableVertexAttribArray, GC3Duint)
DELEGATE_TO_IMPL_3(drawArrays, GC3Denum, GC3Dint, GC3Dsizei)
DELEGATE_TO_IMPL_4(drawElements, GC3Denum, GC3Dsizei, GC3Denum, GC3Dsizeiptr)

DELEGATE_TO_IMPL_1(enable, GC3Denum)
DELEGATE_TO_IMPL_1(enableVertexAttribArray, GC3Duint)
DELEGATE_TO_IMPL(finish)
DELEGATE_TO_IMPL(flush)
DELEGATE_TO_IMPL_4(framebufferRenderbuffer, GC3Denum, GC3Denum, GC3Denum, Platform3DObject)
DELEGATE_TO_IMPL_5(framebufferTexture2D, GC3Denum, GC3Denum, GC3Denum, Platform3DObject, GC3Dint)
DELEGATE_TO_IMPL_1(frontFace, GC3Denum)
DELEGATE_TO_IMPL_1(generateMipmap, GC3Denum)

bool GraphicsContext3DPrivate::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_impl->getActiveAttrib(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

bool GraphicsContext3DPrivate::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_impl->getActiveUniform(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

DELEGATE_TO_IMPL_4(getAttachedShaders, Platform3DObject, GC3Dsizei, GC3Dsizei*, Platform3DObject*)

GC3Dint GraphicsContext3DPrivate::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_impl->getAttribLocation(program, name.utf8().data());
}

DELEGATE_TO_IMPL_2(getBooleanv, GC3Denum, GC3Dboolean*)

DELEGATE_TO_IMPL_3(getBufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)

GraphicsContext3D::Attributes GraphicsContext3DPrivate::getContextAttributes()
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes = m_impl->getContextAttributes();
    GraphicsContext3D::Attributes attributes;
    attributes.alpha = webAttributes.alpha;
    attributes.depth = webAttributes.depth;
    attributes.stencil = webAttributes.stencil;
    attributes.antialias = webAttributes.antialias;
    attributes.premultipliedAlpha = webAttributes.premultipliedAlpha;
    attributes.preserveDrawingBuffer = m_preserveDrawingBuffer;
    return attributes;
}

DELEGATE_TO_IMPL_R(getError, GC3Denum)

DELEGATE_TO_IMPL_2(getFloatv, GC3Denum, GC3Dfloat*)

DELEGATE_TO_IMPL_4(getFramebufferAttachmentParameteriv, GC3Denum, GC3Denum, GC3Denum, GC3Dint*)

DELEGATE_TO_IMPL_2(getIntegerv, GC3Denum, GC3Dint*)

DELEGATE_TO_IMPL_3(getProgramiv, Platform3DObject, GC3Denum, GC3Dint*)

String GraphicsContext3DPrivate::getProgramInfoLog(Platform3DObject program)
{
    return m_impl->getProgramInfoLog(program);
}

DELEGATE_TO_IMPL_3(getRenderbufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)

DELEGATE_TO_IMPL_3(getShaderiv, Platform3DObject, GC3Denum, GC3Dint*)

String GraphicsContext3DPrivate::getShaderInfoLog(Platform3DObject shader)
{
    return m_impl->getShaderInfoLog(shader);
}

String GraphicsContext3DPrivate::getShaderSource(Platform3DObject shader)
{
    return m_impl->getShaderSource(shader);
}

String GraphicsContext3DPrivate::getString(GC3Denum name)
{
    return m_impl->getString(name);
}

DELEGATE_TO_IMPL_3(getTexParameterfv, GC3Denum, GC3Denum, GC3Dfloat*)
DELEGATE_TO_IMPL_3(getTexParameteriv, GC3Denum, GC3Denum, GC3Dint*)

DELEGATE_TO_IMPL_3(getUniformfv, Platform3DObject, GC3Dint, GC3Dfloat*)
DELEGATE_TO_IMPL_3(getUniformiv, Platform3DObject, GC3Dint, GC3Dint*)

GC3Dint GraphicsContext3DPrivate::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_impl->getUniformLocation(program, name.utf8().data());
}

DELEGATE_TO_IMPL_3(getVertexAttribfv, GC3Duint, GC3Denum, GC3Dfloat*)
DELEGATE_TO_IMPL_3(getVertexAttribiv, GC3Duint, GC3Denum, GC3Dint*)

DELEGATE_TO_IMPL_2R(getVertexAttribOffset, GC3Duint, GC3Denum, GC3Dsizeiptr)

DELEGATE_TO_IMPL_2(hint, GC3Denum, GC3Denum)
DELEGATE_TO_IMPL_1R(isBuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_IMPL_1R(isEnabled, GC3Denum, GC3Dboolean)
DELEGATE_TO_IMPL_1R(isFramebuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_IMPL_1R(isProgram, Platform3DObject, GC3Dboolean)
DELEGATE_TO_IMPL_1R(isRenderbuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_IMPL_1R(isShader, Platform3DObject, GC3Dboolean)
DELEGATE_TO_IMPL_1R(isTexture, Platform3DObject, GC3Dboolean)
DELEGATE_TO_IMPL_1(lineWidth, GC3Dfloat)
DELEGATE_TO_IMPL_1(linkProgram, Platform3DObject)
DELEGATE_TO_IMPL_2(pixelStorei, GC3Denum, GC3Dint)
DELEGATE_TO_IMPL_2(polygonOffset, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_IMPL_7(readPixels, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, void*)
DELEGATE_TO_IMPL(releaseShaderCompiler)
DELEGATE_TO_IMPL_4(renderbufferStorage, GC3Denum, GC3Denum, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_IMPL_2(sampleCoverage, GC3Dclampf, GC3Dboolean)
DELEGATE_TO_IMPL_4(scissor, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3DPrivate::shaderSource(Platform3DObject shader, const String& string)
{
    m_impl->shaderSource(shader, string.utf8().data());
}

DELEGATE_TO_IMPL_3(stencilFunc, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_IMPL_4(stencilFuncSeparate, GC3Denum, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_IMPL_1(stencilMask, GC3Duint)
DELEGATE_TO_IMPL_2(stencilMaskSeparate, GC3Denum, GC3Duint)
DELEGATE_TO_IMPL_3(stencilOp, GC3Denum, GC3Denum, GC3Denum)
DELEGATE_TO_IMPL_4(stencilOpSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

bool GraphicsContext3DPrivate::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_impl->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

DELEGATE_TO_IMPL_3(texParameterf, GC3Denum, GC3Denum, GC3Dfloat)
DELEGATE_TO_IMPL_3(texParameteri, GC3Denum, GC3Denum, GC3Dint)

void GraphicsContext3DPrivate::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_impl->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

DELEGATE_TO_IMPL_2(uniform1f, GC3Dint, GC3Dfloat)

void GraphicsContext3DPrivate::uniform1fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_impl->uniform1fv(location, size, v);
}

DELEGATE_TO_IMPL_2(uniform1i, GC3Dint, GC3Dint)

void GraphicsContext3DPrivate::uniform1iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_impl->uniform1iv(location, size, v);
}

DELEGATE_TO_IMPL_3(uniform2f, GC3Dint, GC3Dfloat, GC3Dfloat)

void GraphicsContext3DPrivate::uniform2fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_impl->uniform2fv(location, size, v);
}

DELEGATE_TO_IMPL_3(uniform2i, GC3Dint, GC3Dint, GC3Dint)

void GraphicsContext3DPrivate::uniform2iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_impl->uniform2iv(location, size, v);
}

DELEGATE_TO_IMPL_4(uniform3f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat)

void GraphicsContext3DPrivate::uniform3fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_impl->uniform3fv(location, size, v);
}

DELEGATE_TO_IMPL_4(uniform3i, GC3Dint, GC3Dint, GC3Dint, GC3Dint)

void GraphicsContext3DPrivate::uniform3iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_impl->uniform3iv(location, size, v);
}

DELEGATE_TO_IMPL_5(uniform4f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)

void GraphicsContext3DPrivate::uniform4fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    m_impl->uniform4fv(location, size, v);
}

DELEGATE_TO_IMPL_5(uniform4i, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint)

void GraphicsContext3DPrivate::uniform4iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    m_impl->uniform4iv(location, size, v);
}

void GraphicsContext3DPrivate::uniformMatrix2fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    m_impl->uniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3DPrivate::uniformMatrix3fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    m_impl->uniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3DPrivate::uniformMatrix4fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    m_impl->uniformMatrix4fv(location, size, transpose, value);
}

DELEGATE_TO_IMPL_1(useProgram, Platform3DObject)
DELEGATE_TO_IMPL_1(validateProgram, Platform3DObject)

DELEGATE_TO_IMPL_2(vertexAttrib1f, GC3Duint, GC3Dfloat)
DELEGATE_TO_IMPL_2(vertexAttrib1fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_IMPL_3(vertexAttrib2f, GC3Duint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_IMPL_2(vertexAttrib2fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_IMPL_4(vertexAttrib3f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_IMPL_2(vertexAttrib3fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_IMPL_5(vertexAttrib4f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_IMPL_2(vertexAttrib4fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_IMPL_6(vertexAttribPointer, GC3Duint, GC3Dint, GC3Denum, GC3Dboolean, GC3Dsizei, GC3Dsizeiptr)

DELEGATE_TO_IMPL_4(viewport, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

DELEGATE_TO_IMPL_R(createBuffer, Platform3DObject)
DELEGATE_TO_IMPL_R(createFramebuffer, Platform3DObject)
DELEGATE_TO_IMPL_R(createProgram, Platform3DObject)
DELEGATE_TO_IMPL_R(createRenderbuffer, Platform3DObject)
DELEGATE_TO_IMPL_1R(createShader, GC3Denum, Platform3DObject)
DELEGATE_TO_IMPL_R(createTexture, Platform3DObject)

DELEGATE_TO_IMPL_1(deleteBuffer, Platform3DObject)
DELEGATE_TO_IMPL_1(deleteFramebuffer, Platform3DObject)
DELEGATE_TO_IMPL_1(deleteProgram, Platform3DObject)
DELEGATE_TO_IMPL_1(deleteRenderbuffer, Platform3DObject)
DELEGATE_TO_IMPL_1(deleteShader, Platform3DObject)
DELEGATE_TO_IMPL_1(deleteTexture, Platform3DObject)

DELEGATE_TO_IMPL_1(synthesizeGLError, GC3Denum)

Extensions3D* GraphicsContext3DPrivate::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DChromium(this));
    return m_extensions.get();
}

bool GraphicsContext3DPrivate::isResourceSafe()
{
    if (m_resourceSafety == ResourceSafetyUnknown)
        m_resourceSafety = getExtensions()->isEnabled("GL_CHROMIUM_resource_safe") ? ResourceSafe : ResourceUnsafe;
    return m_resourceSafety == ResourceSafe;
}

namespace {

void splitStringHelper(const String& str, HashSet<String>& set)
{
    Vector<String> substrings;
    str.split(" ", substrings);
    for (size_t i = 0; i < substrings.size(); ++i)
        set.add(substrings[i]);
}

String mapExtensionName(const String& name)
{
    if (name == "GL_ANGLE_framebuffer_blit"
        || name == "GL_ANGLE_framebuffer_multisample")
        return "GL_CHROMIUM_framebuffer_multisample";
    return name;
}

} // anonymous namespace

void GraphicsContext3DPrivate::initializeExtensions()
{
    if (m_initializedAvailableExtensions)
        return;

    m_initializedAvailableExtensions = true;
    bool success = makeContextCurrent();
    ASSERT(success);
    if (!success)
        return;

    String extensionsString = getString(GraphicsContext3D::EXTENSIONS);
    splitStringHelper(extensionsString, m_enabledExtensions);

    String requestableExtensionsString = m_impl->getRequestableExtensionsCHROMIUM();
    splitStringHelper(requestableExtensionsString, m_requestableExtensions);
}


bool GraphicsContext3DPrivate::supportsExtension(const String& name)
{
    initializeExtensions();
    String mappedName = mapExtensionName(name);
    return m_enabledExtensions.contains(mappedName) || m_requestableExtensions.contains(mappedName);
}

bool GraphicsContext3DPrivate::ensureExtensionEnabled(const String& name)
{
    initializeExtensions();

    String mappedName = mapExtensionName(name);
    if (m_enabledExtensions.contains(mappedName))
        return true;

    if (m_requestableExtensions.contains(mappedName)) {
        m_impl->requestExtensionCHROMIUM(mappedName.ascii().data());
        m_enabledExtensions.clear();
        m_requestableExtensions.clear();
        m_initializedAvailableExtensions = false;
    }

    initializeExtensions();
    return m_enabledExtensions.contains(mappedName);
}

bool GraphicsContext3DPrivate::isExtensionEnabled(const String& name)
{
    initializeExtensions();
    String mappedName = mapExtensionName(name);
    return m_enabledExtensions.contains(mappedName);
}

DELEGATE_TO_IMPL_4(postSubBufferCHROMIUM, int, int, int, int)

DELEGATE_TO_IMPL_4R(mapBufferSubDataCHROMIUM, GC3Denum, GC3Dsizeiptr, GC3Dsizei, GC3Denum, void*)
DELEGATE_TO_IMPL_1(unmapBufferSubDataCHROMIUM, const void*)
DELEGATE_TO_IMPL_9R(mapTexSubImage2DCHROMIUM, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, GC3Denum, void*)
DELEGATE_TO_IMPL_1(unmapTexSubImage2DCHROMIUM, const void*)

DELEGATE_TO_IMPL_1(setVisibilityCHROMIUM, bool);

DELEGATE_TO_IMPL_10(blitFramebufferCHROMIUM, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dbitfield, GC3Denum)
DELEGATE_TO_IMPL_5(renderbufferStorageMultisampleCHROMIUM, GC3Denum, GC3Dsizei, GC3Denum, GC3Dsizei, GC3Dsizei)

DELEGATE_TO_IMPL(rateLimitOffscreenContextCHROMIUM)
DELEGATE_TO_IMPL_R(getGraphicsResetStatusARB, GC3Denum)

DELEGATE_TO_IMPL_1R(getTranslatedShaderSourceANGLE, Platform3DObject, String)
DELEGATE_TO_IMPL_5(texImageIOSurface2DCHROMIUM, GC3Denum, GC3Dint, GC3Dint, GC3Duint, GC3Duint)
DELEGATE_TO_IMPL_5(texStorage2DEXT, GC3Denum, GC3Dint, GC3Duint, GC3Dint, GC3Dint)

//----------------------------------------------------------------------
// GraphicsContext3D
//

// Macros to assist in delegating from GraphicsContext3D to
// GraphicsContext3DPrivate.

#define DELEGATE_TO_INTERNAL(name) \
void GraphicsContext3D::name() \
{ \
    m_private->name(); \
}

#define DELEGATE_TO_INTERNAL_R(name, rt)           \
rt GraphicsContext3D::name() \
{ \
    return m_private->name(); \
}

#define DELEGATE_TO_INTERNAL_1(name, t1) \
void GraphicsContext3D::name(t1 a1) \
{ \
    m_private->name(a1); \
}

#define DELEGATE_TO_INTERNAL_1R(name, t1, rt)    \
rt GraphicsContext3D::name(t1 a1) \
{ \
    return m_private->name(a1); \
}

#define DELEGATE_TO_INTERNAL_2(name, t1, t2) \
void GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    m_private->name(a1, a2); \
}

#define DELEGATE_TO_INTERNAL_2R(name, t1, t2, rt)  \
rt GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    return m_private->name(a1, a2); \
}

#define DELEGATE_TO_INTERNAL_3(name, t1, t2, t3)   \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)    \
{ \
    m_private->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_INTERNAL_3R(name, t1, t2, t3, rt)   \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)    \
{ \
    return m_private->name(a1, a2, a3);                  \
}

#define DELEGATE_TO_INTERNAL_4(name, t1, t2, t3, t4)    \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    m_private->name(a1, a2, a3, a4);              \
}

#define DELEGATE_TO_INTERNAL_4R(name, t1, t2, t3, t4, rt)    \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)  \
{ \
    return m_private->name(a1, a2, a3, a4);           \
}

#define DELEGATE_TO_INTERNAL_5(name, t1, t2, t3, t4, t5)      \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)        \
{ \
    m_private->name(a1, a2, a3, a4, a5);   \
}

#define DELEGATE_TO_INTERNAL_6(name, t1, t2, t3, t4, t5, t6)  \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_private->name(a1, a2, a3, a4, a5, a6);   \
}

#define DELEGATE_TO_INTERNAL_6R(name, t1, t2, t3, t4, t5, t6, rt)  \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_private->name(a1, a2, a3, a4, a5, a6);       \
}

#define DELEGATE_TO_INTERNAL_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_private->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_INTERNAL_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_private->name(a1, a2, a3, a4, a5, a6, a7);   \
}

#define DELEGATE_TO_INTERNAL_8(name, t1, t2, t3, t4, t5, t6, t7, t8)       \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_private->name(a1, a2, a3, a4, a5, a6, a7, a8);      \
}

#define DELEGATE_TO_INTERNAL_9(name, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    m_private->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

#define DELEGATE_TO_INTERNAL_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_private->name(a1, a2, a3, a4, a5, a6, a7, a8, a9);   \
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes, HostWindow*, bool)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
    m_private->setContextLostCallback(nullptr);
    m_private->setSwapBuffersCompleteCallbackCHROMIUM(nullptr);
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    return createGraphicsContext(attrs, hostWindow, renderStyle, GraphicsContext3DPrivate::ForUseOnThisThread);
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_private->platformGraphicsContext3D();
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_private->platformTexture();
}

#if USE(SKIA)
GrContext* GraphicsContext3D::grContext()
{
    return m_private->grContext();
}
#endif

void GraphicsContext3D::prepareTexture()
{
    return m_private->prepareTexture();
}

IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    return m_private->getInternalFramebufferSize();
}

bool GraphicsContext3D::isResourceSafe()
{
    return m_private->isResourceSafe();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return 0;
}
#endif

DELEGATE_TO_INTERNAL_R(makeContextCurrent, bool)
DELEGATE_TO_INTERNAL_2(reshape, int, int)

DELEGATE_TO_INTERNAL_1(activeTexture, GC3Denum)
DELEGATE_TO_INTERNAL_2(attachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_INTERNAL_3(bindAttribLocation, Platform3DObject, GC3Duint, const String&)

DELEGATE_TO_INTERNAL_2(bindBuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_INTERNAL_2(bindFramebuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_INTERNAL_2(bindRenderbuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_INTERNAL_2(bindTexture, GC3Denum, Platform3DObject)
DELEGATE_TO_INTERNAL_4(blendColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_INTERNAL_1(blendEquation, GC3Denum)
DELEGATE_TO_INTERNAL_2(blendEquationSeparate, GC3Denum, GC3Denum)
DELEGATE_TO_INTERNAL_2(blendFunc, GC3Denum, GC3Denum)
DELEGATE_TO_INTERNAL_4(blendFuncSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

DELEGATE_TO_INTERNAL_3(bufferData, GC3Denum, GC3Dsizeiptr, GC3Denum)
DELEGATE_TO_INTERNAL_4(bufferData, GC3Denum, GC3Dsizeiptr, const void*, GC3Denum)
DELEGATE_TO_INTERNAL_4(bufferSubData, GC3Denum, GC3Dintptr, GC3Dsizeiptr, const void*)

DELEGATE_TO_INTERNAL_1R(checkFramebufferStatus, GC3Denum, GC3Denum)
DELEGATE_TO_INTERNAL_1(clear, GC3Dbitfield)
DELEGATE_TO_INTERNAL_4(clearColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_INTERNAL_1(clearDepth, GC3Dclampf)
DELEGATE_TO_INTERNAL_1(clearStencil, GC3Dint)
DELEGATE_TO_INTERNAL_4(colorMask, GC3Dboolean, GC3Dboolean, GC3Dboolean, GC3Dboolean)
DELEGATE_TO_INTERNAL_1(compileShader, Platform3DObject)

DELEGATE_TO_INTERNAL_8(compressedTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, const void*)
DELEGATE_TO_INTERNAL_9(compressedTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Denum, GC3Dsizei, const void*)
DELEGATE_TO_INTERNAL_8(copyTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Dint)
DELEGATE_TO_INTERNAL_8(copyTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_INTERNAL_1(cullFace, GC3Denum)
DELEGATE_TO_INTERNAL_1(depthFunc, GC3Denum)
DELEGATE_TO_INTERNAL_1(depthMask, GC3Dboolean)
DELEGATE_TO_INTERNAL_2(depthRange, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_INTERNAL_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_INTERNAL_1(disable, GC3Denum)
DELEGATE_TO_INTERNAL_1(disableVertexAttribArray, GC3Duint)
DELEGATE_TO_INTERNAL_3(drawArrays, GC3Denum, GC3Dint, GC3Dsizei)
DELEGATE_TO_INTERNAL_4(drawElements, GC3Denum, GC3Dsizei, GC3Denum, GC3Dintptr)

DELEGATE_TO_INTERNAL_1(enable, GC3Denum)
DELEGATE_TO_INTERNAL_1(enableVertexAttribArray, GC3Duint)
DELEGATE_TO_INTERNAL(finish)
DELEGATE_TO_INTERNAL(flush)
DELEGATE_TO_INTERNAL_4(framebufferRenderbuffer, GC3Denum, GC3Denum, GC3Denum, Platform3DObject)
DELEGATE_TO_INTERNAL_5(framebufferTexture2D, GC3Denum, GC3Denum, GC3Denum, Platform3DObject, GC3Dint)
DELEGATE_TO_INTERNAL_1(frontFace, GC3Denum)
DELEGATE_TO_INTERNAL_1(generateMipmap, GC3Denum)

DELEGATE_TO_INTERNAL_3R(getActiveAttrib, Platform3DObject, GC3Duint, ActiveInfo&, bool)
DELEGATE_TO_INTERNAL_3R(getActiveUniform, Platform3DObject, GC3Duint, ActiveInfo&, bool)
DELEGATE_TO_INTERNAL_4(getAttachedShaders, Platform3DObject, GC3Dsizei, GC3Dsizei*, Platform3DObject*)
DELEGATE_TO_INTERNAL_2R(getAttribLocation, Platform3DObject, const String&, GC3Dint)
DELEGATE_TO_INTERNAL_2(getBooleanv, GC3Denum, GC3Dboolean*)
DELEGATE_TO_INTERNAL_3(getBufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_R(getContextAttributes, GraphicsContext3D::Attributes)
DELEGATE_TO_INTERNAL_R(getError, GC3Denum)
DELEGATE_TO_INTERNAL_2(getFloatv, GC3Denum, GC3Dfloat*)
DELEGATE_TO_INTERNAL_4(getFramebufferAttachmentParameteriv, GC3Denum, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_2(getIntegerv, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_3(getProgramiv, Platform3DObject, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_1R(getProgramInfoLog, Platform3DObject, String)
DELEGATE_TO_INTERNAL_3(getRenderbufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_3(getShaderiv, Platform3DObject, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_1R(getShaderInfoLog, Platform3DObject, String)
DELEGATE_TO_INTERNAL_1R(getShaderSource, Platform3DObject, String)
DELEGATE_TO_INTERNAL_1R(getString, GC3Denum, String)
DELEGATE_TO_INTERNAL_3(getTexParameterfv, GC3Denum, GC3Denum, GC3Dfloat*)
DELEGATE_TO_INTERNAL_3(getTexParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_3(getUniformfv, Platform3DObject, GC3Dint, GC3Dfloat*)
DELEGATE_TO_INTERNAL_3(getUniformiv, Platform3DObject, GC3Dint, GC3Dint*)
DELEGATE_TO_INTERNAL_2R(getUniformLocation, Platform3DObject, const String&, GC3Dint)
DELEGATE_TO_INTERNAL_3(getVertexAttribfv, GC3Duint, GC3Denum, GC3Dfloat*)
DELEGATE_TO_INTERNAL_3(getVertexAttribiv, GC3Duint, GC3Denum, GC3Dint*)
DELEGATE_TO_INTERNAL_2R(getVertexAttribOffset, GC3Duint, GC3Denum, GC3Dsizeiptr)

DELEGATE_TO_INTERNAL_2(hint, GC3Denum, GC3Denum)
DELEGATE_TO_INTERNAL_1R(isBuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_INTERNAL_1R(isEnabled, GC3Denum, GC3Dboolean)
DELEGATE_TO_INTERNAL_1R(isFramebuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_INTERNAL_1R(isProgram, Platform3DObject, GC3Dboolean)
DELEGATE_TO_INTERNAL_1R(isRenderbuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_INTERNAL_1R(isShader, Platform3DObject, GC3Dboolean)
DELEGATE_TO_INTERNAL_1R(isTexture, Platform3DObject, GC3Dboolean)
DELEGATE_TO_INTERNAL_1(lineWidth, GC3Dfloat)
DELEGATE_TO_INTERNAL_1(linkProgram, Platform3DObject)
DELEGATE_TO_INTERNAL_2(pixelStorei, GC3Denum, GC3Dint)
DELEGATE_TO_INTERNAL_2(polygonOffset, GC3Dfloat, GC3Dfloat)

DELEGATE_TO_INTERNAL_7(readPixels, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, void*)

DELEGATE_TO_INTERNAL(releaseShaderCompiler)
DELEGATE_TO_INTERNAL_4(renderbufferStorage, GC3Denum, GC3Denum, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_INTERNAL_2(sampleCoverage, GC3Dclampf, GC3Dboolean)
DELEGATE_TO_INTERNAL_4(scissor, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_INTERNAL_2(shaderSource, Platform3DObject, const String&)
DELEGATE_TO_INTERNAL_3(stencilFunc, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_INTERNAL_4(stencilFuncSeparate, GC3Denum, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_INTERNAL_1(stencilMask, GC3Duint)
DELEGATE_TO_INTERNAL_2(stencilMaskSeparate, GC3Denum, GC3Duint)
DELEGATE_TO_INTERNAL_3(stencilOp, GC3Denum, GC3Denum, GC3Denum)
DELEGATE_TO_INTERNAL_4(stencilOpSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

DELEGATE_TO_INTERNAL_9R(texImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dsizei, GC3Dsizei, GC3Dint, GC3Denum, GC3Denum, const void*, bool)
DELEGATE_TO_INTERNAL_3(texParameterf, GC3Denum, GC3Denum, GC3Dfloat)
DELEGATE_TO_INTERNAL_3(texParameteri, GC3Denum, GC3Denum, GC3Dint)
DELEGATE_TO_INTERNAL_9(texSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, const void*)

DELEGATE_TO_INTERNAL_2(uniform1f, GC3Dint, GC3Dfloat)
DELEGATE_TO_INTERNAL_3(uniform1fv, GC3Dint, GC3Dfloat*, GC3Dsizei)
DELEGATE_TO_INTERNAL_2(uniform1i, GC3Dint, GC3Dint)
DELEGATE_TO_INTERNAL_3(uniform1iv, GC3Dint, GC3Dint*, GC3Dsizei)
DELEGATE_TO_INTERNAL_3(uniform2f, GC3Dint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_INTERNAL_3(uniform2fv, GC3Dint, GC3Dfloat*, GC3Dsizei)
DELEGATE_TO_INTERNAL_3(uniform2i, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_INTERNAL_3(uniform2iv, GC3Dint, GC3Dint*, GC3Dsizei)
DELEGATE_TO_INTERNAL_4(uniform3f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_INTERNAL_3(uniform3fv, GC3Dint, GC3Dfloat*, GC3Dsizei)
DELEGATE_TO_INTERNAL_4(uniform3i, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_INTERNAL_3(uniform3iv, GC3Dint, GC3Dint*, GC3Dsizei)
DELEGATE_TO_INTERNAL_5(uniform4f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_INTERNAL_3(uniform4fv, GC3Dint, GC3Dfloat*, GC3Dsizei)
DELEGATE_TO_INTERNAL_5(uniform4i, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_INTERNAL_3(uniform4iv, GC3Dint, GC3Dint*, GC3Dsizei)
DELEGATE_TO_INTERNAL_4(uniformMatrix2fv, GC3Dint, GC3Dboolean, GC3Dfloat*, GC3Dsizei)
DELEGATE_TO_INTERNAL_4(uniformMatrix3fv, GC3Dint, GC3Dboolean, GC3Dfloat*, GC3Dsizei)
DELEGATE_TO_INTERNAL_4(uniformMatrix4fv, GC3Dint, GC3Dboolean, GC3Dfloat*, GC3Dsizei)

DELEGATE_TO_INTERNAL_1(useProgram, Platform3DObject)
DELEGATE_TO_INTERNAL_1(validateProgram, Platform3DObject)

DELEGATE_TO_INTERNAL_2(vertexAttrib1f, GC3Duint, GC3Dfloat)
DELEGATE_TO_INTERNAL_2(vertexAttrib1fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_INTERNAL_3(vertexAttrib2f, GC3Duint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_INTERNAL_2(vertexAttrib2fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_INTERNAL_4(vertexAttrib3f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_INTERNAL_2(vertexAttrib3fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_INTERNAL_5(vertexAttrib4f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_INTERNAL_2(vertexAttrib4fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_INTERNAL_6(vertexAttribPointer, GC3Duint, GC3Dint, GC3Denum, GC3Dboolean, GC3Dsizei, GC3Dintptr)

DELEGATE_TO_INTERNAL_4(viewport, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

DELEGATE_TO_INTERNAL(markLayerComposited)
DELEGATE_TO_INTERNAL(markContextChanged)

bool GraphicsContext3D::layerComposited() const
{
    return m_private->layerComposited();
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context, DrawingBuffer* drawingBuffer)
{
    return m_private->paintRenderingResultsToCanvas(context, drawingBuffer);
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    return m_private->paintRenderingResultsToImageData(drawingBuffer);
}

DELEGATE_TO_INTERNAL_1R(paintCompositedResultsToCanvas, CanvasRenderingContext*, bool)

bool GraphicsContext3D::paintsIntoCanvasBuffer() const
{
    return m_private->paintsIntoCanvasBuffer();
}

DELEGATE_TO_INTERNAL_R(createBuffer, Platform3DObject)
DELEGATE_TO_INTERNAL_R(createFramebuffer, Platform3DObject)
DELEGATE_TO_INTERNAL_R(createProgram, Platform3DObject)
DELEGATE_TO_INTERNAL_R(createRenderbuffer, Platform3DObject)
DELEGATE_TO_INTERNAL_1R(createShader, GC3Denum, Platform3DObject)
DELEGATE_TO_INTERNAL_R(createTexture, Platform3DObject)

DELEGATE_TO_INTERNAL_1(deleteBuffer, Platform3DObject)
DELEGATE_TO_INTERNAL_1(deleteFramebuffer, Platform3DObject)
DELEGATE_TO_INTERNAL_1(deleteProgram, Platform3DObject)
DELEGATE_TO_INTERNAL_1(deleteRenderbuffer, Platform3DObject)
DELEGATE_TO_INTERNAL_1(deleteShader, Platform3DObject)
DELEGATE_TO_INTERNAL_1(deleteTexture, Platform3DObject)

DELEGATE_TO_INTERNAL_1(synthesizeGLError, GC3Denum)
DELEGATE_TO_INTERNAL_R(getExtensions, Extensions3D*)

DELEGATE_TO_INTERNAL_1(setContextLostCallback, PassOwnPtr<GraphicsContext3D::ContextLostCallback>)

class GraphicsContextLostCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
public:
    virtual void onContextLost();
    static PassOwnPtr<GraphicsContextLostCallbackAdapter> create(PassOwnPtr<GraphicsContext3D::ContextLostCallback>);
    virtual ~GraphicsContextLostCallbackAdapter() { }
private:
    GraphicsContextLostCallbackAdapter(PassOwnPtr<GraphicsContext3D::ContextLostCallback> cb) : m_contextLostCallback(cb) { }
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
};

void GraphicsContextLostCallbackAdapter::onContextLost()
{
    if (m_contextLostCallback)
        m_contextLostCallback->onContextLost();
}

PassOwnPtr<GraphicsContextLostCallbackAdapter> GraphicsContextLostCallbackAdapter::create(PassOwnPtr<GraphicsContext3D::ContextLostCallback> cb)
{
    return adoptPtr(cb.get() ? new GraphicsContextLostCallbackAdapter(cb) : 0);
}

void GraphicsContext3DPrivate::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> cb)
{
    m_contextLostCallbackAdapter = GraphicsContextLostCallbackAdapter::create(cb);
    m_impl->setContextLostCallback(m_contextLostCallbackAdapter.get());
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_private->isGLES2Compliant();
}

class GraphicsContext3DSwapBuffersCompleteCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM {
public:
    virtual void onSwapBuffersComplete();
    static PassOwnPtr<GraphicsContext3DSwapBuffersCompleteCallbackAdapter> create(PassOwnPtr<Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM>);
    virtual ~GraphicsContext3DSwapBuffersCompleteCallbackAdapter() { }

private:
    GraphicsContext3DSwapBuffersCompleteCallbackAdapter(PassOwnPtr<Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM> cb) : m_swapBuffersCompleteCallback(cb) { }
    OwnPtr<Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM> m_swapBuffersCompleteCallback;
};

void GraphicsContext3DSwapBuffersCompleteCallbackAdapter::onSwapBuffersComplete()
{
    if (m_swapBuffersCompleteCallback)
        m_swapBuffersCompleteCallback->onSwapBuffersComplete();
}

PassOwnPtr<GraphicsContext3DSwapBuffersCompleteCallbackAdapter> GraphicsContext3DSwapBuffersCompleteCallbackAdapter::create(PassOwnPtr<Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM> cb)
{
    return adoptPtr(cb.get() ? new GraphicsContext3DSwapBuffersCompleteCallbackAdapter(cb) : 0);
}

void GraphicsContext3DPrivate::setSwapBuffersCompleteCallbackCHROMIUM(PassOwnPtr<Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM> cb)
{
    m_swapBuffersCompleteCallbackAdapter = GraphicsContext3DSwapBuffersCompleteCallbackAdapter::create(cb);
    m_impl->setSwapBuffersCompleteCallbackCHROMIUM(m_swapBuffersCompleteCallbackAdapter.get());
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
