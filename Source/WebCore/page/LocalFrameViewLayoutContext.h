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
class RenderView;
namespace Layout {
class LayoutState;
class LayoutTree;
}

enum class LayoutOptions : uint8_t;

struct UpdateScrollInfoAfterLayoutTransaction {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(UpdateScrollInfoAfterLayoutTransaction);

    UpdateScrollInfoAfterLayoutTransaction();
    ~UpdateScrollInfoAfterLayoutTransaction();

    int nestedCount { 0 };
    SingleThreadWeakHashSet<RenderBlock> blocks;
};

class LocalFrameViewLayoutContext final : public CanMakeCheckedPtr<LocalFrameViewLayoutContext, WTF::DefaultedOperatorEqual::No, WTF::CheckedPtrDeleteCheckException::Yes> {
    WTF_MAKE_TZONE_ALLOCATED(LocalFrameViewLayoutContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LocalFrameViewLayoutContext);
public:
    LocalFrameViewLayoutContext(LocalFrameView&);
    ~LocalFrameViewLayoutContext();

    WEBCORE_EXPORT void layout(bool canDeferUpdateLayerPositions = false);
    bool needsLayout(OptionSet<LayoutOptions> layoutOptions = { }) const;

    void interleavedLayout();

    // We rely on the side-effects of layout, like compositing updates, to update state in various subsystems
    // whose dependencies are poorly defined. This call triggers such updates.
    void setNeedsLayoutAfterViewConfigurationChange();

    void scheduleLayout();
    void scheduleSubtreeLayout(RenderElement& layoutRoot);
    void unscheduleLayout();

    void disableSetNeedsLayout();
    void enableSetNeedsLayout();

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

    bool isPercentHeightResolveDisabledFor(const RenderBox& flexItem);

    struct TextBoxTrim {
        bool trimFirstFormattedLine { false };
        SingleThreadWeakPtr<const RenderBlockFlow> lastFormattedLineRoot;
    };
    std::optional<TextBoxTrim> textBoxTrim() const { return m_textBoxTrim; }
    void setTextBoxTrim(std::optional<TextBoxTrim> textBoxTrim) { m_textBoxTrim = textBoxTrim; }

    RenderElement* subtreeLayoutRoot() const;
    void clearSubtreeLayoutRoot() { m_subtreeLayoutRoot.clear(); }
    void convertSubtreeLayoutToFullLayout();

    void reset();
    void resetFirstLayoutFlag() { m_firstLayout = true; }
    bool didFirstLayout() const { return !m_firstLayout; }

    void setNeedsFullRepaint() { m_needsFullRepaint = true; }
    bool needsFullRepaint() const { return m_needsFullRepaint; }

    void flushPostLayoutTasks();
    void didLayout(bool canDeferUpdateLayerPositions);

    void requestUpdateLayerPositions(bool needsFullRepaint = false);
    void flushUpdateLayerPositions();
    void markForUpdateLayerPositionsAfterSVGTransformChange();

    bool updateCompositingLayersAfterStyleChange();
    void updateCompositingLayersAfterLayout();
    // Returns true if a pending compositing layer update was done.
    bool updateCompositingLayersAfterLayoutIfNeeded();

    RenderLayoutState* layoutState() const PURE_FUNCTION;
    // Returns true if layoutState should be used for its cached offset and clip.
    bool isPaintOffsetCacheEnabled() const { return !m_paintOffsetCacheDisableCount && layoutState(); }
#ifndef NDEBUG
    void checkLayoutState();
#endif
    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    LayoutSize layoutDelta() const;
    void addLayoutDelta(const LayoutSize& delta);
#if ASSERT_ENABLED
    bool layoutDeltaMatches(const LayoutSize& delta);
#endif
    using LayoutStateStack = Vector<std::unique_ptr<RenderLayoutState>>;

    UpdateScrollInfoAfterLayoutTransaction& updateScrollInfoAfterLayoutTransaction();
    UpdateScrollInfoAfterLayoutTransaction* updateScrollInfoAfterLayoutTransactionIfExists() { return m_updateScrollInfoAfterLayoutTransaction.get(); }
    void setBoxNeedsTransformUpdateAfterContainerLayout(RenderBox&, RenderBlock& container);
    Vector<SingleThreadWeakPtr<RenderBox>> takeBoxesNeedingTransformUpdateAfterContainerLayout(RenderBlock&);

    void startTrackingLayoutUpdates() { m_layoutUpdateCount = 0; }
    unsigned layoutUpdateCount() const { return m_layoutUpdateCount; }

    void startTrackingRenderLayerPositionUpdates() { m_renderLayerPositionUpdateCount = 0; }
    unsigned renderLayerPositionUpdateCount() const { return m_renderLayerPositionUpdateCount; }

    bool addToDetachedRendererList(RenderPtr<RenderObject>&& renderer) const { return m_detachedRendererList.append(WTF::move(renderer)); }
    void deleteDetachedRenderersNow() const { m_detachedRendererList.clear(); }

    Vector<AnchorScrollAdjuster>& anchorScrollAdjusters() { return m_anchorScrollAdjusters; }
    const AnchorScrollAdjuster* anchorScrollAdjusterFor(const RenderBox& anchored) const;
    AnchorScrollAdjuster::Diff registerAnchorScrollAdjuster(AnchorScrollAdjuster&&);
    void unregisterAnchorScrollAdjusterFor(const RenderBox& anchored);
    void invalidateAnchorDependenciesForScroller(const RenderBox& scroller);
    void removeScrollerFromAnchorScrollAdjusters(const RenderBox& scroller);

private:
    friend class LayoutFrameScope;
    friend class LayoutStateMaintainer;
    friend class LayoutStateDisabler;
    friend class SubtreeLayoutStateMaintainer;
    friend class FlexPercentResolveDisabler;
    friend class ContentVisibilityOverrideScope;

    bool needsLayoutInternal() const;

    void performLayout(bool canDeferUpdateLayerPositions);
    bool canPerformLayout() const;
    bool isLayoutSchedulingEnabled() const { return m_layoutSchedulingIsEnabled; }

    bool hasPendingUpdateLayerPositions() const { return !!m_pendingUpdateLayerPositions; }

    void layoutTimerFired();
    void runPostLayoutTasks();
    void runOrScheduleAsynchronousTasks(bool canDeferUpdateLayerPositions);
    bool inAsynchronousTasks() const { return m_inAsynchronousTasks; }

    void setSubtreeLayoutRoot(RenderElement&);

#if ENABLE(TEXT_AUTOSIZING)
    void applyTextSizingIfNeeded(RenderElement& layoutRoot);
#endif
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

    LocalFrame& frame() const;
    Ref<LocalFrame> protectedFrame();
    LocalFrameView& view() const;
    Ref<LocalFrameView> protectedView() const;
    RenderView* renderView() const;
    Document* document() const;
    RefPtr<Document> protectedDocument() const;

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
    LayoutPhase m_layoutPhase { LayoutPhase::OutsideLayout };
    enum class LayoutNestedState : uint8_t  { NotInLayout, NotNested, Nested };
    LayoutNestedState m_layoutNestedState { LayoutNestedState::NotInLayout };
    unsigned m_disableSetNeedsLayoutCount { 0 };
    unsigned m_paintOffsetCacheDisableCount { 0 };
    unsigned m_layoutUpdateCount { 0 };
    unsigned m_renderLayerPositionUpdateCount { 0 };
    LayoutStateStack m_layoutStateStack;
    std::unique_ptr<UpdateScrollInfoAfterLayoutTransaction> m_updateScrollInfoAfterLayoutTransaction;
    SingleThreadWeakHashMap<RenderBlock, Vector<SingleThreadWeakPtr<RenderBox>>> m_containersWithDescendantsNeedingTransformUpdate;
    SingleThreadWeakHashSet<RenderBox> m_percentHeightIgnoreList;
    Vector<AnchorScrollAdjuster> m_anchorScrollAdjusters;
    std::optional<TextBoxTrim> m_textBoxTrim;

    struct UpdateLayerPositions {
        void merge(const UpdateLayerPositions& other)
        {
            needsFullRepaint |= other.needsFullRepaint;
        }

        bool needsFullRepaint { false };
    };
    std::optional<UpdateLayerPositions> m_pendingUpdateLayerPositions;

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
};

} // namespace WebCore
