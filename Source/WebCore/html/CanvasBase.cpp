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

#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "Document.h"
#include "Element.h"
#include "FloatRect.h"
#include "GraphicsClient.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include "StyleCanvasImage.h"
#include "WebCoreOpaqueRoot.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <atomic>
#include <wtf/Vector.h>

static std::atomic<size_t> s_activePixelMemory { 0 };

namespace WebCore {

#if USE(CG)
// FIXME: It seems strange that the default quality is not the one that is literally named "default".
// Should fix names to make this easier to understand, or write an excellent comment here explaining why not.
const InterpolationQuality defaultInterpolationQuality = InterpolationQuality::Low;
#else
const InterpolationQuality defaultInterpolationQuality = InterpolationQuality::Default;
#endif

static std::optional<size_t> maxCanvasAreaForTesting;
static std::optional<size_t> maxActivePixelMemoryForTesting;

CanvasBase::CanvasBase(IntSize size)
    : m_size(size)
{
}

CanvasBase::~CanvasBase()
{
    ASSERT(m_didNotifyObserversCanvasDestroyed);
    ASSERT(m_observers.computesEmpty());
    ASSERT(!m_imageBuffer);
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

void CanvasBase::makeRenderingResultsAvailable()
{
    if (auto* context = renderingContext())
        context->paintRenderingResultsToCanvas();
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

size_t CanvasBase::maxActivePixelMemory()
{
    if (maxActivePixelMemoryForTesting)
        return *maxActivePixelMemoryForTesting;

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

void CanvasBase::setMaxPixelMemoryForTesting(std::optional<size_t> size)
{
    maxActivePixelMemoryForTesting = size;
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

void CanvasBase::notifyObserversCanvasChanged(const std::optional<FloatRect>& rect)
{
    for (auto& observer : m_observers)
        observer.canvasChanged(*this, rect);
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
        if (!is<StyleCanvasImage>(observer))
            continue;

        for (auto& client : downcast<StyleCanvasImage>(observer).clients().values()) {
            if (auto element = client->element())
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

RefPtr<ImageBuffer> CanvasBase::setImageBuffer(RefPtr<ImageBuffer>&& buffer) const
{
    RefPtr<ImageBuffer> returnBuffer;
    {
        Locker locker { m_imageBufferAssignmentLock };
        m_contextStateSaver = nullptr;
        returnBuffer = std::exchange(m_imageBuffer, WTFMove(buffer));
    }

    if (m_imageBuffer && m_size != m_imageBuffer->truncatedLogicalSize())
        m_size = m_imageBuffer->truncatedLogicalSize();

    size_t previousMemoryCost = m_imageBufferCost;
    m_imageBufferCost = memoryCost();
    s_activePixelMemory += m_imageBufferCost - previousMemoryCost;

    auto* context = renderingContext();
    if (context && m_imageBuffer && previousMemoryCost != m_imageBufferCost)
        InspectorInstrumentation::didChangeCanvasMemory(*context);

    if (m_imageBuffer) {
        m_imageBuffer->context().setShadowsIgnoreTransforms(true);
        m_imageBuffer->context().setImageInterpolationQuality(defaultInterpolationQuality);
        m_imageBuffer->context().setStrokeThickness(1);
        m_contextStateSaver = makeUnique<GraphicsContextStateSaver>(m_imageBuffer->context());

        JSC::JSLockHolder lock(scriptExecutionContext()->vm());
        scriptExecutionContext()->vm().heap.reportExtraMemoryAllocated(memoryCost());
    }

    return returnBuffer;
}

GraphicsClient* CanvasBase::graphicsClient() const
{
    if (scriptExecutionContext()->isDocument() && downcast<Document>(scriptExecutionContext())->page())
        return &downcast<Document>(scriptExecutionContext())->page()->chrome();
    if (is<WorkerGlobalScope>(scriptExecutionContext()))
        return downcast<WorkerGlobalScope>(scriptExecutionContext())->workerClient();

    return nullptr;
}

bool CanvasBase::shouldAccelerate(const IntSize& size) const
{
    auto checkedArea = size.area<RecordOverflow>();
    if (checkedArea.hasOverflowed())
        return false;

    return shouldAccelerate(checkedArea.value());
}

bool CanvasBase::shouldAccelerate(unsigned area) const
{
    if (area > scriptExecutionContext()->settingsValues().maximumAccelerated2dCanvasSize)
        return false;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    return scriptExecutionContext()->settingsValues().canvasUsesAcceleratedDrawing;
#else
    return false;
#endif
}

void CanvasBase::createImageBuffer(bool usesDisplayListDrawing, bool avoidBackendSizeCheckForTesting) const
{
    auto checkedArea = size().area<RecordOverflow>();

    if (checkedArea.hasOverflowed() || checkedArea > maxCanvasArea()) {
        auto message = makeString("Canvas area exceeds the maximum limit (width * height > ", maxCanvasArea(), ").");
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
        return;
    }

    // Make sure we don't use more pixel memory than the system can support.
    auto checkedRequestedPixelMemory = (4 * checkedArea) + activePixelMemory();
    if (checkedRequestedPixelMemory.hasOverflowed() || checkedRequestedPixelMemory > maxActivePixelMemory()) {
        auto message = makeString("Total canvas memory use exceeds the maximum limit (", maxActivePixelMemory() / 1024 / 1024, " MB).");
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
        return;
    }

    unsigned area = checkedArea.value();
    if (!area)
        return;

    OptionSet<ImageBufferOptions> bufferOptions;
    if (shouldAccelerate(area))
        bufferOptions.add(ImageBufferOptions::Accelerated);
    // FIXME: Add a new setting for DisplayList drawing on canvas.
    if (usesDisplayListDrawing || scriptExecutionContext()->settingsValues().displayListDrawingEnabled)
        bufferOptions.add(ImageBufferOptions::UseDisplayList);

    auto [colorSpace, pixelFormat] = [&] {
        if (renderingContext())
            return std::pair { renderingContext()->colorSpace(), renderingContext()->pixelFormat() };
        return std::pair { DestinationColorSpace::SRGB(), PixelFormat::BGRA8 };
    }();
    ImageBufferCreationContext context = { };
    context.graphicsClient = graphicsClient();
    context.avoidIOSurfaceSizeCheckInWebProcessForTesting = avoidBackendSizeCheckForTesting;
    setImageBuffer(ImageBuffer::create(size(), RenderingPurpose::Canvas, 1, colorSpace, pixelFormat, bufferOptions, context));
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
