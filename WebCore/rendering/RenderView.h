/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "RenderBlock.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderWidget;

#if USE(ACCELERATED_COMPOSITING)
class RenderLayerCompositor;
#endif

class RenderView : public RenderBlock {
public:
    RenderView(Node*, FrameView*);
    virtual ~RenderView();

    virtual const char* renderName() const { return "RenderView"; }

    virtual bool isRenderView() const { return true; }

    virtual void layout();
    virtual void calcWidth();
    virtual void calcHeight();
    virtual void calcPrefWidths();

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight() const;
    int viewWidth() const;

    float zoomFactor() const;

    FrameView* frameView() const { return m_frameView; }

    virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect&, bool fixed = false);
    virtual void repaintViewRectangle(const IntRect&, bool immediate = false);
    // Repaint the view, and all composited layers that intersect the given absolute rectangle.
    // FIXME: ideally we'd never have to do this, if all repaints are container-relative.
    virtual void repaintRectangleInViewAndCompositedLayers(const IntRect&, bool immediate = false);

    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);

    enum SelectionRepaintMode { RepaintNewXOROld, RepaintNewMinusOld };
    void setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode = RepaintNewXOROld);
    void clearSelection();
    virtual RenderObject* selectionStart() const { return m_selectionStart; }
    virtual RenderObject* selectionEnd() const { return m_selectionEnd; }

    bool printing() const;
    void setPrintImages(bool enable) { m_printImages = enable; }
    bool printImages() const { return m_printImages; }
    void setTruncatedAt(int y) { m_truncatedAt = y; m_bestTruncatedAt = m_truncatorWidth = 0; m_minimumColumnHeight = 0; m_forcedPageBreak = false; }
    void setBestTruncatedAt(int y, RenderBoxModelObject* forRenderer, bool forcedBreak = false);
    void setMinimumColumnHeight(int height) { m_minimumColumnHeight = height; }
    int bestTruncatedAt() const { return m_bestTruncatedAt; }
    int minimumColumnHeight() const { return m_minimumColumnHeight; }

    int truncatedAt() const { return m_truncatedAt; }

    virtual void absoluteRects(Vector<IntRect>&, int tx, int ty);
    virtual void absoluteQuads(Vector<FloatQuad>&);

    IntRect selectionBounds(bool clipToVisibleContent = true) const;

#if USE(ACCELERATED_COMPOSITING)
    void setMaximalOutlineSize(int o);
#else
    void setMaximalOutlineSize(int o) { m_maximalOutlineSize = o; }
#endif
    int maximalOutlineSize() const { return m_maximalOutlineSize; }

    virtual IntRect viewRect() const;

    void selectionStartEnd(int& startPos, int& endPos) const;

    IntRect printRect() const { return m_printRect; }
    void setPrintRect(const IntRect& r) { m_printRect = r; }

    void updateWidgetPositions();
    void addWidget(RenderWidget*);
    void removeWidget(RenderWidget*);

    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    IntSize layoutDelta() const
    {
        return m_layoutState ? m_layoutState->m_layoutDelta : IntSize();
    }
    void addLayoutDelta(const IntSize& delta) 
    {
        if (m_layoutState)
            m_layoutState->m_layoutDelta += delta;
    }

    bool doingFullRepaint() const { return m_frameView->needsFullRepaint(); }

    void pushLayoutState(RenderBox* renderer, const IntSize& offset)
    {
        if (doingFullRepaint())
            return;
        // We push LayoutState even if layoutState is disabled because it stores layoutDelta too.
        m_layoutState = new (renderArena()) LayoutState(m_layoutState, renderer, offset);
    }

    void pushLayoutState(RenderObject*);

    void popLayoutState()
    {
        if (doingFullRepaint())
            return;
        LayoutState* state = m_layoutState;
        m_layoutState = state->m_next;
        state->destroy(renderArena());
    }

    bool shouldDisableLayoutStateForSubtree(RenderObject*) const;

    // Returns true if layoutState should be used for its cached offset and clip.
    bool layoutStateEnabled() const { return m_layoutStateDisableCount == 0 && m_layoutState; }
    LayoutState* layoutState() const { return m_layoutState; }

    // Suspends the LayoutState optimization. Used under transforms that cannot be represented by
    // LayoutState (common in SVG) and when manipulating the render tree during layout in ways
    // that can trigger repaint of a non-child (e.g. when a list item moves its list marker around).
    // Note that even when disabled, LayoutState is still used to store layoutDelta.
    void disableLayoutState() { m_layoutStateDisableCount++; }
    void enableLayoutState() { ASSERT(m_layoutStateDisableCount > 0); m_layoutStateDisableCount--; }

    virtual void updateHitTestResult(HitTestResult&, const IntPoint&);

    // Notifications that this view became visible in a window, or will be
    // removed from the window.
    void didMoveOnscreen();
    void willMoveOffscreen();

#if USE(ACCELERATED_COMPOSITING)
    RenderLayerCompositor* compositor();
    bool usesCompositing() const;
#endif

protected:
    virtual void mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState&) const;
    virtual void mapAbsoluteToLocalPoint(bool fixed, bool useTransforms, TransformState&) const;

private:
    bool shouldRepaint(const IntRect& r) const;
        
    int docHeight() const;
    int docWidth() const;

protected:
    FrameView* m_frameView;

    RenderObject* m_selectionStart;
    RenderObject* m_selectionEnd;
    int m_selectionStartPos;
    int m_selectionEndPos;

    // used to ignore viewport width when printing to the printer
    bool m_printImages;
    int m_truncatedAt;

    int m_maximalOutlineSize; // Used to apply a fudge factor to dirty-rect checks on blocks/tables.
    IntRect m_printRect; // Used when printing.

    typedef HashSet<RenderWidget*> RenderWidgetSet;

    RenderWidgetSet m_widgets;

private:
    int m_bestTruncatedAt;
    int m_truncatorWidth;
    int m_minimumColumnHeight;
    bool m_forcedPageBreak;
    LayoutState* m_layoutState;
    unsigned m_layoutStateDisableCount;
#if USE(ACCELERATED_COMPOSITING)
    OwnPtr<RenderLayerCompositor> m_compositor;
#endif
};

inline RenderView* toRenderView(RenderObject* object)
{
    ASSERT(!object || object->isRenderView());
    return static_cast<RenderView*>(object);
}

inline const RenderView* toRenderView(const RenderObject* object)
{
    ASSERT(!object || object->isRenderView());
    return static_cast<const RenderView*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderView(const RenderView*);


// Stack-based class to assist with LayoutState push/pop
class LayoutStateMaintainer : public Noncopyable {
public:
    // ctor to push now
    LayoutStateMaintainer(RenderView* view, RenderBox* root, IntSize offset, bool disableState = false)
        : m_view(view)
        , m_disabled(disableState)
        , m_didStart(false)
        , m_didEnd(false)
    {
        push(root, offset);
    }
    
    // ctor to maybe push later
    LayoutStateMaintainer(RenderView* view)
        : m_view(view)
        , m_disabled(false)
        , m_didStart(false)
        , m_didEnd(false)
    {
    }
    
    ~LayoutStateMaintainer()
    {
        ASSERT(m_didStart == m_didEnd);   // if this fires, it means that someone did a push(), but forgot to pop().
    }

    void push(RenderBox* root, IntSize offset)
    {
        ASSERT(!m_didStart);
        // We push state even if disabled, because we still need to store layoutDelta
        m_view->pushLayoutState(root, offset);
        if (m_disabled)
            m_view->disableLayoutState();
        m_didStart = true;
    }

    void pop()
    {
        if (m_didStart) {
            ASSERT(!m_didEnd);
            m_view->popLayoutState();
            if (m_disabled)
                m_view->enableLayoutState();
            m_didEnd = true;
        }
    }

    bool didPush() const { return m_didStart; }

private:
    RenderView* m_view;
    bool m_disabled : 1;        // true if the offset and clip part of layoutState is disabled
    bool m_didStart : 1;        // true if we did a push or disable
    bool m_didEnd : 1;          // true if we popped or re-enabled
};

} // namespace WebCore

#endif // RenderView_h
