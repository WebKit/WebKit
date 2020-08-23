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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayRun.h"
#include "InlineItem.h"
#include "InlineLineBox.h"
#include "InlineTextItem.h"

namespace WebCore {
namespace Layout {

struct HangingContent;
class InlineFormattingContext;
class InlineSoftLineBreakItem;

class LineBuilder {
    struct ContinuousContent;

public:
    struct Constraints {
        InlineLayoutPoint logicalTopLeft;
        InlineLayoutUnit availableLogicalWidth { 0 };
        bool lineIsConstrainedByFloat { false };
        struct HeightAndBaseline {
            InlineLayoutUnit height { 0 };
            InlineLayoutUnit baselineOffset { 0 };
            Optional<LineBox::Baseline> strut;
        };
        Optional<HeightAndBaseline> heightAndBaseline;
    };

    enum class IntrinsicSizing { No, Yes };
    LineBuilder(const InlineFormattingContext&, Optional<TextAlignMode>, IntrinsicSizing);
    ~LineBuilder();

    void initialize(const Constraints&);
    void append(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendPartialTrailingTextItem(const InlineTextItem&, InlineLayoutUnit logicalWidth, bool needsHypen);
    void resetContent();
    bool isVisuallyEmpty() const { return m_lineBox.isConsideredEmpty(); }
    bool hasIntrusiveFloat() const { return m_hasIntrusiveFloat; }
    InlineLayoutUnit availableWidth() const { return logicalWidth() - contentLogicalWidth(); }

    InlineLayoutUnit trimmableTrailingWidth() const { return m_trimmableTrailingContent.width(); }
    bool isTrailingRunFullyTrimmable() const { return m_trimmableTrailingContent.isTrailingRunFullyTrimmable(); }

    const LineBox& lineBox() const { return m_lineBox; }
    void moveLogicalLeft(InlineLayoutUnit);
    void moveLogicalRight(InlineLayoutUnit);
    void setHasIntrusiveFloat() { m_hasIntrusiveFloat = true; }

    struct Run {
        bool isText() const { return m_type == InlineItem::Type::Text; }
        bool isBox() const { return m_type == InlineItem::Type::Box; }
        bool isLineBreak() const { return m_type == InlineItem::Type::HardLineBreak || m_type == InlineItem::Type::SoftLineBreak; }
        bool isContainerStart() const { return m_type == InlineItem::Type::ContainerStart; }
        bool isContainerEnd() const { return m_type == InlineItem::Type::ContainerEnd; }

        const Box& layoutBox() const { return *m_layoutBox; }
        const RenderStyle& style() const { return m_layoutBox->style(); }
        const Display::InlineRect& logicalRect() const { return m_logicalRect; }
        Display::Run::Expansion expansion() const { return m_expansion; }
        const Optional<Display::Run::TextContent>& textContent() const { return m_textContent; }

        Run(Run&&) = default;
        Run& operator=(Run&& other) = default;

    private:
        friend class LineBuilder;

        Run(const InlineTextItem&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, bool needsHypen);
        Run(const InlineSoftLineBreakItem&, InlineLayoutUnit logicalLeft);
        Run(const InlineItem&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);

        void expand(const InlineTextItem&, InlineLayoutUnit logicalWidth);

        InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }

        void moveHorizontally(InlineLayoutUnit offset) { m_logicalRect.moveHorizontally(offset); }
        void shrinkHorizontally(InlineLayoutUnit width) { m_logicalRect.expandHorizontally(-width); }

        void adjustLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
        void moveVertically(InlineLayoutUnit offset) { m_logicalRect.moveVertically(offset); }
        void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(logicalHeight); }

        bool hasExpansionOpportunity() const { return m_expansionOpportunityCount; }
        ExpansionBehavior expansionBehavior() const;
        unsigned expansionOpportunityCount() const { return m_expansionOpportunityCount; }
        void setComputedHorizontalExpansion(InlineLayoutUnit logicalExpansion);
        void setExpansionBehavior(ExpansionBehavior);

        void setNeedsHyphen() { m_textContent->setNeedsHyphen(); }

        enum class TrailingWhitespace {
            None,
            NotCollapsible,
            Collapsible,
            Collapsed
        };
        bool hasTrailingWhitespace() const { return m_trailingWhitespaceType != TrailingWhitespace::None; }
        bool hasCollapsibleTrailingWhitespace() const { return m_trailingWhitespaceType == TrailingWhitespace::Collapsible || hasCollapsedTrailingWhitespace(); }
        bool hasCollapsedTrailingWhitespace() const { return m_trailingWhitespaceType == TrailingWhitespace::Collapsed; }
        InlineLayoutUnit trailingWhitespaceWidth() const { return m_trailingWhitespaceWidth; }
        TrailingWhitespace trailingWhitespaceType(const InlineTextItem&) const;
        void removeTrailingWhitespace();
        void visuallyCollapseTrailingWhitespace();

        bool hasTrailingLetterSpacing() const;
        InlineLayoutUnit trailingLetterSpacing() const;
        void removeTrailingLetterSpacing();

        InlineItem::Type m_type { InlineItem::Type::Text };
        const Box* m_layoutBox { nullptr };
        Display::InlineRect m_logicalRect;
        TrailingWhitespace m_trailingWhitespaceType { TrailingWhitespace::None };
        InlineLayoutUnit m_trailingWhitespaceWidth { 0 };
        Optional<Display::Run::TextContent> m_textContent;
        Display::Run::Expansion m_expansion;
        unsigned m_expansionOpportunityCount { 0 };
    };
    using RunList = Vector<Run, 10>;
    enum class IsLastLineWithInlineContent { No, Yes };
    RunList close(IsLastLineWithInlineContent = IsLastLineWithInlineContent::No);

    static LineBox::Baseline halfLeadingMetrics(const FontMetrics&, InlineLayoutUnit lineLogicalHeight);

private:
    InlineLayoutUnit logicalTop() const { return m_lineBox.logicalTop(); }
    InlineLayoutUnit logicalBottom() const { return m_lineBox.logicalBottom(); }

    InlineLayoutUnit logicalLeft() const { return m_lineBox.logicalLeft(); }
    InlineLayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

    InlineLayoutUnit logicalWidth() const { return m_lineLogicalWidth; }
    InlineLayoutUnit logicalHeight() const { return m_lineBox.logicalHeight(); }

    InlineLayoutUnit contentLogicalWidth() const { return m_lineBox.logicalWidth(); }
    InlineLayoutUnit contentLogicalRight() const { return m_lineBox.logicalRight(); }
    InlineLayoutUnit baselineOffset() const { return m_lineBox.baselineOffset(); }

    struct InlineRunDetails {
        InlineLayoutUnit logicalWidth { 0 };
        bool needsHyphen { false };
    };
    void appendWith(const InlineItem&, const InlineRunDetails&);
    void appendNonBreakableSpace(const InlineItem&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
    void appendTextContent(const InlineTextItem&, InlineLayoutUnit logicalWidth, bool needsHyphen);
    void appendNonReplacedInlineBox(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendReplacedInlineBox(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendInlineContainerStart(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendInlineContainerEnd(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendLineBreak(const InlineItem&);

    void removeTrailingTrimmableContent();
    void visuallyCollapsePreWrapOverflowContent();
    HangingContent collectHangingContent(IsLastLineWithInlineContent);
    void alignHorizontally(const HangingContent&, IsLastLineWithInlineContent);
    void alignContentVertically();

    void adjustBaselineAndLineHeight(const Run&, LineBox& parentInlineBox, const LineBox::Baseline&);
    InlineLayoutUnit runContentHeight(const Run&) const;

    void justifyRuns(InlineLayoutUnit availableWidth);

    bool isVisuallyNonEmpty(const Run&) const;

    LayoutState& layoutState() const;
    const InlineFormattingContext& formattingContext() const;

    struct TrimmableTrailingContent {
        TrimmableTrailingContent(RunList&);

        void addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth);
        void addPartiallyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth);
        InlineLayoutUnit remove();
        InlineLayoutUnit removePartiallyTrimmableContent();

        InlineLayoutUnit width() const { return m_fullyTrimmableWidth + m_partiallyTrimmableWidth; }
        bool isEmpty() const { return !m_firstRunIndex.hasValue(); }
        bool isTrailingRunFullyTrimmable() const { return m_hasFullyTrimmableContent; }
        bool isTrailingRunPartiallyTrimmable() const { return m_partiallyTrimmableWidth; }

        void reset();

    private:
        RunList& m_runs;
        Optional<size_t> m_firstRunIndex;
        bool m_hasFullyTrimmableContent { false };
        InlineLayoutUnit m_fullyTrimmableWidth { 0 };
        InlineLayoutUnit m_partiallyTrimmableWidth { 0 };
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    RunList m_runs;
    TrimmableTrailingContent m_trimmableTrailingContent;
    Optional<LineBox::Baseline> m_initialStrut;
    InlineLayoutUnit m_lineLogicalWidth { 0 };
    Optional<TextAlignMode> m_horizontalAlignment;
    bool m_isIntrinsicSizing { false };
    bool m_hasIntrusiveFloat { false };
    LineBox m_lineBox;
    Optional<bool> m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent;
    bool m_shouldIgnoreTrailingLetterSpacing { false };
};

inline void LineBuilder::TrimmableTrailingContent::reset()
{
    m_hasFullyTrimmableContent = false;
    m_firstRunIndex = { };
    m_fullyTrimmableWidth = { };
    m_partiallyTrimmableWidth = { };
}

inline LineBuilder::Run::TrailingWhitespace LineBuilder::Run::trailingWhitespaceType(const InlineTextItem& inlineTextItem) const
{
    if (!inlineTextItem.isWhitespace())
        return TrailingWhitespace::None;
    if (!inlineTextItem.isCollapsible())
        return TrailingWhitespace::NotCollapsible;
    if (inlineTextItem.length() == 1)
        return TrailingWhitespace::Collapsible;
    return TrailingWhitespace::Collapsed;
}

}
}
#endif
