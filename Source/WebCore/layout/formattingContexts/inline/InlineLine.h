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

#include "InlineDisplayBox.h"
#include "InlineItem.h"
#include "InlineTextItem.h"
#include <unicode/ubidi.h>

namespace WebCore {
namespace Layout {

class InlineFormattingContext;
class InlineSoftLineBreakItem;

class Line {
public:
    Line(const InlineFormattingContext&);
    ~Line();

    void initialize(const Vector<InlineItem>& lineSpanningInlineBoxes);

    void append(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);

    bool hasContent() const;
    bool isContentTruncated() const { return m_contentIsTruncated; }

    bool contentNeedsBidiReordering() const { return m_hasNonDefaultBidiLevelRun; }

    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }
    InlineLayoutUnit contentLogicalRight() const { return lastRunLogicalRight() + m_clonedEndDecorationWidthForInlineBoxRuns; }
    size_t nonSpanningInlineLevelBoxCount() const { return m_nonSpanningInlineLevelBoxCount; }

    InlineLayoutUnit trimmableTrailingWidth() const { return m_trimmableTrailingContent.width(); }
    bool isTrailingRunFullyTrimmable() const { return m_trimmableTrailingContent.isTrailingRunFullyTrimmable(); }

    InlineLayoutUnit hangingTrailingContentWidth() const { return m_hangingTrailingContent.width(); }

    std::optional<InlineLayoutUnit> trailingSoftHyphenWidth() const { return m_trailingSoftHyphenWidth; }
    void addTrailingHyphen(InlineLayoutUnit hyphenLogicalWidth);

    enum class TrailingContentAction : uint8_t { Remove, Preserve };
    void handleTrailingTrimmableContent(TrailingContentAction);
    void handleOverflowingNonBreakingSpace(TrailingContentAction, InlineLayoutUnit overflowingWidth);
    void removeHangingGlyphs();
    void resetBidiLevelForTrailingWhitespace(UBiDiLevel rootBidiLevel);
    void applyRunExpansion(InlineLayoutUnit horizontalAvailableSpace);
    void truncate(InlineLayoutUnit logicalRight);

    struct Run {
        enum class Type : uint8_t {
            Text,
            NonBreakingSpace,
            WordSeparator,
            HardLineBreak,
            SoftLineBreak,
            WordBreakOpportunity,
            GenericInlineLevelBox,
            ListMarker,
            InlineBoxStart,
            InlineBoxEnd,
            LineSpanningInlineBoxStart
        };

        bool isText() const { return m_type == Type::Text || isWordSeparator() || isNonBreakingSpace(); }
        bool isNonBreakingSpace() const { return m_type == Type::NonBreakingSpace; }
        bool isWordSeparator() const { return m_type == Type::WordSeparator; }
        bool isBox() const { return m_type == Type::GenericInlineLevelBox; }
        bool isListMarker() const { return m_type == Type::ListMarker; }
        bool isLineBreak() const { return isHardLineBreak() || isSoftLineBreak(); }
        bool isSoftLineBreak() const  { return m_type == Type::SoftLineBreak; }
        bool isHardLineBreak() const { return m_type == Type::HardLineBreak; }
        bool isWordBreakOpportunity() const { return m_type == Type::WordBreakOpportunity; }
        bool isInlineBox() const { return isInlineBoxStart() || isLineSpanningInlineBoxStart() || isInlineBoxEnd(); }
        bool isInlineBoxStart() const { return m_type == Type::InlineBoxStart; }
        bool isLineSpanningInlineBoxStart() const { return m_type == Type::LineSpanningInlineBoxStart; }
        bool isInlineBoxEnd() const { return m_type == Type::InlineBoxEnd; }

        bool isContentful() const { return (isText() && textContent()->length) || isBox() || isLineBreak() || isListMarker(); }
        bool isGenerated() const { return isListMarker(); }

        bool isTruncated() const { return m_isTruncated; }

        const Box& layoutBox() const { return *m_layoutBox; }
        struct Text {
            size_t start { 0 };
            size_t length { 0 };
            struct PartiallyVisibleContent {
                size_t length { 0 };
                InlineLayoutUnit width { 0.f };
            };
            std::optional<PartiallyVisibleContent> partiallyVisibleContent { };
            bool needsHyphen { false };
        };
        const std::optional<Text>& textContent() const { return m_textContent; }

        InlineLayoutUnit logicalWidth() const { return m_logicalWidth; }
        InlineLayoutUnit logicalLeft() const { return m_logicalLeft; }
        InlineLayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

        const InlineDisplay::Box::Expansion& expansion() const { return m_expansion; }

        bool hasTrailingWhitespace() const { return m_trailingWhitespace.has_value(); }
        InlineLayoutUnit trailingWhitespaceWidth() const { return m_trailingWhitespace ? m_trailingWhitespace->width : 0.f; }
        bool isWhitespaceOnly() const { return hasTrailingWhitespace() && m_trailingWhitespace->length == m_textContent->length; }

        bool shouldTrailingWhitespaceHang() const;
        TextDirection inlineDirection() const;
        InlineLayoutUnit letterSpacing() const;
        bool hasTextCombine() const;

        UBiDiLevel bidiLevel() const { return m_bidiLevel; }

    private:
        friend class Line;

        Run(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
        Run(const InlineSoftLineBreakItem&, const RenderStyle&, InlineLayoutUnit logicalLeft);
        Run(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
        Run(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalLeft);
        Run(const InlineItem& lineSpanningInlineBoxItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);

        void expand(const InlineTextItem&, InlineLayoutUnit logicalWidth);
        void moveHorizontally(InlineLayoutUnit offset) { m_logicalLeft += offset; }
        void shrinkHorizontally(InlineLayoutUnit width) { m_logicalWidth -= width; }
        void setExpansion(InlineDisplay::Box::Expansion expansion) { m_expansion = expansion; }
        void setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth);
        void setBidiLevel(UBiDiLevel bidiLevel) { m_bidiLevel = bidiLevel; }

        struct TrailingWhitespace {
            enum class Type {
                NotCollapsible,
                Collapsible,
                Collapsed
            };
            Type type { Type::NotCollapsible };
            InlineLayoutUnit width { 0 };
            size_t length { 0 };
        };
        bool hasCollapsibleTrailingWhitespace() const { return m_trailingWhitespace && (m_trailingWhitespace->type == TrailingWhitespace::Type::Collapsible || hasCollapsedTrailingWhitespace()); }
        bool hasCollapsedTrailingWhitespace() const { return m_trailingWhitespace && m_trailingWhitespace->type == TrailingWhitespace::Type::Collapsed; }
        static std::optional<TrailingWhitespace::Type> trailingWhitespaceType(const InlineTextItem&);
        InlineLayoutUnit removeTrailingWhitespace();

        std::optional<Run> detachTrailingWhitespace();

        bool hasTrailingLetterSpacing() const;
        InlineLayoutUnit trailingLetterSpacing() const;
        InlineLayoutUnit removeTrailingLetterSpacing();
        enum class CanFullyTruncate : uint8_t { Yes, No };
        bool truncate(InlineLayoutUnit truncatedWidth, CanFullyTruncate = CanFullyTruncate::Yes);

        Type m_type { Type::Text };
        const Box* m_layoutBox { nullptr };
        const RenderStyle& m_style;
        InlineLayoutUnit m_logicalLeft { 0 };
        InlineLayoutUnit m_logicalWidth { 0 };
        InlineDisplay::Box::Expansion m_expansion;
        UBiDiLevel m_bidiLevel { UBIDI_DEFAULT_LTR };
        std::optional<TrailingWhitespace> m_trailingWhitespace { };
        std::optional<size_t> m_lastNonWhitespaceContentStart { };
        std::optional<Text> m_textContent;
        bool m_isTruncated { false };
    };
    using RunList = Vector<Run, 10>;
    const RunList& runs() const { return m_runs; }
    using InlineBoxListWithClonedDecorationEnd = HashMap<const Box*, InlineLayoutUnit>;
    const InlineBoxListWithClonedDecorationEnd& inlineBoxListWithClonedDecorationEnd() const { return m_inlineBoxListWithClonedDecorationEnd; }

private:
    InlineLayoutUnit lastRunLogicalRight() const { return m_runs.isEmpty() ? 0.0f : m_runs.last().logicalRight(); }

    void appendTextContent(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
    void appendNonReplacedInlineLevelBox(const InlineItem&, const RenderStyle&, InlineLayoutUnit marginBoxLogicalWidth);
    void appendReplacedInlineLevelBox(const InlineItem&, const RenderStyle&, InlineLayoutUnit marginBoxLogicalWidth);
    void appendInlineBoxStart(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
    void appendInlineBoxEnd(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
    void appendLineBreak(const InlineItem&, const RenderStyle&);
    void appendWordBreakOpportunity(const InlineItem&, const RenderStyle&);

    InlineLayoutUnit addBorderAndPaddingEndForInlineBoxDecorationClone(const InlineItem& inlineBoxStartItem);
    InlineLayoutUnit removeBorderAndPaddingEndForInlineBoxDecorationClone(const InlineItem& inlineBoxEndItem);

    void resetTrailingContent();

    const InlineFormattingContext& formattingContext() const;

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

    struct HangingTrailingContent {
        void add(const InlineTextItem& trailingWhitespace, InlineLayoutUnit logicalWidth);
        void reset();

        size_t length() const { return m_length; }
        InlineLayoutUnit width() const { return m_width; }

    private:
        size_t m_length { 0 };
        InlineLayoutUnit m_width { 0 };
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    RunList m_runs;
    TrimmableTrailingContent m_trimmableTrailingContent;
    HangingTrailingContent m_hangingTrailingContent;
    InlineLayoutUnit m_contentLogicalWidth { 0 };
    size_t m_nonSpanningInlineLevelBoxCount { 0 };
    std::optional<InlineLayoutUnit> m_trailingSoftHyphenWidth { 0 };
    InlineBoxListWithClonedDecorationEnd m_inlineBoxListWithClonedDecorationEnd;
    InlineLayoutUnit m_clonedEndDecorationWidthForInlineBoxRuns { 0 };
    bool m_hasNonDefaultBidiLevelRun { false };
    bool m_contentIsTruncated { false };
};

inline bool Line::hasContent() const
{
    for (auto& run : makeReversedRange(m_runs)) {
        if (run.isContentful() && !run.isGenerated())
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

inline void Line::HangingTrailingContent::reset()
{
    m_width = { };
    m_length = { };
}

inline void Line::Run::setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth)
{
    ASSERT(m_textContent);
    m_textContent->needsHyphen = true;
    m_logicalWidth += hyphenLogicalWidth;
}

inline bool Line::Run::shouldTrailingWhitespaceHang() const
{
    return m_style.whiteSpace() == WhiteSpace::PreWrap;
}

inline TextDirection Line::Run::inlineDirection() const
{
    return m_style.direction();
}

inline InlineLayoutUnit Line::Run::letterSpacing() const
{
    return m_style.letterSpacing();
}

inline bool Line::Run::hasTextCombine() const
{
    return m_style.hasTextCombine();
}

}
}
