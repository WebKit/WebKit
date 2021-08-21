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

#include "LayoutIntegrationBoxTree.h"
#include "LayoutIntegrationLineIterator.h"
#include "LayoutIntegrationRunIterator.h"
#include "LayoutPoint.h"
#include "LayoutState.h"
#include "RenderObjectEnums.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class GraphicsContext;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class RenderBlockFlow;
class RenderBox;
class RenderBoxModelObject;
class RenderInline;
class RenderLineBreak;
struct PaintInfo;

namespace LayoutIntegration {

struct InlineContent;

class LineLayout : public CanMakeWeakPtr<LineLayout> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LineLayout(RenderBlockFlow&);
    ~LineLayout();

    static RenderBlockFlow* blockContainer(RenderObject&);
    static LineLayout* containing(RenderObject&);
    static const LineLayout* containing(const RenderObject&);

    static bool isEnabled();
    static bool canUseFor(const RenderBlockFlow&);
    static bool canUseForAfterStyleChange(const RenderBlockFlow&, StyleDifference);

    bool shouldSwitchToLegacyOnInvalidation() const;

    void updateReplacedDimensions(const RenderBox&);
    void updateInlineBlockDimensions(const RenderBlock&);
    void updateLineBreakBoxDimensions(const RenderLineBreak&);
    void updateInlineBoxDimensions(const RenderInline&);
    void updateStyle(const RenderBoxModelObject&);
    void layout();

    LayoutUnit contentLogicalHeight() const;
    size_t lineCount() const;

    LayoutUnit firstLineBaseline() const;
    LayoutUnit lastLineBaseline() const;

    void adjustForPagination();
    void collectOverflow();

    const InlineContent* inlineContent() const { return m_inlineContent.get(); }
    bool isPaginated() const { return !!m_paginatedHeight; }

    void paint(PaintInfo&, const LayoutPoint& paintOffset);
    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& accumulatedOffset, HitTestAction);

    TextRunIterator textRunsFor(const RenderText&) const;
    RunIterator runFor(const RenderElement&) const;
    LineIterator firstLine() const;
    LineIterator lastLine() const;

    LayoutRect enclosingBorderBoxRectFor(const RenderInline&) const;
    LayoutRect visualOverflowBoundingBoxRectFor(const RenderInline&) const;

    const RenderObject& rendererForLayoutBox(const Layout::Box&) const;
    const RenderBlockFlow& flow() const { return m_boxTree.flow(); }
    RenderBlockFlow& flow() { return m_boxTree.flow(); }

    static void releaseCaches(RenderView&);

#if ENABLE(TREE_DEBUGGING)
    void outputLineTree(WTF::TextStream&, size_t depth) const;
#endif

private:
    void prepareLayoutState();
    void prepareFloatingState();
    void constructContent();
    InlineContent& ensureInlineContent();
    void updateLayoutBoxDimensions(const RenderBox&);

    void paintTextRunUsingPhysicalCoordinates(PaintInfo&, const LayoutPoint& paintOffset, const Line&, const Run&);

    const Layout::ContainerBox& rootLayoutBox() const;
    Layout::ContainerBox& rootLayoutBox();
    void releaseInlineItemCache();

    BoxTree m_boxTree;
    Layout::LayoutState m_layoutState;
    Layout::InlineFormattingState& m_inlineFormattingState;
    RefPtr<InlineContent> m_inlineContent;
    std::optional<LayoutUnit> m_paginatedHeight;
};

}
}

#endif
