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

#include "InlineIteratorBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderView.h"

namespace WebCore {
namespace InlineIterator {

LineBoxIterator::LineBoxIterator(LineBox::PathVariant&& pathVariant)
    : m_lineBox(WTFMove(pathVariant))
{
}

LineBoxIterator::LineBoxIterator(const LineBox& lineBox)
    : m_lineBox(lineBox)
{
}

bool LineBoxIterator::atEnd() const
{
    return WTF::switchOn(m_lineBox.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

LineBoxIterator& LineBoxIterator::traverseNext()
{
    WTF::switchOn(m_lineBox.m_pathVariant, [](auto& path) {
        return path.traverseNext();
    });
    return *this;
}

LineBoxIterator& LineBoxIterator::traversePrevious()
{
    WTF::switchOn(m_lineBox.m_pathVariant, [](auto& path) {
        return path.traversePrevious();
    });
    return *this;
}

LineBoxIterator::operator bool() const
{
    return !atEnd();
}

bool LineBoxIterator::operator==(const LineBoxIterator& other) const
{
    return m_lineBox.m_pathVariant == other.m_lineBox.m_pathVariant;
}

LineBoxIterator firstLineBoxFor(const RenderBlockFlow& flow)
{
    if (auto* lineLayout = flow.modernLineLayout())
        return lineLayout->firstLineBox();

    return { LineBoxIteratorLegacyPath { flow.firstRootBox() } };
}

LineBoxIterator lastLineBoxFor(const RenderBlockFlow& flow)
{
    if (auto* lineLayout = flow.modernLineLayout())
        return lineLayout->lastLineBox();

    return { LineBoxIteratorLegacyPath { flow.lastRootBox() } };
}

LineBoxIterator LineBox::next() const
{
    return LineBoxIterator(*this).traverseNext();
}

LineBoxIterator LineBox::previous() const
{
    return LineBoxIterator(*this).traversePrevious();
}

LeafBoxIterator LineBox::firstLeafBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> LeafBoxIterator {
        return { path.firstLeafBox() };
    });
}

LeafBoxIterator LineBox::lastLeafBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> LeafBoxIterator {
        return { path.lastLeafBox() };
    });
}

LeafBoxIterator closestBoxForHorizontalPosition(const LineBox& lineBox, float horizontalPosition, bool editableOnly)
{
    auto isEditable = [&](auto box) {
        return box && box->renderer().node() && box->renderer().node()->hasEditableStyle();
    };

    auto firstBox = lineBox.firstLeafBox();
    auto lastBox = lineBox.lastLeafBox();

    if (firstBox != lastBox) {
        if (firstBox->isLineBreak())
            firstBox = firstBox->nextOnLineIgnoringLineBreak();
        else if (lastBox->isLineBreak())
            lastBox = lastBox->previousOnLineIgnoringLineBreak();
    }

    if (firstBox == lastBox && (!editableOnly || isEditable(firstBox)))
        return firstBox;

    if (firstBox && horizontalPosition <= firstBox->logicalLeft() && !firstBox->renderer().isListMarker() && (!editableOnly || isEditable(firstBox)))
        return firstBox;

    if (lastBox && horizontalPosition >= lastBox->logicalRight() && !lastBox->renderer().isListMarker() && (!editableOnly || isEditable(lastBox)))
        return lastBox;

    auto closestBox = lastBox;
    for (auto box = firstBox; box; box = box.traverseNextOnLineIgnoringLineBreak()) {
        if (!box->renderer().isListMarker() && (!editableOnly || isEditable(box))) {
            if (horizontalPosition < box->logicalRight())
                return box;
            closestBox = box;
        }
    }

    return closestBox;
}

RenderObject::HighlightState LineBox::ellipsisSelectionState() const
{
    auto lastLeafBox = this->lastLeafBox();
    ASSERT(lastLeafBox);
    if (!lastLeafBox->isText())
        return RenderObject::HighlightState::None;

    auto& text = downcast<InlineIterator::TextBox>(*lastLeafBox);
    if (text.selectionState() == RenderObject::HighlightState::None)
        return RenderObject::HighlightState::None;

    auto selectionRange = text.selectableRange();
    if (!selectionRange.truncation)
        return RenderObject::HighlightState::None;

    auto [selectionStart, selectionEnd] = formattingContextRoot().view().selection().rangeForTextBox(text.renderer(), selectionRange);
    return selectionStart <= *selectionRange.truncation && selectionEnd >= *selectionRange.truncation ? RenderObject::HighlightState::Inside : RenderObject::HighlightState::None;
}

}
}

