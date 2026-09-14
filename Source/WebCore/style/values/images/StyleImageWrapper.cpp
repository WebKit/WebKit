/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleImageWrapper.h"

#include "AnimationUtilities.h"
#include "CSSImageWrapper.h"
#include "CSSValue.h"
#include "ColorBlending.h"
#include "DeprecatedCSSOMValue.h"
#include "StyleBuilderState.h"
#include "StyleCachedImage.h"
#include "StyleColorResolver.h"
#include "StyleCrossfadeImage.h"
#include "StyleFilterImage.h"
#include "StyleGradientImage.h"
#include "StyleInvalidImage.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<ImageWrapper>::operator()(const ImageWrapper& value, const Style::ComputedStyle& style) -> CSS::ImageWrapper
{
    return { protect(value.value)->computedStyleValue(style) };
}

auto ToStyle<CSS::ImageWrapper>::operator()(const CSS::ImageWrapper& value, const BuilderState& state) -> ImageWrapper
{
    if (RefPtr styleImage = state.createStyleImage(value.value))
        return ImageWrapper { styleImage.releaseNonNull() };
    return ImageWrapper { InvalidImage::create() };
}

Ref<CSSValue> CSSValueCreation<ImageWrapper>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const ImageWrapper& value)
{
    return protect(value.value)->computedStyleValue(style);
}

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<ImageWrapper>::operator()(CSSValuePool& pool, const Style::ComputedStyle& style, CSSStyleDeclaration& owner, const ImageWrapper& value)
{
    return protect(value.value)->computedStyleDeprecatedCSSOMValue(pool, style, owner);
}

// MARK: - Serialization

void Serialize<ImageWrapper>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const Style::ComputedStyle& style, const ImageWrapper& value)
{
    builder.append(protect(value.value)->computedStyleValue(style)->cssText(context));
}

// MARK: - Blending

static ImageWrapper crossfadeBlend(Ref<CachedImage>&& fromImage, Ref<CachedImage>&& toImage, const BlendingContext& context)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.

    if (!context.progress)
        return ImageWrapper { WTF::move(fromImage) };
    if (context.progress == 1)
        return ImageWrapper { WTF::move(toImage) };
    if (!fromImage->cachedImage() || !toImage->cachedImage())
        return ImageWrapper { WTF::move(toImage) };
    return ImageWrapper { CrossfadeImage::create(WTF::move(fromImage), WTF::move(toImage), context.progress, false) };
}

static ImageWrapper filterBlend(RefPtr<Image> inputImage, const Filter& from, const Filter& to, const Style::ComputedStyle& fromStyle, const Style::ComputedStyle& toStyle, const BlendingContext& context)
{
    return ImageWrapper { FilterImage::create(WTF::move(inputImage), blend(from, to, fromStyle, toStyle, context)) };
}

// MARK: - Gradient Blending Helpers

static std::optional<LinearGradient::GradientLine> blendLinearGradientLine(
    const LinearGradient::GradientLine& from,
    const LinearGradient::GradientLine& to,
    const BlendingContext& context)
{
    if (from.index() != to.index())
        return std::nullopt;

    return WTF::visit(WTF::makeVisitor(
        [&](const Angle<>& a, const Angle<>& b) -> std::optional<LinearGradient::GradientLine> {
            return LinearGradient::GradientLine { Blending<Angle<>> { }.blend(a, b, context) };
        },
        [](const Horizontal& a, const Horizontal& b) -> std::optional<LinearGradient::GradientLine> {
            return a == b ? std::optional<LinearGradient::GradientLine> { a } : std::nullopt;
        },
        [](const Vertical& a, const Vertical& b) -> std::optional<LinearGradient::GradientLine> {
            return a == b ? std::optional<LinearGradient::GradientLine> { a } : std::nullopt;
        },
        [](const SpaceSeparatedTuple<Horizontal, Vertical>& a, const SpaceSeparatedTuple<Horizontal, Vertical>& b) -> std::optional<LinearGradient::GradientLine> {
            return a == b ? std::optional<LinearGradient::GradientLine> { a } : std::nullopt;
        },
        [](const auto&, const auto&) -> std::optional<LinearGradient::GradientLine> {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    ), from, to);
}

static std::optional<GradientLinearColorStop> blendLinearColorStop(
    const GradientLinearColorStop& from,
    const GradientLinearColorStop& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (static_cast<bool>(from.color) != static_cast<bool>(to.color))
        return std::nullopt;
    if (from.position.has_value() != to.position.has_value())
        return std::nullopt;

    GradientLinearColorStopColor blendedColor;
    if (from.color) {
        ColorResolver fromResolver { fromStyle };
        ColorResolver toResolver { toStyle };
        WebCore::Color blended = WebCore::blend(
            fromResolver.colorResolvingCurrentColor(*from.color),
            toResolver.colorResolvingCurrentColor(*to.color),
            context);
        blendedColor = Color { WTF::move(blended) };
    }

    GradientLinearColorStopPosition blendedPosition;
    if (from.position)
        blendedPosition = Blending<LengthPercentage<CSS::AllLayoutUnitClamped>> { }.blend(*from.position, *to.position, context);

    return GradientLinearColorStop { WTF::move(blendedColor), WTF::move(blendedPosition) };
}

static std::optional<GradientLinearColorStopList> blendLinearColorStopList(
    const GradientLinearColorStopList& from,
    const GradientLinearColorStopList& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (from.value.size() != to.value.size())
        return std::nullopt;

    GradientLinearColorStopList::Container blendedStops;
    blendedStops.reserveInitialCapacity(from.value.size());
    for (size_t i = 0; i < from.value.size(); ++i) {
        auto stop = blendLinearColorStop(from.value[i], to.value[i], fromStyle, toStyle, context);
        if (!stop)
            return std::nullopt;
        blendedStops.append(WTF::move(*stop));
    }
    return GradientLinearColorStopList { WTF::move(blendedStops) };
}

static std::optional<GradientAngularColorStop> blendAngularColorStop(
    const GradientAngularColorStop& from,
    const GradientAngularColorStop& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (static_cast<bool>(from.color) != static_cast<bool>(to.color))
        return std::nullopt;
    if (from.position.has_value() != to.position.has_value())
        return std::nullopt;

    GradientAngularColorStopColor blendedColor;
    if (from.color) {
        ColorResolver fromResolver { fromStyle };
        ColorResolver toResolver { toStyle };
        WebCore::Color blended = WebCore::blend(
            fromResolver.colorResolvingCurrentColor(*from.color),
            toResolver.colorResolvingCurrentColor(*to.color),
            context);
        blendedColor = Color { WTF::move(blended) };
    }

    GradientAngularColorStopPosition blendedPosition;
    if (from.position)
        blendedPosition = Blending<AnglePercentage<>> { }.blend(*from.position, *to.position, context);

    return GradientAngularColorStop { WTF::move(blendedColor), WTF::move(blendedPosition) };
}

static std::optional<GradientAngularColorStopList> blendAngularColorStopList(
    const GradientAngularColorStopList& from,
    const GradientAngularColorStopList& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (from.value.size() != to.value.size())
        return std::nullopt;

    GradientAngularColorStopList::Container blendedStops;
    blendedStops.reserveInitialCapacity(from.value.size());
    for (size_t i = 0; i < from.value.size(); ++i) {
        auto stop = blendAngularColorStop(from.value[i], to.value[i], fromStyle, toStyle, context);
        if (!stop)
            return std::nullopt;
        blendedStops.append(WTF::move(*stop));
    }
    return GradientAngularColorStopList { WTF::move(blendedStops) };
}

static Position blendGradientPosition(const Position& from, const Position& to, const BlendingContext& context)
{
    return Position {
        Position::X { Blending<LengthPercentage<>> { }.blend(from.x.value, to.x.value, context) },
        Position::Y { Blending<LengthPercentage<>> { }.blend(from.y.value, to.y.value, context) }
    };
}

static std::optional<RadialGradient::Ellipse> blendRadialGradientEllipse(
    const RadialGradient::Ellipse& from,
    const RadialGradient::Ellipse& to,
    const BlendingContext& context)
{
    if (from.size.index() != to.size.index())
        return std::nullopt;

    auto blendedSize = WTF::visit(WTF::makeVisitor(
        [&](const RadialGradient::Ellipse::Size& a, const RadialGradient::Ellipse::Size& b) -> std::optional<Variant<RadialGradient::Ellipse::Size, RadialGradient::Extent>> {
            return Variant<RadialGradient::Ellipse::Size, RadialGradient::Extent> {
                RadialGradient::Ellipse::Size {
                    Blending<LengthPercentage<CSS::Nonnegative>> { }.blend(a.value[0], b.value[0], context),
                    Blending<LengthPercentage<CSS::Nonnegative>> { }.blend(a.value[1], b.value[1], context)
                }
            };
        },
        [](const RadialGradient::Extent& a, const RadialGradient::Extent& b) -> std::optional<Variant<RadialGradient::Ellipse::Size, RadialGradient::Extent>> {
            return a == b ? std::optional<Variant<RadialGradient::Ellipse::Size, RadialGradient::Extent>> { a } : std::nullopt;
        },
        [](const auto&, const auto&) -> std::optional<Variant<RadialGradient::Ellipse::Size, RadialGradient::Extent>> {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    ), from.size, to.size);
    if (!blendedSize)
        return std::nullopt;

    if (from.position.has_value() != to.position.has_value())
        return std::nullopt;

    std::optional<Position> blendedPosition;
    if (from.position)
        blendedPosition = blendGradientPosition(*from.position, *to.position, context);

    return RadialGradient::Ellipse { WTF::move(*blendedSize), WTF::move(blendedPosition) };
}

static std::optional<RadialGradient::Circle> blendRadialGradientCircle(
    const RadialGradient::Circle& from,
    const RadialGradient::Circle& to,
    const BlendingContext& context)
{
    if (from.size.index() != to.size.index())
        return std::nullopt;

    auto blendedSize = WTF::visit(WTF::makeVisitor(
        [&](const RadialGradient::Circle::Length& a, const RadialGradient::Circle::Length& b) -> std::optional<Variant<RadialGradient::Circle::Length, RadialGradient::Extent>> {
            return Variant<RadialGradient::Circle::Length, RadialGradient::Extent> { Blending<RadialGradient::Circle::Length> { }.blend(a, b, context) };
        },
        [](const RadialGradient::Extent& a, const RadialGradient::Extent& b) -> std::optional<Variant<RadialGradient::Circle::Length, RadialGradient::Extent>> {
            return a == b ? std::optional<Variant<RadialGradient::Circle::Length, RadialGradient::Extent>> { a } : std::nullopt;
        },
        [](const auto&, const auto&) -> std::optional<Variant<RadialGradient::Circle::Length, RadialGradient::Extent>> {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    ), from.size, to.size);
    if (!blendedSize)
        return std::nullopt;

    if (from.position.has_value() != to.position.has_value())
        return std::nullopt;

    std::optional<Position> blendedPosition;
    if (from.position)
        blendedPosition = blendGradientPosition(*from.position, *to.position, context);

    return RadialGradient::Circle { WTF::move(*blendedSize), WTF::move(blendedPosition) };
}

static std::optional<RadialGradient::GradientBox> blendRadialGradientBox(
    const RadialGradient::GradientBox& from,
    const RadialGradient::GradientBox& to,
    const BlendingContext& context)
{
    if (from.index() != to.index())
        return std::nullopt;

    return WTF::visit(WTF::makeVisitor(
        [&](const RadialGradient::Ellipse& a, const RadialGradient::Ellipse& b) -> std::optional<RadialGradient::GradientBox> {
            if (auto blended = blendRadialGradientEllipse(a, b, context))
                return RadialGradient::GradientBox { WTF::move(*blended) };
            return std::nullopt;
        },
        [&](const RadialGradient::Circle& a, const RadialGradient::Circle& b) -> std::optional<RadialGradient::GradientBox> {
            if (auto blended = blendRadialGradientCircle(a, b, context))
                return RadialGradient::GradientBox { WTF::move(*blended) };
            return std::nullopt;
        },
        [](const auto&, const auto&) -> std::optional<RadialGradient::GradientBox> {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    ), from, to);
}

static std::optional<ConicGradient::GradientBox> blendConicGradientBox(
    const ConicGradient::GradientBox& from,
    const ConicGradient::GradientBox& to,
    const BlendingContext& context)
{
    if (from.angle.has_value() != to.angle.has_value())
        return std::nullopt;
    if (from.position.has_value() != to.position.has_value())
        return std::nullopt;

    std::optional<Angle<>> blendedAngle;
    if (from.angle)
        blendedAngle = Blending<Angle<>> { }.blend(*from.angle, *to.angle, context);

    std::optional<Position> blendedPosition;
    if (from.position)
        blendedPosition = blendGradientPosition(*from.position, *to.position, context);

    return ConicGradient::GradientBox { WTF::move(blendedAngle), WTF::move(blendedPosition) };
}

static std::optional<LinearGradient> blendGradientValue(
    const LinearGradient& from,
    const LinearGradient& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (from.colorInterpolationMethod != to.colorInterpolationMethod)
        return std::nullopt;

    auto blendedLine = blendLinearGradientLine(from.gradientLine, to.gradientLine, context);
    if (!blendedLine)
        return std::nullopt;

    auto blendedStops = blendLinearColorStopList(from.stops, to.stops, fromStyle, toStyle, context);
    if (!blendedStops)
        return std::nullopt;

    return LinearGradient { from.colorInterpolationMethod, WTF::move(*blendedLine), WTF::move(*blendedStops) };
}

static std::optional<RadialGradient> blendGradientValue(
    const RadialGradient& from,
    const RadialGradient& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (from.colorInterpolationMethod != to.colorInterpolationMethod)
        return std::nullopt;

    auto blendedBox = blendRadialGradientBox(from.gradientBox, to.gradientBox, context);
    if (!blendedBox)
        return std::nullopt;

    auto blendedStops = blendLinearColorStopList(from.stops, to.stops, fromStyle, toStyle, context);
    if (!blendedStops)
        return std::nullopt;

    return RadialGradient { from.colorInterpolationMethod, WTF::move(*blendedBox), WTF::move(*blendedStops) };
}

static std::optional<ConicGradient> blendGradientValue(
    const ConicGradient& from,
    const ConicGradient& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    if (from.colorInterpolationMethod != to.colorInterpolationMethod)
        return std::nullopt;

    auto blendedBox = blendConicGradientBox(from.gradientBox, to.gradientBox, context);
    if (!blendedBox)
        return std::nullopt;

    auto blendedStops = blendAngularColorStopList(from.stops, to.stops, fromStyle, toStyle, context);
    if (!blendedStops)
        return std::nullopt;

    return ConicGradient { from.colorInterpolationMethod, WTF::move(*blendedBox), WTF::move(*blendedStops) };
}

// Prefixed and deprecated gradient syntaxes are not supported for interpolation; fall back to cross-fade.
template<typename T>
static std::optional<T> blendGradientValue(const T&, const T&, const Style::ComputedStyle&, const Style::ComputedStyle&, const BlendingContext&)
{
    return std::nullopt;
}

static std::optional<ImageWrapper> gradientBlend(
    const GradientImage& from,
    const GradientImage& to,
    const Style::ComputedStyle& fromStyle,
    const Style::ComputedStyle& toStyle,
    const BlendingContext& context)
{
    const auto& fromGradient = from.gradient();
    const auto& toGradient = to.gradient();

    if (fromGradient.index() != toGradient.index())
        return std::nullopt;

    CheckedRef checkedFromStyle { fromStyle };
    CheckedRef checkedToStyle { toStyle };

    // FIXME: Ideally we'd be able to mark the makeVisitor() lambdas as NOESCAPE to avoid having to suppress.
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE return WTF::visit(WTF::makeVisitor(
        [checkedFromStyle, checkedToStyle, &context]<CSSValueID Name, typename T>(const FunctionNotation<Name, T>& fromG, const FunctionNotation<Name, T>& toG) -> std::optional<ImageWrapper> {
            if (auto result = blendGradientValue(*fromG, *toG, checkedFromStyle.get(), checkedToStyle.get(), context))
                return ImageWrapper { GradientImage::create(Gradient { FunctionNotation<Name, T> { WTF::move(*result) } }) };
            return std::nullopt;
        },
        [](const auto&, const auto&) -> std::optional<ImageWrapper> {
            return std::nullopt;
        }
    ), fromGradient, toGradient);
}

auto Blending<ImageWrapper>::blend(const ImageWrapper& a, const ImageWrapper& b, const Style::ComputedStyle& aStyle, const Style::ComputedStyle& bStyle, const BlendingContext& context) -> ImageWrapper
{
    if (!context.progress)
        return a;
    if (context.progress == 1.0)
        return b;

    Ref aImage = a.value;
    Ref bImage = b.value;

    RefPtr aSelectedUnchecked = aImage->selectedImage();
    RefPtr bSelectedUnchecked = bImage->selectedImage();

    if (!aSelectedUnchecked || !bSelectedUnchecked) {
        if (aSelectedUnchecked)
            return ImageWrapper { aSelectedUnchecked.releaseNonNull() };
        if (bSelectedUnchecked)
            return ImageWrapper { bSelectedUnchecked.releaseNonNull() };
        return context.progress > 0.5 ? b : a;
    }

    Ref aSelected = aSelectedUnchecked.releaseNonNull();
    Ref bSelected = bSelectedUnchecked.releaseNonNull();

    // Interpolation between two generated images. Cross fade for all other cases.
    if (auto [aFilter, bFilter] = std::tuple { dynamicDowncast<FilterImage>(aSelected), dynamicDowncast<FilterImage>(bSelected) }; aFilter && bFilter) {
        // Interpolation of generated images is only possible if the input images are equal.
        // Otherwise fall back to cross fade animation.
        if (aFilter->equalInputImages(*bFilter) && is<CachedImage>(aFilter->inputImage()))
            return filterBlend(aFilter->inputImage(), aFilter->filter(), bFilter->filter(), aStyle, bStyle, context);
    } else if (auto [aCrossfade, bCrossfade] = std::tuple { dynamicDowncast<CrossfadeImage>(aSelected), dynamicDowncast<CrossfadeImage>(bSelected) }; aCrossfade && bCrossfade) {
        if (aCrossfade->equalInputImages(*bCrossfade)) {
            if (RefPtr crossfadeBlend = bCrossfade->blend(*aCrossfade, context))
                return ImageWrapper { crossfadeBlend.releaseNonNull() };
        }
    } else if (auto [aFilter, bCachedImage] = std::tuple { dynamicDowncast<FilterImage>(aSelected), dynamicDowncast<CachedImage>(bSelected) }; aFilter && bCachedImage) {
        RefPtr aFilterInputImage = dynamicDowncast<CachedImage>(aFilter->inputImage());

        if (aFilterInputImage && bCachedImage->equals(*aFilterInputImage))
            return filterBlend(WTF::move(aFilterInputImage), aFilter->filter(), Filter { CSS::Keyword::None { } }, aStyle, bStyle, context);
    } else if (auto [aCachedImage, bFilter] = std::tuple { dynamicDowncast<CachedImage>(aSelected), dynamicDowncast<FilterImage>(bSelected) }; aCachedImage && bFilter) {
        RefPtr bFilterInputImage = dynamicDowncast<CachedImage>(bFilter->inputImage());

        if (bFilterInputImage && aCachedImage->equals(*bFilterInputImage))
            return filterBlend(WTF::move(bFilterInputImage), Filter { CSS::Keyword::None { } }, bFilter->filter(), aStyle, bStyle, context);
    }

    RefPtr aCachedImage = dynamicDowncast<CachedImage>(aSelected);
    RefPtr bCachedImage = dynamicDowncast<CachedImage>(bSelected);
    if (aCachedImage && bCachedImage)
        return crossfadeBlend(aCachedImage.releaseNonNull(), bCachedImage.releaseNonNull(), context);

    if (auto [aGradient, bGradient] = std::tuple { dynamicDowncast<GradientImage>(aSelected), dynamicDowncast<GradientImage>(bSelected) }; aGradient && bGradient) {
        if (auto blended = gradientBlend(*aGradient, *bGradient, aStyle, bStyle, context))
            return WTF::move(*blended);
    }

    // FIXME: Add support cross fade between cached and generated images.
    // https://bugs.webkit.org/show_bug.cgi?id=78293

    return ImageWrapper { WTF::move(bSelected) };
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const ImageWrapper& value)
{
    Ref image = value.value;

    ts << "image"_s;
    if (!image->url().resolved.isEmpty())
        ts << '(' << image->url().resolved << ')';
    return ts;
}

} // namespace Style
} // namespace WebCore
