/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "StyleColorOptions.h"
#include "StyleCurrentColor.h"
#include "StyleResolvedColor.h"
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Document;
class RenderStyle;

namespace Style {

class ExtendedStyleColor;

enum class ForVisitedLink : bool;

// The following style color kinds are forward declared and stored in
// UniqueRefs to avoid unnecessarily growing the size of StyleColor for the
// uncommon case of un-resolvability due to currentColor.
using ColorKind = std::variant<ResolvedColor, CurrentColor, Ref<ExtendedStyleColor>>;

struct ColorLayers;
struct ColorMix;
struct ContrastColor;
template <typename T> struct RelativeColor;

struct Color {
public:
    // The default constructor initializes to StyleCurrentColor to preserve old behavior,
    // we might want to change it to invalid color at some point.
    Color();

    // Convenience constructors that create Style::ResolvedColor.
    Color(WebCore::Color);
    Color(SRGBA<uint8_t>);

    WEBCORE_EXPORT Color(ResolvedColor&&);
    WEBCORE_EXPORT Color(CurrentColor&&);

    Color(Ref<ExtendedStyleColor>&&);
    Color(ColorMix&&);
    Color(ColorLayers&&);
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

    static Color currentColor();
    static Color invalidColor();

    static Color colorFromKeyword(CSSValueID, OptionSet<StyleColorOptions>);
    static Color colorFromAbsoluteKeyword(CSSValueID);

    static bool containsCurrentColor(const CSSPrimitiveValue&);
    static bool isAbsoluteColorKeyword(CSSValueID);
    static bool isCurrentColorKeyword(CSSValueID id) { return id == CSSValueCurrentcolor; }
    static bool isCurrentColor(const CSSPrimitiveValue& value) { return isCurrentColorKeyword(value.valueID()); }

    WEBCORE_EXPORT static bool isSystemColorKeyword(CSSValueID);
    static bool isDeprecatedSystemColorKeyword(CSSValueID);

    static bool containsColorSchemeDependentColor(const CSSPrimitiveValue&);

    enum class CSSColorType : uint8_t {
        Absolute = 1 << 0,
        Current = 1 << 1,
        System = 1 << 2,
    };

    // https://drafts.csswg.org/css-color-4/#typedef-color
    static bool isColorKeyword(CSSValueID, OptionSet<CSSColorType> = { CSSColorType::Absolute, CSSColorType::Current, CSSColorType::System });

    bool containsCurrentColor() const;

    bool isCurrentColor() const;
    bool isExtendedStyleColor() const;
    bool isColorMix() const;
    bool isContrastColor() const;
    bool isRelativeColor() const;
    bool isResolvedColor() const;

    const WebCore::Color& resolvedColor() const;

    const ExtendedStyleColor& extendedStyleColor() const;
    Ref<const ExtendedStyleColor> protectedExtendedStyleColor() const;

    WEBCORE_EXPORT WebCore::Color resolveColor(const WebCore::Color& currentColor) const;
    bool operator==(const Color&) const;

    /*
    friend WEBCORE_EXPORT String serializationForCSS(const Color&);
    friend void serializationForCSS(StringBuilder&, const Color&);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const Color&);
    */
    String debugDescription() const;

    void store(ColorKind&&);

    struct MarkableTraits {
        static bool isEmptyValue(const Color&);
        static Color emptyValue();
    };

    template<typename StyleColorType>
    static ColorKind makeIndirectColor(StyleColorType&&);

private:
    WebCore::Color m_color;
};

WebCore::Color resolveColor(const Color&, const WebCore::Color& currentColor);
bool containsCurrentColor(const Color&);

void serializationForCSS(StringBuilder&, const Color&);
WEBCORE_EXPORT String serializationForCSS(const Color&);
WTF::TextStream& operator<<(WTF::TextStream&, const Color&);

// MARK: - Conversion
Color toStyleColor(const CSS::Color&, ColorResolutionState&);
Color toStyleColor(const CSS::Color&, Ref<const Document>, const RenderStyle&, const CSSToLengthConversionData&, ForVisitedLink);
Color toStyleColorWithResolvedCurrentColor(const CSS::Color&, Ref<const Document>, RenderStyle&, const CSSToLengthConversionData&, ForVisitedLink);

template<> struct ToCSS<Color> {
    auto operator()(const Color&, const RenderStyle&) -> CSS::Color;
};
template<> struct ToStyle<CSS::Color> {
    auto operator()(const CSS::Color&, const BuilderState&, ForVisitedLink) -> Color;
    auto operator()(const CSS::Color&, const BuilderState&) -> Color;
};

} // namespace Style
} // namespace WebCore
