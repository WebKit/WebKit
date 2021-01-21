/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "ColorComponents.h"
#include "ColorSpace.h"
#include <functional>

namespace WebCore {

template<typename> struct AlphaTraits;

template<> struct AlphaTraits<uint8_t> {
    static constexpr uint8_t transparent = 0;
    static constexpr uint8_t opaque = 255;
};

template<> struct AlphaTraits<float> {
    static constexpr float transparent = 0.0f;
    static constexpr float opaque = 1.0f;
};

template<typename T> struct ColorComponentRange {
    T min;
    T max;
};

template<typename> struct RGBModel;
template<typename> struct ExtendedRGBModel;
template<typename> struct LabModel;
template<typename> struct LCHModel;
template<typename> struct HSLModel;
template<typename> struct CMYKModel;
template<typename> struct XYZModel;

template<> struct RGBModel<uint8_t> {
    static constexpr std::array<ColorComponentRange<uint8_t>, 3> ranges { {
        { 0, 255 },
        { 0, 255 },
        { 0, 255 }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct RGBModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, 1 },
        { 0, 1 },
        { 0, 1 }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct ExtendedRGBModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct LabModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct LCHModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, std::numeric_limits<float>::infinity() },
        { 0, std::numeric_limits<float>::infinity() },
        { 0, 360 }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct HSLModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, 360 },
        { 0, 100 },
        { 0, 100 }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct CMYKModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 4> ranges { {
        { 0, 1 },
        { 0, 1 },
        { 0, 1 },
        { 0, 1 }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct XYZModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() }
    } };
    static constexpr bool isInvertible = false;
};

template<typename ColorType, typename T> constexpr ColorType makeFromComponents(const ColorComponents<T>& c)
{
    return ColorType { c[0], c[1], c[2], c[3] };
}

template<typename ColorType, unsigned Index, typename T> constexpr auto clampedComponent(T c) -> typename ColorType::ComponentType
{
    static_assert(std::is_integral_v<T>);

    constexpr auto range = ColorType::Model::ranges[Index];
    return std::clamp<T>(c, range.min, range.max);
}

template<typename ColorType, unsigned Index> constexpr float clampedComponent(float c)
{
    constexpr auto range = ColorType::Model::ranges[Index];

    if constexpr (range.min == -std::numeric_limits<float>::infinity() && range.max == std::numeric_limits<float>::infinity())
        return c;

    if constexpr (range.min == -std::numeric_limits<float>::infinity())
        return std::min(c, range.max);

    if constexpr (range.max == std::numeric_limits<float>::infinity())
        return std::max(c, range.min);

    return std::clamp(c, range.min, range.max);
}

template<typename ColorType, unsigned Index, typename T> constexpr T clampedComponent(const ColorComponents<T>& c)
{
    return clampedComponent<ColorType, Index>(c[Index]);
}

template<typename T, typename ComponentType = T> constexpr ComponentType clampedAlpha(T alpha)
{
    return std::clamp<T>(alpha, AlphaTraits<ComponentType>::transparent, AlphaTraits<ComponentType>::opaque);
}

template<typename ColorType, typename T> constexpr ColorComponents<T> clampedComponents(const ColorComponents<T>& components)
{
    return {
        clampedComponent<ColorType, 0>(components),
        clampedComponent<ColorType, 1>(components),
        clampedComponent<ColorType, 2>(components),
        clampedAlpha(components[3])
    };
}

template<typename ColorType, typename T> constexpr ColorComponents<T> clampedComponentsExceptAlpha(const ColorComponents<T>& components)
{
    return {
        clampedComponent<ColorType, 0>(components),
        clampedComponent<ColorType, 1>(components),
        clampedComponent<ColorType, 2>(components),
        components[3]
    };
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(const ColorComponents<T>& components)
{
    return makeFromComponents<ColorType>(clampedComponents<ColorType>(components));
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(T c1, T c2, T c3)
{
    return makeFromComponents<ColorType>(ColorComponents {
        clampedComponent<ColorType, 0>(c1),
        clampedComponent<ColorType, 1>(c2),
        clampedComponent<ColorType, 2>(c3),
        AlphaTraits<typename ColorType::ComponentType>::opaque
    });
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(T c1, T c2, T c3, T alpha)
{
    return makeFromComponents<ColorType>(ColorComponents {
        clampedComponent<ColorType, 0>(c1),
        clampedComponent<ColorType, 1>(c2),
        clampedComponent<ColorType, 2>(c3),
        clampedAlpha<T, typename ColorType::ComponentType>(alpha)
    });
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClampingExceptAlpha(const ColorComponents<T>& components)
{
    return makeFromComponents<ColorType>(clampedComponentsExceptAlpha<ColorType>(components));
}

template<typename ColorType, typename T, typename Alpha> constexpr ColorType makeFromComponentsClampingExceptAlpha(T c1, T c2, T c3, Alpha alpha)
{
    return makeFromComponents<ColorType>(ColorComponents {
        clampedComponent<ColorType, 0>(c1),
        clampedComponent<ColorType, 1>(c2),
        clampedComponent<ColorType, 2>(c3),
        alpha
    });
}

#if ASSERT_ENABLED

template<typename T> constexpr void assertInRange(T color)
{
    if constexpr (std::is_same_v<typename T::ComponentType, float>) {
        auto components = asColorComponents(color);
        for (unsigned i = 0; i < 3; ++i) {
            ASSERT_WITH_MESSAGE(components[i] >= T::Model::ranges[i].min, "Component at index %d is %f and is less than the allowed minimum %f", i,  components[i], T::Model::ranges[i].min);
            ASSERT_WITH_MESSAGE(components[i] <= T::Model::ranges[i].max, "Component at index %d is %f and is greater than the allowed maximum %f", i,  components[i], T::Model::ranges[i].max);
        }
        ASSERT_WITH_MESSAGE(color.alpha >= AlphaTraits<typename T::ComponentType>::transparent, "Alpha is %f and is less than the allowed minimum (transparent) %f", color.alpha, AlphaTraits<typename T::ComponentType>::transparent);
        ASSERT_WITH_MESSAGE(color.alpha <= AlphaTraits<typename T::ComponentType>::opaque, "Alpha is %f and is greater than the allowed maximum (opaque) %f", color.alpha, AlphaTraits<typename T::ComponentType>::opaque);
    }
}

#else

template<typename T> constexpr void assertInRange(T)
{
}

#endif

template<typename, typename = void> inline constexpr bool HasColorSpaceMember = false;
template<typename T> inline constexpr bool HasColorSpaceMember<T, std::void_t<decltype(std::declval<T>().colorSpace)>> = true;

template<typename, typename = void> inline constexpr bool IsConvertibleToColorComponents = false;
template<typename T> inline constexpr bool IsConvertibleToColorComponents<T, std::void_t<decltype(asColorComponents(std::declval<T>()))>> = true;

template<typename, typename = void> inline constexpr bool HasComponentTypeMember = false;
template<typename T> inline constexpr bool HasComponentTypeMember<T, std::void_t<typename T::ComponentType>> = true;

template<typename T, typename U, bool enabled> inline constexpr bool HasComponentTypeValue = false;
template<typename T, typename U> inline constexpr bool HasComponentTypeValue<T, U, true> = std::is_same_v<typename T::ComponentType, U>;
template<typename T, typename U> inline constexpr bool HasComponentType = HasComponentTypeValue<T, U, HasComponentTypeMember<T>>;

template<typename T> inline constexpr bool IsColorType = HasColorSpaceMember<T> && IsConvertibleToColorComponents<T> && HasComponentTypeMember<T>;
template<typename T, typename U> inline constexpr bool IsColorTypeWithComponentType = HasColorSpaceMember<T> && IsConvertibleToColorComponents<T> && HasComponentType<T, U>;

template<typename Parent> struct ColorWithAlphaHelper {
    // Helper to allow convenient syntax for working with color types.
    // e.g. auto yellowWith50PercentAlpha = Color::yellow.colorWithAlphaByte(128);
    constexpr Parent colorWithAlphaByte(uint8_t overrideAlpha) const
    {
        static_assert(std::is_same_v<decltype(std::declval<Parent>().alpha), uint8_t>, "Only uint8_t based color types are supported.");

        auto copy = *static_cast<const Parent*>(this);
        copy.alpha = overrideAlpha;
        return copy;
    }
};

template<typename T> struct SRGBA : ColorWithAlphaHelper<SRGBA<T>> {
    using ComponentType = T;
    using Model = RGBModel<T>;
    static constexpr auto colorSpace { ColorSpace::SRGB };
    
    constexpr SRGBA(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr SRGBA()
        : SRGBA { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const SRGBA<T>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}

template<typename T> constexpr bool operator==(const SRGBA<T>& a, const SRGBA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const SRGBA<T>& a, const SRGBA<T>& b)
{
    return !(a == b);
}


template<typename T> struct ExtendedSRGBA : ColorWithAlphaHelper<ExtendedSRGBA<T>> {
    using ComponentType = T;
    using Model = ExtendedRGBModel<T>;

    constexpr ExtendedSRGBA(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
    }

    constexpr ExtendedSRGBA()
        : ExtendedSRGBA { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const ExtendedSRGBA<T>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}

template<typename T> constexpr bool operator==(const ExtendedSRGBA<T>& a, const ExtendedSRGBA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const ExtendedSRGBA<T>& a, const ExtendedSRGBA<T>& b)
{
    return !(a == b);
}


template<typename T> struct LinearSRGBA : ColorWithAlphaHelper<LinearSRGBA<T>> {
    using ComponentType = T;
    using Model = RGBModel<T>;
    static constexpr auto colorSpace = ColorSpace::LinearRGB;

    constexpr LinearSRGBA(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr LinearSRGBA()
        : LinearSRGBA { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const LinearSRGBA<T>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}

template<typename T> constexpr bool operator==(const LinearSRGBA<T>& a, const LinearSRGBA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const LinearSRGBA<T>& a, const LinearSRGBA<T>& b)
{
    return !(a == b);
}


template<typename T> struct LinearExtendedSRGBA : ColorWithAlphaHelper<LinearExtendedSRGBA<T>> {
    using ComponentType = T;
    using Model = ExtendedRGBModel<T>;

    constexpr LinearExtendedSRGBA(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
    }

    constexpr LinearExtendedSRGBA()
        : LinearExtendedSRGBA { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const LinearExtendedSRGBA<T>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}

template<typename T> constexpr bool operator==(const LinearExtendedSRGBA<T>& a, const LinearExtendedSRGBA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const LinearExtendedSRGBA<T>& a, const LinearExtendedSRGBA<T>& b)
{
    return !(a == b);
}


template<typename T> struct DisplayP3 : ColorWithAlphaHelper<DisplayP3<T>> {
    using ComponentType = T;
    using Model = RGBModel<T>;
    static constexpr auto colorSpace = ColorSpace::DisplayP3;

    constexpr DisplayP3(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr DisplayP3()
        : DisplayP3 { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const DisplayP3<T>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}

template<typename T> constexpr bool operator==(const DisplayP3<T>& a, const DisplayP3<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const DisplayP3<T>& a, const DisplayP3<T>& b)
{
    return !(a == b);
}


template<typename T> struct LinearDisplayP3 : ColorWithAlphaHelper<LinearDisplayP3<T>> {
    using ComponentType = T;
    using Model = RGBModel<T>;

    constexpr LinearDisplayP3(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr LinearDisplayP3()
        : LinearDisplayP3 { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const LinearDisplayP3<T>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}

template<typename T> constexpr bool operator==(const LinearDisplayP3<T>& a, const LinearDisplayP3<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const LinearDisplayP3<T>& a, const LinearDisplayP3<T>& b)
{
    return !(a == b);
}


template<typename T> struct Lab : ColorWithAlphaHelper<Lab<T>> {
    using ComponentType = T;
    using Model = LabModel<T>;
    static constexpr auto colorSpace = ColorSpace::Lab;

    constexpr Lab(T lightness, T a, T b, T alpha = AlphaTraits<T>::opaque)
        : lightness { lightness }
        , a { a }
        , b { b }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr Lab()
        : Lab { 0, 0, 0, 0 }
    {
    }

    T lightness;
    T a;
    T b;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const Lab<T>& c)
{
    return { c.lightness, c.a, c.b, c.alpha };
}

template<typename T> constexpr bool operator==(const Lab<T>& a, const Lab<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const Lab<T>& a, const Lab<T>& b)
{
    return !(a == b);
}


template<typename T> struct LCHA : ColorWithAlphaHelper<LCHA<T>> {
    using ComponentType = T;
    using Model = LCHModel<T>;

    constexpr LCHA(T lightness, T chroma, T hue, T alpha = AlphaTraits<T>::opaque)
        : lightness { lightness }
        , chroma { chroma }
        , hue { hue }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr LCHA()
        : LCHA { 0, 0, 0, 0 }
    {
    }

    T lightness;
    T chroma;
    T hue;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const LCHA<T>& c)
{
    return { c.lightness, c.chroma, c.hue, c.alpha };
}

template<typename T> constexpr bool operator==(const LCHA<T>& a, const LCHA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const LCHA<T>& a, const LCHA<T>& b)
{
    return !(a == b);
}


template<typename T> struct HSLA : ColorWithAlphaHelper<HSLA<T>> {
    using ComponentType = T;
    using Model = HSLModel<T>;

    constexpr HSLA(T hue, T saturation, T lightness, T alpha = AlphaTraits<T>::opaque)
        : hue { hue }
        , saturation { saturation }
        , lightness { lightness }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr HSLA()
        : HSLA { 0, 0, 0, 0 }
    {
    }

    T hue;
    T saturation;
    T lightness;
    T alpha;
};

template<typename T> HSLA(T, T, T, T) -> HSLA<T>;

template<typename T> constexpr ColorComponents<T> asColorComponents(const HSLA<T>& c)
{
    return { c.hue, c.saturation, c.lightness, c.alpha };
}

template<typename T> constexpr bool operator==(const HSLA<T>& a, const HSLA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const HSLA<T>& a, const HSLA<T>& b)
{
    return !(a == b);
}


// FIXME: When ColorComponents supports more than length == 4, add conversion to/from ColorComponents<T> for CMYKA
template<typename T> struct CMYKA : ColorWithAlphaHelper<CMYKA<T>> {
    using ComponentType = T;
    using Model = CMYKModel<T>;

    constexpr CMYKA(T cyan, T magenta, T yellow, T black, T alpha = AlphaTraits<T>::opaque)
        : cyan { cyan }
        , magenta { magenta }
        , yellow { yellow }
        , black { black }
        , alpha { alpha }
    {
    }

    constexpr CMYKA()
        : CMYKA { 0, 0, 0, 0, 0 }
    {
    }

    T cyan;
    T magenta;
    T yellow;
    T black;
    T alpha;
};

template<typename T> constexpr bool operator==(const CMYKA<T>& a, const CMYKA<T>& b)
{
    return a.cyan == b.cyan && a.magenta == b.magenta && a.yellow == b.yellow && a.black == b.black && a.alpha == b.alpha;
}

template<typename T> constexpr bool operator!=(const CMYKA<T>& a, const CMYKA<T>& b)
{
    return !(a == b);
}


template<typename T> struct XYZA : ColorWithAlphaHelper<XYZA<T>> {
    using ComponentType = T;
    using Model = XYZModel<T>;

    constexpr XYZA(T x, T y, T z, T alpha = AlphaTraits<T>::opaque)
        : x { x }
        , y { y }
        , z { z }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr XYZA()
        : XYZA { 0, 0, 0, 0 }
    {
    }

    T x;
    T y;
    T z;
    T alpha;
};

template<typename T> constexpr ColorComponents<T> asColorComponents(const XYZA<T>& c)
{
    return { c.x, c.y, c.z, c.alpha };
}

template<typename T> constexpr bool operator==(const XYZA<T>& a, const XYZA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const XYZA<T>& a, const XYZA<T>& b)
{
    return !(a == b);
}


template<typename T, typename Functor> constexpr decltype(auto) callWithColorType(const ColorComponents<T>& components, ColorSpace colorSpace, Functor&& functor)
{
    switch (colorSpace) {
    case ColorSpace::SRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<SRGBA<T>>(components));
    case ColorSpace::LinearRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<LinearSRGBA<T>>(components));
    case ColorSpace::DisplayP3:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<DisplayP3<T>>(components));
    case ColorSpace::Lab:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<Lab<T>>(components));
    }

    ASSERT_NOT_REACHED();
    return std::invoke(std::forward<Functor>(functor), makeFromComponents<SRGBA<T>>(components));
}

// Packed Color Formats

namespace PackedColor {

struct RGBA {
    constexpr explicit RGBA(uint32_t rgba)
        : value { rgba }
    {
    }

    constexpr explicit RGBA(SRGBA<uint8_t> color)
        : value { static_cast<uint32_t>(color.red << 24 | color.green << 16 | color.blue << 8 | color.alpha) }
    {
    }

    uint32_t value;
};

struct ARGB {
    constexpr explicit ARGB(uint32_t argb)
        : value { argb }
    {
    }

    constexpr explicit ARGB(SRGBA<uint8_t> color)
        : value { static_cast<uint32_t>(color.alpha << 24 | color.red << 16 | color.green << 8 | color.blue) }
    {
    }

    uint32_t value;
};

}

constexpr SRGBA<uint8_t> asSRGBA(PackedColor::RGBA color)
{
    return { static_cast<uint8_t>(color.value >> 24), static_cast<uint8_t>(color.value >> 16), static_cast<uint8_t>(color.value >> 8), static_cast<uint8_t>(color.value) };
}

constexpr SRGBA<uint8_t> asSRGBA(PackedColor::ARGB color)
{
    return { static_cast<uint8_t>(color.value >> 16), static_cast<uint8_t>(color.value >> 8), static_cast<uint8_t>(color.value), static_cast<uint8_t>(color.value >> 24) };
}

} // namespace WebCore
