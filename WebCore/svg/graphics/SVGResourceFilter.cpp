/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
                  2009 Dirk Schulze <krit@webkit.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGResourceFilter.h"

#include "FilterEffect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PlatformString.h"
#include "SVGFilter.h"
#include "SVGFilterBuilder.h"
#include "SVGRenderTreeAsText.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

SVGResourceFilter::SVGResourceFilter()
    : m_filterBBoxMode(false)
    , m_effectBBoxMode(false)
    , m_xBBoxMode(false)
    , m_yBBoxMode(false)
    , m_savedContext(0)
    , m_sourceGraphicBuffer(0)
{
    m_filterBuilder.set(new SVGFilterBuilder());
}

void SVGResourceFilter::addFilterEffect(SVGFilterPrimitiveStandardAttributes* effectAttributes, PassRefPtr<FilterEffect> effect)
{
    effectAttributes->setStandardAttributes(this, effect.get());
    builder()->add(effectAttributes->result(), effect);
}

FloatRect SVGResourceFilter::filterBBoxForItemBBox(const FloatRect& itemBBox) const
{
    FloatRect filterBBox = filterRect();

    if (filterBoundingBoxMode())
        filterBBox = FloatRect(itemBBox.x() + filterBBox.x() * itemBBox.width(),
                               itemBBox.y() + filterBBox.y() * itemBBox.height(),
                               filterBBox.width() * itemBBox.width(),
                               filterBBox.height() * itemBBox.height());

    return filterBBox;
}

void SVGResourceFilter::prepareFilter(GraphicsContext*& context, const RenderObject* object)
{
    m_itemBBox = object->objectBoundingBox();
    m_filterBBox = filterBBoxForItemBBox(m_itemBBox);

    // clip sourceImage to filterRegion
    FloatRect clippedSourceRect = m_itemBBox;
    clippedSourceRect.intersect(m_filterBBox);

    // prepare Filters
    m_filter = SVGFilter::create(m_itemBBox, m_filterBBox, m_effectBBoxMode, m_filterBBoxMode);

    FilterEffect* lastEffect = m_filterBuilder->lastEffect();
    if (lastEffect)
        lastEffect->calculateEffectRect(m_filter.get());

    // Draw the content of the current element and it's childs to a imageBuffer to get the SourceGraphic.
    // The size of the SourceGraphic is clipped to the size of the filterRegion.
    IntRect bufferRect = enclosingIntRect(clippedSourceRect);
    OwnPtr<ImageBuffer> sourceGraphic(ImageBuffer::create(bufferRect.size(), false));
    
    if (!sourceGraphic.get())
        return;

    GraphicsContext* sourceGraphicContext = sourceGraphic->context();
    sourceGraphicContext->translate(-m_itemBBox.x(), -m_itemBBox.y());
    sourceGraphicContext->clearRect(FloatRect(FloatPoint(), m_itemBBox.size()));
    m_sourceGraphicBuffer.set(sourceGraphic.release());
    m_savedContext = context;

    context = sourceGraphicContext;
}

void SVGResourceFilter::applyFilter(GraphicsContext*& context, const RenderObject*)
{
    if (!m_savedContext)
        return;

    context = m_savedContext;
    m_savedContext = 0;

    FilterEffect* lastEffect = m_filterBuilder->lastEffect();

    if (lastEffect && !m_filterBBox.isEmpty() && !lastEffect->subRegion().isEmpty()) {
        m_filter->setSourceImage(m_sourceGraphicBuffer.release());
        lastEffect->apply(m_filter.get());

        if (lastEffect->resultImage())
            context->drawImage(lastEffect->resultImage()->image(), lastEffect->subRegion());
    }

    m_sourceGraphicBuffer.clear();
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
