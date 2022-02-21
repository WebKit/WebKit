/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "FilterResults.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SourceGraphic.h"

namespace WebCore {

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, RenderingMode renderingMode, const FloatSize& filterScale, ClipOperation clipOperation, const FloatRect& filterRegion, const FloatRect& targetBoundingBox)
{
    auto filter = adoptRef(*new SVGFilter(renderingMode, filterScale, clipOperation, filterRegion, targetBoundingBox, filterElement.primitiveUnits()));

    builder.setupBuiltinEffects(SourceGraphic::create());
    builder.setTargetBoundingBox(targetBoundingBox);
    builder.setPrimitiveUnits(filterElement.primitiveUnits());

    if (!builder.buildFilterEffects(filterElement))
        return nullptr;

    SVGFilterExpression expression;
    if (!builder.buildExpression(expression))
        return nullptr;

    ASSERT(!expression.isEmpty());
    filter->setExpression(WTFMove(expression));

    if (renderingMode == RenderingMode::Accelerated && !filter->supportsAcceleratedRendering())
        filter->setRenderingMode(RenderingMode::Unaccelerated);

    return filter;
}

RefPtr<SVGFilter> SVGFilter::create(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression)
{
    return adoptRef(*new SVGFilter(targetBoundingBox, primitiveUnits, WTFMove(expression)));
}

SVGFilter::SVGFilter(RenderingMode renderingMode, const FloatSize& filterScale, ClipOperation clipOperation, const FloatRect& filterRegion, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits)
    : Filter(Filter::Type::SVGFilter, renderingMode, filterScale, clipOperation, filterRegion)
    , m_targetBoundingBox(targetBoundingBox)
    , m_primitiveUnits(primitiveUnits)
{
}

SVGFilter::SVGFilter(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits, SVGFilterExpression&& expression)
    : Filter(Filter::Type::SVGFilter)
    , m_targetBoundingBox(targetBoundingBox)
    , m_primitiveUnits(primitiveUnits)
    , m_expression(WTFMove(expression))
{
}

FloatSize SVGFilter::resolvedSize(const FloatSize& size) const
{
    return m_primitiveUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX ? size * m_targetBoundingBox.size() : size;
}

bool SVGFilter::supportsAcceleratedRendering() const
{
    if (renderingMode() == RenderingMode::Unaccelerated)
        return false;

    ASSERT(!m_expression.isEmpty());
    for (auto& term : m_expression) {
        if (!term.effect->supportsAcceleratedRendering())
            return false;
    }

    return true;
}

FilterEffectVector SVGFilter::effectsOfType(FilterFunction::Type filterType) const
{
    HashSet<Ref<FilterEffect>> effects;

    for (auto& term : m_expression) {
        auto& effect = term.effect;
        if (effect->filterType() == filterType)
            effects.add(effect);
    }

    return copyToVector(effects);
}

RefPtr<FilterImage> SVGFilter::apply(const Filter&, FilterImage& sourceImage, FilterResults& results)
{
    return apply(&sourceImage, results);
}

RefPtr<FilterImage> SVGFilter::apply(FilterImage* sourceImage, FilterResults& results)
{
    ASSERT(!m_expression.isEmpty());

    FilterImageVector stack;

    for (auto& term : m_expression) {
        auto& effect = term.effect;
        auto geometry = term.geometry;

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

        auto result = term.effect->apply(*this, inputs, results, geometry);
        if (!result)
            return nullptr;

        stack.append(result.releaseNonNull());
    }
    
    ASSERT(stack.size() == 1);
    return stack.takeLast();
}

IntOutsets SVGFilter::outsets() const
{
    IntOutsets outsets;
    for (auto& term : m_expression)
        outsets += term.effect->outsets(*this);
    return outsets;
}

TextStream& SVGFilter::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    for (auto it = m_expression.rbegin(), end = m_expression.rend(); it != end; ++it) {
        auto& term = *it;
        
        // SourceAlpha is a built-in effect. No need to say SourceGraphic is its input.
        if (term.effect->filterType() == FilterEffect::Type::SourceAlpha)
            ++it;

        TextStream::IndentScope indentScope(ts, term.level);
        term.effect->externalRepresentation(ts, representation);
    }

    return ts;
}

} // namespace WebCore
