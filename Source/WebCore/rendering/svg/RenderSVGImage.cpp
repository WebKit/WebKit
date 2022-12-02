/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGImage.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "AXObjectCache.h"
#include "BitmapImage.h"
#include "DocumentInlines.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "PointerEventsHitRules.h"
#include "RenderImageResource.h"
#include "RenderLayer.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceFilter.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImageElement.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGImage);

RenderSVGImage::RenderSVGImage(SVGImageElement& element, RenderStyle&& style)
    : RenderSVGModelObject(element, WTFMove(style))
    , m_imageResource(makeUnique<RenderImageResource>())
{
    imageResource().initialize(*this);
}

RenderSVGImage::~RenderSVGImage() = default;

void RenderSVGImage::willBeDestroyed()
{
    imageResource().shutdown();
    RenderSVGModelObject::willBeDestroyed();
}

SVGImageElement& RenderSVGImage::imageElement() const
{
    return downcast<SVGImageElement>(RenderSVGModelObject::element());
}

FloatRect RenderSVGImage::calculateObjectBoundingBox() const
{
    LayoutSize intrinsicSize;
    if (CachedImage* cachedImage = imageResource().cachedImage())
        intrinsicSize = cachedImage->imageSizeForRenderer(nullptr, style().effectiveZoom());

    SVGLengthContext lengthContext(&imageElement());

    Length width = style().width();
    Length height = style().height();

    float concreteWidth;
    if (!width.isAuto())
        concreteWidth = lengthContext.valueForLength(width, SVGLengthMode::Width);
    else if (!height.isAuto() && !intrinsicSize.isEmpty())
        concreteWidth = lengthContext.valueForLength(height, SVGLengthMode::Height) * intrinsicSize.width() / intrinsicSize.height();
    else
        concreteWidth = intrinsicSize.width();

    float concreteHeight;
    if (!height.isAuto())
        concreteHeight = lengthContext.valueForLength(height, SVGLengthMode::Height);
    else if (!width.isAuto() && !intrinsicSize.isEmpty())
        concreteHeight = lengthContext.valueForLength(width, SVGLengthMode::Width) * intrinsicSize.height() / intrinsicSize.width();
    else
        concreteHeight = intrinsicSize.height();

    return { imageElement().x().value(lengthContext), imageElement().y().value(lengthContext), concreteWidth, concreteHeight };
}

void RenderSVGImage::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    updateImageViewport();
    setCurrentSVGLayoutRect(enclosingLayoutRect(m_objectBoundingBox));

    updateLayerTransform();

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(*this);

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGImage::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    OptionSet<PaintPhase> relevantPaintPhases { PaintPhase::Foreground, PaintPhase::ClippingMask, PaintPhase::Mask, PaintPhase::Outline, PaintPhase::SelfOutline };
    if (!shouldPaintSVGRenderer(paintInfo, relevantPaintPhases) || !imageResource().cachedImage())
        return;

    if (paintInfo.phase == PaintPhase::ClippingMask) {
        // FIXME: [LBSE] Upstream SVGRenderSupport changes
        // SVGRenderSupport::paintSVGClippingMask(*this, paintInfo);
        return;
    }

    auto adjustedPaintOffset = paintOffset + currentSVGLayoutLocation();
    if (paintInfo.phase == PaintPhase::Mask) {
        // FIXME: [LBSE] Upstream SVGRenderSupport changes
        // SVGRenderSupport::paintSVGMask(*this, paintInfo, adjustedPaintOffset);
        return;
    }

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedPaintOffset);
    if (!visualOverflowRect.intersects(paintInfo.rect))
        return;

    if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
        // FIXME: [LBSE] Upstream outline painting
        // paintSVGOutline(paintInfo, adjustedPaintOffset);
        return;
    }

    ASSERT(paintInfo.phase == PaintPhase::Foreground);
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    auto coordinateSystemOriginTranslation = adjustedPaintOffset - flooredLayoutPoint(objectBoundingBox().location());
    paintInfo.context().translate(coordinateSystemOriginTranslation.width(), coordinateSystemOriginTranslation.height());

    if (style().svgStyle().bufferedRendering() == BufferedRendering::Static && bufferForeground(paintInfo, flooredLayoutPoint(objectBoundingBox().location())))
        return;

    paintForeground(paintInfo, flooredLayoutPoint(objectBoundingBox().location()));
}

ImageDrawResult RenderSVGImage::paintIntoRect(PaintInfo& paintInfo, const FloatRect& rect, const FloatRect& sourceRect)
{
    if (!imageResource().cachedImage() || rect.width() <= 0 || rect.height() <= 0)
        return ImageDrawResult::DidNothing;

    RefPtr<Image> image = imageResource().image();
    if (!image || image->isNull())
        return ImageDrawResult::DidNothing;

    if (is<BitmapImage>(image))
        downcast<BitmapImage>(*image).updateFromSettings(settings());

    ImagePaintingOptions options = {
        CompositeOperator::SourceOver,
        DecodingMode::Synchronous,
        imageOrientation(),
        InterpolationQuality::Default
    };

    auto drawResult = paintInfo.context().drawImage(*image, rect, sourceRect, options);
    if (drawResult == ImageDrawResult::DidRequestDecoding)
        imageResource().cachedImage()->addClientWaitingForAsyncDecoding(*this);

    return drawResult;
}

void RenderSVGImage::paintForeground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext& context = paintInfo.context();
    if (context.invalidatingImagesWithAsyncDecodes()) {
        if (cachedImage() && cachedImage()->isClientWaitingForAsyncDecoding(*this))
            cachedImage()->removeAllClientsWaitingForAsyncDecoding();
        return;
    }

    if (!imageResource().cachedImage()) {
        page().addRelevantUnpaintedObject(this, visualOverflowRectEquivalent());
        return;
    }

    RefPtr<Image> image = imageResource().image();
    if (!image || image->isNull()) {
        page().addRelevantUnpaintedObject(this, visualOverflowRectEquivalent());
        return;
    }

    FloatRect contentBoxRect = borderBoxRectEquivalent();
    FloatRect replacedContentRect(0, 0, image->width(), image->height());
    imageElement().preserveAspectRatio().transformRect(contentBoxRect, replacedContentRect);

    contentBoxRect.moveBy(paintOffset);

    ImageDrawResult result = paintIntoRect(paintInfo, contentBoxRect, replacedContentRect);

    if (cachedImage()) {
        // For now, count images as unpainted if they are still progressively loading. We may want
        // to refine this in the future to account for the portion of the image that has painted.
        FloatRect visibleRect = intersection(replacedContentRect, contentBoxRect);
        if (cachedImage()->isLoading() || result == ImageDrawResult::DidRequestDecoding)
            page().addRelevantUnpaintedObject(this, enclosingLayoutRect(visibleRect));
        else
            page().addRelevantRepaintedObject(this, enclosingLayoutRect(visibleRect));
    }
}

bool RenderSVGImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    auto adjustedLocation = accumulatedOffset + currentSVGLayoutLocation();

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedLocation);
    if (!locationInContainer.intersects(visualOverflowRect))
        return false;

    auto localPoint = locationInContainer.point();
    auto boundingBoxTopLeftCorner = flooredLayoutPoint(objectBoundingBox().minXMinYCorner());
    auto coordinateSystemOriginTranslation = boundingBoxTopLeftCorner - adjustedLocation;
    localPoint.move(coordinateSystemOriginTranslation);

    if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, request, style().pointerEvents());
    bool isVisible = (style().visibility() == Visibility::Visible);
    if (isVisible || !hitRules.requireVisible) {
        SVGHitTestCycleDetectionScope hitTestScope(*this);

        if (hitRules.canHitFill) {
            if (m_objectBoundingBox.contains(localPoint)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
                if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer, visualOverflowRect) == HitTestProgress::Stop)
                    return true;
            }
        }
    }

    return false;
}

bool RenderSVGImage::updateImageViewport()
{
    auto oldBoundaries = m_objectBoundingBox;
    m_objectBoundingBox = calculateObjectBoundingBox();

    bool updatedViewport = false;
    URL imageSourceURL = document().completeURL(imageElement().imageSourceURL());

    // Images with preserveAspectRatio=none should force non-uniform scaling. This can be achieved
    // by setting the image's container size to its intrinsic size.
    // See: http://www.w3.org/TR/SVG/single-page.html, 7.8 The ‘preserveAspectRatio’ attribute.
    if (imageElement().preserveAspectRatio().align() == SVGPreserveAspectRatioValue::SVG_PRESERVEASPECTRATIO_NONE) {
        if (CachedImage* cachedImage = imageResource().cachedImage()) {
            LayoutSize intrinsicSize = cachedImage->imageSizeForRenderer(nullptr, style().effectiveZoom());
            if (intrinsicSize != imageResource().imageSize(style().effectiveZoom())) {
                imageResource().setContainerContext(roundedIntSize(intrinsicSize), imageSourceURL);
                updatedViewport = true;
            }
        }
    }

    if (oldBoundaries != m_objectBoundingBox) {
        if (!updatedViewport)
            imageResource().setContainerContext(enclosingIntRect(m_objectBoundingBox).size(), imageSourceURL);
        updatedViewport = true;
    }

    return updatedViewport;
}

void RenderSVGImage::repaintOrMarkForLayout(const IntRect* rect)
{
    // Update the SVGImageCache sizeAndScales entry in case image loading finished after layout.
    // (https://bugs.webkit.org/show_bug.cgi?id=99489)
    m_objectBoundingBox = FloatRect();
    if (updateImageViewport())
        setNeedsLayout();

    m_bufferedForeground = nullptr;

    FloatRect repaintRect = borderBoxRectEquivalent();
    if (rect) {
        // The image changed rect is in source image coordinates (pre-zooming),
        // so map from the bounds of the image to the contentsBox.
        repaintRect.intersect(enclosingIntRect(mapRect(*rect, FloatRect(FloatPoint(), imageResource().imageSize(1.0f)), repaintRect)));
    }

    repaintRectangle(enclosingLayoutRect(repaintRect));

    // Tell any potential compositing layers that the image needs updating.
    if (hasLayer())
        layer()->contentChanged(ImageChanged);
}

void RenderSVGImage::notifyFinished(CachedResource& newImage, const NetworkLoadMetrics& metrics)
{
    if (renderTreeBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

    if (&newImage == cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        if (hasLayer())
            layer()->contentChanged(ImageChanged);
    }

    RenderSVGModelObject::notifyFinished(newImage, metrics);
}

void RenderSVGImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    if (renderTreeBeingDestroyed())
        return;

    // The image resource defaults to nullImage until the resource arrives.
    // This empty image may be cached by SVG resources which must be invalidated.
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this))
        resources->removeClientFromCache(*this);

    // Eventually notify parent resources, that we've changed.
    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*this, false);

    if (hasVisibleBoxDecorations() || hasMask() || hasShapeOutside())
        RenderSVGModelObject::imageChanged(newImage, rect);

    if (newImage != imageResource().imagePtr() || !newImage)
        return;

    repaintOrMarkForLayout(rect);

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferRecomputeIsIgnoredIfNeeded(&imageElement());
}

bool RenderSVGImage::bufferForeground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto& destinationContext = paintInfo.context();

    auto repaintBoundingBox = borderBoxRectEquivalent();
    repaintBoundingBox.moveBy(paintOffset);

    // Invalidate an existing buffer if the scale is not correct.
    const auto& absoluteTransform = destinationContext.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);

    auto absoluteTargetRect = enclosingIntRect(absoluteTransform.mapRect(repaintBoundingBox));
    if (m_bufferedForeground) {
        if (absoluteTargetRect.size() != m_bufferedForeground->backendSize())
            m_bufferedForeground = nullptr;
        else {
            const auto& absoluteTransformBuffer = m_bufferedForeground->context().getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
            if (absoluteTransformBuffer != absoluteTransform)
                m_bufferedForeground = nullptr;
        }
    }

    // Create a new buffer and paint the foreground into it.
    if (!m_bufferedForeground) {
        m_bufferedForeground = destinationContext.createAlignedImageBuffer(expandedIntSize(repaintBoundingBox.size()));
        if (!m_bufferedForeground)
            return false;
    }

    auto& bufferedContext = m_bufferedForeground->context();
    bufferedContext.clearRect(absoluteTargetRect);

    PaintInfo bufferedInfo(paintInfo);
    bufferedInfo.setContext(bufferedContext);
    paintForeground(bufferedInfo, paintOffset);

    destinationContext.concatCTM(absoluteTransform.inverse().value_or(AffineTransform()));
    destinationContext.drawImageBuffer(*m_bufferedForeground, absoluteTargetRect);
    destinationContext.concatCTM(absoluteTransform);

    return true;
}

bool RenderSVGImage::needsHasSVGTransformFlags() const
{
    return imageElement().hasTransformRelatedAttributes();
}

void RenderSVGImage::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    applySVGTransform(transform, imageElement(), style, boundingBox, std::nullopt, std::nullopt, options);
}

} // namespace WebCore

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
