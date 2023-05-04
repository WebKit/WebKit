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
    ASSERT(m_observers.isEmptyIgnoringNullReferences());
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
    if (auto* context = renderingContext()) {
        context->paintRenderingResultsToCanvas();
        postProcessDirtyCanvasBuffer();
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

void CanvasBase::didDraw(const std::optional<FloatRect>& rect, ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    // FIXME: We should exclude rects with ShouldApplyPostProcessingToDirtyRect::No
    if (shouldInjectNoiseBeforeReadback() && shouldApplyPostProcessingToDirtyRect == ShouldApplyPostProcessingToDirtyRect::Yes && rect)
        m_postProcessDirtyRect.unite(*rect);
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

        for (auto entry : downcast<StyleCanvasImage>(observer).clients()) {
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

RefPtr<ImageBuffer> CanvasBase::allocateImageBuffer(bool usesDisplayListDrawing, bool avoidBackendSizeCheckForTesting) const
{
    auto checkedArea = size().area<RecordOverflow>();

    if (checkedArea.hasOverflowed() || checkedArea > maxCanvasArea()) {
        auto message = makeString("Canvas area exceeds the maximum limit (width * height > ", maxCanvasArea(), ").");
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
        return nullptr;
    }

    // Make sure we don't use more pixel memory than the system can support.
    auto checkedRequestedPixelMemory = (4 * checkedArea) + activePixelMemory();
    if (checkedRequestedPixelMemory.hasOverflowed() || checkedRequestedPixelMemory > maxActivePixelMemory()) {
        auto message = makeString("Total canvas memory use exceeds the maximum limit (", maxActivePixelMemory() / 1024 / 1024, " MB).");
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
        return nullptr;
    }

    unsigned area = checkedArea.value();
    if (!area)
        return nullptr;

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
    return ImageBuffer::create(size(), RenderingPurpose::Canvas, 1, colorSpace, pixelFormat, bufferOptions, context);
}

bool CanvasBase::shouldInjectNoiseBeforeReadback() const
{
    // Note, every early-return resulting from this check potentially leaks this state. This is a risk that we're accepting right now.
    return scriptExecutionContext() && scriptExecutionContext()->noiseInjectionHashSalt();
}

void CanvasBase::postProcessDirtyCanvasBuffer() const
{
    if (!shouldInjectNoiseBeforeReadback())
        return;

    if (m_postProcessDirtyRect.isEmpty())
        return;

    ImageBuffer* imageBuffer = buffer();
    if (!imageBuffer)
        return;

    auto imageRect = enclosingIntRect(m_postProcessDirtyRect);
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, imageBuffer->colorSpace() };
    auto pixelBuffer = imageBuffer->getPixelBuffer(format, imageRect);
    if (!is<ByteArrayPixelBuffer>(pixelBuffer))
        return;

    if (postProcessPixelBufferResults(*pixelBuffer, { })) {
        imageBuffer->putPixelBuffer(*pixelBuffer, imageRect, { 0, 0 });
        m_postProcessDirtyRect = { };
    }
}

bool CanvasBase::postProcessPixelBufferResults(Ref<PixelBuffer>&& pixelBuffer, const HashSet<uint32_t>& suppliedColors) const
{
    if (!shouldInjectNoiseBeforeReadback())
        return false;

    ASSERT(pixelBuffer->format().pixelFormat == PixelFormat::RGBA8);

    constexpr auto contextString { "Canvas2DContextString"_s };
    unsigned salt = computeHash<String, uint64_t>(contextString, *scriptExecutionContext()->noiseInjectionHashSalt());
    constexpr int bytesPerPixel = 4;
    auto* bytes = pixelBuffer->bytes();
    HashMap<uint32_t, uint32_t> pixelColorMap;
    bool wasPixelBufferModified { false };

    for (size_t i = 0; i < pixelBuffer->sizeInBytes(); i += bytesPerPixel) {
        auto& redChannel = bytes[i];
        auto& greenChannel = bytes[i + 1];
        auto& blueChannel = bytes[i + 2];
        auto& alphaChannel = bytes[i + 3];
        bool isBlack { !redChannel && !greenChannel && !blueChannel };

        uint32_t pixel = (redChannel << 24) | (greenChannel << 16) | (blueChannel << 8) | alphaChannel;
        // FIXME: Consider isEquivalentColor comparision instead of exact match
        if (!pixel || !alphaChannel || !suppliedColors.isValidValue(pixel) || suppliedColors.contains(pixel))
            continue;

        if (auto color = pixelColorMap.get(pixel)) {
            alphaChannel = static_cast<uint8_t>(color);
            blueChannel = static_cast<uint8_t>(color >> 8);
            greenChannel = static_cast<uint8_t>(color >> 16);
            redChannel = static_cast<uint8_t>(color >> 24);
            continue;
        }

        const uint64_t pixelHash = computeHash(salt, redChannel, greenChannel, blueChannel, alphaChannel);
        // +/- ~13 is roughly 5% of the 255 max value.
        const auto clampedFivePercent = static_cast<uint32_t>(((pixelHash * 26) / std::numeric_limits<uint32_t>::max()) - 13);

        const auto clampedColorComponentOffset = [](int colorComponentOffset, int originalComponentValue) {
            if (colorComponentOffset + originalComponentValue > std::numeric_limits<uint8_t>::max())
                return std::numeric_limits<uint8_t>::max() - originalComponentValue;
            if (colorComponentOffset + originalComponentValue < 0)
                return -originalComponentValue;
            return colorComponentOffset;
        };

        // If alpha is non-zero and the color channels are zero, then only tweak the alpha channel's value;
        if (isBlack)
            alphaChannel += clampedColorComponentOffset(clampedFivePercent, alphaChannel);
        else {
            // If alpha and any of the color channels are non-zero, then tweak all of the channels;
            redChannel += clampedColorComponentOffset(clampedFivePercent, redChannel);
            greenChannel += clampedColorComponentOffset(clampedFivePercent, greenChannel);
            blueChannel += clampedColorComponentOffset(clampedFivePercent, blueChannel);
            alphaChannel += clampedColorComponentOffset(clampedFivePercent, alphaChannel);
        }
        uint32_t modifiedPixel = (redChannel << 24) | (greenChannel << 16) | (blueChannel << 8) | alphaChannel;
        pixelColorMap.set(pixel, modifiedPixel);
        wasPixelBufferModified = true;
    }
    return wasPixelBufferModified;
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
