/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFilterEffect.h"

#include "SVGRenderTreeAsText.h"
#include "SVGResourceFilter.h"
#include "TextStream.h"

namespace WebCore {

SVGFilterEffect::SVGFilterEffect(SVGResourceFilter* filter)
    : m_filter(filter)
    , m_xBBoxMode(false)
    , m_yBBoxMode(false)
    , m_widthBBoxMode(false)
    , m_heightBBoxMode(false)
{
}

FloatRect SVGFilterEffect::primitiveBBoxForFilterBBox(const FloatRect& filterBBox, const FloatRect& itemBBox) const
{
    FloatRect subRegionBBox = subRegion();
    FloatRect useBBox = filterBBox;

    ASSERT(m_filter);
    if (!m_filter)
        return FloatRect();

    if (m_filter->effectBoundingBoxMode()) {
        if (!m_filter->filterBoundingBoxMode())
            useBBox = itemBBox;

        subRegionBBox = FloatRect(useBBox.x() + subRegionBBox.x() * useBBox.width(),
                                  useBBox.y() + subRegionBBox.y() * useBBox.height(),
                                  subRegionBBox.width() * useBBox.width(),
                                  subRegionBBox.height() * useBBox.height());
    } else {
        if (xBoundingBoxMode())
            subRegionBBox.setX(useBBox.x() + subRegionBBox.x() * useBBox.width());

        if (yBoundingBoxMode())
            subRegionBBox.setY(useBBox.y() + subRegionBBox.y() * useBBox.height());

        if (widthBoundingBoxMode())
            subRegionBBox.setWidth(subRegionBBox.width() * useBBox.width());

        if (heightBoundingBoxMode())
            subRegionBBox.setHeight(subRegionBBox.height() * useBBox.height());
    }

    return subRegionBBox;
}

FloatRect SVGFilterEffect::subRegion() const
{
    return m_subRegion;
}

void SVGFilterEffect::setSubRegion(const FloatRect& subRegion)
{
    m_subRegion = subRegion;
}

String SVGFilterEffect::in() const
{
    return m_in;
}

void SVGFilterEffect::setIn(const String& in)
{
    m_in = in;
}

String SVGFilterEffect::result() const
{
    return m_result;
}

void SVGFilterEffect::setResult(const String& result)
{
    m_result = result;
}

SVGResourceFilter* SVGFilterEffect::filter() const
{
    return m_filter;
}

void SVGFilterEffect::setFilter(SVGResourceFilter* filter)
{
    m_filter = filter;
}

TextStream& SVGFilterEffect::externalRepresentation(TextStream& ts) const
{
    if (!in().isEmpty())
        ts << "[in=\"" << in() << "\"]";
    if (!result().isEmpty())
        ts << " [result=\"" << result() << "\"]";
    if (!subRegion().isEmpty())
        ts << " [subregion=\"" << subRegion() << "\"]";
    return ts;
}

TextStream& operator<<(TextStream& ts, const SVGFilterEffect& e)
{
    return e.externalRepresentation(ts);
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
