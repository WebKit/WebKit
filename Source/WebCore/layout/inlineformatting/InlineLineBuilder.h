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

class InlineFormattingContext;

class LineBuilder {
    class InlineItemRun;
    struct ContinousContent;

public:
    struct Constraints {
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
    LineBuilder(const InlineFormattingContext&, Optional<TextAlignMode>, SkipAlignment);
    ~LineBuilder();

    void initialize(const Constraints&);
    void append(const InlineItem&, LayoutUnit logicalWidth);
    bool isVisuallyEmpty() const { return m_lineBox.isConsideredEmpty(); }
    bool hasIntrusiveFloat() const { return m_hasIntrusiveFloat; }
    LayoutUnit availableWidth() const { return logicalWidth() - contentLogicalWidth(); }

    LayoutUnit trailingTrimmableWidth() const { return m_trimmableContent.width(); }

    const LineBox& lineBox() const { return m_lineBox; }
    void moveLogicalLeft(LayoutUnit);
    void moveLogicalRight(LayoutUnit);
    void setHasIntrusiveFloat() { m_hasIntrusiveFloat = true; }

    struct Run {
        Run(const InlineItemRun&);
        Run(const InlineItemRun&, const Display::Rect&, const Display::Run::TextContext&, unsigned expansionOpportunityCount);
        Run(Run&&) = default;
        Run& operator=(Run&& other) = default;

        bool isText() const { return m_type == InlineItem::Type::Text; }
        bool isBox() const { return m_type == InlineItem::Type::Box; }
        bool isForcedLineBreak() const { return m_type == InlineItem::Type::ForcedLineBreak; }
        bool isContainerStart() const { return m_type == InlineItem::Type::ContainerStart; }
        bool isContainerEnd() const { return m_type == InlineItem::Type::ContainerEnd; }

        const Box& layoutBox() const { return *m_layoutBox; }
        const Display::Rect& logicalRect() const { return m_logicalRect; }
        const Optional<Display::Run::TextContext>& textContext() const { return m_textContext; }
        bool isCollapsedToVisuallyEmpty() const { return m_isCollapsedToVisuallyEmpty; }

    private:
        friend class LineBuilder;

        void adjustLogicalTop(LayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
        void moveHorizontally(LayoutUnit offset) { m_logicalRect.moveHorizontally(offset); }
        void moveVertically(LayoutUnit offset) { m_logicalRect.moveVertically(offset); }
        void setLogicalHeight(LayoutUnit logicalHeight) { m_logicalRect.setHeight(logicalHeight); }

        bool hasExpansionOpportunity() const { return m_expansionOpportunityCount; }
        Optional<ExpansionBehavior> expansionBehavior() const;
        unsigned expansionOpportunityCount() const { return m_expansionOpportunityCount; }
        void setComputedHorizontalExpansion(LayoutUnit logicalExpansion);
        void adjustExpansionBehavior(ExpansionBehavior);

        const Box* m_layoutBox { nullptr };
        InlineItem::Type m_type;
        Display::Rect m_logicalRect;
        Optional<Display::Run::TextContext> m_textContext;
        unsigned m_expansionOpportunityCount { 0 };
        bool m_isCollapsedToVisuallyEmpty { false };
    };
    using RunList = Vector<Run, 50>;
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
    void alignContentHorizontally(RunList&, IsLastLineWithInlineContent) const;
    void alignContentVertically(RunList&);

    void adjustBaselineAndLineHeight(const Run&);
    LayoutUnit runContentHeight(const Run&) const;

    bool isTextAlignJustify() const { return m_horizontalAlignment == TextAlignMode::Justify; };
    void justifyRuns(RunList&) const;

    bool isVisuallyNonEmpty(const InlineItemRun&) const;

    LayoutState& layoutState() const;
    const InlineFormattingContext& formattingContext() const;

    class InlineItemRun {
    public:
        InlineItemRun(const InlineItem&, const Display::Rect&, WTF::Optional<Display::Run::TextContext> = WTF::nullopt);

        const Box& layoutBox() const { return m_inlineItem.layoutBox(); }
        const Display::Rect& logicalRect() const { return m_logicalRect; }
        const Optional<Display::Run::TextContext>& textContext() const { return m_textContext; }

        bool isText() const { return m_inlineItem.isText(); }
        bool isBox() const { return m_inlineItem.isBox(); }
        bool isContainerStart() const { return m_inlineItem.isContainerStart(); }
        bool isContainerEnd() const { return m_inlineItem.isContainerEnd(); }
        bool isForcedLineBreak() const { return m_inlineItem.isForcedLineBreak(); }
        InlineItem::Type type() const { return m_inlineItem.type(); }

        void setIsCollapsed() { m_isCollapsed = true; }
        bool isCollapsed() const { return m_isCollapsed; }

        void setCollapsesToZeroAdvanceWidth();
        bool isCollapsedToZeroAdvanceWidth() const { return m_collapsedToZeroAdvanceWidth; }

        bool isCollapsible() const { return is<InlineTextItem>(m_inlineItem) && downcast<InlineTextItem>(m_inlineItem).isCollapsible(); }
        bool isWhitespace() const { return is<InlineTextItem>(m_inlineItem) && downcast<InlineTextItem>(m_inlineItem).isWhitespace(); }

        bool hasExpansionOpportunity() const { return isWhitespace() && !isCollapsedToZeroAdvanceWidth(); }

    private:
        const InlineItem& m_inlineItem;
        Display::Rect m_logicalRect;
        const Optional<Display::Run::TextContext> m_textContext;
        bool m_isCollapsed { false };
        bool m_collapsedToZeroAdvanceWidth { false };
    };

    struct TrimmableContent {
        void append(LayoutUnit itemRunWidth, size_t runIndex);
        void clear();

        LayoutUnit width() const { return m_width; }
        Vector<size_t, 5>& runIndexes() { return m_runIndexes; }
        bool isEmpty() const { return m_runIndexes.isEmpty(); }

    private:
        Vector<size_t, 5> m_runIndexes;
        LayoutUnit m_width;
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    Vector<InlineItemRun, 50> m_inlineItemRuns;
    TrimmableContent m_trimmableContent;
    Optional<LineBox::Baseline> m_initialStrut;
    LayoutUnit m_lineLogicalWidth;
    Optional<TextAlignMode> m_horizontalAlignment;
    bool m_skipAlignment { false };
    bool m_hasIntrusiveFloat { false };
    LineBox m_lineBox;
    Optional<bool> m_lineIsVisuallyEmptyBeforeTrimmableContent;
};

inline void LineBuilder::TrimmableContent::clear()
{
    m_runIndexes.clear();
    m_width = { };
}

}
}
#endif
