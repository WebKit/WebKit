/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CSSValuePool.h"
#include "CanvasRenderingContext.h"
#include "Document.h"
#include "HTMLCanvasElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "JSBlob.h"
#include "JSDOMPromiseDeferred.h"
#include "MIMETypeRegistry.h"
#include "OffscreenCanvasRenderingContext2D.h"
#include "PlaceholderRenderingContext.h"
#include "RuntimeEnabledFeatures.h"
#include "WorkerGlobalScope.h"
#include <wtf/IsoMallocInlines.h>

#if ENABLE(WEBGL)
#include "Settings.h"
#include "WebGLRenderingContext.h"

#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#endif // ENABLE(WEBGL)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OffscreenCanvas);

DetachedOffscreenCanvas::DetachedOffscreenCanvas(RefPtr<ImageBuffer>&& buffer, const IntSize& size, bool originClean)
    : m_buffer(WTFMove(buffer))
    , m_size(size)
    , m_originClean(originClean)
{
}

RefPtr<ImageBuffer> DetachedOffscreenCanvas::takeImageBuffer()
{
    return WTFMove(m_buffer);
}

WeakPtr<HTMLCanvasElement> DetachedOffscreenCanvas::takePlaceholderCanvas()
{
    ASSERT(isMainThread());
    return std::exchange(m_placeholderCanvas, nullptr);
}

bool OffscreenCanvas::enabledForContext(ScriptExecutionContext& context)
{
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    if (context.isWorkerGlobalScope())
        return RuntimeEnabledFeatures::sharedFeatures().offscreenCanvasInWorkersEnabled();
#endif

    ASSERT(context.isDocument());
    return true;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, unsigned width, unsigned height)
{
    return adoptRef(*new OffscreenCanvas(scriptExecutionContext, width, height));
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, std::unique_ptr<DetachedOffscreenCanvas>&& detachedCanvas)
{
    Ref<OffscreenCanvas> clone = adoptRef(*new OffscreenCanvas(scriptExecutionContext, detachedCanvas->size().width(), detachedCanvas->size().height()));
    clone->setImageBuffer(detachedCanvas->takeImageBuffer());
    if (!detachedCanvas->originClean())
        clone->setOriginTainted();

    callOnMainThread([detachedCanvas = WTFMove(detachedCanvas), placeholderData = makeRef(*clone->m_placeholderData)] () mutable {
        placeholderData->canvas = detachedCanvas->takePlaceholderCanvas();
        if (placeholderData->canvas) {
            auto& placeholderContext = downcast<PlaceholderRenderingContext>(*placeholderData->canvas->renderingContext());
            auto& imageBufferPipe = placeholderContext.imageBufferPipe();
            if (imageBufferPipe)
                placeholderData->bufferPipeSource = imageBufferPipe->source();
        }
    });

    return clone;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, HTMLCanvasElement& canvas)
{
    auto offscreen = adoptRef(*new OffscreenCanvas(scriptExecutionContext, canvas.width(), canvas.height()));
    offscreen->setPlaceholderCanvas(canvas);
    return offscreen;
}

OffscreenCanvas::OffscreenCanvas(ScriptExecutionContext& scriptExecutionContext, unsigned width, unsigned height)
    : CanvasBase(IntSize(width, height))
    , ContextDestructionObserver(&scriptExecutionContext)
    , m_placeholderData(PlaceholderData::create())
{
}

OffscreenCanvas::~OffscreenCanvas()
{
    notifyObserversCanvasDestroyed();

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

static bool shouldEnableWebGL(bool webGLEnabled, bool acceleratedCompositingEnabled)
{
    if (!webGLEnabled)
        return false;

    if (!requiresAcceleratedCompositingForWebGL())
        return true;

    return acceleratedCompositingEnabled;
}

void OffscreenCanvas::createContextWebGL(RenderingContextType contextType, WebGLContextAttributes&& attrs)
{
    ASSERT(!m_context);

    auto scriptExecutionContext = this->scriptExecutionContext();
    if (scriptExecutionContext->isWorkerGlobalScope()) {
        WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(*scriptExecutionContext);
        if (!shouldEnableWebGL(workerGlobalScope.settingsValues().webGLEnabled, workerGlobalScope.settingsValues().acceleratedCompositingEnabled))
            return;
    } else if (scriptExecutionContext->isDocument()) {
        auto& settings = downcast<Document>(*scriptExecutionContext).settings();
        if (!shouldEnableWebGL(settings.webGLEnabled(), settings.acceleratedCompositingEnabled()))
            return;
    } else
        return;
    GraphicsContextGLWebGLVersion webGLVersion = GraphicsContextGLWebGLVersion::WebGL1;
#if ENABLE(WEBGL2)
    webGLVersion = (contextType == RenderingContextType::Webgl) ? GraphicsContextGLWebGLVersion::WebGL1 : GraphicsContextGLWebGLVersion::WebGL2;
#else
    UNUSED_PARAM(contextType);
#endif
    m_context = WebGLRenderingContextBase::create(*this, attrs, webGLVersion);
}

#endif // ENABLE(WEBGL)

ExceptionOr<std::optional<OffscreenRenderingContext>> OffscreenCanvas::getContext(JSC::JSGlobalObject& state, RenderingContextType contextType, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_detached)
        return Exception { InvalidStateError };

    if (contextType == RenderingContextType::_2d) {
        if (m_context) {
            if (!is<OffscreenCanvasRenderingContext2D>(*m_context))
                return { { std::nullopt } };
            return { { RefPtr<OffscreenCanvasRenderingContext2D> { &downcast<OffscreenCanvasRenderingContext2D>(*m_context) } } };
        }

        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto settings = convert<IDLDictionary<CanvasRenderingContext2DSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        m_context = makeUnique<OffscreenCanvasRenderingContext2D>(*this, WTFMove(settings));
        if (!m_context)
            return { { std::nullopt } };

        return { { RefPtr<OffscreenCanvasRenderingContext2D> { &downcast<OffscreenCanvasRenderingContext2D>(*m_context) } } };
    }
#if ENABLE(WEBGL)
    else {
        if (m_context) {
            if (is<WebGLRenderingContext>(*m_context))
                return { { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } } };
#if ENABLE(WEBGL2)
            if (is<WebGL2RenderingContext>(*m_context))
                return { { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*m_context) } } };
#endif
            return { { std::nullopt } };
        }

        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        createContextWebGL(contextType, WTFMove(attributes));
        if (!m_context)
            return { { std::nullopt } };

#if ENABLE(WEBGL2)
        if (is<WebGL2RenderingContext>(*m_context))
            return { { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*m_context) } } };
#endif
        return { { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } } };
    }
#endif

    return Exception { TypeError };
}

ExceptionOr<RefPtr<ImageBitmap>> OffscreenCanvas::transferToImageBitmap()
{
    if (m_detached || !m_context)
        return Exception { InvalidStateError };

    if (is<OffscreenCanvasRenderingContext2D>(*m_context)) {
        if (!width() || !height())
            return { RefPtr<ImageBitmap> { nullptr } };

        if (!m_hasCreatedImageBuffer) {
            auto buffer = ImageBitmap::createImageBuffer(*canvasBaseScriptExecutionContext(), size(), RenderingMode::Unaccelerated);
            return { ImageBitmap::create(ImageBitmapBacking(WTFMove(buffer))) };
        }

        // As the canvas context state is stored in GraphicsContext, which is owned
        // by buffer(), to avoid resetting the context state, we have to make a copy and
        // clear the original buffer rather than returning the original buffer.
        auto bufferCopy = buffer()->copyRectToBuffer(FloatRect(FloatPoint(), buffer()->logicalSize()), buffer()->colorSpace(), *drawingContext());
        downcast<OffscreenCanvasRenderingContext2D>(*m_context).clearCanvas();
        clearCopiedImage();

        return { ImageBitmap::create(ImageBitmapBacking(WTFMove(bufferCopy), originClean() ? SerializationState::OriginClean : SerializationState())) };
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(*m_context)) {
        auto webGLContext = &downcast<WebGLRenderingContext>(*m_context);

        // FIXME: We're supposed to create an ImageBitmap using the backing
        // store from this canvas (or its context), but for now we'll just
        // create a new bitmap and paint into it.

        auto imageBitmap = ImageBitmap::create(*canvasBaseScriptExecutionContext(), size());
        if (!imageBitmap->buffer())
            return { RefPtr<ImageBitmap> { nullptr } };

        auto* gc3d = webGLContext->graphicsContextGL();
        gc3d->paintRenderingResultsToCanvas(*imageBitmap->buffer());

        // FIXME: The transfer algorithm requires that the canvas effectively
        // creates a new backing store. Since we're not doing that yet, we
        // need to erase what's there.

        GCGLfloat clearColor[4] { };
        gc3d->getFloatv(GraphicsContextGL::COLOR_CLEAR_VALUE, clearColor);
        gc3d->clearColor(0, 0, 0, 0);
        gc3d->clear(GraphicsContextGL::COLOR_BUFFER_BIT | GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT);
        gc3d->clearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

        return { WTFMove(imageBitmap) };
    }
#endif

    return Exception { NotSupportedError };
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
        promise->reject(SecurityError);
        return;
    }
    if (size().isEmpty()) {
        promise->reject(IndexSizeError);
        return;
    }
    if (m_detached || !buffer()) {
        promise->reject(InvalidStateError);
        return;
    }

    makeRenderingResultsAvailable();

    auto encodingMIMEType = toEncodingMimeType(options.type);
    auto quality = qualityFromDouble(options.quality);

    Vector<uint8_t> blobData = buffer()->toData(encodingMIMEType, quality);
    if (blobData.isEmpty()) {
        promise->reject(EncodingError);
        return;
    }

    Ref<Blob> blob = Blob::create(canvasBaseScriptExecutionContext(), WTFMove(blobData), encodingMIMEType);
    promise->resolveWithNewlyCreated<IDLInterface<Blob>>(WTFMove(blob));
}

void OffscreenCanvas::didDraw(const std::optional<FloatRect>& rect)
{
    clearCopiedImage();
    scheduleCommitToPlaceholderCanvas();
    notifyObserversCanvasChanged(rect);
}

Image* OffscreenCanvas::copiedImage() const
{
    if (m_detached)
        return nullptr;

    if (!m_copiedImage && buffer()) {
        if (m_context)
            m_context->paintRenderingResultsToCanvas();
        m_copiedImage = buffer()->copyImage(CopyBackingStore, PreserveResolution::Yes);
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
    if (is<WorkerGlobalScope>(scriptExecutionContext))
        return &downcast<WorkerGlobalScope>(scriptExecutionContext).topOrigin();

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

    m_detached = true;

    auto detached = makeUnique<DetachedOffscreenCanvas>(takeImageBuffer(), size(), originClean());
    detached->m_placeholderCanvas = std::exchange(m_placeholderData->canvas, nullptr);

    return detached;
}

void OffscreenCanvas::setPlaceholderCanvas(HTMLCanvasElement& canvas)
{
    ASSERT(!m_context);
    ASSERT(isMainThread());
    m_placeholderData->canvas = makeWeakPtr(canvas);
    auto& placeholderContext = downcast<PlaceholderRenderingContext>(*canvas.renderingContext());
    auto& imageBufferPipe = placeholderContext.imageBufferPipe();
    if (imageBufferPipe)
        m_placeholderData->bufferPipeSource = imageBufferPipe->source();
}

void OffscreenCanvas::pushBufferToPlaceholder()
{
    callOnMainThread([placeholderData = makeRef(*m_placeholderData)] () mutable {
        Locker locker { placeholderData->bufferLock };
        if (placeholderData->canvas && placeholderData->pendingCommitBuffer)
            placeholderData->canvas->setImageBufferAndMarkDirty(WTFMove(placeholderData->pendingCommitBuffer));
        placeholderData->pendingCommitBuffer = nullptr;
    });
}

void OffscreenCanvas::commitToPlaceholderCanvas()
{
    auto* imageBuffer = buffer();
    if (!imageBuffer)
        return;

    // FIXME: Transfer texture over if we're using accelerated compositing
    if (m_context && (m_context->isWebGL() || m_context->isAccelerated()))
        m_context->paintRenderingResultsToCanvas();

    if (m_placeholderData->bufferPipeSource) {
        auto bufferCopy = imageBuffer->copyRectToBuffer(FloatRect(FloatPoint(), imageBuffer->logicalSize()), DestinationColorSpace::SRGB(), imageBuffer->context());
        if (bufferCopy)
            m_placeholderData->bufferPipeSource->handle(WTFMove(bufferCopy));
    }

    Locker locker { m_placeholderData->bufferLock };
    bool shouldPushBuffer = !m_placeholderData->pendingCommitBuffer;
    m_placeholderData->pendingCommitBuffer = imageBuffer->copyRectToBuffer(FloatRect(FloatPoint(), imageBuffer->logicalSize()), DestinationColorSpace::SRGB(), imageBuffer->context());
    if (m_placeholderData->pendingCommitBuffer && shouldPushBuffer)
        pushBufferToPlaceholder();
}

void OffscreenCanvas::scheduleCommitToPlaceholderCanvas()
{
    if (!m_hasScheduledCommit) {
        auto& scriptContext = *scriptExecutionContext();
        m_hasScheduledCommit = true;
        scriptContext.postTask([protectedThis = makeRef(*this), this] (ScriptExecutionContext&) {
            m_hasScheduledCommit = false;
            commitToPlaceholderCanvas();
        });
    }
}

CSSValuePool& OffscreenCanvas::cssValuePool()
{
    auto* scriptExecutionContext = canvasBaseScriptExecutionContext();
    if (scriptExecutionContext->isWorkerGlobalScope())
        return downcast<WorkerGlobalScope>(*scriptExecutionContext).cssValuePool();

    ASSERT(scriptExecutionContext->isDocument());
    return CSSValuePool::singleton();
}

void OffscreenCanvas::createImageBuffer() const
{
    m_hasCreatedImageBuffer = true;

    if (!width() || !height())
        return;

    setImageBuffer(ImageBitmap::createImageBuffer(*canvasBaseScriptExecutionContext(), size(), RenderingMode::Unaccelerated));
}

RefPtr<ImageBuffer> OffscreenCanvas::takeImageBuffer() const
{
    ASSERT(m_detached);

    if (size().isEmpty())
        return nullptr;

    clearCopiedImage();
    return setImageBuffer(nullptr);
}

void OffscreenCanvas::reset()
{
    resetGraphicsContextState();
    if (is<OffscreenCanvasRenderingContext2D>(m_context.get()))
        downcast<OffscreenCanvasRenderingContext2D>(*m_context).reset();

    m_hasCreatedImageBuffer = false;
    setImageBuffer(nullptr);
    clearCopiedImage();

    notifyObserversCanvasResized();
    scheduleCommitToPlaceholderCanvas();
}

}

#endif
