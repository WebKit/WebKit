/*
 * Copyright (C) 2017, 2020 Apple Inc. All rights reserved.
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

namespace WebCore {

// All color types must at least implement the following conversions to and from the XYZA color space:
//    XYZA<float> toXYZA(const ColorType<float>&);
//    ColorType<float> toColorType(const XYZA<float>&);
//
// Any additional conversions can be thought of as optimizations, shortcutting unnecessary steps, though
// some may be integral to the base conversion.


// A98RGB
WEBCORE_EXPORT XYZA<float> toXYZA(const A98RGB<float>&);
WEBCORE_EXPORT A98RGB<float> toA98RGB(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearA98RGB<float> toLinearA98RGB(const A98RGB<float>&);

// DisplayP3
WEBCORE_EXPORT XYZA<float> toXYZA(const DisplayP3<float>&);
WEBCORE_EXPORT DisplayP3<float> toDisplayP3(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearDisplayP3<float> toLinearDisplayP3(const DisplayP3<float>&);

// ExtendedSRGBA
WEBCORE_EXPORT XYZA<float> toXYZA(const ExtendedSRGBA<float>&);
WEBCORE_EXPORT ExtendedSRGBA<float> toExtendedSRGBA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearExtendedSRGBA<float> toLinearExtendedSRGBA(const ExtendedSRGBA<float>&);

// HSLA
WEBCORE_EXPORT XYZA<float> toXYZA(const HSLA<float>&);
WEBCORE_EXPORT HSLA<float> toHSLA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT SRGBA<float> toSRGBA(const HSLA<float>&);

// LCHA
WEBCORE_EXPORT XYZA<float> toXYZA(const LCHA<float>&);
WEBCORE_EXPORT LCHA<float> toLCHA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT Lab<float> toLab(const LCHA<float>&);

// Lab
WEBCORE_EXPORT XYZA<float> toXYZA(const Lab<float>&);
WEBCORE_EXPORT Lab<float> toLab(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LCHA<float> toLCHA(const Lab<float>&);

// LinearA98RGB
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearA98RGB<float>&);
WEBCORE_EXPORT LinearA98RGB<float> toLinearA98RGB(const XYZA<float>&);
// Additions
WEBCORE_EXPORT A98RGB<float> toA98RGB(const LinearA98RGB<float>& color);

// LinearDisplayP3
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearDisplayP3<float>&);
WEBCORE_EXPORT LinearDisplayP3<float> toLinearDisplayP3(const XYZA<float>&);
// Additions
WEBCORE_EXPORT DisplayP3<float> toDisplayP3(const LinearDisplayP3<float>&);

// LinearExtendedSRGBA
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearExtendedSRGBA<float>&);
WEBCORE_EXPORT LinearExtendedSRGBA<float> toLinearExtendedSRGBA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT ExtendedSRGBA<float> toExtendedSRGBA(const LinearExtendedSRGBA<float>&);

// LinearRec2020
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearRec2020<float>&);
WEBCORE_EXPORT LinearRec2020<float> toLinearRec2020(const XYZA<float>&);
// Additions
WEBCORE_EXPORT Rec2020<float> toRec2020(const LinearRec2020<float>&);

// LinearSRGBA
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearSRGBA<float>&);
WEBCORE_EXPORT LinearSRGBA<float> toLinearSRGBA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT SRGBA<float> toSRGBA(const LinearSRGBA<float>&);

// Rec2020
WEBCORE_EXPORT XYZA<float> toXYZA(const Rec2020<float>&);
WEBCORE_EXPORT Rec2020<float> toRec2020(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearRec2020<float> toLinearRec2020(const Rec2020<float>&);

// SRGBA
WEBCORE_EXPORT XYZA<float> toXYZA(const SRGBA<float>&);
WEBCORE_EXPORT SRGBA<float> toSRGBA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearSRGBA<float> toLinearSRGBA(const SRGBA<float>&);
WEBCORE_EXPORT HSLA<float> toHSLA(const SRGBA<float>&);


// Identity conversions (useful for generic contexts).

constexpr A98RGB<float> toA98RGB(const A98RGB<float>& color) { return color; }
constexpr DisplayP3<float> toDisplayP3(const DisplayP3<float>& color) { return color; }
constexpr ExtendedSRGBA<float> toExtendedSRGBA(const ExtendedSRGBA<float>& color) { return color; }
constexpr HSLA<float> toHSLA(const HSLA<float>& color) { return color; }
constexpr LCHA<float> toLCHA(const LCHA<float>& color) { return color; }
constexpr Lab<float> toLab(const Lab<float>& color) { return color; }
constexpr LinearA98RGB<float> toLinearA98RGB(const LinearA98RGB<float>& color) { return color; }
constexpr LinearDisplayP3<float> toLinearDisplayP3(const LinearDisplayP3<float>& color) { return color; }
constexpr LinearExtendedSRGBA<float> toLinearExtendedSRGBA(const LinearExtendedSRGBA<float>& color) { return color; }
constexpr LinearRec2020<float> toLinearRec2020(const LinearRec2020<float>& color) { return color; }
constexpr LinearSRGBA<float> toLinearSRGBA(const LinearSRGBA<float>& color) { return color; }
constexpr Rec2020<float> toRec2020(const Rec2020<float>& color) { return color; }
constexpr SRGBA<float> toSRGBA(const SRGBA<float>& color) { return color; }
constexpr XYZA<float> toXYZA(const XYZA<float>& color) { return color; }


// Fallback conversions.

// All types are required to have a conversion to XYZA, so these are guaranteed to work if
// another overload is not already provided.

template<typename T> A98RGB<float> toA98RGB(const T& color)
{
    return toA98RGB(toXYZA(color));
}

template<typename T> DisplayP3<float> toDisplayP3(const T& color)
{
    return toDisplayP3(toXYZA(color));
}

template<typename T> ExtendedSRGBA<float> toExtendedSRGBA(const T& color)
{
    return toExtendedSRGBA(toXYZA(color));
}

template<typename T> HSLA<float> toHSLA(const T& color)
{
    return toHSLA(toXYZA(color));
}

template<typename T> LCHA<float> toLCHA(const T& color)
{
    return toLCHA(toXYZA(color));
}

template<typename T> Lab<float> toLab(const T& color)
{
    return toLab(toXYZA(color));
}

template<typename T> LinearA98RGB<float> toLinearA98RGB(const T& color)
{
    return toLinearA98RGB(toXYZA(color));
}

template<typename T> LinearDisplayP3<float> toLinearDisplayP3(const T& color)
{
    return toLinearDisplayP3(toXYZA(color));
}

template<typename T> LinearExtendedSRGBA<float> toLinearExtendedSRGBA(const T& color)
{
    return toLinearExtendedSRGBA(toXYZA(color));
}

template<typename T> LinearRec2020<float> toLinearRec2020(const T& color)
{
    return toLinearRec2020(toXYZA(color));
}

template<typename T> LinearSRGBA<float> toLinearSRGBA(const T& color)
{
    return toLinearSRGBA(toXYZA(color));
}

template<typename T> Rec2020<float> toRec2020(const T& color)
{
    return toRec2020(toXYZA(color));
}

template<typename T> SRGBA<float> toSRGBA(const T& color)
{
    return toSRGBA(toXYZA(color));
}


template<typename T, typename Functor> constexpr decltype(auto) callWithColorType(const ColorComponents<T>& components, ColorSpace colorSpace, Functor&& functor)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<A98RGB<T>>(components));
    case ColorSpace::DisplayP3:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<DisplayP3<T>>(components));
    case ColorSpace::Lab:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<Lab<T>>(components));
    case ColorSpace::LinearRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<LinearSRGBA<T>>(components));
    case ColorSpace::Rec2020:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<Rec2020<T>>(components));
    case ColorSpace::SRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<SRGBA<T>>(components));
    }

    ASSERT_NOT_REACHED();
    return std::invoke(std::forward<Functor>(functor), makeFromComponents<SRGBA<T>>(components));
}

} // namespace WebCore
