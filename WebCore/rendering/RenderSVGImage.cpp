/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2007 Rob Buis <buis@kde.org>

    This file is part of the WebKit project

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
#include "GraphicsContext.h"
#include "PointerEventsHitRules.h"
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

bool RenderSVGImage::calculateLocalTransform()
{
    AffineTransform oldTransform = m_localTransform;
    m_localTransform = static_cast<SVGStyledTransformableElement*>(element())->animatedLocalTransform();
    return (m_localTransform != oldTransform);
}

void RenderSVGImage::layout()
{
    ASSERT(needsLayout());
    
    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint) {
        oldBounds = absoluteClippedOverflowRect();
        oldOutlineBox = absoluteOutlineBox();
    }
    
    calculateLocalTransform();
    
    // minimum height
    m_height = errorOccurred() ? intrinsicSize().height() : 0;

    calcWidth();
    calcHeight();

    SVGImageElement* image = static_cast<SVGImageElement*>(node());
    m_localBounds = FloatRect(image->x().value(), image->y().value(), image->width().value(), image->height().value());

    calculateAbsoluteBounds();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);

    setNeedsLayout(false);
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled() || style()->visibility() == HIDDEN)
        return;

    paintInfo.context->save();
    paintInfo.context->concatCTM(localTransform());

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
    
    paintInfo.context->restore();
}

bool RenderSVGImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int, int, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, style()->svgStyle()->pointerEvents());
    
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        double localX, localY;
        absoluteTransform().inverse().map(_x, _y, &localX, &localY);

        if (hitRules.canHitFill) {
            if (m_localBounds.contains(narrowPrecisionToFloat(localX), narrowPrecisionToFloat(localY))) {
                updateHitTestResult(result, IntPoint(_x, _y));
                return true;
            }
        }
    }

    return false;
}

bool RenderSVGImage::requiresLayer()
{
    return false;
}

FloatRect RenderSVGImage::relativeBBox(bool) const
{
    return m_localBounds;
}

void RenderSVGImage::imageChanged(CachedImage* image)
{
    RenderImage::imageChanged(image);

    // We override to invalidate a larger rect, since SVG images can draw outside their "bounds"
    repaintRectangle(absoluteClippedOverflowRect());
}

void RenderSVGImage::calculateAbsoluteBounds()
{
    FloatRect absoluteRect = absoluteTransform().mapRect(relativeBBox(true));

#if ENABLE(SVG_FILTERS)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        absoluteRect.unite(filter->filterBBoxForItemBBox(absoluteRect));
#endif

    if (!absoluteRect.isEmpty())
        absoluteRect.inflate(1); // inflate 1 pixel for antialiasing

    m_absoluteBounds = enclosingIntRect(absoluteRect);
}

IntRect RenderSVGImage::absoluteClippedOverflowRect()
{
    return m_absoluteBounds;
}

void RenderSVGImage::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    // this is called from paint() after the localTransform has already been applied
    IntRect contentRect = enclosingIntRect(relativeBBox());
    graphicsContext->addFocusRingRect(contentRect);
}

void RenderSVGImage::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(absoluteClippedOverflowRect());
}

}

#endif // ENABLE(SVG)
