/*
 * This file is part of the WebKit project.
 *
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
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGHiddenContainer.h"

#include "RenderPath.h"
#include "SVGStyledElement.h"

namespace WebCore {

RenderSVGHiddenContainer::RenderSVGHiddenContainer(SVGStyledElement* element)
    : RenderSVGContainer(element)
{
}

RenderSVGHiddenContainer::~RenderSVGHiddenContainer()
{
}

bool RenderSVGHiddenContainer::requiresLayer()
{
    return false;
}

short RenderSVGHiddenContainer::lineHeight(bool b, bool isRootLineBox) const
{
    return 0;
}

short RenderSVGHiddenContainer::baselinePosition(bool b, bool isRootLineBox) const
{
    return 0;
}

void RenderSVGHiddenContainer::layout()
{
    ASSERT(needsLayout());
 
    // Layout our kids to prevent a kid from being marked as needing layout
    // then never being asked to layout.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (selfNeedsLayout())
            child->setNeedsLayout(true);
        
        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
    }
    
    setNeedsLayout(false);    
}

void RenderSVGHiddenContainer::paint(PaintInfo&, int, int)
{
    // This subtree does not paint.
}

IntRect RenderSVGHiddenContainer::absoluteClippedOverflowRect()
{
    return IntRect();
}

void RenderSVGHiddenContainer::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    // This subtree does not take up space or paint
}

AffineTransform RenderSVGHiddenContainer::absoluteTransform() const
{
    return AffineTransform();
}

AffineTransform RenderSVGHiddenContainer::localTransform() const
{
    return AffineTransform();
}

bool RenderSVGHiddenContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    return false;
}

FloatRect RenderSVGHiddenContainer::relativeBBox(bool includeStroke) const
{
    return FloatRect();
}

}

#endif // ENABLE(SVG)
