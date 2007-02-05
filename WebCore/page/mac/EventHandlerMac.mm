/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "BlockExceptions.h"
#include "ClipboardMac.h"
#include "Cursor.h"
#include "Document.h"
#include "EventNames.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "FoundationExtras.h"
#include "FrameLoader.h"
#include "FrameMac.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLFrameSetElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "KeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformScrollBar.h"
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"
#include "WebCoreFrameBridge.h"

namespace WebCore {

using namespace EventNames;


static NSEvent *currentEvent;

NSEvent *EventHandler::currentNSEvent()
{
    return currentEvent;
}

bool EventHandler::wheelEvent(NSEvent *event)
{
    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);

    PlatformWheelEvent wheelEvent(event);
    handleWheelEvent(wheelEvent);

    ASSERT(currentEvent == event);
    HardRelease(event);
    currentEvent = oldCurrentEvent;

    return wheelEvent.isAccepted();
}

PassRefPtr<KeyboardEvent> EventHandler::currentKeyboardEvent() const
{
    NSEvent *event = [NSApp currentEvent];
    if (!event)
        return 0;
    switch ([event type]) {
        case NSKeyDown:
        case NSKeyUp:
            return new KeyboardEvent(event, m_frame->document() ? m_frame->document()->defaultView() : 0);
        default:
            return 0;
    }
}

static bool isKeyboardOptionTab(KeyboardEvent* event)
{
    return event
        && (event->type() == keydownEvent || event->type() == keypressEvent)
        && event->altKey()
        && event->keyIdentifier() == "U+000009";
}

bool EventHandler::tabsToLinks(KeyboardEvent* event) const
{
    if ([Mac(m_frame)->bridge() keyboardUIMode] & KeyboardAccessTabsToLinks)
        return !isKeyboardOptionTab(event);
    return isKeyboardOptionTab(event);
}

bool EventHandler::tabsToAllControls(KeyboardEvent* event) const
{
    KeyboardUIMode keyboardUIMode = [Mac(m_frame)->bridge() keyboardUIMode];
    bool handlingOptionTab = isKeyboardOptionTab(event);

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

bool EventHandler::keyEvent(NSEvent *event)
{
    bool result;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT([event type] == NSKeyDown || [event type] == NSKeyUp);

    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);

    result = keyEvent(PlatformKeyboardEvent(event));
    
    ASSERT(currentEvent == event);
    HardRelease(event);
    currentEvent = oldCurrentEvent;

    return result;

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void EventHandler::focusDocumentView()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *view = m_frame->view()->getDocumentView();
    if ([Mac(m_frame)->bridge() firstResponder] != view)
        [Mac(m_frame)->bridge() makeFirstResponder:view];
    END_BLOCK_OBJC_EXCEPTIONS;
    if (Page* page = m_frame->page())
        page->focusController()->setFocusedFrame(m_frame);
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults& event)
{
    // Figure out which view to send the event to.
    RenderObject* target = event.targetNode() ? event.targetNode()->renderer() : 0;
    if (!target || !target->isWidget())
        return false;
    
    // Double-click events don't exist in Cocoa. Since passWidgetMouseDownEventToWidget will
    // just pass currentEvent down to the widget, we don't want to call it for events that
    // don't correspond to Cocoa events.  The mousedown/ups will have already been passed on as
    // part of the pressed/released handling.
    return passMouseDownEventToWidget(static_cast<RenderWidget*>(target)->widget());
}

bool EventHandler::passWidgetMouseDownEventToWidget(RenderWidget* renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

static bool lastEventIsMouseUp()
{
    // Many AK widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
    // the khtml state with this mouseUp, which khtml never saw.  This method lets us detect
    // that state.

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSEvent *currentEventAfterHandlingMouseDown = [NSApp currentEvent];
    if (currentEvent != currentEventAfterHandlingMouseDown &&
        [currentEventAfterHandlingMouseDown type] == NSLeftMouseUp &&
        [currentEventAfterHandlingMouseDown timestamp] >= [currentEvent timestamp])
            return true;
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool EventHandler::passMouseDownEventToWidget(Widget* widget)
{
    // FIXME: this method always returns true

    if (!widget) {
        LOG_ERROR("hit a RenderWidget without a corresponding Widget, means a frame is half-constructed");
        return true;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSView *nodeView = widget->getView();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[currentEvent locationInWindow] fromView:nil]];
    if (!view)
        // We probably hit the border of a RenderWidget
        return true;
    
    if ([Mac(m_frame)->bridge() firstResponder] == view) {
        // In the case where we just became first responder, we should send the mouseDown:
        // to the NSTextField, not the NSTextField's editor. This code makes sure that happens.
        // If we don't do this, we see a flash of selected text when clicking in a text field.
        // FIXME: This is the only caller of textViewWasFirstResponderAtMouseDownTime. When we
        // eliminate all use of NSTextField/NSTextView in form fields we can eliminate this code,
        // and textViewWasFirstResponderAtMouseDownTime:, and the instance variable WebHTMLView
        // keeps solely to support textViewWasFirstResponderAtMouseDownTime:.
        if ([view isKindOfClass:[NSTextView class]] && ![Mac(m_frame)->bridge() textViewWasFirstResponderAtMouseDownTime:(NSTextView *)view]) {
            NSView *superview = view;
            while (superview != nodeView) {
                superview = [superview superview];
                ASSERT(superview);
                if ([superview isKindOfClass:[NSControl class]]) {
                    NSControl *control = static_cast<NSControl*>(superview);
                    if ([control currentEditor] == view)
                        view = superview;
                    break;
                }
            }
        }
    } else {
        // Normally [NSWindow sendEvent:] handles setting the first responder.
        // But in our case, the event was sent to the view representing the entire web page.
        if ([currentEvent clickCount] <= 1 && [view acceptsFirstResponder] && [view needsPanelToBecomeKey]) {
            [Mac(m_frame)->bridge() makeFirstResponder:view];
        }
    }

    // We need to "defer loading" while tracking the mouse, because tearing down the
    // page while an AppKit control is tracking the mouse can cause a crash.
    
    // FIXME: In theory, WebCore now tolerates tear-down while tracking the
    // mouse. We should confirm that, and then remove the deferrsLoading
    // hack entirely.
    
    bool wasDeferringLoading = m_frame->page()->defersLoading();
    if (!wasDeferringLoading)
        m_frame->page()->setDefersLoading(true);

    ASSERT(!m_sendingEventToSubview);
    m_sendingEventToSubview = true;
    [view mouseDown:currentEvent];
    m_sendingEventToSubview = false;
    
    if (!wasDeferringLoading)
        m_frame->page()->setDefersLoading(false);

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

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}
    
// Note that this does the same kind of check as [target isDescendantOf:superview].
// There are two differences: This is a lot slower because it has to walk the whole
// tree, and this works in cases where the target has already been deallocated.
static bool findViewInSubviews(NSView *superview, NSView *target)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSEnumerator *e = [[superview subviews] objectEnumerator];
    NSView *subview;
    while ((subview = [e nextObject])) {
        if (subview == target || findViewInSubviews(subview, target)) {
            return true;
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    
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
    FrameView* topFrameView = m_frame->view();
    NSView *topView = topFrameView ? topFrameView->getView() : nil;
    if (!topView || !findViewInSubviews(topView, mouseDownView)) {
        m_mouseDownView = nil;
        return nil;
    }
    return mouseDownView;
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent& event) const
{
    return m_activationEventNumber == event.eventNumber();
}

bool EventHandler::eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&)
{
    NSView *view = mouseDownViewIfStillGood();
    
    if (!view)
        return false;
    
    if (!m_mouseDownWasInSubframe) {
        m_sendingEventToSubview = true;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [view mouseDragged:currentEvent];
        END_BLOCK_OBJC_EXCEPTIONS;
        m_sendingEventToSubview = false;
    }
    
    return true;
}
    
bool EventHandler::handleDrag(const MouseEventWithHitTestResults& event)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
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
        HitTestRequest request(true, false);
        HitTestResult result(m_mouseDownPos);
        m_frame->renderer()->layer()->hitTest(request, result);
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
            
            dragState().m_dragSrcInSelection = m_frame->selectionController()->contains(m_mouseDownPos);
        }                
    }
    
    // For drags starting in the selection, the user must wait between the mousedown and mousedrag,
    // or else we bail on the dragging stuff and allow selection to occur
    if (m_mouseDownMayStartDrag && dragState().m_dragSrcInSelection && event.event().timestamp() - m_mouseDownTimestamp < TextDragDelay) {
        m_mouseDownMayStartDrag = false;
        // ...but if this was the first click in the window, we don't even want to start selection
        if (eventActivatedView(event.event()))
            m_mouseDownMayStartSelect = false;
    }
    
    if (!m_mouseDownMayStartDrag)
        return !mouseDownMayStartSelect() && !m_mouseDownMayStartAutoscroll;
    
    // We are starting a text/image/url drag, so the cursor should be an arrow
    m_frame->view()->setCursor(pointerCursor());
    
    if (!dragHysteresisExceeded(event.event().pos())) 
        return true;
    
    // Once we're past the hysteresis point, we don't want to treat this gesture as a click
    invalidateClick();
    
    NSImage *dragImage = nil;       // we use these values if WC is out of the loop
    NSPoint dragLoc = NSZeroPoint;
    DragOperation srcOp = DragOperationNone;                
    BOOL wcWrotePasteboard = NO;
    if (dragState().m_dragSrcMayBeDHTML) {
        NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
        // Must be done before ondragstart adds types and data to the pboard,
        // also done for security, as it erases data from the last drag
        [pasteboard declareTypes:[NSArray array] owner:nil];
        
        freeClipboard();    // would only happen if we missed a dragEnd.  Do it anyway, just
        // to make sure it gets numbified
        dragState().m_dragClipboard = new ClipboardMac(true, pasteboard, ClipboardWritable, Mac(m_frame));
        
        // If this is drag of an element, get set up to generate a default image.  Otherwise
        // WebKit will generate the default, the element doesn't override.
        if (dragState().m_dragSrcIsDHTML) {
            int srcX, srcY;
            dragState().m_dragSrc->renderer()->absolutePosition(srcX, srcY);
            IntSize delta = m_mouseDownPos - IntPoint(srcX, srcY);
            dragState().m_dragClipboard->setDragImageElement(dragState().m_dragSrc.get(), IntPoint() + delta);
        } 
        
        m_mouseDownMayStartDrag = dispatchDragSrcEvent(dragstartEvent, m_mouseDown)
        && !m_frame->selectionController()->isInPasswordField();
        
        // Invalidate clipboard here against anymore pasteboard writing for security.  The drag
        // image can still be changed as we drag, but not the pasteboard data.
        dragState().m_dragClipboard->setAccessPolicy(ClipboardImageWritable);
        
        if (m_mouseDownMayStartDrag) {
            // gather values from DHTML element, if it set any
            dragState().m_dragClipboard->sourceOperation(srcOp);
            
            NSArray *types = [pasteboard types];
            wcWrotePasteboard = types && [types count] > 0;
            
            if (dragState().m_dragSrcMayBeDHTML)
                dragImage = static_cast<ClipboardMac*>(dragState().m_dragClipboard.get())->dragNSImage(dragLoc);
            
            // Yuck, dragSourceMovedTo() can be called as a result of kicking off the drag with
            // dragImage!  Because of that dumb reentrancy, we may think we've not started the
            // drag when that happens.  So we have to assume it's started before we kick it off.
            dragState().m_dragClipboard->setDragHasStarted();
        }
    }
    
    if (m_mouseDownMayStartDrag) {
        bool startedDrag = [Mac(m_frame)->bridge() startDraggingImage:dragImage at:dragLoc operation:srcOp event:currentEvent sourceIsDHTML:dragState().m_dragSrcIsDHTML DHTMLWroteData:wcWrotePasteboard];
        if (!startedDrag && dragState().m_dragSrcMayBeDHTML) {
            // WebKit canned the drag at the last minute - we owe m_dragSrc a DRAGEND event
            PlatformMouseEvent event(PlatformMouseEvent::currentEvent);
            dispatchDragSrcEvent(dragendEvent, event);
            m_mouseDownMayStartDrag = false;
        }
    } 
    
    if (!m_mouseDownMayStartDrag) {
        // something failed to start the drag, cleanup
        freeClipboard();
        dragState().m_dragSrc = 0;
    }
    
    // No more default handling (like selection), whether we're past the hysteresis bounds or not
    return true;   
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
    
}

    
bool EventHandler::eventLoopHandleMouseUp(const MouseEventWithHitTestResults&)
{
    NSView *view = mouseDownViewIfStillGood();
    if (!view)
        return false;
    
    if (!m_mouseDownWasInSubframe) {
        m_sendingEventToSubview = true;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [view mouseUp:currentEvent];
        END_BLOCK_OBJC_EXCEPTIONS;
        m_sendingEventToSubview = false;
    }
 
    return true;
}
    
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults& event, Frame* subframe)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    switch ([currentEvent type]) {
        case NSMouseMoved:
            subframe->eventHandler()->mouseMoved(currentEvent);
            return true;
        
        case NSLeftMouseDown: {
            Node* node = event.targetNode();
            if (!node)
                return false;
            RenderObject* renderer = node->renderer();
            if (!renderer || !renderer->isWidget())
                return false;
            Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
            if (!widget || !widget->isFrameView())
                return false;
            if (!passWidgetMouseDownEventToWidget(static_cast<RenderWidget*>(renderer)))
                return false;
            m_mouseDownWasInSubframe = true;
            return true;
        }
        case NSLeftMouseUp: {
            if (!m_mouseDownWasInSubframe)
                return false;
            NSView *view = mouseDownViewIfStillGood();
            if (!view)
                return false;
            ASSERT(!m_sendingEventToSubview);
            m_sendingEventToSubview = true;
            [view mouseUp:currentEvent];
            m_sendingEventToSubview = false;
            return true;
        }
        case NSLeftMouseDragged: {
            if (!m_mouseDownWasInSubframe)
                return false;
            NSView *view = mouseDownViewIfStillGood();
            if (!view)
                return false;
            ASSERT(!m_sendingEventToSubview);
            m_sendingEventToSubview = true;
            [view mouseDragged:currentEvent];
            m_sendingEventToSubview = false;
            return true;
        }
        default:
            return false;
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool EventHandler::passWheelEventToWidget(Widget* widget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
        
    if ([currentEvent type] != NSScrollWheel || m_sendingEventToSubview || !widget) 
        return false;

    NSView *nodeView = widget->getView();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[currentEvent locationInWindow] fromView:nil]];
    if (!view)
        // We probably hit the border of a RenderWidget
        return true;

    m_sendingEventToSubview = true;
    [view scrollWheel:currentEvent];
    m_sendingEventToSubview = false;
    return true;
            
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

void EventHandler::mouseDown(NSEvent *event)
{
    FrameView* v = m_frame->view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
    m_frame->loader()->resetMultipleFormSubmissionProtection();
#endif
    m_mouseDownView = nil;
    dragState().m_dragSrc = 0;
    
    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);
    m_mouseDown = PlatformMouseEvent(event);
    
    handleMousePressEvent(event);
    
    ASSERT(currentEvent == event);
    HardRelease(event);
    currentEvent = oldCurrentEvent;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void EventHandler::mouseDragged(NSEvent *event)
{
    FrameView* v = m_frame->view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);

    v->handleMouseMoveEvent(event);
    
    ASSERT(currentEvent == event);
    HardRelease(event);
    currentEvent = oldCurrentEvent;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void EventHandler::mouseUp(NSEvent *event)
{
    FrameView* v = m_frame->view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);

    // Our behavior here is a little different that Qt. Qt always sends
    // a mouse release event, even for a double click. To correct problems
    // in khtml's DOM click event handling we do not send a release here
    // for a double click. Instead we send that event from FrameView's
    // handleMouseDoubleClickEvent. Note also that the third click of
    // a triple click is treated as a single click, but the fourth is then
    // treated as another double click. Hence the "% 2" below.
    int clickCount = [event clickCount];
    if (clickCount > 0 && clickCount % 2 == 0)
        handleMouseDoubleClickEvent(event);
    else
        handleMouseReleaseEvent(event);
    
    ASSERT(currentEvent == event);
    HardRelease(event);
    currentEvent = oldCurrentEvent;
    
    m_mouseDownView = nil;

    END_BLOCK_OBJC_EXCEPTIONS;
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
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    m_sendingEventToSubview = false;
    int eventType = [initiatingEvent type];
    if (eventType == NSLeftMouseDown || eventType == NSKeyDown) {
        NSEvent *fakeEvent = nil;
        if (eventType == NSLeftMouseDown) {
            fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                    location:[initiatingEvent locationInWindow]
                                modifierFlags:[initiatingEvent modifierFlags]
                                    timestamp:[initiatingEvent timestamp]
                                windowNumber:[initiatingEvent windowNumber]
                                        context:[initiatingEvent context]
                                    eventNumber:[initiatingEvent eventNumber]
                                    clickCount:[initiatingEvent clickCount]
                                    pressure:[initiatingEvent pressure]];
        
            mouseUp(fakeEvent);
        } else { // eventType == NSKeyDown
            fakeEvent = [NSEvent keyEventWithType:NSKeyUp
                                    location:[initiatingEvent locationInWindow]
                               modifierFlags:[initiatingEvent modifierFlags]
                                   timestamp:[initiatingEvent timestamp]
                                windowNumber:[initiatingEvent windowNumber]
                                     context:[initiatingEvent context]
                                  characters:[initiatingEvent characters] 
                 charactersIgnoringModifiers:[initiatingEvent charactersIgnoringModifiers] 
                                   isARepeat:[initiatingEvent isARepeat] 
                                     keyCode:[initiatingEvent keyCode]];
            keyEvent(fakeEvent);
        }
        // FIXME:  We should really get the current modifierFlags here, but there's no way to poll
        // them in Cocoa, and because the event stream was stolen by the Carbon menu code we have
        // no up-to-date cache of them anywhere.
        fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
                                       location:[[Mac(m_frame)->bridge() window] convertScreenToBase:[NSEvent mouseLocation]]
                                  modifierFlags:[initiatingEvent modifierFlags]
                                      timestamp:[initiatingEvent timestamp]
                                   windowNumber:[initiatingEvent windowNumber]
                                        context:[initiatingEvent context]
                                    eventNumber:0
                                     clickCount:0
                                       pressure:0];
        mouseMoved(fakeEvent);
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

void EventHandler::mouseMoved(NSEvent *event)
{
    // Reject a mouse moved if the button is down - screws up tracking during autoscroll
    // These happen because WebKit sometimes has to fake up moved events.
    if (!m_frame->view() || m_mousePressed || m_sendingEventToSubview)
        return;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);
    
    m_frame->view()->handleMouseMoveEvent(event);
    
    ASSERT(currentEvent == event);
    HardRelease(event);
    currentEvent = oldCurrentEvent;

    END_BLOCK_OBJC_EXCEPTIONS;
}

KeyboardUIMode EventHandler::keyboardUIMode() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [Mac(m_frame)->bridge() keyboardUIMode];
    END_BLOCK_OBJC_EXCEPTIONS;

    return KeyboardAccessDefault;
}

bool EventHandler::inputManagerHasMarkedText() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[NSInputManager currentInputManager] hasMarkedText];
    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passWheelEventToSubframe(PlatformWheelEvent&, Frame* subframe)
{
    return passWheelEventToWidget(subframe->view());
}

bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults&, PlatformScrollbar* scrollbar)
{
    return passMouseDownEventToWidget(scrollbar);
}

}
