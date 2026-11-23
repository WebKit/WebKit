/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AnchorPositionEvaluator.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/RenderLayerModelObject.h>
#include <WebCore/SubtreeScrollbarChangesState.h>
#include <WebCore/Timer.h>
#include <wtf/CheckedRef.h>
#include <wtf/SegmentedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class LayoutScope;
class LayoutSize;
class LocalFrame;
class LocalFrameView;
class RenderBlock;
class RenderBlockFlow;
class RenderBox;
class RenderLayoutState;
class RenderListItem;
class RenderView;
namespace Layout {
class LayoutState;
class LayoutTree;
}

namespace LayoutIntegration {
class InlineContent;
}

enum class LayoutOptions : uint8_t;

struct UpdateScrollInfoAfterLayoutTransaction {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(UpdateScrollInfoAfterLayoutTransaction);

    UpdateScrollInfoAfterLayoutTransaction();
    ~UpdateScrollInfoAfterLayoutTransaction();

    int nestedCount { 0 };
    SingleThreadWeakHashSet<RenderBlock> blocks;
};

class LocalFrameViewLayoutContext final : public CanMakeCheckedPtr<LocalFrameViewLayoutContext> {
    WTF_MAKE_TZONE_ALLOCATED(LocalFrameViewLayoutContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LocalFrameViewLayoutContext);
public:
    LocalFrameViewLayoutContext(LocalFrameView&);
    ~LocalFrameViewLayoutContext();

    WEBCORE_EXPORT void layout(bool canDeferUpdateLayerPositions = false);
    NODELETE bool needsLayout(OptionSet<LayoutOptions> layoutOptions = { }) const;

    void interleavedLayout();

    // We rely on the side-effects of layout, like compositing updates, to update state in various subsystems
    // whose dependencies are poorly defined. This call triggers such updates.
    void setNeedsLayoutAfterViewConfigurationChange();

    void scheduleLayout();
    void scheduleSubtreeLayout(RenderElement& layoutRoot);
    void unscheduleLayout();

    NODELETE void disableSetNeedsLayout();
    NODELETE void enableSetNeedsLayout();

    enum class LayoutPhase : uint8_t {
        OutsideLayout,
        InPreLayout,
        InRenderTreeLayout,
        InViewSizeAdjust,
        InPostLayout
    };
    LayoutPhase layoutPhase() const { return m_layoutPhase; }
    bool isLayoutNested() const { return m_layoutNestedState == LayoutNestedState::Nested; }
    bool isLayoutPending() const { return m_layoutTimer.isActive(); }
    bool isInLayout() const { return layoutPhase() != LayoutPhase::OutsideLayout; }
    bool isInRenderTreeLayout() const { return layoutPhase() == LayoutPhase::InRenderTreeLayout; }
    bool inPaintableState() const { return layoutPhase() != LayoutPhase::InRenderTreeLayout && layoutPhase() != LayoutPhase::InViewSizeAdjust && (layoutPhase() != LayoutPhase::InPostLayout || inAsynchronousTasks()); }

    bool isSkippedContentForLayout(const RenderElement&) const;
    bool isSkippedContentRootForLayout(const RenderBox&) const;

    NODELETE bool isPercentHeightResolveDisabledFor(const RenderBox& flexItem);
    NODELETE bool isInOrthogonalIntrinsicContributionLayout(const RenderBox&) const;
    NODELETE bool isComputingIntrinsicLogicalWidthFor(const RenderBox&) const;

    // The block formatting context roots whose floats the block formatting context placed, which the render tree's float lists do not have
    // then (LayoutIntegration::BlockLayout).
    void addRootWithFloatsPlacedByBlockFormattingContext(const RenderBlockFlow&);
    bool removeRootWithFloatsPlacedByBlockFormattingContext(const RenderBlockFlow&);
    bool hasFloatsPlacedByBlockFormattingContext(const RenderBlockFlow& root) const;

    // The block sizes of the containing block of a box that the block formatting context lays out through the render tree, which the content of
    // the box reads where it looks past the box: the block formatting context writes the geometry of the containing block once all of it is
    // laid out (LayoutIntegration::BlockIntegrationUtils::layoutIndependentFormattingContext).
    struct ContainingBlockBlockSizes {
        // What percentage block sizes that look past the box, which has an auto block size in quirks mode, resolve against; none is an
        // indefinite one (RenderBox::computePercentageLogicalHeight).
        std::optional<LayoutUnit> percentageBasis;
        // RenderBox::availableLogicalHeight of the containing block, without and with its margins, borders and padding.
        LayoutUnit availableBlockSize;
        LayoutUnit availableBlockSizeIncludingMarginBorderPadding;
    };
    std::optional<ContainingBlockBlockSizes> containingBlockBlockSizes(const RenderBox&) const;
    void setContainingBlockBlockSizes(const RenderBox&, const ContainingBlockBlockSizes&);
    void clearContainingBlockBlockSizes(const RenderBox&);
    // The border box inline size the stretch inline sizes of a block-level replaced element that the block formatting context lays out resolve
    // to: the stretch-fit inline size in the space beside the floats it places, which the render tree's float lists do not have
    // (LayoutIntegration::BlockIntegrationUtils::layoutIndependentFormattingContext).
    std::optional<LayoutUnit> stretchFitInlineSize(const RenderBox&) const;
    void setStretchFitInlineSize(const RenderBox&, LayoutUnit);
    void clearStretchFitInlineSize(const RenderBox&);
    // A box the block formatting context lays out whole while it breaks the content around it into fragmentainers, as monolithic content
    // (LayoutIntegration::BlockIntegrationUtils::layoutIndependentFormattingContext): the render tree breaks none of the box's content across
    // them either (RenderLayoutState::computePaginationInformation).
    bool isMonolithicBox(const RenderBox&) const;
    void addMonolithicBox(const RenderBox&);
    void removeMonolithicBox(const RenderBox&);
    // Where the block formatting context puts a box it lays out through the render tree in the multi-column flow it breaks into columns, from
    // the top of the flow, before it writes the geometry of the box and of the boxes around it, which the render tree finds the box's place in
    // the columns from otherwise (RenderFragmentedFlow::offsetFromLogicalTopOfFirstFragment).
    std::optional<LayoutUnit> offsetInFragmentedFlow(const RenderBox&) const;
    void setOffsetInFragmentedFlow(const RenderBox&, LayoutUnit);
    void clearOffsetInFragmentedFlow(const RenderBox&);
    // The block size the percentage block sizes in the content of a flex or grid item or a table cell resolved against at its last layout, from
    // which the block formatting context measured its intrinsic inline sizes (LayoutIntegration::BlockLayout::invalidateIntrinsicInlineSizesIfPercentageBasisChanged).
    std::optional<std::optional<LayoutUnit>> percentageBasisAtLastLayout(const RenderBlockFlow&) const;
    void setPercentageBasisAtLastLayout(const RenderBlockFlow&, std::optional<LayoutUnit>);
    // The fragments of the boxes that the block formatting context of a root broke across pages or columns whose fragments are not slices of the
    // rect they have in the strip of fragmentainers, which painting reads: those with box-decoration-break: clone, each fragment of which has
    // the box's decorations (css-break-3 §5.4), and those whose fragments have gaps between them, whose decorations are the slices of the
    // unbroken box. Replaced at every layout of the root (LayoutIntegration::BlockLayout::layoutContent).
    struct BlockFragment {
        // From the block-start edge of the box's border box.
        LayoutUnit blockOffset;
        LayoutUnit blockSize;
        // Where in the strip of the fragmented flow its block-start edge is, which tells the column it is in.
        LayoutUnit fragmentainerPosition;
    };
    struct BoxFragments {
        bool clonesDecorations { false };
        // A box with no fragments of its own here that one that clones its decorations across columns, or a monolithic box past the end of
        // its column, is in, whose content each column paints what of is in it (paintsColumnsFromFragments).
        bool hasDescendantsPaintedByColumn { false };
        Vector<BlockFragment> fragments;
        // A monolithic box past the end of the fragmentainer it is in: where that fragmentainer is in the strip of the fragmented flow, whose
        // column paints the box whole.
        std::optional<LayoutUnit> monolithicBoxFragmentainerPosition;
    };
    void setBoxFragments(const RenderBlockFlow& root, Vector<std::pair<CheckedRef<const RenderBox>, BoxFragments>>&&, bool isColumnContent);
    const BoxFragments* boxFragments(const RenderBox&) const;
    // A multi-column flow with boxes that clone their decorations across its columns, or with monolithic boxes past the end of their columns,
    // which paint the fragments of the first ones in each of them, and the others whole in theirs, with no clip
    // (RenderMultiColumnSet::collectLayerFragments), and the rest of the content like one strip of columns.
    bool paintsColumnsFromFragments(const RenderBlockFlow& multiColumnFlow) const;

    struct TextBoxTrim {
        bool trimFirstFormattedLine { false };
        SingleThreadWeakPtr<const RenderBlockFlow> lastFormattedLineRoot;
    };
    std::optional<TextBoxTrim> textBoxTrim() const { return m_textBoxTrim; }
    void setTextBoxTrim(std::optional<TextBoxTrim> textBoxTrim) { m_textBoxTrim = textBoxTrim; }

    NODELETE RenderElement* subtreeLayoutRoot() const;
    void clearSubtreeLayoutRoot() { m_subtreeLayoutRoot.clear(); }
    void convertSubtreeLayoutToFullLayout();

    void reset();
    void resetFirstLayoutFlag() { m_firstLayout = true; }
    bool didFirstLayout() const { return !m_firstLayout; }

    void setNeedsFullRepaint() { m_needsFullRepaint = true; }
    bool needsFullRepaint() const { return m_needsFullRepaint; }

    void flushPostLayoutTasks();
    void didLayout(bool canDeferUpdateLayerPositions);

    NODELETE void requestUpdateLayerPositions(bool needsFullRepaint = false);
    void flushUpdateLayerPositions();
    void markForUpdateLayerPositionsAfterSVGTransformChange();

    // LBSE: batches synchronous transform-attribute mutations (SMIL, DOM) so the
    // transform-refresh + delta-repaint runs once per renderer per frame.
    void addPendingSVGTransformAttributeUpdate(RenderLayerModelObject&);
    void flushPendingSVGTransformAttributeUpdatesIfNeeded();

    bool updateCompositingLayersAfterStyleChange();
    void updateCompositingLayersAfterLayout();
    // Returns true if a pending compositing layer update was done.
    bool updateCompositingLayersAfterLayoutIfNeeded();

    NODELETE RenderLayoutState* layoutState() const LIFETIME_BOUND PURE_FUNCTION;
    // Returns true if layoutState should be used for its cached offset and clip.
    bool isPaintOffsetCacheEnabled() const { return !m_paintOffsetCacheDisableCount && layoutState(); }
#ifndef NDEBUG
    void checkLayoutState();
#endif
    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    NODELETE LayoutSize layoutDelta() const;
    NODELETE void addLayoutDelta(const LayoutSize& delta);
#if ASSERT_ENABLED
    bool layoutDeltaMatches(const LayoutSize& delta);
#endif
    using LayoutStateStack = Vector<std::unique_ptr<RenderLayoutState>>;
    const LayoutStateStack& layoutStateStack() const LIFETIME_BOUND { return m_layoutStateStack; }

    bool immediateRendererDestructionEnabledForTesting() const { return m_immediateRendererDestructionEnabledForTesting; }
    void setImmediateRendererDestructionEnabledForTesting(bool enabled) { m_immediateRendererDestructionEnabledForTesting = enabled; }

    std::optional<SubtreeScrollbarChangesState>& subtreeScrollbarChangesState() { return m_subtreeScrollbarChangesState; }
    const std::optional<SubtreeScrollbarChangesState>& subtreeScrollbarChangesState() const { return m_subtreeScrollbarChangesState; }
    void setSubtreeScrollbarChangesState(std::optional<SubtreeScrollbarChangesState>);

    UpdateScrollInfoAfterLayoutTransaction& updateScrollInfoAfterLayoutTransaction() LIFETIME_BOUND;
    NODELETE UpdateScrollInfoAfterLayoutTransaction* updateScrollInfoAfterLayoutTransactionIfExists() LIFETIME_BOUND { return m_updateScrollInfoAfterLayoutTransaction.get(); }
    void setBoxNeedsTransformUpdateAfterContainerLayout(RenderBox&, RenderBoxModelObject& container);
    Vector<SingleThreadWeakPtr<RenderBox>> takeBoxesNeedingTransformUpdateAfterContainerLayout(RenderBoxModelObject&);

    void startTrackingLayoutUpdates() { m_layoutUpdateCount = 0; }
    unsigned layoutUpdateCount() const { return m_layoutUpdateCount; }

    void startTrackingRenderLayerPositionUpdates() { m_renderLayerPositionUpdateCount = 0; }
    unsigned renderLayerPositionUpdateCount() const { return m_renderLayerPositionUpdateCount; }

    void incrementRenderersWithNamedPage() { ++m_renderersWithNamedPageCount; }
    void decrementRenderersWithNamedPage() { ASSERT(m_renderersWithNamedPageCount > 0); --m_renderersWithNamedPageCount; }
    bool hasRenderersWithNamedPage() const { return m_renderersWithNamedPageCount; }

    bool addToDetachedRendererList(RenderPtr<RenderObject>&& renderer) const { return m_detachedRendererList.append(WTF::move(renderer)); }
    void deleteDetachedRenderersNow() const { m_detachedRendererList.clear(); }

    void detachInlineContent(std::unique_ptr<LayoutIntegration::InlineContent>&&) const;
    void deleteDetachedInlineContentNow() const;

    Vector<AnchorScrollAdjuster>& anchorScrollAdjusters() LIFETIME_BOUND { return m_anchorScrollAdjusters; }
    const AnchorScrollAdjuster* anchorScrollAdjusterFor(const RenderBox& anchored) const LIFETIME_BOUND;
    AnchorScrollAdjuster::Diff registerAnchorScrollAdjuster(AnchorScrollAdjuster&&);
    void unregisterAnchorScrollAdjusterFor(const RenderBox& anchored, bool clearAnchorScrollAdjustment = true); // If clearAnchorScrollAdjustment is true, removes the layer's anchorScrollAdjustment; otherwise sets it to zero.
    void invalidateAnchorDependenciesForScroller(const RenderBox& scroller);
    void removeScrollerFromAnchorScrollAdjusters(const RenderBox& scroller);

    bool repaintsBlocked() const { return m_repaintsBlocked; }

    // The list items laying out an outside marker, which their first formatted line aligns with.
    using ListItemList = Vector<SingleThreadWeakPtr<RenderListItem>>;
    const ListItemList& listItemsWithOutsideMarker() const LIFETIME_BOUND { return m_listItemsWithOutsideMarker; }

private:
    friend class ListItemOutsideMarkerScope;
    friend class LayoutFrameScope;
    friend class LayoutStateMaintainer;
    friend class LineGridLayoutStateMaintainer;
    friend class LayoutStateDisabler;
    friend class SubtreeLayoutStateMaintainer;
    friend class FlexPercentResolveDisabler;
    friend class OrthogonalIntrinsicContributionLayoutScope;
    friend class IntrinsicLogicalWidthComputationScope;
    friend class ContentVisibilityOverrideScope;
    friend class RepaintBlocker;

    NODELETE bool needsLayoutInternal() const;

    void performLayout(bool canDeferUpdateLayerPositions);
    NODELETE bool canPerformLayout() const;
    bool isLayoutSchedulingEnabled() const { return m_layoutSchedulingIsEnabled; }

    bool hasPendingUpdateLayerPositions() const { return !!m_pendingUpdateLayerPositions; }

    void layoutTimerFired();
    void runPostLayoutTasks();
    void runOrScheduleAsynchronousTasks(bool canDeferUpdateLayerPositions);
    bool inAsynchronousTasks() const { return m_inAsynchronousTasks; }

    NODELETE void setSubtreeLayoutRoot(RenderElement&);

    void applyTextSizingIfNeeded(RenderElement& layoutRoot);

    void updateStyleForLayout();

    // These functions may only be accessed by LayoutStateMaintainer.
    // Subtree push/pop
    void pushLayoutState(RenderElement&);
    bool pushLayoutState(RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageHeight = 0_lu, bool pageHeightChanged = false);
    void popLayoutState();

    // Suspends the LayoutState optimization. Used under transforms that cannot be represented by
    // LayoutState (common in SVG) and when manipulating the render tree during layout in ways
    // that can trigger repaint of a non-child (e.g. when a list item moves its list marker around).
    // Note that even when disabled, LayoutState is still used to store layoutDelta.
    // These functions may only be accessed by LayoutStateMaintainer or LayoutStateDisabler.
    void disablePaintOffsetCache() { m_paintOffsetCacheDisableCount++; }
    void enablePaintOffsetCache() { ASSERT(m_paintOffsetCacheDisableCount > 0); m_paintOffsetCacheDisableCount--; }

    bool isVisiblityHiddenIgnored() const { return m_visiblityHiddenIsIgnored; }
    void setIsVisiblityHiddenIgnored(bool ignored) { m_visiblityHiddenIsIgnored = ignored; }

    bool isVisiblityAutoIgnored() const { return m_visiblityAutoIsIgnored; }
    void setIsVisiblityAutoIgnored(bool ignored) { m_visiblityAutoIsIgnored = ignored; }

    bool isRevealedWhenFoundIgnored() const { return m_revealedWhenFoundIgnored; }
    void setIsRevealedWhenFoundIgnored(bool ignored) { m_revealedWhenFoundIgnored = ignored; }

    void disablePercentHeightResolveFor(const RenderBox& flexItem);
    void enablePercentHeightResolveFor(const RenderBox& flexItem);

    void addOrthogonalIntrinsicContributionLayout(const RenderBox&);
    void removeOrthogonalIntrinsicContributionLayout(const RenderBox&);

    void addIntrinsicLogicalWidthComputationFor(const RenderBox&);
    void removeIntrinsicLogicalWidthComputationFor(const RenderBox&);

    void allowRepaints() { m_repaintsBlocked = false; }
    void blockRepaints() { m_repaintsBlocked = true; }

    NODELETE LocalFrame& frame() const;
    NODELETE LocalFrameView& view() const;
    NODELETE RenderView* renderView() const;
    NODELETE Document* document() const;

    SingleThreadWeakRef<LocalFrameView> m_frameView;
    Timer m_layoutTimer;
    Timer m_postLayoutTaskTimer;
    SingleThreadWeakPtr<RenderElement> m_subtreeLayoutRoot;

    bool m_layoutSchedulingIsEnabled { true };
    bool m_firstLayout { true };
    bool m_needsFullRepaint { true };
    bool m_inAsynchronousTasks { false };
    bool m_setNeedsLayoutWasDeferred { false };
    bool m_visiblityHiddenIsIgnored { false };
    bool m_visiblityAutoIsIgnored { false };
    bool m_revealedWhenFoundIgnored { false };
    bool m_updateCompositingLayersIsPending { false };
    bool m_repaintsBlocked { false };
    ListItemList m_listItemsWithOutsideMarker;
    LayoutPhase m_layoutPhase { LayoutPhase::OutsideLayout };
    enum class LayoutNestedState : uint8_t  { NotInLayout, NotNested, Nested };
    LayoutNestedState m_layoutNestedState { LayoutNestedState::NotInLayout };
    unsigned m_disableSetNeedsLayoutCount { 0 };
    unsigned m_paintOffsetCacheDisableCount { 0 };
    unsigned m_layoutUpdateCount { 0 };
    unsigned m_renderLayerPositionUpdateCount { 0 };
    unsigned m_renderersWithNamedPageCount { 0 };
    LayoutStateStack m_layoutStateStack;
    const std::unique_ptr<UpdateScrollInfoAfterLayoutTransaction> m_updateScrollInfoAfterLayoutTransaction;
    SingleThreadWeakHashMap<RenderBoxModelObject, Vector<SingleThreadWeakPtr<RenderBox>>> m_containersWithDescendantsNeedingTransformUpdate;
    SingleThreadWeakHashSet<RenderBox> m_percentHeightIgnoreList;
    SingleThreadWeakHashSet<RenderBox> m_boxesInOrthogonalIntrinsicContributionLayout;
    SingleThreadWeakHashSet<RenderBox> m_intrinsicLogicalWidthComputationList;
    SingleThreadWeakHashSet<RenderBlockFlow> m_rootsWithFloatsPlacedByBlockFormattingContext;
    SingleThreadWeakHashMap<RenderBox, ContainingBlockBlockSizes> m_containingBlockBlockSizes;
    SingleThreadWeakHashMap<RenderBox, LayoutUnit> m_stretchFitInlineSizes;
    SingleThreadWeakHashSet<RenderBox> m_monolithicBoxes;
    SingleThreadWeakHashMap<RenderBox, LayoutUnit> m_offsetsInFragmentedFlow;
    SingleThreadWeakHashMap<RenderBlockFlow, std::optional<LayoutUnit>> m_percentageBasesAtLastLayout;
    struct RootBoxFragments {
        SingleThreadWeakPtr<const RenderBlockFlow> root;
        BoxFragments fragments;
    };
    SingleThreadWeakHashMap<RenderBox, RootBoxFragments> m_boxFragments;
    SingleThreadWeakHashSet<RenderBlockFlow> m_multiColumnFlowsPaintingColumnsFromFragments;
    Vector<AnchorScrollAdjuster> m_anchorScrollAdjusters;
    std::optional<TextBoxTrim> m_textBoxTrim;
    std::optional<SubtreeScrollbarChangesState> m_subtreeScrollbarChangesState;
    bool m_immediateRendererDestructionEnabledForTesting { false };

    struct UpdateLayerPositions {
        void merge(const UpdateLayerPositions& other)
        {
            needsFullRepaint |= other.needsFullRepaint;
        }

        bool needsFullRepaint { false };
    };
    std::optional<UpdateLayerPositions> m_pendingUpdateLayerPositions;

    // Vector + per-renderer dedup flag chosen over WeakHashSet - ~10x cheaper on the
    // LBSE MotionMark/Suits rAF hot path (hash + weak-ptr lookup vs. a bit check).
    Vector<SingleThreadWeakPtr<RenderLayerModelObject>> m_pendingSVGTransformAttributeUpdates;

    struct RepaintRectEnvironment {
        float m_deviceScaleFactor { 0 };
        bool m_printing { false };
        bool m_useFixedLayout { false };

        bool operator==(const RepaintRectEnvironment&) const = default;
    };
    RepaintRectEnvironment m_lastRepaintRectEnvironment;

    class DetachedRendererList {
    public:
        bool append(RenderPtr<RenderObject>&&);
        void clear() { m_renderers.clear(); }

    private:
        SegmentedVector<std::unique_ptr<RenderObject>, 50> m_renderers;
    };
    mutable DetachedRendererList m_detachedRendererList;

    class DetachedInlineContentList {
    public:
        ~DetachedInlineContentList();
        void append(std::unique_ptr<LayoutIntegration::InlineContent>&&);
        void clear();

    private:
        Vector<std::unique_ptr<LayoutIntegration::InlineContent>> m_inlineContent;
    };
    mutable DetachedInlineContentList m_detachedInlineContent;
};

class RepaintBlocker {
public:
    explicit RepaintBlocker(Document&);
    ~RepaintBlocker();

private:
    const Ref<Document> m_document;
};

class ListItemOutsideMarkerScope {
public:
    ListItemOutsideMarkerScope(LocalFrameViewLayoutContext&, RenderListItem&);
    // The ones the block formatting context lays the content of out at once, from the outermost one.
    ListItemOutsideMarkerScope(LocalFrameViewLayoutContext&, const Vector<CheckedRef<RenderListItem>>&);
    NODELETE ~ListItemOutsideMarkerScope();

private:
    const CheckedRef<LocalFrameViewLayoutContext> m_layoutContext;
    size_t m_count { 1 };
};

} // namespace WebCore
