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

#include "BlockFormattingState.h"
#include "InlineDamage.h"
#include "InlineFormattingConstraints.h"
#include "InlineFormattingContext.h"
#include "InlineIteratorInlineBox.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "LayoutIntegrationBoxTreeUpdater.h"
#include <WebCore/FloatRect.h>
#include <WebCore/InlineIteratorLineBox.h>
#include <WebCore/InlineIteratorTextBox.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutState.h>
#include <WebCore/RenderObjectEnums.h>
#include <wtf/CheckedPtr.h>

namespace WebCore {

class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class RenderBlockFlow;
class RenderBox;
class RenderBoxModelObject;
class RenderListOutsideMarker;
struct PaintInfo;
struct SVGTextFragment;

namespace Layout {
class InlineDamage;
}

namespace LayoutIntegration {

class InlineContent;
struct LineAdjustment;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(LayoutIntegration_LineLayout);

class LineLayout final : public CanMakeCheckedPtr<LineLayout> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LineLayout, LayoutIntegration_LineLayout);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LineLayout);
public:
    LineLayout(RenderBlockFlow&);
    ~LineLayout();

    static RenderBlockFlow* LEGACY_NODELETE blockContainer(const RenderObject&);
    static LineLayout* containing(RenderObject&);
    static const LineLayout* containing(const RenderObject&);

    static bool canUseForIntrinsicWidthComputation(const RenderBlockFlow&);
    static bool shouldInvalidateLineLayoutAfterContentChange(const RenderBlockFlow& parent, const RenderObject& rendererWithNewContent, const LineLayout&);
    static bool shouldInvalidateLineLayoutAfterTreeMutation(const RenderBlockFlow& parent, const RenderObject& renderer, const LineLayout&, bool isRemoval);

    void updateFormattingContextGeometries(LayoutUnit availableLogicalWidth);

    std::optional<LayoutRect> updateOverflow();

    static void updateStyle(const RenderObject&);

    // Partial invalidation.
    bool insertedIntoTree(const RenderElement& parent, RenderObject& child);
    bool removedFromTree(const RenderElement& parent, RenderObject& child);
    bool updateTextContent(const RenderText&, std::optional<size_t> offset, size_t oldLength);
    bool rootStyleWillChange(const RenderBlockFlow&, const Style::ComputedStyle& newStyle);
    bool styleWillChange(const RenderElement&, const Style::ComputedStyle& newStyle, Style::Difference);
    bool boxContentWillChange(const RenderBox&);

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicWidthConstraints();

    enum class ForceFullLayout : bool { No, Yes };
    std::optional<LayoutRect> layout(RenderBlockFlow::MarginInfo&, ForceFullLayout = ForceFullLayout::No);
    // Builds the display content without line layout. Returns false when the content needs the full layout.
    bool layoutSVGText();
    void paint(PaintInfo&, const LayoutPoint& paintOffset, const RenderBoxModelObject* layerRenderer = nullptr);
    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& accumulatedOffset, HitTestAction, const RenderBoxModelObject* layerRenderer = nullptr);
    void adjustForPagination();
    void shiftLinesByInBlockDirection(LayoutUnit blockShift);

    void collectOverflow();
    LayoutRect inkOverflowBoundingBoxRectFor(const RenderBoxModelObject&) const;
    Vector<FloatRect> collectInlineBoxRects(const RenderBoxModelObject&) const;

    LayoutUnit contentLogicalHeight() const;
    std::optional<LayoutUnit> clampedContentLogicalHeight() const;
    bool LEGACY_NODELETE hasEllipsisInBlockDirectionOnLastFormattedLine() const;
    bool contentFitsWithinMaximumLines() const { return m_inlineContent && m_inlineContent->contentFitsWithinMaximumLines(); }
    bool contains(const RenderElement& renderer) const;

    bool LEGACY_NODELETE isPaginated() const;
    size_t LEGACY_NODELETE lineCount() const;
    size_t LEGACY_NODELETE lineCountIgnoringBlockLevelBoxes() const;
    // Lines with contentful inline content (block-in-inline and empty lines don't count) ending within logicalHeight from the border box top, and whether any ends past it.
    std::pair<size_t, bool> LEGACY_NODELETE lineCountForHeight(LayoutUnit logicalHeight) const;
    bool hasContent() const { return !!m_inlineContent; }
    bool LEGACY_NODELETE hasContentfulInlineOrBlockLine() const;
    bool LEGACY_NODELETE hasContentfulInlineLine() const;
    bool isSelfCollapsingContent() const;
    bool LEGACY_NODELETE hasInkOverflow() const;
    std::optional<LayoutUnit> firstLineBaseline() const;
    std::optional<LayoutUnit> lastLineBaseline() const;
    LayoutRect firstInlineBoxRect(const RenderBoxModelObject&) const;
    LayoutRect enclosingBorderBoxRectFor(const RenderBoxModelObject&) const;

    InlineIterator::TextBoxIterator textBoxesFor(const RenderText&) const;
    InlineIterator::LeafBoxIterator boxFor(const RenderElement&) const;
    InlineIterator::InlineBoxIterator firstInlineBoxFor(const RenderBoxModelObject&) const;
    InlineIterator::InlineBoxIterator firstRootInlineBox() const;
    InlineIterator::BoxIterator lastBox() const;
    InlineIterator::LineBoxIterator firstLineBox() const;
    InlineIterator::LineBoxIterator lastLineBox() const;

    const RenderBlockFlow& flow() const { return downcast<RenderBlockFlow>(*m_rootLayoutBox->rendererForIntegration()); }
    RenderBlockFlow& flow() { return downcast<RenderBlockFlow>(*m_rootLayoutBox->rendererForIntegration()); }

    static void releaseCaches(RenderView&);

#if ENABLE(TREE_DEBUGGING)
    void outputLineTree(WTF::TextStream&, size_t depth) const;
#endif

    // This is temporary, required by partial bailout check.
    bool LEGACY_NODELETE contentNeedsVisualReordering() const;
    bool isDamaged() const { return !!m_lineDamage; }
    const Layout::InlineDamage* damage() const LIFETIME_BOUND { return m_lineDamage.get(); }
#ifndef NDEBUG
    bool hasTruncatedDisplayContent() const { return m_lineDamage && m_lineDamage->hasTruncatedDisplayContent(); }
#endif

    struct SVGTextFragmentsForBoxes {
        const InlineDisplay::Boxes& boxes;
        Vector<Vector<SVGTextFragment>>& fragments;
    };
    // Empties the per box SVG text fragments for SVGTextLayoutEngine to fill in.
    SVGTextFragmentsForBoxes resetSVGTextFragments() LIFETIME_BOUND;
    // Sets the box rects from the SVG text fragments.
    FloatRect applySVGTextFragments();

    bool LEGACY_NODELETE hasBlocks() const;

    // The last layout() pass. A pagination restart runs a second pass from inside layout().
    struct LayoutInfoForTesting {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(LayoutInfoForTesting);
        // Increases with every inline layout of any block container.
        uint64_t sequenceNumber { 0 };
        Layout::InlineLayoutResult::Range range { Layout::InlineLayoutResult::Range::Full };
        bool isFromMaximumIntrinsicWidthLine { false };
        size_t laidOutLineCount { 0 };
        // Lines kept from the previous layout.
        size_t reusedLineCount { 0 };
    };
    WEBCORE_EXPORT static void setCollectsLayoutInfoForTesting(bool);
    const LayoutInfoForTesting* layoutInfoForTesting() const { return m_layoutInfoForTesting.get(); }

private:
    void preparePlacedFloats();
    FloatRect constructContent(const Layout::InlineLayoutState&, std::unique_ptr<Layout::InlineLayoutResult>&&);
    static std::unique_ptr<LayoutInfoForTesting> collectLayoutInfoForTesting(const Layout::InlineLayoutResult*);
    void setLayoutInfoForTesting(std::unique_ptr<LayoutInfoForTesting>&&);
    Vector<LineAdjustment> adjustContentForPagination(const Layout::BlockLayoutState&, bool isPartialLayout);
    void updateRenderTreePositions(const Vector<LineAdjustment>&, const Layout::InlineLayoutState&, bool didDiscardContent);

    InlineContent& ensureInlineContent() LIFETIME_BOUND;
    Layout::BlockFormattingState& ensureBlockFormattingState() LIFETIME_BOUND;
    Layout::InlineContentCache& ensureInlineContentCache() LIFETIME_BOUND;
    const Layout::InlineItemList& inlineItemsForInvalidation() const LIFETIME_BOUND;

    Layout::LayoutState& layoutState() LIFETIME_BOUND { return *m_layoutState; }
    const Layout::LayoutState& layoutState() const LIFETIME_BOUND { return *m_layoutState; }

    Layout::InlineDamage& ensureLineDamage() LIFETIME_BOUND;
    void removeLayoutBox(const RenderElement& parent, RenderObject& child);
    void releaseContentRebuiltFromDamage();

    const Layout::ElementBox& rootLayoutBox() const { return *m_rootLayoutBox; }
    Layout::ElementBox& rootLayoutBox() { return *m_rootLayoutBox; }
    void clearInlineContent();
    void releaseCachesAndResetDamage();

    LayoutUnit LEGACY_NODELETE baselineForLine(const InlineDisplay::Line&) const;

    bool LEGACY_NODELETE isContentConsideredStale() const;

    using ExcludedMarkerList = Vector<CheckedPtr<RenderListOutsideMarker>>;
    ExcludedMarkerList excludedMarkersForFirstFormattedLine(Layout::InlineLayoutState&);
    void setExcludedMarkerPositions(const ExcludedMarkerList&);

private:
    CheckedPtr<Layout::ElementBox> m_rootLayoutBox;
    CheckedPtr<Document> m_document;
    WeakPtr<Layout::LayoutState> m_layoutState;
    // Inline formatting context state, created on first use. The SVG text fast path does not need it.
    Layout::BlockFormattingState* m_blockFormattingState { nullptr };
    Layout::InlineContentCache* m_inlineContentCache { nullptr };
    std::optional<Layout::ConstraintsForInlineContent> m_inlineContentConstraints;
    // FIXME: This should be part of LayoutState.
    std::unique_ptr<Layout::InlineDamage> m_lineDamage;
    std::unique_ptr<InlineContent> m_inlineContent;
    BoxGeometryUpdater m_boxGeometryUpdater;
    std::unique_ptr<LayoutInfoForTesting> m_layoutInfoForTesting;
};

}
}

