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

struct DamagedLine {
    size_t lineIndex { 0 };
    InlineItemPosition leadingInlineItemPosition { };
};
static std::optional<DamagedLine> leadingInlineItemPositionOnDamagedLine(const InlineItems& inlineItems, const DisplayBoxes& displayBoxes)
{
    // 1. Find the last root inline box (this is our damaged line)
    // 2. Find the first content display box on the last line
    // 3. Find the associated InlineItem with partial text offset if applicable
    auto leadingContentDisplayBox = [&]() -> const InlineDisplay::Box* {
        // FIXME: This only covers the append case yet.
        auto rootInlineBoxIndexOnDamagedLine = [&]() -> size_t {
            for (auto index = displayBoxes.size(); index--;) {
                if (displayBoxes[index].isRootInlineBox())
                    return index;
            }
            ASSERT_NOT_REACHED();
            return 0;
        };
        auto firstContentDisplayBoxIndex = rootInlineBoxIndexOnDamagedLine() + 1;
        for (; firstContentDisplayBoxIndex < displayBoxes.size(); ++firstContentDisplayBoxIndex) {
            auto& displayBox = displayBoxes[firstContentDisplayBoxIndex];
            ASSERT(!displayBox.isRootInlineBox());
            if (!displayBox.isNonRootInlineBox() || displayBox.isFirstForLayoutBox())
                return &displayBox;
        }
        return nullptr;
    }();

    if (!leadingContentDisplayBox) {
        // This is a completely empty line (e.g. <div><span> </span></div> where the whitespce is collapsed).
        return { };
    }

    for (size_t index = inlineItems.size(); index--;) {
        if (&inlineItems[index].layoutBox() == &leadingContentDisplayBox->layoutBox() && !inlineItems[index].isInlineBoxEnd()) {
            // This is our last InlineItem associated with this layout box. Let's find out
            // which previous InlineItem is at the beginning of the damaged line.
            // Only text content may produce multiple (content type of) InlineItems for a layout box.
            auto leadingInlineItemPosition = [&]() -> std::optional<InlineItemPosition> {
                if (!leadingContentDisplayBox->isTextOrSoftLineBreak())
                    return InlineItemPosition { index, 0 };
                auto startOffset = leadingContentDisplayBox->text().start();
                while (true) {
                    auto inlineTextItemRange = [&]() -> WTF::Range<unsigned> {
                        if (is<InlineTextItem>(inlineItems[index])) {
                            auto& inlineTextItem = downcast<InlineTextItem>(inlineItems[index]);
                            return { inlineTextItem.start(), inlineTextItem.end() };
                        }
                        if (is<InlineSoftLineBreakItem>(inlineItems[index])) {
                            auto startPosition = downcast<InlineSoftLineBreakItem>(inlineItems[index]).position();
                            return { startPosition, startPosition + 1 };
                        }
                        ASSERT_NOT_REACHED();
                        return { };
                    };
                    auto textRange = inlineTextItemRange();
                    if (textRange.begin() <= startOffset && textRange.end() > startOffset)
                        return InlineItemPosition { index, startOffset - textRange.begin() };
                    if (!index--)
                        break;
                }
                return { };
            };
            if (auto damangedInlineItemPosition = leadingInlineItemPosition()) {
                auto damagedLineIndex = leadingContentDisplayBox->lineIndex();
                if (damagedLineIndex && !*damangedInlineItemPosition) {
                    // This is clearly not correct (starting position is 0 while the damaged line is not the first one).
                    ASSERT_NOT_REACHED();
                    damagedLineIndex = 0;
                }
                return DamagedLine { damagedLineIndex, *damangedInlineItemPosition };
            }
            return { };
        }
    }
    return { };
}

void InlineInvalidation::textInserted(const InlineTextBox&, std::optional<size_t>, std::optional<size_t>)
{
    if (m_displayBoxes.isEmpty())
        return m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);

    if (auto damangedContent = leadingInlineItemPositionOnDamagedLine(m_inlineFormattingState.inlineItems(), m_displayBoxes))
        m_inlineDamage.setDamagedPosition({ damangedContent->lineIndex, damangedContent->leadingInlineItemPosition });
    m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);
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
