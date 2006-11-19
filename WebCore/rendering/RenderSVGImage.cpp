/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "RenderSVGImage.h"

#include "Attr.h"
#include "GraphicsContext.h"
#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"
#include "KRenderingDevice.h"
#include "SVGLength.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGImageElement.h"
#include "SVGImageElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

RenderSVGImage::RenderSVGImage(SVGImageElement *impl)
: RenderImage(impl)
{
}

RenderSVGImage::~RenderSVGImage()
{
}

void RenderSVGImage::adjustRectsForAspectRatio(FloatRect& destRect, FloatRect& srcRect, SVGPreserveAspectRatio *aspectRatio)
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
                    destRect.setY(origDestHeight / 2 - destRect.height() / 2);
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
                    destRect.setX(origDestWidth / 2 - destRect.width() / 2);
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
                    srcRect.setY(image()->height() / 2 - srcRect.height() / 2);
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
                    srcRect.setX(image()->width() / 2 - srcRect.width() / 2);
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

void RenderSVGImage::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled() || (paintInfo.phase != PaintPhaseForeground) || style()->visibility() == HIDDEN)
        return;
    
    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext* context = device->currentContext();
    bool shouldPopContext = false;
    if (context)
        paintInfo.context->save();
    else {
        // Need to push a device context on the stack if empty.
        context = paintInfo.context->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    }

    context->concatCTM(AffineTransform().translate(parentX, parentY));
    context->concatCTM(localTransform());
    translateForAttributes();
    
    FloatRect boundingBox = FloatRect(0, 0, width(), height());
    const SVGRenderStyle* svgStyle = style()->svgStyle();

    if (SVGResourceClipper* clipper = getClipperById(document(), svgStyle->clipPath().substring(1)))
        clipper->applyClip(boundingBox);

    if (SVGResourceMasker* masker = getMaskerById(document(), svgStyle->maskElement().substring(1)))
        masker->applyMask(boundingBox);

    SVGResourceFilter* filter = getFilterById(document(), svgStyle->filter().substring(1));
    if (filter)
        filter->prepareFilter(boundingBox);
    
    OwnPtr<GraphicsContext> c(device->currentContext()->createGraphicsContext());

    float opacity = style()->opacity();
    if (opacity < 1.0f) {
        c->clip(enclosingIntRect(boundingBox));
        c->beginTransparencyLayer(opacity);
    }

    PaintInfo pi(paintInfo);
    pi.context = c.get();
    pi.rect = absoluteTransform().invert().mapRect(paintInfo.rect);

    int x = 0, y = 0;
    if (shouldPaint(pi, x, y)) {
        SVGImageElement* imageElt = static_cast<SVGImageElement*>(node());

        if (imageElt->preserveAspectRatio()->align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
            RenderImage::paint(pi, 0, 0);
        else {
            FloatRect destRect(m_x, m_y, contentWidth(), contentHeight());
            FloatRect srcRect(0, 0, image()->width(), image()->height());
            adjustRectsForAspectRatio(destRect, srcRect, imageElt->preserveAspectRatio());
            c->drawImage(image(), destRect, srcRect);
        }

        if (filter)
            filter->applyFilter(boundingBox);
    }
    
    if (opacity < 1.0f)
        c->endTransparencyLayer();

    // restore drawing state
    if (!shouldPopContext)
        paintInfo.context->restore();
    else {
        device->popContext();
        delete context;
    }
}

void RenderSVGImage::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    AffineTransform transform = translationForAttributes() * localTransform();
    r = transform.mapRect(r);
    
    RenderImage::computeAbsoluteRepaintRect(r, f);
}

bool RenderSVGImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    AffineTransform totalTransform = absoluteTransform();
    totalTransform *= translationForAttributes();
    double localX, localY;
    totalTransform.invert().map(_x + _tx, _y + _ty, &localX, &localY);
    return RenderImage::nodeAtPoint(request, result, (int)localX, (int)localY, 0, 0, hitTestAction);
}

bool RenderSVGImage::requiresLayer()
{
    return false;
}

void RenderSVGImage::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = m_absoluteBounds;

    // minimum height
    m_height = cachedImage() && cachedImage() ? intrinsicHeight() : 0;

    calcWidth();
    calcHeight();

    m_absoluteBounds = getAbsoluteRepaintRect();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

FloatRect RenderSVGImage::relativeBBox(bool includeStroke) const
{
    SVGImageElement *image = static_cast<SVGImageElement*>(node());
    return FloatRect(image->x()->value(), image->y()->value(), width(), height());
}

void RenderSVGImage::imageChanged(CachedImage* image)
{
    RenderImage::imageChanged(image);
    // We override to invalidate a larger rect, since SVG images can draw outside their "bounds"
    repaintRectangle(getAbsoluteRepaintRect());
}

IntRect RenderSVGImage::getAbsoluteRepaintRect()
{
    SVGImageElement *image = static_cast<SVGImageElement*>(node());
    FloatRect repaintRect = absoluteTransform().mapRect(FloatRect(image->x()->value(), image->y()->value(), width(), height()));

    // Filters can expand the bounding box
    SVGResourceFilter *filter = getFilterById(document(), style()->svgStyle()->filter().substring(1));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));

    return enclosingIntRect(repaintRect);
}

void RenderSVGImage::absoluteRects(Vector<IntRect>& rects, int tx, int ty)
{
    rects.append(getAbsoluteRepaintRect());
}


AffineTransform RenderSVGImage::translationForAttributes()
{
    SVGImageElement *image = static_cast<SVGImageElement*>(node());
    return AffineTransform().translate(image->x()->value(), image->y()->value());
}

void RenderSVGImage::translateForAttributes()
{
    KRenderingDeviceContext* context = renderingDevice()->currentContext();
    context->concatCTM(translationForAttributes());
}

}

#endif // SVG_SUPPORT
