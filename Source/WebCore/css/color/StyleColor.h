/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "CSSColorDescriptors.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "StyleAbsoluteColor.h"
#include "StyleCurrentColor.h"
#include <wtf/OptionSet.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Color;

enum class StyleColorOptions : uint8_t {
    ForVisitedLink = 1 << 0,
    UseSystemAppearance = 1 << 1,
    UseDarkAppearance = 1 << 2,
    UseElevatedUserInterfaceLevel = 1 << 3
};

// The following style color kinds are forward declared and stored in
// UniqueRefs to avoid unnecessarily growing the size of StyleColor for the
// uncommon case of un-resolvability due to currentColor.
struct StyleColorLayers;
struct StyleColorMix;
struct StyleContrastColor;
template<typename Descriptor>
struct StyleRelativeColor;

class StyleColor {
public:
    // The default constructor initializes to StyleCurrentColor to preserve old behavior,
    // we might want to change it to invalid color at some point.
    StyleColor();

    // Convenience constructors that create StyleAbsoluteColor.
    StyleColor(Color);
    StyleColor(SRGBA<uint8_t>);

    StyleColor(StyleAbsoluteColor&&);
    StyleColor(StyleCurrentColor&&);
    StyleColor(StyleColorLayers&&);
    StyleColor(StyleColorMix&&);
    StyleColor(StyleContrastColor&&);
    StyleColor(StyleRelativeColor<RGBFunctionModernRelative>&&);
    StyleColor(StyleRelativeColor<HSLFunctionModern>&&);
    StyleColor(StyleRelativeColor<HWBFunction>&&);
    StyleColor(StyleRelativeColor<LabFunction>&&);
    StyleColor(StyleRelativeColor<LCHFunction>&&);
    StyleColor(StyleRelativeColor<OKLabFunction>&&);
    StyleColor(StyleRelativeColor<OKLCHFunction>&&);
    StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>&&);
    StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>&&);
    StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>&&);
    StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>&&);
    StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>&&);
    StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>&&);
    StyleColor(StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>&&);
    StyleColor(StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>&&);

    WEBCORE_EXPORT StyleColor(const StyleColor&);
    StyleColor& operator=(const StyleColor&);

    StyleColor(StyleColor&&);
    StyleColor& operator=(StyleColor&&);

    WEBCORE_EXPORT ~StyleColor();

    static StyleColor currentColor();

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
    bool isColorMix() const;
    bool isContrastColor() const;
    bool isRelativeColor() const;
    bool isAbsoluteColor() const;
    const Color& absoluteColor() const;

    WEBCORE_EXPORT Color resolveColor(const Color& currentColor) const;

    bool operator==(const StyleColor&) const;
    friend WEBCORE_EXPORT String serializationForCSS(const StyleColor&);
    friend void serializationForCSS(StringBuilder&, const StyleColor&);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const StyleColor&);
    String debugDescription() const;

private:
    using ColorKind = std::variant<
        StyleAbsoluteColor,
        StyleCurrentColor,
        UniqueRef<StyleColorLayers>,
        UniqueRef<StyleColorMix>,
        UniqueRef<StyleContrastColor>,
        UniqueRef<StyleRelativeColor<RGBFunctionModernRelative>>,
        UniqueRef<StyleRelativeColor<HSLFunctionModern>>,
        UniqueRef<StyleRelativeColor<HWBFunction>>,
        UniqueRef<StyleRelativeColor<LabFunction>>,
        UniqueRef<StyleRelativeColor<LCHFunction>>,
        UniqueRef<StyleRelativeColor<OKLabFunction>>,
        UniqueRef<StyleRelativeColor<OKLCHFunction>>,
        UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>>,
        UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>>,
        UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>>,
        UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>>,
        UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>>,
        UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>>,
        UniqueRef<StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>>,
        UniqueRef<StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>>
    >;
    StyleColor(ColorKind&&);

    template<typename... F>
    static decltype(auto) visit(const ColorKind&, F&&...);

    template<typename StyleColorType>
    static ColorKind makeIndirectColor(StyleColorType&&);

    static ColorKind copy(const ColorKind&);

    ColorKind m_color;
};

void serializationForCSS(StringBuilder&, const StyleColor&);
WEBCORE_EXPORT String serializationForCSS(const StyleColor&);

WTF::TextStream& operator<<(WTF::TextStream&, const StyleColor&);

} // namespace WebCore
