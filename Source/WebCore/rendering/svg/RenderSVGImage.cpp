/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "BitmapImage.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "LayoutRepainter.h"
#include "PointerEventsHitRules.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderImageResource.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderSVGModelObjectInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImageElement.h"
#include "SVGImageIntrinsicSizing.h"
#include "SVGVisitedRendererTracking.h"
#include "Settings.h"
#include "StyleImageDrawingExtras.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGImage);

RenderSVGImage::RenderSVGImage(SVGImageElement& element, Style::ComputedStyle&& style)
    : RenderSVGModelObject(Type::SVGImage, element, WTF::move(style))
    , m_imageResource(makeUniqueRef<RenderImageResource>())
{
    ASSERT(isRenderSVGImage());
    imageResource().initialize(*this);
}

RenderSVGImage::~RenderSVGImage() = default;

void RenderSVGImage::willBeDestroyed()
{
    imageResource().willBeDestroyed();
    RenderSVGModelObject::willBeDestroyed();
}

SVGImageElement& RenderSVGImage::imageElement() const
{
    return downcast<SVGImageElement>(RenderSVGModelObject::element());
}

FloatRect RenderSVGImage::calculateObjectBoundingBox() const
{
    return calculateSVGImageObjectBoundingBox(imageElement(), style(), imageResource().cachedImage());
}

void RenderSVGImage::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    LayoutRepainter repainter(*this);

    updateImageViewport();

    // Refresh the layout-rect size only. SVGContainerLayout assigns the container-relative location.
    setCurrentSVGLayoutRect({ currentSVGLayoutLocation(), enclosingLayoutRect(m_objectBoundingBox).size() });

    updateLayerTransform();
    updateCachedVisualOverflowRect();

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGImage::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase == PaintPhase::EventRegion) {
        paintSVGEventRegion(paintInfo, paintOffset);
        return;
    }

    OptionSet<PaintPhase> relevantPaintPhases { PaintPhase::Foreground, PaintPhase::ClippingMask, PaintPhase::Mask, PaintPhase::Outline, PaintPhase::SelfOutline };
    if (!shouldPaintSVGRenderer(paintInfo, relevantPaintPhases) || !imageResource().cachedImage())
        return;

    if (paintInfo.phase == PaintPhase::ClippingMask) {
        paintSVGClippingMask(paintInfo, objectBoundingBox());
        return;
    }

    auto adjustedPaintOffset = paintOffset + currentSVGLayoutLocation();
    if (paintInfo.phase == PaintPhase::Mask) {
        paintSVGMask(paintInfo, adjustedPaintOffset);
        return;
    }

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedPaintOffset);
    if (!visualOverflowRect.intersects(paintInfo.rect))
        return;

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
        paintInfo.context().resetClip();
        paintSVGOutline(paintInfo, adjustedPaintOffset);
        return;
    }

    ASSERT(paintInfo.phase == PaintPhase::Foreground);

    auto coordinateSystemOriginTranslation = adjustedPaintOffset - flooredLayoutPoint(objectBoundingBox().location());
    paintInfo.context().translate(coordinateSystemOriginTranslation.width(), coordinateSystemOriginTranslation.height());

    if (style().bufferedRendering() == BufferedRendering::Static && bufferForeground(paintInfo, flooredLayoutPoint(objectBoundingBox().location())))
        return;

    paintForeground(paintInfo, flooredLayoutPoint(objectBoundingBox().location()));
}

ImageDrawResult RenderSVGImage::paintIntoRect(PaintInfo& paintInfo, const FloatRect& rect, const FloatRect& sourceRect, FloatSize imageRenderingSize)
{
    if (!imageResource().cachedImage() || rect.width() <= 0 || rect.height() <= 0)
        return ImageDrawResult::DidNothing;

    RefPtr styleImage = imageResource().styleImage();
    if (!styleImage || !styleImage->canDrawAtSize(*this, rect.size()))
        return ImageDrawResult::DidNothing;

    auto concreteObjectSize = ConcreteObjectSize::fixed(imageRenderingSize);

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    auto invertContent = [&] {
        if (styleImage->drawsSVGImage())
            return AXCustomColorModeController::shouldInvertSVGImage(*this, *styleImage);

        return AXCustomColorModeController::shouldInvertContentImage(*this, rect.size());
    };
#endif

    ImagePaintingOptions options {
        CompositeOperator::SourceOver,
        DecodingMode::Synchronous,
        imageOrientation(),
        styleImage->interpolationQualityForImageDraw(paintInfo.context(), *this, concreteObjectSize, styleImage.get(), LayoutSize(rect.size())),
        settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        invertContent() ? InvertContent::Yes : InvertContent::No,
#endif
        settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No,
        IgnoreRootPreserveAspectRatio::Yes,
        settings().hdrAcceleratedApplyGainMapEnabled() ? AllowAcceleratedApplyGainMap::Yes : AllowAcceleratedApplyGainMap::No,
        paintInfo.paintBehavior.contains(PaintBehavior::DrawsHDRContent) ? DrawsHDRContent::Yes : DrawsHDRContent::No,
        style().dynamicRangeLimit().toPlatformDynamicRangeLimit()
    };

    auto drawResult = styleImage->draw(paintInfo.context(), *this, concreteObjectSize, rect, sourceRect, options);
    if (drawResult == ImageDrawResult::DidRequestDecoding)
        protect(imageResource().cachedImage())->addClientWaitingForAsyncDecoding(protect(cachedImageClient()));

    return drawResult;
}

void RenderSVGImage::paintForeground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext& context = paintInfo.context();
    if (context.invalidatingImagesWithAsyncDecodes()) {
        if (cachedImage() && cachedImage()->isClientWaitingForAsyncDecoding(cachedImageClient()))
            protect(cachedImage())->removeAllClientsWaitingForAsyncDecoding();
        return;
    }

    if (!imageResource().cachedImage()) {
        protect(page())->addRelevantUnpaintedObject(*this, visualOverflowRectEquivalent());
        return;
    }

    RefPtr styleImage = imageResource().styleImage();
    if (!styleImage || !styleImage->canDraw(*this)) {
        protect(page())->addRelevantUnpaintedObject(*this, visualOverflowRectEquivalent());
        return;
    }

    FloatRect contentBoxRect = borderBoxRectEquivalent();
    auto placement = calculateSVGImagePlacement(*styleImage, *this, imageElement().preserveAspectRatio(), contentBoxRect);
    placement.destination.moveBy(paintOffset);
    contentBoxRect.moveBy(paintOffset);

    ImageDrawResult result = paintIntoRect(paintInfo, placement.destination, placement.source, placement.imageRenderingRectangle.size());

    if (cachedImage() && !context.paintingDisabled()) {
        // For now, count images as unpainted if they are still progressively loading. We may want
        // to refine this in the future to account for the portion of the image that has painted.
        auto visibleRect = intersection(placement.destination, contentBoxRect);
        if (cachedImage()->isLoading() || result == ImageDrawResult::DidRequestDecoding)
            protect(page())->addRelevantUnpaintedObject(*this, enclosingLayoutRect(visibleRect));
        else
            protect(page())->addRelevantRepaintedObject(*this, enclosingLayoutRect(visibleRect));

        auto localVisibleRect = visibleRect;
        localVisibleRect.moveBy(-paintOffset);
        protect(document())->didPaintImage(protect(imageElement()).get(), protect(cachedImage()), localVisibleRect);
    }
}

bool RenderSVGImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestAction::Foreground)
        return false;

    auto adjustedLocation = accumulatedOffset + currentSVGLayoutLocation();

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedLocation);
    if (!locationInContainer.intersects(visualOverflowRect))
        return false;

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    auto localPoint = locationInContainer.point();
    auto boundingBoxTopLeftCorner = objectBoundingBoxLocation();
    auto coordinateSystemOriginTranslation = boundingBoxTopLeftCorner - adjustedLocation;
    localPoint.move(coordinateSystemOriginTranslation);

    if (!pointInSVGClippingArea(localPoint))
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGImage, request, usedPointerEvents());
    if (request.isVisibleForStyle(usedStyle()) || !hitRules.requireVisible) {
        if (hitRules.canHitFill) {
            if (m_objectBoundingBox.contains(localPoint)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
                if (result.addNodeToListBasedTestResult(protect(nodeForHitTest()).get(), request, locationInContainer, visualOverflowRect) == HitTestProgress::Stop)
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
    return oldBoundaries != m_objectBoundingBox;
}

void RenderSVGImage::repaintOrMarkForLayout(const IntRect* rect)
{
    // Recompute the object bounding box in case image loading finished after layout.
    // (https://bugs.webkit.org/show_bug.cgi?id=99489)
    m_objectBoundingBox = FloatRect();
    if (updateImageViewport())
        setNeedsLayout();

    m_bufferedForeground = nullptr;

    FloatRect repaintRect = borderBoxRectEquivalent();
    if (RefPtr styleImage = imageResource().styleImage(); styleImage && rect) {
        auto naturalDimensions = calculateSVGImageNaturalDimensions(*styleImage, *this);
        auto placement = calculateSVGImagePlacement(*styleImage, *this, imageElement().preserveAspectRatio(), repaintRect);

        auto changedRectSpace = naturalDimensions.width && naturalDimensions.height ? FloatSize { *naturalDimensions.width, *naturalDimensions.height } : placement.imageRenderingRectangle.size();

        repaintRect.intersect(enclosingIntRect(mapRect(*rect, FloatRect(FloatPoint(), changedRectSpace), placement.imageRenderingRectangle)));
    }

    repaintRectangle(enclosingLayoutRect(repaintRect));

    // Tell any potential compositing layers that the image needs updating.
    if (hasLayer())
        layer()->contentChanged(ContentChangeType::Image);
}

void RenderSVGImage::notifyFinished(CachedResource& newImage, const NetworkLoadMetrics& metrics, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    if (renderTreeBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

    if (&newImage == cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        if (hasLayer())
            layer()->contentChanged(ContentChangeType::Image);
    }

    RenderSVGModelObject::notifyFinished(newImage, metrics, loadWillContinueInAnotherProcess);
}

void RenderSVGImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    if (renderTreeBeingDestroyed() || !parent())
        return;

    repaintClientsOfReferencedSVGResources();

    if (hasVisibleBoxDecorations() || hasMask() || hasShapeOutside())
        RenderSVGModelObject::imageChanged(newImage, rect);

    if (newImage != imageResource().imagePtr() || !newImage)
        return;

    repaintOrMarkForLayout(rect);

    if (CheckedPtr cache = protect(document())->existingAXObjectCache())
        cache->deferRecomputeIsIgnoredIfNeeded(protect(imageElement()).ptr());

    if (RefPtr image = imageResource().cachedImage(); image && image->currentFrameIsComplete()) {
        if (auto styleable = Styleable::fromRenderer(*this))
            protect(document())->didLoadImage(protect(styleable->element).get(), image);
    }
}

bool RenderSVGImage::bufferForeground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto& destinationContext = paintInfo.context();

    FloatRect repaintBoundingBox = borderBoxRectEquivalent();
    repaintBoundingBox.moveBy(paintOffset);

    auto scaledSize = destinationContext.compatibleImageBufferSize(repaintBoundingBox.size());
    if (m_bufferedForeground && (m_bufferedForegroundSize != repaintBoundingBox.size() || m_bufferedForegroundScaledSize != scaledSize))
        m_bufferedForeground = nullptr;

    // Create a new buffer and paint the foreground into it.
    if (!m_bufferedForeground) {
        m_bufferedForeground = destinationContext.createAlignedImageBuffer(repaintBoundingBox.size());
        if (!m_bufferedForeground)
            return false;
        m_bufferedForegroundSize = repaintBoundingBox.size();
        m_bufferedForegroundScaledSize = scaledSize;
    }

    RefPtr bufferedForeground = m_bufferedForeground.copyRef();
    {
        auto& bufferedContext = bufferedForeground->context();
        GraphicsContextStateSaver stateSaver(bufferedContext);
        bufferedContext.translate(-toFloatSize(repaintBoundingBox.location()));
        bufferedContext.clearRect(repaintBoundingBox);

        PaintInfo bufferedInfo(paintInfo);
        bufferedInfo.setContext(bufferedContext);
        paintForeground(bufferedInfo, paintOffset);
    }

    destinationContext.drawImageBuffer(*bufferedForeground, repaintBoundingBox);
    return true;
}

bool RenderSVGImage::needsHasSVGTransformFlags() const
{
    return protect(imageElement())->hasTransformRelatedAttributes();
}

void RenderSVGImage::applyTransform(TransformationMatrix& transform, const Style::ComputedStyle& style, const FloatRect& boundingBox, OptionSet<Style::TransformResolverOption> options) const
{
    applySVGTransform(transform, protect(imageElement()), style, boundingBox, std::nullopt, std::nullopt, options);
}

} // namespace WebCore
