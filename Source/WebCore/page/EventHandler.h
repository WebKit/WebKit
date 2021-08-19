/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
#include "KeyboardScroll.h"
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

enum class WheelEventProcessingSteps : uint8_t;
enum class WheelScrollGestureState : uint8_t;

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

enum class ImmediateActionStage : uint8_t {
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

    WEBCORE_EXPORT HitTestResult hitTestResultAtPoint(const LayoutPoint&, OptionSet<HitTestRequest::Type>) const;

    bool mousePressed() const { return m_mousePressed; }
    Node* mousePressNode() const { return m_mousePressNode.get(); }

    WEBCORE_EXPORT void setCapturingMouseEventsElement(Element*);
    void pointerCaptureElementDidChange(Element*);

#if ENABLE(DRAG_SUPPORT)
    struct DragTargetResponse {
        bool accept { false };
        std::optional<OptionSet<DragOperation>> operationMask;
    };
    DragTargetResponse updateDragAndDrop(const PlatformMouseEvent&, const std::function<std::unique_ptr<Pasteboard>()>&, OptionSet<DragOperation>, bool draggingFiles);
    void cancelDragAndDrop(const PlatformMouseEvent&, std::unique_ptr<Pasteboard>&&, OptionSet<DragOperation>, bool draggingFiles);
    bool performDragAndDrop(const PlatformMouseEvent&, std::unique_ptr<Pasteboard>&&, OptionSet<DragOperation>, bool draggingFiles);
    void updateDragStateAfterEditDragIfNeeded(Element& rootEditableElement);
    RefPtr<Element> draggedElement() const;
#endif

    void scheduleHoverStateUpdate();
    void scheduleCursorUpdate();

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeLayerDestroyed();

    // FIXME: Each Frame has an EventHandler, and not every event goes to all frames, so this position can be stale. It should probably be stored on Page.
    IntPoint lastKnownMousePosition() const;
    IntPoint lastKnownMouseGlobalPosition() const { return m_lastKnownMouseGlobalPosition; }
    Cursor currentMouseCursor() const { return m_currentMouseCursor; }

    IntPoint targetPositionInWindowForSelectionAutoscroll() const;
    bool shouldUpdateAutoscroll();

    static RefPtr<Frame> subframeForTargetNode(Node*);
    static RefPtr<Frame> subframeForHitTestResult(const MouseEventWithHitTestResults&);

    WEBCORE_EXPORT bool scrollOverflow(ScrollDirection, ScrollGranularity, Node* startingNode = nullptr);
    WEBCORE_EXPORT bool scrollRecursively(ScrollDirection, ScrollGranularity, Node* startingNode = nullptr);
    WEBCORE_EXPORT bool logicalScrollRecursively(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = nullptr);

    bool tabsToLinks(KeyboardEvent*) const;
    bool tabsToAllFormControls(KeyboardEvent*) const;

    WEBCORE_EXPORT bool mouseMoved(const PlatformMouseEvent&);
    WEBCORE_EXPORT bool passMouseMovedEventToScrollbars(const PlatformMouseEvent&);

    void lostMouseCapture();

    WEBCORE_EXPORT bool handleMousePressEvent(const PlatformMouseEvent&);
    bool handleMouseMoveEvent(const PlatformMouseEvent&, HitTestResult* = nullptr, bool onlyUpdateScrollbars = false);
    WEBCORE_EXPORT bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleMouseForceEvent(const PlatformMouseEvent&);

    WEBCORE_EXPORT bool handleWheelEvent(const PlatformWheelEvent&, OptionSet<WheelEventProcessingSteps>);
    void defaultWheelEventHandler(Node*, WheelEvent&);
    void wheelEventWasProcessedByMainThread(const PlatformWheelEvent&, OptionSet<EventHandling>);

    bool handlePasteGlobalSelection(const PlatformMouseEvent&);

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    using TouchArray = Vector<RefPtr<Touch>>;
    using EventTargetTouchMap = HashMap<EventTarget*, TouchArray*>;
    using EventTargetTouchArrayMap = HashMap<Ref<EventTarget>, std::unique_ptr<TouchArray>>;
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
    using EventTargetSet = HashSet<RefPtr<EventTarget>>;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    bool dispatchTouchEvent(const PlatformTouchEvent&, const AtomString&, const EventTargetTouchMap&, float, float);
    bool dispatchTouchEvent(const PlatformTouchEvent&, const AtomString&, const EventTargetTouchArrayMap&, float, float);
    bool dispatchSimulatedTouchEvent(IntPoint location);
    Frame* touchEventTargetSubframe() const { return m_touchEventTargetSubframe.get(); }
    const TouchArray& touches() const { return m_touches; }
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
    bool dispatchGestureEvent(const PlatformTouchEvent&, const AtomString&, const EventTargetSet&, float, float);
#elif ENABLE(MAC_GESTURE_EVENTS)
    bool dispatchGestureEvent(const PlatformGestureEvent&, const AtomString&, const EventTargetSet&, float, float);
    WEBCORE_EXPORT bool handleGestureEvent(const PlatformGestureEvent&);
    WEBCORE_EXPORT void didEndMagnificationGesture();
#endif

#if PLATFORM(IOS_FAMILY)
    void defaultTouchEventHandler(Node&, TouchEvent&);
    WEBCORE_EXPORT void dispatchSyntheticMouseOut(const PlatformMouseEvent&);
    WEBCORE_EXPORT void dispatchSyntheticMouseMove(const PlatformMouseEvent&);
#endif

#if ENABLE(CONTEXT_MENU_EVENT)
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
    WEBCORE_EXPORT void dragSourceEndedAt(const PlatformMouseEvent&, OptionSet<DragOperation>, MayExtendDragSession = MayExtendDragSession::No);
#endif

    void focusDocumentView();
    
    WEBCORE_EXPORT void scheduleScrollEvent();

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
#endif

#if ENABLE(TOUCH_EVENTS)
    WEBCORE_EXPORT bool handleTouchEvent(const PlatformTouchEvent&);
#endif

    bool useHandCursor(Node*, bool isOverLink, bool shiftKey);
    void updateCursorIfNeeded();
    void updateCursor();

    bool isHandlingWheelEvent() const { return m_isHandlingWheelEvent; }

    WEBCORE_EXPORT void setImmediateActionStage(ImmediateActionStage stage);
    ImmediateActionStage immediateActionStage() const { return m_immediateActionStage; }

    static Widget* widgetForEventTarget(Element* eventTarget);

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    WEBCORE_EXPORT bool tryToBeginDragAtPoint(const IntPoint& clientPosition, const IntPoint& globalPosition);
#endif
    
#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void startSelectionAutoscroll(RenderObject* renderer, const FloatPoint& positionInWindow);
    WEBCORE_EXPORT void cancelSelectionAutoscroll();
#endif

    WEBCORE_EXPORT std::optional<Cursor> selectCursor(const HitTestResult&, bool shiftKey);

#if ENABLE(KINETIC_SCROLLING)
    std::optional<WheelScrollGestureState> wheelScrollGestureState() const { return m_wheelScrollGestureState; }
#endif

#if ENABLE(DRAG_SUPPORT)
    Element* draggingElement() const;
#endif

    WEBCORE_EXPORT void invalidateClick();

    static bool scrollableAreaCanHandleEvent(const PlatformWheelEvent&, ScrollableArea&);

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

    float scrollDistance(ScrollDirection, ScrollGranularity);
    bool startKeyboardScrolling(KeyboardEvent&);
    void stopKeyboardScrolling();

#if ENABLE(DRAG_SUPPORT)
    bool handleMouseDraggedEvent(const MouseEventWithHitTestResults&, CheckDragHysteresis = ShouldCheckDragHysteresis);
    bool shouldAllowMouseDownToStartDrag() const;
#endif

    WEBCORE_EXPORT bool handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    bool internalKeyEvent(const PlatformKeyboardEvent&);

    void updateCursor(FrameView&, const HitTestResult&, bool shiftKey);

    void hoverTimerFired();

#if ENABLE(IMAGE_ANALYSIS)
    void textRecognitionHoverTimerFired();
#endif

    bool logicalScrollOverflow(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = nullptr);
    
    bool shouldSwapScrollDirection(const HitTestResult&, const PlatformWheelEvent&) const;

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

    Node* nodeUnderMouse() const;
    
    enum class FireMouseOverOut { No, Yes };
    void updateMouseEventTargetNode(const AtomString& eventType, Node*, const PlatformMouseEvent&, FireMouseOverOut);

    ScrollableArea* enclosingScrollableArea(Node*);
    void notifyScrollableAreasOfMouseEvents(const AtomString& eventType, Element* lastElementUnderMouse, Element* elementUnderMouse);

    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    enum class Cancelable { No, Yes };
    bool dispatchMouseEvent(const AtomString& eventType, Node* target, int clickCount, const PlatformMouseEvent&, FireMouseOverOut);

#if ENABLE(DRAG_SUPPORT)
    bool dispatchDragEvent(const AtomString& eventType, Element& target, const PlatformMouseEvent&, DataTransfer&);
    DragTargetResponse dispatchDragEnterOrDragOverEvent(const AtomString& eventType, Element& target, const PlatformMouseEvent&, std::unique_ptr<Pasteboard>&& , OptionSet<DragOperation>, bool draggingFiles);
    void invalidateDataTransfer();

    bool handleDrag(const MouseEventWithHitTestResults&, CheckDragHysteresis);
#endif

    bool handleMouseUp(const MouseEventWithHitTestResults&);

#if ENABLE(DRAG_SUPPORT)
    void clearDragState();

    static bool shouldDispatchEventsToDragSourceElement();
    void dispatchEventToDragSourceElement(const AtomString& eventType, const PlatformMouseEvent&);
    bool dispatchDragStartEventOnSourceElement(DataTransfer&);

    bool dragHysteresisExceeded(const FloatPoint&) const;
    bool dragHysteresisExceeded(const IntPoint&) const;
#endif
    
    bool mouseMovementExceedsThreshold(const FloatPoint&, int pointsThreshold) const;

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, Frame&);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, Frame&, HitTestResult* = nullptr);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, Frame&);

    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame&, HitTestResult* = nullptr);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&, Scrollbar*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);

    bool passMouseDownEventToWidget(Widget*);

    bool handleWheelEventInternal(const PlatformWheelEvent&, OptionSet<WheelEventProcessingSteps>, OptionSet<EventHandling>&);
    bool passWheelEventToWidget(const PlatformWheelEvent&, Widget&, OptionSet<WheelEventProcessingSteps>);
    void determineWheelEventTarget(const PlatformWheelEvent&, RefPtr<Element>& eventTarget, WeakPtr<ScrollableArea>&, bool& isOverWidget);
    void recordWheelEventForDeltaFilter(const PlatformWheelEvent&);
    bool processWheelEventForScrolling(const PlatformWheelEvent&, const WeakPtr<ScrollableArea>&, OptionSet<EventHandling>);
    void processWheelEventForScrollSnap(const PlatformWheelEvent&, const WeakPtr<ScrollableArea>&);
    bool completeWidgetWheelEvent(const PlatformWheelEvent&, const WeakPtr<Widget>&, const WeakPtr<ScrollableArea>&);

    bool handleWheelEventInAppropriateEnclosingBox(Node* startNode, const WheelEvent&, const FloatSize& filteredPlatformDelta, const FloatSize& filteredVelocity, OptionSet<EventHandling>);

    bool handleWheelEventInScrollableArea(const PlatformWheelEvent&, ScrollableArea&, OptionSet<EventHandling>);
    std::optional<WheelScrollGestureState> updateWheelGestureState(const PlatformWheelEvent&, OptionSet<EventHandling>);

    bool platformCompletePlatformWidgetWheelEvent(const PlatformWheelEvent&, const Widget&, const WeakPtr<ScrollableArea>&);

    void defaultSpaceEventHandler(KeyboardEvent&);
    void defaultBackspaceEventHandler(KeyboardEvent&);
    void defaultTabEventHandler(KeyboardEvent&);
    void defaultArrowEventHandler(FocusDirection, KeyboardEvent&);

#if ENABLE(DRAG_SUPPORT)
    OptionSet<DragSourceAction> updateDragSourceActionsAllowed() const;
    bool supportsSelectionUpdatesOnMouseDrag() const;
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

    void clearLatchedState();
    void clearElementUnderMouse();

    bool shouldSendMouseEventsToInactiveWindows() const;

    bool canMouseDownStartSelect(const MouseEventWithHitTestResults&);
    bool mouseDownMayStartSelect() const;
    
    Frame& m_frame;
    RefPtr<Node> m_mousePressNode;
    Timer m_hoverTimer;
#if ENABLE(IMAGE_ANALYSIS)
    DeferrableOneShotTimer m_textRecognitionHoverTimer;
#endif
    std::unique_ptr<AutoscrollController> m_autoscrollController;
    RenderLayer* m_resizeLayer { nullptr };

    double m_maxMouseMovedDuration { 0 };

    bool m_mousePressed { false };
    bool m_capturesDragging { false };
    bool m_mouseDownMayStartSelect { false };
    bool m_mouseDownDelegatedFocus { false };
    bool m_mouseDownWasSingleClickInSelection { false };
    bool m_hasScheduledCursorUpdate { false };
    bool m_mouseDownMayStartAutoscroll { false };
    bool m_mouseDownWasInSubframe { false };
    bool m_didStartDrag { false };
    bool m_isHandlingWheelEvent { false };
    bool m_currentWheelEventAllowsScrolling { true };
    bool m_svgPan { false };
    bool m_eventHandlerWillResetCapturingMouseEventsElement { false };

    enum SelectionInitiationState : uint8_t { HaveNotStartedSelection, PlacedCaret, ExtendedSelection };
    SelectionInitiationState m_selectionInitiationState { HaveNotStartedSelection };
    ImmediateActionStage m_immediateActionStage { ImmediateActionStage::None };

    RefPtr<Element> m_capturingMouseEventsElement;
    RefPtr<Element> m_elementUnderMouse;
    RefPtr<Element> m_lastElementUnderMouse;
    RefPtr<Frame> m_lastMouseMoveEventSubframe;
    WeakPtr<Scrollbar> m_lastScrollbarUnderMouse;
    Cursor m_currentMouseCursor;

    RefPtr<Node> m_clickNode;
    RefPtr<HTMLFrameSetElement> m_frameSetBeingResized;

    LayoutSize m_offsetFromResizeCorner; // In the coords of m_resizeLayer.
    
    int m_clickCount { 0 };

    std::optional<IntPoint> m_lastKnownMousePosition; // Same coordinates as PlatformMouseEvent::position().
    IntPoint m_lastKnownMouseGlobalPosition;
    IntPoint m_mouseDownContentsPosition;
    WallTime m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDownEvent;
    PlatformMouseEvent m_lastPlatformMouseEvent;

#if !ENABLE(IOS_TOUCH_EVENTS)
    Timer m_fakeMouseMoveEventTimer;
#endif

#if ENABLE(CURSOR_VISIBILITY)
    Timer m_autoHideCursorTimer;
#endif

#if ENABLE(DRAG_SUPPORT)
    LayoutPoint m_dragStartPosition;
    RefPtr<Element> m_dragTarget;
    bool m_mouseDownMayStartDrag { false };
    bool m_dragMayStartSelectionInstead { false };
    bool m_shouldOnlyFireDragOverEvent { false };
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    bool m_hasActiveGesture { false };
#endif

#if ENABLE(KINETIC_SCROLLING)
    std::optional<WheelScrollGestureState> m_wheelScrollGestureState;
#endif

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    using TouchTargetMap = HashMap<int, RefPtr<EventTarget>>;
    TouchTargetMap m_originatingTouchPointTargets;
    RefPtr<Document> m_originatingTouchPointDocument;
    unsigned m_originatingTouchPointTargetKey { 0 };
    bool m_touchPressed { false };
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
    float m_gestureInitialDiameter { GestureUnknown };
    float m_gestureInitialRotation { GestureUnknown };
#endif

#if ENABLE(IOS_GESTURE_EVENTS) || ENABLE(MAC_GESTURE_EVENTS)
    float m_gestureLastDiameter { GestureUnknown };
    float m_gestureLastRotation { GestureUnknown };
    EventTargetSet m_gestureTargets;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    unsigned m_firstTouchID { InvalidTouchIdentifier };
    unsigned touchIdentifierForMouseEvents { 0 };
    unsigned m_touchIdentifierForPrimaryTouch { 0 };

    TouchArray m_touches;
    RefPtr<Frame> m_touchEventTargetSubframe;
#endif

#if PLATFORM(COCOA)
    NSView *m_mouseDownView { nullptr };
    bool m_sendingEventToSubview { false };
#endif

#if PLATFORM(MAC)
    int m_activationEventNumber { -1 };
#endif

#if PLATFORM(IOS_FAMILY)
    bool m_shouldAllowMouseDownToStartDrag { false };
    bool m_isAutoscrolling { false };
    IntPoint m_targetAutoscrollPositionInUnscrolledRootViewCoordinates;
    std::optional<IntPoint> m_initialTargetAutoscrollPositionInUnscrolledRootViewCoordinates;
#endif
};

} // namespace WebCore
