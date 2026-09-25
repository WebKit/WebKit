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
#include "NinePieceGeometry.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakRef.h>

namespace WebCore {
namespace Style {

static constexpr auto timeToKeepCachedGradients = 3_s;

class GradientImage::CachedGradient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CachedGradient);
public:
    CachedGradient(GradientImage&, FloatSize, Ref<WebCore::Gradient>&&);

    WebCore::Gradient& gradient() const LIFETIME_BOUND { return m_gradient; }
    void puntEvictionTimer() { m_evictionTimer.restart(); }

    RefPtr<ImageBuffer> patternBuffer(FloatSize adjustedSize, unsigned gradientHash, FloatSize scaleFactor, RenderingMode renderingMode) const
    {
        if (!m_patternBuffer || m_patternGradientHash != gradientHash || m_patternAdjustedSize != adjustedSize || m_patternRenderingMode != renderingMode || !areEssentiallyEqual(scaleFactor, m_patternScaleFactor))
            return nullptr;
        return m_patternBuffer;
    }

    void setPatternBuffer(Ref<ImageBuffer>&& buffer, FloatSize adjustedSize, unsigned gradientHash, FloatSize scaleFactor, RenderingMode renderingMode)
    {
        m_patternBuffer = WTF::move(buffer);
        m_patternAdjustedSize = adjustedSize;
        m_patternGradientHash = gradientHash;
        m_patternScaleFactor = scaleFactor;
        m_patternRenderingMode = renderingMode;
    }

private:
    void evictionTimerFired();

    WeakRef<GradientImage> m_owner;
    const FloatSize m_size;
    const Ref<WebCore::Gradient> m_gradient;
    RefPtr<ImageBuffer> m_patternBuffer;
    FloatSize m_patternAdjustedSize;
    unsigned m_patternGradientHash { 0 };
    FloatSize m_patternScaleFactor;
    RenderingMode m_patternRenderingMode { RenderingMode::Unaccelerated };
    DeferrableOneShotTimer m_evictionTimer;
};

inline GradientImage::CachedGradient::CachedGradient(GradientImage& owner, FloatSize size, Ref<WebCore::Gradient>&& gradient)
    : m_owner(owner)
    , m_size(size)
    , m_gradient(WTF::move(gradient))
    , m_evictionTimer(*this, &GradientImage::CachedGradient::evictionTimerFired, timeToKeepCachedGradients)
{
    m_evictionTimer.restart();
}

void GradientImage::CachedGradient::evictionTimerFired()
{
    protect(m_owner.get())->evictCachedGradient(m_size);
}

GradientImage::CachedGradient* GradientImage::cachedGradientForSize(FloatSize size)
{
    if (size.isEmpty())
        return nullptr;

    auto* cached = m_gradients.get(size);
    if (!cached)
        return nullptr;

    cached->puntEvictionTimer();
    return cached;
}

void GradientImage::evictCachedGradient(FloatSize size)
{
    ASSERT(m_gradients.contains(size));
    m_gradients.remove(size);
}

GradientImage::GradientImage(Gradient&& gradient)
    : GeneratedImage { Type::GradientImage }
    , m_gradient { WTF::move(gradient) }
    , m_knownCacheableBarringFilter { stopsAreCacheable(m_gradient) }
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

    bool cacheable = m_knownCacheableBarringFilter && style->appleColorFilter().isNone();
    if (!cacheable)
        return createPlatformGradient(m_gradient, size, style);

    auto& mutableThis = const_cast<GradientImage&>(*this);
    if (auto* existing = mutableThis.cachedGradientForSize(size)) {
        cached = existing;
        return existing->gradient();
    }

    auto gradient = createPlatformGradient(m_gradient, size, style);
    if (size.isEmpty())
        return gradient;

    auto entry = makeUnique<CachedGradient>(mutableThis, size, gradient.copyRef());
    cached = entry.get();
    mutableThis.m_gradients.add(size, WTF::move(entry));
    return gradient;
}

ImageDrawResult GradientImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool isForFirstLine) const
{
    auto size = concreteObjectSize.size();
    if (size.isEmpty())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);
    return drawUsing(context, gradient, concreteObjectSize, destination, source, options);
}

ImageDrawResult GradientImage::drawUsing(GraphicsContext& context, WebCore::Gradient& gradient, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options) const
{
    return drawIntoDestination(context, destination, source, options, [&](GraphicsContext& context) {
        context.fillRect(FloatRect { { }, concreteObjectSize.size() }, gradient);
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult GradientImage::drawAsPattern(GraphicsContext& context, WebCore::Gradient& gradient, CachedGradient* cached, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options) const
{
    auto size = concreteObjectSize.size();
    if (size.isEmpty())
        return ImageDrawResult::DidNothing;

    auto adjustedSize = size;
    auto adjustedTile = tile;
    gradient.adjustParametersForTiledDrawing(adjustedSize, adjustedTile, spacing);

    auto contextCTM = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    double xScale = std::abs(contextCTM.xScale());
    double yScale = std::abs(contextCTM.yScale());
    auto adjustedPatternCTM = patternTransform;
    adjustedPatternCTM.scale(1.0 / xScale, 1.0 / yScale);
    adjustedTile.scale(xScale, yScale);

    auto gradientHash = gradient.hash();
    auto scaleFactor = context.scaleFactor();

    auto renderingMode = context.renderingMode();

    RefPtr buffer = cached ? cached->patternBuffer(adjustedSize, gradientHash, scaleFactor, renderingMode) : nullptr;
    if (!buffer) {
        auto newBuffer = context.createAlignedImageBuffer(adjustedSize);
        if (!newBuffer)
            return ImageDrawResult::DidNothing;

        newBuffer->context().fillRect(FloatRect { { }, adjustedSize }, gradient);

        if (options.drawLuminanceMask() == DrawLuminanceMask::Yes)
            newBuffer->convertToLuminanceMask();

        buffer = WTF::move(newBuffer);

        if (cached)
            cached->setPatternBuffer(*buffer, adjustedSize, gradientHash, scaleFactor, renderingMode);
    }

    context.drawPattern(*buffer, destination, adjustedTile, adjustedPatternCTM, phase, spacing, options);
    return ImageDrawResult::DidDraw;
}

ImageDrawResult GradientImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, concreteObjectSize.size(), isForFirstLine, cached);
    return drawAsPattern(context, gradient, cached, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options);
}

bool GradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return isOpaque(m_gradient, renderer.style());
}

} // namespace Style
} // namespace WebCore
