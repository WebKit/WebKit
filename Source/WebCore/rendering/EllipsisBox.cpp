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
#include "EllipsisBox.h"

#include "Document.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "PaintInfo.h"
#include "RootInlineBox.h"
#include "TextRun.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EllipsisBox);

EllipsisBox::EllipsisBox(RenderBlockFlow& renderer, const AtomString& ellipsisStr, InlineFlowBox* parent, int width, int height, int y, bool firstLine, bool isHorizontal, InlineBox* markupBox)
    : InlineElementBox(renderer, FloatPoint(0, y), width, firstLine, true, false, false, isHorizontal, 0, 0, parent)
    , m_shouldPaintMarkupBox(markupBox)
    , m_height(height)
    , m_str(ellipsisStr)
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_isEverInChildList = false;
#endif
}

void EllipsisBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    GraphicsContext& context = paintInfo.context();
    const RenderStyle& lineStyle = this->lineStyle();
    Color textColor = lineStyle.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor);
    if (textColor != context.fillColor())
        context.setFillColor(textColor);
    bool setShadow = false;
    if (lineStyle.textShadow()) {
        Color shadowColor = lineStyle.colorByApplyingColorFilter(lineStyle.textShadow()->color());
        context.setShadow(LayoutSize(lineStyle.textShadow()->x(), lineStyle.textShadow()->y()), lineStyle.textShadow()->radius(), shadowColor);
        setShadow = true;
    }

    const FontCascade& font = lineStyle.fontCascade();
    if (selectionState() != RenderObject::HighlightState::None) {
        paintSelection(context, paintOffset, lineStyle, font);

        // Select the correct color for painting the text.
        Color foreground = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : blockFlow().selectionForegroundColor();
        if (foreground.isValid() && foreground != textColor)
            context.setFillColor(foreground);
    }

    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    context.drawText(font, RenderBlock::constructTextRun(m_str, lineStyle, AllowRightExpansion), LayoutPoint(x() + paintOffset.x(), y() + paintOffset.y() + lineStyle.fontMetrics().ascent()));

    // Restore the regular fill color.
    if (textColor != context.fillColor())
        context.setFillColor(textColor);

    if (setShadow)
        context.clearShadow();

    paintMarkupBox(paintInfo, paintOffset, lineTop, lineBottom, lineStyle);
}

InlineBox* EllipsisBox::markupBox() const
{
    if (!m_shouldPaintMarkupBox)
        return 0;

    RootInlineBox* lastLine = blockFlow().lineAtIndex(blockFlow().lineCount() - 1);
    if (!lastLine)
        return 0;

    // If the last line-box on the last line of a block is a link, -webkit-line-clamp paints that box after the ellipsis.
    // It does not actually move the link.
    InlineBox* anchorBox = lastLine->lastChild();
    if (!anchorBox || !anchorBox->renderer().style().isLink())
        return 0;

    return anchorBox;
}

void EllipsisBox::paintMarkupBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const RenderStyle& style)
{
    InlineBox* markupBox = this->markupBox();
    if (!markupBox)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset;
    adjustedPaintOffset.move(x() + logicalWidth() - markupBox->x(),
        y() + style.fontMetrics().ascent() - (markupBox->y() + markupBox->lineStyle().fontMetrics().ascent()));
    markupBox->paint(paintInfo, adjustedPaintOffset, lineTop, lineBottom);
}

IntRect EllipsisBox::selectionRect()
{
    const RenderStyle& lineStyle = this->lineStyle();
    const FontCascade& font = lineStyle.fontCascade();
    const RootInlineBox& rootBox = root();
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    LayoutRect selectionRect { LayoutUnit(x()), LayoutUnit(y() + rootBox.selectionTopAdjustedForPrecedingBlock()), 0_lu, rootBox.selectionHeightAdjustedForPrecedingBlock() };
    font.adjustSelectionRectForText(RenderBlock::constructTextRun(m_str, lineStyle, AllowRightExpansion), selectionRect);
    // FIXME: use directional pixel snapping instead.
    return enclosingIntRect(selectionRect);
}

void EllipsisBox::paintSelection(GraphicsContext& context, const LayoutPoint& paintOffset, const RenderStyle& style, const FontCascade& font)
{
    Color textColor = style.visitedDependentColorWithColorFilter(CSSPropertyColor);
    Color c = blockFlow().selectionBackgroundColor();
    if (!c.isVisible())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = c.invertedColorWithAlpha(1.0);

    const RootInlineBox& rootBox = root();
    GraphicsContextStateSaver stateSaver(context);
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    LayoutRect selectionRect { LayoutUnit(x() + paintOffset.x()), LayoutUnit(y() + paintOffset.y() + rootBox.selectionTop()), 0_lu, rootBox.selectionHeight() };
    TextRun run = RenderBlock::constructTextRun(m_str, style, AllowRightExpansion);
    font.adjustSelectionRectForText(run, selectionRect);
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), run.ltr()), c);
}

bool EllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction hitTestAction)
{
    LayoutPoint adjustedLocation = accumulatedOffset + LayoutPoint(topLeft());

    // Hit test the markup box.
    if (InlineBox* markupBox = this->markupBox()) {
        const RenderStyle& lineStyle = this->lineStyle();
        LayoutUnit mtx { adjustedLocation.x() + logicalWidth() - markupBox->x() };
        LayoutUnit mty { adjustedLocation.y() + lineStyle.fontMetrics().ascent() - (markupBox->y() + markupBox->lineStyle().fontMetrics().ascent()) };
        if (markupBox->nodeAtPoint(request, result, locationInContainer, LayoutPoint(mtx, mty), lineTop, lineBottom, hitTestAction)) {
            blockFlow().updateHitTestResult(result, locationInContainer.point() - LayoutSize(mtx, mty));
            return true;
        }
    }

    LayoutRect boundsRect { adjustedLocation, LayoutSize(LayoutUnit(logicalWidth()), m_height) };
    if (visibleToHitTesting() && boundsRect.intersects(HitTestLocation::rectForPoint(locationInContainer.point(), 0, 0, 0, 0))) {
        blockFlow().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(blockFlow().nodeForHitTest(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

} // namespace WebCore
