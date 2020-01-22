/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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
#include "RenderSVGResourceFilterPrimitive.h"
#include "RenderView.h"
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

std::unique_ptr<SVGFilterBuilder> RenderSVGResourceFilter::buildPrimitives(SVGFilter& filter) const
{
    static const unsigned maxCountChildNodes = 200;
    if (filterElement().countChildNodes() > maxCountChildNodes)
        return nullptr;

    FloatRect targetBoundingBox = filter.targetBoundingBox();

    // Add effects to the builder
    auto builder = makeUnique<SVGFilterBuilder>(SourceGraphic::create(filter));
    builder->setPrimitiveUnits(filterElement().primitiveUnits());
    builder->setTargetBoundingBox(targetBoundingBox);
    
    for (auto& element : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement())) {
        RefPtr<FilterEffect> effect = element.build(builder.get(), filter);
        if (!effect) {
            builder->clearEffects();
            return nullptr;
        }
        builder->appendEffectToEffectReferences(effect.copyRef(), element.renderer());
        element.setStandardAttributes(effect.get());
        effect->setEffectBoundaries(SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(&element, filterElement().primitiveUnits(), targetBoundingBox));
        if (element.renderer())
            effect->setOperatingColorSpace(element.renderer()->style().svgStyle().colorInterpolationFilters() == ColorInterpolation::LinearRGB ? ColorSpace::LinearRGB : ColorSpace::SRGB);
        builder->add(element.result(), WTFMove(effect));
    }
    return builder;
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

    auto filterData = makeUnique<FilterData>();
    FloatRect targetBoundingBox = renderer.objectBoundingBox();

    filterData->boundaries = SVGLengthContext::resolveRectangle<SVGFilterElement>(&filterElement(), filterElement().filterUnits(), targetBoundingBox);
    if (filterData->boundaries.isEmpty())
        return false;

    // Determine absolute transformation matrix for filter. 
    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);
    if (!absoluteTransform.isInvertible())
        return false;

    // Eliminate shear of the absolute transformation matrix, to be able to produce unsheared tile images for feTile.
    filterData->shearFreeAbsoluteTransform = AffineTransform(absoluteTransform.xScale(), 0, 0, absoluteTransform.yScale(), 0, 0);

    // Determine absolute boundaries of the filter and the drawing region.
    filterData->drawingRegion = renderer.strokeBoundingBox();
    filterData->drawingRegion.intersect(filterData->boundaries);
    FloatRect absoluteDrawingRegion = filterData->shearFreeAbsoluteTransform.mapRect(filterData->drawingRegion);

    // Create the SVGFilter object.
    bool primitiveBoundingBoxMode = filterElement().primitiveUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    filterData->filter = SVGFilter::create(filterData->shearFreeAbsoluteTransform, absoluteDrawingRegion, targetBoundingBox, filterData->boundaries, primitiveBoundingBoxMode);

    // Create all relevant filter primitives.
    filterData->builder = buildPrimitives(*filterData->filter);
    if (!filterData->builder)
        return false;

    // Determine scale factor for filter. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    FloatRect tempSourceRect = absoluteDrawingRegion;
    FloatSize scale(1, 1);
    ImageBuffer::sizeNeedsClamping(tempSourceRect.size(), scale);
    tempSourceRect.scale(scale.width(), scale.height());

    // Set the scale level in SVGFilter.
    filterData->filter->setFilterResolution(scale);

    static const unsigned maxTotalOfEffectInputs = 100;
    FilterEffect* lastEffect = filterData->builder->lastEffect();
    if (!lastEffect || lastEffect->totalNumberOfEffectInputs() > maxTotalOfEffectInputs)
        return false;

    LOG_WITH_STREAM(Filters, stream << "RenderSVGResourceFilter::applyResource\n" << *filterData->builder->lastEffect());

    lastEffect->determineFilterPrimitiveSubregion();
    FloatRect subRegion = lastEffect->maxEffectRect();
    // At least one FilterEffect has a too big image size,
    // recalculate the effect sizes with new scale factors.
    if (ImageBuffer::sizeNeedsClamping(subRegion.size(), scale)) {
        filterData->filter->setFilterResolution(scale);
        lastEffect->determineFilterPrimitiveSubregion();
    }

    // If the drawingRegion is empty, we have something like <g filter=".."/>.
    // Even if the target objectBoundingBox() is empty, we still have to draw the last effect result image in postApplyResource.
    if (filterData->drawingRegion.isEmpty()) {
        ASSERT(!m_rendererFilterDataMap.contains(&renderer));
        filterData->savedContext = context;
        m_rendererFilterDataMap.set(&renderer, WTFMove(filterData));
        return false;
    }

    // Change the coordinate transformation applied to the filtered element to reflect the resolution of the filter.
    AffineTransform effectiveTransform;
    effectiveTransform.scale(scale.width(), scale.height());
    effectiveTransform.multiply(filterData->shearFreeAbsoluteTransform);

    RenderingMode renderingMode = renderer.settings().acceleratedFiltersEnabled() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
    auto sourceGraphic = SVGRenderingContext::createImageBuffer(filterData->drawingRegion, effectiveTransform, ColorSpace::LinearRGB, renderingMode, context);
    if (!sourceGraphic) {
        ASSERT(!m_rendererFilterDataMap.contains(&renderer));
        filterData->savedContext = context;
        m_rendererFilterDataMap.set(&renderer, WTFMove(filterData));
        return false;
    }
    
    // Set the rendering mode from the page's settings.
    filterData->filter->setRenderingMode(renderingMode);

    GraphicsContext& sourceGraphicContext = sourceGraphic->context();
  
    filterData->sourceGraphicBuffer = WTFMove(sourceGraphic);
    filterData->savedContext = context;

    context = &sourceGraphicContext;

    ASSERT(!m_rendererFilterDataMap.contains(&renderer));
    m_rendererFilterDataMap.set(&renderer, WTFMove(filterData));

    return true;
}

void RenderSVGResourceFilter::postApplyResource(RenderElement& renderer, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path*, const RenderSVGShape*)
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

    FilterEffect* lastEffect = filterData.builder->lastEffect();

    if (lastEffect && !filterData.boundaries.isEmpty() && !lastEffect->filterPrimitiveSubregion().isEmpty()) {
        // This is the real filtering of the object. It just needs to be called on the
        // initial filtering process. We just take the stored filter result on a
        // second drawing.
        if (filterData.state != FilterData::Built)
            filterData.filter->setSourceImage(WTFMove(filterData.sourceGraphicBuffer));

        // Always true if filterData is just built (filterData->state == FilterData::Built).
        if (!lastEffect->hasResult()) {
            filterData.state = FilterData::Applying;
            lastEffect->apply();
            lastEffect->correctFilterResultIfNeeded();
            lastEffect->transformResultColorSpace(ColorSpace::SRGB);
        }
        filterData.state = FilterData::Built;

        ImageBuffer* resultImage = lastEffect->imageBufferResult();
        if (resultImage) {
            context->concatCTM(filterData.shearFreeAbsoluteTransform.inverse().valueOr(AffineTransform()));

            context->scale(FloatSize(1 / filterData.filter->filterResolution().width(), 1 / filterData.filter->filterResolution().height()));
            context->drawImageBuffer(*resultImage, lastEffect->absolutePaintRect());
            context->scale(filterData.filter->filterResolution());

            context->concatCTM(filterData.shearFreeAbsoluteTransform);
        }
    }
    filterData.sourceGraphicBuffer.reset();

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
        builder->clearResultsRecursive(effect);

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
