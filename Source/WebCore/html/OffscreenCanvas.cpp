/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OffscreenCanvas.h"

#if ENABLE(OFFSCREEN_CANVAS)

#include "BitmapImage.h"
#include "CSSValuePool.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "Document.h"
#include "EventDispatcher.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "HTMLCanvasElement.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageData.h"
#include "JSBlob.h"
#include "JSDOMPromiseDeferred.h"
#include "MIMETypeRegistry.h"
#include "OffscreenCanvasRenderingContext2D.h"
#include "Page.h"
#include "PlaceholderRenderingContext.h"
#include "ScriptTrackingPrivacyCategory.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include "WorkerNavigator.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBGL)
#include "Settings.h"
#include "WebGLRenderingContext.h"
#include "WebGL2RenderingContext.h"
#endif // ENABLE(WEBGL)

#if HAVE(WEBGPU_IMPLEMENTATION)
#include "LocalDomWindow.h"
#include "Navigator.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DetachedOffscreenCanvas);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(OffscreenCanvas);

DetachedOffscreenCanvas::DetachedOffscreenCanvas(const IntSize& size, bool originClean, RefPtr<PlaceholderRenderingContextSource>&& placeholderSource)
    : m_placeholderSource(WTFMove(placeholderSource))
    , m_size(size)
    , m_originClean(originClean)
{
}

DetachedOffscreenCanvas::~DetachedOffscreenCanvas() = default;

RefPtr<PlaceholderRenderingContextSource> DetachedOffscreenCanvas::takePlaceholderSource()
{
    return WTFMove(m_placeholderSource);
}

bool OffscreenCanvas::enabledForContext(ScriptExecutionContext& context)
{
    UNUSED_PARAM(context);

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    if (context.isWorkerGlobalScope())
        return context.settingsValues().offscreenCanvasInWorkersEnabled;
#endif

    ASSERT(context.isDocument());
    return true;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, unsigned width, unsigned height)
{
    auto canvas = adoptRef(*new OffscreenCanvas(scriptExecutionContext, { static_cast<int>(width), static_cast<int>(height) }, nullptr));
    canvas->suspendIfNeeded();
    return canvas;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, std::unique_ptr<DetachedOffscreenCanvas>&& detachedCanvas)
{
    Ref<OffscreenCanvas> clone = adoptRef(*new OffscreenCanvas(scriptExecutionContext, detachedCanvas->size(), detachedCanvas->takePlaceholderSource()));
    if (!detachedCanvas->originClean())
        clone->setOriginTainted();
    clone->suspendIfNeeded();
    return clone;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, PlaceholderRenderingContext& placeholder)
{
    auto offscreen = adoptRef(*new OffscreenCanvas(scriptExecutionContext, placeholder.size(), &placeholder.source()));
    offscreen->suspendIfNeeded();
    return offscreen;
}

OffscreenCanvas::OffscreenCanvas(ScriptExecutionContext& scriptExecutionContext, IntSize size, RefPtr<PlaceholderRenderingContextSource>&& placeholderSource)
    : ActiveDOMObject(&scriptExecutionContext)
    , CanvasBase(WTFMove(size), scriptExecutionContext)
    , m_placeholderSource(WTFMove(placeholderSource))
{
}

OffscreenCanvas::~OffscreenCanvas()
{
    notifyObserversCanvasDestroyed();
    removeCanvasNeedingPreparationForDisplayOrFlush();

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.
    setImageBuffer(nullptr);
}

void OffscreenCanvas::setWidth(unsigned newWidth)
{
    if (m_detached)
        return;
    setSize(IntSize(newWidth, height()));
}

void OffscreenCanvas::setHeight(unsigned newHeight)
{
    if (m_detached)
        return;
    setSize(IntSize(width(), newHeight));
}

void OffscreenCanvas::setSize(const IntSize& newSize)
{
    CanvasBase::setSize(newSize);
    reset();

    if (RefPtr context = dynamicDowncast<GPUBasedCanvasRenderingContext>(m_context.get()))
        context->reshape();
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

static bool shouldEnableWebGL(const SettingsValues& settings, bool isWorker)
{
    if (!settings.webGLEnabled)
        return false;

    if (!settings.allowWebGLInWorkers)
        return false;

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
    if (isWorker && !settings.useGPUProcessForWebGLEnabled)
        return false;
#else
    UNUSED_PARAM(isWorker);
#endif

    if (!requiresAcceleratedCompositingForWebGL())
        return true;

    return settings.acceleratedCompositingEnabled;
}

#endif // ENABLE(WEBGL)

ExceptionOr<std::optional<OffscreenRenderingContext>> OffscreenCanvas::getContext(JSC::JSGlobalObject& state, RenderingContextType contextType, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_detached)
        return Exception { ExceptionCode::InvalidStateError };

    if (contextType == RenderingContextType::_2d) {
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());

            auto settings = convert<IDLDictionary<CanvasRenderingContext2DSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            if (settings.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };

            m_context = OffscreenCanvasRenderingContext2D::create(*this, settings.releaseReturnValue());
        }
        if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(m_context.get()))
            return { { WTFMove(context) } };
        return { { std::nullopt } };
    }
    if (contextType == RenderingContextType::Bitmaprenderer) {
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());

            auto settings = convert<IDLDictionary<ImageBitmapRenderingContextSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            if (settings.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };

            m_context = ImageBitmapRenderingContext::create(*this, settings.releaseReturnValue());
            downcast<ImageBitmapRenderingContext>(m_context.get())->transferFromImageBitmap(nullptr);
        }
        if (RefPtr context = dynamicDowncast<ImageBitmapRenderingContext>(m_context.get()))
            return { { WTFMove(context) } };
        return { { std::nullopt } };
    }
    if (contextType == RenderingContextType::Webgpu) {
#if HAVE(WEBGPU_IMPLEMENTATION)
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            Ref scriptExecutionContext = *this->scriptExecutionContext();
            if (RefPtr globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext)) {
                if (auto* gpu = globalScope->protectedNavigator()->gpu())
                    m_context = GPUCanvasContext::create(*this, *gpu, nullptr);
            } else if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
                if (RefPtr window = document->window()) {
                    if (auto* gpu = window->protectedNavigator()->gpu())
                        m_context = GPUCanvasContext::create(*this, *gpu, document.get());
                }
            }
        }
        if (RefPtr context = dynamicDowncast<GPUCanvasContext>(m_context.get()))
            return { { WTFMove(context) } };
#endif
        return { { std::nullopt } };
    }
#if ENABLE(WEBGL)
    if (contextType == RenderingContextType::Webgl || contextType == RenderingContextType::Webgl2) {
        auto webGLVersion = contextType == RenderingContextType::Webgl ? WebGLVersion::WebGL1 : WebGLVersion::WebGL2;
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());

            auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            if (attributes.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };

            auto* scriptExecutionContext = this->scriptExecutionContext();
            if (shouldEnableWebGL(scriptExecutionContext->settingsValues(), is<WorkerGlobalScope>(scriptExecutionContext)))
                m_context = WebGLRenderingContextBase::create(*this, attributes.releaseReturnValue(), webGLVersion);
        }
        if (webGLVersion == WebGLVersion::WebGL1) {
            if (RefPtr context = dynamicDowncast<WebGLRenderingContext>(m_context.get()))
                return { { WTFMove(context) } };
        } else {
            if (RefPtr context = dynamicDowncast<WebGL2RenderingContext>(m_context.get()))
                return { { WTFMove(context) } };
        }
        return { { std::nullopt } };
    }
#endif

    return Exception { ExceptionCode::TypeError };
}

ExceptionOr<RefPtr<ImageBitmap>> OffscreenCanvas::transferToImageBitmap()
{
    if (m_detached || !m_context)
        return Exception { ExceptionCode::InvalidStateError };
    if (size().isEmpty())
        return { RefPtr<ImageBitmap> { nullptr } };
    clearCopiedImage();
    RefPtr buffer = m_context->transferToImageBuffer();
    if (!buffer)
        return Exception { ExceptionCode::UnknownError }; // UnknownError is used for DOM out-of-memory.
    return { ImageBitmap::create(buffer.releaseNonNull(), originClean()) };
}

static String toEncodingMimeType(const String& mimeType)
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return "image/png"_s;
    return mimeType.convertToASCIILowercase();
}

static std::optional<double> qualityFromDouble(double qualityNumber)
{
    if (!(qualityNumber >= 0 && qualityNumber <= 1))
        return std::nullopt;

    return qualityNumber;
}

void OffscreenCanvas::convertToBlob(ImageEncodeOptions&& options, Ref<DeferredPromise>&& promise)
{
    if (!originClean()) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }
    if (m_detached) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    if (size().isEmpty()) {
        promise->reject(ExceptionCode::IndexSizeError);
        return;
    }

    auto encodingMIMEType = toEncodingMimeType(options.type);
    auto quality = qualityFromDouble(options.quality);

    RefPtr context = canvasBaseScriptExecutionContext();
    if (context && context->requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::Canvas)) {
        RefPtr buffer = createImageForNoiseInjection();
        auto blobData = buffer->toData(encodingMIMEType, quality);
        if (blobData.isEmpty())
            promise->reject(ExceptionCode::EncodingError);
        else
            promise->resolveWithNewlyCreated<IDLInterface<Blob>>(Blob::create(context.get(), WTFMove(blobData), encodingMIMEType));
        return;
    }

    RefPtr buffer = makeRenderingResultsAvailable();
    if (!buffer) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    Vector<uint8_t> blobData = buffer->toData(encodingMIMEType, quality);
    if (blobData.isEmpty()) {
        promise->reject(ExceptionCode::EncodingError);
        return;
    }

    Ref<Blob> blob = Blob::create(context.get(), WTFMove(blobData), encodingMIMEType);
    promise->resolveWithNewlyCreated<IDLInterface<Blob>>(WTFMove(blob));
}

void OffscreenCanvas::didDraw(const std::optional<FloatRect>& rect, ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    clearCopiedImage();
    scheduleCommitToPlaceholderCanvas();
    CanvasBase::didDraw(rect, shouldApplyPostProcessingToDirtyRect);
}

Image* OffscreenCanvas::copiedImage() const
{
    if (m_detached)
        return nullptr;

    if (!m_copiedImage) {
        RefPtr buffer = const_cast<OffscreenCanvas*>(this)->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No);
        if (buffer)
            m_copiedImage = BitmapImage::create(buffer->copyNativeImage());
    }
    return m_copiedImage.get();
}

void OffscreenCanvas::clearCopiedImage() const
{
    m_copiedImage = nullptr;
}

SecurityOrigin* OffscreenCanvas::securityOrigin() const
{
    auto& scriptExecutionContext = *canvasBaseScriptExecutionContext();
    if (auto* globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext))
        return &globalScope->topOrigin();

    return &downcast<Document>(scriptExecutionContext).securityOrigin();
}

bool OffscreenCanvas::canDetach() const
{
    return !m_detached && !m_context;
}

std::unique_ptr<DetachedOffscreenCanvas> OffscreenCanvas::detach()
{
    if (!canDetach())
        return nullptr;

    removeCanvasNeedingPreparationForDisplayOrFlush();

    m_detached = true;

    auto detached = makeUnique<DetachedOffscreenCanvas>(size(), originClean(), WTFMove(m_placeholderSource));
    setSize(IntSize(0, 0));
    return detached;
}

void OffscreenCanvas::commitToPlaceholderCanvas()
{
    if (!m_placeholderSource)
        return;
    if  (!m_context)
        return;
    if (m_context->compositingResultsNeedUpdating())
        m_context->prepareForDisplay();
    RefPtr imageBuffer = m_context->surfaceBufferToImageBuffer(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer);
    if (!imageBuffer)
        return;
    m_placeholderSource->setPlaceholderBuffer(*imageBuffer, m_context->isOpaque());
    }

void OffscreenCanvas::scheduleCommitToPlaceholderCanvas()
{
    if (!m_hasScheduledCommit && m_placeholderSource) {
        auto& scriptContext = *scriptExecutionContext();
        m_hasScheduledCommit = true;
        scriptContext.postTask([protectedThis = Ref { *this }, this] (ScriptExecutionContext&) {
            m_hasScheduledCommit = false;
            commitToPlaceholderCanvas();
        });
    }
}

void OffscreenCanvas::createImageBuffer() const
{
    const_cast<OffscreenCanvas*>(this)->setHasCreatedImageBuffer(true);
    setImageBuffer(allocateImageBuffer());
}

void OffscreenCanvas::setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&& buffer)
{
    setHasCreatedImageBuffer(true);
    setImageBuffer(WTFMove(buffer));

    CanvasBase::didDraw(FloatRect(FloatPoint(), size()));
}

void OffscreenCanvas::reset()
{
    resetGraphicsContextState();
    if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(m_context.get()))
        context->reset();

    setHasCreatedImageBuffer(false);
    setImageBuffer(nullptr);
    clearCopiedImage();

    notifyObserversCanvasResized();
    scheduleCommitToPlaceholderCanvas();
}

void OffscreenCanvas::queueTaskKeepingObjectAlive(TaskSource source, Function<void(CanvasBase&)>&& task)
{
    ActiveDOMObject::queueTaskKeepingObjectAlive(*this, source, [task = WTFMove(task)](auto& canvas) mutable {
        task(canvas);
    });
}

void OffscreenCanvas::dispatchEvent(Event& event)
{
    EventDispatcher::dispatchEvent(std::initializer_list<EventTarget*>({ this }), event);
}

std::unique_ptr<CSSParserContext> OffscreenCanvas::createCSSParserContext() const
{
    // FIXME: Rather than using a default CSSParserContext, there should be one exposed via ScriptExecutionContext.
    return makeUnique<CSSParserContext>(HTMLStandardMode);
}

}

#endif
