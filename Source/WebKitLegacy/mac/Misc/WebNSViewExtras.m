/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import <WebKitLegacy/WebNSViewExtras.h>

#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/WebDataSource.h>
#import <WebKitLegacy/WebFramePrivate.h>
#import <WebKitLegacy/WebFrameViewInternal.h>
#import <WebKitLegacy/WebNSImageExtras.h>
#import <WebKitLegacy/WebNSURLExtras.h>
#import <WebKitLegacy/WebView.h>

#if !PLATFORM(IOS_FAMILY)
#import <WebKitLegacy/WebNSPasteboardExtras.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKWindow.h>
#endif

#define WebDragStartHysteresisX                 5.0f
#define WebDragStartHysteresisY                 5.0f
#define WebMaxDragImageSize                     NSMakeSize(400.0f, 400.0f)
#define WebMaxOriginalImageArea                 (1500.0f * 1500.0f)
#define WebDragIconRightInset                   7.0f
#define WebDragIconBottomInset                  3.0f

@implementation NSView (WebExtras)

- (NSView *)_web_superviewOfClass:(Class)class
{
    NSView *view = [self superview];
    while (view  && ![view isKindOfClass:class])
        view = [view superview];
    return view;
}

- (WebFrameView *)_web_parentWebFrameView
{
    return (WebFrameView *)[self _web_superviewOfClass:[WebFrameView class]];
}

#if !PLATFORM(IOS_FAMILY)
// FIXME: Mail is the only client of _webView, remove this method once no versions of Mail need it.
- (WebView *)_webView
{
    return (WebView *)[self _web_superviewOfClass:[WebView class]];
}

/* Determine whether a mouse down should turn into a drag; started as copy of NSTableView code */
- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis
{
    NSEvent *nextEvent, *firstEvent, *dragEvent, *mouseUp;
    BOOL dragIt;

    if ([mouseDownEvent type] != NSEventTypeLeftMouseDown) {
        return NO;
    }

    nextEvent = nil;
    firstEvent = nil;
    dragEvent = nil;
    mouseUp = nil;
    dragIt = NO;

    while ((nextEvent = [[self window] nextEventMatchingMask:(NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged)
                                                   untilDate:expiration
                                                      inMode:NSEventTrackingRunLoopMode
                                                     dequeue:YES]) != nil) {
        if (firstEvent == nil) {
            firstEvent = nextEvent;
        }

        if ([nextEvent type] == NSEventTypeLeftMouseDragged) {
            float deltax = ABS([nextEvent locationInWindow].x - [mouseDownEvent locationInWindow].x);
            float deltay = ABS([nextEvent locationInWindow].y - [mouseDownEvent locationInWindow].y);
            dragEvent = nextEvent;

            if (deltax >= xHysteresis) {
                dragIt = YES;
                break;
            }

            if (deltay >= yHysteresis) {
                dragIt = YES;
                break;
            }
        } else if ([nextEvent type] == NSEventTypeLeftMouseUp) {
            mouseUp = nextEvent;
            break;
        }
    }

    // Since we've been dequeuing the events (If we don't, we'll never see the mouse up...),
    // we need to push some of the events back on.  It makes sense to put the first and last
    // drag events and the mouse up if there was one.
    if (mouseUp != nil) {
        [NSApp postEvent:mouseUp atStart:YES];
    }
    if (dragEvent != nil) {
        [NSApp postEvent:dragEvent atStart:YES];
    }
    if (firstEvent != mouseUp && firstEvent != dragEvent) {
        [NSApp postEvent:firstEvent atStart:YES];
    }

    return dragIt;
}

- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
{
    return [self _web_dragShouldBeginFromMouseDown:mouseDownEvent
                                    withExpiration:expiration
                                       xHysteresis:WebDragStartHysteresisX
                                       yHysteresis:WebDragStartHysteresisY];
}


- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender
{
    if (![NSApp modalWindow] && 
        ![[self window] attachedSheet] &&
        [sender draggingSource] != self &&
        [[sender draggingPasteboard] _web_bestURL]) {

        return NSDragOperationCopy;
    }
    
    return NSDragOperationNone;
}

#endif // !PLATFORM(IOS_FAMILY)

- (BOOL)_web_firstResponderIsSelfOrDescendantView
{
    NSResponder *responder = [[self window] firstResponder];
    return (responder && 
           (responder == self || 
           ([responder isKindOfClass:[NSView class]] && [(NSView *)responder isDescendantOf:self])));
}

@end

#if PLATFORM(IOS_FAMILY)
@implementation NSView (WebDocumentViewExtras)

- (WebFrame *)_frame
{
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    return [webFrameView webFrame];
}

- (WebView *)_webView
{
    // We used to use the view hierarchy exclusively here, but that won't work
    // right when the first viewDidMoveToSuperview call is done, and this will.
    return [[self _frame] webView];
}

@end
#endif
