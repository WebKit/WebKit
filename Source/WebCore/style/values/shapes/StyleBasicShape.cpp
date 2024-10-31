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

#include "config.h"
#include "StyleBasicShape.h"

#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<BasicShape>::operator()(const BasicShape& value, const RenderStyle& style) -> CSS::BasicShape
{
    return WTF::switchOn(value, [&](const auto& alternative) { return CSS::BasicShape { toCSS(alternative, style) }; });
}

auto ToStyle<CSS::BasicShape>::operator()(const CSS::BasicShape& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable) -> BasicShape
{
    return WTF::switchOn(value, [&](const auto& alternative) { return BasicShape { toStyle(alternative, builderState, symbolTable) }; });
}

// MARK: - Blending

auto Blending<BasicShape>::canBlend(const BasicShape& a, const BasicShape& b) -> bool
{
    return std::visit(WTF::makeVisitor(
        []<typename T>(const T& a, const T& b) {
            return WebCore::Style::canBlend(a, b);
        },
        [](const ShapeFunction& a, const PathFunction& b) {
            return canBlendShapeWithPath(*a, *b);
        },
        [](const PathFunction& a, const ShapeFunction& b) {
            return canBlendShapeWithPath(*b, *a);
        },
        [](const auto&, const auto&) {
            return false;
        }
    ), a, b);
}

auto Blending<BasicShape>::blend(const BasicShape& a, const BasicShape& b, const BlendingContext& context) -> BasicShape
{
    return std::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> BasicShape {
            return { WebCore::Style::blend(a, b, context) };
        },
        [&](const ShapeFunction& a, const PathFunction& b) -> BasicShape {
            return { WebCore::Style::blend(a, ShapeFunction { *makeShapeFromPath(*b) }, context) };
        },
        [&](const PathFunction& a, const ShapeFunction& b) -> BasicShape {
            return { WebCore::Style::blend(ShapeFunction { *makeShapeFromPath(*a) }, b, context) };
        },
        [&](const auto&, const auto&) -> BasicShape {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a, b);
}


// MARK: - Path

WebCore::Path PathComputation<BasicShape>::operator()(const BasicShape& shape, const FloatRect& rect)
{
    return WTF::switchOn(shape, [&](const auto& shape) { return WebCore::Style::path(shape, rect); });
}

// MARK: - Winding

WindRule WindRuleComputation<BasicShape>::operator()(const BasicShape& shape)
{
    return WTF::switchOn(shape,
        [&](const Style::PathFunction& path) { return WebCore::Style::windRule(path); },
        [&](const Style::PolygonFunction& polygon) { return WebCore::Style::windRule(polygon); },
        [&](const Style::ShapeFunction& shape) { return WebCore::Style::windRule(shape); },
        [&](const auto&) { return WindRule::NonZero; }
    );
}

} // namespace Style
} // namespace WebCore
