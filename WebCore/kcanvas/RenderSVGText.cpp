/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *               2006 Alexander Kellett <lypanov@kde.org>
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
#include "RenderSVGText.h"

#include "render_object.h"
#include "SVGTextElementImpl.h"
#include "SVGAnimatedLengthListImpl.h"
#include "KRenderingDevice.h"
#include "KCanvasMatrix.h"
#include "KCanvasRenderingStyle.h"

RenderSVGText::RenderSVGText(KSVG::SVGTextElementImpl *node) 
    : RenderBlock(node)
{
}

QMatrix RenderSVGText::translationTopToBaseline()
{
    int offset = baselinePosition(true, true);
    return QMatrix().translate(0, -offset);
}

QMatrix RenderSVGText::translationForAttributes()
{
    KSVG::SVGTextElementImpl *text = static_cast<KSVG::SVGTextElementImpl *>(element());

    float xOffset = text->x()->baseVal()->getFirst() ? text->x()->baseVal()->getFirst()->value() : 0;
    float yOffset = text->y()->baseVal()->getFirst() ? text->y()->baseVal()->getFirst()->value() : 0;

    return QMatrix().translate(xOffset, yOffset);
}

void RenderSVGText::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
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
    
    context->concatCTM(localTransform());
    context->concatCTM(QMatrix().translate(parentX, parentY));
    context->concatCTM(translationForAttributes());
    context->concatCTM(translationTopToBaseline());
    
    FloatRect boundingBox(0, 0, width(), height());
    const KSVG::SVGRenderStyle *svgStyle = style()->svgStyle();
            
    if (KCanvasClipper *clipper = getClipperById(document(), svgStyle->clipPath().mid(1)))
        clipper->applyClip(boundingBox);

    if (KCanvasMasker *masker = getMaskerById(document(), svgStyle->maskElement().mid(1)))
        masker->applyMask(boundingBox);

    KCanvasFilter *filter = getFilterById(document(), svgStyle->filter().mid(1));
    if (filter)
        filter->prepareFilter(boundingBox);
        
    KRenderingPaintServer *fillPaintServer = KSVG::KSVGPainterFactory::fillPaintServer(style(), this);
    if (fillPaintServer) {
        fillPaintServer->setPaintingText(true);
        // fillPaintServer->setActiveClient(this);
        if (fillPaintServer->setup(context, this, APPLY_TO_FILL)) {
            RenderBlock::paint(paintInfo, 0, 0);
            fillPaintServer->teardown(context, this, APPLY_TO_FILL);
        }
        fillPaintServer->setPaintingText(false);
    }
    
    KRenderingPaintServer *strokePaintServer = KSVG::KSVGPainterFactory::strokePaintServer(style(), this);
    if (strokePaintServer) {
        strokePaintServer->setPaintingText(true);
        // strokePaintServer->setActiveClient(this);
        if (strokePaintServer->setup(context, this, APPLY_TO_STROKE)) {
            RenderBlock::paint(paintInfo, 0, 0);
            strokePaintServer->teardown(context, this, APPLY_TO_STROKE);
        }
        strokePaintServer->setPaintingText(false);
    }

    if (filter)
        filter->applyFilter(boundingBox);

    // restore drawing state
    if (shouldPopContext) {
        renderingDevice->popContext();
        delete context;
    } else
        paintInfo.p->restore();
}

bool RenderSVGText::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, WebCore::HitTestAction hitTestAction)
{
    QMatrix totalTransform = absoluteTransform();
    totalTransform *= localTransform();
    totalTransform *= translationForAttributes();
    totalTransform *= translationTopToBaseline();
    double localX, localY;
    totalTransform.invert().map(_x, _y, &localX, &localY);
    return RenderBlock::nodeAtPoint(info, (int)localX, (int)localY, _tx, _ty, hitTestAction);
}
