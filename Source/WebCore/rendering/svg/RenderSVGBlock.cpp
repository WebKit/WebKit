/*
 * Copyright (C) 2006 Apple Inc.
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
 */

#include "config.h"
#include "RenderSVGBlock.h"

#include "RenderSVGResource.h"
#include "SVGResourcesCache.h"
#include "StyleInheritedData.h"

namespace WebCore {

RenderSVGBlock::RenderSVGBlock(SVGGraphicsElement& element, PassRef<RenderStyle> style)
    : RenderBlockFlow(element, std::move(style))
{
}

LayoutRect RenderSVGBlock::visualOverflowRect() const
{
    LayoutRect borderRect = borderBoxRect();

    if (const ShadowData* textShadow = style().textShadow())
        textShadow->adjustRectForShadow(borderRect);

    return borderRect;
}

void RenderSVGBlock::updateFromStyle()
{
    RenderBlockFlow::updateFromStyle();

    // RenderSVGlock, used by Render(SVGText|ForeignObject), is not allowed to call setHasOverflowClip(true).
    // RenderBlock assumes a layer to be present when the overflow clip functionality is requested. Both
    // Render(SVGText|ForeignObject) return 'false' on 'requiresLayer'. Fine for RenderSVGText.
    //
    // If we want to support overflow rules for <foreignObject> we can choose between two solutions:
    // a) make RenderSVGForeignObject require layers and SVG layer aware
    // b) reactor overflow logic out of RenderLayer (as suggested by dhyatt), which is a large task
    //
    // Until this is resolved, disable overflow support. Opera/FF don't support it as well at the moment (Feb 2010).
    //
    // Note: This does NOT affect overflow handling on outer/inner <svg> elements - this is handled
    // manually by RenderSVGRoot - which owns the documents enclosing root layer and thus works fine.
    setHasOverflowClip(false);
}

void RenderSVGBlock::absoluteRects(Vector<IntRect>&, const LayoutPoint&) const
{
    // This code path should never be taken for SVG, as we're assuming useTransforms=true everywhere, absoluteQuads should be used.
    ASSERT_NOT_REACHED();
}

void RenderSVGBlock::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(*this);
    RenderBlockFlow::willBeDestroyed();
}

void RenderSVGBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (diff == StyleDifferenceLayout)
        setNeedsBoundariesUpdate();
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(*this, diff, style());
}

}
