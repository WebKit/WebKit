/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "RenderSVGResourceFilter.h"

#include "ElementChildIterator.h"
#include "FilterEffect.h"
#include "FloatPoint.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageData.h"
#include "IntRect.h"
#include "Logging.h"
#include "RenderSVGResourceFilterInlines.h"
#include "RenderSVGResourceFilterPrimitive.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include "SVGRenderingContext.h"
#include "Settings.h"
#include "SourceGraphic.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FilterData);
WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceFilter);

RenderSVGResourceFilter::RenderSVGResourceFilter(SVGFilterElement& element, RenderStyle&& style)
    : RenderSVGResourceContainer(element, WTFMove(style))
{
}

RenderSVGResourceFilter::~RenderSVGResourceFilter() = default;

void RenderSVGResourceFilter::removeAllClientsFromCache(bool markForInvalidation)
{
    LOG(Filters, "RenderSVGResourceFilter %p removeAllClientsFromCache", this);

    m_rendererFilterDataMap.clear();

    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceFilter::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    LOG(Filters, "RenderSVGResourceFilter %p removing client %p", this, &client);
    
    auto findResult = m_rendererFilterDataMap.find(&client);
    if (findResult != m_rendererFilterDataMap.end()) {
        FilterData& filterData = *findResult->value;
        if (filterData.savedContext)
            filterData.state = FilterData::MarkedForRemoval;
        else
            m_rendererFilterDataMap.remove(findResult);
    }

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceFilter::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    LOG(Filters, "RenderSVGResourceFilter %p applyResource renderer %p", this, &renderer);

    if (m_rendererFilterDataMap.contains(&renderer)) {
        FilterData* filterData = m_rendererFilterDataMap.get(&renderer);
        if (filterData->state == FilterData::PaintingSource || filterData->state == FilterData::Applying)
            filterData->state = FilterData::CycleDetected;
        return false; // Already built, or we're in a cycle, or we're marked for removal. Regardless, just do nothing more now.
    }

    auto addResult = m_rendererFilterDataMap.set(&renderer, makeUnique<FilterData>());
    auto filterData = addResult.iterator->value.get();
    
    FloatRect targetBoundingBox = renderer.objectBoundingBox();

    filterData->boundaries = SVGLengthContext::resolveRectangle<SVGFilterElement>(&filterElement(), filterElement().filterUnits(), targetBoundingBox);
    if (filterData->boundaries.isEmpty()) {
        m_rendererFilterDataMap.remove(&renderer);
        return false;
    }

    // Determine absolute transformation matrix for filter. 
    auto absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);
    if (!absoluteTransform.isInvertible()) {
        m_rendererFilterDataMap.remove(&renderer);
        return false;
    }

    // Eliminate shear of the absolute transformation matrix, to be able to produce unsheared tile images for feTile.
    FloatSize filterScale(absoluteTransform.xScale(), absoluteTransform.yScale());

    // Determine absolute boundaries of the filter and the drawing region.
    filterData->drawingRegion = renderer.strokeBoundingBox();
    filterData->drawingRegion.intersect(filterData->boundaries);

    // Determine scale factor for filter. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(filterData->drawingRegion.size(), filterScale);

    // Set the rendering mode from the page's settings.
    auto renderingMode = renderer.page().acceleratedFiltersEnabled() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

    // Create the SVGFilter object.
    filterData->builder = makeUnique<SVGFilterBuilder>();
    filterData->filter = SVGFilter::create(filterElement(), *filterData->builder, renderingMode, filterScale, Filter::ClipOperation::Intersect, filterData->boundaries, targetBoundingBox);
    if (!filterData->filter) {
        m_rendererFilterDataMap.remove(&renderer);
        return false;
    }

    if (filterData->filter->clampFilterRegionIfNeeded())
        filterScale = filterData->filter->filterScale();
    
    // If the drawingRegion is empty, we have something like <g filter=".."/>.
    // Even if the target objectBoundingBox() is empty, we still have to draw the last effect result image in postApplyResource.
    if (filterData->drawingRegion.isEmpty()) {
        ASSERT(m_rendererFilterDataMap.contains(&renderer));
        filterData->savedContext = context;
        return false;
    }

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
    auto colorSpace = DestinationColorSpace::LinearSRGB();
#else
    auto colorSpace = DestinationColorSpace::SRGB();
#endif

    auto sourceGraphic = context->createImageBuffer(filterData->drawingRegion, filterScale, colorSpace, filterData->filter->renderingMode());
    if (!sourceGraphic) {
        ASSERT(m_rendererFilterDataMap.contains(&renderer));
        filterData->savedContext = context;
        return false;
    }

    auto& sourceGraphicContext = sourceGraphic->context();

    filterData->sourceGraphicBuffer = WTFMove(sourceGraphic);
    filterData->savedContext = context;

    context = &sourceGraphicContext;

    ASSERT(m_rendererFilterDataMap.contains(&renderer));
    return true;
}

void RenderSVGResourceFilter::postApplyResource(RenderElement& renderer, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path*, const RenderElement*)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    auto findResult = m_rendererFilterDataMap.find(&renderer);
    if (findResult == m_rendererFilterDataMap.end())
        return;

    FilterData& filterData = *findResult->value;

    LOG_WITH_STREAM(Filters, stream << "\nRenderSVGResourceFilter " << this << " postApplyResource - renderer " << &renderer << " filter state " << filterData.state);

    switch (filterData.state) {
    case FilterData::MarkedForRemoval:
        m_rendererFilterDataMap.remove(findResult);
        return;

    case FilterData::CycleDetected:
    case FilterData::Applying:
        // We have a cycle if we are already applying the data.
        // This can occur due to FeImage referencing a source that makes use of the FEImage itself.
        // This is the first place we've hit the cycle, so set the state back to PaintingSource so the return stack
        // will continue correctly.
        filterData.state = FilterData::PaintingSource;
        return;

    case FilterData::PaintingSource:
        if (!filterData.savedContext) {
            removeClientFromCache(renderer);
            return;
        }

        context = filterData.savedContext;
        filterData.savedContext = nullptr;
        break;

    case FilterData::Built:
        break;
    }

    if (!filterData.boundaries.isEmpty()) {
        filterData.state = FilterData::Built;
        context->drawFilteredImageBuffer(filterData.sourceGraphicBuffer.get(), filterData.drawingRegion, *filterData.filter, filterData.results);
    }

    LOG_WITH_STREAM(Filters, stream << "RenderSVGResourceFilter " << this << " postApplyResource done\n");
}

FloatRect RenderSVGResourceFilter::resourceBoundingBox(const RenderObject& object)
{
    return SVGLengthContext::resolveRectangle<SVGFilterElement>(&filterElement(), filterElement().filterUnits(), object.objectBoundingBox());
}

void RenderSVGResourceFilter::primitiveAttributeChanged(RenderObject* object, const QualifiedName& attribute)
{
    SVGFilterPrimitiveStandardAttributes* primitve = static_cast<SVGFilterPrimitiveStandardAttributes*>(object->node());

    LOG(Filters, "RenderSVGResourceFilter %p primitiveAttributeChanged renderer %p", this, object);

    for (const auto& objectFilterDataPair : m_rendererFilterDataMap) {
        const auto& filterData = objectFilterDataPair.value;
        if (filterData->state != FilterData::Built)
            continue;

        SVGFilterBuilder* builder = filterData->builder.get();
        FilterEffect* effect = builder->effectByRenderer(object);
        if (!effect)
            continue;
        // Since all effects shares the same attribute value, all
        // or none of them will be changed.
        if (!primitve->setFilterEffectAttribute(effect, attribute))
            return;
        filterData->results.clearEffectResult(*effect);

        // Repaint the image on the screen.
        markClientForInvalidation(*objectFilterDataPair.key, RepaintInvalidation);
    }
    markAllClientLayersForInvalidation();
}

FloatRect RenderSVGResourceFilter::drawingRegion(RenderObject* object) const
{
    FilterData* filterData = m_rendererFilterDataMap.get(object);
    return filterData ? filterData->drawingRegion : FloatRect();
}

TextStream& operator<<(TextStream& ts, FilterData::FilterDataState state)
{
    switch (state) {
    case FilterData::PaintingSource:
        ts << "painting source";
        break;
    case FilterData::Applying:
        ts << "applying";
        break;
    case FilterData::Built:
        ts << "built";
        break;
    case FilterData::CycleDetected:
        ts << "cycle detected";
        break;
    case FilterData::MarkedForRemoval:
        ts << "marked for removal";
        break;
    }
    return ts;
}

} // namespace WebCore
