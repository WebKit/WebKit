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
#if SVG_SUPPORT
#include "RenderSVGImage.h"

#include "GraphicsContext.h"
#include "KCanvasMaskerQuartz.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "KCanvasRenderingStyle.h"
#include "KCanvasResourcesQuartz.h"
#include "KRenderingDevice.h"
#include "SVGAnimatedLength.h"
#include "SVGImageElement.h"
#include "Attr.h"

#include "SVGImageElement.h"

#include "ksvg.h"

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
    if (aspectRatio->meetOrSlice() == SVG_MEETORSLICE_MEET) {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        if (origDestHeight > (origDestWidth * widthToHeightMultiplier)) {
            destRect.setHeight(origDestWidth * widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMINYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                    destRect.setY(origDestHeight / 2 - destRect.height() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setY(origDestHeight - destRect.height());
                    break;
            }
        }
        if (origDestWidth > (origDestHeight / widthToHeightMultiplier)) {
            destRect.setWidth(origDestHeight / widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    destRect.setX(origDestWidth / 2 - destRect.width() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setX(origDestWidth - destRect.width());
                    break;
            }
        }
    } else if (aspectRatio->meetOrSlice() == SVG_MEETORSLICE_SLICE) {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        // if the destination height is less than the height of the image we'll be drawing
        if (origDestHeight < (origDestWidth * widthToHeightMultiplier)) {
            float destToSrcMultiplier = srcRect.width() / destRect.width();
            srcRect.setHeight(destRect.height() * destToSrcMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMINYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                    srcRect.setY(image()->height() / 2 - srcRect.height() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setY(image()->height() - srcRect.height());
                    break;
            }
        }
        // if the destination width is less than the width of the image we'll be drawing
        if (origDestWidth < (origDestHeight / widthToHeightMultiplier)) {
            float destToSrcMultiplier = srcRect.height() / destRect.height();
            srcRect.setWidth(destRect.width() * destToSrcMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    srcRect.setX(image()->width() / 2 - srcRect.width() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setX(image()->width() - srcRect.width());
                    break;
            }
        }
    }
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintPhaseForeground) || style()->visibility() == HIDDEN)
        return;
    
    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext* context = device->currentContext();
    bool shouldPopContext = false;
    if (!context) {
        // Need to push a device context on the stack if empty.
        context = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    } else
        paintInfo.p->save();

    context->concatCTM(QMatrix().translate(parentX, parentY));
    context->concatCTM(localTransform());
    translateForAttributes();
    
    FloatRect boundingBox = relativeBBox(true);
    const SVGRenderStyle *svgStyle = style()->svgStyle();
            
    if (KCanvasClipper *clipper = getClipperById(document(), svgStyle->clipPath().mid(1)))
        clipper->applyClip(boundingBox);

    if (KCanvasMasker *masker = getMaskerById(document(), svgStyle->maskElement().mid(1)))
        masker->applyMask(boundingBox);

    KCanvasFilter *filter = getFilterById(document(), svgStyle->filter().mid(1));
    if (filter)
        filter->prepareFilter(boundingBox);
    
    int x = 0, y = 0;
    if (!shouldPaint(paintInfo, x, y))
        return;
        
    SVGImageElement *imageElt = static_cast<SVGImageElement *>(node());
        
    if (imageElt->preserveAspectRatio()->baseVal()->align() == SVG_PRESERVEASPECTRATIO_NONE)
        RenderImage::paint(paintInfo, 0, 0);
    else {
        FloatRect destRect(m_x, m_y, contentWidth(), contentHeight());
        FloatRect srcRect(0, 0, image()->width(), image()->height());
        adjustRectsForAspectRatio(destRect, srcRect, imageElt->preserveAspectRatio()->baseVal());
        paintInfo.p->drawImage(image(), destRect,
                                    srcRect.x(), srcRect.y(), srcRect.width(), srcRect.height(), 
                                    Image::CompositeSourceOver);
    }

    if (filter)
        filter->applyFilter(boundingBox);
    
    // restore drawing state
    if (shouldPopContext) {
        device->popContext();
        delete context;
    } else
        paintInfo.p->restore();
}

FloatRect RenderSVGImage::relativeBBox(bool includeStroke) const
{
    return FloatRect(0, 0, width(), height());
}

void RenderSVGImage::imageChanged(CachedImage* image)
{
    RenderImage::imageChanged(image);
    // We override to invalidate a larger rect, since SVG images can draw outside their "bounds"
    repaintRectangle(getAbsoluteRepaintRect());
}

IntRect RenderSVGImage::getAbsoluteRepaintRect()
{
    FloatRect repaintRect = absoluteTransform().mapRect(relativeBBox(true));

    // Filters can expand the bounding box
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));

    return enclosingIntRect(repaintRect);
}

void RenderSVGImage::translateForAttributes()
{
    KRenderingDeviceContext *context = renderingDevice()->currentContext();
    SVGImageElement *image = static_cast<SVGImageElement *>(node());
    float xOffset = image->x()->baseVal() ? image->x()->baseVal()->value() : 0;
    float yOffset = image->y()->baseVal() ? image->y()->baseVal()->value() : 0;
    context->concatCTM(QMatrix().translate(xOffset, yOffset));
}

}

#endif // SVG_SUPPORT
