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

#include "InlineItem.h"
#include "InlineLineRun.h"
#include "InlineTextItem.h"

namespace WebCore {
namespace Layout {

class InlineFormattingContext;
class InlineSoftLineBreakItem;

class Line {
public:
    Line(const InlineFormattingContext&);
    ~Line();

    void initialize();

    void append(const InlineItem&, InlineLayoutUnit logicalWidth);

    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }
    InlineLayoutUnit contentLogicalRight() const { return m_runs.isEmpty() ? 0.0f : m_runs.last().logicalRight(); }

    InlineLayoutUnit trimmableTrailingWidth() const { return m_trimmableTrailingContent.width(); }
    bool isTrailingRunFullyTrimmable() const { return m_trimmableTrailingContent.isTrailingRunFullyTrimmable(); }

    Optional<InlineLayoutUnit> trailingSoftHyphenWidth() const { return m_trailingSoftHyphenWidth; }
    void addTrailingHyphen(InlineLayoutUnit hyphenLogicalWidth);

    void removeCollapsibleContent(InlineLayoutUnit extraHorizontalSpace);
    void applyRunExpansion(InlineLayoutUnit extraHorizontalSpace);

    struct Run {
        bool isText() const { return m_type == InlineItem::Type::Text; }
        bool isBox() const { return m_type == InlineItem::Type::Box; }
        bool isLineBreak() const { return isHardLineBreak() || isSoftLineBreak(); }
        bool isSoftLineBreak() const  { return m_type == InlineItem::Type::SoftLineBreak; }
        bool isHardLineBreak() const { return m_type == InlineItem::Type::HardLineBreak; }
        bool isWordBreakOpportunity() const { return m_type == InlineItem::Type::WordBreakOpportunity; }
        bool isInlineBoxStart() const { return m_type == InlineItem::Type::InlineBoxStart; }
        bool isInlineBoxEnd() const { return m_type == InlineItem::Type::InlineBoxEnd; }

        const Box& layoutBox() const { return *m_layoutBox; }
        const RenderStyle& style() const { return m_layoutBox->style(); }
        const Optional<LineRun::Text>& textContent() const { return m_textContent; }

        InlineLayoutUnit logicalWidth() const { return m_logicalWidth; }
        InlineLayoutUnit logicalLeft() const { return m_logicalLeft; }
        InlineLayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

        const LineRun::Expansion& expansion() const { return m_expansion; }
        bool hasExpansionOpportunity() const { return m_expansionOpportunityCount; }
        ExpansionBehavior expansionBehavior() const;
        unsigned expansionOpportunityCount() const { return m_expansionOpportunityCount; }

        bool hasTrailingWhitespace() const { return m_trailingWhitespaceType != TrailingWhitespace::None; }
        InlineLayoutUnit trailingWhitespaceWidth() const { return m_trailingWhitespaceWidth; }

    private:
        friend class Line;

        Run(const InlineTextItem&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
        Run(const InlineSoftLineBreakItem&, InlineLayoutUnit logicalLeft);
        Run(const InlineItem&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);

        void expand(const InlineTextItem&, InlineLayoutUnit logicalWidth);
        void moveHorizontally(InlineLayoutUnit offset) { m_logicalLeft += offset; }
        void shrinkHorizontally(InlineLayoutUnit width) { m_logicalWidth -= width; }
        void setExpansionBehavior(ExpansionBehavior);
        void setHorizontalExpansion(InlineLayoutUnit logicalExpansion);
        void setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth);

        enum class TrailingWhitespace {
            None,
            NotCollapsible,
            Collapsible,
            Collapsed
        };
        bool hasCollapsibleTrailingWhitespace() const { return m_trailingWhitespaceType == TrailingWhitespace::Collapsible || hasCollapsedTrailingWhitespace(); }
        bool hasCollapsedTrailingWhitespace() const { return m_trailingWhitespaceType == TrailingWhitespace::Collapsed; }
        TrailingWhitespace trailingWhitespaceType(const InlineTextItem&) const;
        void removeTrailingWhitespace();
        void visuallyCollapseTrailingWhitespace();

        bool hasTrailingLetterSpacing() const;
        InlineLayoutUnit trailingLetterSpacing() const;
        void removeTrailingLetterSpacing();

        InlineItem::Type m_type { InlineItem::Type::Text };
        const Box* m_layoutBox { nullptr };
        InlineLayoutUnit m_logicalLeft { 0 };
        InlineLayoutUnit m_logicalWidth { 0 };
        bool m_whitespaceIsExpansionOpportunity { false };
        TrailingWhitespace m_trailingWhitespaceType { TrailingWhitespace::None };
        InlineLayoutUnit m_trailingWhitespaceWidth { 0 };
        Optional<LineRun::Text> m_textContent;
        LineRun::Expansion m_expansion;
        unsigned m_expansionOpportunityCount { 0 };
    };
    using RunList = Vector<Run, 10>;
    const RunList& runs() const { return m_runs; }

private:
    void appendNonBreakableSpace(const InlineItem&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
    void appendTextContent(const InlineTextItem&, InlineLayoutUnit logicalWidth);
    void appendNonReplacedInlineBox(const InlineItem&, InlineLayoutUnit marginBoxLogicalWidth);
    void appendReplacedInlineBox(const InlineItem&, InlineLayoutUnit marginBoxLogicalWidth);
    void appendInlineBoxStart(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendInlineBoxEnd(const InlineItem&, InlineLayoutUnit logicalWidth);
    void appendLineBreak(const InlineItem&);
    void appendWordBreakOpportunity(const InlineItem&);

    void removeTrailingTrimmableContent();
    void visuallyCollapsePreWrapOverflowContent(InlineLayoutUnit extraHorizontalSpace);

    const InlineFormattingContext& formattingContext() const;

    struct TrimmableTrailingContent {
        TrimmableTrailingContent(RunList&);

        void addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth);
        void addPartiallyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth);
        InlineLayoutUnit remove();
        InlineLayoutUnit removePartiallyTrimmableContent();

        InlineLayoutUnit width() const { return m_fullyTrimmableWidth + m_partiallyTrimmableWidth; }
        bool isEmpty() const { return !m_firstTrimmableRunIndex.hasValue(); }
        bool isTrailingRunFullyTrimmable() const { return m_hasFullyTrimmableContent; }
        bool isTrailingRunPartiallyTrimmable() const { return m_partiallyTrimmableWidth; }

        void reset();

    private:
        RunList& m_runs;
        Optional<size_t> m_firstTrimmableRunIndex;
        bool m_hasFullyTrimmableContent { false };
        InlineLayoutUnit m_fullyTrimmableWidth { 0 };
        InlineLayoutUnit m_partiallyTrimmableWidth { 0 };
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    RunList m_runs;
    TrimmableTrailingContent m_trimmableTrailingContent;
    InlineLayoutUnit m_contentLogicalWidth { 0 };
    Optional<InlineLayoutUnit> m_trailingSoftHyphenWidth { 0 };
};

inline void Line::TrimmableTrailingContent::reset()
{
    m_hasFullyTrimmableContent = false;
    m_firstTrimmableRunIndex = { };
    m_fullyTrimmableWidth = { };
    m_partiallyTrimmableWidth = { };
}

inline Line::Run::TrailingWhitespace Line::Run::trailingWhitespaceType(const InlineTextItem& inlineTextItem) const
{
    if (!inlineTextItem.isWhitespace())
        return TrailingWhitespace::None;
    if (InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
        return TrailingWhitespace::NotCollapsible;
    if (inlineTextItem.length() == 1)
        return TrailingWhitespace::Collapsible;
    return TrailingWhitespace::Collapsed;
}

inline void Line::Run::setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth)
{
    ASSERT(m_textContent);
    m_textContent->setNeedsHyphen();
    m_logicalWidth += hyphenLogicalWidth;
}

}
}
#endif
