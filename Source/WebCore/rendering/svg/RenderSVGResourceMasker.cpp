/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "RenderSVGResourceMasker.h"

#include "Element.h"
#include "ElementIterator.h"
#include "FloatPoint.h"
#include "Image.h"
#include "IntRect.h"
#include "SVGRenderingContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceMasker);

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGMaskElement& element, RenderStyle&& style)
    : RenderSVGResourceContainer(element, WTFMove(style))
{
}

RenderSVGResourceMasker::~RenderSVGResourceMasker() = default;

void RenderSVGResourceMasker::removeAllClientsFromCache(bool markForInvalidation)
{
    m_maskContentBoundaries = FloatRect();
    m_masker.clear();

    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceMasker::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    m_masker.remove(&client);

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceMasker::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    bool missingMaskerData = !m_masker.contains(&renderer);
    if (missingMaskerData)
        m_masker.set(&renderer, std::make_unique<MaskerData>());

    MaskerData* maskerData = m_masker.get(&renderer);
    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);
    FloatRect repaintRect = renderer.repaintRectInLocalCoordinates();

    if (!maskerData->maskImage && !repaintRect.isEmpty()) {
        const SVGRenderStyle& svgStyle = style().svgStyle();
        ColorSpace colorSpace = svgStyle.colorInterpolation() == ColorInterpolation::LinearRGB ? ColorSpaceLinearRGB : ColorSpaceSRGB;
        // FIXME (149470): This image buffer should not be unconditionally unaccelerated. Making it match the context breaks alpha masking, though.
        maskerData->maskImage = SVGRenderingContext::createImageBuffer(repaintRect, absoluteTransform, colorSpace, Unaccelerated, context);
        if (!maskerData->maskImage)
            return false;

        if (!drawContentIntoMaskImage(maskerData, colorSpace, &renderer))
            maskerData->maskImage.reset();
    }

    if (!maskerData->maskImage)
        return false;

    SVGRenderingContext::clipToImageBuffer(*context, absoluteTransform, repaintRect, maskerData->maskImage, missingMaskerData);
    return true;
}

bool RenderSVGResourceMasker::drawContentIntoMaskImage(MaskerData* maskerData, ColorSpace colorSpace, RenderObject* object)
{
    GraphicsContext& maskImageContext = maskerData->maskImage->context();

    // Eventually adjust the mask image context according to the target objectBoundingBox.
    AffineTransform maskContentTransformation;
    if (maskElement().maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        FloatRect objectBoundingBox = object->objectBoundingBox();
        maskContentTransformation.translate(objectBoundingBox.location());
        maskContentTransformation.scale(objectBoundingBox.size());
        maskImageContext.concatCTM(maskContentTransformation);
    }

    // Draw the content into the ImageBuffer.
    for (auto& child : childrenOfType<SVGElement>(maskElement())) {
        auto renderer = child.renderer();
        if (!renderer)
            continue;
        if (renderer->needsLayout())
            return false;
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
            continue;
        SVGRenderingContext::renderSubtreeToImageBuffer(maskerData->maskImage.get(), *renderer, maskContentTransformation);
    }

#if !USE(CG)
    maskerData->maskImage->transformColorSpace(ColorSpaceSRGB, colorSpace);
#else
    UNUSED_PARAM(colorSpace);
#endif

    // Create the luminance mask.
    if (style().svgStyle().maskType() == MaskType::Luminance)
        maskerData->maskImage->convertToLuminanceMask();

    return true;
}

void RenderSVGResourceMasker::calculateMaskContentRepaintRect()
{
    for (Node* childNode = maskElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
             continue;
        m_maskContentBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(const RenderObject& object)
{
    FloatRect objectBoundingBox = object.objectBoundingBox();
    FloatRect maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(&maskElement(), maskElement().maskUnits(), objectBoundingBox);

    // Resource was not layouted yet. Give back clipping rect of the mask.
    if (selfNeedsLayout())
        return maskBoundaries;

    if (m_maskContentBoundaries.isEmpty())
        calculateMaskContentRepaintRect();

    FloatRect maskRect = m_maskContentBoundaries;
    if (maskElement().maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        maskRect = transform.mapRect(maskRect);
    }

    maskRect.intersect(maskBoundaries);
    return maskRect;
}

}
