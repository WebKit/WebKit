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
#include "SVGFilterElement.h"
#include "SVGRenderSupport.h"
#include "SVGRenderTreeAsText.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

static const float kMaxFilterSize = 5000.0f;

using std::min;

namespace WebCore {

SVGResourceFilter::SVGResourceFilter(const SVGFilterElement* ownerElement)
    : SVGResource()
    , m_ownerElement(ownerElement)
    , m_filterBBoxMode(false)
    , m_effectBBoxMode(false)
    , m_filterRes(false)
    , m_scaleX(1.f)
    , m_scaleY(1.f)
    , m_savedContext(0)
    , m_sourceGraphicBuffer(0)
{
    m_filterBuilder.set(new SVGFilterBuilder());    
}

SVGResourceFilter::~SVGResourceFilter()
{
}

FloatRect SVGResourceFilter::filterBoundingBox(const FloatRect& obb) const
{
    return m_ownerElement->filterBoundingBox(obb);
}

static inline bool shouldProcessFilter(SVGResourceFilter* filter, const FloatRect& filterRect)
{
    return (!filter->scaleX() || !filter->scaleY() || !filterRect.width() || !filterRect.height());
}

void SVGResourceFilter::addFilterEffect(SVGFilterPrimitiveStandardAttributes* effectAttributes, PassRefPtr<FilterEffect> effect)
{
    effectAttributes->setStandardAttributes(this, effect.get());
    builder()->add(effectAttributes->result(), effect);
}

bool SVGResourceFilter::fitsInMaximumImageSize(const FloatSize& size)
{
    bool matchesFilterSize = true;
    if (size.width() > kMaxFilterSize) {
        m_scaleX *= kMaxFilterSize / size.width();
        matchesFilterSize = false;
    }
    if (size.height() > kMaxFilterSize) {
        m_scaleY *= kMaxFilterSize / size.height();
        matchesFilterSize = false;
    }

    return matchesFilterSize;
}

bool SVGResourceFilter::prepareFilter(GraphicsContext*& context, const RenderObject* object)
{
    m_ownerElement->buildFilter(object->objectBoundingBox());
    const SVGRenderBase* renderer = object->toSVGRenderBase();
    if (!renderer)
        return false;

    FloatRect paintRect = renderer->strokeBoundingBox();
    paintRect.unite(renderer->markerBoundingBox());

    if (shouldProcessFilter(this, m_filterBBox))
        return false;

    // clip sourceImage to filterRegion
    FloatRect clippedSourceRect = paintRect;
    clippedSourceRect.intersect(m_filterBBox);

    // scale filter size to filterRes
    FloatRect tempSourceRect = clippedSourceRect;
    if (m_filterRes) {
        m_scaleX = m_filterResSize.width() / m_filterBBox.width();
        m_scaleY = m_filterResSize.height() / m_filterBBox.height();
    }

    // scale to big sourceImage size to kMaxFilterSize
    tempSourceRect.scale(m_scaleX, m_scaleY);
    fitsInMaximumImageSize(tempSourceRect.size());

    // prepare Filters
    m_filter = SVGFilter::create(paintRect, m_filterBBox, m_effectBBoxMode);
    m_filter->setFilterResolution(FloatSize(m_scaleX, m_scaleY));

    FilterEffect* lastEffect = m_filterBuilder->lastEffect();
    if (lastEffect) {
        lastEffect->calculateEffectRect(m_filter.get());
        // at least one FilterEffect has a too big image size,
        // recalculate the effect sizes with new scale factors
        if (!fitsInMaximumImageSize(m_filter->maxImageSize())) {
            m_filter->setFilterResolution(FloatSize(m_scaleX, m_scaleY));
            lastEffect->calculateEffectRect(m_filter.get());
        }
    } else
        return false;

    clippedSourceRect.scale(m_scaleX, m_scaleY);

    // Draw the content of the current element and it's childs to a imageBuffer to get the SourceGraphic.
    // The size of the SourceGraphic is clipped to the size of the filterRegion.
    IntRect bufferRect = enclosingIntRect(clippedSourceRect);
    OwnPtr<ImageBuffer> sourceGraphic(ImageBuffer::create(bufferRect.size(), LinearRGB));
    
    if (!sourceGraphic.get())
        return false;

    GraphicsContext* sourceGraphicContext = sourceGraphic->context();
    sourceGraphicContext->translate(-clippedSourceRect.x(), -clippedSourceRect.y());
    sourceGraphicContext->scale(FloatSize(m_scaleX, m_scaleY));
    sourceGraphicContext->clearRect(FloatRect(FloatPoint(), paintRect.size()));
    m_sourceGraphicBuffer.set(sourceGraphic.release());
    m_savedContext = context;

    context = sourceGraphicContext;
    return true;
}

void SVGResourceFilter::applyFilter(GraphicsContext*& context, const RenderObject* object)
{
    if (!m_savedContext)
        return;

    context = m_savedContext;
    m_savedContext = 0;
#if !PLATFORM(CG)
    m_sourceGraphicBuffer->transformColorSpace(DeviceRGB, LinearRGB);
#endif

    FilterEffect* lastEffect = m_filterBuilder->lastEffect();

    if (lastEffect && !m_filterBBox.isEmpty() && !lastEffect->subRegion().isEmpty()) {
        m_filter->setSourceImage(m_sourceGraphicBuffer.release());
        lastEffect->apply(m_filter.get());

        ImageBuffer* resultImage = lastEffect->resultImage();
        if (resultImage) {
#if !PLATFORM(CG)
            resultImage->transformColorSpace(LinearRGB, DeviceRGB);
#endif
            ColorSpace colorSpace = DeviceColorSpace;
            if (object)
                colorSpace = object->style()->colorSpace();
            context->drawImage(resultImage->image(), colorSpace, lastEffect->subRegion());
        }
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

SVGResourceFilter* getFilterById(Document* document, const AtomicString& id, const RenderObject* object)
{
    SVGResource* resource = getResourceById(document, id, object);
    if (resource && resource->isFilter())
        return static_cast<SVGResourceFilter*>(resource);

    return 0;
}


} // namespace WebCore

#endif // ENABLE(SVG)
