/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(THREADED_ANIMATIONS)

#include "AnimationUtilities.h"
#include <optional>
#include <wtf/Variant.h>

namespace WebCore {

template<typename> struct AcceleratedEffectInterpolation;

struct AcceleratedEffectInterpolationCanBlendInvoker {
    template<typename T> bool operator()(const T& a, const T& b) const
    {
        return AcceleratedEffectInterpolation<T>{}.canBlend(a, b);
    }
};
inline constexpr AcceleratedEffectInterpolationCanBlendInvoker canBlendForAcceleratedEffect{};

struct AcceleratedEffectInterpolationBlendInvoker {
    template<typename T> T operator()(const T& a, const T& b, const BlendingContext& context) const
    {
        return AcceleratedEffectInterpolation<T>{}.blend(a, b, context);
    }
};
inline constexpr AcceleratedEffectInterpolationBlendInvoker blendForAcceleratedEffect{};

// Specialization for `std::optional`.
template<typename T> struct AcceleratedEffectInterpolation<std::optional<T>> {
    auto canBlend(const std::optional<T>& a, const std::optional<T>& b) -> bool
    {
        if (a && b)
            return WebCore::canBlendForAcceleratedEffect(*a, *b);
        return !a && !b;
    }
    auto blend(const std::optional<T>& a, const std::optional<T>& b, const BlendingContext& context) -> std::optional<T>
    {
        if (a && b)
            return WebCore::blendForAcceleratedEffect(*a, *b, context);
        return std::nullopt;
    }
};

// Specialization for `Variant`.
template<typename... Ts> struct AcceleratedEffectInterpolation<Variant<Ts...>> {
    auto canBlend(const Variant<Ts...>& a, const Variant<Ts...>& b) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            []<typename T>(const T& a, const T& b) -> bool {
                return WebCore::canBlendForAcceleratedEffect(a, b);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
    auto blend(const Variant<Ts...>& a, const Variant<Ts...>& b, const BlendingContext& context) -> Variant<Ts...>
    {
        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> Variant<Ts...> {
                return WebCore::blendForAcceleratedEffect(a, b, context);
            },
            [](const auto&, const auto&) -> Variant<Ts...> {
                RELEASE_ASSERT_NOT_REACHED();
            }
        ), a, b);
    }
};

template<> struct AcceleratedEffectInterpolation<float> {
    auto canBlend(float, float) -> bool
    {
        return true;
    }
    auto blend(float a, float b, const BlendingContext& context) -> float
    {
        return WebCore::blend(a, b, context);
    }
};

template<> struct AcceleratedEffectInterpolation<double> {
    auto canBlend(double, double) -> bool
    {
        return true;
    }
    auto blend(double a, double b, const BlendingContext& context) -> double
    {
        return WebCore::blend(a, b, context);
    }
};

template<> struct AcceleratedEffectInterpolation<FloatPoint> {
    auto canBlend(FloatPoint, FloatPoint) -> bool
    {
        return true;
    }
    auto blend(FloatPoint a, FloatPoint b, const BlendingContext& context) -> FloatPoint
    {
        return WebCore::blend(a, b, context);
    }
};

template<> struct AcceleratedEffectInterpolation<FloatSize> {
    auto canBlend(FloatSize, FloatSize) -> bool
    {
        return true;
    }
    auto blend(FloatSize a, FloatSize b, const BlendingContext& context) -> FloatSize
    {
        return WebCore::blend(a, b, context);
    }
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
