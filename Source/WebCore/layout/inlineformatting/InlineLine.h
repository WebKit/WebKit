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

namespace WebCore {
namespace Layout {

class Line {
public:
    Line(const LayoutState&);

    void reset(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimumLineHeight, LayoutUnit baselineOffset);

    struct LineItem {
        LineItem(Display::Run, const InlineItem&, bool isCollapsed, bool canBeExtended);

        // Relative to the baseline.
        Display::Run inlineRun;
        const InlineItem& inlineItem;
        bool isCollapsed { false };
        bool canBeExtended { false };
    };

    using LineItems = Vector<std::unique_ptr<LineItem>>;
    const LineItems& close();

    void appendTextContent(const InlineTextItem&, LayoutSize);
    void appendNonReplacedInlineBox(const InlineItem&, LayoutSize);
    void appendReplacedInlineBox(const InlineItem&, LayoutSize);
    void appendInlineContainerStart(const InlineItem&);
    void appendInlineContainerEnd(const InlineItem&);
    void appendHardLineBreak(const InlineItem&);

    bool hasContent() const;

    LayoutUnit trailingTrimmableWidth() const;

    void moveLogicalLeft(LayoutUnit);
    void moveLogicalRight(LayoutUnit);

    LayoutUnit availableWidth() const { return logicalWidth() - contentLogicalWidth(); }
    
    LayoutUnit contentLogicalRight() const { return logicalLeft() + contentLogicalWidth(); }
    LayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }

    LayoutUnit logicalTop() const { return m_logicalTopLeft.y(); }
    LayoutUnit logicalLeft() const { return m_logicalTopLeft.x(); }
    LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }
    LayoutUnit logicalBottom() const { return logicalTop() + logicalHeight(); }
    LayoutUnit logicalWidth() const { return m_lineLogicalWidth; }
    LayoutUnit logicalHeight() const { return m_logicalHeight.height + m_logicalHeight.depth; }

    struct UsedHeightAndDepth {
        LayoutUnit height;
        LayoutUnit depth;
    };
    static UsedHeightAndDepth halfLeadingMetrics(const FontMetrics&, LayoutUnit lineLogicalHeight);

private:
    void appendNonBreakableSpace(const InlineItem&, const Display::Rect& logicalRect);
    void removeTrailingTrimmableContent();

    const LayoutState& m_layoutState;
    LineItems m_lineItems;
    ListHashSet<LineItem*> m_trimmableContent;

    LayoutPoint m_logicalTopLeft;
    LayoutUnit m_contentLogicalWidth;

    UsedHeightAndDepth m_logicalHeight;
    LayoutUnit m_lineLogicalWidth;
};

}
}
#endif
