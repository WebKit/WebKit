/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "CSSStyleSelector.h"
#include "ChromeClient.h"
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
#include "Page.h"
#include "RenderPart.h"
#include "RenderPartObject.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"

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
        , m_layoutTimer(view, &FrameView::layoutTimerFired)
        , m_layoutRoot(0)
        , m_postLayoutTasksTimer(view, &FrameView::postLayoutTimerFired)
        , m_mediaType("screen")
        , m_enqueueEvents(0)
        , m_overflowStatusDirty(true)
        , m_viewportRenderer(0)
        , m_wasScrolledByUser(false)
        , m_inProgrammaticScroll(false)
        , m_shouldUpdateWhileOffscreen(true)
    {
        m_isTransparent = false;
        m_baseBackgroundColor = Color::white;
        m_vmode = m_hmode = ScrollbarAuto;
        m_needToInitScrollbars = true;
        reset();
    }
    void reset()
    {
        m_useSlowRepaints = false;
        m_borderX = 30;
        m_borderY = 30;
        m_layoutTimer.stop();
        m_layoutRoot = 0;
        m_delayedLayout = false;
        m_doFullRepaint = true;
        m_layoutSchedulingEnabled = true;
        m_midLayout = false;
        m_layoutCount = 0;
        m_nestedLayoutCount = 0;
        m_postLayoutTasksTimer.stop();
        m_firstLayout = true;
        m_firstLayoutCallbackPending = false;
        m_wasScrolledByUser = false;
        m_lastLayoutSize = IntSize();
        m_lastZoomFactor = 1.0f;
        m_deferringRepaints = 0;
        m_repaintCount = 0;
        m_repaintRect = IntRect();
        m_repaintRects.clear();
    }

    bool m_doFullRepaint;
    
    ScrollbarMode m_vmode;
    ScrollbarMode m_hmode;
    bool m_useSlowRepaints;
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
    bool m_needToInitScrollbars;
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
    IntRect m_repaintRect;
    Vector<IntRect> m_repaintRects;

    bool m_shouldUpdateWhileOffscreen;
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
    if (d->m_postLayoutTasksTimer.isActive()) {
        d->m_postLayoutTasksTimer.stop();
        d->m_scheduledEvents.clear();
        d->m_enqueueEvents = 0;
    }

    resetScrollbars();

    ASSERT(m_refCount == 0);
    ASSERT(d->m_scheduledEvents.isEmpty());
    ASSERT(!d->m_enqueueEvents);

    if (m_frame) {
        ASSERT(m_frame->view() != this || !m_frame->document() || !m_frame->contentRenderer());
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
    d->m_firstLayout = true;
    suppressScrollbars(true);
    ScrollView::setVScrollbarMode(d->m_vmode);
    ScrollView::setHScrollbarMode(d->m_hmode);
    suppressScrollbars(false);
}

void FrameView::init()
{
    m_margins = IntSize(-1, -1); // undefined
    m_size = IntSize();
}

void FrameView::clear()
{
    setCanBlitOnScroll(true);
    
    d->reset();

    if (m_frame)
        if (RenderPart* renderer = m_frame->ownerRenderer())
            renderer->viewCleared();

    suppressScrollbars(true);
}

bool FrameView::didFirstLayout() const
{
    return !d->m_firstLayout;
}

void FrameView::initScrollbars()
{
    if (!d->m_needToInitScrollbars)
        return;
    d->m_needToInitScrollbars = false;
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
    RenderView* root = m_frame->contentRenderer();
    if (!root)
        return;
    setContentsSize(IntSize(root->overflowWidth(), root->overflowHeight()));
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
    return d->m_layoutCount;
}

bool FrameView::needsFullRepaint() const
{
    return d->m_doFullRepaint;
}

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : d->m_layoutRoot;
}

void FrameView::layout(bool allowSubtree)
{
    if (d->m_midLayout)
        return;

    d->m_layoutTimer.stop();
    d->m_delayedLayout = false;

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

    if (!allowSubtree && d->m_layoutRoot) {
        d->m_layoutRoot->markContainingBlocksForLayout(false);
        d->m_layoutRoot = 0;
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

    d->m_layoutSchedulingEnabled = false;

    if (!d->m_nestedLayoutCount && d->m_postLayoutTasksTimer.isActive()) {
        // This is a new top-level layout. If there are any remaining tasks from the previous
        // layout, finish them now.
        d->m_postLayoutTasksTimer.stop();
        performPostLayoutTasks();
    }

    // Viewport-dependent media queries may cause us to need completely different style information.
    // Check that here.
    if (document->styleSelector()->affectedByViewportChange())
        document->updateStyleSelector();

    // Always ensure our style info is up-to-date.  This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    if (m_frame->needsReapplyStyles())
        m_frame->reapplyStyles();
    else if (document->hasChangedChild())
        document->recalcStyle();
    
    bool subtree = d->m_layoutRoot;

    // If there is only one ref to this view left, then its going to be destroyed as soon as we exit, 
    // so there's no point to continuing to layout
    if (protector->hasOneRef())
        return;

    RenderObject* root = subtree ? d->m_layoutRoot : document->renderer();
    if (!root) {
        // FIXME: Do we need to set m_size here?
        d->m_layoutSchedulingEnabled = true;
        return;
    }

    d->m_nestedLayoutCount++;

    ScrollbarMode hMode = d->m_hmode;
    ScrollbarMode vMode = d->m_vmode;

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
                    if (!d->m_firstLayout && m_size.height() != visibleHeight()
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
        if (d->m_firstLayout && !document->ownerElement())
            printf("Elapsed time before first layout: %d\n", document->elapsedTime());
#endif
    }

    d->m_doFullRepaint = !subtree && (d->m_firstLayout || static_cast<RenderView*>(root)->printing());

    if (!subtree) {
        // Now set our scrollbar state for the layout.
        ScrollbarMode currentHMode = hScrollbarMode();
        ScrollbarMode currentVMode = vScrollbarMode();

        if (d->m_firstLayout || (hMode != currentHMode || vMode != currentVMode)) {
            suppressScrollbars(true);
            if (d->m_firstLayout) {
                d->m_firstLayout = false;
                d->m_firstLayoutCallbackPending = true;
                d->m_lastLayoutSize = IntSize(width(), height());
                d->m_lastZoomFactor = root->style()->zoom();

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
            d->m_doFullRepaint = true;
    }
    
    RenderLayer* layer = root->enclosingLayer();

    pauseScheduledEvents();

    if (subtree)
        root->view()->pushLayoutState(root);
        
    d->m_midLayout = true;
    beginDeferredRepaints();
    root->layout();
    endDeferredRepaints();
    d->m_midLayout = false;

    if (subtree)
        root->view()->popLayoutState();
    d->m_layoutRoot = 0;

    m_frame->invalidateSelection();
   
    d->m_layoutSchedulingEnabled = true;

    if (!subtree && !static_cast<RenderView*>(root)->printing())
        adjustViewSize();

    // Now update the positions of all layers.
    beginDeferredRepaints();
    layer->updateLayerPositions(d->m_doFullRepaint);
    endDeferredRepaints();
    
    d->m_layoutCount++;

#if PLATFORM(MAC)
    if (AXObjectCache::accessibilityEnabled())
        root->document()->axObjectCache()->postNotificationToElement(root, "AXLayoutComplete");
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    updateDashboardRegions();
#endif

    ASSERT(!root->needsLayout());

    setCanBlitOnScroll(!useSlowRepaints());

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(visibleWidth() < contentsWidth(),
                             visibleHeight() < contentsHeight());

    if (!d->m_postLayoutTasksTimer.isActive()) {
        // Calls resumeScheduledEvents()
        performPostLayoutTasks();

        if (needsLayout()) {
            // Post-layout widget updates or an event handler made us need layout again.
            // Lay out again, but this time defer widget updates and event dispatch until after
            // we return.
            d->m_postLayoutTasksTimer.startOneShot(0);
            pauseScheduledEvents();
            layout();
        }
    } else {
        resumeScheduledEvents();
        ASSERT(d->m_enqueueEvents);
    }

    d->m_nestedLayoutCount--;
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
    return d->m_useSlowRepaints || d->m_slowRepaintObjectCount > 0;
}

void FrameView::setUseSlowRepaints()
{
    d->m_useSlowRepaints = true;
    setCanBlitOnScroll(false);
}

void FrameView::addSlowRepaintObject()
{
    if (!d->m_slowRepaintObjectCount)
        setCanBlitOnScroll(false);
    d->m_slowRepaintObjectCount++;
}

void FrameView::removeSlowRepaintObject()
{
    ASSERT(d->m_slowRepaintObjectCount > 0);
    d->m_slowRepaintObjectCount--;
    if (!d->m_slowRepaintObjectCount)
        setCanBlitOnScroll(!d->m_useSlowRepaints);
}

void FrameView::setScrollbarsMode(ScrollbarMode mode)
{
    d->m_vmode = mode;
    d->m_hmode = mode;
    
    ScrollView::setScrollbarsMode(mode);
}

void FrameView::setVScrollbarMode(ScrollbarMode mode)
{
    d->m_vmode = mode;
    ScrollView::setVScrollbarMode(mode);
}

void FrameView::setHScrollbarMode(ScrollbarMode mode)
{
    d->m_hmode = mode;
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

void FrameView::setScrollPosition(const IntPoint& scrollPoint)
{
    if (frame()->prohibitsScrolling())
        return;
    bool wasInProgrammaticScroll = d->m_inProgrammaticScroll;
    d->m_inProgrammaticScroll = true;
    ScrollView::setScrollPosition(scrollPoint);
    d->m_inProgrammaticScroll = wasInProgrammaticScroll;
}

const unsigned cRepaintRectUnionThreshold = 25;

void FrameView::repaintContentRectangle(const IntRect& r, bool immediate)
{
    ASSERT(!m_frame->document()->ownerElement());

    if (d->m_deferringRepaints && !immediate) {
        IntRect visibleContent = visibleContentRect();
        visibleContent.intersect(r);
        if (!visibleContent.isEmpty()) {
            d->m_repaintCount++;
            d->m_repaintRect.unite(r);
            if (d->m_repaintCount == cRepaintRectUnionThreshold)
                d->m_repaintRects.clear();
            else if (d->m_repaintCount < cRepaintRectUnionThreshold)
                d->m_repaintRects.append(r);
        }
        return;
    }
    
    updateContents(r, immediate);
}

void FrameView::beginDeferredRepaints()
{
    Page* page = m_frame->page();
    if (page->mainFrame() != m_frame)
        return page->mainFrame()->view()->beginDeferredRepaints();

    d->m_deferringRepaints++;
    d->m_repaintCount = 0;
    d->m_repaintRect = IntRect();
    d->m_repaintRects.clear();
}


void FrameView::endDeferredRepaints()
{
    Page* page = m_frame->page();
    if (page->mainFrame() != m_frame)
        return page->mainFrame()->view()->endDeferredRepaints();

    ASSERT(d->m_deferringRepaints > 0);
    if (--d->m_deferringRepaints == 0) {
        if (d->m_repaintCount >= cRepaintRectUnionThreshold)
            repaintContentRectangle(d->m_repaintRect, false);
        else {
            unsigned size = d->m_repaintRects.size();
            for (unsigned i = 0; i < size; i++)
                repaintContentRectangle(d->m_repaintRects[i], false);
            d->m_repaintRects.clear();
        }
    }
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

    if (d->m_layoutRoot) {
        d->m_layoutRoot->markContainingBlocksForLayout(false);
        d->m_layoutRoot = 0;
    }
    if (!d->m_layoutSchedulingEnabled)
        return;

    if (!m_frame->document() || !m_frame->document()->shouldScheduleLayout())
        return;

    int delay = m_frame->document()->minimumLayoutDelay();
    if (d->m_layoutTimer.isActive() && d->m_delayedLayout && !delay)
        unscheduleRelayout();
    if (d->m_layoutTimer.isActive())
        return;

    d->m_delayedLayout = delay != 0;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Scheduling layout for %d\n", delay);
#endif

    d->m_layoutTimer.startOneShot(delay * 0.001);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderObject* relayoutRoot)
{
    ASSERT(m_frame->view() == this);

    if (!d->m_layoutSchedulingEnabled || (m_frame->contentRenderer()
            && m_frame->contentRenderer()->needsLayout())) {
        if (relayoutRoot)
            relayoutRoot->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending()) {
        if (d->m_layoutRoot != relayoutRoot) {
            if (isObjectAncestorContainerOf(d->m_layoutRoot, relayoutRoot)) {
                // Keep the current root
                relayoutRoot->markContainingBlocksForLayout(false, d->m_layoutRoot);
            } else if (d->m_layoutRoot && isObjectAncestorContainerOf(relayoutRoot, d->m_layoutRoot)) {
                // Re-root at relayoutRoot
                d->m_layoutRoot->markContainingBlocksForLayout(false, relayoutRoot);
                d->m_layoutRoot = relayoutRoot;
            } else {
                // Just do a full relayout
                if (d->m_layoutRoot)
                    d->m_layoutRoot->markContainingBlocksForLayout(false);
                d->m_layoutRoot = 0;
                relayoutRoot->markContainingBlocksForLayout(false);
            }
        }
    } else {
        int delay = m_frame->document()->minimumLayoutDelay();
        d->m_layoutRoot = relayoutRoot;
        d->m_delayedLayout = delay != 0;
        d->m_layoutTimer.startOneShot(delay * 0.001);
    }
}

bool FrameView::layoutPending() const
{
    return d->m_layoutTimer.isActive();
}

bool FrameView::needsLayout() const
{
    // It is possible that our document will not have a body yet. If this is the case, 
    // then we are not allowed to schedule layouts yet, so we won't be pending layout.
    if (!m_frame)
        return false;
    RenderView* root = m_frame->contentRenderer();
    Document * doc = m_frame->document();
    // doc->hasChangedChild() condition can occur when using WebKit ObjC interface
    return layoutPending() || (root && root->needsLayout()) || d->m_layoutRoot || (doc && doc->hasChangedChild()) || m_frame->needsReapplyStyles();
}

void FrameView::setNeedsLayout()
{
    RenderView* root = m_frame->contentRenderer();
    if (root)
        root->setNeedsLayout(true);
}

void FrameView::unscheduleRelayout()
{
    if (!d->m_layoutTimer.isActive())
        return;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (m_frame->document() && !m_frame->document()->ownerElement())
        printf("Layout timer unscheduled at %d\n", m_frame->document()->elapsedTime());
#endif
    
    d->m_layoutTimer.stop();
    d->m_delayedLayout = false;
}

bool FrameView::isTransparent() const
{
    return d->m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    d->m_isTransparent = isTransparent;
}

Color FrameView::baseBackgroundColor() const
{
    return d->m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(Color bc)
{
    if (!bc.isValid())
        bc = Color::white;
    d->m_baseBackgroundColor = bc;
}

bool FrameView::shouldUpdateWhileOffscreen() const
{
    return d->m_shouldUpdateWhileOffscreen;
}

void FrameView::setShouldUpdateWhileOffscreen(bool shouldUpdateWhileOffscreen)
{
    d->m_shouldUpdateWhileOffscreen = shouldUpdateWhileOffscreen;
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
    if (d->m_firstLayoutCallbackPending) {
        d->m_firstLayoutCallbackPending = false;
        m_frame->loader()->didFirstLayout();
    }
    
    RenderView* root = m_frame->contentRenderer();

    root->updateWidgetPositions();
    if (m_widgetUpdateSet && d->m_nestedLayoutCount <= 1) {
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
        float currentZoomFactor = root->style()->zoom();
        bool resized = !d->m_firstLayout && (currentSize != d->m_lastLayoutSize || currentZoomFactor != d->m_lastZoomFactor);
        d->m_lastLayoutSize = currentSize;
        d->m_lastZoomFactor = currentZoomFactor;
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
        d->m_horizontalOverflow = horizontalOverflow;
        d->m_verticalOverflow = verticalOverflow;
        d->m_overflowStatusDirty = false;
        return;
    }
    
    bool horizontalOverflowChanged = (d->m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (d->m_verticalOverflow != verticalOverflow);
    
    if (horizontalOverflowChanged || verticalOverflowChanged) {
        d->m_horizontalOverflow = horizontalOverflow;
        d->m_verticalOverflow = verticalOverflow;
        
        scheduleEvent(OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow,
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
    IntRect clipRect = contentsToWindow(visibleContentRect(!clipToContents));
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

#if ENABLE(DASHBOARD_SUPPORT)
void FrameView::updateDashboardRegions()
{
    Document* document = m_frame->document();
    if (!document->hasDashboardRegions())
        return;
    Vector<DashboardRegionValue> newRegions;
    document->renderer()->collectDashboardRegions(newRegions);
    if (newRegions == document->dashboardRegions())
        return;
    document->setDashboardRegions(newRegions);
    Page* page = m_frame->page();
    if (!page)
        return;
    page->chrome()->client()->dashboardRegionsChanged();
}
#endif

void FrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.
    
    // Optimize the common case where we bring a window to the front while it's still empty.
    if (!m_frame || m_frame->loader()->url().isEmpty()) 
        return;
    
    if (theme()->supportsControlTints() && m_frame->contentRenderer()) {
        if (needsLayout())
            layout();
        PlatformGraphicsContext* const noContext = 0;
        GraphicsContext context(noContext);
        context.setUpdatingControlTints(true);
#if !PLATFORM(MAC)
        ScrollView::paint(&context, frameGeometry());
#else
        m_frame->paint(&context, visibleContentRect());
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

    const HashSet<Widget*>* viewChildren = children();
    HashSet<Widget*>::const_iterator end = viewChildren->end();
    for (HashSet<Widget*>::const_iterator current = viewChildren->begin(); current != end; ++current)
        if ((*current)->isFrameView())
            static_cast<FrameView*>(*current)->layoutIfNeededRecursive();
}
#endif

}
