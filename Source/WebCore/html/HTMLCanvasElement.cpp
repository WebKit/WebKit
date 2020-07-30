/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "HTMLCanvasElement.h"

#include "Blob.h"
#include "BlobCallback.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "DisplayListDrawingContext.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GPUBasedCanvasRenderingContext.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "JSDOMConvertDictionary.h"
#include "MIMETypeRegistry.h"
#include "RenderElement.h"
#include "RenderHTMLCanvas.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "Settings.h"
#include "StringAdaptors.h"
#include <JavaScriptCore/JSCInlines.h>
#include <math.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/RAMSize.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(MEDIA_STREAM)
#include "CanvasCaptureMediaStreamTrack.h"
#include "MediaStream.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLContextAttributes.h"
#include "WebGLRenderingContext.h"
#endif

#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif

#if ENABLE(WEBGPU)
#include "GPUCanvasContext.h"
#endif

#if ENABLE(WEBXR)
#include "DOMWindow.h"
#include "Navigator.h"
#include "NavigatorWebXR.h"
#include "WebXRSystem.h"
#endif

#if USE(CG)
#include "ImageBufferUtilitiesCG.h"
#endif

#if USE(GSTREAMER)
#include "MediaSampleGStreamer.h"
#endif

#if PLATFORM(COCOA)
#include "MediaSampleAVFObjC.h"
#include <pal/cf/CoreMediaSoftLink.h>
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLCanvasElement);

using namespace PAL;
using namespace HTMLNames;

// These values come from the WhatWG/W3C HTML spec.
const int defaultWidth = 300;
const int defaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size. The maximum canvas size is in device pixels.
#if PLATFORM(IOS_FAMILY)
const unsigned maxCanvasArea = 4096 * 4096;
#else
const unsigned maxCanvasArea = 16384 * 16384;
#endif

static size_t maxActivePixelMemoryForTesting = 0;

HTMLCanvasElement::HTMLCanvasElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , CanvasBase(IntSize(defaultWidth, defaultHeight))
    , ActiveDOMObject(document)
{
    ASSERT(hasTagName(canvasTag));
    addObserver(document);
}

Ref<HTMLCanvasElement> HTMLCanvasElement::create(Document& document)
{
    auto canvas = adoptRef(*new HTMLCanvasElement(canvasTag, document));
    canvas->suspendIfNeeded();
    return canvas;
}

Ref<HTMLCanvasElement> HTMLCanvasElement::create(const QualifiedName& tagName, Document& document)
{
    auto canvas = adoptRef(*new HTMLCanvasElement(tagName, document));
    canvas->suspendIfNeeded();
    return canvas;
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    // FIXME: This has to be called here because CSSCanvasValue::CanvasObserverProxy::canvasDestroyed()
    // downcasts the CanvasBase object to HTMLCanvasElement. That invokes virtual methods, which should be
    // avoided in destructors, but works as long as it's done before HTMLCanvasElement destructs completely.
    notifyObserversCanvasDestroyed();
    document().clearCanvasPreparation(this);

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.
    setImageBuffer(nullptr);
}

void HTMLCanvasElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == widthAttr || name == heightAttr)
        reset();
    HTMLElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> HTMLCanvasElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    RefPtr<Frame> frame = document().frame();
    if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return createRenderer<RenderHTMLCanvas>(*this, WTFMove(style));
    return HTMLElement::createElementRenderer(WTFMove(style), insertionPosition);
}

bool HTMLCanvasElement::canContainRangeEndPoint() const
{
    return false;
}

bool HTMLCanvasElement::canStartSelection() const
{
    return false;
}

ExceptionOr<void> HTMLCanvasElement::setHeight(unsigned value)
{
    if (m_context && m_context->isPlaceholder())
        return Exception { InvalidStateError };
    setAttributeWithoutSynchronization(heightAttr, AtomString::number(limitToOnlyHTMLNonNegative(value, defaultHeight)));
    return { };
}

ExceptionOr<void> HTMLCanvasElement::setWidth(unsigned value)
{
    if (m_context && m_context->isPlaceholder())
        return Exception { InvalidStateError };
    setAttributeWithoutSynchronization(widthAttr, AtomString::number(limitToOnlyHTMLNonNegative(value, defaultWidth)));
    return { };
}

void HTMLCanvasElement::setSize(const IntSize& newSize)
{
    if (newSize == size())
        return;

    m_ignoreReset = true;
    setWidth(newSize.width());
    setHeight(newSize.height());
    m_ignoreReset = false;
    reset();
}

static inline size_t maxActivePixelMemory()
{
    if (maxActivePixelMemoryForTesting)
        return maxActivePixelMemoryForTesting;

    static size_t maxPixelMemory;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(IOS_FAMILY)
        maxPixelMemory = ramSize() / 4;
#else
        maxPixelMemory = std::max(ramSize() / 4, 2151 * MB);
#endif
    });

    return maxPixelMemory;
}

void HTMLCanvasElement::setMaxPixelMemoryForTesting(size_t size)
{
    maxActivePixelMemoryForTesting = size;
}

ExceptionOr<Optional<RenderingContext>> HTMLCanvasElement::getContext(JSC::JSGlobalObject& state, const String& contextId, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_context) {
        if (m_context->isPlaceholder())
            return Exception { InvalidStateError };

        if (m_context->is2d()) {
            if (!is2dType(contextId))
                return Optional<RenderingContext> { WTF::nullopt };
            return Optional<RenderingContext> { RefPtr<CanvasRenderingContext2D> { &downcast<CanvasRenderingContext2D>(*m_context) } };
        }

        if (m_context->isBitmapRenderer()) {
            if (!isBitmapRendererType(contextId))
                return Optional<RenderingContext> { WTF::nullopt };
            return Optional<RenderingContext> { RefPtr<ImageBitmapRenderingContext> { &downcast<ImageBitmapRenderingContext>(*m_context) } };
        }

#if ENABLE(WEBGL)
        if (m_context->isWebGL()) {
            if (!isWebGLType(contextId))
                return Optional<RenderingContext> { WTF::nullopt };
            if (is<WebGLRenderingContext>(*m_context))
                return Optional<RenderingContext> { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } };
#if ENABLE(WEBGL2)
            ASSERT(is<WebGL2RenderingContext>(*m_context));
            return Optional<RenderingContext> { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*m_context) } };
#endif
        }
#endif

#if ENABLE(WEBGPU)
        if (m_context->isWebGPU()) {
            if (!isWebGPUType(contextId))
                return Optional<RenderingContext> { WTF::nullopt };
            return Optional<RenderingContext> { RefPtr<GPUCanvasContext> { &downcast<GPUCanvasContext>(*m_context) } };
        }
#endif

        ASSERT_NOT_REACHED();
        return Optional<RenderingContext> { WTF::nullopt };
    }

    if (is2dType(contextId)) {
        auto context = createContext2d(contextId);
        if (!context)
            return Optional<RenderingContext> { WTF::nullopt };
        return Optional<RenderingContext> { RefPtr<CanvasRenderingContext2D> { context } };
    }

    if (isBitmapRendererType(contextId)) {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<ImageBitmapRenderingContextSettings>>(state, !arguments.isEmpty() ? arguments[0].get() : JSC::jsUndefined());
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        auto context = createContextBitmapRenderer(contextId, WTFMove(attributes));
        if (!context)
            return Optional<RenderingContext> { WTF::nullopt };
        return Optional<RenderingContext> { RefPtr<ImageBitmapRenderingContext> { context } };
    }

#if ENABLE(WEBGL)
    if (isWebGLType(contextId)) {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, !arguments.isEmpty() ? arguments[0].get() : JSC::jsUndefined());
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        auto context = createContextWebGL(contextId, WTFMove(attributes));
        if (!context)
            return Optional<RenderingContext> { WTF::nullopt };

        if (is<WebGLRenderingContext>(*context))
            return Optional<RenderingContext> { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*context) } };
#if ENABLE(WEBGL2)
        ASSERT(is<WebGL2RenderingContext>(*context));
        return Optional<RenderingContext> { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*context) } };
#endif
    }
#endif

#if ENABLE(WEBGPU)
    if (isWebGPUType(contextId)) {
        auto context = createContextWebGPU(contextId);
        if (!context)
            return Optional<RenderingContext> { WTF::nullopt };
        return Optional<RenderingContext> { RefPtr<GPUCanvasContext> { context } };
    }
#endif

    return Optional<RenderingContext> { WTF::nullopt };
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type)
{
    if (HTMLCanvasElement::is2dType(type))
        return getContext2d(type);

    if (HTMLCanvasElement::isBitmapRendererType(type))
        return getContextBitmapRenderer(type);

#if ENABLE(WEBGL)
    if (HTMLCanvasElement::isWebGLType(type))
        return getContextWebGL(type);
#endif

#if ENABLE(WEBGPU)
    if (HTMLCanvasElement::isWebGPUType(type))
        return getContextWebGPU(type);
#endif

    return nullptr;
}

bool HTMLCanvasElement::is2dType(const String& type)
{
    return type == "2d";
}

CanvasRenderingContext2D* HTMLCanvasElement::createContext2d(const String& type)
{
    ASSERT_UNUSED(HTMLCanvasElement::is2dType(type), type);
    ASSERT(!m_context);

    // Make sure we don't use more pixel memory than the system can support.
    size_t requestedPixelMemory = 4 * width() * height();
    if (activePixelMemory() + requestedPixelMemory > maxActivePixelMemory()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Total canvas memory use exceeds the maximum limit (");
        stringBuilder.appendNumber(maxActivePixelMemory() / 1024 / 1024);
        stringBuilder.appendLiteral(" MB).");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return nullptr;
    }

    m_context = CanvasRenderingContext2D::create(*this, document().inQuirksMode());

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
    // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
    invalidateStyleAndLayerComposition();
#endif

    return static_cast<CanvasRenderingContext2D*>(m_context.get());
}

CanvasRenderingContext2D* HTMLCanvasElement::getContext2d(const String& type)
{
    ASSERT_UNUSED(HTMLCanvasElement::is2dType(type), type);

    if (m_context && !m_context->is2d())
        return nullptr;

    if (!m_context)
        return createContext2d(type);
    return static_cast<CanvasRenderingContext2D*>(m_context.get());
}

#if ENABLE(WEBGL)

static bool requiresAcceleratedCompositingForWebGL()
{
#if PLATFORM(GTK) || PLATFORM(WIN_CAIRO)
    return false;
#else
    return true;
#endif

}
static bool shouldEnableWebGL(const Settings& settings)
{
    if (!settings.webGLEnabled())
        return false;

    if (!requiresAcceleratedCompositingForWebGL())
        return true;

    return settings.acceleratedCompositingEnabled();
}

bool HTMLCanvasElement::isWebGLType(const String& type)
{
    // Retain support for the legacy "webkit-3d" name.
    return type == "webgl" || type == "experimental-webgl"
#if ENABLE(WEBGL2)
        || type == "webgl2"
#endif
        || type == "webkit-3d";
}

WebGLRenderingContextBase* HTMLCanvasElement::createContextWebGL(const String& type, WebGLContextAttributes&& attrs)
{
    ASSERT(HTMLCanvasElement::isWebGLType(type));
    ASSERT(!m_context);

    if (!shouldEnableWebGL(document().settings()))
        return nullptr;

#if ENABLE(WEBXR)
    // https://immersive-web.github.io/webxr/#xr-compatible
    if (attrs.xrCompatible) {
        if (auto* window = document().domWindow())
            NavigatorWebXR::xr(window->navigator()).ensureImmersiveXRDeviceIsSelected();
    }
#endif

    // TODO(WEBXR): ensure the context is created in a compatible graphics
    // adapter when there is an active immersive device.
    m_context = WebGLRenderingContextBase::create(*this, attrs, type);
    if (m_context) {
        // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
        invalidateStyleAndLayerComposition();
#if ENABLE(WEBXR)
        ASSERT(!attrs.xrCompatible || downcast<WebGLRenderingContextBase>(m_context.get())->isXRCompatible());
#endif
    }

    return downcast<WebGLRenderingContextBase>(m_context.get());
}

WebGLRenderingContextBase* HTMLCanvasElement::getContextWebGL(const String& type, WebGLContextAttributes&& attrs)
{
    ASSERT(HTMLCanvasElement::isWebGLType(type));

    if (!shouldEnableWebGL(document().settings()))
        return nullptr;

    if (m_context && !m_context->isWebGL())
        return nullptr;

    if (!m_context)
        return createContextWebGL(type, WTFMove(attrs));
    return &downcast<WebGLRenderingContextBase>(*m_context);
}

#endif // ENABLE(WEBGL)

#if ENABLE(WEBGPU)

bool HTMLCanvasElement::isWebGPUType(const String& type)
{
    return type == "gpu";
}

GPUCanvasContext* HTMLCanvasElement::createContextWebGPU(const String& type)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isWebGPUType(type));
    ASSERT(!m_context);

    if (!RuntimeEnabledFeatures::sharedFeatures().webGPUEnabled())
        return nullptr;

    m_context = GPUCanvasContext::create(*this);
    if (m_context) {
        // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
        invalidateStyleAndLayerComposition();
    }

    return static_cast<GPUCanvasContext*>(m_context.get());
}

GPUCanvasContext* HTMLCanvasElement::getContextWebGPU(const String& type)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isWebGPUType(type));

    if (!RuntimeEnabledFeatures::sharedFeatures().webGPUEnabled())
        return nullptr;

    if (m_context && !m_context->isWebGPU())
        return nullptr;

    if (!m_context)
        return createContextWebGPU(type);
    return static_cast<GPUCanvasContext*>(m_context.get());
}

#endif // ENABLE(WEBGPU)

bool HTMLCanvasElement::isBitmapRendererType(const String& type)
{
    return type == "bitmaprenderer";
}

ImageBitmapRenderingContext* HTMLCanvasElement::createContextBitmapRenderer(const String& type, ImageBitmapRenderingContextSettings&& settings)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isBitmapRendererType(type));
    ASSERT(!m_context);

    m_context = ImageBitmapRenderingContext::create(*this, WTFMove(settings));

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
    // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
    invalidateStyleAndLayerComposition();
#endif

    return static_cast<ImageBitmapRenderingContext*>(m_context.get());
}

ImageBitmapRenderingContext* HTMLCanvasElement::getContextBitmapRenderer(const String& type, ImageBitmapRenderingContextSettings&& settings)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isBitmapRendererType(type));
    if (!m_context)
        return createContextBitmapRenderer(type, WTFMove(settings));
    return static_cast<ImageBitmapRenderingContext*>(m_context.get());
}

void HTMLCanvasElement::didDraw(const FloatRect& rect)
{
    clearCopiedImage();

    FloatRect dirtyRect = rect;
    if (auto* renderer = renderBox()) {
        FloatRect destRect;
        if (is<RenderReplaced>(renderer))
            destRect = downcast<RenderReplaced>(renderer)->replacedContentRect();
        else
            destRect = renderer->contentBoxRect();

        // Inflate dirty rect to cover antialiasing on image buffers.
        if (drawingContext() && drawingContext()->shouldAntialias())
            dirtyRect.inflate(1);

        FloatRect r = mapRect(dirtyRect, FloatRect(0, 0, size().width(), size().height()), destRect);
        r.intersect(destRect);

        if (!r.isEmpty() && !m_dirtyRect.contains(r)) {
            m_dirtyRect.unite(r);
            renderer->repaintRectangle(enclosingIntRect(m_dirtyRect));
        }
    }
    notifyObserversCanvasChanged(dirtyRect);
}

void HTMLCanvasElement::reset()
{
    if (m_ignoreReset)
        return;

    bool hadImageBuffer = hasCreatedImageBuffer();

    int w = limitToOnlyHTMLNonNegative(attributeWithoutSynchronization(widthAttr), defaultWidth);
    int h = limitToOnlyHTMLNonNegative(attributeWithoutSynchronization(heightAttr), defaultHeight);

    resetGraphicsContextState();
    if (is<CanvasRenderingContext2D>(m_context.get()))
        downcast<CanvasRenderingContext2D>(*m_context).reset();

    IntSize oldSize = size();
    IntSize newSize(w, h);
    // If the size of an existing buffer matches, we can just clear it instead of reallocating.
    // This optimization is only done for 2D canvases for now.
    if (m_hasCreatedImageBuffer && oldSize == newSize && m_context && m_context->is2d()) {
        if (!m_didClearImageBuffer)
            clearImageBuffer();
        return;
    }

    setSurfaceSize(newSize);

    if (isGPUBased() && oldSize != size())
        downcast<GPUBasedCanvasRenderingContext>(*m_context).reshape(width(), height());

    auto renderer = this->renderer();
    if (is<RenderHTMLCanvas>(renderer)) {
        auto& canvasRenderer = downcast<RenderHTMLCanvas>(*renderer);
        if (oldSize != size()) {
            canvasRenderer.canvasSizeChanged();
            if (canvasRenderer.hasAcceleratedCompositing())
                canvasRenderer.contentChanged(CanvasChanged);
        }
        if (hadImageBuffer)
            canvasRenderer.repaint();
    }

    notifyObserversCanvasResized();
}

bool HTMLCanvasElement::paintsIntoCanvasBuffer() const
{
    ASSERT(m_context);
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (m_context->is2d() || m_context->isBitmapRenderer())
        return true;
#endif

    if (!m_context->isAccelerated())
        return true;

    if (renderBox() && renderBox()->hasAcceleratedCompositing())
        return false;

    return true;
}


void HTMLCanvasElement::paint(GraphicsContext& context, const LayoutRect& r)
{
    // Clear the dirty rect
    m_dirtyRect = FloatRect();

    if (!context.paintingDisabled()) {
        bool shouldPaint = true;

        if (m_context) {
            shouldPaint = paintsIntoCanvasBuffer() || document().printing() || m_isSnapshotting;
            if (shouldPaint)
                m_context->paintRenderingResultsToCanvas();
        }

        if (shouldPaint) {
            if (hasCreatedImageBuffer()) {
                if (m_presentedImage)
                    context.drawImage(*m_presentedImage, snappedIntRect(r), renderer()->imageOrientation());
                else if (ImageBuffer* imageBuffer = buffer())
                    context.drawImageBuffer(*imageBuffer, snappedIntRect(r));
            }

            if (isGPUBased())
                downcast<GPUBasedCanvasRenderingContext>(*m_context).markLayerComposited();
        }
    }

    if (UNLIKELY(m_context && m_context->callTracingActive()))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*m_context);
}

bool HTMLCanvasElement::isGPUBased() const
{
    return m_context && m_context->isGPUBased();
}

void HTMLCanvasElement::makePresentationCopy()
{
    if (!m_presentedImage) {
        // The buffer contains the last presented data, so save a copy of it.
        m_presentedImage = buffer()->copyImage(CopyBackingStore, PreserveResolution::Yes);
    }
}

void HTMLCanvasElement::clearPresentationCopy()
{
    m_presentedImage = nullptr;
}

void HTMLCanvasElement::setSurfaceSize(const IntSize& size)
{
    CanvasBase::setSize(size);
    m_hasCreatedImageBuffer = false;
    setImageBuffer(nullptr);
    clearCopiedImage();
}

static String toEncodingMimeType(const String& mimeType)
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return "image/png"_s;
    return mimeType.convertToASCIILowercase();
}

// https://html.spec.whatwg.org/multipage/canvas.html#a-serialisation-of-the-bitmap-as-a-file
static Optional<double> qualityFromJSValue(JSC::JSValue qualityValue)
{
    if (!qualityValue.isNumber())
        return WTF::nullopt;

    double qualityNumber = qualityValue.asNumber();
    if (qualityNumber < 0 || qualityNumber > 1)
        return WTF::nullopt;

    return qualityNumber;
}

ExceptionOr<UncachedString> HTMLCanvasElement::toDataURL(const String& mimeType, JSC::JSValue qualityValue)
{
    if (!originClean())
        return Exception { SecurityError };

    if (size().isEmpty() || !buffer())
        return UncachedString { "data:,"_s };
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    auto encodingMIMEType = toEncodingMimeType(mimeType);
    auto quality = qualityFromJSValue(qualityValue);

#if USE(CG)
    // Try to get ImageData first, as that may avoid lossy conversions.
    if (auto imageData = getImageData())
        return UncachedString { dataURL(*imageData, encodingMIMEType, quality) };
#endif

    makeRenderingResultsAvailable();

    return UncachedString { buffer()->toDataURL(encodingMIMEType, quality) };
}

ExceptionOr<UncachedString> HTMLCanvasElement::toDataURL(const String& mimeType)
{
    return toDataURL(mimeType, { });
}

ExceptionOr<void> HTMLCanvasElement::toBlob(ScriptExecutionContext& context, Ref<BlobCallback>&& callback, const String& mimeType, JSC::JSValue qualityValue)
{
    if (!originClean())
        return Exception { SecurityError };

    if (size().isEmpty() || !buffer()) {
        callback->scheduleCallback(context, nullptr);
        return { };
    }
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    auto encodingMIMEType = toEncodingMimeType(mimeType);
    auto quality = qualityFromJSValue(qualityValue);

#if USE(CG)
    if (auto imageData = getImageData()) {
        RefPtr<Blob> blob;
        Vector<uint8_t> blobData = data(*imageData, encodingMIMEType, quality);
        if (!blobData.isEmpty())
            blob = Blob::create(WTFMove(blobData), encodingMIMEType);
        callback->scheduleCallback(context, WTFMove(blob));
        return { };
    }
#endif

    makeRenderingResultsAvailable();

    RefPtr<Blob> blob;
    Vector<uint8_t> blobData = buffer()->toData(encodingMIMEType, quality);
    if (!blobData.isEmpty())
        blob = Blob::create(WTFMove(blobData), encodingMIMEType);
    callback->scheduleCallback(context, WTFMove(blob));
    return { };
}

RefPtr<ImageData> HTMLCanvasElement::getImageData()
{
#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(m_context.get())) {
        if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
            ResourceLoadObserver::shared().logCanvasRead(document());
        return downcast<WebGLRenderingContextBase>(*m_context).paintRenderingResultsToImageData();
    }
#endif
    return nullptr;
}

#if ENABLE(MEDIA_STREAM)

RefPtr<MediaSample> HTMLCanvasElement::toMediaSample()
{
    auto* imageBuffer = buffer();
    if (!imageBuffer)
        return nullptr;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

#if PLATFORM(COCOA)
    makeRenderingResultsAvailable();
    return MediaSampleAVFObjC::createImageSample(imageBuffer->toBGRAData(), width(), height());
#elif USE(GSTREAMER)
    makeRenderingResultsAvailable();
    return MediaSampleGStreamer::createImageSample(imageBuffer->toBGRAData(), width(), height());
#else
    return nullptr;
#endif
}

ExceptionOr<Ref<MediaStream>> HTMLCanvasElement::captureStream(Document& document, Optional<double>&& frameRequestRate)
{
    if (!originClean())
        return Exception(SecurityError, "Canvas is tainted"_s);
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(this->document());

    if (frameRequestRate && frameRequestRate.value() < 0)
        return Exception(NotSupportedError, "frameRequestRate is negative"_s);

    auto track = CanvasCaptureMediaStreamTrack::create(document, *this, WTFMove(frameRequestRate));
    auto stream =  MediaStream::create(document);
    stream->addTrack(track);
    return stream;
}
#endif

SecurityOrigin* HTMLCanvasElement::securityOrigin() const
{
    return &document().securityOrigin();
}

bool HTMLCanvasElement::shouldAccelerate(const IntSize& size) const
{
    auto& settings = document().settings();

    auto area = size.area<RecordOverflow>();
    if (area.hasOverflowed())
        return false;

    if (area > settings.maximumAccelerated2dCanvasSize())
        return false;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    return settings.canvasUsesAcceleratedDrawing();
#elif ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context && !m_context->is2d())
        return false;

    if (!settings.accelerated2dCanvasEnabled())
        return false;

    if (area < settings.minimumAccelerated2dCanvasSize())
        return false;

    return true;
#else
    UNUSED_PARAM(size);
    return false;
#endif
}

void HTMLCanvasElement::setUsesDisplayListDrawing(bool usesDisplayListDrawing)
{
    m_usesDisplayListDrawing = usesDisplayListDrawing;
}

void HTMLCanvasElement::setTracksDisplayListReplay(bool tracksDisplayListReplay)
{
    m_tracksDisplayListReplay = tracksDisplayListReplay;

    if (!buffer())
        return;

    auto& buffer = *this->buffer();
    if (buffer.drawingContext())
        buffer.drawingContext()->setTracksDisplayListReplay(m_tracksDisplayListReplay);
}

String HTMLCanvasElement::displayListAsText(DisplayList::AsTextFlags flags) const
{
    if (!buffer())
        return String();

    auto& buffer = *this->buffer();
    if (buffer.drawingContext())
        return buffer.drawingContext()->displayList().asText(flags);

    return String();
}

String HTMLCanvasElement::replayDisplayListAsText(DisplayList::AsTextFlags flags) const
{
    if (!buffer())
        return String();

    auto& buffer = *this->buffer();
    if (buffer.drawingContext() && buffer.drawingContext()->replayedDisplayList())
        return buffer.drawingContext()->replayedDisplayList()->asText(flags);

    return String();
}

void HTMLCanvasElement::createImageBuffer() const
{
    ASSERT(!hasCreatedImageBuffer());

    m_hasCreatedImageBuffer = true;
    m_didClearImageBuffer = true;

    // Perform multiplication as floating point to avoid overflow
    if (float(width()) * height() > maxCanvasArea) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Canvas area exceeds the maximum limit (width * height > ");
        stringBuilder.appendNumber(maxCanvasArea);
        stringBuilder.appendLiteral(").");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return;
    }
    
    // Make sure we don't use more pixel memory than the system can support.
    size_t requestedPixelMemory = 4 * width() * height();
    if (activePixelMemory() + requestedPixelMemory > maxActivePixelMemory()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Total canvas memory use exceeds the maximum limit (");
        stringBuilder.appendNumber(maxActivePixelMemory() / 1024 / 1024);
        stringBuilder.appendLiteral(" MB).");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return;
    }

    if (!width() || !height())
        return;

    auto hostWindow = (document().view() && document().view()->root()) ? document().view()->root()->hostWindow() : nullptr;

    auto accelerate = shouldAccelerate(size()) ? ShouldAccelerate::Yes : ShouldAccelerate::No;
    // FIXME: Add a new setting for DisplayList drawing on canvas.
    auto useDisplayList = m_usesDisplayListDrawing.valueOr(document().settings().displayListDrawingEnabled()) ? ShouldUseDisplayList::Yes : ShouldUseDisplayList::No;
    setImageBuffer(ImageBuffer::create(size(), accelerate, useDisplayList, RenderingPurpose::Canvas, 1, ColorSpace::SRGB, hostWindow));

    if (buffer() && buffer()->drawingContext())
        buffer()->drawingContext()->setTracksDisplayListReplay(m_tracksDisplayListReplay);

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context && m_context->is2d()) {
        // Recalculate compositing requirements if acceleration state changed.
        const_cast<HTMLCanvasElement*>(this)->invalidateStyleAndLayerComposition();
    }
#endif
}

void HTMLCanvasElement::setImageBufferAndMarkDirty(std::unique_ptr<ImageBuffer>&& buffer)
{
    m_hasCreatedImageBuffer = true;
    setImageBuffer(WTFMove(buffer));
    didDraw(FloatRect(FloatPoint(), size()));
}

Image* HTMLCanvasElement::copiedImage() const
{
    if (!m_copiedImage && buffer()) {
        if (m_context)
            m_context->paintRenderingResultsToCanvas();
        m_copiedImage = buffer()->copyImage(CopyBackingStore, PreserveResolution::Yes);
    }
    return m_copiedImage.get();
}

void HTMLCanvasElement::clearImageBuffer() const
{
    ASSERT(m_hasCreatedImageBuffer);
    ASSERT(!m_didClearImageBuffer);
    ASSERT(m_context);

    m_didClearImageBuffer = true;

    if (is<CanvasRenderingContext2D>(*m_context)) {
        // No need to undo transforms/clip/etc. because we are called right after the context is reset.
        downcast<CanvasRenderingContext2D>(*m_context).clearRect(0, 0, width(), height());
    }
}

void HTMLCanvasElement::clearCopiedImage()
{
    m_copiedImage = nullptr;
    m_didClearImageBuffer = false;
}

const char* HTMLCanvasElement::activeDOMObjectName() const
{
    return "HTMLCanvasElement";
}

bool HTMLCanvasElement::virtualHasPendingActivity() const
{
#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(m_context.get())) {
        // WebGL rendering context may fire contextlost / contextchange / contextrestored events at any point.
        return m_hasRelevantWebGLEventListener && !downcast<WebGLRenderingContextBase>(*m_context).isContextUnrecoverablyLost();
    }
#endif

    return false;
}

void HTMLCanvasElement::eventListenersDidChange()
{
#if ENABLE(WEBGL)
    m_hasRelevantWebGLEventListener = hasEventListeners(eventNames().webglcontextchangedEvent)
        || hasEventListeners(eventNames().webglcontextlostEvent)
        || hasEventListeners(eventNames().webglcontextrestoredEvent);
#endif
}

void HTMLCanvasElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    oldDocument.clearCanvasPreparation(this);
    removeObserver(oldDocument);
    addObserver(newDocument);

    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
}

Node::InsertedIntoAncestorResult HTMLCanvasElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (insertionType.connectedToDocument)
        addObserver(parentOfInsertedTree.document());

    return HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

void HTMLCanvasElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.disconnectedFromDocument) {
        oldParentOfRemovedTree.document().clearCanvasPreparation(this);
        removeObserver(oldParentOfRemovedTree.document());
    }

    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

bool HTMLCanvasElement::needsPreparationForDisplay()
{
#if ENABLE(WEBGL)
    return is<WebGLRenderingContextBase>(m_context.get());
#else
    return false;
#endif
}

void HTMLCanvasElement::prepareForDisplay()
{
#if ENABLE(WEBGL)
    ASSERT(needsPreparationForDisplay());

    if (is<WebGLRenderingContextBase>(m_context.get()))
        downcast<WebGLRenderingContextBase>(m_context.get())->prepareForDisplay();
#endif
}

}
