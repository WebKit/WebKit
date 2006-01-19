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

#include <kdom/core/AttrImpl.h>

#include "RenderSVGImage.h"

#include "SVGAnimatedLengthImpl.h"
#include "KCanvasRenderingStyle.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGImageElementImpl.h"

#include "KCanvasResourcesQuartz.h"
#include "KCanvasMaskerQuartz.h"

using namespace KSVG;

RenderSVGImage::RenderSVGImage(SVGImageElementImpl *impl)
: RenderImage(impl)
{
}

RenderSVGImage::~RenderSVGImage()
{
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintActionForeground) || style()->visibility() == khtml::HIDDEN)
        return;
    
    KRenderingDevice *renderingDevice = QPainter::renderingDevice();
    KRenderingDeviceContext *context = renderingDevice->currentContext();
    bool shouldPopContext = false;
    if (!context) {
        // Need to push a device context on the stack if empty.
        context = paintInfo.p->createRenderingDeviceContext();
        renderingDevice->pushContext(context);
        shouldPopContext = true;
    } else
        paintInfo.p->save();

    context->concatCTM(QMatrix().translate(parentX, parentY));
    context->concatCTM(localTransform());
    translateForAttributes();
    
    FloatRect boundingBox(0, 0, width(), height());
    const KSVG::SVGRenderStyle *svgStyle = style()->svgStyle();
            
    if (KCanvasClipper *clipper = getClipperById(document(), svgStyle->clipPath().mid(1)))
        clipper->applyClip(boundingBox);

    if (KCanvasMasker *masker = getMaskerById(document(), svgStyle->maskElement().mid(1)))
        masker->applyMask(boundingBox);

    KCanvasFilter *filter = getFilterById(document(), svgStyle->filter().mid(1));
    if (filter)
        filter->prepareFilter(boundingBox);
    
    khtml::RenderImage::paint(paintInfo, 0, 0);
    
    if (filter)
        filter->applyFilter(boundingBox);

    // restore drawing state
    if (shouldPopContext) {
        renderingDevice->popContext();
        delete context;
    } else
        paintInfo.p->restore();
}

void RenderSVGImage::translateForAttributes()
{
    KRenderingDeviceContext *context = QPainter::renderingDevice()->currentContext();
    SVGImageElementImpl *image = static_cast<SVGImageElementImpl *>(node());
    float xOffset = image->x()->baseVal() ? image->x()->baseVal()->value() : 0;
    float yOffset = image->y()->baseVal() ? image->y()->baseVal()->value() : 0;
    context->concatCTM(QMatrix().translate(xOffset, yOffset));
}
