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

#include "GraphicsContext3DPrivate.h"

#include "DrawingBuffer.h"
#include "Extensions3DChromium.h"
#include "GrContext.h"
#include "GrGLInterface.h"
#include "ImageBuffer.h"
#include <public/WebGraphicsContext3D.h>
#include <public/WebGraphicsMemoryAllocation.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace {

// The limit of the number of textures we hold in the GrContext's bitmap->texture cache.
const int maxGaneshTextureCacheCount = 2048;
// The limit of the bytes allocated toward textures in the GrContext's bitmap->texture cache.
const size_t maxGaneshTextureCacheBytes = 96 * 1024 * 1024;

}

namespace WebCore {

//----------------------------------------------------------------------
// GraphicsContext3DPrivate

GraphicsContext3DPrivate::GraphicsContext3DPrivate(PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, bool preserveDrawingBuffer)
    : m_impl(webContext.get())
    , m_ownedWebContext(webContext)
    , m_initializedAvailableExtensions(false)
    , m_layerComposited(false)
    , m_preserveDrawingBuffer(preserveDrawingBuffer)
    , m_resourceSafety(ResourceSafetyUnknown)
    , m_grContext(0)
{
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate(WebKit::WebGraphicsContext3D* webContext, GrContext* grContext, bool preserveDrawingBuffer)
    : m_impl(webContext)
    , m_initializedAvailableExtensions(false)
    , m_layerComposited(false)
    , m_preserveDrawingBuffer(preserveDrawingBuffer)
    , m_resourceSafety(ResourceSafetyUnknown)
    , m_grContext(grContext)
{
}


GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
    if (m_grContext) {
        m_impl->setMemoryAllocationChangedCallbackCHROMIUM(0);
        m_grContext->contextDestroyed();
    }
}

PassRefPtr<GraphicsContext3D> GraphicsContext3DPrivate::createGraphicsContextFromWebContext(PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, bool preserveDrawingBuffer)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(GraphicsContext3D::Attributes(), 0));

    OwnPtr<GraphicsContext3DPrivate> priv = adoptPtr(new GraphicsContext3DPrivate(webContext, preserveDrawingBuffer));
    context->m_private = priv.release();
    return context.release();
}

PassRefPtr<GraphicsContext3D> GraphicsContext3DPrivate::createGraphicsContextFromExternalWebContextAndGrContext(WebKit::WebGraphicsContext3D* webContext, GrContext* grContext, bool preserveDrawingBuffer)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(GraphicsContext3D::Attributes(), 0));

    OwnPtr<GraphicsContext3DPrivate> priv = adoptPtr(new GraphicsContext3DPrivate(webContext, grContext, preserveDrawingBuffer));
    context->m_private = priv.release();
    return context.release();
}

WebKit::WebGraphicsContext3D* GraphicsContext3DPrivate::extractWebGraphicsContext3D(GraphicsContext3D* context)
{
    if (!context)
        return 0;
    return context->m_private->webContext();
}

class GrMemoryAllocationChangedCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM {
public:
    GrMemoryAllocationChangedCallbackAdapter(GrContext* context)
        : m_context(context)
    {
    }

    virtual void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation allocation) OVERRIDE
    {
        if (!m_context)
            return;

        if (!allocation.gpuResourceSizeInBytes) {
            m_context->freeGpuResources();
            m_context->setTextureCacheLimits(0, 0);
        } else
            m_context->setTextureCacheLimits(maxGaneshTextureCacheCount, maxGaneshTextureCacheBytes);
    }

private:
    GrContext* m_context;
};

namespace {
void bindWebGraphicsContext3DGLContextCallback(const GrGLInterface* interface)
{
    reinterpret_cast<WebKit::WebGraphicsContext3D*>(interface->fCallbackData)->makeContextCurrent();
}
}

GrContext* GraphicsContext3DPrivate::grContext()
{
    if (m_grContext)
        return m_grContext;

    SkAutoTUnref<GrGLInterface> interface(m_impl->createGrGLInterface());
    if (!interface)
        return 0;

    interface->fCallback = bindWebGraphicsContext3DGLContextCallback;
    interface->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(m_impl);

    m_ownedGrContext.reset(GrContext::Create(kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(interface.get())));
    m_grContext = m_ownedGrContext;
    if (!m_grContext)
        return 0;

    m_grContext->setTextureCacheLimits(maxGaneshTextureCacheCount, maxGaneshTextureCacheBytes);
    m_grContextMemoryAllocationCallbackAdapter = adoptPtr(new GrMemoryAllocationChangedCallbackAdapter(m_grContext));
    m_impl->setMemoryAllocationChangedCallbackCHROMIUM(m_grContextMemoryAllocationCallbackAdapter.get());

    return m_grContext;
}

void GraphicsContext3DPrivate::markContextChanged()
{
    m_layerComposited = false;
}

bool GraphicsContext3DPrivate::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContext3DPrivate::markLayerComposited()
{
    m_layerComposited = true;
}

void GraphicsContext3DPrivate::paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer* imageBuffer)
{
    unsigned char* pixels = 0;
    size_t bufferSize = 4 * width * height;

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

    m_impl->readBackFramebuffer(pixels, 4 * width * height, framebuffer, width, height);

    if (premultiplyAlpha) {
        for (size_t i = 0; i < bufferSize; i += 4) {
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    readbackBitmap->notifyPixelsChanged();
    if (m_resizingBitmap.readyToDraw()) {
        // We need to draw the resizing bitmap into the canvas's backing store.
        SkCanvas canvas(*canvasBitmap);
        SkRect dst;
        dst.set(SkIntToScalar(0), SkIntToScalar(0), SkIntToScalar(canvasBitmap->width()), SkIntToScalar(canvasBitmap->height()));
        canvas.drawBitmapRect(m_resizingBitmap, 0, dst);
    }
}

class GraphicsContext3DContextLostCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
public:
    GraphicsContext3DContextLostCallbackAdapter(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
        : m_contextLostCallback(callback) { }
    virtual ~GraphicsContext3DContextLostCallbackAdapter() { }

    virtual void onContextLost()
    {
        if (m_contextLostCallback)
            m_contextLostCallback->onContextLost();
    }
private:
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
};

void GraphicsContext3DPrivate::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
{
    m_contextLostCallbackAdapter = adoptPtr(new GraphicsContext3DContextLostCallbackAdapter(callback));
    m_impl->setContextLostCallback(m_contextLostCallbackAdapter.get());
}

class GraphicsContext3DErrorMessageCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsErrorMessageCallback {
public:
    GraphicsContext3DErrorMessageCallbackAdapter(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback> callback)
        : m_errorMessageCallback(callback) { }
    virtual ~GraphicsContext3DErrorMessageCallbackAdapter() { }

    virtual void onErrorMessage(const WebKit::WebString& message, WebKit::WGC3Dint id)
    {
        if (m_errorMessageCallback)
            m_errorMessageCallback->onErrorMessage(message, id);
    }
private:
    OwnPtr<GraphicsContext3D::ErrorMessageCallback> m_errorMessageCallback;
};

void GraphicsContext3DPrivate::setErrorMessageCallback(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback> callback)
{
    m_errorMessageCallbackAdapter = adoptPtr(new GraphicsContext3DErrorMessageCallbackAdapter(callback));
    m_impl->setErrorMessageCallback(m_errorMessageCallbackAdapter.get());
}

Extensions3D* GraphicsContext3DPrivate::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DChromium(this));
    return m_extensions.get();
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
    bool success = m_impl->makeContextCurrent();
    ASSERT(success);
    if (!success)
        return;

    String extensionsString = m_impl->getString(GraphicsContext3D::EXTENSIONS);
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


bool GraphicsContext3DPrivate::isResourceSafe()
{
    if (m_resourceSafety == ResourceSafetyUnknown)
        m_resourceSafety = getExtensions()->isEnabled("GL_CHROMIUM_resource_safe") ? ResourceSafe : ResourceUnsafe;
    return m_resourceSafety == ResourceSafe;
}

} // namespace WebCore
