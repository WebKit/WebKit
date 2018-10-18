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
#include "Document.h"
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
#include <JavaScriptCore/JSLock.h>
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

#if ENABLE(WEBMETAL)
#include "WebMetalRenderingContext.h"
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

#if USE(CG)
// FIXME: It seems strange that the default quality is not the one that is literally named "default".
// Should fix names to make this easier to understand, or write an excellent comment here explaining why not.
const InterpolationQuality defaultInterpolationQuality = InterpolationLow;
#else
const InterpolationQuality defaultInterpolationQuality = InterpolationDefault;
#endif

static size_t activePixelMemory = 0;

HTMLCanvasElement::HTMLCanvasElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , CanvasBase(&document)
    , m_size(defaultWidth, defaultHeight)
{
    ASSERT(hasTagName(canvasTag));
}

Ref<HTMLCanvasElement> HTMLCanvasElement::create(Document& document)
{
    return adoptRef(*new HTMLCanvasElement(canvasTag, document));
}

Ref<HTMLCanvasElement> HTMLCanvasElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLCanvasElement(tagName, document));
}

static void removeFromActivePixelMemory(size_t pixelsReleased)
{
    if (!pixelsReleased)
        return;

    if (pixelsReleased < activePixelMemory)
        activePixelMemory -= pixelsReleased;
    else
        activePixelMemory = 0;
}
    
HTMLCanvasElement::~HTMLCanvasElement()
{
    notifyObserversCanvasDestroyed();

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.

    releaseImageBufferAndContext();
}

void HTMLCanvasElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
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
    setAttributeWithoutSynchronization(heightAttr, AtomicString::number(limitToOnlyHTMLNonNegative(value, defaultHeight)));
    return { };
}

ExceptionOr<void> HTMLCanvasElement::setWidth(unsigned value)
{
    if (m_context && m_context->isPlaceholder())
        return Exception { InvalidStateError };
    setAttributeWithoutSynchronization(widthAttr, AtomicString::number(limitToOnlyHTMLNonNegative(value, defaultWidth)));
    return { };
}

static inline size_t maxActivePixelMemory()
{
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

ExceptionOr<std::optional<RenderingContext>> HTMLCanvasElement::getContext(JSC::ExecState& state, const String& contextId, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_context) {
        if (m_context->isPlaceholder())
            return Exception { InvalidStateError };

        if (m_context->is2d()) {
            if (!is2dType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            return std::optional<RenderingContext> { RefPtr<CanvasRenderingContext2D> { &downcast<CanvasRenderingContext2D>(*m_context) } };
        }

        if (m_context->isBitmapRenderer()) {
            if (!isBitmapRendererType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            return std::optional<RenderingContext> { RefPtr<ImageBitmapRenderingContext> { &downcast<ImageBitmapRenderingContext>(*m_context) } };
        }

#if ENABLE(WEBGL)
        if (m_context->isWebGL()) {
            if (!isWebGLType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            if (is<WebGLRenderingContext>(*m_context))
                return std::optional<RenderingContext> { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } };
#if ENABLE(WEBGL2)
            ASSERT(is<WebGL2RenderingContext>(*m_context));
            return std::optional<RenderingContext> { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*m_context) } };
#endif
        }
#endif

#if ENABLE(WEBMETAL)
        if (m_context->isWebMetal()) {
            if (!isWebMetalType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            return std::optional<RenderingContext> { RefPtr<WebMetalRenderingContext> { &downcast<WebMetalRenderingContext>(*m_context) } };
        }
#endif

        ASSERT_NOT_REACHED();
        return std::optional<RenderingContext> { std::nullopt };
    }

    if (is2dType(contextId)) {
        auto context = createContext2d(contextId);
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };
        return std::optional<RenderingContext> { RefPtr<CanvasRenderingContext2D> { context } };
    }

    if (isBitmapRendererType(contextId)) {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<ImageBitmapRenderingContextSettings>>(state, !arguments.isEmpty() ? arguments[0].get() : JSC::jsUndefined());
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        auto context = createContextBitmapRenderer(contextId, WTFMove(attributes));
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };
        return std::optional<RenderingContext> { RefPtr<ImageBitmapRenderingContext> { context } };
    }

#if ENABLE(WEBGL)
    if (isWebGLType(contextId)) {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, !arguments.isEmpty() ? arguments[0].get() : JSC::jsUndefined());
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        auto context = createContextWebGL(contextId, WTFMove(attributes));
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };

        if (is<WebGLRenderingContext>(*context))
            return std::optional<RenderingContext> { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*context) } };
#if ENABLE(WEBGL2)
        ASSERT(is<WebGL2RenderingContext>(*context));
        return std::optional<RenderingContext> { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*context) } };
#endif
    }
#endif

#if ENABLE(WEBMETAL)
    if (isWebMetalType(contextId)) {
        auto context = createContextWebMetal(contextId);
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };
        return std::optional<RenderingContext> { RefPtr<WebMetalRenderingContext> { context } };
    }
#endif

    return std::optional<RenderingContext> { std::nullopt };
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type)
{
    if (HTMLCanvasElement::is2dType(type))
        return getContext2d(type);

    if (HTMLCanvasElement::isBitmapRendererType(type))
        return getContextBitmapRenderer(type);

#if ENABLE(WEBMETAL)
    if (HTMLCanvasElement::isWebMetalType(type) && RuntimeEnabledFeatures::sharedFeatures().webMetalEnabled())
        return getContextWebMetal(type);
#endif

#if ENABLE(WEBGL)
    if (HTMLCanvasElement::isWebGLType(type))
        return getContextWebGL(type);
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

    bool usesDashboardCompatibilityMode = false;
#if ENABLE(DASHBOARD_SUPPORT)
    usesDashboardCompatibilityMode = document().settings().usesDashboardBackwardCompatibilityMode();
#endif

    // Make sure we don't use more pixel memory than the system can support.
    size_t requestedPixelMemory = 4 * width() * height();
    if (activePixelMemory + requestedPixelMemory > maxActivePixelMemory()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Total canvas memory use exceeds the maximum limit (");
        stringBuilder.appendNumber(maxActivePixelMemory() / 1024 / 1024);
        stringBuilder.appendLiteral(" MB).");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return nullptr;
    }

    m_context = CanvasRenderingContext2D::create(*this, document().inQuirksMode(), usesDashboardCompatibilityMode);

    downcast<CanvasRenderingContext2D>(*m_context).setUsesDisplayListDrawing(m_usesDisplayListDrawing);
    downcast<CanvasRenderingContext2D>(*m_context).setTracksDisplayListReplay(m_tracksDisplayListReplay);

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

    m_context = WebGLRenderingContextBase::create(*this, attrs, type);
    if (m_context) {
        // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
        invalidateStyleAndLayerComposition();
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

#if ENABLE(WEBMETAL)

bool HTMLCanvasElement::isWebMetalType(const String& type)
{
    return type == "webmetal";
}

WebMetalRenderingContext* HTMLCanvasElement::createContextWebMetal(const String& type)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isWebMetalType(type));
    ASSERT(!m_context);

    if (!RuntimeEnabledFeatures::sharedFeatures().webMetalEnabled())
        return nullptr;

    m_context = WebMetalRenderingContext::create(*this);
    if (m_context) {
        // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
        invalidateStyleAndLayerComposition();
    }

    return static_cast<WebMetalRenderingContext*>(m_context.get());
}

WebMetalRenderingContext* HTMLCanvasElement::getContextWebMetal(const String& type)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isWebMetalType(type));

    if (!RuntimeEnabledFeatures::sharedFeatures().webMetalEnabled())
        return nullptr;

    if (m_context && !m_context->isWebMetal())
        return nullptr;

    if (!m_context)
        return createContextWebMetal(type);
    return static_cast<WebMetalRenderingContext*>(m_context.get());
}
#endif

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

    if (m_contextStateSaver) {
        // Reset to the initial graphics context state.
        m_contextStateSaver->restore();
        m_contextStateSaver->save();
    }

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
    if (UNLIKELY(m_context && m_context->callTracingActive()))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*m_context);

    // Clear the dirty rect
    m_dirtyRect = FloatRect();

    if (context.paintingDisabled())
        return;
    
    if (m_context) {
        if (!paintsIntoCanvasBuffer() && !document().printing())
            return;

        m_context->paintRenderingResultsToCanvas();
    }

    if (hasCreatedImageBuffer()) {
        ImageBuffer* imageBuffer = buffer();
        if (imageBuffer) {
            if (m_presentedImage) {
                ImageOrientationDescription orientationDescription;
#if ENABLE(CSS_IMAGE_ORIENTATION)
                orientationDescription.setImageOrientationEnum(renderer()->style().imageOrientation());
#endif 
                context.drawImage(*m_presentedImage, snappedIntRect(r), ImagePaintingOptions(orientationDescription));
            } else
                context.drawImageBuffer(*imageBuffer, snappedIntRect(r));
        }
    }

    if (isGPUBased())
        downcast<GPUBasedCanvasRenderingContext>(*m_context).markLayerComposited();
}

bool HTMLCanvasElement::isGPUBased() const
{
    return m_context && m_context->isGPUBased();
}

void HTMLCanvasElement::makeRenderingResultsAvailable()
{
    if (m_context)
        m_context->paintRenderingResultsToCanvas();
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

void HTMLCanvasElement::releaseImageBufferAndContext()
{
    m_contextStateSaver = nullptr;
    setImageBuffer(nullptr);
}
    
void HTMLCanvasElement::setSurfaceSize(const IntSize& size)
{
    m_size = size;
    m_hasCreatedImageBuffer = false;
    releaseImageBufferAndContext();
    clearCopiedImage();
}

static String toEncodingMimeType(const String& mimeType)
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return "image/png"_s;
    return mimeType.convertToASCIILowercase();
}

// https://html.spec.whatwg.org/multipage/canvas.html#a-serialisation-of-the-bitmap-as-a-file
static std::optional<double> qualityFromJSValue(JSC::JSValue qualityValue)
{
    if (!qualityValue.isNumber())
        return std::nullopt;

    double qualityNumber = qualityValue.asNumber();
    if (qualityNumber < 0 || qualityNumber > 1)
        return std::nullopt;

    return qualityNumber;
}

ExceptionOr<UncachedString> HTMLCanvasElement::toDataURL(const String& mimeType, JSC::JSValue qualityValue)
{
    if (!originClean())
        return Exception { SecurityError };

    if (m_size.isEmpty() || !buffer())
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

    if (m_size.isEmpty() || !buffer()) {
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
#else
    return nullptr;
#endif
}

ExceptionOr<Ref<MediaStream>> HTMLCanvasElement::captureStream(ScriptExecutionContext& context, std::optional<double>&& frameRequestRate)
{
    if (!originClean())
        return Exception(SecurityError, "Canvas is tainted"_s);
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    if (frameRequestRate && frameRequestRate.value() < 0)
        return Exception(NotSupportedError, "frameRequestRate is negative"_s);

    auto track = CanvasCaptureMediaStreamTrack::create(context, *this, WTFMove(frameRequestRate));
    auto stream =  MediaStream::create(context);
    stream->addTrack(track);
    return WTFMove(stream);
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

size_t HTMLCanvasElement::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful
    // about what data we access here and how. We need to hold a lock to prevent m_imageBuffer
    // from being changed while we access it.
    auto locker = holdLock(m_imageBufferAssignmentLock);
    if (!m_imageBuffer)
        return 0;
    return m_imageBuffer->memoryCost();
}

size_t HTMLCanvasElement::externalMemoryCost() const
{
    // externalMemoryCost() may be invoked concurrently from a GC thread, and we need to be careful
    // about what data we access here and how. We need to hold a lock to prevent m_imageBuffer
    // from being changed while we access it.
    auto locker = holdLock(m_imageBufferAssignmentLock);
    if (!m_imageBuffer)
        return 0;
    return m_imageBuffer->externalMemoryCost();
}

void HTMLCanvasElement::setUsesDisplayListDrawing(bool usesDisplayListDrawing)
{
    if (usesDisplayListDrawing == m_usesDisplayListDrawing)
        return;
    
    m_usesDisplayListDrawing = usesDisplayListDrawing;

    if (is<CanvasRenderingContext2D>(m_context.get()))
        downcast<CanvasRenderingContext2D>(*m_context).setUsesDisplayListDrawing(m_usesDisplayListDrawing);
}

void HTMLCanvasElement::setTracksDisplayListReplay(bool tracksDisplayListReplay)
{
    if (tracksDisplayListReplay == m_tracksDisplayListReplay)
        return;

    m_tracksDisplayListReplay = tracksDisplayListReplay;

    if (is<CanvasRenderingContext2D>(m_context.get()))
        downcast<CanvasRenderingContext2D>(*m_context).setTracksDisplayListReplay(m_tracksDisplayListReplay);
}

String HTMLCanvasElement::displayListAsText(DisplayList::AsTextFlags flags) const
{
    if (is<CanvasRenderingContext2D>(m_context.get()))
        return downcast<CanvasRenderingContext2D>(*m_context).displayListAsText(flags);

    return String();
}

String HTMLCanvasElement::replayDisplayListAsText(DisplayList::AsTextFlags flags) const
{
    if (is<CanvasRenderingContext2D>(m_context.get()))
        return downcast<CanvasRenderingContext2D>(*m_context).replayDisplayListAsText(flags);

    return String();
}

void HTMLCanvasElement::createImageBuffer() const
{
    ASSERT(!m_imageBuffer);

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
    if (activePixelMemory + requestedPixelMemory > maxActivePixelMemory()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Total canvas memory use exceeds the maximum limit (");
        stringBuilder.appendNumber(maxActivePixelMemory() / 1024 / 1024);
        stringBuilder.appendLiteral(" MB).");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return;
    }

    if (!width() || !height())
        return;

    RenderingMode renderingMode = shouldAccelerate(size()) ? Accelerated : Unaccelerated;

    auto hostWindow = (document().view() && document().view()->root()) ? document().view()->root()->hostWindow() : nullptr;
    setImageBuffer(ImageBuffer::create(size(), renderingMode, 1, ColorSpaceSRGB, hostWindow));
    if (!m_imageBuffer)
        return;
    m_imageBuffer->context().setShadowsIgnoreTransforms(true);
    m_imageBuffer->context().setImageInterpolationQuality(defaultInterpolationQuality);
    m_imageBuffer->context().setStrokeThickness(1);
    m_contextStateSaver = std::make_unique<GraphicsContextStateSaver>(m_imageBuffer->context());

    JSC::JSLockHolder lock(HTMLElement::scriptExecutionContext()->vm());
    HTMLElement::scriptExecutionContext()->vm().heap.reportExtraMemoryAllocated(memoryCost());

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context && m_context->is2d()) {
        // Recalculate compositing requirements if acceleration state changed.
        const_cast<HTMLCanvasElement*>(this)->invalidateStyleAndLayerComposition();
    }
#endif
}

void HTMLCanvasElement::setImageBuffer(std::unique_ptr<ImageBuffer>&& buffer) const
{
    size_t previousMemoryCost = memoryCost();
    removeFromActivePixelMemory(previousMemoryCost);

    {
        auto locker = holdLock(m_imageBufferAssignmentLock);
        m_imageBuffer = WTFMove(buffer);
    }

    if (m_imageBuffer && m_size != m_imageBuffer->internalSize())
        m_size = m_imageBuffer->internalSize();

    size_t currentMemoryCost = memoryCost();
    activePixelMemory += currentMemoryCost;

    if (m_context && m_imageBuffer && previousMemoryCost != currentMemoryCost)
        InspectorInstrumentation::didChangeCanvasMemory(*m_context);
}

void HTMLCanvasElement::setImageBufferAndMarkDirty(std::unique_ptr<ImageBuffer>&& buffer)
{
    m_hasCreatedImageBuffer = true;
    setImageBuffer(WTFMove(buffer));
    didDraw(FloatRect(FloatPoint(), m_size));
}

GraphicsContext* HTMLCanvasElement::drawingContext() const
{
    if (m_context && !m_context->is2d())
        return nullptr;

    return buffer() ? &m_imageBuffer->context() : nullptr;
}

GraphicsContext* HTMLCanvasElement::existingDrawingContext() const
{
    if (!m_hasCreatedImageBuffer)
        return nullptr;

    return drawingContext();
}

ImageBuffer* HTMLCanvasElement::buffer() const
{
    if (!m_hasCreatedImageBuffer)
        createImageBuffer();
    return m_imageBuffer.get();
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

AffineTransform HTMLCanvasElement::baseTransform() const
{
    ASSERT(m_hasCreatedImageBuffer);
    return m_imageBuffer->baseTransform();
}

}
