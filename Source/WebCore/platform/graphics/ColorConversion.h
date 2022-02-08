/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ColorTypes.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class DestinationColorSpace;

enum class ColorSpace : uint8_t;

// Conversion function for typed colors.
template<typename Output, typename Input> Output convertColor(const Input& color);

// Conversion functions for raw color components with associated color spaces.
ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, ColorSpace outputColorSpace);
ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, const DestinationColorSpace& outputColorSpace);


// All color types, other than XYZA or those inheriting from RGBType, must implement
// the following conversions to and from their "Reference" color.
//
//    template<> struct ColorConversion<`ColorType`<float>::Reference, `ColorType`<float>> {
//        WEBCORE_EXPORT static `ColorType`<float>::Reference convert(const `ColorType`<float>&);
//    };
//    
//    template<> struct ColorConversion<`ColorType`<float>, `ColorType`<float>::Reference> {
//        WEBCORE_EXPORT static `ColorType`<float> convert(const `ColorType`<float>::Reference&);
//    };
//

template<typename Output, typename Input, typename = void> struct ColorConversion;

template<typename Output, typename Input> inline Output convertColor(const Input& color)
{
    return ColorConversion<CanonicalColorType<Output>, CanonicalColorType<Input>>::convert(color);
}


// MARK: Chromatic Adaptation conversions.

// http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
template<WhitePoint From, WhitePoint To> struct ChromaticAdapation;

template<> struct ChromaticAdapation<WhitePoint::D65, WhitePoint::D50> {
    static constexpr ColorMatrix<3, 3> matrix {
         1.0478112f, 0.0228866f, -0.0501270f,
         0.0295424f, 0.9904844f, -0.0170491f,
        -0.0092345f, 0.0150436f,  0.7521316f
    };
};

template<> struct ChromaticAdapation<WhitePoint::D50, WhitePoint::D65> {
    static constexpr ColorMatrix<3, 3> matrix {
         0.9555766f, -0.0230393f, 0.0631636f,
        -0.0282895f,  1.0099416f, 0.0210077f,
         0.0122982f, -0.0204830f, 1.3299098f
    };
};

// MARK: HSLA
template<> struct ColorConversion<SRGBA<float>, HSLA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const HSLA<float>&);
};
template<> struct ColorConversion<HSLA<float>, SRGBA<float>> {
    WEBCORE_EXPORT static HSLA<float> convert(const SRGBA<float>&);
};

// MARK: HWBA
template<> struct ColorConversion<SRGBA<float>, HWBA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const HWBA<float>&);
};
template<> struct ColorConversion<HWBA<float>, SRGBA<float>> {
    WEBCORE_EXPORT static HWBA<float> convert(const SRGBA<float>&);
};

// MARK: LCHA
template<> struct ColorConversion<Lab<float>, LCHA<float>> {
    WEBCORE_EXPORT static Lab<float> convert(const LCHA<float>&);
};
template<> struct ColorConversion<LCHA<float>, Lab<float>> {
    WEBCORE_EXPORT static LCHA<float> convert(const Lab<float>&);
};

// MARK: Lab
template<> struct ColorConversion<XYZA<float, WhitePoint::D50>, Lab<float>> {
    WEBCORE_EXPORT static XYZA<float, WhitePoint::D50> convert(const Lab<float>&);
};
template<> struct ColorConversion<Lab<float>, XYZA<float, WhitePoint::D50>> {
    WEBCORE_EXPORT static Lab<float> convert(const XYZA<float, WhitePoint::D50>&);
};

// MARK: OKLCHA
template<> struct ColorConversion<OKLab<float>, OKLCHA<float>> {
    WEBCORE_EXPORT static OKLab<float> convert(const OKLCHA<float>&);
};
template<> struct ColorConversion<OKLCHA<float>, OKLab<float>> {
    WEBCORE_EXPORT static OKLCHA<float> convert(const OKLab<float>&);
};

// MARK: OKLab
template<> struct ColorConversion<XYZA<float, WhitePoint::D65>, OKLab<float>> {
    WEBCORE_EXPORT static XYZA<float, WhitePoint::D65> convert(const OKLab<float>&);
};
template<> struct ColorConversion<OKLab<float>, XYZA<float, WhitePoint::D65>> {
    WEBCORE_EXPORT static OKLab<float> convert(const XYZA<float, WhitePoint::D65>&);
};

// Identity conversion.

template<typename ColorType> struct ColorConversion<ColorType, ColorType> {
    static ColorType convert(const ColorType& color)
    {
        return color;
    }
};

// MARK: DeltaE color difference algorithms.

template<typename ColorType1, typename ColorType2> inline constexpr float computeDeltaEOK(ColorType1 color1, ColorType2 color2)
{
    // https://drafts.csswg.org/css-color/#color-difference-OK

    auto [L1, a1, b1, alpha1] = convertColor<OKLab<float>>(color1).resolved();
    auto [L2, a2, b2, alpha2] = convertColor<OKLab<float>>(color2).resolved();

    auto deltaL = (L1 / 100.0f) - (L2 / 100.0f);
    auto deltaA = a1 - a2;
    auto deltaB = b1 - b2;

    return std::hypot(deltaL, deltaA, deltaB);
}

// MARK: Gamut mapping algorithms.

struct ClipGamutMapping {
    template<typename ColorType> static auto mapToBoundedGamut(const ColorType& color) -> typename ColorType::BoundedCounterpart
    {
        return clipToGamut<typename ColorType::BoundedCounterpart>(color);
    }
};

struct CSSGamutMapping {
    // This implements the CSS gamut mapping algorithm (https://drafts.csswg.org/css-color/#css-gamut-mapping) for RGB
    // colors that are out of gamut for a particular RGB color space. It implements a relative colorimetric intent mapping
    // for colors that are outside the destionation gamut and leaves colors inside the destination gamut unchanged.

    // A simple optimization over the psuedocode in the specification has been made to avoid unnecessary work in the
    // main bisection loop by checking the gamut using the extended linear color space of the RGB family regardless of
    // of whether the final type is gamma encoded or not. This avoids unnecessary gamma encoding for non final loops.

    // FIXME: This is a naive iterative solution that works for any bounded RGB color space. This can be optimized by
    // using an analytical solution that computes the exact intersection.

    static constexpr float JND = 0.02f;

    template<typename ColorType> static auto mapToBoundedGamut(const ColorType& color) -> typename ColorType::BoundedCounterpart
    {
        using BoundedColorType = typename ColorType::BoundedCounterpart;
        using ExtendedLinearColorType = ExtendedLinearEncoded<typename BoundedColorType::ComponentType, typename BoundedColorType::Descriptor>;
        using BoundedLinearColorType = BoundedLinearEncoded<typename BoundedColorType::ComponentType, typename BoundedColorType::Descriptor>;

        auto resolvedColor = color.resolved();

        if (auto result = colorIfInGamut<BoundedColorType>(resolvedColor))
            return *result;

        auto colorInOKLCHColorSpace = convertColor<OKLCHA<float>>(resolvedColor).resolved();

        if (WTF::areEssentiallyEqual(colorInOKLCHColorSpace.lightness, 100.0f) || colorInOKLCHColorSpace.lightness > 100.0f)
            return { 1.0, 1.0, 1.0, resolvedColor.alpha };
        else if (WTF::areEssentiallyEqual(colorInOKLCHColorSpace.lightness, 0.0f))
            return { 0.0f, 0.0f, 0.0f, resolvedColor.alpha };

        float min = 0.0f;
        float max = colorInOKLCHColorSpace.chroma;

        while (true) {
            auto chroma = (min + max) / 2.0f;

            auto current = colorInOKLCHColorSpace;
            current.chroma = chroma;

            auto currentInExtendedLinearColorSpace = convertColor<ExtendedLinearColorType>(current).resolved();

            if (inGamut<BoundedLinearColorType>(currentInExtendedLinearColorSpace)) {
                min = chroma;
                continue;
            }

            auto currentClippedToBoundedLinearColorSpace = clipToGamut<BoundedLinearColorType>(currentInExtendedLinearColorSpace);

            auto deltaE = computeDeltaEOK(currentClippedToBoundedLinearColorSpace, current);
            if (deltaE < JND)
                return convertColor<BoundedColorType>(currentClippedToBoundedLinearColorSpace);

            max = chroma;
        }
    }
};

// Main conversion.

//                ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
//                 Matrix Conversions    ┌───────────┐│┌───────────┐
//                │                      │ XYZ (D50) │││ XYZ (D65) │                                                                                                                      │
//                                       └─────▲─────┘│└─────▲─────┘
//                │                            │      │      │                                                                                                                            │
//       ┌─────────────────────────┬───────────┘      │      └───────────┬───────────────────────────────┬───────────────────────────────┬───────────────────────────────┬─────────────────────────┐
//       │        │                │                  │                  │                               │                               │                               │                │        │
//       │                         │                  │                  │                               │                               │                               │                         │
//       │        │                │                  │                  │                               │                               │                               │                │        │
//       │          ProPhotoRGB───────────────────┐   │   SRGB──────────────────────────┐ DisplayP3─────────────────────┐ A98RGB────────────────────────┐ Rec2020───────────────────────┐          │
//       │        │ │           ┌────────────────┐│   │   │           ┌────────────────┐│ │           ┌────────────────┐│ │           ┌────────────────┐│ │           ┌────────────────┐│ │        │
//       │          │     ┌─────▶︎ LinearExtended ││   │   │     ┌─────▶︎ LinearExtended ││ │     ┌─────▶︎ LinearExtended ││ │     ┌─────▶︎ LinearExtended ││ │     ┌─────▶︎ LinearExtended ││          │
//       │        │ │     │     └────────▲───────┘│   │   │     │     └────────▲───────┘│ │     │     └────────▲───────┘│ │     │     └────────▲───────┘│ │     │     └────────▲───────┘│ │        │
//       │         ─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─ ─│─ ─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─         │
//       │          │┌────────┐          │        │   │   │┌────────┐          │        │ │┌────────┐          │        │ │┌────────┐          │        │ │┌────────┐          │        │          │
//       │          ││ Linear │          │        │   │   ││ Linear │          │        │ ││ Linear │          │        │ ││ Linear │          │        │ ││ Linear │          │        │          │
//       │          │└────▲───┘          │        │   │   │└────▲───┘          │        │ │└────▲───┘          │        │ │└────▲───┘          │        │ │└────▲───┘          │        │          │
//       │          │     │              │        │   │   │     │              │        │ │     │              │        │ │     │              │        │ │     │              │        │          │
// ┌───────────┐    │┌────────┐ ┌────────────────┐│   │   │┌────────┐ ┌────────────────┐│ │┌────────┐ ┌────────────────┐│ │┌────────┐ ┌────────────────┐│ │┌────────┐ ┌────────────────┐│    ┌───────────┐
// │    Lab    │    ││ Gamma  │─│ GammaExtended  ││   │   ││ Gamma  │─│ GammaExtended  ││ ││ Gamma  │─│ GammaExtended  ││ ││ Gamma  │─│ GammaExtended  ││ ││ Gamma  │─│ GammaExtended  ││    │   OKLab   │
// └─────▲─────┘    │└────────┘ └────────────────┘│   │   │└────▲───┘ └────────────────┘│ │└────────┘ └────────────────┘│ │└────────┘ └────────────────┘│ │└────────┘ └────────────────┘│    └─────▲─────┘
//       │          └─────────────────────────────┘   │   └─────┼───────────────────────┘ └─────────────────────────────┘ └─────────────────────────────┘ └─────────────────────────────┘          │
//       │                                            │      ┌──┴──────────┐                                                                                                                       │
//       │                                            │      │             │                                                                                                                       │
// ┌───────────┐                                      │┌───────────┐ ┌───────────┐                                                                                                           ┌───────────┐
// │    LCH    │                                      ││    HSL    │ │    HWB    │                                                                                                           │   OKLCH   │
// └───────────┘                                      │└───────────┘ └───────────┘                                                                                                           └───────────┘

template<typename Output, typename Input, typename> struct ColorConversion {
public:
    static constexpr Output convert(const Input& color)
    {
        // 1. Handle the special case of Input or Output with a uint8_t component type.
        if constexpr (std::is_same_v<typename Input::ComponentType, uint8_t>)
            return handleToFloatConversion(color);
        else if constexpr (std::is_same_v<typename Output::ComponentType, uint8_t>)
            return handleToByteConversion(color);

        // 2. Handle all color types that are not IsRGBType<T> or IsXYZA<T> for Input and Output. For all
        //    these other color types, we can uncondtionally convert them to their "reference" color, as
        //    either they have already been handled by a ColorConversion specialization or this will
        //    get us closer to the final conversion.
        else if constexpr (!IsRGBType<Input> && !IsXYZA<Input>)
            return convertColor<Output>(convertColor<typename Input::Reference>(color));
        else if constexpr (!IsRGBType<Output> && !IsXYZA<Output>)
            return convertColor<Output>(convertColor<typename Output::Reference>(color));

        // 3. Handle conversions within a RGBFamily (e.g. all have the same descriptor).
        else if constexpr (IsSameRGBTypeFamily<Output, Input>)
            return handleRGBFamilyConversion(color);

        // 4. Handle any gamma conversions for the Input and Output.
        else if constexpr (IsRGBGammaEncodedType<Input>)
            return convertColor<Output>(convertColor<typename Input::LinearCounterpart>(color));
        else if constexpr (IsRGBGammaEncodedType<Output>)
            return convertColor<Output>(convertColor<typename Output::LinearCounterpart>(color));

        // 5. Handle any bounds conversions for the Input and Output.
        else if constexpr (IsRGBBoundedType<Input>)
            return convertColor<Output>(convertColor<typename Input::ExtendedCounterpart>(color));
        else if constexpr (IsRGBBoundedType<Output>)
            return convertColor<Output>(convertColor<typename Output::ExtendedCounterpart>(color));

        // 6. At this point, Input and Output are each either ExtendedLinear-RGB types (of different familes) or XYZA
        //    and therefore all additional conversion can happen via matrix transformation.
        else
            return handleMatrixConversion(color);
    }

private:
    static inline constexpr Output handleToFloatConversion(const Input& color)
    {
        static_assert(IsRGBBoundedType<Input>, "Only bounded ([0..1]) RGB color types support conversion to/from bytes.");

        using InputWithReplacement = ColorTypeWithReplacementComponent<Input, float>;
        if constexpr (std::is_same_v<InputWithReplacement, Output>)
            return makeFromComponents<InputWithReplacement>(asColorComponents(color.resolved()).map([](uint8_t value) -> float { return value / 255.0f; }));
        else
            return convertColor<Output>(convertColor<InputWithReplacement>(color));
    }

    static inline constexpr Output handleToByteConversion(const Input& color)
    {
        static_assert(IsRGBBoundedType<Output>, "Only bounded ([0..1]) RGB color types support conversion to/from bytes.");

        using OutputWithReplacement = ColorTypeWithReplacementComponent<Output, float>;
        if constexpr (std::is_same_v<OutputWithReplacement, Input>)
            return makeFromComponents<Output>(asColorComponents(color.resolved()).map([](float value) -> uint8_t { return std::clamp(std::lround(value * 255.0f), 0l, 255l); }));
        else
            return convertColor<Output>(convertColor<OutputWithReplacement>(color));
    }

    template<typename ColorType> static inline constexpr auto toLinearEncoded(const ColorType& color) -> typename ColorType::LinearCounterpart
    {
        auto [c1, c2, c3, alpha] = color.resolved();
        return { ColorType::TransferFunction::toLinear(c1), ColorType::TransferFunction::toLinear(c2), ColorType::TransferFunction::toLinear(c3), alpha };
    }

    template<typename ColorType> static inline constexpr auto toGammaEncoded(const ColorType& color) -> typename ColorType::GammaEncodedCounterpart
    {
        auto [c1, c2, c3, alpha] = color.resolved();
        return { ColorType::TransferFunction::toGammaEncoded(c1), ColorType::TransferFunction::toGammaEncoded(c2), ColorType::TransferFunction::toGammaEncoded(c3), alpha };
    }

    template<typename ColorType> static inline constexpr auto toExtended(const ColorType& color) -> typename ColorType::ExtendedCounterpart
    {
        return makeFromComponents<typename ColorType::ExtendedCounterpart>(asColorComponents(color.resolved()));
    }

    template<typename ColorType> static inline constexpr auto toBounded(const ColorType& color) -> typename ColorType::BoundedCounterpart
    {
        return CSSGamutMapping::mapToBoundedGamut(color);
    }

    static inline constexpr Output handleRGBFamilyConversion(const Input& color)
    {
        static_assert(IsSameRGBTypeFamily<Output, Input>);

        //  RGB Family────────────────────┐
        //  │           ┌────────────────┐│
        //  │     ┌─────▶︎ LinearExtended ││
        //  │     │     └────────▲───────┘│
        //  │     │              │        │
        //  │┌────────┐          │        │
        //  ││ Linear │          │        │
        //  │└────▲───┘          │        │
        //  │     │              │        │
        //  │┌────────┐ ┌────────────────┐│
        //  ││ Gamma  │─│ GammaExtended  ││
        //  │└────────┘ └────────────────┘│
        //  └─────────────────────────────┘

        // This handles conversions between any two of these within the same family, so SRGBLinear -> SRGB, but not
        // SRGBLinear -> A98RGB.

        auto boundsConversion = [](auto color) {
            if constexpr (IsRGBExtendedType<Output> && IsRGBBoundedType<Input>)
                return toExtended(color);
            else if constexpr (IsRGBBoundedType<Output> && IsRGBExtendedType<Input>)
                return toBounded(color);
            else
                return color;
        };

        auto gammaConversion = [](auto color) {
            if constexpr (IsRGBGammaEncodedType<Output> && IsRGBLinearEncodedType<Input>)
                return toGammaEncoded(color);
            else if constexpr (IsRGBLinearEncodedType<Output> && IsRGBGammaEncodedType<Input>)
                return toLinearEncoded(color);
            else
                return color;
        };

        return boundsConversion(gammaConversion(color));
    }

    static inline constexpr Output handleMatrixConversion(const Input& color)
    {
        static_assert((IsRGBLinearEncodedType<Input> && IsRGBExtendedType<Input>) || IsXYZA<Input>);
        static_assert((IsRGBLinearEncodedType<Output> && IsRGBExtendedType<Output>) || IsXYZA<Output>);

        //  ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
        //   Matrix Conversions    ┌───────────┐│┌───────────┐
        //  │                      │ XYZ (D50) │││ XYZ (D65) │                                                                                                                      │
        //                         └─────▲─────┘│└─────▲─────┘
        //  │                            │      │      │                                                                                                                            │
        //                   ┌───────────┘      │      └───────────┬───────────────────────────────┬───────────────────────────────┬───────────────────────────────┐
        //  │                │                  │                  │                               │                               │                               │                │
        //                   │                  │                  │                               │                               │                               │
        //  │                │                  │                  │                               │                               │                               │                │
        //    ProPhotoRGB───────────────────┐   │   SRGB──────────────────────────┐ DisplayP3─────────────────────┐ A98RGB────────────────────────┐ Rec2020───────────────────────┐
        //  │ │           ┌────────────────┐│   │   │           ┌────────────────┐│ │           ┌────────────────┐│ │           ┌────────────────┐│ │           ┌────────────────┐│ │
        //    │     ┌─────▶︎ LinearExtended ││   │   │     ┌─────▶︎ LinearExtended ││ │     ┌─────▶︎ LinearExtended ││ │     ┌─────▶︎ LinearExtended ││ │     ┌─────▶︎ LinearExtended ││
        //  │ │     │     └────────▲───────┘│   │   │     │     └────────▲───────┘│ │     │     └────────▲───────┘│ │     │     └────────▲───────┘│ │     │     └────────▲───────┘│ │
        //   ─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─ ─│─ ─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─│─ ─ ─│─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─│─

        // This handles conversions between extended linear color types that can be converted using pre-defined
        // 3x3 matrices.
        
        // FIXME: Pre-compute (using constexpr) the concatenation of the matrices prior to applying them
        // to reduce number of matrix multiplications to a minimum. This will likely give subtly different
        // results (due to floating point effects) so if this optimization is considered we should ensure we
        // have sufficient testing coverage to notice any adverse effects.

        auto applyMatrices = [](const Input& color, auto... matrices) {
            return makeFromComponents<Output>(applyMatricesToColorComponents(asColorComponents(color.resolved()), matrices...));
        };

        if constexpr (Input::whitePoint == Output::whitePoint) {
            if constexpr (IsXYZA<Input>)
                return applyMatrices(color, Output::xyzToLinear);
            else if constexpr (IsXYZA<Output>)
                return applyMatrices(color, Input::linearToXYZ);
            else
                return applyMatrices(color, Input::linearToXYZ, Output::xyzToLinear);
        } else {
            if constexpr (IsXYZA<Input> && IsXYZA<Output>)
                return applyMatrices(color, ChromaticAdapation<Input::whitePoint, Output::whitePoint>::matrix);
            else if constexpr (IsXYZA<Input>)
                return applyMatrices(color, ChromaticAdapation<Input::whitePoint, Output::whitePoint>::matrix, Output::xyzToLinear);
            else if constexpr (IsXYZA<Output>)
                return applyMatrices(color, Input::linearToXYZ, ChromaticAdapation<Input::whitePoint, Output::whitePoint>::matrix);
            else
                return applyMatrices(color, Input::linearToXYZ, ChromaticAdapation<Input::whitePoint, Output::whitePoint>::matrix, Output::xyzToLinear);
        }
    }
};

} // namespace WebCore
