/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "EventSendingController.h"

#import "DumpRenderTree.h"
#import "DumpRenderTreeDraggingInfo.h"

#import <Carbon/Carbon.h>                           // for GetCurrentEventTime()
#import <WebKit/WebKit.h>

NSPoint lastMousePosition;

@implementation EventSendingController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(mouseDown)
            || aSelector == @selector(mouseUp)
            || aSelector == @selector(mouseClick)
            || aSelector == @selector(mouseMoveToX:Y:)
            || aSelector == @selector(leapForward:)
            || aSelector == @selector(keyDown:withModifiers:))
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(mouseMoveToX:Y:))
        return @"mouseMoveTo";
    if (aSelector == @selector(leapForward:))
        return @"leapForward";
    if (aSelector == @selector(keyDown:withModifiers:))
        return @"keyDown";
    return nil;
}

- (id)init
{
    lastMousePosition = NSMakePoint(0, 0);
    down = NO;
    clickCount = 0;
    lastClick = 0;
    return self;
}

- (double)currentEventTime
{
    return GetCurrentEventTime() + timeOffset;
}

- (void)leapForward:(int)milliseconds
{
    timeOffset += milliseconds / 1000.0;
}

- (void)mouseDown
{
    [[[frame frameView] documentView] layout];
    if ([self currentEventTime] - lastClick >= 1)
        clickCount = 1;
    else
        clickCount++;
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseDown 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        [subView mouseDown:event];
        down = YES;
    }
}

- (void)mouseUp
{
    [[[frame frameView] documentView] layout];
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseUp 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        [subView mouseUp:event];
        down = NO;
        lastClick = [event timestamp];
        if (draggingInfo) {
            WebView *webView = [frame webView];
            
            NSDragOperation dragOperation = [webView draggingUpdated:draggingInfo];
            
            [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] endedAt:lastMousePosition operation:dragOperation];
            if (dragOperation != NSDragOperationNone)
                [webView performDragOperation:draggingInfo];
            [draggingInfo release];
            draggingInfo = nil;
        }
    }
}

- (void)mouseMoveToX:(int)x Y:(int)y
{
    lastMousePosition = NSMakePoint(x, [[frame webView] frame].size.height - y);
    NSEvent *event = [NSEvent mouseEventWithType:(down ? NSLeftMouseDragged : NSMouseMoved) 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:(down ? clickCount : 0) 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        if (down) {
            [subView mouseDragged:event];
            [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] movedTo:lastMousePosition];
            [[frame webView] draggingUpdated:draggingInfo];
        }
        else
            [subView mouseMoved:event];
    }
}

- (void)mouseClick
{
    [[[frame frameView] documentView] layout];
    if ([self currentEventTime] - lastClick >= 1)
        clickCount = 1;
    else
        clickCount++;
    NSEvent *mouseDownEvent = [NSEvent mouseEventWithType:NSLeftMouseDown 
                                        location:lastMousePosition 
                                   modifierFlags:nil 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:nil];

    NSView *subView = [[frame webView] hitTest:[mouseDownEvent locationInWindow]];
    if (subView) {
        [self leapForward:1];
        NSEvent *mouseUpEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                                   location:lastMousePosition
                                              modifierFlags:nil
                                                  timestamp:[self currentEventTime]
                                               windowNumber:[[[frame webView] window] windowNumber]
                                                    context:[NSGraphicsContext currentContext]
                                                eventNumber:++eventNumber
                                                 clickCount:clickCount
                                                   pressure:nil];
        [NSApp postEvent:mouseUpEvent atStart:NO];
        [subView mouseDown:mouseDownEvent];
        lastClick = [mouseUpEvent timestamp];
    }
}

- (void)keyDown:(NSString *)character withModifiers:(WebScriptObject *)modifiers
{
    NSString *modifier = nil;
    int mask = 0;
    
    for (unsigned i = 0; modifiers && [modifiers webScriptValueAtIndex:i]; i++) {
        modifier = (NSString *)[modifiers webScriptValueAtIndex:i];
        if ([modifier isEqual:@"ctrlKey"])
            mask |= NSControlKeyMask;
        else if ([modifier isEqual:@"shiftKey"])
            mask |= NSShiftKeyMask;
        else if ([modifier isEqual:@"altKey"])
            mask |= NSAlternateKeyMask;
        else if ([modifier isEqual:@"metaKey"])
            mask |= NSCommandKeyMask;
        else
            break;
    }

    [[[frame frameView] documentView] layout];
    
    NSEvent *event = [NSEvent keyEventWithType:NSKeyDown
                        location:NSMakePoint(5, 5)
                        modifierFlags:mask
                        timestamp:[self currentEventTime]
                        windowNumber:[[[frame webView] window] windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:character
                        charactersIgnoringModifiers:character
                        isARepeat:NO
                        keyCode:0];
    

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        [subView keyDown:event];
        down = YES;
    }
}

@end
