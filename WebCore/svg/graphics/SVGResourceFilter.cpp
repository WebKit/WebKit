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
#include "SVGResourceFilter.h"

#include "SVGRenderTreeAsText.h"
#include "SVGFilterEffect.h"
#include "TextStream.h"

namespace WebCore {

SVGResourceFilter::SVGResourceFilter()
    : m_platformData(createPlatformData())
    , m_filterBBoxMode(false)
    , m_effectBBoxMode(false)
    , m_xBBoxMode(false)
    , m_yBBoxMode(false)
{
}

void SVGResourceFilter::clearEffects()
{
    m_effects.clear();
}

void SVGResourceFilter::addFilterEffect(SVGFilterEffect* effect)
{
    ASSERT(effect);

    if (effect) {
        ASSERT(effect->filter() == this);
        m_effects.append(effect);
    }
}

FloatRect SVGResourceFilter::filterBBoxForItemBBox(const FloatRect& itemBBox) const
{
    FloatRect filterBBox = filterRect();

    float xOffset = 0.0f;
    float yOffset = 0.0f;

    if (!effectBoundingBoxMode()) {
        xOffset = itemBBox.x();
        yOffset = itemBBox.y();
    }

    if (filterBoundingBoxMode()) {
        filterBBox = FloatRect(xOffset + filterBBox.x() * itemBBox.width(),
                               yOffset + filterBBox.y() * itemBBox.height(),
                               filterBBox.width() * itemBBox.width(),
                               filterBBox.height() * itemBBox.height());
    } else {
        if (xBoundingBoxMode())
            filterBBox.setX(xOffset + filterBBox.x());

        if (yBoundingBoxMode())
            filterBBox.setY(yOffset + filterBBox.y());
    }

    return filterBBox;
}

TextStream& SVGResourceFilter::externalRepresentation(TextStream& ts) const
{
    ts << "[type=FILTER] ";

    FloatRect bbox = filterRect();
    static FloatRect defaultFilterRect(0, 0, 1, 1);

    if (!filterBoundingBoxMode() || bbox != defaultFilterRect) {
        ts << " [bounding box=";
        if (filterBoundingBoxMode()) {
            bbox.scale(100.f);
            ts << "at (" << bbox.x() << "%," <<  bbox.y() << "%) size " << bbox.width() << "%x" << bbox.height() << "%";
        } else
            ts << filterRect();
        ts << "]";
    }

    if (!filterBoundingBoxMode()) // default is true
        ts << " [bounding box mode=" << filterBoundingBoxMode() << "]";
    if (effectBoundingBoxMode()) // default is false
        ts << " [effect bounding box mode=" << effectBoundingBoxMode() << "]";
    if (m_effects.size() > 0)
        ts << " [effects=" << m_effects << "]";

    return ts;
}

SVGResourceFilter* getFilterById(Document* document, const AtomicString& id)
{
    SVGResource* resource = getResourceById(document, id);
    if (resource && resource->isFilter())
        return static_cast<SVGResourceFilter*>(resource);

    return 0;
}


} // namespace WebCore

#endif // ENABLE(SVG)
