/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
    Copyright (C) 2009, Google, Inc.
    Copyright (C) 2009 Dirk Schulze <krit@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGImage.h"

#include "Attr.h"
#include "FloatConversion.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "PointerEventsHitRules.h"
#include "RenderLayer.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGResourceFilter.h"
#include "SVGImageElement.h"
#include "SVGLength.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRenderSupport.h"
#include "SVGResources.h"

namespace WebCore {

RenderSVGImage::RenderSVGImage(SVGImageElement* impl)
    : RenderImage(impl)
    , m_needsTransformUpdate(true)
{
    setImageResource(RenderImageResource::create());
}

void RenderSVGImage::layout()
{
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, m_everHadLayout && checkForRepaintDuringLayout());
    SVGImageElement* image = static_cast<SVGImageElement*>(node());

    bool updateCachedBoundariesInParents = false;
    if (m_needsTransformUpdate) {
        m_localTransform = image->animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    // minimum height
    setHeight(imageResource()->errorOccurred() ? intrinsicSize().height() : 0);

    calcWidth();
    calcHeight();

    // FIXME: Optimize caching the repaint rects.
    FloatRect oldBoundaries = m_localBounds;
    m_localBounds = FloatRect(image->x().value(image), image->y().value(image), image->width().value(image), image->height().value(image));
    m_cachedLocalRepaintRect = FloatRect();

    if (!updateCachedBoundariesInParents)
        updateCachedBoundariesInParents = oldBoundaries != m_localBounds;

    // Invalidate all resources of this client if our layout changed.
    if (m_everHadLayout && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        RenderImage::setNeedsBoundariesUpdate();

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled() || style()->visibility() == HIDDEN)
        return;

    paintInfo.context->save();
    paintInfo.context->concatCTM(localToParentTransform());

    if (paintInfo.phase == PaintPhaseForeground) {
        PaintInfo savedInfo(paintInfo);

        if (SVGRenderSupport::prepareToRenderSVGContent(this, paintInfo)) {
            Image* image = imageResource()->image();
            FloatRect destRect = m_localBounds;
            FloatRect srcRect(0, 0, image->width(), image->height());

            SVGImageElement* imageElt = static_cast<SVGImageElement*>(node());
            if (imageElt->preserveAspectRatio().align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
                imageElt->preserveAspectRatio().transformRect(destRect, srcRect);

            paintInfo.context->drawImage(image, DeviceColorSpace, destRect, srcRect);
        }
        SVGRenderSupport::finishRenderSVGContent(this, paintInfo, savedInfo.context);
    }

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth())
        paintOutline(paintInfo.context, 0, 0, width(), height());

    paintInfo.context->restore();
}

void RenderSVGImage::destroy()
{
    SVGResourcesCache::clientDestroyed(this);
    RenderImage::destroy();
}

void RenderSVGImage::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    if (diff == StyleDifferenceLayout)
        setNeedsBoundariesUpdate();
    RenderImage::styleWillChange(diff, newStyle);
}

void RenderSVGImage::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderImage::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, style());
}

void RenderSVGImage::updateFromElement()
{
    RenderImage::updateFromElement();
    SVGResourcesCache::clientUpdatedFromElement(this, style());
}

bool RenderSVGImage::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, request, style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);
            
        if (!SVGRenderSupport::pointInClippingArea(this, localPoint))
            return false;

        if (hitRules.canHitFill) {
            if (m_localBounds.contains(localPoint)) {
                updateHitTestResult(result, roundedIntPoint(localPoint));
                return true;
            }
        }
    }

    return false;
}

bool RenderSVGImage::nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

FloatRect RenderSVGImage::repaintRectInLocalCoordinates() const
{
    // If we already have a cached repaint rect, return that
    if (!m_cachedLocalRepaintRect.isEmpty())
        return m_cachedLocalRepaintRect;

    m_cachedLocalRepaintRect = m_localBounds;
    SVGRenderSupport::intersectRepaintRectWithResources(this, m_cachedLocalRepaintRect);

    return m_cachedLocalRepaintRect;
}

void RenderSVGImage::imageChanged(WrappedImagePtr image, const IntRect* rect)
{
    RenderImage::imageChanged(image, rect);

    // The image resource defaults to nullImage until the resource arrives.
    // This empty image may be cached by SVG resources which must be invalidated.
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this))
        resources->removeClientFromCache(this);

    // Eventually notify parent resources, that we've changed.
    RenderSVGResource::markForLayoutAndParentResourceInvalidation(this, false);

    repaint();
}

IntRect RenderSVGImage::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGImage::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    SVGRenderSupport::computeRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

void RenderSVGImage::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState) const
{
    SVGRenderSupport::mapLocalToContainer(this, repaintContainer, fixed, useTransforms, transformState);
}

void RenderSVGImage::addFocusRingRects(Vector<IntRect>& rects, int, int)
{
    // this is called from paint() after the localTransform has already been applied
    IntRect contentRect = enclosingIntRect(repaintRectInLocalCoordinates());
    if (!contentRect.isEmpty())
        rects.append(contentRect);
}

void RenderSVGImage::absoluteRects(Vector<IntRect>&, int, int)
{
    // This code path should never be taken for SVG, as we're assuming useTransforms=true everywhere, absoluteQuads should be used.
    ASSERT_NOT_REACHED();
}

void RenderSVGImage::absoluteQuads(Vector<FloatQuad>& quads)
{
    quads.append(localToAbsoluteQuad(strokeBoundingBox()));
}

}

#endif // ENABLE(SVG)
