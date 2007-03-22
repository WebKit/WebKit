/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Jonas Witt <jonas.witt@gmail.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
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
#import <WebKit/DOMPrivate.h>

extern void _NSNewKillRingSequence();

NSPoint lastMousePosition;
NSArray *webkitDomEventNames;
NSMutableArray *savedMouseEvents; // mouse events sent between mouseDown and mouseUp are stored here, and then executed at once.
BOOL replayingSavedEvents;

@implementation EventSendingController

+ (void)initialize
{
    webkitDomEventNames = [[NSArray alloc] initWithObjects:
        @"abort",
        @"beforecopy",
        @"beforecut",
        @"beforepaste",
        @"blur",
        @"change",
        @"click",
        @"contextmenu",
        @"copy",
        @"cut",
        @"dblclick",
        @"drag",
        @"dragend",
        @"dragenter",
        @"dragleave",
        @"dragover",
        @"dragstart",
        @"drop",
        @"error",
        @"focus",
        @"input",
        @"keydown",
        @"keypress",
        @"keyup",
        @"load",
        @"mousedown",
        @"mousemove",
        @"mouseout",
        @"mouseover",
        @"mouseup",
        @"mousewheel",
        @"beforeunload",
        @"paste",
        @"readystatechange",
        @"reset",
        @"resize", 
        @"scroll", 
        @"search",
        @"select",
        @"selectstart",
        @"submit", 
        @"textInput", 
        @"unload",
        @"zoom",
        nil];
}

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(mouseDown)
            || aSelector == @selector(mouseUp)
            || aSelector == @selector(mouseClick)
            || aSelector == @selector(contextClick)
            || aSelector == @selector(mouseMoveToX:Y:)
            || aSelector == @selector(leapForward:)
            || aSelector == @selector(keyDown:withModifiers:)
            || aSelector == @selector(enableDOMUIEventLogging:)
            || aSelector == @selector(fireKeyboardEventsToElement:)
            || aSelector == @selector(clearKillRing))
        return NO;
    return YES;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char*)name
{
    if (strcmp(name, "dragMode") == 0)
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
    if (aSelector == @selector(enableDOMUIEventLogging:))
        return @"enableDOMUIEventLogging";
    if (aSelector == @selector(fireKeyboardEventsToElement:))
        return @"fireKeyboardEventsToElement";
    if (aSelector == @selector(setDragMode:))
        return @"setDragMode";
    return nil;
}

- (id)init
{
    self = [super init];
    if (self)
        dragMode = YES;
    return self;
}

- (void)dealloc
{
    [savedMouseEvents release];
    savedMouseEvents = nil;
    [super dealloc];
}

- (double)currentEventTime
{
    return GetCurrentEventTime() + timeOffset;
}

- (void)leapForward:(int)milliseconds
{
    if (dragMode && down && !replayingSavedEvents) {
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[EventSendingController instanceMethodSignatureForSelector:@selector(leapForward:)]];
        [invocation setTarget:self];
        [invocation setSelector:@selector(leapForward:)];
        [invocation setArgument:&milliseconds atIndex:2];
        
        [EventSendingController saveEvent:invocation];
        
        return;
    }

    timeOffset += milliseconds / 1000.0;
}

- (void)clearKillRing
{
    _NSNewKillRingSequence();
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
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        [subView mouseDown:event];
        down = YES;
    }
}

- (void)mouseUp
{
    if (dragMode && !replayingSavedEvents) {
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[EventSendingController instanceMethodSignatureForSelector:@selector(mouseUp)]];
        [invocation setTarget:self];
        [invocation setSelector:@selector(mouseUp)];
        
        [EventSendingController saveEvent:invocation];
        [EventSendingController replaySavedEvents];

        return;
    }

    [[[frame frameView] documentView] layout];
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseUp 
                                        location:lastMousePosition 
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];

    NSView *targetView = [[frame webView] hitTest:[event locationInWindow]];
    // FIXME: Silly hack to teach DRT to respect capturing mouse events outside the WebView.
    // The right solution is just to use NSApplication's built-in event sending methods, 
    // instead of rolling our own algorithm for selecting an event target.
    targetView = targetView ? targetView : [[frame frameView] documentView];
    assert(targetView);
    [targetView mouseUp:event];
    down = NO;
    lastClick = [event timestamp];
    if (draggingInfo) {
        WebView *webView = [frame webView];
        
        NSDragOperation dragOperation = [webView draggingUpdated:draggingInfo];
        
        if (dragOperation != NSDragOperationNone)
            [webView performDragOperation:draggingInfo];
        else
            [webView draggingExited:draggingInfo];
        [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] endedAt:lastMousePosition operation:dragOperation];
        [draggingInfo release];
        draggingInfo = nil;
    }
}

- (void)mouseMoveToX:(int)x Y:(int)y
{
    if (dragMode && down && !replayingSavedEvents) {
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[EventSendingController instanceMethodSignatureForSelector:@selector(mouseMoveToX:Y:)]];
        [invocation setTarget:self];
        [invocation setSelector:@selector(mouseMoveToX:Y:)];
        [invocation setArgument:&x atIndex:2];
        [invocation setArgument:&y atIndex:3];
        
        [EventSendingController saveEvent:invocation];
        
        return;
    }

    NSView *view = [frame webView];
    lastMousePosition = [view convertPoint:NSMakePoint(x, [view frame].size.height - y) toView:nil];
    NSEvent *event = [NSEvent mouseEventWithType:(down ? NSLeftMouseDragged : NSMouseMoved) 
                                        location:lastMousePosition 
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[view window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:(down ? clickCount : 0) 
                                        pressure:0.0];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView) {
        if (down) {
            [subView mouseDragged:event];
            if (draggingInfo) {
                [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] movedTo:lastMousePosition];
                [[frame webView] draggingUpdated:draggingInfo];
            }
        } else
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
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];

    NSView *subView = [[frame webView] hitTest:[mouseDownEvent locationInWindow]];
    if (subView) {
        [self leapForward:1];
        NSEvent *mouseUpEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                                   location:lastMousePosition
                                              modifierFlags:0
                                                  timestamp:[self currentEventTime]
                                               windowNumber:[[[frame webView] window] windowNumber]
                                                    context:[NSGraphicsContext currentContext]
                                                eventNumber:++eventNumber
                                                 clickCount:clickCount
                                                   pressure:0.0];
        [subView mouseDown:mouseDownEvent];
        [subView mouseUp:mouseUpEvent];
        lastClick = [mouseUpEvent timestamp];
    }
}

- (void)contextClick
{
    [[[frame frameView] documentView] layout];
    if ([self currentEventTime] - lastClick >= 1)
        clickCount = 1;
    else
        clickCount++;
    NSEvent *event = [NSEvent mouseEventWithType:NSRightMouseDown 
                                        location:lastMousePosition 
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[frame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];

    NSView *subView = [[frame webView] hitTest:[event locationInWindow]];
    if (subView)
        [subView menuForEvent:event];
}

+ (void)saveEvent:(NSInvocation *)event
{
    if (!savedMouseEvents)
        savedMouseEvents = [[NSMutableArray alloc] init];
    [savedMouseEvents addObject:event];
}

+ (void)replaySavedEvents
{
    replayingSavedEvents = YES;
    while ([savedMouseEvents count]) {
        // if a drag is initiated, the remaining saved events will be dispatched from our dragging delegate
        NSInvocation *invocation = [[[savedMouseEvents objectAtIndex:0] retain] autorelease];
        [savedMouseEvents removeObjectAtIndex:0];
        [invocation invoke];
    }
    replayingSavedEvents = NO;
}

- (void)keyDown:(NSString *)character withModifiers:(WebScriptObject *)modifiers
{
    NSString *modifier = nil;
    int mask = 0;
    
    if ([modifiers isKindOfClass:[WebScriptObject class]])
        for (unsigned i = 0; [[modifiers webScriptValueAtIndex:i] isKindOfClass:[NSString class]]; i++) {
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
    

    NSResponder *firstResponder = [[[frame webView] window] firstResponder];
    [firstResponder keyDown:event];
}

- (void)enableDOMUIEventLogging:(WebScriptObject *)node
{
    NSEnumerator *eventEnumerator = [webkitDomEventNames objectEnumerator];
    id eventName;
    while ((eventName = [eventEnumerator nextObject])) {
        [(id<DOMEventTarget>)node addEventListener:eventName listener:self useCapture:NO];
    }
}

- (void)handleEvent:(DOMEvent *)event
{
    DOMNode *target = [event target];

    printf("event type:      %s\n", [[event type] UTF8String]);
    printf("  target:        <%s>\n", [[[target nodeName] lowercaseString] UTF8String]);
    
    if ([event isKindOfClass:[DOMEvent class]]) {
        printf("  eventPhase:    %d\n", [event eventPhase]);
        printf("  bubbles:       %d\n", [event bubbles] ? 1 : 0);
        printf("  cancelable:    %d\n", [event cancelable] ? 1 : 0);
    }
    
    if ([event isKindOfClass:[DOMUIEvent class]]) {
        printf("  detail:        %d\n", [(DOMUIEvent*)event detail]);
        
        DOMAbstractView *view = [(DOMUIEvent*)event view];
        if (view) {
            printf("  view:          OK");            
            if ([view document])
                printf(" (document: OK)");
            printf("\n");
        }
    }
    
    if ([event isKindOfClass:[DOMKeyboardEvent class]]) {
        printf("  keyIdentifier: %s\n", [[(DOMKeyboardEvent*)event keyIdentifier] UTF8String]);
        printf("  keyLocation:   %d\n", [(DOMKeyboardEvent*)event keyLocation]);
        printf("  modifier keys: c:%d s:%d a:%d m:%d\n", 
               [(DOMKeyboardEvent*)event ctrlKey] ? 1 : 0, 
               [(DOMKeyboardEvent*)event shiftKey] ? 1 : 0, 
               [(DOMKeyboardEvent*)event altKey] ? 1 : 0, 
               [(DOMKeyboardEvent*)event metaKey] ? 1 : 0);
        printf("  keyCode:       %d\n", [(DOMKeyboardEvent*)event keyCode]);
        printf("  charCode:      %d\n", [(DOMKeyboardEvent*)event charCode]);
    }
    
    if ([event isKindOfClass:[DOMMouseEvent class]]) {
        printf("  button:        %d\n", [(DOMMouseEvent*)event button]);
        printf("  clientX:       %d\n", [(DOMMouseEvent*)event clientX]);
        printf("  clientY:       %d\n", [(DOMMouseEvent*)event clientY]);
        printf("  screenX:       %d\n", [(DOMMouseEvent*)event screenX]);
        printf("  screenY:       %d\n", [(DOMMouseEvent*)event screenY]);
        printf("  modifier keys: c:%d s:%d a:%d m:%d\n", 
               [(DOMMouseEvent*)event ctrlKey] ? 1 : 0, 
               [(DOMMouseEvent*)event shiftKey] ? 1 : 0, 
               [(DOMMouseEvent*)event altKey] ? 1 : 0, 
               [(DOMMouseEvent*)event metaKey] ? 1 : 0);
        id relatedTarget = [(DOMMouseEvent*)event relatedTarget];
        if (relatedTarget) {
            printf("  relatedTarget: %s", [[[relatedTarget class] description] UTF8String]);
            if ([relatedTarget isKindOfClass:[DOMNode class]])
                printf(" (nodeName: %s)", [[(DOMNode*)relatedTarget nodeName] UTF8String]);
            printf("\n");
        }
    }
    
    if ([event isKindOfClass:[DOMMutationEvent class]]) {
        printf("  prevValue:     %s\n", [[(DOMMutationEvent*)event prevValue] UTF8String]);
        printf("  newValue:      %s\n", [[(DOMMutationEvent*)event newValue] UTF8String]);
        printf("  attrName:      %s\n", [[(DOMMutationEvent*)event attrName] UTF8String]);
        printf("  attrChange:    %d\n", [(DOMMutationEvent*)event attrChange]);
        DOMNode *relatedNode = [(DOMMutationEvent*)event relatedNode];
        if (relatedNode) {
            printf("  relatedNode:   %s (nodeName: %s)\n", 
                   [[[relatedNode class] description] UTF8String],
                   [[relatedNode nodeName] UTF8String]);
        }
    }
    
    if ([event isKindOfClass:[DOMWheelEvent class]]) {
        printf("  clientX:       %d\n", [(DOMWheelEvent*)event clientX]);
        printf("  clientY:       %d\n", [(DOMWheelEvent*)event clientY]);
        printf("  screenX:       %d\n", [(DOMWheelEvent*)event screenX]);
        printf("  screenY:       %d\n", [(DOMWheelEvent*)event screenY]);
        printf("  modifier keys: c:%d s:%d a:%d m:%d\n", 
               [(DOMWheelEvent*)event ctrlKey] ? 1 : 0, 
               [(DOMWheelEvent*)event shiftKey] ? 1 : 0, 
               [(DOMWheelEvent*)event altKey] ? 1 : 0, 
               [(DOMWheelEvent*)event metaKey] ? 1 : 0);
        printf("  isHorizontal:  %d\n", [(DOMWheelEvent*)event isHorizontal] ? 1 : 0);
        printf("  wheelDelta:    %d\n", [(DOMWheelEvent*)event wheelDelta]);
    }
}

// FIXME: It's not good to have a test hard-wired into this controller like this.
// Instead we need to get testing framework based on the Objective-C bindings
// to work well enough that we can test that way instead.
- (void)fireKeyboardEventsToElement:(WebScriptObject *)element {
    
    if (![element isKindOfClass:[DOMHTMLElement class]]) {
        return;
    }
    
    DOMHTMLElement *target = (DOMHTMLElement*)element;
    DOMDocument *document = [target ownerDocument];
    
    // Keyboard Event 1
    
    DOMEvent *domEvent = [document createEvent:@"KeyboardEvent"];
    [(DOMKeyboardEvent*)domEvent initKeyboardEvent:@"keydown" 
                                         canBubble:YES
                                        cancelable:YES
                                              view:[document defaultView]
                                     keyIdentifier:@"U+000041" 
                                       keyLocation:0
                                           ctrlKey:YES
                                            altKey:NO
                                          shiftKey:NO
                                           metaKey:NO];
    [target dispatchEvent:domEvent];  
        
    // Keyboard Event 2
    
    domEvent = [document createEvent:@"KeyboardEvent"];
    [(DOMKeyboardEvent*)domEvent initKeyboardEvent:@"keypress" 
                                         canBubble:YES
                                        cancelable:YES
                                              view:[document defaultView]
                                     keyIdentifier:@"U+000045" 
                                       keyLocation:1
                                           ctrlKey:NO
                                            altKey:YES
                                          shiftKey:NO
                                           metaKey:NO];
    [target dispatchEvent:domEvent];    
    
    // Keyboard Event 3
    
    domEvent = [document createEvent:@"KeyboardEvent"];
    [(DOMKeyboardEvent*)domEvent initKeyboardEvent:@"keyup" 
                                         canBubble:YES
                                        cancelable:YES
                                              view:[document defaultView]
                                     keyIdentifier:@"U+000056" 
                                       keyLocation:0
                                           ctrlKey:NO
                                            altKey:NO
                                          shiftKey:NO
                                           metaKey:NO];
    [target dispatchEvent:domEvent];   
    
}

@end
