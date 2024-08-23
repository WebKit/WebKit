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
#include "SVGElementTypeHelpers.h"
#include "SVGGraphicsElement.h"
#include "SVGLengthContext.h"
#include "SVGRenderStyle.h"
#include "SVGVisitedRendererTracking.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGResourceMasker);

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
    auto imageBuffer = ImageBuffer::create(clampedSize, RenderingPurpose::Unspecified, 1, colorSpace, ImageBufferPixelFormat::BGRA8);
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

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto objectBoundingBox = targetRenderer.objectBoundingBox();
    auto boundingBoxTopLeftCorner = flooredLayoutPoint(objectBoundingBox.minXMinYCorner());
    auto coordinateSystemOriginTranslation = adjustedPaintOffset - boundingBoxTopLeftCorner;
    if (!coordinateSystemOriginTranslation.isZero())
        context.translate(coordinateSystemOriginTranslation);

    // FIXME: This needs to be bounding box and should not use repaint rect.
    // https://bugs.webkit.org/show_bug.cgi?id=278551
    auto repaintBoundingBox = targetRenderer.repaintRectInLocalCoordinates(RepaintRectCalculation::Accurate);
    auto absoluteTransform = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);

    auto maskColorSpace = DestinationColorSpace::SRGB();
    auto drawColorSpace = DestinationColorSpace::SRGB();

    Ref svgStyle = style().svgStyle();
    if (svgStyle->colorInterpolation() == ColorInterpolation::LinearRGB) {
#if USE(CG) || USE(SKIA)
        maskColorSpace = DestinationColorSpace::LinearSRGB();
#endif
        drawColorSpace = DestinationColorSpace::LinearSRGB();
    }

    RefPtr<ImageBuffer> maskImage = m_masker.get(targetRenderer);
    bool missingMaskerData = !maskImage;
    if (missingMaskerData) {
        // FIXME: try to use GraphicsContext::createScaledImageBuffer instead.
        maskImage = createImageBuffer(repaintBoundingBox, absoluteTransform, maskColorSpace, &context);
        if (!maskImage)
            return;
    }

    context.setCompositeOperation(CompositeOperator::DestinationIn);
    context.beginTransparencyLayer(1);

    if (missingMaskerData) {
        drawContentIntoContext(maskImage->context(), objectBoundingBox);

#if !USE(CG) && !USE(SKIA)
        maskImage->transformToColorSpace(drawColorSpace);
#else
        UNUSED_PARAM(drawColorSpace);
#endif

        if (svgStyle->maskType() == MaskType::Luminance)
            maskImage->convertToLuminanceMask();
        m_masker.set(targetRenderer, maskImage);
    }
    context.setCompositeOperation(CompositeOperator::SourceOver);

    // The mask image has been created in the absolute coordinate space, as the image should not be scaled.
    // So the actual masking process has to be done in the absolute coordinate space as well.
    FloatRect absoluteTargetRect = enclosingIntRect(absoluteTransform.mapRect(repaintBoundingBox));
    context.concatCTM(absoluteTransform.inverse().value_or(AffineTransform()));
    context.drawImageBuffer(*maskImage, absoluteTargetRect);
    context.endTransparencyLayer();
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation repaintRectCalculation)
{
    auto targetBoundingBox = object.objectBoundingBox();
    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return targetBoundingBox;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    Ref maskElement = this->maskElement();
    auto maskRect = maskElement->calculateMaskContentRepaintRect(repaintRectCalculation);
    if (maskElement->maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform contentTransform;
        contentTransform.translate(targetBoundingBox.location());
        contentTransform.scale(targetBoundingBox.size());
        maskRect = contentTransform.mapRect(maskRect);
    }

    auto maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(maskElement.ptr(), maskElement->maskUnits(), targetBoundingBox);
    maskRect.intersect(maskBoundaries);
    if (maskRect.isEmpty())
        return targetBoundingBox;
    return maskRect;
}

void RenderSVGResourceMasker::removeReferencingCSSClient(const RenderElement& client)
{
    if (auto renderer = dynamicDowncast<RenderLayerModelObject>(client))
        m_masker.remove(renderer);
}

bool RenderSVGResourceMasker::drawContentIntoContext(GraphicsContext& context, const FloatRect& objectBoundingBox)
{
    // Eventually adjust the mask image context according to the target objectBoundingBox.
    AffineTransform maskContentTransformation;

    if (protectedMaskElement()->maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskContentTransformation.translate(objectBoundingBox.location());
        maskContentTransformation.scale(objectBoundingBox.size());
    }

    // Draw the content into the ImageBuffer.
    checkedLayer()->paintSVGResourceLayer(context, maskContentTransformation);
    return true;
}

bool RenderSVGResourceMasker::drawContentIntoContext(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, ImagePaintingOptions options)
{
    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    context.translate(destinationRect.location());

    if (destinationRect.size() != sourceRect.size())
        context.scale(destinationRect.size() / sourceRect.size());

    context.translate(-sourceRect.location());
    return drawContentIntoContext(context, { { }, destinationRect.size() });
}

}
