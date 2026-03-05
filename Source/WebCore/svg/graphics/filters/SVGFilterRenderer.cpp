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
#include "SVGFilterRenderer.h"

#include "ElementChildIteratorInlines.h"
#include "FilterResults.h"
#include "GeometryUtilities.h"
#include "Logging.h"
#include "SVGFilterEffectGraph.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveGraph.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include <numbers>

namespace WebCore {

static constexpr unsigned maxCountChildNodes = 200;

RefPtr<SVGFilterRenderer> SVGFilterRenderer::create(SVGElement *contextElement, SVGFilterElement& filterElement, const FilterGeometry& geometry, OptionSet<FilterRenderingMode> preferredRenderingModes, const GraphicsContext& destinationContext, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
{
    auto filter = adoptRef(*new SVGFilterRenderer(geometry, filterElement.primitiveUnits(), renderingResourceIdentifier));

    auto result = buildExpression(contextElement, filterElement, filter, destinationContext);
    if (!result)
        return nullptr;

    auto& expression = std::get<SVGFilterExpression>(*result);
    auto& effects = std::get<FilterEffectVector>(*result);

    ASSERT(!expression.isEmpty());
    ASSERT(!effects.isEmpty());
    filter->setExpression(WTF::move(expression));
    filter->setEffects(WTF::move(effects));

    filter->setFilterRenderingModes(preferredRenderingModes);

    LOG_WITH_STREAM(Filters, stream << "SVGFilterRenderer::create - rendering modes " << filter->filterRenderingModes());
    return filter;
}

Ref<SVGFilterRenderer> SVGFilterRenderer::create(SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression, FilterEffectVector&& effects, const FilterGeometry& geometry, OptionSet<FilterRenderingMode> preferredRenderingModes, bool showDebugOverlay, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
{
    Ref filter = adoptRef(*new SVGFilterRenderer(geometry, primitiveUnits, WTF::move(expression), WTF::move(effects), renderingResourceIdentifier));
    // Setting filter rendering modes cannot be moved to the constructor because it ends up
    // calling supportedFilterRenderingModes() which is a virtual function.
    filter->setFilterRenderingModes(preferredRenderingModes);
    filter->setIsShowingDebugOverlay(showDebugOverlay);
    return filter;
}

SVGFilterRenderer::SVGFilterRenderer(const FilterGeometry& geometry, SVGUnitTypes::SVGUnitType primitiveUnits, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : Filter(Filter::Type::SVGFilterRenderer, geometry, renderingResourceIdentifier)
    , m_primitiveUnits(primitiveUnits)
{
}

SVGFilterRenderer::SVGFilterRenderer(const FilterGeometry& geometry, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression, FilterEffectVector&& effects, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : Filter(Filter::Type::SVGFilterRenderer, geometry, renderingResourceIdentifier)
    , m_primitiveUnits(primitiveUnits)
    , m_expression(WTF::move(expression))
    , m_effects(WTF::move(effects))
{
}

static std::optional<std::tuple<SVGFilterEffectGraph, FilterEffectGeometryMap>> buildFilterEffectGraph(SVGElement *contextElement, SVGFilterElement& filterElement, const SVGFilterRenderer& filter, const GraphicsContext& destinationContext)
{
    if (filterElement.countChildNodes() > maxCountChildNodes)
        return std::nullopt;

#if USE(CAIRO)
    const auto colorSpace = filterElement.colorInterpolation() == ColorInterpolation::LinearRGB ? DestinationColorSpace::LinearSRGB() : DestinationColorSpace::SRGB();
#else
    const auto colorSpace = DestinationColorSpace::SRGB();
#endif

    SVGFilterEffectGraph graph(SourceGraphic::create(colorSpace), SourceAlpha::create(colorSpace));
    FilterEffectGeometryMap effectGeometryMap;

    for (Ref effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        auto inputs = graph.getNamedNodes(effectElement->filterEffectInputsNames());
        if (!inputs)
            return std::nullopt;

        auto effect = effectElement->filterEffect(*inputs, destinationContext);
        if (!effect)
            return std::nullopt;

        if (auto flags = effectElement->effectGeometryFlags()) {
            auto effectBoundaries = SVGLengthContext::resolveRectangle(contextElement, effectElement.get(), filter.primitiveUnits(), filter.referenceBox());
            effectGeometryMap.add(*effect, FilterEffectGeometry(effectBoundaries, flags));
        }

        if (effectElement->colorInterpolation() == ColorInterpolation::LinearRGB)
            effect->setOperatingColorSpace(DestinationColorSpace::LinearSRGB());

        graph.addNamedNode(AtomString { effectElement->result() }, { *effect });
        graph.setNodeInputs(*effect, WTF::move(*inputs));
    }

    return { { WTF::move(graph), WTF::move(effectGeometryMap) } };
}

std::optional<std::tuple<SVGFilterExpression, FilterEffectVector>> SVGFilterRenderer::buildExpression(SVGElement *contextElement, SVGFilterElement& filterElement, const SVGFilterRenderer& filter, const GraphicsContext& destinationContext)
{
    auto result = buildFilterEffectGraph(contextElement, filterElement, filter, destinationContext);
    if (!result)
        return std::nullopt;

    auto& graph = std::get<SVGFilterEffectGraph>(*result);
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
    return { { WTF::move(expression), WTF::move(effects) } };
}

static std::optional<SVGFilterPrimitiveGraph> buildFilterPrimitiveGraph(SVGFilterElement& filterElement)
{
    auto countChildNodes = filterElement.countChildNodes();
    if (!countChildNodes || countChildNodes > maxCountChildNodes)
        return std::nullopt;

    SVGFilterPrimitiveGraph graph;

    for (Ref effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        // We should not be strict about not finding the input primitives here because SourceGraphic and SourceAlpha do not have primitives.
        auto inputs = graph.getNamedNodes(effectElement->filterEffectInputsNames()).value_or(SVGFilterPrimitiveGraph::NodeVector());
        graph.addNamedNode(AtomString { effectElement->result() }, effectElement.copyRef());
        graph.setNodeInputs(effectElement, WTF::move(inputs));
    }

    return graph;
}

bool SVGFilterRenderer::isIdentity(SVGFilterElement& filterElement)
{
    auto graph = buildFilterPrimitiveGraph(filterElement);
    if (!graph)
        return false;

    bool isIdentity = true;
    graph->visit([&](SVGFilterPrimitiveStandardAttributes& primitive, unsigned) {
        if (!primitive.isIdentity())
            isIdentity = false;
    });

    return isIdentity;
}

IntOutsets SVGFilterRenderer::calculateOutsets(SVGFilterElement& filterElement, const FloatRect& targetBoundingBox)
{
    auto graph = buildFilterPrimitiveGraph(filterElement);
    if (!graph)
        return { };

    Vector<std::pair<IntOutsets, unsigned>> outsetsStack;

    // Remove the outsets of the last level and return their maximum.
    auto lastLevelOutsets([](auto& outsetsStack) -> IntOutsets {
        IntOutsets lastLevelOutsets;
        for (unsigned lastLevel = outsetsStack.last().second; lastLevel == outsetsStack.last().second; outsetsStack.takeLast())
            lastLevelOutsets = max(lastLevelOutsets, outsetsStack.last().first);
        return lastLevelOutsets;
    });

    bool result = graph->visit([&](SVGFilterPrimitiveStandardAttributes& primitive, unsigned level) {
        auto primitiveOutsets = primitive.outsets(targetBoundingBox, filterElement.primitiveUnits());
        unsigned lastLevel = outsetsStack.isEmpty() ? 0 : outsetsStack.last().second;

        // Expand the last outsets of this level with the maximum of the outsets of its children.
        if (level < lastLevel) {
            auto childrenOutsets = lastLevelOutsets(outsetsStack);
            outsetsStack.last().first += childrenOutsets;
        }

        outsetsStack.append(std::make_pair(primitiveOutsets, level));
    });

    if (!result)
        return IntOutsets();

    ASSERT(!outsetsStack.isEmpty());

    // Calculate the whole filter outsets by going back to the lastNode of the graph.
    while (outsetsStack.size() > 1) {
        auto childrenOutsets = lastLevelOutsets(outsetsStack);
        outsetsStack.last().first += childrenOutsets;
    }

    return outsetsStack.takeLast().first;
}

FloatSize SVGFilterRenderer::calculateResolvedSize(const FloatSize& size, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits)
{
    return primitiveUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX ? size * targetBoundingBox.size() : size;
}

FloatSize SVGFilterRenderer::resolvedSize(const FloatSize& size) const
{
    return calculateResolvedSize(size, referenceBox(), m_primitiveUnits);
}

FloatPoint3D SVGFilterRenderer::resolvedPoint3D(const FloatPoint3D& point) const
{
    if (m_primitiveUnits != SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        return point;

    auto referenceBox = this->referenceBox();
    FloatPoint3D resolvedPoint;
    resolvedPoint.setX(referenceBox.x() + point.x() * referenceBox.width());
    resolvedPoint.setY(referenceBox.y() + point.y() * referenceBox.height());

    // https://www.w3.org/TR/SVG/filters.html#fePointLightZAttribute and https://www.w3.org/TR/SVG/coords.html#Units_viewport_percentage
    resolvedPoint.setZ(point.z() * euclidianDistance(referenceBox.minXMinYCorner(), referenceBox.maxXMaxYCorner()) / std::numbers::sqrt2_v<float>);

    return resolvedPoint;
}

OptionSet<FilterRenderingMode> SVGFilterRenderer::supportedFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes) const
{
    OptionSet<FilterRenderingMode> modes = allFilterRenderingModes;

    for (auto& effect : m_effects) {
        auto effectModes = effect->supportedFilterRenderingModes(preferredFilterRenderingModes);
#if !LOG_DISABLED
        if (preferredFilterRenderingModes.contains(FilterRenderingMode::Accelerated) && !effectModes.contains(FilterRenderingMode::Accelerated))
            LOG_WITH_STREAM(Filters, stream << "SVGFilterRenderer::supportedFilterRenderingModes: accelerated rendering not supported by " << effect.get());
#endif
        modes = modes & effectModes;
    }

    ASSERT(modes);
    return modes;
}

FilterEffectVector SVGFilterRenderer::effectsOfType(FilterFunction::Type filterType) const
{
    FilterEffectVector effects;

    for (auto& effect : m_effects) {
        if (effect->filterType() == filterType)
            effects.append(effect);
    }

    return effects;
}

FilterResults& SVGFilterRenderer::ensureResults(NOESCAPE const FilterResultsCreator& resultsCreator)
{
    if (!m_results)
        m_results = resultsCreator();
    return *m_results;
}

void SVGFilterRenderer::clearEffectResult(FilterEffect& effect)
{
    if (m_results)
        m_results->clearEffectResult(effect);
}

void SVGFilterRenderer::mergeEffects(const FilterEffectVector& effects)
{
    ASSERT(m_effects.size() == effects.size());

    for (unsigned index = 0; index < m_effects.size(); ++index) {
        if (arePointingToEqualData(m_effects[index], effects[index]))
            continue;

        clearEffectResult(m_effects[index]);
        m_effects[index] = effects[index];
    }
}

RefPtr<FilterImage> SVGFilterRenderer::apply(const Filter&, FilterImage& sourceImage, FilterResults& results)
{
    return apply(&sourceImage, results);
}

RefPtr<FilterImage> SVGFilterRenderer::apply(FilterImage* sourceImage, FilterResults& results)
{
    ASSERT(!m_expression.isEmpty());
    ASSERT(filterRenderingModes().contains(FilterRenderingMode::Software));
    ASSERT(isValidSVGFilterExpression(m_expression, m_effects));

    LOG_WITH_STREAM(Filters, stream << "\nSVGFilterRenderer " << this << " apply - filterRegion " << filterRegion() << " scale " << filterScale());

    FilterImageVector stack;

    for (auto& term : m_expression) {
        auto& effect = m_effects[term.index];
        auto& geometry = term.geometry;

        if (effect->filterType() == FilterEffect::Type::SourceGraphic) {
            if (RefPtr result = results.effectResult(effect)) {
                stack.append(result.releaseNonNull());
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

bool SVGFilterRenderer::isValidSVGFilterExpression(const SVGFilterExpression& expression, const FilterEffectVector& effects)
{
    if (expression.isEmpty() || effects.isEmpty())
        return false;

    for (const auto& term : expression) {
        if (term.index >= effects.size())
            return false;
    }

    return true;
}

FilterStyleVector SVGFilterRenderer::createFilterStyles(GraphicsContext& context, const Filter&, const FilterStyle& sourceStyle) const
{
    return createFilterStyles(context, sourceStyle);
}

FilterStyleVector SVGFilterRenderer::createFilterStyles(GraphicsContext& context, const FilterStyle& sourceStyle) const
{
    ASSERT(!m_expression.isEmpty());
    ASSERT(filterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

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

TextStream& SVGFilterRenderer::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
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
