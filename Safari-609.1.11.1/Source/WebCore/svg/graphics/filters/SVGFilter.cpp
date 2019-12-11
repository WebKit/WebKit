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

namespace WebCore {

SVGFilter::SVGFilter(const AffineTransform& absoluteTransform, const FloatRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
    : Filter(absoluteTransform)
    , m_absoluteSourceDrawingRegion(absoluteSourceDrawingRegion)
    , m_targetBoundingBox(targetBoundingBox)
    , m_filterRegion(filterRegion)
    , m_effectBBoxMode(effectBBoxMode)
{
    m_absoluteFilterRegion = absoluteTransform.mapRect(filterRegion);
}

FloatSize SVGFilter::scaledByFilterResolution(FloatSize size) const
{
    if (m_effectBBoxMode)
        size = size * m_targetBoundingBox.size();

    return Filter::scaledByFilterResolution(size) * m_absoluteFilterRegion.size() / m_filterRegion.size();
}

Ref<SVGFilter> SVGFilter::create(const AffineTransform& absoluteTransform, const FloatRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
{
    return adoptRef(*new SVGFilter(absoluteTransform, absoluteSourceDrawingRegion, targetBoundingBox, filterRegion, effectBBoxMode));
}

} // namespace WebCore
