/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, FilterEffect& previousEffect)
{
    return create(filterElement, builder, filterScale, sourceImageRect, filterRegion, filterRegion, &previousEffect);
}

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, const FloatRect& targetBoundingBox)
{
    return create(filterElement, builder, filterScale, sourceImageRect, filterRegion, targetBoundingBox, nullptr);
}

RefPtr<SVGFilter> SVGFilter::create(SVGFilterElement& filterElement, SVGFilterBuilder& builder, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, const FloatRect& targetBoundingBox, FilterEffect* previousEffect)
{
    bool primitiveBoundingBoxMode = filterElement.primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    auto filter = adoptRef(*new SVGFilter(filterScale, sourceImageRect, filterRegion, targetBoundingBox, primitiveBoundingBoxMode));

    if (!previousEffect)
        builder.setupBuiltinEffects(SourceGraphic::create(filter));
    else
        builder.setupBuiltinEffects({ *previousEffect });

    builder.setTargetBoundingBox(targetBoundingBox);
    builder.setPrimitiveUnits(filterElement.primitiveUnits());

    auto lastEffect = builder.buildFilterEffects(filter, filterElement);
    if (!lastEffect)
        return nullptr;

    filter->setLastEffect(WTFMove(lastEffect));
    return filter;
}

SVGFilter::SVGFilter(const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, const FloatRect& targetBoundingBox, bool effectBBoxMode)
    : Filter(Filter::Type::SVGFilter, filterScale, sourceImageRect, filterRegion)
    , m_targetBoundingBox(targetBoundingBox)
    , m_effectBBoxMode(effectBBoxMode)
{
}

FloatSize SVGFilter::scaledByFilterScale(FloatSize size) const
{
    if (m_effectBBoxMode)
        size = size * m_targetBoundingBox.size();

    return Filter::scaledByFilterScale(size);
}

IntOutsets SVGFilter::outsets() const
{
    return m_lastEffect->outsets();
}

void SVGFilter::clearResult()
{
    m_lastEffect->clearResultsRecursive();
}

} // namespace WebCore
