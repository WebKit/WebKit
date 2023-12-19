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
#include "LegacyRenderSVGHiddenContainer.h"

#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGHiddenContainer);

LegacyRenderSVGHiddenContainer::LegacyRenderSVGHiddenContainer(Type type, SVGElement& element, RenderStyle&& style, OptionSet<SVGModelObjectFlag> svgFlags)
    : LegacyRenderSVGContainer(type, element, WTFMove(style), svgFlags | SVGModelObjectFlag::IsHiddenContainer)
{
}

void LegacyRenderSVGHiddenContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    SVGRenderSupport::layoutChildren(*this, selfNeedsLayout());
    clearNeedsLayout();
}

void LegacyRenderSVGHiddenContainer::paint(PaintInfo&, const LayoutPoint&)
{
    // This subtree does not paint.
}

void LegacyRenderSVGHiddenContainer::absoluteQuads(Vector<FloatQuad>&, bool*) const
{
    // This subtree does not take up space or paint
}

bool LegacyRenderSVGHiddenContainer::nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint&, HitTestAction)
{
    return false;
}

}
