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
#include "InlineInvalidation.h"

#include "InlineDamage.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

InlineInvalidation::InlineInvalidation(InlineDamage& inlineDamage, const InlineFormattingState& inlineFormattingState, const Vector<InlineDisplay::Box>& displayBoxes)
    : m_inlineDamage(inlineDamage)
    , m_inlineFormattingState(inlineFormattingState)
    , m_displayBoxes(displayBoxes)
{
}

void InlineInvalidation::styleChanged(const Box& layoutBox, const RenderStyle& oldStyle)
{
    UNUSED_PARAM(layoutBox);
    UNUSED_PARAM(oldStyle);

    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);
}

static size_t damagedLineIndex(const InlineTextBox* damagedInlineTextBox, std::optional<size_t> offset, const DisplayBoxes& displayBoxes)
{
    UNUSED_PARAM(damagedInlineTextBox);
    UNUSED_PARAM(offset);

    ASSERT(!displayBoxes.isEmpty());
    // Append only yet.
    return displayBoxes.last().lineIndex();
}

static const InlineDisplay::Box* leadingContentDisplayForLineIndex(size_t lineIndex, const DisplayBoxes& displayBoxes)
{
    auto rootInlineBoxIndexOnLine = [&]() -> size_t {
        for (auto index = displayBoxes.size(); index--;) {
            auto& displayBox = displayBoxes[index];
            if (displayBox.lineIndex() == lineIndex && displayBox.isRootInlineBox())
                return index;
        }
        ASSERT_NOT_REACHED();
        return 0;
    };

    for (auto firstContentDisplayBoxIndex = rootInlineBoxIndexOnLine() + 1; firstContentDisplayBoxIndex < displayBoxes.size(); ++firstContentDisplayBoxIndex) {
        auto& displayBox = displayBoxes[firstContentDisplayBoxIndex];
        ASSERT(!displayBox.isRootInlineBox());
        if (!displayBox.isNonRootInlineBox() || displayBox.isFirstForLayoutBox())
            return &displayBox;
    }
    return nullptr;
}

static std::optional<InlineItemPosition> inlineItemPositionForDisplayBox(const InlineDisplay::Box& displayBox, const InlineItems& inlineItems)
{
    for (size_t index = inlineItems.size(); index--;) {
        if (&inlineItems[index].layoutBox() != &displayBox.layoutBox() || inlineItems[index].isInlineBoxEnd())
            continue;

        // This is our last InlineItem associated with this layout box. Let's find out
        // which previous InlineItem is at the beginning of this display box.
        auto damagedInlineItemPosition = [&displayBox, &inlineItems] (auto damagedInlineItemIndex) -> std::optional<InlineItemPosition> {
            if (!displayBox.isTextOrSoftLineBreak())
                return InlineItemPosition { damagedInlineItemIndex, 0 };
            // 1. A display box could consist of one or more InlineItems.
            // 2. A display box could start at any offset inide and InlineItem.
            auto startOffset = displayBox.text().start();
            while (true) {
                auto inlineTextItemRange = [&]() -> WTF::Range<unsigned> {
                    if (is<InlineTextItem>(inlineItems[damagedInlineItemIndex])) {
                        auto& inlineTextItem = downcast<InlineTextItem>(inlineItems[damagedInlineItemIndex]);
                        return { inlineTextItem.start(), inlineTextItem.end() };
                    }
                    if (is<InlineSoftLineBreakItem>(inlineItems[damagedInlineItemIndex])) {
                        auto startPosition = downcast<InlineSoftLineBreakItem>(inlineItems[damagedInlineItemIndex]).position();
                        return { startPosition, startPosition + 1 };
                    }
                    ASSERT_NOT_REACHED();
                    return { };
                };
                auto textRange = inlineTextItemRange();
                if (textRange.begin() <= startOffset && textRange.end() > startOffset)
                    return InlineItemPosition { damagedInlineItemIndex, startOffset - textRange.begin() };
                if (!damagedInlineItemIndex--)
                    break;
            }
            return { };
        };
        return damagedInlineItemPosition(index);
    }
    return { };
}

struct DamagedLine {
    size_t lineIndex { 0 };
    InlineItemPosition leadingInlineItemPosition { };
};
static std::optional<DamagedLine> leadingInlineItemPositionOnLastLine(const InlineItems& inlineItems, const DisplayBoxes& displayBoxes)
{
    if (displayBoxes.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    // 1. Find the last root inline box (this is our damaged line)
    // 2. Find the first content display box on the last line
    // 3. Find the associated InlineItem with partial text offset if applicable
    auto lineIndex = damagedLineIndex({ }, { }, displayBoxes);
    auto leadingContentDisplayBoxOnDamagedLine = leadingContentDisplayForLineIndex(lineIndex, displayBoxes);
    if (!leadingContentDisplayBoxOnDamagedLine) {
        // This is a completely empty line (e.g. <div><span> </span></div> where the whitespce is collapsed).
        return { };
    }
    if (auto damagedInlineItemPosition = inlineItemPositionForDisplayBox(*leadingContentDisplayBoxOnDamagedLine, inlineItems)) {
        if (lineIndex && !*damagedInlineItemPosition) {
            // This is clearly not correct (starting position is 0 while the damaged line is not the first one).
            ASSERT_NOT_REACHED();
            lineIndex = 0;
        }
        return DamagedLine { lineIndex, *damagedInlineItemPosition };
    }
    return { };
}

static std::optional<DamagedLine> leadingInlineItemPositionByDamagedBox(const InlineTextBox& damagedInlineTextBox, size_t offset, const InlineItems& inlineItems, const DisplayBoxes& displayBoxes)
{
    UNUSED_PARAM(damagedInlineTextBox);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(inlineItems);
    UNUSED_PARAM(displayBoxes);
    return { };
}

void InlineInvalidation::textInserted(const InlineTextBox* damagedInlineTextBox, std::optional<size_t> offset, std::optional<size_t> length)
{
    UNUSED_PARAM(length);

    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);
    if (m_displayBoxes.isEmpty())
        return;

    if (!damagedInlineTextBox) {
        // New text box got appended. Let's dirty the last existing line.
        ASSERT(!offset);
        ASSERT(!length);
        if (auto damagedContent = leadingInlineItemPositionOnLastLine(m_inlineFormattingState.inlineItems(), m_displayBoxes))
            m_inlineDamage.setDamagedPosition({ damagedContent->lineIndex, damagedContent->leadingInlineItemPosition });
        return;
    }
    // Existing text box got modified. Dirty all the way up to the damaged position's line.
    if (auto damagedContent = leadingInlineItemPositionByDamagedBox(*damagedInlineTextBox, offset.value_or(0), m_inlineFormattingState.inlineItems(), m_displayBoxes))
        m_inlineDamage.setDamagedPosition({ damagedContent->lineIndex, damagedContent->leadingInlineItemPosition });
}

void InlineInvalidation::textWillBeRemoved(const InlineTextBox& textBox, std::optional<size_t> offset, std::optional<size_t> length)
{
    UNUSED_PARAM(textBox);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(length);

    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);
}

void InlineInvalidation::inlineLevelBoxInserted(const Box& layoutBox)
{
    UNUSED_PARAM(layoutBox);

    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);
}

void InlineInvalidation::inlineLevelBoxWillBeRemoved(const Box& layoutBox)
{
    UNUSED_PARAM(layoutBox);

    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);
}

void InlineInvalidation::horizontalConstraintChanged()
{
    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsLineLayout);
}

}
}
