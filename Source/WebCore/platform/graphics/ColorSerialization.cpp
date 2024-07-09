/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorSerialization.h"

#include "Color.h"
#include "ColorNormalization.h"
#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

struct CSSColorNumericComponent { float value; };

static CSSColorNumericComponent numericComponent(float value)
{
    return { value };
}

}

namespace WTF {

template<> class StringTypeAdapter<WebCore::CSSColorNumericComponent, void> {
public:
    struct NaN {
        static constexpr ASCIILiteral text = "none"_s;
        unsigned length() const { return text.length(); }
        template<typename CharacterType> void writeTo(CharacterType* destination) const
        {
            StringImpl::copyCharacters(destination, text.span8());
        }
    };
    struct Finite {
        unsigned length() const { return number.length(); }
        template<typename CharacterType> void writeTo(CharacterType* destination) const
        {
            StringImpl::copyCharacters(destination, number.span());
        }

        FormattedCSSNumber number;
    };
    struct NonFinite {
        static constexpr ASCIILiteral prefix = "calc("_s;
        static constexpr ASCIILiteral suffix = ")"_s;
        unsigned length() const { return prefix.length() + number.length() + suffix.length(); }
        template<typename CharacterType> void writeTo(CharacterType* destination) const
        {
            StringImpl::copyCharacters(destination, prefix.span8());
            destination += prefix.length();
            StringImpl::copyCharacters(destination, number.span());
            destination += number.length();
            StringImpl::copyCharacters(destination, suffix.span8());
        }

        FormattedCSSNumber number;
    };

    static std::variant<NaN, Finite, NonFinite> compute(WebCore::CSSColorNumericComponent value)
    {
        if (std::isnan(value.value))
            return NaN { };
        if (std::isfinite(value.value))
            return Finite { FormattedCSSNumber::create(value.value) };
        return NonFinite { FormattedCSSNumber::create(value.value) };
    }

    StringTypeAdapter(WebCore::CSSColorNumericComponent component)
        : m_value { compute(component) }
    {
    }

    bool is8Bit() const { return true; }

    unsigned length() const
    {
        return WTF::switchOn(m_value, [](const auto& value) { return value.length(); });
    }

    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        WTF::switchOn(m_value, [&](auto& value) { value.writeTo(destination); });
    }

private:
    std::variant<NaN, Finite, NonFinite> m_value;
};

}

namespace WebCore {


template<typename Maker> static typename Maker::Result serializationForCSS(const A98RGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const A98RGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const A98RGB<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const DisplayP3<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const DisplayP3<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const DisplayP3<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ExtendedA98RGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ExtendedA98RGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ExtendedA98RGB<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ExtendedDisplayP3<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ExtendedDisplayP3<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ExtendedDisplayP3<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ExtendedLinearSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ExtendedLinearSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ExtendedLinearSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ExtendedProPhotoRGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ExtendedProPhotoRGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ExtendedProPhotoRGB<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ExtendedRec2020<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ExtendedRec2020<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ExtendedRec2020<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ExtendedSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ExtendedSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ExtendedSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const HSLA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const HSLA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const HSLA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const HWBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const HWBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const HWBA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const LCHA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const LCHA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const LCHA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const Lab<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const Lab<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const Lab<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const LinearSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const LinearSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const LinearSRGBA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const OKLCHA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const OKLCHA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const OKLCHA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const OKLab<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const OKLab<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const OKLab<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const ProPhotoRGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const ProPhotoRGB<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const ProPhotoRGB<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const Rec2020<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const Rec2020<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const Rec2020<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const SRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const SRGBA<float>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const SRGBA<float>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(SRGBA<uint8_t>, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(SRGBA<uint8_t>, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(SRGBA<uint8_t>, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization, Maker&&);

template<typename Maker> static typename Maker::Result serializationForCSS(const XYZA<float, WhitePoint::D65>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForHTML(const XYZA<float, WhitePoint::D65>&, bool useColorFunctionSerialization, Maker&&);
template<typename Maker> static typename Maker::Result serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D65>&, bool useColorFunctionSerialization, Maker&&);

String serializationForCSS(const Color& color)
{
    return color.callOnUnderlyingType([&](auto underlyingColor) {
        return serializationForCSS(underlyingColor, color.usesColorFunctionSerialization(), SerializeUsingMakeString { });
    });
}

void serializationForCSS(StringBuilder& builder, const Color& color)
{
    color.callOnUnderlyingType([&](auto underlyingColor) {
        serializationForCSS(underlyingColor, color.usesColorFunctionSerialization(), SerializeUsingStringBuilder { builder });
    });
}

String serializationForHTML(const Color& color)
{
    return color.callOnUnderlyingType([&](auto underlyingColor) {
        return serializationForHTML(underlyingColor, color.usesColorFunctionSerialization(), SerializeUsingMakeString { });
    });
}

void serializationForHTML(StringBuilder& builder, const Color& color)
{
    color.callOnUnderlyingType([&](auto underlyingColor) {
        serializationForHTML(underlyingColor, color.usesColorFunctionSerialization(), SerializeUsingStringBuilder { builder });
    });
}

String serializationForRenderTreeAsText(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForRenderTreeAsText(underlyingColor, color.usesColorFunctionSerialization(), SerializeUsingMakeString { });
    });
}

void serializationForRenderTreeAsText(StringBuilder& builder, const Color& color)
{
    color.callOnUnderlyingType([&](auto underlyingColor) {
        serializationForRenderTreeAsText(underlyingColor, color.usesColorFunctionSerialization(), SerializeUsingStringBuilder { builder });
    });
}

ASCIILiteral serialization(ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
    case ColorSpace::ExtendedA98RGB:
        return "a98-rgb"_s;
    case ColorSpace::DisplayP3:
    case ColorSpace::ExtendedDisplayP3:
        return "display-p3"_s;
    case ColorSpace::HSL:
        return "hsl"_s;
    case ColorSpace::HWB:
        return "hwb"_s;
    case ColorSpace::LCH:
        return "lch"_s;
    case ColorSpace::Lab:
        return "lab"_s;
    case ColorSpace::LinearSRGB:
    case ColorSpace::ExtendedLinearSRGB:
        return "srgb-linear"_s;
    case ColorSpace::OKLCH:
        return "oklch"_s;
    case ColorSpace::OKLab:
        return "oklab"_s;
    case ColorSpace::ProPhotoRGB:
    case ColorSpace::ExtendedProPhotoRGB:
        return "prophoto-rgb"_s;
    case ColorSpace::Rec2020:
    case ColorSpace::ExtendedRec2020:
        return "rec2020"_s;
    case ColorSpace::SRGB:
    case ColorSpace::ExtendedSRGB:
        return "srgb"_s;
    case ColorSpace::XYZ_D50:
        return "xyz-d50"_s;
    case ColorSpace::XYZ_D65:
        return "xyz-d65"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

void serialization(StringBuilder& builder, ColorSpace colorSpace)
{
    builder.append(serialization(colorSpace));
}

template<typename ColorType, typename Maker> static typename Maker::Result serializationUsingColorFunction(const ColorType& color, Maker&& maker)
{
    static_assert(std::is_same_v<typename ColorType::ComponentType, float>);

    auto [c1, c2, c3, alpha] = color.unresolved();
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return maker("color("_s, serialization(ColorSpaceFor<ColorType>), ' ', numericComponent(c1), ' ', numericComponent(c2), ' ', numericComponent(c3), ')');
    return maker("color("_s, serialization(ColorSpaceFor<ColorType>), ' ', numericComponent(c1), ' ', numericComponent(c2), ' ', numericComponent(c3), " / "_s, numericComponent(alpha), ')');
}

template<typename Maker> static typename Maker::Result serializationUsingColorFunction(const SRGBA<uint8_t>& color, Maker&& maker)
{
    return serializationUsingColorFunction(convertColor<SRGBA<float>>(color), std::forward<Maker>(maker));
}

template<typename ColorType, typename Maker> static typename Maker::Result serializationOfLabLikeColorsForCSS(const ColorType& color, Maker&& maker)
{
    static_assert(std::is_same_v<typename ColorType::ComponentType, float>);

    // https://www.w3.org/TR/css-color-4/#serializing-lab-lch
    auto [c1, c2, c3, alpha] = color.unresolved();
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return maker(serialization(ColorSpaceFor<ColorType>), '(', numericComponent(c1), ' ', numericComponent(c2), ' ', numericComponent(c3), ')');
    return maker(serialization(ColorSpaceFor<ColorType>), '(', numericComponent(c1), ' ', numericComponent(c2), ' ', numericComponent(c3), " / "_s, numericComponent(alpha), ')');
}

template<typename ColorType, typename Maker> static typename Maker::Result serializationOfLCHLikeColorsForCSS(const ColorType& color, Maker&& maker)
{
    static_assert(std::is_same_v<typename ColorType::ComponentType, float>);

    // https://www.w3.org/TR/css-color-4/#serializing-lab-lch

    // NOTE: It is unclear whether normalizing the hue component for serialization is required. The question has been
    // raised with the editors as https://github.com/w3c/csswg-drafts/issues/7782.

    auto [c1, c2, c3, alpha] = color.unresolved();
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return maker(serialization(ColorSpaceFor<ColorType>), '(', numericComponent(c1), ' ', numericComponent(c2), ' ', numericComponent(normalizeHue(c3)), ')');
    return maker(serialization(ColorSpaceFor<ColorType>), '(', numericComponent(c1), ' ', numericComponent(c2), ' ', numericComponent(normalizeHue(c3)), " / "_s, numericComponent(alpha), ')');
}

// MARK: A98RGB<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const A98RGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const A98RGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const A98RGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: DisplayP3<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const DisplayP3<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const DisplayP3<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const DisplayP3<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: ExtendedA98RGB<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ExtendedA98RGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ExtendedA98RGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ExtendedA98RGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: ExtendedDisplayP3<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ExtendedDisplayP3<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ExtendedDisplayP3<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ExtendedDisplayP3<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: ExtendedLinearSRGBA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ExtendedLinearSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ExtendedLinearSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ExtendedLinearSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: ExtendedProPhotoRGB<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ExtendedProPhotoRGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ExtendedProPhotoRGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ExtendedProPhotoRGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: ExtendedRec2020<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ExtendedRec2020<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ExtendedRec2020<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ExtendedRec2020<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: ExtendedSRGBA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ExtendedSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ExtendedSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ExtendedSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: HSLA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const HSLA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    // FIXME: The spec is not completely clear on whether missing components should be
    // carried forward here, but it seems like people are leaning toward thinking they
    // should be. See https://github.com/w3c/csswg-drafts/issues/10254.

    if (useColorFunctionSerialization)
        return serializationForCSS(convertColorCarryingForwardMissing<ExtendedSRGBA<float>>(color), true, std::forward<Maker>(maker));

    return serializationForCSS(convertColor<SRGBA<uint8_t>>(color), false, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const HSLA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForHTML(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const HSLA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForRenderTreeAsText(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: HWBA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const HWBA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    // FIXME: The spec is not completely clear on whether missing components should be
    // carried forward here, but it seems like people are leaning toward thinking they
    // should be. See https://github.com/w3c/csswg-drafts/issues/10254.

    if (useColorFunctionSerialization)
        return serializationForCSS(convertColorCarryingForwardMissing<ExtendedSRGBA<float>>(color), true, std::forward<Maker>(maker));

    return serializationForCSS(convertColor<SRGBA<uint8_t>>(color), false, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const HWBA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForHTML(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const HWBA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForRenderTreeAsText(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: LCHA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const LCHA<float>& color, bool, Maker&& maker)
{
    return serializationOfLCHLikeColorsForCSS(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const LCHA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const LCHA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: Lab<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const Lab<float>& color, bool, Maker&& maker)
{
    return serializationOfLabLikeColorsForCSS(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const Lab<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const Lab<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: LinearSRGBA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const LinearSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const LinearSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const LinearSRGBA<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: OKLCHA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const OKLCHA<float>& color, bool, Maker&& maker)
{
    return serializationOfLCHLikeColorsForCSS(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const OKLCHA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const OKLCHA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: OKLab<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const OKLab<float>& color, bool, Maker&& maker)
{
    return serializationOfLabLikeColorsForCSS(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const OKLab<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const OKLab<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: ProPhotoRGB<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const ProPhotoRGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const ProPhotoRGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const ProPhotoRGB<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: Rec2020<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const Rec2020<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const Rec2020<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const Rec2020<float>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: SRGBA<float> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const SRGBA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color, std::forward<Maker>(maker));

    return serializationForCSS(convertColor<SRGBA<uint8_t>>(color), false, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const SRGBA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const SRGBA<float>& color, bool useColorFunctionSerialization, Maker&& maker)
{
    return serializationForCSS(color, useColorFunctionSerialization, std::forward<Maker>(maker));
}

// MARK: SRGBA<uint8_t> overloads

static char decimalDigit(unsigned number)
{
    ASSERT(number < 10);
    return '0' + number;
}

static std::array<char, 4> fractionDigitsForFractionalAlphaValue(uint8_t alpha)
{
    ASSERT(alpha > 0);
    ASSERT(alpha < 0xFF);
    if (((alpha * 100 + 0x7F) / 0xFF * 0xFF + 50) / 100 != alpha)
        return { { decimalDigit(alpha * 10 / 0xFF % 10), decimalDigit(alpha * 100 / 0xFF % 10), decimalDigit((alpha * 1000 + 0x7F) / 0xFF % 10), '\0' } };
    if (int thirdDigit = (alpha * 100 + 0x7F) / 0xFF % 10)
        return { { decimalDigit(alpha * 10 / 0xFF), decimalDigit(thirdDigit), '\0', '\0' } };
    return { { decimalDigit((alpha * 10 + 0x7F) / 0xFF), '\0', '\0', '\0' } };
}

template<typename Maker> typename Maker::Result serializationForCSS(SRGBA<uint8_t> color, bool useColorFunctionSerialization, Maker&& maker)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color, std::forward<Maker>(maker));

    auto [red, green, blue, alpha] = color.resolved();
    switch (alpha) {
    case 0:
        return maker("rgba("_s, red, ", "_s, green, ", "_s, blue, ", 0)"_s);
    case 0xFF:
        return maker("rgb("_s, red, ", "_s, green, ", "_s, blue, ')');
    default:
        return maker("rgba("_s, red, ", "_s, green, ", "_s, blue, ", 0."_s, span(fractionDigitsForFractionalAlphaValue(alpha).data()), ')');
    }
}

template<typename Maker> typename Maker::Result serializationForHTML(SRGBA<uint8_t> color, bool useColorFunctionSerialization, Maker&& maker)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color, std::forward<Maker>(maker));

    auto [red, green, blue, alpha] = color.resolved();
    if (alpha == 0xFF)
        return maker('#', hex(red, 2, Lowercase), hex(green, 2, Lowercase), hex(blue, 2, Lowercase));
    return serializationForCSS(color, false, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(SRGBA<uint8_t> color, bool useColorFunctionSerialization, Maker&& maker)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color, std::forward<Maker>(maker));

    auto [red, green, blue, alpha] = color.resolved();
    if (alpha < 0xFF)
        return maker('#', hex(red, 2), hex(green, 2), hex(blue, 2), hex(alpha, 2));
    return maker('#', hex(red, 2), hex(green, 2), hex(blue, 2));
}

// MARK: XYZA<float, WhitePoint::D50> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const XYZA<float, WhitePoint::D50>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const XYZA<float, WhitePoint::D50>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D50>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

// MARK: XYZA<float, WhitePoint::D65> overloads

template<typename Maker> typename Maker::Result serializationForCSS(const XYZA<float, WhitePoint::D65>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForHTML(const XYZA<float, WhitePoint::D65>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

template<typename Maker> typename Maker::Result serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D65>& color, bool, Maker&& maker)
{
    return serializationUsingColorFunction(color, std::forward<Maker>(maker));
}

}
