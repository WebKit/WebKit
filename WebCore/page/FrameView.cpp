/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 */

#include "config.h"
#include "FrameView.h"

#include "AXObjectCache.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "OverflowEvent.h"
#include "RenderPart.h"
#include "RenderPartObject.h"
#include "RenderTheme.h"
#include "RenderView.h"

namespace WebCore {

using namespace HTMLNames;

struct ScheduledEvent {
    RefPtr<Event> m_event;
    RefPtr<EventTargetNode> m_eventTarget;
    bool m_tempEvent;
};

class FrameViewPrivate {
public:
    FrameViewPrivate(FrameView* view)
        : m_slowRepaintObjectCount(0)
        , layoutTimer(view, &FrameView::layoutTimerFired)
        , layoutRoot(0)
        , postLayoutTasksTimer(view, &FrameView::postLayoutTimerFired)
        , m_mediaType("screen")
        , m_enqueueEvents(0)
        , m_overflowStatusDirty(true)
        , m_viewportRenderer(0)
        , m_wasScrolledByUser(false)
        , m_inProgrammaticScroll(false)
    {
        isTransparent = false;
        baseBackgroundColor = Color::white;
        vmode = hmode = ScrollbarAuto;
        needToInitScrollbars = true;
        reset();
    }
    void reset()
    {
        useSlowRepaints = false;
        borderX = 30;
        borderY = 30;
        layoutTimer.stop();
        layoutRoot = 0;
        delayedLayout = false;
        doFullRepaint = true;
        layoutSchedulingEnabled = true;
        midLayout = false;
        layoutCount = 0;
        nestedLayoutCount = 0;
        postLayoutTasksTimer.stop();
        firstLayout = true;
        repaintRects.clear();
        m_wasScrolledByUser = false;
        lastLayoutSize = IntSize();
    }

    bool doFullRepaint;
    
    ScrollbarMode vmode;
    ScrollbarMode hmode;
    bool useSlowRepaints;
    unsigned m_slowRepaintObjectCount;

    int borderX, borderY;

    Timer<FrameView> layoutTimer;
    bool delayedLayout;
    RenderObject* layoutRoot;
    
    bool layoutSchedulingEnabled;
    bool midLayout;
    int layoutCount;
    unsigned nestedLayoutCount;
    Timer<FrameView> postLayoutTasksTimer;

    bool firstLayout;
    bool needToInitScrollbars;
    bool isTransparent;
    Color baseBackgroundColor;
    IntSize lastLayoutSize;

    // Used by objects during layout to communicate repaints that need to take place only
    // after all layout has been completed.
    Vector<RenderObject::RepaintInfo> repaintRects;
    
    String m_mediaType;
    
    unsigned m_enqueueEvents;
    Vector<ScheduledEvent*> m_scheduledEvents;
    
    bool m_overflowStatusDirty;
    bool horizontalOverflow;
    bool m_verticalOverflow;    
    RenderObject* m_viewportRenderer;

    bool m_wasScrolledByUser;
    bool m_inProgrammaticScroll;
};

FrameView::FrameView(Frame* frame)
    : m_refCount(1)
    , m_frame(frame)
    , d(new FrameViewPrivate(this))
{
    init();
    show();
}

#if !PLATFORM(MAC)
FrameView::FrameView(Frame* frame, const IntSize& initialSize)
    : m_refCount(1)
    , m_frame(frame)
    , d(new FrameViewPrivate(this))
{
    init();
    Widget::setFrameGeometry(IntRect(x(), y(), initialSize.width(), initialSize.height()));
    show();
}
#endif

FrameView::~FrameView()
{
    if (d->postLayoutTasksTimer.isActive()) {
        d->postLayoutTasksTimer.stop();
        d->m_scheduledEvents.clear();
        d->m_enqueueEvents = 0;
    }

    resetScrollbars();

    ASSERT(m_refCount == 0);
    ASSERT(d->m_scheduledEvents.isEmpty());
    ASSERT(!d->m_enqueueEvents);

    if (m_frame) {
        ASSERT(m_frame->view() != this || !m_frame->document() || !m_frame->document()->renderer());
        RenderPart* renderer = m_frame->ownerRenderer();
        if (renderer && renderer->widget() == this)
            renderer->setWidget(0);
    }

    delete d;
    d = 0;
}

bool FrameView::isFrameView() const 
{ 
    return true; 
}

void FrameView::clearFrame()
{
    m_frame = 0;
}

void FrameView::resetScrollbars()
{
    // Reset the document's scrollbars back to our defaults before we yield the floor.
    d->firstLayout = true;
    suppressScrollbars(true);
    ScrollView::setVScrollbarMode(d->vmode);
    ScrollView::setHScrollbarMode(d->hmode);
    suppressScrollbars(false);
}

void FrameView::init()
{
    m_margins = IntSize(-1, -1); // undefined
    m_size = IntSize();
}

void FrameView::clear()
{
    setStaticBackground(false);
    
    d->reset();

    if (m_frame)
        if (RenderPart* renderer = m_frame->ownerRenderer())
            renderer->viewCleared();

    suppressScrollbars(true);
}

bool FrameView::didFirstLayout() const
{
    return !d->firstLayout;
}

void FrameView::initScrollbars()
{
    if (!d->needToInitScrollbars)
        return;
    d->needToInitScrollbars = false;
    setScrollbarsMode(hScrollbarMode());
}

void FrameView::setMarginWidth(int w)
{
    // make it update the rendering area when set
    m_margins.setWidth(w);
}

void FrameView::setMarginHeight(int h)
{
    // make it update the rendering area when set
    m_margins.setHeight(h);
}

void FrameView::adjustViewSize()
{
    ASSERT(m_frame->view() == this);
    RenderView* root = static_cast<RenderView*>(m_frame->renderer());
    if (!root)
        return;
    resizeContents(root->overflowWidth(), root->overflowHeight());
}

void FrameView::applyOverflowToViewport(RenderObject* o, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.
    switch (o->style()->overflowX()) {
        case OHIDDEN:
            hMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            hMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            hMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }
    
     switch (o->style()->overflowY()) {
        case OHIDDEN:
            vMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            vMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            vMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }

    d->m_viewportRenderer = o;
}

int FrameView::layoutCount() const
{
    return d->layoutCount;
}

bool FrameView::needsFullRepaint() const
{
    return d->doFullRepaint;
}

void FrameView::addRepaintInfo(RenderObject* o, const IntRect& r)
{
    d->repaintRects.append(RenderObject::RepaintInfo(o, r));
}

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : d->layoutRoot;
}

void FrameView::layout(bool allowSubtree)
{
    if (d->midLayout)
        return;

    d->layoutTimer.stop();
    d->delayedLayout = false;

    // Protect the view from being deleted during layout (in recalcStyle)
    RefPtr<FrameView> protector(this);

    if (!m_frame) {
        // FIXME: Do we need to set m_size.width here?
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(visibleWidth());
        return;
    }
    
    // we shouldn't enter layout() while painting
    ASSERT(!m_frame->isPainting());
    if (m_frame->isPainting())
        return;

    if (!allowSubtree && d->layoutRoot) {
        d->layoutRoot->markContainingBlocksForLayout(false);
        d->layoutRoot = 0;
    }

    ASSERT(m_frame->view() == this);
    // This early return should be removed when rdar://5598072 is resolved. In the meantime, there is a
    // gigantic CrashTracer because of this issue, and the early return will hopefully cause graceful 
    // failure instead.  
    if (m_frame->view() != this)
        return;

    Document* document = m_frame->document();
    if (!document) {
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(visibleWidth());
        return;
    }

    d->layoutSchedulingEnabled = false;

    if (!d->nestedLayoutCount && d->postLayoutTasksTimer.isActive()) {
        // This is a new top-level layout. If there are any remaining tasks from the previous
        // layout, finish them now.
        d->postLayoutTasksTimer.stop();
        performPostLayoutTasks();
    }

    // Always ensure our style info is up-to-date.  This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    if (m_frame->needsReapplyStyles())
        m_frame->reapplyStyles();
    else if (document->hasChangedChild())
        document->recalcStyle();
    
    bool subtree = d->layoutRoot;

    // If there is only one ref to this view left, then its going to be destroyed as soon as we exit, 
    // so there's no point to continuing to layout
    if (protector->hasOneRef())
        return;

    RenderObject* root = subtree ? d->layoutRoot : document->renderer();
    if (!root) {
        // FIXME: Do we need to set m_size here?
        d->layoutSchedulingEnabled = true;
        return;
    }

    d->nestedLayoutCount++;

    ScrollbarMode hMode = d->hmode;
    ScrollbarMode vMode = d->vmode;

    if (!subtree) {
        RenderObject* rootRenderer = document->documentElement() ? document->documentElement()->renderer() : 0;
        if (document->isHTMLDocument()) {
            Node* body = static_cast<HTMLDocument*>(document)->body();
            if (body && body->renderer()) {
                if (body->hasTagName(framesetTag)) {
                    body->renderer()->setChildNeedsLayout(true);
                    vMode = ScrollbarAlwaysOff;
                    hMode = ScrollbarAlwaysOff;
                } else if (body->hasTagName(bodyTag)) {
                    if (!d->firstLayout && m_size.height() != visibleHeight()
                            && static_cast<RenderBox*>(body->renderer())->stretchesToViewHeight())
                        body->renderer()->setChildNeedsLayout(true);
                    // It's sufficient to just check the X overflow,
                    // since it's illegal to have visible in only one direction.
                    RenderObject* o = rootRenderer->style()->overflowX() == OVISIBLE 
                        ? body->renderer() : rootRenderer;
                    applyOverflowToViewport(o, hMode, vMode); // Only applies to HTML UAs, not to XML/XHTML UAs
                }
            }
        } else if (rootRenderer)
            applyOverflowToViewport(rootRenderer, hMode, vMode); // XML/XHTML UAs use the root element.
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (d->firstLayout && !document->ownerElement())
            printf("Elapsed time before first layout: %d\n", document->elapsedTime());
#endif
    }

    d->doFullRepaint = !subtree && (d->firstLayout || static_cast<RenderView*>(root)->printing());
    ASSERT(d->nestedLayoutCount > 1 || d->repaintRects.isEmpty());

    bool didFirstLayout = false;
    if (!subtree) {
        // Now set our scrollbar state for the layout.
        ScrollbarMode currentHMode = hScrollbarMode();
        ScrollbarMode currentVMode = vScrollbarMode();

        if (d->firstLayout || (hMode != currentHMode || vMode != currentVMode)) {
            suppressScrollbars(true);
            if (d->firstLayout) {
                d->firstLayout = false;
                didFirstLayout = true;
                d->lastLayoutSize = IntSize(width(), height());
                
                // Set the initial vMode to AlwaysOn if we're auto.
                if (vMode == ScrollbarAuto)
                    ScrollView::setVScrollbarMode(ScrollbarAlwaysOn); // This causes a vertical scrollbar to appear.
                // Set the initial hMode to AlwaysOff if we're auto.
                if (hMode == ScrollbarAuto)
                    ScrollView::setHScrollbarMode(ScrollbarAlwaysOff); // This causes a horizontal scrollbar to disappear.
            }
            
            if (hMode == vMode)
                ScrollView::setScrollbarsMode(hMode);
            else {
                ScrollView::setHScrollbarMode(hMode);
                ScrollView::setVScrollbarMode(vMode);
            }

            suppressScrollbars(false, true);
        }

        IntSize oldSize = m_size;

        m_size = IntSize(visibleWidth(), visibleHeight());

        if (oldSize != m_size)
            d->doFullRepaint = true;
    }
    
    RenderLayer* layer = root->enclosingLayer();

    pauseScheduledEvents();

    if (subtree)
        root->view()->pushLayoutState(root);
    d->midLayout = true;
    root->layout();
    d->midLayout = false;
    if (subtree)
        root->view()->popLayoutState();
    d->layoutRoot = 0;

    m_frame->invalidateSelection();
   
    d->layoutSchedulingEnabled = true;

    if (!subtree && !static_cast<RenderView*>(root)->printing())
        adjustViewSize();

    // Now update the positions of all layers.
    layer->updateLayerPositions(d->doFullRepaint);

    // FIXME: Could optimize this and have objects removed from this list
    // if they ever do full repaints.
    Vector<RenderObject::RepaintInfo>::iterator end = d->repaintRects.end();
    for (Vector<RenderObject::RepaintInfo>::iterator it = d->repaintRects.begin(); it != end; ++it)
        it->m_object->repaintRectangle(it->m_repaintRect);
    d->repaintRects.clear();
    
    d->layoutCount++;

#if PLATFORM(MAC)
    if (AXObjectCache::accessibilityEnabled())
        root->document()->axObjectCache()->postNotificationToElement(root, "AXLayoutComplete");
#endif
    updateDashboardRegions();

    if (didFirstLayout)
        m_frame->loader()->didFirstLayout();
    
    ASSERT(!root->needsLayout());

    setStaticBackground(useSlowRepaints());

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(visibleWidth() < contentsWidth(),
                             visibleHeight() < contentsHeight());

    if (!d->postLayoutTasksTimer.isActive()) {
        // Calls resumeScheduledEvents()
        performPostLayoutTasks();

        if (needsLayout()) {
            // Post-layout widget updates or an event handler made us need layout again.
            // Lay out again, but this time defer widget updates and event dispatch until after
            // we return.
            d->postLayoutTasksTimer.startOneShot(0);
            pauseScheduledEvents();
            layout();
        }
    } else {
        resumeScheduledEvents();
        ASSERT(d->m_enqueueEvents);
    }

    d->nestedLayoutCount--;
}

void FrameView::addWidgetToUpdate(RenderPartObject* object)
{
    if (!m_widgetUpdateSet)
        m_widgetUpdateSet.set(new HashSet<RenderPartObject*>);

    m_widgetUpdateSet->add(object);
}

void FrameView::removeWidgetToUpdate(RenderPartObject* object)
{
    if (!m_widgetUpdateSet)
        return;

    m_widgetUpdateSet->remove(object);
}

//
// Event Handling
//
/////////////////

bool FrameView::scrollTo(const IntRect& bounds)
{
    int x, y, xe, ye;
    x = bounds.x();
    y = bounds.y();
    xe = bounds.right() - 1;
    ye = bounds.bottom() - 1;
    
    int deltax;
    int deltay;

    int curHeight = visibleHeight();
    int curWidth = visibleWidth();

    if (ye - y>curHeight-d->borderY)
        ye = y + curHeight - d->borderY;

    if (xe - x>curWidth-d->borderX)
        xe = x + curWidth - d->borderX;

    // is xpos of target left of the view's border?
    if (x < contentsX() + d->borderX)
        deltax = x - contentsX() - d->borderX;
    // is xpos of target right of the view's right border?
    else if (xe + d->borderX > contentsX() + curWidth)
        deltax = xe + d->borderX - (contentsX() + curWidth);
    else
        deltax = 0;

    // is ypos of target above upper border?
    if (y < contentsY() + d->borderY)
        deltay = y - contentsY() - d->borderY;
    // is ypos of target below lower border?
    else if (ye + d->borderY > contentsY() + curHeight)
        deltay = ye + d->borderY - (contentsY() + curHeight);
    else
        deltay = 0;

    int maxx = curWidth - d->borderX;
    int maxy = curHeight - d->borderY;

    int scrollX = deltax > 0 ? (deltax > maxx ? maxx : deltax) : deltax == 0 ? 0 : (deltax > -maxx ? deltax : -maxx);
    int scrollY = deltay > 0 ? (deltay > maxy ? maxy : deltay) : deltay == 0 ? 0 : (deltay > -maxy ? deltay : -maxy);

    if (contentsX() + scrollX < 0)
        scrollX = -contentsX();
    else if (contentsWidth() - visibleWidth() - contentsX() < scrollX)
        scrollX = contentsWidth() - visibleWidth() - contentsX();

    if (contentsY() + scrollY < 0)
        scrollY = -contentsY();
    else if (contentsHeight() - visibleHeight() - contentsY() < scrollY)
        scrollY = contentsHeight() - visibleHeight() - contentsY();

    scrollBy(scrollX, scrollY);

    // generate abs(scroll.)
    if (scrollX < 0)
        scrollX = -scrollX;
    if (scrollY < 0)
        scrollY = -scrollY;

    return scrollX != maxx && scrollY != maxy;
}

void FrameView::setMediaType(const String& mediaType)
{
    d->m_mediaType = mediaType;
}

String FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType = m_frame->loader()->client()->overrideMediaType();
    if (!overrideType.isNull())
        return overrideType;
    return d->m_mediaType;
}

bool FrameView::useSlowRepaints() const
{
    return d->useSlowRepaints || d->m_slowRepaintObjectCount > 0;
}

void FrameView::setUseSlowRepaints()
{
    d->useSlowRepaints = true;
    setStaticBackground(true);
}

void FrameView::addSlowRepaintObject()
{
    if (!d->m_slowRepaintObjectCount)
        setStaticBackground(true);
    d->m_slowRepaintObjectCount++;
}

void FrameView::removeSlowRepaintObject()
{
    ASSERT(d->m_slowRepaintObjectCount > 0);
    d->m_slowRepaintObjectCount--;
    if (!d->m_slowRepaintObjectCount)
        setStaticBackground(d->useSlowRepaints);
}

void FrameView::setScrollbarsMode(ScrollbarMode mode)
{
    d->vmode = mode;
    d->hmode = mode;
    
    ScrollView::setScrollbarsMode(mode);
}

void FrameView::setVScrollbarMode(ScrollbarMode mode)
{
    d->vmode = mode;
    ScrollView::setVScrollbarMode(mode);
}

void FrameView::setHScrollbarMode(ScrollbarMode mode)
{
    d->hmode = mode;
    ScrollView::setHScrollbarMode(mode);
}

void FrameView::restoreScrollbar()
{
    suppressScrollbars(false);
}

void FrameView::scrollRectIntoViewRecursively(const IntRect& r)
{
    if (frame()->prohibitsScrolling())
        return;
    bool wasInProgrammaticScroll = d->m_inProgrammaticScroll;
    d->m_inProgrammaticScroll = true;
    ScrollView::scrollRectIntoViewRecursively(r);
    d->m_inProgrammaticScroll = wasInProgrammaticScroll;
}

void FrameView::setContentsPos(int x, int y)
{
    if (frame()->prohibitsScrolling())
        return;
    bool wasInProgrammaticScroll = d->m_inProgrammaticScroll;
    d->m_inProgrammaticScroll = true;
    ScrollView::setContentsPos(x, y);
    d->m_inProgrammaticScroll = wasInProgrammaticScroll;
}

void FrameView::repaintRectangle(const IntRect& r, bool immediate)
{
    updateContents(r, immediate);
}

void FrameView::layoutTimerFired(Timer<FrameView>*)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (m_frame->document() && !m_frame->document()->ownerElement())
        printf("Layout timer fired at %d\n", m_frame->document()->elapsedTime());
#endif
    layout();
}

void FrameView::scheduleRelayout()
{
    ASSERT(!m_frame->document() || !m_frame->document()->inPageCache());
    ASSERT(m_frame->view() == this);

    if (d->layoutRoot) {
        d->layoutRoot->markContainingBlocksForLayout(false);
        d->layoutRoot = 0;
    }
    if (!d->layoutSchedulingEnabled)
        return;

    if (!m_frame->document() || !m_frame->document()->shouldScheduleLayout())
        return;

    int delay = m_frame->document()->minimumLayoutDelay();
    if (d->layoutTimer.isActive() && d->delayedLayout && !delay)
        unscheduleRelayout();
    if (d->layoutTimer.isActive())
        return;

    d->delayedLayout = delay != 0;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Scheduling layout for %d\n", delay);
#endif

    d->layoutTimer.startOneShot(delay * 0.001);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderObject* o)
{
    ASSERT(m_frame->view() == this);

    if (!d->layoutSchedulingEnabled || (m_frame->document()
            && m_frame->document()->renderer()
            && m_frame->document()->renderer()->needsLayout())) {
        if (o)
            o->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending()) {
        if (d->layoutRoot != o) {
            if (isObjectAncestorContainerOf(d->layoutRoot, o)) {
                // Keep the current root
                o->markContainingBlocksForLayout(false, d->layoutRoot);
            } else if (d->layoutRoot && isObjectAncestorContainerOf(o, d->layoutRoot)) {
                // Re-root at o
                d->layoutRoot->markContainingBlocksForLayout(false, o);
                d->layoutRoot = o;
            } else {
                // Just do a full relayout
                if (d->layoutRoot)
                    d->layoutRoot->markContainingBlocksForLayout(false);
                d->layoutRoot = 0;
                o->markContainingBlocksForLayout(false);
            }
        }
    } else {
        int delay = m_frame->document()->minimumLayoutDelay();
        d->layoutRoot = o;
        d->delayedLayout = delay != 0;
        d->layoutTimer.startOneShot(delay * 0.001);
    }
}

bool FrameView::layoutPending() const
{
    return d->layoutTimer.isActive();
}

bool FrameView::needsLayout() const
{
    // It is possible that our document will not have a body yet. If this is the case, 
    // then we are not allowed to schedule layouts yet, so we won't be pending layout.
    if (!m_frame)
        return false;
    RenderView* root = static_cast<RenderView*>(m_frame->renderer());
    Document * doc = m_frame->document();
    // doc->hasChangedChild() condition can occur when using WebKit ObjC interface
    return layoutPending() || (root && root->needsLayout()) || d->layoutRoot || (doc && doc->hasChangedChild()) || m_frame->needsReapplyStyles();
}

void FrameView::setNeedsLayout()
{
    if (m_frame->renderer())
        m_frame->renderer()->setNeedsLayout(true);
}

void FrameView::unscheduleRelayout()
{
    if (!d->layoutTimer.isActive())
        return;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (m_frame->document() && !m_frame->document()->ownerElement())
        printf("Layout timer unscheduled at %d\n", m_frame->document()->elapsedTime());
#endif
    
    d->layoutTimer.stop();
    d->delayedLayout = false;
}

bool FrameView::isTransparent() const
{
    return d->isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    d->isTransparent = isTransparent;
}

Color FrameView::baseBackgroundColor() const
{
    return d->baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(Color bc)
{
    if (!bc.isValid())
        bc = Color::white;
    d->baseBackgroundColor = bc;
}

void FrameView::scheduleEvent(PassRefPtr<Event> event, PassRefPtr<EventTargetNode> eventTarget, bool tempEvent)
{
    if (!d->m_enqueueEvents) {
        ExceptionCode ec = 0;
        eventTarget->dispatchEvent(event, ec, tempEvent);
        return;
    }

    ScheduledEvent* scheduledEvent = new ScheduledEvent;
    scheduledEvent->m_event = event;
    scheduledEvent->m_eventTarget = eventTarget;
    scheduledEvent->m_tempEvent = tempEvent;
    d->m_scheduledEvents.append(scheduledEvent);
}

void FrameView::pauseScheduledEvents()
{
    ASSERT(d->m_scheduledEvents.isEmpty() || d->m_enqueueEvents);
    d->m_enqueueEvents++;
}

void FrameView::resumeScheduledEvents()
{
    d->m_enqueueEvents--;
    if (!d->m_enqueueEvents)
        dispatchScheduledEvents();
    ASSERT(d->m_scheduledEvents.isEmpty() || d->m_enqueueEvents);
}

void FrameView::performPostLayoutTasks()
{
    RenderView* root = static_cast<RenderView*>(m_frame->document()->renderer());

    root->updateWidgetPositions();
    if (m_widgetUpdateSet && d->nestedLayoutCount <= 1) {
        Vector<RenderPartObject*> objectVector;
        copyToVector(*m_widgetUpdateSet, objectVector);
        size_t size = objectVector.size();
        for (size_t i = 0; i < size; ++i) {
            RenderPartObject* object = objectVector[i];
            object->updateWidget(false);

            // updateWidget() can destroy the RenderPartObject, so we need to make sure it's
            // alive by checking if it's still in m_widgetUpdateSet.
            if (m_widgetUpdateSet->contains(object))
                object->updateWidgetPosition();
        }
        m_widgetUpdateSet->clear();
    }

    resumeScheduledEvents();

    if (!root->printing()) {
        IntSize currentSize = IntSize(width(), height());
        bool resized = !d->firstLayout && currentSize != d->lastLayoutSize;
        d->lastLayoutSize = currentSize;
        if (resized)
            m_frame->sendResizeEvent();
    }
}

void FrameView::postLayoutTimerFired(Timer<FrameView>*)
{
    performPostLayoutTasks();
}

void FrameView::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (!d->m_viewportRenderer)
        return;
    
    if (d->m_overflowStatusDirty) {
        d->horizontalOverflow = horizontalOverflow;
        d->m_verticalOverflow = verticalOverflow;
        d->m_overflowStatusDirty = false;
        return;
    }
    
    bool horizontalOverflowChanged = (d->horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (d->m_verticalOverflow != verticalOverflow);
    
    if (horizontalOverflowChanged || verticalOverflowChanged) {
        d->horizontalOverflow = horizontalOverflow;
        d->m_verticalOverflow = verticalOverflow;
        
        scheduleEvent(new OverflowEvent(horizontalOverflowChanged, horizontalOverflow,
            verticalOverflowChanged, verticalOverflow),
            EventTargetNodeCast(d->m_viewportRenderer->element()), true);
    }
    
}

void FrameView::dispatchScheduledEvents()
{
    if (d->m_scheduledEvents.isEmpty())
        return;

    Vector<ScheduledEvent*> scheduledEventsCopy = d->m_scheduledEvents;
    d->m_scheduledEvents.clear();
    
    Vector<ScheduledEvent*>::iterator end = scheduledEventsCopy.end();
    for (Vector<ScheduledEvent*>::iterator it = scheduledEventsCopy.begin(); it != end; ++it) {
        ScheduledEvent* scheduledEvent = *it;
        
        ExceptionCode ec = 0;
        
        // Only dispatch events to nodes that are in the document
        if (scheduledEvent->m_eventTarget->inDocument())
            scheduledEvent->m_eventTarget->dispatchEvent(scheduledEvent->m_event,
                ec, scheduledEvent->m_tempEvent);
        
        delete scheduledEvent;
    }
}

IntRect FrameView::windowClipRect() const
{
    return windowClipRect(true);
}

IntRect FrameView::windowClipRect(bool clipToContents) const
{
    ASSERT(m_frame->view() == this);

    // Set our clip rect to be our contents.
    IntRect clipRect;
    if (clipToContents)
        clipRect = enclosingIntRect(visibleContentRect());
    else
        clipRect = IntRect(contentsX(), contentsY(), width(), height());
    clipRect = contentsToWindow(clipRect);

    if (!m_frame || !m_frame->document() || !m_frame->document()->ownerElement())
        return clipRect;

    // Take our owner element and get the clip rect from the enclosing layer.
    Element* elt = m_frame->document()->ownerElement();
    RenderLayer* layer = elt->renderer()->enclosingLayer();
    // FIXME: layer should never be null, but sometimes seems to be anyway.
    if (!layer)
        return clipRect;
    FrameView* parentView = elt->document()->view();
    clipRect.intersect(parentView->windowClipRectForLayer(layer, true));
    return clipRect;
}

IntRect FrameView::windowClipRectForLayer(const RenderLayer* layer, bool clipToLayerContents) const
{
    // If we have no layer, just return our window clip rect.
    if (!layer)
        return windowClipRect();

    // Apply the clip from the layer.
    IntRect clipRect;
    if (clipToLayerContents)
        clipRect = layer->childrenClipRect();
    else
        clipRect = layer->selfClipRect();
    clipRect = contentsToWindow(clipRect); 
    return intersection(clipRect, windowClipRect());
}

void FrameView::updateDashboardRegions()
{
    Document* doc = m_frame->document();
    if (doc->hasDashboardRegions()) {
        Vector<DashboardRegionValue> newRegions;
        doc->renderer()->collectDashboardRegions(newRegions);
        doc->setDashboardRegions(newRegions);
        m_frame.get()->dashboardRegionsChanged();
    }
}

void FrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.
    
    // Optimize the common case where we bring a window to the front while it's still empty.
    if (!m_frame || m_frame->loader()->url().isEmpty()) 
        return;
    
    if (theme()->supportsControlTints() && m_frame->renderer()) {
        if (needsLayout())
            layout();
        PlatformGraphicsContext* const noContext = 0;
        GraphicsContext context(noContext);
        context.setUpdatingControlTints(true);
#if !PLATFORM(MAC)
        ScrollView::paint(&context, frameGeometry());
#else
        m_frame->paint(&context, enclosingIntRect(visibleContentRect()));
#endif
    }
}

bool FrameView::wasScrolledByUser() const
{
    return d->m_wasScrolledByUser;
}

void FrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    if (d->m_inProgrammaticScroll)
        return;
    d->m_wasScrolledByUser = wasScrolledByUser;
}

#if PLATFORM(WIN) || PLATFORM(GTK) || PLATFORM(QT)
void FrameView::layoutIfNeededRecursive()
{
    // We have to crawl our entire tree looking for any FrameViews that need
    // layout and make sure they are up to date.
    // Mac actually tests for intersection with the dirty region and tries not to
    // update layout for frames that are outside the dirty region.  Not only does this seem
    // pointless (since those frames will have set a zero timer to layout anyway), but
    // it is also incorrect, since if two frames overlap, the first could be excluded from the dirty
    // region but then become included later by the second frame adding rects to the dirty region
    // when it lays out.

    if (needsLayout())
        layout();

    HashSet<Widget*>* viewChildren = children();
    HashSet<Widget*>::iterator end = viewChildren->end();
    for (HashSet<Widget*>::iterator current = viewChildren->begin(); current != end; ++current)
        if ((*current)->isFrameView())
            static_cast<FrameView*>(*current)->layoutIfNeededRecursive();
}
#endif

}
