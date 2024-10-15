/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include "BitmapImage.h"
#include "Blob.h"
#include "BlobCallback.h"
#include "CSSParserContext.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasRenderingContext2DSettings.h"
#include "DisplayListDrawingContext.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "GPU.h"
#include "GPUBasedCanvasRenderingContext.h"
#include "GPUCanvasContext.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HostWindow.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageBitmapRenderingContextSettings.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "JSDOMConvertDictionary.h"
#include "JSNodeCustomInlines.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "Navigator.h"
#include "OffscreenCanvas.h"
#include "PlaceholderRenderingContext.h"
#include "Quirks.h"
#include "RenderBoxInlines.h"
#include "RenderElement.h"
#include "RenderHTMLCanvas.h"
#include "ResourceLoadObserver.h"
#include "ScriptController.h"
#include "ScriptTelemetryCategory.h"
#include "Settings.h"
#include "StringAdaptors.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/JSCInlines.h>
#include <math.h>
#include <wtf/RAMSize.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(MEDIA_STREAM)
#include "CanvasCaptureMediaStreamTrack.h"
#include "MediaStream.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLContextAttributes.h"
#include "WebGLRenderingContext.h"
#include "WebGL2RenderingContext.h"
#endif

#if ENABLE(WEBXR)
#include "LocalDOMWindow.h"
#include "Navigator.h"
#include "NavigatorWebXR.h"
#include "WebXRSystem.h"
#endif

#if USE(CG)
#include "ImageBufferUtilitiesCG.h"
#endif

#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif

#if PLATFORM(COCOA)
#include "GPUAvailability.h"
#include "VideoFrameCV.h"
#include <pal/cf/CoreMediaSoftLink.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLCanvasElement);

using namespace HTMLNames;

// These values come from the WhatWG/W3C HTML spec.
const int defaultWidth = 300;
const int defaultHeight = 150;

HTMLCanvasElement::HTMLCanvasElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasDidMoveToNewDocument)
    , CanvasBase(IntSize(defaultWidth, defaultHeight), document)
    , ActiveDOMObject(document)
{
    ASSERT(hasTagName(canvasTag));
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
    // FIXME: This has to be called here because StyleCanvasImage::canvasDestroyed()
    // downcasts the CanvasBase object to HTMLCanvasElement. That invokes virtual methods, which should be
    // avoided in destructors, but works as long as it's done before HTMLCanvasElement destructs completely.
    notifyObserversCanvasDestroyed();
    removeCanvasNeedingPreparationForDisplayOrFlush();

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.
    setImageBuffer(nullptr);
}

bool HTMLCanvasElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLCanvasElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        applyAspectRatioWithoutDimensionalRulesFromWidthAndHeightAttributesToStyle(value, attributeWithoutSynchronization(heightAttr), style);
    else if (name == heightAttr)
        applyAspectRatioWithoutDimensionalRulesFromWidthAndHeightAttributesToStyle(attributeWithoutSynchronization(widthAttr), value, style);
    else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLCanvasElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == widthAttr || name == heightAttr)
        reset();
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

RenderPtr<RenderElement> HTMLCanvasElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    RefPtr frame { document().frame() };
    if (frame && frame->script().canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
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
    if (isControlledByOffscreen())
        return Exception { ExceptionCode::InvalidStateError };
    setAttributeWithoutSynchronization(heightAttr, AtomString::number(limitToOnlyHTMLNonNegative(value, defaultHeight)));
    return { };
}

ExceptionOr<void> HTMLCanvasElement::setWidth(unsigned value)
{
    if (isControlledByOffscreen())
        return Exception { ExceptionCode::InvalidStateError };
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

ExceptionOr<std::optional<RenderingContext>> HTMLCanvasElement::getContext(JSC::JSGlobalObject& state, const String& contextId, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_context) {
        if (m_context->isPlaceholder())
            return Exception { ExceptionCode::InvalidStateError };

        if (RefPtr context = dynamicDowncast<CanvasRenderingContext2D>(*m_context)) {
            if (!is2dType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            return std::optional<RenderingContext> { WTFMove(context) };
        }

        if (RefPtr context = dynamicDowncast<ImageBitmapRenderingContext>(*m_context)) {
            if (!isBitmapRendererType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            return std::optional<RenderingContext> { WTFMove(context) };
        }

#if ENABLE(WEBGL)
        if (m_context->isWebGL()) {
            if (!isWebGLType(contextId))
                return std::optional<RenderingContext> { std::nullopt };
            auto version = toWebGLVersion(contextId);
            if ((version == WebGLVersion::WebGL1) != m_context->isWebGL1())
                return std::optional<RenderingContext> { std::nullopt };
            if (RefPtr context = dynamicDowncast<WebGLRenderingContext>(*m_context))
                return std::optional<RenderingContext> { WTFMove(context) };
            return std::optional<RenderingContext> { RefPtr { &downcast<WebGL2RenderingContext>(*m_context) } };
        }
#endif

        if (RefPtr context = dynamicDowncast<GPUCanvasContext>(m_context.get())) {
            if (!isWebGPUType(contextId))
                return { std::nullopt };
            return { context };
        }

        ASSERT_NOT_REACHED();
        return std::optional<RenderingContext> { std::nullopt };
    }

    if (is2dType(contextId)) {
        Ref vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto settings = convert<IDLDictionary<CanvasRenderingContext2DSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
        if (UNLIKELY(settings.hasException(scope)))
            return Exception { ExceptionCode::ExistingExceptionError };

        RefPtr context = createContext2d(contextId, settings.releaseReturnValue());
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };
        return std::optional<RenderingContext> { WTFMove(context) };
    }

    if (isBitmapRendererType(contextId)) {
        Ref vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto settings = convert<IDLDictionary<ImageBitmapRenderingContextSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
        if (UNLIKELY(settings.hasException(scope)))
            return Exception { ExceptionCode::ExistingExceptionError };

        RefPtr context = createContextBitmapRenderer(contextId, settings.releaseReturnValue());
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };
        return std::optional<RenderingContext> { WTFMove(context) };
    }

#if ENABLE(WEBGL)
    if (isWebGLType(contextId)) {
        Ref vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
        if (UNLIKELY(attributes.hasException(scope)))
            return Exception { ExceptionCode::ExistingExceptionError };

        RefPtr context = createContextWebGL(toWebGLVersion(contextId), attributes.releaseReturnValue());
        if (!context)
            return std::optional<RenderingContext> { std::nullopt };

        if (auto* webGLContext = dynamicDowncast<WebGLRenderingContext>(*context))
            return std::optional<RenderingContext> { RefPtr { webGLContext } };

        return std::optional<RenderingContext> { downcast<WebGL2RenderingContext>(WTFMove(context)) };
    }
#endif

    if (isWebGPUType(contextId)) {
        GPU* gpu = nullptr;
        if (RefPtr window = document().domWindow()) {
            // FIXME: Should we be instead getting this through jsDynamicCast<JSDOMWindow*>(state)->wrapped().navigator().gpu()?
            gpu = window->navigator().gpu();
        }
        auto context = createContextWebGPU(contextId, gpu);
        if (!context)
            return { std::nullopt };
        return { context };
    }

    return std::optional<RenderingContext> { std::nullopt };
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type)
{
    if (HTMLCanvasElement::is2dType(type))
        return getContext2d(type, { });

    if (HTMLCanvasElement::isBitmapRendererType(type))
        return getContextBitmapRenderer(type, { });

#if ENABLE(WEBGL)
    if (HTMLCanvasElement::isWebGLType(type))
        return getContextWebGL(HTMLCanvasElement::toWebGLVersion(type));
#endif

    if (HTMLCanvasElement::isWebGPUType(type))
        return getContextWebGPU(type, nullptr);

    return nullptr;
}

bool HTMLCanvasElement::is2dType(const String& type)
{
    return type == "2d"_s;
}

CanvasRenderingContext2D* HTMLCanvasElement::createContext2d(const String& type, CanvasRenderingContext2DSettings&& settings)
{
    ASSERT_UNUSED(HTMLCanvasElement::is2dType(type), type);
    ASSERT(!m_context);

    m_context = CanvasRenderingContext2D::create(*this, WTFMove(settings), document().inQuirksMode());

#if USE(CA) || USE(SKIA)
    // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
    invalidateStyleAndLayerComposition();
#endif

    return downcast<CanvasRenderingContext2D>(m_context.get());
}

CanvasRenderingContext2D* HTMLCanvasElement::getContext2d(const String& type, CanvasRenderingContext2DSettings&& settings)
{
    ASSERT_UNUSED(HTMLCanvasElement::is2dType(type), type);

    if (m_context && !m_context->is2d())
        return nullptr;

    if (!m_context)
        return createContext2d(type, WTFMove(settings));
    return downcast<CanvasRenderingContext2D>(m_context.get());
}

#if ENABLE(WEBGL)

static bool requiresAcceleratedCompositingForWebGL()
{
#if PLATFORM(GTK) || PLATFORM(WIN)
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
    return type == "webgl"_s || type == "experimental-webgl"_s
        || type == "webgl2"_s
        || type == "webkit-3d"_s;
}

WebGLVersion HTMLCanvasElement::toWebGLVersion(const String& type)
{
    ASSERT(isWebGLType(type));
    if (type == "webgl2"_s)
        return WebGLVersion::WebGL2;
    return WebGLVersion::WebGL1;
}

WebGLRenderingContextBase* HTMLCanvasElement::createContextWebGL(WebGLVersion type, WebGLContextAttributes&& attrs)
{
    ASSERT(!m_context);

    if (!shouldEnableWebGL(document().settings()))
        return nullptr;

#if HAVE(GPU_AVAILABILITY_CHECK)
    if (!document().settings().useGPUProcessForWebGLEnabled() && !isGPUAvailable()) {
        RELEASE_LOG_FAULT(WebGL, "GPU is not available.");
        return nullptr;
    }
#endif

#if ENABLE(WEBXR)
    // https://immersive-web.github.io/webxr/#xr-compatible
    if (attrs.xrCompatible) {
        if (RefPtr window = document().domWindow()) {
            // FIXME: how to make this sync without blocking the main thread?
            // For reference: https://immersive-web.github.io/webxr/#ref-for-dom-webglcontextattributes-xrcompatible
            NavigatorWebXR::xr(window->navigator()).ensureImmersiveXRDeviceIsSelected([]() { });
        }
    }
#endif

    // TODO(WEBXR): ensure the context is created in a compatible graphics
    // adapter when there is an active immersive device.
    auto context = WebGLRenderingContextBase::create(*this, attrs, type);
    WeakPtr weakContext = context.get();
    m_context = WTFMove(context);
    if (weakContext) {
        // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
        invalidateStyleAndLayerComposition();
        if (CheckedPtr box = renderBox())
            box->contentChanged(ContentChangeType::Canvas);
#if ENABLE(WEBXR)
        ASSERT(!attrs.xrCompatible || weakContext->isXRCompatible());
#endif
    }

    return weakContext.get();
}

WebGLRenderingContextBase* HTMLCanvasElement::getContextWebGL(WebGLVersion type, WebGLContextAttributes&& attrs)
{
    if (!shouldEnableWebGL(document().settings()))
        return nullptr;

    if (m_context) {
        auto* glContext = dynamicDowncast<WebGLRenderingContextBase>(*m_context);
        if (!glContext)
            return nullptr;

        if ((type == WebGLVersion::WebGL1) != glContext->isWebGL1())
            return nullptr;

        return glContext;
    }

    return createContextWebGL(type, WTFMove(attrs));
}

#endif // ENABLE(WEBGL)

bool HTMLCanvasElement::isBitmapRendererType(const String& type)
{
    return type == "bitmaprenderer"_s;
}

ImageBitmapRenderingContext* HTMLCanvasElement::createContextBitmapRenderer(const String& type, ImageBitmapRenderingContextSettings&& settings)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isBitmapRendererType(type));
    ASSERT(!m_context);

    auto context = ImageBitmapRenderingContext::create(*this, WTFMove(settings));
    WeakPtr weakContext = *context;
    m_context = WTFMove(context);
    weakContext->transferFromImageBitmap(nullptr);

#if USE(CA) || USE(SKIA)
    // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
    invalidateStyleAndLayerComposition();
#endif

    return weakContext.get();
}

ImageBitmapRenderingContext* HTMLCanvasElement::getContextBitmapRenderer(const String& type, ImageBitmapRenderingContextSettings&& settings)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isBitmapRendererType(type));
    if (!m_context)
        return createContextBitmapRenderer(type, WTFMove(settings));
    return downcast<ImageBitmapRenderingContext>(m_context.get());
}

bool HTMLCanvasElement::isWebGPUType(const String& type)
{
    return type == "webgpu"_s;
}

GPUCanvasContext* HTMLCanvasElement::createContextWebGPU(const String& type, GPU* gpu)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isWebGPUType(type));
    ASSERT(!m_context);

    if (!document().settings().webGPUEnabled() || !gpu)
        return nullptr;

    m_context = GPUCanvasContext::create(*this, *gpu);

    if (m_context) {
        // Need to make sure a RenderLayer and compositing layer get created for the Canvas.
        invalidateStyleAndLayerComposition();
    }

    return downcast<GPUCanvasContext>(m_context.get());
}

GPUCanvasContext* HTMLCanvasElement::getContextWebGPU(const String& type, GPU* gpu)
{
    ASSERT_UNUSED(type, HTMLCanvasElement::isWebGPUType(type));

    if (!document().settings().webGPUEnabled())
        return nullptr;

    if (m_context && !m_context->isWebGPU())
        return nullptr;

    if (!m_context)
        return createContextWebGPU(type, gpu);

    return downcast<GPUCanvasContext>(m_context.get());
}

void HTMLCanvasElement::didDraw(const std::optional<FloatRect>& rect, ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    clearCopiedImage();
    if (CheckedPtr renderer = renderBox()) {
        if (usesContentsAsLayerContents())
            renderer->contentChanged(ContentChangeType::CanvasPixels);
        else if (rect) {
            FloatRect destRect;
            if (CheckedPtr renderReplaced = dynamicDowncast<RenderReplaced>(*renderer))
                destRect = renderReplaced->replacedContentRect();
            else
                destRect = renderer->contentBoxRect();

            FloatRect r = mapRect(*rect, FloatRect { { }, size() }, destRect);
            r.intersect(destRect);

            if (!r.isEmpty())
                renderer->repaintRectangle(enclosingIntRect(r));
        }
    }
    CanvasBase::didDraw(rect, shouldApplyPostProcessingToDirtyRect);
}

void HTMLCanvasElement::reset()
{
    if (m_ignoreReset || isControlledByOffscreen())
        return;

    bool hadImageBuffer = hasCreatedImageBuffer();

    int w = limitToOnlyHTMLNonNegative(attributeWithoutSynchronization(widthAttr), defaultWidth);
    int h = limitToOnlyHTMLNonNegative(attributeWithoutSynchronization(heightAttr), defaultHeight);

    if (RefPtr context = dynamicDowncast<CanvasRenderingContext2D>(m_context.get()))
        context->reset();
    else
        resetGraphicsContextState();

    IntSize oldSize = size();
    IntSize newSize(w, h);
    // If the size of an existing buffer matches, we can just clear it instead of reallocating.
    // This optimization is only done for 2D canvases for now.
    if (hasCreatedImageBuffer() && oldSize == newSize && m_context && m_context->is2d() && buffer() && m_context->colorSpace() == buffer()->colorSpace() && m_context->pixelFormat() == buffer()->pixelFormat()) {
        if (!m_didClearImageBuffer)
            clearImageBuffer();
        return;
    }

    setSurfaceSize(newSize);

    if (m_context) {
        if (RefPtr context = dynamicDowncast<GPUBasedCanvasRenderingContext>(*m_context))
            context->reshape();
    }

    if (CheckedPtr canvasRenderer = dynamicDowncast<RenderHTMLCanvas>(renderer())) {
        if (oldSize != size()) {
            canvasRenderer->canvasSizeChanged();
            if (canvasRenderer->hasAcceleratedCompositing())
                canvasRenderer->contentChanged(ContentChangeType::Canvas);
        }
        if (hadImageBuffer)
            canvasRenderer->repaint();
    }

    notifyObserversCanvasResized();
}

bool HTMLCanvasElement::usesContentsAsLayerContents() const
{
    auto* renderBox = this->renderBox();
    if (!renderBox)
        return false;
    if (!m_context)
        return false;
    return renderBox->hasAcceleratedCompositing() && m_context->delegatesDisplay();
}

void HTMLCanvasElement::paint(GraphicsContext& context, const LayoutRect& r)
{
    if (!m_context)
        return;
    m_context->clearAccumulatedDirtyRect();

    if (!context.paintingDisabled()) {
        if (!usesContentsAsLayerContents() || document().printing() || m_isSnapshotting) {
            if (m_context->compositingResultsNeedUpdating())
                m_context->prepareForDisplay();
            const bool skipTransparentBlackDraw = context.compositeMode() == CompositeMode { CompositeOperator::SourceOver, BlendMode::Normal };
            if (!skipTransparentBlackDraw || !m_context->isSurfaceBufferTransparentBlack(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer)) {
                RefPtr buffer = m_context->surfaceBufferToImageBuffer(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer);
                if (buffer)
                    context.drawImageBuffer(*buffer, snappedIntRect(r), { context.compositeOperation() });
            }
        }
    }

    if (UNLIKELY(m_context->hasActiveInspectorCanvasCallTracer()))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*m_context);
}

void HTMLCanvasElement::setSurfaceSize(const IntSize& size)
{
    CanvasBase::setSize(size);
    setHasCreatedImageBuffer(false);
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
        return Exception { ExceptionCode::SecurityError };

    if (size().isEmpty())
        return UncachedString { "data:,"_s };
    if (document().settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    auto encodingMIMEType = toEncodingMimeType(mimeType);
    auto quality = qualityFromJSValue(qualityValue);

    if (document().requiresScriptExecutionTelemetry(ScriptTelemetryCategory::Canvas)) {
        if (RefPtr buffer = createImageForNoiseInjection())
            return UncachedString { buffer->toDataURL(encodingMIMEType, quality) };

        return UncachedString { "data:,"_s };
    }

#if USE(CG)
    // Try to get ImageData first, as that may avoid lossy conversions.
    if (auto imageData = getImageData())
        return UncachedString { dataURL(imageData->pixelBuffer(), encodingMIMEType, quality) };
#endif

    if (auto url = document().quirks().advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(lastFillText(), width(), height()); !url.isNull()) {
        RELEASE_LOG(FingerprintingMitigation, "HTMLCanvasElement::toDataURL: Quirking returned URL for identified fingerprinting script");
        auto consoleMessage = "Detected fingerprinting script. Quirking value returned from HTMLCanvasElement.toDataURL()"_s;
        canvasBaseScriptExecutionContext()->addConsoleMessage(MessageSource::Rendering, MessageLevel::Info, consoleMessage);
        return UncachedString { url };
    }
    RefPtr buffer = makeRenderingResultsAvailable();
    if (!buffer)
        return UncachedString { "data:,"_s };
    return UncachedString { buffer->toDataURL(encodingMIMEType, quality) };
}

ExceptionOr<UncachedString> HTMLCanvasElement::toDataURL(const String& mimeType)
{
    return toDataURL(mimeType, { });
}

ExceptionOr<void> HTMLCanvasElement::toBlob(Ref<BlobCallback>&& callback, const String& mimeType, JSC::JSValue qualityValue)
{
    if (!originClean())
        return Exception { ExceptionCode::SecurityError };

    if (size().isEmpty()) {
        callback->scheduleCallback(document(), nullptr);
        return { };
    }
    if (document().settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    auto encodingMIMEType = toEncodingMimeType(mimeType);
    auto quality = qualityFromJSValue(qualityValue);
    auto scheduleCallbackWithBlobData = [&](Ref<BlobCallback>&& callback, Vector<uint8_t>&& blobData) {
        RefPtr<Blob> blob;
        if (!blobData.isEmpty())
            blob = Blob::create(&document(), WTFMove(blobData), encodingMIMEType);
        callback->scheduleCallback(document(), WTFMove(blob));
    };

    if (document().requiresScriptExecutionTelemetry(ScriptTelemetryCategory::Canvas)) {
        RefPtr buffer = createImageForNoiseInjection();
        scheduleCallbackWithBlobData(WTFMove(callback), buffer ? buffer->toData(encodingMIMEType, quality) : Vector<uint8_t> { });
        return { };
    }

#if USE(CG)
    if (auto imageData = getImageData()) {
        scheduleCallbackWithBlobData(WTFMove(callback), encodeData(imageData->pixelBuffer(), encodingMIMEType, quality));
        return { };
    }
#endif

    RefPtr buffer = makeRenderingResultsAvailable();
    if (!buffer) {
        callback->scheduleCallback(document(), nullptr);
        return { };
    }
    scheduleCallbackWithBlobData(WTFMove(callback), buffer->toData(encodingMIMEType, quality));
    return { };
}

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<Ref<OffscreenCanvas>> HTMLCanvasElement::transferControlToOffscreen()
{
    if (m_context)
        return Exception { ExceptionCode::InvalidStateError };

    std::unique_ptr placeholderContext = PlaceholderRenderingContext::create(*this);
    Ref offscreen = OffscreenCanvas::create(document(), *placeholderContext);
    m_context = WTFMove(placeholderContext);
    if (m_context->delegatesDisplay())
        invalidateStyleAndLayerComposition();
    return offscreen;
}
#endif

RefPtr<ImageData> HTMLCanvasElement::getImageData()
{
#if ENABLE(WEBGL)
    RefPtr context = dynamicDowncast<WebGLRenderingContextBase>(m_context.get());
    if (!context)
        return nullptr;

    if (document().settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    RefPtr pixelBuffer = context->drawingBufferToPixelBuffer();
    if (!pixelBuffer)
        return nullptr;

    postProcessPixelBufferResults(*pixelBuffer);
    return ImageData::create(pixelBuffer.releaseNonNull());
#else
    return nullptr;
#endif
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)

RefPtr<VideoFrame> HTMLCanvasElement::toVideoFrame()
{
#if PLATFORM(COCOA) || USE(GSTREAMER)
#if ENABLE(WEBGL)
    if (RefPtr context = dynamicDowncast<WebGLRenderingContextBase>(m_context.get())) {
        if (document().settings().webAPIStatisticsEnabled())
            ResourceLoadObserver::shared().logCanvasRead(document());
        return context->surfaceBufferToVideoFrame(CanvasRenderingContext::SurfaceBuffer::DrawingBuffer);
    }
#endif

    if (document().settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(document());

    RefPtr imageBuffer = makeRenderingResultsAvailable();
    if (!imageBuffer)
        return nullptr;

    // FIXME: This can likely be optimized quite a bit, especially in the cases where
    // the ImageBuffer is backed by GPU memory already and/or is in the GPU process by
    // specializing toVideoFrame() in ImageBufferBackend to not use getPixelBuffer().
    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { { }, imageBuffer->truncatedLogicalSize() });
    if (!pixelBuffer)
        return nullptr;

    // FIXME: Set color space.
#if PLATFORM(COCOA)
    return VideoFrame::createFromPixelBuffer(pixelBuffer.releaseNonNull());
#elif USE(GSTREAMER)
    return VideoFrameGStreamer::createFromPixelBuffer(pixelBuffer.releaseNonNull(), VideoFrameGStreamer::CanvasContentType::Canvas2D);
#endif
#else
    return nullptr;
#endif
}

#endif // ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)

#if ENABLE(MEDIA_STREAM)

ExceptionOr<Ref<MediaStream>> HTMLCanvasElement::captureStream(std::optional<double>&& frameRequestRate)
{
    if (!originClean())
        return Exception(ExceptionCode::SecurityError, "Canvas is tainted"_s);
    if (document().settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasRead(this->document());

    if (frameRequestRate && frameRequestRate.value() < 0)
        return Exception(ExceptionCode::NotSupportedError, "frameRequestRate is negative"_s);

    auto track = CanvasCaptureMediaStreamTrack::create(document(), *this, WTFMove(frameRequestRate));
    auto stream = MediaStream::create(document());
    stream->addTrack(track);
    return stream;
}
#endif

SecurityOrigin* HTMLCanvasElement::securityOrigin() const
{
    return &document().securityOrigin();
}

void HTMLCanvasElement::createImageBuffer() const
{
    ASSERT(!hasCreatedImageBuffer());

    const_cast<HTMLCanvasElement*>(this)->setHasCreatedImageBuffer(true);
    m_didClearImageBuffer = true;
    setImageBuffer(allocateImageBuffer());

#if USE(CA) || USE(SKIA)
    if (m_context && m_context->is2d()) {
        // Recalculate compositing requirements if acceleration state changed.
        const_cast<HTMLCanvasElement*>(this)->invalidateStyleAndLayerComposition();
    }
#endif
}

void HTMLCanvasElement::setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&& buffer)
{
    IntSize oldSize = size();
    setHasCreatedImageBuffer(true);
    setImageBuffer(WTFMove(buffer));

    if (isControlledByOffscreen() && oldSize != size()) {
        setAttributeWithoutSynchronization(widthAttr, AtomString::number(width()));
        setAttributeWithoutSynchronization(heightAttr, AtomString::number(height()));

        if (CheckedPtr canvasRenderer = dynamicDowncast<RenderHTMLCanvas>(renderer())) {
            canvasRenderer->canvasSizeChanged();
            canvasRenderer->contentChanged(ContentChangeType::Canvas);
        }

        notifyObserversCanvasResized();
    }

    CanvasBase::didDraw(FloatRect(FloatPoint(), size()));
}

Image* HTMLCanvasElement::copiedImage() const
{
    if (!m_copiedImage) {
        RefPtr buffer = const_cast<HTMLCanvasElement*>(this)->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No);
        if (buffer)
            m_copiedImage = BitmapImage::create(buffer->copyNativeImage());
    }
    return m_copiedImage.get();
}

void HTMLCanvasElement::clearImageBuffer() const
{
    ASSERT(hasCreatedImageBuffer());
    ASSERT(!m_didClearImageBuffer);
    ASSERT(m_context);

    m_didClearImageBuffer = true;

    if (RefPtr canvas = dynamicDowncast<CanvasRenderingContext2D>(*m_context)) {
        // No need to undo transforms/clip/etc. because we are called right after the context is reset.
        canvas->clearRect(0, 0, width(), height());
    }
}

void HTMLCanvasElement::clearCopiedImage() const
{
    m_copiedImage = nullptr;
    m_didClearImageBuffer = false;
}

bool HTMLCanvasElement::virtualHasPendingActivity() const
{
#if ENABLE(WEBGL)
    if (m_hasRelevantWebGLEventListener) {
        auto* context = dynamicDowncast<WebGLRenderingContextBase>(m_context.get());
        // WebGL rendering context may fire contextlost / contextrestored events at any point.
        return context && !context->isContextUnrecoverablyLost();
    }
#endif

    return false;
}

void HTMLCanvasElement::eventListenersDidChange()
{
#if ENABLE(WEBGL)
    auto& eventNames = WebCore::eventNames();
    m_hasRelevantWebGLEventListener = hasEventListeners(eventNames.webglcontextlostEvent)
        || hasEventListeners(eventNames.webglcontextrestoredEvent);
#endif
}

void HTMLCanvasElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ActiveDOMObject::didMoveToNewDocument(newDocument);
    if (RefPtr context = renderingContext()) {
        oldDocument.removeCanvasNeedingPreparationForDisplayOrFlush(*context);
        newDocument.addCanvasNeedingPreparationForDisplayOrFlush(*context);
    }
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
}

bool HTMLCanvasElement::needsPreparationForDisplay()
{
    return m_context && m_context->needsPreparationForDisplay();
}

void HTMLCanvasElement::prepareForDisplay()
{
    ASSERT(needsPreparationForDisplay());

    bool shouldPrepare = true;
#if ENABLE(WEBGL)
    // FIXME: Currently the below prepare skip logic is conservative and applies only to
    // WebGL elements.
    if (is<WebGLRenderingContextBase>(m_context)) {
        // If the canvas is not in the document body, then it won't be
        // composited and thus doesn't need preparation. Unfortunately
        // it can't tell at the time it was added to the list, since it
        // could be inserted or removed from the document body afterwards.
        shouldPrepare = isInTreeScope() || hasDisplayBufferObservers();
    }
#endif
    if (!shouldPrepare)
        return;
    if (m_context)
        m_context->prepareForDisplay();
    notifyObserversCanvasDisplayBufferPrepared();
}

bool HTMLCanvasElement::isControlledByOffscreen() const
{
    return m_context && m_context->isPlaceholder();
}

void HTMLCanvasElement::queueTaskKeepingObjectAlive(TaskSource source, Function<void()>&& task)
{
    ActiveDOMObject::queueTaskKeepingObjectAlive(*this, source, WTFMove(task));
}

void HTMLCanvasElement::dispatchEvent(Event& event)
{
    Node::dispatchEvent(event);
}

const CSSParserContext& HTMLCanvasElement::cssParserContext() const
{
    if (!m_cssParserContext)
        m_cssParserContext = WTF::makeUnique<CSSParserContext>(document());
    return *m_cssParserContext;
}

WebCoreOpaqueRoot root(HTMLCanvasElement* canvas)
{
    return root(static_cast<Node*>(canvas));
}

}
