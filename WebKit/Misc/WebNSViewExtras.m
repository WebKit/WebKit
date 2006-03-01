/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebNSViewExtras.h>

#import <WebCore/WebCoreImageRenderer.h>
#import <WebCore/DOMPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKitSystemInterface.h>

#define WebDragStartHysteresisX                 5.0
#define WebDragStartHysteresisY                 5.0
#define WebMaxDragImageSize                     NSMakeSize(400, 400)
#define WebMaxOriginalImageArea                 (1500 * 1500)
#define WebDragIconRightInset                   7.0
#define WebDragIconBottomInset                  3.0

@implementation NSView (WebExtras)

- (NSView *)_web_superviewOfClass:(Class)class stoppingAtClass:(Class)limitClass
{
    NSView *view = self;
    while ((view = [view superview]) != nil) {
        if ([view isKindOfClass:class]) {
            return view;
        } else if (limitClass && [view isKindOfClass:limitClass]) {
            break;
        }
    }

    return nil;
}

- (NSView *)_web_superviewOfClass:(Class)class
{
    return [self _web_superviewOfClass:class stoppingAtClass:nil];
}

- (WebFrameView *)_web_parentWebFrameView
{
    WebFrameView *view = (WebFrameView *)[[[self superview] superview] superview];
    if ([view isKindOfClass: [WebFrameView class]])
        return view;
    return nil;
}

- (WebView *)_web_parentWebView
{
    return [[self _web_parentWebFrameView] _webView];
}

/* Determine whether a mouse down should turn into a drag; started as copy of NSTableView code */
- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis
{
    NSEvent *nextEvent, *firstEvent, *dragEvent, *mouseUp;
    BOOL dragIt;

    if ([mouseDownEvent type] != NSLeftMouseDown) {
        return NO;
    }

    nextEvent = nil;
    firstEvent = nil;
    dragEvent = nil;
    mouseUp = nil;
    dragIt = NO;

    while ((nextEvent = [[self window] nextEventMatchingMask:(NSLeftMouseUpMask | NSLeftMouseDraggedMask)
                                                   untilDate:expiration
                                                      inMode:NSEventTrackingRunLoopMode
                                                     dequeue:YES]) != nil) {
        if (firstEvent == nil) {
            firstEvent = nextEvent;
        }

        if ([nextEvent type] == NSLeftMouseDragged) {
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
        } else if ([nextEvent type] == NSLeftMouseUp) {
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
    } else {
        return NSDragOperationNone;
    }
}

- (void)_web_dragImage:(WebImageRenderer *)wir
               element:(DOMElement *)element
                  rect:(NSRect)rect
                 event:(NSEvent *)event
            pasteboard:(NSPasteboard *)pasteboard 
                source:(id)source
                offset:(NSPoint *)dragImageOffset
{
    NSPoint mouseDownPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    NSImage *dragImage;
    NSPoint origin;

    NSImage *image = (wir != nil) ? [wir image] : [element image];
    if (image != nil && [image size].height * [image size].width <= WebMaxOriginalImageArea) {
        NSSize originalSize = rect.size;
        origin = rect.origin;
        
        dragImage = [[image copy] autorelease];
        [dragImage setScalesWhenResized:YES];
        [dragImage setSize:originalSize];
        
        [dragImage _web_scaleToMaxSize:WebMaxDragImageSize];
        NSSize newSize = [dragImage size];
        
        [dragImage _web_dissolveToFraction:WebDragImageAlpha];
        
        // Properly orient the drag image and orient it differently if it's smaller than the original
        origin.x = mouseDownPoint.x - (((mouseDownPoint.x - origin.x) / originalSize.width) * newSize.width);
        origin.y = origin.y + originalSize.height;
        origin.y = mouseDownPoint.y - (((mouseDownPoint.y - origin.y) / originalSize.height) * newSize.height);
    } else {
        NSString *extension = WKGetPreferredExtensionForMIMEType([wir MIMEType]);
        if (extension == nil) {
            extension = @"";
        }
        dragImage = [[NSWorkspace sharedWorkspace] iconForFileType:extension];
        NSSize offset = NSMakeSize([dragImage size].width - WebDragIconRightInset, -WebDragIconBottomInset);
        origin = NSMakePoint(mouseDownPoint.x - offset.width, mouseDownPoint.y - offset.height);
    }

    // This is the offset from the lower left corner of the image to the mouse location.  Because we
    // are a flipped view the calculation of Y is inverted.
    if (dragImageOffset) {
        dragImageOffset->x = mouseDownPoint.x - origin.x;
        dragImageOffset->y = origin.y - mouseDownPoint.y;
    }
    
    // Per kwebster, offset arg is ignored
    [self dragImage:dragImage at:origin offset:NSZeroSize event:event pasteboard:pasteboard source:source slideBack:YES];
}

- (BOOL)_web_firstResponderIsSelfOrDescendantView
{
    NSResponder *responder = [[self window] firstResponder];
    return (responder && 
           (responder == self || 
           ([responder isKindOfClass:[NSView class]] && [(NSView *)responder isDescendantOf:self])));
}

- (BOOL)_web_firstResponderCausesFocusDisplay
{
    return [self _web_firstResponderIsSelfOrDescendantView] || [[self window] firstResponder] == [self _web_parentWebFrameView];
}

@end

@implementation NSView (WebDocumentViewExtras)

- (WebView *)_webView
{
    // We used to use the view hierarchy exclusively here, but that won't work
    // right when the first viewDidMoveToSuperview call is done, and this wil.
    return [[self _frame] webView];
}

- (WebFrame *)_frame
{
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    return [webFrameView webFrame];
}

// Required so view can access the part's selection.
- (WebFrameBridge *)_bridge
{
    return [[self _frame] _bridge];
}

- (WebDataSource *)_dataSource
{
    return [[self _frame] dataSource];
}

- (NSURL *)_webViewURL
{
    return [[[[[self superview] _frame] dataSource] request] URL];
}

@end
