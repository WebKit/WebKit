/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SourceGraphic.h"

namespace WebCore {

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, RenderingMode renderingMode, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, ClipOperation clipOperation, FilterEffect& previousEffect)
{
    return create(filterElement, builder, renderingMode, filterScale, sourceImageRect, filterRegion, clipOperation, filterRegion, &previousEffect);
}

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, RenderingMode renderingMode, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, const FloatRect& targetBoundingBox)
{
    return create(filterElement, builder, renderingMode, filterScale, sourceImageRect, filterRegion, ClipOperation::Intersect, targetBoundingBox, nullptr);
}

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, RenderingMode renderingMode, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, ClipOperation clipOperation, const FloatRect& targetBoundingBox, FilterEffect* previousEffect)
{
    auto filter = adoptRef(*new SVGFilter(renderingMode, filterScale, sourceImageRect, filterRegion, clipOperation, targetBoundingBox, filterElement.primitiveUnits()));

    if (!previousEffect)
        builder.setupBuiltinEffects(SourceGraphic::create());
    else
        builder.setupBuiltinEffects({ *previousEffect });

    builder.setTargetBoundingBox(targetBoundingBox);
    builder.setPrimitiveUnits(filterElement.primitiveUnits());

    auto lastEffect = builder.buildFilterEffects(filterElement);
    if (!lastEffect)
        return nullptr;

    FilterEffectVector expression;
    if (!builder.buildExpression(expression))
        return nullptr;

    ASSERT(!expression.isEmpty());
    filter->setExpression(WTFMove(expression));
    
#if USE(CORE_IMAGE)
    if (!filter->supportsCoreImageRendering())
        filter->setRenderingMode(RenderingMode::Unaccelerated);
#endif

    return filter;
}

SVGFilter::SVGFilter(RenderingMode renderingMode, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, ClipOperation clipOperation, const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits)
    : Filter(Filter::Type::SVGFilter, renderingMode, filterScale, sourceImageRect, filterRegion, clipOperation)
    , m_targetBoundingBox(targetBoundingBox)
    , m_primitiveUnits(primitiveUnits)
{
}

FloatSize SVGFilter::resolvedSize(const FloatSize& size) const
{
    return m_primitiveUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX ? size * m_targetBoundingBox.size() : size;
}

#if USE(CORE_IMAGE)
bool SVGFilter::supportsCoreImageRendering() const
{
    if (renderingMode() == RenderingMode::Unaccelerated)
        return false;

    ASSERT(!m_expression.isEmpty());
    for (auto& effect : m_expression) {
        if (!effect->supportsCoreImageRendering())
            return false;
    }

    return true;
}
#endif

bool SVGFilter::apply(const Filter& filter)
{
    setSourceImage({ filter.sourceImage() });
    return apply();
}

RefPtr<FilterImage> SVGFilter::apply()
{
    ASSERT(!m_expression.isEmpty());
    for (auto& effect : m_expression) {
        if (!effect->apply(*this))
            return nullptr;
    }
    return lastEffect()->filterImage();
}

IntOutsets SVGFilter::outsets() const
{
    ASSERT(lastEffect());
    return lastEffect()->outsets();
}

void SVGFilter::clearResult()
{
    ASSERT(!m_expression.isEmpty());
    for (auto& effect : m_expression)
        effect->clearResult();
}

} // namespace WebCore
