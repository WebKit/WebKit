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

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <optional>
#include <wtf/Variant.h>

namespace WebCore {

struct BlendingContext;
template<typename> struct AcceleratedEffectInterpolation;

// ray() = ray( <angle> && <ray-size>? && contain? && [at <position>]? )
// <ray-size> = closest-side | closest-corner | farthest-side | farthest-corner | sides
struct AcceleratedEffectRayFunction {
    enum class RaySize : uint8_t { ClosestCorner, ClosestSide, FarthestCorner, FarthestSide, Sides };
    struct Contain { constexpr bool operator==(const Contain&) const = default; };

    double angle { 0 };
    RaySize size;
    std::optional<Contain> contain;
    std::optional<FloatPoint> position;

    constexpr bool operator==(const AcceleratedEffectRayFunction&) const = default;
};

// MARK: - Blending

template<> struct AcceleratedEffectInterpolation<AcceleratedEffectRayFunction> {
    auto canBlend(const AcceleratedEffectRayFunction&, const AcceleratedEffectRayFunction&) -> bool;
    auto blend(const AcceleratedEffectRayFunction&, const AcceleratedEffectRayFunction&, const BlendingContext&) -> AcceleratedEffectRayFunction;
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
