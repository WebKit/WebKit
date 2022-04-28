/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGFilterBuilder.h"

#include "ElementIterator.h"
#include "SVGFilterElement.h"
#include "SVGFilterGraph.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
#include "CSSComputedStyleDeclaration.h"
#include "CSSPrimitiveValueMappings.h"
#endif

namespace WebCore {

static constexpr unsigned maxTotalNumberFilterEffects = 100;
static constexpr unsigned maxCountChildNodes = 200;

static OptionSet<FilterEffectGeometry::Flags> effectGeometryFlagsForElement(SVGElement& element)
{
    OptionSet<FilterEffectGeometry::Flags> flags;

    if (element.hasAttribute(SVGNames::xAttr))
        flags.add(FilterEffectGeometry::Flags::HasX);

    if (element.hasAttribute(SVGNames::yAttr))
        flags.add(FilterEffectGeometry::Flags::HasY);

    if (element.hasAttribute(SVGNames::widthAttr))
        flags.add(FilterEffectGeometry::Flags::HasWidth);

    if (element.hasAttribute(SVGNames::heightAttr))
        flags.add(FilterEffectGeometry::Flags::HasHeight);

    return flags;
}

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
static ColorInterpolation colorInterpolationForElement(SVGElement& element)
{
    if (auto renderer = element.renderer())
        return renderer->style().svgStyle().colorInterpolationFilters();

    // Try to determine the property value from the computed style.
    if (auto value = ComputedStyleExtractor(&element).propertyValue(CSSPropertyColorInterpolationFilters, DoNotUpdateLayout)) {
        if (is<CSSPrimitiveValue>(value))
            return downcast<CSSPrimitiveValue>(*value);
    }

    return ColorInterpolation::Auto;
}
#endif

static bool appendSubGraphToExpression(const SVGFilterEffectsGraph& graph, const FilterEffectGeometryMap& effectGeometryMap, FilterEffect& effect, FilterEffectVector& stack, unsigned level, SVGFilterExpression& expression)
{
    // A cycle is detected.
    if (stack.containsIf([&](auto& item) { return item.ptr() == &effect; }))
        return false;

    auto effectGeometry = [&](FilterEffect& effect) -> std::optional<FilterEffectGeometry> {
        auto it = effectGeometryMap.find(effect);
        if (it != effectGeometryMap.end())
            return it->value;
        return std::nullopt;
    };

    stack.append(effect);

    expression.append({ effect, effectGeometry(effect), level });

    for (auto& input : graph.getNodeInputs(effect)) {
        if (!appendSubGraphToExpression(graph, effectGeometryMap, input, stack, level + 1, expression))
            return false;
    }

    ASSERT(!stack.isEmpty());
    ASSERT(stack.last().ptr() == &effect);

    stack.removeLast();
    return true;
}

static bool appendGraphToExpression(const SVGFilterEffectsGraph& graph, const FilterEffectGeometryMap& effectGeometryMap, SVGFilterExpression& expression)
{
    if (!graph.lastNode())
        return false;

    FilterEffectVector stack;
    if (!appendSubGraphToExpression(graph, effectGeometryMap, *graph.lastNode(), stack, 0, expression))
        return false;

    if (expression.size() > maxTotalNumberFilterEffects)
        return false;

    expression.reverse();
    expression.shrinkToFit();
    return true;
}

std::optional<SVGFilterExpression> SVGFilterBuilder::buildFilterExpression(SVGFilterElement& filterElement, const SVGFilter& filter, const GraphicsContext& destinationContext)
{
    if (filterElement.countChildNodes() > maxCountChildNodes)
        return std::nullopt;

    SVGFilterEffectsGraph graph(SourceGraphic::create(), SourceAlpha::create());
    FilterEffectGeometryMap effectGeometryMap;

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        auto inputs = graph.getNamedNodes(effectElement.filterEffectInputsNames());
        if (!inputs)
            return std::nullopt;

        auto effect = effectElement.filterEffect(filter, *inputs, destinationContext);
        if (!effect)
            return std::nullopt;

        if (auto flags = effectGeometryFlagsForElement(effectElement)) {
            auto effectBoundaries = SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(&effectElement, filter.primitiveUnits(), filter.targetBoundingBox());
            effectGeometryMap.add(*effect, FilterEffectGeometry(effectBoundaries, flags));
        }

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        if (colorInterpolationForElement(effectElement) == ColorInterpolation::LinearRGB)
            effect->setOperatingColorSpace(DestinationColorSpace::LinearSRGB());
#endif

        if (auto renderer = effectElement.renderer())
            m_effectRenderer.add(renderer, effect.get());

        graph.addNamedNode(AtomString { effectElement.result() }, { *effect });
        graph.setNodeInputs(*effect, WTFMove(*inputs));
    }

    SVGFilterExpression expression;
    if (!appendGraphToExpression(graph, effectGeometryMap, expression))
        return std::nullopt;

    return expression;
}

static IntOutsets calculateSubGraphOutsets(const SVGFilterPrimitivesGraph& graph, SVGFilterPrimitiveStandardAttributes& primitive, Vector<Ref<SVGFilterPrimitiveStandardAttributes>>& stack, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits)
{
    // A cycle is detected.
    if (stack.containsIf([&](auto& item) { return item.ptr() == &primitive; }))
        return { };

    stack.append(primitive);

    IntOutsets outsets;
    for (auto& input : graph.getNodeInputs(primitive))
        outsets += calculateSubGraphOutsets(graph, input, stack, targetBoundingBox, primitiveUnits);
    outsets += primitive.outsets(targetBoundingBox, primitiveUnits);

    ASSERT(!stack.isEmpty());
    ASSERT(stack.last().ptr() == &primitive);

    stack.removeLast();
    return outsets;
}

static IntOutsets calculateGraphOutsets(const SVGFilterPrimitivesGraph& graph, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits)
{
    if (!graph.lastNode())
        return { };
    
    Vector<Ref<SVGFilterPrimitiveStandardAttributes>> stack;
    return calculateSubGraphOutsets(graph, *graph.lastNode(), stack, targetBoundingBox, primitiveUnits);
}

IntOutsets SVGFilterBuilder::calculateFilterOutsets(SVGFilterElement& filterElement, const FloatRect& targetBoundingBox)
{
    if (filterElement.countChildNodes() > maxCountChildNodes)
        return { };

    SVGFilterPrimitivesGraph graph;

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        // We should not be strict about not finding the input primitives here because SourceGraphic and SourceAlpha do not have primitives.
        auto inputs = graph.getNamedNodes(effectElement.filterEffectInputsNames()).value_or(SVGFilterPrimitivesGraph::NodeVector());
        graph.addNamedNode(AtomString { effectElement.result() }, { effectElement });
        graph.setNodeInputs(effectElement, WTFMove(inputs));
    }

    return calculateGraphOutsets(graph, targetBoundingBox, filterElement.primitiveUnits());
}

} // namespace WebCore
