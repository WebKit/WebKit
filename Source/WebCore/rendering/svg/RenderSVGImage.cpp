/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "PointerEventsHitRules.h"
#include "RenderImageResource.h"
#include "RenderLayer.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceFilter.h"
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
    , m_needsBoundariesUpdate(true)
    , m_needsTransformUpdate(true)
    , m_imageResource(std::make_unique<RenderImageResource>())
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

bool RenderSVGImage::updateImageViewport()
{
    FloatRect oldBoundaries = m_objectBoundingBox;
    bool updatedViewport = false;

    SVGLengthContext lengthContext(&imageElement());
    m_objectBoundingBox = FloatRect(imageElement().x().value(lengthContext), imageElement().y().value(lengthContext), imageElement().width().value(lengthContext), imageElement().height().value(lengthContext));

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
        m_needsBoundariesUpdate = true;
    }

    return updatedViewport;
}

void RenderSVGImage::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, SVGRenderSupport::checkForSVGRepaintDuringLayout(*this) && selfNeedsLayout());
    updateImageViewport();

    bool transformOrBoundariesUpdate = m_needsTransformUpdate || m_needsBoundariesUpdate;
    if (m_needsTransformUpdate) {
        m_localTransform = imageElement().animatedLocalTransform();
        m_needsTransformUpdate = false;
    }

    if (m_needsBoundariesUpdate) {
        m_repaintBoundingBoxExcludingShadow = m_objectBoundingBox;
        SVGRenderSupport::intersectRepaintRectWithResources(*this, m_repaintBoundingBoxExcludingShadow);

        m_repaintBoundingBox = m_repaintBoundingBoxExcludingShadow;

        m_needsBoundariesUpdate = false;
    }

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(*this);

    // If our bounds changed, notify the parents.
    if (transformOrBoundariesUpdate)
        RenderSVGModelObject::setNeedsBoundariesUpdate();

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGImage::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context().paintingDisabled() || paintInfo.phase != PaintPhase::Foreground
        || style().visibility() == Visibility::Hidden || !imageResource().cachedImage())
        return;

    FloatRect boundingBox = repaintRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsRepaintRect(boundingBox, m_localTransform, paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(childPaintInfo.context());
    childPaintInfo.applyTransform(m_localTransform);

    if (childPaintInfo.phase == PaintPhase::Foreground) {
        SVGRenderingContext renderingContext(*this, childPaintInfo);

        if (renderingContext.isRenderingPrepared()) {
            if (style().svgStyle().bufferedRendering() == BufferedRendering::Static && renderingContext.bufferForeground(m_bufferedForeground))
                return;

            paintForeground(childPaintInfo);
        }
    }

    if (style().outlineWidth())
        paintOutline(childPaintInfo, IntRect(boundingBox));
}

void RenderSVGImage::paintForeground(PaintInfo& paintInfo)
{
    RefPtr<Image> image = imageResource().image();
    if (!image)
        return;

    FloatRect destRect = m_objectBoundingBox;
    FloatRect srcRect(0, 0, image->width(), image->height());

    imageElement().preserveAspectRatio().transformRect(destRect, srcRect);

    paintInfo.context().drawImage(*image, destRect, srcRect);
}

void RenderSVGImage::invalidateBufferedForeground()
{
    m_bufferedForeground.reset();
}

bool RenderSVGImage::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, request, style().pointerEvents());
    bool isVisible = (style().visibility() == Visibility::Visible);
    if (isVisible || !hitRules.requireVisible) {
        FloatPoint localPoint = localToParentTransform().inverse().value_or(AffineTransform()).mapPoint(pointInParent);
            
        if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
            return false;

        if (hitRules.canHitFill) {
            if (m_objectBoundingBox.contains(localPoint)) {
                updateHitTestResult(result, LayoutPoint(localPoint));
                if (result.addNodeToListBasedTestResult(&imageElement(), request, localPoint) == HitTestProgress::Stop)
                    return true;
            }
        }
    }

    return false;
}

void RenderSVGImage::imageChanged(WrappedImagePtr, const IntRect*)
{
    // The image resource defaults to nullImage until the resource arrives.
    // This empty image may be cached by SVG resources which must be invalidated.
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this))
        resources->removeClientFromCache(*this);

    // Eventually notify parent resources, that we've changed.
    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*this, false);

    // Update the SVGImageCache sizeAndScales entry in case image loading finished after layout.
    // (https://bugs.webkit.org/show_bug.cgi?id=99489)
    m_objectBoundingBox = FloatRect();
    updateImageViewport();

    invalidateBufferedForeground();

    repaint();
}

void RenderSVGImage::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&, const RenderLayerModelObject*)
{
    // this is called from paint() after the localTransform has already been applied
    LayoutRect contentRect = LayoutRect(repaintRectInLocalCoordinates());
    if (!contentRect.isEmpty())
        rects.append(contentRect);
}

} // namespace WebCore
