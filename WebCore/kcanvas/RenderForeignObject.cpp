/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderForeignObject.h"
#include "SVGForeignObjectElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#import "KRenderingDevice.h"
#import "KCanvasMatrix.h"

RenderForeignObject::RenderForeignObject(KSVG::SVGForeignObjectElementImpl *node) 
    : RenderBlock(node)
{
}

QMatrix RenderForeignObject::translationForAttributes()
{
    KSVG::SVGForeignObjectElementImpl *foreign = static_cast<KSVG::SVGForeignObjectElementImpl *>(element());
    return QMatrix().translate(foreign->x()->baseVal()->value(), foreign->y()->baseVal()->value());
}

void RenderForeignObject::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;

    KRenderingDevice *device = QPainter::renderingDevice();
    KRenderingDeviceContext *context = device->currentContext();
    bool shouldPopContext = false;
    if (!context) {
        // Only need to setup for KCanvas rendering if it hasn't already been done.
        context = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    } else
        paintInfo.p->save();

    context->concatCTM(QMatrix().translate(parentX, parentY));
    context->concatCTM(localTransform());
    context->concatCTM(translationForAttributes());
    
    RenderBlock::paint(paintInfo, 0, 0);

    if (shouldPopContext) {
        device->popContext();
        delete context;
    } else
        paintInfo.p->restore();
}

bool RenderForeignObject::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    QMatrix totalTransform = absoluteTransform();
    totalTransform *= translationForAttributes();
    double localX, localY;
    totalTransform.invert().map(_x, _y, &localX, &localY);
    return RenderBlock::nodeAtPoint(info, (int)localX, (int)localY, _tx, _ty, hitTestAction);
}
