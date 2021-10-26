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

#include "CSSCanvasValue.h"
#include "CanvasRenderingContext.h"
#include "Element.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
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

CanvasBase::CanvasBase(IntSize size)
    : m_size(size)
{
}

CanvasBase::~CanvasBase()
{
    ASSERT(m_didNotifyObserversCanvasDestroyed);
    ASSERT(m_observers.isEmpty());
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

void CanvasBase::addObserver(CanvasObserver& observer)
{
    m_observers.add(&observer);

    if (is<CSSCanvasValue::CanvasObserverProxy>(observer))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*this);
}

void CanvasBase::removeObserver(CanvasObserver& observer)
{
    m_observers.remove(&observer);

    if (is<CSSCanvasValue::CanvasObserverProxy>(observer))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*this);
}

void CanvasBase::notifyObserversCanvasChanged(const std::optional<FloatRect>& rect)
{
    for (auto& observer : m_observers)
        observer->canvasChanged(*this, rect);
}

void CanvasBase::notifyObserversCanvasResized()
{
    for (auto& observer : m_observers)
        observer->canvasResized(*this);
}

void CanvasBase::notifyObserversCanvasDestroyed()
{
    ASSERT(!m_didNotifyObserversCanvasDestroyed);

    for (auto& observer : copyToVector(m_observers))
        observer->canvasDestroyed(*this);

    m_observers.clear();

#if ASSERT_ENABLED
    m_didNotifyObserversCanvasDestroyed = true;
#endif
}

HashSet<Element*> CanvasBase::cssCanvasClients() const
{
    HashSet<Element*> cssCanvasClients;
    for (auto& observer : m_observers) {
        if (!is<CSSCanvasValue::CanvasObserverProxy>(observer))
            continue;

        auto clients = downcast<CSSCanvasValue::CanvasObserverProxy>(observer)->ownerValue().clients();
        for (auto& entry : clients) {
            if (RefPtr<Element> element = entry.key->element())
                cssCanvasClients.add(element.get());
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

    if (m_imageBuffer && m_size != m_imageBuffer->logicalSize())
        m_size = m_imageBuffer->logicalSize();

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

}
