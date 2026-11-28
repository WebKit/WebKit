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

#include <WebCore/ColorChromaticAdaptation.h>
#include <WebCore/ColorGamutMapping.h>
#include <WebCore/ColorSpaceName.h>
#include <WebCore/ColorTypes.h>
#include <WebCore/PlatformExportMacros.h>
#include <array>
#include <concepts>
#include <numeric>
#include <optional>
#include <wtf/MathExtras.h>

namespace WebCore {

class ColorSpace;

// Conversion function for typed colors. Conversions to a bounded RGB type bring the color
// into its gamut with `GamutMapping`, such as ClipGamutMapping or RayTraceGamutMapping.
template<UnpremultipliedColorType Output, GamutMappingAlgorithm GamutMapping = ClipGamutMapping, UnpremultipliedColorType Input> Output convertColor(const Input& color);

// Conversion function for changing the component type of a premultiplied color, which,
// unlike the other conversions, is safe as it only rescales the components.
template<typename ComponentType, typename ColorType> ResolvedPremultiplied<ColorTypeWithReplacementComponent<ColorType, ComponentType>> convertComponentType(const ResolvedPremultiplied<ColorType>&);
template<typename ComponentType, typename T> ResolvedPremultiplied<ColorComponents<ComponentType, 4>> convertComponentType(const ResolvedPremultiplied<ColorComponents<T, 4>>&);

// Conversion functions for raw color components with associated color spaces. Conversions to a bounded
// RGB type bring the color into its gamut with `GamutMapping`, such as ClipGamutMapping or RayTraceGamutMapping.
template<typename Output, GamutMappingAlgorithm GamutMapping = ClipGamutMapping> Output convertColorComponents(ColorSpaceName inputColorSpace, const ColorComponents<float, 4>& input);
ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpaceName inputColorSpace, ColorComponents<float, 4> inputColorComponents, ColorSpaceName outputColorSpace);
ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpaceName inputColorSpace, ColorComponents<float, 4> inputColorComponents, const ColorSpace& outputColorSpace);

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

// MARK: HSLA
template<> struct ColorConversion<ExtendedSRGBA<float>, HSLA<float>> {
    WEBCORE_EXPORT static ExtendedSRGBA<float> convert(const HSLA<float>&);
};
template<> struct ColorConversion<HSLA<float>, ExtendedSRGBA<float>> {
    WEBCORE_EXPORT static HSLA<float> convert(const ExtendedSRGBA<float>&);
};

// MARK: HWBA
template<> struct ColorConversion<ExtendedSRGBA<float>, HWBA<float>> {
    WEBCORE_EXPORT static ExtendedSRGBA<float> convert(const HWBA<float>&);
};
template<> struct ColorConversion<HWBA<float>, ExtendedSRGBA<float>> {
    WEBCORE_EXPORT static HWBA<float> convert(const ExtendedSRGBA<float>&);
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
        //    these other color types, we can unconditionally convert them to their "reference" color, as
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

        // 6. At this point, Input and Output are each either ExtendedLinear-RGB types (of different families) or XYZA
        //    and therefore all additional conversion can happen via matrix transformation.
        else
            return handleMatrixConversion(color);
    }

private:
    static constexpr Output handleToFloatConversion(const Input& color)
    {
        static_assert(IsRGBBoundedType<Input>, "Only bounded ([0..1]) RGB color types support conversion to/from bytes.");

        using InputWithReplacement = ColorTypeWithReplacementComponent<Input, float>;
        if constexpr (std::is_same_v<InputWithReplacement, Output>)
            return makeFromComponents<InputWithReplacement>(asColorComponents(color.resolved()).map([](uint8_t value) -> float { return value / 255.0f; }));
        else
            return convertColor<Output>(convertColor<InputWithReplacement>(color));
    }

    static constexpr Output handleToByteConversion(const Input& color)
    {
        static_assert(IsRGBBoundedType<Output>, "Only bounded ([0..1]) RGB color types support conversion to/from bytes.");

        using OutputWithReplacement = ColorTypeWithReplacementComponent<Output, float>;
        if constexpr (std::is_same_v<OutputWithReplacement, Input>)
            return makeFromComponents<Output>(asColorComponents(color.resolved()).map([](float value) -> uint8_t { return std::clamp(std::lround(value * 255.0f), 0l, 255l); }));
        else
            return convertColor<Output>(convertColor<OutputWithReplacement>(color));
    }

    template<typename ColorType> static constexpr auto toLinearEncoded(const ColorType& color) -> typename ColorType::LinearCounterpart
    {
        auto [c1, c2, c3, alpha] = color.resolved();
        return { ColorType::TransferFunction::toLinear(c1), ColorType::TransferFunction::toLinear(c2), ColorType::TransferFunction::toLinear(c3), alpha };
    }

    template<typename ColorType> static constexpr auto toGammaEncoded(const ColorType& color) -> typename ColorType::GammaEncodedCounterpart
    {
        auto [c1, c2, c3, alpha] = color.resolved();
        return { ColorType::TransferFunction::toGammaEncoded(c1), ColorType::TransferFunction::toGammaEncoded(c2), ColorType::TransferFunction::toGammaEncoded(c3), alpha };
    }

    template<typename ColorType> static constexpr auto toExtended(const ColorType& color) -> typename ColorType::ExtendedCounterpart
    {
        return makeFromComponents<typename ColorType::ExtendedCounterpart>(asColorComponents(color.resolved()));
    }

    template<typename ColorType> static constexpr auto toBounded(const ColorType& color) -> typename ColorType::BoundedCounterpart
    {
        return clipToGamut<typename ColorType::BoundedCounterpart>(color);
    }

    static constexpr Output handleRGBFamilyConversion(const Input& color)
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

    static constexpr Output handleMatrixConversion(const Input& color)
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
                return applyMatrices(color, ChromaticAdaptation<Input::whitePoint, Output::whitePoint>::matrix);
            else if constexpr (IsXYZA<Input>)
                return applyMatrices(color, ChromaticAdaptation<Input::whitePoint, Output::whitePoint>::matrix, Output::xyzToLinear);
            else if constexpr (IsXYZA<Output>)
                return applyMatrices(color, Input::linearToXYZ, ChromaticAdaptation<Input::whitePoint, Output::whitePoint>::matrix);
            else
                return applyMatrices(color, Input::linearToXYZ, ChromaticAdaptation<Input::whitePoint, Output::whitePoint>::matrix, Output::xyzToLinear);
        }
    }
};

// MARK: Conversion paths.

// The last color type before XYZ in the conversions of a color type, or XYZ itself. Two color types
// with the same one convert to each other without going through XYZ.
template<typename ColorType> struct LastColorTypeBeforeXYZHelper {
    static constexpr auto type()
    {
        if constexpr (std::same_as<typename ColorType::ComponentType, uint8_t>)
            return std::type_identity<typename LastColorTypeBeforeXYZHelper<ColorTypeWithReplacementComponent<ColorType, float>>::Type> { };
        else if constexpr (IsXYZA<ColorType>)
            return std::type_identity<ColorType> { };
        else if constexpr (IsRGBBoundedType<ColorType>)
            return std::type_identity<typename LastColorTypeBeforeXYZHelper<typename ColorType::ExtendedCounterpart>::Type> { };
        else if constexpr (IsRGBGammaEncodedType<ColorType>)
            return std::type_identity<typename LastColorTypeBeforeXYZHelper<typename ColorType::LinearCounterpart>::Type> { };
        else if constexpr (IsRGBType<ColorType>)
            return std::type_identity<ColorType> { };
        else if constexpr (IsXYZA<typename ColorType::Reference>)
            return std::type_identity<ColorType> { };
        else
            return std::type_identity<typename LastColorTypeBeforeXYZHelper<typename ColorType::Reference>::Type> { };
    }

    using Type = typename decltype(type())::type;
};

template<typename ColorType> using LastColorTypeBeforeXYZ = typename LastColorTypeBeforeXYZHelper<ColorType>::Type;

// The XYZ, of the white point of `ColorType`, that it is converted to and from.
template<typename ColorType> struct XYZForColorTypeHelper {
    static constexpr auto type()
    {
        if constexpr (IsXYZA<LastColorTypeBeforeXYZ<ColorType>>)
            return std::type_identity<LastColorTypeBeforeXYZ<ColorType>> { };
        else
            return std::type_identity<typename LastColorTypeBeforeXYZ<ColorType>::Reference> { };
    }

    using Type = typename decltype(type())::type;
};

template<typename ColorType> using XYZForColorType = typename XYZForColorTypeHelper<ColorType>::Type;

// Whether two color types convert to each other directly, such as sRGB and HSL, without going
// through XYZ. Those that do not, such as sRGB and Display P3, have their conversions split at XYZ instead.
template<typename ColorType1, typename ColorType2> concept ConvertsWithoutXYZ = std::same_as<LastColorTypeBeforeXYZ<ColorType1>, LastColorTypeBeforeXYZ<ColorType2>>;

// MARK: Gamut mapping.

// Whether converting to `Output` applies `GamutMapping`, as clipping is already part of converting to a bounded RGB type.
template<typename Output, typename GamutMapping> concept MapsToGamut = IsRGBBoundedType<Output> && !std::same_as<GamutMapping, ClipGamutMapping>;

// Not inlined, so that there is one copy per output type.
template<typename Output, GamutMappingAlgorithm GamutMapping> requires IsRGBBoundedType<Output> NEVER_INLINE Output mapToGamut(const LastColorTypeBeforeXYZ<Output>& color)
{
    using LinearExtended = LastColorTypeBeforeXYZ<Output>;

    ColorComponents<float, 4> components = asColorComponents(color.resolved());
    if (!inGamut<typename LinearExtended::BoundedCounterpart>(components)) {
        static constexpr auto gamut = RGBGamut::of<LinearExtended>();
        auto [r, g, b] = GamutMapping::map(gamut, { components[0], components[1], components[2] });
        components = { r, g, b, components[3] };
    }
    return convertColor<Output>(makeFromComponents<LinearExtended>(components));
}

// MARK: Conversion functions for typed colors.

template<UnpremultipliedColorType Output, GamutMappingAlgorithm GamutMapping, UnpremultipliedColorType Input> Output convertColor(const Input& color)
{
    if constexpr (MapsToGamut<Output, GamutMapping>) {
        if constexpr (std::same_as<typename Output::ComponentType, uint8_t>)
            return convertColor<Output>(convertColor<ColorTypeWithReplacementComponent<Output, float>, GamutMapping>(color));
        else
            return mapToGamut<Output, GamutMapping>(convertColor<LastColorTypeBeforeXYZ<Output>>(color));
    } else {
        return ColorConversion<CanonicalColorType<Output>, CanonicalColorType<Input>>::convert(color);
    }
}

template<typename ComponentType, typename ColorType> ResolvedPremultiplied<ColorTypeWithReplacementComponent<ColorType, ComponentType>> convertComponentType(const ResolvedPremultiplied<ColorType>& color)
{
    return Premultiplied { convertColor<ColorTypeWithReplacementComponent<ColorType, ComponentType>>(makeFromComponents<ColorType>(asColorComponents(color.resolved()))).resolved() };
}

template<typename ComponentType, typename T> ResolvedPremultiplied<ColorComponents<ComponentType, 4>> convertComponentType(const ResolvedPremultiplied<ColorComponents<T, 4>>& color)
{
    if constexpr (std::is_same_v<ComponentType, T>)
        return color;
    else if constexpr (std::is_same_v<ComponentType, uint8_t> && std::is_same_v<T, float>)
        return Premultiplied { resolvedColor(color.resolved().map([](float value) -> uint8_t { return std::clamp(std::lround(value * 255.0f), 0l, 255l); })) };
    else {
        static_assert(std::is_same_v<ComponentType, float> && std::is_same_v<T, uint8_t>);
        return Premultiplied { resolvedColor(color.resolved().map([](uint8_t value) -> float { return value / 255.0f; })) };
    }
}

// MARK: DeltaE color difference algorithms.

template<typename ColorType1, typename ColorType2> constexpr float computeDeltaEOK(ColorType1 color1, ColorType2 color2)
{
    // https://drafts.csswg.org/css-color/#color-difference-OK

    auto [L1, a1, b1, alpha1] = convertColor<OKLab<float>>(color1).resolved();
    auto [L2, a2, b2, alpha2] = convertColor<OKLab<float>>(color2).resolved();

    auto deltaL = (L1 / 100.0f) - (L2 / 100.0f);
    auto deltaA = a1 - a2;
    auto deltaB = b1 - b2;

    return std::hypot(deltaL, deltaA, deltaB);
}

// MARK: Conversions for color types known only at runtime.

// Conversions from a color whose type is only known at runtime, as a ColorSpaceName. Converting
// directly would instantiate a conversion for every pair of input and output types, so instead,
// conversions are split at XYZ, into one conversion to XYZ per input type and one from XYZ per
// output type. Each color type converts to the XYZ of its own white point, and adapts between white
// points only when the input and output differ, so the steps are the same as converting directly.
//
// Pairs whose direct conversion does not go through XYZ, such as from HSL to sRGB or from LCH to Lab,
// still convert directly, as splitting them at XYZ would do more work and lose precision.

// Not inlined, so that there is one copy per input type.
template<typename ColorType> NEVER_INLINE XYZForColorType<ColorType> convertColorComponentsToXYZ(const ColorComponents<float, 4>& components)
{
    return convertColor<XYZForColorType<ColorType>>(makeFromComponents<ColorType>(components));
}

// Not inlined, so that there is one copy per output type.
template<typename Output, WhitePoint whitePoint> NEVER_INLINE Output convertColorFromXYZ(const XYZA<float, whitePoint>& color)
{
    using XYZ = XYZForColorType<Output>;

    if constexpr (XYZ::whitePoint == whitePoint)
        return convertColor<Output>(color);
    else
        return convertColor<Output>(convertColor<XYZ>(color));
}

template<typename Input, typename Output> Output convertColorComponentsSplitAtXYZ(const ColorComponents<float, 4>& input)
{
    if constexpr (IsXYZA<Input>)
        return convertColorFromXYZ<Output>(makeFromComponents<Input>(input));
    else
        return convertColorFromXYZ<Output>(convertColorComponentsToXYZ<Input>(input));
}

template<typename Output, GamutMappingAlgorithm GamutMapping> Output convertColorComponents(ColorSpaceName inputColorSpace, const ColorComponents<float, 4>& input)
{
    if constexpr (MapsToGamut<Output, GamutMapping>) {
        if constexpr (std::same_as<typename Output::ComponentType, uint8_t>)
            return convertColor<Output>(convertColorComponents<ColorTypeWithReplacementComponent<Output, float>, GamutMapping>(inputColorSpace, input));
        else
            return mapToGamut<Output, GamutMapping>(convertColorComponents<LastColorTypeBeforeXYZ<Output>>(inputColorSpace, input));
    } else {
        return callWithColorType<float>(inputColorSpace, [&]<typename Input> -> Output {
            if constexpr (ConvertsWithoutXYZ<Input, Output>)
                return convertColor<Output>(makeFromComponents<Input>(input));
            else
                return convertColorComponentsSplitAtXYZ<Input, Output>(input);
        });
    }
}

} // namespace WebCore
