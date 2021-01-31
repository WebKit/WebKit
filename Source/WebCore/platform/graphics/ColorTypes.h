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
#include "ColorModels.h"
#include "ColorSpace.h"

namespace WebCore {

template<typename> struct A98RGB;
template<typename> struct DisplayP3;
template<typename> struct ExtendedSRGBA;
template<typename> struct HSLA;
template<typename> struct LCHA;
template<typename> struct Lab;
template<typename> struct LinearA98RGB;
template<typename> struct LinearDisplayP3;
template<typename> struct LinearExtendedSRGBA;
template<typename> struct LinearRec2020;
template<typename> struct LinearSRGBA;
template<typename> struct Rec2020;
template<typename> struct SRGBA;
template<typename> struct XYZ;

// MARK: Make functions.

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
    return { clampedComponent<ColorType, 0>(components), clampedComponent<ColorType, 1>(components), clampedComponent<ColorType, 2>(components), clampedAlpha(components[3]) };
}

template<typename ColorType, typename T> constexpr ColorComponents<T> clampedComponentsExceptAlpha(const ColorComponents<T>& components)
{
    return { clampedComponent<ColorType, 0>(components), clampedComponent<ColorType, 1>(components), clampedComponent<ColorType, 2>(components), components[3] };
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(const ColorComponents<T>& components)
{
    return makeFromComponents<ColorType>(clampedComponents<ColorType>(components));
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(T c1, T c2, T c3)
{
    return makeFromComponents<ColorType>(ColorComponents { clampedComponent<ColorType, 0>(c1), clampedComponent<ColorType, 1>(c2), clampedComponent<ColorType, 2>(c3), AlphaTraits<typename ColorType::ComponentType>::opaque });
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(T c1, T c2, T c3, T alpha)
{
    return makeFromComponents<ColorType>(ColorComponents { clampedComponent<ColorType, 0>(c1), clampedComponent<ColorType, 1>(c2), clampedComponent<ColorType, 2>(c3), clampedAlpha<T, typename ColorType::ComponentType>(alpha) });
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClampingExceptAlpha(const ColorComponents<T>& components)
{
    return makeFromComponents<ColorType>(clampedComponentsExceptAlpha<ColorType>(components));
}

template<typename ColorType, typename T, typename Alpha> constexpr ColorType makeFromComponentsClampingExceptAlpha(T c1, T c2, T c3, Alpha alpha)
{
    return makeFromComponents<ColorType>(ColorComponents { clampedComponent<ColorType, 0>(c1), clampedComponent<ColorType, 1>(c2), clampedComponent<ColorType, 2>(c3), alpha });
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


template<typename ColorType, typename std::enable_if_t<IsConvertibleToColorComponents<ColorType>>* = nullptr>
constexpr bool operator==(const ColorType& a, const ColorType& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename ColorType, typename std::enable_if_t<IsConvertibleToColorComponents<ColorType>>* = nullptr>
constexpr bool operator!=(const ColorType& a, const ColorType& b)
{
    return !(a == b);
}


// MARK: - RGB Color Types.

template<template<typename> class C, typename T, typename M> struct RGBAType : ColorWithAlphaHelper<C<T>> {
    using ComponentType = T;
    using Model = M;
    
    constexpr RGBAType(T red, T green, T blue, T alpha = AlphaTraits<T>::opaque)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr RGBAType()
        : RGBAType { 0, 0, 0, 0 }
    {
    }

    T red;
    T green;
    T blue;
    T alpha;
};

template<template<typename> class ColorType, typename T, typename M> constexpr ColorComponents<T> asColorComponents(const RGBAType<ColorType, T, M>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}


template<typename T> struct A98RGB : RGBAType<A98RGB, T, RGBModel<T>> {
    using RGBAType<A98RGB, T, RGBModel<T>>::RGBAType;
    using LinearCounterpart = LinearA98RGB<T>;
    static constexpr auto colorSpace = ColorSpace::A98RGB;
};
template<typename T> A98RGB(T, T, T, T) -> A98RGB<T>;

template<typename T> struct DisplayP3 : RGBAType<DisplayP3, T, RGBModel<T>> {
    using RGBAType<DisplayP3, T, RGBModel<T>>::RGBAType;
    using LinearCounterpart = LinearDisplayP3<T>;
    static constexpr auto colorSpace = ColorSpace::DisplayP3;
};
template<typename T> DisplayP3(T, T, T, T) -> DisplayP3<T>;

template<typename T> struct ExtendedSRGBA : RGBAType<ExtendedSRGBA, T, ExtendedRGBModel<T>> {
    using RGBAType<ExtendedSRGBA, T, ExtendedRGBModel<T>>::RGBAType;
    using LinearCounterpart = LinearExtendedSRGBA<T>;
};
template<typename T> ExtendedSRGBA(T, T, T, T) -> ExtendedSRGBA<T>;

template<typename T> struct LinearA98RGB : RGBAType<LinearA98RGB, T, RGBModel<T>> {
    using RGBAType<LinearA98RGB, T, RGBModel<T>>::RGBAType;
    using GammaEncodedCounterpart = A98RGB<T>;
};
template<typename T> LinearA98RGB(T, T, T, T) -> LinearA98RGB<T>;

template<typename T> struct LinearDisplayP3 : RGBAType<LinearDisplayP3, T, RGBModel<T>> {
    using RGBAType<LinearDisplayP3, T, RGBModel<T>>::RGBAType;
    using GammaEncodedCounterpart = DisplayP3<T>;
};
template<typename T> LinearDisplayP3(T, T, T, T) -> LinearDisplayP3<T>;

template<typename T> struct LinearExtendedSRGBA : RGBAType<LinearExtendedSRGBA, T, ExtendedRGBModel<T>> {
    using RGBAType<LinearExtendedSRGBA, T, ExtendedRGBModel<T>>::RGBAType;
    using GammaEncodedCounterpart = ExtendedSRGBA<T>;
};
template<typename T> LinearExtendedSRGBA(T, T, T, T) -> LinearExtendedSRGBA<T>;

template<typename T> struct LinearRec2020 : RGBAType<LinearRec2020, T, RGBModel<T>> {
    using RGBAType<LinearRec2020, T, RGBModel<T>>::RGBAType;
    using GammaEncodedCounterpart = Rec2020<T>;
};
template<typename T> LinearRec2020(T, T, T, T) -> LinearRec2020<T>;

template<typename T> struct LinearSRGBA : RGBAType<LinearSRGBA, T, RGBModel<T>> {
    using RGBAType<LinearSRGBA, T, RGBModel<T>>::RGBAType;
    using GammaEncodedCounterpart = SRGBA<T>;
    static constexpr auto colorSpace = ColorSpace::LinearRGB;
};
template<typename T> LinearSRGBA(T, T, T, T) -> LinearSRGBA<T>;

template<typename T> struct Rec2020 : RGBAType<Rec2020, T, RGBModel<T>> {
    using RGBAType<Rec2020, T, RGBModel<T>>::RGBAType;
    using LinearCounterpart = LinearRec2020<T>;
    static constexpr auto colorSpace { ColorSpace::Rec2020 };
};
template<typename T> Rec2020(T, T, T, T) -> Rec2020<T>;

template<typename T> struct SRGBA : RGBAType<SRGBA, T, RGBModel<T>> {
    using RGBAType<SRGBA, T, RGBModel<T>>::RGBAType;
    using LinearCounterpart = LinearSRGBA<T>;
    static constexpr auto colorSpace { ColorSpace::SRGB };
};
template<typename T> SRGBA(T, T, T, T) -> SRGBA<T>;

// MARK: - Lab Color Type.

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

// MARK: - LCHA Color Type.

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


// MARK: - HSLA Color Type.

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

template<typename T> constexpr ColorComponents<T> asColorComponents(const HSLA<T>& c)
{
    return { c.hue, c.saturation, c.lightness, c.alpha };
}

// MARK: - XYZ Color Type.

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
