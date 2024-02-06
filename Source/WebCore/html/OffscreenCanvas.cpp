/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include "WorkerNavigator.h"
#include <wtf/IsoMallocInlines.h>

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

WTF_MAKE_ISO_ALLOCATED_IMPL(OffscreenCanvas);

DetachedOffscreenCanvas::DetachedOffscreenCanvas(std::unique_ptr<SerializedImageBuffer> buffer, const IntSize& size, bool originClean)
    : m_buffer(WTFMove(buffer))
    , m_size(size)
    , m_originClean(originClean)
{
}

RefPtr<ImageBuffer> DetachedOffscreenCanvas::takeImageBuffer(ScriptExecutionContext& context)
{
    if (!m_buffer)
        return nullptr;
    return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(m_buffer), context.graphicsClient());
}

WeakPtr<HTMLCanvasElement, WeakPtrImplWithEventTargetData> DetachedOffscreenCanvas::takePlaceholderCanvas()
{
    ASSERT(isMainThread());
    return std::exchange(m_placeholderCanvas, nullptr);
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
    auto canvas = adoptRef(*new OffscreenCanvas(scriptExecutionContext, width, height));
    canvas->suspendIfNeeded();
    return canvas;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, std::unique_ptr<DetachedOffscreenCanvas>&& detachedCanvas)
{
    Ref<OffscreenCanvas> clone = adoptRef(*new OffscreenCanvas(scriptExecutionContext, detachedCanvas->size().width(), detachedCanvas->size().height()));
    clone->setImageBuffer(detachedCanvas->takeImageBuffer(scriptExecutionContext));
    if (!detachedCanvas->originClean())
        clone->setOriginTainted();

    callOnMainThread([detachedCanvas = WTFMove(detachedCanvas), placeholderData = Ref { *clone->m_placeholderData }] () mutable {
        if (RefPtr canvas = detachedCanvas->takePlaceholderCanvas().get()) {
            placeholderData->canvas = canvas;
            auto& placeholderContext = downcast<PlaceholderRenderingContext>(*canvas->renderingContext());
            auto& imageBufferPipe = placeholderContext.imageBufferPipe();
            if (imageBufferPipe)
                placeholderData->bufferPipeSource = imageBufferPipe->source();
        }
    });

    clone->suspendIfNeeded();
    return clone;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, HTMLCanvasElement& canvas)
{
    auto offscreen = adoptRef(*new OffscreenCanvas(scriptExecutionContext, canvas.width(), canvas.height()));
    offscreen->setPlaceholderCanvas(canvas);
    offscreen->suspendIfNeeded();
    return offscreen;
}

OffscreenCanvas::OffscreenCanvas(ScriptExecutionContext& scriptExecutionContext, unsigned width, unsigned height)
    : ActiveDOMObject(&scriptExecutionContext)
    , CanvasBase(IntSize(width, height), scriptExecutionContext.noiseInjectionHashSalt())
    , m_placeholderData(PlaceholderData::create())
{
}

OffscreenCanvas::~OffscreenCanvas()
{
    notifyObserversCanvasDestroyed();
    removeCanvasNeedingPreparationForDisplayOrFlush();

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.
    setImageBuffer(nullptr);
}

unsigned OffscreenCanvas::width() const
{
    if (m_detached)
        return 0;
    return CanvasBase::width();
}

unsigned OffscreenCanvas::height() const
{
    if (m_detached)
        return 0;
    return CanvasBase::height();
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
        context->reshape(width(), height());
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

static bool shouldEnableWebGL(const Settings::Values& settings, bool isWorker)
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
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            m_context = OffscreenCanvasRenderingContext2D::create(*this, WTFMove(settings));
        }
        if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(m_context.get()))
            return { { WTFMove(context) } };
        return { { std::nullopt } };
    }
    if (contextType == RenderingContextType::Bitmaprenderer) {
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());
            auto settings = convert<IDLDictionary<ImageBitmapRenderingContextSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            m_context = ImageBitmapRenderingContext::create(*this, WTFMove(settings));
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
                if (auto* gpu = globalScope->navigator().gpu())
                    m_context = GPUCanvasContext::create(*this, *gpu);
            } else if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
                if (RefPtr domWindow = document->domWindow()) {
                    if (auto* gpu = domWindow->navigator().gpu())
                        m_context = GPUCanvasContext::create(*this, *gpu);
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
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            auto* scriptExecutionContext = this->scriptExecutionContext();
            if (shouldEnableWebGL(scriptExecutionContext->settingsValues(), is<WorkerGlobalScope>(scriptExecutionContext)))
                m_context = WebGLRenderingContextBase::create(*this, attributes, webGLVersion);
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

    if (is<OffscreenCanvasRenderingContext2D>(*m_context) || is<ImageBitmapRenderingContext>(*m_context)) {
        if (!width() || !height())
            return { RefPtr<ImageBitmap> { nullptr } };

        if (!m_hasCreatedImageBuffer) {
            auto buffer = allocateImageBuffer();
            if (!buffer)
                return { RefPtr<ImageBitmap> { nullptr } };
            return { ImageBitmap::create(buffer.releaseNonNull(), originClean()) };
        }

        if (!buffer())
            return { RefPtr<ImageBitmap> { nullptr } };

        RefPtr<ImageBuffer> bitmap;
        if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(*m_context)) {
            // As the canvas context state is stored in GraphicsContext, which is owned
            // by buffer(), to avoid resetting the context state, we have to make a copy and
            // clear the original buffer rather than returning the original buffer.
            bitmap = buffer()->clone();
            context->clearCanvas();
        } else {
            // ImageBitmapRenderingContext doesn't use the context state, so we can just take its
            // buffer, and then call transferFromImageBitmap(nullptr) which will trigger it to allocate
            // a new blank bitmap.
            bitmap = buffer();
            downcast<ImageBitmapRenderingContext>(*m_context).transferFromImageBitmap(nullptr);
        }
        clearCopiedImage();
        if (!bitmap)
            return { RefPtr<ImageBitmap> { nullptr } };
        return { ImageBitmap::create(bitmap.releaseNonNull(), originClean(), false, false) };
    }

#if ENABLE(WEBGL)
    if (auto* webGLContext = dynamicDowncast<WebGLRenderingContextBase>(*m_context)) {
        // FIXME: We're supposed to create an ImageBitmap using the backing
        // store from this canvas (or its context), but for now we'll just
        // create a new bitmap and paint into it.
        auto buffer = allocateImageBuffer();
        if (!buffer)
            return { RefPtr<ImageBitmap> { nullptr } };

        RefPtr gc3d = webGLContext->graphicsContextGL();
        gc3d->drawSurfaceBufferToImageBuffer(GraphicsContextGL::SurfaceBuffer::DrawingBuffer, *buffer);

        // FIXME: The transfer algorithm requires that the canvas effectively
        // creates a new backing store. Since we're not doing that yet, we
        // need to erase what's there.

        GCGLfloat clearColor[4] { };
        gc3d->getFloatv(GraphicsContextGL::COLOR_CLEAR_VALUE, clearColor);
        gc3d->clearColor(0, 0, 0, 0);
        gc3d->clear(GraphicsContextGL::COLOR_BUFFER_BIT | GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT);
        gc3d->clearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        return { ImageBitmap::create(buffer.releaseNonNull(), originClean()) };
    }
#endif

    return Exception { ExceptionCode::NotSupportedError };
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
    if (size().isEmpty()) {
        promise->reject(ExceptionCode::IndexSizeError);
        return;
    }
    if (m_detached || !buffer()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    makeRenderingResultsAvailable();

    auto encodingMIMEType = toEncodingMimeType(options.type);
    auto quality = qualityFromDouble(options.quality);

    Vector<uint8_t> blobData = buffer()->toData(encodingMIMEType, quality);
    if (blobData.isEmpty()) {
        promise->reject(ExceptionCode::EncodingError);
        return;
    }

    Ref<Blob> blob = Blob::create(canvasBaseScriptExecutionContext(), WTFMove(blobData), encodingMIMEType);
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

    if (!m_copiedImage && buffer()) {
        if (m_context)
            m_context->drawBufferToCanvas(CanvasRenderingContext::SurfaceBuffer::DrawingBuffer);
        m_copiedImage = BitmapImage::create(buffer()->copyNativeImage());
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

    auto detached = makeUnique<DetachedOffscreenCanvas>(takeImageBuffer(), size(), originClean());
    detached->m_placeholderCanvas = std::exchange(m_placeholderData->canvas, nullptr);

    return detached;
}

void OffscreenCanvas::setPlaceholderCanvas(HTMLCanvasElement& canvas)
{
    ASSERT(!m_context);
    ASSERT(isMainThread());
    Ref protectedCanvas { canvas };
    m_placeholderData->canvas = canvas;
    auto& placeholderContext = downcast<PlaceholderRenderingContext>(*canvas.renderingContext());
    auto& imageBufferPipe = placeholderContext.imageBufferPipe();
    if (imageBufferPipe)
        m_placeholderData->bufferPipeSource = imageBufferPipe->source();
}

void OffscreenCanvas::pushBufferToPlaceholder()
{
    callOnMainThread([placeholderData = Ref { *m_placeholderData }] () mutable {
        Locker locker { placeholderData->bufferLock };
        if (RefPtr canvas = placeholderData->canvas.get()) {
            if (placeholderData->pendingCommitBuffer) {
                auto imageBuffer = SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(placeholderData->pendingCommitBuffer), canvas->document().graphicsClient());
                canvas->setImageBufferAndMarkDirty(WTFMove(imageBuffer));
            }
        }
        placeholderData->pendingCommitBuffer = nullptr;
    });
}

void OffscreenCanvas::commitToPlaceholderCanvas()
{
    RefPtr imageBuffer = buffer();
    if (!imageBuffer)
        return;

    // FIXME: Transfer texture over if we're using accelerated compositing
    if (m_context && (m_context->isWebGL() || m_context->isAccelerated())) {
        if (m_context->compositingResultsNeedUpdating())
            m_context->prepareForDisplay();
        m_context->drawBufferToCanvas(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer);
    }

    if (m_placeholderData->bufferPipeSource) {
        m_placeholderData->bufferPipeSource->handle(*imageBuffer);
    }

    Locker locker { m_placeholderData->bufferLock };
    bool shouldPushBuffer = !m_placeholderData->pendingCommitBuffer;
    if (auto clone = imageBuffer->clone())
        m_placeholderData->pendingCommitBuffer = ImageBuffer::sinkIntoSerializedImageBuffer(WTFMove(clone));
    if (m_placeholderData->pendingCommitBuffer && shouldPushBuffer)
        pushBufferToPlaceholder();
}

void OffscreenCanvas::scheduleCommitToPlaceholderCanvas()
{
    if (!m_hasScheduledCommit) {
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
    m_hasCreatedImageBuffer = true;
    setImageBuffer(allocateImageBuffer());
}

void OffscreenCanvas::setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&& buffer)
{
    m_hasCreatedImageBuffer = true;
    setImageBuffer(WTFMove(buffer));

    CanvasBase::didDraw(FloatRect(FloatPoint(), size()));
}

std::unique_ptr<SerializedImageBuffer> OffscreenCanvas::takeImageBuffer() const
{
    ASSERT(m_detached);

    if (size().isEmpty())
        return nullptr;

    clearCopiedImage();
    RefPtr<ImageBuffer> buffer = setImageBuffer(nullptr);
    if (!buffer)
        return nullptr;
    return ImageBuffer::sinkIntoSerializedImageBuffer(WTFMove(buffer));
}

void OffscreenCanvas::reset()
{
    resetGraphicsContextState();
    if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(m_context.get()))
        context->reset();

    m_hasCreatedImageBuffer = false;
    setImageBuffer(nullptr);
    clearCopiedImage();

    notifyObserversCanvasResized();
    scheduleCommitToPlaceholderCanvas();
}

void OffscreenCanvas::queueTaskKeepingObjectAlive(TaskSource source, Function<void()>&& task)
{
    ActiveDOMObject::queueTaskKeepingObjectAlive(*this, source, WTFMove(task));
}

void OffscreenCanvas::dispatchEvent(Event& event)
{
    EventDispatcher::dispatchEvent({ this }, event);
}

}

#endif
