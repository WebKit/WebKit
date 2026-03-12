/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/InlineDisplayBox.h>
#include <WebCore/InlineItem.h>
#include <WebCore/InlineLineTypes.h>
#include <WebCore/InlineTextItem.h>
#include <WebCore/RenderStyle.h>
#include <ranges>
#include <unicode/ubidi.h>
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

struct ExpansionInfo;
class InlineContentAligner;
class InlineFormattingContext;
class InlineSoftLineBreakItem;
enum class IntrinsicWidthMode;

class Line {
public:
    Line(const InlineFormattingContext&);

    void initialize(const Vector<InlineItem, 1>& lineSpanningInlineBoxes, bool isFirstFormattedLine);

    enum class ShapingBoundary : uint8_t { NotApplicable, Start, Inside, End };
    void appendText(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit logicalWidth, std::optional<ShapingBoundary>);
    void appendTextFast(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit logicalWidth); // Reserved for TextOnlySimpleLineBuilder
    void appendAtomicInlineBox(const InlineItem&, const RenderStyle&, InlineLayoutUnit marginBoxLogicalWidth);
    void appendInlineBoxStart(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment);
    void appendInlineBoxEnd(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
    void appendLineBreak(const InlineItem&, const RenderStyle&);
    void appendWordBreakOpportunity(const InlineItem&, const RenderStyle&);
    void appendOutOfFlow(const InlineItem&, const RenderStyle&);
    void appendBlock(const InlineItem&, InlineLayoutUnit marginBoxLogicalWidth);

    void setContentNeedsBidiReordering() { m_hasNonDefaultBidiLevelRun = true; }

    enum class IncludeInsideListMarker : bool { No, Yes };
    bool hasContent(IncludeInsideListMarker = IncludeInsideListMarker::No) const;
    bool hasContentOrDecoration(IncludeInsideListMarker = IncludeInsideListMarker::No) const;
    bool hasRubyContent() const { return m_hasRubyContent; }

    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }
    InlineLayoutUnit contentLogicalRight() const { return lastRunLogicalRight(); }

    bool contentNeedsBidiReordering() const { return m_hasNonDefaultBidiLevelRun; }
    size_t nonSpanningInlineLevelBoxCount() const { return m_nonSpanningInlineLevelBoxCount; }
    InlineLayoutUnit hangingTrailingContentWidth() const { return m_hangingContent.trailingWidth(); }
    size_t hangingTrailingWhitespaceLength() const { return m_hangingContent.trailingWhitespaceLength(); }
    bool isHangingTrailingContentWhitespace() const { return !!m_hangingContent.trailingWhitespaceLength(); }

    InlineLayoutUnit trimmableTrailingWidth() const { return m_trimmableTrailingContent.width(); }
    bool isTrailingRunFullyTrimmable() const { return m_trimmableTrailingContent.isTrailingRunFullyTrimmable(); }

    std::optional<InlineLayoutUnit> trailingSoftHyphenWidth() const { return m_trailingSoftHyphenWidth; }
    void addTrailingHyphen(InlineLayoutUnit hyphenLogicalWidth);

    enum class TrailingContentAction : uint8_t { Remove, Preserve };
    InlineLayoutUnit handleTrailingTrimmableContent(TrailingContentAction);
    void handleTrailingHangingContent(std::optional<IntrinsicWidthMode>, InlineLayoutUnit horizontalAvailableSpace, bool isLastFormattedLine);
    void handleOverflowingNonBreakingSpace(TrailingContentAction, InlineLayoutUnit overflowingWidth);
    const Box* removeOverflowingOutOfFlowContent();
    void resetBidiLevelForTrailingWhitespace(UBiDiLevel rootBidiLevel);

    struct Run {
        enum class Type : uint8_t {
            Text,
            NonBreakingSpace,
            WordSeparator,
            HardLineBreak,
            SoftLineBreak,
            WordBreakOpportunity,
            AtomicInlineBox,
            ListMarkerInside,
            ListMarkerOutside,
            InlineBoxStart,
            InlineBoxEnd,
            LineSpanningInlineBoxStart,
            OutOfFlow,
            Block
        };

        bool isText() const { return m_type == Type::Text || isWordSeparator() || isNonBreakingSpace(); }
        bool isNonBreakingSpace() const { return m_type == Type::NonBreakingSpace; }
        bool isWordSeparator() const { return m_type == Type::WordSeparator; }
        bool isAtomicInlineBox() const { return m_type == Type::AtomicInlineBox; }
        bool isListMarker() const { return isListMarkerInside() || isListMarkerOutside(); }
        bool isListMarkerInside() const { return m_type == Type::ListMarkerInside; }
        bool isListMarkerOutside() const { return m_type == Type::ListMarkerOutside; }
        bool isLineBreak() const { return isHardLineBreak() || isSoftLineBreak(); }
        bool isSoftLineBreak() const  { return m_type == Type::SoftLineBreak; }
        bool isHardLineBreak() const { return m_type == Type::HardLineBreak; }
        bool isWordBreakOpportunity() const { return m_type == Type::WordBreakOpportunity; }
        bool isInlineBox() const { return isInlineBoxStart() || isLineSpanningInlineBoxStart() || isInlineBoxEnd(); }
        bool isInlineBoxStart() const { return m_type == Type::InlineBoxStart; }
        bool isLineSpanningInlineBoxStart() const { return m_type == Type::LineSpanningInlineBoxStart; }
        bool isInlineBoxEnd() const { return m_type == Type::InlineBoxEnd; }
        bool isOutOfFlow() const { return m_type == Type::OutOfFlow; }
        bool isBlock() const { return m_type == Type::Block; }

        bool isContentful() const { return (isText() && textContent().length) || isAtomicInlineBox() || isLineBreak() || isListMarker() || isBlock(); }
        static bool isContentfulOrHasDecoration(const Run&, const InlineFormattingContext&);

        const Box& layoutBox() const { return *m_layoutBox; }
        struct Text {
            size_t start { 0 };
            size_t length { 0 };
            bool needsHyphen { false };
        };
        const Text& textContent() const LIFETIME_BOUND { return m_textContent; }

        InlineLayoutUnit logicalWidth() const { return m_logicalWidth; }
        InlineLayoutUnit logicalLeft() const { return m_logicalLeft; }
        InlineLayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

        const InlineDisplay::Box::Expansion& expansion() const LIFETIME_BOUND { return m_expansion; }

        bool hasTrailingWhitespace() const { return m_trailingWhitespace.type != TrailingWhitespace::Type::NotApplicable; }
        InlineLayoutUnit trailingWhitespaceWidth() const { return m_trailingWhitespace.width; }
        bool isWhitespaceOnly() const { return hasTrailingWhitespace() && m_trailingWhitespace.length == m_textContent.length; }

        struct GlyphOverflow {
            bool isEmpty() const { return !top && !bottom; }

            uint8_t top : 5 { 0 };
            uint8_t bottom : 3 { 0 };
        };
        GlyphOverflow glyphOverflow() const { return m_glyphOverflow; }

        inline TextDirection inlineDirection() const;
        InlineLayoutUnit NODELETE letterSpacing() const;
        bool NODELETE hasTextCombine() const;
        InlineLayoutUnit textSpacingAdjustment() const { return m_textSpacingAdjustment; }

        UBiDiLevel bidiLevel() const { return m_bidiLevel; }

        bool isShapingBoundaryStart() const { return m_shapingBoundary == Line::ShapingBoundary::Start; }
        bool isShapingBoundaryEnd() const { return m_shapingBoundary == Line::ShapingBoundary::End; }
        bool isInsideShapingBoundary() const { return m_shapingBoundary == Line::ShapingBoundary::Inside; }
        bool isShapingBoundary() const { return m_shapingBoundary != Line::ShapingBoundary::NotApplicable; }

        // FIXME: Maybe add create functions intead?
        Run(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalLeft);
        Run(const InlineItem& lineSpanningInlineBoxItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0.f);
        Run(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0.f, std::optional<Line::ShapingBoundary> = std::nullopt);

    private:
        friend class Line;
        friend class InlineContentAligner;
        friend class RubyFormattingContext;

        Run(const InlineSoftLineBreakItem&, const RenderStyle&, InlineLayoutUnit logicalLeft);
        Run(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0.f);

        const RenderStyle& style() const LIFETIME_BOUND { return m_style; }
        void expand(const InlineTextItem&, InlineLayoutUnit logicalWidth);
        void moveHorizontally(InlineLayoutUnit offset) { m_logicalLeft += offset; }
        void shrinkHorizontally(InlineLayoutUnit width) { m_logicalWidth -= width; }
        void setExpansion(InlineDisplay::Box::Expansion expansion) { m_expansion = expansion; }
        void setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth);
        void setBidiLevel(UBiDiLevel bidiLevel) { m_bidiLevel = bidiLevel; }

        struct TrailingWhitespace {
            enum class Type : uint8_t {
                NotApplicable,
                NotCollapsible,
                Collapsible,
                Collapsed
            };
            Type type { Type::NotApplicable };
            size_t length : 24 { 0 };
            InlineLayoutUnit width { 0.f };
        };
        bool hasCollapsibleTrailingWhitespace() const { return hasTrailingWhitespace() && (m_trailingWhitespace.type == TrailingWhitespace::Type::Collapsible || hasCollapsedTrailingWhitespace()); }
        bool hasCollapsedTrailingWhitespace() const { return hasTrailingWhitespace() && m_trailingWhitespace.type == TrailingWhitespace::Type::Collapsed; }
        static std::optional<TrailingWhitespace::Type> trailingWhitespaceType(const InlineTextItem&);
        InlineLayoutUnit removeTrailingWhitespace();

        std::optional<Run> NODELETE detachTrailingWhitespace();

        bool NODELETE hasTrailingLetterSpacing() const;
        InlineLayoutUnit NODELETE trailingLetterSpacing() const;
        InlineLayoutUnit removeTrailingLetterSpacing();

        TrailingWhitespace m_trailingWhitespace { };
        Type m_type { Type::Text };
        Line::ShapingBoundary m_shapingBoundary { Line::ShapingBoundary::NotApplicable };
        InlineLayoutUnit m_logicalLeft { 0 };
        Markable<size_t> m_lastNonWhitespaceContentStart { };
        InlineLayoutUnit m_logicalWidth { 0 };
        UBiDiLevel m_bidiLevel { UBIDI_DEFAULT_LTR };
        InlineLayoutUnit m_textSpacingAdjustment { 0 };
        GlyphOverflow m_glyphOverflow;
        const Box* m_layoutBox { nullptr };
        const RenderStyle& m_style;
        InlineDisplay::Box::Expansion m_expansion;
        Text m_textContent;
    };
    using RunList = Vector<Run, 1>;
    const RunList& runs() const LIFETIME_BOUND { return m_runs; }
    RunList& runs() LIFETIME_BOUND { return m_runs; }
    void inflateContentLogicalWidth(InlineLayoutUnit delta) { m_contentLogicalWidth += delta; }
    // FIXME: This is temporary and should be removed when annotation transitions to inline box structure.
    void adjustContentRightWithRubyAlign(InlineLayoutUnit offset) { m_rubyAlignContentRightOffset = offset; }

    using InlineBoxListWithClonedDecorationEnd = Vector<const Box*>;
    const InlineBoxListWithClonedDecorationEnd& inlineBoxListWithClonedDecorationEnd() const LIFETIME_BOUND { return m_inlineBoxListWithClonedDecorationEnd; }

    struct Result {
        RunList runs;
        InlineLayoutUnit contentLogicalWidth { 0.f };
        InlineLayoutUnit contentLogicalRight { 0.f };
        bool isContentful { false };
        bool isHangingTrailingContentWhitespace { false };
        InlineLayoutUnit hangingTrailingContentWidth { 0.f };
        InlineLayoutUnit hangablePunctuationStartWidth { 0.f };
        bool contentNeedsBidiReordering { false };
        size_t nonSpanningInlineLevelBoxCount { 0 };
    };
    Result close();

    static bool restoreTrimmedTrailingWhitespace(InlineLayoutUnit trimmedTrailingWhitespaceWidth, RunList&, InlineItemRange, const InlineItemList&);
    static bool NODELETE hasTrailingForcedLineBreak(const RunList&);

private:
    InlineLayoutUnit lastRunLogicalRight() const { return m_runs.isEmpty() ? 0.0f : m_runs.last().logicalRight(); }

    void NODELETE resetTrailingContent();

    bool lineHasVisuallyNonEmptyContent() const;

    bool isFirstFormattedLine() const { return m_isFirstFormattedLine; }
    const InlineFormattingContext& NODELETE formattingContext() const LIFETIME_BOUND;
    static bool appendTrailingInlineItemAsTrailingRun(RunList&, InlineLayoutUnit trimmedTrailingWhitespaceWidth, InlineItemRange, const InlineItemList&);

    struct TrimmableTrailingContent {
        TrimmableTrailingContent(RunList&);

        void addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableContentOffset, InlineLayoutUnit trimmableWidth);
        void addPartiallyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth);
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

inline bool Line::hasContent(IncludeInsideListMarker includeInsideListMarker) const
{
    if (m_runs.isEmpty())
        return false;
    if (includeInsideListMarker == IncludeInsideListMarker::Yes && m_runs.first().isListMarkerInside())
        return true;
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

inline void Line::Run::setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth)
{
    ASSERT(isText());
    m_textContent.needsHyphen = true;
    m_logicalWidth += hyphenLogicalWidth;
}

inline TextDirection Line::Run::inlineDirection() const
{
    return m_style.writingMode().bidiDirection();
}

}
}
