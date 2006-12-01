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
#include "FoundationExtras.h"
#include "FrameLoader.h"
#include "FrameMac.h"
#include "FrameTree.h"
#include "FrameView.h"
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

// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
const float LinkDragHysteresis = 40.0;
const float ImageDragHysteresis = 5.0;
const float TextDragHysteresis = 3.0;
const float GeneralDragHysteresis = 3.0;
const double TextDragDelay = 0.15;

struct EventHandlerDragState {
    RefPtr<Node> m_dragSrc; // element that may be a drag source, for the current mouse gesture
    bool m_dragSrcIsLink;
    bool m_dragSrcIsImage;
    bool m_dragSrcInSelection;
    bool m_dragSrcMayBeDHTML;
    bool m_dragSrcMayBeUA; // Are DHTML and/or the UserAgent allowed to drag out?
    bool m_dragSrcIsDHTML;
    RefPtr<ClipboardMac> m_dragClipboard; // used on only the source side of dragging
};

static EventHandlerDragState& dragState()
{
    static EventHandlerDragState state;
    return state;
}

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

NSView* EventHandler::nextKeyViewInFrame(Node* n, SelectionDirection direction, bool* focusCallResultedInViewBeingCreated)
{
    Document* doc = m_frame->document();
    if (!doc)
        return nil;

    RefPtr<KeyboardEvent> event = currentKeyboardEvent();

    RefPtr<Node> node = n;
    for (;;) {
        node = direction == SelectingNext
            ? doc->nextFocusedNode(node.get(), event.get())
            : doc->previousFocusedNode(node.get(), event.get());
        if (!node)
            return nil;
        
        RenderObject* renderer = node->renderer();
        
        if (!renderer->isWidget()) {
            static_cast<Element*>(node.get())->focus(); 
            // The call to focus might have triggered event handlers that causes the 
            // current renderer to be destroyed.
            if (!(renderer = node->renderer()))
                continue;
                
            // FIXME: When all input elements are native, we should investigate if this extra check is needed
            if (!renderer->isWidget()) {
                [Mac(m_frame)->bridge() willMakeFirstResponderForNodeFocus];
                return [Mac(m_frame)->bridge() documentView];
            }
            if (focusCallResultedInViewBeingCreated)
                *focusCallResultedInViewBeingCreated = true;
        }

        if (Widget* widget = static_cast<RenderWidget*>(renderer)->widget()) {
            NSView* view;
            if (widget->isFrameView())
                view = static_cast<FrameView*>(widget)->frame()->eventHandler()->nextKeyViewInFrame(0, direction);
            else
                view = widget->getView();
            if (view)
                return view;
        }
    }
}

NSView *EventHandler::nextKeyViewInFrameHierarchy(Node* node, SelectionDirection direction)
{
    bool focusCallResultedInViewBeingCreated = false;
    NSView *next = nextKeyViewInFrame(node, direction, &focusCallResultedInViewBeingCreated);
    if (!next)
        if (Frame* parent = m_frame->tree()->parent())
            next = parent->eventHandler()->nextKeyViewInFrameHierarchy(m_frame->ownerElement(), direction);
    
    // remove focus from currently focused node if we're giving focus to another view
    // unless the other view was created as a result of calling focus in nextKeyViewWithFrame.
    // FIXME: The focusCallResultedInViewBeingCreated calls can be removed when all input element types
    // have been made native.
    if (next && (next != [Mac(m_frame)->bridge() documentView] && !focusCallResultedInViewBeingCreated))
        if (Document* doc = m_frame->document())
            doc->setFocusedNode(0);

    // The common case where a view was created is when an <input> element changed from native 
    // to non-native. When this happens, HTMLGenericFormElement::attach() method will call setFocus()
    // on the widget. For views with a field editor, setFocus() will set the active responder to be the field editor. 
    // In this case, we want to return the field editor as the next key view. Otherwise, the focus will be lost
    // and a blur message will be sent. 
    // FIXME: This code can be removed when all input element types are native.
    if (focusCallResultedInViewBeingCreated) {
        if ([[next window] firstResponder] == [[next window] fieldEditor:NO forObject:next])
            return [[next window] fieldEditor:NO forObject:next];
    }
    
    return next;
}

NSView *EventHandler::nextKeyView(Node* node, SelectionDirection direction)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *next = nextKeyViewInFrameHierarchy(node, direction);
    if (next)
        return next;

    // Look at views from the top level part up, looking for a next key view that we can use.
    next = direction == SelectingNext
        ? [Mac(m_frame)->bridge() nextKeyViewOutsideWebFrameViews]
        : [Mac(m_frame)->bridge() previousKeyViewOutsideWebFrameViews];
    if (next)
        return next;

    END_BLOCK_OBJC_EXCEPTIONS;
    
    // If all else fails, make a loop by starting from 0.
    return nextKeyViewInFrameHierarchy(0, direction);
}

NSView *EventHandler::nextKeyView(Widget* startingWidget, SelectionDirection direction)
{
    WidgetClient* client = startingWidget->client();
    if (!client)
        return nil;
    Element* element = client->element(startingWidget);
    if (!element)
        return nil;
    Frame* frame = element->document()->frame();
    if (!frame)
        return nil;
    return frame->eventHandler()->nextKeyView(element, direction);
}

bool EventHandler::currentEventIsMouseDownInWidget(Widget* candidate)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    switch ([[NSApp currentEvent] type]) {
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            break;
        default:
            return false;
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!candidate)
        return false;
    WidgetClient* client = candidate->client();
    if (!client)
        return false;
    Element* element = client->element(candidate);
    if (!element)
        return false;
    Frame* frame = element->document()->frame();
    if (!frame)
        return false;
    return frame->eventHandler()->nodeUnderMouse() == element;
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
    if ([Mac(m_frame)->bridge() keyboardUIMode] & WebCoreKeyboardAccessTabsToLinks)
        return !isKeyboardOptionTab(event);
    return isKeyboardOptionTab(event);
}

bool EventHandler::tabsToAllControls(KeyboardEvent* event) const
{
    WebCoreKeyboardUIMode keyboardUIMode = [Mac(m_frame)->bridge() keyboardUIMode];
    bool handlingOptionTab = isKeyboardOptionTab(event);

    // If tab-to-links is off, option-tab always highlights all controls
    if ((keyboardUIMode & WebCoreKeyboardAccessTabsToLinks) == 0 && handlingOptionTab)
        return true;
    
    // If system preferences say to include all controls, we always include all controls
    if (keyboardUIMode & WebCoreKeyboardAccessFull)
        return true;
    
    // Otherwise tab-to-links includes all controls, unless the sense is flipped via option-tab.
    if (keyboardUIMode & WebCoreKeyboardAccessTabsToLinks)
        return !handlingOptionTab;
    
    return handlingOptionTab;
}

void EventHandler::freeClipboard()
{
    if (dragState().m_dragClipboard)
        dragState().m_dragClipboard->setAccessPolicy(ClipboardNumb);
}

bool EventHandler::keyEvent(NSEvent *event)
{
    bool result;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT([event type] == NSKeyDown || [event type] == NSKeyUp);

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document* doc = m_frame->document();
    if (!doc)
        return false;
    Node* node = doc->focusedNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return false;
    }

    if ([event type] == NSKeyDown)
        m_frame->loader()->resetMultipleFormSubmissionProtection();

    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(event);

    // We want to send both a down and a press for the initial key event.
    // To get the rest of WebCore to do this, we send a second KeyPress with "is repeat" set to true,
    // which causes it to send a press to the DOM.
    // We should do this a better way.
    if ([event type] == NSKeyDown && ![event isARepeat])
        if (!EventTargetNodeCast(node)->dispatchKeyEvent(PlatformKeyboardEvent(event, true)))
            result = true;

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
    if (view == nil) {
        LOG_ERROR("KHTML says we hit a RenderWidget, but AppKit doesn't agree we hit the corresponding NSView");
        return true;
    }
    
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

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

bool EventHandler::lastEventIsMouseUp() const
{
    // Many AK widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
    // the khtml state with this mouseUp, which khtml never saw.  This method lets us detect
    // that state.

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSEvent *currentEventAfterHandlingMouseDown = [NSApp currentEvent];
    if (currentEvent != currentEventAfterHandlingMouseDown) {
        if ([currentEventAfterHandlingMouseDown type] == NSLeftMouseUp)
            return true;
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
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

bool EventHandler::eventMayStartDrag(NSEvent *event) const
{
    // This is a pre-flight check of whether the event might lead to a drag being started.  Be careful
    // that its logic needs to stay in sync with handleMouseMoveEvent() and the way we setMouseDownMayStartDrag
    // in handleMousePressEvent
    
    if ([event type] != NSLeftMouseDown || [event clickCount] != 1) {
        return false;
    }
    
    BOOL DHTMLFlag, UAFlag;
    [Mac(m_frame)->bridge() allowDHTMLDrag:&DHTMLFlag UADrag:&UAFlag];
    if (!DHTMLFlag && !UAFlag) {
        return false;
    }

    NSPoint loc = [event locationInWindow];
    HitTestRequest request(true, false);
    IntPoint mouseDownPos = m_frame->view()->windowToContents(IntPoint(loc));
    HitTestResult result(mouseDownPos);
    m_frame->renderer()->layer()->hitTest(request, result);
    bool srcIsDHTML;
    return result.innerNode() && result.innerNode()->renderer()->draggableNode(DHTMLFlag, UAFlag, mouseDownPos.x(), mouseDownPos.y(), srcIsDHTML);
}

bool EventHandler::dragHysteresisExceeded(const FloatPoint& floatDragViewportLocation) const
{
    IntPoint dragViewportLocation((int)floatDragViewportLocation.x(), (int)floatDragViewportLocation.y());
    IntPoint dragLocation = m_frame->view()->windowToContents(dragViewportLocation);
    IntSize delta = dragLocation - m_mouseDownPos;
    
    float threshold = GeneralDragHysteresis;
    if (dragState().m_dragSrcIsImage)
        threshold = ImageDragHysteresis;
    else if (dragState().m_dragSrcIsLink)
        threshold = LinkDragHysteresis;
    else if (dragState().m_dragSrcInSelection)
        threshold = TextDragHysteresis;

    return fabsf(delta.width()) >= threshold || fabsf(delta.height()) >= threshold;
}

bool EventHandler::handleDrag(const MouseEventWithHitTestResults& event)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if ([currentEvent type] == NSLeftMouseDragged) {
        NSView *view = mouseDownViewIfStillGood();

        if (view) {
            if (!m_mouseDownWasInSubframe) {
                m_sendingEventToSubview = true;
                [view mouseDragged:currentEvent];
                m_sendingEventToSubview = false;
            }
            return true;
        }

        // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    
        if (m_mouseDownMayStartDrag && !dragState().m_dragSrc) {
            BOOL tempFlag1, tempFlag2;
            [Mac(m_frame)->bridge() allowDHTMLDrag:&tempFlag1 UADrag:&tempFlag2];
            dragState().m_dragSrcMayBeDHTML = tempFlag1;
            dragState().m_dragSrcMayBeUA = tempFlag2;
            if (!dragState().m_dragSrcMayBeDHTML && !dragState().m_dragSrcMayBeUA)
                m_mouseDownMayStartDrag = false;     // no element is draggable
        }
        
        if (m_mouseDownMayStartDrag && !dragState().m_dragSrc) {
            // try to find an element that wants to be dragged
            HitTestRequest request(true, false);
            HitTestResult result(m_mouseDownPos);
            m_frame->renderer()->layer()->hitTest(request, result);
            Node* node = result.innerNode();
            dragState().m_dragSrc = (node && node->renderer())
                ? node->renderer()->draggableNode(dragState().m_dragSrcMayBeDHTML, dragState().m_dragSrcMayBeUA,
                    m_mouseDownPos.x(), m_mouseDownPos.y(), dragState().m_dragSrcIsDHTML)
                : 0;
            if (!dragState().m_dragSrc) {
                m_mouseDownMayStartDrag = false;     // no element is draggable
            } else {
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
        if (m_mouseDownMayStartDrag && dragState().m_dragSrcInSelection && [currentEvent timestamp] - m_mouseDownTimestamp < TextDragDelay) {
            m_mouseDownMayStartDrag = false;
            // ...but if this was the first click in the window, we don't even want to start selection
            if (m_activationEventNumber == [currentEvent eventNumber])
                m_mouseDownMayStartSelect = false;
        }

        if (m_mouseDownMayStartDrag) {
            // We are starting a text/image/url drag, so the cursor should be an arrow
            m_frame->view()->setCursor(pointerCursor());
            
            if (dragHysteresisExceeded([currentEvent locationInWindow])) {
                
                // Once we're past the hysteresis point, we don't want to treat this gesture as a click
                invalidateClick();

                NSImage *dragImage = nil;       // we use these values if WC is out of the loop
                NSPoint dragLoc = NSZeroPoint;
                NSDragOperation srcOp = NSDragOperationNone;                
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
                            dragImage = dragState().m_dragClipboard->dragNSImage(dragLoc);
                        
                        // Yuck, dragSourceMovedTo() can be called as a result of kicking off the drag with
                        // dragImage!  Because of that dumb reentrancy, we may think we've not started the
                        // drag when that happens.  So we have to assume it's started before we kick it off.
                        dragState().m_dragClipboard->setDragHasStarted();
                    }
                }
                
                if (m_mouseDownMayStartDrag) {
                    BOOL startedDrag = [Mac(m_frame)->bridge() startDraggingImage:dragImage at:dragLoc operation:srcOp event:currentEvent sourceIsDHTML:dragState().m_dragSrcIsDHTML DHTMLWroteData:wcWrotePasteboard];
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
            }

            // No more default handling (like selection), whether we're past the hysteresis bounds or not
            return true;
        }

        if (!mouseDownMayStartSelect() && !m_mouseDownMayStartAutoscroll)
            return true;
            
    } else {
        // If we allowed the other side of the bridge to handle a drag
        // last time, then m_bMousePressed might still be set. So we
        // clear it now to make sure the next move after a drag
        // doesn't look like a drag.
        m_bMousePressed = false;
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults& event)
{
    NSView *view = mouseDownViewIfStillGood();
    if (view) {
        if (!m_mouseDownWasInSubframe) {
            m_sendingEventToSubview = true;
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            [view mouseUp:currentEvent];
            END_BLOCK_OBJC_EXCEPTIONS;
            m_sendingEventToSubview = false;
        }
        return true;
    }

    // If this was the first click in the window, we don't even want to clear the selection.
    // This case occurs when the user clicks on a draggable element, since we have to process
    // the mouse down and drag events to see if we might start a drag.  For other first clicks
    // in a window, we just don't acceptFirstMouse, and the whole down-drag-up sequence gets
    // ignored upstream of this layer.
    if (m_activationEventNumber == [currentEvent eventNumber])
        return true;

    return false;
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

    ASSERT(view);
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

    m_frame->loader()->resetMultipleFormSubmissionProtection();

    m_mouseDownView = nil;
    dragState().m_dragSrc = 0;
    
    NSEvent *oldCurrentEvent = currentEvent;
    currentEvent = HardRetain(event);
    m_mouseDown = PlatformMouseEvent(event);
    NSPoint loc = [event locationInWindow];
    m_mouseDownPos = m_frame->view()->windowToContents(IntPoint(loc));
    m_mouseDownTimestamp = [event timestamp];

    m_mouseDownMayStartDrag = false;
    m_mouseDownMayStartSelect = false;
    m_mouseDownMayStartAutoscroll = false;
    
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
    if (!m_frame->view() || m_bMousePressed || m_sendingEventToSubview)
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

// Called as we walk up the element chain for nodes with CSS property -webkit-user-drag == auto
bool EventHandler::shouldDragAutoNode(Node* node, const IntPoint& point) const
{
    // We assume that WebKit only cares about dragging things that can be leaf nodes (text, images, urls).
    // This saves a bunch of expensive calls (creating WC and WK element dicts) as we walk farther up
    // the node hierarchy, and we also don't have to cook up a way to ask WK about non-leaf nodes
    // (since right now WK just hit-tests using a cached lastMouseDown).
    if (node->hasChildNodes() || !m_frame->view())
        return false;
    return [Mac(m_frame)->bridge() mayStartDragAtEventLocation:m_frame->view()->contentsToWindow(point)];
}

WebCoreKeyboardUIMode EventHandler::keyboardUIMode() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [Mac(m_frame)->bridge() keyboardUIMode];
    END_BLOCK_OBJC_EXCEPTIONS;

    return WebCoreKeyboardAccessDefault;
}

void EventHandler::dragSourceMovedTo(const PlatformMouseEvent& event)
{
    if (dragState().m_dragSrc && dragState().m_dragSrcMayBeDHTML)
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(dragEvent, event);
}

void EventHandler::dragSourceEndedAt(const PlatformMouseEvent& event, NSDragOperation operation)
{
    if (dragState().m_dragSrc && dragState().m_dragSrcMayBeDHTML) {
        dragState().m_dragClipboard->setDestinationOperation(operation);
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(dragendEvent, event);
    }
    freeClipboard();
    dragState().m_dragSrc = 0;
}

// returns if we should continue "default processing", i.e., whether eventhandler canceled
bool EventHandler::dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent& event)
{
    bool noDefaultProc = dispatchDragEvent(eventType, dragState().m_dragSrc.get(), event, dragState().m_dragClipboard.get());
    return !noDefaultProc;
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
    return passWheelEventToWidget(scrollbar);
}

}
