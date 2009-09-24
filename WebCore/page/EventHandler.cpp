/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "CachedImage.h"
#include "ChromeClient.h"
#include "Cursor.h"
#include "Document.h"
#include "DragController.h"
#include "Editor.h"
#include "EventNames.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameElementBase.h"
#include "HTMLFrameSetElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "InspectorController.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderFrameSet.h"
#include "RenderTextControlSingleLine.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "Scrollbar.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TextEvent.h"
#include "htmlediting.h" // for comparePositions()
#include <wtf/StdLibExtras.h>

#if ENABLE(SVG)
#include "SVGDocument.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

#if ENABLE(DRAG_SUPPORT)
// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
const int LinkDragHysteresis = 40;
const int ImageDragHysteresis = 5;
const int TextDragHysteresis = 3;
const int GeneralDragHysteresis = 3;
#endif // ENABLE(DRAG_SUPPORT)

// Match key code of composition keydown event on windows.
// IE sends VK_PROCESSKEY which has value 229;
const int CompositionEventKeyCode = 229;

#if ENABLE(SVG)
using namespace SVGNames;
#endif

// When the autoscroll or the panScroll is triggered when do the scroll every 0.05s to make it smooth
const double autoscrollInterval = 0.05;

static Frame* subframeForHitTestResult(const MouseEventWithHitTestResults&);

static inline void scrollAndAcceptEvent(float delta, ScrollDirection positiveDirection, ScrollDirection negativeDirection, PlatformWheelEvent& e, Node* node, Node** stopNode)
{
    if (!delta)
        return;
        
    // Find the nearest enclosing box.
    RenderBox* enclosingBox = node->renderer()->enclosingBox();

    if (e.granularity() == ScrollByPageWheelEvent) {
        if (enclosingBox->scroll(delta < 0 ? negativeDirection : positiveDirection, ScrollByPage, 1, stopNode))
            e.accept();
        return;
    }

    float pixelsToScroll = delta > 0 ? delta : -delta;
    if (enclosingBox->scroll(delta < 0 ? negativeDirection : positiveDirection, ScrollByPixel, pixelsToScroll, stopNode))
        e.accept();
}

#if !PLATFORM(MAC)

inline bool EventHandler::eventLoopHandleMouseUp(const MouseEventWithHitTestResults&)
{
    return false;
}

inline bool EventHandler::eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&)
{
    return false;
}

#endif

EventHandler::EventHandler(Frame* frame)
    : m_frame(frame)
    , m_mousePressed(false)
    , m_capturesDragging(false)
    , m_mouseDownMayStartSelect(false)
#if ENABLE(DRAG_SUPPORT)
    , m_mouseDownMayStartDrag(false)
#endif
    , m_mouseDownWasSingleClickInSelection(false)
    , m_beganSelectingText(false)
    , m_panScrollInProgress(false)
    , m_panScrollButtonPressed(false)
    , m_springLoadedPanScrollInProgress(false)
    , m_hoverTimer(this, &EventHandler::hoverTimerFired)
    , m_autoscrollTimer(this, &EventHandler::autoscrollTimerFired)
    , m_autoscrollRenderer(0)
    , m_autoscrollInProgress(false)
    , m_mouseDownMayStartAutoscroll(false)
    , m_mouseDownWasInSubframe(false)
#if ENABLE(SVG)
    , m_svgPan(false)
#endif
    , m_resizeLayer(0)
    , m_capturingMouseEventsNode(0)
    , m_clickCount(0)
    , m_mouseDownTimestamp(0)
    , m_useLatchedWheelEventNode(false)
    , m_widgetIsLatched(false)
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
    
#if ENABLE(DRAG_SUPPORT)
EventHandler::EventHandlerDragState& EventHandler::dragState()
{
    DEFINE_STATIC_LOCAL(EventHandlerDragState, state, ());
    return state;
}
#endif // ENABLE(DRAG_SUPPORT)
    
void EventHandler::clear()
{
    m_hoverTimer.stop();
    m_resizeLayer = 0;
    m_nodeUnderMouse = 0;
    m_lastNodeUnderMouse = 0;
#if ENABLE(SVG)
    m_instanceUnderMouse = 0;
    m_lastInstanceUnderMouse = 0;
#endif
    m_lastMouseMoveEventSubframe = 0;
    m_lastScrollbarUnderMouse = 0;
    m_clickCount = 0;
    m_clickNode = 0;
    m_frameSetBeingResized = 0;
#if ENABLE(DRAG_SUPPORT)
    m_dragTarget = 0;
#endif
    m_currentMousePosition = IntPoint();
    m_mousePressNode = 0;
    m_mousePressed = false;
    m_capturesDragging = false;
    m_capturingMouseEventsNode = 0;
    m_latchedWheelEventNode = 0;
    m_previousWheelScrolledNode = 0;
}

void EventHandler::selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults& result)
{
    Node* innerNode = result.targetNode();
    VisibleSelection newSelection;

    if (innerNode && innerNode->renderer() && m_mouseDownMayStartSelect) {
        VisiblePosition pos(innerNode->renderer()->positionForPoint(result.localPoint()));
        if (pos.isNotNull()) {
            newSelection = VisibleSelection(pos);
            newSelection.expandUsingGranularity(WordGranularity);
        }
    
        if (newSelection.isRange()) {
            m_frame->setSelectionGranularity(WordGranularity);
            m_beganSelectingText = true;
            if (result.event().clickCount() == 2 && m_frame->editor()->isSelectTrailingWhitespaceEnabled()) 
                newSelection.appendTrailingWhitespace();            
        }
        
        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selection()->setSelection(newSelection);
    }
}

void EventHandler::selectClosestWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults& result)
{
    if (!result.hitTestResult().isLiveLink())
        return selectClosestWordFromMouseEvent(result);

    Node* innerNode = result.targetNode();

    if (innerNode && innerNode->renderer() && m_mouseDownMayStartSelect) {
        VisibleSelection newSelection;
        Element* URLElement = result.hitTestResult().URLElement();
        VisiblePosition pos(innerNode->renderer()->positionForPoint(result.localPoint()));
        if (pos.isNotNull() && pos.deepEquivalent().node()->isDescendantOf(URLElement))
            newSelection = VisibleSelection::selectionFromContentsOfNode(URLElement);
    
        if (newSelection.isRange()) {
            m_frame->setSelectionGranularity(WordGranularity);
            m_beganSelectingText = true;
        }

        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selection()->setSelection(newSelection);
    }
}

bool EventHandler::handleMousePressEventDoubleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;

    if (m_frame->selection()->isRange())
        // A double-click when range is already selected
        // should not change the selection.  So, do not call
        // selectClosestWordFromMouseEvent, but do set
        // m_beganSelectingText to prevent handleMouseReleaseEvent
        // from setting caret selection.
        m_beganSelectingText = true;
    else
        selectClosestWordFromMouseEvent(event);

    return true;
}

bool EventHandler::handleMousePressEventTripleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;
    
    Node* innerNode = event.targetNode();
    if (!(innerNode && innerNode->renderer() && m_mouseDownMayStartSelect))
        return false;

    VisibleSelection newSelection;
    VisiblePosition pos(innerNode->renderer()->positionForPoint(event.localPoint()));
    if (pos.isNotNull()) {
        newSelection = VisibleSelection(pos);
        newSelection.expandUsingGranularity(ParagraphGranularity);
    }
    if (newSelection.isRange()) {
        m_frame->setSelectionGranularity(ParagraphGranularity);
        m_beganSelectingText = true;
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selection()->setSelection(newSelection);

    return true;
}

bool EventHandler::handleMousePressEventSingleClick(const MouseEventWithHitTestResults& event)
{
    Node* innerNode = event.targetNode();
    if (!(innerNode && innerNode->renderer() && m_mouseDownMayStartSelect))
        return false;

    // Extend the selection if the Shift key is down, unless the click is in a link.
    bool extendSelection = event.event().shiftKey() && !event.isOverLink();

    // Don't restart the selection when the mouse is pressed on an
    // existing selection so we can allow for text dragging.
    if (FrameView* view = m_frame->view()) {
        IntPoint vPoint = view->windowToContents(event.event().pos());
        if (!extendSelection && m_frame->selection()->contains(vPoint)) {
            m_mouseDownWasSingleClickInSelection = true;
            return false;
        }
    }

    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(event.localPoint()));
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(innerNode, 0, DOWNSTREAM);
    Position pos = visiblePos.deepEquivalent();
    
    VisibleSelection newSelection = m_frame->selection()->selection();
    if (extendSelection && newSelection.isCaretOrRange()) {
        m_frame->selection()->setLastChangeWasHorizontalExtension(false);
        
        // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection 
        // was created right-to-left
        Position start = newSelection.start();
        Position end = newSelection.end();
        if (comparePositions(pos, start) <= 0)
            newSelection = VisibleSelection(pos, end);
        else
            newSelection = VisibleSelection(start, pos);

        if (m_frame->selectionGranularity() != CharacterGranularity)
            newSelection.expandUsingGranularity(m_frame->selectionGranularity());
        m_beganSelectingText = true;
    } else {
        newSelection = VisibleSelection(visiblePos);
        m_frame->setSelectionGranularity(CharacterGranularity);
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selection()->setSelection(newSelection);

    return true;
}

bool EventHandler::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
#if ENABLE(DRAG_SUPPORT)
    // Reset drag state.
    dragState().m_dragSrc = 0;
#endif

    if (ScrollView* scrollView = m_frame->view()) {
        if (scrollView->isPointInScrollbarCorner(event.event().pos()))
            return false;
    }

    bool singleClick = event.event().clickCount() <= 1;

    // If we got the event back, that must mean it wasn't prevented,
    // so it's allowed to start a drag or selection.
    m_mouseDownMayStartSelect = canMouseDownStartSelect(event.targetNode());
    
#if ENABLE(DRAG_SUPPORT)
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    m_mouseDownMayStartDrag = singleClick;
#endif

    m_mouseDownWasSingleClickInSelection = false;

    m_mouseDown = event.event();

    if (event.isOverWidget() && passWidgetMouseDownEventToWidget(event))
        return true;

#if ENABLE(SVG)
    if (m_frame->document()->isSVGDocument() &&
       static_cast<SVGDocument*>(m_frame->document())->zoomAndPanEnabled()) {
        if (event.event().shiftKey() && singleClick) {
            m_svgPan = true;
            static_cast<SVGDocument*>(m_frame->document())->startPan(event.event().pos());
            return true;
        }
    }
#endif

    // We don't do this at the start of mouse down handling,
    // because we don't want to do it until we know we didn't hit a widget.
    if (singleClick)
        focusDocumentView();

    Node* innerNode = event.targetNode();

    m_mousePressNode = innerNode;
#if ENABLE(DRAG_SUPPORT)
    m_dragStartPos = event.event().pos();
#endif

    bool swallowEvent = false;
    m_frame->selection()->setCaretBlinkingSuspended(true);
    m_mousePressed = true;
    m_beganSelectingText = false;

    if (event.event().clickCount() == 2)
        swallowEvent = handleMousePressEventDoubleClick(event);
    else if (event.event().clickCount() >= 3)
        swallowEvent = handleMousePressEventTripleClick(event);
    else
        swallowEvent = handleMousePressEventSingleClick(event);
    
    m_mouseDownMayStartAutoscroll = m_mouseDownMayStartSelect || 
        (m_mousePressNode && m_mousePressNode->renderBox() && m_mousePressNode->renderBox()->canBeProgramaticallyScrolled(true));

    return swallowEvent;
}

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::handleMouseDraggedEvent(const MouseEventWithHitTestResults& event)
{
    if (handleDrag(event))
        return true;

    if (!m_mousePressed)
        return false;

    Node* targetNode = event.targetNode();
    if (event.event().button() != LeftButton || !targetNode || !targetNode->renderer())
        return false;

#if PLATFORM(MAC) // FIXME: Why does this assertion fire on other platforms?
    ASSERT(m_mouseDownMayStartSelect || m_mouseDownMayStartAutoscroll);
#endif

    m_mouseDownMayStartDrag = false;

    if (m_mouseDownMayStartAutoscroll && !m_panScrollInProgress) {            
        // If the selection is contained in a layer that can scroll, that layer should handle the autoscroll
        // Otherwise, let the bridge handle it so the view can scroll itself.
        RenderObject* renderer = targetNode->renderer();
        while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeScrolledAndHasScrollableArea())) {
            if (!renderer->parent() && renderer->node() == renderer->document() && renderer->document()->ownerElement())
                renderer = renderer->document()->ownerElement()->renderer();
            else
                renderer = renderer->parent();
        }
        
        if (renderer) {
            m_autoscrollInProgress = true;
            handleAutoscroll(renderer);
        }
        
        m_mouseDownMayStartAutoscroll = false;
    }
    
    updateSelectionForMouseDrag(targetNode, event.localPoint());
    return true;
}
    
bool EventHandler::eventMayStartDrag(const PlatformMouseEvent& event) const
{
    // This is a pre-flight check of whether the event might lead to a drag being started.  Be careful
    // that its logic needs to stay in sync with handleMouseMoveEvent() and the way we setMouseDownMayStartDrag
    // in handleMousePressEvent
    
    if (!m_frame->contentRenderer() || !m_frame->contentRenderer()->hasLayer())
        return false;

    if (event.button() != LeftButton || event.clickCount() != 1)
        return false;
    
    bool DHTMLFlag;
    bool UAFlag;
    allowDHTMLDrag(DHTMLFlag, UAFlag);
    if (!DHTMLFlag && !UAFlag)
        return false;

    FrameView* view = m_frame->view();
    if (!view)
        return false;

    HitTestRequest request(HitTestRequest::ReadOnly);
    HitTestResult result(view->windowToContents(event.pos()));
    m_frame->contentRenderer()->layer()->hitTest(request, result);
    bool srcIsDHTML;
    return result.innerNode() && result.innerNode()->renderer()->draggableNode(DHTMLFlag, UAFlag, result.point().x(), result.point().y(), srcIsDHTML);
}

void EventHandler::updateSelectionForMouseDrag()
{
    FrameView* view = m_frame->view();
    if (!view)
        return;
    RenderView* renderer = m_frame->contentRenderer();
    if (!renderer)
        return;
    RenderLayer* layer = renderer->layer();
    if (!layer)
        return;

    HitTestRequest request(HitTestRequest::ReadOnly |
                           HitTestRequest::Active |
                           HitTestRequest::MouseMove);
    HitTestResult result(view->windowToContents(m_currentMousePosition));
    layer->hitTest(request, result);
    updateSelectionForMouseDrag(result.innerNode(), result.localPoint());
}

void EventHandler::updateSelectionForMouseDrag(Node* targetNode, const IntPoint& localPoint)
{
    if (!m_mouseDownMayStartSelect)
        return;

    if (!targetNode)
        return;

    RenderObject* targetRenderer = targetNode->renderer();
    if (!targetRenderer)
        return;
        
    if (!canMouseDragExtendSelect(targetNode))
        return;

    VisiblePosition targetPosition(targetRenderer->positionForPoint(localPoint));

    // Don't modify the selection if we're not on a node.
    if (targetPosition.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in handleMousePressEvent, but not if the mouse press was on an existing selection.
    VisibleSelection newSelection = m_frame->selection()->selection();

#if ENABLE(SVG)
    // Special case to limit selection to the containing block for SVG text.
    // FIXME: Isn't there a better non-SVG-specific way to do this?
    if (Node* selectionBaseNode = newSelection.base().node())
        if (RenderObject* selectionBaseRenderer = selectionBaseNode->renderer())
            if (selectionBaseRenderer->isSVGText())
                if (targetNode->renderer()->containingBlock() != selectionBaseRenderer->containingBlock())
                    return;
#endif

    if (!m_beganSelectingText) {
        m_beganSelectingText = true;
        newSelection = VisibleSelection(targetPosition);
    }

    newSelection.setExtent(targetPosition);
    if (m_frame->selectionGranularity() != CharacterGranularity)
        newSelection.expandUsingGranularity(m_frame->selectionGranularity());

    if (m_frame->shouldChangeSelection(newSelection)) {
        m_frame->selection()->setLastChangeWasHorizontalExtension(false);
        m_frame->selection()->setSelection(newSelection);
    }
}
#endif // ENABLE(DRAG_SUPPORT)
    
bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults& event)
{
    if (eventLoopHandleMouseUp(event))
        return true;
    
    // If this was the first click in the window, we don't even want to clear the selection.
    // This case occurs when the user clicks on a draggable element, since we have to process
    // the mouse down and drag events to see if we might start a drag.  For other first clicks
    // in a window, we just don't acceptFirstMouse, and the whole down-drag-up sequence gets
    // ignored upstream of this layer.
    return eventActivatedView(event.event());
}    

bool EventHandler::handleMouseReleaseEvent(const MouseEventWithHitTestResults& event)
{
    if (m_autoscrollInProgress)
        stopAutoscrollTimer();

    if (handleMouseUp(event))
        return true;

    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    m_frame->selection()->setCaretBlinkingSuspended(false);
    m_mousePressed = false;
    m_capturesDragging = false;
#if ENABLE(DRAG_SUPPORT)
    m_mouseDownMayStartDrag = false;
#endif
    m_mouseDownMayStartSelect = false;
    m_mouseDownMayStartAutoscroll = false;
    m_mouseDownWasInSubframe = false;
  
    bool handled = false;

    // Clear the selection if the mouse didn't move after the last mouse
    // press and it's not a context menu click.  We do this so when clicking
    // on the selection, the selection goes away.  However, if we are
    // editing, place the caret.
    if (m_mouseDownWasSingleClickInSelection && !m_beganSelectingText
#if ENABLE(DRAG_SUPPORT)
            && m_dragStartPos == event.event().pos()
#endif
            && m_frame->selection()->isRange()
            && event.event().button() != RightButton) {
        VisibleSelection newSelection;
        Node *node = event.targetNode();
        bool caretBrowsing = m_frame->settings()->caretBrowsingEnabled();
        if (node && (caretBrowsing || node->isContentEditable()) && node->renderer()) {
            VisiblePosition pos = node->renderer()->positionForPoint(event.localPoint());
            newSelection = VisibleSelection(pos);
        }
        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selection()->setSelection(newSelection);

        handled = true;
    }

    m_frame->notifyRendererOfSelectionChange(true);

    m_frame->selection()->selectFrameElementInParentIfFullySelected();

    return handled;
}

void EventHandler::handleAutoscroll(RenderObject* renderer)
{
    // We don't want to trigger the autoscroll or the panScroll if it's already active
    if (m_autoscrollTimer.isActive())
        return;     

    setAutoscrollRenderer(renderer);

#if ENABLE(PAN_SCROLLING)
    if (m_panScrollInProgress) {
        m_panScrollStartPos = currentMousePosition();
        if (FrameView* view = m_frame->view())
            view->addPanScrollIcon(m_panScrollStartPos);
        // If we're not in the top frame we notify it that we doing a panScroll.
        if (Page* page = m_frame->page()) {
            Frame* mainFrame = page->mainFrame();
            if (m_frame != mainFrame)
                mainFrame->eventHandler()->setPanScrollInProgress(true);
        }
    }
#endif

    startAutoscrollTimer();
}

void EventHandler::autoscrollTimerFired(Timer<EventHandler>*)
{
    RenderObject* r = autoscrollRenderer();
    if (!r || !r->isBox()) {
        stopAutoscrollTimer();
        return;
    }

    if (m_autoscrollInProgress) {
        if (!m_mousePressed) {
            stopAutoscrollTimer();
            return;
        }
        toRenderBox(r)->autoscroll();
    } else {
        // we verify that the main frame hasn't received the order to stop the panScroll
        if (Page* page = m_frame->page()) {
            if (!page->mainFrame()->eventHandler()->panScrollInProgress()) {
                stopAutoscrollTimer();
                return;
            }
        }
#if ENABLE(PAN_SCROLLING)
        updatePanScrollState();
        toRenderBox(r)->panScroll(m_panScrollStartPos);
#endif
    }
}

#if ENABLE(PAN_SCROLLING)

void EventHandler::updatePanScrollState()
{
    FrameView* view = m_frame->view();
    if (!view)
        return;

    // At the original click location we draw a 4 arrowed icon. Over this icon there won't be any scroll
    // So we don't want to change the cursor over this area
    bool east = m_panScrollStartPos.x() < (m_currentMousePosition.x() - ScrollView::noPanScrollRadius);
    bool west = m_panScrollStartPos.x() > (m_currentMousePosition.x() + ScrollView::noPanScrollRadius);
    bool north = m_panScrollStartPos.y() > (m_currentMousePosition.y() + ScrollView::noPanScrollRadius);
    bool south = m_panScrollStartPos.y() < (m_currentMousePosition.y() - ScrollView::noPanScrollRadius);
         
    if ((east || west || north || south) && m_panScrollButtonPressed)
        m_springLoadedPanScrollInProgress = true;

    if (north) {
        if (east)
            view->setCursor(northEastPanningCursor());
        else if (west)
            view->setCursor(northWestPanningCursor());
        else
            view->setCursor(northPanningCursor());
    } else if (south) {
        if (east)
            view->setCursor(southEastPanningCursor());
        else if (west)
            view->setCursor(southWestPanningCursor());
        else
            view->setCursor(southPanningCursor());
    } else if (east)
        view->setCursor(eastPanningCursor());
    else if (west)
        view->setCursor(westPanningCursor());
    else
        view->setCursor(middlePanningCursor());
}

#endif  // ENABLE(PAN_SCROLLING)

RenderObject* EventHandler::autoscrollRenderer() const
{
    return m_autoscrollRenderer;
}

void EventHandler::updateAutoscrollRenderer()
{
    if (!m_autoscrollRenderer)
        return;

    HitTestResult hitTest = hitTestResultAtPoint(m_panScrollStartPos, true);

    if (Node* nodeAtPoint = hitTest.innerNode())
        m_autoscrollRenderer = nodeAtPoint->renderer();

    while (m_autoscrollRenderer && (!m_autoscrollRenderer->isBox() || !toRenderBox(m_autoscrollRenderer)->canBeScrolledAndHasScrollableArea()))
        m_autoscrollRenderer = m_autoscrollRenderer->parent();
}

void EventHandler::setAutoscrollRenderer(RenderObject* renderer)
{
    m_autoscrollRenderer = renderer;
}

#if ENABLE(DRAG_SUPPORT)
void EventHandler::allowDHTMLDrag(bool& flagDHTML, bool& flagUA) const
{
    flagDHTML = false;
    flagUA = false;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    FrameView* view = m_frame->view();
    if (!view)
        return;

    unsigned mask = page->dragController()->delegateDragSourceAction(view->contentsToWindow(m_mouseDownPos));
    flagDHTML = (mask & DragSourceActionDHTML) != DragSourceActionNone;
    flagUA = ((mask & DragSourceActionImage) || (mask & DragSourceActionLink) || (mask & DragSourceActionSelection));
}
#endif // ENABLE(DRAG_SUPPORT)
    
HitTestResult EventHandler::hitTestResultAtPoint(const IntPoint& point, bool allowShadowContent, bool ignoreClipping, HitTestScrollbars testScrollbars)
{
    HitTestResult result(point);
    if (!m_frame->contentRenderer())
        return result;
    int hitType = HitTestRequest::ReadOnly | HitTestRequest::Active;
    if (ignoreClipping)
        hitType |= HitTestRequest::IgnoreClipping;
    m_frame->contentRenderer()->layer()->hitTest(HitTestRequest(hitType), result);

    while (true) {
        Node* n = result.innerNode();
        if (!result.isOverWidget() || !n || !n->renderer() || !n->renderer()->isWidget())
            break;
        RenderWidget* renderWidget = toRenderWidget(n->renderer());
        Widget* widget = renderWidget->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<HTMLFrameElementBase*>(n)->contentFrame();
        if (!frame || !frame->contentRenderer())
            break;
        FrameView* view = static_cast<FrameView*>(widget);
        IntPoint widgetPoint(result.localPoint().x() + view->scrollX() - renderWidget->borderLeft() - renderWidget->paddingLeft(), 
            result.localPoint().y() + view->scrollY() - renderWidget->borderTop() - renderWidget->paddingTop());
        HitTestResult widgetHitTestResult(widgetPoint);
        frame->contentRenderer()->layer()->hitTest(HitTestRequest(hitType), widgetHitTestResult);
        result = widgetHitTestResult;

        if (testScrollbars == ShouldHitTestScrollbars) {
            Scrollbar* eventScrollbar = view->scrollbarAtPoint(point);
            if (eventScrollbar)
                result.setScrollbar(eventScrollbar);
        }
    }
    
    // If our HitTestResult is not visible, then we started hit testing too far down the frame chain. 
    // Another hit test at the main frame level should get us the correct visible result.
    Frame* resultFrame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document()->frame() : 0;
    if (Page* page = m_frame->page()) {
        Frame* mainFrame = page->mainFrame();
        if (m_frame != mainFrame && resultFrame && resultFrame != mainFrame && !resultFrame->editor()->insideVisibleArea(result.point())) {
            FrameView* resultView = resultFrame->view();
            FrameView* mainView = mainFrame->view();
            if (resultView && mainView) {
                IntPoint windowPoint = resultView->contentsToWindow(result.point());
                IntPoint mainFramePoint = mainView->windowToContents(windowPoint);
                result = mainFrame->eventHandler()->hitTestResultAtPoint(mainFramePoint, allowShadowContent, ignoreClipping);
            }
        }
    }

    if (!allowShadowContent)
        result.setToNonShadowAncestor();

    return result;
}


void EventHandler::startAutoscrollTimer()
{
    m_autoscrollTimer.startRepeating(autoscrollInterval);
}

void EventHandler::stopAutoscrollTimer(bool rendererIsBeingDestroyed)
{
    if (m_autoscrollInProgress) {
        if (m_mouseDownWasInSubframe) {
            if (Frame* subframe = subframeForTargetNode(m_mousePressNode.get()))
                subframe->eventHandler()->stopAutoscrollTimer(rendererIsBeingDestroyed);
            return;
        }
    }

    if (autoscrollRenderer()) {
        if (!rendererIsBeingDestroyed && (m_autoscrollInProgress || m_panScrollInProgress))
            toRenderBox(autoscrollRenderer())->stopAutoscroll();
#if ENABLE(PAN_SCROLLING)
        if (m_panScrollInProgress) {
            if (FrameView* view = m_frame->view()) {
                view->removePanScrollIcon();
                view->setCursor(pointerCursor());
            }
        }
#endif

        setAutoscrollRenderer(0);
    }

    m_autoscrollTimer.stop();

    m_panScrollInProgress = false;
    m_springLoadedPanScrollInProgress = false;

    // If we're not in the top frame we notify it that we are not doing a panScroll any more.
    if (Page* page = m_frame->page()) {
        Frame* mainFrame = page->mainFrame();
        if (m_frame != mainFrame)
            mainFrame->eventHandler()->setPanScrollInProgress(false);
    }

    m_autoscrollInProgress = false;
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
    Node* node = m_frame->document()->focusedNode();
    if (!node)
        node = m_mousePressNode.get();
    
    if (node) {
        RenderObject* r = node->renderer();
        if (r && !r->isListBox())
            return r->enclosingBox()->scroll(direction, granularity);
    }

    return false;
}

bool EventHandler::scrollRecursively(ScrollDirection direction, ScrollGranularity granularity)
{
    bool handled = scrollOverflow(direction, granularity);
    if (!handled) {
        Frame* frame = m_frame;
        do {
            FrameView* view = frame->view();
            handled = view ? view->scroll(direction, granularity) : false;
            frame = frame->tree()->parent();
        } while (!handled && frame);
     }

    return handled;
}

IntPoint EventHandler::currentMousePosition() const
{
    return m_currentMousePosition;
}

Frame* subframeForHitTestResult(const MouseEventWithHitTestResults& hitTestResult)
{
    if (!hitTestResult.isOverWidget())
        return 0;
    return EventHandler::subframeForTargetNode(hitTestResult.targetNode());
}

Frame* EventHandler::subframeForTargetNode(Node* node)
{
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;

    Widget* widget = toRenderWidget(renderer)->widget();
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
    return frame->selection()->rootEditableElement() != node->rootEditableElement();
}

Cursor EventHandler::selectCursor(const MouseEventWithHitTestResults& event, Scrollbar* scrollbar)
{
    // During selection, use an I-beam no matter what we're over.
    // If you're capturing mouse events for a particular node, don't treat this as a selection.
    if (m_mousePressed && m_mouseDownMayStartSelect && m_frame->selection()->isCaretOrRange() && !m_capturingMouseEventsNode)
        return iBeamCursor();

    Node* node = event.targetNode();
    RenderObject* renderer = node ? node->renderer() : 0;
    RenderStyle* style = renderer ? renderer->style() : 0;

    if (renderer && renderer->isFrameSet()) {
        RenderFrameSet* frameSetRenderer = toRenderFrameSet(renderer);
        if (frameSetRenderer->canResizeRow(event.localPoint()))
            return rowResizeCursor();
        if (frameSetRenderer->canResizeColumn(event.localPoint()))
            return columnResizeCursor();
    }

    if (style && style->cursors()) {
        const CursorList* cursors = style->cursors();
        for (unsigned i = 0; i < cursors->size(); ++i) {
            CachedImage* cimage = (*cursors)[i].cursorImage.get();
            IntPoint hotSpot = (*cursors)[i].hotSpot;
            if (!cimage)
                continue;
            // Limit the size of cursors so that they cannot be used to cover UI elements in chrome.
            IntSize size = cimage->image()->size();
            if (size.width() > 128 || size.height() > 128)
                continue;
            // Do not let the hotspot be outside the bounds of the image. 
            if (hotSpot.x() < 0 || hotSpot.y() < 0 || hotSpot.x() > size.width() || hotSpot.y() > size.height())
                continue;
            if (cimage->image()->isNull())
                break;
            if (!cimage->errorOccurred())
                return Cursor(cimage->image(), hotSpot);
        }
    }

    switch (style ? style->cursor() : CURSOR_AUTO) {
        case CURSOR_AUTO: {
            bool editable = (node && node->isContentEditable());
            bool editableLinkEnabled = false;

            // If the link is editable, then we need to check the settings to see whether or not the link should be followed
            if (editable) {
                ASSERT(m_frame->settings());
                switch (m_frame->settings()->editableLinkBehavior()) {
                    default:
                    case EditableLinkDefaultBehavior:
                    case EditableLinkAlwaysLive:
                        editableLinkEnabled = true;
                        break;

                    case EditableLinkNeverLive:
                        editableLinkEnabled = false;
                        break;

                    case EditableLinkLiveWhenNotFocused:
                        editableLinkEnabled = nodeIsNotBeingEdited(node, m_frame) || event.event().shiftKey();
                        break;
                    
                    case EditableLinkOnlyLiveWithShiftKey:
                        editableLinkEnabled = event.event().shiftKey();
                        break;
                }
            }
            
            if ((event.isOverLink() || isSubmitImage(node)) && (!editable || editableLinkEnabled))
                return handCursor();
            bool inResizer = false;
            if (renderer) {
                if (RenderLayer* layer = renderer->enclosingLayer()) {
                    if (FrameView* view = m_frame->view())
                        inResizer = layer->isPointInResizeControl(view->windowToContents(event.event().pos()));
                }
            }
            if ((editable || (renderer && renderer->isText() && node->canStartSelection())) && !inResizer && !scrollbar)
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
            return notAllowedCursor();
        case CURSOR_DEFAULT:
            return pointerCursor();
        case CURSOR_WEBKIT_ZOOM_IN:
            return zoomInCursor();
        case CURSOR_WEBKIT_ZOOM_OUT:
            return zoomOutCursor();
        case CURSOR_WEBKIT_GRAB:
            return grabCursor();
        case CURSOR_WEBKIT_GRABBING:
            return grabbingCursor();
    }
    return pointerCursor();
}
    
static IntPoint documentPointForWindowPoint(Frame* frame, const IntPoint& windowPoint)
{
    FrameView* view = frame->view();
    // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
    // Historically the code would just crash; this is clearly no worse than that.
    return view ? view->windowToContents(windowPoint) : windowPoint;
}

bool EventHandler::handleMousePressEvent(const PlatformMouseEvent& mouseEvent)
{
    RefPtr<FrameView> protector(m_frame->view());

    m_mousePressed = true;
    m_capturesDragging = true;
    m_currentMousePosition = mouseEvent.pos();
    m_mouseDownTimestamp = mouseEvent.timestamp();
#if ENABLE(DRAG_SUPPORT)
    m_mouseDownMayStartDrag = false;
#endif
    m_mouseDownMayStartSelect = false;
    m_mouseDownMayStartAutoscroll = false;
    if (FrameView* view = m_frame->view())
        m_mouseDownPos = view->windowToContents(mouseEvent.pos());
    else {
        invalidateClick();
        return false;
    }
    m_mouseDownWasInSubframe = false;

    HitTestRequest request(HitTestRequest::Active);
    // Save the document point we generate in case the window coordinate is invalidated by what happens 
    // when we dispatch the event.
    IntPoint documentPoint = documentPointForWindowPoint(m_frame, mouseEvent.pos());
    MouseEventWithHitTestResults mev = m_frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);

    if (!mev.targetNode()) {
        invalidateClick();
        return false;
    }

    m_mousePressNode = mev.targetNode();

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page()) {
        InspectorController* inspector = page->inspectorController();
        if (inspector && inspector->enabled() && inspector->searchingForNodeInPage()) {
            inspector->handleMousePressOnNode(m_mousePressNode.get());
            invalidateClick();
            return true;
        }
    }
#endif

    Frame* subframe = subframeForHitTestResult(mev);
    if (subframe && passMousePressEventToSubframe(mev, subframe)) {
        // Start capturing future events for this frame.  We only do this if we didn't clear
        // the m_mousePressed flag, which may happen if an AppKit widget entered a modal event loop.
        m_capturesDragging = subframe->eventHandler()->capturesDragging();
        if (m_mousePressed && m_capturesDragging)
            m_capturingMouseEventsNode = mev.targetNode();
        invalidateClick();
        return true;
    }

#if ENABLE(PAN_SCROLLING)
    Page* page = m_frame->page();
    if (page && page->mainFrame()->eventHandler()->panScrollInProgress() || m_autoscrollInProgress) {
        stopAutoscrollTimer();
        invalidateClick();
        return true;
    }

    if (mouseEvent.button() == MiddleButton && !mev.isOverLink()) {
        RenderObject* renderer = mev.targetNode()->renderer();

        while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeScrolledAndHasScrollableArea())) {
            if (!renderer->parent() && renderer->node() == renderer->document() && renderer->document()->ownerElement())
                renderer = renderer->document()->ownerElement()->renderer();
            else
                renderer = renderer->parent();
        }

        if (renderer) {
            m_panScrollInProgress = true;
            m_panScrollButtonPressed = true;
            handleAutoscroll(renderer);
            invalidateClick();
            return true;
        }
    }
#endif

    m_clickCount = mouseEvent.clickCount();
    m_clickNode = mev.targetNode();

    if (FrameView* view = m_frame->view()) {
        RenderLayer* layer = m_clickNode->renderer() ? m_clickNode->renderer()->enclosingLayer() : 0;
        IntPoint p = view->windowToContents(mouseEvent.pos());
        if (layer && layer->isPointInResizeControl(p)) {
            layer->setInResizeMode(true);
            m_resizeLayer = layer;
            m_offsetFromResizeCorner = layer->offsetFromResizeCorner(p);
            invalidateClick();
            return true;
        }
    }

    bool swallowEvent = dispatchMouseEvent(eventNames().mousedownEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);
    m_capturesDragging = !swallowEvent;

    // If the hit testing originally determined the event was in a scrollbar, refetch the MouseEventWithHitTestResults
    // in case the scrollbar widget was destroyed when the mouse event was handled.
    if (mev.scrollbar()) {
        const bool wasLastScrollBar = mev.scrollbar() == m_lastScrollbarUnderMouse.get();
        HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
        mev = m_frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);
        if (wasLastScrollBar && mev.scrollbar() != m_lastScrollbarUnderMouse.get())
            m_lastScrollbarUnderMouse = 0;
    }

    if (swallowEvent) {
        // scrollbars should get events anyway, even disabled controls might be scrollable
        Scrollbar* scrollbar = mev.scrollbar();

        updateLastScrollbarUnderMouse(scrollbar, true);

        if (scrollbar)
            passMousePressEventToScrollbar(mev, scrollbar);
    } else {
        // Refetch the event target node if it currently is the shadow node inside an <input> element.
        // If a mouse event handler changes the input element type to one that has a widget associated,
        // we'd like to EventHandler::handleMousePressEvent to pass the event to the widget and thus the
        // event target node can't still be the shadow node.
        if (mev.targetNode()->isShadowNode() && mev.targetNode()->shadowParentNode()->hasTagName(inputTag)) {
            HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
            mev = m_frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);
        }

        FrameView* view = m_frame->view();
        Scrollbar* scrollbar = view ? view->scrollbarAtPoint(mouseEvent.pos()) : 0;
        if (!scrollbar)
            scrollbar = mev.scrollbar();

        updateLastScrollbarUnderMouse(scrollbar, true);

        if (scrollbar && passMousePressEventToScrollbar(mev, scrollbar))
            swallowEvent = true;
        else
            swallowEvent = handleMousePressEvent(mev);
    }

    return swallowEvent;
}

// This method only exists for platforms that don't know how to deliver 
bool EventHandler::handleMouseDoubleClickEvent(const PlatformMouseEvent& mouseEvent)
{
    RefPtr<FrameView> protector(m_frame->view());

    // We get this instead of a second mouse-up 
    m_mousePressed = false;
    m_currentMousePosition = mouseEvent.pos();

    HitTestRequest request(HitTestRequest::Active);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseEvent);
    Frame* subframe = subframeForHitTestResult(mev);
    if (subframe && passMousePressEventToSubframe(mev, subframe)) {
        m_capturingMouseEventsNode = 0;
        return true;
    }

    m_clickCount = mouseEvent.clickCount();
    bool swallowMouseUpEvent = dispatchMouseEvent(eventNames().mouseupEvent, mev.targetNode(), true, m_clickCount, mouseEvent, false);

    bool swallowClickEvent = false;
    // Don't ever dispatch click events for right clicks
    if (mouseEvent.button() != RightButton && mev.targetNode() == m_clickNode)
        swallowClickEvent = dispatchMouseEvent(eventNames().clickEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    if (m_lastScrollbarUnderMouse)
        swallowMouseUpEvent = m_lastScrollbarUnderMouse->mouseUp();
            
    bool swallowMouseReleaseEvent = false;
    if (!swallowMouseUpEvent)
        swallowMouseReleaseEvent = handleMouseReleaseEvent(mev);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

bool EventHandler::mouseMoved(const PlatformMouseEvent& event)
{
    HitTestResult hoveredNode = HitTestResult(IntPoint());
    bool result = handleMouseMoveEvent(event, &hoveredNode);

    Page* page = m_frame->page();
    if (!page)
        return result;

    hoveredNode.setToNonShadowAncestor();
    page->chrome()->mouseDidMoveOverElement(hoveredNode, event.modifierFlags());
    page->chrome()->setToolTip(hoveredNode);
    return result;
}

bool EventHandler::handleMouseMoveEvent(const PlatformMouseEvent& mouseEvent, HitTestResult* hoveredNode)
{
    // in Radar 3703768 we saw frequent crashes apparently due to the
    // part being null here, which seems impossible, so check for nil
    // but also assert so that we can try to figure this out in debug
    // builds, if it happens.
    ASSERT(m_frame);
    if (!m_frame)
        return false;

    RefPtr<FrameView> protector(m_frame->view());
    m_currentMousePosition = mouseEvent.pos();

    if (m_hoverTimer.isActive())
        m_hoverTimer.stop();

#if ENABLE(SVG)
    if (m_svgPan) {
        static_cast<SVGDocument*>(m_frame->document())->updatePan(m_currentMousePosition);
        return true;
    }
#endif

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(eventNames().mousemoveEvent, m_frameSetBeingResized.get(), false, 0, mouseEvent, false);

    // Send events right to a scrollbar if the mouse is pressed.
    if (m_lastScrollbarUnderMouse && m_mousePressed)
        return m_lastScrollbarUnderMouse->mouseMoved(mouseEvent);

    // Treat mouse move events while the mouse is pressed as "read-only" in prepareMouseEvent
    // if we are allowed to select.
    // This means that :hover and :active freeze in the state they were in when the mouse
    // was pressed, rather than updating for nodes the mouse moves over as you hold the mouse down.
    int hitType = HitTestRequest::MouseMove;
    if (m_mousePressed && m_mouseDownMayStartSelect)
        hitType |= HitTestRequest::ReadOnly;
    if (m_mousePressed)
        hitType |= HitTestRequest::Active;
    HitTestRequest request(hitType);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseEvent);
    if (hoveredNode)
        *hoveredNode = mev.hitTestResult();

    Scrollbar* scrollbar = 0;

    if (m_resizeLayer && m_resizeLayer->inResizeMode())
        m_resizeLayer->resize(mouseEvent, m_offsetFromResizeCorner);
    else {
        if (FrameView* view = m_frame->view())
            scrollbar = view->scrollbarAtPoint(mouseEvent.pos());

        if (!scrollbar)
            scrollbar = mev.scrollbar();

        updateLastScrollbarUnderMouse(scrollbar, !m_mousePressed);
    }

    bool swallowEvent = false;
    RefPtr<Frame> newSubframe = m_capturingMouseEventsNode.get() ? subframeForTargetNode(m_capturingMouseEventsNode.get()) : subframeForHitTestResult(mev);
 
    // We want mouseouts to happen first, from the inside out.  First send a move event to the last subframe so that it will fire mouseouts.
    if (m_lastMouseMoveEventSubframe && m_lastMouseMoveEventSubframe->tree()->isDescendantOf(m_frame) && m_lastMouseMoveEventSubframe != newSubframe)
        passMouseMoveEventToSubframe(mev, m_lastMouseMoveEventSubframe.get());

    if (newSubframe) {
        // Update over/out state before passing the event to the subframe.
        updateMouseEventTargetNode(mev.targetNode(), mouseEvent, true);
        
        // Event dispatch in updateMouseEventTargetNode may have caused the subframe of the target
        // node to be detached from its FrameView, in which case the event should not be passed.
        if (newSubframe->view())
            swallowEvent |= passMouseMoveEventToSubframe(mev, newSubframe.get(), hoveredNode);
    } else {
        if (scrollbar && !m_mousePressed)
            scrollbar->mouseMoved(mouseEvent); // Handle hover effects on platforms that support visual feedback on scrollbar hovering.
        if (Page* page = m_frame->page()) {
            if ((!m_resizeLayer || !m_resizeLayer->inResizeMode()) && !page->mainFrame()->eventHandler()->panScrollInProgress()) {
                if (FrameView* view = m_frame->view())
                    view->setCursor(selectCursor(mev, scrollbar));
            }
        }
    }
    
    m_lastMouseMoveEventSubframe = newSubframe;

    if (swallowEvent)
        return true;
    
    swallowEvent = dispatchMouseEvent(eventNames().mousemoveEvent, mev.targetNode(), false, 0, mouseEvent, true);
#if ENABLE(DRAG_SUPPORT)
    if (!swallowEvent)
        swallowEvent = handleMouseDraggedEvent(mev);
#endif // ENABLE(DRAG_SUPPORT)

    return swallowEvent;
}

void EventHandler::invalidateClick()
{
    m_clickCount = 0;
    m_clickNode = 0;
}

bool EventHandler::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    RefPtr<FrameView> protector(m_frame->view());

#if ENABLE(PAN_SCROLLING)
    if (mouseEvent.button() == MiddleButton)
        m_panScrollButtonPressed = false;
    if (m_springLoadedPanScrollInProgress)
       stopAutoscrollTimer();
#endif

    m_mousePressed = false;
    m_currentMousePosition = mouseEvent.pos();

#if ENABLE(SVG)
    if (m_svgPan) {
        m_svgPan = false;
        static_cast<SVGDocument*>(m_frame->document())->updatePan(m_currentMousePosition);
        return true;
    }
#endif

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(eventNames().mouseupEvent, m_frameSetBeingResized.get(), true, m_clickCount, mouseEvent, false);

    if (m_lastScrollbarUnderMouse) {
        invalidateClick();
        return m_lastScrollbarUnderMouse->mouseUp();
    }

    HitTestRequest request(HitTestRequest::MouseUp);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseEvent);
    Frame* subframe = m_capturingMouseEventsNode.get() ? subframeForTargetNode(m_capturingMouseEventsNode.get()) : subframeForHitTestResult(mev);
    if (subframe && passMouseReleaseEventToSubframe(mev, subframe)) {
        m_capturingMouseEventsNode = 0;
        return true;
    }

    bool swallowMouseUpEvent = dispatchMouseEvent(eventNames().mouseupEvent, mev.targetNode(), true, m_clickCount, mouseEvent, false);

    // Don't ever dispatch click events for right clicks
    bool swallowClickEvent = false;
    if (m_clickCount > 0 && mouseEvent.button() != RightButton && mev.targetNode() == m_clickNode)
        swallowClickEvent = dispatchMouseEvent(eventNames().clickEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

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

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::dispatchDragEvent(const AtomicString& eventType, Node* dragTarget, const PlatformMouseEvent& event, Clipboard* clipboard)
{
    FrameView* view = m_frame->view();

    // FIXME: We might want to dispatch a dragleave even if the view is gone.
    if (!view)
        return false;

    view->resetDeferredRepaintDelay();
    IntPoint contentsPos = view->windowToContents(event.pos());

    RefPtr<MouseEvent> me = MouseEvent::create(eventType,
        true, true, m_frame->document()->defaultView(),
        0, event.globalX(), event.globalY(), contentsPos.x(), contentsPos.y(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        0, 0, clipboard);

    ExceptionCode ec;
    dragTarget->dispatchEvent(me.get(), ec);
    return me->defaultPrevented();
}

bool EventHandler::updateDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;

    if (!m_frame->view())
        return false;

    HitTestRequest request(HitTestRequest::ReadOnly);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, event);

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
        if (newTarget) {
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(eventNames().dragenterEvent, newTarget, event, clipboard);
        }

        if (m_dragTarget) {
            Frame* frame = (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag)) 
                            ? static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame() : 0;
            if (frame)
                accept = frame->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                dispatchDragEvent(eventNames().dragleaveEvent, m_dragTarget.get(), event, clipboard);
        }
    } else {
        if (newTarget) {
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(eventNames().dragoverEvent, newTarget, event, clipboard);
        }
    }
    m_dragTarget = newTarget;

    return accept;
}

void EventHandler::cancelDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    if (m_dragTarget) {
        Frame* frame = (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag)) 
                        ? static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame() : 0;
        if (frame)
            frame->eventHandler()->cancelDragAndDrop(event, clipboard);
        else
            dispatchDragEvent(eventNames().dragleaveEvent, m_dragTarget.get(), event, clipboard);
    }
    clearDragState();
}

bool EventHandler::performDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;
    if (m_dragTarget) {
        Frame* frame = (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag)) 
                        ? static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame() : 0;
        if (frame)
            accept = frame->eventHandler()->performDragAndDrop(event, clipboard);
        else
            accept = dispatchDragEvent(eventNames().dropEvent, m_dragTarget.get(), event, clipboard);
    }
    clearDragState();
    return accept;
}

void EventHandler::clearDragState()
{
    m_dragTarget = 0;
    m_capturingMouseEventsNode = 0;
#if PLATFORM(MAC)
    m_sendingEventToSubview = false;
#endif
}
#endif // ENABLE(DRAG_SUPPORT)

void EventHandler::setCapturingMouseEventsNode(PassRefPtr<Node> n)
{
    m_capturingMouseEventsNode = n;
}

MouseEventWithHitTestResults EventHandler::prepareMouseEvent(const HitTestRequest& request, const PlatformMouseEvent& mev)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());
    
    return m_frame->document()->prepareMouseEvent(request, documentPointForWindowPoint(m_frame, mev.pos()), mev);
}

#if ENABLE(SVG)
static inline SVGElementInstance* instanceAssociatedWithShadowTreeElement(Node* referenceNode)
{
    if (!referenceNode || !referenceNode->isSVGElement())
        return 0;

    Node* shadowTreeElement = referenceNode->shadowTreeRootNode();
    if (!shadowTreeElement)
        return 0;

    Node* shadowTreeParentElement = shadowTreeElement->shadowParentNode();
    if (!shadowTreeParentElement)
        return 0;

    ASSERT(shadowTreeParentElement->hasTagName(useTag));
    return static_cast<SVGUseElement*>(shadowTreeParentElement)->instanceForShadowTreeElement(referenceNode);
}
#endif

void EventHandler::updateMouseEventTargetNode(Node* targetNode, const PlatformMouseEvent& mouseEvent, bool fireMouseOverOut)
{
    Node* result = targetNode;
    
    // If we're capturing, we always go right to that node.
    if (m_capturingMouseEventsNode)
        result = m_capturingMouseEventsNode.get();
    else {
        // If the target node is a text node, dispatch on the parent node - rdar://4196646
        if (result && result->isTextNode())
            result = result->parentNode();
        if (result)
            result = result->shadowAncestorNode();
    }
    m_nodeUnderMouse = result;
#if ENABLE(SVG)
    m_instanceUnderMouse = instanceAssociatedWithShadowTreeElement(result);

    // <use> shadow tree elements may have been recloned, update node under mouse in any case
    if (m_lastInstanceUnderMouse) {
        SVGElement* lastCorrespondingElement = m_lastInstanceUnderMouse->correspondingElement();
        SVGElement* lastCorrespondingUseElement = m_lastInstanceUnderMouse->correspondingUseElement();

        if (lastCorrespondingElement && lastCorrespondingUseElement) {
            HashSet<SVGElementInstance*> instances = lastCorrespondingElement->instancesForElement();

            // Locate the recloned shadow tree element for our corresponding instance
            HashSet<SVGElementInstance*>::iterator end = instances.end();
            for (HashSet<SVGElementInstance*>::iterator it = instances.begin(); it != end; ++it) {
                SVGElementInstance* instance = (*it);
                ASSERT(instance->correspondingElement() == lastCorrespondingElement);

                if (instance == m_lastInstanceUnderMouse)
                    continue;

                if (instance->correspondingUseElement() != lastCorrespondingUseElement)
                    continue;

                SVGElement* shadowTreeElement = instance->shadowTreeElement();
                if (!shadowTreeElement->inDocument() || m_lastNodeUnderMouse == shadowTreeElement)
                    continue;

                m_lastNodeUnderMouse = shadowTreeElement;
                m_lastInstanceUnderMouse = instance;
                break;
            }
        }
    }
#endif

    // Fire mouseout/mouseover if the mouse has shifted to a different node.
    if (fireMouseOverOut) {
        if (m_lastNodeUnderMouse && m_lastNodeUnderMouse->document() != m_frame->document()) {
            m_lastNodeUnderMouse = 0;
            m_lastScrollbarUnderMouse = 0;
#if ENABLE(SVG)
            m_lastInstanceUnderMouse = 0;
#endif
        }

        if (m_lastNodeUnderMouse != m_nodeUnderMouse) {
            // send mouseout event to the old node
            if (m_lastNodeUnderMouse)
                m_lastNodeUnderMouse->dispatchMouseEvent(mouseEvent, eventNames().mouseoutEvent, 0, m_nodeUnderMouse.get());
            // send mouseover event to the new node
            if (m_nodeUnderMouse)
                m_nodeUnderMouse->dispatchMouseEvent(mouseEvent, eventNames().mouseoverEvent, 0, m_lastNodeUnderMouse.get());
        }
        m_lastNodeUnderMouse = m_nodeUnderMouse;
#if ENABLE(SVG)
        m_lastInstanceUnderMouse = instanceAssociatedWithShadowTreeElement(m_nodeUnderMouse.get());
#endif
    }
}

bool EventHandler::dispatchMouseEvent(const AtomicString& eventType, Node* targetNode, bool /*cancelable*/, int clickCount, const PlatformMouseEvent& mouseEvent, bool setUnder)
{
    if (FrameView* view = m_frame->view())
        view->resetDeferredRepaintDelay();

    updateMouseEventTargetNode(targetNode, mouseEvent, setUnder);

    bool swallowEvent = false;

    if (m_nodeUnderMouse)
        swallowEvent = m_nodeUnderMouse->dispatchMouseEvent(mouseEvent, eventType, clickCount);
    
    if (!swallowEvent && eventType == eventNames().mousedownEvent) {
        // The layout needs to be up to date to determine if an element is focusable.
        m_frame->document()->updateLayoutIgnorePendingStylesheets();

        // Blur current focus node when a link/button is clicked; this
        // is expected by some sites that rely on onChange handlers running
        // from form fields before the button click is processed.
        Node* node = m_nodeUnderMouse.get();
        RenderObject* renderer = node ? node->renderer() : 0;

        // Walk up the render tree to search for a node to focus.
        // Walking up the DOM tree wouldn't work for shadow trees, like those behind the engine-based text fields.
        while (renderer) {
            node = renderer->node();
            if (node && node->isFocusable()) {
                // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus a 
                // node on mouse down if it's selected and inside a focused node. It will be 
                // focused if the user does a mouseup over it, however, because the mouseup
                // will set a selection inside it, which will call setFocuseNodeIfNeeded.
                ExceptionCode ec = 0;
                Node* n = node->isShadowNode() ? node->shadowParentNode() : node;
                if (m_frame->selection()->isRange() && 
                    m_frame->selection()->toNormalizedRange()->compareNode(n, ec) == Range::NODE_INSIDE &&
                    n->isDescendantOf(m_frame->document()->focusedNode()))
                    return false;
                    
                break;
            }
            
            renderer = renderer->parent();
        }

        // If focus shift is blocked, we eat the event.  Note we should never clear swallowEvent
        // if the page already set it (e.g., by canceling default behavior).
        if (Page* page = m_frame->page()) {
            if (node && node->isMouseFocusable()) {
                if (!page->focusController()->setFocusedNode(node, m_frame))
                    swallowEvent = true;
            } else if (!node || !node->focused()) {
                if (!page->focusController()->setFocusedNode(0, m_frame))
                    swallowEvent = true;
            }
        }
    }

    return swallowEvent;
}

#if !PLATFORM(GTK)
bool EventHandler::shouldTurnVerticalTicksIntoHorizontal(const HitTestResult& result) const
{
    return false;
}
#endif

bool EventHandler::handleWheelEvent(PlatformWheelEvent& e)
{
    Document* doc = m_frame->document();

    RenderObject* docRenderer = doc->renderer();
    if (!docRenderer)
        return false;
    
    RefPtr<FrameView> protector(m_frame->view());

    FrameView* view = m_frame->view();
    if (!view)
        return false;
    IntPoint vPoint = view->windowToContents(e.pos());

    Node* node;
    bool isOverWidget;
    bool didSetLatchedNode = false;

    HitTestRequest request(HitTestRequest::ReadOnly);
    HitTestResult result(vPoint);
    doc->renderView()->layer()->hitTest(request, result);

    if (m_useLatchedWheelEventNode) {
        if (!m_latchedWheelEventNode) {
            m_latchedWheelEventNode = result.innerNode();
            m_widgetIsLatched = result.isOverWidget();
            didSetLatchedNode = true;
        }

        node = m_latchedWheelEventNode.get();
        isOverWidget = m_widgetIsLatched;
    } else {
        if (m_latchedWheelEventNode)
            m_latchedWheelEventNode = 0;
        if (m_previousWheelScrolledNode)
            m_previousWheelScrolledNode = 0;

        node = result.innerNode();
        isOverWidget = result.isOverWidget();
    }

    if (shouldTurnVerticalTicksIntoHorizontal(result))
        e.turnVerticalTicksIntoHorizontal();

    if (node) {
        // Figure out which view to send the event to.
        RenderObject* target = node->renderer();
        
        if (isOverWidget && target && target->isWidget()) {
            Widget* widget = toRenderWidget(target)->widget();
            if (widget && passWheelEventToWidget(e, widget)) {
                e.accept();
                return true;
            }
        }

        node = node->shadowAncestorNode();
        node->dispatchWheelEvent(e);
        if (e.isAccepted())
            return true;

        // If we don't have a renderer, send the wheel event to the first node we find with a renderer.
        // This is needed for <option> and <optgroup> elements so that <select>s get a wheel scroll.
        while (node && !node->renderer())
            node = node->parent();

        if (node && node->renderer()) {
            // Just break up into two scrolls if we need to.  Diagonal movement on 
            // a MacBook pro is an example of a 2-dimensional mouse wheel event (where both deltaX and deltaY can be set).
            Node* stopNode = m_previousWheelScrolledNode.get();
            scrollAndAcceptEvent(e.deltaX(), ScrollLeft, ScrollRight, e, node, &stopNode);
            scrollAndAcceptEvent(e.deltaY(), ScrollUp, ScrollDown, e, node, &stopNode);
            if (!m_useLatchedWheelEventNode)
                m_previousWheelScrolledNode = stopNode;
        }
    }

    if (e.isAccepted())
        return true;

    view = m_frame->view();
    if (!view)
        return false;

    view->wheelEvent(e);
    return e.isAccepted();
}

#if ENABLE(CONTEXT_MENUS)
bool EventHandler::sendContextMenuEvent(const PlatformMouseEvent& event)
{
    Document* doc = m_frame->document();
    FrameView* v = m_frame->view();
    if (!v)
        return false;
    
    bool swallowEvent;
    IntPoint viewportPos = v->windowToContents(event.pos());
    HitTestRequest request(HitTestRequest::Active);
    MouseEventWithHitTestResults mev = doc->prepareMouseEvent(request, viewportPos, event);

    // Context menu events shouldn't select text in GTK+ applications or in Chromium.
    // FIXME: This should probably be configurable by embedders. Consider making it a WebPreferences setting.
    // See: https://bugs.webkit.org/show_bug.cgi?id=15279
#if !PLATFORM(GTK) && !PLATFORM(CHROMIUM)
    if (!m_frame->selection()->contains(viewportPos) && 
        // FIXME: In the editable case, word selection sometimes selects content that isn't underneath the mouse.
        // If the selection is non-editable, we do word selection to make it easier to use the contextual menu items
        // available for text selections.  But only if we're above text.
        (m_frame->selection()->isContentEditable() || (mev.targetNode() && mev.targetNode()->isTextNode()))) {
        m_mouseDownMayStartSelect = true; // context menu events are always allowed to perform a selection
        selectClosestWordOrLinkFromMouseEvent(mev);
    }
#endif

    swallowEvent = dispatchMouseEvent(eventNames().contextmenuEvent, mev.targetNode(), true, 0, event, true);
    
    return swallowEvent;
}
#endif // ENABLE(CONTEXT_MENUS)

void EventHandler::scheduleHoverStateUpdate()
{
    if (!m_hoverTimer.isActive())
        m_hoverTimer.startOneShot(0);
}

// Whether or not a mouse down can begin the creation of a selection.  Fires the selectStart event.
bool EventHandler::canMouseDownStartSelect(Node* node)
{
    if (!node || !node->renderer())
        return true;
    
    // Some controls and images can't start a select on a mouse down.
    if (!node->canStartSelection())
        return false;
            
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent()) {
        if (Node* node = curr->node())
            return node->dispatchEvent(Event::create(eventNames().selectstartEvent, true, true));
    }

    return true;
}

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::canMouseDragExtendSelect(Node* node)
{
    if (!node || !node->renderer())
        return true;
            
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent()) {
        if (Node* node = curr->node())
            return node->dispatchEvent(Event::create(eventNames().selectstartEvent, true, true));
    }

    return true;
}
#endif // ENABLE(DRAG_SUPPORT)

void EventHandler::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    m_frameSetBeingResized = frameSet;
}

void EventHandler::resizeLayerDestroyed()
{
    ASSERT(m_resizeLayer);
    m_resizeLayer = 0;
}

void EventHandler::hoverTimerFired(Timer<EventHandler>*)
{
    m_hoverTimer.stop();

    ASSERT(m_frame);
    ASSERT(m_frame->document());

    if (RenderView* renderer = m_frame->contentRenderer()) {
        if (FrameView* view = m_frame->view()) {
            HitTestRequest request(HitTestRequest::MouseMove);
            HitTestResult result(view->windowToContents(m_currentMousePosition));
            renderer->layer()->hitTest(request, result);
            m_frame->document()->updateStyleIfNeeded();
        }
    }
}

static Node* eventTargetNodeForDocument(Document* doc)
{
    if (!doc)
        return 0;
    Node* node = doc->focusedNode();
    if (!node && doc->isHTMLDocument())
        node = doc->body();
    if (!node)
        node = doc->documentElement();
    return node;
}

bool EventHandler::handleAccessKey(const PlatformKeyboardEvent& evt)
{
    // FIXME: Ignoring the state of Shift key is what neither IE nor Firefox do.
    // IE matches lower and upper case access keys regardless of Shift key state - but if both upper and
    // lower case variants are present in a document, the correct element is matched based on Shift key state.
    // Firefox only matches an access key if Shift is not pressed, and does that case-insensitively.
    ASSERT(!(accessKeyModifiers() & PlatformKeyboardEvent::ShiftKey));
    if ((evt.modifiers() & ~PlatformKeyboardEvent::ShiftKey) != accessKeyModifiers())
        return false;
    String key = evt.unmodifiedText();
    Element* elem = m_frame->document()->getElementByAccessKey(key.lower());
    if (!elem)
        return false;
    elem->accessKeyAction(false);
    return true;
}

#if !PLATFORM(MAC)
bool EventHandler::needsKeyboardEventDisambiguationQuirks() const
{
    return false;
}
#endif

bool EventHandler::keyEvent(const PlatformKeyboardEvent& initialKeyEvent)
{
#if ENABLE(PAN_SCROLLING)
    if (Page* page = m_frame->page()) {
        if (page->mainFrame()->eventHandler()->panScrollInProgress() || m_autoscrollInProgress) {
            // If a key is pressed while the autoscroll/panScroll is in progress then we want to stop
            if (initialKeyEvent.type() == PlatformKeyboardEvent::KeyDown || initialKeyEvent.type() == PlatformKeyboardEvent::RawKeyDown) 
                stopAutoscrollTimer();

            // If we were in autoscroll/panscroll mode, we swallow the key event
            return true;
        }
    }
#endif

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    RefPtr<Node> node = eventTargetNodeForDocument(m_frame->document());
    if (!node)
        return false;

    if (FrameView* view = m_frame->view())
        view->resetDeferredRepaintDelay();

    // FIXME: what is this doing here, in keyboard event handler?
    m_frame->loader()->resetMultipleFormSubmissionProtection();

    // In IE, access keys are special, they are handled after default keydown processing, but cannot be canceled - this is hard to match.
    // On Mac OS X, we process them before dispatching keydown, as the default keydown handler implements Emacs key bindings, which may conflict
    // with access keys. Then we dispatch keydown, but suppress its default handling.
    // On Windows, WebKit explicitly calls handleAccessKey() instead of dispatching a keypress event for WM_SYSCHAR messages.
    // Other platforms currently match either Mac or Windows behavior, depending on whether they send combined KeyDown events.
    bool matchedAnAccessKey = false;
    if (initialKeyEvent.type() == PlatformKeyboardEvent::KeyDown)
        matchedAnAccessKey = handleAccessKey(initialKeyEvent);

    // FIXME: it would be fair to let an input method handle KeyUp events before DOM dispatch.
    if (initialKeyEvent.type() == PlatformKeyboardEvent::KeyUp || initialKeyEvent.type() == PlatformKeyboardEvent::Char)
        return !node->dispatchKeyEvent(initialKeyEvent);

    bool backwardCompatibilityMode = needsKeyboardEventDisambiguationQuirks();

    ExceptionCode ec;
    PlatformKeyboardEvent keyDownEvent = initialKeyEvent;    
    if (keyDownEvent.type() != PlatformKeyboardEvent::RawKeyDown)
        keyDownEvent.disambiguateKeyDownEvent(PlatformKeyboardEvent::RawKeyDown, backwardCompatibilityMode);
    RefPtr<KeyboardEvent> keydown = KeyboardEvent::create(keyDownEvent, m_frame->document()->defaultView());
    if (matchedAnAccessKey)
        keydown->setDefaultPrevented(true);
    keydown->setTarget(node);

    if (initialKeyEvent.type() == PlatformKeyboardEvent::RawKeyDown) {
        node->dispatchEvent(keydown, ec);
        return keydown->defaultHandled() || keydown->defaultPrevented();
    }

    // Run input method in advance of DOM event handling.  This may result in the IM
    // modifying the page prior the keydown event, but this behaviour is necessary
    // in order to match IE:
    // 1. preventing default handling of keydown and keypress events has no effect on IM input;
    // 2. if an input method handles the event, its keyCode is set to 229 in keydown event.
    m_frame->editor()->handleInputMethodKeydown(keydown.get());
    
    bool handledByInputMethod = keydown->defaultHandled();
    
    if (handledByInputMethod) {
        keyDownEvent.setWindowsVirtualKeyCode(CompositionEventKeyCode);
        keydown = KeyboardEvent::create(keyDownEvent, m_frame->document()->defaultView());
        keydown->setTarget(node);
        keydown->setDefaultHandled();
    }

    node->dispatchEvent(keydown, ec);
    bool keydownResult = keydown->defaultHandled() || keydown->defaultPrevented();
    if (handledByInputMethod || (keydownResult && !backwardCompatibilityMode))
        return keydownResult;
    
    // Focus may have changed during keydown handling, so refetch node.
    // But if we are dispatching a fake backward compatibility keypress, then we pretend that the keypress happened on the original node.
    if (!keydownResult) {
        node = eventTargetNodeForDocument(m_frame->document());
        if (!node)
            return false;
    }

    PlatformKeyboardEvent keyPressEvent = initialKeyEvent;
    keyPressEvent.disambiguateKeyDownEvent(PlatformKeyboardEvent::Char, backwardCompatibilityMode);
    if (keyPressEvent.text().isEmpty())
        return keydownResult;
    RefPtr<KeyboardEvent> keypress = KeyboardEvent::create(keyPressEvent, m_frame->document()->defaultView());
    keypress->setTarget(node);
    if (keydownResult)
        keypress->setDefaultPrevented(true);
#if PLATFORM(MAC)
    keypress->keypressCommands() = keydown->keypressCommands();
#endif
    node->dispatchEvent(keypress, ec);

    return keydownResult || keypress->defaultPrevented() || keypress->defaultHandled();
}

void EventHandler::handleKeyboardSelectionMovement(KeyboardEvent* event)
{
    if (!event)
        return;
    
    String key = event->keyIdentifier();           
    bool isShifted = event->getModifierState("Shift");
    bool isOptioned = event->getModifierState("Alt");
    bool isCommanded = event->getModifierState("Meta");
    
    if (key == "Up") {
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::BACKWARD, (isCommanded) ? DocumentBoundary : LineGranularity, true);
        event->setDefaultHandled();
    }
    else if (key == "Down") { 
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::FORWARD, (isCommanded) ? DocumentBoundary : LineGranularity, true);
        event->setDefaultHandled();
    }
    else if (key == "Left") {
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::LEFT, (isCommanded) ? LineBoundary : (isOptioned) ? WordGranularity : CharacterGranularity, true);
        event->setDefaultHandled();
    }
    else if (key == "Right") {
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::RIGHT, (isCommanded) ? LineBoundary : (isOptioned) ? WordGranularity : CharacterGranularity, true);
        event->setDefaultHandled();
    }    
}
    
void EventHandler::defaultKeyboardEventHandler(KeyboardEvent* event)
{
    if (event->type() == eventNames().keydownEvent) {
        m_frame->editor()->handleKeyboardEvent(event);
        if (event->defaultHandled())
            return;
        if (event->keyIdentifier() == "U+0009")
            defaultTabEventHandler(event);

       // provides KB navigation and selection for enhanced accessibility users
       if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
           handleKeyboardSelectionMovement(event);       
    }
    if (event->type() == eventNames().keypressEvent) {
        m_frame->editor()->handleKeyboardEvent(event);
        if (event->defaultHandled())
            return;
        if (event->charCode() == ' ')
            defaultSpaceEventHandler(event);
    }
}

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::dragHysteresisExceeded(const FloatPoint& floatDragViewportLocation) const
{
    IntPoint dragViewportLocation((int)floatDragViewportLocation.x(), (int)floatDragViewportLocation.y());
    return dragHysteresisExceeded(dragViewportLocation);
}
    
bool EventHandler::dragHysteresisExceeded(const IntPoint& dragViewportLocation) const
{
    FrameView* view = m_frame->view();
    if (!view)
        return false;
    IntPoint dragLocation = view->windowToContents(dragViewportLocation);
    IntSize delta = dragLocation - m_mouseDownPos;
    
    int threshold = GeneralDragHysteresis;
    if (dragState().m_dragSrcIsImage)
        threshold = ImageDragHysteresis;
    else if (dragState().m_dragSrcIsLink)
        threshold = LinkDragHysteresis;
    else if (dragState().m_dragSrcInSelection)
        threshold = TextDragHysteresis;
    
    return abs(delta.width()) >= threshold || abs(delta.height()) >= threshold;
}
    
void EventHandler::freeClipboard()
{
    if (dragState().m_dragClipboard)
        dragState().m_dragClipboard->setAccessPolicy(ClipboardNumb);
}

bool EventHandler::shouldDragAutoNode(Node* node, const IntPoint& point) const
{
    if (!node || node->hasChildNodes() || !m_frame->view())
        return false;
    Page* page = m_frame->page();
    return page && page->dragController()->mayStartDragAtEventLocation(m_frame, point);
}
    
void EventHandler::dragSourceMovedTo(const PlatformMouseEvent& event)
{
    if (dragState().m_dragSrc && dragState().m_dragSrcMayBeDHTML)
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(eventNames().dragEvent, event);
}
    
void EventHandler::dragSourceEndedAt(const PlatformMouseEvent& event, DragOperation operation)
{
    if (dragState().m_dragSrc && dragState().m_dragSrcMayBeDHTML) {
        dragState().m_dragClipboard->setDestinationOperation(operation);
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(eventNames().dragendEvent, event);
    }
    freeClipboard();
    dragState().m_dragSrc = 0;
    // In case the drag was ended due to an escape key press we need to ensure
    // that consecutive mousemove events don't reinitiate the drag and drop.
    m_mouseDownMayStartDrag = false;
}
    
// returns if we should continue "default processing", i.e., whether eventhandler canceled
bool EventHandler::dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent& event)
{
    return !dispatchDragEvent(eventType, dragState().m_dragSrc.get(), event, dragState().m_dragClipboard.get());
}
    
bool EventHandler::handleDrag(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton || event.event().eventType() != MouseEventMoved) {
        // If we allowed the other side of the bridge to handle a drag
        // last time, then m_mousePressed might still be set. So we
        // clear it now to make sure the next move after a drag
        // doesn't look like a drag.
        m_mousePressed = false;
        return false;
    }
    
    if (eventLoopHandleMouseDragged(event))
        return true;
    
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    
    if (m_mouseDownMayStartDrag && !dragState().m_dragSrc) {
        allowDHTMLDrag(dragState().m_dragSrcMayBeDHTML, dragState().m_dragSrcMayBeUA);
        if (!dragState().m_dragSrcMayBeDHTML && !dragState().m_dragSrcMayBeUA)
            m_mouseDownMayStartDrag = false;     // no element is draggable
    }

    if (m_mouseDownMayStartDrag && !dragState().m_dragSrc) {
        // try to find an element that wants to be dragged
        HitTestRequest request(HitTestRequest::ReadOnly);
        HitTestResult result(m_mouseDownPos);
        m_frame->contentRenderer()->layer()->hitTest(request, result);
        Node* node = result.innerNode();
        if (node && node->renderer())
            dragState().m_dragSrc = node->renderer()->draggableNode(dragState().m_dragSrcMayBeDHTML, dragState().m_dragSrcMayBeUA,
                                                                    m_mouseDownPos.x(), m_mouseDownPos.y(), dragState().m_dragSrcIsDHTML);
        else
            dragState().m_dragSrc = 0;
        
        if (!dragState().m_dragSrc)
            m_mouseDownMayStartDrag = false;     // no element is draggable
        else {
            // remember some facts about this source, while we have a HitTestResult handy
            node = result.URLElement();
            dragState().m_dragSrcIsLink = node && node->isLink();
            
            node = result.innerNonSharedNode();
            dragState().m_dragSrcIsImage = node && node->renderer() && node->renderer()->isImage();
            
            dragState().m_dragSrcInSelection = m_frame->selection()->contains(m_mouseDownPos);
        }                
    }
    
    // For drags starting in the selection, the user must wait between the mousedown and mousedrag,
    // or else we bail on the dragging stuff and allow selection to occur
    if (m_mouseDownMayStartDrag && !dragState().m_dragSrcIsImage && dragState().m_dragSrcInSelection && event.event().timestamp() - m_mouseDownTimestamp < TextDragDelay) {
        m_mouseDownMayStartDrag = false;
        dragState().m_dragSrc = 0;
        // ...but if this was the first click in the window, we don't even want to start selection
        if (eventActivatedView(event.event()))
            m_mouseDownMayStartSelect = false;
    }
    
    if (!m_mouseDownMayStartDrag)
        return !mouseDownMayStartSelect() && !m_mouseDownMayStartAutoscroll;
    
    // We are starting a text/image/url drag, so the cursor should be an arrow
    if (FrameView* view = m_frame->view())
        view->setCursor(pointerCursor());
    
    if (!dragHysteresisExceeded(event.event().pos())) 
        return true;
    
    // Once we're past the hysteresis point, we don't want to treat this gesture as a click
    invalidateClick();
    
    DragOperation srcOp = DragOperationNone;      
    
    freeClipboard();    // would only happen if we missed a dragEnd.  Do it anyway, just
                        // to make sure it gets numbified
    dragState().m_dragClipboard = createDraggingClipboard();  
    
    if (dragState().m_dragSrcMayBeDHTML) {
        // Check to see if the is a DOM based drag, if it is get the DOM specified drag 
        // image and offset
        if (dragState().m_dragSrcIsDHTML) {
            if (RenderObject* renderer = dragState().m_dragSrc->renderer()) {
                // FIXME: This doesn't work correctly with transforms.
                FloatPoint absPos = renderer->localToAbsolute();
                IntSize delta = m_mouseDownPos - roundedIntPoint(absPos);
                dragState().m_dragClipboard->setDragImageElement(dragState().m_dragSrc.get(), IntPoint() + delta);
            } else {
                // The renderer has disappeared, this can happen if the onStartDrag handler has hidden
                // the element in some way.  In this case we just kill the drag.
                m_mouseDownMayStartDrag = false;
                goto cleanupDrag;
            }
        } 
        
        m_mouseDownMayStartDrag = dispatchDragSrcEvent(eventNames().dragstartEvent, m_mouseDown)
            && !m_frame->selection()->isInPasswordField();
        
        // Invalidate clipboard here against anymore pasteboard writing for security.  The drag
        // image can still be changed as we drag, but not the pasteboard data.
        dragState().m_dragClipboard->setAccessPolicy(ClipboardImageWritable);
        
        if (m_mouseDownMayStartDrag) {
            // gather values from DHTML element, if it set any
            dragState().m_dragClipboard->sourceOperation(srcOp);
            
            // Yuck, dragSourceMovedTo() can be called as a result of kicking off the drag with
            // dragImage!  Because of that dumb reentrancy, we may think we've not started the
            // drag when that happens.  So we have to assume it's started before we kick it off.
            dragState().m_dragClipboard->setDragHasStarted();
        }
    }
    
    if (m_mouseDownMayStartDrag) {
        Page* page = m_frame->page();
        DragController* dragController = page ? page->dragController() : 0;
        bool startedDrag = dragController && dragController->startDrag(m_frame, dragState().m_dragClipboard.get(), srcOp, event.event(), m_mouseDownPos, dragState().m_dragSrcIsDHTML);
        if (!startedDrag && dragState().m_dragSrcMayBeDHTML) {
            // Drag was canned at the last minute - we owe m_dragSrc a DRAGEND event
            dispatchDragSrcEvent(eventNames().dragendEvent, event.event());
            m_mouseDownMayStartDrag = false;
        }
    } 

cleanupDrag:
    if (!m_mouseDownMayStartDrag) {
        // something failed to start the drag, cleanup
        freeClipboard();
        dragState().m_dragSrc = 0;
    }
    
    // No more default handling (like selection), whether we're past the hysteresis bounds or not
    return true;
}
#endif // ENABLE(DRAG_SUPPORT)
  
bool EventHandler::handleTextInputEvent(const String& text, Event* underlyingEvent, bool isLineBreak, bool isBackTab)
{
    // Platforms should differentiate real commands like selectAll from text input in disguise (like insertNewline),
    // and avoid dispatching text input events from keydown default handlers.
    ASSERT(!underlyingEvent || !underlyingEvent->isKeyboardEvent() || static_cast<KeyboardEvent*>(underlyingEvent)->type() == eventNames().keypressEvent);

    if (!m_frame)
        return false;

    EventTarget* target;
    if (underlyingEvent)
        target = underlyingEvent->target();
    else
        target = eventTargetNodeForDocument(m_frame->document());
    if (!target)
        return false;
    
    if (FrameView* view = m_frame->view())
        view->resetDeferredRepaintDelay();

    RefPtr<TextEvent> event = TextEvent::create(m_frame->domWindow(), text);
    event->setUnderlyingEvent(underlyingEvent);
    event->setIsLineBreak(isLineBreak);
    event->setIsBackTab(isBackTab);
    ExceptionCode ec;
    target->dispatchEvent(event, ec);
    return event->defaultHandled();
}
    
    
#if !PLATFORM(MAC) && !PLATFORM(QT) && !PLATFORM(HAIKU)
bool EventHandler::invertSenseOfTabsToLinks(KeyboardEvent*) const
{
    return false;
}
#endif

bool EventHandler::tabsToLinks(KeyboardEvent* event) const
{
    Page* page = m_frame->page();
    if (!page)
        return false;

    if (page->chrome()->client()->tabsToLinks())
        return !invertSenseOfTabsToLinks(event);

    return invertSenseOfTabsToLinks(event);
}

void EventHandler::defaultTextInputEventHandler(TextEvent* event)
{
    String data = event->data();
    if (data == "\n") {
        if (event->isLineBreak()) {
            if (m_frame->editor()->insertLineBreak())
                event->setDefaultHandled();
        } else {
            if (m_frame->editor()->insertParagraphSeparator())
                event->setDefaultHandled();
        }
    } else {
        if (m_frame->editor()->insertTextWithoutSendingTextEvent(data, false, event))
            event->setDefaultHandled();
    }
}

#if PLATFORM(QT) || PLATFORM(MAC)

// These two platforms handle the space event in the platform-specific WebKit code.
// Eventually it would be good to eliminate that and use the code here instead, but
// the Qt version is inside an ifdef and the Mac version has some extra behavior
// so we can't unify everything yet.
void EventHandler::defaultSpaceEventHandler(KeyboardEvent*)
{
}

#else

void EventHandler::defaultSpaceEventHandler(KeyboardEvent* event)
{
    ScrollDirection direction = event->shiftKey() ? ScrollUp : ScrollDown;
    if (scrollOverflow(direction, ScrollByPage)) {
        event->setDefaultHandled();
        return;
    }

    FrameView* view = m_frame->view();
    if (!view)
        return;

    if (view->scroll(direction, ScrollByPage))
        event->setDefaultHandled();
}

#endif

void EventHandler::defaultTabEventHandler(KeyboardEvent* event)
{
    // We should only advance focus on tabs if no special modifier keys are held down.
    if (event->ctrlKey() || event->metaKey() || event->altGraphKey())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;
    if (!page->tabKeyCyclesThroughElements())
        return;

    FocusDirection focusDirection = event->shiftKey() ? FocusDirectionBackward : FocusDirectionForward;

    // Tabs can be used in design mode editing.
    if (m_frame->document()->inDesignMode())
        return;

    if (page->focusController()->advanceFocus(focusDirection, event))
        event->setDefaultHandled();
}

void EventHandler::capsLockStateMayHaveChanged()
{
    Document* d = m_frame->document();
    if (Node* node = d->focusedNode()) {
        if (RenderObject* r = node->renderer()) {
            if (r->isTextField())
                toRenderTextControlSingleLine(r)->capsLockStateMayHaveChanged();
        }
    }
}

void EventHandler::sendResizeEvent()
{
    m_frame->document()->dispatchWindowEvent(Event::create(eventNames().resizeEvent, false, false));
}

void EventHandler::sendScrollEvent()
{
    FrameView* v = m_frame->view();
    if (!v)
        return;
    v->setWasScrolledByUser(true);
    m_frame->document()->dispatchEvent(Event::create(eventNames().scrollEvent, true, false));
}

bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults& mev, Scrollbar* scrollbar)
{
    if (!scrollbar || !scrollbar->enabled())
        return false;
    return scrollbar->mouseDown(mev.event());
}

// If scrollbar (under mouse) is different from last, send a mouse exited. Set
// last to scrollbar if setLast is true; else set last to 0.
void EventHandler::updateLastScrollbarUnderMouse(Scrollbar* scrollbar, bool setLast)
{
    if (m_lastScrollbarUnderMouse != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse->mouseExited();
        m_lastScrollbarUnderMouse = setLast ? scrollbar : 0;
    }
}

}
