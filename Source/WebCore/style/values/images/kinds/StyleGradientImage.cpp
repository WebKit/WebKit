/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleGradientImage.h"

#include "CSSGradientValue.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "StyleAppleColorFilter.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

static constexpr auto timeToKeepCachedGradients = 3_s;

class GradientImage::CachedGradient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CachedGradient);
public:
    CachedGradient(GradientImage&, FloatSize, Ref<WebCore::Gradient>&&, const AppleColorFilter&);

    FloatSize size() const { return m_size; }
    WebCore::Gradient& LEGACY_NODELETE gradient() const LIFETIME_BOUND { return m_gradient; }
    const AppleColorFilter& colorFilter() const LIFETIME_BOUND { return m_colorFilter; }

    std::optional<RenderingResourceIdentifier> patternBufferIdentifierForTesting() const
    {
        if (!m_patternBuffer)
            return std::nullopt;
        return m_patternBuffer->renderingResourceIdentifier();
    }

    void puntEvictionTimer() { m_evictionTimer.restart(); }

    RefPtr<ImageBuffer> LEGACY_NODELETE patternBuffer(FloatSize adjustedSize, unsigned gradientHash, FloatSize scaleFactor) const
    {
        if (!m_patternBuffer || m_patternGradientHash != gradientHash || m_patternAdjustedSize != adjustedSize || !areEssentiallyEqual(scaleFactor, m_patternScaleFactor))
            return nullptr;
        return m_patternBuffer;
    }

    void setPatternBuffer(Ref<ImageBuffer>&& buffer, FloatSize adjustedSize, unsigned gradientHash, FloatSize scaleFactor)
    {
        m_patternBuffer = WTF::move(buffer);
        m_patternAdjustedSize = adjustedSize;
        m_patternGradientHash = gradientHash;
        m_patternScaleFactor = scaleFactor;
    }

private:
    void evictionTimerFired();

    WeakRef<GradientImage> m_owner;
    const FloatSize m_size;
    const Ref<WebCore::Gradient> m_gradient;
    const AppleColorFilter m_colorFilter;
    RefPtr<ImageBuffer> m_patternBuffer;
    FloatSize m_patternAdjustedSize;
    unsigned m_patternGradientHash { 0 };
    FloatSize m_patternScaleFactor;
    DeferrableOneShotTimer m_evictionTimer;
};

inline GradientImage::CachedGradient::CachedGradient(GradientImage& owner, FloatSize size, Ref<WebCore::Gradient>&& gradient, const AppleColorFilter& colorFilter)
    : m_owner(owner)
    , m_size(size)
    , m_gradient(WTF::move(gradient))
    , m_colorFilter(colorFilter)
    , m_evictionTimer(*this, &GradientImage::CachedGradient::evictionTimerFired, timeToKeepCachedGradients)
{
    m_evictionTimer.restart();
}

void GradientImage::CachedGradient::evictionTimerFired()
{
    // NOTE: This is essentially a "delete this", the object is no longer valid after this line.
    protect(m_owner.get())->evictCachedGradient(*this);
}

GradientImage::CachedGradient* GradientImage::cachedGradient(FloatSize size, const AppleColorFilter& colorFilter)
{
    if (size.isEmpty())
        return nullptr;

    auto it = m_gradients.find(size);
    if (it == m_gradients.end())
        return nullptr;

    for (auto& cached : it->value) {
        if (cached->colorFilter() == colorFilter) {
            cached->puntEvictionTimer();
            return cached.get();
        }
    }
    return nullptr;
}

void GradientImage::evictCachedGradient(CachedGradient& cached)
{
    auto it = m_gradients.find(cached.size());
    ASSERT(it != m_gradients.end());

    bool removed = it->value.removeFirstMatching([&](auto& entry) {
        return entry.get() == &cached;
    });
    ASSERT_UNUSED(removed, removed);

    if (it->value.isEmpty())
        m_gradients.remove(it);
}

auto GradientImage::cachedGradientsForTesting() const -> Vector<CachedGradientForTesting>
{
    Vector<CachedGradientForTesting> result;
    for (auto& [size, entries] : m_gradients) {
        for (auto& entry : entries) {
            auto patternBufferIdentifier = entry->patternBufferIdentifierForTesting();
            result.append({
                size.width(),
                size.height(),
                !entry->colorFilter().isNone(),
                patternBufferIdentifier ? patternBufferIdentifier->toUInt64() : 0,
            });
        }
    }
    std::ranges::stable_sort(result, [](auto& a, auto& b) {
        return std::pair { a.width, a.height } < std::pair { b.width, b.height };
    });
    return result;
}

GradientImage::GradientImage(Gradient&& gradient)
    : GeneratedImage { Type::GradientImage }
    , m_gradient { WTF::move(gradient) }
    , m_stopsAreCacheable { stopsAreCacheable(m_gradient) }
{
}

GradientImage::~GradientImage() = default;

bool GradientImage::operator==(const Image& other) const
{
    auto* otherGradientImage = dynamicDowncast<GradientImage>(other);
    return otherGradientImage && equals(*otherGradientImage);
}

bool GradientImage::equals(const GradientImage& other) const
{
    return m_gradient == other.m_gradient;
}

Ref<CSSValue> GradientImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSGradientValue::create(toCSS(m_gradient, style));
}

Ref<DeprecatedCSSOMValue> GradientImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool GradientImage::isPending() const
{
    return false;
}

void GradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

Ref<WebCore::Gradient> GradientImage::gradientForSize(const RenderElement& renderer, FloatSize size, bool isForFirstLine, CachedGradient*& cached) const
{
    cached = nullptr;

    CheckedRef style = isForFirstLine ? renderer.firstLineStyle() : renderer.style();

    if (!m_stopsAreCacheable)
        return createPlatformGradient(m_gradient, size, style);

    auto& colorFilter = style->appleColorFilter();
    auto& mutableThis = const_cast<GradientImage&>(*this);
    if (auto* existing = mutableThis.cachedGradient(size, colorFilter)) {
        cached = existing;
        return existing->gradient();
    }

    auto gradient = createPlatformGradient(m_gradient, size, style);
    auto entry = makeUnique<CachedGradient>(mutableThis, size, gradient.copyRef(), colorFilter);
    cached = entry.get();
    mutableThis.m_gradients.add(size, Vector<std::unique_ptr<CachedGradient>, 1> { }).iterator->value.append(WTF::move(entry));
    return gradient;
}

ImageDrawResult GradientImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, bool isForFirstLine) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (size.isEmpty())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);

    return drawIntoDestination(context, destination, source, options, [&](GraphicsContext& context) {
        context.fillRect(FloatRect { { }, size }, gradient);
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult GradientImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, bool isForFirstLine) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (size.isEmpty() || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);

    // Allow the generator to provide visually-equivalent tiling parameters for better performance.
    auto adjustedSize = size;
    auto adjustedTile = tile;
    gradient->adjustParametersForTiledDrawing(adjustedSize, adjustedTile, spacing);

    // Factor in the destination context's scale to generate at the best resolution.
    auto contextCTM = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    double xScale = std::abs(contextCTM.xScale());
    double yScale = std::abs(contextCTM.yScale());
    auto adjustedPatternCTM = patternTransform;
    adjustedPatternCTM.scale(1.0 / xScale, 1.0 / yScale);
    adjustedTile.scale(xScale, yScale);

    auto gradientHash = gradient->hash();
    auto scaleFactor = context.scaleFactor();

    RefPtr buffer = cached ? cached->patternBuffer(adjustedSize, gradientHash, scaleFactor) : nullptr;
    if (!buffer) {
        auto newBuffer = context.createAlignedImageBuffer(adjustedSize);
        if (!newBuffer)
            return ImageDrawResult::DidDraw;

        newBuffer->context().fillRect(FloatRect { { }, adjustedSize }, gradient);

        if (options.drawLuminanceMask() == DrawLuminanceMask::Yes)
            newBuffer->convertToLuminanceMask();

        buffer = newBuffer.copyRef();
        if (cached)
            cached->setPatternBuffer(newBuffer.releaseNonNull(), adjustedSize, gradientHash, scaleFactor);
    }

    context.drawPattern(*buffer, destination, adjustedTile, adjustedPatternCTM, phase, spacing, options.rasterDrawingOptions());
    return ImageDrawResult::DidDraw;
}

bool GradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return isOpaque(m_gradient, renderer.style());
}

bool GradientImage::containsCurrentColor() const
{
    return Style::containsCurrentColor(m_gradient);
}

} // namespace Style
} // namespace WebCore
