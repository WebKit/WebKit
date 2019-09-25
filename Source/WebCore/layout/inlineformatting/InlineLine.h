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
        HeightAndBaseline heightAndBaseline;
    };
    enum class SkipVerticalAligment { No, Yes };
    Line(const InlineFormattingContext&, const InitialConstraints&, SkipVerticalAligment);

    class Content {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        struct Run {
            WTF_MAKE_STRUCT_FAST_ALLOCATED;
            struct TextContext {
                unsigned start { 0 };
                unsigned length { 0 };
                bool isCollapsed { false };
                bool isWhitespace { false };
                bool canBeExtended { false };
            };
            Run(const InlineItem&, const Display::Rect&);
            Run(const InlineItem&, const TextContext&, const Display::Rect&);

            const Box& layoutBox() const { return m_layoutBox; }
            const Display::Rect& logicalRect() const { return m_logicalRect; }
            const Optional<TextContext> textContext() const { return m_textContext; }
            InlineItem::Type type() const { return m_type; }

            bool isText() const { return m_type == InlineItem::Type::Text; }
            bool isBox() const { return m_type == InlineItem::Type::Box; }
            bool isLineBreak() const { return m_type == InlineItem::Type::HardLineBreak; }
            bool isContainerStart() const { return m_type == InlineItem::Type::ContainerStart; }
            bool isContainerEnd() const { return m_type == InlineItem::Type::ContainerEnd; }

        private:
            friend class Line;
            void adjustLogicalTop(LayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
            void moveVertically(LayoutUnit offset) { m_logicalRect.moveVertically(offset); }
            void moveHorizontally(LayoutUnit offset) { m_logicalRect.moveHorizontally(offset); }
            void setTextIsCollapsed() { m_textContext->isCollapsed = true; }

            const Box& m_layoutBox;
            const InlineItem::Type m_type;
            Display::Rect m_logicalRect;
            Optional<TextContext> m_textContext;
        };
        using Runs = Vector<std::unique_ptr<Run>>;
        const Runs& runs() const { return m_runs; }
        const LineBox& lineBox() const { return m_lineBox; }
        LineBox& lineBox() { return m_lineBox; }

    private:
        friend class Line;

        void setLineBox(const LineBox& lineBox) { m_lineBox = lineBox; }
        Runs& runs() { return m_runs; }

        LineBox m_lineBox;
        Runs m_runs;
    };
    std::unique_ptr<Content> close();

    void append(const InlineItem&, LayoutUnit logicalWidth);
    bool hasContent() const { return !isVisuallyEmpty(); }

    LayoutUnit trailingTrimmableWidth() const;

    void moveLogicalLeft(LayoutUnit);
    void moveLogicalRight(LayoutUnit);

    LayoutUnit availableWidth() const { return logicalWidth() - contentLogicalWidth(); }
    LayoutUnit contentLogicalRight() const { return m_content->lineBox().logicalRight(); }
    LayoutUnit logicalTop() const { return m_content->lineBox().logicalTop(); }
    LayoutUnit logicalBottom() const { return m_content->lineBox().logicalBottom(); }

    static LineBox::Baseline halfLeadingMetrics(const FontMetrics&, LayoutUnit lineLogicalHeight);

private:
    LayoutUnit logicalLeft() const { return m_content->lineBox().logicalLeft(); }
    LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

    LayoutUnit logicalWidth() const { return m_lineLogicalWidth; }
    LayoutUnit logicalHeight() const { return m_content->lineBox().logicalHeight(); }

    LayoutUnit contentLogicalWidth() const { return m_content->lineBox().logicalWidth(); }
    LayoutUnit baselineOffset() const { return m_content->lineBox().baselineOffset(); }

    void appendNonBreakableSpace(const InlineItem&, const Display::Rect& logicalRect);
    void appendTextContent(const InlineTextItem&, LayoutUnit logicalWidth);
    void appendNonReplacedInlineBox(const InlineItem&, LayoutUnit logicalWidth);
    void appendReplacedInlineBox(const InlineItem&, LayoutUnit logicalWidth);
    void appendInlineContainerStart(const InlineItem&, LayoutUnit logicalWidth);
    void appendInlineContainerEnd(const InlineItem&, LayoutUnit logicalWidth);
    void appendHardLineBreak(const InlineItem&);

    void removeTrailingTrimmableContent();

    void adjustBaselineAndLineHeight(const InlineItem&, LayoutUnit runHeight);
    LayoutUnit inlineItemContentHeight(const InlineItem&) const;
    bool isVisuallyEmpty() const;

    LayoutState& layoutState() const;
    const InlineFormattingContext& formattingContext() const; 

    const InlineFormattingContext& m_inlineFormattingContext;
    std::unique_ptr<Content> m_content;
    ListHashSet<Content::Run*> m_trimmableContent;

    Optional<LineBox::Baseline> m_initialStrut;
    LayoutUnit m_lineLogicalWidth;
    bool m_skipVerticalAligment { false };
};

}
}
#endif
