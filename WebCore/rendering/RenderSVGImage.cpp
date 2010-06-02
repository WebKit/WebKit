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

namespace WebCore {

RenderSVGImage::RenderSVGImage(SVGImageElement* impl)
    : RenderImage(impl)
    , m_needsTransformUpdate(true)
{
}

void RenderSVGImage::layout()
{
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    SVGImageElement* image = static_cast<SVGImageElement*>(node());

    if (m_needsTransformUpdate) {
        m_localTransform = image->animatedLocalTransform();
        m_needsTransformUpdate = false;
    }

    // minimum height
    setHeight(errorOccurred() ? intrinsicSize().height() : 0);

    calcWidth();
    calcHeight();

    m_localBounds = FloatRect(image->x().value(image), image->y().value(image), image->width().value(image), image->height().value(image));
    m_cachedLocalRepaintRect = FloatRect();

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
        RenderSVGResourceFilter* filter = 0;

        PaintInfo savedInfo(paintInfo);

        if (prepareToRenderSVGContent(this, paintInfo, m_localBounds, filter)) {
            FloatRect destRect = m_localBounds;
            FloatRect srcRect(0, 0, image()->width(), image()->height());

            SVGImageElement* imageElt = static_cast<SVGImageElement*>(node());
            if (imageElt->preserveAspectRatio().align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
                imageElt->preserveAspectRatio().transformRect(destRect, srcRect);

            paintInfo.context->drawImage(image(), DeviceColorSpace, destRect, srcRect);
        }
        finishRenderSVGContent(this, paintInfo, filter, savedInfo.context);
    }

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth())
        paintOutline(paintInfo.context, 0, 0, width(), height());

    paintInfo.context->restore();
}

void RenderSVGImage::destroy()
{
    deregisterFromResources(this);
    RenderImage::destroy();
}

bool RenderSVGImage::nodeAtFloatPoint(const HitTestRequest&, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, style()->pointerEvents());
    
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);

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

FloatRect RenderSVGImage::objectBoundingBox() const
{
    return m_localBounds;
}

FloatRect RenderSVGImage::repaintRectInLocalCoordinates() const
{
    // If we already have a cached repaint rect, return that
    if (!m_cachedLocalRepaintRect.isEmpty())
        return m_cachedLocalRepaintRect;

    m_cachedLocalRepaintRect = m_localBounds;

    // FIXME: We need to be careful here. We assume that there is no filter,
    // clipper or masker if the rects are empty.
    FloatRect rect = filterBoundingBoxForRenderer(this);
    if (!rect.isEmpty())
        m_cachedLocalRepaintRect = rect;

    rect = clipperBoundingBoxForRenderer(this);
    if (!rect.isEmpty())
        m_cachedLocalRepaintRect.intersect(rect);

    rect = maskerBoundingBoxForRenderer(this);
    if (!rect.isEmpty())
        m_cachedLocalRepaintRect.intersect(rect);

    style()->svgStyle()->inflateForShadow(m_cachedLocalRepaintRect);

    return m_cachedLocalRepaintRect;
}

void RenderSVGImage::imageChanged(WrappedImagePtr image, const IntRect* rect)
{
    RenderImage::imageChanged(image, rect);
#if ENABLE(FILTERS)
    // The image resource defaults to nullImage until the resource arrives.
    // This empty image may be cached by SVG filter effects which must be invalidated.
    if (RenderSVGResourceFilter* filter = getRenderSVGResourceById<RenderSVGResourceFilter>(document(), style()->svgStyle()->filterResource()))
        filter->invalidateClient(this);
#endif
    repaint();
}

IntRect RenderSVGImage::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    return SVGRenderBase::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGImage::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    SVGRenderBase::computeRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

void RenderSVGImage::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState) const
{
    SVGRenderBase::mapLocalToContainer(this, repaintContainer, fixed, useTransforms, transformState);
}

void RenderSVGImage::addFocusRingRects(Vector<IntRect>& rects, int, int)
{
    // this is called from paint() after the localTransform has already been applied
    IntRect contentRect = enclosingIntRect(repaintRectInLocalCoordinates());
    if (!contentRect.isEmpty())
        rects.append(contentRect);
}

void RenderSVGImage::absoluteRects(Vector<IntRect>& rects, int, int)
{
    rects.append(absoluteClippedOverflowRect());
}

void RenderSVGImage::absoluteQuads(Vector<FloatQuad>& quads)
{
    quads.append(FloatRect(absoluteClippedOverflowRect()));
}

}

#endif // ENABLE(SVG)
