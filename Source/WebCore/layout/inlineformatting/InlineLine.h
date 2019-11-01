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
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class InlineFormattingContext;

class Line {
    WTF_MAKE_ISO_ALLOCATED(Line);
public:
    struct InitialConstraints {
        LayoutPoint logicalTopLeft;
        LayoutUnit availableLogicalWidth;
        bool lineIsConstrainedByFloat { false };
        struct HeightAndBaseline {
            LayoutUnit height;
            LayoutUnit baselineOffset;
            Optional<LineBox::Baseline> strut;
        };
        Optional<HeightAndBaseline> heightAndBaseline;
    };
    enum class SkipAlignment { No, Yes };
    Line(const InlineFormattingContext&, const InitialConstraints&, Optional<TextAlignMode>, SkipAlignment);

    void append(const InlineItem&, LayoutUnit logicalWidth);
    bool hasContent() const { return !isVisuallyEmpty(); }
    LayoutUnit availableWidth() const { return logicalWidth() - contentLogicalWidth(); }

    LayoutUnit trailingTrimmableWidth() const;

    const LineBox& lineBox() const { return m_lineBox; }
    void moveLogicalLeft(LayoutUnit);
    void moveLogicalRight(LayoutUnit);

    struct Run {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Run(const InlineItem&, const Display::Run&);

        const Display::Run& displayRun() const { return m_displayRun; }
        const Box& layoutBox() const { return m_layoutBox; }

        const Display::Rect& logicalRect() const { return m_displayRun.logicalRect(); }
        bool isCollapsedToZeroAdvanceWidth() const;
        bool isCollapsed() const { return m_isCollapsed; }

        bool isText() const { return m_type == InlineItem::Type::Text; }
        bool isBox() const { return m_type == InlineItem::Type::Box; }
        bool isForcedLineBreak() const { return m_type == InlineItem::Type::ForcedLineBreak; }
        bool isContainerStart() const { return m_type == InlineItem::Type::ContainerStart; }
        bool isContainerEnd() const { return m_type == InlineItem::Type::ContainerEnd; }

    private:
        friend class Line;
        void adjustLogicalTop(LayoutUnit logicalTop) { m_displayRun.setLogicalTop(logicalTop); }
        void moveVertically(LayoutUnit offset) { m_displayRun.moveVertically(offset); }
        void moveHorizontally(LayoutUnit offset) { m_displayRun.moveHorizontally(offset); }

        void expand(const Run&);

        void setIsCollapsed();
        void setCollapsesToZeroAdvanceWidth();

        void setHasExpansionOpportunity(ExpansionBehavior);
        bool hasExpansionOpportunity() const { return m_expansionOpportunityCount.hasValue(); }
        Optional<ExpansionBehavior> expansionBehavior() const;
        Optional<unsigned> expansionOpportunityCount() const { return m_expansionOpportunityCount; }
        void adjustExpansionBehavior(ExpansionBehavior);
        void setComputedHorizontalExpansion(LayoutUnit logicalExpansion);

        bool isCollapsible() const { return m_isCollapsible; }
        bool hasTrailingCollapsedContent() const { return m_hasTrailingCollapsedContent; }
        bool isWhitespace() const { return m_isWhitespace; }

        const Box& m_layoutBox;
        Display::Run m_displayRun;
        const InlineItem::Type m_type;
        bool m_isWhitespace { false };
        bool m_isCollapsible { false };
        bool m_isCollapsed { false };
        bool m_collapsedToZeroAdvanceWidth { false };
        bool m_hasTrailingCollapsedContent { false };
        Optional<unsigned> m_expansionOpportunityCount;
    };
    using RunList = Vector<std::unique_ptr<Run>>;
    enum class IsLastLineWithInlineContent { No, Yes };
    RunList close(IsLastLineWithInlineContent = IsLastLineWithInlineContent::No);

    static LineBox::Baseline halfLeadingMetrics(const FontMetrics&, LayoutUnit lineLogicalHeight);

private:
    LayoutUnit logicalTop() const { return m_lineBox.logicalTop(); }
    LayoutUnit logicalBottom() const { return m_lineBox.logicalBottom(); }

    LayoutUnit logicalLeft() const { return m_lineBox.logicalLeft(); }
    LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

    LayoutUnit logicalWidth() const { return m_lineLogicalWidth; }
    LayoutUnit logicalHeight() const { return m_lineBox.logicalHeight(); }

    LayoutUnit contentLogicalWidth() const { return m_lineBox.logicalWidth(); }
    LayoutUnit contentLogicalRight() const { return m_lineBox.logicalRight(); }
    LayoutUnit baselineOffset() const { return m_lineBox.baselineOffset(); }

    void appendNonBreakableSpace(const InlineItem&, const Display::Rect& logicalRect);
    void appendTextContent(const InlineTextItem&, LayoutUnit logicalWidth);
    void appendNonReplacedInlineBox(const InlineItem&, LayoutUnit logicalWidth);
    void appendReplacedInlineBox(const InlineItem&, LayoutUnit logicalWidth);
    void appendInlineContainerStart(const InlineItem&, LayoutUnit logicalWidth);
    void appendInlineContainerEnd(const InlineItem&, LayoutUnit logicalWidth);
    void appendLineBreak(const InlineItem&);

    void removeTrailingTrimmableContent();
    void alignContentHorizontally(IsLastLineWithInlineContent);
    void alignContentVertically();

    void adjustBaselineAndLineHeight(const InlineItem&);
    LayoutUnit inlineItemContentHeight(const InlineItem&) const;
    bool isVisuallyEmpty() const;

    bool isTextAlignJustify() const { return m_horizontalAlignment == TextAlignMode::Justify; };
    void justifyRuns();

    LayoutState& layoutState() const;
    const InlineFormattingContext& formattingContext() const; 

    const InlineFormattingContext& m_inlineFormattingContext;
    RunList m_runList;
    ListHashSet<Run*> m_trimmableContent;

    Optional<LineBox::Baseline> m_initialStrut;
    LayoutUnit m_lineLogicalWidth;
    Optional<TextAlignMode> m_horizontalAlignment;
    bool m_skipAlignment { false };
    LineBox m_lineBox;
};

inline void Line::Run::setIsCollapsed()
{
    ASSERT(isWhitespace());
    m_isCollapsed = true;
    m_hasTrailingCollapsedContent = true;
}

inline bool Line::Run::isCollapsedToZeroAdvanceWidth() const
{
    ASSERT(!m_collapsedToZeroAdvanceWidth || !m_displayRun.logicalWidth());
    return m_collapsedToZeroAdvanceWidth;
}

inline void Line::Run::setCollapsesToZeroAdvanceWidth()
{
    setIsCollapsed();
    m_collapsedToZeroAdvanceWidth = true;
    m_displayRun.setLogicalWidth({ });
    m_expansionOpportunityCount = { };
    m_displayRun.textContext()->resetExpansion();
}

inline Optional<ExpansionBehavior> Line::Run::expansionBehavior() const
{
    ASSERT(isText());
    if (auto expansionContext = m_displayRun.textContext()->expansion())
        return expansionContext->behavior;
    return { };
}

inline void Line::Run::setHasExpansionOpportunity(ExpansionBehavior expansionBehavior)
{
    ASSERT(isText());
    ASSERT(!hasExpansionOpportunity());
    m_expansionOpportunityCount = 1;
    m_displayRun.textContext()->setExpansion({ expansionBehavior, { } });
}

inline void Line::Run::adjustExpansionBehavior(ExpansionBehavior expansionBehavior)
{
    ASSERT(isText());
    ASSERT(hasExpansionOpportunity());
    m_displayRun.textContext()->setExpansion({ expansionBehavior, m_displayRun.textContext()->expansion()->horizontalExpansion });
}

inline void Line::Run::setComputedHorizontalExpansion(LayoutUnit logicalExpansion)
{
    ASSERT(isText());
    ASSERT(hasExpansionOpportunity());
    ASSERT(m_displayRun.textContext()->expansion());
    m_displayRun.expandHorizontally(logicalExpansion);
    m_displayRun.textContext()->setExpansion({ m_displayRun.textContext()->expansion()->behavior, logicalExpansion });
}

}
}
#endif
