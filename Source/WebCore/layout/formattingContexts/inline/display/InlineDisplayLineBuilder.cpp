/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InlineDisplayLineBuilder.h"

#include "InlineDisplayContentBuilder.h"
#include "LayoutBoxGeometry.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

InlineDisplayLineBuilder::InlineDisplayLineBuilder(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

InlineDisplayLineBuilder::EnclosingLineGeometry InlineDisplayLineBuilder::collectEnclosingLineGeometry(const LineBox& lineBox, const InlineRect& lineBoxRect) const
{
    auto initialEnclosingTopAndBottom = [&]() -> std::tuple<std::optional<InlineLayoutUnit>, std::optional<InlineLayoutUnit>>  {
        auto& rootInlineBox = lineBox.rootInlineBox();
        if (!lineBox.hasContent() || !rootInlineBox.hasContent())
            return { };
        return {
            lineBoxRect.top() + rootInlineBox.logicalTop() - rootInlineBox.annotationAbove().value_or(0.f),
            lineBoxRect.top() + rootInlineBox.logicalBottom() + rootInlineBox.annotationUnder().value_or(0.f)
        };
    };
    auto [enclosingTop, enclosingBottom] = initialEnclosingTopAndBottom();
    auto scrollableOverflowRect = lineBoxRect;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        if (!inlineLevelBox.isAtomicInlineLevelBox() && !inlineLevelBox.isInlineBox() && !inlineLevelBox.isLineBreakBox())
            continue;

        auto& layoutBox = inlineLevelBox.layoutBox();
        auto borderBox = InlineRect { };

        if (inlineLevelBox.isAtomicInlineLevelBox()) {
            borderBox = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            borderBox.moveBy(lineBoxRect.topLeft());
        } else if (inlineLevelBox.isInlineBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto isContentful = [&] {
                // In standards mode, inline boxes always start with an imaginary strut.
                return layoutState().inStandardsMode() || inlineLevelBox.hasContent() || boxGeometry.horizontalBorder() || (boxGeometry.horizontalPadding() && boxGeometry.horizontalPadding().value());
            };
            if (!isContentful())
                continue;
            borderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            borderBox.moveBy(lineBoxRect.topLeft());
            // Collect scrollable overflow from inline boxes. All other inline level boxes (e.g atomic inline level boxes) stretch the line.
            if (lineBox.hasContent()) {
                // Empty lines (e.g. continuation pre/post blocks) don't expect scrollbar overflow.
                scrollableOverflowRect.expandVerticallyToContain(borderBox);
            }
        } else if (inlineLevelBox.isLineBreakBox()) {
            borderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            borderBox.moveBy(lineBoxRect.topLeft());
        } else
            ASSERT_NOT_REACHED();

        auto adjustedBorderBoxTop = borderBox.top() - inlineLevelBox.annotationAbove().value_or(0.f);
        auto adjustedBorderBoxBottom = borderBox.bottom() + inlineLevelBox.annotationUnder().value_or(0.f);
        enclosingTop = std::min(enclosingTop.value_or(adjustedBorderBoxTop), adjustedBorderBoxTop);
        enclosingBottom = std::max(enclosingBottom.value_or(adjustedBorderBoxBottom), adjustedBorderBoxBottom);
    }
    return { { enclosingTop.value_or(lineBoxRect.top()), enclosingBottom.value_or(lineBoxRect.top()) }, scrollableOverflowRect };
}

InlineDisplay::Line InlineDisplayLineBuilder::build(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const ConstraintsForInlineContent& constraints) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto isLeftToRightDirection = lineContent.inlineBaseDirection == TextDirection::LTR;
    auto lineBoxLogicalWidth = lineBox.logicalRect().width();
    auto lineBoxVisualLeft = isLeftToRightDirection
        ? lineContent.lineLogicalTopLeft.x()
        : InlineLayoutUnit { constraints.visualLeft() + constraints.horizontal().logicalWidth + constraints.horizontal().logicalLeft  } - (lineContent.lineLogicalTopLeft.x() + lineBoxLogicalWidth);

    auto contentVisualLeft = isLeftToRightDirection
        ? lineBox.rootInlineBoxAlignmentOffset()
        : lineBoxLogicalWidth - lineBox.rootInlineBoxAlignmentOffset() - lineContent.contentLogicalRight;
    auto lineBoxRect = InlineRect { lineContent.lineLogicalTopLeft.y(), lineBoxVisualLeft, lineBox.hasContent() ? lineContent.lineLogicalWidth : 0.f, lineBox.logicalRect().height() };
    auto enclosingLineGeometry = collectEnclosingLineGeometry(lineBox, lineBoxRect);

    // FIXME: Figure out if properties like enclosingLineGeometry top and bottom needs to be flipped as well.
    auto writingMode = root().style().writingMode();
    return InlineDisplay::Line { flipLogicalLineRectToVisualForWritingMode(lineBoxRect, writingMode)
        , flipLogicalLineRectToVisualForWritingMode(enclosingLineGeometry.scrollableOverflowRect, writingMode)
        , enclosingLineGeometry.enclosingTopAndBottom
        , rootInlineBox.logicalTop() + rootInlineBox.ascent()
        , lineBox.baselineType()
        , contentVisualLeft
        , rootInlineBox.logicalWidth()
        , lineBox.isHorizontal()
        , trailingEllipsisRect(lineContent, lineBox, lineBoxRect)
    };
}

// FIXME: for bidi content, we may need to run this code after we finished constructing the display boxes
// and also run truncation on the (visual)display box list and not on the (logical)line runs.
std::optional<FloatRect> InlineDisplayLineBuilder::trailingEllipsisRect(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const FloatRect& lineBoxVisualRect) const
{
    if (!lineContent.lineNeedsTrailingEllipsis)
        return { };

    auto ellipsisStart = 0.f;
    for (auto& lineRun : lineContent.runs) {
        if (lineRun.isInlineBox())
            continue;
        if (lineRun.isTruncated()) {
            if (lineRun.isText() && lineRun.textContent()->partiallyVisibleContent)
                ellipsisStart = std::max(ellipsisStart, lineRun.logicalLeft() + lineRun.textContent()->partiallyVisibleContent->width);
            break;
        }
        ellipsisStart = std::max(ellipsisStart, lineRun.logicalRight());
    }
    auto ellipsisWidth = !lineBox.lineIndex() ? root().firstLineStyle().fontCascade().width(TextUtil::ellipsisTextRun()) : root().style().fontCascade().width(TextUtil::ellipsisTextRun());
    auto rootInlineBoxRect = lineBox.logicalRectForRootInlineBox();
    auto ellipsisRect = FloatRect { lineBoxVisualRect.x() + ellipsisStart, lineBoxVisualRect.y() + rootInlineBoxRect.top(), ellipsisWidth, rootInlineBoxRect.height() };

    if (root().style().isLeftToRightDirection())
        return ellipsisRect;
    ellipsisRect.setX(lineBoxVisualRect.maxX() - (ellipsisStart + ellipsisRect.width()));
    return ellipsisRect;
}

InlineRect InlineDisplayLineBuilder::flipLogicalLineRectToVisualForWritingMode(const InlineRect& lineLogicalRect, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        return lineLogicalRect;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft:
        // See InlineFormattingGeometry for more info.
        return { lineLogicalRect.left(), lineLogicalRect.top(), lineLogicalRect.height(), lineLogicalRect.width() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return lineLogicalRect;
}

}
}

