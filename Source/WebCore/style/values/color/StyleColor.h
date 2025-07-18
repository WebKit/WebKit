/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#pragma once

#include "CSSColor.h"
#include "CSSColorDescriptors.h"
#include "CSSColorType.h"
#include "CSSValueKeywords.h"
#include "StyleColorOptions.h"
#include "StyleCurrentColor.h"
#include "StyleResolvedColor.h"
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Color;
class Document;
class RenderStyle;

namespace Style {

enum class ForVisitedLink : bool;

// The following style color kinds are forward declared and stored in
// UniqueRefs to avoid unnecessarily growing the size of Color for the
// uncommon case of un-resolvability due to currentColor.
struct ColorLayers;
struct ColorMix;
struct ContrastColor;
template<typename Descriptor> struct RelativeColor;

struct Color {
private:
    friend struct MarkableTraits<Color>;
    struct EmptyToken { constexpr bool operator==(const EmptyToken&) const = default; };

    // FIXME: Replace Variant with a generic CompactPointerVariant type.
    using ColorKind = Variant<
        EmptyToken,
        ResolvedColor,
        CurrentColor,
        UniqueRef<ColorLayers>,
        UniqueRef<ColorMix>,
        UniqueRef<ContrastColor>,
        UniqueRef<RelativeColor<RGBFunctionModernRelative>>,
        UniqueRef<RelativeColor<HSLFunctionModern>>,
        UniqueRef<RelativeColor<HWBFunction>>,
        UniqueRef<RelativeColor<LabFunction>>,
        UniqueRef<RelativeColor<LCHFunction>>,
        UniqueRef<RelativeColor<OKLabFunction>>,
        UniqueRef<RelativeColor<OKLCHFunction>>,
        UniqueRef<RelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>>,
        UniqueRef<RelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>>,
        UniqueRef<RelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>>,
        UniqueRef<RelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>>,
        UniqueRef<RelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>>,
        UniqueRef<RelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>>,
        UniqueRef<RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>>,
        UniqueRef<RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>>
    >;

    Color(EmptyToken);
    Color(ColorKind&&);

public:
    // The default constructor initializes to Style::CurrentColor to preserve old behavior,
    // we might want to remove it entirely at some point.
    Color();

    // Convenience constructors that create Style::ResolvedColor.
    Color(WebCore::Color);
    Color(SRGBA<uint8_t>);

    WEBCORE_EXPORT Color(ResolvedColor&&);
    WEBCORE_EXPORT Color(CurrentColor&&);
    Color(ColorLayers&&);
    Color(ColorMix&&);
    Color(ContrastColor&&);
    Color(RelativeColor<RGBFunctionModernRelative>&&);
    Color(RelativeColor<HSLFunctionModern>&&);
    Color(RelativeColor<HWBFunction>&&);
    Color(RelativeColor<LabFunction>&&);
    Color(RelativeColor<LCHFunction>&&);
    Color(RelativeColor<OKLabFunction>&&);
    Color(RelativeColor<OKLCHFunction>&&);
    Color(RelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>&&);
    Color(RelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>&&);
    Color(RelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>&&);
    Color(RelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>&&);
    Color(RelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>&&);
    Color(RelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>&&);
    Color(RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>&&);
    Color(RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>&&);

    WEBCORE_EXPORT Color(const Color&);
    Color& operator=(const Color&);

    WEBCORE_EXPORT Color(Color&&);
    Color& operator=(Color&&);

    WEBCORE_EXPORT ~Color();

    bool operator==(const Color&) const;

    static Color currentColor();

    bool containsCurrentColor() const;
    bool isCurrentColor() const;
    bool isColorMix() const;
    bool isContrastColor() const;
    bool isRelativeColor() const;

    bool isResolvedColor() const;
    const WebCore::Color& resolvedColor() const;

    WEBCORE_EXPORT WebCore::Color resolveColor(const WebCore::Color& currentColor) const;

    // This helper allows us to treat all the alternatives in ColorKind
    // as const references, pretending the UniqueRefs don't exist.
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    String debugDescription() const;

private:
    template<typename T>
    static ColorKind makeIndirectColor(T&&);
    static ColorKind copy(const ColorKind&);

    ColorKind value;
};

WebCore::Color resolveColor(const Color&, const WebCore::Color& currentColor);
bool containsCurrentColor(const Color&);

template<> struct Serialize<Color> {
    void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const Color&);
};

WTF::TextStream& operator<<(WTF::TextStream&, const Color&);

// MARK: - Conversion

Color toStyleColor(const CSS::Color&, ColorResolutionState&);
Color toStyleColor(const CSS::Color&, Ref<const Document>, const RenderStyle&, const CSSToLengthConversionData&, ForVisitedLink);

template<> struct ToCSS<Color> {
    auto operator()(const Color&, const RenderStyle&) -> CSS::Color;
};
template<> struct ToStyle<CSS::Color> {
    auto operator()(const CSS::Color&, const BuilderState&, ForVisitedLink) -> Color;
    auto operator()(const CSS::Color&, const BuilderState&) -> Color;
};

template<> struct CSSValueConversion<Color> {
    auto operator()(BuilderState&, const CSSValue&, ForVisitedLink) -> Color;
};
template<> struct CSSValueCreation<Color> {
    auto operator()(CSSValuePool&, const RenderStyle&, const Color&) -> Ref<CSSValue>;
};

// MARK: - Blending

template<> struct Blending<Color> {
    auto equals(const Color&, const Color&, const RenderStyle&, const RenderStyle&) -> bool;
    auto canBlend(const Color&, const Color&) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const Color&, const Color&) -> bool { return true; }
    auto blend(const Color&, const Color&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> Color;
};

// MARK: - Color Implementation

template<typename... F> decltype(auto) Color::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
    using ResultType = decltype(visitor(std::declval<ResolvedColor>()));

    return WTF::switchOn(value,
        [&](const EmptyToken&) -> ResultType {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const ResolvedColor& resolvedColor) -> ResultType {
            return visitor(resolvedColor);
        },
        [&](const CurrentColor& currentColor) -> ResultType {
            return visitor(currentColor);
        },
        [&]<typename T>(const UniqueRef<T>& color) -> ResultType {
            return visitor(color.get());
        }
    );
}

} // namespace Style
} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::Style::Color> {
    static bool isEmptyValue(const WebCore::Style::Color& color) { return std::holds_alternative<WebCore::Style::Color::EmptyToken>(color.value); }
    static WebCore::Style::Color emptyValue() { return WebCore::Style::Color(WebCore::Style::Color::EmptyToken()); }
};

}

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::Color> = true;
