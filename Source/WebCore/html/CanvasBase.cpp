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
#include "NoiseInjectionPolicy.h"
#include "RenderElementInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "ScriptTrackingPrivacyCategory.h"
#include "StyleCanvasImage.h"
#include "WebCoreOpaqueRoot.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <atomic>
#include <wtf/Vector.h>
#include <wtf/WeakRandom.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

constexpr InterpolationQuality defaultInterpolationQuality = InterpolationQuality::Low;
static std::optional<size_t> maxCanvasAreaForTesting;

static std::optional<uint64_t> canvasNoiseHashSaltIfNeeded(ScriptExecutionContext& context)
{
    auto policies = context.noiseInjectionPolicies();
    if (policies.contains(NoiseInjectionPolicy::Minimal)
        || (policies.contains(NoiseInjectionPolicy::Enhanced) && context.requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::Canvas)))
        return context.noiseInjectionHashSalt();
    return { };
}

CanvasDisplayBufferObserver::~CanvasDisplayBufferObserver() = default;

CanvasBase::CanvasBase(IntSize size, ScriptExecutionContext& context)
    : m_size { size }
    , m_canvasNoiseHashSalt { canvasNoiseHashSaltIfNeeded(context) }
{
}

CanvasBase::~CanvasBase()
{
    ASSERT(m_didNotifyObserversCanvasDestroyed);
    ASSERT(m_observers.isEmptyIgnoringNullReferences());
    ASSERT(!m_imageBuffer);
    m_canvasNoiseHashSalt = std::nullopt;
}

ImageBuffer* CanvasBase::buffer() const
{
    if (!hasCreatedImageBuffer())
        createImageBuffer();
    return m_imageBuffer.get();
}

RefPtr<ImageBuffer> CanvasBase::makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    if (RefPtr context = renderingContext()) {
        RefPtr buffer = context->surfaceBufferToImageBuffer(CanvasRenderingContext::SurfaceBuffer::DrawingBuffer);
        if (m_canvasNoiseHashSalt && shouldApplyPostProcessingToDirtyRect == ShouldApplyPostProcessingToDirtyRect::Yes)
            m_canvasNoiseInjection.postProcessDirtyCanvasBuffer(buffer.get(), *m_canvasNoiseHashSalt, context->is2d() ? CanvasNoiseInjectionPostProcessArea::DirtyRect : CanvasNoiseInjectionPostProcessArea::FullBuffer);
        return buffer;
    }
    return buffer();
}

size_t CanvasBase::memoryCost() const
{
    // May be called from GC threads.
    return m_imageBufferMemoryCost.load(std::memory_order_relaxed);
}

#if ENABLE(RESOURCE_USAGE)
size_t CanvasBase::externalMemoryCost() const
{
    // For the purposes of Web Inspector, external memory means memory reported as 1) being traceable from JS objects, i.e. GC owned memory
    // 2) not allocated from "Page" category, e.g. from bmalloc.
    return memoryCost();
}
#endif

static inline size_t maxCanvasArea()
{
    if (maxCanvasAreaForTesting)
        return *maxCanvasAreaForTesting;

    // Firefox limits width/height to 32767 pixels, but slows down dramatically before it
    // reaches that limit. We limit by area instead, giving us larger maximum dimensions,
    // in exchange for a smaller maximum canvas size. The maximum canvas size is in device pixels.
#if PLATFORM(IOS_FAMILY)
    return 8192 * 8192;
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
    for (CheckedRef observer : m_observers)
        observer->canvasChanged(*this, rect);
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
    for (CheckedRef observer : m_observers)
        observer->canvasResized(*this);
}

void CanvasBase::notifyObserversCanvasDestroyed()
{
    ASSERT(!m_didNotifyObserversCanvasDestroyed);

    for (CheckedRef observer : std::exchange(m_observers, WeakHashSet<CanvasObserver>()))
        observer->canvasDestroyed(*this);

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
    for (Ref observer : m_displayBufferObservers)
        observer->canvasDisplayBufferPrepared(*this);
}

HashSet<Element*> CanvasBase::cssCanvasClients() const
{
    HashSet<Element*> cssCanvasClients;
    for (CheckedRef observer : m_observers) {
        RefPtr image = dynamicDowncast<StyleCanvasImage>(observer.get());
        if (!image)
            continue;

        for (auto entry : image->clients()) {
            CheckedRef client = entry.key;
            if (RefPtr element = client->element())
                cssCanvasClients.add(element.get());
        }
    }
    return cssCanvasClients;
}

bool CanvasBase::hasActiveInspectorCanvasCallTracer() const
{
    RefPtr context = renderingContext();
    return context && context->hasActiveInspectorCanvasCallTracer();
}

void CanvasBase::setSize(const IntSize& size)
{
    if (size == m_size)
        return;

    m_size = size;

    if (RefPtr context = renderingContext())
        InspectorInstrumentation::didChangeCanvasSize(*context);
}

RefPtr<ImageBuffer> CanvasBase::setImageBuffer(RefPtr<ImageBuffer>&& buffer) const
{
    RefPtr returnBuffer = std::exchange(m_imageBuffer, WTF::move(buffer));
    if (returnBuffer)
        returnBuffer->context().restore();

    IntSize oldSize = m_size;
    size_t oldMemoryCost = m_imageBufferMemoryCost.load(std::memory_order_relaxed);
    size_t newMemoryCost = 0;
    if (RefPtr imageBuffer = m_imageBuffer) {
        m_size = imageBuffer->truncatedLogicalSize();
        newMemoryCost = imageBuffer->memoryCost();
        auto& context = imageBuffer->context();
        context.setShadowsIgnoreTransforms(true);
        context.setImageInterpolationQuality(defaultInterpolationQuality);
        context.setStrokeThickness(1);
        // Save the initial state, so that 2D rendering context reset can restore it.
        // Will be made more localized when the ImageBuffer is moved to 2D rendering context.
        context.save();
    }
    m_imageBufferMemoryCost.store(newMemoryCost, std::memory_order_relaxed);
    if (newMemoryCost) {
        if (RefPtr scriptExecutionContext = this->scriptExecutionContext()) {
            JSC::JSLockHolder lock(scriptExecutionContext->vm());
            scriptExecutionContext->vm().heap.reportExtraMemoryAllocated(static_cast<JSCell*>(nullptr), newMemoryCost);
        }
    }
    if (RefPtr context = renderingContext()) {
        if (oldSize != m_size)
            InspectorInstrumentation::didChangeCanvasSize(*context);
        if (oldMemoryCost != newMemoryCost)
            InspectorInstrumentation::didChangeCanvasMemory(*context);
    }
    return returnBuffer;
}

bool CanvasBase::shouldAccelerate(const IntSize& size) const
{
    return shouldAccelerate(size.unclampedArea());
}

bool CanvasBase::shouldAccelerate(uint64_t area) const
{
    RefPtr scriptExecutionContext = this->scriptExecutionContext();
#if USE(CA) || USE(SKIA)
    if (!scriptExecutionContext->settingsValues().canvasUsesAcceleratedDrawing)
        return false;
    if (area < scriptExecutionContext->settingsValues().minimumAccelerated2DContextArea)
        return false;
#if PLATFORM(GTK)
    if (!scriptExecutionContext->settingsValues().acceleratedCompositingEnabled)
        return false;
#endif
    return true;
#else
    UNUSED_PARAM(area);
    return false;
#endif
}

RefPtr<ImageBuffer> CanvasBase::allocateImageBuffer() const
{
    uint64_t area = size().unclampedArea();
    RefPtr scriptExecutionContext = this->scriptExecutionContext();
    if (!area)
        return nullptr;
    if (area > maxCanvasArea()) {
        auto message = makeString("Canvas area exceeds the maximum limit (width * height > "_s, maxCanvasArea(), ")."_s);
        scriptExecutionContext->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
        return nullptr;
    }

    RefPtr context = renderingContext();
    bool willReadFrequently = context ? context->willReadFrequently() : false;

    RenderingMode renderingMode;
    if (auto renderingModeForTesting = context ? context->renderingModeForTesting() : std::nullopt)
        renderingMode = *renderingModeForTesting;
    else
        renderingMode = !willReadFrequently && shouldAccelerate(area) ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

    auto colorSpace = context ? context->colorSpace() : DestinationColorSpace::SRGB();
    auto pixelFormat = context ? context->pixelFormat() : PixelFormat::BGRA8;

    return ImageBuffer::create(size(), renderingMode, RenderingPurpose::Canvas, 1, colorSpace, pixelFormat, scriptExecutionContext->graphicsClient());
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
    RefPtr context = renderingContext();
    if (!context)
        return;
    if (context->isInPreparationForDisplayOrFlush())
        return;
    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext()))
        document->addCanvasNeedingPreparationForDisplayOrFlush(*context);
    // FIXME: WorkerGlobalContext does not have prepare phase yet.
}

void CanvasBase::removeCanvasNeedingPreparationForDisplayOrFlush()
{
    RefPtr context = renderingContext();
    if (!context)
        return;
    if (!context->isInPreparationForDisplayOrFlush())
        return;
    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext()))
        document->removeCanvasNeedingPreparationForDisplayOrFlush(*context);
    // FIXME: WorkerGlobalContext does not have prepare phase yet.
}

bool CanvasBase::postProcessPixelBufferResults(Ref<PixelBuffer>&& pixelBuffer) const
{
    if (m_canvasNoiseHashSalt)
        return m_canvasNoiseInjection.postProcessPixelBufferResults(std::forward<Ref<PixelBuffer>>(pixelBuffer), *m_canvasNoiseHashSalt);
    return false;
}

RefPtr<ImageBuffer> CanvasBase::createImageForNoiseInjection() const
{
    RefPtr context = canvasBaseScriptExecutionContext();
    if (!context)
        return { };

    auto seed = static_cast<unsigned>(context->noiseInjectionHashSalt().value_or(0));
    auto buffer = ImageBuffer::create(size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return { };

    auto componentValues = WeakRandom { seed }.getUint32();
    auto fillColor = SRGBA<uint8_t> {
        static_cast<uint8_t>((componentValues >> 24) & 0xFF),
        static_cast<uint8_t>((componentValues >> 16) & 0xFF),
        static_cast<uint8_t>((componentValues >> 8) & 0xFF),
        static_cast<uint8_t>(componentValues & 0xFF),
    };
    buffer->context().setFillColor(fillColor);
    buffer->context().fillRect({ IntPoint { }, size() });
    return buffer;
}

WebCoreOpaqueRoot root(CanvasBase* canvas)
{
    return WebCoreOpaqueRoot { canvas };
}

RefPtr<ScriptExecutionContext> CanvasBase::protectedCanvasBaseScriptExecutionContext() const
{
    return canvasBaseScriptExecutionContext();
}

RefPtr<ScriptExecutionContext> CanvasBase::protectedScriptExecutionContext() const
{
    return scriptExecutionContext();
}

} // namespace WebCore
