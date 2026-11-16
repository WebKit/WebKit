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
#include "InlineIteratorLineBox.h"
#include "InlineIteratorLineBoxInlines.h"

#include "InlineIteratorBoxInlines.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlowInlines.h"
#include "RenderObjectDocument.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace InlineIterator {

static_assert(std::ranges::bidirectional_range<LineBoxRange> && std::ranges::common_range<LineBoxRange> && std::ranges::view<LineBoxRange> && std::ranges::borrowed_range<LineBoxRange>);

LineBoxIterator firstLineBoxFor(const RenderBlockFlow& flow)
{
    if (CheckedPtr lineLayout = flow.inlineLayout())
        return lineLayout->firstLineBox();
    return { };
}

LineBoxIterator lastLineBoxFor(const RenderBlockFlow& flow)
{
    if (CheckedPtr lineLayout = flow.inlineLayout())
        return lineLayout->lastLineBox();
    return { };
}

LineBox lineBoxFor(const LayoutIntegration::InlineContent& inlineContent, size_t lineIndex)
{
    return { inlineContent, lineIndex };
}

LineBoxIterator LineBox::next() const
{
    ASSERT(!atEnd());
    return LineBox { *m_inlineContent, m_lineIndex + 1 };
}

LineBoxIterator LineBox::previous() const
{
    ASSERT(!atEnd());
    if (!m_lineIndex)
        return { };
    return LineBox { *m_inlineContent, m_lineIndex - 1 };
}

LeafBoxIterator LineBox::lineLeftmostLeafBox() const
{
    if (!line().boxCount())
        return { };
    auto box = boxFor(*m_inlineContent, line().firstBoxIndex());
    if (box.isInlineBox())
        return box.nextLineRightwardOnLine();
    return box;
}

LeafBoxIterator LineBox::lineRightmostLeafBox() const
{
    auto boxCount = line().boxCount();
    if (!boxCount)
        return { };
    auto box = boxFor(*m_inlineContent, line().firstBoxIndex() + boxCount - 1);
    if (box.isInlineBox())
        return box.nextLineLeftwardOnLine();
    return box;
}

LeafBoxRange LineBox::leafBoxes() const
{
    return { lineLeftmostLeafBox() };
}

LeafBoxIterator closestBoxForHorizontalPosition(const LineBox& lineBox, float horizontalPosition, bool editableOnly)
{
    auto isEditable = [&](auto box) {
        return box && box->renderer().node() && protect(box->renderer().node())->hasEditableStyle();
    };

    auto firstBox = lineBox.logicalLeftmostLeafBox();
    auto lastBox = lineBox.logicalRightmostLeafBox();

    if (firstBox != lastBox) {
        if (firstBox->isLineBreak())
            firstBox = firstBox->nextLogicalRightwardOnLineIgnoringLineBreak();
        else if (lastBox->isLineBreak())
            lastBox = lastBox->nextLogicalLeftwardOnLineIgnoringLineBreak();
    }

    if (firstBox == lastBox && (!editableOnly || isEditable(firstBox)))
        return firstBox;

    if (firstBox && horizontalPosition <= firstBox->logicalLeft() && !firstBox->renderer().isRenderListOutsideMarker() && (!editableOnly || isEditable(firstBox)))
        return firstBox;

    if (lastBox && horizontalPosition >= lastBox->logicalRight() && !lastBox->renderer().isRenderListOutsideMarker() && (!editableOnly || isEditable(lastBox)))
        return lastBox;

    auto closestBox = lastBox;
    for (auto box = firstBox; box; box = box.traverseLogicalRightwardOnLineIgnoringLineBreak()) {
        if (!box->renderer().isRenderListOutsideMarker() && (!editableOnly || isEditable(box))) {
            if (horizontalPosition < box->logicalRight())
                return box;
            closestBox = box;
        }
    }

    return closestBox;
}

RenderObject::HighlightState LineBox::ellipsisSelectionState() const
{
    auto lastLeafBox = this->lineRightmostLeafBox();
    if (!lastLeafBox || !lastLeafBox->isText())
        return RenderObject::HighlightState::None;

    auto text = downcast<TextBox>(*lastLeafBox);
    if (text.selectionState() == RenderObject::HighlightState::None)
        return RenderObject::HighlightState::None;

    auto selectionRange = text.selectableRange();
    if (!selectionRange.truncation)
        return RenderObject::HighlightState::None;

    auto [selectionStart, selectionEnd] = formattingContextRoot().view().selection().rangeForTextBox(protect(text.renderer()), selectionRange);
    return selectionStart <= *selectionRange.truncation && selectionEnd >= *selectionRange.truncation ? RenderObject::HighlightState::Inside : RenderObject::HighlightState::None;
}

LeafBoxIterator LineBox::blockLevelBox() const
{
    if (!hasBlockContent())
        return { };
    auto blockBox = lineRightmostLeafBox();
    if (!blockBox || !blockBox->isBlockLevelBox()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return blockBox;
}

}
}

