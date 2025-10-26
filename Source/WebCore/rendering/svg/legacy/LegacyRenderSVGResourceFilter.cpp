/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "LegacyRenderSVGResourceFilterInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "SVGFilterRenderer.h"
#include "SVGRenderingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceFilter);

LegacyRenderSVGResourceFilter::LegacyRenderSVGResourceFilter(SVGFilterElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(Type::LegacySVGResourceFilter, element, WTFMove(style))
{
}

bool LegacyRenderSVGResourceFilter::isIdentity() const
{
    return SVGFilterRenderer::isIdentity(protectedFilterElement());
}

FloatRect LegacyRenderSVGResourceFilter::drawingRegion(RenderElement& renderer) const
{
    return RenderFilterResource::sourceImageRect(renderer);
}

FloatRect LegacyRenderSVGResourceFilter::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation)
{
    Ref filterElement = this->filterElement();

    CheckedPtr renderer = dynamicDowncast<RenderElement>(object);
    if (!renderer)
        return SVGLengthContext::resolveRectangle(filterElement.get(), filterElement->filterUnits(), object.objectBoundingBox());

    RefPtr contextElement = dynamicDowncast<SVGElement>(renderer->element());

    return SVGLengthContext::resolveRectangle<SVGFilterElement>(contextElement.get(), filterElement.get(), filterElement->filterUnits(), object.objectBoundingBox());
}

std::optional<std::tuple<Ref<Filter>, FloatRect>> LegacyRenderSVGResourceFilter::createFilter(RenderElement& renderer, const FloatRect& targetBoundingBox, GraphicsContext& context)
{
    Ref filterElement = this->filterElement();
    RefPtr contextElement = dynamicDowncast<SVGElement>(renderer.element());

    auto filterRegion = SVGLengthContext::resolveRectangle(contextElement.get(), filterElement.get(), filterElement->filterUnits(), targetBoundingBox);
    if (filterRegion.isEmpty())
        return std::nullopt;

    // Determine absolute transformation matrix for filter.
    auto absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);
    if (!absoluteTransform.isInvertible())
        return std::nullopt;


    // Eliminate shear of the absolute transformation matrix, to be able to produce unsheared tile images for feTile.
    FloatSize filterScale(absoluteTransform.xScale(), absoluteTransform.yScale());

    auto sourceImageRect = renderer.strokeBoundingBox();
    sourceImageRect.intersect(filterRegion);

    // Determine scale factor for filter. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(sourceImageRect.size(), filterScale);

    auto preferredFilterModes = renderer.page().preferredFilterRenderingModes();

    auto filter = SVGFilterRenderer::create(contextElement.get(), filterElement, preferredFilterModes, filterScale, filterRegion, targetBoundingBox, context, RenderingResourceIdentifier::generate());
    if (!filter)
        return std::nullopt;

    filter->clampFilterRegionIfNeeded();

    return { { filter.releaseNonNull(), sourceImageRect } };
}

DestinationColorSpace LegacyRenderSVGResourceFilter::operatingColorSpace() const
{
#if USE(CAIRO)
    return DestinationColorSpace::SRGB();
#else
    return DestinationColorSpace::LinearSRGB();
#endif
}

auto LegacyRenderSVGResourceFilter::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode) -> OptionSet<ApplyResult>
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    if (RenderFilterResource::beginDrawSourceImage(renderer, renderer.objectBoundingBox(), std::nullopt, context) == SwitcherState::PaintingSource)
        return { ApplyResult::ResourceApplied };

    return { };
}

void LegacyRenderSVGResourceFilter::postApplyResource(RenderElement& renderer, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path*, const RenderElement*)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    auto state = RenderFilterResource::endDrawSourceImage(renderer, context);
    if (state == SwitcherState::SourcePainted)
        RenderFilterResource::applyFilter(renderer, *context);
}

void LegacyRenderSVGResourceFilter::markFilterForRepaint(FilterEffect& effect)
{
    RenderFilterResource::clearEffectResult(effect);
    for (auto client : RenderFilterResource::allClients())
        markClientForInvalidation(client, RepaintInvalidation);
}

void LegacyRenderSVGResourceFilter::markFilterForRebuild()
{
    removeAllClientsFromCache();
}

void LegacyRenderSVGResourceFilter::removeAllClientsFromCache()
{
    RenderFilterResource::removeAllClients();
}

void LegacyRenderSVGResourceFilter::removeClientFromCache(RenderElement& renderer)
{
    RenderFilterResource::removeClient(renderer);
}

} // namespace WebCore
