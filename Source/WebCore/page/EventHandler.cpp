/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "AutoscrollController.h"
#include "BackForwardController.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "CursorList.h"
#include "DocumentMarkerController.h"
#include "DragController.h"
#include "DragEvent.h"
#include "DragState.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "EventNames.h"
#include "FileList.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "HTMLDocument.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "InspectorInstrumentation.h"
#include "KeyboardEvent.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "Pasteboard.h"
#include "PlatformEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginDocument.h"
#include "Range.h"
#include "RenderFrameSet.h"
#include "RenderLayer.h"
#include "RenderListBox.h"
#include "RenderTextControlSingleLine.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceLoadObserver.h"
#include "RuntimeApplicationChecks.h"
#include "SVGDocument.h"
#include "SVGNames.h"
#include "ScrollAnimator.h"
#include "ScrollLatchingState.h"
#include "Scrollbar.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpatialNavigation.h"
#include "StaticPasteboard.h"
#include "StyleCachedImage.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "UserGestureIndicator.h"
#include "UserTypingGestureIndicator.h"
#include "ValidationMessageClient.h"
#include "VisibleUnits.h"
#include "WheelEvent.h"
#include "WheelEventDeltaFilter.h"
#include "WheelEventTestMonitor.h"
#include "WindowsKeyboardCodes.h"
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include "PlatformTouchEventIOS.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include "TouchEvent.h"
#include "TouchList.h"
#endif

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
#include "PlatformTouchEvent.h"
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
#include "PlatformGestureEventMac.h"
#endif

#if ENABLE(POINTER_LOCK)
#include "PointerLockController.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "DOMTimerHoldingTank.h"
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
const int ColorDragHystersis = 3;
const int GeneralDragHysteresis = 3;
#if PLATFORM(MAC)
const Seconds EventHandler::TextDragDelay { 150_ms };
#else
const Seconds EventHandler::TextDragDelay { 0_s };
#endif
#endif // ENABLE(DRAG_SUPPORT)

#if ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
const float GestureUnknown = 0;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
// FIXME: Share this constant with EventHandler and SliderThumbElement.
const unsigned InvalidTouchIdentifier = 0;
#endif

// Match key code of composition keydown event on windows.
// IE sends VK_PROCESSKEY which has value 229;
const int CompositionEventKeyCode = 229;

using namespace SVGNames;

#if !ENABLE(IOS_TOUCH_EVENTS)
// The amount of time to wait before sending a fake mouse event, triggered
// during a scroll. The short interval is used if the content responds to the mouse events
// in fakeMouseMoveDurationThreshold or less, otherwise the long interval is used.
const double fakeMouseMoveDurationThreshold = 0.01;
const Seconds fakeMouseMoveShortInterval = { 100_ms };
const Seconds fakeMouseMoveLongInterval = { 250_ms };
#endif

// The amount of time to wait for a cursor update on style and layout changes
// Set to 50Hz, no need to be faster than common screen refresh rate
static const Seconds cursorUpdateInterval { 20_ms };

const int maximumCursorSize = 128;

#if ENABLE(MOUSE_CURSOR_SCALE)
// It's pretty unlikely that a scale of less than one would ever be used. But all we really
// need to ensure here is that the scale isn't so small that integer overflow can occur when
// dividing cursor sizes (limited above) by the scale.
const double minimumCursorScale = 0.001;
#endif

class MaximumDurationTracker {
public:
    explicit MaximumDurationTracker(double *maxDuration)
        : m_maxDuration(maxDuration)
        , m_start(MonotonicTime::now())
    {
    }

    ~MaximumDurationTracker()
    {
        *m_maxDuration = std::max(*m_maxDuration, (MonotonicTime::now() - m_start).seconds());
    }

private:
    double* m_maxDuration;
    MonotonicTime m_start;
};

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
class SyntheticTouchPoint : public PlatformTouchPoint {
public:

    // The default values are based on http://dvcs.w3.org/hg/webevents/raw-file/tip/touchevents.html
    explicit SyntheticTouchPoint(const PlatformMouseEvent& event)
    {
        static constexpr int idDefaultValue = 0;
        static constexpr int radiusYDefaultValue = 1;
        static constexpr int radiusXDefaultValue = 1;
        static constexpr float rotationAngleDefaultValue = 0.0f;
        static constexpr float forceDefaultValue = 1.0f;

        m_id = idDefaultValue; // There is only one active TouchPoint.
        m_screenPos = event.globalPosition();
        m_pos = event.position();
        m_radiusY = radiusYDefaultValue;
        m_radiusX = radiusXDefaultValue;
        m_rotationAngle = rotationAngleDefaultValue;
        m_force = forceDefaultValue;

        PlatformEvent::Type type = event.type();
        ASSERT(type == PlatformEvent::MouseMoved || type == PlatformEvent::MousePressed || type == PlatformEvent::MouseReleased);

        switch (type) {
        case PlatformEvent::MouseMoved:
            m_state = TouchMoved;
            break;
        case PlatformEvent::MousePressed:
            m_state = TouchPressed;
            break;
        case PlatformEvent::MouseReleased:
            m_state = TouchReleased;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
};

class SyntheticSingleTouchEvent : public PlatformTouchEvent {
public:
    explicit SyntheticSingleTouchEvent(const PlatformMouseEvent& event)
    {
        switch (event.type()) {
        case PlatformEvent::MouseMoved:
            m_type = TouchMove;
            break;
        case PlatformEvent::MousePressed:
            m_type = TouchStart;
            break;
        case PlatformEvent::MouseReleased:
            m_type = TouchEnd;
            break;
        default:
            ASSERT_NOT_REACHED();
            m_type = NoType;
            break;
        }
        m_timestamp = event.timestamp();
        m_modifiers = event.modifiers();
        m_touchPoints.append(SyntheticTouchPoint(event));
    }
};
#endif // ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)

static inline ScrollGranularity wheelGranularityToScrollGranularity(unsigned deltaMode)
{
    switch (deltaMode) {
    case WheelEvent::DOM_DELTA_PAGE:
        return ScrollByPage;
    case WheelEvent::DOM_DELTA_LINE:
        return ScrollByLine;
    case WheelEvent::DOM_DELTA_PIXEL:
        return ScrollByPixel;
    default:
        return ScrollByPixel;
    }
}

static void handleWheelEventPhaseInScrollableArea(ScrollableArea& scrollableArea, const WheelEvent& wheelEvent)
{
#if PLATFORM(MAC)
    if (wheelEvent.phase() == PlatformWheelEventPhaseMayBegin || wheelEvent.phase() == PlatformWheelEventPhaseCancelled)
        scrollableArea.scrollAnimator().handleWheelEventPhase(wheelEvent.phase());
#else
    UNUSED_PARAM(scrollableArea);
    UNUSED_PARAM(wheelEvent);
#endif
}

static bool didScrollInScrollableArea(ScrollableArea& scrollableArea, const WheelEvent& wheelEvent)
{
    ScrollGranularity scrollGranularity = wheelGranularityToScrollGranularity(wheelEvent.deltaMode());
    bool didHandleWheelEvent = false;
    if (float absoluteDelta = std::abs(wheelEvent.deltaX()))
        didHandleWheelEvent |= scrollableArea.scroll(wheelEvent.deltaX() > 0 ? ScrollRight : ScrollLeft, scrollGranularity, absoluteDelta);
    
    if (float absoluteDelta = std::abs(wheelEvent.deltaY()))
        didHandleWheelEvent |= scrollableArea.scroll(wheelEvent.deltaY() > 0 ? ScrollDown : ScrollUp, scrollGranularity, absoluteDelta);
    
    return didHandleWheelEvent;
}

static bool handleWheelEventInAppropriateEnclosingBox(Node* startNode, const WheelEvent& wheelEvent, RefPtr<Element>& stopElement, const FloatSize& filteredPlatformDelta, const FloatSize& filteredVelocity)
{
    bool shouldHandleEvent = wheelEvent.deltaX() || wheelEvent.deltaY();
#if ENABLE(WHEEL_EVENT_LATCHING)
    shouldHandleEvent |= wheelEvent.phase() == PlatformWheelEventPhaseEnded;
#if ENABLE(CSS_SCROLL_SNAP)
    shouldHandleEvent |= wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded;
#endif
#endif
    if (!startNode->renderer())
        return false;

    RenderBox& initialEnclosingBox = startNode->renderer()->enclosingBox();

    // RenderListBox is special because it's a ScrollableArea that the scrolling tree doesn't know about.
    if (is<RenderListBox>(initialEnclosingBox))
        handleWheelEventPhaseInScrollableArea(downcast<RenderListBox>(initialEnclosingBox), wheelEvent);

    if (!shouldHandleEvent)
        return false;

    if (is<RenderListBox>(initialEnclosingBox))
        return didScrollInScrollableArea(downcast<RenderListBox>(initialEnclosingBox), wheelEvent);

    RenderBox* currentEnclosingBox = &initialEnclosingBox;
    while (currentEnclosingBox) {
        if (RenderLayer* boxLayer = currentEnclosingBox->layer()) {
            auto platformEvent = wheelEvent.underlyingPlatformEvent();
            bool scrollingWasHandled;
            if (platformEvent) {
                auto copiedEvent = platformEvent->copyWithDeltasAndVelocity(filteredPlatformDelta.width(), filteredPlatformDelta.height(), filteredVelocity);
                scrollingWasHandled = boxLayer->handleWheelEvent(copiedEvent);
            } else
                scrollingWasHandled = didScrollInScrollableArea(*boxLayer, wheelEvent);

            if (scrollingWasHandled) {
                stopElement = currentEnclosingBox->element();
                return true;
            }
        }

        if (stopElement.get() && stopElement.get() == currentEnclosingBox->element())
            return true;

        currentEnclosingBox = currentEnclosingBox->containingBlock();
        if (!currentEnclosingBox || currentEnclosingBox->isRenderView())
            return false;
    }
    return false;
}

#if (ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS_FAMILY))
static bool shouldGesturesTriggerActive()
{
    // If the platform we're on supports GestureTapDown and GestureTapCancel then we'll
    // rely on them to set the active state. Unfortunately there's no generic way to
    // know in advance what event types are supported.
    return false;
}
#endif

#if !PLATFORM(COCOA)

bool EventHandler::eventLoopHandleMouseUp(const MouseEventWithHitTestResults&)
{
    return false;
}

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&)
{
    return false;
}
#endif

#endif

EventHandler::EventHandler(Frame& frame)
    : m_frame(frame)
    , m_hoverTimer(*this, &EventHandler::hoverTimerFired)
#if PLATFORM(MAC)
    , m_clearLatchingStateTimer(*this, &EventHandler::clearLatchedStateTimerFired)
#endif
    , m_autoscrollController(makeUnique<AutoscrollController>())
#if !ENABLE(IOS_TOUCH_EVENTS)
    , m_fakeMouseMoveEventTimer(*this, &EventHandler::fakeMouseMoveEventTimerFired)
#endif
#if ENABLE(CURSOR_VISIBILITY)
    , m_autoHideCursorTimer(*this, &EventHandler::autoHideCursorTimerFired)
#endif
{
}

EventHandler::~EventHandler()
{
#if !ENABLE(IOS_TOUCH_EVENTS)
    ASSERT(!m_fakeMouseMoveEventTimer.isActive());
#endif
#if ENABLE(CURSOR_VISIBILITY)
    ASSERT(!m_autoHideCursorTimer.isActive());
#endif
}
    
#if ENABLE(DRAG_SUPPORT)

DragState& EventHandler::dragState()
{
    static NeverDestroyed<DragState> state;
    return state;
}

#endif
    
void EventHandler::clear()
{
    m_hoverTimer.stop();
    m_hasScheduledCursorUpdate = false;
#if !ENABLE(IOS_TOUCH_EVENTS)
    m_fakeMouseMoveEventTimer.stop();
#endif
#if ENABLE(CURSOR_VISIBILITY)
    cancelAutoHideCursorTimer();
#endif
    m_resizeLayer = nullptr;
    m_elementUnderMouse = nullptr;
    m_lastElementUnderMouse = nullptr;
    m_lastMouseMoveEventSubframe = nullptr;
    m_lastScrollbarUnderMouse = nullptr;
    m_clickCount = 0;
    m_clickNode = nullptr;
#if ENABLE(IOS_GESTURE_EVENTS)
    m_gestureInitialDiameter = GestureUnknown;
    m_gestureInitialRotation = GestureUnknown;
#endif
#if ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
    m_gestureLastDiameter = GestureUnknown;
    m_gestureLastRotation = GestureUnknown;
    m_gestureTargets.clear();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    m_touches.clear();
    m_firstTouchID = InvalidTouchIdentifier;
    m_touchEventTargetSubframe = nullptr;
#endif
    m_frameSetBeingResized = nullptr;
#if ENABLE(DRAG_SUPPORT)
    m_dragTarget = nullptr;
    m_shouldOnlyFireDragOverEvent = false;
#endif
    m_mousePositionIsUnknown = true;
    m_lastKnownMousePosition = { };
    m_lastKnownMouseGlobalPosition = { };
    m_mousePressNode = nullptr;
    m_mousePressed = false;
    m_capturesDragging = false;
    m_capturingMouseEventsElement = nullptr;
    clearLatchedState();
#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    m_originatingTouchPointTargets.clear();
    m_originatingTouchPointDocument = nullptr;
    m_originatingTouchPointTargetKey = 0;
#endif
    m_maxMouseMovedDuration = 0;
    m_didStartDrag = false;
}

void EventHandler::nodeWillBeRemoved(Node& nodeToBeRemoved)
{
    if (nodeToBeRemoved.contains(m_clickNode.get()))
        m_clickNode = nullptr;
}

static void setSelectionIfNeeded(FrameSelection& selection, const VisibleSelection& newSelection)
{
    if (selection.selection() != newSelection && selection.shouldChangeSelection(newSelection))
        selection.setSelection(newSelection);
}

static inline bool dispatchSelectStart(Node* node)
{
    if (!node || !node->renderer())
        return true;

    auto event = Event::create(eventNames().selectstartEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
    node->dispatchEvent(event);
    return !event->defaultPrevented();
}

static Node* nodeToSelectOnMouseDownForNode(Node& targetNode)
{
#if ENABLE(USERSELECT_ALL)
    if (Node* rootUserSelectAll = Position::rootUserSelectAllForNode(&targetNode))
        return rootUserSelectAll;
#endif

    if (targetNode.shouldSelectOnMouseDown())
        return &targetNode;

    return nullptr;
}

static VisibleSelection expandSelectionToRespectSelectOnMouseDown(Node& targetNode, const VisibleSelection& selection)
{
    Node* nodeToSelect = nodeToSelectOnMouseDownForNode(targetNode);
    if (!nodeToSelect)
        return selection;

    VisibleSelection newSelection(selection);
    newSelection.setBase(positionBeforeNode(nodeToSelect).upstream(CanCrossEditingBoundary));
    newSelection.setExtent(positionAfterNode(nodeToSelect).downstream(CanCrossEditingBoundary));

    return newSelection;
}

bool EventHandler::updateSelectionForMouseDownDispatchingSelectStart(Node* targetNode, const VisibleSelection& selection, TextGranularity granularity)
{
    if (Position::nodeIsUserSelectNone(targetNode))
        return false;

    if (!dispatchSelectStart(targetNode)) {
        m_mouseDownMayStartSelect = false;
        return false;
    }

    if (selection.isRange())
        m_selectionInitiationState = ExtendedSelection;
    else {
        granularity = TextGranularity::CharacterGranularity;
        m_selectionInitiationState = PlacedCaret;
    }

    m_frame.selection().setSelectionByMouseIfDifferent(selection, granularity);

    return true;
}

void EventHandler::selectClosestWordFromHitTestResult(const HitTestResult& result, AppendTrailingWhitespace appendTrailingWhitespace)
{
    Node* targetNode = result.targetNode();
    VisibleSelection newSelection;

    if (targetNode && targetNode->renderer()) {
        VisiblePosition pos(targetNode->renderer()->positionForPoint(result.localPoint(), nullptr));
        if (pos.isNotNull()) {
            newSelection = VisibleSelection(pos);
            newSelection.expandUsingGranularity(TextGranularity::WordGranularity);
        }

        if (appendTrailingWhitespace == ShouldAppendTrailingWhitespace && newSelection.isRange())
            newSelection.appendTrailingWhitespace();

        updateSelectionForMouseDownDispatchingSelectStart(targetNode, expandSelectionToRespectSelectOnMouseDown(*targetNode, newSelection), TextGranularity::WordGranularity);
    }
}

static AppendTrailingWhitespace shouldAppendTrailingWhitespace(const MouseEventWithHitTestResults& result, const Frame& frame)
{
    return (result.event().clickCount() == 2 && frame.editor().isSelectTrailingWhitespaceEnabled()) ? ShouldAppendTrailingWhitespace : DontAppendTrailingWhitespace;
}

void EventHandler::selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults& result)
{
    if (m_mouseDownMayStartSelect)
        selectClosestWordFromHitTestResult(result.hitTestResult(), shouldAppendTrailingWhitespace(result, m_frame));
}

#if !PLATFORM(MAC)
VisibleSelection EventHandler::selectClosestWordFromHitTestResultBasedOnLookup(const HitTestResult&)
{
    return VisibleSelection();
}
#endif
    
void EventHandler::selectClosestContextualWordFromMouseEvent(const MouseEventWithHitTestResults& mouseEvent)
{
    Node* targetNode = mouseEvent.targetNode();
    const HitTestResult& result = mouseEvent.hitTestResult();
    VisibleSelection newSelection;
    bool appendTrailingWhitespace = shouldAppendTrailingWhitespace(mouseEvent, m_frame);
    
    if (targetNode && targetNode->renderer()) {
        newSelection = selectClosestWordFromHitTestResultBasedOnLookup(result);
        if (newSelection.isNone()) {
            VisiblePosition pos(targetNode->renderer()->positionForPoint(result.localPoint(), nullptr));
            if (pos.isNotNull()) {
                newSelection = VisibleSelection(pos);
                newSelection.expandUsingGranularity(TextGranularity::WordGranularity);
            }
        }
        
        if (appendTrailingWhitespace == ShouldAppendTrailingWhitespace && newSelection.isRange())
            newSelection.appendTrailingWhitespace();
        
        updateSelectionForMouseDownDispatchingSelectStart(targetNode, expandSelectionToRespectSelectOnMouseDown(*targetNode, newSelection), TextGranularity::WordGranularity);
    }
}
    
void EventHandler::selectClosestContextualWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults& result)
{
    Element* urlElement = result.hitTestResult().URLElement();
    if (!urlElement || !isDraggableLink(*urlElement)) {
        if (Node* targetNode = result.targetNode()) {
            if (isEditableNode(*targetNode))
                return selectClosestWordFromMouseEvent(result);
        }

        return selectClosestContextualWordFromMouseEvent(result);
    }

    Node* targetNode = result.targetNode();

    if (targetNode && targetNode->renderer() && m_mouseDownMayStartSelect) {
        VisibleSelection newSelection;
        VisiblePosition pos(targetNode->renderer()->positionForPoint(result.localPoint(), nullptr));
        if (pos.isNotNull() && pos.deepEquivalent().deprecatedNode()->isDescendantOf(*urlElement))
            newSelection = VisibleSelection::selectionFromContentsOfNode(urlElement);

        updateSelectionForMouseDownDispatchingSelectStart(targetNode, expandSelectionToRespectSelectOnMouseDown(*targetNode, newSelection), TextGranularity::WordGranularity);
    }
}

bool EventHandler::handleMousePressEventDoubleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;

    if (m_frame.selection().isRange())
        // A double-click when range is already selected
        // should not change the selection.  So, do not call
        // selectClosestWordFromMouseEvent, but do set
        // m_beganSelectingText to prevent handleMouseReleaseEvent
        // from setting caret selection.
        m_selectionInitiationState = ExtendedSelection;
    else
        selectClosestWordFromMouseEvent(event);

    return true;
}

bool EventHandler::handleMousePressEventTripleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;
    
    Node* targetNode = event.targetNode();
    if (!(targetNode && targetNode->renderer() && m_mouseDownMayStartSelect))
        return false;

    VisibleSelection newSelection;
    VisiblePosition pos(targetNode->renderer()->positionForPoint(event.localPoint(), nullptr));
    if (pos.isNotNull()) {
        newSelection = VisibleSelection(pos);
        newSelection.expandUsingGranularity(TextGranularity::ParagraphGranularity);
    }

    return updateSelectionForMouseDownDispatchingSelectStart(targetNode, expandSelectionToRespectSelectOnMouseDown(*targetNode, newSelection), TextGranularity::ParagraphGranularity);
}

static uint64_t textDistance(const Position& start, const Position& end)
{
    auto startBoundary = makeBoundaryPoint(start);
    auto endBoundary = makeBoundaryPoint(end);
    if (!startBoundary || !endBoundary)
        return 0;
    return characterCount({ *startBoundary, *endBoundary }, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
}

bool EventHandler::handleMousePressEventSingleClick(const MouseEventWithHitTestResults& event)
{
    Ref<Frame> protectedFrame(m_frame);

    m_frame.document()->updateLayoutIgnorePendingStylesheets();
    Node* targetNode = event.targetNode();
    if (!(targetNode && targetNode->renderer() && m_mouseDownMayStartSelect))
        return false;

    // Extend the selection if the Shift key is down, unless the click is in a link.
    bool extendSelection = event.event().shiftKey() && !event.isOverLink();

    // Don't restart the selection when the mouse is pressed on an
    // existing selection so we can allow for text dragging.
    if (FrameView* view = m_frame.view()) {
        LayoutPoint vPoint = view->windowToContents(event.event().position());
        if (!extendSelection && m_frame.selection().contains(vPoint)) {
            m_mouseDownWasSingleClickInSelection = true;
            return false;
        }
    }

    VisiblePosition visiblePosition(targetNode->renderer()->positionForPoint(event.localPoint(), nullptr));
    if (visiblePosition.isNull())
        visiblePosition = VisiblePosition(firstPositionInOrBeforeNode(targetNode), DOWNSTREAM);
    Position pos = visiblePosition.deepEquivalent();

    VisibleSelection newSelection = m_frame.selection().selection();
    TextGranularity granularity = TextGranularity::CharacterGranularity;

    if (!m_frame.editor().client()->shouldAllowSingleClickToChangeSelection(*targetNode, newSelection))
        return true;

    if (extendSelection && newSelection.isCaretOrRange()) {
        VisibleSelection selectionInUserSelectAll = expandSelectionToRespectSelectOnMouseDown(*targetNode, VisibleSelection(pos));
        if (selectionInUserSelectAll.isRange()) {
            if (comparePositions(selectionInUserSelectAll.start(), newSelection.start()) < 0)
                pos = selectionInUserSelectAll.start();
            else if (comparePositions(newSelection.end(), selectionInUserSelectAll.end()) < 0)
                pos = selectionInUserSelectAll.end();
        }

        if (!m_frame.editor().behavior().shouldConsiderSelectionAsDirectional() && pos.isNotNull()) {
            // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection
            // was created right-to-left
            Position start = newSelection.start();
            Position end = newSelection.end();
            int distanceToStart = textDistance(start, pos);
            int distanceToEnd = textDistance(pos, end);
            if (distanceToStart <= distanceToEnd)
                newSelection = VisibleSelection(end, pos);
            else
                newSelection = VisibleSelection(start, pos);
        } else
            newSelection.setExtent(pos);

        if (m_frame.selection().granularity() != TextGranularity::CharacterGranularity) {
            granularity = m_frame.selection().granularity();
            newSelection.expandUsingGranularity(m_frame.selection().granularity());
        }
    } else {
        if (event.event().syntheticClickType() != SyntheticClickType::NoTap) {
            auto adjustedVisiblePosition = wordBoundaryForPositionWithoutCrossingLine(visiblePosition).first;
            if (adjustedVisiblePosition.isNotNull())
                visiblePosition = WTFMove(adjustedVisiblePosition);
        }
        newSelection = expandSelectionToRespectSelectOnMouseDown(*targetNode, visiblePosition);
    }

    bool handled = updateSelectionForMouseDownDispatchingSelectStart(targetNode, newSelection, granularity);

    if (event.event().button() == MiddleButton) {
        // Ignore handled, since we want to paste to where the caret was placed anyway.
        handled = handlePasteGlobalSelection(event.event()) || handled;
    }
    return handled;
}

bool EventHandler::canMouseDownStartSelect(const MouseEventWithHitTestResults& event)
{
    auto* node = event.targetNode();

    if (Page* page = m_frame.page()) {
        if (!page->chrome().client().shouldUseMouseEventForSelection(event.event()))
            return false;
    }
    
    if (!node || !node->renderer())
        return true;

    return node->canStartSelection() || Position::nodeIsUserSelectAll(node);
}

bool EventHandler::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
    Ref<Frame> protectedFrame(m_frame);

#if ENABLE(DRAG_SUPPORT)
    // Reset drag state.
    dragState().source = nullptr;
#endif

#if !ENABLE(IOS_TOUCH_EVENTS)
    cancelFakeMouseMoveEvent();
#endif

    m_frame.document()->updateLayoutIgnorePendingStylesheets();

    if (ScrollView* scrollView = m_frame.view()) {
        if (scrollView->isPointInScrollbarCorner(event.event().position()))
            return false;
    }

    bool singleClick = event.event().clickCount() <= 1;

    // If we got the event back, that must mean it wasn't prevented,
    // so it's allowed to start a drag or selection if it wasn't in a scrollbar.
    m_mouseDownMayStartSelect = canMouseDownStartSelect(event) && !event.scrollbar();
    
#if ENABLE(DRAG_SUPPORT)
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    // FIXME: eventMayStartDrag() does not check for shift key press, link or image event targets.
    // Bug: https://bugs.webkit.org/show_bug.cgi?id=155390

    // Single mouse down on links or images can always trigger drag-n-drop.
    bool isMouseDownOnLinkOrImage = event.isOverLink() || event.hitTestResult().image();
    m_mouseDownMayStartDrag = singleClick && (!event.event().shiftKey() || isMouseDownOnLinkOrImage) && shouldAllowMouseDownToStartDrag();
#endif

    m_mouseDownWasSingleClickInSelection = false;

    m_mouseDownEvent = event.event();

    if (m_immediateActionStage != ImmediateActionStage::PerformedHitTest)
        m_immediateActionStage = ImmediateActionStage::None;

    if (event.isOverWidget() && passWidgetMouseDownEventToWidget(event))
        return true;

    if (is<SVGDocument>(*m_frame.document()) && downcast<SVGDocument>(*m_frame.document()).zoomAndPanEnabled()) {
        if (event.event().shiftKey() && singleClick) {
            m_svgPan = true;
            downcast<SVGDocument>(*m_frame.document()).startPan(m_frame.view()->windowToContents(event.event().position()));
            return true;
        }
    }

    // We don't do this at the start of mouse down handling,
    // because we don't want to do it until we know we didn't hit a widget.
    if (singleClick)
        focusDocumentView();

    m_mousePressNode = event.targetNode();
    m_frame.document()->setFocusNavigationStartingNode(event.targetNode());

#if ENABLE(DRAG_SUPPORT)
    m_dragStartPosition = event.event().position();
#endif

    m_mousePressed = true;
    m_selectionInitiationState = HaveNotStartedSelection;

    bool swallowEvent = false;
    if (event.event().clickCount() == 2)
        swallowEvent = handleMousePressEventDoubleClick(event);
    else if (event.event().clickCount() >= 3)
        swallowEvent = handleMousePressEventTripleClick(event);
    else
        swallowEvent = handleMousePressEventSingleClick(event);
    
    m_mouseDownMayStartAutoscroll = m_mouseDownMayStartSelect
        || (m_mousePressNode && m_mousePressNode->renderBox() && m_mousePressNode->renderBox()->canBeProgramaticallyScrolled());

    return swallowEvent;
}

VisiblePosition EventHandler::selectionExtentRespectingEditingBoundary(const VisibleSelection& selection, const LayoutPoint& localPoint, Node* targetNode)
{
    FloatPoint selectionEndPoint = localPoint;
    Element* editableElement = selection.rootEditableElement();

    if (!targetNode || !targetNode->renderer())
        return VisiblePosition();

    if (editableElement && !editableElement->contains(targetNode)) {
        if (!editableElement->renderer())
            return VisiblePosition();

        FloatPoint absolutePoint = targetNode->renderer()->localToAbsolute(FloatPoint(selectionEndPoint));
        selectionEndPoint = editableElement->renderer()->absoluteToLocal(absolutePoint);
        targetNode = editableElement;
    }

    return targetNode->renderer()->positionForPoint(LayoutPoint(selectionEndPoint), nullptr);
}

#if ENABLE(DRAG_SUPPORT)

#if !PLATFORM(IOS_FAMILY)

bool EventHandler::supportsSelectionUpdatesOnMouseDrag() const
{
    return true;
}

bool EventHandler::shouldAllowMouseDownToStartDrag() const
{
    return true;
}

#endif

bool EventHandler::handleMouseDraggedEvent(const MouseEventWithHitTestResults& event, CheckDragHysteresis checkDragHysteresis)
{
    if (!m_mousePressed)
        return false;

    Ref<Frame> protectedFrame(m_frame);

    if (handleDrag(event, checkDragHysteresis))
        return true;

    Node* targetNode = event.targetNode();
    if (event.event().button() != LeftButton || !targetNode)
        return false;

    RenderObject* renderer = targetNode->renderer();
    if (!renderer) {
        Element* parent = targetNode->parentOrShadowHostElement();
        if (!parent)
            return false;

        renderer = parent->renderer();
        if (!renderer || !renderer->isListBox())
            return false;
    }

#if PLATFORM(COCOA) // FIXME: Why does this assertion fire on other platforms?
    ASSERT(m_mouseDownMayStartSelect || m_mouseDownMayStartAutoscroll);
#endif

    m_mouseDownMayStartDrag = false;

    if (m_mouseDownMayStartAutoscroll && !panScrollInProgress()) {
        m_autoscrollController->startAutoscrollForSelection(renderer);
        m_mouseDownMayStartAutoscroll = false;
    }

    if (m_selectionInitiationState != ExtendedSelection) {
        HitTestResult result(m_mouseDownContentsPosition);
        m_frame.document()->hitTest(HitTestRequest(), result);

        updateSelectionForMouseDrag(result);
    }
    updateSelectionForMouseDrag(event.hitTestResult());
    return true;
}
    
bool EventHandler::eventMayStartDrag(const PlatformMouseEvent& event) const
{
    // This is a pre-flight check of whether the event might lead to a drag being started.  Be careful
    // that its logic needs to stay in sync with handleMouseMoveEvent() and the way we setMouseDownMayStartDrag
    // in handleMousePressEvent
    auto* document = m_frame.document();
    if (!document)
        return false;

    if (event.button() != LeftButton || event.clickCount() != 1)
        return false;

    FrameView* view = m_frame.view();
    if (!view)
        return false;

    Page* page = m_frame.page();
    if (!page)
        return false;

    Ref<Frame> protectedFrame(m_frame);

    updateDragSourceActionsAllowed();
    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::DisallowUserAgentShadowContent };
    HitTestResult result(view->windowToContents(event.position()));
    document->hitTest(hitType, result);
    DragState state;
    Element* targetElement = result.targetElement();
    return targetElement && page->dragController().draggableElement(&m_frame, targetElement, result.roundedPointInInnerNodeFrame(), state);
}

void EventHandler::updateSelectionForMouseDrag()
{
    if (!supportsSelectionUpdatesOnMouseDrag())
        return;

    FrameView* view = m_frame.view();
    if (!view)
        return;
    auto* document = m_frame.document();
    if (!document)
        return;

    constexpr OptionSet<HitTestRequest::RequestType> hitType {  HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::Move, HitTestRequest::DisallowUserAgentShadowContent };
    HitTestResult result(view->windowToContents(m_lastKnownMousePosition));
    document->hitTest(hitType, result);
    updateSelectionForMouseDrag(result);
}

void EventHandler::updateSelectionForMouseDrag(const HitTestResult& hitTestResult)
{
    if (!supportsSelectionUpdatesOnMouseDrag())
        return;

    if (!m_mouseDownMayStartSelect)
        return;

    Node* target = hitTestResult.targetNode();
    if (!target)
        return;

    VisiblePosition targetPosition = selectionExtentRespectingEditingBoundary(m_frame.selection().selection(), hitTestResult.localPoint(), target);

    // Don't modify the selection if we're not on a node.
    if (targetPosition.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in handleMousePressEvent, but not if the mouse press was on an existing selection.
    VisibleSelection newSelection = m_frame.selection().selection();

    // Special case to limit selection to the containing block for SVG text.
    // FIXME: Isn't there a better non-SVG-specific way to do this?
    if (Node* selectionBaseNode = newSelection.base().deprecatedNode()) {
        if (RenderObject* selectionBaseRenderer = selectionBaseNode->renderer()) {
            if (selectionBaseRenderer->isSVGText()) {
                if (target->renderer()->containingBlock() != selectionBaseRenderer->containingBlock())
                    return;
            }
        }
    }


    if (m_selectionInitiationState == HaveNotStartedSelection && !dispatchSelectStart(target)) {
        m_mouseDownMayStartSelect = false;
        return;
    }

    if (m_selectionInitiationState != ExtendedSelection) {
        // Always extend selection here because it's caused by a mouse drag
        m_selectionInitiationState = ExtendedSelection;
        newSelection = VisibleSelection(targetPosition);
    }

#if ENABLE(USERSELECT_ALL)
    Node* rootUserSelectAllForMousePressNode = Position::rootUserSelectAllForNode(m_mousePressNode.get());
    if (rootUserSelectAllForMousePressNode && rootUserSelectAllForMousePressNode == Position::rootUserSelectAllForNode(target)) {
        newSelection.setBase(positionBeforeNode(rootUserSelectAllForMousePressNode).upstream(CanCrossEditingBoundary));
        newSelection.setExtent(positionAfterNode(rootUserSelectAllForMousePressNode).downstream(CanCrossEditingBoundary));
    } else {
        // Reset base for user select all when base is inside user-select-all area and extent < base.
        if (rootUserSelectAllForMousePressNode && comparePositions(target->renderer()->positionForPoint(hitTestResult.localPoint(), nullptr), m_mousePressNode->renderer()->positionForPoint(m_dragStartPosition, nullptr)) < 0)
            newSelection.setBase(positionAfterNode(rootUserSelectAllForMousePressNode).downstream(CanCrossEditingBoundary));
        
        Node* rootUserSelectAllForTarget = Position::rootUserSelectAllForNode(target);
        if (rootUserSelectAllForTarget && m_mousePressNode->renderer() && comparePositions(target->renderer()->positionForPoint(hitTestResult.localPoint(), nullptr), m_mousePressNode->renderer()->positionForPoint(m_dragStartPosition, nullptr)) < 0)
            newSelection.setExtent(positionBeforeNode(rootUserSelectAllForTarget).upstream(CanCrossEditingBoundary));
        else if (rootUserSelectAllForTarget && m_mousePressNode->renderer())
            newSelection.setExtent(positionAfterNode(rootUserSelectAllForTarget).downstream(CanCrossEditingBoundary));
        else
            newSelection.setExtent(targetPosition);
    }
#else
    newSelection.setExtent(targetPosition);
#endif

    if (m_frame.selection().granularity() != TextGranularity::CharacterGranularity)
        newSelection.expandUsingGranularity(m_frame.selection().granularity());

    m_frame.selection().setSelectionByMouseIfDifferent(newSelection, m_frame.selection().granularity(),
        FrameSelection::EndPointsAdjustmentMode::AdjustAtBidiBoundary);
}
#endif // ENABLE(DRAG_SUPPORT)

void EventHandler::lostMouseCapture()
{
    m_frame.selection().setCaretBlinkingSuspended(false);
}

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
    if (autoscrollInProgress())
        stopAutoscrollTimer();

    Ref<Frame> protectedFrame(m_frame);

    if (handleMouseUp(event))
        return true;

    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
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
    if (m_mouseDownWasSingleClickInSelection && m_selectionInitiationState != ExtendedSelection
#if ENABLE(DRAG_SUPPORT)
            && m_dragStartPosition == event.event().position()
#endif
            && m_frame.selection().isRange()
            && event.event().button() != RightButton) {
        VisibleSelection newSelection;
        Node* node = event.targetNode();
        bool caretBrowsing = m_frame.settings().caretBrowsingEnabled();
        bool allowSelectionChanges = true;
        if (node && node->renderer() && (caretBrowsing || node->hasEditableStyle())) {
            VisiblePosition pos = node->renderer()->positionForPoint(event.localPoint(), nullptr);
            newSelection = VisibleSelection(pos);
#if PLATFORM(IOS_FAMILY)
            // On iOS, selection changes are triggered using platform-specific text interaction gestures rather than
            // default behavior on click or mouseup. As such, the only time we should allow click events to change the
            // selection on iOS is when we focus a different editable element, in which case the text interaction
            // gestures will fail.
            allowSelectionChanges = m_frame.selection().selection().rootEditableElement() != newSelection.rootEditableElement();
#endif
        }

        if (allowSelectionChanges)
            setSelectionIfNeeded(m_frame.selection(), newSelection);

        handled = true;
    }

    if (event.event().button() == MiddleButton) {
        // Ignore handled, since we want to paste to where the caret was placed anyway.
        handled = handlePasteGlobalSelection(event.event()) || handled;
    }

    return handled;
}

#if ENABLE(PAN_SCROLLING)

void EventHandler::didPanScrollStart()
{
    m_autoscrollController->didPanScrollStart();
}

void EventHandler::didPanScrollStop()
{
    m_autoscrollController->didPanScrollStop();
}

void EventHandler::startPanScrolling(RenderElement& renderer)
{
#if !PLATFORM(IOS_FAMILY)
    if (!is<RenderBox>(renderer))
        return;
    m_autoscrollController->startPanScrolling(&downcast<RenderBox>(renderer), lastKnownMousePosition());
    invalidateClick();
#endif
}

#endif // ENABLE(PAN_SCROLLING)

RenderBox* EventHandler::autoscrollRenderer() const
{
    return m_autoscrollController->autoscrollRenderer();
}

void EventHandler::updateAutoscrollRenderer()
{
    m_autoscrollController->updateAutoscrollRenderer();
}

bool EventHandler::autoscrollInProgress() const
{
    return m_autoscrollController->autoscrollInProgress();
}

bool EventHandler::panScrollInProgress() const
{
    return m_autoscrollController->panScrollInProgress();
}

#if ENABLE(DRAG_SUPPORT)
OptionSet<DragSourceAction> EventHandler::updateDragSourceActionsAllowed() const
{
    Page* page = m_frame.page();
    if (!page)
        return { };

    FrameView* view = m_frame.view();
    if (!view)
        return { };

    return page->dragController().delegateDragSourceAction(view->contentsToRootView(m_mouseDownContentsPosition));
}
#endif // ENABLE(DRAG_SUPPORT)

HitTestResult EventHandler::hitTestResultAtPoint(const LayoutPoint& point, OptionSet<HitTestRequest::RequestType> hitType, const LayoutSize& padding) const
{
    ASSERT((hitType & HitTestRequest::CollectMultipleElements) || padding.isEmpty());

    Ref<Frame> protectedFrame(m_frame);

    // We always send hitTestResultAtPoint to the main frame if we have one,
    // otherwise we might hit areas that are obscured by higher frames.
    if (!m_frame.isMainFrame()) {
        Frame& mainFrame = m_frame.mainFrame();
        FrameView* frameView = m_frame.view();
        FrameView* mainView = mainFrame.view();
        if (frameView && mainView) {
            IntPoint mainFramePoint = mainView->rootViewToContents(frameView->contentsToRootView(roundedIntPoint(point)));
            return mainFrame.eventHandler().hitTestResultAtPoint(mainFramePoint, hitType, padding);
        }
    }

    unsigned nonNegativePaddingWidth = std::max<LayoutUnit>(0, padding.width()).toUnsigned();
    unsigned nonNegativePaddingHeight = std::max<LayoutUnit>(0, padding.height()).toUnsigned();

    // We should always start hit testing a clean tree.
    if (auto* frameView = m_frame.view())
        frameView->updateLayoutAndStyleIfNeededRecursive();

    HitTestResult result(point, nonNegativePaddingHeight, nonNegativePaddingWidth, nonNegativePaddingHeight, nonNegativePaddingWidth);
    auto* document = m_frame.document();
    if (!document)
        return result;

    HitTestRequest request(hitType);
    document->hitTest(request, result);
    if (!request.readOnly())
        m_frame.document()->updateHoverActiveState(request, result.targetElement());

    if (request.disallowsUserAgentShadowContent())
        result.setToNonUserAgentShadowAncestor();

    return result;
}

void EventHandler::stopAutoscrollTimer(bool rendererIsBeingDestroyed)
{
    m_autoscrollController->stopAutoscrollTimer(rendererIsBeingDestroyed);
}

bool EventHandler::scrollOverflow(ScrollDirection direction, ScrollGranularity granularity, Node* startingNode)
{
    Node* node = startingNode;

    if (!node)
        node = m_frame.document()->focusedElement();

    if (!node)
        node = m_mousePressNode.get();
    
    if (node) {
        auto r = node->renderer();
        if (r && !r->isListBox() && r->enclosingBox().scroll(direction, granularity)) {
            setFrameWasScrolledByUser();
            return true;
        }
    }

    return false;
}

bool EventHandler::logicalScrollOverflow(ScrollLogicalDirection direction, ScrollGranularity granularity, Node* startingNode)
{
    Node* node = startingNode;

    if (!node)
        node = m_frame.document()->focusedElement();

    if (!node)
        node = m_mousePressNode.get();
    
    if (node) {
        auto r = node->renderer();
        if (r && !r->isListBox() && r->enclosingBox().logicalScroll(direction, granularity)) {
            setFrameWasScrolledByUser();
            return true;
        }
    }

    return false;
}

bool EventHandler::scrollRecursively(ScrollDirection direction, ScrollGranularity granularity, Node* startingNode)
{
    Ref<Frame> protectedFrame(m_frame);

    // The layout needs to be up to date to determine if we can scroll. We may be
    // here because of an onLoad event, in which case the final layout hasn't been performed yet.
    m_frame.document()->updateLayoutIgnorePendingStylesheets();
    if (scrollOverflow(direction, granularity, startingNode))
        return true;    
    Frame* frame = &m_frame;
    FrameView* view = frame->view();
    if (view && view->scroll(direction, granularity))
        return true;
    frame = frame->tree().parent();
    if (!frame)
        return false;
    return frame->eventHandler().scrollRecursively(direction, granularity, m_frame.ownerElement());
}

bool EventHandler::logicalScrollRecursively(ScrollLogicalDirection direction, ScrollGranularity granularity, Node* startingNode)
{
    Ref<Frame> protectedFrame(m_frame);

    // The layout needs to be up to date to determine if we can scroll. We may be
    // here because of an onLoad event, in which case the final layout hasn't been performed yet.
    m_frame.document()->updateLayoutIgnorePendingStylesheets();
    if (logicalScrollOverflow(direction, granularity, startingNode))
        return true;    
    Frame* frame = &m_frame;
    FrameView* view = frame->view();
    
    bool scrolled = false;
#if PLATFORM(COCOA)
    // Mac also resets the scroll position in the inline direction.
    if (granularity == ScrollByDocument && view && view->logicalScroll(ScrollInlineDirectionBackward, ScrollByDocument))
        scrolled = true;
#endif
    if (view && view->logicalScroll(direction, granularity))
        scrolled = true;
    
    if (scrolled)
        return true;
    
    frame = frame->tree().parent();
    if (!frame)
        return false;

    return frame->eventHandler().logicalScrollRecursively(direction, granularity, m_frame.ownerElement());
}

IntPoint EventHandler::lastKnownMousePosition() const
{
    return m_lastKnownMousePosition;
}

RefPtr<Frame> EventHandler::subframeForHitTestResult(const MouseEventWithHitTestResults& hitTestResult)
{
    if (!hitTestResult.isOverWidget())
        return nullptr;
    return subframeForTargetNode(hitTestResult.targetNode());
}

RefPtr<Frame> EventHandler::subframeForTargetNode(Node* node)
{
    if (!node)
        return nullptr;

    auto renderer = node->renderer();
    if (!is<RenderWidget>(renderer))
        return nullptr;

    Widget* widget = downcast<RenderWidget>(*renderer).widget();
    if (!is<FrameView>(widget))
        return nullptr;

    return &downcast<FrameView>(*widget).frame();
}

static bool isSubmitImage(Node* node)
{
    return is<HTMLInputElement>(node) && downcast<HTMLInputElement>(*node).isImageButton();
}

// Returns true if the node's editable block is not current focused for editing
static bool nodeIsNotBeingEdited(const Node& node, const Frame& frame)
{
    return frame.selection().selection().rootEditableElement() != node.rootEditableElement();
}

bool EventHandler::useHandCursor(Node* node, bool isOverLink, bool shiftKey)
{
    if (!node)
        return false;

    bool editable = node->hasEditableStyle();
    bool editableLinkEnabled = false;

    // If the link is editable, then we need to check the settings to see whether or not the link should be followed
    if (editable) {
        switch (m_frame.settings().editableLinkBehavior()) {
        default:
        case EditableLinkDefaultBehavior:
        case EditableLinkAlwaysLive:
            editableLinkEnabled = true;
            break;

        case EditableLinkNeverLive:
            editableLinkEnabled = false;
            break;

        case EditableLinkLiveWhenNotFocused:
            editableLinkEnabled = nodeIsNotBeingEdited(*node, m_frame) || shiftKey;
            break;

        case EditableLinkOnlyLiveWithShiftKey:
            editableLinkEnabled = shiftKey;
            break;
        }
    }

    return ((isOverLink || isSubmitImage(node)) && (!editable || editableLinkEnabled));
}

void EventHandler::updateCursorIfNeeded()
{
    if (std::exchange(m_hasScheduledCursorUpdate, false))
        updateCursor();
}

void EventHandler::updateCursor()
{
    if (m_mousePositionIsUnknown)
        return;

    if (Page* page = m_frame.page()) {
        if (!page->chrome().client().supportsSettingCursor())
            return;
    }

    FrameView* view = m_frame.view();
    if (!view)
        return;

    auto* document = m_frame.document();
    if (!document)
        return;

    if (!view->shouldSetCursor())
        return;

    bool shiftKey;
    bool ctrlKey;
    bool altKey;
    bool metaKey;
    PlatformKeyboardEvent::getCurrentModifierState(shiftKey, ctrlKey, altKey, metaKey);

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::AllowFrameScrollbars };
    HitTestResult result(view->windowToContents(m_lastKnownMousePosition));
    document->hitTest(hitType, result);

    updateCursor(*view, result, shiftKey);
}

void EventHandler::updateCursor(FrameView& view, const HitTestResult& result, bool shiftKey)
{
    if (auto optionalCursor = selectCursor(result, shiftKey)) {
        m_currentMouseCursor = WTFMove(optionalCursor.value());
        view.setCursor(m_currentMouseCursor);
    }
}

Optional<Cursor> EventHandler::selectCursor(const HitTestResult& result, bool shiftKey)
{
    if (m_resizeLayer && m_resizeLayer->inResizeMode())
        return WTF::nullopt;

    if (!m_frame.page())
        return WTF::nullopt;

#if ENABLE(PAN_SCROLLING)
    if (m_frame.mainFrame().eventHandler().panScrollInProgress())
        return WTF::nullopt;
#endif

    Ref<Frame> protectedFrame(m_frame);

    // Use always pointer cursor for scrollbars.
    if (result.scrollbar()) {
#if ENABLE(CURSOR_VISIBILITY)
        cancelAutoHideCursorTimer();
#endif
        return pointerCursor();
    }

    Node* node = result.targetNode();
    if (!node)
        return WTF::nullopt;

    auto renderer = node->renderer();
    auto* style = renderer ? &renderer->style() : nullptr;
    bool horizontalText = !style || style->isHorizontalWritingMode();
    const Cursor& iBeam = horizontalText ? iBeamCursor() : verticalTextCursor();

#if ENABLE(CURSOR_VISIBILITY)
    if (style && style->cursorVisibility() == CursorVisibility::AutoHide)
        startAutoHideCursorTimer();
    else
        cancelAutoHideCursorTimer();
#endif

    if (renderer) {
        Cursor overrideCursor;
        switch (renderer->getCursor(roundedIntPoint(result.localPoint()), overrideCursor)) {
        case SetCursorBasedOnStyle:
            break;
        case SetCursor:
            return overrideCursor;
        case DoNotSetCursor:
            return WTF::nullopt;
        }
    }

    if (style && style->cursors()) {
        const CursorList* cursors = style->cursors();
        for (unsigned i = 0; i < cursors->size(); ++i) {
            StyleImage* styleImage = (*cursors)[i].image();
            if (!styleImage)
                continue;
            CachedImage* cachedImage = styleImage->cachedImage();
            if (!cachedImage)
                continue;
            float scale = styleImage->imageScaleFactor();
            // Get hotspot and convert from logical pixels to physical pixels.
            IntPoint hotSpot = (*cursors)[i].hotSpot();
            FloatSize size = cachedImage->imageForRenderer(renderer)->size();
            if (cachedImage->errorOccurred())
                continue;
            // Limit the size of cursors (in UI pixels) so that they cannot be
            // used to cover UI elements in chrome.
            size.scale(1 / scale);
            if (size.width() > maximumCursorSize || size.height() > maximumCursorSize)
                continue;

            Image* image = cachedImage->imageForRenderer(renderer);
#if ENABLE(MOUSE_CURSOR_SCALE)
            // Ensure no overflow possible in calculations above.
            if (scale < minimumCursorScale)
                continue;
            return Cursor(image, hotSpot, scale);
#else
            ASSERT(scale == 1);
            return Cursor(image, hotSpot);
#endif // ENABLE(MOUSE_CURSOR_SCALE)
        }
    }

    // During selection, use an I-beam regardless of the content beneath the cursor.
    // If a drag may be starting or we're capturing mouse events for a particular node, don't treat this as a selection.
    if (m_mousePressed
        && m_mouseDownMayStartSelect
#if ENABLE(DRAG_SUPPORT)
        && !m_mouseDownMayStartDrag
#endif
        && m_frame.selection().isCaretOrRange()
        && !m_capturingMouseEventsElement)
        return iBeam;

    switch (style ? style->cursor() : CursorType::Auto) {
    case CursorType::Auto: {
        bool editable = node->hasEditableStyle();

        if (useHandCursor(node, result.isOverLink(), shiftKey))
            return handCursor();

        bool inResizer = false;
        if (renderer && renderer->hasLayer()) {
            // FIXME: With right-aligned text in a box, the renderer here is usually a RenderText, which prevents showing the resize cursor: webkit.org/b/210935.
            if (auto* layer = downcast<RenderLayerModelObject>(*renderer).layer()) {
                inResizer = layer->isPointInResizeControl(roundedIntPoint(result.localPoint()));
                if (inResizer)
                    return layer->shouldPlaceBlockDirectionScrollbarOnLeft() ? southWestResizeCursor() : southEastResizeCursor();
            }
        }

        if ((editable || (renderer && renderer->isText() && node->canStartSelection())) && !inResizer && !result.scrollbar())
            return iBeam;
        return pointerCursor();
    }
    case CursorType::Default:
        return pointerCursor();
    case CursorType::None:
        return noneCursor();
    case CursorType::ContextMenu:
        return contextMenuCursor();
    case CursorType::Help:
        return helpCursor();
    case CursorType::Pointer:
        return handCursor();
    case CursorType::Progress:
        return progressCursor();
    case CursorType::Wait:
        return waitCursor();
    case CursorType::Cell:
        return cellCursor();
    case CursorType::Crosshair:
        return crossCursor();
    case CursorType::Text:
        return iBeamCursor();
    case CursorType::VerticalText:
        return verticalTextCursor();
    case CursorType::Alias:
        return aliasCursor();
    case CursorType::Copy:
        return copyCursor();
    case CursorType::Move:
        return moveCursor();
    case CursorType::NoDrop:
        return noDropCursor();
    case CursorType::NotAllowed:
        return notAllowedCursor();
    case CursorType::Grab:
        return grabCursor();
    case CursorType::Grabbing:
        return grabbingCursor();
    case CursorType::EResize:
        return eastResizeCursor();
    case CursorType::NResize:
        return northResizeCursor();
    case CursorType::NEResize:
        return northEastResizeCursor();
    case CursorType::NWResize:
        return northWestResizeCursor();
    case CursorType::SResize:
        return southResizeCursor();
    case CursorType::SEResize:
        return southEastResizeCursor();
    case CursorType::SWResize:
        return southWestResizeCursor();
    case CursorType::WResize:
        return westResizeCursor();
    case CursorType::EWResize:
        return eastWestResizeCursor();
    case CursorType::NSResize:
        return northSouthResizeCursor();
    case CursorType::NESWResize:
        return northEastSouthWestResizeCursor();
    case CursorType::NWSEResize:
        return northWestSouthEastResizeCursor();
    case CursorType::ColumnResize:
        return columnResizeCursor();
    case CursorType::RowResize:
        return rowResizeCursor();
    case CursorType::AllScroll:
        return moveCursor();
    case CursorType::ZoomIn:
        return zoomInCursor();
    case CursorType::ZoomOut:
        return zoomOutCursor();
    }
    return pointerCursor();
}

#if ENABLE(CURSOR_VISIBILITY)
void EventHandler::startAutoHideCursorTimer()
{
    Page* page = m_frame.page();
    if (!page)
        return;

    m_autoHideCursorTimer.startOneShot(page->settings().timeWithoutMouseMovementBeforeHidingControls());

#if !ENABLE(IOS_TOUCH_EVENTS)
    // The fake mouse move event screws up the auto-hide feature (by resetting the auto-hide timer)
    // so cancel any pending fake mouse moves.
    if (m_fakeMouseMoveEventTimer.isActive())
        m_fakeMouseMoveEventTimer.stop();
#endif
}

void EventHandler::cancelAutoHideCursorTimer()
{
    if (m_autoHideCursorTimer.isActive())
        m_autoHideCursorTimer.stop();
}

void EventHandler::autoHideCursorTimerFired()
{
    FrameView* view = m_frame.view();
    if (!view || !view->isActive())
        return;

    if (auto page = m_frame.page())
        page->chrome().setCursorHiddenUntilMouseMoves(true);
}
#endif

static LayoutPoint documentPointForWindowPoint(Frame& frame, const IntPoint& windowPoint)
{
    FrameView* view = frame.view();
    // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
    // Historically the code would just crash; this is clearly no worse than that.
    return view ? view->windowToContents(windowPoint) : windowPoint;
}

static Scrollbar* scrollbarForMouseEvent(const MouseEventWithHitTestResults& mouseEvent, FrameView* view)
{
    if (view) {
        if (auto* scrollbar = view->scrollbarAtPoint(mouseEvent.event().position()))
            return scrollbar;
    }
    return mouseEvent.scrollbar();

}

bool EventHandler::handleMousePressEvent(const PlatformMouseEvent& platformMouseEvent)
{
    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

    if (InspectorInstrumentation::handleMousePress(m_frame)) {
        invalidateClick();
        return true;
    }

#if ENABLE(POINTER_LOCK)
    if (m_frame.page()->pointerLockController().isLocked()) {
        m_frame.page()->pointerLockController().dispatchLockedMouseEvent(platformMouseEvent, eventNames().mousedownEvent);
        return true;
    }
#endif

    if (m_frame.page()->pageOverlayController().handleMouseEvent(platformMouseEvent))
        return true;

#if ENABLE(TOUCH_EVENTS)
    bool defaultPrevented = dispatchSyntheticTouchEventIfEnabled(platformMouseEvent);
    if (defaultPrevented)
        return true;
#endif

    UserGestureIndicator gestureIndicator(ProcessingUserGesture, m_frame.document());

    // FIXME (bug 68185): this call should be made at another abstraction layer
    m_frame.loader().resetMultipleFormSubmissionProtection();

#if !ENABLE(IOS_TOUCH_EVENTS)
    cancelFakeMouseMoveEvent();
#endif
    m_mousePressed = true;
    m_capturesDragging = true;
    setLastKnownMousePosition(platformMouseEvent);
    m_mouseDownTimestamp = platformMouseEvent.timestamp();
#if ENABLE(DRAG_SUPPORT)
    m_mouseDownMayStartDrag = false;
#endif
    m_mouseDownMayStartSelect = false;
    m_mouseDownMayStartAutoscroll = false;
    if (FrameView* view = m_frame.view())
        m_mouseDownContentsPosition = view->windowToContents(platformMouseEvent.position());
    else {
        invalidateClick();
        return false;
    }
    m_mouseDownWasInSubframe = false;

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent };
    // Save the document point we generate in case the window coordinate is invalidated by what happens
    // when we dispatch the event.
    LayoutPoint documentPoint = documentPointForWindowPoint(m_frame, platformMouseEvent.position());
    MouseEventWithHitTestResults mouseEvent = m_frame.document()->prepareMouseEvent(hitType, documentPoint, platformMouseEvent);

    if (!mouseEvent.targetNode()) {
        invalidateClick();
        return false;
    }

    m_mousePressNode = mouseEvent.targetNode();
    m_frame.document()->setFocusNavigationStartingNode(mouseEvent.targetNode());

    Scrollbar* scrollbar = scrollbarForMouseEvent(mouseEvent, m_frame.view());
    updateLastScrollbarUnderMouse(scrollbar, SetOrClearLastScrollbar::Set);
    bool passedToScrollbar = scrollbar && passMousePressEventToScrollbar(mouseEvent, scrollbar);

    if (!passedToScrollbar) {
        RefPtr<Frame> subframe = subframeForHitTestResult(mouseEvent);
        if (subframe && passMousePressEventToSubframe(mouseEvent, *subframe)) {
            // Start capturing future events for this frame. We only do this if we didn't clear
            // the m_mousePressed flag, which may happen if an AppKit widget entered a modal event loop.
            m_capturesDragging = subframe->eventHandler().capturesDragging();
            if (m_mousePressed && m_capturesDragging) {
                m_capturingMouseEventsElement = subframe->ownerElement();
                m_eventHandlerWillResetCapturingMouseEventsElement = true;
            }
            invalidateClick();
            return true;
        }
    }

#if ENABLE(PAN_SCROLLING)
    // We store whether pan scrolling is in progress before calling stopAutoscrollTimer()
    // because it will set m_autoscrollType to NoAutoscroll on return.
    bool isPanScrollInProgress = m_frame.mainFrame().eventHandler().panScrollInProgress();
    stopAutoscrollTimer();
    if (isPanScrollInProgress) {
        // We invalidate the click when exiting pan scrolling so that we don't inadvertently navigate
        // away from the current page (e.g. the click was on a hyperlink). See <rdar://problem/6095023>.
        invalidateClick();
        return true;
    }
#endif

    m_clickCount = platformMouseEvent.clickCount();
    m_clickNode = mouseEvent.targetNode();

    if (!m_clickNode) {
        invalidateClick();
        return false;
    }

    RenderLayer* layer = m_clickNode->renderer() ? m_clickNode->renderer()->enclosingLayer() : nullptr;
    auto localPoint = roundedIntPoint(mouseEvent.hitTestResult().localPoint());
    if (layer && layer->isPointInResizeControl(localPoint)) {
        layer->setInResizeMode(true);
        m_resizeLayer = layer;
        m_offsetFromResizeCorner = layer->offsetFromResizeCorner(localPoint);
        invalidateClick();
        return true;
    }

    m_frame.selection().setCaretBlinkingSuspended(true);

    bool swallowEvent = !dispatchMouseEvent(eventNames().mousedownEvent, mouseEvent.targetNode(), m_clickCount, platformMouseEvent, FireMouseOverOut::Yes);
    m_capturesDragging = !swallowEvent || mouseEvent.scrollbar();

    // If the hit testing originally determined the event was in a scrollbar, refetch the MouseEventWithHitTestResults
    // in case the scrollbar widget was destroyed when the mouse event was handled.
    if (mouseEvent.scrollbar()) {
        const bool wasLastScrollBar = mouseEvent.scrollbar() == m_lastScrollbarUnderMouse;
        mouseEvent = m_frame.document()->prepareMouseEvent(HitTestRequest(), documentPoint, platformMouseEvent);
        if (wasLastScrollBar && mouseEvent.scrollbar() != m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse = nullptr;
    }

    if (!swallowEvent) {
        // Refetch the event target node if it currently is the shadow node inside an <input> element.
        // If a mouse event handler changes the input element type to one that has a widget associated,
        // we'd like to EventHandler::handleMousePressEvent to pass the event to the widget and thus the
        // event target node can't still be the shadow node.
        if (is<ShadowRoot>(*mouseEvent.targetNode()) && is<HTMLInputElement>(*downcast<ShadowRoot>(*mouseEvent.targetNode()).host()))
            mouseEvent = m_frame.document()->prepareMouseEvent(HitTestRequest(), documentPoint, platformMouseEvent);
    }

    if (!swallowEvent) {
        if (passedToScrollbar)
            swallowEvent = true;
        else
            swallowEvent = handleMousePressEvent(mouseEvent);
    }
    return swallowEvent;
}

// This method only exists for platforms that don't know how to deliver 
bool EventHandler::handleMouseDoubleClickEvent(const PlatformMouseEvent& platformMouseEvent)
{
    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

    m_frame.selection().setCaretBlinkingSuspended(false);

    UserGestureIndicator gestureIndicator(ProcessingUserGesture, m_frame.document());

#if ENABLE(POINTER_LOCK)
    if (m_frame.page()->pointerLockController().isLocked()) {
        m_frame.page()->pointerLockController().dispatchLockedMouseEvent(platformMouseEvent, eventNames().mouseupEvent);
        return true;
    }
#endif

    // We get this instead of a second mouse-up 
    m_mousePressed = false;
    setLastKnownMousePosition(platformMouseEvent);

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Release, HitTestRequest::DisallowUserAgentShadowContent };
    MouseEventWithHitTestResults mouseEvent = prepareMouseEvent(hitType, platformMouseEvent);
    auto subframe = subframeForHitTestResult(mouseEvent);

    if (m_eventHandlerWillResetCapturingMouseEventsElement)
        m_capturingMouseEventsElement = nullptr;

    if (subframe && passMousePressEventToSubframe(mouseEvent, *subframe))
        return true;

    m_clickCount = platformMouseEvent.clickCount();
    bool swallowMouseUpEvent = !dispatchMouseEvent(eventNames().mouseupEvent, mouseEvent.targetNode(), m_clickCount, platformMouseEvent, FireMouseOverOut::No);

    bool swallowClickEvent = platformMouseEvent.button() != RightButton && mouseEvent.targetNode() == m_clickNode && !dispatchMouseEvent(eventNames().clickEvent, mouseEvent.targetNode(), m_clickCount, platformMouseEvent, FireMouseOverOut::Yes);

    if (m_lastScrollbarUnderMouse)
        swallowMouseUpEvent = m_lastScrollbarUnderMouse->mouseUp(platformMouseEvent);

    bool swallowMouseReleaseEvent = !swallowMouseUpEvent && handleMouseReleaseEvent(mouseEvent);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

ScrollableArea* EventHandler::enclosingScrollableArea(Node* node)
{
    for (auto ancestor = node; ancestor; ancestor = ancestor->parentOrShadowHostNode()) {
        if (is<HTMLIFrameElement>(*ancestor))
            return nullptr;

        if (is<HTMLHtmlElement>(*ancestor) || is<HTMLDocument>(*ancestor))
            break;

        auto renderer = ancestor->renderer();
        if (!renderer)
            continue;

        if (is<RenderListBox>(*renderer)) {
            auto* scrollableArea = static_cast<ScrollableArea*>(downcast<RenderListBox>(renderer));
            if (scrollableArea->isScrollableOrRubberbandable())
                return scrollableArea;
        }

        auto* layer = renderer->enclosingLayer();
        if (!layer)
            return nullptr;

        if (auto* scrollableLayer = layer->enclosingScrollableLayer(IncludeSelfOrNot::IncludeSelf, CrossFrameBoundaries::No)) {
            if (!scrollableLayer->isRenderViewLayer())
                return scrollableLayer;
        }
    }

    return m_frame.view();
}

bool EventHandler::mouseMoved(const PlatformMouseEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());
    MaximumDurationTracker maxDurationTracker(&m_maxMouseMovedDuration);

    if (m_frame.page() && m_frame.page()->pageOverlayController().handleMouseEvent(event))
        return true;

    HitTestResult hitTestResult;
    bool result = handleMouseMoveEvent(event, &hitTestResult);

    Page* page = m_frame.page();
    if (!page)
        return result;

    hitTestResult.setToNonUserAgentShadowAncestor();
    page->chrome().mouseDidMoveOverElement(hitTestResult, event.modifierFlags());
    return result;
}

bool EventHandler::passMouseMovedEventToScrollbars(const PlatformMouseEvent& event)
{
    HitTestResult hitTestResult;
    return handleMouseMoveEvent(event, &hitTestResult, true);
}

bool EventHandler::handleMouseMoveEvent(const PlatformMouseEvent& platformMouseEvent, HitTestResult* hitTestResult, bool onlyUpdateScrollbars)
{
#if ENABLE(TOUCH_EVENTS)
    bool defaultPrevented = dispatchSyntheticTouchEventIfEnabled(platformMouseEvent);
    if (defaultPrevented)
        return true;
#endif

    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

#if ENABLE(POINTER_LOCK)
    if (m_frame.page()->pointerLockController().isLocked()) {
        m_frame.page()->pointerLockController().dispatchLockedMouseEvent(platformMouseEvent, eventNames().mousemoveEvent);
        return true;
    }
#endif

    setLastKnownMousePosition(platformMouseEvent);

    if (m_hoverTimer.isActive())
        m_hoverTimer.stop();

    m_hasScheduledCursorUpdate = false;

#if !ENABLE(IOS_TOUCH_EVENTS)
    cancelFakeMouseMoveEvent();
#endif

    if (m_svgPan) {
        downcast<SVGDocument>(*m_frame.document()).updatePan(m_frame.view()->windowToContents(m_lastKnownMousePosition));
        return true;
    }

    if (m_frameSetBeingResized)
        return !dispatchMouseEvent(eventNames().mousemoveEvent, m_frameSetBeingResized.get(), 0, platformMouseEvent, FireMouseOverOut::No);

    // On iOS, our scrollbars are managed by UIKit.
#if !PLATFORM(IOS_FAMILY)
    // Send events right to a scrollbar if the mouse is pressed.
    if (m_lastScrollbarUnderMouse && m_mousePressed)
        return m_lastScrollbarUnderMouse->mouseMoved(platformMouseEvent);
#endif

    OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Move, HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowFrameScrollbars };
    if (m_mousePressed)
        hitType.add(HitTestRequest::Active);
    else if (onlyUpdateScrollbars) {
        // Mouse events should be treated as "read-only" if we're updating only scrollbars. This  
        // means that :hover and :active freeze in the state they were in, rather than updating  
        // for nodes the mouse moves while the window is not key (which will be the case if 
        // onlyUpdateScrollbars is true). 
        hitType.add(HitTestRequest::ReadOnly);
    }

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    // Treat any mouse move events as readonly if the user is currently touching the screen.
    if (m_touchPressed) {
        hitType.add(HitTestRequest::Active);
        hitType.add(HitTestRequest::ReadOnly);
    }
#endif
    HitTestRequest request(hitType);
    MouseEventWithHitTestResults mouseEvent = prepareMouseEvent(request, platformMouseEvent);
    if (hitTestResult)
        *hitTestResult = mouseEvent.hitTestResult();

    if (m_resizeLayer && m_resizeLayer->inResizeMode())
        m_resizeLayer->resize(platformMouseEvent, m_offsetFromResizeCorner);
    else {
        Scrollbar* scrollbar = mouseEvent.scrollbar();
        updateLastScrollbarUnderMouse(scrollbar, m_mousePressed ? SetOrClearLastScrollbar::Clear : SetOrClearLastScrollbar::Set);

        // On iOS, our scrollbars are managed by UIKit.
#if !PLATFORM(IOS_FAMILY)
        if (!m_mousePressed && scrollbar)
            scrollbar->mouseMoved(platformMouseEvent); // Handle hover effects on platforms that support visual feedback on scrollbar hovering.
#endif
        if (onlyUpdateScrollbars) {
            if (shouldSendMouseEventsToInactiveWindows())
                updateMouseEventTargetNode(eventNames().mousemoveEvent, mouseEvent.targetNode(), platformMouseEvent, FireMouseOverOut::Yes);

            return true;
        }
    }

    bool swallowEvent = false;
    RefPtr<Frame> newSubframe = m_capturingMouseEventsElement.get() ? subframeForTargetNode(m_capturingMouseEventsElement.get()) : subframeForHitTestResult(mouseEvent);
 
    // We want mouseouts to happen first, from the inside out.  First send a move event to the last subframe so that it will fire mouseouts.
    if (m_lastMouseMoveEventSubframe && m_lastMouseMoveEventSubframe->tree().isDescendantOf(&m_frame) && m_lastMouseMoveEventSubframe != newSubframe)
        passMouseMoveEventToSubframe(mouseEvent, *m_lastMouseMoveEventSubframe);

    if (newSubframe) {
        // Update over/out state before passing the event to the subframe.
        updateMouseEventTargetNode(eventNames().mousemoveEvent, mouseEvent.targetNode(), platformMouseEvent, FireMouseOverOut::Yes);
        
        // Event dispatch in updateMouseEventTargetNode may have caused the subframe of the target
        // node to be detached from its FrameView, in which case the event should not be passed.
        if (newSubframe->view())
            swallowEvent |= passMouseMoveEventToSubframe(mouseEvent, *newSubframe, hitTestResult);
    }

    if (!newSubframe || mouseEvent.scrollbar()) {
        if (auto* view = m_frame.view())
            updateCursor(*view, mouseEvent.hitTestResult(), platformMouseEvent.shiftKey());
    }

    m_lastMouseMoveEventSubframe = newSubframe;

    if (swallowEvent)
        return true;
    
    swallowEvent = !dispatchMouseEvent(eventNames().mousemoveEvent, mouseEvent.targetNode(), 0, platformMouseEvent, FireMouseOverOut::Yes);

#if ENABLE(DRAG_SUPPORT)
    if (!swallowEvent)
        swallowEvent = handleMouseDraggedEvent(mouseEvent);
#endif

    return swallowEvent;
}

bool EventHandler::shouldSendMouseEventsToInactiveWindows() const
{
#if PLATFORM(GTK)
    return true;
#endif
    return false;
}

void EventHandler::invalidateClick()
{
    m_clickCount = 0;
    m_clickNode = nullptr;
}

static RefPtr<Node> targetNodeForClickEvent(Node* mousePressNode, Node* mouseReleaseNode)
{
    if (!mousePressNode || !mouseReleaseNode)
        return nullptr;

    if (mousePressNode == mouseReleaseNode)
        return mouseReleaseNode;

    // If mousePressNode and mouseReleaseNode differ, we should fire the event at their common ancestor if there is one.
    if (&mousePressNode->document() == &mouseReleaseNode->document()) {
        if (auto commonAncestor = commonInclusiveAncestor(*mousePressNode, *mouseReleaseNode))
            return commonAncestor;
    }

    auto mouseReleaseShadowHost = mouseReleaseNode->shadowHost();
    if (mouseReleaseShadowHost && mouseReleaseShadowHost == mousePressNode->shadowHost()) {
        // We want to dispatch the click to the shadow tree host element to give listeners the illusion that the
        // shadom tree is a single element. For example, we want to give the illusion that <input type="range">
        // is a single element even though it is a composition of multiple shadom tree elements.
        return mouseReleaseShadowHost;
    }

    return nullptr;
}

bool EventHandler::handleMouseReleaseEvent(const PlatformMouseEvent& platformMouseEvent)
{
    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

    m_frame.selection().setCaretBlinkingSuspended(false);

#if ENABLE(POINTER_LOCK)
    if (m_frame.page()->pointerLockController().isLocked()) {
        m_frame.page()->pointerLockController().dispatchLockedMouseEvent(platformMouseEvent, eventNames().mouseupEvent);
        return true;
    }
#endif

    if (m_frame.page()->pageOverlayController().handleMouseEvent(platformMouseEvent))
        return true;

#if ENABLE(TOUCH_EVENTS)
    bool defaultPrevented = dispatchSyntheticTouchEventIfEnabled(platformMouseEvent);
    if (defaultPrevented)
        return true;
#endif

    UserGestureIndicator gestureIndicator(ProcessingUserGesture, m_frame.document());

#if ENABLE(PAN_SCROLLING)
    m_autoscrollController->handleMouseReleaseEvent(platformMouseEvent);
#endif

    m_mousePressed = false;
    setLastKnownMousePosition(platformMouseEvent);

    if (m_svgPan) {
        m_svgPan = false;
        downcast<SVGDocument>(*m_frame.document()).updatePan(m_frame.view()->windowToContents(m_lastKnownMousePosition));
        return true;
    }

    if (m_frameSetBeingResized)
        return !dispatchMouseEvent(eventNames().mouseupEvent, m_frameSetBeingResized.get(), m_clickCount, platformMouseEvent, FireMouseOverOut::No);

    // If an immediate action began or was completed using this series of mouse events, then we should send mouseup to
    // the DOM and return now so that we don't perform our own default behaviors.
    if (m_immediateActionStage == ImmediateActionStage::ActionCompleted || m_immediateActionStage == ImmediateActionStage::ActionUpdated || m_immediateActionStage == ImmediateActionStage::ActionCancelledAfterUpdate) {
        m_immediateActionStage = ImmediateActionStage::None;
        return !dispatchMouseEvent(eventNames().mouseupEvent, m_lastElementUnderMouse.get(), m_clickCount, platformMouseEvent, FireMouseOverOut::No);
    }
    m_immediateActionStage = ImmediateActionStage::None;

    if (m_lastScrollbarUnderMouse) {
        invalidateClick();
        m_lastScrollbarUnderMouse->mouseUp(platformMouseEvent);
        return !dispatchMouseEvent(eventNames().mouseupEvent, m_lastElementUnderMouse.get(), m_clickCount, platformMouseEvent, FireMouseOverOut::No);
    }

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Release, HitTestRequest::DisallowUserAgentShadowContent };
    MouseEventWithHitTestResults mouseEvent = prepareMouseEvent(hitType, platformMouseEvent);
    RefPtr<Frame> subframe = m_capturingMouseEventsElement.get() ? subframeForTargetNode(m_capturingMouseEventsElement.get()) : subframeForHitTestResult(mouseEvent);
    if (m_eventHandlerWillResetCapturingMouseEventsElement)
        m_capturingMouseEventsElement = nullptr;
    if (subframe && passMouseReleaseEventToSubframe(mouseEvent, *subframe))
        return true;

    bool swallowMouseUpEvent = !dispatchMouseEvent(eventNames().mouseupEvent, mouseEvent.targetNode(), m_clickCount, platformMouseEvent, FireMouseOverOut::No);

    bool contextMenuEvent = platformMouseEvent.button() == RightButton;

    auto nodeToClick = targetNodeForClickEvent(m_clickNode.get(), mouseEvent.targetNode());
    bool swallowClickEvent = m_clickCount > 0 && !contextMenuEvent && nodeToClick && !dispatchMouseEvent(eventNames().clickEvent, nodeToClick.get(), m_clickCount, platformMouseEvent, FireMouseOverOut::Yes);

    if (m_resizeLayer) {
        m_resizeLayer->setInResizeMode(false);
        m_resizeLayer = nullptr;
    }

    bool swallowMouseReleaseEvent = false;
    if (!swallowMouseUpEvent)
        swallowMouseReleaseEvent = handleMouseReleaseEvent(mouseEvent);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

#if ENABLE(MOUSE_FORCE_EVENTS)
bool EventHandler::handleMouseForceEvent(const PlatformMouseEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

#if ENABLE(POINTER_LOCK)
    if (m_frame.page()->pointerLockController().isLocked()) {
        m_frame.page()->pointerLockController().dispatchLockedMouseEvent(event, eventNames().webkitmouseforcechangedEvent);
        if (event.type() == PlatformEvent::MouseForceDown)
            m_frame.page()->pointerLockController().dispatchLockedMouseEvent(event, eventNames().webkitmouseforcedownEvent);
        if (event.type() == PlatformEvent::MouseForceUp)
            m_frame.page()->pointerLockController().dispatchLockedMouseEvent(event, eventNames().webkitmouseforceupEvent);
        return true;
    }
#endif

    setLastKnownMousePosition(event);

    OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::DisallowUserAgentShadowContent };

    if (event.force())
        hitType.add(HitTestRequest::Active);

    auto mouseEvent = prepareMouseEvent(hitType, event);

    bool swallowedEvent = !dispatchMouseEvent(eventNames().webkitmouseforcechangedEvent, mouseEvent.targetNode(), 0, event, FireMouseOverOut::No);
    if (event.type() == PlatformEvent::MouseForceDown)
        swallowedEvent |= !dispatchMouseEvent(eventNames().webkitmouseforcedownEvent, mouseEvent.targetNode(), 0, event, FireMouseOverOut::No);
    if (event.type() == PlatformEvent::MouseForceUp)
        swallowedEvent |= !dispatchMouseEvent(eventNames().webkitmouseforceupEvent, mouseEvent.targetNode(), 0, event, FireMouseOverOut::No);

    return swallowedEvent;
}
#else
bool EventHandler::handleMouseForceEvent(const PlatformMouseEvent& )
{
    return false;
}
#endif // #if ENABLE(MOUSE_FORCE_EVENTS)

bool EventHandler::handlePasteGlobalSelection(const PlatformMouseEvent& platformMouseEvent)
{
    // If the event was a middle click, attempt to copy global selection in after
    // the newly set caret position.
    //
    // This code is called from either the mouse up or mouse down handling. There
    // is some debate about when the global selection is pasted:
    //   xterm: pastes on up.
    //   GTK: pastes on down.
    //   Qt: pastes on up.
    //   Firefox: pastes on up.
    //   Chromium: pastes on up.
    //
    // There is something of a webcompat angle to this well, as highlighted by
    // crbug.com/14608. Pages can clear text boxes 'onclick' and, if we paste on
    // down then the text is pasted just before the onclick handler runs and
    // clears the text box. So it's important this happens after the event
    // handlers have been fired.
#if PLATFORM(GTK)
    if (platformMouseEvent.type() != PlatformEvent::MousePressed)
        return false;
#else
    if (platformMouseEvent.type() != PlatformEvent::MouseReleased)
        return false;
#endif

    if (!m_frame.page())
        return false;
    Frame& focusFrame = m_frame.page()->focusController().focusedOrMainFrame();
    // Do not paste here if the focus was moved somewhere else.
    if (&m_frame == &focusFrame && m_frame.editor().client()->supportsGlobalSelection())
        return m_frame.editor().command("PasteGlobalSelection"_s).execute();

    return false;
}

#if ENABLE(DRAG_SUPPORT)

bool EventHandler::dispatchDragEvent(const AtomString& eventType, Element& dragTarget, const PlatformMouseEvent& event, DataTransfer& dataTransfer)
{
    Ref<Frame> protectedFrame(m_frame);
    FrameView* view = m_frame.view();

    // FIXME: We might want to dispatch a dragleave even if the view is gone.
    if (!view)
        return false;

    auto dragEvent = DragEvent::create(eventType, Event::CanBubble::Yes, Event::IsCancelable::Yes, Event::IsComposed::Yes,
        event.timestamp().approximateMonotonicTime(), &m_frame.windowProxy(), 0,
        event.globalPosition(), event.position(),
#if ENABLE(POINTER_LOCK)
        event.movementDelta(),
#else
        { },
#endif
        event.modifiers(), 0, 0, nullptr, event.force(), NoTap, &dataTransfer);

    dragTarget.dispatchEvent(dragEvent);

    if (auto* cache = m_frame.document()->existingAXObjectCache()) {
        if (eventType == eventNames().dragstartEvent)
            cache->postNotification(&dragTarget, AXObjectCache::AXDraggingStarted);
        else if (eventType == eventNames().dragendEvent)
            cache->postNotification(&dragTarget, AXObjectCache::AXDraggingEnded);
        else if (eventType == eventNames().dragenterEvent)
            cache->postNotification(&dragTarget, AXObjectCache::AXDraggingEnteredDropZone);
        else if (eventType == eventNames().dragleaveEvent)
            cache->postNotification(&dragTarget, AXObjectCache::AXDraggingExitedDropZone);
        else if (eventType == eventNames().dropEvent)
            cache->postNotification(&dragTarget, AXObjectCache::AXDraggingDropped);
    }

    return dragEvent->defaultPrevented();
}

Element* EventHandler::draggingElement() const
{
    return dragState().source.get();
}

static bool targetIsFrame(Node* target, Frame*& frame)
{
    if (!is<HTMLFrameElementBase>(target))
        return false;

    frame = downcast<HTMLFrameElementBase>(*target).contentFrame();
    return true;
}

static Optional<DragOperation> convertDropZoneOperationToDragOperation(const String& dragOperation)
{
    if (dragOperation == "copy")
        return DragOperation::Copy;
    if (dragOperation == "move")
        return DragOperation::Move;
    if (dragOperation == "link")
        return DragOperation::Link;
    return WTF::nullopt;
}

static String convertDragOperationToDropZoneOperation(Optional<DragOperation> operation)
{
    if (operation) {
        switch (*operation) {
        case DragOperation::Move:
            return "move"_s;
        case DragOperation::Link:
            return "link"_s;
        default:
            break;
        }
    }
    return "copy"_s;
}

static bool hasDropZoneType(DataTransfer& dataTransfer, const String& keyword)
{
    if (keyword.startsWith("file:"))
        return dataTransfer.hasFileOfType(keyword.substring(5));

    if (keyword.startsWith("string:"))
        return dataTransfer.hasStringOfType(keyword.substring(7));

    return false;
}

static bool findDropZone(Node& target, DataTransfer& dataTransfer)
{
    RefPtr<Element> element = is<Element>(target) ? &downcast<Element>(target) : target.parentElement();
    for (; element; element = element->parentElement()) {
        SpaceSplitString keywords(element->attributeWithoutSynchronization(webkitdropzoneAttr), true);
        bool matched = false;
        Optional<DragOperation> dragOperation;
        for (unsigned i = 0, size = keywords.size(); i < size; ++i) {
            if (auto operationFromKeyword = convertDropZoneOperationToDragOperation(keywords[i])) {
                if (!dragOperation)
                    dragOperation = operationFromKeyword;
            } else
                matched = matched || hasDropZoneType(dataTransfer, keywords[i].string());
            if (matched && dragOperation)
                break;
        }
        if (matched) {
            dataTransfer.setDropEffect(convertDragOperationToDropZoneOperation(dragOperation));
            return true;
        }
    }
    return false;
}

EventHandler::DragTargetResponse EventHandler::dispatchDragEnterOrDragOverEvent(const AtomString& eventType, Element& target, const PlatformMouseEvent& event, std::unique_ptr<Pasteboard>&& pasteboard, OptionSet<DragOperation> sourceOperationMask, bool draggingFiles)
{
    auto dataTransfer = DataTransfer::createForUpdatingDropTarget(target.document(), WTFMove(pasteboard), sourceOperationMask, draggingFiles);
    bool accept = dispatchDragEvent(eventType, target, event, dataTransfer.get());
    if (!accept)
        accept = findDropZone(target, dataTransfer);
    dataTransfer->makeInvalidForSecurity();
    if (accept && !dataTransfer->dropEffectIsUninitialized())
        return { true, dataTransfer->destinationOperationMask() };
    return { accept, WTF::nullopt };
}

EventHandler::DragTargetResponse EventHandler::updateDragAndDrop(const PlatformMouseEvent& event, const std::function<std::unique_ptr<Pasteboard>()>& makePasteboard, OptionSet<DragOperation> sourceOperationMask, bool draggingFiles)
{
    Ref<Frame> protectedFrame(m_frame);
    if (!m_frame.view())
        return { };

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::DisallowUserAgentShadowContent };
    MouseEventWithHitTestResults mouseEvent = prepareMouseEvent(hitType, event);

    RefPtr<Element> newTarget;
    if (Node* targetNode = mouseEvent.targetNode()) {
        // Drag events should never go to non-element nodes (following IE, and proper mouseover/out dispatch)
        if (!is<Element>(*targetNode))
            newTarget = targetNode->parentOrShadowHostElement();
        else
            newTarget = downcast<Element>(targetNode);
    }

    m_autoscrollController->updateDragAndDrop(newTarget.get(), event.position(), event.timestamp());

    DragTargetResponse response;
    if (m_dragTarget != newTarget) {
        // FIXME: this ordering was explicitly chosen to match WinIE. However,
        // it is sometimes incorrect when dragging within subframes, as seen with
        // LayoutTests/fast/events/drag-in-frames.html.
        //
        // Moreover, this ordering conforms to section 7.9.4 of the HTML 5 spec. <http://dev.w3.org/html5/spec/Overview.html#drag-and-drop-processing-model>.
        Frame* targetFrame;
        if (targetIsFrame(newTarget.get(), targetFrame)) {
            if (targetFrame)
                response = targetFrame->eventHandler().updateDragAndDrop(event, makePasteboard, sourceOperationMask, draggingFiles);
        } else if (newTarget) {
            // As per section 7.9.4 of the HTML 5 spec., we must always fire a drag event before firing a dragenter, dragleave, or dragover event.
            dispatchEventToDragSourceElement(eventNames().dragEvent, event);
            response = dispatchDragEnterOrDragOverEvent(eventNames().dragenterEvent, *newTarget, event, makePasteboard(), sourceOperationMask, draggingFiles);
        }

        if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
            // FIXME: Recursing again here doesn't make sense if the newTarget and m_dragTarget were in the same frame.
            if (targetFrame)
                response = targetFrame->eventHandler().updateDragAndDrop(event, makePasteboard, sourceOperationMask, draggingFiles);
        } else if (m_dragTarget) {
            auto dataTransfer = DataTransfer::createForUpdatingDropTarget(m_dragTarget->document(), makePasteboard(), sourceOperationMask, draggingFiles);
            dispatchDragEvent(eventNames().dragleaveEvent, *m_dragTarget, event, dataTransfer.get());
            dataTransfer->makeInvalidForSecurity();
        }

        if (newTarget) {
            // We do not explicitly call dispatchDragEvent here because it could ultimately result in the appearance that
            // two dragover events fired. So, we mark that we should only fire a dragover event on the next call to this function.
            m_shouldOnlyFireDragOverEvent = true;
        }
    } else {
        Frame* targetFrame;
        if (targetIsFrame(newTarget.get(), targetFrame)) {
            if (targetFrame)
                response = targetFrame->eventHandler().updateDragAndDrop(event, makePasteboard, sourceOperationMask, draggingFiles);
        } else if (newTarget) {
            // Note, when dealing with sub-frames, we may need to fire only a dragover event as a drag event may have been fired earlier.
            if (!m_shouldOnlyFireDragOverEvent)
                dispatchEventToDragSourceElement(eventNames().dragEvent, event);
            response = dispatchDragEnterOrDragOverEvent(eventNames().dragoverEvent, *newTarget, event, makePasteboard(), sourceOperationMask, draggingFiles);
            m_shouldOnlyFireDragOverEvent = false;
        }
    }
    m_dragTarget = WTFMove(newTarget);
    return response;
}

void EventHandler::cancelDragAndDrop(const PlatformMouseEvent& event, std::unique_ptr<Pasteboard>&& pasteboard, OptionSet<DragOperation> sourceOperationMask, bool draggingFiles)
{
    Ref<Frame> protectedFrame(m_frame);

    Frame* targetFrame;
    if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
        if (targetFrame)
            targetFrame->eventHandler().cancelDragAndDrop(event, WTFMove(pasteboard), sourceOperationMask, draggingFiles);
    } else if (m_dragTarget) {
        dispatchEventToDragSourceElement(eventNames().dragEvent, event);

        auto dataTransfer = DataTransfer::createForUpdatingDropTarget(m_dragTarget->document(), WTFMove(pasteboard), sourceOperationMask, draggingFiles);
        dispatchDragEvent(eventNames().dragleaveEvent, *m_dragTarget, event, dataTransfer.get());
        dataTransfer->makeInvalidForSecurity();
    }
    clearDragState();
}

bool EventHandler::performDragAndDrop(const PlatformMouseEvent& event, std::unique_ptr<Pasteboard>&& pasteboard, OptionSet<DragOperation> sourceOperationMask, bool draggingFiles)
{
    Ref<Frame> protectedFrame(m_frame);

    Frame* targetFrame;
    bool preventedDefault = false;
    if (targetIsFrame(m_dragTarget.get(), targetFrame)) {
        if (targetFrame)
            preventedDefault = targetFrame->eventHandler().performDragAndDrop(event, WTFMove(pasteboard), sourceOperationMask, draggingFiles);
    } else if (m_dragTarget) {
        auto dataTransfer = DataTransfer::createForDrop(m_dragTarget->document(), WTFMove(pasteboard), sourceOperationMask, draggingFiles);
        preventedDefault = dispatchDragEvent(eventNames().dropEvent, *m_dragTarget, event, dataTransfer);
        dataTransfer->makeInvalidForSecurity();
    }
    clearDragState();
    return preventedDefault;
}

void EventHandler::clearDragState()
{
    stopAutoscrollTimer();
    m_dragTarget = nullptr;
    m_capturingMouseEventsElement = nullptr;
    m_shouldOnlyFireDragOverEvent = false;
#if PLATFORM(COCOA)
    m_sendingEventToSubview = false;
#endif
}

#endif // ENABLE(DRAG_SUPPORT)

void EventHandler::setCapturingMouseEventsElement(Element* element)
{
    m_capturingMouseEventsElement = element;
    m_eventHandlerWillResetCapturingMouseEventsElement = false;
}

void EventHandler::pointerCaptureElementDidChange(Element* element)
{
    if (m_capturingMouseEventsElement == element)
        return;

    setCapturingMouseEventsElement(element);

    // Now that we have a new capture element, we need to dispatch boundary mouse events.
    updateMouseEventTargetNode(eventNames().gotpointercaptureEvent, element, m_lastPlatformMouseEvent, FireMouseOverOut::Yes);
}

MouseEventWithHitTestResults EventHandler::prepareMouseEvent(const HitTestRequest& request, const PlatformMouseEvent& mouseEvent)
{
    m_lastPlatformMouseEvent = mouseEvent;
    Ref<Frame> protectedFrame(m_frame);
    ASSERT(m_frame.document());
    return m_frame.document()->prepareMouseEvent(request, documentPointForWindowPoint(m_frame, mouseEvent.position()), mouseEvent);
}

static bool hierarchyHasCapturingEventListeners(Element* element, const AtomString& pointerEventName, const AtomString& compatibilityMouseEventName)
{
    for (ContainerNode* curr = element; curr; curr = curr->parentInComposedTree()) {
        if (curr->hasCapturingEventListeners(pointerEventName) || curr->hasCapturingEventListeners(compatibilityMouseEventName))
            return true;
    }
    return false;
}

void EventHandler::updateMouseEventTargetNode(const AtomString& eventType, Node* targetNode, const PlatformMouseEvent& platformMouseEvent, FireMouseOverOut fireMouseOverOut)
{
    Ref<Frame> protectedFrame(m_frame);
    Element* targetElement = nullptr;
    
    // If we're capturing, we always go right to that element.
    if (m_capturingMouseEventsElement)
        targetElement = m_capturingMouseEventsElement.get();
    else if (targetNode) {
        // If the target node is a non-element, dispatch on the parent. <rdar://problem/4196646>
        while (targetNode && !is<Element>(*targetNode))
            targetNode = targetNode->parentInComposedTree();
        targetElement = downcast<Element>(targetNode);
    }

    m_elementUnderMouse = targetElement;

    ASSERT_IMPLIES(m_elementUnderMouse, &m_elementUnderMouse->document() == m_frame.document());
    ASSERT_IMPLIES(m_lastElementUnderMouse, &m_lastElementUnderMouse->document() == m_frame.document());

    // Fire mouseout/mouseover if the mouse has shifted to a different node.
    if (fireMouseOverOut == FireMouseOverOut::Yes) {
        notifyScrollableAreasOfMouseEvents(eventType, m_lastElementUnderMouse.get(), m_elementUnderMouse.get());

        if (m_lastElementUnderMouse && &m_lastElementUnderMouse->document() != m_frame.document()) {
            m_lastElementUnderMouse = nullptr;
            m_lastScrollbarUnderMouse = nullptr;
        }

        if (m_lastElementUnderMouse != m_elementUnderMouse) {
            // mouseenter and mouseleave events are only dispatched if there is a capturing eventhandler on an ancestor
            // or a normal eventhandler on the element itself (they don't bubble).
            // This optimization is necessary since these events can cause O(n^2) capturing event-handler checks.
            bool hasCapturingMouseEnterListener = hierarchyHasCapturingEventListeners(m_elementUnderMouse.get(), eventNames().pointerenterEvent, eventNames().mouseenterEvent);
            bool hasCapturingMouseLeaveListener = hierarchyHasCapturingEventListeners(m_lastElementUnderMouse.get(), eventNames().pointerleaveEvent, eventNames().mouseleaveEvent);

            Vector<Ref<Element>, 32> leftElementsChain;
            for (Element* element = m_lastElementUnderMouse.get(); element; element = element->parentElementInComposedTree())
                leftElementsChain.append(*element);
            Vector<Ref<Element>, 32> enteredElementsChain;
            for (Element* element = m_elementUnderMouse.get(); element; element = element->parentElementInComposedTree())
                enteredElementsChain.append(*element);

            if (!leftElementsChain.isEmpty() && !enteredElementsChain.isEmpty() && leftElementsChain.last().ptr() == enteredElementsChain.last().ptr()) {
                size_t minHeight = std::min(leftElementsChain.size(), enteredElementsChain.size());
                size_t i;
                for (i = 0; i < minHeight; ++i) {
                    if (leftElementsChain[leftElementsChain.size() - i - 1].ptr() != enteredElementsChain[enteredElementsChain.size() - i - 1].ptr())
                        break;
                }
                leftElementsChain.shrink(leftElementsChain.size() - i);
                enteredElementsChain.shrink(enteredElementsChain.size() - i);
            }

            if (m_lastElementUnderMouse)
                m_lastElementUnderMouse->dispatchMouseEvent(platformMouseEvent, eventNames().mouseoutEvent, 0, m_elementUnderMouse.get());

            for (auto& chain : leftElementsChain) {
                if (hasCapturingMouseLeaveListener || chain->hasEventListeners(eventNames().pointerleaveEvent) || chain->hasEventListeners(eventNames().mouseleaveEvent))
                    chain->dispatchMouseEvent(platformMouseEvent, eventNames().mouseleaveEvent, 0, m_elementUnderMouse.get());
            }

            if (m_elementUnderMouse)
                m_elementUnderMouse->dispatchMouseEvent(platformMouseEvent, eventNames().mouseoverEvent, 0, m_lastElementUnderMouse.get());

            for (auto& chain : WTF::makeReversedRange(enteredElementsChain)) {
                if (hasCapturingMouseEnterListener || chain->hasEventListeners(eventNames().pointerenterEvent) || chain->hasEventListeners(eventNames().mouseenterEvent))
                    chain->dispatchMouseEvent(platformMouseEvent, eventNames().mouseenterEvent, 0, m_lastElementUnderMouse.get());
            }
        }

        // Event handling may have moved the element to a different document.
        if (m_elementUnderMouse && &m_elementUnderMouse->document() != m_frame.document())
            m_elementUnderMouse = nullptr;

        m_lastElementUnderMouse = m_elementUnderMouse;
    }
}

void EventHandler::notifyScrollableAreasOfMouseEvents(const AtomString& eventType, Element* lastElementUnderMouse, Element* elementUnderMouse)
{
    auto* frameView = m_frame.view();
    if (!frameView)
        return;

    auto scrollableAreaForLastNode = enclosingScrollableArea(lastElementUnderMouse);
    auto scrollableAreaForNodeUnderMouse = enclosingScrollableArea(elementUnderMouse);

    if (!!lastElementUnderMouse != !!elementUnderMouse) {
        if (elementUnderMouse) {
            if (scrollableAreaForNodeUnderMouse != frameView)
                frameView->mouseEnteredContentArea();

            if (scrollableAreaForNodeUnderMouse)
                scrollableAreaForNodeUnderMouse->mouseEnteredContentArea();
        } else {
            if (scrollableAreaForLastNode)
                scrollableAreaForLastNode->mouseExitedContentArea();

            if (scrollableAreaForLastNode != frameView)
                frameView->mouseExitedContentArea();
        }
        return;
    }

    if (!scrollableAreaForLastNode && !scrollableAreaForNodeUnderMouse)
        return;

    // FIXME: This does doesn't handle nested ScrollableAreas well. It really needs to know
    // the hierarchical relationship between scrollableAreaForLastNode and scrollableAreaForNodeUnderMouse.
    bool movedBetweenScrollableaAreas = scrollableAreaForLastNode && scrollableAreaForNodeUnderMouse && (scrollableAreaForLastNode != scrollableAreaForNodeUnderMouse);
    if (eventType == eventNames().mousemoveEvent) {
        frameView->mouseMovedInContentArea();

        if (!movedBetweenScrollableaAreas && scrollableAreaForNodeUnderMouse && scrollableAreaForNodeUnderMouse != frameView)
            scrollableAreaForNodeUnderMouse->mouseMovedInContentArea();
    }

    if (!movedBetweenScrollableaAreas)
        return;

    if (scrollableAreaForLastNode && scrollableAreaForLastNode != frameView)
        scrollableAreaForLastNode->mouseExitedContentArea();

    if (scrollableAreaForNodeUnderMouse && scrollableAreaForNodeUnderMouse != frameView)
        scrollableAreaForNodeUnderMouse->mouseEnteredContentArea();
}

static RefPtr<Element> findFirstMouseFocusableElementInComposedTree(Element& host)
{
    ASSERT(host.shadowRoot());
    for (auto& node : composedTreeDescendants(host)) {
        if (!is<Element>(node))
            continue;
        auto& element = downcast<Element>(node);
        if (element.isMouseFocusable())
            return &element;
    }
    return nullptr;
}

bool EventHandler::dispatchMouseEvent(const AtomString& eventType, Node* targetNode, int clickCount, const PlatformMouseEvent& platformMouseEvent, FireMouseOverOut fireMouseOverOut)
{
    Ref<Frame> protectedFrame(m_frame);

    updateMouseEventTargetNode(eventType, targetNode, platformMouseEvent, fireMouseOverOut);

    if (m_elementUnderMouse && !m_elementUnderMouse->dispatchMouseEvent(platformMouseEvent, eventType, clickCount))
        return false;

    if (eventType != eventNames().mousedownEvent)
        return true;

    // If clicking on a frame scrollbar, do not make any change to which element is focused.
    auto* view = m_frame.view();
    if (view && view->scrollbarAtPoint(platformMouseEvent.position()))
        return true;

    // The layout needs to be up to date to determine if an element is focusable.
    m_frame.document()->updateLayoutIgnorePendingStylesheets();

    // Remove focus from the currently focused element when a link or button is clicked.
    // This is expected by some sites that rely on change event handlers running
    // from form fields before the button click is processed, behavior that was inherited
    // from the user interface of Windows, where pushing a button moves focus to the button.

    // Walk up the DOM tree to search for an element to focus.
    RefPtr<Element> element;
    for (element = m_elementUnderMouse.get(); element; element = element->parentElementInComposedTree()) {
        if (auto* shadowRoot = element->shadowRoot()) {
            if (shadowRoot->delegatesFocus()) {
                element = findFirstMouseFocusableElementInComposedTree(*element);
                break;
            }
        }
        if (element->isMouseFocusable())
            break;
    }

    // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus an
    // element on mouse down if it's selected and inside a focused element. It will be
    // focused if the user does a mouseup over it, however, because the mouseup
    // will set a selection inside it, which will also set the focused element.
    if (element && m_frame.selection().isRange()) {
        if (auto range = m_frame.selection().selection().toNormalizedRange()) {
            auto result = createLiveRange(*range)->compareNode(*element);
            if (!result.hasException() && result.releaseReturnValue() == Range::NODE_INSIDE && element->isDescendantOf(m_frame.document()->focusedElement()))
                return true;
        }
    }

    // Only change the focus when clicking scrollbars if it can be transferred to a mouse focusable node.
    if (!element && isInsideScrollbar(platformMouseEvent.position()))
        return false;

    // If focus shift is blocked, we eat the event.
    auto* page = m_frame.page();
    if (page && !page->focusController().setFocusedElement(element.get(), m_frame))
        return false;

    return true;
}

bool EventHandler::isInsideScrollbar(const IntPoint& windowPoint) const
{
    if (auto* document = m_frame.document()) {
        HitTestResult result { windowPoint };
        document->hitTest(OptionSet<HitTestRequest::RequestType> { HitTestRequest::ReadOnly, HitTestRequest::DisallowUserAgentShadowContent }, result);
        return result.scrollbar();
    }

    return false;
}

#if !USE(GLIB)

bool EventHandler::shouldSwapScrollDirection(const HitTestResult&, const PlatformWheelEvent&) const
{
    return false;
}

#endif

void EventHandler::clearLatchedStateTimerFired()
{
    LOG(ScrollLatching, "EventHandler %p clearLatchedStateTimerFired()", this);
    clearLatchedState();
}

#if !PLATFORM(MAC)

void EventHandler::determineWheelEventTarget(const PlatformWheelEvent&, const HitTestResult&, RefPtr<Element>&, RefPtr<ContainerNode>&, WeakPtr<ScrollableArea>&, bool&)
{
}

void EventHandler::recordWheelEventForDeltaFilter(const PlatformWheelEvent& event)
{
    if (auto* page = m_frame.page())
        page->wheelEventDeltaFilter()->updateFromDelta(FloatSize(event.deltaX(), event.deltaY()));
}

bool EventHandler::processWheelEventForScrolling(const PlatformWheelEvent& event, ContainerNode*, const WeakPtr<ScrollableArea>&)
{
    Ref<Frame> protectedFrame(m_frame);

    // We do another check on the frame view because the event handler can run JS which results in the frame getting destroyed.
    FrameView* view = m_frame.view();
    
    bool didHandleEvent = view ? view->wheelEvent(event) : false;
    m_isHandlingWheelEvent = false;
    return didHandleEvent;
}

bool EventHandler::platformCompletePlatformWidgetWheelEvent(const PlatformWheelEvent&, const Widget&, ContainerNode*)
{
    return true;
}

void EventHandler::processWheelEventForScrollSnap(const PlatformWheelEvent&, const WeakPtr<ScrollableArea>&)
{
}

void EventHandler::clearOrScheduleClearingLatchedStateIfNeeded(const PlatformWheelEvent&)
{
    clearLatchedState();
}
    
#if !PLATFORM(IOS_FAMILY)
    
IntPoint EventHandler::targetPositionInWindowForSelectionAutoscroll() const
{
    return m_lastKnownMousePosition;
}
    
#endif // !PLATFORM(IOS_FAMILY)
    
#endif // !PLATFORM(MAC)
    
#if !PLATFORM(IOS_FAMILY)
    
bool EventHandler::shouldUpdateAutoscroll()
{
    return mousePressed();
}
    
#endif // !PLATFORM(IOS_FAMILY)

Widget* EventHandler::widgetForEventTarget(Element* eventTarget)
{
    if (!eventTarget)
        return nullptr;

    auto* target = eventTarget->renderer();
    if (!is<RenderWidget>(target))
        return nullptr;

    return downcast<RenderWidget>(*target).widget();
}

static WeakPtr<Widget> widgetForElement(const Element& element)
{
    auto target = element.renderer();
    if (!is<RenderWidget>(target) || !downcast<RenderWidget>(*target).widget())
        return { };

    return makeWeakPtr(*downcast<RenderWidget>(*target).widget());
}

bool EventHandler::completeWidgetWheelEvent(const PlatformWheelEvent& event, const WeakPtr<Widget>& widget, const WeakPtr<ScrollableArea>& scrollableArea, ContainerNode* scrollableContainer)
{
    m_isHandlingWheelEvent = false;
    
    // We do another check on the widget because the event handler can run JS which results in the frame getting destroyed.
    if (!widget)
        return false;
    
    if (scrollableArea)
        scrollableArea->setScrollShouldClearLatchedState(false);

    processWheelEventForScrollSnap(event, scrollableArea);

    if (!widget->platformWidget())
        return true;

    return platformCompletePlatformWidgetWheelEvent(event, *widget.get(), scrollableContainer);
}

bool EventHandler::handleWheelEvent(const PlatformWheelEvent& event)
{
    auto* document = m_frame.document();
    if (!document)
        return false;

    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

    FrameView* view = m_frame.view();
    if (!view)
        return false;

#if ENABLE(POINTER_LOCK)
    if (m_frame.page()->pointerLockController().isLocked()) {
        m_frame.page()->pointerLockController().dispatchLockedWheelEvent(event);
        return true;
    }
#endif

#if PLATFORM(COCOA)
    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "EventHandler::handleWheelEvent on main thread, phase " << event.phase() << " momentum phase " << event.momentumPhase());
    if (auto monitor = m_frame.page()->wheelEventTestMonitor())
        monitor->receivedWheelEvent(event);

    WheelEventTestMonitorCompletionDeferrer deferrer(m_frame.page()->wheelEventTestMonitor().get(), this, WheelEventTestMonitor::DeferReason::HandlingWheelEventOnMainThread);
#endif

    m_isHandlingWheelEvent = true;
    setFrameWasScrolledByUser();

    HitTestRequest request;
    HitTestResult result(view->windowToContents(event.position()));
    document->hitTest(request, result);

    RefPtr<Element> element = result.targetElement();
    RefPtr<ContainerNode> scrollableContainer;
    WeakPtr<ScrollableArea> scrollableArea;
    bool isOverWidget = result.isOverWidget();
    determineWheelEventTarget(event, result, element, scrollableContainer, scrollableArea, isOverWidget);

#if ENABLE(WHEEL_EVENT_LATCHING)
    if (event.phase() == PlatformWheelEventPhaseNone && event.momentumPhase() == PlatformWheelEventPhaseNone && m_frame.page())
        m_frame.page()->resetLatchingState();
#endif

    // FIXME: It should not be necessary to do this mutation here.
    // Instead, the handlers should know convert vertical scrolls appropriately.
    PlatformWheelEvent adjustedEvent = shouldSwapScrollDirection(result, event) ? event.copySwappingDirection() : event;
    recordWheelEventForDeltaFilter(adjustedEvent);

    if (element) {
        if (isOverWidget) {
            if (WeakPtr<Widget> widget = widgetForElement(*element)) {
                if (passWheelEventToWidget(event, *widget.get()))
                    return completeWidgetWheelEvent(adjustedEvent, widget, scrollableArea, scrollableContainer.get());
            }
        }

        if (!element->dispatchWheelEvent(adjustedEvent)) {
            m_isHandlingWheelEvent = false;
            if (scrollableArea && scrollableArea->scrollShouldClearLatchedState()) {
                // Web developer is controlling scrolling, so don't attempt to latch.
                clearLatchedState();
                scrollableArea->setScrollShouldClearLatchedState(false);
            }

            processWheelEventForScrollSnap(adjustedEvent, scrollableArea);
            return true;
        }
    }

    if (scrollableArea)
        scrollableArea->setScrollShouldClearLatchedState(false);

    bool handledEvent = processWheelEventForScrolling(adjustedEvent, scrollableContainer.get(), scrollableArea);
    processWheelEventForScrollSnap(adjustedEvent, scrollableArea);
    return handledEvent;
}

void EventHandler::clearLatchedState()
{
    auto* page = m_frame.page();
    if (!page)
        return;

#if ENABLE(WHEEL_EVENT_LATCHING)
    LOG_WITH_STREAM(ScrollLatching, stream << "EventHandler::clearLatchedState()");
    page->resetLatchingState();
#endif
    if (auto filter = page->wheelEventDeltaFilter())
        filter->endFilteringDeltas();
}

void EventHandler::defaultWheelEventHandler(Node* startNode, WheelEvent& wheelEvent)
{
    if (!startNode)
        return;
    
    auto protectedFrame = makeRef(m_frame);

    FloatSize filteredPlatformDelta(wheelEvent.deltaX(), wheelEvent.deltaY());
    FloatSize filteredVelocity;
    if (auto platformWheelEvent = wheelEvent.underlyingPlatformEvent()) {
        filteredPlatformDelta.setWidth(platformWheelEvent->deltaX());
        filteredPlatformDelta.setHeight(platformWheelEvent->deltaY());
    }

    RefPtr<Element> stopElement;
#if ENABLE(WHEEL_EVENT_LATCHING)
    ScrollLatchingState* latchedState = m_frame.page() ? m_frame.page()->latchingState() : nullptr;
    stopElement = latchedState ? latchedState->previousWheelScrolledElement() : nullptr;

    if (m_frame.page() && m_frame.page()->wheelEventDeltaFilter()->isFilteringDeltas()) {
        filteredPlatformDelta = m_frame.page()->wheelEventDeltaFilter()->filteredDelta();
        filteredVelocity = m_frame.page()->wheelEventDeltaFilter()->filteredVelocity();
    }
#endif

    if (handleWheelEventInAppropriateEnclosingBox(startNode, wheelEvent, stopElement, filteredPlatformDelta, filteredVelocity))
        wheelEvent.setDefaultHandled();

#if ENABLE(WHEEL_EVENT_LATCHING)
    if (latchedState && !latchedState->wheelEventElement())
        latchedState->setPreviousWheelScrolledElement(stopElement.get());
#endif
}

#if ENABLE(CONTEXT_MENU_EVENT)
bool EventHandler::sendContextMenuEvent(const PlatformMouseEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);

    Document* doc = m_frame.document();
    FrameView* view = m_frame.view();
    if (!view)
        return false;

    // Caret blinking is normally un-suspended in handleMouseReleaseEvent, but we
    // won't receive that event once the context menu is up.
    m_frame.selection().setCaretBlinkingSuspended(false);
    // Clear mouse press state to avoid initiating a drag while context menu is up.
    m_mousePressed = false;
    bool swallowEvent;
    LayoutPoint viewportPos = view->windowToContents(event.position());
    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent };
    MouseEventWithHitTestResults mouseEvent = doc->prepareMouseEvent(hitType, viewportPos, event);

    // Do not show context menus when clicking on scrollbars.
    if (mouseEvent.scrollbar() || view->scrollbarAtPoint(event.position()))
        return false;

    if (m_frame.editor().behavior().shouldSelectOnContextualMenuClick()
        && !m_frame.selection().contains(viewportPos)
        // FIXME: In the editable case, word selection sometimes selects content that isn't underneath the mouse.
        // If the selection is non-editable, we do word selection to make it easier to use the contextual menu items
        // available for text selections.  But only if we're above text.
        && (m_frame.selection().selection().isContentEditable() || (mouseEvent.targetNode() && mouseEvent.targetNode()->isTextNode()))) {
        m_mouseDownMayStartSelect = true; // context menu events are always allowed to perform a selection
        selectClosestContextualWordOrLinkFromMouseEvent(mouseEvent);
    }

    swallowEvent = !dispatchMouseEvent(eventNames().contextmenuEvent, mouseEvent.targetNode(), 0, event, FireMouseOverOut::No);
    
    return swallowEvent;
}

bool EventHandler::sendContextMenuEventForKey()
{
    Ref<Frame> protectedFrame(m_frame);

    FrameView* view = m_frame.view();
    if (!view)
        return false;

    Document* doc = m_frame.document();
    if (!doc)
        return false;

    // Clear mouse press state to avoid initiating a drag while context menu is up.
    m_mousePressed = false;

    static const int kContextMenuMargin = 1;

#if OS(WINDOWS)
    int rightAligned = ::GetSystemMetrics(SM_MENUDROPALIGNMENT);
#else
    int rightAligned = 0;
#endif
    IntPoint location;

    Element* focusedElement = doc->focusedElement();
    const VisibleSelection& selection = m_frame.selection().selection();
    Position start = selection.start();

    if (start.deprecatedNode() && (selection.rootEditableElement() || selection.isRange())) {
        auto selectionRange = selection.toNormalizedRange();
        IntRect firstRect = m_frame.editor().firstRectForRange(createLiveRange(selectionRange).get());

        int x = rightAligned ? firstRect.maxX() : firstRect.x();
        // In a multiline edit, firstRect.maxY() would endup on the next line, so -1.
        int y = firstRect.maxY() ? firstRect.maxY() - 1 : 0;
        location = IntPoint(x, y);
    } else if (focusedElement) {
        RenderBoxModelObject* box = focusedElement->renderBoxModelObject();
        if (!box)
            return false;

        IntRect boundingBoxRect = box->absoluteBoundingBoxRect(true);
        location = IntPoint(boundingBoxRect.x(), boundingBoxRect.maxY() - 1);
    } else {
        location = IntPoint(
            rightAligned ? view->contentsWidth() - kContextMenuMargin : kContextMenuMargin,
            kContextMenuMargin);
    }

    m_frame.view()->setCursor(pointerCursor());

    IntPoint position = view->contentsToRootView(location);
    IntPoint globalPosition = view->hostWindow()->rootViewToScreen(IntRect(position, IntSize())).location();

    Node* targetNode = doc->focusedElement();
    if (!targetNode)
        targetNode = doc;

    // Use the focused node as the target for hover and active.
    HitTestResult result(position);
    result.setInnerNode(targetNode);
    doc->updateHoverActiveState(OptionSet<HitTestRequest::RequestType> { HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent }, result.targetElement());

    // The contextmenu event is a mouse event even when invoked using the keyboard.
    // This is required for web compatibility.

#if OS(WINDOWS)
    PlatformEvent::Type eventType = PlatformEvent::MouseReleased;
#else
    PlatformEvent::Type eventType = PlatformEvent::MousePressed;
#endif

    PlatformMouseEvent platformMouseEvent(position, globalPosition, RightButton, eventType, 1, false, false, false, false, WallTime::now(), ForceAtClick, NoTap);

    return sendContextMenuEvent(platformMouseEvent);
}
#endif // ENABLE(CONTEXT_MENU_EVENT)

void EventHandler::scheduleHoverStateUpdate()
{
    if (!m_hoverTimer.isActive())
        m_hoverTimer.startOneShot(0_s);
}

void EventHandler::scheduleCursorUpdate()
{
    if (m_hasScheduledCursorUpdate)
        return;

    auto* page = m_frame.page();
    if (!page)
        return;

    if (!page->chrome().client().supportsSettingCursor())
        return;

    m_hasScheduledCursorUpdate = true;
    page->scheduleRenderingUpdate();
}

void EventHandler::dispatchFakeMouseMoveEventSoon()
{
#if !ENABLE(IOS_TOUCH_EVENTS)
    if (m_mousePressed)
        return;

    if (m_mousePositionIsUnknown)
        return;

    if (Page* page = m_frame.page()) {
        if (!page->chrome().client().shouldDispatchFakeMouseMoveEvents())
            return;
    }

    // If the content has ever taken longer than fakeMouseMoveShortInterval we
    // reschedule the timer and use a longer time. This will cause the content
    // to receive these moves only after the user is done scrolling, reducing
    // pauses during the scroll.
    if (m_fakeMouseMoveEventTimer.isActive())
        m_fakeMouseMoveEventTimer.stop();
    m_fakeMouseMoveEventTimer.startOneShot(m_maxMouseMovedDuration > fakeMouseMoveDurationThreshold ? fakeMouseMoveLongInterval : fakeMouseMoveShortInterval);
#endif
}

void EventHandler::dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad& quad)
{
#if ENABLE(IOS_TOUCH_EVENTS)
    UNUSED_PARAM(quad);
#else
    FrameView* view = m_frame.view();
    if (!view)
        return;

    if (!quad.containsPoint(view->windowToContents(m_lastKnownMousePosition)))
        return;

    dispatchFakeMouseMoveEventSoon();
#endif
}

#if !ENABLE(IOS_TOUCH_EVENTS)
void EventHandler::cancelFakeMouseMoveEvent()
{
    m_fakeMouseMoveEventTimer.stop();
}

void EventHandler::fakeMouseMoveEventTimerFired()
{
    ASSERT(!m_mousePressed);

    FrameView* view = m_frame.view();
    if (!view)
        return;

    if (!m_frame.page() || !m_frame.page()->isVisible() || !m_frame.page()->focusController().isActive())
        return;

    bool shiftKey;
    bool ctrlKey;
    bool altKey;
    bool metaKey;
    PlatformKeyboardEvent::getCurrentModifierState(shiftKey, ctrlKey, altKey, metaKey);
    PlatformMouseEvent fakeMouseMoveEvent(m_lastKnownMousePosition, m_lastKnownMouseGlobalPosition, NoButton, PlatformEvent::MouseMoved, 0, shiftKey, ctrlKey, altKey, metaKey, WallTime::now(), 0, NoTap);
    mouseMoved(fakeMouseMoveEvent);
}
#endif // !ENABLE(IOS_TOUCH_EVENTS)

void EventHandler::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    m_frameSetBeingResized = frameSet;
}

void EventHandler::resizeLayerDestroyed()
{
    ASSERT(m_resizeLayer);
    m_resizeLayer = nullptr;
}

void EventHandler::hoverTimerFired()
{
    m_hoverTimer.stop();

    ASSERT(m_frame.document());

    Ref<Frame> protectedFrame(m_frame);

    if (auto* document = m_frame.document()) {
        if (FrameView* view = m_frame.view()) {
            HitTestResult result(view->windowToContents(m_lastKnownMousePosition));
            constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Move, HitTestRequest::DisallowUserAgentShadowContent };
            document->hitTest(hitType, result);
            document->updateHoverActiveState(hitType, result.targetElement());
        }
    }
}

bool EventHandler::handleAccessKey(const PlatformKeyboardEvent& event)
{
    // FIXME: Ignoring the state of Shift key is what neither IE nor Firefox do.
    // IE matches lower and upper case access keys regardless of Shift key state - but if both upper and
    // lower case variants are present in a document, the correct element is matched based on Shift key state.
    // Firefox only matches an access key if Shift is not pressed, and does that case-insensitively.
    ASSERT(!accessKeyModifiers().contains(PlatformEvent::Modifier::ShiftKey));

    if ((event.modifiers() - PlatformEvent::Modifier::ShiftKey) != accessKeyModifiers())
        return false;
    auto* element = m_frame.document()->elementForAccessKey(event.unmodifiedText());
    if (!element)
        return false;
    element->accessKeyAction(false);
    return true;
}

#if !PLATFORM(MAC)
bool EventHandler::needsKeyboardEventDisambiguationQuirks() const
{
    return false;
}
#endif

#if ENABLE(FULLSCREEN_API)
bool EventHandler::isKeyEventAllowedInFullScreen(const PlatformKeyboardEvent& keyEvent) const
{
    Document* document = m_frame.document();
    if (document->fullscreenManager().isFullscreenKeyboardInputAllowed())
        return true;

    if (keyEvent.type() == PlatformKeyboardEvent::Char) {
        if (keyEvent.text().length() != 1)
            return false;
        UChar character = keyEvent.text()[0];
        return character == ' ';
    }

    int keyCode = keyEvent.windowsVirtualKeyCode();
    return (keyCode >= VK_BACK && keyCode <= VK_CAPITAL)
        || (keyCode >= VK_SPACE && keyCode <= VK_DELETE)
        || (keyCode >= VK_OEM_1 && keyCode <= VK_OEM_PLUS)
        || (keyCode >= VK_MULTIPLY && keyCode <= VK_OEM_8);
}
#endif

bool EventHandler::keyEvent(const PlatformKeyboardEvent& keyEvent)
{
    Document* topDocument = m_frame.document() ? &m_frame.document()->topDocument() : nullptr;
    MonotonicTime savedLastHandledUserGestureTimestamp;
    bool savedUserDidInteractWithPage = topDocument ? topDocument->userDidInteractWithPage() : false;

    if (m_frame.document())
        savedLastHandledUserGestureTimestamp = m_frame.document()->lastHandledUserGestureTimestamp();

    bool wasHandled = internalKeyEvent(keyEvent);

    // If the key event was not handled, do not treat it as user interaction with the page.
    if (topDocument) {
        if (!wasHandled)
            topDocument->setUserDidInteractWithPage(savedUserDidInteractWithPage);
        else
            ResourceLoadObserver::shared().logUserInteractionWithReducedTimeResolution(*topDocument);
    }

    if (!wasHandled && m_frame.document())
        m_frame.document()->updateLastHandledUserGestureTimestamp(savedLastHandledUserGestureTimestamp);

    return wasHandled;
}

void EventHandler::capsLockStateMayHaveChanged() const
{
    auto* focusedElement = m_frame.document()->focusedElement();
    if (!is<HTMLInputElement>(focusedElement))
        return;
    downcast<HTMLInputElement>(*focusedElement).capsLockStateMayHaveChanged();
}

bool EventHandler::internalKeyEvent(const PlatformKeyboardEvent& initialKeyEvent)
{
    Ref<Frame> protectedFrame(m_frame);
    RefPtr<FrameView> protector(m_frame.view());

    LOG(Editing, "EventHandler %p keyEvent (text %s keyIdentifier %s)", this, initialKeyEvent.text().utf8().data(), initialKeyEvent.keyIdentifier().utf8().data());

#if ENABLE(POINTER_LOCK)
    if (initialKeyEvent.type() == PlatformEvent::KeyDown && initialKeyEvent.windowsVirtualKeyCode() == VK_ESCAPE && m_frame.page()->pointerLockController().element()) {
        m_frame.page()->pointerLockController().requestPointerUnlockAndForceCursorVisible();
    }
#endif

    if (initialKeyEvent.type() == PlatformEvent::KeyDown && initialKeyEvent.windowsVirtualKeyCode() == VK_ESCAPE) {
        if (auto* page = m_frame.page()) {
            if (auto* validationMessageClient = page->validationMessageClient())
                validationMessageClient->hideAnyValidationMessage();
        }
    }

#if ENABLE(FULLSCREEN_API)
    if (m_frame.document()->fullscreenManager().isFullscreen()) {
        if (initialKeyEvent.type() == PlatformEvent::KeyDown && initialKeyEvent.windowsVirtualKeyCode() == VK_ESCAPE) {
            m_frame.document()->fullscreenManager().cancelFullscreen();
            return true;
        }

        if (!isKeyEventAllowedInFullScreen(initialKeyEvent))
            return false;
    }
#endif

    if (initialKeyEvent.windowsVirtualKeyCode() == VK_CAPITAL)
        capsLockStateMayHaveChanged();

#if ENABLE(PAN_SCROLLING)
    if (m_frame.mainFrame().eventHandler().panScrollInProgress()) {
        // If a key is pressed while the panScroll is in progress then we want to stop
        if (initialKeyEvent.type() == PlatformEvent::KeyDown || initialKeyEvent.type() == PlatformEvent::RawKeyDown)
            stopAutoscrollTimer();

        // If we were in panscroll mode, we swallow the key event
        return true;
    }
#endif

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    RefPtr<Element> element = eventTargetElementForDocument(m_frame.document());
    if (!element)
        return false;

    UserGestureType gestureType = UserGestureType::Other;
    if (initialKeyEvent.windowsVirtualKeyCode() == VK_ESCAPE)
        gestureType = UserGestureType::EscapeKey;

    UserGestureIndicator gestureIndicator(ProcessingUserGesture, m_frame.document(), gestureType, UserGestureIndicator::ProcessInteractionStyle::Delayed);
    UserTypingGestureIndicator typingGestureIndicator(m_frame);

    // FIXME (bug 68185): this call should be made at another abstraction layer
    m_frame.loader().resetMultipleFormSubmissionProtection();

    // In IE, access keys are special, they are handled after default keydown processing, but cannot be canceled - this is hard to match.
    // On Mac OS X, we process them before dispatching keydown, as the default keydown handler implements Emacs key bindings, which may conflict
    // with access keys. Then we dispatch keydown, but suppress its default handling.
    // On Windows, WebKit explicitly calls handleAccessKey() instead of dispatching a keypress event for WM_SYSCHAR messages.
    // Other platforms currently match either Mac or Windows behavior, depending on whether they send combined KeyDown events.
    bool matchedAnAccessKey = false;
    if (initialKeyEvent.type() == PlatformEvent::KeyDown)
        matchedAnAccessKey = handleAccessKey(initialKeyEvent);

    // FIXME: it would be fair to let an input method handle KeyUp events before DOM dispatch.
    if (initialKeyEvent.type() == PlatformEvent::KeyUp || initialKeyEvent.type() == PlatformEvent::Char)
        return !element->dispatchKeyEvent(initialKeyEvent);

    bool backwardCompatibilityMode = needsKeyboardEventDisambiguationQuirks();

    PlatformKeyboardEvent keyDownEvent = initialKeyEvent;    
    if (keyDownEvent.type() != PlatformEvent::RawKeyDown)
        keyDownEvent.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown, backwardCompatibilityMode);
    auto keydown = KeyboardEvent::create(keyDownEvent, &m_frame.windowProxy());
    if (matchedAnAccessKey)
        keydown->preventDefault();
    keydown->setTarget(element);

    if (initialKeyEvent.type() == PlatformEvent::RawKeyDown) {
        element->dispatchEvent(keydown);
        // If frame changed as a result of keydown dispatch, then return true to avoid sending a subsequent keypress message to the new frame.
        bool changedFocusedFrame = m_frame.page() && &m_frame != &m_frame.page()->focusController().focusedOrMainFrame();
        return keydown->defaultHandled() || keydown->defaultPrevented() || changedFocusedFrame;
    }

    // Run input method in advance of DOM event handling.  This may result in the IM
    // modifying the page prior the keydown event, but this behaviour is necessary
    // in order to match IE:
    // 1. preventing default handling of keydown and keypress events has no effect on IM input;
    // 2. if an input method handles the event, its keyCode is set to 229 in keydown event.
    m_frame.editor().handleInputMethodKeydown(keydown.get());
    
    bool handledByInputMethod = keydown->defaultHandled();
    
    if (handledByInputMethod) {
        keyDownEvent.setWindowsVirtualKeyCode(CompositionEventKeyCode);
        keydown = KeyboardEvent::create(keyDownEvent, &m_frame.windowProxy());
        keydown->setTarget(element);
        keydown->setIsDefaultEventHandlerIgnored();
    }
    
    if (accessibilityPreventsEventPropagation(keydown))
        keydown->stopPropagation();

#if PLATFORM(IOS_FAMILY)
    DeferDOMTimersForScope deferralScope { m_frame.document()->quirks().needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() };
#endif

    element->dispatchEvent(keydown);
    if (handledByInputMethod) {
        m_frame.editor().didDispatchInputMethodKeydown(keydown.get());
        return true;
    }

    // If frame changed as a result of keydown dispatch, then return early to avoid sending a subsequent keypress message to the new frame.
    bool changedFocusedFrame = m_frame.page() && &m_frame != &m_frame.page()->focusController().focusedOrMainFrame();
    bool keydownResult = keydown->defaultHandled() || keydown->defaultPrevented() || changedFocusedFrame;
    if (keydownResult && !backwardCompatibilityMode)
        return keydownResult;

    // Focus may have changed during keydown handling, so refetch element.
    // But if we are dispatching a fake backward compatibility keypress, then we pretend that the keypress happened on the original element.
    if (!keydownResult) {
        element = eventTargetElementForDocument(m_frame.document());
        if (!element)
            return false;
    }

    PlatformKeyboardEvent keyPressEvent = initialKeyEvent;
    keyPressEvent.disambiguateKeyDownEvent(PlatformEvent::Char, backwardCompatibilityMode);
    if (keyPressEvent.text().isEmpty())
        return keydownResult;
    auto keypress = KeyboardEvent::create(keyPressEvent, &m_frame.windowProxy());
    keypress->setTarget(element);
    if (keydownResult)
        keypress->preventDefault();
#if PLATFORM(COCOA)
    keypress->keypressCommands() = keydown->keypressCommands();
#endif
    element->dispatchEvent(keypress);

    return keydownResult || keypress->defaultPrevented() || keypress->defaultHandled();
}

static FocusDirection focusDirectionForKey(const AtomString& keyIdentifier)
{
    static MainThreadNeverDestroyed<const AtomString> Down("Down", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> Up("Up", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> Left("Left", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> Right("Right", AtomString::ConstructFromLiteral);

    FocusDirection retVal = FocusDirectionNone;

    if (keyIdentifier == Down)
        retVal = FocusDirectionDown;
    else if (keyIdentifier == Up)
        retVal = FocusDirectionUp;
    else if (keyIdentifier == Left)
        retVal = FocusDirectionLeft;
    else if (keyIdentifier == Right)
        retVal = FocusDirectionRight;

    return retVal;
}

static void setInitialKeyboardSelection(Frame& frame, SelectionDirection direction)
{
    Document* document = frame.document();
    if (!document)
        return;

    FrameSelection& selection = frame.selection();

    if (!selection.isNone())
        return;

    Element* focusedElement = document->focusedElement();
    VisiblePosition visiblePosition;

    switch (direction) {
    case SelectionDirection::Backward:
    case SelectionDirection::Left:
        if (focusedElement)
            visiblePosition = VisiblePosition(positionBeforeNode(focusedElement));
        else
            visiblePosition = endOfDocument(document);
        break;
    case SelectionDirection::Forward:
    case SelectionDirection::Right:
        if (focusedElement)
            visiblePosition = VisiblePosition(positionAfterNode(focusedElement));
        else
            visiblePosition = startOfDocument(document);
        break;
    }

    AXTextStateChangeIntent intent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, false });
    selection.setSelection(visiblePosition, FrameSelection::defaultSetSelectionOptions(UserTriggered), intent);
}

static void handleKeyboardSelectionMovement(Frame& frame, KeyboardEvent& event)
{
    FrameSelection& selection = frame.selection();

    bool isCommanded = event.getModifierState("Meta");
    bool isOptioned = event.getModifierState("Alt");
    bool isSelection = !selection.isNone();

    FrameSelection::EAlteration alternation = event.getModifierState("Shift") ? FrameSelection::AlterationExtend : FrameSelection::AlterationMove;
    SelectionDirection direction = SelectionDirection::Forward;
    TextGranularity granularity = TextGranularity::CharacterGranularity;

    switch (focusDirectionForKey(event.keyIdentifier())) {
    case FocusDirectionNone:
        return;
    case FocusDirectionForward:
    case FocusDirectionBackward:
        ASSERT_NOT_REACHED();
        return;
    case FocusDirectionUp:
        direction = SelectionDirection::Backward;
        granularity = isCommanded ? TextGranularity::DocumentBoundary : TextGranularity::LineGranularity;
        break;
    case FocusDirectionDown:
        direction = SelectionDirection::Forward;
        granularity = isCommanded ? TextGranularity::DocumentBoundary : TextGranularity::LineGranularity;
        break;
    case FocusDirectionLeft:
        direction = SelectionDirection::Left;
        granularity = (isCommanded) ? TextGranularity::LineBoundary : (isOptioned) ? TextGranularity::WordGranularity : TextGranularity::CharacterGranularity;
        break;
    case FocusDirectionRight:
        direction = SelectionDirection::Right;
        granularity = (isCommanded) ? TextGranularity::LineBoundary : (isOptioned) ? TextGranularity::WordGranularity : TextGranularity::CharacterGranularity;
        break;
    }

    if (isSelection)
        selection.modify(alternation, direction, granularity, UserTriggered);
    else
        setInitialKeyboardSelection(frame, direction);

    event.setDefaultHandled();
}

void EventHandler::handleKeyboardSelectionMovementForAccessibility(KeyboardEvent& event)
{
    if (event.type() == eventNames().keydownEvent) {
        if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
            handleKeyboardSelectionMovement(m_frame, event);
    }
}

bool EventHandler::accessibilityPreventsEventPropagation(KeyboardEvent& event)
{
#if PLATFORM(COCOA)
    if (!AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
        return false;

    if (!m_frame.settings().preventKeyboardDOMEventDispatch())
        return false;

    // Check for key events that are relevant to accessibility: tab and arrows keys that change focus
    if (event.keyIdentifier() == "U+0009")
        return true;
    FocusDirection direction = focusDirectionForKey(event.keyIdentifier());
    if (direction != FocusDirectionNone)
        return true;
#else
    UNUSED_PARAM(event);
#endif
    return false;
}

void EventHandler::defaultKeyboardEventHandler(KeyboardEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);

    if (event.type() == eventNames().keydownEvent) {
        m_frame.editor().handleKeyboardEvent(event);
        if (event.defaultHandled())
            return;
        if (event.keyIdentifier() == "U+0009")
            defaultTabEventHandler(event);
        else if (event.keyIdentifier() == "U+0008")
            defaultBackspaceEventHandler(event);
        else {
            FocusDirection direction = focusDirectionForKey(event.keyIdentifier());
            if (direction != FocusDirectionNone)
                defaultArrowEventHandler(direction, event);
        }

        handleKeyboardSelectionMovementForAccessibility(event);
    }
    if (event.type() == eventNames().keypressEvent) {
        m_frame.editor().handleKeyboardEvent(event);
        if (event.defaultHandled())
            return;
        if (event.charCode() == ' ')
            defaultSpaceEventHandler(event);
    }
}

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::dragHysteresisExceeded(const IntPoint& floatDragViewportLocation) const
{
    FloatPoint dragViewportLocation(floatDragViewportLocation.x(), floatDragViewportLocation.y());
    return dragHysteresisExceeded(dragViewportLocation);
}

bool EventHandler::dragHysteresisExceeded(const FloatPoint& dragViewportLocation) const
{
    auto dragOperation = dragState().type.toSingleValue();
    ASSERT(dragOperation);
    int threshold = GeneralDragHysteresis;
    if (dragOperation) {
        switch (*dragOperation) {
        case DragSourceAction::Selection:
            threshold = TextDragHysteresis;
            break;
        case DragSourceAction::Image:
#if ENABLE(ATTACHMENT_ELEMENT)
        case DragSourceAction::Attachment:
#endif
            threshold = ImageDragHysteresis;
            break;
        case DragSourceAction::Link:
            threshold = LinkDragHysteresis;
            break;
#if ENABLE(INPUT_TYPE_COLOR)
        case DragSourceAction::Color:
            threshold = ColorDragHystersis;
            break;
#endif
        case DragSourceAction::DHTML:
            break;
        }
    }

    return mouseMovementExceedsThreshold(dragViewportLocation, threshold);
}

void EventHandler::invalidateDataTransfer()
{
    if (!dragState().dataTransfer)
        return;
    dragState().dataTransfer->makeInvalidForSecurity();
    dragState().dataTransfer = nullptr;
}

static void removeDraggedContentDocumentMarkersFromAllFramesInPage(Page& page)
{
    for (Frame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document())
            document->markers().removeMarkers(DocumentMarker::DraggedContent);
    }

    if (auto* mainFrameRenderer = page.mainFrame().contentRenderer())
        mainFrameRenderer->repaintRootContents();
}

void EventHandler::dragCancelled()
{
#if ENABLE(DATA_INTERACTION)
    if (auto* page = m_frame.page())
        removeDraggedContentDocumentMarkersFromAllFramesInPage(*page);
#endif
}

void EventHandler::didStartDrag()
{
#if ENABLE(DATA_INTERACTION)
    auto dragSource = dragState().source;
    if (!dragSource)
        return;

    auto* renderer = dragSource->renderer();
    if (!renderer)
        return;

    Optional<SimpleRange> draggedContentRange;
    if (dragState().type.contains(DragSourceAction::Selection))
        draggedContentRange = m_frame.selection().selection().toNormalizedRange();
    else
        draggedContentRange = makeRangeSelectingNode(*dragSource);

    if (draggedContentRange) {
        draggedContentRange->start.document().markers().addDraggedContentMarker(*draggedContentRange);
        if (auto* renderer = m_frame.contentRenderer())
            renderer->repaintRootContents();
    }
#endif
}

void EventHandler::dragSourceEndedAt(const PlatformMouseEvent& event, OptionSet<DragOperation> dragOperationMask, MayExtendDragSession mayExtendDragSession)
{
    // Send a hit test request so that RenderLayer gets a chance to update the :hover and :active pseudoclasses.
    prepareMouseEvent(OptionSet<HitTestRequest::RequestType> { HitTestRequest::Release, HitTestRequest::DisallowUserAgentShadowContent }, event);

    if (shouldDispatchEventsToDragSourceElement()) {
        dragState().dataTransfer->setDestinationOperationMask(dragOperationMask);
        dispatchEventToDragSourceElement(eventNames().dragendEvent, event);
    }
    invalidateDataTransfer();

    if (mayExtendDragSession == MayExtendDragSession::No) {
        if (auto* page = m_frame.page())
            removeDraggedContentDocumentMarkersFromAllFramesInPage(*page);
    }

    dragState().source = nullptr;
    // In case the drag was ended due to an escape key press we need to ensure
    // that consecutive mousemove events don't reinitiate the drag and drop.
    m_mouseDownMayStartDrag = false;
}

void EventHandler::updateDragStateAfterEditDragIfNeeded(Element& rootEditableElement)
{
    // If inserting the dragged contents removed the drag source, we still want to fire dragend at the root editable element.
    if (dragState().source && !dragState().source->isConnected())
        dragState().source = &rootEditableElement;
}

bool EventHandler::shouldDispatchEventsToDragSourceElement()
{
    return dragState().source && dragState().dataTransfer && dragState().shouldDispatchEvents;
}

void EventHandler::dispatchEventToDragSourceElement(const AtomString& eventType, const PlatformMouseEvent& event)
{
    if (shouldDispatchEventsToDragSourceElement())
        dispatchDragEvent(eventType, *dragState().source, event, *dragState().dataTransfer);
}

bool EventHandler::dispatchDragStartEventOnSourceElement(DataTransfer& dataTransfer)
{
    if (auto* page = m_frame.page())
        page->dragController().prepareForDragStart(m_frame, dragState().type, *dragState().source, dataTransfer, m_mouseDownContentsPosition);
    return !dispatchDragEvent(eventNames().dragstartEvent, *dragState().source, m_mouseDownEvent, dataTransfer) && !m_frame.selection().selection().isInPasswordField();
}

RefPtr<Element> EventHandler::draggedElement() const
{
    return dragState().source;
}

bool EventHandler::handleDrag(const MouseEventWithHitTestResults& event, CheckDragHysteresis checkDragHysteresis)
{
    if (event.event().button() != LeftButton || event.event().type() != PlatformEvent::MouseMoved) {
        // If we allowed the other side of the bridge to handle a drag
        // last time, then m_mousePressed might still be set. So we
        // clear it now to make sure the next move after a drag
        // doesn't look like a drag.
        m_mousePressed = false;
        return false;
    }
    
    Ref<Frame> protectedFrame(m_frame);

    if (eventLoopHandleMouseDragged(event))
        return true;
    
    // Careful that the drag starting logic stays in sync with eventMayStartDrag().
    if (m_mouseDownMayStartDrag && !dragState().source) {
        dragState().shouldDispatchEvents = updateDragSourceActionsAllowed().contains(DragSourceAction::DHTML);

        // Try to find an element that wants to be dragged.
        HitTestResult result(m_mouseDownContentsPosition);
        m_frame.document()->hitTest(OptionSet<HitTestRequest::RequestType> { HitTestRequest::ReadOnly, HitTestRequest::DisallowUserAgentShadowContent }, result);
        if (m_frame.page())
            dragState().source = m_frame.page()->dragController().draggableElement(&m_frame, result.targetElement(), m_mouseDownContentsPosition, dragState());
        
        if (!dragState().source)
            m_mouseDownMayStartDrag = false; // no element is draggable
        else
            m_dragMayStartSelectionInstead = dragState().type.contains(DragSourceAction::Selection);
    }
    
    // For drags starting in the selection, the user must wait between the mousedown and mousedrag,
    // or else we bail on the dragging stuff and allow selection to occur
    if (m_mouseDownMayStartDrag && m_dragMayStartSelectionInstead && dragState().type.contains(DragSourceAction::Selection) && event.event().timestamp() - m_mouseDownTimestamp < TextDragDelay) {
        ASSERT(event.event().type() == PlatformEvent::MouseMoved);
        if (dragState().type.contains(DragSourceAction::Image)) {
            // ... unless the mouse is over an image, then we start dragging just the image
            dragState().type = DragSourceAction::Image;
        } else if (!dragState().type.containsAny({ DragSourceAction::DHTML, DragSourceAction::Link })) {
            // ... but only bail if we're not over an unselectable element.
            m_mouseDownMayStartDrag = false;
            dragState().source = nullptr;
            // ... but if this was the first click in the window, we don't even want to start selection
            if (eventActivatedView(event.event()))
                m_mouseDownMayStartSelect = false;
        } else {
            // Prevent the following case from occuring:
            // 1. User starts a drag immediately after mouse down over an unselectable element.
            // 2. We enter this block and decided that since we're over an unselectable element, don't cancel the drag.
            // 3. The drag gets resolved as a potential selection drag below /but/ we haven't exceeded the drag hysteresis yet.
            // 4. We enter this block again, and since it's now marked as a selection drag, we cancel the drag.
            m_dragMayStartSelectionInstead = false;
        }
    }
    
    if (!m_mouseDownMayStartDrag)
        return !mouseDownMayStartSelect() && !m_mouseDownMayStartAutoscroll;
    ASSERT(dragState().source);

    if (!dragState().type.hasExactlyOneBitSet()) {
        ASSERT(dragState().type.contains(DragSourceAction::Selection));
#ifndef NDEBUG
        auto actionMaskCopy = dragState().type;
        actionMaskCopy.remove(DragSourceAction::Selection);
        ASSERT(actionMaskCopy.hasExactlyOneBitSet());
#endif

        dragState().type = DragSourceAction::Selection;
    }

    // We are starting a text/image/url drag, so the cursor should be an arrow
    if (FrameView* view = m_frame.view()) {
        // FIXME <rdar://7577595>: Custom cursors aren't supported during drag and drop (default to pointer).
        view->setCursor(pointerCursor());
    }

    if (checkDragHysteresis == ShouldCheckDragHysteresis && !dragHysteresisExceeded(event.event().position()))
        return true;
    
    // Once we're past the hysteresis point, we don't want to treat this gesture as a click
    invalidateClick();
    
    OptionSet<DragOperation> sourceOperationMask;
    
    // This does work only if we missed a dragEnd. Do it anyway, just to make sure the old dataTransfer gets numbed.
    // FIXME: Consider doing this earlier in this function as the earliest point we're sure it would be safe to drop an old drag.
    invalidateDataTransfer();

    dragState().dataTransfer = DataTransfer::createForDrag();
    auto hasNonDefaultPasteboardData = HasNonDefaultPasteboardData::No;
    
    if (dragState().shouldDispatchEvents) {
        ASSERT(dragState().source);
        auto dragStartDataTransfer = DataTransfer::createForDragStartEvent(dragState().source->document());
        m_mouseDownMayStartDrag = dispatchDragStartEventOnSourceElement(dragStartDataTransfer);
        if (downcast<StaticPasteboard>(dragStartDataTransfer->pasteboard()).hasNonDefaultData())
            hasNonDefaultPasteboardData = HasNonDefaultPasteboardData::Yes;
        dragState().dataTransfer->moveDragState(WTFMove(dragStartDataTransfer));

        if (dragState().source && dragState().type == DragSourceAction::DHTML && !dragState().dataTransfer->hasDragImage()) {
            dragState().source->document().updateStyleIfNeeded();
            if (auto* renderer = dragState().source->renderer()) {
                auto absolutePosition = renderer->localToAbsolute();
                auto delta = m_mouseDownContentsPosition - roundedIntPoint(absolutePosition);
                dragState().dataTransfer->setDragImage(dragState().source.get(), delta.width(), delta.height());
            } else {
                dispatchEventToDragSourceElement(eventNames().dragendEvent, event.event());
                m_mouseDownMayStartDrag = false;
                invalidateDataTransfer();
                dragState().source = nullptr;
                return true;
            }
        }

        dragState().dataTransfer->makeInvalidForSecurity();

        if (m_mouseDownMayStartDrag) {
            // Gather values from DHTML element, if it set any.
            sourceOperationMask = dragState().dataTransfer->sourceOperationMask();
            
            // Yuck, a draggedImage:moveTo: message can be fired as a result of kicking off the
            // drag with dragImage! Because of that dumb reentrancy, we may think we've not
            // started the drag when that happens. So we have to assume it's started before we kick it off.
            dragState().dataTransfer->setDragHasStarted();
        }
    }
    
    if (m_mouseDownMayStartDrag) {
        Page* page = m_frame.page();
        m_didStartDrag = page && page->dragController().startDrag(m_frame, dragState(), sourceOperationMask, event.event(), m_mouseDownContentsPosition, hasNonDefaultPasteboardData);
        // In WebKit2 we could re-enter this code and start another drag.
        // On OS X this causes problems with the ownership of the pasteboard and the promised types.
        if (m_didStartDrag) {
            m_mouseDownMayStartDrag = false;
            return true;
        }
        if (shouldDispatchEventsToDragSourceElement()) {
            // Drag was canned at the last minute. We owe dragSource a dragend event.
            dispatchEventToDragSourceElement(eventNames().dragendEvent, event.event());
            m_mouseDownMayStartDrag = false;
        }
    }

    if (!m_mouseDownMayStartDrag) {
        // Something failed to start the drag, clean up.
        invalidateDataTransfer();
        dragState().source = nullptr;
    }
    
    // No more default handling (like selection), whether we're past the hysteresis bounds or not
    return true;
}
#endif // ENABLE(DRAG_SUPPORT)

bool EventHandler::mouseMovementExceedsThreshold(const FloatPoint& viewportLocation, int pointsThreshold) const
{
    FrameView* view = m_frame.view();
    if (!view)
        return false;
    IntPoint location = view->windowToContents(flooredIntPoint(viewportLocation));
    IntSize delta = location - m_mouseDownContentsPosition;
    
    return abs(delta.width()) >= pointsThreshold || abs(delta.height()) >= pointsThreshold;
}

bool EventHandler::handleTextInputEvent(const String& text, Event* underlyingEvent, TextEventInputType inputType)
{
    LOG(Editing, "EventHandler %p handleTextInputEvent (text %s)", this, text.utf8().data());

    // Platforms should differentiate real commands like selectAll from text input in disguise (like insertNewline),
    // and avoid dispatching text input events from keydown default handlers.
    ASSERT(!is<KeyboardEvent>(underlyingEvent) || downcast<KeyboardEvent>(*underlyingEvent).type() == eventNames().keypressEvent);

    Ref<Frame> protectedFrame(m_frame);

    EventTarget* target;
    if (underlyingEvent)
        target = underlyingEvent->target();
    else
        target = eventTargetElementForDocument(m_frame.document());
    if (!target)
        return false;

    auto event = TextEvent::create(&m_frame.windowProxy(), text, inputType);
    event->setUnderlyingEvent(underlyingEvent);

    target->dispatchEvent(event);
    return event->defaultHandled();
}
    
bool EventHandler::isKeyboardOptionTab(KeyboardEvent& event)
{
    return (event.type() == eventNames().keydownEvent || event.type() == eventNames().keypressEvent)
        && event.altKey()
        && event.keyIdentifier() == "U+0009";
}

bool EventHandler::eventInvertsTabsToLinksClientCallResult(KeyboardEvent& event)
{
#if PLATFORM(COCOA)
    return isKeyboardOptionTab(event);
#else
    UNUSED_PARAM(event);
    return false;
#endif
}

bool EventHandler::tabsToLinks(KeyboardEvent* event) const
{
    // FIXME: This function needs a better name. It can be called for keypresses other than Tab when spatial navigation is enabled.

    Page* page = m_frame.page();
    if (!page)
        return false;

    bool tabsToLinksClientCallResult = page->chrome().client().keyboardUIMode() & KeyboardAccessTabsToLinks;
    return (event && eventInvertsTabsToLinksClientCallResult(*event)) ? !tabsToLinksClientCallResult : tabsToLinksClientCallResult;
}

void EventHandler::defaultTextInputEventHandler(TextEvent& event)
{
    if (m_frame.editor().handleTextEvent(event))
        event.setDefaultHandled();
}


void EventHandler::defaultSpaceEventHandler(KeyboardEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);

    ASSERT(event.type() == eventNames().keypressEvent);

    if (event.ctrlKey() || event.metaKey() || event.altKey() || event.altGraphKey())
        return;

    ScrollLogicalDirection direction = event.shiftKey() ? ScrollBlockDirectionBackward : ScrollBlockDirectionForward;
    if (logicalScrollOverflow(direction, ScrollByPage)) {
        event.setDefaultHandled();
        return;
    }

    FrameView* view = m_frame.view();
    if (!view)
        return;

    if (view->logicalScroll(direction, ScrollByPage))
        event.setDefaultHandled();
}

void EventHandler::defaultBackspaceEventHandler(KeyboardEvent& event)
{
    ASSERT(event.type() == eventNames().keydownEvent);

    if (event.ctrlKey() || event.metaKey() || event.altKey() || event.altGraphKey())
        return;

    if (!m_frame.editor().behavior().shouldNavigateBackOnBackspace())
        return;
    
    Page* page = m_frame.page();
    if (!page)
        return;

    if (!m_frame.settings().backspaceKeyNavigationEnabled())
        return;
    
    bool handledEvent = false;

    if (event.shiftKey())
        handledEvent = page->backForward().goForward();
    else
        handledEvent = page->backForward().goBack();

    if (handledEvent)
        event.setDefaultHandled();
}


void EventHandler::defaultArrowEventHandler(FocusDirection focusDirection, KeyboardEvent& event)
{
    ASSERT(event.type() == eventNames().keydownEvent);

    if (event.ctrlKey() || event.metaKey() || event.altGraphKey() || event.shiftKey())
        return;

    Page* page = m_frame.page();
    if (!page)
        return;

    if (!isSpatialNavigationEnabled(&m_frame))
        return;

    // Arrows and other possible directional navigation keys can be used in design
    // mode editing.
    if (m_frame.document()->inDesignMode())
        return;

    if (page->focusController().advanceFocus(focusDirection, &event))
        event.setDefaultHandled();
}

void EventHandler::defaultTabEventHandler(KeyboardEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);

    ASSERT(event.type() == eventNames().keydownEvent);

    // We should only advance focus on tabs if no special modifier keys are held down.
    if (event.ctrlKey() || event.metaKey() || event.altGraphKey())
        return;

    Page* page = m_frame.page();
    if (!page)
        return;

    FocusDirection focusDirection = event.shiftKey() ? FocusDirectionBackward : FocusDirectionForward;

    // Tabs can be used in design mode editing.
    if (m_frame.document()->inDesignMode())
        return;

    // Allow shift-tab to relinquish focus even if we don't allow tab to cycle between elements inside the view.
    // We can only do this for shift-tab, not tab itself because tabKeyCyclesThroughElements is used to make
    // tab character insertion work in editable web views.
    if (!page->tabKeyCyclesThroughElements()) {
        if (focusDirection == FocusDirectionBackward) {
            if (page->focusController().relinquishFocusToChrome(focusDirection))
                event.setDefaultHandled();
        }
        return;
    }

    if (page->focusController().advanceFocus(focusDirection, &event))
        event.setDefaultHandled();
}

void EventHandler::sendScrollEvent()
{
    Ref<Frame> protectedFrame(m_frame);
    setFrameWasScrolledByUser();
    if (!m_frame.view())
        return;
    auto document = makeRefPtr(m_frame.document());
    if (!document)
        return;
    document->addPendingScrollEventTarget(*document);
}

void EventHandler::setFrameWasScrolledByUser()
{
    FrameView* v = m_frame.view();
    if (v)
        v->setWasScrolledByUser(true);
}

bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults& mouseEventAndResult, Scrollbar* scrollbar)
{
    if (!scrollbar || !scrollbar->enabled())
        return false;
    setFrameWasScrolledByUser();
    return scrollbar->mouseDown(mouseEventAndResult.event());
}

// If scrollbar (under mouse) is different from last, send a mouse exited.
void EventHandler::updateLastScrollbarUnderMouse(Scrollbar* scrollbar, SetOrClearLastScrollbar setOrClear)
{
    if (m_lastScrollbarUnderMouse != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse->mouseExited();

        // Send mouse entered if we're setting a new scrollbar.
        if (scrollbar && setOrClear == SetOrClearLastScrollbar::Set) {
            scrollbar->mouseEntered();
            m_lastScrollbarUnderMouse = makeWeakPtr(*scrollbar);
        } else
            m_lastScrollbarUnderMouse = nullptr;
    }
}

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
static const AtomString& eventNameForTouchPointState(PlatformTouchPoint::State state)
{
    switch (state) {
    case PlatformTouchPoint::TouchReleased:
        return eventNames().touchendEvent;
    case PlatformTouchPoint::TouchCancelled:
        return eventNames().touchcancelEvent;
    case PlatformTouchPoint::TouchPressed:
        return eventNames().touchstartEvent;
    case PlatformTouchPoint::TouchMoved:
        return eventNames().touchmoveEvent;
    case PlatformTouchPoint::TouchStationary:
        // TouchStationary state is not converted to touch events, so fall through to assert.
    default:
        ASSERT_NOT_REACHED();
        return emptyAtom();
    }
}

static HitTestResult hitTestResultInFrame(Frame* frame, const LayoutPoint& point, OptionSet<HitTestRequest::RequestType> hitType)
{
    HitTestResult result(point);

    if (!frame || !frame->contentRenderer())
        return result;

    if (frame->view()) {
        IntRect rect = frame->view()->visibleContentRect();
        if (!rect.contains(roundedIntPoint(point)))
            return result;
    }
    frame->document()->hitTest(hitType, result);
    return result;
}

bool EventHandler::handleTouchEvent(const PlatformTouchEvent& event)
{
    Ref<Frame> protectedFrame(m_frame);

    // First build up the lists to use for the 'touches', 'targetTouches' and 'changedTouches' attributes
    // in the JS event. See http://www.sitepen.com/blog/2008/07/10/touching-and-gesturing-on-the-iphone/
    // for an overview of how these lists fit together.

    // Holds the complete set of touches on the screen and will be used as the 'touches' list in the JS event.
    RefPtr<TouchList> touches = TouchList::create();

    // A different view on the 'touches' list above, filtered and grouped by event target. Used for the
    // 'targetTouches' list in the JS event.
    typedef HashMap<EventTarget*, RefPtr<TouchList>> TargetTouchesMap;
    TargetTouchesMap touchesByTarget;

    // Array of touches per state, used to assemble the 'changedTouches' list in the JS event.
    typedef HashSet<RefPtr<EventTarget>> EventTargetSet;
    struct {
        // The touches corresponding to the particular change state this struct instance represents.
        RefPtr<TouchList> m_touches;
        // Set of targets involved in m_touches.
        EventTargetSet m_targets;
    } changedTouches[PlatformTouchPoint::TouchStateEnd];

    const Vector<PlatformTouchPoint>& points = event.touchPoints();

    UserGestureIndicator gestureIndicator(ProcessingUserGesture, m_frame.document());

    bool freshTouchEvents = true;
    bool allTouchReleased = true;
    for (auto& point : points) {
        if (point.state() != PlatformTouchPoint::TouchPressed)
            freshTouchEvents = false;
        if (point.state() != PlatformTouchPoint::TouchReleased && point.state() != PlatformTouchPoint::TouchCancelled)
            allTouchReleased = false;
    }

    for (auto& point : points) {
        PlatformTouchPoint::State pointState = point.state();
        LayoutPoint pagePoint = documentPointForWindowPoint(m_frame, point.pos());

        OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::TouchEvent };
        // The HitTestRequest types used for mouse events map quite adequately
        // to touch events. Note that in addition to meaning that the hit test
        // should affect the active state of the current node if necessary,
        // HitTestRequest::Active signifies that the hit test is taking place
        // with the mouse (or finger in this case) being pressed.
        switch (pointState) {
        case PlatformTouchPoint::TouchPressed:
            hitType.add(HitTestRequest::Active);
            break;
        case PlatformTouchPoint::TouchMoved:
            hitType.add({ HitTestRequest::Active, HitTestRequest::Move, HitTestRequest::ReadOnly });
            break;
        case PlatformTouchPoint::TouchReleased:
        case PlatformTouchPoint::TouchCancelled:
            hitType.add(HitTestRequest::Release);
            break;
        case PlatformTouchPoint::TouchStationary:
            hitType.add({ HitTestRequest::Active, HitTestRequest::ReadOnly });
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        if (shouldGesturesTriggerActive())
            hitType.add(HitTestRequest::ReadOnly);

        // Increment the platform touch id by 1 to avoid storing a key of 0 in the hashmap.
        unsigned touchPointTargetKey = point.id() + 1;
        RefPtr<EventTarget> touchTarget;
        if (pointState == PlatformTouchPoint::TouchPressed) {
            HitTestResult result;
            if (freshTouchEvents) {
                result = hitTestResultAtPoint(pagePoint, hitType | HitTestRequest::AllowChildFrameContent);
                m_originatingTouchPointTargetKey = touchPointTargetKey;
            } else if (m_originatingTouchPointDocument.get() && m_originatingTouchPointDocument->frame()) {
                LayoutPoint pagePointInOriginatingDocument = documentPointForWindowPoint(*m_originatingTouchPointDocument->frame(), point.pos());
                result = hitTestResultInFrame(m_originatingTouchPointDocument->frame(), pagePointInOriginatingDocument, hitType);
                if (!result.innerNode())
                    continue;
            } else
                continue;

            Element* element = result.targetElement();
            ASSERT(element);

            if (element && InspectorInstrumentation::handleTouchEvent(m_frame, *element))
                return true;

            Document& doc = element->document();
            // Record the originating touch document even if it does not have a touch listener.
            if (freshTouchEvents) {
                m_originatingTouchPointDocument = &doc;
                freshTouchEvents = false;
            }
            if (!doc.hasTouchEventHandlers())
                continue;
            m_originatingTouchPointTargets.set(touchPointTargetKey, element);
            touchTarget = element;
        } else if (pointState == PlatformTouchPoint::TouchReleased || pointState == PlatformTouchPoint::TouchCancelled) {
            // No need to perform a hit-test since we only need to unset :hover and :active states.
            if (!shouldGesturesTriggerActive() && allTouchReleased)
                m_frame.document()->updateHoverActiveState(hitType, 0);
            if (touchPointTargetKey == m_originatingTouchPointTargetKey)
                m_originatingTouchPointTargetKey = 0;

            // The target should be the original target for this touch, so get it from the hashmap. As it's a release or cancel
            // we also remove it from the map.
            touchTarget = m_originatingTouchPointTargets.take(touchPointTargetKey);
        } else
            // No hittest is performed on move or stationary, since the target is not allowed to change anyway.
            touchTarget = m_originatingTouchPointTargets.get(touchPointTargetKey);

        if (!is<Node>(touchTarget))
            continue;
        auto& document = downcast<Node>(*touchTarget).document();
        if (!document.hasTouchEventHandlers())
            continue;
        auto* targetFrame = document.frame();
        if (!targetFrame)
            continue;

        if (&m_frame != targetFrame) {
            // pagePoint should always be relative to the target elements containing frame.
            pagePoint = documentPointForWindowPoint(*targetFrame, point.pos());
        }

        float scaleFactor = targetFrame->pageZoomFactor() * targetFrame->frameScaleFactor();

        int adjustedPageX = lroundf(pagePoint.x() / scaleFactor);
        int adjustedPageY = lroundf(pagePoint.y() / scaleFactor);

        auto touch = Touch::create(targetFrame, touchTarget.get(), point.id(),
            point.screenPos().x(), point.screenPos().y(), adjustedPageX, adjustedPageY,
            point.radiusX(), point.radiusY(), point.rotationAngle(), point.force());

        // Ensure this target's touch list exists, even if it ends up empty, so it can always be passed to TouchEvent::Create below.
        TargetTouchesMap::iterator targetTouchesIterator = touchesByTarget.find(touchTarget.get());
        if (targetTouchesIterator == touchesByTarget.end())
            targetTouchesIterator = touchesByTarget.set(touchTarget.get(), TouchList::create()).iterator;

        // touches and targetTouches should only contain information about touches still on the screen, so if this point is
        // released or cancelled it will only appear in the changedTouches list.
        if (pointState != PlatformTouchPoint::TouchReleased && pointState != PlatformTouchPoint::TouchCancelled) {
            touches->append(touch.copyRef());
            targetTouchesIterator->value->append(touch.copyRef());
        }

        // Now build up the correct list for changedTouches.
        // Note that  any touches that are in the TouchStationary state (e.g. if
        // the user had several points touched but did not move them all) should
        // never be in the changedTouches list so we do not handle them explicitly here.
        // See https://bugs.webkit.org/show_bug.cgi?id=37609 for further discussion
        // about the TouchStationary state.
        if (pointState != PlatformTouchPoint::TouchStationary) {
            ASSERT(pointState < PlatformTouchPoint::TouchStateEnd);
            if (!changedTouches[pointState].m_touches)
                changedTouches[pointState].m_touches = TouchList::create();
            changedTouches[pointState].m_touches->append(WTFMove(touch));
            changedTouches[pointState].m_targets.add(touchTarget);
        }
    }
    m_touchPressed = touches->length() > 0;
    if (allTouchReleased)
        m_originatingTouchPointDocument = nullptr;

    // Now iterate the changedTouches list and m_targets within it, sending events to the targets as required.
    bool swallowedEvent = false;
    RefPtr<TouchList> emptyList = TouchList::create();
    for (unsigned state = 0; state != PlatformTouchPoint::TouchStateEnd; ++state) {
        if (!changedTouches[state].m_touches)
            continue;

        // When sending a touch cancel event, use empty touches and targetTouches lists.
        bool isTouchCancelEvent = (state == PlatformTouchPoint::TouchCancelled);
        RefPtr<TouchList>& effectiveTouches(isTouchCancelEvent ? emptyList : touches);
        const AtomString& stateName(eventNameForTouchPointState(static_cast<PlatformTouchPoint::State>(state)));

        for (auto& target : changedTouches[state].m_targets) {
            ASSERT(is<Node>(target));

            RefPtr<TouchList> targetTouches(isTouchCancelEvent ? emptyList : touchesByTarget.get(target.get()));
            ASSERT(targetTouches);

            Ref<TouchEvent> touchEvent = TouchEvent::create(effectiveTouches.get(), targetTouches.get(), changedTouches[state].m_touches.get(),
                stateName, downcast<Node>(*target).document().windowProxy(), { }, event.modifiers());
            target->dispatchEvent(touchEvent);
            swallowedEvent = swallowedEvent || touchEvent->defaultPrevented() || touchEvent->defaultHandled();
        }
    }

    return swallowedEvent;
}
#endif // ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)

#if ENABLE(TOUCH_EVENTS)
bool EventHandler::dispatchSyntheticTouchEventIfEnabled(const PlatformMouseEvent& platformMouseEvent)
{
#if ENABLE(IOS_TOUCH_EVENTS)
    UNUSED_PARAM(platformMouseEvent);
    return false;
#else
    if (!m_frame.settings().isTouchEventEmulationEnabled())
        return false;

    PlatformEvent::Type eventType = platformMouseEvent.type();
    if (eventType != PlatformEvent::MouseMoved && eventType != PlatformEvent::MousePressed && eventType != PlatformEvent::MouseReleased)
        return false;

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent };
    MouseEventWithHitTestResults mouseEvent = prepareMouseEvent(hitType, platformMouseEvent);
    if (mouseEvent.scrollbar() || subframeForHitTestResult(mouseEvent))
        return false;

    // The order is important. This check should follow the subframe test: http://webkit.org/b/111292.
    if (eventType == PlatformEvent::MouseMoved && !m_touchPressed)
        return true;

    SyntheticSingleTouchEvent touchEvent(platformMouseEvent);
    return handleTouchEvent(touchEvent);
#endif
}
#endif // ENABLE(TOUCH_EVENTS)

void EventHandler::setLastKnownMousePosition(const PlatformMouseEvent& event)
{
    m_mousePositionIsUnknown = false;
    m_lastKnownMousePosition = event.position();
    m_lastKnownMouseGlobalPosition = event.globalPosition();
}

void EventHandler::setImmediateActionStage(ImmediateActionStage stage)
{
    m_immediateActionStage = stage;
}

#if !PLATFORM(COCOA)
OptionSet<PlatformEvent::Modifier> EventHandler::accessKeyModifiers()
{
    return PlatformEvent::Modifier::AltKey;
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mouseEventAndResult, Frame& subframe)
{
    subframe.eventHandler().handleMousePressEvent(mouseEventAndResult.event());
    return true;
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mouseEventAndResult, Frame& subframe)
{
    subframe.eventHandler().handleMouseReleaseEvent(mouseEventAndResult.event());
    return true;
}

bool EventHandler::passWheelEventToWidget(const PlatformWheelEvent& event, Widget& widget)
{
    if (!is<FrameView>(widget))
        return false;

    return downcast<FrameView>(widget).frame().eventHandler().handleWheelEvent(event);
}

bool EventHandler::tabsToAllFormControls(KeyboardEvent*) const
{
    // We always allow tabs to all controls
    return true;
}

bool EventHandler::passWidgetMouseDownEventToWidget(RenderWidget* renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults& event)
{
    RenderObject* target = event.targetNode() ? event.targetNode()->renderer() : nullptr;
    if (!is<RenderWidget>(target))
        return false;
    return passMouseDownEventToWidget(downcast<RenderWidget>(*target).widget());
}

bool EventHandler::passMouseDownEventToWidget(Widget*)
{
    notImplemented();
    return false;
}

void EventHandler::focusDocumentView()
{
    if (Page* page = m_frame.page())
        page->focusController().setFocusedFrame(&m_frame);
}
#endif // !PLATFORM(COCOA)

#if !PLATFORM(COCOA) && !PLATFORM(WIN)
bool EventHandler::eventActivatedView(const PlatformMouseEvent&) const
{
    notImplemented();
    return false;
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mouseEventAndResult, Frame& subframe, HitTestResult* result)
{
    subframe.eventHandler().handleMouseMoveEvent(mouseEventAndResult.event(), result);
    return true;
}
#endif // !PLATFORM(COCOA) && !PLATFORM(WIN)

} // namespace WebCore
