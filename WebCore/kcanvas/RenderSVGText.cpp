/*
 * This file is part of the KDE project.
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
#include "RenderSVGText.h"

#include "SVGTextElementImpl.h"
#include "SVGAnimatedLengthListImpl.h"
#include "KRenderingDevice.h"
#include "KCanvasMatrix.h"

RenderSVGText::RenderSVGText(KSVG::SVGTextElementImpl *node) 
    : RenderBlock(node)
{
}

void RenderSVGText::translateTopToBaseline()
{
    KRenderingDeviceContext *context = QPainter::renderingDevice()->currentContext();

    int offset = baselinePosition(true, true);
    context->concatCTM(QMatrix().translate(0, -offset));
}

void RenderSVGText::translateForAttributes()
{
    KRenderingDeviceContext *context = QPainter::renderingDevice()->currentContext();

    KSVG::SVGTextElementImpl *text = static_cast<KSVG::SVGTextElementImpl *>(element());

    float xOffset = text->x()->baseVal()->getFirst() ? text->x()->baseVal()->getFirst()->value() : 0;
    float yOffset = text->y()->baseVal()->getFirst() ? text->y()->baseVal()->getFirst()->value() : 0;

    context->concatCTM(QMatrix().translate(xOffset, yOffset));
}

void RenderSVGText::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;

    paintInfo.p->save();

    KRenderingDeviceContext *context = QPainter::renderingDevice()->currentContext();
    context->concatCTM(QMatrix().translate(parentX, parentY));
    translateForAttributes();
    translateTopToBaseline();
    
    RenderBlock::paint(paintInfo, 0, 0);

    paintInfo.p->restore();
}
