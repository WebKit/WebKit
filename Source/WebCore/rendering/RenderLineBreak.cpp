/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
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
#include "RenderLineBreak.h"

#include "Document.h"
#include "FontMetrics.h"
#include "HTMLElement.h"
#include "HTMLWBRElement.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorSVGTextBox.h"
#include "InlineRunAndOffset.h"
#include "LineSelection.h"
#include "LogicalSelectionOffsetCachesInlines.h"
#include "RenderBlock.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGInlineTextBox.h"
#include "SelectionGeometry.h"
#include "VisiblePosition.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderLineBreak);

RenderLineBreak::RenderLineBreak(HTMLElement& element, RenderStyle&& style)
    : RenderBoxModelObject(Type::LineBreak, element, WTF::move(style), { }, is<HTMLWBRElement>(element) ? OptionSet<LineBreakFlag> { LineBreakFlag::IsWBR } : OptionSet<LineBreakFlag> { })
{
    ASSERT(isRenderLineBreak());
}

RenderLineBreak::~RenderLineBreak()
{
}

int RenderLineBreak::caretMinOffset() const
{
    return 0;
}

int RenderLineBreak::caretMaxOffset() const
{ 
    return 1;
}

bool RenderLineBreak::canBeSelectionLeaf() const
{
    return true;
}

PositionWithAffinity RenderLineBreak::positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*)
{
    return createPositionWithAffinity(0, Affinity::Downstream);
}

IntRect RenderLineBreak::linesBoundingBox() const
{
    auto run = InlineIterator::boxFor(*this);
    if (!run)
        return { };

    return enclosingIntRect(run->visualRectIgnoringBlockDirection());
}

void RenderLineBreak::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    auto box = InlineIterator::boxFor(*this);
    if (!box)
        return;

    auto rect = LayoutRect { box->visualRectIgnoringBlockDirection() };
    rect.moveBy(accumulatedOffset);
    rects.append(rect);
}

void RenderLineBreak::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    auto box = InlineIterator::boxFor(*this);
    if (!box)
        return;

    auto rect = box->visualRectIgnoringBlockDirection();
    quads.append(localToAbsoluteQuad(FloatRect(rect.location(), rect.size()), UseTransforms, wasFixed));
}

void RenderLineBreak::updateFromStyle()
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(isInline());
}

void RenderLineBreak::collectSelectionGeometries(Vector<SelectionGeometry>& rects, unsigned, unsigned)
{
    auto run = InlineIterator::boxFor(*this);

    if (!run)
        return;
    auto lineBox = run->lineBox();

    auto lineSelectionRect = LineSelection::logicalRect(*lineBox);
    LayoutRect rect = IntRect(run->logicalLeftIgnoringInlineDirection(), lineSelectionRect.y(), 0, lineSelectionRect.height());
    if (!lineBox->isHorizontal())
        rect = rect.transposedRect();

    if (lineBox->isFirstAfterPageBreak()) {
        if (run->isHorizontal())
            rect.shiftYEdgeTo(lineBox->logicalTop());
        else
            rect.shiftXEdgeTo(lineBox->logicalTop());
    }

    // FIXME: Out-of-flow positioned line breaks do not follow normal containing block chain.
    CheckedPtr containingBlock = RenderObject::containingBlockForPositionType(PositionType::Static, *this);
    // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
    LogicalSelectionOffsetCaches cache(*containingBlock);
    LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
    LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
    LayoutRect extentsRect = rect;
    if (run->isHorizontal()) {
        extentsRect.setX(leftOffset);
        extentsRect.setWidth(rightOffset - leftOffset);
    } else {
        extentsRect.setY(leftOffset);
        extentsRect.setHeight(rightOffset - leftOffset);
    }
    extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
    if (!run->isHorizontal())
        extentsRect = extentsRect.transposedRect();
    bool isFirstOnLine = !run->nextLineLeftwardOnLine();
    bool isLastOnLine = !run->nextLineRightwardOnLine();

    bool isFixed = false;
    auto absoluteQuad = localToAbsoluteQuad(FloatRect(rect), UseTransforms, &isFixed);
    bool boxIsHorizontal = !is<InlineIterator::SVGTextBoxIterator>(run) ? run->isHorizontal() : !writingMode().isVertical();

    rects.append(SelectionGeometry(absoluteQuad, HTMLElement::selectionRenderingBehavior(WTF::protect(element())), run->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, run->isLineBreak(), isFirstOnLine, isLastOnLine, false, false, boxIsHorizontal, isFixed, checkedView()->pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x())));
}

} // namespace WebCore
