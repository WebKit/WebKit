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
//    template<> struct ColorConversion<XYZFor<`ColorType`<float>>, `ColorType`<float>> {
//        WEBCORE_EXPORT static XYZFor<`ColorType`<float>> convert(const `ColorType`<float>&);
//    };
//    
//    template<> struct ColorConversion<`ColorType`<float>, XYZFor<`ColorType`<float>>> {
//        WEBCORE_EXPORT static `ColorType`<float> convert(const XYZFor<`ColorType`<float>>&);
//    };
//
// Any additional conversions can be thought of as optimizations, shortcutting unnecessary steps, though
// some may be integral to the base conversion.

template<typename ColorType> using XYZFor = XYZA<typename ColorType::ComponentType, ColorType::whitePoint>;

template<typename Output, typename Input> struct ColorConversion;

template<typename Output, typename Input> inline Output convertColor(const Input& color)
{
    return ColorConversion<Output, Input>::convert(color);
}

// A98RGB
template<> struct ColorConversion<XYZFor<A98RGB<float>>, A98RGB<float>> {
    WEBCORE_EXPORT static XYZFor<A98RGB<float>> convert(const A98RGB<float>&);
};

template<> struct ColorConversion<A98RGB<float>, XYZFor<A98RGB<float>>> {
    WEBCORE_EXPORT static A98RGB<float> convert(const XYZFor<A98RGB<float>>&);
};

// Additions
template<> struct ColorConversion<LinearA98RGB<float>, A98RGB<float>> {
    WEBCORE_EXPORT static LinearA98RGB<float> convert(const A98RGB<float>&);
};


// DisplayP3
template<> struct ColorConversion<XYZFor<DisplayP3<float>>, DisplayP3<float>> {
    WEBCORE_EXPORT static XYZFor<DisplayP3<float>> convert(const DisplayP3<float>&);
};

template<> struct ColorConversion<DisplayP3<float>, XYZFor<DisplayP3<float>>> {
    WEBCORE_EXPORT static DisplayP3<float> convert(const XYZFor<DisplayP3<float>>&);
};

// Additions
template<> struct ColorConversion<LinearDisplayP3<float>, DisplayP3<float>> {
    WEBCORE_EXPORT static LinearDisplayP3<float> convert(const DisplayP3<float>&);
};


// ExtendedSRGBA
template<> struct ColorConversion<XYZFor<ExtendedSRGBA<float>>, ExtendedSRGBA<float>> {
    WEBCORE_EXPORT static XYZFor<ExtendedSRGBA<float>> convert(const ExtendedSRGBA<float>&);
};

template<> struct ColorConversion<ExtendedSRGBA<float>, XYZFor<ExtendedSRGBA<float>>> {
    WEBCORE_EXPORT static ExtendedSRGBA<float> convert(const XYZFor<ExtendedSRGBA<float>>&);
};

// Additions
template<> struct ColorConversion<LinearExtendedSRGBA<float>, ExtendedSRGBA<float>> {
    WEBCORE_EXPORT static LinearExtendedSRGBA<float> convert(const ExtendedSRGBA<float>&);
};


// HSLA
template<> struct ColorConversion<XYZFor<HSLA<float>>, HSLA<float>> {
    WEBCORE_EXPORT static XYZFor<HSLA<float>> convert(const HSLA<float>&);
};

template<> struct ColorConversion<HSLA<float>, XYZFor<HSLA<float>>> {
    WEBCORE_EXPORT static HSLA<float> convert(const XYZFor<HSLA<float>>&);
};

// Additions
template<> struct ColorConversion<SRGBA<float>, HSLA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const HSLA<float>&);
};


// HWBA
template<> struct ColorConversion<XYZFor<HWBA<float>>, HWBA<float>> {
    WEBCORE_EXPORT static XYZFor<HWBA<float>> convert(const HWBA<float>&);
};

template<> struct ColorConversion<HWBA<float>, XYZFor<HWBA<float>>> {
    WEBCORE_EXPORT static HWBA<float> convert(const XYZFor<HWBA<float>>&);
};

// Additions
template<> struct ColorConversion<SRGBA<float>, HWBA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const HWBA<float>&);
};


// LCHA
template<> struct ColorConversion<XYZFor<LCHA<float>>, LCHA<float>> {
    WEBCORE_EXPORT static XYZFor<LCHA<float>> convert(const LCHA<float>&);
};

template<> struct ColorConversion<LCHA<float>, XYZFor<LCHA<float>>> {
    WEBCORE_EXPORT static LCHA<float> convert(const XYZFor<LCHA<float>>&);
};

// Additions
template<> struct ColorConversion<Lab<float>, LCHA<float>> {
    WEBCORE_EXPORT static Lab<float> convert(const LCHA<float>&);
};


// Lab
template<> struct ColorConversion<XYZFor<Lab<float>>, Lab<float>> {
    WEBCORE_EXPORT static XYZFor<Lab<float>> convert(const Lab<float>&);
};

template<> struct ColorConversion<Lab<float>, XYZFor<Lab<float>>> {
    WEBCORE_EXPORT static Lab<float> convert(const XYZFor<Lab<float>>&);
};

// Additions
template<> struct ColorConversion<LCHA<float>, Lab<float>> {
    WEBCORE_EXPORT static LCHA<float> convert(const Lab<float>&);
};


// LinearA98RGB
template<> struct ColorConversion<XYZFor<LinearA98RGB<float>>, LinearA98RGB<float>> {
    WEBCORE_EXPORT static XYZFor<LinearA98RGB<float>> convert(const LinearA98RGB<float>&);
};

template<> struct ColorConversion<LinearA98RGB<float>, XYZFor<LinearA98RGB<float>>> {
    WEBCORE_EXPORT static LinearA98RGB<float> convert(const XYZFor<LinearA98RGB<float>>&);
};

// Additions
template<> struct ColorConversion<A98RGB<float>, LinearA98RGB<float>> {
    WEBCORE_EXPORT static A98RGB<float> convert(const LinearA98RGB<float>&);
};


// LinearDisplayP3
template<> struct ColorConversion<XYZFor<LinearDisplayP3<float>>, LinearDisplayP3<float>> {
    WEBCORE_EXPORT static XYZFor<LinearDisplayP3<float>> convert(const LinearDisplayP3<float>&);
};

template<> struct ColorConversion<LinearDisplayP3<float>, XYZFor<LinearDisplayP3<float>>> {
    WEBCORE_EXPORT static LinearDisplayP3<float> convert(const XYZFor<LinearDisplayP3<float>>&);
};

// Additions
template<> struct ColorConversion<DisplayP3<float>, LinearDisplayP3<float>> {
    WEBCORE_EXPORT static DisplayP3<float> convert(const LinearDisplayP3<float>&);
};


// LinearExtendedSRGBA
template<> struct ColorConversion<XYZFor<LinearExtendedSRGBA<float>>, LinearExtendedSRGBA<float>> {
    WEBCORE_EXPORT static XYZFor<LinearExtendedSRGBA<float>> convert(const LinearExtendedSRGBA<float>&);
};

template<> struct ColorConversion<LinearExtendedSRGBA<float>, XYZFor<LinearExtendedSRGBA<float>>> {
    WEBCORE_EXPORT static LinearExtendedSRGBA<float> convert(const XYZFor<LinearExtendedSRGBA<float>>&);
};

// Additions
template<> struct ColorConversion<ExtendedSRGBA<float>, LinearExtendedSRGBA<float>> {
    WEBCORE_EXPORT static ExtendedSRGBA<float> convert(const LinearExtendedSRGBA<float>&);
};


// LinearProPhotoRGB
template<> struct ColorConversion<XYZFor<LinearProPhotoRGB<float>>, LinearProPhotoRGB<float>> {
    WEBCORE_EXPORT static XYZFor<LinearProPhotoRGB<float>> convert(const LinearProPhotoRGB<float>&);
};

template<> struct ColorConversion<LinearProPhotoRGB<float>, XYZFor<LinearProPhotoRGB<float>>> {
    WEBCORE_EXPORT static LinearProPhotoRGB<float> convert(const XYZFor<LinearProPhotoRGB<float>>&);
};

// Additions
template<> struct ColorConversion<ProPhotoRGB<float>, LinearProPhotoRGB<float>> {
    WEBCORE_EXPORT static ProPhotoRGB<float> convert(const LinearProPhotoRGB<float>&);
};


// LinearRec2020
template<> struct ColorConversion<XYZFor<LinearRec2020<float>>, LinearRec2020<float>> {
    WEBCORE_EXPORT static XYZFor<LinearRec2020<float>> convert(const LinearRec2020<float>&);
};

template<> struct ColorConversion<LinearRec2020<float>, XYZFor<LinearRec2020<float>>> {
    WEBCORE_EXPORT static LinearRec2020<float> convert(const XYZFor<LinearRec2020<float>>&);
};

// Additions
template<> struct ColorConversion<Rec2020<float>, LinearRec2020<float>> {
    WEBCORE_EXPORT static Rec2020<float> convert(const LinearRec2020<float>&);
};


// LinearSRGBA
template<> struct ColorConversion<XYZFor<LinearSRGBA<float>>, LinearSRGBA<float>> {
    WEBCORE_EXPORT static XYZFor<LinearSRGBA<float>> convert(const LinearSRGBA<float>&);
};

template<> struct ColorConversion<LinearSRGBA<float>, XYZFor<LinearSRGBA<float>>> {
    WEBCORE_EXPORT static LinearSRGBA<float> convert(const XYZFor<LinearSRGBA<float>>&);
};

// Additions
template<> struct ColorConversion<SRGBA<float>, LinearSRGBA<float>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const LinearSRGBA<float>&);
};


// ProPhotoRGB
template<> struct ColorConversion<XYZFor<ProPhotoRGB<float>>, ProPhotoRGB<float>> {
    WEBCORE_EXPORT static XYZFor<ProPhotoRGB<float>> convert(const ProPhotoRGB<float>&);
};

template<> struct ColorConversion<ProPhotoRGB<float>, XYZFor<ProPhotoRGB<float>>> {
    WEBCORE_EXPORT static ProPhotoRGB<float> convert(const XYZFor<ProPhotoRGB<float>>&);
};

// Additions
template<> struct ColorConversion<LinearProPhotoRGB<float>, ProPhotoRGB<float>> {
    WEBCORE_EXPORT static LinearProPhotoRGB<float> convert(const ProPhotoRGB<float>&);
};


// Rec2020
template<> struct ColorConversion<XYZFor<Rec2020<float>>, Rec2020<float>> {
    WEBCORE_EXPORT static XYZFor<Rec2020<float>> convert(const Rec2020<float>&);
};

template<> struct ColorConversion<Rec2020<float>, XYZFor<Rec2020<float>>> {
    WEBCORE_EXPORT static Rec2020<float> convert(const XYZFor<Rec2020<float>>&);
};

// Additions
template<> struct ColorConversion<LinearRec2020<float>, Rec2020<float>> {
    WEBCORE_EXPORT static LinearRec2020<float> convert(const Rec2020<float>&);
};


// SRGBA
template<> struct ColorConversion<XYZFor<SRGBA<float>>, SRGBA<float>> {
    WEBCORE_EXPORT static XYZFor<SRGBA<float>> convert(const SRGBA<float>&);
};

template<> struct ColorConversion<SRGBA<float>, XYZFor<SRGBA<float>>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const XYZFor<SRGBA<float>>&);
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

// Special cases for SRGBA<uint8_t>. Only sRGB supports non-floating point component types.
template<> struct ColorConversion<SRGBA<float>, SRGBA<uint8_t>> {
    WEBCORE_EXPORT static SRGBA<float> convert(const SRGBA<uint8_t>&);
};

template<> struct ColorConversion<SRGBA<uint8_t>, SRGBA<float>> {
    WEBCORE_EXPORT static SRGBA<uint8_t> convert(const SRGBA<float>&);
};


// XYZA Chromatic Adaptation conversions.
template<> struct ColorConversion<XYZA<float, WhitePoint::D65>, XYZA<float, WhitePoint::D50>> {
    WEBCORE_EXPORT static XYZA<float, WhitePoint::D65> convert(const XYZA<float, WhitePoint::D50>&);
};

template<> struct ColorConversion<XYZA<float, WhitePoint::D50>, XYZA<float, WhitePoint::D65>> {
    WEBCORE_EXPORT static XYZA<float, WhitePoint::D50> convert(const XYZA<float, WhitePoint::D65>&);
};


// Identity conversion.

template<typename ColorType> struct ColorConversion<ColorType, ColorType> {
    static ColorType convert(const ColorType& color)
    {
        return color;
    }
};

// Fallback conversion.

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
        if constexpr (std::is_same_v<Input, SRGBA<uint8_t>>)
            return convertColor<Output>(convertColor<SRGBA<float>>(color));
        else if constexpr (std::is_same_v<Output, SRGBA<uint8_t>>)
            return convertColor<SRGBA<uint8_t>>(convertColor<SRGBA<float>>(color));
        else {
            auto xyz1 = convertColor<XYZFor<Input>>(color);
            auto xyz2 = convertColor<XYZFor<Output>>(xyz1);
            return convertColor<Output>(xyz2);
        }
    }
};

} // namespace WebCore
