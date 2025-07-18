/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleColor.h"

#include "AnimationUtilities.h"
#include "CSSColorValue.h"
#include "CSSKeywordColor.h"
#include "CSSValuePool.h"
#include "ColorBlending.h"
#include "Document.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "StyleAbsoluteColor.h"
#include "StyleColorLayers.h"
#include "StyleColorMix.h"
#include "StyleColorResolutionState.h"
#include "StyleContrastColor.h"
#include "StyleHexColor.h"
#include "StyleKeywordColor.h"
#include "StyleLightDarkColor.h"
#include "StyleRelativeColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

Color::Color(Color::ColorKind&& color)
    : value { WTFMove(color) }
{
}

Color::Color(EmptyToken token)
    : value { token }
{
}

Color::Color()
    : value { CurrentColor { } }
{
}

Color::Color(WebCore::Color color)
    : value { ResolvedColor { WTFMove(color) } }
{
}

Color::Color(SRGBA<uint8_t> color)
    : value { ResolvedColor { WebCore::Color { color } } }
{
}

Color::Color(ResolvedColor&& color)
    : value { WTFMove(color) }
{
}

Color::Color(CurrentColor&& color)
    : value { WTFMove(color) }
{
}

Color::Color(ColorLayers&& colorLayers)
    : value { makeIndirectColor(WTFMove(colorLayers)) }
{
}

Color::Color(ColorMix&& colorMix)
    : value { makeIndirectColor(WTFMove(colorMix)) }
{
}

Color::Color(ContrastColor&& contrastColor)
    : value { makeIndirectColor(WTFMove(contrastColor)) }
{
}

Color::Color(RelativeColor<RGBFunctionModernRelative>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<HSLFunctionModern>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<HWBFunction>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<LabFunction>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<LCHFunction>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<OKLabFunction>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<OKLCHFunction>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>&& relative)
    : value { makeIndirectColor(WTFMove(relative)) }
{
}

Color::Color(const Color& other)
    : value { copy(other.value) }
{
}

Color& Color::operator=(const Color& other)
{
    value = copy(other.value);
    return *this;
}

Color::Color(Color&&) = default;
Color& Color::operator=(Color&&) = default;

Color::~Color() = default;

bool Color::operator==(const Color& other) const = default;

Color Color::currentColor()
{
    return Color { CurrentColor { } };
}

Color::ColorKind Color::copy(const Color::ColorKind& other)
{
    return WTF::switchOn(other,
        []<typename T>(const T& color) -> Color::ColorKind {
            return color;
        },
        []<typename T>(const UniqueRef<T>& color) -> Color::ColorKind {
            return makeUniqueRef<T>(color.get());
        }
    );
}

String Color::debugDescription() const
{
    TextStream ts;
    ts << *this;
    return ts.release();
}

WebCore::Color Color::resolveColor(const WebCore::Color& currentColor) const
{
    return switchOn([&](const auto& kind) { return WebCore::Style::resolveColor(kind, currentColor); });
}

bool Color::containsCurrentColor() const
{
    return switchOn([](const auto& kind) { return WebCore::Style::containsCurrentColor(kind); });
}

bool Color::isCurrentColor() const
{
    return std::holds_alternative<CurrentColor>(value);
}

bool Color::isColorMix() const
{
    return std::holds_alternative<UniqueRef<ColorMix>>(value);
}

bool Color::isContrastColor() const
{
    return std::holds_alternative<UniqueRef<ContrastColor>>(value);
}

bool Color::isRelativeColor() const
{
    return std::holds_alternative<UniqueRef<RelativeColor<RGBFunctionModernRelative>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<HSLFunctionModern>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<HWBFunction>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<LabFunction>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<LCHFunction>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<OKLabFunction>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<OKLCHFunction>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>>>(value)
        || std::holds_alternative<UniqueRef<RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>>>(value);
}

bool Color::isResolvedColor() const
{
    return std::holds_alternative<ResolvedColor>(value);
}

const WebCore::Color& Color::resolvedColor() const
{
    ASSERT(isResolvedColor());
    return std::get<ResolvedColor>(value).color;
}

template<typename T> Color::ColorKind Color::makeIndirectColor(T&& colorType)
{
    return { makeUniqueRef<T>(WTFMove(colorType)) };
}

WebCore::Color resolveColor(const Color& value, const WebCore::Color& currentColor)
{
    return value.resolveColor(currentColor);
}

bool containsCurrentColor(const Color& value)
{
    return value.containsCurrentColor();
}

// MARK: - Serialization

void Serialize<Color>::operator()(StringBuilder& builder, const CSS::SerializationContext&, const RenderStyle& style, const Color& value)
{
    // NOTE: The specialization of Style::Serialize is used for computed value serialization, so the resolved "used" value is used.
    builder.append(WebCore::serializationForCSS(style.colorResolvingCurrentColor(value)));
}

// MARK: - TextStream.

TextStream& operator<<(TextStream& ts, const Color& value)
{
    ts << "Style::Color["_s;
    WTF::switchOn(value, [&](const auto& kind) { ts << kind; });
    ts << ']';

    return ts;
}

// MARK: - Conversion

Color toStyleColor(const CSS::Color& value, ColorResolutionState& state)
{
    return WTF::switchOn(value, [&](const auto& color) { return toStyleColor(color, state); });
}

Color toStyleColor(const CSS::Color& value, Ref<const Document> document, const RenderStyle& style, const CSSToLengthConversionData& conversionData, ForVisitedLink forVisitedLink)
{
    auto resolutionState = ColorResolutionState {
        .document = document,
        .style = style,
        .conversionData = conversionData,
        .forVisitedLink = forVisitedLink
    };
    return toStyleColor(value, resolutionState);
}

auto ToCSS<Color>::operator()(const Color& value, const RenderStyle& style) -> CSS::Color
{
    return CSS::Color { CSS::ResolvedColor { style.colorResolvingCurrentColor(value) } };
}

auto ToStyle<CSS::Color>::operator()(const CSS::Color& value, const BuilderState& builderState, ForVisitedLink forVisitedLink) -> Color
{
    return toStyleColor(value, builderState.document(), builderState.style(), builderState.cssToLengthConversionData(), forVisitedLink);
}

auto ToStyle<CSS::Color>::operator()(const CSS::Color& value, const BuilderState& builderState) -> Color
{
    return toStyle(value, builderState, ForVisitedLink::No);
}

auto CSSValueConversion<Color>::operator()(BuilderState& builderState, const CSSValue& value, ForVisitedLink forVisitedLink) -> Color
{
    if (!builderState.element() || !builderState.element()->isLink())
        forVisitedLink = ForVisitedLink::No;

    if (RefPtr color = dynamicDowncast<CSSColorValue>(value))
        return toStyle(color->color(), builderState, forVisitedLink);
    return toStyle(CSS::Color { CSS::KeywordColor { value.valueID() } }, builderState, forVisitedLink);
}

Ref<CSSValue> CSSValueCreation<Color>::operator()(CSSValuePool& pool, const RenderStyle& style, const Color& value)
{
    return pool.createColorValue(style.colorResolvingCurrentColor(value));
}

// MARK: - Blending

auto Blending<Color>::equals(const Color& a, const Color& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (a.isCurrentColor() && b.isCurrentColor())
        return true;

    if (a.isResolvedColor() && b.isResolvedColor())
        return a.resolvedColor() == b.resolvedColor();

    return aStyle.colorResolvingCurrentColor(a) == bStyle.colorResolvingCurrentColor(b);
}

auto Blending<Color>::canBlend(const Color& a, const Color& b) -> bool
{
    // We don't animate on currentcolor-only transition.
    // https://github.com/WebKit/WebKit/blob/main/LayoutTests/imported/w3c/web-platform-tests/css/css-transitions/currentcolor-animation-001.html#L27
    return !(a.isCurrentColor() && b.isCurrentColor());
}

auto Blending<Color>::blend(const Color& a, const Color& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> Color
{
    return WebCore::blend(aStyle.colorResolvingCurrentColor(a), bStyle.colorResolvingCurrentColor(b), context);
}

} // namespace Style
} // namespace WebCore
