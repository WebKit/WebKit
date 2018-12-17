/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "EventSendingController.h"

#import "DumpRenderTree.h"
#import "DumpRenderTreeDraggingInfo.h"
#import "DumpRenderTreeFileDraggingSource.h"
#import "DumpRenderTreePasteboard.h"
#import "WebCoreTestSupport.h"
#import <WebKit/DOMPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebViewPrivate.h>
#import <functional>
#import <wtf/RetainPtr.h>

#if !PLATFORM(IOS_FAMILY)
#import <Carbon/Carbon.h> // for GetCurrentEventTime()
#import <WebKit/WebHTMLView.h>
#import <objc/runtime.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#import <WebKit/KeyEventCodesIOS.h>
#import <WebKit/WAKWindow.h>
#import <WebKit/WebEvent.h>
#import <pal/spi/ios/GraphicsServicesSPI.h> // for GSCurrentEventTimestamp()
#endif

#if !PLATFORM(IOS_FAMILY)
extern "C" void _NSNewKillRingSequence();

@interface NSApplication ()
- (void)_setCurrentEvent:(NSEvent *)event;
@end
#endif

enum MouseAction {
    MouseDown,
    MouseUp,
    MouseDragged
};

// Match the DOM spec (sadly the DOM spec does not provide an enum)
enum MouseButton {
    LeftMouseButton = 0,
    MiddleMouseButton = 1,
    RightMouseButton = 2,
    NoMouseButton = -1
};

struct KeyMappingEntry {
    int macKeyCode;
    int macNumpadKeyCode;
    unichar character;
    NSString* characterName;
};

NSPoint lastMousePosition;
NSPoint lastClickPosition;
int lastClickButton = NoMouseButton;
NSArray *webkitDomEventNames;
NSMutableArray *savedMouseEvents; // mouse events sent between mouseDown and mouseUp are stored here, and then executed at once.
BOOL replayingSavedEvents;


#if PLATFORM(IOS_FAMILY)
@interface SyntheticTouch : NSObject {
@public
    CGPoint _location;
    UITouchPhase _phase;
    unsigned _identifier;
};

@property (nonatomic) CGPoint location;
@property (nonatomic) UITouchPhase phase;
@property (nonatomic) unsigned identifier;

+ (id)touchWithLocation:(CGPoint)location phase:(UITouchPhase)phase identifier:(unsigned)identifier;
- (id)initWithLocation:(CGPoint)location phase:(UITouchPhase)phase identifier:(unsigned)identifier;
@end

@implementation SyntheticTouch

@synthesize location = _location;
@synthesize phase = _phase;
@synthesize identifier = _identifier;

+ (id)touchWithLocation:(CGPoint)location phase:(UITouchPhase)phase identifier:(unsigned)identifier
{
    return [[[SyntheticTouch alloc] initWithLocation:location phase:phase identifier:identifier] autorelease];
}

- (id)initWithLocation:(CGPoint)location phase:(UITouchPhase)phase identifier:(unsigned)identifier
{
    if ((self = [super init])) {
        _location = location;
        _phase = phase;
        _identifier = identifier;
    }
    return self;
}
@end // SyntheticTouch
#endif

#if !PLATFORM(IOS_FAMILY)
@interface WebView (WebViewInternalForTesting)
- (WebCore::Frame*)_mainCoreFrame;
@end
#endif

@implementation EventSendingController

#if PLATFORM(MAC)
static NSDraggingSession *drt_WebHTMLView_beginDraggingSessionWithItemsEventSource(WebHTMLView *self, id _cmd, NSArray<NSDraggingItem *> *items, NSEvent *event, id<NSDraggingSource> source)
{
    ASSERT(!draggingInfo);

    WebFrameView *webFrameView = ^ {
        for (NSView *superview = self.superview; superview; superview = superview.superview) {
            if ([superview isKindOfClass:WebFrameView.class])
                return (WebFrameView *)superview;
        }

        ASSERT_NOT_REACHED();
        return (WebFrameView *)nil;
    }();

    WebView *webView = webFrameView.webFrame.webView;

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    for (NSDraggingItem *item in items)
        [pasteboard writeObjects:@[ item.item ]];

    draggingInfo = [[DumpRenderTreeDraggingInfo alloc] initWithImage:nil offset:NSZeroSize pasteboard:pasteboard source:source];
    [webView draggingUpdated:draggingInfo];
    [EventSendingController replaySavedEvents];

    return nullptr;
}
#endif

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
        @"textzoomin",
        @"textzoomout",
        @"unload",
        @"zoom",
        nil];

#if PLATFORM(MAC)
    // Add an implementation of -[WebHTMLView beginDraggingSessionWithItems:event:source:].
    SEL selector = @selector(beginDraggingSessionWithItems:event:source:);
    const char* typeEncoding = method_getTypeEncoding(class_getInstanceMethod(NSView.class, selector));

    if (!class_addMethod(WebHTMLView.class, selector, reinterpret_cast<IMP>(drt_WebHTMLView_beginDraggingSessionWithItemsEventSource), typeEncoding))
        ASSERT_NOT_REACHED();
#endif
}

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(clearKillRing)
            || aSelector == @selector(contextClick)
            || aSelector == @selector(enableDOMUIEventLogging:)
            || aSelector == @selector(fireKeyboardEventsToElement:)
            || aSelector == @selector(keyDown:withModifiers:withLocation:)
            || aSelector == @selector(leapForward:)
            || aSelector == @selector(mouseDown:withModifiers:)
            || aSelector == @selector(mouseMoveToX:Y:)
            || aSelector == @selector(mouseUp:withModifiers:)
            || aSelector == @selector(scheduleAsynchronousClick)
            || aSelector == @selector(scheduleAsynchronousKeyDown:withModifiers:withLocation:)
            || aSelector == @selector(textZoomIn)
            || aSelector == @selector(textZoomOut)
            || aSelector == @selector(zoomPageIn)
            || aSelector == @selector(zoomPageOut)
            || aSelector == @selector(scalePageBy:atX:andY:)
            || aSelector == @selector(mouseScrollByX:andY:)
            || aSelector == @selector(mouseScrollByX:andY:withWheel:andMomentumPhases:)
            || aSelector == @selector(continuousMouseScrollByX:andY:)
            || aSelector == @selector(monitorWheelEvents)
            || aSelector == @selector(callAfterScrollingCompletes:)
#if PLATFORM(MAC)
            || aSelector == @selector(beginDragWithFiles:)
            || aSelector == @selector(beginDragWithFilePromises:)
#endif
#if PLATFORM(IOS_FAMILY)
            || aSelector == @selector(addTouchAtX:y:)
            || aSelector == @selector(updateTouchAtIndex:x:y:)
            || aSelector == @selector(cancelTouchAtIndex:)
            || aSelector == @selector(clearTouchPoints)
            || aSelector == @selector(markAllTouchesAsStationary)
            || aSelector == @selector(releaseTouchAtIndex:)
            || aSelector == @selector(setTouchModifier:value:)
            || aSelector == @selector(touchStart)
            || aSelector == @selector(touchMove)
            || aSelector == @selector(touchEnd)
            || aSelector == @selector(touchCancel)
#endif            
            )
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
#if PLATFORM(MAC)
    if (aSelector == @selector(beginDragWithFiles:))
        return @"beginDragWithFiles";
    if (aSelector == @selector(beginDragWithFilePromises:))
        return @"beginDragWithFilePromises";
#endif
    if (aSelector == @selector(contextClick))
        return @"contextClick";
    if (aSelector == @selector(enableDOMUIEventLogging:))
        return @"enableDOMUIEventLogging";
    if (aSelector == @selector(fireKeyboardEventsToElement:))
        return @"fireKeyboardEventsToElement";
    if (aSelector == @selector(keyDown:withModifiers:withLocation:))
        return @"keyDown";
    if (aSelector == @selector(scheduleAsynchronousKeyDown:withModifiers:withLocation:))
        return @"scheduleAsynchronousKeyDown";
    if (aSelector == @selector(leapForward:))
        return @"leapForward";
    if (aSelector == @selector(mouseDown:withModifiers:))
        return @"mouseDown";
    if (aSelector == @selector(mouseUp:withModifiers:))
        return @"mouseUp";
    if (aSelector == @selector(mouseMoveToX:Y:))
        return @"mouseMoveTo";
    if (aSelector == @selector(mouseScrollByX:andY:))
        return @"mouseScrollBy";
    if (aSelector == @selector(mouseScrollByX:andY:withWheel:andMomentumPhases:))
        return @"mouseScrollByWithWheelAndMomentumPhases";
    if (aSelector == @selector(continuousMouseScrollByX:andY:))
        return @"continuousMouseScrollBy";
    if (aSelector == @selector(scalePageBy:atX:andY:))
        return @"scalePageBy";
    if (aSelector == @selector(monitorWheelEvents))
        return @"monitorWheelEvents";
    if (aSelector == @selector(callAfterScrollingCompletes:))
        return @"callAfterScrollingCompletes";
#if PLATFORM(IOS_FAMILY)
    if (aSelector == @selector(addTouchAtX:y:))
        return @"addTouchPoint";
    if (aSelector == @selector(updateTouchAtIndex:x:y:))
        return @"updateTouchPoint";
    if (aSelector == @selector(cancelTouchAtIndex:))
        return @"cancelTouchPoint";
    if (aSelector == @selector(clearTouchPoints))
        return @"clearTouchPoints";
    if (aSelector == @selector(markAllTouchesAsStationary))
        return @"markAllTouchesAsStationary";
    if (aSelector == @selector(releaseTouchAtIndex:))
        return @"releaseTouchPoint";
    if (aSelector == @selector(setTouchModifier:value:))
        return @"setTouchModifier";
    if (aSelector == @selector(touchStart))
        return @"touchStart";
    if (aSelector == @selector(touchMove))
        return @"touchMove";
    if (aSelector == @selector(touchEnd))
        return @"touchEnd";
    if (aSelector == @selector(touchCancel))
        return @"touchCancel";
#endif
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
#if PLATFORM(IOS_FAMILY)
    [touches release];
#endif
    [super dealloc];
}

- (double)currentEventTime
{
#if !PLATFORM(IOS_FAMILY)
    return GetCurrentEventTime() + timeOffset;
#else
    return GSCurrentEventTimestamp() + timeOffset;
#endif
}

- (void)leapForward:(int)milliseconds
{
    if (dragMode && leftMouseButtonDown && !replayingSavedEvents) {
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
#if !PLATFORM(IOS_FAMILY)
    _NSNewKillRingSequence();
#endif
}

#if !PLATFORM(IOS_FAMILY)
static NSEventType eventTypeForMouseButtonAndAction(int button, MouseAction action)
{
    switch (button) {
    case LeftMouseButton:
        switch (action) {
        case MouseDown:
            return NSEventTypeLeftMouseDown;
        case MouseUp:
            return NSEventTypeLeftMouseUp;
        case MouseDragged:
            return NSEventTypeLeftMouseDragged;
        }
    case RightMouseButton:
        switch (action) {
        case MouseDown:
            return NSEventTypeRightMouseDown;
        case MouseUp:
            return NSEventTypeRightMouseUp;
        case MouseDragged:
            return NSEventTypeRightMouseDragged;
        }
    default:
        switch (action) {
        case MouseDown:
            return NSEventTypeOtherMouseDown;
        case MouseUp:
            return NSEventTypeOtherMouseUp;
        case MouseDragged:
            return NSEventTypeOtherMouseDragged;
        }
    }
    assert(0);
    return static_cast<NSEventType>(0);
}

- (void)beginDragWithFiles:(WebScriptObject*)jsFilePaths
{
    assert(!draggingInfo);
    assert([jsFilePaths isKindOfClass:[WebScriptObject class]]);

    NSPasteboard *pboard = [NSPasteboard pasteboardWithUniqueName];
    [pboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];

    NSURL *currentTestURL = [NSURL URLWithString:[[mainFrame webView] mainFrameURL]];

    NSMutableArray *filePaths = [NSMutableArray array];
    for (unsigned i = 0; [[jsFilePaths webScriptValueAtIndex:i] isKindOfClass:[NSString class]]; i++) {
        NSString *filePath = (NSString *)[jsFilePaths webScriptValueAtIndex:i];
        // Have NSURL encode the name so that we handle '?' in file names correctly.
        NSURL *fileURL = [NSURL fileURLWithPath:filePath];
        NSURL *absoluteFileURL = [NSURL URLWithString:[fileURL relativeString]  relativeToURL:currentTestURL];
        [filePaths addObject:[absoluteFileURL path]];
    }

    [pboard setPropertyList:filePaths forType:NSFilenamesPboardType];
    assert([pboard propertyListForType:NSFilenamesPboardType]); // setPropertyList will silently fail on error, assert that it didn't fail

    // Provide a source, otherwise [DumpRenderTreeDraggingInfo draggingSourceOperationMask] defaults to NSDragOperationNone
    DumpRenderTreeFileDraggingSource *source = [[[DumpRenderTreeFileDraggingSource alloc] init] autorelease];
    draggingInfo = [[DumpRenderTreeDraggingInfo alloc] initWithImage:nil offset:NSZeroSize pasteboard:pboard source:source];
    [[mainFrame webView] draggingEntered:draggingInfo];

    dragMode = NO; // dragMode saves events and then replays them later.  We don't need/want that.
    leftMouseButtonDown = YES; // Make the rest of eventSender think a drag is in progress
}

- (void)beginDragWithFilePromises:(WebScriptObject *)filePaths
{
    assert(!draggingInfo);

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSFilesPromisePboardType, NSFilenamesPboardType] owner:nil];

    NSURL *currentTestURL = [NSURL URLWithString:mainFrame.webView.mainFrameURL];

    size_t i = 0;
    NSMutableArray *fileURLs = [NSMutableArray array];
    NSMutableArray *fileUTIs = [NSMutableArray array];
    while (true) {
        id filePath = [filePaths webScriptValueAtIndex:i++];
        if (![filePath isKindOfClass:NSString.class])
            break;

        NSURL *fileURL = [NSURL fileURLWithPath:(NSString *)filePath relativeToURL:currentTestURL];
        [fileURLs addObject:fileURL];

        NSString *fileUTI;
        if (![fileURL getResourceValue:&fileUTI forKey:NSURLTypeIdentifierKey error:nil])
            break;
        [fileUTIs addObject:fileUTI];
    }

    [pasteboard setPropertyList:fileUTIs forType:NSFilesPromisePboardType];
    assert([pasteboard propertyListForType:NSFilesPromisePboardType]);

    [pasteboard setPropertyList:@[@"file-name-should-not-be-used"] forType:NSFilenamesPboardType];
    assert([pasteboard propertyListForType:NSFilenamesPboardType]);

    auto source = adoptNS([[DumpRenderTreeFileDraggingSource alloc] initWithPromisedFileURLs:fileURLs]);
    draggingInfo = [[DumpRenderTreeDraggingInfo alloc] initWithImage:nil offset:NSZeroSize pasteboard:pasteboard source:source.get()];
    [mainFrame.webView draggingEntered:draggingInfo];

    dragMode = NO;
    leftMouseButtonDown = YES;
}
#endif // !PLATFORM(IOS_FAMILY)

- (void)updateClickCountForButton:(int)buttonNumber
{
    if (([self currentEventTime] - lastClick >= 1) ||
        !NSEqualPoints(lastMousePosition, lastClickPosition) ||
        lastClickButton != buttonNumber) {
        clickCount = 1;
        lastClickButton = buttonNumber;
    } else
        clickCount++;
}

static int modifierFlags(const NSString* modifierName)
{
#if !PLATFORM(IOS_FAMILY)
    const int controlKeyMask = NSEventModifierFlagControl;
    const int shiftKeyMask = NSEventModifierFlagShift;
    const int alternateKeyMask = NSEventModifierFlagOption;
    const int commandKeyMask = NSEventModifierFlagCommand;
    const int capsLockKeyMask = NSEventModifierFlagCapsLock;
#else
    const int controlKeyMask = WebEventFlagMaskLeftControlKey;
    const int shiftKeyMask = WebEventFlagMaskLeftShiftKey;
    const int alternateKeyMask = WebEventFlagMaskLeftOptionKey;
    const int commandKeyMask = WebEventFlagMaskLeftCommandKey;
    const int capsLockKeyMask = WebEventFlagMaskLeftCapsLockKey;
#endif

    int flags = 0;
    if ([modifierName isEqual:@"ctrlKey"])
        flags |= controlKeyMask;
    else if ([modifierName isEqual:@"shiftKey"] || [modifierName isEqual:@"rangeSelectionKey"])
        flags |= shiftKeyMask;
    else if ([modifierName isEqual:@"altKey"])
        flags |= alternateKeyMask;
    else if ([modifierName isEqual:@"metaKey"] || [modifierName isEqual:@"addSelectionKey"])
        flags |= commandKeyMask;
    else if ([modifierName isEqual:@"capsLockKey"])
        flags |= capsLockKeyMask;

    return flags;
}

static int buildModifierFlags(const WebScriptObject* modifiers)
{
    int flags = 0;
    if ([modifiers isKindOfClass:[NSString class]])
        return modifierFlags((NSString*)modifiers);
    else if (![modifiers isKindOfClass:[WebScriptObject class]])
        return flags;
    for (unsigned i = 0; [[modifiers webScriptValueAtIndex:i] isKindOfClass:[NSString class]]; i++) {
        NSString* modifierName = (NSString*)[modifiers webScriptValueAtIndex:i];
        flags |= modifierFlags(modifierName);
    }
    return flags;
}

- (void)mouseDown:(int)buttonNumber withModifiers:(WebScriptObject*)modifiers
{
    [[[mainFrame frameView] documentView] layout];
    [self updateClickCountForButton:buttonNumber];
    
#if !PLATFORM(IOS_FAMILY)
    NSEventType eventType = eventTypeForMouseButtonAndAction(buttonNumber, MouseDown);
    NSEvent *event = [NSEvent mouseEventWithType:eventType
                                        location:lastMousePosition 
                                   modifierFlags:buildModifierFlags(modifiers)
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[mainFrame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];
#else
    WebEvent *event = [[WebEvent alloc] initWithMouseEventType:WebEventMouseDown
                                                     timeStamp:[self currentEventTime]
                                                      location:lastMousePosition];
#endif

    NSView *subView = [[mainFrame webView] hitTest:[event locationInWindow]];
    if (subView) {
#if !PLATFORM(IOS_FAMILY)
        [NSApp _setCurrentEvent:event];
#endif
        [subView mouseDown:event];
#if !PLATFORM(IOS_FAMILY)
        [NSApp _setCurrentEvent:nil];
#endif
        if (buttonNumber == LeftMouseButton)
            leftMouseButtonDown = YES;
    }

#if PLATFORM(IOS_FAMILY)
    [event release];
#endif
}

- (void)mouseDown:(int)buttonNumber
{
    [self mouseDown:buttonNumber withModifiers:nil];
}

- (void)textZoomIn
{
    [[mainFrame webView] makeTextLarger:self];
}

- (void)textZoomOut
{
    [[mainFrame webView] makeTextSmaller:self];
}

- (void)zoomPageIn
{
    [[mainFrame webView] zoomPageIn:self];
}

- (void)zoomPageOut
{
    [[mainFrame webView] zoomPageOut:self];
}

- (void)scalePageBy:(float)scale atX:(float)x andY:(float)y
{
#if !PLATFORM(IOS_FAMILY)
    // -[WebView _scaleWebView:] is Mac-specific API, and calls functions that
    // assert to not be used in iOS.
    [[mainFrame webView] _scaleWebView:scale atOrigin:NSMakePoint(x, y)];
#endif
}

- (void)mouseUp:(int)buttonNumber withModifiers:(WebScriptObject*)modifiers
{
    if (dragMode && !replayingSavedEvents) {
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[EventSendingController instanceMethodSignatureForSelector:@selector(mouseUp:withModifiers:)]];
        [invocation setTarget:self];
        [invocation setSelector:@selector(mouseUp:withModifiers:)];
        [invocation setArgument:&buttonNumber atIndex:2];
        [invocation setArgument:&modifiers atIndex:3];
        
        [EventSendingController saveEvent:invocation];
        [EventSendingController replaySavedEvents];

        return;
    }

    [[[mainFrame frameView] documentView] layout];
#if !PLATFORM(IOS_FAMILY)
    NSEventType eventType = eventTypeForMouseButtonAndAction(buttonNumber, MouseUp);
    NSEvent *event = [NSEvent mouseEventWithType:eventType
                                        location:lastMousePosition 
                                   modifierFlags:buildModifierFlags(modifiers)
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[mainFrame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];
#else
    WebEvent *event = [[WebEvent alloc] initWithMouseEventType:WebEventMouseUp
                                                     timeStamp:[self currentEventTime]
                                                      location:lastMousePosition];
#endif

    NSView *targetView = [[mainFrame webView] hitTest:[event locationInWindow]];
    // FIXME: Silly hack to teach DRT to respect capturing mouse events outside the WebView.
    // The right solution is just to use NSApplication's built-in event sending methods, 
    // instead of rolling our own algorithm for selecting an event target.
    targetView = targetView ? targetView : [[mainFrame frameView] documentView];
    assert(targetView);
#if !PLATFORM(IOS_FAMILY)
    [NSApp _setCurrentEvent:event];
#endif
    [targetView mouseUp:event];
#if !PLATFORM(IOS_FAMILY)
    [NSApp _setCurrentEvent:nil];
#endif
    if (buttonNumber == LeftMouseButton)
        leftMouseButtonDown = NO;
    lastClick = [event timestamp];
    lastClickPosition = lastMousePosition;
#if !PLATFORM(IOS_FAMILY)
    if (draggingInfo) {
        WebView *webView = [mainFrame webView];
        
        NSDragOperation dragOperation = [webView draggingUpdated:draggingInfo];
        
        if (dragOperation != NSDragOperationNone)
            [webView performDragOperation:draggingInfo];
        else
            [webView draggingExited:draggingInfo];
        // Per NSDragging.h: draggingSources may not implement draggedImage:endedAt:operation:
        if ([[draggingInfo draggingSource] respondsToSelector:@selector(draggedImage:endedAt:operation:)])
            [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] endedAt:lastMousePosition operation:dragOperation];
        [draggingInfo release];
        draggingInfo = nil;
    }
#endif

#if PLATFORM(IOS_FAMILY)
    [event release];
#endif
}

- (void)mouseUp:(int)buttonNumber
{
    [self mouseUp:buttonNumber withModifiers:nil];
}

- (void)mouseMoveToX:(int)x Y:(int)y
{
    if (dragMode && leftMouseButtonDown && !replayingSavedEvents) {
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[EventSendingController instanceMethodSignatureForSelector:@selector(mouseMoveToX:Y:)]];
        [invocation setTarget:self];
        [invocation setSelector:@selector(mouseMoveToX:Y:)];
        [invocation setArgument:&x atIndex:2];
        [invocation setArgument:&y atIndex:3];
        
        [EventSendingController saveEvent:invocation];
        return;
    }

    NSView *view = [mainFrame webView];
#if !PLATFORM(IOS_FAMILY)
    NSPoint newMousePosition = [view convertPoint:NSMakePoint(x, [view frame].size.height - y) toView:nil];
    NSEvent *event = [NSEvent mouseEventWithType:(leftMouseButtonDown ? NSEventTypeLeftMouseDragged : NSEventTypeMouseMoved)
                                        location:newMousePosition
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[view window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:(leftMouseButtonDown ? clickCount : 0) 
                                        pressure:0.0];
    CGEventRef cgEvent = event.CGEvent;
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaX, newMousePosition.x - lastMousePosition.x);
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaY, newMousePosition.y - lastMousePosition.y);
    event = [NSEvent eventWithCGEvent:cgEvent];
    lastMousePosition = newMousePosition;
#else
    lastMousePosition = [view convertPoint:NSMakePoint(x, y) toView:nil];
    WebEvent *event = [[WebEvent alloc] initWithMouseEventType:WebEventMouseMoved
                                                     timeStamp:[self currentEventTime]
                                                      location:lastMousePosition];
#endif // !PLATFORM(IOS_FAMILY)

    NSView *subView = [[mainFrame webView] hitTest:[event locationInWindow]];
    if (subView) {
#if !PLATFORM(IOS_FAMILY)
        [NSApp _setCurrentEvent:event];
#endif
        if (leftMouseButtonDown) {
#if !PLATFORM(IOS_FAMILY)
            if (draggingInfo) {
                // Per NSDragging.h: draggingSources may not implement draggedImage:movedTo:
                if ([[draggingInfo draggingSource] respondsToSelector:@selector(draggedImage:movedTo:)])
                    [[draggingInfo draggingSource] draggedImage:[draggingInfo draggedImage] movedTo:lastMousePosition];
                [[mainFrame webView] draggingUpdated:draggingInfo];
            } else
                [subView mouseDragged:event];
#endif
        } else
            [subView mouseMoved:event];
#if !PLATFORM(IOS_FAMILY)
        [NSApp _setCurrentEvent:nil];
#endif
    }

#if PLATFORM(IOS_FAMILY)
    [event release];
#endif
}

- (void)mouseScrollByX:(int)x andY:(int)y continuously:(BOOL)continuously
{
#if !PLATFORM(IOS_FAMILY)
    CGScrollEventUnit unit = continuously ? kCGScrollEventUnitPixel : kCGScrollEventUnitLine;
    CGEventRef cgScrollEvent = CGEventCreateScrollWheelEvent(NULL, unit, 2, y, x);
    
    // Set the CGEvent location in flipped coords relative to the first screen, which
    // compensates for the behavior of +[NSEvent eventWithCGEvent:] when the event has
    // no associated window. See <rdar://problem/17180591>.
    CGPoint lastGlobalMousePosition = CGPointMake(lastMousePosition.x, [[[NSScreen screens] objectAtIndex:0] frame].size.height - lastMousePosition.y);
    CGEventSetLocation(cgScrollEvent, lastGlobalMousePosition);

    NSEvent *scrollEvent = [NSEvent eventWithCGEvent:cgScrollEvent];
    CFRelease(cgScrollEvent);

    NSView *subView = [[mainFrame webView] hitTest:[scrollEvent locationInWindow]];
    if (subView) {
        [NSApp _setCurrentEvent:scrollEvent];
        [subView scrollWheel:scrollEvent];
        [NSApp _setCurrentEvent:nil];
    } else
        printf("mouseScrollByXandYContinuously: Unable to locate target view for current mouse location.");
#endif
}

- (void)continuousMouseScrollByX:(int)x andY:(int)y
{
    [self mouseScrollByX:x andY:y continuously:YES];
}

- (void)mouseScrollByX:(int)x andY:(int)y
{
    [self mouseScrollByX:x andY:y continuously:NO];
}

- (void)mouseScrollByX:(int)x andY:(int)y withWheel:(NSString*)phaseName andMomentumPhases:(NSString*)momentumName
{
#if PLATFORM(MAC)
    uint32_t phase = 0;
    if ([phaseName isEqualToString: @"none"])
        phase = 0;
    else if ([phaseName isEqualToString: @"began"])
        phase = 1; // kCGScrollPhaseBegan
    else if ([phaseName isEqualToString: @"changed"])
        phase = 2; // kCGScrollPhaseChanged;
    else if ([phaseName isEqualToString: @"ended"])
        phase = 4; // kCGScrollPhaseEnded
    else if ([phaseName isEqualToString: @"cancelled"])
        phase = 8; // kCGScrollPhaseCancelled
    else if ([phaseName isEqualToString: @"maybegin"])
        phase = 128; // kCGScrollPhaseMayBegin

    uint32_t momentum = 0;
    if ([momentumName isEqualToString: @"none"])
        momentum = 0; //kCGMomentumScrollPhaseNone;
    else if ([momentumName isEqualToString:@"begin"])
        momentum = 1; // kCGMomentumScrollPhaseBegin;
    else if ([momentumName isEqualToString:@"continue"])
        momentum = 2; // kCGMomentumScrollPhaseContinue;
    else if ([momentumName isEqualToString:@"end"])
        momentum = 3; // kCGMomentumScrollPhaseEnd;

    CGEventRef cgScrollEvent = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 2, y, x);

    // Set the CGEvent location in flipped coords relative to the first screen, which
    // compensates for the behavior of +[NSEvent eventWithCGEvent:] when the event has
    // no associated window. See <rdar://problem/17180591>.
    CGPoint lastGlobalMousePosition = CGPointMake(lastMousePosition.x, [[[NSScreen screens] objectAtIndex:0] frame].size.height - lastMousePosition.y);
    CGEventSetLocation(cgScrollEvent, lastGlobalMousePosition);
    CGEventSetIntegerValueField(cgScrollEvent, kCGScrollWheelEventIsContinuous, 1);
    CGEventSetIntegerValueField(cgScrollEvent, kCGScrollWheelEventScrollPhase, phase);
    CGEventSetIntegerValueField(cgScrollEvent, kCGScrollWheelEventMomentumPhase, momentum);
    
    NSEvent* scrollEvent = [NSEvent eventWithCGEvent:cgScrollEvent];
    CFRelease(cgScrollEvent);

    if (NSView* targetView = [[mainFrame webView] hitTest:[scrollEvent locationInWindow]]) {
        [NSApp _setCurrentEvent:scrollEvent];
        [targetView scrollWheel:scrollEvent];
        [NSApp _setCurrentEvent:nil];
    } else
        printf("mouseScrollByX...andMomentumPhases: Unable to locate target view for current mouse location.");
#endif
}

- (NSArray *)contextClick
{
#if PLATFORM(MAC)
    [[[mainFrame frameView] documentView] layout];
    [self updateClickCountForButton:RightMouseButton];

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                        location:lastMousePosition 
                                   modifierFlags:0 
                                       timestamp:[self currentEventTime]
                                    windowNumber:[[[mainFrame webView] window] windowNumber] 
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:clickCount 
                                        pressure:0.0];

    NSView *subView = [[mainFrame webView] hitTest:[event locationInWindow]];
    NSMutableArray *menuItemStrings = [NSMutableArray array];
    
    if (subView) {
        [NSApp _setCurrentEvent:event];
        NSMenu* menu = [subView menuForEvent:event];
        [NSApp _setCurrentEvent:nil];

        for (int i = 0; i < [menu numberOfItems]; ++i) {
            NSMenuItem* menuItem = [menu itemAtIndex:i];
            if (!strcmp("Inspect Element", [[menuItem title] UTF8String]))
                continue;

            if ([menuItem isSeparatorItem])
                [menuItemStrings addObject:@"<separator>"];
            else
                [menuItemStrings addObject:[menuItem title]];
        }
    }
    
    return menuItemStrings;
#else
    return nil;
#endif
}

- (void)scheduleAsynchronousClick
{
    [self performSelector:@selector(mouseDown:) withObject:nil afterDelay:0];
    [self performSelector:@selector(mouseUp:) withObject:nil afterDelay:0];
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

+ (void)clearSavedEvents
{
    [savedMouseEvents release];
    savedMouseEvents = nil;
}

- (void)keyDown:(NSString *)character withModifiers:(WebScriptObject *)modifiers withLocation:(unsigned long)location
{
    NSString *eventCharacter = character;
    unsigned short keyCode = 0;
    if ([character isEqualToString:@"leftArrow"]) {
        const unichar ch = NSLeftArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7B;
    } else if ([character isEqualToString:@"rightArrow"]) {
        const unichar ch = NSRightArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7C;
    } else if ([character isEqualToString:@"upArrow"]) {
        const unichar ch = NSUpArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7E;
    } else if ([character isEqualToString:@"downArrow"]) {
        const unichar ch = NSDownArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7D;
    } else if ([character isEqualToString:@"pageUp"]) {
        const unichar ch = NSPageUpFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x74;
    } else if ([character isEqualToString:@"pageDown"]) {
        const unichar ch = NSPageDownFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x79;
    } else if ([character isEqualToString:@"home"]) {
        const unichar ch = NSHomeFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x73;
    } else if ([character isEqualToString:@"end"]) {
        const unichar ch = NSEndFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x77;
    } else if ([character isEqualToString:@"insert"]) {
        const unichar ch = NSInsertFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x72;
    } else if ([character isEqualToString:@"delete"]) {
        const unichar ch = NSDeleteFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x75;
    } else if ([character isEqualToString:@"escape"]) {
        const unichar ch = 0x1B;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x35;
    } else if ([character isEqualToString:@"printScreen"]) {
        const unichar ch = NSPrintScreenFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x0; // There is no known virtual key code for PrintScreen.
    } else if ([character isEqualToString:@"cyrillicSmallLetterA"]) {
        const unichar ch = 0x0430;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3; // Shares key with "F" on Russian layout.
    } else if ([character isEqualToString:@"leftControl"]) {
        const unichar ch = 0xFFE3;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3B;
    } else if ([character isEqualToString:@"leftShift"]) {
        const unichar ch = 0xFFE1;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x38;
    } else if ([character isEqualToString:@"leftAlt"]) {
        const unichar ch = 0xFFE7;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3A;
    } else if ([character isEqualToString:@"rightControl"]) {
        const unichar ch = 0xFFE4;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3E;
    } else if ([character isEqualToString:@"rightShift"]) {
        const unichar ch = 0xFFE2;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3C;
    } else if ([character isEqualToString:@"rightAlt"]) {
        const unichar ch = 0xFFE8;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3D;
    }

    // Compare the input string with the function-key names defined by the DOM spec (i.e. "F1",...,"F24").
    // If the input string is a function-key name, set its key code.
    for (unsigned i = 1; i <= 24; i++) {
        if ([character isEqualToString:[NSString stringWithFormat:@"F%u", i]]) {
            const unichar ch = NSF1FunctionKey + (i - 1);
            eventCharacter = [NSString stringWithCharacters:&ch length:1];
            switch (i) {
                case 1: keyCode = 0x7A; break;
                case 2: keyCode = 0x78; break;
                case 3: keyCode = 0x63; break;
                case 4: keyCode = 0x76; break;
                case 5: keyCode = 0x60; break;
                case 6: keyCode = 0x61; break;
                case 7: keyCode = 0x62; break;
                case 8: keyCode = 0x64; break;
                case 9: keyCode = 0x65; break;
                case 10: keyCode = 0x6D; break;
                case 11: keyCode = 0x67; break;
                case 12: keyCode = 0x6F; break;
                case 13: keyCode = 0x69; break;
                case 14: keyCode = 0x6B; break;
                case 15: keyCode = 0x71; break;
                case 16: keyCode = 0x6A; break;
                case 17: keyCode = 0x40; break;
                case 18: keyCode = 0x4F; break;
                case 19: keyCode = 0x50; break;
                case 20: keyCode = 0x5A; break;
            }
        }
    }

    // FIXME: No keyCode is set for most keys.
    if ([character isEqualToString:@"\t"])
        keyCode = 0x30;
    else if ([character isEqualToString:@" "])
        keyCode = 0x31;
    else if ([character isEqualToString:@"\r"])
        keyCode = 0x24;
    else if ([character isEqualToString:@"\n"])
        keyCode = 0x4C;
    else if ([character isEqualToString:@"\x8"])
        keyCode = 0x33;
    else if ([character isEqualToString:@"a"])
        keyCode = 0x00;
    else if ([character isEqualToString:@"b"])
        keyCode = 0x0B;
    else if ([character isEqualToString:@"d"])
        keyCode = 0x02;
    else if ([character isEqualToString:@"e"])
        keyCode = 0x0E;
    else if ([character isEqualToString:@"\x1b"])
        keyCode = 0x1B;

    KeyMappingEntry table[] = {
        {0x2F, 0x41, '.', nil},
        {0,    0x43, '*', nil},
        {0,    0x45, '+', nil},
        {0,    0x47, NSClearLineFunctionKey, @"clear"},
        {0x2C, 0x4B, '/', nil},
        {0,    0x4C, 3, @"enter" },
        {0x1B, 0x4E, '-', nil},
        {0x18, 0x51, '=', nil},
        {0x1D, 0x52, '0', nil},
        {0x12, 0x53, '1', nil},
        {0x13, 0x54, '2', nil},
        {0x14, 0x55, '3', nil},
        {0x15, 0x56, '4', nil},
        {0x17, 0x57, '5', nil},
        {0x16, 0x58, '6', nil},
        {0x1A, 0x59, '7', nil},
        {0x1C, 0x5B, '8', nil},
        {0x19, 0x5C, '9', nil},
    };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(table); ++i) {
        NSString* currentCharacterString = [NSString stringWithCharacters:&table[i].character length:1];
        if ([character isEqualToString:currentCharacterString] || [character isEqualToString:table[i].characterName]) {
            if (location == DOM_KEY_LOCATION_NUMPAD)
                keyCode = table[i].macNumpadKeyCode;
            else
                keyCode = table[i].macKeyCode;
            eventCharacter = currentCharacterString;
            break;
        }
    }

    NSString *charactersIgnoringModifiers = eventCharacter;

    int modifierFlags = 0;

    if ([character length] == 1 && [character characterAtIndex:0] >= 'A' && [character characterAtIndex:0] <= 'Z') {
#if !PLATFORM(IOS_FAMILY)
        modifierFlags |= NSEventModifierFlagShift;
#else
        modifierFlags |= WebEventFlagMaskLeftShiftKey;
#endif
        charactersIgnoringModifiers = [character lowercaseString];
    }

    modifierFlags |= buildModifierFlags(modifiers);

    if (location == DOM_KEY_LOCATION_NUMPAD)
        modifierFlags |= NSEventModifierFlagNumericPad;

    [[[mainFrame frameView] documentView] layout];

#if !PLATFORM(IOS_FAMILY)
    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierFlags
                        timestamp:[self currentEventTime]
                        windowNumber:[[[mainFrame webView] window] windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:eventCharacter
                        charactersIgnoringModifiers:charactersIgnoringModifiers
                        isARepeat:NO
                        keyCode:keyCode];
#else
    WebEvent *event = [[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:[self currentEventTime] characters:eventCharacter charactersIgnoringModifiers:charactersIgnoringModifiers modifiers:(WebEventFlags)modifierFlags isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:[character characterAtIndex:0] isTabKey:([character characterAtIndex:0] == '\t')];
#endif

#if !PLATFORM(IOS_FAMILY)
    [NSApp _setCurrentEvent:event];
#endif
    [[[[mainFrame webView] window] firstResponder] keyDown:event];
#if !PLATFORM(IOS_FAMILY)
    [NSApp _setCurrentEvent:nil];
#endif

#if !PLATFORM(IOS_FAMILY)
    event = [NSEvent keyEventWithType:NSEventTypeKeyUp
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierFlags
                        timestamp:[self currentEventTime]
                        windowNumber:[[[mainFrame webView] window] windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:eventCharacter
                        charactersIgnoringModifiers:charactersIgnoringModifiers
                        isARepeat:NO
                        keyCode:keyCode];
#else
    [event release];
    event = [[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:[self currentEventTime] characters:eventCharacter charactersIgnoringModifiers:charactersIgnoringModifiers modifiers:(WebEventFlags)modifierFlags isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:[character characterAtIndex:0] isTabKey:([character characterAtIndex:0] == '\t')];
#endif

#if !PLATFORM(IOS_FAMILY)
    [NSApp _setCurrentEvent:event];
#endif
    [[[[mainFrame webView] window] firstResponder] keyUp:event];
#if !PLATFORM(IOS_FAMILY)
    [NSApp _setCurrentEvent:nil];
#endif

#if PLATFORM(IOS_FAMILY)
    [event release];
#endif
}

- (void)keyDownWrapper:(NSString *)character withModifiers:(WebScriptObject *)modifiers withLocation:(unsigned long)location
{
    [self keyDown:character withModifiers:modifiers withLocation:location];
}

- (void)scheduleAsynchronousKeyDown:(NSString *)character withModifiers:(WebScriptObject *)modifiers withLocation:(unsigned long)location
{
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[EventSendingController instanceMethodSignatureForSelector:@selector(keyDownWrapper:withModifiers:withLocation:)]];
    [invocation retainArguments];
    [invocation setTarget:self];
    [invocation setSelector:@selector(keyDownWrapper:withModifiers:withLocation:)];
    [invocation setArgument:&character atIndex:2];
    [invocation setArgument:&modifiers atIndex:3];
    [invocation setArgument:&location atIndex:4];
    [invocation performSelector:@selector(invoke) withObject:nil afterDelay:0];
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
        printf("  keyLocation:   %d\n", [(DOMKeyboardEvent*)event location]);
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
    
    if (![element isKindOfClass:[DOMHTMLElement class]])
        return;
    
    DOMHTMLElement *target = (DOMHTMLElement*)element;
    DOMDocument *document = [target ownerDocument];
    
    // Keyboard Event 1
    
    DOMEvent *domEvent = [document createEvent:@"KeyboardEvent"];
    [(DOMKeyboardEvent*)domEvent initKeyboardEvent:@"keydown" 
                                         canBubble:YES
                                        cancelable:YES
                                              view:[document defaultView]
                                     keyIdentifier:@"U+000041" 
                                       location:0
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
                                       location:1
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
                                       location:0
                                           ctrlKey:NO
                                            altKey:NO
                                          shiftKey:NO
                                           metaKey:NO];
    [target dispatchEvent:domEvent];   
    
}

- (void)monitorWheelEvents
{
#if PLATFORM(MAC)
    WebCore::Frame* frame = [[mainFrame webView] _mainCoreFrame];
    if (!frame)
        return;

    WebCoreTestSupport::monitorWheelEvents(*frame);
#endif
}

- (void)callAfterScrollingCompletes:(WebScriptObject*)callback
{
#if PLATFORM(MAC)
    JSObjectRef jsCallbackFunction = [callback JSObject];
    if (!jsCallbackFunction)
        return;

    WebCore::Frame* frame = [[mainFrame webView] _mainCoreFrame];
    if (!frame)
        return;

    JSGlobalContextRef globalContext = [mainFrame globalContext];
    WebCoreTestSupport::setTestCallbackAndStartNotificationTimer(*frame, globalContext, jsCallbackFunction);
#endif
}

#if PLATFORM(IOS_FAMILY)
- (void)addTouchAtX:(int)x y:(int)y
{
    if (!touches)
        touches = [[NSMutableArray alloc] init];

    [touches addObject:[SyntheticTouch touchWithLocation:CGPointMake(x, y) phase:UITouchPhaseBegan identifier:currentTouchIdentifier++]];
}

- (void)cancelTouchAtIndex:(unsigned)index
{
    if (index < [touches count])
        [[touches objectAtIndex:index] setPhase:UITouchPhaseCancelled];
}

- (void)clearTouchPoints
{
    [touches removeAllObjects];
}

- (void)releaseTouchAtIndex:(unsigned)index
{
    if (index < [touches count]) {
        SyntheticTouch *touch = [touches objectAtIndex:index];
        [touch setPhase:UITouchPhaseEnded];
    }
}

- (void)markAllTouchesAsStationary
{
    for (SyntheticTouch *touch in touches)
        [touch setPhase:UITouchPhaseStationary];
}

- (void)updateTouchAtIndex:(unsigned)index x:(int)x y:(int)y
{
    if (index < [touches count]) {
        SyntheticTouch *touch = [touches objectAtIndex:index];
        [touch setPhase:UITouchPhaseMoved];
        [touch setLocation:CGPointMake(x, y)];
    }
}

- (void)setTouchModifier:(NSString*)modifierName value:(BOOL)flag
{
    unsigned modifier = 0;
    
    if ([modifierName isEqualToString:@"alt"])
        modifier = WebEventFlagMaskLeftOptionKey;
    else if ([modifierName isEqualToString:@"shift"])
        modifier = WebEventFlagMaskLeftShiftKey;
    else if ([modifierName isEqualToString:@"meta"])
        modifier = WebEventFlagMaskLeftCommandKey;
    else if ([modifierName isEqualToString:@"ctrl"])
        modifier = WebEventFlagMaskLeftControlKey;

    if (!modifier)
        return;

    if (flag)
        nextEventFlags |= modifier;
    else
        nextEventFlags &= ~modifier;
}

- (void)sentTouchEventOfType:(WebEventType)type
{
    NSMutableArray *touchLocations = [[NSMutableArray alloc] initWithCapacity:[touches count]];
    NSMutableArray *touchIdentifiers = [[NSMutableArray alloc] initWithCapacity:[touches count]];
    NSMutableArray *touchPhases = [[NSMutableArray alloc] initWithCapacity:[touches count]];
    
    CGPoint centroid = CGPointZero;
    NSUInteger touchesDownCount = 0;

    for (SyntheticTouch *currTouch in touches) {
        [touchLocations addObject:[NSValue valueWithCGPoint:currTouch.location]];
        [touchIdentifiers addObject:[NSNumber numberWithUnsignedInt:currTouch.identifier]];
        [touchPhases addObject:[NSNumber numberWithUnsignedInt:currTouch.phase]];

        if ((currTouch.phase == UITouchPhaseEnded) || (currTouch.phase == UITouchPhaseCancelled))
            continue;
        
        centroid.x += currTouch.location.x;
        centroid.y += currTouch.location.y;

        touchesDownCount++;
    }

    if (touchesDownCount > 0)
        centroid = CGPointMake(centroid.x / touchesDownCount, centroid.y / touchesDownCount);
    else
        centroid = CGPointZero;

    WebEvent *event = [[WebEvent alloc] initWithTouchEventType:type
                                        timeStamp:[self currentEventTime]
                                        location:centroid
                                        modifiers:(WebEventFlags)nextEventFlags
                                        touchCount:[touches count]
                                        touchLocations:touchLocations
                                        touchIdentifiers:touchIdentifiers
                                        touchPhases:touchPhases
                                        isGesture:(touchesDownCount > 1)
                                        gestureScale:1
                                        gestureRotation:0];
    // Ensure that layout is up-to-date so that hit-testing through WAKViews works correctly.
    [mainFrame updateLayout];
    [[[mainFrame webView] window] sendEventSynchronously:event];
    [event release];
    
    [touchLocations release];
    [touchIdentifiers release];
    [touchPhases release];
    
    nextEventFlags = 0;
}

- (void)touchStart
{
    [self sentTouchEventOfType:WebEventTouchBegin];
}

- (void)touchMove
{
    [self sentTouchEventOfType:WebEventTouchChange];
}

- (void)touchEnd
{
    [self sentTouchEventOfType:WebEventTouchEnd];

    NSMutableArray *touchesToRemove = [[NSMutableArray alloc] init];
    for (SyntheticTouch *currTouch in touches) {
        if (currTouch.phase == UITouchPhaseEnded)
            [touchesToRemove addObject:currTouch];
    }

    [touches removeObjectsInArray:touchesToRemove];
    [touchesToRemove release];
}

- (void)touchCancel
{
    [self sentTouchEventOfType:WebEventTouchCancel];

    NSMutableArray *touchesToRemove = [[NSMutableArray alloc] init];
    for (SyntheticTouch *currTouch in touches) {
        if (currTouch.phase == UITouchPhaseCancelled)
            [touchesToRemove addObject:currTouch];
    }

    [touches removeObjectsInArray:touchesToRemove];
    [touchesToRemove release];
}
#endif

@end
