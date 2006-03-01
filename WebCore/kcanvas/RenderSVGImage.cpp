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
#include "KCanvasRenderingStyle.h"
#include "KCanvasResourcesQuartz.h"
#include "KRenderingDevice.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGImageElementImpl.h"
#include <kcanvas/KCanvas.h>
#include <kdom/core/AttrImpl.h>

namespace WebCore {

RenderSVGImage::RenderSVGImage(SVGImageElementImpl *impl)
: RenderImage(impl)
{
}

RenderSVGImage::~RenderSVGImage()
{
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintActionForeground) || style()->visibility() == HIDDEN)
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
    const KSVG::SVGRenderStyle *svgStyle = style()->svgStyle();
            
    if (KCanvasClipper *clipper = getClipperById(document(), svgStyle->clipPath().mid(1)))
        clipper->applyClip(boundingBox);

    if (KCanvasMasker *masker = getMaskerById(document(), svgStyle->maskElement().mid(1)))
        masker->applyMask(boundingBox);

    KCanvasFilter *filter = getFilterById(document(), svgStyle->filter().mid(1));
    if (filter)
        filter->prepareFilter(boundingBox);
    
    RenderImage::paint(paintInfo, 0, 0);
    
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
    SVGImageElementImpl *image = static_cast<SVGImageElementImpl *>(node());
    float xOffset = image->x()->baseVal() ? image->x()->baseVal()->value() : 0;
    float yOffset = image->y()->baseVal() ? image->y()->baseVal()->value() : 0;
    context->concatCTM(QMatrix().translate(xOffset, yOffset));
}

}

#endif // SVG_SUPPORT
