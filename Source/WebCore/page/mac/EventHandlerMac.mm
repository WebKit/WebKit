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

#import "config.h"
#import "EventHandler.h"

#if PLATFORM(MAC)

#import "AXObjectCache.h"
#import "Chrome.h"
#import "ChromeClient.h"
#import "DataTransfer.h"
#import "DictionaryLookup.h"
#import "DragController.h"
#import "Editor.h"
#import "FocusController.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameView.h"
#import "HTMLBodyElement.h"
#import "HTMLDocument.h"
#import "HTMLFrameSetElement.h"
#import "HTMLHtmlElement.h"
#import "HTMLIFrameElement.h"
#import "KeyboardEvent.h"
#import "Logging.h"
#import "MouseEventWithHitTestResults.h"
#import "Page.h"
#import "Pasteboard.h"
#import "PlatformEventFactoryMac.h"
#import "PlatformScreen.h"
#import "Range.h"
#import "RenderLayer.h"
#import "RenderListBox.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "RuntimeApplicationChecks.h"
#import "ScreenProperties.h"
#import "ScrollAnimator.h"
#import "ScrollLatchingController.h"
#import "ScrollableArea.h"
#import "Scrollbar.h"
#import "Settings.h"
#import "ShadowRoot.h"
#import "SimpleRange.h"
#import "WheelEventDeltaFilter.h"
#import "WheelEventTestMonitor.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/text/TextStream.h>

#if ENABLE(MAC_GESTURE_EVENTS)
#import <WebKitAdditions/EventHandlerMacGesture.cpp>
#endif

namespace WebCore {

static const Seconds resetLatchedStateTimeout { 100_ms };

static RetainPtr<NSEvent>& currentNSEventSlot()
{
    static NeverDestroyed<RetainPtr<NSEvent>> event;
    return event;
}

NSEvent *EventHandler::currentNSEvent()
{
    return currentNSEventSlot().get();
}

static RetainPtr<NSEvent>& correspondingPressureEventSlot()
{
    static NeverDestroyed<RetainPtr<NSEvent>> event;
    return event;
}

NSEvent *EventHandler::correspondingPressureEvent()
{
    return correspondingPressureEventSlot().get();
}

class CurrentEventScope {
     WTF_MAKE_NONCOPYABLE(CurrentEventScope);
public:
    CurrentEventScope(NSEvent *, NSEvent *correspondingPressureEvent);
    ~CurrentEventScope();

private:
    RetainPtr<NSEvent> m_savedCurrentEvent;
#if ASSERT_ENABLED
    RetainPtr<NSEvent> m_event;
#endif
    RetainPtr<NSEvent> m_savedPressureEvent;
    RetainPtr<NSEvent> m_correspondingPressureEvent;
};

inline CurrentEventScope::CurrentEventScope(NSEvent *event, NSEvent *correspondingPressureEvent)
    : m_savedCurrentEvent(currentNSEventSlot())
#if ASSERT_ENABLED
    , m_event(event)
#endif
    , m_savedPressureEvent(correspondingPressureEventSlot())
    , m_correspondingPressureEvent(correspondingPressureEvent)
{
    currentNSEventSlot() = event;
    correspondingPressureEventSlot() = correspondingPressureEvent;
}

inline CurrentEventScope::~CurrentEventScope()
{
    ASSERT(currentNSEventSlot() == m_event);
    currentNSEventSlot() = m_savedCurrentEvent;
    correspondingPressureEventSlot() = m_savedPressureEvent;
}

bool EventHandler::wheelEvent(NSEvent *event)
{
    Page* page = m_frame.page();
    if (!page)
        return false;

    CurrentEventScope scope(event, nil);
    return handleWheelEvent(PlatformEventFactory::createPlatformWheelEvent(event, page->chrome().platformPageClient()));
}

bool EventHandler::keyEvent(NSEvent *event)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp);

    CurrentEventScope scope(event, nil);
    return keyEvent(PlatformEventFactory::createPlatformKeyboardEvent(event));

    END_BLOCK_OBJC_EXCEPTIONS

    return false;
}

void EventHandler::focusDocumentView()
{
    Page* page = m_frame.page();
    if (!page)
        return;

    if (auto frameView = makeRefPtr(m_frame.view())) {
        if (NSView *documentView = frameView->documentView()) {
            page->chrome().focusNSView(documentView);
            // Check page() again because focusNSView can cause reentrancy.
            if (!m_frame.page())
                return;
        }
    }

    RELEASE_ASSERT(page == m_frame.page());
    page->focusController().setFocusedFrame(&m_frame);
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults& event)
{
    // Figure out which view to send the event to.
    auto* target = event.targetNode() ? event.targetNode()->renderer() : nullptr;
    if (!is<RenderWidget>(target))
        return false;

    // Double-click events don't exist in Cocoa. Since passWidgetMouseDownEventToWidget() will
    // just pass currentEvent down to the widget, we don't want to call it for events that
    // don't correspond to Cocoa events. The mousedown/ups will have already been passed on as
    // part of the pressed/released handling.
    return passMouseDownEventToWidget(downcast<RenderWidget>(*target).widget());
}

bool EventHandler::passWidgetMouseDownEventToWidget(RenderWidget* renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

static bool lastEventIsMouseUp()
{
    // Many AppKit widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on. We need to update
    // the WebCore state with this mouseUp, which we never saw. This method lets us detect
    // that state. Handling this was critical when we used AppKit widgets for form elements.
    // It's not clear in what cases this is helpful now -- it's possible it can be removed. 

    ASSERT([NSApp isRunning]);
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSEvent *currentEventAfterHandlingMouseDown = [NSApp currentEvent];
    return EventHandler::currentNSEvent() != currentEventAfterHandlingMouseDown
        && [currentEventAfterHandlingMouseDown type] == NSEventTypeLeftMouseUp
        && [currentEventAfterHandlingMouseDown timestamp] >= [EventHandler::currentNSEvent() timestamp];
    END_BLOCK_OBJC_EXCEPTIONS

    return false;
}

bool EventHandler::passMouseDownEventToWidget(Widget* pWidget)
{
    // FIXME: This function always returns true. It should be changed either to return
    // false in some cases or the return value should be removed.

    RefPtr<Widget> widget = pWidget;

    if (!widget) {
        LOG_ERROR("hit a RenderWidget without a corresponding Widget, means a frame is half-constructed");
        return true;
    }

    // In WebKit2 we will never have a native widget. Just return early and let the regular event handler machinery take care of
    // dispatching the event.
    if (!widget->platformWidget())
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSView *nodeView = widget->platformWidget();
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[currentNSEvent() locationInWindow] fromView:nil]];
    if (!view) {
        // We probably hit the border of a RenderWidget
        return true;
    }

    Page* page = m_frame.page();
    if (!page)
        return true;

    if (page->chrome().client().firstResponder() != view) {
        // Normally [NSWindow sendEvent:] handles setting the first responder.
        // But in our case, the event was sent to the view representing the entire web page.
        if ([currentNSEvent() clickCount] <= 1 && [view acceptsFirstResponder] && [view needsPanelToBecomeKey])
            page->chrome().client().makeFirstResponder(view);
    }

    // We need to "defer loading" while tracking the mouse, because tearing down the
    // page while an AppKit control is tracking the mouse can cause a crash.

    // FIXME: In theory, WebCore now tolerates tear-down while tracking the
    // mouse. We should confirm that, and then remove the deferrsLoading
    // hack entirely.

    bool wasDeferringLoading = page->defersLoading();
    if (!wasDeferringLoading)
        page->setDefersLoading(true);

    ASSERT(!m_sendingEventToSubview);
    m_sendingEventToSubview = true;

    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        [view mouseDown:currentNSEvent()];
    }

    m_sendingEventToSubview = false;
    
    if (!wasDeferringLoading)
        page->setDefersLoading(false);

    // Remember which view we sent the event to, so we can direct the release event properly.
    m_mouseDownView = view;
    m_mouseDownWasInSubframe = false;

    // Many AppKit widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
    // the EventHandler state with this mouseUp, which we never saw.
    // If this event isn't a mouseUp, we assume that the mouseUp will be coming later.  There
    // is a hole here if the widget consumes both the mouseUp and subsequent events.
    if (lastEventIsMouseUp())
        m_mousePressed = false;

    END_BLOCK_OBJC_EXCEPTIONS

    return true;
}

// Note that this does the same kind of check as [target isDescendantOf:superview].
// There are two differences: This is a lot slower because it has to walk the whole
// tree, and this works in cases where the target has already been deallocated.
static bool findViewInSubviews(NSView *superview, NSView *target)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSEnumerator *e = [[superview subviews] objectEnumerator];
    NSView *subview;
    while ((subview = [e nextObject])) {
        if (subview == target || findViewInSubviews(subview, target)) {
            return true;
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS

    return false;
}

NSView *EventHandler::mouseDownViewIfStillGood()
{
    // Since we have no way of tracking the lifetime of m_mouseDownView, we have to assume that
    // it could be deallocated already. We search for it in our subview tree; if we don't find
    // it, we set it to nil.
    NSView *mouseDownView = m_mouseDownView;
    if (!mouseDownView) {
        return nil;
    }
    FrameView* topFrameView = m_frame.view();
    NSView *topView = topFrameView ? topFrameView->platformWidget() : nil;
    if (!topView || !findViewInSubviews(topView, mouseDownView)) {
        m_mouseDownView = nil;
        return nil;
    }
    return mouseDownView;
}

#if ENABLE(DRAG_SUPPORT)
bool EventHandler::eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&)
{
    NSView *view = mouseDownViewIfStillGood();
    
    if (!view)
        return false;
    
    if (!m_mouseDownWasInSubframe) {
        ASSERT(!m_sendingEventToSubview);
        m_sendingEventToSubview = true;
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [view mouseDragged:currentNSEvent()];
        END_BLOCK_OBJC_EXCEPTIONS
        m_sendingEventToSubview = false;
    }
    
    return true;
}
#endif // ENABLE(DRAG_SUPPORT)

bool EventHandler::eventLoopHandleMouseUp(const MouseEventWithHitTestResults&)
{
    NSView *view = mouseDownViewIfStillGood();
    if (!view)
        return false;

    if (!m_mouseDownWasInSubframe) {
        ASSERT(!m_sendingEventToSubview);
        m_sendingEventToSubview = true;
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [view mouseUp:currentNSEvent()];
        END_BLOCK_OBJC_EXCEPTIONS
        m_sendingEventToSubview = false;
    }
 
    return true;
}
    
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults& event, Frame& subframe, HitTestResult* hitTestResult)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    switch ([currentNSEvent() type]) {
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeRightMouseDragged:
        // This check is bogus and results in <rdar://6813830>, but removing it breaks a number of
        // layout tests.
        if (!m_mouseDownWasInSubframe)
            return false;
#if ENABLE(DRAG_SUPPORT)
        if (subframe.page()->dragController().didInitiateDrag())
            return false;
#endif
    case NSEventTypeMouseMoved:
        // Since we're passing in currentNSEvent() here, we can call
        // handleMouseMoveEvent() directly, since the save/restore of
        // currentNSEvent() that mouseMoved() does would have no effect.
        ASSERT(!m_sendingEventToSubview);
        m_sendingEventToSubview = true;
        subframe.eventHandler().handleMouseMoveEvent(currentPlatformMouseEvent(), hitTestResult);
        m_sendingEventToSubview = false;
        return true;
        
    case NSEventTypeLeftMouseDown: {
        Node* node = event.targetNode();
        if (!node)
            return false;
        auto* renderer = node->renderer();
        if (!is<RenderWidget>(renderer))
            return false;
        Widget* widget = downcast<RenderWidget>(*renderer).widget();
        if (!widget || !widget->isFrameView())
            return false;
        if (!passWidgetMouseDownEventToWidget(downcast<RenderWidget>(renderer)))
            return false;
        m_mouseDownWasInSubframe = true;
        return true;
    }
    case NSEventTypeLeftMouseUp: {
        if (!m_mouseDownWasInSubframe)
            return false;
        ASSERT(!m_sendingEventToSubview);
        m_sendingEventToSubview = true;
        subframe.eventHandler().handleMouseReleaseEvent(currentPlatformMouseEvent());
        m_sendingEventToSubview = false;
        return true;
    }
    default:
        return false;
    }
    END_BLOCK_OBJC_EXCEPTIONS

    return false;
}

static IMP originalNSScrollViewScrollWheel;
static bool _nsScrollViewScrollWheelShouldRetainSelf;
static void selfRetainingNSScrollViewScrollWheel(NSScrollView *, SEL, NSEvent *);

static bool nsScrollViewScrollWheelShouldRetainSelf()
{
    ASSERT(isMainThread());

    return _nsScrollViewScrollWheelShouldRetainSelf;
}

static void setNSScrollViewScrollWheelShouldRetainSelf(bool shouldRetain)
{
    ASSERT(isMainThread());

    if (!originalNSScrollViewScrollWheel) {
        Method method = class_getInstanceMethod(objc_getRequiredClass("NSScrollView"), @selector(scrollWheel:));
        originalNSScrollViewScrollWheel = method_setImplementation(method, reinterpret_cast<IMP>(selfRetainingNSScrollViewScrollWheel));
    }

    _nsScrollViewScrollWheelShouldRetainSelf = shouldRetain;
}

static void selfRetainingNSScrollViewScrollWheel(NSScrollView *self, SEL selector, NSEvent *event)
{
    bool shouldRetainSelf = isMainThread() && nsScrollViewScrollWheelShouldRetainSelf();

    if (shouldRetainSelf)
        CFRetain((__bridge CFTypeRef)self);
    wtfCallIMP<void>(originalNSScrollViewScrollWheel, self, selector, event);
    if (shouldRetainSelf)
        CFRelease((__bridge CFTypeRef)self);
}

bool EventHandler::passWheelEventToWidget(const PlatformWheelEvent& wheelEvent, Widget& widget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSView* nodeView = widget.platformWidget();
    if (!nodeView) {
        // WebKit2 code path.
        if (!is<FrameView>(widget))
            return false;

        return downcast<FrameView>(widget).frame().eventHandler().handleWheelEvent(wheelEvent);
    }

    if ([currentNSEvent() type] != NSEventTypeScrollWheel || m_sendingEventToSubview)
        return false;

    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[currentNSEvent() locationInWindow] fromView:nil]];
    if (!view) {
        // We probably hit the border of a RenderWidget
        return false;
    }

    ASSERT(!m_sendingEventToSubview);
    m_sendingEventToSubview = true;
    // Work around <rdar://problem/6806810> which can cause -[NSScrollView scrollWheel:] to
    // crash if the NSScrollView is released during timer or network callback dispatch
    // in the nested tracking runloop that -[NSScrollView scrollWheel:] runs.
    setNSScrollViewScrollWheelShouldRetainSelf(true);
    [view scrollWheel:currentNSEvent()];
    setNSScrollViewScrollWheelShouldRetainSelf(false);
    m_sendingEventToSubview = false;
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

void EventHandler::mouseDown(NSEvent *event, NSEvent *correspondingPressureEvent)
{
    FrameView* v = m_frame.view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    m_mouseDownView = nil;
    
    CurrentEventScope scope(event, correspondingPressureEvent);

    handleMousePressEvent(currentPlatformMouseEvent());

    END_BLOCK_OBJC_EXCEPTIONS
}

void EventHandler::mouseDragged(NSEvent *event, NSEvent *correspondingPressureEvent)
{
    FrameView* v = m_frame.view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    CurrentEventScope scope(event, correspondingPressureEvent);
    handleMouseMoveEvent(currentPlatformMouseEvent());

    END_BLOCK_OBJC_EXCEPTIONS
}

void EventHandler::mouseUp(NSEvent *event, NSEvent *correspondingPressureEvent)
{
    FrameView* v = m_frame.view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    CurrentEventScope scope(event, correspondingPressureEvent);

    // Our behavior here is a little different that Qt. Qt always sends
    // a mouse release event, even for a double click. To correct problems
    // in khtml's DOM click event handling we do not send a release here
    // for a double click. Instead we send that event from FrameView's
    // handleMouseDoubleClickEvent. Note also that the third click of
    // a triple click is treated as a single click, but the fourth is then
    // treated as another double click. Hence the "% 2" below.
    int clickCount = [event clickCount];
    if (clickCount > 0 && clickCount % 2 == 0)
        handleMouseDoubleClickEvent(currentPlatformMouseEvent());
    else
        handleMouseReleaseEvent(currentPlatformMouseEvent());
    
    m_mouseDownView = nil;

    END_BLOCK_OBJC_EXCEPTIONS
}

/*
 A hack for the benefit of AK's PopUpButton, which uses the Carbon menu manager, which thus
 eats all subsequent events after it is starts its modal tracking loop.  After the interaction
 is done, this routine is used to fix things up.  When a mouse down started us tracking in
 the widget, we post a fake mouse up to balance the mouse down we started with. When a 
 key down started us tracking in the widget, we post a fake key up to balance things out.
 In addition, we post a fake mouseMoved to get the cursor in sync with whatever we happen to 
 be over after the tracking is done.
 */
void EventHandler::sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent)
{
    FrameView* view = m_frame.view();
    if (!view)
        return;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    m_sendingEventToSubview = false;
    int eventType = [initiatingEvent type];
    if (eventType == NSEventTypeLeftMouseDown || eventType == NSEventTypeKeyDown) {
        ASSERT([NSApp isRunning]);
        NSEvent *fakeEvent = nil;
        if (eventType == NSEventTypeLeftMouseDown) {
            fakeEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                           location:[initiatingEvent locationInWindow]
                                      modifierFlags:[initiatingEvent modifierFlags]
                                          timestamp:[initiatingEvent timestamp]
                                       windowNumber:[initiatingEvent windowNumber]
                                            context:nullptr
                                        eventNumber:[initiatingEvent eventNumber]
                                         clickCount:[initiatingEvent clickCount]
                                           pressure:[initiatingEvent pressure]];
        
            [NSApp postEvent:fakeEvent atStart:YES];
        } else { // eventType == NSEventTypeKeyDown
            fakeEvent = [NSEvent keyEventWithType:NSEventTypeKeyUp
                                         location:[initiatingEvent locationInWindow]
                                    modifierFlags:[initiatingEvent modifierFlags]
                                        timestamp:[initiatingEvent timestamp]
                                     windowNumber:[initiatingEvent windowNumber]
                                          context:nullptr
                                       characters:[initiatingEvent characters] 
                      charactersIgnoringModifiers:[initiatingEvent charactersIgnoringModifiers] 
                                        isARepeat:[initiatingEvent isARepeat] 
                                          keyCode:[initiatingEvent keyCode]];
            [NSApp postEvent:fakeEvent atStart:YES];
        }

        // FIXME: We should really get the current modifierFlags here, but there's no way to poll
        // them in Cocoa, and because the event stream was stolen by the Carbon menu code we have
        // no up-to-date cache of them anywhere.
        fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                       location:[[view->platformWidget() window]
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                                  convertScreenToBase:[NSEvent mouseLocation]]
        ALLOW_DEPRECATED_DECLARATIONS_END
                                  modifierFlags:[initiatingEvent modifierFlags]
                                      timestamp:[initiatingEvent timestamp]
                                   windowNumber:[initiatingEvent windowNumber]
                                        context:nullptr
                                    eventNumber:0
                                     clickCount:0
                                       pressure:0];
        [NSApp postEvent:fakeEvent atStart:YES];
    }
    
    END_BLOCK_OBJC_EXCEPTIONS
}

void EventHandler::mouseMoved(NSEvent *event, NSEvent* correspondingPressureEvent)
{
    // Reject a mouse moved if the button is down - screws up tracking during autoscroll
    // These happen because WebKit sometimes has to fake up moved events.
    if (!m_frame.view() || m_mousePressed || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    CurrentEventScope scope(event, correspondingPressureEvent);
    mouseMoved(currentPlatformMouseEvent());
    END_BLOCK_OBJC_EXCEPTIONS
}

void EventHandler::pressureChange(NSEvent *event, NSEvent* correspondingPressureEvent)
{
    if (!m_frame.view() || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    CurrentEventScope scope(event, correspondingPressureEvent);
    handleMouseForceEvent(currentPlatformMouseEvent());
    END_BLOCK_OBJC_EXCEPTIONS
}

void EventHandler::passMouseMovedEventToScrollbars(NSEvent *event, NSEvent* correspondingPressureEvent)
{
    // Reject a mouse moved if the button is down - screws up tracking during autoscroll
    // These happen because WebKit sometimes has to fake up moved events.
    if (!m_frame.view() || m_mousePressed || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    CurrentEventScope scope(event, correspondingPressureEvent);
    passMouseMovedEventToScrollbars(currentPlatformMouseEvent());
    END_BLOCK_OBJC_EXCEPTIONS
}

static bool frameHasPlatformWidget(const Frame& frame)
{
    if (FrameView* frameView = frame.view()) {
        if (frameView->platformWidget())
            return true;
    }

    return false;
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mouseEventAndResult, Frame& subframe)
{
    // WebKit1 code path.
    if (frameHasPlatformWidget(m_frame))
        return passSubframeEventToSubframe(mouseEventAndResult, subframe);

    // WebKit2 code path.
    subframe.eventHandler().handleMousePressEvent(mouseEventAndResult.event());
    return true;
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mouseEventAndResult, Frame& subframe, HitTestResult* hitTestResult)
{
    // WebKit1 code path.
    if (frameHasPlatformWidget(m_frame))
        return passSubframeEventToSubframe(mouseEventAndResult, subframe, hitTestResult);

#if ENABLE(DRAG_SUPPORT)
    // WebKit2 code path.
    if (m_mouseDownMayStartDrag && !m_mouseDownWasInSubframe)
        return false;
#endif

    subframe.eventHandler().handleMouseMoveEvent(mouseEventAndResult.event(), hitTestResult);
    return true;
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mouseEventAndResult, Frame& subframe)
{
    // WebKit1 code path.
    if (frameHasPlatformWidget(m_frame))
        return passSubframeEventToSubframe(mouseEventAndResult, subframe);

    // WebKit2 code path.
    subframe.eventHandler().handleMouseReleaseEvent(mouseEventAndResult.event());
    return true;
}

PlatformMouseEvent EventHandler::currentPlatformMouseEvent() const
{
    NSView *windowView = nil;
    if (Page* page = m_frame.page())
        windowView = page->chrome().platformPageClient();
    return PlatformEventFactory::createPlatformMouseEvent(currentNSEvent(), correspondingPressureEvent(), windowView);
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent& event) const
{
    return m_activationEventNumber == event.eventNumber();
}

bool EventHandler::tabsToAllFormControls(KeyboardEvent* event) const
{
    Page* page = m_frame.page();
    if (!page)
        return false;

    KeyboardUIMode keyboardUIMode = page->chrome().client().keyboardUIMode();
    bool handlingOptionTab = event && isKeyboardOptionTab(*event);

    // If tab-to-links is off, option-tab always highlights all controls
    if ((keyboardUIMode & KeyboardAccessTabsToLinks) == 0 && handlingOptionTab)
        return true;
    
    // If system preferences say to include all controls, we always include all controls
    if (keyboardUIMode & KeyboardAccessFull)
        return true;
    
    // Otherwise tab-to-links includes all controls, unless the sense is flipped via option-tab.
    if (keyboardUIMode & KeyboardAccessTabsToLinks)
        return !handlingOptionTab;
    
    return handlingOptionTab;
}

bool EventHandler::needsKeyboardEventDisambiguationQuirks() const
{
    if (m_frame.settings().needsKeyboardEventDisambiguationQuirks())
        return true;

    return false;
}

OptionSet<PlatformEvent::Modifier> EventHandler::accessKeyModifiers()
{
    // Control+Option key combinations are usually unused on Mac OS X, but not when VoiceOver is enabled.
    // So, we use Control in this case, even though it conflicts with Emacs-style key bindings.
    // See <https://bugs.webkit.org/show_bug.cgi?id=21107> for more detail.
    if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
        return PlatformEvent::Modifier::ControlKey;

    return { PlatformEvent::Modifier::ControlKey, PlatformEvent::Modifier::AltKey };
}

static ScrollableArea* scrollableAreaForBox(RenderBox& box)
{
    if (is<RenderListBox>(box))
        return downcast<RenderListBox>(&box);

    return box.layer();
}

// FIXME: This could be written in terms of ScrollableArea::enclosingScrollableArea().
static ContainerNode* findEnclosingScrollableContainer(ContainerNode* node, const PlatformWheelEvent& wheelEvent)
{
    auto biasedDelta = ScrollController::wheelDeltaBiasingTowardsVertical(wheelEvent);

    // Find the first node with a valid scrollable area starting with the current
    // node and traversing its parents (or shadow hosts).
    for (ContainerNode* candidate = node; candidate; candidate = candidate->parentOrShadowHostNode()) {
        if (is<HTMLIFrameElement>(*candidate))
            continue;

        if (is<HTMLHtmlElement>(*candidate) || is<HTMLDocument>(*candidate))
            return nullptr;

        RenderBox* box = candidate->renderBox();
        if (!box || !box->canBeScrolledAndHasScrollableArea())
            continue;
        
        auto* scrollableArea = scrollableAreaForBox(*box);
        if (!scrollableArea)
            continue;

        if (wheelEvent.phase() == PlatformWheelEventPhaseMayBegin || wheelEvent.phase() == PlatformWheelEventPhaseCancelled)
            return candidate;

        if ((biasedDelta.height() > 0 && !scrollableArea->scrolledToTop()) || (biasedDelta.height() < 0 && !scrollableArea->scrolledToBottom())
            || (biasedDelta.width() > 0 && !scrollableArea->scrolledToLeft()) || (biasedDelta.width() < 0 && !scrollableArea->scrolledToRight()))
            return candidate;
    }
    
    return nullptr;
}

static WeakPtr<ScrollableArea> scrollableAreaForEventTarget(Element* eventTarget)
{
    auto* widget = EventHandler::widgetForEventTarget(eventTarget);
    if (!widget || !widget->isScrollView())
        return { };

    return makeWeakPtr(static_cast<ScrollableArea&>(static_cast<ScrollView&>(*widget)));
}
    
static bool eventTargetIsPlatformWidget(Element* eventTarget)
{
    Widget* widget = EventHandler::widgetForEventTarget(eventTarget);
    if (!widget)
        return false;
    
    return widget->platformWidget();
}

static WeakPtr<ScrollableArea> scrollableAreaForContainerNode(ContainerNode& container)
{
    auto box = container.renderBox();
    if (!box)
        return { };

    auto scrollableAreaPtr = scrollableAreaForBox(*box);
    if (!scrollableAreaPtr)
        return { };
    
    return makeWeakPtr(*scrollableAreaPtr);
}

void EventHandler::determineWheelEventTarget(const PlatformWheelEvent& wheelEvent, RefPtr<Element>& wheelEventTarget, WeakPtr<ScrollableArea>& scrollableArea, bool& isOverWidget)
{
    auto* page = m_frame.page();
    if (!page)
        return;

    auto* view = m_frame.view();
    if (!view)
        return;

    if (eventTargetIsPlatformWidget(wheelEventTarget.get()))
        scrollableArea = scrollableAreaForEventTarget(wheelEventTarget.get());
    else {
        auto* scrollableContainer = findEnclosingScrollableContainer(wheelEventTarget.get(), wheelEvent);
        if (scrollableContainer)
            scrollableArea = scrollableAreaForContainerNode(*scrollableContainer);
        else
            scrollableArea = makeWeakPtr(static_cast<ScrollableArea&>(*view));
    }

    LOG_WITH_STREAM(ScrollLatching, stream << "EventHandler::determineWheelEventTarget() - event" << wheelEvent << " found scrollableArea " << ValueOrNull(scrollableArea.get()) << ", latching state is " << page->scrollLatchingController());

    if (scrollableArea && page->isMonitoringWheelEvents())
        scrollableArea->scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());

    if (wheelEvent.shouldResetLatching() || wheelEvent.isNonGestureEvent())
        return;

    if (m_frame.isMainFrame() && wheelEvent.isGestureStart())
        page->wheelEventDeltaFilter()->beginFilteringDeltas();

    page->scrollLatchingController().updateAndFetchLatchingStateForFrame(m_frame, wheelEvent, wheelEventTarget, scrollableArea, isOverWidget);
}

void EventHandler::recordWheelEventForDeltaFilter(const PlatformWheelEvent& wheelEvent)
{
    auto* page = m_frame.page();
    if (!page)
        return;

    switch (wheelEvent.phase()) {
        case PlatformWheelEventPhaseBegan:
            page->wheelEventDeltaFilter()->beginFilteringDeltas();
            break;
        case PlatformWheelEventPhaseEnded:
            page->wheelEventDeltaFilter()->endFilteringDeltas();
            break;
        default:
            break;
    }
    page->wheelEventDeltaFilter()->updateFromDelta(FloatSize(wheelEvent.deltaX(), wheelEvent.deltaY()));
}

bool EventHandler::processWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, const WeakPtr<ScrollableArea>& scrollableArea)
{
    LOG_WITH_STREAM(ScrollLatching, stream << "EventHandler::processWheelEventForScrolling " << wheelEvent << " - scrollableArea " << ValueOrNull(scrollableArea.get()) << " use latched element " << wheelEvent.useLatchedEventElement());

#if ASSERT_ENABLED
    {
        // FIXME: Clean up processWheelEventForScrollSnap() and then turn this into an early return.
        WeakPtr<ScrollableArea> latchedScrollableArea;
        ASSERT(m_frame.page()->scrollLatchingController().latchingAllowsScrollingInFrame(m_frame, latchedScrollableArea));
    }
#endif

    Ref<Frame> protectedFrame(m_frame);

    if (!m_frame.page())
        return false;

    FrameView* view = m_frame.view();
    // We do another check on the frame view because the event handler can run JS which results in the frame getting destroyed.
    if (!view)
        return false;

    // We handle non-view scrollableAreas elsewhere.
    if (wheelEvent.useLatchedEventElement() && scrollableArea) {
        m_isHandlingWheelEvent = false;

        LOG_WITH_STREAM(ScrollLatching, stream << "  latching state " << m_frame.page()->scrollLatchingController());

        if (!frameHasPlatformWidget(m_frame) && scrollableArea != view) {
            LOG_WITH_STREAM(Scrolling, stream << "  latched to non-view scroller " << scrollableArea << " and not propagating");
            return true;
        }

        LOG_WITH_STREAM(ScrollLatching, stream << " sending to view " << *view);

        bool didHandleWheelEvent = view->wheelEvent(wheelEvent);
        // If the platform widget is handling the event, we always want to return false.
        if (view->platformWidget())
            didHandleWheelEvent = false;

        LOG_WITH_STREAM(ScrollLatching, stream << "  EventHandler::processWheelEventForScrolling returning " << didHandleWheelEvent);
        return didHandleWheelEvent;
    }
    
    bool didHandleEvent = view->wheelEvent(wheelEvent);
    m_isHandlingWheelEvent = false;
    return didHandleEvent;
}

bool EventHandler::platformCompletePlatformWidgetWheelEvent(const PlatformWheelEvent& wheelEvent, const Widget& widget, const WeakPtr<ScrollableArea>& scrollableArea)
{
    // WebKit1: Prevent multiple copies of the scrollWheel event from being sent to the NSScrollView widget.
    if (frameHasPlatformWidget(m_frame) && widget.isFrameView())
        return true;

    if (!m_frame.page())
        return false;

    WeakPtr<ScrollableArea> latchedScrollableArea;
    if (!m_frame.page()->scrollLatchingController().latchingAllowsScrollingInFrame(m_frame, latchedScrollableArea))
        return false;

    return wheelEvent.useLatchedEventElement() && latchedScrollableArea && scrollableArea == latchedScrollableArea;
}

void EventHandler::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent, const WeakPtr<ScrollableArea>& scrollableArea)
{
    if (!scrollableArea)
        return;

    // Special case handling for ending wheel gesture to activate snap animation:
    if (wheelEvent.phase() != PlatformWheelEventPhaseEnded && wheelEvent.momentumPhase() != PlatformWheelEventPhaseEnded)
        return;

#if ENABLE(CSS_SCROLL_SNAP)
    if (auto* scrollAnimator = scrollableArea->existingScrollAnimator())
        scrollAnimator->processWheelEventForScrollSnap(wheelEvent);
#endif
}

VisibleSelection EventHandler::selectClosestWordFromHitTestResultBasedOnLookup(const HitTestResult& result)
{
    if (!m_frame.editor().behavior().shouldSelectBasedOnDictionaryLookup())
        return { };

    auto range = DictionaryLookup::rangeAtHitTestResult(result);
    if (!range)
        return { };

    return std::get<SimpleRange>(*range);
}

static IntSize autoscrollAdjustmentFactorForScreenBoundaries(const IntPoint& screenPoint, const FloatRect& screenRect)
{
    // If the window is at the edge of the screen, and the mouse position is also at that edge of the screen,
    // we need to adjust the autoscroll amount in order for the user to be able to autoscroll in that direction.
    // We can pretend that the mouse position is slightly beyond the edge of the screen, and then autoscrolling
    // will occur as excpected. This function figures out just how much to adjust the autoscroll amount by
    // in order to get autoscrolling to feel natural in this situation.

    constexpr float edgeDistanceThreshold = 50;
    constexpr float pixelsMultiplier = 20;

    IntSize adjustmentFactor;
    
    float screenLeftEdge = screenRect.x();
    float insetScreenLeftEdge = screenLeftEdge + edgeDistanceThreshold;
    float screenRightEdge = screenRect.maxX();
    float insetScreenRightEdge = screenRightEdge - edgeDistanceThreshold;
    if (screenPoint.x() >= screenLeftEdge && screenPoint.x() < insetScreenLeftEdge) {
        float distanceFromEdge = screenPoint.x() - screenLeftEdge - edgeDistanceThreshold;
        if (distanceFromEdge < 0)
            adjustmentFactor.setWidth((distanceFromEdge / edgeDistanceThreshold) * pixelsMultiplier);
    } else if (screenPoint.x() >= insetScreenRightEdge && screenPoint.x() < screenRightEdge) {
        float distanceFromEdge = edgeDistanceThreshold - (screenRightEdge - screenPoint.x());
        if (distanceFromEdge > 0)
            adjustmentFactor.setWidth((distanceFromEdge / edgeDistanceThreshold) * pixelsMultiplier);
    }

    float screenTopEdge = screenRect.y();
    float insetScreenTopEdge = screenTopEdge + edgeDistanceThreshold;
    float screenBottomEdge = screenRect.maxY();
    float insetScreenBottomEdge = screenBottomEdge - edgeDistanceThreshold;

    if (screenPoint.y() >= screenTopEdge && screenPoint.y() < insetScreenTopEdge) {
        float distanceFromEdge = screenPoint.y() - screenTopEdge - edgeDistanceThreshold;
        if (distanceFromEdge < 0)
            adjustmentFactor.setHeight((distanceFromEdge / edgeDistanceThreshold) * pixelsMultiplier);
    } else if (screenPoint.y() >= insetScreenBottomEdge && screenPoint.y() < screenBottomEdge) {
        float distanceFromEdge = edgeDistanceThreshold - (screenBottomEdge - screenPoint.y());
        if (distanceFromEdge > 0)
            adjustmentFactor.setHeight((distanceFromEdge / edgeDistanceThreshold) * pixelsMultiplier);
    }

    return adjustmentFactor;
}

IntPoint EventHandler::targetPositionInWindowForSelectionAutoscroll() const
{
    Page* page = m_frame.page();
    if (!page)
        return m_lastKnownMousePosition;

    auto frame = toUserSpaceForPrimaryScreen(screenRectForDisplay(page->chrome().displayID()));
    return m_lastKnownMousePosition + autoscrollAdjustmentFactorForScreenBoundaries(m_lastKnownMouseGlobalPosition, frame);
}

}

#endif // PLATFORM(MAC)
