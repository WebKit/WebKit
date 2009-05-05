/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
    Copyright (C) 2009, Google, Inc.

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
#include "SVGImageElement.h"
#include "SVGLength.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRenderSupport.h"
#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"

namespace WebCore {

RenderSVGImage::RenderSVGImage(SVGImageElement* impl)
    : RenderImage(impl)
{
}

RenderSVGImage::~RenderSVGImage()
{
}

void RenderSVGImage::adjustRectsForAspectRatio(FloatRect& destRect, FloatRect& srcRect, SVGPreserveAspectRatio* aspectRatio)
{
    float origDestWidth = destRect.width();
    float origDestHeight = destRect.height();
    if (aspectRatio->meetOrSlice() == SVGPreserveAspectRatio::SVG_MEETORSLICE_MEET) {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        if (origDestHeight > (origDestWidth * widthToHeightMultiplier)) {
            destRect.setHeight(origDestWidth * widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                    destRect.setY(destRect.y() + origDestHeight / 2.0f - destRect.height() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setY(destRect.y() + origDestHeight - destRect.height());
                    break;
            }
        }
        if (origDestWidth > (origDestHeight / widthToHeightMultiplier)) {
            destRect.setWidth(origDestHeight / widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    destRect.setX(destRect.x() + origDestWidth / 2.0f - destRect.width() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setX(destRect.x() + origDestWidth - destRect.width());
                    break;
            }
        }
    } else if (aspectRatio->meetOrSlice() == SVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE) {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        // if the destination height is less than the height of the image we'll be drawing
        if (origDestHeight < (origDestWidth * widthToHeightMultiplier)) {
            float destToSrcMultiplier = srcRect.width() / destRect.width();
            srcRect.setHeight(destRect.height() * destToSrcMultiplier);
            switch(aspectRatio->align()) {
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                    srcRect.setY(destRect.y() + image()->height() / 2.0f - srcRect.height() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setY(destRect.y() + image()->height() - srcRect.height());
                    break;
            }
        }
        // if the destination width is less than the width of the image we'll be drawing
        if (origDestWidth < (origDestHeight / widthToHeightMultiplier)) {
            float destToSrcMultiplier = srcRect.height() / destRect.height();
            srcRect.setWidth(destRect.width() * destToSrcMultiplier);
            switch(aspectRatio->align()) {
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    srcRect.setX(destRect.x() + image()->width() / 2.0f - srcRect.width() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setX(destRect.x() + image()->width() - srcRect.width());
                    break;
            }
        }
    }
}

void RenderSVGImage::layout()
{
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    SVGImageElement* image = static_cast<SVGImageElement*>(node());
    m_localTransform = image->animatedLocalTransform();
    
    // minimum height
    setHeight(errorOccurred() ? intrinsicSize().height() : 0);

    calcWidth();
    calcHeight();

    m_localBounds = FloatRect(image->x().value(image), image->y().value(image), image->width().value(image), image->height().value(image));

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
        SVGResourceFilter* filter = 0;

        PaintInfo savedInfo(paintInfo);

        prepareToRenderSVGContent(this, paintInfo, m_localBounds, filter);

        FloatRect destRect = m_localBounds;
        FloatRect srcRect(0, 0, image()->width(), image()->height());

        SVGImageElement* imageElt = static_cast<SVGImageElement*>(node());
        if (imageElt->preserveAspectRatio()->align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
            adjustRectsForAspectRatio(destRect, srcRect, imageElt->preserveAspectRatio());

        paintInfo.context->drawImage(image(), destRect, srcRect);

        finishRenderSVGContent(this, paintInfo, m_localBounds, filter, savedInfo.context);
    }

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth())
        paintOutline(paintInfo.context, 0, 0, width(), height(), style());

    paintInfo.context->restore();
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
    FloatRect repaintRect = m_localBounds;

    // Filters can paint outside the image content
    repaintRect.unite(filterBoundingBoxForRenderer(this));

    return repaintRect;
}

void RenderSVGImage::imageChanged(WrappedImagePtr image, const IntRect* rect)
{
    RenderImage::imageChanged(image, rect);

    // We override to invalidate a larger rect, since SVG images can draw outside their "bounds"
    repaintRectangle(absoluteClippedOverflowRect());    // FIXME: Isn't this just repaint()?
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

void RenderSVGImage::addFocusRingRects(GraphicsContext* graphicsContext, int, int)
{
    // this is called from paint() after the localTransform has already been applied
    IntRect contentRect = enclosingIntRect(repaintRectInLocalCoordinates());
    graphicsContext->addFocusRingRect(contentRect);
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
