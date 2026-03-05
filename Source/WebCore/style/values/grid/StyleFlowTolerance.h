/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/StyleLengthWrapper.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'flow-tolerance'> = normal | <length-percentage [0,∞]> | infinite
// https://drafts.csswg.org/css-grid-3/#placement-tolerance
struct FlowTolerance : LengthWrapperBase<LengthPercentage<CSS::NonnegativeUnzoomed>, CSS::Keyword::Normal, CSS::Keyword::Infinite> {
    using Base::Base;

    ALWAYS_INLINE bool isNormal() const { return holdsAlternative<CSS::Keyword::Normal>(); }
    ALWAYS_INLINE bool isInfinite() const { return holdsAlternative<CSS::Keyword::Infinite>(); }
};

// MARK: - Conversion

template<> struct CSSValueConversion<FlowTolerance> {
    auto operator()(BuilderState&, const CSSValue&) -> FlowTolerance;
};

// MARK: - Blending

template<> struct Blending<FlowTolerance> {
    auto canBlend(const FlowTolerance&, const FlowTolerance&) -> bool;
    auto blend(const FlowTolerance&, const FlowTolerance&, const BlendingContext&) -> FlowTolerance;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FlowTolerance)
