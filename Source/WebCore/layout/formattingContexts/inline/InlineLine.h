/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#pragma once

#include "InlineFormattingContext.h"
#include <WebCore/InlineDisplayBox.h>
#include <WebCore/InlineItem.h>
#include <WebCore/InlineLineRun.h>
#include <WebCore/InlineLineTypes.h>
#include <WebCore/InlineTextItem.h>
#include <WebCore/StyleComputedStyle.h>
#include <ranges>
#include <unicode/ubidi.h>
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

struct ExpansionInfo;
class InlineContentAligner;
class InlineSoftLineBreakItem;
enum class IntrinsicWidthMode;

class Line {
public:
    Line(const InlineFormattingContext&);

    void initialize(const Vector<InlineItem, 1>& lineSpanningInlineBoxes, bool isFirstFormattedLine);

    using ShapingBoundary = LineShapingBoundary;
    void appendText(const InlineTextItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalWidth, std::optional<ShapingBoundary>);
    void appendTextFast(const InlineTextItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalWidth); // Reserved for TextOnlySimpleLineBuilder
    void appendAtomicInlineBox(const InlineItem&, const Style::ComputedStyle&, InlineLayoutUnit marginBoxLogicalWidth);
    void appendInlineBoxStart(const InlineItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment);
    void appendInlineBoxEnd(const InlineItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalWidth);
    void appendLineBreak(const InlineItem&, const Style::ComputedStyle&);
    void appendWordBreakOpportunity(const InlineItem&, const Style::ComputedStyle&);
    void appendOutOfFlow(const InlineItem&, const Style::ComputedStyle&);
    void appendBlock(const InlineItem&, InlineLayoutUnit marginBoxLogicalWidth);

    void setContentNeedsBidiReordering() { m_hasNonDefaultBidiLevelRun = true; }

    bool hasContent() const;
    bool hasContentOrDecoration() const;
    bool hasRubyContent() const { return m_hasRubyContent; }

    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }
    InlineLayoutUnit contentLogicalRight() const { return lastRunLogicalRight(); }

    bool contentNeedsBidiReordering() const { return m_hasNonDefaultBidiLevelRun; }
    size_t nonSpanningInlineLevelBoxCount() const { return m_nonSpanningInlineLevelBoxCount; }
    InlineLayoutUnit hangingTrailingContentWidth() const { return m_hangingContent.trailingWidth(); }
    size_t hangingTrailingWhitespaceLength() const { return m_hangingContent.trailingWhitespaceLength(); }
    bool isHangingTrailingContentWhitespace() const { return !!m_hangingContent.trailingWhitespaceLength(); }
    std::optional<size_t> detachHangingTrailingWhitespaceIfApplicable();

    InlineLayoutUnit trimmableTrailingWidth() const { return m_trimmableTrailingContent.width(); }
    bool isTrailingRunFullyTrimmable() const { return m_trimmableTrailingContent.isTrailingRunFullyTrimmable(); }

    std::optional<InlineLayoutUnit> trailingSoftHyphenWidth() const { return m_trailingSoftHyphenWidth; }
    void LEGACY_NODELETE addTrailingHyphen(InlineLayoutUnit hyphenLogicalWidth);

    enum class TrailingContentAction : uint8_t { Remove, Preserve };
    InlineLayoutUnit handleTrailingTrimmableContent(TrailingContentAction);
    void handleTrailingHangingContent(std::optional<IntrinsicWidthMode>, InlineLayoutUnit horizontalAvailableSpace, bool isLastFormattedLine);
    void handleOverflowingNonBreakingSpace(TrailingContentAction, InlineLayoutUnit overflowingWidth);
    const Box* LEGACY_NODELETE removeOverflowingOutOfFlowContent();
    void resetBidiLevelForTrailingWhitespace(UBiDiLevel rootBidiLevel);

    using Run = LineRun;
    using RunList = LineRunList;
    const RunList& runs() const LIFETIME_BOUND { return m_runs; }
    RunList& runs() LIFETIME_BOUND { return m_runs; }
    void inflateContentLogicalWidth(InlineLayoutUnit delta) { m_contentLogicalWidth += delta; }
    // FIXME: This is temporary and should be removed when annotation transitions to inline box structure.
    void adjustContentRightWithRubyAlign(InlineLayoutUnit offset) { m_rubyAlignContentRightOffset = offset; }

    using InlineBoxListWithClonedDecorationEnd = Vector<const Box*>;
    const InlineBoxListWithClonedDecorationEnd& inlineBoxListWithClonedDecorationEnd() const LIFETIME_BOUND { return m_inlineBoxListWithClonedDecorationEnd; }

    using Result = LineResult;
    Result close(TrailingContentAction hangingTrailingWhitespace = TrailingContentAction::Preserve);

    static bool restoreTrimmedTrailingWhitespace(InlineLayoutUnit trimmedTrailingWhitespaceWidth, RunList&, InlineItemRange, const InlineItemList&);
    static bool LEGACY_NODELETE hasTrailingForcedLineBreak(const RunList&);

private:
    InlineLayoutUnit lastRunLogicalRight() const { return m_runs.isEmpty() ? 0.0f : m_runs.last().logicalRight(); }

    void LEGACY_NODELETE resetTrailingContent();
    void removeHangingTrailingWhitespace();

    bool lineHasVisuallyNonEmptyContent() const;

    bool isFirstFormattedLine() const { return m_isFirstFormattedLine; }
    const InlineFormattingContext& LEGACY_NODELETE formattingContext() const LIFETIME_BOUND;
    static bool appendTrailingInlineItemAsTrailingRun(RunList&, InlineLayoutUnit trimmedTrailingWhitespaceWidth, InlineItemRange, const InlineItemList&);

    struct TrimmableTrailingContent {
        TrimmableTrailingContent(RunList&);

        void LEGACY_NODELETE addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableContentOffset, InlineLayoutUnit trimmableWidth);
        void LEGACY_NODELETE addPartiallyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth);
        InlineLayoutUnit remove();
        InlineLayoutUnit removePartiallyTrimmableContent();

        InlineLayoutUnit width() const { return m_fullyTrimmableWidth + m_partiallyTrimmableWidth; }
        bool isEmpty() const { return !m_firstTrimmableRunIndex.has_value(); }
        bool isTrailingRunFullyTrimmable() const { return m_hasFullyTrimmableContent; }
        bool isTrailingRunPartiallyTrimmable() const { return m_partiallyTrimmableWidth; }

        void reset();

    private:
        RunList& m_runs;
        std::optional<size_t> m_firstTrimmableRunIndex;
        bool m_hasFullyTrimmableContent { false };
        InlineLayoutUnit m_trimmableContentOffset { 0 };
        InlineLayoutUnit m_fullyTrimmableWidth { 0 };
        InlineLayoutUnit m_partiallyTrimmableWidth { 0 };
    };

    struct HangingContent {
        void setLeadingPunctuation(InlineLayoutUnit logicalWidth) { m_leadingPunctuationWidth = logicalWidth; }
        void setTrailingPunctuation(InlineLayoutUnit logicalWidth);
        void setTrailingStopOrComma(InlineLayoutUnit logicalWidth, bool isConditional);
        void setTrailingWhitespace(size_t length, InlineLayoutUnit logicalWidth);

        void resetTrailingContent() { m_trailingContent = { }; }
        InlineLayoutUnit trailingWidth() const { return m_trailingContent ? m_trailingContent->width : 0.f; }
        InlineLayoutUnit trailingWhitespaceWidth() const { return m_trailingContent && m_trailingContent->type == TrailingContent::Type::Whitespace ? m_trailingContent->width : 0.f; }

        InlineLayoutUnit leadingPunctuationWidth() const { return m_leadingPunctuationWidth; }
        InlineLayoutUnit width() const { return m_leadingPunctuationWidth + trailingWidth(); }

        size_t length() const;
        size_t trailingWhitespaceLength() const { return m_trailingContent && m_trailingContent->type == TrailingContent::Type::Whitespace ? m_trailingContent->length : 0; }

        bool isTrailingContentPunctuation() const { return m_trailingContent && m_trailingContent->type == TrailingContent::Type::Punctuation; }
        bool isTrailingContentConditional() const { return m_trailingContent && m_trailingContent->isConditional == TrailingContent::IsConditional::Yes; }
        bool isTrailingContentConditionalWhenFollowedByForcedLineBreak() const { return m_trailingContent && m_trailingContent->isConditional == TrailingContent::IsConditional::WhenFollowedByForcedLineBreak; }

    private:
        InlineLayoutUnit m_leadingPunctuationWidth { 0.f };
        // There's either a whitespace or punctuation trailing content.
        struct TrailingContent {
            enum class Type : uint8_t { Whitespace, StopOrComma, Punctuation };
            Type type { Type::Whitespace };

            enum class IsConditional : uint8_t { Yes, No, WhenFollowedByForcedLineBreak };
            IsConditional isConditional { IsConditional::No };
            size_t length { 0 };
            InlineLayoutUnit width { 0.f };
        };
        std::optional<TrailingContent> m_trailingContent { };
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    RunList m_runs;
    TrimmableTrailingContent m_trimmableTrailingContent;
    HangingContent m_hangingContent;
    InlineLayoutUnit m_contentLogicalWidth { 0 };
    size_t m_nonSpanningInlineLevelBoxCount { 0 };
    std::optional<InlineLayoutUnit> m_trailingSoftHyphenWidth { };
    InlineBoxListWithClonedDecorationEnd m_inlineBoxListWithClonedDecorationEnd;
    bool m_hasNonDefaultBidiLevelRun { false };
    bool m_isFirstFormattedLine { false };
    bool m_hasRubyContent { false };
    InlineLayoutUnit m_rubyAlignContentRightOffset { 0.f };
    Vector<InlineLayoutUnit> m_inlineBoxLogicalLeftStack;
};

inline bool Line::hasContent() const
{
    if (m_runs.isEmpty())
        return false;
    for (auto& run : m_runs | std::views::reverse) {
        if (run.isContentful() && !run.isListMarker())
            return true;
    }
    return false;
}

inline void Line::TrimmableTrailingContent::reset()
{
    m_hasFullyTrimmableContent = false;
    m_firstTrimmableRunIndex = { };
    m_fullyTrimmableWidth = { };
    m_partiallyTrimmableWidth = { };
    m_trimmableContentOffset = { };
}

inline void Line::HangingContent::setTrailingPunctuation(InlineLayoutUnit logicalWidth)
{
    m_trailingContent = { TrailingContent::Type::Punctuation, TrailingContent::IsConditional::No, 1, logicalWidth };
}

inline void Line::HangingContent::setTrailingStopOrComma(InlineLayoutUnit logicalWidth, bool isConditional)
{
    m_trailingContent = { TrailingContent::Type::StopOrComma, isConditional ? TrailingContent::IsConditional::Yes : TrailingContent::IsConditional::No, 1, logicalWidth };
}

inline void Line::HangingContent::setTrailingWhitespace(size_t length, InlineLayoutUnit logicalWidth)
{
    // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
    // by a forced line break, in which case it must conditionally hang the sequence is instead.
    // Note that end of last line in a paragraph is considered a forced break.
    m_trailingContent = { TrailingContent::Type::Whitespace, TrailingContent::IsConditional::WhenFollowedByForcedLineBreak, length, logicalWidth };
}

inline size_t Line::HangingContent::length() const
{
    size_t length = 0;
    if (m_leadingPunctuationWidth)
        ++length;
    if (m_trailingContent)
        length += m_trailingContent->length;
    return length;
}

}
}
