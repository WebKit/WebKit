/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSCubicBezierEasingFunction.h"
#include "CSSLinearEasingFunction.h"
#include "CSSSpringEasingFunction.h"
#include "CSSStepsEasingFunction.h"

namespace WebCore {
namespace CSS {

// `EasingFunction` uses a wrapping a struct to allow forward declaration.
struct EasingFunction {
    std::variant<
        // <linear()>
        Keyword::Linear,        // Equivalent to linear(0, 1)
        LinearEasingFunction,

        // <cubic-bezier()>
        Keyword::Ease,          // Equivalent to cubic-bezier(0.25, 0.1, 0.25, 1)
        Keyword::EaseIn,        // Equivalent to cubic-bezier(0.42, 0, 1, 1)
        Keyword::EaseOut,       // Equivalent to cubic-bezier(0, 0, 0.58, 1)
        Keyword::EaseInOut,     // Equivalent to cubic-bezier(0.42, 0, 0.58, 1)
        CubicBezierEasingFunction,

        // <steps()>
        Keyword::StepStart,     // Equivalent to steps(1, start)
        Keyword::StepEnd,       // Equivalent to steps(1, end)
        StepsEasingFunction,

        // <spring()>
        SpringEasingFunction
    > value;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const EasingFunction&) const = default;
};

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::EasingFunction> = true;
