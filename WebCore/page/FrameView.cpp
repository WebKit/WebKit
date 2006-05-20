/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "CachedImage.h"
#include "Cursor.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLFrameSetElement.h"
#include "HTMLInputElement.h"
#include "Image.h"
#include "AccessibilityObjectCache.h"
#include "PlatformKeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "RenderPart.h"
#include "RenderText.h"
#include "SelectionController.h"
#include "PlatformWheelEvent.h"
#include "cssstyleselector.h"
#include "dom2_eventsimpl.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "RenderArena.h"
#include "RenderCanvas.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

class FrameViewPrivate {
public:
    FrameViewPrivate(FrameView* view)
        : m_hasBorder(false)
        , layoutTimer(view, &FrameView::layoutTimerFired)
        , hoverTimer(view, &FrameView::hoverTimerFired)
        , m_mediaType("screen")
    {
        repaintRects = 0;
        isTransparent = false;
        vmode = hmode = ScrollBarAuto;
        needToInitScrollBars = true;
        reset();
    }
    ~FrameViewPrivate()
    {
        delete repaintRects;
    }
    void reset()
    {
        underMouse = 0;
        oldUnder = 0;
        oldSubframe = 0;
        linkPressed = false;
        useSlowRepaints = false;
        dragTarget = 0;
        borderTouched = false;
        scrollBarMoved = false;
        ignoreWheelEvents = false;
        borderX = 30;
        borderY = 30;
        clickCount = 0;
        clickNode = 0;
        scrollingSelf = false;
        layoutTimer.stop();
        delayedLayout = false;
        mousePressed = false;
        doFullRepaint = true;
        layoutSchedulingEnabled = true;
        layoutSuppressed = false;
        layoutCount = 0;
        firstLayout = true;
        hoverTimer.stop();
        if (repaintRects)
            repaintRects->clear();
        resizingFrameSet = 0;
    }

    RefPtr<Node> underMouse;
    RefPtr<Node> oldUnder;
    RefPtr<Frame> oldSubframe;

    bool borderTouched : 1;
    bool borderStart : 1;
    bool scrollBarMoved : 1;
    bool doFullRepaint : 1;
    bool m_hasBorder : 1;
    
    ScrollBarMode vmode;
    ScrollBarMode hmode;
    bool linkPressed;
    bool useSlowRepaints;
    bool ignoreWheelEvents;

    int borderX, borderY;
    int clickCount;
    RefPtr<Node> clickNode;

    bool scrollingSelf;
    Timer<FrameView> layoutTimer;
    bool delayedLayout;
    
    bool layoutSchedulingEnabled;
    bool layoutSuppressed;
    int layoutCount;

    bool firstLayout;
    bool needToInitScrollBars;
    bool mousePressed;
    bool isTransparent;
    
    Timer<FrameView> hoverTimer;
    
    // Used by objects during layout to communicate repaints that need to take place only
    // after all layout has been completed.
    DeprecatedPtrList<RenderObject::RepaintInfo>* repaintRects;
    
    RefPtr<Node> dragTarget;
    RefPtr<HTMLFrameSetElement> resizingFrameSet;
    
    String m_mediaType;
};

FrameView::FrameView(Frame *frame)
    : m_refCount(1)
    , m_frame(frame)
    , d(new FrameViewPrivate(this))
{
    init();

    show();
}

FrameView::~FrameView()
{
    resetScrollBars();

    ASSERT(m_refCount == 0);

    if (d->hoverTimer.isActive())
        d->hoverTimer.stop();
    if (m_frame) {
        // FIXME: Is this really the right place to call detach on the document?
        if (Document* doc = m_frame->document())
            doc->detach();
        if (RenderPart* renderer = m_frame->ownerRenderer())
            renderer->setWidget(0);
    }

    delete d;
    d = 0;
}

void FrameView::clearPart()
{
    m_frame = 0;
}

void FrameView::resetScrollBars()
{
    // Reset the document's scrollbars back to our defaults before we yield the floor.
    d->firstLayout = true;
    suppressScrollBars(true);
    ScrollView::setVScrollBarMode(d->vmode);
    ScrollView::setHScrollBarMode(d->hmode);
    suppressScrollBars(false);
}

void FrameView::init()
{
    m_margins = IntSize(-1, -1); // undefined
    m_size = IntSize();
}

void FrameView::clear()
{
    setStaticBackground(false);
    
    // FIXME: 6498 Should just be able to call m_frame->selection().clear()
    m_frame->setSelection(SelectionController());

    d->reset();

#if INSTRUMENT_LAYOUT_SCHEDULING
    if (d->layoutTimer.isActive() && m_frame->document() && !m_frame->document()->ownerElement())
        printf("Killing the layout timer from a clear at %d\n", m_frame->document()->elapsedTime());
#endif    
    d->layoutTimer.stop();

    cleared();

    suppressScrollBars(true);
}

void FrameView::initScrollBars()
{
    if (!d->needToInitScrollBars)
        return;
    d->needToInitScrollBars = false;
    setScrollBarsMode(hScrollBarMode());
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
    if (m_frame->document()) {
        Document *document = m_frame->document();

        RenderCanvas* root = static_cast<RenderCanvas *>(document->renderer());
        if (!root)
            return;
        
        int docw = root->docWidth();
        int doch = root->docHeight();
    
        resizeContents(docw, doch);
    }
}

void FrameView::applyOverflowToViewport(RenderObject* o, ScrollBarMode& hMode, ScrollBarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.
    switch(o->style()->overflow()) {
        case OHIDDEN:
            hMode = vMode = ScrollBarAlwaysOff;
            break;
        case OSCROLL:
            hMode = vMode = ScrollBarAlwaysOn;
            break;
        case OAUTO:
            hMode = vMode = ScrollBarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }
}

bool FrameView::inLayout() const
{
    return d->layoutSuppressed;
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
    if (!d->repaintRects) {
        d->repaintRects = new DeprecatedPtrList<RenderObject::RepaintInfo>;
        d->repaintRects->setAutoDelete(true);
    }
    
    d->repaintRects->append(new RenderObject::RepaintInfo(o, r));
}

void FrameView::layout()
{
    if (d->layoutSuppressed)
        return;
    
    d->layoutTimer.stop();
    d->delayedLayout = false;

    if (!m_frame) {
        // FIXME: Do we need to set m_size.width here?
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(visibleWidth());
        return;
    }

    Document* document = m_frame->document();
    if (!document) {
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(visibleWidth());
        return;
    }

    d->layoutSchedulingEnabled = false;

    // Always ensure our style info is up-to-date.  This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    if (document->hasChangedChild())
        document->recalcStyle();

    RenderCanvas* root = static_cast<RenderCanvas*>(document->renderer());
    if (!root) {
        // FIXME: Do we need to set m_size here?
        d->layoutSchedulingEnabled = true;
        return;
    }

    ScrollBarMode hMode = d->hmode;
    ScrollBarMode vMode = d->vmode;
    
    RenderObject* rootRenderer = document->documentElement() ? document->documentElement()->renderer() : 0;
    if (document->isHTMLDocument()) {
        Node *body = static_cast<HTMLDocument*>(document)->body();
        if (body && body->renderer()) {
            if (body->hasTagName(framesetTag)) {
                body->renderer()->setNeedsLayout(true);
                vMode = ScrollBarAlwaysOff;
                hMode = ScrollBarAlwaysOff;
            }
            else if (body->hasTagName(bodyTag)) {
                RenderObject* o = (rootRenderer->style()->overflow() == OVISIBLE) ? body->renderer() : rootRenderer;
                applyOverflowToViewport(o, hMode, vMode); // Only applies to HTML UAs, not to XML/XHTML UAs
            }
        }
    }
    else if (rootRenderer)
        applyOverflowToViewport(rootRenderer, hMode, vMode); // XML/XHTML UAs use the root element.

#if INSTRUMENT_LAYOUT_SCHEDULING
    if (d->firstLayout && !document->ownerElement())
        printf("Elapsed time before first layout: %d\n", document->elapsedTime());
#endif

    d->doFullRepaint = d->firstLayout || root->printingMode();
    if (d->repaintRects)
        d->repaintRects->clear();

    // Now set our scrollbar state for the layout.
    ScrollBarMode currentHMode = hScrollBarMode();
    ScrollBarMode currentVMode = vScrollBarMode();

    bool didFirstLayout = false;
    if (d->firstLayout || (hMode != currentHMode || vMode != currentVMode)) {
        suppressScrollBars(true);
        if (d->firstLayout) {
            d->firstLayout = false;
            didFirstLayout = true;
            
            // Set the initial vMode to AlwaysOn if we're auto.
            if (vMode == ScrollBarAuto)
                ScrollView::setVScrollBarMode(ScrollBarAlwaysOn); // This causes a vertical scrollbar to appear.
            // Set the initial hMode to AlwaysOff if we're auto.
            if (hMode == ScrollBarAuto)
                ScrollView::setHScrollBarMode(ScrollBarAlwaysOff); // This causes a horizontal scrollbar to disappear.
        }
        
        if (hMode == vMode)
            ScrollView::setScrollBarsMode(hMode);
        else {
            ScrollView::setHScrollBarMode(hMode);
            ScrollView::setVScrollBarMode(vMode);
        }

        suppressScrollBars(false, true);
    }

    IntSize oldSize = m_size;

    m_size = IntSize(visibleWidth(), visibleHeight());

    if (oldSize != m_size)
        d->doFullRepaint = true;
    
    RenderLayer* layer = root->layer();
     
    if (!d->doFullRepaint) {
        layer->computeRepaintRects();
        root->repaintObjectsBeforeLayout();
    }

    root->layout();

    m_frame->invalidateSelection();
   
    d->layoutSchedulingEnabled=true;
    d->layoutSuppressed = false;

    if (!root->printingMode())
        resizeContents(layer->width(), layer->height());

    // Now update the positions of all layers.
    layer->updateLayerPositions(d->doFullRepaint);

    // We update our widget positions right after doing a layout.
    root->updateWidgetPositions();
    
    if (d->repaintRects && !d->repaintRects->isEmpty()) {
        // FIXME: Could optimize this and have objects removed from this list
        // if they ever do full repaints.
        RenderObject::RepaintInfo* r;
        DeprecatedPtrListIterator<RenderObject::RepaintInfo> it(*d->repaintRects);
        for (; (r = it.current()); ++it)
            r->m_object->repaintRectangle(r->m_repaintRect);
        d->repaintRects->clear();
    }
    
    d->layoutCount++;

#if __APPLE__
    if (AccessibilityObjectCache::accessibilityEnabled())
        root->document()->getAccObjectCache()->postNotification(root, "AXLayoutComplete");
    updateDashboardRegions();
#endif

    if (root->needsLayout()) {
        scheduleRelayout();
        return;
    }
    setStaticBackground(d->useSlowRepaints);

    if (didFirstLayout)
        m_frame->didFirstLayout();
}

//
// Event Handling
//
/////////////////

static Frame* subframeForEvent(const MouseEventWithHitTestResults& mev)
{
    if (!mev.targetNode())
        return 0;

    RenderObject* renderer = mev.targetNode()->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;

    Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
    if (!widget || !widget->isFrameView())
        return 0;

    return static_cast<FrameView*>(widget)->frame();
}

void FrameView::handleMousePressEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return;

    RefPtr<FrameView> protector(this);

    d->mousePressed = true;

    MouseEventWithHitTestResults mev = prepareMouseEvent(false, true, false, mouseEvent);

    if (m_frame->passSubframeEventToSubframe(mev)) {
        invalidateClick();
        return;
    }

    d->clickCount = mouseEvent.clickCount();
    d->clickNode = mev.targetNode();

    bool swallowEvent = dispatchMouseEvent(mousedownEvent, mev.targetNode(), true, d->clickCount, mouseEvent, true);

    if (!swallowEvent) {
        m_frame->handleMousePressEvent(mev);
        // Many AK widgets run their own event loops and consume events while the mouse is down.
        // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
        // the khtml state with this mouseUp, which khtml never saw.
        // If this event isn't a mouseUp, we assume that the mouseUp will be coming later.  There
        // is a hole here if the widget consumes the mouseUp and subsequent events.
        if (m_frame->lastEventIsMouseUp())
            d->mousePressed = false;
    }
}

void FrameView::handleMouseDoubleClickEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return;

    RefPtr<FrameView> protector(this);

    // We get this instead of a second mouse-up 
    d->mousePressed = false;

    MouseEventWithHitTestResults mev = prepareMouseEvent(false, true, false, mouseEvent);

    if (m_frame->passSubframeEventToSubframe(mev))
        return;

    d->clickCount = mouseEvent.clickCount();
    bool swallowEvent = dispatchMouseEvent(mouseupEvent, mev.targetNode(), true, d->clickCount, mouseEvent, false);

    if (mev.targetNode() == d->clickNode)
        dispatchMouseEvent(clickEvent, mev.targetNode(), true, d->clickCount, mouseEvent, true);

    // Qt delivers a release event AND a double click event.
    if (!swallowEvent) {
        m_frame->handleMouseReleaseEvent(mev);
        m_frame->handleMouseReleaseDoubleClickEvent(mev);
    }

    invalidateClick();
}

static bool isSubmitImage(Node *node)
{
    return node && node->hasTagName(inputTag) && static_cast<HTMLInputElement*>(node)->inputType() == HTMLInputElement::IMAGE;
}

static Cursor selectCursor(const MouseEventWithHitTestResults& event, Frame* frame, bool mousePressed)
{
    // During selection, use an I-beam no matter what we're over.
    if (mousePressed && frame->hasSelection())
        return iBeamCursor();

    Node* node = event.targetNode();
    RenderObject* renderer = node ? node->renderer() : 0;
    RenderStyle* style = renderer ? renderer->style() : 0;

    if (style && style->cursorImage() && !style->cursorImage()->image()->isNull())
        if (!style->cursorImage()->isErrorImage())
            return style->cursorImage()->image();
        else 
            style = 0; // Fallback to CURSOR_AUTO

    switch (style ? style->cursor() : CURSOR_AUTO) {
        case CURSOR_AUTO: {
            bool editable = (node && node->isContentEditable());
            if ((event.isOverLink() || isSubmitImage(node)) && (!editable || event.event().shiftKey()))
                return handCursor();
            if (editable || (renderer && renderer->isText() && renderer->canSelect()))
                return iBeamCursor();
            return pointerCursor();
        }
        case CURSOR_CROSS:
            return crossCursor();
        case CURSOR_POINTER:
            return handCursor();
        case CURSOR_MOVE:
            return moveCursor();
        case CURSOR_E_RESIZE:
            return eastResizeCursor();
        case CURSOR_W_RESIZE:
            return westResizeCursor();
        case CURSOR_N_RESIZE:
            return northResizeCursor();
        case CURSOR_S_RESIZE:
            return southResizeCursor();
        case CURSOR_NE_RESIZE:
            return northEastResizeCursor();
        case CURSOR_SW_RESIZE:
            return southWestResizeCursor();
        case CURSOR_NW_RESIZE:
            return northWestResizeCursor();
        case CURSOR_SE_RESIZE:
            return southEastResizeCursor();
        case CURSOR_TEXT:
            return iBeamCursor();
        case CURSOR_WAIT:
            return waitCursor();
        case CURSOR_HELP:
            return helpCursor();
        case CURSOR_DEFAULT:
            return pointerCursor();
    }
    return pointerCursor();
}

void FrameView::handleMouseMoveEvent(const PlatformMouseEvent& mouseEvent)
{
    // in Radar 3703768 we saw frequent crashes apparently due to the
    // part being null here, which seems impossible, so check for nil
    // but also assert so that we can try to figure this out in debug
    // builds, if it happens.
    ASSERT(m_frame);
    if (!m_frame || !m_frame->document())
        return;

    RefPtr<FrameView> protector(this);
    
    if (d->hoverTimer.isActive())
        d->hoverTimer.stop();

    if (d->resizingFrameSet) {
        dispatchMouseEvent(mousemoveEvent, d->resizingFrameSet.get(), false, 0, mouseEvent, false);
        return;
    }

    // Treat mouse move events while the mouse is pressed as "read-only" in prepareMouseEvent
    // if we are allowed to select.
    // This means that :hover and :active freeze in the state they were in when the mouse
    // was pressed, rather than updating for nodes the mouse moves over as you hold the mouse down.
    MouseEventWithHitTestResults mev = prepareMouseEvent(d->mousePressed && m_frame->mouseDownMayStartSelect(),
        d->mousePressed, true, mouseEvent);

    if (d->oldSubframe)
        m_frame->passSubframeEventToSubframe(mev, d->oldSubframe.get());

    bool swallowEvent = dispatchMouseEvent(mousemoveEvent, mev.targetNode(), false, 0, mouseEvent, true);
    if (!swallowEvent)
        m_frame->handleMouseMoveEvent(mev);
    
    RefPtr<Frame> newSubframe = subframeForEvent(mev);
    
    if (newSubframe && d->oldSubframe != newSubframe)
        m_frame->passSubframeEventToSubframe(mev, newSubframe.get());
    else
        setCursor(selectCursor(mev, m_frame.get(), d->mousePressed));
    
    d->oldSubframe = newSubframe;
}

void FrameView::invalidateClick()
{
    d->clickCount = 0;
    d->clickNode = 0;
}

void FrameView::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return;

    RefPtr<FrameView> protector(this);

    d->mousePressed = false;

    if (d->resizingFrameSet) {
        dispatchMouseEvent(mouseupEvent, d->resizingFrameSet.get(), true, d->clickCount, mouseEvent, false);
        return;
    }

    MouseEventWithHitTestResults mev = prepareMouseEvent(false, false, false, mouseEvent);

    if (m_frame->passSubframeEventToSubframe(mev))
        return;

    bool swallowEvent = dispatchMouseEvent(mouseupEvent, mev.targetNode(), true, d->clickCount, mouseEvent, false);

    if (d->clickCount > 0 && mev.targetNode() == d->clickNode)
        dispatchMouseEvent(clickEvent, mev.targetNode(), true, d->clickCount, mouseEvent, true);

    if (!swallowEvent)
        m_frame->handleMouseReleaseEvent(mev);

    invalidateClick();
}

bool FrameView::dispatchDragEvent(const AtomicString& eventType, Node *dragTarget, const PlatformMouseEvent& event, Clipboard* clipboard)
{
    IntPoint clientPos = viewportToContents(event.pos());
    
    RefPtr<MouseEvent> me = new MouseEvent(eventType,
        true, true, m_frame->document()->defaultView(),
        0, event.globalX(), event.globalY(), clientPos.x(), clientPos.y(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        0, 0, clipboard);

    ExceptionCode ec = 0;
    EventTargetNodeCast(dragTarget)->dispatchEvent(me.get(), ec, true);
    return me->defaultPrevented();
}

bool FrameView::updateDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;

    MouseEventWithHitTestResults mev = prepareMouseEvent(true, false, false, PlatformMouseEvent());

    // Drag events should never go to text nodes (following IE, and proper mouseover/out dispatch)
    Node* newTarget = mev.targetNode();
    if (newTarget && newTarget->isTextNode())
        newTarget = newTarget->parentNode();
    if (newTarget)
        newTarget = newTarget->shadowAncestorNode();

    if (d->dragTarget != newTarget) {
        // note this ordering is explicitly chosen to match WinIE
        if (newTarget)
            accept = dispatchDragEvent(dragenterEvent, newTarget, event, clipboard);
        if (d->dragTarget)
            dispatchDragEvent(dragleaveEvent, d->dragTarget.get(), event, clipboard);
    } else {
        if (newTarget)
            accept = dispatchDragEvent(dragoverEvent, newTarget, event, clipboard);
    }
    d->dragTarget = newTarget;

    return accept;
}

void FrameView::cancelDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    if (d->dragTarget)
        dispatchDragEvent(dragleaveEvent, d->dragTarget.get(), event, clipboard);
    d->dragTarget = 0;
}

bool FrameView::performDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;
    if (d->dragTarget)
        accept = dispatchDragEvent(dropEvent, d->dragTarget.get(), event, clipboard);
    d->dragTarget = 0;
    return accept;
}

Node* FrameView::nodeUnderMouse() const
{
    return d->underMouse.get();
}

bool FrameView::scrollTo(const IntRect& bounds)
{
    d->scrollingSelf = true; // so scroll events get ignored

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

    d->scrollingSelf = false;

    return scrollX != maxx && scrollY != maxy;
}

void FrameView::focusNextPrevNode(bool next)
{
    // Sets the focus node of the document to be the node after (or if next is false, before) the current focus node.
    // Only nodes that are selectable (i.e. for which isSelectable() returns true) are taken into account, and the order
    // used is that specified in the HTML spec (see Document::nextFocusNode() and Document::previousFocusNode()
    // for details).

    Document *doc = m_frame->document();
    Node *oldFocusNode = doc->focusNode();
    Node *newFocusNode;

    // Find the next/previous node from the current one
    if (next)
        newFocusNode = doc->nextFocusNode(oldFocusNode);
    else
        newFocusNode = doc->previousFocusNode(oldFocusNode);

    // If there was previously no focus node and the user has scrolled the document, then instead of picking the first
    // focusable node in the document, use the first one that lies within the visible area (if possible).
    if (!oldFocusNode && newFocusNode && d->scrollBarMoved) {
        bool visible = false;
        Node *toFocus = newFocusNode;
        while (!visible && toFocus) {
            if (toFocus->getRect().intersects(IntRect(contentsX(), contentsY(), visibleWidth(), visibleHeight()))) {
                // toFocus is visible in the contents area
                visible = true;
            } else {
                // toFocus is _not_ visible in the contents area, pick the next node
                if (next)
                    toFocus = doc->nextFocusNode(toFocus);
                else
                    toFocus = doc->previousFocusNode(toFocus);
            }
        }

        if (toFocus)
            newFocusNode = toFocus;
    }

    d->scrollBarMoved = false;

    if (!newFocusNode)
      {
        // No new focus node, scroll to bottom or top depending on next
        if (next)
            scrollTo(IntRect(contentsX()+visibleWidth()/2,contentsHeight(),0,0));
        else
            scrollTo(IntRect(contentsX()+visibleWidth()/2,0,0,0));
    }
    else {
        // EDIT FIXME: if it's an editable element, activate the caret
        // otherwise, hide it
        if (newFocusNode->isContentEditable()) {
            // make caret visible
        } 
        else {
            // hide caret
        }

        // Scroll the view as necessary to ensure that the new focus node is visible
        if (oldFocusNode) {
            if (!scrollTo(newFocusNode->getRect()))
                return;
        }
        else {
            if (doc->renderer()) {
                doc->renderer()->enclosingLayer()->scrollRectToVisible(IntRect(contentsX(), next ? 0: contentsHeight(), 0, 0));
            }
        }
    }
    // Set focus node on the document
    m_frame->document()->setFocusNode(newFocusNode);
}

void FrameView::setMediaType(const String& mediaType)
{
    d->m_mediaType = mediaType;
}

String FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType = m_frame->overrideMediaType();
    if (!overrideType.isNull())
        return overrideType;
    return d->m_mediaType;
}

void FrameView::useSlowRepaints()
{
    d->useSlowRepaints = true;
    setStaticBackground(true);
}

void FrameView::setScrollBarsMode(ScrollBarMode mode)
{
    d->vmode = mode;
    d->hmode = mode;
    
    ScrollView::setScrollBarsMode(mode);
}

void FrameView::setVScrollBarMode(ScrollBarMode mode)
{
    d->vmode = mode;
    ScrollView::setVScrollBarMode(mode);
}

void FrameView::setHScrollBarMode(ScrollBarMode mode)
{
    d->hmode = mode;
    ScrollView::setHScrollBarMode(mode);
}

void FrameView::restoreScrollBar()
{
    suppressScrollBars(false);
}

void FrameView::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    d->resizingFrameSet = frameSet;
}

MouseEventWithHitTestResults FrameView::prepareMouseEvent(bool readonly, bool active, bool mouseMove, const PlatformMouseEvent& mev)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());
    
    IntPoint vPoint = viewportToContents(mev.pos());
    return m_frame->document()->prepareMouseEvent(readonly, active, mouseMove, vPoint, mev);
}

bool FrameView::dispatchMouseEvent(const AtomicString& eventType, Node* targetNode, bool cancelable,
    int clickCount, const PlatformMouseEvent& mouseEvent, bool setUnder)
{
    // if the target node is a text node, dispatch on the parent node - rdar://4196646
    if (targetNode && targetNode->isTextNode())
        targetNode = targetNode->parentNode();
    if (targetNode)
        targetNode = targetNode->shadowAncestorNode();
    d->underMouse = targetNode;

    // mouseout/mouseover
    if (setUnder) {
        if (d->oldUnder && d->oldUnder->document() != frame()->document())
            d->oldUnder = 0;

        if (d->oldUnder != targetNode) {
            // send mouseout event to the old node
            if (d->oldUnder)
                EventTargetNodeCast(d->oldUnder.get())->dispatchMouseEvent(mouseEvent, mouseoutEvent, 0, targetNode);
            // send mouseover event to the new node
            if (targetNode)
                EventTargetNodeCast(targetNode)->dispatchMouseEvent(mouseEvent, mouseoverEvent, 0, d->oldUnder.get());
        }
        d->oldUnder = targetNode;
    }

    bool swallowEvent = false;

    if (targetNode)
        swallowEvent = EventTargetNodeCast(targetNode)->dispatchMouseEvent(mouseEvent, eventType, clickCount);
    
    if (!swallowEvent && eventType == mousedownEvent) {
        // Blur current focus node when a link/button is clicked; this
        // is expected by some sites that rely on onChange handlers running
        // from form fields before the button click is processed.
        Node* node = targetNode;
        RenderObject* renderer = node ? node->renderer() : 0;
                
        // Walk up the render tree to search for a node to focus.
        // Walking up the DOM tree wouldn't work for shadow trees, like those behind the engine-based text fields.
        while (renderer) {
            node = renderer->element();
            if (node && node->isFocusable())
                break;
            renderer = renderer->parent();
        }
        // If focus shift is blocked, we eat the event.  Note we should never clear swallowEvent
        // if the page already set it (e.g., by canceling default behavior).
        if (node && node->isMouseFocusable()) {
            if (!m_frame->document()->setFocusNode(node))
                swallowEvent = true;
        } else if (!node || !node->focused()) {
            if (!m_frame->document()->setFocusNode(0))
                swallowEvent = true;
        }
    }

    return swallowEvent;
}

void FrameView::setIgnoreWheelEvents(bool e)
{
    d->ignoreWheelEvents = e;
}

void FrameView::handleWheelEvent(PlatformWheelEvent& e)
{
    Document *doc = m_frame->document();
    if (doc) {
        RenderObject *docRenderer = doc->renderer();
        if (docRenderer) {
            IntPoint vPoint = viewportToContents(e.pos());

            RenderObject::NodeInfo hitTestResult(true, false);
            doc->renderer()->layer()->hitTest(hitTestResult, vPoint); 
            Node *node = hitTestResult.innerNode();

           if (m_frame->passWheelEventToChildWidget(node)) {
                e.accept();
                return;
            }
            if (node) {
                node = node->shadowAncestorNode();
                EventTargetNodeCast(node)->dispatchWheelEvent(e);
                if (e.isAccepted())
                    return;
            }
        }
    }
}

void FrameView::scrollBarMoved()
{
    // FIXME: Need to arrange for this to be called when the view is scrolled!
    if (!d->scrollingSelf)
        d->scrollBarMoved = true;
}

void FrameView::repaintRectangle(const IntRect& r, bool immediate)
{
    updateContents(r, immediate);
}

void FrameView::layoutTimerFired(Timer<FrameView>*)
{
#if INSTRUMENT_LAYOUT_SCHEDULING
    if (m_frame->document() && !m_frame->document()->ownerElement())
        printf("Layout timer fired at %d\n", m_frame->document()->elapsedTime());
#endif
    layout();
}

void FrameView::hoverTimerFired(Timer<FrameView>*)
{
    d->hoverTimer.stop();
    prepareMouseEvent(false, false, true, PlatformMouseEvent());
}

void FrameView::scheduleRelayout()
{
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

#if INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Scheduling layout for %d\n", delay);
#endif

    d->layoutTimer.startOneShot(delay * 0.001);
}

bool FrameView::layoutPending()
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

#if INSTRUMENT_LAYOUT_SCHEDULING
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

void FrameView::scheduleHoverStateUpdate()
{
    if (!d->hoverTimer.isActive())
        d->hoverTimer.startOneShot(0);
}

void FrameView::setHasBorder(bool b)
{
    d->m_hasBorder = b;
    updateBorder();
}

bool FrameView::hasBorder() const
{
    return d->m_hasBorder;
}

void FrameView::cleared()
{
    if (m_frame)
        if (RenderPart* renderer = m_frame->ownerRenderer())
            renderer->viewCleared();
}

}
