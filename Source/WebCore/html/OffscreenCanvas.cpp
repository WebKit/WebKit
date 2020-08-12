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
#include "WebGLRenderingContext.h"
#include "WorkerGlobalScope.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OffscreenCanvas);

DetachedOffscreenCanvas::DetachedOffscreenCanvas(std::unique_ptr<ImageBuffer>&& buffer, const IntSize& size, bool originClean)
    : m_buffer(WTFMove(buffer))
    , m_size(size)
    , m_originClean(originClean)
{
}

std::unique_ptr<ImageBuffer> DetachedOffscreenCanvas::takeImageBuffer()
{
    return WTFMove(m_buffer);
}

WeakPtr<HTMLCanvasElement> DetachedOffscreenCanvas::takePlaceholderCanvas()
{
    ASSERT(isMainThread());
    return std::exchange(m_placeholderCanvas, nullptr);
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& context, unsigned width, unsigned height)
{
    return adoptRef(*new OffscreenCanvas(context, width, height));
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& context, std::unique_ptr<DetachedOffscreenCanvas>&& detachedCanvas)
{
    Ref<OffscreenCanvas> clone = adoptRef(*new OffscreenCanvas(context, detachedCanvas->size().width(), detachedCanvas->size().height()));
    clone->setImageBuffer(detachedCanvas->takeImageBuffer());
    if (!detachedCanvas->originClean())
        clone->setOriginTainted();

    callOnMainThread([detachedCanvas = WTFMove(detachedCanvas), offscreenCanvas = makeRef(clone.get())] () mutable {
        offscreenCanvas->m_placeholderCanvas = detachedCanvas->takePlaceholderCanvas();
        offscreenCanvas->scriptExecutionContext()->postTask({ ScriptExecutionContext::Task::CleanupTask,
            [releaseOffscreenCanvas = WTFMove(offscreenCanvas)] (ScriptExecutionContext&) { } });
    });

    return clone;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& context, HTMLCanvasElement& canvas)
{
    auto offscreen = adoptRef(*new OffscreenCanvas(context, canvas.width(), canvas.height()));
    offscreen->setPlaceholderCanvas(canvas);
    return offscreen;
}

OffscreenCanvas::OffscreenCanvas(ScriptExecutionContext& context, unsigned width, unsigned height)
    : CanvasBase(IntSize(width, height))
    , ContextDestructionObserver(&context)
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

ExceptionOr<Optional<OffscreenRenderingContext>> OffscreenCanvas::getContext(JSC::JSGlobalObject& state, RenderingContextType contextType, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_detached)
        return Exception { InvalidStateError };

    if (contextType == RenderingContextType::_2d) {
        if (m_context) {
            if (!is<OffscreenCanvasRenderingContext2D>(*m_context))
                return { { WTF::nullopt } };
            return { { RefPtr<OffscreenCanvasRenderingContext2D> { &downcast<OffscreenCanvasRenderingContext2D>(*m_context) } } };
        }

        m_context = makeUnique<OffscreenCanvasRenderingContext2D>(*this);
        if (!m_context)
            return { { WTF::nullopt } };

        return { { RefPtr<OffscreenCanvasRenderingContext2D> { &downcast<OffscreenCanvasRenderingContext2D>(*m_context) } } };
    }
#if ENABLE(WEBGL)
    if (contextType == RenderingContextType::Webgl) {
        if (m_context) {
            if (is<WebGLRenderingContext>(*m_context))
                return { { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } } };
            return { { WTF::nullopt } };
        }

        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, !arguments.isEmpty() ? arguments[0].get() : JSC::jsUndefined());
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        m_context = WebGLRenderingContextBase::create(*this, attributes, "webgl");
        if (!m_context)
            return { { WTF::nullopt } };

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

        if (!m_hasCreatedImageBuffer)
            return { ImageBitmap::create({ ImageBuffer::create(size(), RenderingMode::Unaccelerated), ImageBuffer::SerializationState { true, false, false }}) };

        auto buffer = takeImageBuffer();
        if (!buffer)
            return { RefPtr<ImageBitmap> { nullptr } };

        return { ImageBitmap::create({ WTFMove(buffer), ImageBuffer::SerializationState { originClean(), false, false }}) };
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(*m_context)) {
        auto webGLContext = &downcast<WebGLRenderingContext>(*m_context);

        // FIXME: We're supposed to create an ImageBitmap using the backing
        // store from this canvas (or its context), but for now we'll just
        // create a new bitmap and paint into it.

        auto imageBitmap = ImageBitmap::create(size());
        if (!imageBitmap->buffer())
            return { RefPtr<ImageBitmap> { nullptr } };

        auto* gc3d = webGLContext->graphicsContextGL();
        gc3d->paintRenderingResultsToCanvas(imageBitmap->buffer());

        // FIXME: The transfer algorithm requires that the canvas effectively
        // creates a new backing store. Since we're not doing that yet, we
        // need to erase what's there.

        GCGLfloat clearColor[4];
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

static Optional<double> qualityFromDouble(double qualityNumber)
{
    if (!(qualityNumber >= 0 && qualityNumber <= 1))
        return WTF::nullopt;

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

    Ref<Blob> blob = Blob::create(WTFMove(blobData), encodingMIMEType);
    promise->resolveWithNewlyCreated<IDLInterface<Blob>>(WTFMove(blob));
}

void OffscreenCanvas::didDraw(const FloatRect& rect)
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
    auto& context = *canvasBaseScriptExecutionContext();
    if (is<WorkerGlobalScope>(context))
        return &downcast<WorkerGlobalScope>(context).topOrigin();

    return &downcast<Document>(context).securityOrigin();
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
    detached->m_placeholderCanvas = std::exchange(m_placeholderCanvas, nullptr);

    return detached;
}

void OffscreenCanvas::setPlaceholderCanvas(HTMLCanvasElement& canvas)
{
    ASSERT(!m_context);
    ASSERT(isMainThread());
    m_placeholderCanvas = makeWeakPtr(canvas);
}

void OffscreenCanvas::pushBufferToPlaceholder()
{
    callOnMainThread([protectedThis = makeRef(*this), this] () mutable {
        auto locker = holdLock(m_commitLock);

        if (m_placeholderCanvas && m_hasPendingCommitData) {
            std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(FloatSize(m_pendingCommitData->size()), RenderingMode::Unaccelerated);
            buffer->putImageData(AlphaPremultiplication::Premultiplied, *m_pendingCommitData, IntRect(IntPoint(), m_pendingCommitData->size()));
            m_placeholderCanvas->setImageBufferAndMarkDirty(WTFMove(buffer));
            m_hasPendingCommitData = false;
        }

        scriptExecutionContext()->postTask([releaseThis = WTFMove(protectedThis)] (ScriptExecutionContext&) { });
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

    if (isMainThread()) {
        if (m_placeholderCanvas) {
            if (auto bufferCopy = imageBuffer->copyRectToBuffer(FloatRect(FloatPoint(), imageBuffer->logicalSize()), ColorSpace::SRGB, imageBuffer->context()))
                m_placeholderCanvas->setImageBufferAndMarkDirty(WTFMove(bufferCopy));
        }
        return;
    }

    auto locker = holdLock(m_commitLock);

    bool shouldPushBuffer = !m_hasPendingCommitData;
    m_pendingCommitData = imageBuffer->getImageData(AlphaPremultiplication::Premultiplied, IntRect(IntPoint(), imageBuffer->logicalSize()));
    m_hasPendingCommitData = true;

    if (shouldPushBuffer)
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
    auto* context = canvasBaseScriptExecutionContext();
    if (context->isWorkerGlobalScope())
        return downcast<WorkerGlobalScope>(*context).cssValuePool();

    ASSERT(context->isDocument());
    return CSSValuePool::singleton();
}

void OffscreenCanvas::createImageBuffer() const
{
    m_hasCreatedImageBuffer = true;

    if (!width() || !height())
        return;

    setImageBuffer(ImageBuffer::create(size(), RenderingMode::Unaccelerated));
}

std::unique_ptr<ImageBuffer> OffscreenCanvas::takeImageBuffer() const
{
    if (!m_detached)
        m_hasCreatedImageBuffer = true;

    // This function is primarily for use with transferToImageBitmap, which
    // requires that the canvas bitmap refer to a new, blank bitmap of the same
    // size after the existing bitmap is taken. In the case of a zero-size
    // bitmap, our buffer is null, so returning early here is valid.
    if (size().isEmpty())
        return nullptr;

    clearCopiedImage();
    return setImageBuffer(m_detached ? nullptr : ImageBuffer::create(size(), RenderingMode::Unaccelerated));
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
