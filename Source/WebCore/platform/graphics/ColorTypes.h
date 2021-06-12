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

#pragma once

#include "ColorComponents.h"
#include "ColorMatrix.h"
#include "ColorModels.h"
#include "ColorTransferFunctions.h"

namespace WebCore {

enum class WhitePoint { D50, D65 };

template<typename, typename> struct BoundedGammaEncoded;
template<typename, typename> struct BoundedLinearEncoded;
template<typename, typename> struct ExtendedGammaEncoded;
template<typename, typename> struct ExtendedLinearEncoded;
template<typename> struct HSLA;
template<typename> struct HWBA;
template<typename> struct LCHA;
template<typename> struct Lab;
template<typename> struct Oklab;
template<typename> struct Oklch;
template<typename, WhitePoint> struct XYZA;

// MARK: Make functions.

template<typename ColorType, typename T> constexpr ColorType makeFromComponents(const ColorComponents<T, 4>& c)
{
    return ColorType { c[0], c[1], c[2], c[3] };
}

template<typename ColorType, unsigned Index, typename T> constexpr auto clampedComponent(T c) -> typename ColorType::ComponentType
{
    static_assert(std::is_integral_v<T>);

    constexpr auto componentInfo = ColorType::Model::componentInfo[Index];
    return std::clamp<T>(c, componentInfo.min, componentInfo.max);
}

template<typename ColorType, unsigned Index> constexpr float clampedComponent(float c)
{
    constexpr auto componentInfo = ColorType::Model::componentInfo[Index];

    if constexpr (componentInfo.min == -std::numeric_limits<float>::infinity() && componentInfo.max == std::numeric_limits<float>::infinity())
        return c;

    if constexpr (componentInfo.min == -std::numeric_limits<float>::infinity())
        return std::min(c, componentInfo.max);

    if constexpr (componentInfo.max == std::numeric_limits<float>::infinity())
        return std::max(c, componentInfo.min);

    return std::clamp(c, componentInfo.min, componentInfo.max);
}

template<typename ColorType, unsigned Index, typename T> constexpr T clampedComponent(const ColorComponents<T, 4>& c)
{
    return clampedComponent<ColorType, Index>(c[Index]);
}

template<typename T, typename ComponentType = T> constexpr ComponentType clampedAlpha(T alpha)
{
    return std::clamp<T>(alpha, AlphaTraits<ComponentType>::transparent, AlphaTraits<ComponentType>::opaque);
}

template<typename ColorType, typename T> constexpr ColorComponents<T, 4> clampedComponents(const ColorComponents<T, 4>& components)
{
    return { clampedComponent<ColorType, 0>(components), clampedComponent<ColorType, 1>(components), clampedComponent<ColorType, 2>(components), clampedAlpha(components[3]) };
}

template<typename ColorType, typename T> constexpr ColorComponents<T, 4> clampedComponentsExceptAlpha(const ColorComponents<T, 4>& components)
{
    return { clampedComponent<ColorType, 0>(components), clampedComponent<ColorType, 1>(components), clampedComponent<ColorType, 2>(components), components[3] };
}

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClamping(const ColorComponents<T, 4>& components)
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

template<typename ColorType, typename T> constexpr ColorType makeFromComponentsClampingExceptAlpha(const ColorComponents<T, 4>& components)
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
            ASSERT_WITH_MESSAGE(components[i] >= T::Model::componentInfo[i].min, "Component at index %d is %f and is less than the allowed minimum %f", i,  components[i], T::Model::componentInfo[i].min);
            ASSERT_WITH_MESSAGE(components[i] <= T::Model::componentInfo[i].max, "Component at index %d is %f and is greater than the allowed maximum %f", i,  components[i], T::Model::componentInfo[i].max);
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

template<typename, typename = void> inline constexpr bool IsConvertibleToColorComponents = false;
template<typename T> inline constexpr bool IsConvertibleToColorComponents<T, std::void_t<decltype(asColorComponents(std::declval<T>()))>> = true;

template<typename, typename = void> inline constexpr bool HasComponentTypeMember = false;
template<typename T> inline constexpr bool HasComponentTypeMember<T, std::void_t<typename T::ComponentType>> = true;

template<typename T, typename U, bool enabled> inline constexpr bool HasComponentTypeValue = false;
template<typename T, typename U> inline constexpr bool HasComponentTypeValue<T, U, true> = std::is_same_v<typename T::ComponentType, U>;
template<typename T, typename U> inline constexpr bool HasComponentType = HasComponentTypeValue<T, U, HasComponentTypeMember<T>>;

template<typename T> inline constexpr bool IsColorType = IsConvertibleToColorComponents<T> && HasComponentTypeMember<T>;
template<typename T, typename U> inline constexpr bool IsColorTypeWithComponentType = IsConvertibleToColorComponents<T> && HasComponentType<T, U>;

template<template<typename> class ColorType, typename Replacement> struct ColorTypeReplacingComponentTypeHelper { using type = ColorType<Replacement>; };
template<template<typename> class ColorType, typename Replacement> using ColorTypeReplacingComponentType = typename ColorTypeReplacingComponentTypeHelper<ColorType, Replacement>::type;

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

template<typename T, typename D, typename ColorType, typename M, typename TF> struct RGBAType : ColorWithAlphaHelper<ColorType> {
    using ComponentType = T;
    using Model = M;
    using TransferFunction = TF;
    using Descriptor = D;
    static constexpr WhitePoint whitePoint = D::whitePoint;

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

template<typename T, typename D, typename ColorType, typename M, typename TF> constexpr ColorComponents<T, 4> asColorComponents(const RGBAType<T, D, ColorType, M, TF>& c)
{
    return { c.red, c.green, c.blue, c.alpha };
}


template<typename T, typename D>
struct BoundedGammaEncoded : RGBAType<T, D, BoundedGammaEncoded<T, D>, RGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Clamped>> {
    using RGBAType<T, D, BoundedGammaEncoded<T, D>, RGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Clamped>>::RGBAType;

    using LinearCounterpart = BoundedLinearEncoded<T, D>;
    using ExtendedCounterpart = ExtendedGammaEncoded<T, D>;
    
    template<typename Replacement> using SelfWithReplacementComponent = BoundedGammaEncoded<Replacement, D>;
};

template<typename T, typename D>
struct BoundedLinearEncoded : RGBAType<T, D, BoundedLinearEncoded<T, D>, RGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Clamped>> {
    using RGBAType<T, D, BoundedLinearEncoded<T, D>, RGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Clamped>>::RGBAType;

    static constexpr auto linearToXYZ = D::linearToXYZ;
    static constexpr auto xyzToLinear = D::xyzToLinear;

    using GammaEncodedCounterpart = BoundedGammaEncoded<T, D>;
    using ExtendedCounterpart = ExtendedLinearEncoded<T, D>;

    template<typename Replacement> using SelfWithReplacementComponent = BoundedLinearEncoded<Replacement, D>;
};

template<typename T, typename D>
struct ExtendedGammaEncoded : RGBAType<T, D, ExtendedGammaEncoded<T, D>, ExtendedRGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Unclamped>> {
    using RGBAType<T, D, ExtendedGammaEncoded<T, D>, ExtendedRGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Unclamped>>::RGBAType;

    using LinearCounterpart = ExtendedLinearEncoded<T, D>;
    using BoundedCounterpart = BoundedGammaEncoded<T, D>;
    using Reference = LinearCounterpart;
};

template<typename T, typename D>
struct ExtendedLinearEncoded : RGBAType<T, D, ExtendedLinearEncoded<T, D>, ExtendedRGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Unclamped>> {
    using RGBAType<T, D, ExtendedLinearEncoded<T, D>, ExtendedRGBModel<T>, typename D::template TransferFunction<T, TransferFunctionMode::Unclamped>>::RGBAType;

    static constexpr auto linearToXYZ = D::linearToXYZ;
    static constexpr auto xyzToLinear = D::xyzToLinear;

    using GammaEncodedCounterpart = ExtendedGammaEncoded<T, D>;
    using BoundedCounterpart = BoundedLinearEncoded<T, D>;
    using Reference = XYZA<T, D::whitePoint>;
};

template<typename, typename = void> inline constexpr bool HasDescriptorMember = false;
template<typename ColorType> inline constexpr bool HasDescriptorMember<ColorType, std::void_t<typename ColorType::Descriptor>> = true;

template<typename, typename = void> inline constexpr bool HasExtendedCounterpartMember = false;
template<typename ColorType> inline constexpr bool HasExtendedCounterpartMember<ColorType, std::void_t<typename ColorType::ExtendedCounterpart>> = true;

template<typename, typename = void> inline constexpr bool HasBoundedCounterpartMember = false;
template<typename ColorType> inline constexpr bool HasBoundedCounterpartMember<ColorType, std::void_t<typename ColorType::BoundedCounterpart>> = true;

template<typename, typename = void> inline constexpr bool HasGammaEncodedCounterpartMember = false;
template<typename ColorType> inline constexpr bool HasGammaEncodedCounterpartMember<ColorType, std::void_t<typename ColorType::GammaEncodedCounterpart>> = true;

template<typename, typename = void> inline constexpr bool HasLinearCounterpartMember = false;
template<typename ColorType> inline constexpr bool HasLinearCounterpartMember<ColorType, std::void_t<typename ColorType::LinearCounterpart>> = true;

template<typename, typename = void> inline constexpr bool HasSelfWithReplacementComponentMember = false;
template<typename ColorType> inline constexpr bool HasSelfWithReplacementComponentMember<ColorType, std::void_t<typename ColorType::SelfWithReplacementComponent>> = true;

template<typename ColorType, typename Replacement> using ColorTypeWithReplacementComponent = typename ColorType::template SelfWithReplacementComponent<Replacement>;

template<typename ColorType> inline constexpr bool IsRGBType = HasDescriptorMember<ColorType>;
template<typename ColorType> inline constexpr bool IsRGBExtendedType = IsRGBType<ColorType> && HasBoundedCounterpartMember<ColorType>;
template<typename ColorType> inline constexpr bool IsRGBBoundedType = IsRGBType<ColorType> && HasExtendedCounterpartMember<ColorType>;
template<typename ColorType> inline constexpr bool IsRGBGammaEncodedType = IsRGBType<ColorType> && HasLinearCounterpartMember<ColorType>;
template<typename ColorType> inline constexpr bool IsRGBLinearEncodedType = IsRGBType<ColorType> && HasGammaEncodedCounterpartMember<ColorType>;

template<typename ColorType1, typename ColorType2, bool enabled> inline constexpr bool IsSameRGBTypeFamilyValue = false;
template<typename ColorType1, typename ColorType2> inline constexpr bool IsSameRGBTypeFamilyValue<ColorType1, ColorType2, true> = std::is_same_v<typename ColorType1::Descriptor, typename ColorType2::Descriptor>;
template<typename ColorType1, typename ColorType2> inline constexpr bool IsSameRGBTypeFamily = IsSameRGBTypeFamilyValue<ColorType1, ColorType2, IsRGBType<ColorType1> && IsRGBType<ColorType2>>;

struct SRGBADescriptor {
    template<typename T, TransferFunctionMode Mode> using TransferFunction = SRGBTransferFunction<T, Mode>;
    static constexpr WhitePoint whitePoint = WhitePoint::D65;

    // https://drafts.csswg.org/css-color/#color-conversion-code
    static constexpr ColorMatrix<3, 3> xyzToLinear {
         3.2409699419045226f,  -1.537383177570094f,   -0.4986107602930034f,
        -0.9692436362808796f,   1.8759675015077202f,   0.04155505740717559f,
         0.05563007969699366f, -0.20397695888897652f,  1.0569715142428786f
    };
    static constexpr ColorMatrix<3, 3> linearToXYZ {
        0.41239079926595934f, 0.357584339383878f,   0.1804807884018343f,
        0.21263900587151027f, 0.715168678767756f,   0.07219231536073371f,
        0.01933081871559182f, 0.11919477979462598f, 0.9505321522496607f
    };
};

template<typename T> using SRGBA = BoundedGammaEncoded<T, SRGBADescriptor>;
template<typename T> using LinearSRGBA = BoundedLinearEncoded<T, SRGBADescriptor>;
template<typename T> using ExtendedSRGBA = ExtendedGammaEncoded<T, SRGBADescriptor>;
template<typename T> using LinearExtendedSRGBA = ExtendedLinearEncoded<T, SRGBADescriptor>;


struct A98RGBDescriptor {
    template<typename T, TransferFunctionMode Mode> using TransferFunction = A98RGBTransferFunction<T, Mode>;
    static constexpr WhitePoint whitePoint = WhitePoint::D65;

    // https://drafts.csswg.org/css-color/#color-conversion-code
    static constexpr ColorMatrix<3, 3> xyzToLinear {
         2.493496911941425f,  -0.9313836179191239f, -0.4027107844507168f,
        -0.8294889695615747f,  1.7626640603183463f,  0.0236246858419436f,
         0.0358458302437845f, -0.0761723892680418f,  0.9568845240076872f
    };
    static constexpr ColorMatrix<3, 3> linearToXYZ {
        0.5766690429101305f,   0.1855582379065463f,   0.1882286462349947f,
        0.29734497525053605f,  0.6273635662554661f,   0.07529145849399788f,
        0.02703136138641234f,  0.07068885253582723f,  0.9913375368376388f
    };
};

template<typename T> using A98RGB = BoundedGammaEncoded<T, A98RGBDescriptor>;
template<typename T> using LinearA98RGB = BoundedLinearEncoded<T, A98RGBDescriptor>;


struct DisplayP3Descriptor {
    template<typename T, TransferFunctionMode Mode> using TransferFunction = SRGBTransferFunction<T, Mode>;
    static constexpr WhitePoint whitePoint = WhitePoint::D65;

    // https://drafts.csswg.org/css-color/#color-conversion-code
    static constexpr ColorMatrix<3, 3> xyzToLinear {
         2.493496911941425f,  -0.9313836179191239f, -0.4027107844507168f,
        -0.8294889695615747f,  1.7626640603183463f,  0.0236246858419436f,
         0.0358458302437845f, -0.0761723892680418f,  0.9568845240076872f
    };
    static constexpr ColorMatrix<3, 3> linearToXYZ {
        0.4865709486482162f, 0.2656676931690931f, 0.198217285234363f,
        0.2289745640697488f, 0.6917385218365064f, 0.079286914093745f,
        0.0f,                0.0451133818589026f, 1.043944368900976f
    };
};

template<typename T> using DisplayP3 = BoundedGammaEncoded<T, DisplayP3Descriptor>;
template<typename T> using LinearDisplayP3 = BoundedLinearEncoded<T, DisplayP3Descriptor>;


struct ProPhotoRGBDescriptor {
    template<typename T, TransferFunctionMode Mode> using TransferFunction = ProPhotoRGBTransferFunction<T, Mode>;
    static constexpr WhitePoint whitePoint = WhitePoint::D50;

    // https://drafts.csswg.org/css-color/#color-conversion-code
    static constexpr ColorMatrix<3, 3> xyzToLinear {
         1.3457989731028281f,  -0.25558010007997534f,  -0.05110628506753401f,
        -0.5446224939028347f,   1.5082327413132781f,    0.02053603239147973f,
         0.0f,                  0.0f,                   1.2119675456389454f
    };
    static constexpr ColorMatrix<3, 3> linearToXYZ {
        0.7977604896723027f,  0.13518583717574031f,  0.0313493495815248f,
        0.2880711282292934f,  0.7118432178101014f,   0.00008565396060525902f,
        0.0f,                 0.0f,                  0.8251046025104601f
    };
};

template<typename T> using ProPhotoRGB = BoundedGammaEncoded<T, ProPhotoRGBDescriptor>;
template<typename T> using LinearProPhotoRGB = BoundedLinearEncoded<T, ProPhotoRGBDescriptor>;


struct Rec2020Descriptor {
    template<typename T, TransferFunctionMode Mode> using TransferFunction = Rec2020TransferFunction<T, Mode>;
    static constexpr WhitePoint whitePoint = WhitePoint::D65;

    // https://drafts.csswg.org/css-color/#color-conversion-code
    static constexpr ColorMatrix<3, 3> xyzToLinear {
         1.7166511879712674f,   -0.35567078377639233f, -0.25336628137365974f,
        -0.6666843518324892f,    1.6164812366349395f,   0.01576854581391113f,
         0.017639857445310783f, -0.042770613257808524f, 0.9421031212354738f
    };
    static constexpr ColorMatrix<3, 3> linearToXYZ {
        0.6369580483012914f, 0.14461690358620832f,  0.1688809751641721f,
        0.2627002120112671f, 0.6779980715188708f,   0.05930171646986196f,
        0.000000000000000f,  0.028072693049087428f, 1.060985057710791f
    };
};

template<typename T> using Rec2020 = BoundedGammaEncoded<T, Rec2020Descriptor>;
template<typename T> using LinearRec2020 = BoundedLinearEncoded<T, Rec2020Descriptor>;


// MARK: - Lab Color Type.

template<typename T> struct Lab : ColorWithAlphaHelper<Lab<T>> {
    using ComponentType = T;
    using Model = LabModel<T>;
    static constexpr WhitePoint whitePoint = WhitePoint::D50;
    using Reference =  XYZA<T, whitePoint>;

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

template<typename T> constexpr ColorComponents<T, 4> asColorComponents(const Lab<T>& c)
{
    return { c.lightness, c.a, c.b, c.alpha };
}

template<typename ColorType> inline constexpr bool IsLab = std::is_same_v<Lab<typename ColorType::ComponentType>, ColorType>;

// MARK: - LCHA Color Type.

template<typename T> struct LCHA : ColorWithAlphaHelper<LCHA<T>> {
    using ComponentType = T;
    using Model = LCHModel<T>;
    static constexpr WhitePoint whitePoint = WhitePoint::D50;
    using Reference = Lab<T>;

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

template<typename T> constexpr ColorComponents<T, 4> asColorComponents(const LCHA<T>& c)
{
    return { c.lightness, c.chroma, c.hue, c.alpha };
}

template<typename ColorType> inline constexpr bool IsLCHA = std::is_same_v<LCHA<typename ColorType::ComponentType>, ColorType>;


// MARK: - HSLA Color Type.

template<typename T> struct HSLA : ColorWithAlphaHelper<HSLA<T>> {
    using ComponentType = T;
    using Model = HSLModel<T>;
    static constexpr WhitePoint whitePoint = WhitePoint::D65;
    using Reference = SRGBA<T>;

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

template<typename T> constexpr ColorComponents<T, 4> asColorComponents(const HSLA<T>& c)
{
    return { c.hue, c.saturation, c.lightness, c.alpha };
}

template<typename ColorType> inline constexpr bool IsHSLA = std::is_same_v<HSLA<typename ColorType::ComponentType>, ColorType>;

// MARK: - HWBA Color Type.

template<typename T> struct HWBA : ColorWithAlphaHelper<HWBA<T>> {
    using ComponentType = T;
    using Model = HWBModel<T>;
    static constexpr WhitePoint whitePoint = WhitePoint::D65;
    using Reference = SRGBA<T>;

    constexpr HWBA(T hue, T whiteness, T blackness, T alpha = AlphaTraits<T>::opaque)
        : hue { hue }
        , whiteness { whiteness }
        , blackness { blackness }
        , alpha { alpha }
    {
        assertInRange(*this);
    }

    constexpr HWBA()
        : HWBA { 0, 0, 0, 0 }
    {
    }

    T hue;
    T whiteness;
    T blackness;
    T alpha;
};

template<typename T> constexpr ColorComponents<T, 4> asColorComponents(const HWBA<T>& c)
{
    return { c.hue, c.whiteness, c.blackness, c.alpha };
}

template<typename ColorType> inline constexpr bool IsHWBA = std::is_same_v<HWBA<typename ColorType::ComponentType>, ColorType>;

// MARK: - XYZ Color Type.

template<typename T, WhitePoint W> struct XYZA : ColorWithAlphaHelper<XYZA<T, W>> {
    using ComponentType = T;
    using Model = XYZModel<T>;
    using ReferenceXYZ = XYZA<T, W>;
    static constexpr WhitePoint whitePoint = W;

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

template<typename T, WhitePoint W> constexpr ColorComponents<T, 4> asColorComponents(const XYZA<T, W>& c)
{
    return { c.x, c.y, c.z, c.alpha };
}

template<typename ColorType> inline constexpr bool IsXYZA = std::is_same_v<XYZA<typename ColorType::ComponentType, ColorType::whitePoint>, ColorType>;

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
