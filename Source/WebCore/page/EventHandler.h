/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Cursor.h"
#include "DragActions.h"
#include "FocusDirection.h"
#include "HitTestRequest.h"
#include "LayoutPoint.h"
#include "PlatformMouseEvent.h"
#include "RenderObject.h"
#include "ScrollTypes.h"
#include "TextEventInputType.h"
#include "TextGranularity.h"
#include "Timer.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSView;
#endif

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS WebEvent;
#endif

#if PLATFORM(MAC)
OBJC_CLASS NSEvent;
#endif

#if PLATFORM(IOS_FAMILY) && defined(__OBJC__)
#include "WAKAppKitStubs.h"
#endif

namespace WebCore {

class AutoscrollController;
class ContainerNode;
class DataTransfer;
class Document;
class Element;
class Event;
class EventTarget;
class FloatQuad;
class Frame;
class FrameView;
class HTMLFrameSetElement;
class HitTestResult;
class KeyboardEvent;
class MouseEventWithHitTestResults;
class Node;
class Pasteboard;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformTouchEvent;
class PlatformWheelEvent;
class RenderBox;
class RenderElement;
class RenderLayer;
class RenderWidget;
class ScrollableArea;
class Scrollbar;
class TextEvent;
class Touch;
class TouchEvent;
class VisibleSelection;
class WheelEvent;
class Widget;

struct DragState;

#if ENABLE(DRAG_SUPPORT)
extern const int LinkDragHysteresis;
extern const int ImageDragHysteresis;
extern const int TextDragHysteresis;
extern const int ColorDragHystersis;
extern const int GeneralDragHysteresis;
#endif

#if ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
extern const float GestureUnknown;
extern const unsigned InvalidTouchIdentifier;
#endif

enum AppendTrailingWhitespace { ShouldAppendTrailingWhitespace, DontAppendTrailingWhitespace };
enum CheckDragHysteresis { ShouldCheckDragHysteresis, DontCheckDragHysteresis };

enum class ImmediateActionStage {
    None,
    PerformedHitTest,
    ActionUpdated,
    ActionCancelledWithoutUpdate,
    ActionCancelledAfterUpdate,
    ActionCompleted
};

class EventHandler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit EventHandler(Frame&);
    ~EventHandler();

    void clear();
    void nodeWillBeRemoved(Node&);

    WEBCORE_EXPORT VisiblePosition selectionExtentRespectingEditingBoundary(const VisibleSelection&, const LayoutPoint&, Node*);

#if ENABLE(DRAG_SUPPORT)
    void updateSelectionForMouseDrag();
#endif

#if ENABLE(PAN_SCROLLING)
    void didPanScrollStart();
    void didPanScrollStop();
    void startPanScrolling(RenderElement&);
#endif

    void stopAutoscrollTimer(bool rendererIsBeingDestroyed = false);
    RenderBox* autoscrollRenderer() const;
    void updateAutoscrollRenderer();
    bool autoscrollInProgress() const;
    bool mouseDownWasInSubframe() const { return m_mouseDownWasInSubframe; }
    bool panScrollInProgress() const;

    WEBCORE_EXPORT void dispatchFakeMouseMoveEventSoon();
    void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

    WEBCORE_EXPORT HitTestResult hitTestResultAtPoint(const LayoutPoint&, HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent, const LayoutSize& padding = LayoutSize()) const;

    bool mousePressed() const { return m_mousePressed; }
    Node* mousePressNode() const { return m_mousePressNode.get(); }

    WEBCORE_EXPORT void setCapturingMouseEventsElement(Element*);

#if ENABLE(DRAG_SUPPORT)
    struct DragTargetResponse {
        bool accept { false };
        std::optional<DragOperation> operation;
    };
    DragTargetResponse updateDragAndDrop(const PlatformMouseEvent&, const std::function<std::unique_ptr<Pasteboard>()>&, DragOperation sourceOperation, bool draggingFiles);
    void cancelDragAndDrop(const PlatformMouseEvent&, std::unique_ptr<Pasteboard>&&, DragOperation, bool draggingFiles);
    bool performDragAndDrop(const PlatformMouseEvent&, std::unique_ptr<Pasteboard>&&, DragOperation, bool draggingFiles);
    void updateDragStateAfterEditDragIfNeeded(Element& rootEditableElement);
    RefPtr<Element> draggedElement() const;
#endif

    void scheduleHoverStateUpdate();
#if ENABLE(CURSOR_SUPPORT)
    void scheduleCursorUpdate();
#endif

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeLayerDestroyed();

    IntPoint lastKnownMousePosition() const;
    IntPoint lastKnownMouseGlobalPosition() const { return m_lastKnownMouseGlobalPosition; }
    Cursor currentMouseCursor() const { return m_currentMouseCursor; }

    IntPoint targetPositionInWindowForSelectionAutoscroll() const;
    bool shouldUpdateAutoscroll();

    static Frame* subframeForTargetNode(Node*);
    static Frame* subframeForHitTestResult(const MouseEventWithHitTestResults&);

    WEBCORE_EXPORT bool scrollOverflow(ScrollDirection, ScrollGranularity, Node* startingNode = nullptr);
    WEBCORE_EXPORT bool scrollRecursively(ScrollDirection, ScrollGranularity, Node* startingNode = nullptr);
    WEBCORE_EXPORT bool logicalScrollRecursively(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = nullptr);

    bool tabsToLinks(KeyboardEvent*) const;
    bool tabsToAllFormControls(KeyboardEvent*) const;

    WEBCORE_EXPORT bool mouseMoved(const PlatformMouseEvent&);
    WEBCORE_EXPORT bool passMouseMovedEventToScrollbars(const PlatformMouseEvent&);

    void lostMouseCapture();

    WEBCORE_EXPORT bool handleMousePressEvent(const PlatformMouseEvent&);
    bool handleMouseMoveEvent(const PlatformMouseEvent&, HitTestResult* hoveredNode = nullptr, bool onlyUpdateScrollbars = false);
    WEBCORE_EXPORT bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleMouseForceEvent(const PlatformMouseEvent&);
    WEBCORE_EXPORT bool handleWheelEvent(const PlatformWheelEvent&);
    void defaultWheelEventHandler(Node*, WheelEvent&);
    bool handlePasteGlobalSelection(const PlatformMouseEvent&);

    void platformPrepareForWheelEvents(const PlatformWheelEvent&, const HitTestResult&, RefPtr<Element>& eventTarget, RefPtr<ContainerNode>& scrollableContainer, WeakPtr<ScrollableArea>&, bool& isOverWidget);
    void platformRecordWheelEvent(const PlatformWheelEvent&);
    bool platformCompleteWheelEvent(const PlatformWheelEvent&, ContainerNode* scrollableContainer, const WeakPtr<ScrollableArea>&);
    bool platformCompletePlatformWidgetWheelEvent(const PlatformWheelEvent&, const Widget&, ContainerNode* scrollableContainer);
    void platformNotifyIfEndGesture(const PlatformWheelEvent&, const WeakPtr<ScrollableArea>&);

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    using TouchArray = Vector<RefPtr<Touch>>;
    using EventTargetTouchMap = HashMap<EventTarget*, TouchArray*>;
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
    using EventTargetSet = HashSet<RefPtr<EventTarget>>;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    bool dispatchTouchEvent(const PlatformTouchEvent&, const AtomicString&, const EventTargetTouchMap&, float, float);
    bool dispatchSimulatedTouchEvent(IntPoint location);
    Frame* touchEventTargetSubframe() const { return m_touchEventTargetSubframe.get(); }
    const TouchArray& touches() const { return m_touches; }
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
    bool dispatchGestureEvent(const PlatformTouchEvent&, const AtomicString&, const EventTargetSet&, float, float);
#elif ENABLE(MAC_GESTURE_EVENTS)
    bool dispatchGestureEvent(const PlatformGestureEvent&, const AtomicString&, const EventTargetSet&, float, float);
    WEBCORE_EXPORT bool handleGestureEvent(const PlatformGestureEvent&);
#endif

#if PLATFORM(IOS_FAMILY)
    void defaultTouchEventHandler(Node&, TouchEvent&);
#endif

#if ENABLE(CONTEXT_MENUS)
    WEBCORE_EXPORT bool sendContextMenuEvent(const PlatformMouseEvent&);
    WEBCORE_EXPORT bool sendContextMenuEventForKey();
#endif

    void setMouseDownMayStartAutoscroll() { m_mouseDownMayStartAutoscroll = true; }

    bool needsKeyboardEventDisambiguationQuirks() const;

    WEBCORE_EXPORT static OptionSet<PlatformEvent::Modifier> accessKeyModifiers();
    WEBCORE_EXPORT bool handleAccessKey(const PlatformKeyboardEvent&);
    WEBCORE_EXPORT bool keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent&);
    WEBCORE_EXPORT void capsLockStateMayHaveChanged() const;

    bool accessibilityPreventsEventPropagation(KeyboardEvent&);
    WEBCORE_EXPORT void handleKeyboardSelectionMovementForAccessibility(KeyboardEvent&);

    bool handleTextInputEvent(const String& text, Event* underlyingEvent = nullptr, TextEventInputType = TextEventInputKeyboard);
    void defaultTextInputEventHandler(TextEvent&);

#if ENABLE(DRAG_SUPPORT)
    WEBCORE_EXPORT bool eventMayStartDrag(const PlatformMouseEvent&) const;
    
    WEBCORE_EXPORT void didStartDrag();
    WEBCORE_EXPORT void dragCancelled();
    WEBCORE_EXPORT void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation, MayExtendDragSession = MayExtendDragSession::No);
#endif

    void focusDocumentView();
    
    WEBCORE_EXPORT void sendScrollEvent();

#if PLATFORM(MAC)
    WEBCORE_EXPORT void mouseDown(NSEvent *, NSEvent *correspondingPressureEvent);
    WEBCORE_EXPORT void mouseDragged(NSEvent *, NSEvent *correspondingPressureEvent);
    WEBCORE_EXPORT void mouseUp(NSEvent *, NSEvent *correspondingPressureEvent);
    WEBCORE_EXPORT void mouseMoved(NSEvent *, NSEvent *correspondingPressureEvent);
    WEBCORE_EXPORT void pressureChange(NSEvent *, NSEvent* correspondingPressureEvent);
    WEBCORE_EXPORT bool keyEvent(NSEvent *);
    WEBCORE_EXPORT bool wheelEvent(NSEvent *);
#endif

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void mouseDown(WebEvent *);
    WEBCORE_EXPORT void mouseUp(WebEvent *);
    WEBCORE_EXPORT void mouseMoved(WebEvent *);
    WEBCORE_EXPORT bool keyEvent(WebEvent *);
    WEBCORE_EXPORT bool wheelEvent(WebEvent *);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    WEBCORE_EXPORT void touchEvent(WebEvent *);
#endif

#if PLATFORM(MAC)
    WEBCORE_EXPORT void passMouseMovedEventToScrollbars(NSEvent *, NSEvent* correspondingPressureEvent);

    WEBCORE_EXPORT void sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent);

    void setActivationEventNumber(int num) { m_activationEventNumber = num; }

    WEBCORE_EXPORT static NSEvent *currentNSEvent();
    static NSEvent *correspondingPressureEvent();
#endif

#if PLATFORM(IOS_FAMILY)
    static WebEvent *currentEvent();

    void invalidateClick();
#endif

#if ENABLE(TOUCH_EVENTS)
    WEBCORE_EXPORT bool handleTouchEvent(const PlatformTouchEvent&);
#endif

    bool useHandCursor(Node*, bool isOverLink, bool shiftKey);
    void updateCursor();

    bool isHandlingWheelEvent() const { return m_isHandlingWheelEvent; }

    WEBCORE_EXPORT void setImmediateActionStage(ImmediateActionStage stage);
    ImmediateActionStage immediateActionStage() const { return m_immediateActionStage; }

    static Widget* widgetForEventTarget(Element* eventTarget);

#if ENABLE(DATA_INTERACTION)
    WEBCORE_EXPORT bool tryToBeginDataInteractionAtPoint(const IntPoint& clientPosition, const IntPoint& globalPosition);
#endif
    
#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void startSelectionAutoscroll(RenderObject* renderer, const FloatPoint& positionInWindow);
    WEBCORE_EXPORT void cancelSelectionAutoscroll();
    IntPoint m_targetAutoscrollPositionInWindow;
    bool m_isAutoscrolling { false };
#endif

private:
#if ENABLE(DRAG_SUPPORT)
    static DragState& dragState();
    static const Seconds TextDragDelay;
#endif

    bool eventActivatedView(const PlatformMouseEvent&) const;
    bool updateSelectionForMouseDownDispatchingSelectStart(Node*, const VisibleSelection&, TextGranularity);
    void selectClosestWordFromHitTestResult(const HitTestResult&, AppendTrailingWhitespace);
    VisibleSelection selectClosestWordFromHitTestResultBasedOnLookup(const HitTestResult&);
    void selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestContextualWordFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestContextualWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults&);

    bool handleMouseDoubleClickEvent(const PlatformMouseEvent&);

    WEBCORE_EXPORT bool handleMousePressEvent(const MouseEventWithHitTestResults&);
    bool handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);

#if ENABLE(DRAG_SUPPORT)
    bool handleMouseDraggedEvent(const MouseEventWithHitTestResults&, CheckDragHysteresis = ShouldCheckDragHysteresis);
#endif

    WEBCORE_EXPORT bool handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    bool internalKeyEvent(const PlatformKeyboardEvent&);

    std::optional<Cursor> selectCursor(const HitTestResult&, bool shiftKey);
    void updateCursor(FrameView&, const HitTestResult&, bool shiftKey);

    void hoverTimerFired();

#if ENABLE(CURSOR_SUPPORT)
    void cursorUpdateTimerFired();
#endif

    bool logicalScrollOverflow(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = nullptr);
    
    bool shouldSwapScrollDirection(const HitTestResult&, const PlatformWheelEvent&) const;
    
    bool mouseDownMayStartSelect() const { return m_mouseDownMayStartSelect; }

    static bool isKeyboardOptionTab(KeyboardEvent&);
    static bool eventInvertsTabsToLinksClientCallResult(KeyboardEvent&);

#if !ENABLE(IOS_TOUCH_EVENTS)
    void fakeMouseMoveEventTimerFired();
    void cancelFakeMouseMoveEvent();
#endif

    bool isInsideScrollbar(const IntPoint&) const;

#if ENABLE(TOUCH_EVENTS)
    bool dispatchSyntheticTouchEventIfEnabled(const PlatformMouseEvent&);
#endif

#if !PLATFORM(IOS_FAMILY)
    void invalidateClick();
#endif

    Node* nodeUnderMouse() const;
    
    void updateMouseEventTargetNode(Node*, const PlatformMouseEvent&, bool fireMouseOverOut);
    void fireMouseOverOut(bool fireMouseOver = true, bool fireMouseOut = true, bool updateLastNodeUnderMouse = true);
    
    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target, bool cancelable, int clickCount, const PlatformMouseEvent&, bool setUnder);

#if ENABLE(DRAG_SUPPORT)
    bool dispatchDragEvent(const AtomicString& eventType, Element& target, const PlatformMouseEvent&, DataTransfer&);
    DragTargetResponse dispatchDragEnterOrDragOverEvent(const AtomicString& eventType, Element& target, const PlatformMouseEvent&, std::unique_ptr<Pasteboard>&& , DragOperation, bool draggingFiles);
    void invalidateDataTransfer();

    bool handleDrag(const MouseEventWithHitTestResults&, CheckDragHysteresis);
#endif

    bool handleMouseUp(const MouseEventWithHitTestResults&);

#if ENABLE(DRAG_SUPPORT)
    void clearDragState();

    void dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);
    bool dispatchDragStartEventOnSourceElement(DataTransfer&);

    bool dragHysteresisExceeded(const FloatPoint&) const;
    bool dragHysteresisExceeded(const IntPoint&) const;
#endif
    
    bool mouseMovementExceedsThreshold(const FloatPoint&, int pointsThreshold) const;

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = nullptr);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);

    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = nullptr);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&, Scrollbar*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);

    bool passMouseDownEventToWidget(Widget*);
    bool widgetDidHandleWheelEvent(const PlatformWheelEvent&, Widget&);
    bool completeWidgetWheelEvent(const PlatformWheelEvent&, const WeakPtr<Widget>&, const WeakPtr<ScrollableArea>&, ContainerNode*);

    void defaultSpaceEventHandler(KeyboardEvent&);
    void defaultBackspaceEventHandler(KeyboardEvent&);
    void defaultTabEventHandler(KeyboardEvent&);
    void defaultArrowEventHandler(FocusDirection, KeyboardEvent&);

#if ENABLE(DRAG_SUPPORT)
    DragSourceAction updateDragSourceActionsAllowed() const;
#endif

    // The following are called at the beginning of handleMouseUp and handleDrag.  
    // If they return true it indicates that they have consumed the event.
    bool eventLoopHandleMouseUp(const MouseEventWithHitTestResults&);

#if ENABLE(DRAG_SUPPORT)
    bool eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&);
    void updateSelectionForMouseDrag(const HitTestResult&);
#endif

    enum class SetOrClearLastScrollbar { Clear, Set };
    void updateLastScrollbarUnderMouse(Scrollbar*, SetOrClearLastScrollbar);
    
    void setFrameWasScrolledByUser();

    bool capturesDragging() const { return m_capturesDragging; }

#if PLATFORM(COCOA) && defined(__OBJC__)
    NSView *mouseDownViewIfStillGood();

    PlatformMouseEvent currentPlatformMouseEvent() const;
#endif

#if ENABLE(FULLSCREEN_API)
    bool isKeyEventAllowedInFullScreen(const PlatformKeyboardEvent&) const;
#endif

    void setLastKnownMousePosition(const PlatformMouseEvent&);

#if ENABLE(CURSOR_VISIBILITY)
    void startAutoHideCursorTimer();
    void cancelAutoHideCursorTimer();
    void autoHideCursorTimerFired();
#endif

    void clearOrScheduleClearingLatchedStateIfNeeded(const PlatformWheelEvent&);
    void clearLatchedState();

    bool shouldSendMouseEventsToInactiveWindows() const;

    Frame& m_frame;

    bool m_mousePressed { false };
    bool m_capturesDragging { false };
    RefPtr<Node> m_mousePressNode;

    bool m_mouseDownMayStartSelect { false };

#if ENABLE(DRAG_SUPPORT)
    bool m_mouseDownMayStartDrag { false };
    bool m_dragMayStartSelectionInstead { false };
#endif

    bool m_mouseDownWasSingleClickInSelection { false };
    enum SelectionInitiationState { HaveNotStartedSelection, PlacedCaret, ExtendedSelection };
    SelectionInitiationState m_selectionInitiationState { HaveNotStartedSelection };

#if ENABLE(DRAG_SUPPORT)
    LayoutPoint m_dragStartPosition;
#endif

    Timer m_hoverTimer;

#if ENABLE(CURSOR_SUPPORT)
    Timer m_cursorUpdateTimer;
#endif

#if PLATFORM(MAC)
    Timer m_pendingMomentumWheelEventsTimer;
#endif

    std::unique_ptr<AutoscrollController> m_autoscrollController;
    bool m_mouseDownMayStartAutoscroll { false };
    bool m_mouseDownWasInSubframe { false };

#if !ENABLE(IOS_TOUCH_EVENTS)
    Timer m_fakeMouseMoveEventTimer;
#endif

    bool m_svgPan { false };

    RenderLayer* m_resizeLayer { nullptr };

    RefPtr<Element> m_capturingMouseEventsElement;
    bool m_eventHandlerWillResetCapturingMouseEventsElement { false };
    
    RefPtr<Element> m_elementUnderMouse;
    RefPtr<Element> m_lastElementUnderMouse;
    RefPtr<Frame> m_lastMouseMoveEventSubframe;
    WeakPtr<Scrollbar> m_lastScrollbarUnderMouse;
    Cursor m_currentMouseCursor;

    int m_clickCount { 0 };
    RefPtr<Node> m_clickNode;

#if ENABLE(IOS_GESTURE_EVENTS)
    float m_gestureInitialDiameter { GestureUnknown };
    float m_gestureInitialRotation { GestureUnknown };
#endif

#if ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
    float m_gestureLastDiameter { GestureUnknown };
    float m_gestureLastRotation { GestureUnknown };
    EventTargetSet m_gestureTargets;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    bool m_hasActiveGesture { false };
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    unsigned m_firstTouchID { InvalidTouchIdentifier };

    TouchArray m_touches;
    RefPtr<Frame> m_touchEventTargetSubframe;
#endif

#if ENABLE(DRAG_SUPPORT)
    RefPtr<Element> m_dragTarget;
    bool m_shouldOnlyFireDragOverEvent { false };
#endif
    
    RefPtr<HTMLFrameSetElement> m_frameSetBeingResized;

    LayoutSize m_offsetFromResizeCorner; // In the coords of m_resizeLayer.
    
    bool m_mousePositionIsUnknown { true };
    IntPoint m_lastKnownMousePosition;
    IntPoint m_lastKnownMouseGlobalPosition;
    IntPoint m_mouseDownPos; // In our view's coords.
    WallTime m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDown;

#if PLATFORM(COCOA)
    NSView *m_mouseDownView { nullptr };
    bool m_sendingEventToSubview { false };
#endif

#if PLATFORM(MAC)
    int m_activationEventNumber { -1 };
#endif

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    using TouchTargetMap = HashMap<int, RefPtr<EventTarget>>;
    TouchTargetMap m_originatingTouchPointTargets;
    RefPtr<Document> m_originatingTouchPointDocument;
    unsigned m_originatingTouchPointTargetKey { 0 };
    bool m_touchPressed { false };
#endif

    double m_maxMouseMovedDuration { 0 };
    bool m_didStartDrag { false };
    bool m_isHandlingWheelEvent { false };

#if ENABLE(CURSOR_VISIBILITY)
    Timer m_autoHideCursorTimer;
#endif

    ImmediateActionStage m_immediateActionStage { ImmediateActionStage::None };
};

} // namespace WebCore
