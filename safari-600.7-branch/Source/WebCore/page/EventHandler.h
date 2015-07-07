/*
 * Copyright (C) 2006-2014 Apple Inc. All rights reserved.
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

#ifndef EventHandler_h
#define EventHandler_h

#include "Cursor.h"
#include "DragActions.h"
#include "FocusDirection.h"
#include "HitTestRequest.h"
#include "LayoutPoint.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollTypes.h"
#include "TextEventInputType.h"
#include "TextGranularity.h"
#include "Timer.h"
#include "WheelEventDeltaTracker.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

#if PLATFORM(IOS)
#ifdef __OBJC__
@class WebEvent;
@class WAKView;
#include "WAKAppKitStubs.h"
#else
class WebEvent;
#endif
#endif // PLATFORM(IOS)

#if PLATFORM(COCOA) && !defined(__OBJC__)
class NSView;
#endif

#if ENABLE(TOUCH_EVENTS)
#include <wtf/HashMap.h>
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#endif

namespace WebCore {

class AutoscrollController;
class ContainerNode;
class DataTransfer;
class Document;
class Element;
class Event;
class EventTarget;
class FloatPoint;
class FloatQuad;
class Frame;
class HTMLFrameSetElement;
class HitTestRequest;
class HitTestResult;
class KeyboardEvent;
class MouseEventWithHitTestResults;
class Node;
class OptionalCursor;
class PlatformKeyboardEvent;
class PlatformTouchEvent;
class PlatformWheelEvent;
class RenderBox;
class RenderElement;
class RenderLayer;
class RenderWidget;
class ScrollableArea;
class SVGElementInstance;
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
extern const int GeneralDragHysteresis;
#endif // ENABLE(DRAG_SUPPORT)

#if ENABLE(IOS_GESTURE_EVENTS)
extern const float GestureUnknown;
#endif

enum AppendTrailingWhitespace { ShouldAppendTrailingWhitespace, DontAppendTrailingWhitespace };
enum CheckDragHysteresis { ShouldCheckDragHysteresis, DontCheckDragHysteresis };

class EventHandler {
    WTF_MAKE_NONCOPYABLE(EventHandler);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit EventHandler(Frame&);
    ~EventHandler();

    void clear();
    void nodeWillBeRemoved(Node*);

#if ENABLE(DRAG_SUPPORT)
    void updateSelectionForMouseDrag();
#endif

#if ENABLE(PAN_SCROLLING)
    void didPanScrollStart();
    void didPanScrollStop();
    void startPanScrolling(RenderElement*);
#endif

    void stopAutoscrollTimer(bool rendererIsBeingDestroyed = false);
    RenderBox* autoscrollRenderer() const;
    void updateAutoscrollRenderer();
    bool autoscrollInProgress() const;
    bool mouseDownWasInSubframe() const { return m_mouseDownWasInSubframe; }
    bool panScrollInProgress() const;

    void dispatchFakeMouseMoveEventSoon();
    void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

    HitTestResult hitTestResultAtPoint(const LayoutPoint&,
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent,
        const LayoutSize& padding = LayoutSize());

    bool mousePressed() const { return m_mousePressed; }
    Node* mousePressNode() const { return m_mousePressNode.get(); }

    void setCapturingMouseEventsElement(PassRefPtr<Element>); // A caller is responsible for resetting capturing element to 0.

#if ENABLE(DRAG_SUPPORT)
    bool updateDragAndDrop(const PlatformMouseEvent&, DataTransfer*);
    void cancelDragAndDrop(const PlatformMouseEvent&, DataTransfer*);
    bool performDragAndDrop(const PlatformMouseEvent&, DataTransfer*);
    void updateDragStateAfterEditDragIfNeeded(Element* rootEditableElement);
#endif

    void scheduleHoverStateUpdate();
#if ENABLE(CURSOR_SUPPORT)
    void scheduleCursorUpdate();
#endif

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeLayerDestroyed();

    IntPoint lastKnownMousePosition() const;
    Cursor currentMouseCursor() const { return m_currentMouseCursor; }

    static Frame* subframeForTargetNode(Node*);
    static Frame* subframeForHitTestResult(const MouseEventWithHitTestResults&);

    bool scrollOverflow(ScrollDirection, ScrollGranularity, Node* startingNode = 0);
    bool scrollRecursively(ScrollDirection, ScrollGranularity, Node* startingNode = 0);
    bool logicalScrollRecursively(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = 0);

    bool tabsToLinks(KeyboardEvent*) const;
    bool tabsToAllFormControls(KeyboardEvent*) const;

    bool mouseMoved(const PlatformMouseEvent&);
    bool passMouseMovedEventToScrollbars(const PlatformMouseEvent&);

    void lostMouseCapture();

    bool handleMousePressEvent(const PlatformMouseEvent&);
    bool handleMouseMoveEvent(const PlatformMouseEvent&, HitTestResult* hoveredNode = 0, bool onlyUpdateScrollbars = false);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleWheelEvent(const PlatformWheelEvent&);
    void defaultWheelEventHandler(Node*, WheelEvent*);
    bool handlePasteGlobalSelection(const PlatformMouseEvent&);

    void platformPrepareForWheelEvents(const PlatformWheelEvent&, const HitTestResult&, RefPtr<Element>& eventTarget, RefPtr<ContainerNode>& scrollableContainer, ScrollableArea*&, bool& isOverWidget);
    void platformRecordWheelEvent(const PlatformWheelEvent&);
    bool platformCompleteWheelEvent(const PlatformWheelEvent&, Element* eventTarget, ContainerNode* scrollableContainer, ScrollableArea*);
    bool platformCompletePlatformWidgetWheelEvent(const PlatformWheelEvent&, const Widget&, ContainerNode* scrollableContainer);

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    typedef Vector<RefPtr<Touch>> TouchArray;
    typedef HashMap<EventTarget*, TouchArray*> EventTargetTouchMap;
    typedef HashSet<RefPtr<EventTarget>> EventTargetSet;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    bool dispatchTouchEvent(const PlatformTouchEvent&, const AtomicString&, const EventTargetTouchMap&, float, float);
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
    bool dispatchGestureEvent(const PlatformTouchEvent&, const AtomicString&, const EventTargetSet&, float, float);
#endif

#if PLATFORM(IOS)
    void defaultTouchEventHandler(Node*, TouchEvent*);
#endif

#if ENABLE(CONTEXT_MENUS)
    bool sendContextMenuEvent(const PlatformMouseEvent&);
    bool sendContextMenuEventForKey();
#endif

    void setMouseDownMayStartAutoscroll() { m_mouseDownMayStartAutoscroll = true; }

    bool needsKeyboardEventDisambiguationQuirks() const;

    static unsigned accessKeyModifiers();
    bool handleAccessKey(const PlatformKeyboardEvent&);
    bool keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent*);

    void handleKeyboardSelectionMovementForAccessibility(KeyboardEvent*);

    bool handleTextInputEvent(const String& text, Event* underlyingEvent = 0, TextEventInputType = TextEventInputKeyboard);
    void defaultTextInputEventHandler(TextEvent*);

#if ENABLE(DRAG_SUPPORT)
    bool eventMayStartDrag(const PlatformMouseEvent&) const;
    
    void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation);
#endif

    void focusDocumentView();

    void capsLockStateMayHaveChanged(); // Only called by FrameSelection
    
    void sendScrollEvent(); // Ditto

#if PLATFORM(COCOA) && defined(__OBJC__)
#if !PLATFORM(IOS)
    void mouseDown(NSEvent *);
    void mouseDragged(NSEvent *);
    void mouseUp(NSEvent *);
    void mouseMoved(NSEvent *);
    bool keyEvent(NSEvent *);
    bool wheelEvent(NSEvent *);
#else
    void mouseDown(WebEvent *);
    void mouseUp(WebEvent *);
    void mouseMoved(WebEvent *);
    bool keyEvent(WebEvent *);
    bool wheelEvent(WebEvent *);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEvent(WebEvent *);
#endif

#if !PLATFORM(IOS)
    void passMouseMovedEventToScrollbars(NSEvent *);

    void sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent);
#endif

#if !PLATFORM(IOS)
    void setActivationEventNumber(int num) { m_activationEventNumber = num; }

    static NSEvent *currentNSEvent();
#else
    static WebEvent *currentEvent();
#endif // !PLATFORM(IOS)
#endif // PLATFORM(COCOA) && defined(__OBJC__)

#if PLATFORM(IOS)
    void invalidateClick();
#endif

#if ENABLE(TOUCH_EVENTS)
    bool handleTouchEvent(const PlatformTouchEvent&);
#endif

    bool useHandCursor(Node*, bool isOverLink, bool shiftKey);
    void updateCursor();

    bool isHandlingWheelEvent() const { return m_isHandlingWheelEvent; }

private:
#if ENABLE(DRAG_SUPPORT)
    static DragState& dragState();
    static const double TextDragDelay;
    
    PassRefPtr<DataTransfer> createDraggingDataTransfer() const;
#endif // ENABLE(DRAG_SUPPORT)

    bool eventActivatedView(const PlatformMouseEvent&) const;
    bool updateSelectionForMouseDownDispatchingSelectStart(Node*, const VisibleSelection&, TextGranularity);
    void selectClosestWordFromHitTestResult(const HitTestResult&, AppendTrailingWhitespace);
    void selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults&);

    bool handleMouseDoubleClickEvent(const PlatformMouseEvent&);

    bool handleMousePressEvent(const MouseEventWithHitTestResults&);
    bool handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);
#if ENABLE(DRAG_SUPPORT)
    bool handleMouseDraggedEvent(const MouseEventWithHitTestResults&);
#endif
    bool handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    OptionalCursor selectCursor(const HitTestResult&, bool shiftKey);

    void hoverTimerFired(Timer&);
#if ENABLE(CURSOR_SUPPORT)
    void cursorUpdateTimerFired(Timer&);
#endif

    bool logicalScrollOverflow(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = 0);
    
    bool shouldTurnVerticalTicksIntoHorizontal(const HitTestResult&, const PlatformWheelEvent&) const;
    
    bool mouseDownMayStartSelect() const { return m_mouseDownMayStartSelect; }

    static bool isKeyboardOptionTab(KeyboardEvent*);
    static bool eventInvertsTabsToLinksClientCallResult(KeyboardEvent*);

#if !ENABLE(IOS_TOUCH_EVENTS)
    void fakeMouseMoveEventTimerFired(Timer&);
    void cancelFakeMouseMoveEvent();
#endif

    bool isInsideScrollbar(const IntPoint&) const;

#if ENABLE(TOUCH_EVENTS)
    bool dispatchSyntheticTouchEventIfEnabled(const PlatformMouseEvent&);
#endif

#if !PLATFORM(IOS)
    void invalidateClick();
#endif

    Node* nodeUnderMouse() const;
    
    void updateMouseEventTargetNode(Node*, const PlatformMouseEvent&, bool fireMouseOverOut);
    void fireMouseOverOut(bool fireMouseOver = true, bool fireMouseOut = true, bool updateLastNodeUnderMouse = true);
    
    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target, bool cancelable, int clickCount, const PlatformMouseEvent&, bool setUnder);
#if ENABLE(DRAG_SUPPORT)
    bool dispatchDragEvent(const AtomicString& eventType, Element& target, const PlatformMouseEvent&, DataTransfer*);

    void freeDataTransfer();

    bool handleDrag(const MouseEventWithHitTestResults&, CheckDragHysteresis);
#endif
    bool handleMouseUp(const MouseEventWithHitTestResults&);
#if ENABLE(DRAG_SUPPORT)
    void clearDragState();

    bool dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);

    bool dragHysteresisExceeded(const FloatPoint&) const;
    bool dragHysteresisExceeded(const IntPoint&) const;
#endif // ENABLE(DRAG_SUPPORT)

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = 0);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);

    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = 0);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&, Scrollbar*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);

    bool passMouseDownEventToWidget(Widget*);
    bool passWheelEventToWidget(const PlatformWheelEvent&, Widget*);

    void defaultSpaceEventHandler(KeyboardEvent*);
    void defaultBackspaceEventHandler(KeyboardEvent*);
    void defaultTabEventHandler(KeyboardEvent*);
    void defaultArrowEventHandler(FocusDirection, KeyboardEvent*);

#if ENABLE(DRAG_SUPPORT)
    DragSourceAction updateDragSourceActionsAllowed() const;
#endif

    // The following are called at the beginning of handleMouseUp and handleDrag.  
    // If they return true it indicates that they have consumed the event.
    bool eventLoopHandleMouseUp(const MouseEventWithHitTestResults&);
#if ENABLE(DRAG_SUPPORT)
    bool eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&);
#endif

#if ENABLE(DRAG_SUPPORT)
    void updateSelectionForMouseDrag(const HitTestResult&);
#endif

    void updateLastScrollbarUnderMouse(Scrollbar*, bool);
    
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
    void autoHideCursorTimerFired(Timer&);
#endif

    void clearLatchedState();

    Frame& m_frame;

    bool m_mousePressed;
    bool m_capturesDragging;
    RefPtr<Node> m_mousePressNode;

    bool m_mouseDownMayStartSelect;
#if ENABLE(DRAG_SUPPORT)
    bool m_mouseDownMayStartDrag;
    bool m_dragMayStartSelectionInstead;
#endif
    bool m_mouseDownWasSingleClickInSelection;
    enum SelectionInitiationState { HaveNotStartedSelection, PlacedCaret, ExtendedSelection };
    SelectionInitiationState m_selectionInitiationState;

#if ENABLE(DRAG_SUPPORT)
    LayoutPoint m_dragStartPos;
#endif

    bool m_panScrollButtonPressed;

    Timer m_hoverTimer;
#if ENABLE(CURSOR_SUPPORT)
    Timer m_cursorUpdateTimer;
#endif

    std::unique_ptr<AutoscrollController> m_autoscrollController;
    bool m_mouseDownMayStartAutoscroll;
    bool m_mouseDownWasInSubframe;

#if !ENABLE(IOS_TOUCH_EVENTS)
    Timer m_fakeMouseMoveEventTimer;
#endif

    bool m_svgPan;
    RefPtr<SVGElementInstance> m_instanceUnderMouse;
    RefPtr<SVGElementInstance> m_lastInstanceUnderMouse;

    RenderLayer* m_resizeLayer;

    RefPtr<Element> m_capturingMouseEventsElement;
    bool m_eventHandlerWillResetCapturingMouseEventsElement;
    
    RefPtr<Element> m_elementUnderMouse;
    RefPtr<Element> m_lastElementUnderMouse;
    RefPtr<Frame> m_lastMouseMoveEventSubframe;
    RefPtr<Scrollbar> m_lastScrollbarUnderMouse;
    Cursor m_currentMouseCursor;

    int m_clickCount;
    RefPtr<Node> m_clickNode;

#if ENABLE(IOS_GESTURE_EVENTS)
    float m_gestureInitialDiameter;
    float m_gestureLastDiameter;
    float m_gestureInitialRotation;
    float m_gestureLastRotation;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    unsigned m_firstTouchID;

    TouchArray m_touches;
    EventTargetSet m_gestureTargets;
    RefPtr<Frame> m_touchEventTargetSubframe;
#endif

#if ENABLE(DRAG_SUPPORT)
    RefPtr<Element> m_dragTarget;
    bool m_shouldOnlyFireDragOverEvent;
#endif
    
    RefPtr<HTMLFrameSetElement> m_frameSetBeingResized;

    LayoutSize m_offsetFromResizeCorner; // In the coords of m_resizeLayer.
    
    bool m_mousePositionIsUnknown;
    IntPoint m_lastKnownMousePosition;
    IntPoint m_lastKnownMouseGlobalPosition;
    IntPoint m_mouseDownPos; // In our view's coords.
    double m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDown;

    std::unique_ptr<WheelEventDeltaTracker> m_recentWheelEventDeltaTracker;
    RefPtr<Element> m_latchedWheelEventElement;
    bool m_widgetIsLatched;

    RefPtr<Element> m_previousWheelScrolledElement;

#if PLATFORM(COCOA)
    NSView *m_mouseDownView;
    RefPtr<ContainerNode> m_latchedScrollableContainer;
    bool m_sendingEventToSubview;
    bool m_startedGestureAtScrollLimit;
#if !PLATFORM(IOS)
    int m_activationEventNumber;
#endif
#endif
#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    typedef HashMap<int, RefPtr<EventTarget>> TouchTargetMap;
    TouchTargetMap m_originatingTouchPointTargets;
    RefPtr<Document> m_originatingTouchPointDocument;
    unsigned m_originatingTouchPointTargetKey;
    bool m_touchPressed;
#endif

    double m_maxMouseMovedDuration;
    PlatformEvent::Type m_baseEventType;
    bool m_didStartDrag;
    bool m_didLongPressInvokeContextMenu;
    bool m_isHandlingWheelEvent;

#if ENABLE(CURSOR_VISIBILITY)
    Timer m_autoHideCursorTimer;
#endif
};

} // namespace WebCore

#endif // EventHandler_h
