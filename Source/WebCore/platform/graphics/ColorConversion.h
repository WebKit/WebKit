/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "ColorUtilities.h"

namespace WebCore {

// All color types must at least implement the following conversions to and from their reference XYZ color space
//
//    template<> struct ColorConversion<`ColorType`<float>::ReferenceXYZ, `ColorType`<float>> {
//        WEBCORE_EXPORT static `ColorType`<float>::ReferenceXYZ convert(const `ColorType`<float>&);
//    };
//    
//    template<> struct ColorConversion<`ColorType`<float>, `ColorType`<float>::ReferenceXYZ> {
//        WEBCORE_EXPORT static `ColorType`<float> convert(const `ColorType`<float>::ReferenceXYZ&);
//    };
//
// Any additional conversions can be thought of as optimizations, shortcutting unnecessary steps, though
// some may be integral to the base conversion.


template<typename Output, typename Input> struct ColorConversion;

template<typename Output, typename Input> inline Output convertColor(const Input& color)
{
    return ColorConversion<Output, Input>::convert(color);
}


// A98RGB
template<> struct ColorConversion<A98RGB<float>::ReferenceXYZ, A98RGB<float>> {
    WEBCORE_EXPORT static A98RGB<float>::ReferenceXYZ convert(const A98RGB<float>&);
};

template<> struct ColorConversion<A98RGB<float>, A98RGB<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static A98RGB<float> convert(const A98RGB<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LinearA98RGB<float>, A98RGB<float>> {
    WEBCORE_EXPORT static LinearA98RGB<float> convert(const A98RGB<float>&);
};


// DisplayP3
template<> struct ColorConversion<DisplayP3<float>::ReferenceXYZ, DisplayP3<float>> {
    WEBCORE_EXPORT static DisplayP3<float>::ReferenceXYZ convert(const DisplayP3<float>&);
};

template<> struct ColorConversion<DisplayP3<float>, DisplayP3<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static DisplayP3<float> convert(const DisplayP3<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LinearDisplayP3<float>, DisplayP3<float>> {
    WEBCORE_EXPORT static LinearDisplayP3<float> convert(const DisplayP3<float>&);
};


// ExtendedSRGBA
template<> struct ColorConversion<ExtendedSRGBA<float>::ReferenceXYZ, ExtendedSRGBA<float>> {
    WEBCORE_EXPORT static ExtendedSRGBA<float>::ReferenceXYZ convert(const ExtendedSRGBA<float>&);
};

template<> struct ColorConversion<ExtendedSRGBA<float>, ExtendedSRGBA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static ExtendedSRGBA<float> convert(const ExtendedSRGBA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LinearExtendedSRGBA<float>, ExtendedSRGBA<float>> {
    WEBCORE_EXPORT static LinearExtendedSRGBA<float> convert(const ExtendedSRGBA<float>&);
};


// HSLA
template<> struct ColorConversion<HSLA<float>::ReferenceXYZ, HSLA<float>> {
    WEBCORE_EXPORT static HSLA<float>::ReferenceXYZ convert(const HSLA<float>&);
};

template<> struct ColorConversion<HSLA<float>, HSLA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static HSLA<float> convert(const HSLA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<SRGBA<float>, HSLA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const HSLA<float>&);
};


// HWBA
template<> struct ColorConversion<HWBA<float>::ReferenceXYZ, HWBA<float>> {
    WEBCORE_EXPORT static HWBA<float>::ReferenceXYZ convert(const HWBA<float>&);
};

template<> struct ColorConversion<HWBA<float>, HWBA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static HWBA<float> convert(const HWBA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<SRGBA<float>, HWBA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const HWBA<float>&);
};


// LCHA
template<> struct ColorConversion<LCHA<float>::ReferenceXYZ, LCHA<float>> {
    WEBCORE_EXPORT static LCHA<float>::ReferenceXYZ convert(const LCHA<float>&);
};

template<> struct ColorConversion<LCHA<float>, LCHA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LCHA<float> convert(const LCHA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<Lab<float>, LCHA<float>> {
    WEBCORE_EXPORT static Lab<float> convert(const LCHA<float>&);
};


// Lab
template<> struct ColorConversion<Lab<float>::ReferenceXYZ, Lab<float>> {
    WEBCORE_EXPORT static Lab<float>::ReferenceXYZ convert(const Lab<float>&);
};

template<> struct ColorConversion<Lab<float>, Lab<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static Lab<float> convert(const Lab<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LCHA<float>, Lab<float>> {
    WEBCORE_EXPORT static LCHA<float> convert(const Lab<float>&);
};


// LinearA98RGB
template<> struct ColorConversion<LinearA98RGB<float>::ReferenceXYZ, LinearA98RGB<float>> {
    WEBCORE_EXPORT static LinearA98RGB<float>::ReferenceXYZ convert(const LinearA98RGB<float>&);
};

template<> struct ColorConversion<LinearA98RGB<float>, LinearA98RGB<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LinearA98RGB<float> convert(const LinearA98RGB<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<A98RGB<float>, LinearA98RGB<float>> {
    WEBCORE_EXPORT static A98RGB<float> convert(const LinearA98RGB<float>&);
};


// LinearDisplayP3
template<> struct ColorConversion<LinearDisplayP3<float>::ReferenceXYZ, LinearDisplayP3<float>> {
    WEBCORE_EXPORT static LinearDisplayP3<float>::ReferenceXYZ convert(const LinearDisplayP3<float>&);
};

template<> struct ColorConversion<LinearDisplayP3<float>, LinearDisplayP3<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LinearDisplayP3<float> convert(const LinearDisplayP3<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<DisplayP3<float>, LinearDisplayP3<float>> {
    WEBCORE_EXPORT static DisplayP3<float> convert(const LinearDisplayP3<float>&);
};


// LinearExtendedSRGBA
template<> struct ColorConversion<LinearExtendedSRGBA<float>::ReferenceXYZ, LinearExtendedSRGBA<float>> {
    WEBCORE_EXPORT static LinearExtendedSRGBA<float>::ReferenceXYZ convert(const LinearExtendedSRGBA<float>&);
};

template<> struct ColorConversion<LinearExtendedSRGBA<float>, LinearExtendedSRGBA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LinearExtendedSRGBA<float> convert(const LinearExtendedSRGBA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<ExtendedSRGBA<float>, LinearExtendedSRGBA<float>> {
    WEBCORE_EXPORT static ExtendedSRGBA<float> convert(const LinearExtendedSRGBA<float>&);
};


// LinearProPhotoRGB
template<> struct ColorConversion<LinearProPhotoRGB<float>::ReferenceXYZ, LinearProPhotoRGB<float>> {
    WEBCORE_EXPORT static LinearProPhotoRGB<float>::ReferenceXYZ convert(const LinearProPhotoRGB<float>&);
};

template<> struct ColorConversion<LinearProPhotoRGB<float>, LinearProPhotoRGB<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LinearProPhotoRGB<float> convert(const LinearProPhotoRGB<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<ProPhotoRGB<float>, LinearProPhotoRGB<float>> {
    WEBCORE_EXPORT static ProPhotoRGB<float> convert(const LinearProPhotoRGB<float>&);
};


// LinearRec2020
template<> struct ColorConversion<LinearRec2020<float>::ReferenceXYZ, LinearRec2020<float>> {
    WEBCORE_EXPORT static LinearRec2020<float>::ReferenceXYZ convert(const LinearRec2020<float>&);
};

template<> struct ColorConversion<LinearRec2020<float>, LinearRec2020<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LinearRec2020<float> convert(const LinearRec2020<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<Rec2020<float>, LinearRec2020<float>> {
    WEBCORE_EXPORT static Rec2020<float> convert(const LinearRec2020<float>&);
};


// LinearSRGBA
template<> struct ColorConversion<LinearSRGBA<float>::ReferenceXYZ, LinearSRGBA<float>> {
    WEBCORE_EXPORT static LinearSRGBA<float>::ReferenceXYZ convert(const LinearSRGBA<float>&);
};

template<> struct ColorConversion<LinearSRGBA<float>, LinearSRGBA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static LinearSRGBA<float> convert(const LinearSRGBA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<SRGBA<float>, LinearSRGBA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const LinearSRGBA<float>&);
};


// ProPhotoRGB
template<> struct ColorConversion<ProPhotoRGB<float>::ReferenceXYZ, ProPhotoRGB<float>> {
    WEBCORE_EXPORT static ProPhotoRGB<float>::ReferenceXYZ convert(const ProPhotoRGB<float>&);
};

template<> struct ColorConversion<ProPhotoRGB<float>, ProPhotoRGB<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static ProPhotoRGB<float> convert(const ProPhotoRGB<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LinearProPhotoRGB<float>, ProPhotoRGB<float>> {
    WEBCORE_EXPORT static LinearProPhotoRGB<float> convert(const ProPhotoRGB<float>&);
};


// Rec2020
template<> struct ColorConversion<Rec2020<float>::ReferenceXYZ, Rec2020<float>> {
    WEBCORE_EXPORT static Rec2020<float>::ReferenceXYZ convert(const Rec2020<float>&);
};

template<> struct ColorConversion<Rec2020<float>, Rec2020<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static Rec2020<float> convert(const Rec2020<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LinearRec2020<float>, Rec2020<float>> {
    WEBCORE_EXPORT static LinearRec2020<float> convert(const Rec2020<float>&);
};


// SRGBA
template<> struct ColorConversion<SRGBA<float>::ReferenceXYZ, SRGBA<float>> {
    WEBCORE_EXPORT static SRGBA<float>::ReferenceXYZ convert(const SRGBA<float>&);
};

template<> struct ColorConversion<SRGBA<float>, SRGBA<float>::ReferenceXYZ> {
    WEBCORE_EXPORT static SRGBA<float> convert(const SRGBA<float>::ReferenceXYZ&);
};

// Additions
template<> struct ColorConversion<LinearSRGBA<float>, SRGBA<float>> {
    WEBCORE_EXPORT static LinearSRGBA<float> convert(const SRGBA<float>&);
};

template<> struct ColorConversion<HSLA<float>, SRGBA<float>> {
    WEBCORE_EXPORT static HSLA<float> convert(const SRGBA<float>&);
};

template<> struct ColorConversion<HWBA<float>, SRGBA<float>> {
    WEBCORE_EXPORT static HWBA<float> convert(const SRGBA<float>&);
};



// Chromatic Adaptation conversions.
WEBCORE_EXPORT XYZA<float, WhitePoint::D65> convertFromD50WhitePointToD65WhitePoint(const XYZA<float, WhitePoint::D50>&);
WEBCORE_EXPORT XYZA<float, WhitePoint::D50> convertFromD65WhitePointToD50WhitePoint(const XYZA<float, WhitePoint::D65>&);

template<typename T, WhitePoint inputWhitePoint> constexpr typename T::ReferenceXYZ performChomaticAdapatation(const XYZA<float, inputWhitePoint>& color)
{
    constexpr WhitePoint outputWhitePoint = T::ReferenceXYZ::whitePoint;

    if constexpr (outputWhitePoint == inputWhitePoint)
        return color;
    else if constexpr (outputWhitePoint == WhitePoint::D50)
        return convertFromD65WhitePointToD50WhitePoint(color);
    else if constexpr (outputWhitePoint == WhitePoint::D65)
        return convertFromD50WhitePointToD65WhitePoint(color);
}

// Fallback conversions.

// All types are required to have a conversion to their reference XYZ space, so this is guaranteed
// to work if another specialization is not already provided.

// FIXME: Rather than always converting through reference XYZ color space
// we should instead switch to computing the least common reference ancestor
// type where each color type defines its reference type as the color it needs
// to get one step closer to the reference XYZ color space. For instance,
// the reference type of SRGBA is LinearSRGBA, and the reference type of LCHA
// is Lab. Together, the color types make a tree with XYZ at the root. The
// conversion should end up looking something like:
//
//   using LCA = LeastCommonAncestor<Output, Input>::Type;
//   return convertColor<Output>(convertColor<LCA>(color));
//
// An example of where this would be more efficient is when converting from
// HSLA<float> to HWBA<float>. LCA for this pair is SRGBA<float> so conversion
// through XYZ is wasteful. This model would also allow us to remove the
// combination conversions, reducing the number of things each new color type
// requires.
//
// FIXME: Reduce total number of matrix tranforms by concatenating the matrices
// of sequential matrix transforms at compile time. Take the conversion from
// ProPhotoRGB<float> to SRGBA<float> as an example:
//
//   1. ProPhotoRGB<float> -> LinearProPhotoRGB<float>                  (transfer function transform, not matrix)
//   2. LinearProPhotoRGB<float> -> XYZA<float, WhitePoint::D50>        MATRIX TRANSFORM
//   3. XYZA<float, WhitePoint::D50> -> XYZA<float, WhitePoint::D65>    MATRIX TRANSFORM
//   4. XYZA<float, WhitePoint::D65> -> LinearSRGBA<float>              MATRIX TRANSFORM
//   5. LinearSRGBA<float> -> SRGBA<float>                              (transfer function transform, not matrix)
//
// Steps 2, 3, and 4 can be combined into one single matrix if we linearly
// concatented the three matrices. To do this, we will have to tag which conversions
// are matrix based, expose the matrices, add support for constexpr concatenting
// of ColorMatrix and find a way to merge the conversions.

template<typename Output, typename Input> struct ColorConversion {
    static Output convert(const Input& color)
    {
        if constexpr(std::is_same_v<Output, Input>)
            return color;
        else if constexpr (std::is_same_v<typename Input::ComponentType, uint8_t>)
            return convertColor<Output>(convertTo<SRGBA<float>>(color));
        else if constexpr (std::is_same_v<typename Output::ComponentType, uint8_t>)
            return convertTo<SRGBA<uint8_t>>(convertColor<SRGBA<float>>(color));
        else {
            auto xyz1 = convertColor<typename Input::ReferenceXYZ>(color);
            auto xyz2 = performChomaticAdapatation<Output>(xyz1);
            return convertColor<Output>(xyz2);
        }
    }
};

} // namespace WebCore
