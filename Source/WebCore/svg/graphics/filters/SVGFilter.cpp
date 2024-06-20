/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "SVGFilter.h"

#include "ElementChildIteratorInlines.h"
#include "FilterResults.h"
#include "GeometryUtilities.h"
#include "SVGFilterElement.h"
#include "SVGFilterGraph.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SourceGraphic.h"

namespace WebCore {

static constexpr unsigned maxCountChildNodes = 200;

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatSize& filterScale, const FloatRect& filterRegion, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
{
    auto filter = adoptRef(*new SVGFilter(filterScale, filterRegion, targetBoundingBox, filterElement.primitiveUnits(), renderingResourceIdentifier));

    auto result = buildExpression(filterElement, filter, destinationContext);
    if (!result)
        return nullptr;

    auto& expression = std::get<SVGFilterExpression>(*result);
    auto& effects = std::get<FilterEffectVector>(*result);

    ASSERT(!expression.isEmpty());
    ASSERT(!effects.isEmpty());
    filter->setExpression(WTFMove(expression));
    filter->setEffects(WTFMove(effects));

    filter->setFilterRenderingModes(preferredFilterRenderingModes);
    return filter;
}

Ref<SVGFilter> SVGFilter::create(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression, FilterEffectVector&& effects, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
{
    return adoptRef(*new SVGFilter(targetBoundingBox, primitiveUnits, WTFMove(expression), WTFMove(effects), renderingResourceIdentifier));
}

Ref<SVGFilter> SVGFilter::create(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression, FilterEffectVector&& effects, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier, OptionSet<FilterRenderingMode> filterRenderingModes, const FloatSize& filterScale, const FloatRect& filterRegion)
{
    Ref filter = adoptRef(*new SVGFilter(targetBoundingBox, primitiveUnits, WTFMove(expression), WTFMove(effects), renderingResourceIdentifier, filterScale, filterRegion));
    // Setting filter rendering modes cannot be moved to the constructor because it ends up
    // calling supportedFilterRenderingModes() which is a virtual function.
    filter->setFilterRenderingModes(filterRenderingModes);
    return filter;
}

SVGFilter::SVGFilter(const FloatSize& filterScale, const FloatRect& filterRegion, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : Filter(Filter::Type::SVGFilter, filterScale, filterRegion, renderingResourceIdentifier)
    , m_targetBoundingBox(targetBoundingBox)
    , m_primitiveUnits(primitiveUnits)
{
}

SVGFilter::SVGFilter(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression, FilterEffectVector&& effects, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : Filter(Filter::Type::SVGFilter, renderingResourceIdentifier)
    , m_targetBoundingBox(targetBoundingBox)
    , m_primitiveUnits(primitiveUnits)
    , m_expression(WTFMove(expression))
    , m_effects(WTFMove(effects))
{
}

SVGFilter::SVGFilter(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression, FilterEffectVector&& effects, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier, const FloatSize& filterScale, const FloatRect& filterRegion)
    : Filter(Filter::Type::SVGFilter, filterScale, filterRegion, renderingResourceIdentifier)
    , m_targetBoundingBox(targetBoundingBox)
    , m_primitiveUnits(primitiveUnits)
    , m_expression(WTFMove(expression))
    , m_effects(WTFMove(effects))
{
}

static std::optional<std::tuple<SVGFilterEffectsGraph, FilterEffectGeometryMap>> buildFilterEffectsGraph(SVGFilterElement& filterElement, const SVGFilter& filter, const GraphicsContext& destinationContext)
{
    if (filterElement.countChildNodes() > maxCountChildNodes)
        return std::nullopt;

    const auto colorSpace = filterElement.colorInterpolation() == ColorInterpolation::LinearRGB ? DestinationColorSpace::LinearSRGB() : DestinationColorSpace::SRGB();
    SVGFilterEffectsGraph graph(SourceGraphic::create(colorSpace), SourceAlpha::create(colorSpace));
    FilterEffectGeometryMap effectGeometryMap;

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        auto inputs = graph.getNamedNodes(effectElement.filterEffectInputsNames());
        if (!inputs)
            return std::nullopt;

        auto effect = effectElement.filterEffect(*inputs, destinationContext);
        if (!effect)
            return std::nullopt;

        if (auto flags = effectElement.effectGeometryFlags()) {
            auto effectBoundaries = SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(&effectElement, filter.primitiveUnits(), filter.targetBoundingBox());
            effectGeometryMap.add(*effect, FilterEffectGeometry(effectBoundaries, flags));
        }

        if (effectElement.colorInterpolation() == ColorInterpolation::LinearRGB)
            effect->setOperatingColorSpace(DestinationColorSpace::LinearSRGB());

        graph.addNamedNode(AtomString { effectElement.result() }, { *effect });
        graph.setNodeInputs(*effect, WTFMove(*inputs));
    }

    return { { WTFMove(graph), WTFMove(effectGeometryMap) } };
}

std::optional<std::tuple<SVGFilterExpression, FilterEffectVector>> SVGFilter::buildExpression(SVGFilterElement& filterElement, const SVGFilter& filter, const GraphicsContext& destinationContext)
{
    auto result = buildFilterEffectsGraph(filterElement, filter, destinationContext);
    if (!result)
        return std::nullopt;

    auto& graph = std::get<SVGFilterEffectsGraph>(*result);
    auto& effectGeometryMap = std::get<FilterEffectGeometryMap>(*result);

    auto effectGeometry = [&](FilterEffect& effect) -> std::optional<FilterEffectGeometry> {
        auto it = effectGeometryMap.find(effect);
        if (it != effectGeometryMap.end())
            return it->value;
        return std::nullopt;
    };
    
    SVGFilterExpression expression;
    auto effects = graph.nodes();

    bool success = graph.visit([&](FilterEffect& effect, unsigned level) {
        auto index = effects.findIf([&](auto& item) {
            return item.ptr() == &effect;
        });
        ASSERT(index != notFound);
        expression.append({ static_cast<unsigned>(index), level, effectGeometry(effect) });
    });

    if (!success)
        return std::nullopt;

    expression.reverse();
    expression.shrinkToFit();
    return { { WTFMove(expression), WTFMove(effects) } };
}

static std::optional<SVGFilterPrimitivesGraph> buildFilterPrimitivesGraph(SVGFilterElement& filterElement)
{
    auto countChildNodes = filterElement.countChildNodes();
    if (!countChildNodes || countChildNodes > maxCountChildNodes)
        return std::nullopt;

    SVGFilterPrimitivesGraph graph;

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        // We should not be strict about not finding the input primitives here because SourceGraphic and SourceAlpha do not have primitives.
        auto inputs = graph.getNamedNodes(effectElement.filterEffectInputsNames()).value_or(SVGFilterPrimitivesGraph::NodeVector());
        graph.addNamedNode(AtomString { effectElement.result() }, { effectElement });
        graph.setNodeInputs(effectElement, WTFMove(inputs));
    }

    return graph;
}

bool SVGFilter::isIdentity(SVGFilterElement& filterElement)
{
    auto graph = buildFilterPrimitivesGraph(filterElement);
    if (!graph)
        return false;

    bool isIdentity = true;
    graph->visit([&](SVGFilterPrimitiveStandardAttributes& primitive, unsigned) {
        if (!primitive.isIdentity())
            isIdentity = false;
    });

    return isIdentity;
}

IntOutsets SVGFilter::calculateOutsets(SVGFilterElement& filterElement, const FloatRect& targetBoundingBox)
{
    auto graph = buildFilterPrimitivesGraph(filterElement);
    if (!graph)
        return { };

    IntOutsets outsets;
    bool result = graph->visit([&](SVGFilterPrimitiveStandardAttributes& primitive, unsigned) {
        outsets += primitive.outsets(targetBoundingBox, filterElement.primitiveUnits());
    });

    return result ? outsets : IntOutsets();
}

FloatSize SVGFilter::calculateResolvedSize(const FloatSize& size, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits)
{
    return primitiveUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX ? size * targetBoundingBox.size() : size;
}

FloatSize SVGFilter::resolvedSize(const FloatSize& size) const
{
    return calculateResolvedSize(size, m_targetBoundingBox, m_primitiveUnits);
}

FloatPoint3D SVGFilter::resolvedPoint3D(const FloatPoint3D& point) const
{
    if (m_primitiveUnits != SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        return point;

    FloatPoint3D resolvedPoint;
    resolvedPoint.setX(m_targetBoundingBox.x() + point.x() * m_targetBoundingBox.width());
    resolvedPoint.setY(m_targetBoundingBox.y() + point.y() * m_targetBoundingBox.height());

    // https://www.w3.org/TR/SVG/filters.html#fePointLightZAttribute and https://www.w3.org/TR/SVG/coords.html#Units_viewport_percentage
    resolvedPoint.setZ(point.z() * euclidianDistance(m_targetBoundingBox.minXMinYCorner(), m_targetBoundingBox.maxXMaxYCorner()) / sqrtOfTwoFloat);

    return resolvedPoint;
}

OptionSet<FilterRenderingMode> SVGFilter::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = allFilterRenderingModes;

    for (auto& effect : m_effects)
        modes = modes & effect->supportedFilterRenderingModes();

    ASSERT(modes);
    return modes;
}

FilterEffectVector SVGFilter::effectsOfType(FilterFunction::Type filterType) const
{
    FilterEffectVector effects;

    for (auto& effect : m_effects) {
        if (effect->filterType() == filterType)
            effects.append(effect);
    }

    return effects;
}

FilterResults& SVGFilter::ensureResults(const FilterResultsCreator& resultsCreator)
{
    if (!m_results)
        m_results = resultsCreator();
    return *m_results;
}

void SVGFilter::clearEffectResult(FilterEffect& effect)
{
    if (m_results)
        m_results->clearEffectResult(effect);
}

void SVGFilter::mergeEffects(const FilterEffectVector& effects)
{
    ASSERT(m_effects.size() == effects.size());

    for (unsigned index = 0; index < m_effects.size(); ++index) {
        if (m_effects[index].get() == effects[index].get())
            continue;

        clearEffectResult(m_effects[index]);
        m_effects[index] = effects[index];
    }
}

RefPtr<FilterImage> SVGFilter::apply(const Filter&, FilterImage& sourceImage, FilterResults& results)
{
    return apply(&sourceImage, results);
}

RefPtr<FilterImage> SVGFilter::apply(FilterImage* sourceImage, FilterResults& results)
{
    ASSERT(!m_expression.isEmpty());
    ASSERT(supportedFilterRenderingModes().contains(FilterRenderingMode::Software));

    FilterImageVector stack;

    for (auto& term : m_expression) {
        auto& effect = m_effects[term.index];
        auto& geometry = term.geometry;

        if (effect->filterType() == FilterEffect::Type::SourceGraphic) {
            if (auto result = results.effectResult(effect)) {
                stack.append({ *result });
                continue;
            }

            if (!sourceImage)
                return nullptr;

            // Add sourceImage as an input to the SourceGraphic.
            stack.append(Ref { *sourceImage });
        }

        // Need to remove the inputs here in case the effect already has a result.
        auto inputs = effect->takeImageInputs(stack);

        auto result = effect->apply(*this, inputs, results, geometry);
        if (!result)
            return nullptr;

        stack.append(result.releaseNonNull());
    }
    
    ASSERT(stack.size() == 1);
    return stack.takeLast();
}

FilterStyleVector SVGFilter::createFilterStyles(GraphicsContext& context, const Filter&, const FilterStyle& sourceStyle) const
{
    return createFilterStyles(context, sourceStyle);
}

FilterStyleVector SVGFilter::createFilterStyles(GraphicsContext& context, const FilterStyle& sourceStyle) const
{
    ASSERT(!m_expression.isEmpty());
    ASSERT(supportedFilterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

    FilterStyleVector styles;
    FilterStyle lastStyle = sourceStyle;

    for (auto& term : m_expression) {
        auto& effect = m_effects[term.index];
        auto& geometry = term.geometry;

        if (effect->filterType() == FilterEffect::Type::SourceGraphic)
            continue;
        
        ASSERT(effect->numberOfImageInputs() == 1);
        auto style = effect->createFilterStyle(context, *this, lastStyle, geometry);

        lastStyle = style;
        styles.append(style);
    }

    return styles;
}

TextStream& SVGFilter::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    for (auto it = m_expression.rbegin(), end = m_expression.rend(); it != end; ++it) {
        auto& term = *it;
        auto& effect = m_effects[term.index];

        // SourceAlpha is a built-in effect. No need to say SourceGraphic is its input.
        if (effect->filterType() == FilterEffect::Type::SourceAlpha)
            ++it;

        TextStream::IndentScope indentScope(ts, term.level);
        effect->externalRepresentation(ts, representation);
    }

    return ts;
}

} // namespace WebCore
