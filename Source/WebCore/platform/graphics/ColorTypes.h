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

#include "ColorSpace.h"

namespace WebCore {

template<typename> struct ColorComponents;
template<typename> struct ComponentTraits;

template<> struct ComponentTraits<uint8_t> {
    static constexpr uint8_t minValue = 0;
    static constexpr uint8_t maxValue = 255;
};

template<> struct ComponentTraits<float> {
    static constexpr float minValue = 0.0f;
    static constexpr float maxValue = 1.0f;
};

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
    static constexpr auto colorSpace = ColorSpace::SRGB;

    constexpr SRGBA(T red, T green, T blue, T alpha = ComponentTraits<T>::maxValue)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
    }

    constexpr SRGBA()
        : SRGBA { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

template<typename T> constexpr SRGBA<T> asSRGBA(const ColorComponents<T>& c)
{
    return { c[0], c[1], c[2], c[3] };
}

template<typename T> constexpr bool operator==(const SRGBA<T>& a, const SRGBA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const SRGBA<T>& a, const SRGBA<T>& b)
{
    return !(a == b);
}


template<typename T> struct LinearSRGBA : ColorWithAlphaHelper<LinearSRGBA<T>> {
    using ComponentType = T;
    static constexpr auto colorSpace = ColorSpace::LinearRGB;

    constexpr LinearSRGBA(T red, T green, T blue, T alpha = ComponentTraits<T>::maxValue)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
    }

    constexpr LinearSRGBA()
        : LinearSRGBA { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

template<typename T> constexpr LinearSRGBA<T> asLinearSRGBA(const ColorComponents<T>& c)
{
    return { c[0], c[1], c[2], c[3] };
}

template<typename T> constexpr bool operator==(const LinearSRGBA<T>& a, const LinearSRGBA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const LinearSRGBA<T>& a, const LinearSRGBA<T>& b)
{
    return !(a == b);
}


template<typename T> struct DisplayP3 : ColorWithAlphaHelper<DisplayP3<T>> {
    using ComponentType = T;
    static constexpr auto colorSpace = ColorSpace::DisplayP3;

    constexpr DisplayP3(T red, T green, T blue, T alpha = ComponentTraits<T>::maxValue)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
    }

    constexpr DisplayP3()
        : DisplayP3 { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

template<typename T> constexpr DisplayP3<T> asDisplayP3(const ColorComponents<T>& c)
{
    return { c[0], c[1], c[2], c[3] };
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

    constexpr LinearDisplayP3(T red, T green, T blue, T alpha = ComponentTraits<T>::maxValue)
        : red { red }
        , green { green }
        , blue { blue }
        , alpha { alpha }
    {
    }

    constexpr LinearDisplayP3()
        : LinearDisplayP3 { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

template<typename T> constexpr LinearDisplayP3<T> asLinearDisplayP3(const ColorComponents<T>& c)
{
    return { c[0], c[1], c[2], c[3] };
}

template<typename T> constexpr bool operator==(const LinearDisplayP3<T>& a, const LinearDisplayP3<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const LinearDisplayP3<T>& a, const LinearDisplayP3<T>& b)
{
    return !(a == b);
}


template<typename T> struct HSLA : ColorWithAlphaHelper<HSLA<T>> {
    using ComponentType = T;

    constexpr HSLA(T hue, T saturation, T lightness, T alpha = ComponentTraits<T>::maxValue)
        : hue { hue }
        , saturation { saturation }
        , lightness { lightness }
        , alpha { alpha }
    {
    }

    constexpr HSLA()
        : HSLA { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

template<typename T> constexpr HSLA<T> asHSLA(const ColorComponents<T>& c)
{
    return { c[0], c[1], c[2], c[3] };
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

    constexpr CMYKA(T cyan, T magenta, T yellow, T black, T alpha = ComponentTraits<T>::maxValue)
        : cyan { cyan }
        , magenta { magenta }
        , yellow { yellow }
        , black { black }
        , alpha { alpha }
    {
    }

    constexpr CMYKA()
        : CMYKA { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

    constexpr XYZA(T x, T y, T z, T alpha = ComponentTraits<T>::maxValue)
        : x { x }
        , y { y }
        , z { z }
        , alpha { alpha }
    {
    }

    constexpr XYZA()
        : XYZA { ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue, ComponentTraits<T>::minValue }
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

template<typename T> constexpr XYZA<T> asXYZA(const ColorComponents<T>& c)
{
    return { c[0], c[1], c[2], c[3] };
}

template<typename T> constexpr bool operator==(const XYZA<T>& a, const XYZA<T>& b)
{
    return asColorComponents(a) == asColorComponents(b);
}

template<typename T> constexpr bool operator!=(const XYZA<T>& a, const XYZA<T>& b)
{
    return !(a == b);
}


// Packed Color Formats

namespace Packed {

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

constexpr SRGBA<uint8_t> asSRGBA(Packed::RGBA color)
{
    return { static_cast<uint8_t>(color.value >> 24), static_cast<uint8_t>(color.value >> 16), static_cast<uint8_t>(color.value >> 8), static_cast<uint8_t>(color.value) };
}

constexpr SRGBA<uint8_t> asSRGBA(Packed::ARGB color)
{
    return { static_cast<uint8_t>(color.value >> 16), static_cast<uint8_t>(color.value >> 8), static_cast<uint8_t>(color.value), static_cast<uint8_t>(color.value >> 24) };
}

} // namespace WebCore
