/**
 * Copyright (C) 2003, 2006 Apple Inc.
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
#include "LegacyEllipsisBox.h"

#include "Document.h"
#include "EllipsisBoxPainter.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "PaintInfo.h"
#include "RenderElementInlines.h"
#include "TextRun.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyEllipsisBox);

LegacyEllipsisBox::LegacyEllipsisBox(RenderBlockFlow& renderer, const AtomString& ellipsisStr, LegacyInlineFlowBox* parent, int width, int height, int y, bool firstLine, bool isHorizontal, LegacyInlineBox* markupBox)
    : LegacyInlineElementBox(renderer, FloatPoint(0, y), width, firstLine, true, false, false, isHorizontal, 0, 0, parent)
    , m_shouldPaintMarkupBox(markupBox)
    , m_height(height)
    , m_str(ellipsisStr)
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_isEverInChildList = false;
#endif
}

void LegacyEllipsisBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    auto lineBox = InlineIterator::LineBox { InlineIterator::LineBoxIteratorLegacyPath { &root() } };
    EllipsisBoxPainter { lineBox, paintInfo, paintOffset, blockFlow().selectionForegroundColor(), blockFlow().selectionBackgroundColor() }.paint();
    paintMarkupBox(paintInfo, paintOffset, lineTop, lineBottom, lineStyle());
}

LegacyInlineBox* LegacyEllipsisBox::markupBox() const
{
    if (!m_shouldPaintMarkupBox)
        return 0;

    LegacyRootInlineBox* lastLine = blockFlow().lastRootBox();
    if (!lastLine)
        return 0;

    // If the last line-box on the last line of a block is a link, -webkit-line-clamp paints that box after the ellipsis.
    // It does not actually move the link.
    LegacyInlineBox* anchorBox = lastLine->lastLeafDescendant();
    if (!anchorBox || !anchorBox->renderer().style().isLink())
        return 0;

    return anchorBox;
}

void LegacyEllipsisBox::paintMarkupBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const RenderStyle& style)
{
    LegacyInlineBox* markupBox = this->markupBox();
    if (!markupBox)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset;
    adjustedPaintOffset.move(x() + logicalWidth() - markupBox->x(),
        y() + style.metricsOfPrimaryFont().ascent() - (markupBox->y() + markupBox->lineStyle().metricsOfPrimaryFont().ascent()));
    markupBox->paint(paintInfo, adjustedPaintOffset, lineTop, lineBottom);
}

IntRect LegacyEllipsisBox::selectionRect() const
{
    const RenderStyle& lineStyle = this->lineStyle();
    const FontCascade& font = lineStyle.fontCascade();
    const LegacyRootInlineBox& rootBox = root();
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    LayoutRect selectionRect { LayoutUnit(x()), LayoutUnit(y() + rootBox.selectionTopAdjustedForPrecedingBlock()), 0_lu, rootBox.selectionHeightAdjustedForPrecedingBlock() };
    font.adjustSelectionRectForText(RenderBlock::constructTextRun(m_str, lineStyle, ExpansionBehavior::allowRightOnly()), selectionRect);
    // FIXME: use directional pixel snapping instead.
    return enclosingIntRect(selectionRect);
}

bool LegacyEllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction hitTestAction)
{
    LayoutPoint adjustedLocation = accumulatedOffset + LayoutPoint(topLeft());

    // Hit test the markup box.
    if (LegacyInlineBox* markupBox = this->markupBox()) {
        const RenderStyle& lineStyle = this->lineStyle();
        LayoutUnit mtx { adjustedLocation.x() + logicalWidth() - markupBox->x() };
        LayoutUnit mty { adjustedLocation.y() + lineStyle.metricsOfPrimaryFont().ascent() - (markupBox->y() + markupBox->lineStyle().metricsOfPrimaryFont().ascent()) };
        if (markupBox->nodeAtPoint(request, result, locationInContainer, LayoutPoint(mtx, mty), lineTop, lineBottom, hitTestAction)) {
            blockFlow().updateHitTestResult(result, locationInContainer.point() - LayoutSize(mtx, mty));
            return true;
        }
    }

    auto boundsRect = LayoutRect { adjustedLocation, LayoutSize(LayoutUnit(logicalWidth()), m_height) };
    if (renderer().visibleToHitTesting(request) && locationInContainer.intersects(boundsRect)) {
        blockFlow().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(blockFlow().nodeForHitTest(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

TextRun LegacyEllipsisBox::createTextRun() const
{
    return RenderBlock::constructTextRun(m_str, lineStyle(), ExpansionBehavior::allowRightOnly());
}

} // namespace WebCore
