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

LineBoxIterator::LineBoxIterator(const LineBox& lineBox)
    : m_lineBox(lineBox)
{
}

bool LineBoxIterator::atEnd() const
{
    return m_lineBox.atEnd();
}

LineBoxIterator& LineBoxIterator::traverseNext()
{
    ASSERT(!atEnd());

    ++m_lineBox.m_lineIndex;
    return *this;
}

LineBoxIterator& LineBoxIterator::traversePrevious()
{
    ASSERT(!atEnd());

    if (!m_lineBox.m_lineIndex) {
        m_lineBox.setAtEnd();
        return *this;
    }

    --m_lineBox.m_lineIndex;
    return *this;
}

LineBoxIterator::operator bool() const
{
    return !atEnd();
}

bool LineBoxIterator::operator==(const LineBoxIterator& other) const
{
    return m_lineBox.m_inlineContent == other.m_lineBox.m_inlineContent && m_lineBox.m_lineIndex == other.m_lineBox.m_lineIndex;
}

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

LineBoxIterator lineBoxFor(const LayoutIntegration::InlineContent& inlineContent, size_t lineIndex)
{
    return { LineBox { inlineContent, lineIndex } };
}

LineBoxIterator LineBox::next() const
{
    return LineBoxIterator(*this).traverseNext();
}

LineBoxIterator LineBox::previous() const
{
    return LineBoxIterator(*this).traversePrevious();
}

LeafBoxIterator LineBox::lineLeftmostLeafBox() const
{
    if (!line().boxCount())
        return { };
    auto box = boxFor(*m_inlineContent, line().firstBoxIndex());
    if (box->isInlineBox())
        box.traverseLineRightwardOnLine();
    return box;
}

LeafBoxIterator LineBox::lineRightmostLeafBox() const
{
    auto boxCount = line().boxCount();
    if (!boxCount)
        return { };
    auto box = boxFor(*m_inlineContent, line().firstBoxIndex() + boxCount - 1);
    if (box->isInlineBox())
        box.traverseLineLeftwardOnLine();
    return box;
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
    if (!lastLeafBox)
        return RenderObject::HighlightState::None;

    auto* text = dynamicDowncast<InlineIterator::TextBox>(*lastLeafBox);
    if (!text || text->selectionState() == RenderObject::HighlightState::None)
        return RenderObject::HighlightState::None;

    auto selectionRange = text->selectableRange();
    if (!selectionRange.truncation)
        return RenderObject::HighlightState::None;

    auto [selectionStart, selectionEnd] = formattingContextRoot().view().selection().rangeForTextBox(text->renderer(), selectionRange);
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

