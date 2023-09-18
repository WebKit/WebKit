/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "RubyFormattingContext.h"

#include "InlineLine.h"

namespace WebCore {
namespace Layout {

RubyFormattingContext::RubyFormattingContext(const InlineFormattingContext& parentFormattingContext, const InlineItems& inlineItems)
    : m_parentFormattingContext(parentFormattingContext)
    , m_inlineItems(inlineItems)
{
}

void RubyFormattingContext::layoutInlineAxis(const InlineItemRange& rubyRange, Line& line, InlineLayoutUnit availableWidth)
{
    UNUSED_PARAM(availableWidth);

    ASSERT(!rubyRange.isEmpty());
    // Ruby container inline item list is as follows:
    // [ruby container start][ruby base start][ruby base content][ruby base end][...][ruby container end]

    auto& rubyContainerStart = m_inlineItems[rubyRange.startIndex()];
    auto rubyContainerEndIndex = rubyRange.endIndex() - 1;
    ASSERT(rubyContainerStart.layoutBox().isRuby());
    line.append(rubyContainerStart, rubyContainerStart.style(), { });

    auto baseContentIndex = rubyRange.startIndex() + 1;
    while (baseContentIndex < rubyContainerEndIndex) {
        auto handleRubyColumn = [&] {
            // ruby column: represented by a single ruby base and one ruby annotation
            // from each interlinear annotation level in its ruby segment.
            auto& rubyBaseStart = m_inlineItems[baseContentIndex++];
            ASSERT(rubyBaseStart.style().display() == DisplayType::RubyBase);
            line.append(rubyBaseStart, rubyBaseStart.style(), { });

            baseContentIndex += layoutRubyBaseInlineAxis(line, rubyBaseStart.layoutBox(), baseContentIndex);

            auto& rubyBaseEnd = m_inlineItems[baseContentIndex++];
            ASSERT(rubyBaseEnd.layoutBox().style().display() == DisplayType::RubyBase);
            line.append(rubyBaseEnd, rubyBaseEnd.layoutBox().style(), { });
        };
        handleRubyColumn();
    }

    auto& rubyContainerEnd = m_inlineItems[rubyContainerEndIndex];
    ASSERT(rubyContainerEnd.layoutBox().isRuby());
    line.append(rubyContainerEnd, rubyContainerEnd.style(), { });
}

size_t RubyFormattingContext::layoutRubyBaseInlineAxis(Line& line, const Box& rubyBaseLayoutBox, size_t rubyBaseContentStart)
{
    // Append ruby base content (including start/end inline box) to the line and apply "ruby-align: space-between" on the ruby subrange.
    auto& formattingGeometry = parentFormattingContext().formattingGeometry();
    auto annotationBoxLogicalWidth = InlineLayoutUnit { rubyBaseLayoutBox.associatedRubyAnnotationBox() ? InlineLayoutUnit(parentFormattingContext().geometryForBox(*rubyBaseLayoutBox.associatedRubyAnnotationBox()).marginBoxWidth()) : 0.f };
    auto lineLogicalRight = line.contentLogicalRight();
    auto baseContentLogicalWidth = InlineLayoutUnit { };
    auto baseRunStart = line.runs().size();

    for (size_t index = rubyBaseContentStart; index < m_inlineItems.size(); ++index) {
        auto& rubyBaseInlineItem = m_inlineItems[index];
        if (&rubyBaseInlineItem.layoutBox() == &rubyBaseLayoutBox) {
            auto baseRunCount = line.runs().size() - baseRunStart;
            if (!baseRunCount) {
                ASSERT_NOT_REACHED();
                return { };
            }
            // End of base content.
            // https://drafts.csswg.org/css-ruby/#interlinear-inline
            // Within each base and annotation box, how the extra space is distributed when its content is narrower than
            // the measure of the box is specified by its ruby-align property.
            if (annotationBoxLogicalWidth > baseContentLogicalWidth) {
                auto baseRunRange = WTF::Range<size_t> { baseRunStart, baseRunStart + baseRunCount };
                auto expansion = ExpansionInfo { };
                // ruby-align: space-between
                // The ruby content expands as defined for normal text justification (as defined by text-justify), except that if there are no justification
                // opportunities the content is centered.
                TextUtil::computedExpansions(line.runs(), baseRunRange, { }, expansion);
                if (expansion.opportunityCount)
                    line.applyExpansionOnRange(baseRunRange, expansion, annotationBoxLogicalWidth - baseContentLogicalWidth);
                else {
                    auto centerOffset = (annotationBoxLogicalWidth - baseContentLogicalWidth) / 2;
                    line.moveRunsBy(centerOffset, baseRunRange.begin());
                }
                return index - rubyBaseContentStart;
            }
        }
        auto logicalWidth = formattingGeometry.inlineItemWidth(rubyBaseInlineItem, lineLogicalRight + baseContentLogicalWidth, { });
        line.append(rubyBaseInlineItem, rubyBaseInlineItem.style(), logicalWidth);
        baseContentLogicalWidth += logicalWidth;
    }
    ASSERT_NOT_REACHED();
    return m_inlineItems.size() - rubyBaseContentStart;
}

}
}

