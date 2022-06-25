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

namespace WebCore {
namespace InlineIterator {

TextBoxIterator TextBox::nextTextBox() const
{
    return TextBoxIterator(*this).traverseNextTextBox();
}

LayoutRect TextBox::selectionRect(unsigned rangeStart, unsigned rangeEnd) const
{
    auto [clampedStart, clampedEnd] = selectableRange().clamp(rangeStart, rangeEnd);

    if (clampedStart >= clampedEnd && !(rangeStart == rangeEnd && rangeStart >= start() && rangeStart <= end()))
        return { };

    auto lineSelectionRect = LineSelection::logicalRect(*lineBox());
    auto selectionRect = LayoutRect { logicalLeft(), lineSelectionRect.y(), logicalWidth(), lineSelectionRect.height() };

    TextRun textRun = createTextRun();
    if (clampedStart || clampedEnd != textRun.length())
        fontCascade().adjustSelectionRectForText(textRun, selectionRect, clampedStart, clampedEnd);

    return snappedSelectionRect(selectionRect, logicalRight(), lineSelectionRect.y(), lineSelectionRect.height(), isHorizontal());
}

unsigned TextBox::offsetForPosition(float x, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;
    if (x - logicalLeft() > logicalWidth())
        return isLeftToRightDirection() ? length() : 0;
    if (x - logicalLeft() < 0)
        return isLeftToRightDirection() ? 0 : length();
    return fontCascade().offsetForPosition(createTextRun(CreateTextRunMode::Editing), x - logicalLeft(), includePartialGlyphs);
}

float TextBox::positionForOffset(unsigned offset) const
{
    ASSERT(offset >= start());
    ASSERT(offset <= end());

    if (isLineBreak())
        return logicalLeft();

    auto [startOffset, endOffset] = [&] {
        if (direction() == TextDirection::RTL)
            return std::pair { selectableRange().clamp(offset), length() };
        return std::pair { 0u, selectableRange().clamp(offset) };
    }();

    auto selectionRect = LayoutRect(logicalLeft(), 0, 0, 0);
    
    auto textRun = createTextRun(CreateTextRunMode::Editing);
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
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(text))
        return lineLayout->textBoxesFor(text);
#endif

    return { BoxLegacyPath { text.firstTextBox() } };
}

TextBoxIterator textBoxFor(const LegacyInlineTextBox* legacyInlineTextBox)
{
    return { BoxLegacyPath { legacyInlineTextBox } };
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, const InlineDisplay::Box& box)
{
    return textBoxFor(content, content.indexForBox(box));
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    ASSERT(content.boxes[boxIndex].text());
    return { BoxModernPath { content, boxIndex } };
}
#endif

TextBoxRange textBoxesFor(const RenderText& text)
{
    return { firstTextBoxFor(text) };
}

}
}
