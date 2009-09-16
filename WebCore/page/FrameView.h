/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef FrameView_h
#define FrameView_h

#include "Frame.h"
#include "IntSize.h"
#include "Page.h"
#include "RenderLayer.h"
#include "ScrollView.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class Color;
class Event;
class Frame;
class FrameViewPrivate;
class InspectorTimelineAgent;
class IntRect;
class Node;
class PlatformMouseEvent;
class RenderLayer;
class RenderObject;
class RenderPartObject;
class ScheduledEvent;
class String;

template <typename T> class Timer;

class FrameView : public ScrollView {
public:
    friend class RenderView;

    static PassRefPtr<FrameView> create(Frame*);
    static PassRefPtr<FrameView> create(Frame*, const IntSize& initialSize);

    virtual ~FrameView();

    virtual HostWindow* hostWindow() const;
    
    virtual void invalidateRect(const IntRect&);

    Frame* frame() const { return m_frame.get(); }
    void clearFrame();

    int marginWidth() const { return m_margins.width(); } // -1 means default
    int marginHeight() const { return m_margins.height(); } // -1 means default
    void setMarginWidth(int);
    void setMarginHeight(int);

    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);

    virtual void setContentsSize(const IntSize&);

    void layout(bool allowSubtree = true);
    bool didFirstLayout() const;
    void layoutTimerFired(Timer<FrameView>*);
    void scheduleRelayout();
    void scheduleRelayoutOfSubtree(RenderObject*);
    void unscheduleRelayout();
    bool layoutPending() const;

    RenderObject* layoutRoot(bool onlyDuringLayout = false) const;
    int layoutCount() const { return m_layoutCount; }

    // These two helper functions just pass through to the RenderView.
    bool needsLayout() const;
    void setNeedsLayout();

    bool needsFullRepaint() const { return m_doFullRepaint; }

#if USE(ACCELERATED_COMPOSITING)
    void updateCompositingLayers();

    // Called when changes to the GraphicsLayer hierarchy have to be synchronized with
    // content rendered via the normal painting path.
    void setNeedsOneShotDrawingSynchronization();
#endif
    // Only used with accelerated compositing, but outside the #ifdef to make linkage easier.
    // Returns true if the sync was completed.
    bool syncCompositingStateRecursive();

    void didMoveOnscreen();
    void willMoveOffscreen();

    void resetScrollbars();
    void detachCustomScrollbars();

    void clear();

    bool isTransparent() const;
    void setTransparent(bool isTransparent);

    Color baseBackgroundColor() const;
    void setBaseBackgroundColor(Color);
    void updateBackgroundRecursively(const Color&, bool);

    bool shouldUpdateWhileOffscreen() const;
    void setShouldUpdateWhileOffscreen(bool);

    void adjustViewSize();
    
    virtual IntRect windowClipRect(bool clipToContents = true) const;
    IntRect windowClipRectForLayer(const RenderLayer*, bool clipToLayerContents) const;

    virtual IntRect windowResizerRect() const;

    virtual void scrollRectIntoViewRecursively(const IntRect&);
    virtual void setScrollPosition(const IntPoint&);

    String mediaType() const;
    void setMediaType(const String&);

    void setUseSlowRepaints();
    void setIsOverlapped(bool);
    void setContentIsOpaque(bool);

    void addSlowRepaintObject();
    void removeSlowRepaintObject();

    void beginDeferredRepaints();
    void endDeferredRepaints();
    void checkStopDelayingDeferredRepaints();
    void resetDeferredRepaintDelay();

#if ENABLE(DASHBOARD_SUPPORT)
    void updateDashboardRegions();
#endif
    void updateControlTints();

    void restoreScrollbar();

    void scheduleEvent(PassRefPtr<Event>, PassRefPtr<Node>);
    void pauseScheduledEvents();
    void resumeScheduledEvents();
    void postLayoutTimerFired(Timer<FrameView>*);

    bool wasScrolledByUser() const;
    void setWasScrolledByUser(bool);

    void addWidgetToUpdate(RenderPartObject*);
    void removeWidgetToUpdate(RenderPartObject*);

    virtual void paintContents(GraphicsContext*, const IntRect& damageRect);
    void setPaintRestriction(PaintRestriction);
    bool isPainting() const;
    void setNodeToDraw(Node*);

    static double currentPaintTimeStamp() { return sCurrentPaintTimeStamp; } // returns 0 if not painting
    
    void layoutIfNeededRecursive();
    void flushDeferredRepaints();

    void setIsVisuallyNonEmpty() { m_isVisuallyNonEmpty = true; }

    void forceLayout(bool allowSubtree = false);
    void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth, bool adjustViewSize);

    void adjustPageHeight(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

    void maintainScrollPositionAtAnchor(Node*);

    // Methods to convert points and rects between the coordinate space of the renderer, and this view.
    virtual IntRect convertFromRenderer(const RenderObject*, const IntRect&) const;
    virtual IntRect convertToRenderer(const RenderObject*, const IntRect&) const;
    virtual IntPoint convertFromRenderer(const RenderObject*, const IntPoint&) const;
    virtual IntPoint convertToRenderer(const RenderObject*, const IntPoint&) const;

    bool isFrameViewScrollCorner(RenderScrollbarPart* scrollCorner) const { return m_scrollCorner == scrollCorner; }
    void invalidateScrollCorner();

private:
    FrameView(Frame*);

    void reset();
    void init();

    virtual bool isFrameView() const;

    friend class RenderWidget;
    bool useSlowRepaints() const;

    void applyOverflowToViewport(RenderObject*, ScrollbarMode& hMode, ScrollbarMode& vMode);

    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void dispatchScheduledEvents();
    void performPostLayoutTasks();

    virtual void repaintContentRectangle(const IntRect&, bool immediate);
    virtual void contentsResized() { setNeedsLayout(); }
    virtual void visibleContentsResized();

    // Override ScrollView methods to do point conversion via renderers, in order to
    // take transforms into account.
    virtual IntRect convertToContainingView(const IntRect&) const;
    virtual IntRect convertFromContainingView(const IntRect&) const;
    virtual IntPoint convertToContainingView(const IntPoint&) const;
    virtual IntPoint convertFromContainingView(const IntPoint&) const;

    // ScrollBarClient interface
    virtual void valueChanged(Scrollbar*);
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
    virtual bool isActive() const;
    virtual void getTickmarks(Vector<IntRect>&) const;

    void deferredRepaintTimerFired(Timer<FrameView>*);
    void doDeferredRepaints();
    void updateDeferredRepaintDelay();
    double adjustedDeferredRepaintDelay() const;

    bool updateWidgets();
    void scrollToAnchor();

#if ENABLE(INSPECTOR)
    InspectorTimelineAgent* inspectorTimelineAgent() const;
#endif
    
    bool hasCustomScrollbars() const;

    virtual void updateScrollCorner();
    virtual void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect);

    static double sCurrentPaintTimeStamp; // used for detecting decoded resource thrash in the cache

    IntSize m_size;
    IntSize m_margins;
    OwnPtr<HashSet<RenderPartObject*> > m_widgetUpdateSet;
    RefPtr<Frame> m_frame;

    bool m_doFullRepaint;
    
    bool m_useSlowRepaints;
    bool m_isOverlapped;
    bool m_contentIsOpaque;
    unsigned m_slowRepaintObjectCount;

    int m_borderX, m_borderY;

    Timer<FrameView> m_layoutTimer;
    bool m_delayedLayout;
    RenderObject* m_layoutRoot;
    
    bool m_layoutSchedulingEnabled;
    bool m_midLayout;
    int m_layoutCount;
    unsigned m_nestedLayoutCount;
    Timer<FrameView> m_postLayoutTasksTimer;
    bool m_firstLayoutCallbackPending;

    bool m_firstLayout;
    bool m_isTransparent;
    Color m_baseBackgroundColor;
    IntSize m_lastLayoutSize;
    float m_lastZoomFactor;

    String m_mediaType;
    
    unsigned m_enqueueEvents;
    Vector<ScheduledEvent*> m_scheduledEvents;
    
    bool m_overflowStatusDirty;
    bool m_horizontalOverflow;
    bool m_verticalOverflow;    
    RenderObject* m_viewportRenderer;

    bool m_wasScrolledByUser;
    bool m_inProgrammaticScroll;
    
    unsigned m_deferringRepaints;
    unsigned m_repaintCount;
    Vector<IntRect> m_repaintRects;
    Timer<FrameView> m_deferredRepaintTimer;
    double m_deferredRepaintDelay;
    double m_lastPaintTime;

    bool m_shouldUpdateWhileOffscreen;

    unsigned m_deferSetNeedsLayouts;
    bool m_setNeedsLayoutWasDeferred;

    RefPtr<Node> m_nodeToDraw;
    PaintRestriction m_paintRestriction;
    bool m_isPainting;

    bool m_isVisuallyNonEmpty;
    bool m_firstVisuallyNonEmptyLayoutCallbackPending;

    RefPtr<Node> m_maintainScrollPositionAnchor;

    // Renderer to hold our custom scroll corner.
    RenderScrollbarPart* m_scrollCorner;
};

#if ENABLE(INSPECTOR)
inline InspectorTimelineAgent* FrameView::inspectorTimelineAgent() const
{
    return m_frame->page() ? m_frame->page()->inspectorTimelineAgent() : 0;
}
#endif

} // namespace WebCore

#endif // FrameView_h
