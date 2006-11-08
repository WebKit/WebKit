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
#include "CachedImage.h"
#include "Cursor.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLDocument.h"
#include "HTMLFrameElementBase.h"
#include "HTMLFrameSetElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "OverflowEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformScrollBar.h"
#include "PlatformWheelEvent.h"
#include "RenderArena.h"
#include "RenderPart.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SelectionController.h"
#include "Settings.h"
#include "cssstyleselector.h"


#ifdef SVG_SUPPORT
#include "XLinkNames.h"
#include "SVGNames.h"
#include "SVGCursorElement.h"
#include "SVGLength.h"
#endif

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;
#ifdef SVG_SUPPORT
using namespace SVGNames;
#endif

struct ScheduledEvent {
    RefPtr<Event> m_event;
    RefPtr<EventTargetNode> m_eventTarget;
    bool m_tempEvent;
};

class FrameViewPrivate {
public:
    FrameViewPrivate(FrameView* view)
        : m_hasBorder(false)
        , layoutTimer(view, &FrameView::layoutTimerFired)
        , hoverTimer(view, &FrameView::hoverTimerFired)
        , m_resizeLayer(0)
        , m_mediaType("screen")
        , m_scheduledEvents(0)
        , m_overflowStatusDirty(true)
        , m_viewportRenderer(0)
    {
        repaintRects = 0;
        isTransparent = false;
        baseBackgroundColor = Color::white;
        vmode = hmode = ScrollbarAuto;
        needToInitScrollbars = true;
        reset();
    }
    ~FrameViewPrivate()
    {
        delete repaintRects;
        delete m_scheduledEvents;
    }
    void reset()
    {
        underMouse = 0;
        oldUnder = 0;
        oldSubframe = 0;
        oldScrollbar = 0;
        linkPressed = false;
        useSlowRepaints = false;
        slowRepaintObjectCount = 0;
        dragTarget = 0;
        borderTouched = false;
        ignoreWheelEvents = false;
        borderX = 30;
        borderY = 30;
        clickCount = 0;
        clickNode = 0;
        layoutTimer.stop();
        layoutRoot = 0;
        delayedLayout = false;
        mousePressed = false;
        doFullRepaint = true;
        layoutSchedulingEnabled = true;
        layoutCount = 0;
        firstLayout = true;
        hoverTimer.stop();
        if (repaintRects)
            repaintRects->clear();
        resizingFrameSet = 0;
        m_resizeLayer = 0;
        m_currentMousePosition = IntPoint();
    }

    RefPtr<Node> underMouse;
    RefPtr<Node> oldUnder;
    RefPtr<Frame> oldSubframe;
    RefPtr<PlatformScrollbar> oldScrollbar;

    bool borderTouched : 1;
    bool borderStart : 1;
    bool doFullRepaint : 1;
    bool m_hasBorder : 1;
    
    ScrollbarMode vmode;
    ScrollbarMode hmode;
    bool linkPressed;
    bool useSlowRepaints;
    unsigned slowRepaintObjectCount;
    bool ignoreWheelEvents;

    int borderX, borderY;
    int clickCount;
    RefPtr<Node> clickNode;

    Timer<FrameView> layoutTimer;
    bool delayedLayout;
    RefPtr<Node> layoutRoot;
    
    bool layoutSchedulingEnabled;
    int layoutCount;

    bool firstLayout;
    bool needToInitScrollbars;
    bool mousePressed;
    bool isTransparent;
    Color baseBackgroundColor;

    Timer<FrameView> hoverTimer;
    
    RenderLayer* m_resizeLayer;
    IntSize offsetFromResizeCorner;
    
    // Used by objects during layout to communicate repaints that need to take place only
    // after all layout has been completed.
    DeprecatedPtrList<RenderObject::RepaintInfo>* repaintRects;
    
    RefPtr<Node> dragTarget;
    RefPtr<HTMLFrameSetElement> resizingFrameSet;
    
    String m_mediaType;
    
    Vector<ScheduledEvent*>* m_scheduledEvents;
    
    bool m_overflowStatusDirty;
    bool horizontalOverflow;
    bool m_verticalOverflow;    
    RenderObject* m_viewportRenderer;
    
    IntPoint m_currentMousePosition;
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
    resetScrollbars();

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
    
    m_frame->selectionController()->clear();

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
    if (m_frame->document()) {
        Document *document = m_frame->document();

        RenderView* root = static_cast<RenderView *>(document->renderer());
        if (!root)
            return;
        
        int docw = root->docWidth();
        int doch = root->docHeight();
    
        resizeContents(docw, doch);
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
    if (!d->repaintRects) {
        d->repaintRects = new DeprecatedPtrList<RenderObject::RepaintInfo>;
        d->repaintRects->setAutoDelete(true);
    }
    
    d->repaintRects->append(new RenderObject::RepaintInfo(o, r));
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
            Node *body = static_cast<HTMLDocument*>(document)->body();
            if (body && body->renderer()) {
                if (body->hasTagName(framesetTag)) {
                    body->renderer()->setNeedsLayout(true);
                    vMode = ScrollbarAlwaysOff;
                    hMode = ScrollbarAlwaysOff;
                } else if (body->hasTagName(bodyTag)) {
                    if (!d->firstLayout && m_size.height() != visibleHeight() && static_cast<RenderBox*>(body->renderer())->stretchesToViewHeight())
                        body->renderer()->setChildNeedsLayout(true);
                    // It's sufficient to just check one overflow direction, since it's illegal to have visible in only one direction.
                    RenderObject* o = rootRenderer->style()->overflowX() == OVISIBLE ? body->renderer() : rootRenderer;
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

    d->doFullRepaint = !subtree && (d->firstLayout || static_cast<RenderView*>(root)->printingMode());
    if (d->repaintRects)
        d->repaintRects->clear();

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
    root->layout();
    d->layoutRoot = 0;

    m_frame->invalidateSelection();
   
    d->layoutSchedulingEnabled=true;

    if (!subtree && !static_cast<RenderView*>(root)->printingMode())
        resizeContents(layer->width(), layer->height());

    // Now update the positions of all layers.
    layer->updateLayerPositions(d->doFullRepaint);

    // We update our widget positions right after doing a layout.
    if (!subtree)
        static_cast<RenderView*>(root)->updateWidgetPositions();
    
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
}

//
// Event Handling
//
/////////////////

static Frame* subframeForTargetNode(Node* node)
{
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;

    Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
    if (!widget || !widget->isFrameView())
        return 0;

    return static_cast<FrameView*>(widget)->frame();
}

IntPoint FrameView::currentMousePosition() const
{
    return d->m_currentMousePosition;
}

void FrameView::handleMousePressEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return;

    RefPtr<FrameView> protector(this);

    d->mousePressed = true;
    d->m_currentMousePosition = mouseEvent.pos();
    
    MouseEventWithHitTestResults mev = prepareMouseEvent(false, true, false, mouseEvent);

    Frame* subframe = subframeForTargetNode(mev.targetNode());
    if (subframe && passMousePressEventToSubframe(mev, subframe)) {
        invalidateClick();
        return;
    }

    d->clickCount = mouseEvent.clickCount();
    d->clickNode = mev.targetNode();
    
    RenderLayer* layer = d->clickNode->renderer()? d->clickNode->renderer()->enclosingLayer() : 0;
    IntPoint p =  windowToContents(mouseEvent.pos());
    if (layer && layer->isPointInResizeControl(p)) {
        layer->setInResizeMode(true);
        d->m_resizeLayer = layer;
        d->offsetFromResizeCorner = layer->offsetFromResizeCorner(p);
        invalidateClick();
        return;  
    }

    bool swallowEvent = dispatchMouseEvent(mousedownEvent, mev.targetNode(), true, d->clickCount, mouseEvent, true);

    if (!swallowEvent) {
        // Refetch the event target node if it currently is the shadow node inside an <input> element.
        // If a mouse event handler changes the input element type to one that has a widget associated,
        // we'd like to Frame::handleMousePressEvent to pass the event to the widget and thus the
        // event target node can't still be the shadow node.
        if (mev.targetNode()->isShadowNode() && mev.targetNode()->shadowParentNode()->hasTagName(inputTag))
            mev = prepareMouseEvent(true, true, false, mouseEvent);

        PlatformScrollbar* scrollbar = scrollbarUnderMouse(mouseEvent);
        if (!scrollbar)
            scrollbar = mev.scrollbar();
        if (!scrollbar || !passMousePressEventToScrollbar(mev, scrollbar))
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

// This method only exists for platforms that don't know how to deliver 
void FrameView::handleMouseDoubleClickEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return;

    RefPtr<FrameView> protector(this);

    // We get this instead of a second mouse-up 
    d->mousePressed = false;
    d->m_currentMousePosition = mouseEvent.pos();

    MouseEventWithHitTestResults mev = prepareMouseEvent(false, true, false, mouseEvent);
    Frame* subframe = subframeForTargetNode(mev.targetNode());
    if (subframe && passMousePressEventToSubframe(mev, subframe))
        return;

    d->clickCount = mouseEvent.clickCount();
    bool swallowEvent = dispatchMouseEvent(mouseupEvent, mev.targetNode(), true, d->clickCount, mouseEvent, false);

    if (mev.targetNode() == d->clickNode)
        dispatchMouseEvent(clickEvent, mev.targetNode(), true, d->clickCount, mouseEvent, true);

    if (!swallowEvent)
        m_frame->handleMouseReleaseEvent(mev);

    invalidateClick();
}

static bool isSubmitImage(Node *node)
{
    return node && node->hasTagName(inputTag) && static_cast<HTMLInputElement*>(node)->inputType() == HTMLInputElement::IMAGE;
}

// Returns true if the node's editable block is not current focused for editing
static bool nodeIsNotBeingEdited(Node* node, Frame* frame)
{
    return frame->selectionController()->rootEditableElement() != node->rootEditableElement();
}

static Cursor selectCursor(const MouseEventWithHitTestResults& event, Frame* frame, bool mousePressed, PlatformScrollbar* scrollbar)
{
    // During selection, use an I-beam no matter what we're over.
    if (mousePressed && frame->selectionController()->isCaretOrRange())
        return iBeamCursor();

    Node* node = event.targetNode();
    RenderObject* renderer = node ? node->renderer() : 0;
    RenderStyle* style = renderer ? renderer->style() : 0;

    if (style && style->cursors()) {
        const CursorList* cursors = style->cursors();
        for (unsigned i = 0; i < cursors->size(); ++i) {
            CachedImage* cimage = (*cursors)[i].cursorImage;
            IntPoint hotSpot = (*cursors)[i].hotSpot;
#ifdef SVG_SUPPORT
            if (!cimage) {
                Element* e = node->document()->getElementById((*cursors)[i].cursorFragmentId);
                if (e && e->hasTagName(cursorTag)) {
                    hotSpot.setX(int(static_cast<SVGCursorElement*>(e)->x()->value()));
                    hotSpot.setY(int(static_cast<SVGCursorElement*>(e)->y()->value()));
                    cimage = static_cast<SVGCursorElement*>(e)->cachedImage();
                }
            }
#endif
            if (!cimage)
                continue;
            if (cimage->image()->isNull())
                break;
            if (!cimage->isErrorImage()) {
                return Cursor(cimage->image(), hotSpot);
}
        }
    }

    switch (style ? style->cursor() : CURSOR_AUTO) {
        case CURSOR_AUTO: {
            bool editable = (node && node->isContentEditable());
            bool editableLinkEnabled = false;

            // If the link is editable, then we need to check the settings to see whether or not the link should be followed
            if (editable) {
                switch(frame->settings()->editableLinkBehavior()) {
                    default:
                    case Settings::EditableLinkDefaultBehavior:
                    case Settings::EditableLinkAlwaysLive:
                        editableLinkEnabled = true;
                        break;
                    
                    case Settings::EditableLinkLiveWhenNotFocused:
                        editableLinkEnabled = nodeIsNotBeingEdited(node, frame) || event.event().shiftKey();
                        break;
                    
                    case Settings::EditableLinkOnlyLiveWithShiftKey:
                        editableLinkEnabled = event.event().shiftKey();
                        break;
                }
            }
            
            if ((event.isOverLink() || isSubmitImage(node)) && (!editable || editableLinkEnabled))
                return handCursor();
            RenderLayer* layer = renderer ? renderer->enclosingLayer() : 0;
            bool inResizer = false;
            if (frame->view() && layer && layer->isPointInResizeControl(frame->view()->windowToContents(event.event().pos())))
                inResizer = true;
            if ((editable || (renderer && renderer->isText() && renderer->canSelect())) && !inResizer && !scrollbar)
                return iBeamCursor();
            return pointerCursor();
        }
        case CURSOR_CROSS:
            return crossCursor();
        case CURSOR_POINTER:
            return handCursor();
        case CURSOR_MOVE:
            return moveCursor();
        case CURSOR_ALL_SCROLL:
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
        case CURSOR_NS_RESIZE:
            return northSouthResizeCursor();
        case CURSOR_EW_RESIZE:
            return eastWestResizeCursor();
        case CURSOR_NESW_RESIZE:
            return northEastSouthWestResizeCursor();
        case CURSOR_NWSE_RESIZE:
            return northWestSouthEastResizeCursor();
        case CURSOR_COL_RESIZE:
            return columnResizeCursor();
        case CURSOR_ROW_RESIZE:
            return rowResizeCursor();
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
    d->m_currentMousePosition = mouseEvent.pos();
   
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

    if (d->oldSubframe && d->oldSubframe->tree()->isDescendantOf(m_frame.get()))
        passMouseMoveEventToSubframe(mev, d->oldSubframe.get());

    bool swallowEvent = dispatchMouseEvent(mousemoveEvent, mev.targetNode(), false, 0, mouseEvent, true);
    
    PlatformScrollbar* scrollbar = scrollbarUnderMouse(mouseEvent);
    if (!scrollbar)
        scrollbar = mev.scrollbar();

    if (d->oldScrollbar != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (d->oldScrollbar)
            d->oldScrollbar->handleMouseOutEvent(mouseEvent);
        d->oldScrollbar = scrollbar;
    }

    if (d->m_resizeLayer && d->m_resizeLayer->inResizeMode())
        d->m_resizeLayer->resize(mouseEvent, d->offsetFromResizeCorner);

    if (!swallowEvent)
        m_frame->handleMouseMoveEvent(mev);
    
    RefPtr<Frame> newSubframe = subframeForTargetNode(mev.targetNode());
    if (newSubframe) {
        if (d->oldSubframe != newSubframe)
            passMouseMoveEventToSubframe(mev, newSubframe.get());
    } else {
        if (scrollbar && !d->mousePressed)
            scrollbar->handleMouseMoveEvent(mouseEvent); // Handle hover effects on platforms that support visual feedback on scrollbar hovering.
        if (!d->m_resizeLayer || !d->m_resizeLayer->inResizeMode())
            setCursor(selectCursor(mev, m_frame.get(), d->mousePressed, scrollbar));
    }

    d->oldSubframe = newSubframe;
}

void FrameView::invalidateClick()
{
    d->clickCount = 0;
    d->clickNode = 0;
}

bool FrameView::mousePressed()
{
    return d->mousePressed;
}

void FrameView::setMousePressed(bool pressed)
{
    d->mousePressed = pressed;
}

void FrameView::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return;

    RefPtr<FrameView> protector(this);

    d->mousePressed = false;
    d->m_currentMousePosition = mouseEvent.pos();

    if (d->resizingFrameSet) {
        dispatchMouseEvent(mouseupEvent, d->resizingFrameSet.get(), true, d->clickCount, mouseEvent, false);
        return;
    }

    MouseEventWithHitTestResults mev = prepareMouseEvent(false, false, false, mouseEvent);
    Frame* subframe = subframeForTargetNode(mev.targetNode());
    if (subframe && passMouseReleaseEventToSubframe(mev, subframe))
        return;

    bool swallowEvent = dispatchMouseEvent(mouseupEvent, mev.targetNode(), true, d->clickCount, mouseEvent, false);

    if (d->clickCount > 0 && mev.targetNode() == d->clickNode)
        dispatchMouseEvent(clickEvent, mev.targetNode(), true, d->clickCount, mouseEvent, true);

    if (d->m_resizeLayer) {
        d->m_resizeLayer->setInResizeMode(false);
        d->m_resizeLayer = 0;
    }

    if (!swallowEvent)
        m_frame->handleMouseReleaseEvent(mev);

    invalidateClick();
}

bool FrameView::dispatchDragEvent(const AtomicString& eventType, Node *dragTarget, const PlatformMouseEvent& event, Clipboard* clipboard)
{
    IntPoint contentsPos = windowToContents(event.pos());
    
    RefPtr<MouseEvent> me = new MouseEvent(eventType,
        true, true, m_frame->document()->defaultView(),
        0, event.globalX(), event.globalY(), contentsPos.x(), contentsPos.y(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        0, 0, clipboard);

    ExceptionCode ec = 0;
    EventTargetNodeCast(dragTarget)->dispatchEvent(me.get(), ec, true);
    return me->defaultPrevented();
}

bool FrameView::updateDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;

    MouseEventWithHitTestResults mev = prepareMouseEvent(true, false, false, event);

    // Drag events should never go to text nodes (following IE, and proper mouseover/out dispatch)
    Node* newTarget = mev.targetNode();
    if (newTarget && newTarget->isTextNode())
        newTarget = newTarget->parentNode();
    if (newTarget)
        newTarget = newTarget->shadowAncestorNode();

    if (d->dragTarget != newTarget) {
        // FIXME: this ordering was explicitly chosen to match WinIE. However,
        // it is sometimes incorrect when dragging within subframes, as seen with
        // LayoutTests/fast/events/drag-in-frames.html.
        if (newTarget)
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->view()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(dragenterEvent, newTarget, event, clipboard);
        
        if (d->dragTarget)
            if (d->dragTarget->hasTagName(frameTag) || d->dragTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(d->dragTarget.get())->contentFrame()->view()->updateDragAndDrop(event, clipboard);
            else
                dispatchDragEvent(dragleaveEvent, d->dragTarget.get(), event, clipboard);
    } else {
        if (newTarget)
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->view()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(dragoverEvent, newTarget, event, clipboard);
    }
    d->dragTarget = newTarget;

    return accept;
}

void FrameView::cancelDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    if (d->dragTarget)
        if (d->dragTarget->hasTagName(frameTag) || d->dragTarget->hasTagName(iframeTag))
            static_cast<HTMLFrameElementBase*>(d->dragTarget.get())->contentFrame()->view()->cancelDragAndDrop(event, clipboard);
        else
            dispatchDragEvent(dragleaveEvent, d->dragTarget.get(), event, clipboard);
    d->dragTarget = 0;
}

bool FrameView::performDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;
    if (d->dragTarget)
        if (d->dragTarget->hasTagName(frameTag) || d->dragTarget->hasTagName(iframeTag))
            accept = static_cast<HTMLFrameElementBase*>(d->dragTarget.get())->contentFrame()->view()->performDragAndDrop(event, clipboard);
        else
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

bool FrameView::advanceFocus(bool forward)
{
    Document* document = m_frame->document();
    if (!document)
        return false;

    Node* node = forward
        ? document->nextFocusNode(document->focusNode())
        : document->previousFocusNode(document->focusNode());

    if (!node)
        // FIXME: Need to support tabbing out of the document to the UI.
        return false;

    if (!node->isElementNode())
        // FIXME: May need a way to focus a document here.
        return false;

    static_cast<Element*>(node)->focus();
    return true;
}

void FrameView::setMediaType(const String& mediaType)
{
    d->m_mediaType = mediaType;
}

String FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType = m_frame->loader()->overrideMediaType();
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

void FrameView::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    d->resizingFrameSet = frameSet;
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

MouseEventWithHitTestResults FrameView::prepareMouseEvent(bool readonly, bool active, bool mouseMove, const PlatformMouseEvent& mev)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());
    
    IntPoint vPoint = windowToContents(mev.pos());
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
        if (d->oldUnder && d->oldUnder->document() != frame()->document()) {
            d->oldUnder = 0;
            d->oldScrollbar = 0;
        }

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

#if PLATFORM(WIN)
        // It's ok to shift focus to this view now that we know that no focus change got blocked.
        if (!swallowEvent && !hasFocus())
            setFocus();
#endif
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
            IntPoint vPoint = windowToContents(e.pos());

            HitTestRequest hitTestRequest(true, false);
            HitTestResult hitTestResult(vPoint);
            doc->renderer()->layer()->hitTest(hitTestRequest, hitTestResult); 
            Node *node = hitTestResult.innerNode();
            Frame* subframe = subframeForTargetNode(node);
            if (subframe && passWheelEventToSubframe(e, subframe)) {
                e.accept();
                return;
            }
            
            if (node) {
                node = node->shadowAncestorNode();
                EventTargetNodeCast(node)->dispatchWheelEvent(e);
                if (e.isAccepted())
                    return;
                    
                if (node->renderer()) {
                    // Just break up into two scrolls if we need to.  Diagonal movement on 
                    // a MacBook pro is an example of a 2-dimensional mouse wheel event (where both deltaX and deltaY can be set).
                    if (e.deltaX() && node->renderer()->scroll(e.deltaX() < 0 ? ScrollRight : ScrollLeft, ScrollByWheel,
                                                               e.deltaX() < 0 ? -e.deltaX() : e.deltaX()))
                        e.accept();
                    if (e.deltaY() && node->renderer()->scroll(e.deltaY() < 0 ? ScrollDown : ScrollUp, ScrollByWheel,
                                                               e.deltaY() < 0 ? -e.deltaY() : e.deltaY()))
                        e.accept();
                        
                    if (!e.isAccepted())
                        wheelEvent(e);
                }
            }
        }
    }
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

void FrameView::hoverTimerFired(Timer<FrameView>*)
{
    d->hoverTimer.stop();
    prepareMouseEvent(false, false, true, PlatformMouseEvent(PlatformMouseEvent::currentEvent));
}

void FrameView::scheduleRelayout()
{
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
    if (!d->layoutSchedulingEnabled || m_frame->document() && m_frame->document()->renderer() && m_frame->document()->renderer()->needsLayout()) {
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

void FrameView::scheduleEvent(PassRefPtr<Event> event, PassRefPtr<EventTargetNode> eventTarget, bool tempEvent)
{
    if (!d->m_scheduledEvents)
        d->m_scheduledEvents = new Vector<ScheduledEvent*>;
    
    ScheduledEvent *scheduledEvent = new ScheduledEvent;
    scheduledEvent->m_event = event;
    scheduledEvent->m_eventTarget = eventTarget;
    scheduledEvent->m_tempEvent = tempEvent;
    
    d->m_scheduledEvents->append(scheduledEvent);
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
        
        scheduleEvent(new OverflowEvent(horizontalOverflowChanged, horizontalOverflow, verticalOverflowChanged, verticalOverflow),
                                        EventTargetNodeCast(d->m_viewportRenderer->element()), true);
    }
    
}

void FrameView::dispatchScheduledEvents()
{
    if (!d->m_scheduledEvents)
        return;
    
    Vector<ScheduledEvent*> scheduledEventsCopy = *d->m_scheduledEvents;
    d->m_scheduledEvents->clear();
    
    Vector<ScheduledEvent*>::iterator end = scheduledEventsCopy.end();
    for (Vector<ScheduledEvent*>::iterator it = scheduledEventsCopy.begin(); it != end; ++it) {
        ScheduledEvent* scheduledEvent = *it;
        
        ExceptionCode ec = 0;
        
        // Only dispatch events to nodes that are in the document
        if (scheduledEvent->m_eventTarget->inDocument())
            scheduledEvent->m_eventTarget->dispatchEvent(scheduledEvent->m_event, ec, scheduledEvent->m_tempEvent);
        
        delete scheduledEvent;
    }    
}

IntRect FrameView::windowClipRect() const
{
    return windowClipRect(true);
}

IntRect FrameView::windowClipRect(bool clipToContents) const
{
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
 
    // Now apply the clip from our view.
    clipRect.intersect(windowClipRect());

    // Return the result.
    return clipRect;
}

}
