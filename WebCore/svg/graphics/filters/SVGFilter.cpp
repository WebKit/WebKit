/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFilter.h"

namespace WebCore {

SVGFilter::SVGFilter(const AffineTransform& absoluteTransform, const FloatRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
    : Filter()
    , m_absoluteTransform(absoluteTransform)
    , m_absoluteSourceDrawingRegion(absoluteSourceDrawingRegion)
    , m_targetBoundingBox(targetBoundingBox)
    , m_filterRegion(filterRegion)
    , m_effectBBoxMode(effectBBoxMode)
{
    m_absoluteFilterRegion = absoluteTransform.mapRect(filterRegion);
}

void SVGFilter::determineFilterPrimitiveSubregion(FilterEffect* effect, const FloatRect& unionOfPreviousPrimitiveSubregions)
{
    FloatRect subRegionBBox = effect->effectBoundaries();
    FloatRect newSubRegion = subRegionBBox;

    if (m_effectBBoxMode) {
        newSubRegion = unionOfPreviousPrimitiveSubregions;

        if (effect->hasX())
            newSubRegion.setX(m_targetBoundingBox.x() + subRegionBBox.x() * m_targetBoundingBox.width());

       if (effect->hasY())
            newSubRegion.setY(m_targetBoundingBox.y() + subRegionBBox.y() * m_targetBoundingBox.height());

        if (effect->hasWidth())
            newSubRegion.setWidth(subRegionBBox.width() * m_targetBoundingBox.width());

        if (effect->hasHeight())
            newSubRegion.setHeight(subRegionBBox.height() * m_targetBoundingBox.height());
    } else {
        if (!effect->hasX())
            newSubRegion.setX(unionOfPreviousPrimitiveSubregions.x());

        if (!effect->hasY())
            newSubRegion.setY(unionOfPreviousPrimitiveSubregions.y());

        if (!effect->hasWidth())
            newSubRegion.setWidth(unionOfPreviousPrimitiveSubregions.width());

        if (!effect->hasHeight())
            newSubRegion.setHeight(unionOfPreviousPrimitiveSubregions.height());
    }

    // clip every filter effect to the filter region
    newSubRegion.intersect(m_filterRegion);

    effect->setFilterPrimitiveSubregion(newSubRegion);
    // TODO: Everything above should be moved to a first phase of layout in RenderSVGResourceFilterPrimitive.
    // The scaling of the subregion to the repaint rect should be merged with a more intelligent repaint logic
    // and moved to the second phase of layout in RenderSVGResourceFilterPrimitive.
    // See bug https://bugs.webkit.org/show_bug.cgi?id=45614.
    newSubRegion = m_absoluteTransform.mapRect(newSubRegion);
    newSubRegion.scale(filterResolution().width(), filterResolution().height());
    effect->setMaxEffectRect(enclosingIntRect(newSubRegion));
    if (!effect->isSourceInput())
        m_maxImageSize = m_maxImageSize.expandedTo(newSubRegion.size()); 
}

float SVGFilter::applyHorizontalScale(float value) const
{
    if (m_effectBBoxMode)
        value *= m_targetBoundingBox.width();
    return Filter::applyHorizontalScale(value) * m_absoluteFilterRegion.width() / m_filterRegion.width();
}

float SVGFilter::applyVerticalScale(float value) const
{
    if (m_effectBBoxMode)
        value *= m_targetBoundingBox.height();
    return Filter::applyVerticalScale(value) * m_absoluteFilterRegion.height() / m_filterRegion.height();
}

PassRefPtr<SVGFilter> SVGFilter::create(const AffineTransform& absoluteTransform, const FloatRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
{
    return adoptRef(new SVGFilter(absoluteTransform, absoluteSourceDrawingRegion, targetBoundingBox, filterRegion, effectBBoxMode));
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
