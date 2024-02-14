/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2021, 2022, 2023 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "Element.h"
#include "ElementIterator.h"
#include "FloatPoint.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "RenderLayer.h"
#include "RenderLayerInlines.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceMaskerInlines.h"
#include "SVGContainerLayout.h"
#include "SVGGraphicsElement.h"
#include "SVGLengthContext.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceMasker);

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGMaskElement& element, RenderStyle&& style)
    : RenderSVGResourceContainer(Type::SVGResourceMasker, element, WTFMove(style))
{
}

RenderSVGResourceMasker::~RenderSVGResourceMasker() = default;

static RefPtr<ImageBuffer> createImageBuffer(const FloatRect& targetRect, const AffineTransform& absoluteTransform, const DestinationColorSpace& colorSpace, const GraphicsContext* context)
{
    IntRect paintRect = enclosingIntRect(absoluteTransform.mapRect(targetRect));
    // Don't create empty ImageBuffers.
    if (paintRect.isEmpty())
        return nullptr;

    FloatSize scale;
    FloatSize clampedSize = ImageBuffer::clampedSize(paintRect.size(), scale);

    UNUSED_PARAM(context);
    auto imageBuffer = ImageBuffer::create(clampedSize, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;

    AffineTransform transform;
    transform.scale(scale).translate(-paintRect.location()).multiply(absoluteTransform);

    auto& imageContext = imageBuffer->context();
    imageContext.concatCTM(transform);

    return imageBuffer;
}

void RenderSVGResourceMasker::applyMask(PaintInfo& paintInfo, const RenderLayerModelObject& targetRenderer, const LayoutPoint& adjustedPaintOffset)
{
    ASSERT(hasLayer());
    ASSERT(layer()->isSelfPaintingLayer());
    ASSERT(targetRenderer.hasLayer());

    if (SVGHitTestCycleDetectionScope::isVisiting(*this))
        return;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto objectBoundingBox = targetRenderer.objectBoundingBox();
    auto boundingBoxTopLeftCorner = flooredLayoutPoint(objectBoundingBox.minXMinYCorner());
    auto coordinateSystemOriginTranslation = adjustedPaintOffset - boundingBoxTopLeftCorner;
    if (!coordinateSystemOriginTranslation.isZero())
        context.translate(coordinateSystemOriginTranslation);

    AffineTransform contentTransform;
    auto& maskElement = this->maskElement();
    if (maskElement.maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        contentTransform.scale(objectBoundingBox.width(), objectBoundingBox.height());
    }

    auto repaintBoundingBox = targetRenderer.repaintRectInLocalCoordinates();
    auto absoluteTransform = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);

    auto maskColorSpace = DestinationColorSpace::SRGB();
    auto drawColorSpace = DestinationColorSpace::SRGB();

    const auto& svgStyle = style().svgStyle();
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
    if (svgStyle.colorInterpolation() == ColorInterpolation::LinearRGB) {
#if USE(CG)
        maskColorSpace = DestinationColorSpace::LinearSRGB();
#endif
        drawColorSpace = DestinationColorSpace::LinearSRGB();
    }
#endif

    // FIXME: try to use GraphicsContext::createScaledImageBuffer instead.
    auto maskImage = createImageBuffer(repaintBoundingBox, absoluteTransform, maskColorSpace, &context);
    if (!maskImage)
        return;

    context.setCompositeOperation(CompositeOperator::DestinationIn);
    context.beginTransparencyLayer(1);

    auto& maskImageContext = maskImage->context();
    layer()->paintSVGResourceLayer(maskImageContext, contentTransform);

#if !USE(CG)
    maskImage->transformToColorSpace(drawColorSpace);
#else
    UNUSED_PARAM(drawColorSpace);
#endif

    if (svgStyle.maskType() == MaskType::Luminance)
        maskImage->convertToLuminanceMask();

    context.setCompositeOperation(CompositeOperator::SourceOver);

    // The mask image has been created in the absolute coordinate space, as the image should not be scaled.
    // So the actual masking process has to be done in the absolute coordinate space as well.
    FloatRect absoluteTargetRect = enclosingIntRect(absoluteTransform.mapRect(repaintBoundingBox));
    context.concatCTM(absoluteTransform.inverse().value_or(AffineTransform()));
    context.drawConsumingImageBuffer(WTFMove(maskImage), absoluteTargetRect);
    context.endTransparencyLayer();
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation repaintRectCalculation)
{
    auto targetBoundingBox = object.objectBoundingBox();

    if (SVGHitTestCycleDetectionScope::isVisiting(*this))
        return targetBoundingBox;

    SVGHitTestCycleDetectionScope queryScope(*this);

    auto& maskElement = this->maskElement();

    auto maskRect = maskElement.calculateMaskContentRepaintRect(repaintRectCalculation);
    if (maskElement.maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform contentTransform;
        contentTransform.translate(targetBoundingBox.location());
        contentTransform.scale(targetBoundingBox.size());
        maskRect = contentTransform.mapRect(maskRect);
    }

    auto maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(&maskElement, maskElement.maskUnits(), targetBoundingBox);
    maskRect.intersect(maskBoundaries);
    if (maskRect.isEmpty())
        return targetBoundingBox;
    return maskRect;
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
