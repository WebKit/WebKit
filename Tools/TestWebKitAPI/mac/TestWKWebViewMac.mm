/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TestWKWebViewMac.h"

#if WK_API_ENABLED && PLATFORM(MAC)

#import "TestNavigationDelegate.h"
#import "Utilities.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <WebKit/WebKitPrivate.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

@implementation TestMessageHandler {
    dispatch_block_t _handler;
    NSString *_message;
}

- (instancetype)initWithMessage:(NSString *)message handler:(dispatch_block_t)handler
{
    if (!(self = [super init]))
        return nil;

    _handler = [handler copy];
    _message = message;

    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([(NSString *)[message body] isEqualToString:_message] && _handler)
        _handler();
}

@end

@interface TestWKWebViewHostWindow : NSWindow
@end

@implementation TestWKWebViewHostWindow {
    BOOL _forceKeyWindow;
}

static int gEventNumber = 1;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101003
NSEventMask __simulated_forceClickAssociatedEventsMask(id self, SEL _cmd)
{
    return NSEventMaskPressure | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
}
#endif

- (void)_mouseDownAtPoint:(NSPoint)point simulatePressure:(BOOL)simulatePressure
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    NSEventType mouseEventType = NSEventTypeLeftMouseDown;
#else
    NSEventType mouseEventType = NSLeftMouseDown;
#endif

    NSEventMask modifierFlags = 0;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101003
    if (simulatePressure)
        modifierFlags |= NSEventMaskPressure;
#else
    simulatePressure = NO;
#endif

    NSEvent *event = [NSEvent mouseEventWithType:mouseEventType location:point modifierFlags:modifierFlags timestamp:GetCurrentEventTime() windowNumber:self.windowNumber context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:1 pressure:simulatePressure];
    if (!simulatePressure) {
        [self sendEvent:event];
        return;
    }

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101003
    IMP simulatedAssociatedEventsMaskImpl = (IMP)__simulated_forceClickAssociatedEventsMask;
    Method associatedEventsMaskMethod = class_getInstanceMethod([NSEvent class], @selector(associatedEventsMask));
    IMP originalAssociatedEventsMaskImpl = method_setImplementation(associatedEventsMaskMethod, simulatedAssociatedEventsMaskImpl);
    @try {
        [self sendEvent:event];
    } @finally {
        // In the case where event sending raises an exception, we still want to restore the original implementation
        // to prevent subsequent event sending tests from being affected.
        method_setImplementation(associatedEventsMaskMethod, originalAssociatedEventsMaskImpl);
    }
#endif
}

- (BOOL)isKeyWindow
{
    return _forceKeyWindow || [super isKeyWindow];
}

- (void)makeKeyWindow
{
    if (_forceKeyWindow)
        return;

    _forceKeyWindow = YES;
    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidBecomeKeyNotification object:self];
}

- (void)resignKeyWindow
{
    _forceKeyWindow = NO;
    [super resignKeyWindow];
}

@end

@implementation TestWKWebView {
    TestWKWebViewHostWindow *_hostWindow;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    WKWebViewConfiguration *defaultConfiguration = [[WKWebViewConfiguration alloc] init];
    return [self initWithFrame:frame configuration:defaultConfiguration];
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (self = [super initWithFrame:frame configuration:configuration])
        [self _setUpTestWindow:frame];

    return self;
}

- (void)_setUpTestWindow:(NSRect)frame
{
    _hostWindow = [[TestWKWebViewHostWindow alloc] initWithContentRect:frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [_hostWindow setFrameOrigin:NSMakePoint(0, 0)];
    [[_hostWindow contentView] addSubview:self];
    [_hostWindow setIsVisible:YES];
    [_hostWindow makeKeyAndOrderFront:self];
}

- (void)mouseDownAtPoint:(NSPoint)point simulatePressure:(BOOL)simulatePressure
{
    [_hostWindow _mouseDownAtPoint:point simulatePressure:simulatePressure];
}

- (void)performAfterReceivingMessage:(NSString *)message action:(dispatch_block_t)action
{
    RetainPtr<TestMessageHandler> handler = adoptNS([[TestMessageHandler alloc] initWithMessage:message handler:action]);
    WKUserContentController* contentController = [[self configuration] userContentController];
    [contentController removeScriptMessageHandlerForName:@"testHandler"];
    [contentController addScriptMessageHandler:handler.get() name:@"testHandler"];
}

- (void)loadTestPageNamed:(NSString *)pageName
{
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:pageName withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [self loadRequest:request];
}

- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName
{
    [self loadTestPageNamed:pageName];
    [self _test_waitForDidFinishNavigation];
}

- (void)typeCharacter:(char)character {
    NSString *characterAsString = [NSString stringWithFormat:@"%c" , character];
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    NSEventType keyDownEventType = NSEventTypeKeyDown;
    NSEventType keyUpEventType = NSEventTypeKeyUp;
#else
    NSEventType keyDownEventType = NSKeyDown;
    NSEventType keyUpEventType = NSKeyUp;
#endif
    [self keyDown:[NSEvent keyEventWithType:keyDownEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:_hostWindow.windowNumber context:nil characters:characterAsString charactersIgnoringModifiers:characterAsString isARepeat:NO keyCode:character]];
    [self keyUp:[NSEvent keyEventWithType:keyUpEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:_hostWindow.windowNumber context:nil characters:characterAsString charactersIgnoringModifiers:characterAsString isARepeat:NO keyCode:character]];
}

- (NSString *)stringByEvaluatingJavaScript:(NSString *)script
{
    __block bool isWaitingForJavaScript = false;
    __block NSString *evalResult = nil;
    [self evaluateJavaScript:script completionHandler:^(id result, NSError *error)
    {
        evalResult = [NSString stringWithFormat:@"%@", result];
        isWaitingForJavaScript = true;
        EXPECT_TRUE(!error);
    }];

    TestWebKitAPI::Util::run(&isWaitingForJavaScript);
    return evalResult;
}

- (void)waitForMessage:(NSString *)message
{
    __block bool isDoneWaiting = false;
    [self performAfterReceivingMessage:message action:^()
    {
        isDoneWaiting = true;
    }];
    TestWebKitAPI::Util::run(&isDoneWaiting);
}

- (void)performAfterLoading:(dispatch_block_t)actions {
    TestMessageHandler *handler = [[TestMessageHandler alloc] initWithMessage:@"loaded" handler:actions];
    NSString *onloadScript = @"window.onload = () => window.webkit.messageHandlers.onloadHandler.postMessage('loaded')";
    WKUserScript *script = [[WKUserScript alloc] initWithSource:onloadScript injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO];

    WKUserContentController* contentController = [[self configuration] userContentController];
    [contentController addUserScript:script];
    [contentController addScriptMessageHandler:handler name:@"onloadHandler"];
}

@end

#endif /* WK_API_ENABLED && PLATFORM(MAC) */
