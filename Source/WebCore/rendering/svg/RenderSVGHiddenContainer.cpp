/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (c) 2022 Igalia S.L.
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
#include "RenderSVGHiddenContainer.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderLayer.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGHiddenContainer);

RenderSVGHiddenContainer::RenderSVGHiddenContainer(SVGElement& element, RenderStyle&& style)
    : RenderSVGContainer(element, WTFMove(style))
{
}

void RenderSVGHiddenContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    layoutChildren();
    clearNeedsLayout();    
}

void RenderSVGHiddenContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderSVGContainer::styleDidChange(diff, oldStyle);

    // Ensure that descendants with layers are rooted within our layer.
    if (hasLayer())
        layer()->setIsOpportunisticStackingContext(true);
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
