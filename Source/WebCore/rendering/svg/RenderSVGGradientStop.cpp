/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "RenderSVGGradientStop.h"

#include "ElementInlines.h"
#include "RenderSVGGradientStopInlines.h"
#include "RenderSVGResourceContainer.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGradientElement.h"
#include "SVGNames.h"
#include "SVGResourcesCache.h"
#include "SVGStopElement.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {
    
using namespace SVGNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGGradientStop);

RenderSVGGradientStop::RenderSVGGradientStop(SVGStopElement& element, RenderStyle&& style)
    : RenderElement(element, WTFMove(style), 0)
{
}

RenderSVGGradientStop::~RenderSVGGradientStop() = default;

void RenderSVGGradientStop::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderElement::styleDidChange(diff, oldStyle);
    if (diff == StyleDifference::Equal)
        return;

    // <stop> elements should only be allowed to make renderers under gradient elements
    // but I can imagine a few cases we might not be catching, so let's not crash if our parent isn't a gradient.
    const auto* gradient = gradientElement();
    if (!gradient)
        return;

    RenderElement* renderer = gradient->renderer();
    if (!renderer)
        return;

    downcast<RenderSVGResourceContainer>(*renderer).removeAllClientsFromCache();
}

void RenderSVGGradientStop::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    clearNeedsLayout();
}

SVGGradientElement* RenderSVGGradientStop::gradientElement()
{
    if (is<SVGGradientElement>(element().parentElement()))
        return downcast<SVGGradientElement>(element().parentElement());
    return nullptr;
}

}
