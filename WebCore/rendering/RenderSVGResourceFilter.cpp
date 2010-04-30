/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *               2004, 2005 Rob Buis <buis@kde.org>
 *               2005 Eric Seidel <eric@webkit.org>
 *               2009 Dirk Schulze <krit@webkit.org>
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
 *
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "RenderSVGResourceFilter.h"

#include "AffineTransform.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "IntRect.h"
#include "RenderSVGResource.h"
#include "SVGElement.h"
#include "SVGFilter.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGStyledElement.h"
#include "SVGUnitTypes.h"
#include <wtf/Vector.h>
#include <wtf/UnusedParam.h>

static const float kMaxFilterSize = 5000.0f;

using namespace std;

namespace WebCore {

RenderSVGResourceType RenderSVGResourceFilter::s_resourceType = FilterResourceType;

RenderSVGResourceFilter::RenderSVGResourceFilter(SVGFilterElement* node)
    : RenderSVGResourceContainer(node)
    , m_savedContext(0)
    , m_sourceGraphicBuffer(0)
{
}

RenderSVGResourceFilter::~RenderSVGResourceFilter()
{
    deleteAllValues(m_filter);
    m_filter.clear();
}

void RenderSVGResourceFilter::invalidateClients()
{
    HashMap<RenderObject*, FilterData*>::const_iterator end = m_filter.end();
    for (HashMap<RenderObject*, FilterData*>::const_iterator it = m_filter.begin(); it != end; ++it) {
        RenderObject* renderer = it->first;
        renderer->setNeedsBoundariesUpdate();
        renderer->setNeedsLayout(true);
    }
    deleteAllValues(m_filter);
    m_filter.clear();
}

void RenderSVGResourceFilter::invalidateClient(RenderObject* object)
{
    ASSERT(object);

    // FIXME: The HashMap should always contain the object on calling invalidateClient. A race condition
    // during the parsing can causes a call of invalidateClient right before the call of applyResource.
    // We return earlier for the moment. This bug should be fixed in:
    // https://bugs.webkit.org/show_bug.cgi?id=35181
    if (!m_filter.contains(object))
        return;

    delete m_filter.take(object);
    markForLayoutAndResourceInvalidation(object);
}

PassOwnPtr<SVGFilterBuilder> RenderSVGResourceFilter::buildPrimitives()
{
    SVGFilterElement* filterElement = static_cast<SVGFilterElement*>(node());
    bool primitiveBoundingBoxMode = filterElement->primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    // Add effects to the builder
    OwnPtr<SVGFilterBuilder> builder(new SVGFilterBuilder);
    builder->clearEffects();
    for (Node* node = filterElement->firstChild(); node; node = node->nextSibling()) {
        if (!node->isSVGElement())
            continue;

        SVGElement* element = static_cast<SVGElement*>(node);
        if (!element->isFilterEffect())
            continue;

        SVGFilterPrimitiveStandardAttributes* effectElement = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);
        RefPtr<FilterEffect> effect = effectElement->build(builder.get());
        if (!effect) {
            builder->clearEffects();
            return 0;
        }
        effectElement->setStandardAttributes(primitiveBoundingBoxMode, effect.get());
        builder->add(effectElement->result(), effect);
    }
    return builder.release();
}

bool RenderSVGResourceFilter::fitsInMaximumImageSize(const FloatSize& size, FloatSize& scale)
{
    bool matchesFilterSize = true;
    if (size.width() > kMaxFilterSize) {
        scale.setWidth(scale.width() * kMaxFilterSize / size.width());
        matchesFilterSize = false;
    }
    if (size.height() > kMaxFilterSize) {
        scale.setHeight(scale.height() * kMaxFilterSize / size.height());
        matchesFilterSize = false;
    }

    return matchesFilterSize;
}

bool RenderSVGResourceFilter::applyResource(RenderObject* object, RenderStyle*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
#ifndef NDEBUG
    ASSERT(resourceMode == ApplyToDefaultMode);
#else
    UNUSED_PARAM(resourceMode);
#endif

    // Returning false here, to avoid drawings onto the context. We just want to
    // draw the stored filter output, not the unfiltered object as well.
    if (m_filter.contains(object)) {
        FilterData* filterData = m_filter.get(object);
        if (filterData->builded)
            return false;

        delete m_filter.take(object); // Oops, have to rebuild, go through normal code path
    }

    OwnPtr<FilterData> filterData(new FilterData);
    filterData->builder = buildPrimitives();
    if (!filterData->builder)
        return false;

    FloatRect paintRect = object->strokeBoundingBox();

    // Calculate the scale factor for the use of filterRes.
    // Also see http://www.w3.org/TR/SVG/filters.html#FilterEffectsRegion
    SVGFilterElement* filterElement = static_cast<SVGFilterElement*>(node());
    filterData->boundaries = filterElement->filterBoundingBox(object->objectBoundingBox());
    if (filterData->boundaries.isEmpty())
        return false;

    FloatSize scale(1.0f, 1.0f);
    if (filterElement->hasAttribute(SVGNames::filterResAttr)) {
        scale.setWidth(filterElement->filterResX() / filterData->boundaries.width());
        scale.setHeight(filterElement->filterResY() / filterData->boundaries.height());
    }

    if (scale.isEmpty())
        return false;

    // clip sourceImage to filterRegion
    FloatRect clippedSourceRect = paintRect;
    clippedSourceRect.intersect(filterData->boundaries);

    // scale filter size to filterRes
    FloatRect tempSourceRect = clippedSourceRect;

    // scale to big sourceImage size to kMaxFilterSize
    tempSourceRect.scale(scale.width(), scale.height());
    fitsInMaximumImageSize(tempSourceRect.size(), scale);

    // prepare Filters
    bool primitiveBoundingBoxMode = filterElement->primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    filterData->filter = SVGFilter::create(paintRect, filterData->boundaries, primitiveBoundingBoxMode);
    filterData->filter->setFilterResolution(scale);

    FilterEffect* lastEffect = filterData->builder->lastEffect();
    if (!lastEffect)
        return false;
    
    lastEffect->calculateEffectRect(filterData->filter.get());
    // At least one FilterEffect has a too big image size,
    // recalculate the effect sizes with new scale factors.
    if (!fitsInMaximumImageSize(filterData->filter->maxImageSize(), scale)) {
        filterData->filter->setFilterResolution(scale);
        lastEffect->calculateEffectRect(filterData->filter.get());
    }

    clippedSourceRect.scale(scale.width(), scale.height());

    // Draw the content of the current element and it's childs to a imageBuffer to get the SourceGraphic.
    // The size of the SourceGraphic is clipped to the size of the filterRegion.
    IntRect bufferRect = enclosingIntRect(clippedSourceRect);
    OwnPtr<ImageBuffer> sourceGraphic(ImageBuffer::create(bufferRect.size(), LinearRGB));
    
    if (!sourceGraphic.get())
        return false;

    GraphicsContext* sourceGraphicContext = sourceGraphic->context();
    sourceGraphicContext->translate(-clippedSourceRect.x(), -clippedSourceRect.y());
    sourceGraphicContext->scale(scale);
    sourceGraphicContext->clearRect(FloatRect(FloatPoint(), paintRect.size()));
    m_sourceGraphicBuffer.set(sourceGraphic.release());
    m_savedContext = context;

    context = sourceGraphicContext;
    m_filter.set(object, filterData.release());

    return true;
}

void RenderSVGResourceFilter::postApplyResource(RenderObject* object, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
#ifndef NDEBUG
    ASSERT(resourceMode == ApplyToDefaultMode);
#else
    UNUSED_PARAM(resourceMode);
#endif

    if (!m_filter.contains(object))
        return;

    FilterData* filterData = m_filter.get(object);
    if (!filterData->builded) {
        if (!m_savedContext) {
            invalidateClient(object);
            return;
        }

        context = m_savedContext;
        m_savedContext = 0;
#if !PLATFORM(CG)
        m_sourceGraphicBuffer->transformColorSpace(DeviceRGB, LinearRGB);
#endif
    }

    FilterEffect* lastEffect = filterData->builder->lastEffect();
    
    if (lastEffect && !filterData->boundaries.isEmpty() && !lastEffect->subRegion().isEmpty()) {
        // This is the real filtering of the object. It just needs to be called on the
        // initial filtering process. We just take the stored filter result on a
        // second drawing.
        if (!filterData->builded) {
            filterData->filter->setSourceImage(m_sourceGraphicBuffer.release());
            lastEffect->apply(filterData->filter.get());
            filterData->builded = true;
        }

        ImageBuffer* resultImage = lastEffect->resultImage();
        if (resultImage) {
#if !PLATFORM(CG)
            resultImage->transformColorSpace(LinearRGB, DeviceRGB);
#endif
            context->drawImage(resultImage->image(), object->style()->colorSpace(), lastEffect->subRegion());
        }
    }

    m_sourceGraphicBuffer.clear();
}

FloatRect RenderSVGResourceFilter::resourceBoundingBox(const FloatRect& objectBoundingBox) const
{
    if (SVGFilterElement* element = static_cast<SVGFilterElement*>(node()))
        return element->filterBoundingBox(objectBoundingBox);

    return FloatRect();
}

}
#endif
