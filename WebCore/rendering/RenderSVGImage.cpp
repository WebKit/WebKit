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
                    destRect.setY(origDestHeight / 2.0f - destRect.height() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setY(origDestHeight - destRect.height());
                    break;
            }
        }
        if (origDestWidth > (origDestHeight / widthToHeightMultiplier)) {
            destRect.setWidth(origDestHeight / widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    destRect.setX(origDestWidth / 2.0f - destRect.width() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setX(origDestWidth - destRect.width());
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
                    srcRect.setY(image()->height() / 2.0f - srcRect.height() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setY(image()->height() - srcRect.height());
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
                    srcRect.setX(image()->width() / 2.0f - srcRect.width() / 2.0f);
                    break;
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setX(image()->width() - srcRect.width());
                    break;
            }
        }
    }
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled() || style()->visibility() == HIDDEN)
        return;

    paintInfo.context->save();
    paintInfo.context->concatCTM(localTransform());

    if (paintInfo.phase == PaintPhaseForeground) {
        SVGResourceFilter* filter = 0;

        AffineTransform imageCtm(translationForAttributes());
        PaintInfo savedInfo(paintInfo);

        FloatRect boundingBox = FloatRect(imageCtm.e(), imageCtm.f(), m_imageWidth, m_imageHeight);
        prepareToRenderSVGContent(this, paintInfo, boundingBox, filter);

        SVGImageElement* imageElt = static_cast<SVGImageElement*>(node());

        FloatRect destRect(m_x, m_y, m_imageWidth, m_imageHeight);
        FloatRect srcRect(0, 0, image()->width(), image()->height());

        if (imageElt->preserveAspectRatio()->align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
            adjustRectsForAspectRatio(destRect, srcRect, imageElt->preserveAspectRatio());

        paintInfo.context->concatCTM(imageCtm);
        paintInfo.context->drawImage(image(), destRect, srcRect);

        finishRenderSVGContent(this, paintInfo, boundingBox, filter, savedInfo.context);
    }
    
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth())
        paintOutline(paintInfo.context, 0, 0, width(), height(), style());

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
        translationForAttributes().inverse().map(localX, localY, &localX, &localY);

        if (hitRules.canHitFill) {
            if (FloatRect(0.0f, 0.0f, m_width, m_height).contains(narrowPrecisionToFloat(localX), narrowPrecisionToFloat(localY))) {
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

FloatRect RenderSVGImage::relativeBBox(bool includeStroke) const
{
    SVGImageElement* image = static_cast<SVGImageElement*>(node());
    return FloatRect(image->x().value(), image->y().value(), width(), height());
}

void RenderSVGImage::imageChanged(CachedImage* image)
{
    RenderImage::imageChanged(image);

    // We override to invalidate a larger rect, since SVG images can draw outside their "bounds"
    repaintRectangle(absoluteClippedOverflowRect());
}

IntRect RenderSVGImage::absoluteClippedOverflowRect()
{
    FloatRect repaintRect = absoluteTransform().mapRect(relativeBBox(true));

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));
#endif

    if (!repaintRect.isEmpty())
        repaintRect.inflate(1); // inflate 1 pixel for antialiasing

    return enclosingIntRect(repaintRect);
}

void RenderSVGImage::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    graphicsContext->addFocusRingRect(m_absoluteBounds);
}

void RenderSVGImage::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(absoluteClippedOverflowRect());
}

AffineTransform RenderSVGImage::translationForAttributes()
{
    SVGImageElement* image = static_cast<SVGImageElement*>(node());
    return AffineTransform().translate(image->x().value(), image->y().value());
}

void RenderSVGImage::calcWidth()
{
    RenderImage::calcWidth();
    SVGImageElement* image = static_cast<SVGImageElement*>(node());
    m_imageWidth = image->width().value();
}

void RenderSVGImage::calcHeight()
{
    RenderImage::calcHeight();
    SVGImageElement* image = static_cast<SVGImageElement*>(node());
    m_imageHeight = image->height().value();
}

}

#endif // ENABLE(SVG)
