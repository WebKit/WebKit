/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "LegacyRenderSVGResourceFilter.h"

#include "FilterEffect.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LegacyRenderSVGResourceFilterInlines.h"
#include "Logging.h"
#include "SVGElementTypeHelpers.h"
#include "SVGRenderStyle.h"
#include "SVGRenderingContext.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FilterData);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceFilter);

LegacyRenderSVGResourceFilter::LegacyRenderSVGResourceFilter(SVGFilterElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(Type::LegacySVGResourceFilter, element, WTFMove(style))
{
}

LegacyRenderSVGResourceFilter::~LegacyRenderSVGResourceFilter() = default;

bool LegacyRenderSVGResourceFilter::isIdentity() const
{
    return SVGFilter::isIdentity(protectedFilterElement());
}

void LegacyRenderSVGResourceFilter::removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    LOG(Filters, "LegacyRenderSVGResourceFilter %p removeAllClientsFromCacheIfNeeded", this);

    m_rendererFilterDataMap.clear();

    markAllClientsForInvalidationIfNeeded(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation, visitedRenderers);
}

void LegacyRenderSVGResourceFilter::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    LOG(Filters, "LegacyRenderSVGResourceFilter %p removing client %p", this, &client);
    
    auto findResult = m_rendererFilterDataMap.find(client);
    if (findResult != m_rendererFilterDataMap.end()) {
        FilterData& filterData = *findResult->value;
        if (filterData.savedContext)
            filterData.state = FilterData::MarkedForRemoval;
        else
            m_rendererFilterDataMap.remove(findResult);
    }

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

auto LegacyRenderSVGResourceFilter::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode) -> OptionSet<ApplyResult>
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    LOG(Filters, "LegacyRenderSVGResourceFilter %p applyResource renderer %p", this, &renderer);

    if (m_rendererFilterDataMap.contains(renderer)) {
        FilterData* filterData = m_rendererFilterDataMap.get(renderer);
        if (filterData->state == FilterData::PaintingSource || filterData->state == FilterData::Applying) {
            filterData->state = FilterData::CycleDetected;
            return { }; // Already built, or we're in a cycle, or we're marked for removal. Regardless, just do nothing more now.
        }
        
        ASSERT(filterData->targetSwitcher);
        if (filterData->targetSwitcher->hasSourceImage())
            return { };

        filterData->targetSwitcher->beginDrawSourceImage(*context);
        return { ApplyResult::ResourceApplied };
    }

    auto addResult = m_rendererFilterDataMap.set(renderer, makeUnique<FilterData>());
    auto filterData = addResult.iterator->value.get();

    auto targetBoundingBox = renderer.objectBoundingBox();
    Ref filterElement = this->filterElement();
    auto filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement.ptr(), filterElement->filterUnits(), targetBoundingBox);
    if (filterRegion.isEmpty()) {
        m_rendererFilterDataMap.remove(renderer);
        return { };
    }

    // Determine absolute transformation matrix for filter.
    auto absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);
    if (!absoluteTransform.isInvertible()) {
        m_rendererFilterDataMap.remove(renderer);
        return { };
    }

    // Eliminate shear of the absolute transformation matrix, to be able to produce unsheared tile images for feTile.
    FloatSize filterScale(absoluteTransform.xScale(), absoluteTransform.yScale());

    // Determine absolute boundaries of the filter and the drawing region.
    filterData->sourceImageRect = renderer.strokeBoundingBox();
    filterData->sourceImageRect.intersect(filterRegion);

    // Determine scale factor for filter. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(filterData->sourceImageRect.size(), filterScale);

    auto preferredFilterModes = renderer.page().preferredFilterRenderingModes();

    // Create the SVGFilter object.
    filterData->filter = SVGFilter::create(filterElement, preferredFilterModes, filterScale, filterRegion, targetBoundingBox, *context, RenderingResourceIdentifier::generate());
    if (!filterData->filter) {
        m_rendererFilterDataMap.remove(renderer);
        return { };
    }

    filterData->filter->clampFilterRegionIfNeeded();

#if USE(CAIRO)
    auto colorSpace = DestinationColorSpace::SRGB();
#else
    auto colorSpace = DestinationColorSpace::LinearSRGB();
#endif

    auto& results = filterData->filter->ensureResults([&]() {
        return makeUnique<FilterResults>();
    });

    filterData->targetSwitcher = GraphicsContextSwitcher::create(*context, filterData->sourceImageRect, colorSpace, filterData->filter, &results);
    if (!filterData->targetSwitcher) {
        m_rendererFilterDataMap.remove(renderer);
        return { };
    }
    
    // If the sourceImageRect is empty, we have something like <g filter=".."/>.
    // Even if the target objectBoundingBox() is empty, we still have to draw the last effect result image in postApplyResource.
    if (filterData->sourceImageRect.isEmpty()) {
        ASSERT(m_rendererFilterDataMap.contains(renderer));
        filterData->savedContext = context;
        return { };
    }

    filterData->targetSwitcher->beginDrawSourceImage(*context);

    filterData->savedContext = context;
    context = filterData->targetSwitcher->drawingContext(*context);

    ASSERT(m_rendererFilterDataMap.contains(renderer));
    return { ApplyResult::ResourceApplied };
}

void LegacyRenderSVGResourceFilter::postApplyResource(RenderElement& renderer, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path*, const RenderElement*)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    auto findResult = m_rendererFilterDataMap.find(renderer);
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

    if (filterData.targetSwitcher) {
        filterData.state = FilterData::Built;
        filterData.targetSwitcher->endDrawSourceImage(*context, DestinationColorSpace::LinearSRGB());
    }

    LOG_WITH_STREAM(Filters, stream << "LegacyRenderSVGResourceFilter " << this << " postApplyResource done\n");
}

FloatRect LegacyRenderSVGResourceFilter::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation)
{
    Ref filterElement = this->filterElement();
    return SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement.ptr(), filterElement->filterUnits(), object.objectBoundingBox());
}

void LegacyRenderSVGResourceFilter::markFilterForRepaint(FilterEffect& effect)
{
    LOG(Filters, "LegacyRenderSVGResourceFilter %p markFilterForRepaint effect %p", this, &effect);

    for (const auto& objectFilterDataPair : m_rendererFilterDataMap) {
        const auto& filterData = objectFilterDataPair.value;
        if (filterData->state != FilterData::Built)
            continue;

        // Repaint the image on the screen.
        markClientForInvalidation(objectFilterDataPair.key, RepaintInvalidation);

        filterData->filter->clearEffectResult(effect);
    }
}

void LegacyRenderSVGResourceFilter::markFilterForRebuild()
{
    LOG(Filters, "LegacyRenderSVGResourceFilter %p markFilterForRebuild", this);

    removeAllClientsFromCache();
}

FloatRect LegacyRenderSVGResourceFilter::drawingRegion(RenderObject& object) const
{
    FilterData* filterData = m_rendererFilterDataMap.get(object);
    return filterData ? filterData->sourceImageRect : FloatRect();
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
