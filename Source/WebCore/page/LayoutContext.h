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

#include "Timer.h"

#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Frame;
class FrameView;
class LayoutScope;
class RenderElement;
class RenderView;
    
class LayoutContext {
public:
    LayoutContext(FrameView&);

    void layout();

    void setNeedsLayout();
    bool needsLayout() const;

    void scheduleLayout();
    void scheduleSubtreeLayout(RenderElement& layoutRoot);
    void unscheduleLayout();

    void startDisallowingLayout() { ++m_layoutDisallowedCount; }
    void endDisallowingLayout() { ASSERT(m_layoutDisallowedCount > 0); --m_layoutDisallowedCount; }
    
    void disableSetNeedsLayout();
    void enableSetNeedsLayout();

    enum class LayoutPhase {
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

    unsigned layoutCount() const { return m_layoutCount; }

    RenderElement* subtreeLayoutRoot() const { return m_subtreeLayoutRoot.get(); }
    void clearSubtreeLayoutRoot() { m_subtreeLayoutRoot.clear(); }
    void convertSubtreeLayoutToFullLayout();

    void reset();
    void resetFirstLayoutFlag() { m_firstLayout = true; }
    bool didFirstLayout() const { return !m_firstLayout; }

    void setNeedsFullRepaint() { m_needsFullRepaint = true; }
    bool needsFullRepaint() const { return m_needsFullRepaint; }

    void flushAsynchronousTasks();

private:
    friend class LayoutScope;

    bool canPerformLayout() const;
    bool layoutDisallowed() const { return m_layoutDisallowedCount; }
    bool isLayoutSchedulingEnabled() const { return m_layoutSchedulingIsEnabled; }

    void layoutTimerFired();
    void runAsynchronousTasks();
    void runOrScheduleAsynchronousTasks();
    bool inAsynchronousTasks() const { return m_inAsynchronousTasks; }

    void setSubtreeLayoutRoot(RenderElement&);

#if ENABLE(TEXT_AUTOSIZING)
    void applyTextSizingIfNeeded(RenderElement& layoutRoot);
#endif
    void updateStyleForLayout();

    bool handleLayoutWithFrameFlatteningIfNeeded();
    void startLayoutAtMainFrameViewIfNeeded();

    Frame& frame() const;
    FrameView& view() const;
    RenderView* renderView() const;
    Document* document() const;

    FrameView& m_frameView;
    Timer m_layoutTimer;
    Timer m_asynchronousTasksTimer;

    bool m_layoutSchedulingIsEnabled { true };
    bool m_delayedLayout { false };
    bool m_firstLayout { true };
    bool m_needsFullRepaint { true };
    bool m_inAsynchronousTasks { false };
    bool m_setNeedsLayoutWasDeferred { false };
    LayoutPhase m_layoutPhase { LayoutPhase::OutsideLayout };
    enum class LayoutNestedState { NotInLayout, NotNested, Nested };
    LayoutNestedState m_layoutNestedState { LayoutNestedState::NotInLayout };
    unsigned m_layoutCount { 0 };
    unsigned m_disableSetNeedsLayoutCount { 0 };
    int m_layoutDisallowedCount { 0 };
    WeakPtr<RenderElement> m_subtreeLayoutRoot;
};

} // namespace WebCore
