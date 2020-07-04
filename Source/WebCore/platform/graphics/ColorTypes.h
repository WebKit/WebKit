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

namespace WebCore {

template<typename T> struct SRGBA {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> SRGBA(T, T, T, T) -> SRGBA<T>;

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

template<typename T, typename Functor> void forEachNonAlphaComponent(SRGBA<T>& color, Functor&& f)
{
    color.red = f(color.red);
    color.green = f(color.green);
    color.blue = f(color.blue);
}


template<typename T> struct LinearSRGBA {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> LinearSRGBA(T, T, T, T) -> LinearSRGBA<T>;

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


template<typename T> struct DisplayP3 {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> DisplayP3(T, T, T, T) -> DisplayP3<T>;

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


template<typename T> struct LinearDisplayP3 {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> LinearDisplayP3(T, T, T, T) -> LinearDisplayP3<T>;

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


template<typename T> struct HSLA {
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
template<typename T> struct CMYKA {
    T cyan;
    T magenta;
    T yellow;
    T black;
    T alpha;
};

template<typename T> CMYKA(T, T, T, T, T) -> CMYKA<T>;

template<typename T> constexpr bool operator==(const CMYKA<T>& a, const CMYKA<T>& b)
{
    return a.cyan == b.cyan && a.magenta == b.magenta && a.yellow == b.yellow && a.black == b.black && a.alpha == b.alpha;
}

template<typename T> constexpr bool operator!=(const CMYKA<T>& a, const CMYKA<T>& b)
{
    return !(a == b);
}


template<typename T> struct XYZA {
    T x;
    T y;
    T z;
    T alpha;
};

template<typename T> XYZA(T, T, T, T) -> XYZA<T>;

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

struct ARGB {
    uint32_t value;
};

constexpr ARGB asARGB(SRGBA<uint8_t> color)
{
    return { static_cast<unsigned>(color.alpha << 24 | color.red << 16 | color.green << 8 | color.blue) };
}

constexpr SRGBA<uint8_t> asSRGBA(ARGB color)
{
    return { static_cast<uint8_t>(color.value >> 16), static_cast<uint8_t>(color.value >> 8), static_cast<uint8_t>(color.value), static_cast<uint8_t>(color.value >> 24) };
}

} // namespace WebCore
