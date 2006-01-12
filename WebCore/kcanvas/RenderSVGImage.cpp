/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>

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
    if (paintInfo.p->paintingDisabled())
        return;
    
    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintActionForeground) || style()->visibility() == khtml::HIDDEN)
        return;

    paintInfo.p->save();

    KRenderingDeviceContext *context = QPainter::renderingDevice()->currentContext();
    context->concatCTM(QMatrix().translate(parentX, parentY));
    translateForAttributes();
    
    // FIXME: All Quartz dependencies should be removed from this method!
    CGContextRef cgContext = paintInfo.p->currentContext();
    QRectF boundingBox(0, 0, width(), height());
    
    QString clipname = style()->svgStyle()->clipPath().mid(1);
    KCanvasClipperQuartz *clipper = static_cast<KCanvasClipperQuartz *>(getClipperById(document(), clipname));
    if (clipper)
        clipper->applyClip(cgContext, CGRect(boundingBox));

    QString maskname = style()->svgStyle()->maskElement().mid(1);
    KCanvasMaskerQuartz *masker = static_cast<KCanvasMaskerQuartz *>(getMaskerById(document(), maskname));
    if (masker)
        masker->applyMask(cgContext, CGRect(boundingBox));

    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter)
        filter->prepareFilter(QPainter::renderingDevice(), boundingBox);
    
    khtml::RenderImage::paint(paintInfo, 0, 0);
    
    if (filter)
        filter->applyFilter(QPainter::renderingDevice(), boundingBox);

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
