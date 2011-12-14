/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 David Smith (catfish.man@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebViewInternal.h"

#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebTextCompletionController.h"
#import "WebViewData.h"
#import <WebCore/Frame.h>

using namespace WebCore;

@class NSTextInputContext;

@interface NSResponder (WebNSResponderDetails)
- (NSTextInputContext *)inputContext;
@end

#ifndef BUILDING_ON_LEOPARD
@interface NSObject (NSTextInputContextDetails)
- (BOOL)wantsToHandleMouseEvents;
- (BOOL)handleMouseEvent:(NSEvent *)event;
@end
#endif

@implementation WebView (WebViewEventHandling)

static WebView *lastMouseoverView;

- (void)_closingEventHandling
{
    if (lastMouseoverView == self)
        lastMouseoverView = nil;
}

- (void)_setMouseDownEvent:(NSEvent *)event
{
    ASSERT(!event || [event type] == NSLeftMouseDown || [event type] == NSRightMouseDown || [event type] == NSOtherMouseDown);

    if (event == _private->mouseDownEvent)
        return;

    [event retain];
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = event;
}

- (void)mouseDown:(NSEvent *)event
{
    // FIXME (Viewless): This method should be shared with WebHTMLView, which needs to
    // do the same work in the usesDocumentViews case. We don't want to maintain two
    // duplicate copies of this method.

    if (_private->usesDocumentViews) {
        [super mouseDown:event];
        return;
    }
    
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    RetainPtr<WebView> protector = self;
    if ([[self inputContext] wantsToHandleMouseEvents] && [[self inputContext] handleMouseEvent:event])
        return;

    _private->handlingMouseDownEvent = YES;

    // Record the mouse down position so we can determine drag hysteresis.
    [self _setMouseDownEvent:event];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        goto done;

    [_private->completionController endRevertingChange:NO moveLeft:NO];

    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if (!([event modifierFlags] & NSControlKeyMask)) {
        _private->ignoringMouseDraggedEvents = NO;

        // Don't do any mouseover while the mouse is down.
        [self _cancelUpdateMouseoverTimer];

        // Let WebCore get a chance to deal with the event. This will call back to us
        // to start the autoscroll timer if appropriate.
        if (Frame* frame = [self _mainCoreFrame])
            frame->eventHandler()->mouseDown(event);
    }

done:
    _private->handlingMouseDownEvent = NO;
}

- (void)mouseUp:(NSEvent *)event
{
    // FIXME (Viewless): This method should be shared with WebHTMLView, which needs to
    // do the same work in the usesDocumentViews case. We don't want to maintain two
    // duplicate copies of this method.

    if (_private->usesDocumentViews) {
        [super mouseUp:event];
        return;
    }

    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    [self _setMouseDownEvent:nil];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    [self _stopAutoscrollTimer];
    if (Frame* frame = [self _mainCoreFrame])
        frame->eventHandler()->mouseUp(event);
    [self _updateMouseoverWithFakeEvent];

    [self release];
}

+ (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    WebView *oldView = lastMouseoverView;

    lastMouseoverView = nil;

    NSView *contentView = [[event window] contentView];
    NSPoint locationForHitTest = [[contentView superview] convertPoint:[event locationInWindow] fromView:nil];
    for (NSView *hitView = [contentView hitTest:locationForHitTest]; hitView; hitView = [hitView superview]) {
        if ([hitView isKindOfClass:[WebView class]]) {
            lastMouseoverView = static_cast<WebView *>(hitView);
            break;
        }
    }

    if (lastMouseoverView && lastMouseoverView->_private->hoverFeedbackSuspended)
        lastMouseoverView = nil;

    if (lastMouseoverView != oldView) {
        if (Frame* oldCoreFrame = [oldView _mainCoreFrame]) {
            NSEvent *oldViewEvent = [NSEvent mouseEventWithType:NSMouseMoved
                location:NSMakePoint(-1, -1)
                modifierFlags:[[NSApp currentEvent] modifierFlags]
                timestamp:[NSDate timeIntervalSinceReferenceDate]
                windowNumber:[[oldView window] windowNumber]
                context:[[NSApp currentEvent] context]
                eventNumber:0 clickCount:0 pressure:0];
            oldCoreFrame->eventHandler()->mouseMoved(oldViewEvent);
        }
    }

    if (!lastMouseoverView)
        return;

    if (Frame* coreFrame = core([lastMouseoverView mainFrame]))
        coreFrame->eventHandler()->mouseMoved(event);
}

- (void)_updateMouseoverWithFakeEvent
{
    [self _cancelUpdateMouseoverTimer];
    
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [[self class] _updateMouseoverWithEvent:fakeEvent];
}

- (void)_cancelUpdateMouseoverTimer
{
    if (_private->updateMouseoverTimer) {
        CFRunLoopTimerInvalidate(_private->updateMouseoverTimer);
        CFRelease(_private->updateMouseoverTimer);
        _private->updateMouseoverTimer = NULL;
    }
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [_private->autoscrollTriggerEvent release];
    _private->autoscrollTriggerEvent = nil;
    [timer invalidate];
    [timer release];
}

- (void)_setToolTip:(NSString *)toolTip
{
    if (_private->usesDocumentViews) {
        id documentView = [[[self _selectedOrMainFrame] frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [documentView _setToolTip:toolTip];
        return;
    }

    // FIXME (Viewless): Code to handle tooltips needs to move into WebView.
}

@end
