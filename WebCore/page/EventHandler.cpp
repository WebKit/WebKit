/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "EventHandler.h"

#include "CachedImage.h"
#include "Cursor.h"
#include "Document.h"
#include "Editor.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "HTMLFrameSetElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "PlatformScrollBar.h"
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Settings.h"

#ifdef SVG_SUPPORT
#include "SVGCursorElement.h"
#include "SVGLength.h"
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
const float LinkDragHysteresis = 40.0;
const float ImageDragHysteresis = 5.0;
const float TextDragHysteresis = 3.0;
const float GeneralDragHysteresis = 3.0;
const double TextDragDelay = 0.15;

#ifdef SVG_SUPPORT
using namespace SVGNames;
#endif

const double autoscrollInterval = 0.1;

static Frame* subframeForTargetNode(Node* node);

EventHandler::EventHandler(Frame* frame)
    : m_frame(frame)
    , m_mousePressed(false)
    , m_beganSelectingText(false)
    , m_hoverTimer(this, &EventHandler::hoverTimerFired)
    , m_autoscrollTimer(this, &EventHandler::autoscrollTimerFired)
    , m_autoscrollRenderer(0)
    , m_mouseDownMayStartAutoscroll(false)
    , m_mouseDownMayStartDrag(false)
    , m_resizeLayer(0)
    , m_capturingMouseEventsNode(0)
    , m_ignoreWheelEvents(false)
    , m_clickCount(0)
    , m_mouseDownTimestamp(0)
    , m_mouseDownWasInSubframe(false)
    , m_mouseDownMayStartSelect(false)
#if PLATFORM(MAC)
    , m_mouseDownView(nil)
    , m_sendingEventToSubview(false)
    , m_activationEventNumber(0)
#endif
{
}

EventHandler::~EventHandler()
{
}

void EventHandler::clear()
{
    m_hoverTimer.stop();
    m_resizeLayer = 0;
    m_nodeUnderMouse = 0;
    m_lastNodeUnderMouse = 0;
    m_lastMouseMoveEventSubframe = 0;
    m_lastScrollbarUnderMouse = 0;
    m_clickCount = 0;
    m_clickNode = 0;
    m_frameSetBeingResized = 0;
    m_dragTarget = 0;
    m_currentMousePosition = IntPoint();
    m_ignoreWheelEvents = false;
    m_mousePressNode = 0;
    m_mousePressed = false;
}

void EventHandler::selectClosestWordFromMouseEvent(const PlatformMouseEvent& mouse, Node *innerNode)
{
    Selection newSelection;

    if (innerNode && innerNode->renderer() && m_mouseDownMayStartSelect && innerNode->renderer()->shouldSelect()) {
        IntPoint vPoint = m_frame->view()->windowToContents(mouse.pos());
        VisiblePosition pos(innerNode->renderer()->positionForPoint(vPoint));
        if (pos.isNotNull()) {
            newSelection = Selection(pos);
            newSelection.expandUsingGranularity(WordGranularity);
        }
    }
    
    if (newSelection.isRange()) {
        m_frame->setSelectionGranularity(WordGranularity);
        m_beganSelectingText = true;
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection);
}

bool EventHandler::handleMousePressEventDoubleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;

    if (m_frame->selectionController()->isRange())
        // A double-click when range is already selected
        // should not change the selection.  So, do not call
        // selectClosestWordFromMouseEvent, but do set
        // m_beganSelectingText to prevent handleMouseReleaseEvent
        // from setting caret selection.
        m_beganSelectingText = true;
    else
        selectClosestWordFromMouseEvent(event.event(), event.targetNode());

    return true;
}

bool EventHandler::handleMousePressEventTripleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;
    
    Node* innerNode = event.targetNode();
    if (!(innerNode && innerNode->renderer() && m_mouseDownMayStartSelect
          && innerNode->renderer()->shouldSelect()))
        return false;

    Selection newSelection;
    IntPoint vPoint = m_frame->view()->windowToContents(event.event().pos());
    VisiblePosition pos(innerNode->renderer()->positionForPoint(vPoint));
    if (pos.isNotNull()) {
        newSelection = Selection(pos);
        newSelection.expandUsingGranularity(ParagraphGranularity);
    }
    if (newSelection.isRange()) {
        m_frame->setSelectionGranularity(ParagraphGranularity);
        m_beganSelectingText = true;
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection);

    return true;
}

bool EventHandler::handleMousePressEventSingleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;
    
    Node* innerNode = event.targetNode();
    if (!(innerNode && innerNode->renderer() && m_mouseDownMayStartSelect
          && innerNode->renderer()->shouldSelect()))
        return false;

    // Extend the selection if the Shift key is down, unless the click is in a link.
    bool extendSelection = event.event().shiftKey() && !event.isOverLink();

    // Don't restart the selection when the mouse is pressed on an
    // existing selection so we can allow for text dragging.
    IntPoint vPoint = m_frame->view()->windowToContents(event.event().pos());
    if (!extendSelection && m_frame->selectionController()->contains(vPoint))
        return false;

    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(vPoint));
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(innerNode, innerNode->caretMinOffset(), DOWNSTREAM);
    Position pos = visiblePos.deepEquivalent();
    
    Selection newSelection = m_frame->selectionController()->selection();
    if (extendSelection && newSelection.isCaretOrRange()) {
        m_frame->selectionController()->clearModifyBias();
        
        // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection 
        // was created right-to-left
        Position start = newSelection.start();
        Position end = newSelection.end();
        short before = Range::compareBoundaryPoints(pos.node(), pos.offset(), start.node(), start.offset());
        if (before <= 0)
            newSelection = Selection(pos, end);
        else
            newSelection = Selection(start, pos);

        if (m_frame->selectionGranularity() != CharacterGranularity)
            newSelection.expandUsingGranularity(m_frame->selectionGranularity());
        m_beganSelectingText = true;
    } else {
        newSelection = Selection(visiblePos);
        m_frame->setSelectionGranularity(CharacterGranularity);
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection);

    return true;
}

bool EventHandler::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
    bool singleClick = event.event().clickCount() <= 1;

    // If we got the event back, that must mean it wasn't prevented,
    // so it's allowed to start a drag or selection.
    m_mouseDownMayStartSelect = canMouseDownStartSelect(event.targetNode());
    
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    m_mouseDownMayStartDrag = singleClick;

    if (passWidgetMouseDownEventToWidget(event))
        return true;

    // We don't do this at the start of mouse down handling,
    // because we don't want to do it until we know we didn't hit a widget.
    if (singleClick)
        focusDocumentView();

    Node* innerNode = event.targetNode();

    m_mousePressNode = innerNode;
    m_dragStartPos = event.event().pos();

    bool swallowEvent = false;
    if (event.event().button() == LeftButton || event.event().button() == MiddleButton) {
        m_frame->selectionController()->setCaretBlinkingSuspended(true);
        m_mousePressed = true;
        m_beganSelectingText = false;

        if (event.event().clickCount() == 2)
            return handleMousePressEventDoubleClick(event);
        if (event.event().clickCount() >= 3)
            return handleMousePressEventTripleClick(event);
        swallowEvent = handleMousePressEventSingleClick(event);
    }
    
   m_mouseDownMayStartAutoscroll = m_mouseDownMayStartSelect || 
        (m_mousePressNode && m_mousePressNode->renderer() && m_mousePressNode->renderer()->shouldAutoscroll());

   return swallowEvent;
}

bool EventHandler::handleMouseMoveEvent(const MouseEventWithHitTestResults& event)
{
    if (handleDrag(event))
        return true;

    // Mouse not pressed. Do nothing.
    if (!m_mousePressed)
        return false;

    Node* innerNode = event.targetNode();

    if (event.event().button() != 0 || !innerNode || !innerNode->renderer())
        return false;

#if PLATFORM(MAC) // FIXME: Why does this assertion fire on other platforms?
    ASSERT(m_mouseDownMayStartSelect || m_mouseDownMayStartAutoscroll);
#endif

    m_mouseDownMayStartDrag = false;
    invalidateClick();

    if (m_mouseDownMayStartAutoscroll) {            
        // If the selection is contained in a layer that can scroll, that layer should handle the autoscroll
        // Otherwise, let the bridge handle it so the view can scroll itself.
        RenderObject* renderer = innerNode->renderer();
        while (renderer && !renderer->shouldAutoscroll())
            renderer = renderer->parent();
        if (renderer)
            handleAutoscroll(renderer);
    }
    
    if (!(m_mouseDownMayStartSelect && innerNode->renderer()->shouldSelect()))
        return false;

    // handle making selection
    IntPoint vPoint = m_frame->view()->windowToContents(event.event().pos());        
    VisiblePosition pos(innerNode->renderer()->positionForPoint(vPoint));

    updateSelectionForMouseDragOverPosition(pos);

    return true;
}

void EventHandler::updateSelectionForMouseDragOverPosition(const VisiblePosition& pos)
{
    // Don't modify the selection if we're not on a node.
    if (pos.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in handleMousePressEvent, but not if the mouse press was on an existing selection.
    Selection newSelection = m_frame->selectionController()->selection();
    m_frame->selectionController()->clearModifyBias();
    
    if (!m_beganSelectingText) {
        m_beganSelectingText = true;
        newSelection = Selection(pos);
    }

    newSelection.setExtent(pos);
    if (m_frame->selectionGranularity() != CharacterGranularity)
        newSelection.expandUsingGranularity(m_frame->selectionGranularity());

    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection);
}

bool EventHandler::handleMouseReleaseEvent(const MouseEventWithHitTestResults& event)
{
    stopAutoscrollTimer();

    if (handleMouseUp(event))
        return true;

    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    m_frame->selectionController()->setCaretBlinkingSuspended(false);
    m_mousePressed = false;
  
    bool handled = false;

    // Clear the selection if the mouse didn't move after the last mouse press.
    // We do this so when clicking on the selection, the selection goes away.
    // However, if we are editing, place the caret.
    if (m_mouseDownMayStartSelect && !m_beganSelectingText
            && m_dragStartPos == event.event().pos()
            && m_frame->selectionController()->isRange()) {
        Selection newSelection;
        Node *node = event.targetNode();
        if (node && node->isContentEditable() && node->renderer()) {
            IntPoint vPoint = m_frame->view()->windowToContents(event.event().pos());
            VisiblePosition pos = node->renderer()->positionForPoint(vPoint);
            newSelection = Selection(pos);
        }
        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selectionController()->setSelection(newSelection);

        handled = true;
    }

    m_frame->notifyRendererOfSelectionChange(true);

    m_frame->selectionController()->selectFrameElementInParentIfFullySelected();

    return handled;
}

void EventHandler::handleAutoscroll(RenderObject* renderer)
{
    if (m_autoscrollTimer.isActive())
        return;
    setAutoscrollRenderer(renderer);
    startAutoscrollTimer();
}

void EventHandler::autoscrollTimerFired(Timer<EventHandler>*)
{
    if (!m_mousePressed) {
        stopAutoscrollTimer();
        return;
    }
    if (RenderObject* r = autoscrollRenderer())
        r->autoscroll();
}

RenderObject* EventHandler::autoscrollRenderer() const
{
    return m_autoscrollRenderer;
}

void EventHandler::setAutoscrollRenderer(RenderObject* renderer)
{
    m_autoscrollRenderer = renderer;
}

HitTestResult EventHandler::hitTestResultAtPoint(const IntPoint& point, bool allowShadowContent)
{
    HitTestResult result(point);
    if (!m_frame->renderer())
        return result;
    m_frame->renderer()->layer()->hitTest(HitTestRequest(true, true), result);

    IntPoint widgetPoint(point);
    while (true) {
        Node* n = result.innerNode();
        if (!n || !n->renderer() || !n->renderer()->isWidget())
            break;
        Widget* widget = static_cast<RenderWidget*>(n->renderer())->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<HTMLFrameElementBase*>(n)->contentFrame();
        if (!frame || !frame->renderer())
            break;
        int absX, absY;
        n->renderer()->absolutePosition(absX, absY, true);
        FrameView* view = static_cast<FrameView*>(widget);
        widgetPoint.move(view->contentsX() - absX, view->contentsY() - absY);
        HitTestResult widgetHitTestResult(widgetPoint);
        frame->renderer()->layer()->hitTest(HitTestRequest(true, true), widgetHitTestResult);
        result = widgetHitTestResult;
    }

    if (!allowShadowContent) {
        Node* node = result.innerNode();
        if (node)
            node = node->shadowAncestorNode();
        result.setInnerNode(node);
        node = result.innerNonSharedNode();
        if (node)
            node = node->shadowAncestorNode();
        result.setInnerNonSharedNode(node); 
    }

    return result;
}


void EventHandler::startAutoscrollTimer()
{
    m_autoscrollTimer.startRepeating(autoscrollInterval);
}

void EventHandler::stopAutoscrollTimer(bool rendererIsBeingDestroyed)
{
    if (m_mouseDownWasInSubframe) {
        Frame* subframe = subframeForTargetNode(m_mousePressNode.get());
        subframe->eventHandler()->stopAutoscrollTimer(rendererIsBeingDestroyed);
        return;
    }

    if (!rendererIsBeingDestroyed && autoscrollRenderer())
        autoscrollRenderer()->stopAutoscroll();
    setAutoscrollRenderer(0);
    m_autoscrollTimer.stop();
}

Node* EventHandler::mousePressNode() const
{
    return m_mousePressNode.get();
}

void EventHandler::setMousePressNode(PassRefPtr<Node> node)
{
    m_mousePressNode = node;
}

bool EventHandler::scrollOverflow(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!m_frame->document())
        return false;
    
    Node* node = m_frame->document()->focusedNode();
    if (!node)
        node = m_mousePressNode.get();
    
    if (node) {
        RenderObject *r = node->renderer();
        if (r && !r->isListBox())
            return r->scroll(direction, granularity);
    }

    return false;
}

IntPoint EventHandler::currentMousePosition() const
{
    return m_currentMousePosition;
}

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

static bool isSubmitImage(Node* node)
{
    return node && node->hasTagName(inputTag)
        && static_cast<HTMLInputElement*>(node)->inputType() == HTMLInputElement::IMAGE;
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
                    hotSpot.setX(int(static_cast<SVGCursorElement*>(e)->x().value()));
                    hotSpot.setY(int(static_cast<SVGCursorElement*>(e)->y().value()));
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
                    case EditableLinkDefaultBehavior:
                    case EditableLinkAlwaysLive:
                        editableLinkEnabled = true;
                        break;

                    case EditableLinkNeverLive:
                        editableLinkEnabled = false;
                        break;

                    case EditableLinkLiveWhenNotFocused:
                        editableLinkEnabled = nodeIsNotBeingEdited(node, frame) || event.event().shiftKey();
                        break;
                    
                    case EditableLinkOnlyLiveWithShiftKey:
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
        case CURSOR_VERTICAL_TEXT:
            return verticalTextCursor();
        case CURSOR_CELL:
            return cellCursor();
        case CURSOR_CONTEXT_MENU:
            return contextMenuCursor();
        case CURSOR_PROGRESS:
            return progressCursor();
        case CURSOR_NO_DROP:
            return noDropCursor();
        case CURSOR_ALIAS:
            return aliasCursor();
        case CURSOR_COPY:
            return copyCursor();
        case CURSOR_NONE:
            return noneCursor();
        case CURSOR_NOT_ALLOWED:
            return noDropCursor();
        case CURSOR_DEFAULT:
            return pointerCursor();
    }
    return pointerCursor();
}

bool EventHandler::handleMousePressEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());

    m_mousePressed = true;
    m_currentMousePosition = mouseEvent.pos();
    m_mouseDownWasInSubframe = false;
    
    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(false, true), mouseEvent);

    if (!mev.targetNode()) {
        invalidateClick();
        return false;
    }

    m_mousePressNode = mev.targetNode();

    Frame* subframe = subframeForTargetNode(mev.targetNode());
    if (subframe && passMousePressEventToSubframe(mev, subframe)) {
        invalidateClick();
        return true;
    }

    m_clickCount = mouseEvent.clickCount();
    m_clickNode = mev.targetNode();
    
    RenderLayer* layer = m_clickNode->renderer() ? m_clickNode->renderer()->enclosingLayer() : 0;
    IntPoint p = m_frame->view()->windowToContents(mouseEvent.pos());
    if (layer && layer->isPointInResizeControl(p)) {
        layer->setInResizeMode(true);
        m_resizeLayer = layer;
        m_offsetFromResizeCorner = layer->offsetFromResizeCorner(p);
        invalidateClick();
        return true;
    }

    bool swallowEvent = dispatchMouseEvent(mousedownEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    if (!swallowEvent) {
        // Refetch the event target node if it currently is the shadow node inside an <input> element.
        // If a mouse event handler changes the input element type to one that has a widget associated,
        // we'd like to EventHandler::handleMousePressEvent to pass the event to the widget and thus the
        // event target node can't still be the shadow node.
        if (mev.targetNode()->isShadowNode() && mev.targetNode()->shadowParentNode()->hasTagName(inputTag))
            mev = prepareMouseEvent(HitTestRequest(true, true), mouseEvent);

        PlatformScrollbar* scrollbar = m_frame->view()->scrollbarUnderMouse(mouseEvent);
        if (!scrollbar)
            scrollbar = mev.scrollbar();
        if (scrollbar && passMousePressEventToScrollbar(mev, scrollbar))
            swallowEvent = true;
        else
            swallowEvent = handleMousePressEvent(mev);
        
        // Many AK widgets run their own event loops and consume events while the mouse is down.
        // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
        // the khtml state with this mouseUp, which khtml never saw.
        // If this event isn't a mouseUp, we assume that the mouseUp will be coming later.  There
        // is a hole here if the widget consumes the mouseUp and subsequent events.
        if (lastEventIsMouseUp())
            m_mousePressed = false;
    }

    return swallowEvent;
}

// This method only exists for platforms that don't know how to deliver 
bool EventHandler::handleMouseDoubleClickEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());

    // We get this instead of a second mouse-up 
    m_mousePressed = false;
    m_currentMousePosition = mouseEvent.pos();

    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(false, true), mouseEvent);
    Frame* subframe = subframeForTargetNode(mev.targetNode());
    if (subframe && passMousePressEventToSubframe(mev, subframe))
        return true;

    m_clickCount = mouseEvent.clickCount();
    bool swallowMouseUpEvent = dispatchMouseEvent(mouseupEvent, mev.targetNode(), true, m_clickCount, mouseEvent, false);

    bool swallowClickEvent = false;
    if (mev.targetNode() == m_clickNode)
        swallowClickEvent = dispatchMouseEvent(clickEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    bool swallowMouseReleaseEvent = false;
    if (!swallowMouseUpEvent)
        swallowMouseReleaseEvent = handleMouseReleaseEvent(mev);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

bool EventHandler::handleMouseMoveEvent(const PlatformMouseEvent& mouseEvent)
{
    // in Radar 3703768 we saw frequent crashes apparently due to the
    // part being null here, which seems impossible, so check for nil
    // but also assert so that we can try to figure this out in debug
    // builds, if it happens.
    ASSERT(m_frame);
    if (!m_frame || !m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());
    m_currentMousePosition = mouseEvent.pos();
   
    if (m_hoverTimer.isActive())
        m_hoverTimer.stop();

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(mousemoveEvent, m_frameSetBeingResized.get(), false, 0, mouseEvent, false);

    // Treat mouse move events while the mouse is pressed as "read-only" in prepareMouseEvent
    // if we are allowed to select.
    // This means that :hover and :active freeze in the state they were in when the mouse
    // was pressed, rather than updating for nodes the mouse moves over as you hold the mouse down.
    HitTestRequest request(m_mousePressed && m_mouseDownMayStartSelect, m_mousePressed, true);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseEvent);

    if (m_lastMouseMoveEventSubframe && m_lastMouseMoveEventSubframe->tree()->isDescendantOf(m_frame))
        passMouseMoveEventToSubframe(mev, m_lastMouseMoveEventSubframe.get());

    bool swallowEvent = dispatchMouseEvent(mousemoveEvent, mev.targetNode(), false, 0, mouseEvent, true);
    
    PlatformScrollbar* scrollbar = m_frame->view()->scrollbarUnderMouse(mouseEvent);
    if (!scrollbar)
        scrollbar = mev.scrollbar();

    if (m_lastScrollbarUnderMouse != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse->handleMouseOutEvent(mouseEvent);
        m_lastScrollbarUnderMouse = scrollbar;
    }

    if (m_resizeLayer && m_resizeLayer->inResizeMode())
        m_resizeLayer->resize(mouseEvent, m_offsetFromResizeCorner);

    if (!swallowEvent)
        swallowEvent = handleMouseMoveEvent(mev);
    
    RefPtr<Frame> newSubframe = subframeForTargetNode(mev.targetNode());
    if (newSubframe) {
        if (m_lastMouseMoveEventSubframe != newSubframe)
            swallowEvent |= passMouseMoveEventToSubframe(mev, newSubframe.get());
    } else {
        if (scrollbar && !m_mousePressed)
            scrollbar->handleMouseMoveEvent(mouseEvent); // Handle hover effects on platforms that support visual feedback on scrollbar hovering.
        if ((!m_resizeLayer || !m_resizeLayer->inResizeMode()) && m_frame->view())
            m_frame->view()->setCursor(selectCursor(mev, m_frame, m_mousePressed, scrollbar));
    }

    m_lastMouseMoveEventSubframe = newSubframe;

    return swallowEvent;
}

void EventHandler::invalidateClick()
{
    m_clickCount = 0;
    m_clickNode = 0;
}

bool EventHandler::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());

    m_mousePressed = false;
    m_currentMousePosition = mouseEvent.pos();

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(mouseupEvent, m_frameSetBeingResized.get(), true, m_clickCount, mouseEvent, false);

    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(false, false, false, true), mouseEvent);
    Frame* subframe = subframeForTargetNode(mev.targetNode());
    if (subframe && passMouseReleaseEventToSubframe(mev, subframe))
        return true;

    bool swallowMouseUpEvent = dispatchMouseEvent(mouseupEvent, mev.targetNode(), true, m_clickCount, mouseEvent, false);

    bool swallowClickEvent = false;
    if (m_clickCount > 0 && mev.targetNode() == m_clickNode)
        swallowClickEvent = dispatchMouseEvent(clickEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    if (m_resizeLayer) {
        m_resizeLayer->setInResizeMode(false);
        m_resizeLayer = 0;
    }

    bool swallowMouseReleaseEvent = false;
    if (!swallowMouseUpEvent)
        swallowMouseReleaseEvent = handleMouseReleaseEvent(mev);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

bool EventHandler::dispatchDragEvent(const AtomicString& eventType, Node* dragTarget, const PlatformMouseEvent& event, Clipboard* clipboard)
{
    IntPoint contentsPos = m_frame->view()->windowToContents(event.pos());
    
    RefPtr<MouseEvent> me = new MouseEvent(eventType,
        true, true, m_frame->document()->defaultView(),
        0, event.globalX(), event.globalY(), contentsPos.x(), contentsPos.y(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        0, 0, clipboard);

    ExceptionCode ec = 0;
    EventTargetNodeCast(dragTarget)->dispatchEvent(me.get(), ec, true);
    return me->defaultPrevented();
}

bool EventHandler::updateDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;

    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(true, false), event);

    // Drag events should never go to text nodes (following IE, and proper mouseover/out dispatch)
    Node* newTarget = mev.targetNode();
    if (newTarget && newTarget->isTextNode())
        newTarget = newTarget->parentNode();
    if (newTarget)
        newTarget = newTarget->shadowAncestorNode();

    if (m_dragTarget != newTarget) {
        // FIXME: this ordering was explicitly chosen to match WinIE. However,
        // it is sometimes incorrect when dragging within subframes, as seen with
        // LayoutTests/fast/events/drag-in-frames.html.
        if (newTarget)
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(dragenterEvent, newTarget, event, clipboard);
        
        if (m_dragTarget)
            if (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                dispatchDragEvent(dragleaveEvent, m_dragTarget.get(), event, clipboard);
    } else {
        if (newTarget)
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(dragoverEvent, newTarget, event, clipboard);
    }
    m_dragTarget = newTarget;

    return accept;
}

void EventHandler::cancelDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    if (m_dragTarget)
        if (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag))
            static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame()->eventHandler()->cancelDragAndDrop(event, clipboard);
        else
            dispatchDragEvent(dragleaveEvent, m_dragTarget.get(), event, clipboard);
    m_dragTarget = 0;
}

bool EventHandler::performDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;
    if (m_dragTarget)
        if (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag))
            accept = static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame()->eventHandler()->performDragAndDrop(event, clipboard);
        else
            accept = dispatchDragEvent(dropEvent, m_dragTarget.get(), event, clipboard);
    m_dragTarget = 0;
    return accept;
}

Node* EventHandler::nodeUnderMouse() const
{
    return m_nodeUnderMouse.get();
}

void EventHandler::setCapturingMouseEventsNode(PassRefPtr<Node> n)
{
    m_capturingMouseEventsNode = n;
}

MouseEventWithHitTestResults EventHandler::prepareMouseEvent(const HitTestRequest& request, const PlatformMouseEvent& mev)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());
    
    IntPoint documentPoint = m_frame->view()->windowToContents(mev.pos());
    return m_frame->document()->prepareMouseEvent(request, documentPoint, mev);
}

bool EventHandler::dispatchMouseEvent(const AtomicString& eventType, Node* targetNode, bool cancelable,
    int clickCount, const PlatformMouseEvent& mouseEvent, bool setUnder)
{
    // if the target node is a text node, dispatch on the parent node - rdar://4196646
    if (targetNode && targetNode->isTextNode())
        targetNode = targetNode->parentNode();
    if (targetNode)
        targetNode = targetNode->shadowAncestorNode();

    if (m_capturingMouseEventsNode)
        targetNode = m_capturingMouseEventsNode.get();
        
    m_nodeUnderMouse = targetNode;

    // mouseout/mouseover
    if (setUnder) {
        if (m_lastNodeUnderMouse && m_lastNodeUnderMouse->document() != m_frame->document()) {
            m_lastNodeUnderMouse = 0;
            m_lastScrollbarUnderMouse = 0;
        }

        if (m_lastNodeUnderMouse != targetNode) {
            // send mouseout event to the old node
            if (m_lastNodeUnderMouse)
                EventTargetNodeCast(m_lastNodeUnderMouse.get())->dispatchMouseEvent(mouseEvent, mouseoutEvent, 0, targetNode);
            // send mouseover event to the new node
            if (targetNode)
                EventTargetNodeCast(targetNode)->dispatchMouseEvent(mouseEvent, mouseoverEvent, 0, m_lastNodeUnderMouse.get());
        }
        m_lastNodeUnderMouse = targetNode;
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
            if (!m_frame->document()->setFocusedNode(node))
                swallowEvent = true;
        } else if (!node || !node->focused()) {
            if (!m_frame->document()->setFocusedNode(0))
                swallowEvent = true;
        }
    }

    return swallowEvent;
}

void EventHandler::setIgnoreWheelEvents(bool e)
{
    m_ignoreWheelEvents = e;
}

bool EventHandler::handleWheelEvent(PlatformWheelEvent& e)
{
    if (m_ignoreWheelEvents)
        return false;

    Document* doc = m_frame->document();
    if (!doc)
        return false;

    RenderObject* docRenderer = doc->renderer();
    if (!docRenderer)
        return false;

    IntPoint vPoint = m_frame->view()->windowToContents(e.pos());

    HitTestRequest request(true, false);
    HitTestResult result(vPoint);
    doc->renderer()->layer()->hitTest(request, result);
    Node* node = result.innerNode();    
    Frame* subframe = subframeForTargetNode(node);
    if (subframe && passWheelEventToSubframe(e, subframe)) {
        e.accept();
        return true;
    }

    if (!node)
        return false;

    node = node->shadowAncestorNode();
    EventTargetNodeCast(node)->dispatchWheelEvent(e);
    if (e.isAccepted())
        return true;
            
    if (!node->renderer())
        return false;

    // Just break up into two scrolls if we need to.  Diagonal movement on 
    // a MacBook pro is an example of a 2-dimensional mouse wheel event (where both deltaX and deltaY can be set).
    if (e.deltaX() && node->renderer()->scroll(e.deltaX() < 0 ? ScrollRight : ScrollLeft, ScrollByWheel,
                                               e.deltaX() < 0 ? -e.deltaX() : e.deltaX()))
        e.accept();
    if (e.deltaY() && node->renderer()->scroll(e.deltaY() < 0 ? ScrollDown : ScrollUp, ScrollByWheel,
                                               e.deltaY() < 0 ? -e.deltaY() : e.deltaY()))
        e.accept();
        
    if (!e.isAccepted())
        m_frame->view()->wheelEvent(e);
    
    return e.isAccepted();
}

bool EventHandler::sendContextMenuEvent(PlatformMouseEvent event)
{
    Document* doc = m_frame->document();
    FrameView* v = m_frame->view();
    if (!doc || !v)
        return false;
    
    bool swallowEvent;
    IntPoint viewportPos = v->windowToContents(event.pos());
    MouseEventWithHitTestResults mev = doc->prepareMouseEvent(HitTestRequest(false, true), viewportPos, event);
    
    swallowEvent = dispatchMouseEvent(contextmenuEvent, mev.targetNode(), true, 0, event, true);
    if (!swallowEvent && !m_frame->selectionController()->contains(viewportPos) &&
            (m_frame->editor()->selectWordBeforeMenuEvent() || m_frame->editor()->clientIsEditable()
            || (mev.targetNode() && mev.targetNode()->isContentEditable()))) {
        m_mouseDownMayStartSelect = true; // context menu events are always allowed to perform a selection
        selectClosestWordFromMouseEvent(event, mev.targetNode());
    }
    
    return swallowEvent;
}

void EventHandler::scheduleHoverStateUpdate()
{
    if (!m_hoverTimer.isActive())
        m_hoverTimer.startOneShot(0);
}

bool EventHandler::canMouseDownStartSelect(Node* node)
{
    if (!node || !node->renderer())
        return true;
    
    // Check to see if -webkit-user-select has been set to none
    if (!node->renderer()->canSelect())
        return false;
    
    // Some controls and images can't start a select on a mouse down.
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent())
        if (curr->style()->userSelect() == SELECT_IGNORE)
            return false;
    
    return true;
}

#if !PLATFORM(MAC)
bool EventHandler::inputManagerHasMarkedText() const
{
    return false;
}
#endif

void EventHandler::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    m_frameSetBeingResized = frameSet;
}

void EventHandler::hoverTimerFired(Timer<EventHandler>*)
{
    m_hoverTimer.stop();
    prepareMouseEvent(HitTestRequest(false, false, true), PlatformMouseEvent(PlatformMouseEvent::currentEvent));
}

}
