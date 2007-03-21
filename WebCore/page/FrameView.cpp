/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "FrameView.h"

#include "AXObjectCache.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLDocument.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "OverflowEvent.h"
#include "RenderPart.h"
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
        : layoutTimer(view, &FrameView::layoutTimerFired)
        , m_mediaType("screen")
        , m_enqueueEvents(0)
        , m_overflowStatusDirty(true)
        , m_viewportRenderer(0)
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
        slowRepaintObjectCount = 0;
        borderX = 30;
        borderY = 30;
        layoutTimer.stop();
        layoutRoot = 0;
        delayedLayout = false;
        doFullRepaint = true;
        layoutSchedulingEnabled = true;
        layoutCount = 0;
        firstLayout = true;
        repaintRects.clear();
    }

    bool doFullRepaint;
    
    ScrollbarMode vmode;
    ScrollbarMode hmode;
    bool useSlowRepaints;
    unsigned slowRepaintObjectCount;

    int borderX, borderY;

    Timer<FrameView> layoutTimer;
    bool delayedLayout;
    RefPtr<Node> layoutRoot;
    
    bool layoutSchedulingEnabled;
    int layoutCount;

    bool firstLayout;
    bool needToInitScrollbars;
    bool isTransparent;
    Color baseBackgroundColor;

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
};

FrameView::FrameView(Frame* frame)
    : m_refCount(1)
    , m_frame(frame)
    , d(new FrameViewPrivate(this))
{
    init();
    show();
}

FrameView::~FrameView()
{
    resetScrollbars();

    ASSERT(m_refCount == 0);

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

void FrameView::clearPart()
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

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (d->layoutTimer.isActive() && m_frame->document() && !m_frame->document()->ownerElement())
        printf("Killing the layout timer from a clear at %d\n", m_frame->document()->elapsedTime());
#endif    
    d->layoutTimer.stop();

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
    if (Document* document = m_frame->document()) {
        RenderView* root = static_cast<RenderView*>(document->renderer());
        if (!root)
            return;
        resizeContents(root->overflowWidth(), root->overflowHeight());
    }
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

Node* FrameView::layoutRoot() const
{
    return layoutPending() ? 0 : d->layoutRoot.get();
}

void FrameView::layout(bool allowSubtree)
{
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

    if (!allowSubtree && d->layoutRoot) {
        if (d->layoutRoot->renderer())
            d->layoutRoot->renderer()->markContainingBlocksForLayout(false);
        d->layoutRoot = 0;
    }

    bool subtree = d->layoutRoot;

    ASSERT(m_frame->view() == this);

    Document* document = m_frame->document();
    if (!document) {
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(visibleWidth());
        return;
    }

    Node* rootNode = subtree ? d->layoutRoot.get() : document;
    d->layoutSchedulingEnabled = false;

    // Always ensure our style info is up-to-date.  This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    if (document->hasChangedChild())
        document->recalcStyle();
    
    // If there is only one ref to this view left, then its going to be destroyed as soon as we exit, 
    // so there's no point to continuiing to layout
    if (protector->hasOneRef())
        return;

    RenderObject* root = rootNode->renderer();
    if (!root) {
        // FIXME: Do we need to set m_size here?
        d->layoutSchedulingEnabled = true;
        return;
    }

    ScrollbarMode hMode = d->hmode;
    ScrollbarMode vMode = d->vmode;
    
    if (!subtree) {
        Document* document = static_cast<Document*>(rootNode);
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
    d->repaintRects.clear();

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
     
    if (!d->doFullRepaint) {
        layer->checkForRepaintOnResize();
        root->repaintObjectsBeforeLayout();
    }

    if (subtree) {
        if (root->recalcMinMax())
            root->recalcMinMaxWidths();
    }
    d->m_enqueueEvents++;
    root->layout();
    d->layoutRoot = 0;

    m_frame->invalidateSelection();
   
    d->layoutSchedulingEnabled=true;

    if (!subtree && !static_cast<RenderView*>(root)->printing())
        adjustViewSize();

    // Now update the positions of all layers.
    layer->updateLayerPositions(d->doFullRepaint);

    // We update our widget positions right after doing a layout.
    if (!subtree)
        static_cast<RenderView*>(root)->updateWidgetPositions();
    
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
    updateDashboardRegions();
#endif

    if (didFirstLayout)
        m_frame->loader()->didFirstLayout();
    
    if (root->needsLayout()) {
        scheduleRelayout();
        return;
    }
    setStaticBackground(useSlowRepaints());

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(visibleWidth() < contentsWidth(),
                             visibleHeight() < contentsHeight());

    // Dispatch events scheduled during layout
    dispatchScheduledEvents();
    d->m_enqueueEvents--;
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
    return d->useSlowRepaints || d->slowRepaintObjectCount > 0;
}

void FrameView::setUseSlowRepaints()
{
    d->useSlowRepaints = true;
    setStaticBackground(true);
}

void FrameView::addSlowRepaintObject()
{
    if (d->slowRepaintObjectCount == 0)
        setStaticBackground(true);
    d->slowRepaintObjectCount++;
}

void FrameView::removeSlowRepaintObject()
{
    d->slowRepaintObjectCount--;
    if (d->slowRepaintObjectCount == 0)
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

void FrameView::scrollPointRecursively(int x, int y)
{
    if (frame()->prohibitsScrolling())
        return;
    ScrollView::scrollPointRecursively(x, y);
}

void FrameView::setContentsPos(int x, int y)
{
    if (frame()->prohibitsScrolling())
        return;
    ScrollView::setContentsPos(x, y);
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
    ASSERT(m_frame->view() == this);

    if (d->layoutRoot) {
        if (d->layoutRoot->renderer())
            d->layoutRoot->renderer()->markContainingBlocksForLayout(false);
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

void FrameView::scheduleRelayoutOfSubtree(Node* n)
{
    ASSERT(m_frame->view() == this);

    if (!d->layoutSchedulingEnabled || (m_frame->document()
            && m_frame->document()->renderer()
            && m_frame->document()->renderer()->needsLayout())) {
        if (n->renderer())
            n->renderer()->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending()) {
        if (d->layoutRoot != n) {
            // Just do a full relayout
            if (d->layoutRoot && d->layoutRoot->renderer())
                d->layoutRoot->renderer()->markContainingBlocksForLayout(false);
            d->layoutRoot = 0;
            if (n->renderer())
                n->renderer()->markContainingBlocksForLayout(false);
        }
    } else {
        int delay = m_frame->document()->minimumLayoutDelay();
        d->layoutRoot = n;
        d->delayedLayout = delay != 0;
        d->layoutTimer.startOneShot(delay * 0.001);
    }
}

bool FrameView::layoutPending() const
{
    return d->layoutTimer.isActive();
}

bool FrameView::haveDelayedLayoutScheduled()
{
    return d->layoutTimer.isActive() && d->delayedLayout;
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
    if (!d->m_scheduledEvents)
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
    clipRect.setLocation(contentsToWindow(clipRect.location()));
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
    // Apply the clip from the layer.
    IntRect clipRect;
    if (clipToLayerContents)
        clipRect = layer->childrenClipRect();
    else
        clipRect = layer->selfClipRect();
    clipRect.setLocation(contentsToWindow(clipRect.location()));
 
    return intersection(clipRect, windowClipRect());
}

bool FrameView::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    return m_frame->eventHandler()->handleMouseMoveEvent(event);
}

bool FrameView::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    return m_frame->eventHandler()->handleMouseReleaseEvent(event);
}

}
