/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGInlineFlowBox.h"

#if ENABLE(SVG)
#include "GraphicsContext.h"
#include "SVGRenderSupport.h"

namespace WebCore {

void SVGInlineFlowBox::paint(PaintInfo& paintInfo, int, int)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(!paintInfo.context->paintingDisabled());

    RenderObject* boxRenderer = renderer();
    ASSERT(boxRenderer);

    PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context->save();

    if (SVGRenderSupport::prepareToRenderSVGContent(boxRenderer, childPaintInfo)) {
        for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
            child->paint(childPaintInfo, 0, 0);
    }

    SVGRenderSupport::finishRenderSVGContent(boxRenderer, childPaintInfo, paintInfo.context);
    childPaintInfo.context->restore();
}

IntRect SVGInlineFlowBox::calculateBoundaries() const
{
    IntRect childRect;
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        childRect.unite(child->calculateBoundaries());
    return childRect;
}

} // namespace WebCore

#endif // ENABLE(SVG)
