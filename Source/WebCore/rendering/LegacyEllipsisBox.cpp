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
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "PaintInfo.h"
#include "TextRun.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyEllipsisBox);

LegacyEllipsisBox::LegacyEllipsisBox(RenderBlockFlow& renderer, const AtomString& ellipsisStr, LegacyInlineFlowBox* parent, int width, int height, int y, bool firstLine, bool isHorizontal)
    : LegacyInlineElementBox(renderer, FloatPoint(0, y), width, firstLine, true, false, false, isHorizontal, 0, 0, parent)
    , m_height(height)
    , m_str(ellipsisStr)
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_isEverInChildList = false;
#endif
}

void LegacyEllipsisBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit)
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
}

IntRect LegacyEllipsisBox::selectionRect()
{
    const RenderStyle& lineStyle = this->lineStyle();
    const FontCascade& font = lineStyle.fontCascade();
    const LegacyRootInlineBox& rootBox = root();
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    LayoutRect selectionRect { LayoutUnit(x()), LayoutUnit(y() + rootBox.selectionTopAdjustedForPrecedingBlock()), 0_lu, rootBox.selectionHeightAdjustedForPrecedingBlock() };
    font.adjustSelectionRectForText(RenderBlock::constructTextRun(m_str, lineStyle, AllowRightExpansion), selectionRect);
    // FIXME: use directional pixel snapping instead.
    return enclosingIntRect(selectionRect);
}

void LegacyEllipsisBox::paintSelection(GraphicsContext& context, const LayoutPoint& paintOffset, const RenderStyle& style, const FontCascade& font)
{
    Color textColor = style.visitedDependentColorWithColorFilter(CSSPropertyColor);
    Color c = blockFlow().selectionBackgroundColor();
    if (!c.isVisible())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = c.invertedColorWithAlpha(1.0);

    const LegacyRootInlineBox& rootBox = root();
    GraphicsContextStateSaver stateSaver(context);
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    LayoutRect selectionRect { LayoutUnit(x() + paintOffset.x()), LayoutUnit(y() + paintOffset.y() + rootBox.selectionTop()), 0_lu, rootBox.selectionHeight() };
    TextRun run = RenderBlock::constructTextRun(m_str, style, AllowRightExpansion);
    font.adjustSelectionRectForText(run, selectionRect);
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), run.ltr()), c);
}

bool LegacyEllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit, LayoutUnit, HitTestAction)
{
    LayoutPoint adjustedLocation = accumulatedOffset + LayoutPoint(topLeft());

    auto boundsRect = LayoutRect { adjustedLocation, LayoutSize(LayoutUnit(logicalWidth()), m_height) };
    if (visibleToHitTesting(request) && locationInContainer.intersects(boundsRect)) {
        blockFlow().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(blockFlow().nodeForHitTest(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

} // namespace WebCore
