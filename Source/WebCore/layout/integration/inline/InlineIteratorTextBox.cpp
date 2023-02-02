/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "InlineIteratorTextBox.h"

#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LineSelection.h"
#include "RenderCombineText.h"
#include "SVGInlineTextBox.h"

namespace WebCore {
namespace InlineIterator {

TextBoxIterator TextBox::nextTextBox() const
{
    return TextBoxIterator(*this).traverseNextTextBox();
}

LayoutRect TextBox::selectionRect(unsigned rangeStart, unsigned rangeEnd) const
{
    if (is<SVGInlineTextBox>(legacyInlineBox()))
        return downcast<SVGInlineTextBox>(*legacyInlineBox()).localSelectionRect(rangeStart, rangeEnd);

    bool isCaretCase = rangeStart == rangeEnd;

    auto [clampedStart, clampedEnd] = selectableRange().clamp(rangeStart, rangeEnd);

    if (clampedStart >= clampedEnd) {
        if (isCaretCase) {
            // handle unitary range, e.g.: representing caret position
            bool isCaretWithinTextBox = rangeStart >= start() && rangeStart < end();
            // For last text box in a InlineTextBox chain, we allow the caret to move to a position 'after' the end of the last text box.
            bool isCaretWithinLastTextBox = rangeStart >= start() && rangeStart <= end();

            auto itEnd = TextBoxRange(TextBoxIterator(*this)).end();
            auto isLastTextBox = nextTextBox() == itEnd;

            if ((isLastTextBox && !isCaretWithinLastTextBox) || (!isLastTextBox && !isCaretWithinTextBox))
                return { };
        } else {
            bool isRangeWithinTextBox = (rangeStart >= start() && rangeStart <= end());
            if (!isRangeWithinTextBox)
                return { };
        }
    }

    auto lineSelectionRect = LineSelection::logicalRect(*lineBox());
    auto selectionRect = LayoutRect { logicalLeftIgnoringInlineDirection(), lineSelectionRect.y(), logicalWidth(), lineSelectionRect.height() };

    auto textRun = this->textRun();
    if (clampedStart || clampedEnd != textRun.length())
        fontCascade().adjustSelectionRectForText(textRun, selectionRect, clampedStart, clampedEnd);

    return snappedSelectionRect(selectionRect, logicalRightIgnoringInlineDirection(), lineSelectionRect.y(), lineSelectionRect.height(), isHorizontal());
}

unsigned TextBox::offsetForPosition(float x, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;
    if (x - logicalLeftIgnoringInlineDirection() > logicalWidth())
        return isLeftToRightDirection() ? length() : 0;
    if (x - logicalLeftIgnoringInlineDirection() < 0)
        return isLeftToRightDirection() ? 0 : length();
    return fontCascade().offsetForPosition(textRun(TextRunMode::Editing), x - logicalLeftIgnoringInlineDirection(), includePartialGlyphs);
}

float TextBox::positionForOffset(unsigned offset) const
{
    ASSERT(offset >= start());
    ASSERT(offset <= end());

    if (isLineBreak())
        return logicalLeftIgnoringInlineDirection();

    auto [startOffset, endOffset] = [&] {
        if (direction() == TextDirection::RTL)
            return std::pair { selectableRange().clamp(offset), length() };
        return std::pair { 0u, selectableRange().clamp(offset) };
    }();

    auto selectionRect = LayoutRect(logicalLeftIgnoringInlineDirection(), 0, 0, 0);
    
    auto textRun = this->textRun(TextRunMode::Editing);
    fontCascade().adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);
    return snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()).maxX();
}

bool TextBox::isCombinedText() const
{
    auto& renderer = this->renderer();
    return is<RenderCombineText>(renderer) && downcast<RenderCombineText>(renderer).isCombined();
}

const FontCascade& TextBox::fontCascade() const
{
    if (isCombinedText())
        return downcast<RenderCombineText>(renderer()).textCombineFont();

    return style().fontCascade();
}

TextBoxIterator::TextBoxIterator(Box::PathVariant&& pathVariant)
    : LeafBoxIterator(WTFMove(pathVariant))
{
}

TextBoxIterator::TextBoxIterator(const Box& box)
    : LeafBoxIterator(box)
{
}

TextBoxIterator& TextBoxIterator::traverseNextTextBox()
{
    WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        path.traverseNextTextBox();
    });
    return *this;
}

TextBoxIterator firstTextBoxFor(const RenderText& text)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(text))
        return lineLayout->textBoxesFor(text);

    return { BoxLegacyPath { text.firstTextBox() } };
}

TextBoxIterator textBoxFor(const LegacyInlineTextBox* legacyInlineTextBox)
{
    return { BoxLegacyPath { legacyInlineTextBox } };
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, const InlineDisplay::Box& box)
{
    return textBoxFor(content, content.indexForBox(box));
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    ASSERT(content.boxes[boxIndex].text());
    return { BoxModernPath { content, boxIndex } };
}

TextBoxRange textBoxesFor(const RenderText& text)
{
    return { firstTextBoxFor(text) };
}

}
}
