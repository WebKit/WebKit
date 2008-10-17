/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGBlock.h"

#include "SVGElement.h"

namespace WebCore {

RenderSVGBlock::RenderSVGBlock(SVGElement* node) 
    : RenderBlock(node)
{
}

void RenderSVGBlock::setStyle(PassRefPtr<RenderStyle> style) 
{
    RefPtr<RenderStyle> useStyle = style;

    // SVG text layout code expects us to be a block-level style element.   
    if (useStyle->display() == NONE)
        setChildrenInline(false);
    else if (useStyle->isDisplayInlineType()) {
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        newStyle->inheritFrom(useStyle.get());
        newStyle->setDisplay(BLOCK);
        useStyle = newStyle.release();
    }

    RenderBlock::setStyle(useStyle.release());
    setReplaced(false);

    //FIXME: Once overflow rules are supported by SVG we should
    //probably map the CSS overflow rules rather than just ignoring
    //them
    setHasOverflowClip(false);
}

}

#endif // ENABLE(SVG)
