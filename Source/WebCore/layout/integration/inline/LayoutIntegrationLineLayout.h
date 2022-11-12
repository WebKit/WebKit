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

#include "FloatRect.h"
#include "InlineFormattingConstraints.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorTextBox.h"
#include "LayoutIntegrationBoxTree.h"
#include "LayoutPoint.h"
#include "LayoutState.h"
#include "RenderObjectEnums.h"
#include <wtf/CheckedPtr.h>

namespace WebCore {

class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class RenderBlockFlow;
class RenderBox;
class RenderBoxModelObject;
class RenderInline;
class RenderLineBreak;
class RenderListItem;
class RenderListMarker;
class RenderTable;
struct PaintInfo;

namespace Layout {
class InlineDamage;
}

namespace LayoutIntegration {

struct InlineContent;

class LineLayout : public CanMakeCheckedPtr {
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
    static bool canUseForAfterInlineBoxStyleChange(const RenderInline&, StyleDifference);

    bool shouldSwitchToLegacyOnInvalidation() const;

    void updateInlineContentConstraints();
    void updateInlineContentDimensions();
    void updateStyle(const RenderBoxModelObject&, const RenderStyle& oldStyle);
    void updateOverflow();

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicWidthConstraints();

    void layout();
    void paint(PaintInfo&, const LayoutPoint& paintOffset, const RenderInline* layerRenderer = nullptr);
    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& accumulatedOffset, HitTestAction, const RenderInline* layerRenderer = nullptr);
    void adjustForPagination();

    void collectOverflow();
    LayoutRect visualOverflowBoundingBoxRectFor(const RenderInline&) const;
    Vector<FloatRect> collectInlineBoxRects(const RenderInline&) const;

    bool isPaginated() const { return m_isPaginatedContent; }
    LayoutUnit contentLogicalHeight() const;
    size_t lineCount() const;
    bool hasVisualOverflow() const;
    LayoutUnit firstLinePhysicalBaseline() const;
    LayoutUnit lastLinePhysicalBaseline() const;
    LayoutUnit lastLineLogicalBaseline() const;
    LayoutRect firstInlineBoxRect(const RenderInline&) const;
    LayoutRect enclosingBorderBoxRectFor(const RenderInline&) const;

    InlineIterator::TextBoxIterator textBoxesFor(const RenderText&) const;
    InlineIterator::LeafBoxIterator boxFor(const RenderElement&) const;
    InlineIterator::InlineBoxIterator firstInlineBoxFor(const RenderInline&) const;
    InlineIterator::InlineBoxIterator firstRootInlineBox() const;
    InlineIterator::LineBoxIterator firstLineBox() const;
    InlineIterator::LineBoxIterator lastLineBox() const;

    const RenderObject& rendererForLayoutBox(const Layout::Box&) const;
    const RenderBlockFlow& flow() const { return downcast<RenderBlockFlow>(m_boxTree.rootRenderer()); }
    RenderBlockFlow& flow() { return downcast<RenderBlockFlow>(m_boxTree.rootRenderer()); }

    static void releaseCaches(RenderView&);

#if ENABLE(TREE_DEBUGGING)
    void outputLineTree(WTF::TextStream&, size_t depth) const;
#endif

private:
    void updateReplacedDimensions(const RenderBox&);
    void updateInlineBlockDimensions(const RenderBlock&);
    void updateLineBreakBoxDimensions(const RenderLineBreak&);
    void updateInlineBoxDimensions(const RenderInline&);
    void updateInlineTableDimensions(const RenderTable&);
    void updateListItemDimensions(const RenderListItem&);
    void updateListMarkerDimensions(const RenderListMarker&);

    void prepareLayoutState();
    void prepareFloatingState();
    void constructContent();
    InlineContent& ensureInlineContent();
    void updateLayoutBoxDimensions(const RenderBox&);

    Layout::LayoutState& layoutState() { return *m_layoutState; }

    Layout::InlineDamage& ensureLineDamage();

    const Layout::ElementBox& rootLayoutBox() const;
    Layout::ElementBox& rootLayoutBox();
    void clearInlineContent();
    void releaseCaches();

    LayoutUnit physicalBaselineForLine(LayoutIntegration::Line&) const;
    
    BoxTree m_boxTree;
    WeakPtr<Layout::LayoutState> m_layoutState;
    Layout::BlockFormattingState& m_blockFormattingState;
    Layout::InlineFormattingState& m_inlineFormattingState;
    std::optional<Layout::ConstraintsForInlineContent> m_inlineContentConstraints;
    // FIXME: This should be part of LayoutState.
    std::unique_ptr<Layout::InlineDamage> m_lineDamage;
    std::unique_ptr<InlineContent> m_inlineContent;
    bool m_isPaginatedContent { false };
};

}
}

