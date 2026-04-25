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

#include <WebCore/CSSPolygonFunction.h>
#include <WebCore/StyleFillRule.h>
#include <WebCore/StylePathComputation.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleWindRuleComputation.h>

namespace WebCore {

struct AcceleratedEffectPolygonFunction;
struct TransformOperationData;

namespace Style {

struct Polygon {
    using Vertex = SpaceSeparatedPoint<LengthPercentage<CSS::AllUnzoomed>>;
    using Vertices = CommaSeparatedVector<Vertex>;

    // FIXME: Add support the "round" clause.

    std::optional<FillRule> fillRule;
    Vertices vertices;

    bool operator==(const Polygon&) const = default;
};
using PolygonFunction = FunctionNotation<CSSValuePolygon, Polygon>;

template<size_t I> const auto& get(const Polygon& value)
{
    if constexpr (!I)
        return value.fillRule;
    else if constexpr (I == 1)
        return value.vertices;
}

DEFINE_TYPE_MAPPING(CSS::Polygon, Polygon)

template<> struct PathComputation<Polygon> { WebCore::Path operator()(const Polygon&, const FloatRect&, ZoomFactor); };
template<> struct WindRuleComputation<Polygon> { WebCore::WindRule NODELETE operator()(const Polygon&); };

// MARK: - Blending

template<> struct Blending<Polygon> {
    bool NODELETE canBlend(const Polygon&, const Polygon&);
    auto blend(const Polygon&, const Polygon&, const BlendingContext&) -> Polygon;
};

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATIONS)

template<> struct Evaluation<PolygonFunction, AcceleratedEffectPolygonFunction> { AcceleratedEffectPolygonFunction operator()(const PolygonFunction&, const TransformOperationData&, ZoomFactor); };

#endif

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Polygon, 2)
