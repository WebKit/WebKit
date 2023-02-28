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
#include "InlineFormattingState.h"
#include "InlineSoftLineBreakItem.h"
#include "LayoutUnit.h"
#include "TextDirection.h"
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

void InlineInvalidation::textInserted(const InlineTextBox&, std::optional<size_t>, std::optional<size_t>)
{
    if (m_displayBoxes.isEmpty())
        return m_inlineDamage.setDamageType(InlineDamage::Type::NeedsContentUpdateAndLineLayout);

    // FIXME: This only covers the append case yet.
    auto rootInlineBoxIndexOnDamagedLine = [&]() -> size_t {
        for (auto index = m_displayBoxes.size(); index--;) {
            if (m_displayBoxes[index].isRootInlineBox())
                return index;
        }
        ASSERT_NOT_REACHED();
        return 0;
    };
    auto damagedRootInlineBoxIndex = rootInlineBoxIndexOnDamagedLine();
    auto leadingInlineItemPositionOnDamagedLine = [&]() -> std::optional<InlineItemPosition> {
        auto leadingContentDisplayBoxIndex = damagedRootInlineBoxIndex + 1; 
        if (leadingContentDisplayBoxIndex == m_displayBoxes.size() || m_displayBoxes[leadingContentDisplayBoxIndex].isRootInlineBox()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        auto& leadingContentDisplayBox = m_displayBoxes[leadingContentDisplayBoxIndex];
        auto& inlineItems = m_inlineFormattingState.inlineItems();
        for (size_t index = inlineItems.size(); index--;) {
            if (&inlineItems[index].layoutBox() == &leadingContentDisplayBox.layoutBox()) {
                // This is our last InlineItem associated with this layout box. Let's find out
                // which previous InlineItem is at the beginning of the damaged line.
                // Only text content may produce multiple (content type of) InlineItems for a layout box.
                auto leadingInlineItemPosition = [&]() -> std::optional<InlineItemPosition> {
                    if (!leadingContentDisplayBox.isText())
                        return InlineItemPosition { index, 0 };
                    auto startOffset = leadingContentDisplayBox.text().start();
                    while (true) {
                        auto& inlineTextItem = downcast<InlineTextItem>(inlineItems[index]);
                        if (inlineTextItem.start() <= startOffset && inlineTextItem.end() > startOffset)
                            return InlineItemPosition { index, startOffset - inlineTextItem.start() };
                        if (!index--)
                            break;
                    }
                    return { };
                };
                return leadingInlineItemPosition();
            }
        }
        return { };
    };
    if (auto damagedLeadingInlineItemPosition = leadingInlineItemPositionOnDamagedLine())
        m_inlineDamage.setDamagedPosition({ m_displayBoxes[damagedRootInlineBoxIndex].lineIndex(), *damagedLeadingInlineItemPosition });
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
