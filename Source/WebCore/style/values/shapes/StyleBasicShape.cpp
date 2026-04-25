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

#include "AcceleratedEffectBasicShape.h"
#include "CSSBasicShapeValue.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "TransformOperationData.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<BasicShape>::operator()(const BasicShape& value, const RenderStyle& style, PathConversion conversion) -> CSS::BasicShape
{
    return WTF::switchOn(value,
        [&](const auto& shape) {
            return CSS::BasicShape { toCSS(shape, style) };
        },
        [&](const PathFunction& path) {
            return CSS::BasicShape { toCSS(path, style, conversion) };
        }
    );
}

auto ToStyle<CSS::BasicShape>::operator()(const CSS::BasicShape& value, const BuilderState& builderState, std::optional<float> zoom) -> BasicShape
{
    return WTF::switchOn(value,
        [&](const auto& shape) {
            return BasicShape { toStyle(shape, builderState) };
        },
        [&](const CSS::PathFunction& path) {
            return BasicShape { toStyle(path, builderState, zoom) };
        }
    );
}

Ref<CSSValue> CSSValueCreation<BasicShape>::operator()(CSSValuePool&, const RenderStyle& style, const BasicShape& value, PathConversion conversion)
{
    return CSSBasicShapeValue::create(toCSS(value, style, conversion));
}

BasicShape CSSValueConversion<BasicShape>::operator()(BuilderState& builderState, const CSSValue& value, std::optional<float> zoom)
{
    return toStyle(downcast<CSSBasicShapeValue>(value).shape(), builderState, zoom);
}

// MARK: - Serialization

void Serialize<BasicShape>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const BasicShape& value, PathConversion conversion)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style, conversion));
}

// MARK: - Blending

auto Blending<BasicShape>::canBlend(const BasicShape& a, const BasicShape& b) -> bool
{
    return WTF::visit(WTF::makeVisitor(
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
    return WTF::visit(WTF::makeVisitor(
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

WebCore::Path PathComputation<BasicShape>::operator()(const BasicShape& shape, const FloatRect& rect, ZoomFactor zoom)
{
    return WTF::switchOn(shape, [&](const auto& shape) { return WebCore::Style::path(shape, rect, zoom); });
}

std::optional<WebCore::Path> tryPath(const BasicShape& shape, const TransformOperationData& transformData, ZoomFactor zoom)
{
    if (auto motionPathData = transformData.motionPathData) {
        auto containingBlockRect = motionPathData->offsetRect().rect();
        return WTF::switchOn(shape,
            [&]<ShapeWithCenterCoordinate T>(const T& shape) -> std::optional<WebCore::Path> {
                if (!shape->position)
                    return pathForCenterCoordinate(*shape, containingBlockRect, motionPathData->usedStartingPosition, zoom);
                return path(shape, containingBlockRect, zoom);
            },
            [&](const auto& shape) -> std::optional<WebCore::Path> {
                return path(shape, containingBlockRect, zoom);
            }
        );
    }
    return path(shape, transformData.boundingBox, zoom);
}

// MARK: - Winding

WindRule WindRuleComputation<BasicShape>::operator()(const BasicShape& shape)
{
    return WTF::switchOn(shape,
        [&](const PathFunction& path) { return WebCore::Style::windRule(path); },
        [&](const PolygonFunction& polygon) { return WebCore::Style::windRule(polygon); },
        [&](const ShapeFunction& shape) { return WebCore::Style::windRule(shape); },
        [&](const auto&) { return WindRule::NonZero; }
    );
}

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATIONS)

AcceleratedEffectBasicShape Evaluation<BasicShape, AcceleratedEffectBasicShape>::operator()(const BasicShape& shape, const TransformOperationData& data, ZoomFactor zoom)
{
    return WTF::switchOn(shape,
        [&](const Style::CircleFunction& shape) -> AcceleratedEffectBasicShape {
            return { .function = evaluate<AcceleratedEffectCircleFunction>(shape, data, zoom) };
        },
        [&](const Style::EllipseFunction& shape) -> AcceleratedEffectBasicShape {
            return { .function = evaluate<AcceleratedEffectEllipseFunction>(shape, data, zoom) };
        },
        [&](const Style::InsetFunction& shape) -> AcceleratedEffectBasicShape {
            return { .function = evaluate<AcceleratedEffectInsetFunction>(shape, data, zoom) };
        },
        [&](const Style::PathFunction& shape) -> AcceleratedEffectBasicShape {
            return { .function = evaluate<AcceleratedEffectPathFunction>(shape, data, zoom) };
        },
        [&](const Style::PolygonFunction& shape) -> AcceleratedEffectBasicShape {
            return { .function = evaluate<AcceleratedEffectPolygonFunction>(shape, data, zoom) };
        },
        [&](const Style::ShapeFunction& shape) -> AcceleratedEffectBasicShape {
            return { .function = evaluate<AcceleratedEffectShapeFunction>(shape, data, zoom) };
        }
    );
}

#endif

} // namespace Style
} // namespace WebCore
