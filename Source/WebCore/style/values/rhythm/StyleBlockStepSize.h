/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'block-step-size'> = none | <length [0,∞]>
// https://www.w3.org/TR/css-rhythm-1/#propdef-block-step-size
struct BlockStepSize : ValueOrKeyword<Length<CSS::Nonnegative, float>, CSS::Keyword::None> {
    using Base::Base;
    using Length = typename Base::Value;

    bool isNone() const { return isKeyword(); }
    bool isLength() const { return isValue(); }
    std::optional<Length> tryLength() const { return tryValue(); }
};

// MARK: - Blending

template<> struct Blending<BlockStepSize> {
    auto canBlend(const BlockStepSize&, const BlockStepSize&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BlockStepSize&, const BlockStepSize&) -> bool;
    auto blend(const BlockStepSize&, const BlockStepSize&, const BlendingContext&) -> BlockStepSize;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BlockStepSize)
