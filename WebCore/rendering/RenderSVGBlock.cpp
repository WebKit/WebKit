/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
    if (useStyle->isDisplayInlineType()) {
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        newStyle->inheritFrom(useStyle.get());
        newStyle->setDisplay(BLOCK);
        useStyle = newStyle.release();
    }

    RenderBlock::setStyle(useStyle.release());
}

void RenderSVGBlock::updateBoxModelInfoFromStyle()
{
    RenderBlock::updateBoxModelInfoFromStyle();

    // RenderSVGlock, used by Render(SVGText|ForeignObject), is not allowed to call setHasOverflowClip(true).
    // RenderBlock assumes a layer to be present when the overflow clip functionality is requested. Both
    // Render(SVGText|ForeignObject) return 'false' on 'requiresLayer'. Fine for RenderSVGText.
    //
    // If we want to support overflow rules for <foreignObject> we can choose between two solutions:
    // a) make RenderForeignObject require layers and SVG layer aware
    // b) reactor overflow logic out of RenderLayer (as suggested by dhyatt), which is a large task
    //
    // Until this is resolved, disable overflow support. Opera/FF don't support it as well at the moment (Feb 2010).
    //
    // Note: This does NOT affect overflow handling on outer/inner <svg> elements - this is handled
    // manually by RenderSVGRoot - which owns the documents enclosing root layer and thus works fine.
    setHasOverflowClip(false);
}

}

#endif
