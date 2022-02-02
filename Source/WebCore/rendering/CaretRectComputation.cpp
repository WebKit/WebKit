/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "CaretRectComputation.h"

#include "Editing.h"
#include "InlineIteratorLine.h"
#include "InlineIteratorTextBox.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderSVGInlineText.h"
#include "RenderText.h"

namespace WebCore {

static LayoutRect computeCaretRectForEmptyElement(const RenderBoxModelObject& renderer, LayoutUnit width, LayoutUnit textIndentOffset, CaretRectMode caretRectMode)
{
    ASSERT(!renderer.firstChild());

    // FIXME: This does not take into account either :first-line or :first-letter
    // However, as soon as some content is entered, the line boxes will be
    // constructed and this kludge is not called any more. So only the caret size
    // of an empty :first-line'd block is wrong. I think we can live with that.
    const RenderStyle& currentStyle = renderer.firstLineStyle();

    enum CaretAlignment { AlignLeft, AlignRight, AlignCenter };

    CaretAlignment alignment = AlignLeft;

    switch (currentStyle.textAlign()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        break;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        alignment = AlignCenter;
        break;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        alignment = AlignRight;
        break;
    case TextAlignMode::Justify:
    case TextAlignMode::Start:
        if (!currentStyle.isLeftToRightDirection())
            alignment = AlignRight;
        break;
    case TextAlignMode::End:
        if (currentStyle.isLeftToRightDirection())
            alignment = AlignRight;
        break;
    }

    LayoutUnit x = renderer.borderLeft() + renderer.paddingLeft();
    LayoutUnit maxX = width - renderer.borderRight() - renderer.paddingRight();

    switch (alignment) {
    case AlignLeft:
        if (currentStyle.isLeftToRightDirection())
            x += textIndentOffset;
        break;
    case AlignCenter:
        x = (x + maxX) / 2;
        if (currentStyle.isLeftToRightDirection())
            x += textIndentOffset / 2;
        else
            x -= textIndentOffset / 2;
        break;
    case AlignRight:
        x = maxX - caretWidth;
        if (!currentStyle.isLeftToRightDirection())
            x -= textIndentOffset;
        break;
    }
    x = std::min(x, std::max<LayoutUnit>(maxX - caretWidth, 0));

    auto lineHeight = renderer.lineHeight(true, currentStyle.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    auto height = std::min(lineHeight, LayoutUnit { currentStyle.metricsOfPrimaryFont().height() });
    auto y = renderer.paddingTop() + renderer.borderTop() + (lineHeight > height ? (lineHeight - height) / 2 : LayoutUnit { });

    auto rect = LayoutRect(x, y, caretWidth, height);

    if (caretRectMode == CaretRectMode::ExpandToEndOfLine)
        rect.shiftMaxXEdgeTo(width);

    return currentStyle.isHorizontalWritingMode() ? rect : rect.transposedRect();
}

static LayoutRect computeCaretRectForLinePosition(const InlineIterator::LineIterator& line, float logicalLeftPosition, CaretRectMode caretRectMode)
{
    auto& containingBlock = line->containingBlock();
    auto lineSelectionRect = line->selectionRect();

    int height = lineSelectionRect.height();
    int top = lineSelectionRect.y();

    // Distribute the caret's width to either side of the offset.
    float left = logicalLeftPosition;
    int caretWidthLeftOfOffset = caretWidth / 2;
    left -= caretWidthLeftOfOffset;
    int caretWidthRightOfOffset = caretWidth - caretWidthLeftOfOffset;
    left = roundf(left);

    float lineLeft = lineSelectionRect.x();
    float lineRight = lineSelectionRect.maxX();

    bool rightAligned = false;
    switch (containingBlock.style().textAlign()) {
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        rightAligned = true;
        break;
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        break;
    case TextAlignMode::Justify:
    case TextAlignMode::Start:
        rightAligned = !containingBlock.style().isLeftToRightDirection();
        break;
    case TextAlignMode::End:
        rightAligned = containingBlock.style().isLeftToRightDirection();
        break;
    }

    float leftEdge = std::min<float>(0, lineLeft);
    float rightEdge = std::max<float>(containingBlock.logicalWidth(), lineRight);

    if (rightAligned) {
        left = std::max(left, leftEdge);
        left = std::min(left, lineRight - caretWidth);
    } else {
        left = std::min(left, rightEdge - caretWidthRightOfOffset);
        left = std::max(left, lineLeft);
    }

    auto rect = IntRect(left, top, caretWidth, height);

    if (caretRectMode == CaretRectMode::ExpandToEndOfLine)
        rect.shiftMaxXEdgeTo(lineRight);

    return containingBlock.style().isHorizontalWritingMode() ? rect : rect.transposedRect();
}

static LayoutRect computeCaretRectForText(const InlineRunAndOffset& runAndOffset, CaretRectMode caretRectMode)
{
    if (!runAndOffset.run)
        return { };

    auto& textRun = downcast<InlineIterator::TextBoxIterator>(runAndOffset.run);
    auto line = textRun->line();

    float position = textRun->positionForOffset(runAndOffset.offset);
    return computeCaretRectForLinePosition(line, position, caretRectMode);
}

static LayoutRect computeCaretRectForLineBreak(const InlineRunAndOffset& runAndOffset, CaretRectMode caretRectMode)
{
    ASSERT(!runAndOffset.offset);

    if (!runAndOffset.run)
        return { };

    auto line = runAndOffset.run->line();
    return computeCaretRectForLinePosition(line, line->contentLogicalLeft(), caretRectMode);
}

static LayoutRect computeCaretRectForSVGInlineText(const InlineRunAndOffset& runAndOffset, CaretRectMode)
{
    auto* box = runAndOffset.run ? runAndOffset.run->legacyInlineBox() : nullptr;
    auto caretOffset = runAndOffset.offset;

    if (!is<LegacyInlineTextBox>(box))
        return { };

    auto& textBox = downcast<LegacyInlineTextBox>(*box);
    if (caretOffset < textBox.start() || caretOffset > textBox.start() + textBox.len())
        return { };

    // Use the edge of the selection rect to determine the caret rect.
    if (caretOffset < textBox.start() + textBox.len()) {
        LayoutRect rect = textBox.localSelectionRect(caretOffset, caretOffset + 1);
        LayoutUnit x = textBox.isLeftToRightDirection() ? rect.x() : rect.maxX();
        return LayoutRect(x, rect.y(), caretWidth, rect.height());
    }

    LayoutRect rect = textBox.localSelectionRect(caretOffset - 1, caretOffset);
    LayoutUnit x = textBox.isLeftToRightDirection() ? rect.maxX() : rect.x();
    return { x, rect.y(), caretWidth, rect.height() };
}

static LayoutRect computeCaretRectForBox(const RenderBox& renderer, const InlineRunAndOffset& runAndOffset, CaretRectMode caretRectMode)
{
    // VisiblePositions at offsets inside containers either a) refer to the positions before/after
    // those containers (tables and select elements) or b) refer to the position inside an empty block.
    // They never refer to children.
    // FIXME: Paint the carets inside empty blocks differently than the carets before/after elements.

    LayoutRect rect(renderer.location(), LayoutSize(caretWidth, renderer.height()));
    bool ltr = runAndOffset.run ? runAndOffset.run->isLeftToRightDirection() : renderer.style().isLeftToRightDirection();

    if ((!runAndOffset.offset) ^ ltr)
        rect.move(LayoutSize(renderer.width() - caretWidth, 0_lu));

    if (runAndOffset.run) {
        auto line = runAndOffset.run->line();
        LayoutUnit top = line->top();
        rect.setY(top);
        rect.setHeight(line->bottom() - top);
    }

    // If height of box is smaller than font height, use the latter one,
    // otherwise the caret might become invisible.
    //
    // Also, if the box is not a replaced element, always use the font height.
    // This prevents the "big caret" bug described in:
    // <rdar://problem/3777804> Deleting all content in a document can result in giant tall-as-window insertion point
    //
    // FIXME: ignoring :first-line, missing good reason to take care of
    LayoutUnit fontHeight = renderer.style().metricsOfPrimaryFont().height();
    if (fontHeight > rect.height() || (!renderer.isReplacedOrInlineBlock() && !renderer.isTable()))
        rect.setHeight(fontHeight);

    // Move to local coords
    rect.moveBy(-renderer.location());

    // FIXME: Border/padding should be added for all elements but this workaround
    // is needed because we use offsets inside an "atomic" element to represent
    // positions before and after the element in deprecated editing offsets.
    if (renderer.element() && !(editingIgnoresContent(*renderer.element()) || isRenderedTable(renderer.element()))) {
        rect.setX(rect.x() + renderer.borderLeft() + renderer.paddingLeft());
        rect.setY(rect.y() + renderer.paddingTop() + renderer.borderTop());
    }

    if (caretRectMode == CaretRectMode::ExpandToEndOfLine)
        rect.shiftMaxXEdgeTo(renderer.x() + renderer.width());

    return renderer.isHorizontalWritingMode() ? rect : rect.transposedRect();
}

static LayoutRect computeCaretRectForBlock(const RenderBlock& renderer, const InlineRunAndOffset& runAndOffset, CaretRectMode caretRectMode)
{
    // Do the normal calculation in most cases.
    if (renderer.firstChild())
        return computeCaretRectForBox(renderer, runAndOffset, caretRectMode);

    return computeCaretRectForEmptyElement(renderer, renderer.width(), renderer.textIndentOffset(), caretRectMode);
}

static LayoutRect computeCaretRectForInline(const RenderInline& renderer)
{
    if (renderer.firstChild()) {
        // This condition is possible if the RenderInline is at an editing boundary,
        // i.e. the VisiblePosition is:
        //   <RenderInline editingBoundary=true>|<RenderText> </RenderText></RenderInline>
        // FIXME: need to figure out how to make this return a valid rect, note that
        // there are no line boxes created in the above case.
        return { };
    }

    LayoutRect caretRect = computeCaretRectForEmptyElement(renderer, renderer.horizontalBorderAndPaddingExtent(), 0, CaretRectMode::Normal);

    if (LegacyInlineBox* firstBox = renderer.firstLineBox())
        caretRect.moveBy(LayoutPoint(firstBox->topLeft()));

    return caretRect;
}

LayoutRect computeLocalCaretRect(const RenderObject& renderer, const InlineRunAndOffset& runAndOffset, CaretRectMode caretRectMode)
{
    if (is<RenderSVGInlineText>(renderer))
        return computeCaretRectForSVGInlineText(runAndOffset, caretRectMode);

    if (is<RenderText>(renderer))
        return computeCaretRectForText(runAndOffset, caretRectMode);

    if (is<RenderLineBreak>(renderer))
        return computeCaretRectForLineBreak(runAndOffset, caretRectMode);

    if (is<RenderBlock>(renderer))
        return computeCaretRectForBlock(downcast<RenderBlock>(renderer), runAndOffset, caretRectMode);

    if (is<RenderBox>(renderer))
        return computeCaretRectForBox(downcast<RenderBox>(renderer), runAndOffset, caretRectMode);

    if (is<RenderInline>(renderer))
        return computeCaretRectForInline(downcast<RenderInline>(renderer));

    return { };
}

};
