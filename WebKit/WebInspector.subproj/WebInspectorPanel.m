/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebInspectorPanel.h"

#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebDashboardRegion.h>
#import <WebKit/DOMCore.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/DOMCSS.h>

@implementation WebInspectorPanel
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

- (void)moveWindow:(NSEvent *)event
{
    NSPoint startLocation = [event locationInWindow];
    NSPoint lastLocation = startLocation;
    BOOL mouseUpOccurred = NO;

    while (!mouseUpOccurred) {
        // set mouseUp flag here, but process location of event before exiting from loop, leave mouseUp in queue
        event = [self nextEventMatchingMask:(NSLeftMouseDraggedMask | NSLeftMouseUpMask) untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];

        if ([event type] == NSLeftMouseUp)
            mouseUpOccurred = YES;

        NSPoint newLocation = [event locationInWindow];
        if (NSEqualPoints(newLocation, lastLocation))
            continue;

        NSPoint origin = [self frame].origin;
        [self setFrameOrigin:NSMakePoint(origin.x + newLocation.x - startLocation.x, origin.y + newLocation.y - startLocation.y)];
        lastLocation = newLocation;
    }
}

- (void)resizeWindow:(NSEvent *)event
{
    NSRect startFrame = [self frame];
    NSPoint startLocation = [self convertBaseToScreen:[event locationInWindow]];
    NSPoint lastLocation = startLocation;
    NSSize minSize = [self minSize];
    NSSize maxSize = [self maxSize];
    BOOL mouseUpOccurred = NO;

    while (!mouseUpOccurred) {
        // set mouseUp flag here, but process location of event before exiting from loop, leave mouseUp in queue
        event = [self nextEventMatchingMask:(NSLeftMouseDraggedMask | NSLeftMouseUpMask) untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];

        if ([event type] == NSLeftMouseUp)
            mouseUpOccurred = YES;

        NSPoint newLocation = [self convertBaseToScreen:[event locationInWindow]];
        if (NSEqualPoints(newLocation, lastLocation))
            continue;

        NSRect proposedRect = startFrame;
        proposedRect.size.width += newLocation.x - startLocation.x;;
        proposedRect.size.height -= newLocation.y - startLocation.y;
        proposedRect.origin.y += newLocation.y - startLocation.y;

        if (proposedRect.size.width < minSize.width) {
            proposedRect.size.width = minSize.width;
        } else if (proposedRect.size.width > maxSize.width) {
            proposedRect.size.width = maxSize.width;
        }

        if (proposedRect.size.height < minSize.height) {
            proposedRect.origin.y -= minSize.height - proposedRect.size.height;
            proposedRect.size.height = minSize.height;
        } else if (proposedRect.size.height > maxSize.height) {
            proposedRect.origin.y -= maxSize.height - proposedRect.size.height;
            proposedRect.size.height = maxSize.height;
        }

        [self setFrame:proposedRect display:YES];
        lastLocation = newLocation;
    }
}

- (void)sendEvent:(NSEvent *)event
{
    if (_mouseInRegion && [event type] == NSLeftMouseUp)
        _mouseInRegion = NO;

    if (([event type] == NSLeftMouseDown || [event type] == NSLeftMouseDragged) && !_mouseInRegion) {
        NSPoint pointInView = [[[[(WebView *)[self contentView] mainFrame] frameView] documentView] convertPoint:[event locationInWindow] fromView:nil];
        NSDictionary *regions = [(WebView *)[self contentView] _dashboardRegions];

        WebDashboardRegion *region = [[regions objectForKey:@"resizeTop"] lastObject];
        region = [[regions objectForKey:@"resize"] lastObject];
        if (region) {
            if (NSPointInRect(pointInView, [region dashboardRegionClip])) {
                // we are in a resize control region, resize the window now and eat the event
                [self resizeWindow:event];
                return;
            }
        }

        NSArray *controlRegions = [regions objectForKey:@"control"];
        NSEnumerator *enumerator = [controlRegions objectEnumerator];
        while ((region = [enumerator nextObject])) {
            if (NSPointInRect(pointInView, [region dashboardRegionClip])) {
                // we are in a control region, lets pass the event down
                _mouseInRegion = YES;
                [super sendEvent:event];
                return;
            }
        }

        // if we are dragging and the mouse isn't in a control region move the window
        if ([event type] == NSLeftMouseDragged) {
            [self moveWindow:event];
            return;
        }
    }

    [super sendEvent:event];
}
@end
