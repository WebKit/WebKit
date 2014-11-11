/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderView_h
#define RenderView_h

#include "FrameView.h"
#include "LayoutState.h"
#include "PODFreeListArena.h"
#include "Region.h"
#include "RenderBlockFlow.h"
#include "SelectionSubtreeRoot.h"
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

#if ENABLE(SERVICE_CONTROLS)
#include "SelectionRectGatherer.h"
#endif

namespace WebCore {

class FlowThreadController;
class ImageQualityController;
class RenderLayerCompositor;
class RenderQuote;

class RenderView final : public RenderBlockFlow, public SelectionSubtreeRoot {
public:
    RenderView(Document&, PassRef<RenderStyle>);
    virtual ~RenderView();

    WEBCORE_EXPORT bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    virtual const char* renderName() const override { return "RenderView"; }

    virtual bool requiresLayer() const override { return true; }

    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;

    virtual void layout() override;
    virtual void updateLogicalWidth() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    virtual LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const override;

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight() const;
    int viewWidth() const;
    int viewLogicalWidth() const { return style().isHorizontalWritingMode() ? viewWidth() : viewHeight(); }
    int viewLogicalHeight() const;

    LayoutUnit clientLogicalWidthForFixedPosition() const;
    LayoutUnit clientLogicalHeightForFixedPosition() const;

    float zoomFactor() const;

    FrameView& frameView() const { return m_frameView; }

    virtual LayoutRect visualOverflowRect() const override;
    virtual void computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect&, bool fixed = false) const override;
    void repaintRootContents();
    void repaintViewRectangle(const LayoutRect&) const;
    void repaintViewAndCompositedLayers();

    virtual void paint(PaintInfo&, const LayoutPoint&) override;
    virtual void paintBoxDecorations(PaintInfo&, const LayoutPoint&) override;

    enum SelectionRepaintMode { RepaintNewXOROld, RepaintNewMinusOld, RepaintNothing };
    void setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode = RepaintNewXOROld);
    void getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const;
    void clearSelection();
    RenderObject* selectionUnsplitStart() const { return m_selectionUnsplitStart; }
    RenderObject* selectionUnsplitEnd() const { return m_selectionUnsplitEnd; }
    IntRect selectionBounds(bool clipToVisibleContent = true) const;
    void repaintSelection() const;

    bool printing() const;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    void setMaximalOutlineSize(int o);
    int maximalOutlineSize() const { return m_maximalOutlineSize; }

    LayoutRect viewRect() const;

    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    LayoutSize layoutDelta() const
    {
        return m_layoutState ? m_layoutState->m_layoutDelta : LayoutSize();
    }
    void addLayoutDelta(const LayoutSize& delta) 
    {
        if (m_layoutState) {
            m_layoutState->m_layoutDelta += delta;
#if !ASSERT_DISABLED && ENABLE(SATURATED_LAYOUT_ARITHMETIC)
            m_layoutState->m_layoutDeltaXSaturated |= m_layoutState->m_layoutDelta.width() == LayoutUnit::max() || m_layoutState->m_layoutDelta.width() == LayoutUnit::min();
            m_layoutState->m_layoutDeltaYSaturated |= m_layoutState->m_layoutDelta.height() == LayoutUnit::max() || m_layoutState->m_layoutDelta.height() == LayoutUnit::min();
#endif
        }
    }
    
#if !ASSERT_DISABLED
    bool layoutDeltaMatches(const LayoutSize& delta)
    {
        if (!m_layoutState)
            return false;
#if ENABLE(SATURATED_LAYOUT_ARITHMETIC)
        return (delta.width() == m_layoutState->m_layoutDelta.width() || m_layoutState->m_layoutDeltaXSaturated) && (delta.height() == m_layoutState->m_layoutDelta.height() || m_layoutState->m_layoutDeltaYSaturated);
#else
        return delta == m_layoutState->m_layoutDelta;
#endif
    }
#endif

    bool doingFullRepaint() const { return frameView().needsFullRepaint(); }

    // Subtree push/pop
    void pushLayoutState(RenderObject&);
    void popLayoutState(RenderObject&) { return popLayoutState(); } // Just doing this to keep popLayoutState() private and to make the subtree calls symmetrical.

    bool shouldDisableLayoutStateForSubtree(RenderObject*) const;

    // Returns true if layoutState should be used for its cached offset and clip.
    bool layoutStateEnabled() const { return m_layoutStateDisableCount == 0 && m_layoutState; }
    LayoutState* layoutState() const { return m_layoutState.get(); }

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    void setPageLogicalHeight(LayoutUnit height)
    {
        if (m_pageLogicalHeight != height) {
            m_pageLogicalHeight = height;
            m_pageLogicalHeightChanged = true;
        }
    }
    LayoutUnit pageOrViewLogicalHeight() const;

    // This method is used to assign a page number only when pagination modes have
    // a block progression. This happens with vertical-rl books for example, but it
    // doesn't happen for normal horizontal-tb books. This is a very specialized
    // function and should not be mistaken for a general page number API.
    unsigned pageNumberForBlockProgressionOffset(int offset) const;

    unsigned pageCount() const;

    // FIXME: These functions are deprecated. No code should be added that uses these.
    int bestTruncatedAt() const { return m_legacyPrinting.m_bestTruncatedAt; }
    void setBestTruncatedAt(int y, RenderBoxModelObject* forRenderer, bool forcedBreak = false);
    int truncatedAt() const { return m_legacyPrinting.m_truncatedAt; }
    void setTruncatedAt(int y)
    { 
        m_legacyPrinting.m_truncatedAt = y;
        m_legacyPrinting.m_bestTruncatedAt = 0;
        m_legacyPrinting.m_truncatorWidth = 0;
        m_legacyPrinting.m_forcedPageBreak = false;
    }
    const IntRect& printRect() const { return m_legacyPrinting.m_printRect; }
    void setPrintRect(const IntRect& r) { m_legacyPrinting.m_printRect = r; }
    // End deprecated functions.

    // Notification that this view moved into or out of a native window.
    void setIsInWindow(bool);

    WEBCORE_EXPORT RenderLayerCompositor& compositor();
    WEBCORE_EXPORT bool usesCompositing() const;

    WEBCORE_EXPORT IntRect unscaledDocumentRect() const;
    LayoutRect unextendedBackgroundRect(RenderBox* backgroundRenderer) const;
    LayoutRect backgroundRect(RenderBox* backgroundRenderer) const;

    WEBCORE_EXPORT IntRect documentRect() const;

    // Renderer that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;
    
    bool hasRenderNamedFlowThreads() const;
    bool checkTwoPassLayoutForAutoHeightRegions() const;
    FlowThreadController& flowThreadController();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    IntervalArena* intervalArena();

    IntSize viewportSizeForCSSViewportUnits() const;

    void setRenderQuoteHead(RenderQuote* head) { m_renderQuoteHead = head; }
    RenderQuote* renderQuoteHead() const { return m_renderQuoteHead; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter and quotes code.
    void addRenderCounter() { m_renderCounterCount++; }
    void removeRenderCounter() { ASSERT(m_renderCounterCount > 0); m_renderCounterCount--; }
    bool hasRenderCounters() { return m_renderCounterCount; }
    
    ImageQualityController& imageQualityController();

    void setHasSoftwareFilters(bool hasSoftwareFilters) { m_hasSoftwareFilters = hasSoftwareFilters; }
    bool hasSoftwareFilters() const { return m_hasSoftwareFilters; }

    uint64_t rendererCount() const { return m_rendererCount; }
    void didCreateRenderer() { ++m_rendererCount; }
    void didDestroyRenderer() { --m_rendererCount; }

    WEBCORE_EXPORT void resumePausedImageAnimationsIfNeeded();
    void addRendererWithPausedImageAnimations(RenderElement&);
    void removeRendererWithPausedImageAnimations(RenderElement&);

    class RepaintRegionAccumulator {
        WTF_MAKE_NONCOPYABLE(RepaintRegionAccumulator);
    public:
        RepaintRegionAccumulator(RenderView*);
        ~RepaintRegionAccumulator();

    private:
        RenderView* m_rootView;
        bool m_wasAccumulatingRepaintRegion;
    };

    void scheduleLazyRepaint(RenderBox&);
    void unscheduleLazyRepaint(RenderBox&);

protected:
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const override;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const override;
    virtual bool requiresColumns(int desiredColumnCount) const override;

private:
    void initializeLayoutState(LayoutState&);

    virtual void computeColumnCountAndWidth() override;

    bool shouldRepaint(const LayoutRect&) const;
    void flushAccumulatedRepaintRegion() const;

    // These functions may only be accessed by LayoutStateMaintainer.
    bool pushLayoutState(RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false)
    {
        // We push LayoutState even if layoutState is disabled because it stores layoutDelta too.
        if (!doingFullRepaint() || m_layoutState->isPaginated() || renderer.flowThreadContainingBlock()
            || m_layoutState->lineGrid() || (renderer.style().lineGrid() != RenderStyle::initialLineGrid() && renderer.isRenderBlockFlow())) {
            m_layoutState = std::make_unique<LayoutState>(WTF::move(m_layoutState), &renderer, offset, pageHeight, pageHeightChanged);
            pushLayoutStateForCurrentFlowThread(renderer);
            return true;
        }
        return false;
    }

    void popLayoutState()
    {
        popLayoutStateForCurrentFlowThread();
        m_layoutState = WTF::move(m_layoutState->m_next);
    }

    // Suspends the LayoutState optimization. Used under transforms that cannot be represented by
    // LayoutState (common in SVG) and when manipulating the render tree during layout in ways
    // that can trigger repaint of a non-child (e.g. when a list item moves its list marker around).
    // Note that even when disabled, LayoutState is still used to store layoutDelta.
    // These functions may only be accessed by LayoutStateMaintainer or LayoutStateDisabler.
    void disableLayoutState() { m_layoutStateDisableCount++; }
    void enableLayoutState() { ASSERT(m_layoutStateDisableCount > 0); m_layoutStateDisableCount--; }

    void layoutContent(const LayoutState&);
    void layoutContentInAutoLogicalHeightRegions(const LayoutState&);
    void layoutContentToComputeOverflowInRegions(const LayoutState&);
#ifndef NDEBUG
    void checkLayoutState(const LayoutState&);
#endif

    void pushLayoutStateForCurrentFlowThread(const RenderObject&);
    void popLayoutStateForCurrentFlowThread();

    friend class LayoutStateMaintainer;
    friend class LayoutStateDisabler;

    virtual bool isScrollableOrRubberbandableBox() const override;

    void splitSelectionBetweenSubtrees(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode blockRepaintMode);
    void clearSubtreeSelection(const SelectionSubtreeRoot&, SelectionRepaintMode, OldSelectionData&);
    void updateSelectionForSubtrees(RenderSubtreesMap&, SelectionRepaintMode);
    void applySubtreeSelection(SelectionSubtreeRoot&, RenderObject* start, RenderObject* end, int endPos, SelectionRepaintMode, const OldSelectionData&);
    LayoutRect subtreeSelectionBounds(const SelectionSubtreeRoot&, bool clipToVisibleContent = true) const;
    void repaintSubtreeSelection(const SelectionSubtreeRoot&) const;

private:
    FrameView& m_frameView;

    RenderObject* m_selectionUnsplitStart;
    RenderObject* m_selectionUnsplitEnd;
    int m_selectionUnsplitStartPos;
    int m_selectionUnsplitEndPos;

    uint64_t m_rendererCount;

    mutable std::unique_ptr<Region> m_accumulatedRepaintRegion;

    // FIXME: Only used by embedded WebViews inside AppKit NSViews.  Find a way to remove.
    struct LegacyPrinting {
        LegacyPrinting()
            : m_bestTruncatedAt(0)
            , m_truncatedAt(0)
            , m_truncatorWidth(0)
            , m_forcedPageBreak(false)
        { }

        int m_bestTruncatedAt;
        int m_truncatedAt;
        int m_truncatorWidth;
        IntRect m_printRect;
        bool m_forcedPageBreak;
    };
    LegacyPrinting m_legacyPrinting;
    // End deprecated members.

    int m_maximalOutlineSize; // Used to apply a fudge factor to dirty-rect checks on blocks/tables.

    bool shouldUsePrintingLayout() const;

    void lazyRepaintTimerFired(Timer<RenderView>&);

    Timer<RenderView> m_lazyRepaintTimer;
    HashSet<RenderBox*> m_renderersNeedingLazyRepaint;

    std::unique_ptr<ImageQualityController> m_imageQualityController;
    LayoutUnit m_pageLogicalHeight;
    bool m_pageLogicalHeightChanged;
    std::unique_ptr<LayoutState> m_layoutState;
    unsigned m_layoutStateDisableCount;
    std::unique_ptr<RenderLayerCompositor> m_compositor;
    std::unique_ptr<FlowThreadController> m_flowThreadController;
    RefPtr<IntervalArena> m_intervalArena;

    RenderQuote* m_renderQuoteHead;
    unsigned m_renderCounterCount;

    bool m_selectionWasCaret;
    bool m_hasSoftwareFilters;

    HashSet<RenderElement*> m_renderersWithPausedImageAnimation;

#if ENABLE(SERVICE_CONTROLS)
    SelectionRectGatherer m_selectionRectGatherer;
#endif
};

RENDER_OBJECT_TYPE_CASTS(RenderView, isRenderView())

// Stack-based class to assist with LayoutState push/pop
class LayoutStateMaintainer {
    WTF_MAKE_NONCOPYABLE(LayoutStateMaintainer);
public:
    // Constructor to push now.
    explicit LayoutStateMaintainer(RenderView& view, RenderBox& root, LayoutSize offset, bool disableState = false, LayoutUnit pageHeight = 0, bool pageHeightChanged = false)
        : m_view(view)
        , m_disabled(disableState)
        , m_didStart(false)
        , m_didEnd(false)
        , m_didCreateLayoutState(false)
    {
        push(root, offset, pageHeight, pageHeightChanged);
    }

    // Constructor to maybe push later.
    explicit LayoutStateMaintainer(RenderView& view)
        : m_view(view)
        , m_disabled(false)
        , m_didStart(false)
        , m_didEnd(false)
        , m_didCreateLayoutState(false)
    {
    }

    ~LayoutStateMaintainer()
    {
        ASSERT(m_didStart == m_didEnd);   // if this fires, it means that someone did a push(), but forgot to pop().
    }

    void push(RenderBox& root, LayoutSize offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false)
    {
        ASSERT(!m_didStart);
        // We push state even if disabled, because we still need to store layoutDelta
        m_didCreateLayoutState = m_view.pushLayoutState(root, offset, pageHeight, pageHeightChanged);
        if (m_disabled && m_didCreateLayoutState)
            m_view.disableLayoutState();
        m_didStart = true;
    }

    void pop()
    {
        if (m_didStart) {
            ASSERT(!m_didEnd);
            if (m_didCreateLayoutState) {
                m_view.popLayoutState();
                if (m_disabled)
                    m_view.enableLayoutState();
            }
            
            m_didEnd = true;
        }
    }

    bool didPush() const { return m_didStart; }

private:
    RenderView& m_view;
    bool m_disabled : 1;        // true if the offset and clip part of layoutState is disabled
    bool m_didStart : 1;        // true if we did a push or disable
    bool m_didEnd : 1;          // true if we popped or re-enabled
    bool m_didCreateLayoutState : 1; // true if we actually made a layout state.
};

class LayoutStateDisabler {
    WTF_MAKE_NONCOPYABLE(LayoutStateDisabler);
public:
    LayoutStateDisabler(RenderView* view)
        : m_view(view)
    {
        if (m_view)
            m_view->disableLayoutState();
    }

    ~LayoutStateDisabler()
    {
        if (m_view)
            m_view->enableLayoutState();
    }
private:
    RenderView* m_view;
};

} // namespace WebCore

#endif // RenderView_h
