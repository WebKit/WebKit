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

#import "ClassMethodSwizzler.h"
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

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <wtf/SoftLinking.h>
SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIWindow)

@implementation WKWebView (WKWebViewTestingQuirks)

// TestWebKitAPI is currently not a UIApplication so we are unable to track if it is in
// the background or not (https://bugs.webkit.org/show_bug.cgi?id=175204). This can
// cause our processes to get suspended on iOS. We work around this by having
// WKWebView._isBackground always return NO in the context of API tests.
- (BOOL)_isBackground
{
    return NO;
}
@end
#endif

@implementation WKWebView (TestWebKitAPI)

- (BOOL)_synchronouslyExecuteEditCommand:(NSString *)command argument:(NSString *)argument
{
    __block bool done = false;
    __block bool success;
    [self _executeEditCommand:command argument:argument completion:^(BOOL completionSuccess) {
        done = true;
        success = completionSuccess;
    }];
    TestWebKitAPI::Util::run(&done);
    return success;
}

@end

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

NSEventMask __simulated_forceClickAssociatedEventsMask(id self, SEL _cmd)
{
    return NSEventMaskPressure | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
}

- (void)_mouseDownAtPoint:(NSPoint)point simulatePressure:(BOOL)simulatePressure clickCount:(NSUInteger)clickCount
{
    NSEventType mouseEventType = NSEventTypeLeftMouseDown;

    NSEventMask modifierFlags = 0;
    if (simulatePressure)
        modifierFlags |= NSEventMaskPressure;

    NSEvent *event = [NSEvent mouseEventWithType:mouseEventType location:point modifierFlags:modifierFlags timestamp:GetCurrentEventTime() windowNumber:self.windowNumber context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:clickCount pressure:simulatePressure];
    if (!simulatePressure) {
        [self sendEvent:event];
        return;
    }

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
#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<TestWebKitAPI::ClassMethodSwizzler> _sharedCalloutBarSwizzler;
#endif
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

#if PLATFORM(IOS_FAMILY)

static UICalloutBar *suppressUICalloutBar()
{
    return nil;
}

#endif

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration addToWindow:(BOOL)addToWindow
{
    self = [super initWithFrame:frame configuration:configuration];
    if (!self)
        return nil;

    if (addToWindow)
        [self _setUpTestWindow:frame];

#if PLATFORM(IOS_FAMILY)
    // FIXME: Remove this workaround once <https://webkit.org/b/175204> is fixed.
    _sharedCalloutBarSwizzler = std::make_unique<TestWebKitAPI::ClassMethodSwizzler>([UICalloutBar class], @selector(sharedCalloutBar), reinterpret_cast<IMP>(suppressUICalloutBar));
#endif

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

- (void)synchronouslyLoadHTMLString:(NSString *)html baseURL:(NSURL *)url
{
    [self loadHTMLString:html baseURL:url];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadHTMLString:(NSString *)html
{
    [self synchronouslyLoadHTMLString:html baseURL:[[[NSBundle mainBundle] bundleURL] URLByAppendingPathComponent:@"TestWebKitAPI.resources"]];
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

- (id)objectByEvaluatingJavaScriptWithUserGesture:(NSString *)script
{
    bool isWaitingForJavaScript = false;
    RetainPtr<id> evalResult;
    [self evaluateJavaScript:script completionHandler:[&] (id result, NSError *error) {
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

- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName
{
    NSString *script = [NSString stringWithFormat:@"getComputedStyle(getSelection().getRangeAt(0).startContainer.parentElement)['%@']", propertyName];
    return [self stringByEvaluatingJavaScript:script];
}

- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName
{
    NSString *script = [NSString stringWithFormat:@"getComputedStyle(getSelection().getRangeAt(0).endContainer.parentElement)['%@']", propertyName];
    return [self stringByEvaluatingJavaScript:script];
}

- (void)collapseToStart
{
    [self evaluateJavaScript:@"getSelection().collapseToStart()" completionHandler:nil];
}

- (void)collapseToEnd
{
    [self evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
}

@end

#if PLATFORM(IOS_FAMILY)

@implementation TestWKWebView (IOSOnly)

- (UIView <UITextInputPrivate, UITextInputMultiDocument> *)textInputContentView
{
    return (UIView <UITextInputPrivate, UITextInputMultiDocument> *)[self valueForKey:@"_currentContentView"];
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

- (CGRect)caretViewRectInContentCoordinates
{
    UIView *selectionView = [self.textInputContentView valueForKeyPath:@"interactionAssistant.selectionView"];
    CGRect caretFrame = [[selectionView valueForKeyPath:@"caretView.frame"] CGRectValue];
    return [selectionView convertRect:caretFrame toView:self.textInputContentView];
}

- (NSArray<NSValue *> *)selectionViewRectsInContentCoordinates
{
    NSMutableArray *selectionRects = [NSMutableArray array];
    NSArray<UITextSelectionRect *> *rects = [self.textInputContentView valueForKeyPath:@"interactionAssistant.selectionView.rangeView.rects"];
    for (UITextSelectionRect *rect in rects)
        [selectionRects addObject:[NSValue valueWithCGRect:rect.rect]];
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
- (void)mouseDownAtPoint:(NSPoint)pointInWindow simulatePressure:(BOOL)simulatePressure
{
    [_hostWindow _mouseDownAtPoint:pointInWindow simulatePressure:simulatePressure clickCount:1];
}

- (void)mouseUpAtPoint:(NSPoint)pointInWindow
{
    [_hostWindow _mouseUpAtPoint:pointInWindow clickCount:1];
}

- (void)mouseMoveToPoint:(NSPoint)pointInWindow withFlags:(NSEventModifierFlags)flags
{
    [self mouseMoved:[self _mouseEventWithType:NSEventTypeMouseMoved atLocation:pointInWindow flags:flags timestamp:GetCurrentEventTime() clickCount:0]];
}

- (void)sendClicksAtPoint:(NSPoint)pointInWindow numberOfClicks:(NSUInteger)numberOfClicks
{
    for (NSUInteger clickCount = 1; clickCount <= numberOfClicks; ++clickCount) {
        [_hostWindow _mouseDownAtPoint:pointInWindow simulatePressure:NO clickCount:clickCount];
        [_hostWindow _mouseUpAtPoint:pointInWindow clickCount:clickCount];
    }
}

- (void)mouseEnterAtPoint:(NSPoint)pointInWindow
{
    [self mouseEntered:[self _mouseEventWithType:NSEventTypeMouseEntered atLocation:pointInWindow]];
}

- (void)mouseDragToPoint:(NSPoint)pointInWindow
{
    [self mouseDragged:[self _mouseEventWithType:NSEventTypeLeftMouseDragged atLocation:pointInWindow]];
}

- (NSEvent *)_mouseEventWithType:(NSEventType)type atLocation:(NSPoint)pointInWindow
{
    return [self _mouseEventWithType:type atLocation:pointInWindow flags:0 timestamp:GetCurrentEventTime() clickCount:0];
}

- (NSEvent *)_mouseEventWithType:(NSEventType)type atLocation:(NSPoint)locationInWindow flags:(NSEventModifierFlags)flags timestamp:(NSTimeInterval)timestamp clickCount:(NSUInteger)clickCount
{
    switch (type) {
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
        return [NSEvent enterExitEventWithType:type location:locationInWindow modifierFlags:flags timestamp:timestamp windowNumber:[_hostWindow windowNumber] context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber trackingNumber:1 userData:nil];
    default:
        return [NSEvent mouseEventWithType:type location:locationInWindow modifierFlags:flags timestamp:timestamp windowNumber:[_hostWindow windowNumber] context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:clickCount pressure:0];
    }
}

- (NSWindow *)hostWindow
{
    return _hostWindow.get();
}

- (void)typeCharacter:(char)character
{
    NSString *characterAsString = [NSString stringWithFormat:@"%c" , character];
    NSEventType keyDownEventType = NSEventTypeKeyDown;
    NSEventType keyUpEventType = NSEventTypeKeyUp;
    [self keyDown:[NSEvent keyEventWithType:keyDownEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:[_hostWindow windowNumber] context:nil characters:characterAsString charactersIgnoringModifiers:characterAsString isARepeat:NO keyCode:character]];
    [self keyUp:[NSEvent keyEventWithType:keyUpEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:[_hostWindow windowNumber] context:nil characters:characterAsString charactersIgnoringModifiers:characterAsString isARepeat:NO keyCode:character]];
}

@end
#endif

#endif // WK_API_ENABLED
