/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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
#include "SVGInlineFlowBox.h"

#include "GraphicsContext.h"
#include "SVGElementTypeHelpers.h"
#include "SVGInlineTextBox.h"
#include "SVGRenderingContext.h"
#include "SVGTextFragment.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGInlineFlowBox);

void SVGInlineFlowBox::paintSelectionBackground(PaintInfo& paintInfo)
{
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection);
    ASSERT(!paintInfo.context().paintingDisabled());

    PaintInfo childPaintInfo(paintInfo);
    for (auto* child = firstChild(); child; child = child->nextOnLine()) {
        if (auto* textBox = dynamicDowncast<SVGInlineTextBox>(*child))
            textBox->paintSelectionBackground(childPaintInfo);
        else if (auto* flowBox = dynamicDowncast<SVGInlineFlowBox>(*child))
            flowBox->paintSelectionBackground(childPaintInfo);
    }
}

void SVGInlineFlowBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit)
{
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection);
    ASSERT(!paintInfo.context().paintingDisabled());

    SVGRenderingContext renderingContext(renderer(), paintInfo, SVGRenderingContext::SaveGraphicsContext);
    if (renderingContext.isRenderingPrepared()) {
        for (auto* child = firstChild(); child; child = child->nextOnLine())
            child->paint(paintInfo, paintOffset, 0, 0);
    }
}

FloatRect SVGInlineFlowBox::calculateBoundaries() const
{
    FloatRect childRect;
    for (auto* child = firstChild(); child; child = child->nextOnLine()) {
        if (auto* textBox = dynamicDowncast<SVGInlineTextBox>(child)) {
            childRect.unite(textBox->calculateBoundaries());
            continue;
        }
        if (auto* flowBox = dynamicDowncast<SVGInlineFlowBox>(child)) {
            childRect.unite(flowBox->calculateBoundaries());
            continue;
        }
    }
    return childRect;
}

} // namespace WebCore
