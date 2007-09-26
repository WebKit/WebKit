/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
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

#include "RenderSVGInlineText.h"

#include "AffineTransform.h"
#include "GraphicsContext.h"
#include "SVGInlineTextBox.h"
#include "KCanvasRenderingStyle.h"

namespace WebCore {
    
RenderSVGInlineText::RenderSVGInlineText(Node* n, StringImpl* str) 
    : RenderText(n, str)
{
}

void RenderSVGInlineText::absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool)
{
    FloatRect absoluteRect = absoluteTransform().mapRect(FloatRect(tx, ty, width(), height()));
    rects.append(enclosingIntRect(absoluteRect));
}

IntRect RenderSVGInlineText::selectionRect(bool clipToVisibleContent)
{
    IntRect rect = RenderText::selectionRect(clipToVisibleContent);
    rect = parent()->absoluteTransform().mapRect(rect);
    return rect;
}

InlineTextBox* RenderSVGInlineText::createInlineTextBox()
{
    return new (renderArena()) SVGInlineTextBox(this);
}

}

#endif // ENABLE(SVG)
