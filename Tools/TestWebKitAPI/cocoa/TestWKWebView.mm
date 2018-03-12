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
#import "TestWKWebView.h"

#if WK_API_ENABLED

#import "TestNavigationDelegate.h"
#import "Utilities.h"

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>
#endif

#if PLATFORM(IOS)
#import <UIKit/UIKit.h>
#import <wtf/SoftLinking.h>
SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIWindow)
#endif

@implementation TestMessageHandler {
    NSMutableDictionary<NSString *, dispatch_block_t> *_messageHandlers;
}

- (void)addMessage:(NSString *)message withHandler:(dispatch_block_t)handler
{
    if (!_messageHandlers)
        _messageHandlers = [NSMutableDictionary dictionary];

    _messageHandlers[message] = [handler copy];
}

- (void)removeMessage:(NSString *)message
{
    [_messageHandlers removeObjectForKey:message];
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    dispatch_block_t handler = _messageHandlers[message.body];
    if (handler)
        handler();
}

@end

#if PLATFORM(MAC)
@interface TestWKWebViewHostWindow : NSWindow
#else
@interface TestWKWebViewHostWindow : UIWindow
#endif // PLATFORM(MAC)
@end

@implementation TestWKWebViewHostWindow {
    BOOL _forceKeyWindow;
}

#if PLATFORM(MAC)
static int gEventNumber = 1;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101003
NSEventMask __simulated_forceClickAssociatedEventsMask(id self, SEL _cmd)
{
    return NSEventMaskPressure | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
}
#endif

- (void)_mouseDownAtPoint:(NSPoint)point simulatePressure:(BOOL)simulatePressure clickCount:(NSUInteger)clickCount
{
    NSEventType mouseEventType = NSEventTypeLeftMouseDown;

    NSEventMask modifierFlags = 0;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101003
    if (simulatePressure)
        modifierFlags |= NSEventMaskPressure;
#else
    simulatePressure = NO;
#endif

    NSEvent *event = [NSEvent mouseEventWithType:mouseEventType location:point modifierFlags:modifierFlags timestamp:GetCurrentEventTime() windowNumber:self.windowNumber context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:clickCount pressure:simulatePressure];
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

- (void)_mouseUpAtPoint:(NSPoint)point clickCount:(NSUInteger)clickCount
{
    [self sendEvent:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:point modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:self.windowNumber context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:clickCount pressure:0]];
}
#endif // PLATFORM(MAC)

- (BOOL)isKeyWindow
{
    return _forceKeyWindow || [super isKeyWindow];
}

- (void)makeKeyWindow
{
    if (_forceKeyWindow)
        return;

    _forceKeyWindow = YES;
#if PLATFORM(MAC)
    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidBecomeKeyNotification object:self];
#else
    [[NSNotificationCenter defaultCenter] postNotificationName:UIWindowDidBecomeKeyNotification object:self];
#endif
}

- (void)resignKeyWindow
{
    _forceKeyWindow = NO;
    [super resignKeyWindow];
}

@end

@implementation TestWKWebView {
    RetainPtr<TestWKWebViewHostWindow> _hostWindow;
    RetainPtr<TestMessageHandler> _testHandler;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    WKWebViewConfiguration *defaultConfiguration = [[[WKWebViewConfiguration alloc] init] autorelease];
    return [self initWithFrame:frame configuration:defaultConfiguration];
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    return [self initWithFrame:frame configuration:configuration addToWindow:YES];
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration addToWindow:(BOOL)addToWindow
{
    self = [super initWithFrame:frame configuration:configuration];
    if (!self)
        return nil;

    if (addToWindow)
        [self _setUpTestWindow:frame];

    return self;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration processPoolConfiguration:(_WKProcessPoolConfiguration *)processPoolConfiguration
{
    [configuration setProcessPool:[[[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration] autorelease]];
    return [self initWithFrame:frame configuration:configuration];
}

- (void)_setUpTestWindow:(NSRect)frame
{
#if PLATFORM(MAC)
    _hostWindow = adoptNS([[TestWKWebViewHostWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [_hostWindow setFrameOrigin:NSMakePoint(0, 0)];
    [_hostWindow setIsVisible:YES];
    [_hostWindow contentView].wantsLayer = YES;
    [[_hostWindow contentView] addSubview:self];
    [_hostWindow makeKeyAndOrderFront:self];
#else
    _hostWindow = adoptNS([[TestWKWebViewHostWindow alloc] initWithFrame:frame]);
    [_hostWindow setHidden:NO];
    [_hostWindow addSubview:self];
#endif
}

- (void)clearMessageHandlers:(NSArray *)messageNames
{
    for (NSString *messageName in messageNames)
        [_testHandler removeMessage:messageName];
}

- (void)performAfterReceivingMessage:(NSString *)message action:(dispatch_block_t)action
{
    if (!_testHandler) {
        _testHandler = adoptNS([[TestMessageHandler alloc] init]);
        [[[self configuration] userContentController] addScriptMessageHandler:_testHandler.get() name:@"testHandler"];
    }

    [_testHandler addMessage:message withHandler:action];
}

- (void)loadTestPageNamed:(NSString *)pageName
{
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:pageName withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [self loadRequest:request];
}

- (void)synchronouslyLoadHTMLString:(NSString *)html
{
    NSURL *testResourceURL = [[[NSBundle mainBundle] bundleURL] URLByAppendingPathComponent:@"TestWebKitAPI.resources"];
    [self loadHTMLString:html baseURL:testResourceURL];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName
{
    [self loadTestPageNamed:pageName];
    [self _test_waitForDidFinishNavigation];
}

- (id)objectByEvaluatingJavaScript:(NSString *)script
{
    bool isWaitingForJavaScript = false;
    RetainPtr<id> evalResult;
    [self _evaluateJavaScriptWithoutUserGesture:script completionHandler:[&] (id result, NSError *error) {
        evalResult = result;
        isWaitingForJavaScript = true;
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error: %@ while evaluating script: %@", error, script);
    }];
    TestWebKitAPI::Util::run(&isWaitingForJavaScript);
    return evalResult.autorelease();
}

- (NSString *)stringByEvaluatingJavaScript:(NSString *)script
{
    return [NSString stringWithFormat:@"%@", [self objectByEvaluatingJavaScript:script]];
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
    TestMessageHandler *handler = [[TestMessageHandler alloc] init];
    [handler addMessage:@"loaded" withHandler:actions];

    NSString *onloadScript = @"window.onload = () => window.webkit.messageHandlers.onloadHandler.postMessage('loaded')";
    WKUserScript *script = [[WKUserScript alloc] initWithSource:onloadScript injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO];

    WKUserContentController* contentController = [[self configuration] userContentController];
    [contentController addUserScript:script];
    [contentController addScriptMessageHandler:handler name:@"onloadHandler"];
}

- (void)waitForNextPresentationUpdate
{
    __block bool done = false;
    [self _doAfterNextPresentationUpdate:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

@end

#if PLATFORM(IOS)

@implementation TestWKWebView (IOSOnly)

- (UIView <UITextInput> *)textInputContentView
{
    return (UIView <UITextInput> *)[self valueForKey:@"_currentContentView"];
}

- (RetainPtr<NSArray>)selectionRectsAfterPresentationUpdate
{
    RetainPtr<TestWKWebView> retainedSelf = self;

    __block bool isDone = false;
    __block RetainPtr<NSArray> selectionRects;
    [self _doAfterNextPresentationUpdate:^() {
        selectionRects = adoptNS([[retainedSelf _uiTextSelectionRects] retain]);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    return selectionRects;
}

- (_WKActivatedElementInfo *)activatedElementAtPosition:(CGPoint)position
{
    __block RetainPtr<_WKActivatedElementInfo> info;
    __block bool finished = false;
    [self _requestActivatedElementAtPosition:position completionBlock:^(_WKActivatedElementInfo *elementInfo) {
        info = elementInfo;
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
    return info.autorelease();
}

@end

#endif

#if PLATFORM(MAC)
@implementation TestWKWebView (MacOnly)
- (void)mouseDownAtPoint:(NSPoint)point simulatePressure:(BOOL)simulatePressure
{
    [_hostWindow _mouseDownAtPoint:point simulatePressure:simulatePressure clickCount:1];
}

- (void)mouseUpAtPoint:(NSPoint)point
{
    [_hostWindow _mouseUpAtPoint:point clickCount:1];
}

- (void)mouseMoveToPoint:(NSPoint)point withFlags:(NSEventModifierFlags)flags
{
    [self mouseMoved:[NSEvent mouseEventWithType:NSEventTypeMouseMoved location:point modifierFlags:flags timestamp:GetCurrentEventTime() windowNumber:[_hostWindow windowNumber] context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:0 pressure:0]];
}

- (void)sendClicksAtPoint:(NSPoint)point numberOfClicks:(NSUInteger)numberOfClicks
{
    for (NSUInteger clickCount = 1; clickCount <= numberOfClicks; ++clickCount) {
        [_hostWindow _mouseDownAtPoint:point simulatePressure:NO clickCount:clickCount];
        [_hostWindow _mouseUpAtPoint:point clickCount:clickCount];
    }
}

- (NSWindow *)hostWindow
{
    return _hostWindow.get();
}

- (void)typeCharacter:(char)character {
    NSString *characterAsString = [NSString stringWithFormat:@"%c" , character];
    NSEventType keyDownEventType = NSEventTypeKeyDown;
    NSEventType keyUpEventType = NSEventTypeKeyUp;
    [self keyDown:[NSEvent keyEventWithType:keyDownEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:[_hostWindow windowNumber] context:nil characters:characterAsString charactersIgnoringModifiers:characterAsString isARepeat:NO keyCode:character]];
    [self keyUp:[NSEvent keyEventWithType:keyUpEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:[_hostWindow windowNumber] context:nil characters:characterAsString charactersIgnoringModifiers:characterAsString isARepeat:NO keyCode:character]];
}
@end
#endif

#endif // WK_API_ENABLED
