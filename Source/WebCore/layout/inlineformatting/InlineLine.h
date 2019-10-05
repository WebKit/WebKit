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
        const Box& layoutBox() const { return m_inlineItem.layoutBox(); }

        const Display::Rect& logicalRect() const { return m_displayRun.logicalRect(); }
        bool isVisuallyEmpty() const { return m_isVisuallyEmpty; }

        bool isText() const { return m_inlineItem.isText(); }
        bool isBox() const { return m_inlineItem.isBox(); }
        bool isLineBreak() const { return m_inlineItem.isLineBreak(); }
        bool isContainerStart() const { return m_inlineItem.isContainerStart(); }
        bool isContainerEnd() const { return m_inlineItem.isContainerEnd(); }

    private:
        friend class Line;
        void adjustLogicalTop(LayoutUnit logicalTop) { m_displayRun.setLogicalTop(logicalTop); }
        void moveVertically(LayoutUnit offset) { m_displayRun.moveVertically(offset); }
        void moveHorizontally(LayoutUnit offset) { m_displayRun.moveHorizontally(offset); }

        void expand(const Run&);

        void setVisuallyIsEmpty() { m_isVisuallyEmpty = true; }

        bool isWhitespace() const;
        bool canBeExtended() const;

        const InlineItem& m_inlineItem;
        Display::Run m_displayRun;
        bool m_isVisuallyEmpty { false };
    };
    using RunList = Vector<std::unique_ptr<Run>>;
    RunList close();

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
    void appendHardLineBreak(const InlineItem&);

    void removeTrailingTrimmableContent();
    void alignContentHorizontally();
    void alignContentVertically();

    void adjustBaselineAndLineHeight(const InlineItem&, LayoutUnit runHeight);
    LayoutUnit inlineItemContentHeight(const InlineItem&) const;
    bool isVisuallyEmpty() const;

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

inline void Line::Run::expand(const Run& other)
{
    ASSERT(isText());
    ASSERT(other.isText());

    auto& otherDisplayRun = other.displayRun();
    m_displayRun.expandHorizontally(otherDisplayRun.logicalWidth());
    m_displayRun.textContext()->expand(otherDisplayRun.textContext()->length());
}

}
}
#endif
