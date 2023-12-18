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

#include "LayoutUnit.h"
#include "RenderLayerModelObject.h"
#include "Timer.h"
#include <wtf/CheckedRef.h>
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
class RenderObject;
class RenderElement;
class RenderLayoutState;
class RenderView;
namespace Layout {
class LayoutState;
class LayoutTree;
}

struct UpdateScrollInfoAfterLayoutTransaction {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    UpdateScrollInfoAfterLayoutTransaction();
    ~UpdateScrollInfoAfterLayoutTransaction();

    int nestedCount { 0 };
    SingleThreadWeakHashSet<RenderBlock> blocks;
};

class LocalFrameViewLayoutContext : public CanMakeCheckedPtr {
public:
    LocalFrameViewLayoutContext(LocalFrameView&);
    ~LocalFrameViewLayoutContext();

    WEBCORE_EXPORT void layout();
    bool needsLayout() const;

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

    bool needsSkippedContentLayout() const { return m_needsSkippedContentLayout; }
    void setNeedsSkippedContentLayout(bool needsSkippedContentLayout) { m_needsSkippedContentLayout = needsSkippedContentLayout; }

    unsigned layoutCount() const { return m_layoutCount; }

    RenderElement* subtreeLayoutRoot() const;
    void clearSubtreeLayoutRoot() { m_subtreeLayoutRoot.clear(); }
    void convertSubtreeLayoutToFullLayout();

    void reset();
    void resetFirstLayoutFlag() { m_firstLayout = true; }
    bool didFirstLayout() const { return !m_firstLayout; }

    void setNeedsFullRepaint() { m_needsFullRepaint = true; }
    bool needsFullRepaint() const { return m_needsFullRepaint; }

    void flushPostLayoutTasks();

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

    const Layout::LayoutState* layoutFormattingState() const { return m_layoutState.get(); }

    UpdateScrollInfoAfterLayoutTransaction& updateScrollInfoAfterLayoutTransaction();
    UpdateScrollInfoAfterLayoutTransaction* updateScrollInfoAfterLayoutTransactionIfExists() { return m_updateScrollInfoAfterLayoutTransaction.get(); }
    void setBoxNeedsTransformUpdateAfterContainerLayout(RenderBox&, RenderBlock& container);
    Vector<SingleThreadWeakPtr<RenderBox>> takeBoxesNeedingTransformUpdateAfterContainerLayout(RenderBlock&);

private:
    friend class LayoutScope;
    friend class LayoutStateMaintainer;
    friend class LayoutStateDisabler;
    friend class SubtreeLayoutStateMaintainer;

    void performLayout();
    bool canPerformLayout() const;
    bool isLayoutSchedulingEnabled() const { return m_layoutSchedulingIsEnabled; }

    void layoutTimerFired();
    void runPostLayoutTasks();
    void runOrScheduleAsynchronousTasks();
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

    LocalFrame& frame() const;
    LocalFrameView& view() const;
    RenderView* renderView() const;
    Document* document() const;

    LocalFrameView& m_frameView;
    Timer m_layoutTimer;
    Timer m_postLayoutTaskTimer;
    SingleThreadWeakPtr<RenderElement> m_subtreeLayoutRoot;

    bool m_layoutSchedulingIsEnabled { true };
    bool m_firstLayout { true };
    bool m_needsFullRepaint { true };
    bool m_inAsynchronousTasks { false };
    bool m_setNeedsLayoutWasDeferred { false };
    bool m_needsSkippedContentLayout { false };
    LayoutPhase m_layoutPhase { LayoutPhase::OutsideLayout };
    enum class LayoutNestedState : uint8_t  { NotInLayout, NotNested, Nested };
    LayoutNestedState m_layoutNestedState { LayoutNestedState::NotInLayout };
    unsigned m_layoutCount { 0 };
    unsigned m_disableSetNeedsLayoutCount { 0 };
    unsigned m_paintOffsetCacheDisableCount { 0 };
    LayoutStateStack m_layoutStateStack;
    std::unique_ptr<Layout::LayoutTree> m_layoutTree;
    std::unique_ptr<Layout::LayoutState> m_layoutState;
    std::unique_ptr<UpdateScrollInfoAfterLayoutTransaction> m_updateScrollInfoAfterLayoutTransaction;
    SingleThreadWeakHashMap<RenderBlock, Vector<SingleThreadWeakPtr<RenderBox>>> m_containersWithDescendantsNeedingTransformUpdate;
};

} // namespace WebCore
