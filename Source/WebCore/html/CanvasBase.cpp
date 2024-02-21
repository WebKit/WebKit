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
#include "CanvasBase.h"

#include "ByteArrayPixelBuffer.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "Document.h"
#include "Element.h"
#include "GraphicsClient.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include "IntRect.h"
#include "StyleCanvasImage.h"
#include "RenderElement.h"
#include "WebCoreOpaqueRoot.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <atomic>
#include <wtf/Vector.h>

static std::atomic<size_t> s_activePixelMemory { 0 };

namespace WebCore {

constexpr InterpolationQuality defaultInterpolationQuality = InterpolationQuality::Low;
static std::optional<size_t> maxCanvasAreaForTesting;

CanvasBase::CanvasBase(IntSize size, const std::optional<NoiseInjectionHashSalt>& noiseHashSalt)
    : m_size(size)
    , m_canvasNoiseHashSalt(noiseHashSalt)
{
}

CanvasBase::~CanvasBase()
{
    ASSERT(m_didNotifyObserversCanvasDestroyed);
    ASSERT(m_observers.isEmptyIgnoringNullReferences());
    ASSERT(!m_imageBuffer);
    m_canvasNoiseHashSalt = std::nullopt;
}

GraphicsContext* CanvasBase::drawingContext() const
{
    auto* context = renderingContext();
    if (context && !context->is2d() && !context->isOffscreen2d())
        return nullptr;

    return buffer() ? &m_imageBuffer->context() : nullptr;
}

GraphicsContext* CanvasBase::existingDrawingContext() const
{
    if (!hasCreatedImageBuffer())
        return nullptr;

    return drawingContext();
}

ImageBuffer* CanvasBase::buffer() const
{
    if (!hasCreatedImageBuffer())
        createImageBuffer();
    return m_imageBuffer.get();
}

AffineTransform CanvasBase::baseTransform() const
{
    ASSERT(hasCreatedImageBuffer());
    return m_imageBuffer->baseTransform();
}

void CanvasBase::makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    if (auto* context = renderingContext()) {
        context->drawBufferToCanvas(CanvasRenderingContext::SurfaceBuffer::DrawingBuffer);
        if (m_canvasNoiseHashSalt && shouldApplyPostProcessingToDirtyRect == ShouldApplyPostProcessingToDirtyRect::Yes)
            m_canvasNoiseInjection.postProcessDirtyCanvasBuffer(buffer(), *m_canvasNoiseHashSalt, context->is2d() ? CanvasNoiseInjectionPostProcessArea::DirtyRect : CanvasNoiseInjectionPostProcessArea::FullBuffer);
    }
}

size_t CanvasBase::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful
    // about what data we access here and how. We need to hold a lock to prevent m_imageBuffer
    // from being changed while we access it.
    Locker locker { m_imageBufferAssignmentLock };
    if (!m_imageBuffer)
        return 0;
    return m_imageBuffer->memoryCost();
}

size_t CanvasBase::externalMemoryCost() const
{
    // externalMemoryCost() may be invoked concurrently from a GC thread, and we need to be careful
    // about what data we access here and how. We need to hold a lock to prevent m_imageBuffer
    // from being changed while we access it.
    Locker locker { m_imageBufferAssignmentLock };
    if (!m_imageBuffer)
        return 0;
    return m_imageBuffer->externalMemoryCost();
}

static inline size_t maxCanvasArea()
{
    if (maxCanvasAreaForTesting)
        return *maxCanvasAreaForTesting;

    // Firefox limits width/height to 32767 pixels, but slows down dramatically before it
    // reaches that limit. We limit by area instead, giving us larger maximum dimensions,
    // in exchange for a smaller maximum canvas size. The maximum canvas size is in device pixels.
#if PLATFORM(IOS_FAMILY)
    return 4096 * 4096;
#else
    return 16384 * 16384;
#endif
}

void CanvasBase::setMaxCanvasAreaForTesting(std::optional<size_t> size)
{
    maxCanvasAreaForTesting = size;
}

void CanvasBase::addObserver(CanvasObserver& observer)
{
    m_observers.add(observer);

    if (is<StyleCanvasImage>(observer))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*this);
}

void CanvasBase::removeObserver(CanvasObserver& observer)
{
    m_observers.remove(observer);

    if (is<StyleCanvasImage>(observer))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*this);
}

bool CanvasBase::hasObserver(CanvasObserver& observer) const
{
    return m_observers.contains(observer);
}

void CanvasBase::notifyObserversCanvasChanged(const FloatRect& rect)
{
    for (auto& observer : m_observers)
        observer.canvasChanged(*this, rect);
}

void CanvasBase::didDraw(const std::optional<FloatRect>& rect, ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    addCanvasNeedingPreparationForDisplayOrFlush();
    IntRect dirtyRect { { }, size() };
    if (rect)
        dirtyRect.intersect(enclosingIntRect(*rect));
    notifyObserversCanvasChanged(dirtyRect);

    // FIXME: We should exclude rects with ShouldApplyPostProcessingToDirtyRect::No
    if (shouldInjectNoiseBeforeReadback()) {
        if (shouldApplyPostProcessingToDirtyRect == ShouldApplyPostProcessingToDirtyRect::Yes) {
            m_canvasNoiseInjection.updateDirtyRect(dirtyRect);
        } else if (!rect)
            m_canvasNoiseInjection.clearDirtyRect();
    }
}

void CanvasBase::notifyObserversCanvasResized()
{
    for (auto& observer : m_observers)
        observer.canvasResized(*this);
}

void CanvasBase::notifyObserversCanvasDestroyed()
{
    ASSERT(!m_didNotifyObserversCanvasDestroyed);

    for (auto& observer : std::exchange(m_observers, WeakHashSet<CanvasObserver>()))
        observer.canvasDestroyed(*this);

#if ASSERT_ENABLED
    m_didNotifyObserversCanvasDestroyed = true;
#endif
}

void CanvasBase::addDisplayBufferObserver(CanvasDisplayBufferObserver& observer)
{
    m_displayBufferObservers.add(observer);
}

void CanvasBase::removeDisplayBufferObserver(CanvasDisplayBufferObserver& observer)
{
    m_displayBufferObservers.remove(observer);
}

void CanvasBase::notifyObserversCanvasDisplayBufferPrepared()
{
    for (auto& observer : m_displayBufferObservers)
        observer.canvasDisplayBufferPrepared(*this);
}

HashSet<Element*> CanvasBase::cssCanvasClients() const
{
    HashSet<Element*> cssCanvasClients;
    for (auto& observer : m_observers) {
        auto* image = dynamicDowncast<StyleCanvasImage>(observer);
        if (!image)
            continue;

        for (auto entry : image->clients()) {
            auto& client = entry.key;
            if (auto element = client.element())
                cssCanvasClients.add(element);
        }
    }
    return cssCanvasClients;
}

bool CanvasBase::hasActiveInspectorCanvasCallTracer() const
{
    auto* context = renderingContext();
    return context && context->hasActiveInspectorCanvasCallTracer();
}

void CanvasBase::setSize(const IntSize& size)
{
    if (size == m_size)
        return;

    m_size = size;

    if (auto* context = renderingContext())
        InspectorInstrumentation::didChangeCanvasSize(*context);
}

RefPtr<ImageBuffer> CanvasBase::setImageBuffer(RefPtr<ImageBuffer>&& buffer) const
{
    RefPtr<ImageBuffer> returnBuffer;
    {
        Locker locker { m_imageBufferAssignmentLock };
        m_contextStateSaver = nullptr;
        returnBuffer = std::exchange(m_imageBuffer, WTFMove(buffer));
    }

    auto* context = renderingContext();

    if (m_imageBuffer && m_size != m_imageBuffer->truncatedLogicalSize()) {
        m_size = m_imageBuffer->truncatedLogicalSize();

        if (context)
            InspectorInstrumentation::didChangeCanvasSize(*context);
    }

    size_t previousMemoryCost = m_imageBufferCost;
    m_imageBufferCost = memoryCost();
    s_activePixelMemory += m_imageBufferCost - previousMemoryCost;

    if (context && m_imageBuffer && previousMemoryCost != m_imageBufferCost)
        InspectorInstrumentation::didChangeCanvasMemory(*context);

    if (m_imageBuffer) {
        m_imageBuffer->context().setShadowsIgnoreTransforms(true);
        m_imageBuffer->context().setImageInterpolationQuality(defaultInterpolationQuality);
        m_imageBuffer->context().setStrokeThickness(1);
        m_contextStateSaver = makeUnique<GraphicsContextStateSaver>(m_imageBuffer->context());

        JSC::JSLockHolder lock(scriptExecutionContext()->vm());
        scriptExecutionContext()->vm().heap.reportExtraMemoryAllocated(static_cast<JSCell*>(nullptr), memoryCost());
    }

    return returnBuffer;
}

bool CanvasBase::shouldAccelerate(const IntSize& size) const
{
    return shouldAccelerate(size.unclampedArea());
}

bool CanvasBase::shouldAccelerate(uint64_t area) const
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (!scriptExecutionContext()->settingsValues().canvasUsesAcceleratedDrawing)
        return false;
    if (area < scriptExecutionContext()->settingsValues().minimumAccelerated2DContextArea)
        return false;
    return true;
#else
    UNUSED_PARAM(area);
    return false;
#endif
}

RefPtr<ImageBuffer> CanvasBase::allocateImageBuffer() const
{
    uint64_t area = size().unclampedArea();
    if (!area)
        return nullptr;
    if (area > maxCanvasArea()) {
        auto message = makeString("Canvas area exceeds the maximum limit (width * height > ", maxCanvasArea(), ").");
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
        return nullptr;
    }

    OptionSet<ImageBufferOptions> bufferOptions;
    if (shouldAccelerate(area))
        bufferOptions.add(ImageBufferOptions::Accelerated);

    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = PixelFormat::BGRA8;

    if (auto* context = renderingContext()) {
        bufferOptions = context->adjustImageBufferOptionsForTesting(bufferOptions);
        colorSpace = context->colorSpace();
        pixelFormat = context->pixelFormat();
    }

    return ImageBuffer::create(size(), RenderingPurpose::Canvas, 1, colorSpace, pixelFormat, bufferOptions, scriptExecutionContext()->graphicsClient());
}

bool CanvasBase::shouldInjectNoiseBeforeReadback() const
{
    // Note, every early-return resulting from this check potentially leaks this state. This is a risk that we're accepting right now.
    return !!m_canvasNoiseHashSalt;
}

void CanvasBase::recordLastFillText(const String& text)
{
    if (!shouldInjectNoiseBeforeReadback())
        return;
    m_lastFillText = text;
}

void CanvasBase::addCanvasNeedingPreparationForDisplayOrFlush()
{
    auto* context = renderingContext();
    if (!context)
        return;
    if (context->isInPreparationForDisplayOrFlush())
        return;
    if (auto* document = dynamicDowncast<Document>(scriptExecutionContext()))
        document->addCanvasNeedingPreparationForDisplayOrFlush(*context);
    // FIXME: WorkerGlobalContext does not have prepare phase yet.
}

void CanvasBase::removeCanvasNeedingPreparationForDisplayOrFlush()
{
    auto* context = renderingContext();
    if (!context)
        return;
    if (!context->isInPreparationForDisplayOrFlush())
        return;
    if (auto* document = dynamicDowncast<Document>(scriptExecutionContext()))
        document->removeCanvasNeedingPreparationForDisplayOrFlush(*context);
    // FIXME: WorkerGlobalContext does not have prepare phase yet.
}

bool CanvasBase::postProcessPixelBufferResults(Ref<PixelBuffer>&& pixelBuffer) const
{
    if (m_canvasNoiseHashSalt)
        return m_canvasNoiseInjection.postProcessPixelBufferResults(std::forward<Ref<PixelBuffer>>(pixelBuffer), *m_canvasNoiseHashSalt);
    return false;
}

size_t CanvasBase::activePixelMemory()
{
    return s_activePixelMemory.load();
}

void CanvasBase::resetGraphicsContextState() const
{
    if (m_contextStateSaver) {
        // Reset to the initial graphics context state.
        m_contextStateSaver->restore();
        m_contextStateSaver->save();
    }
}

WebCoreOpaqueRoot root(CanvasBase* canvas)
{
    return WebCoreOpaqueRoot { canvas };
}

} // namespace WebCore
