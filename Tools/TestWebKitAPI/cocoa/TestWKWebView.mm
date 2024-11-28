/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#import "ClassMethodSwizzler.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"

#import <WebKit/WKContentWorld.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKTextInputContext.h>
#import <objc/runtime.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Deque.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <wtf/SoftLinking.h>
SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIWindow)

#if USE(BROWSERENGINEKIT)
// FIXME: This workaround can be removed once the fix for rdar://120390585 lands in the SDK.
SOFT_LINK_CLASS(UIKit, UIKeyEvent)
#endif

static NSString *overrideBundleIdentifier(id, SEL)
{
    return @"com.apple.TestWebKitAPI";
}
#endif // PLATFORM(IOS_FAMILY)

@interface WKWebView (TextServices)
- (void)_lookup:(id)sender;
@end

@implementation WKWebView (TestWebKitAPI)

- (void)loadTestPageNamed:(NSString *)pageName
{
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:pageName withExtension:@"html"]];
    [self loadRequest:request];
}

- (void)synchronouslyGoBack
{
    [self goBack];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyGoForward
{
    [self goForward];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadRequest:(NSURLRequest *)request
{
    [self loadRequest:request];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadRequest:(NSURLRequest *)request preferences:(WKWebpagePreferences *)preferences
{
    [self loadRequest:request];
    [self _test_waitForDidFinishNavigationWithPreferences:preferences];
}

- (void)synchronouslyLoadSimulatedRequest:(NSURLRequest *)request responseHTMLString:(NSString *)htmlString
{
    [self loadSimulatedRequest:request responseHTMLString:htmlString];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadRequestIgnoringSSLErrors:(NSURLRequest *)request
{
    [self loadRequest:request];
    [self _test_waitForDidFinishNavigationWhileIgnoringSSLErrors];
}

- (void)synchronouslyLoadHTMLString:(NSString *)html baseURL:(NSURL *)url
{
    [self loadHTMLString:html baseURL:url];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadHTMLString:(NSString *)html
{
    [self synchronouslyLoadHTMLString:html baseURL:NSBundle.test_resourcesBundle.resourceURL];
}

- (void)synchronouslyLoadHTMLString:(NSString *)html preferences:(WKWebpagePreferences *)preferences
{
    [self loadHTMLString:html baseURL:NSBundle.test_resourcesBundle.resourceURL];
    [self _test_waitForDidFinishNavigationWithPreferences:preferences];
}

- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName
{
    [self loadTestPageNamed:pageName];
    [self _test_waitForDidFinishNavigation];
}

- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName asStringWithBaseURL:(NSURL *)url
{
    [self synchronouslyLoadHTMLString:[NSString stringWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:pageName withExtension:@"html"] encoding:NSUTF8StringEncoding error:nil] baseURL:url];
}

- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName preferences:(WKWebpagePreferences *)preferences
{
    [self loadTestPageNamed:pageName];
    [self _test_waitForDidFinishNavigationWithPreferences:preferences];
}

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

#if PLATFORM(IOS_FAMILY)

- (UIView <UITextInputPrivate, UITextInputMultiDocument> *)textInputContentView
{
    return (UIView <UITextInputPrivate, UITextInputMultiDocument> *)[self valueForKey:@"_currentContentView"];
}

- (NSArray<_WKTextInputContext *> *)synchronouslyRequestTextInputContextsInRect:(CGRect)rect
{
    __block bool finished = false;
    __block RetainPtr<NSArray<_WKTextInputContext *>> result;
    [self _requestTextInputContextsInRect:rect completionHandler:^(NSArray<_WKTextInputContext *> *contexts) {
        result = contexts;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (void)defineSelection
{
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput) {
        [self lookup:nil];
        return;
    }
#endif
    [self _lookup:nil];
}

- (void)shareSelection
{
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput) {
        [self share:nil];
        return;
    }
#endif
    [self _share:nil];
}

- (BOOL)hasAsyncTextInput
{
#if USE(BROWSERENGINEKIT)
    return !!self.asyncTextInput;
#else
    return NO;
#endif
}

- (CGRect)selectionClipRect
{
#if USE(BROWSERENGINEKIT)
    if (id<BETextInput> asyncTextInput = self.asyncTextInput)
        return asyncTextInput.selectionClipRect;
#endif
    return self.textInputContentView._selectionClipRect;
}

- (void)moveSelectionToStartOfParagraph
{
#if USE(BROWSERENGINEKIT)
    if (id<BETextInput> asyncTextInput = self.asyncTextInput) {
        [asyncTextInput moveInStorageDirection:UITextStorageDirectionBackward byGranularity:UITextGranularityParagraph];
        return;
    }
#endif
    [self.textInputContentView _moveToStartOfParagraph:NO withHistory:nil];
}

- (void)extendSelectionToStartOfParagraph
{
#if USE(BROWSERENGINEKIT)
    if (id<BETextInput> asyncTextInput = self.asyncTextInput) {
        [asyncTextInput extendInStorageDirection:UITextStorageDirectionBackward byGranularity:UITextGranularityParagraph];
        return;
    }
#endif
    [self.textInputContentView _moveToStartOfParagraph:YES withHistory:nil];
}

- (void)moveSelectionToEndOfParagraph
{
#if USE(BROWSERENGINEKIT)
    if (id<BETextInput> asyncTextInput = self.asyncTextInput) {
        [asyncTextInput moveInStorageDirection:UITextStorageDirectionForward byGranularity:UITextGranularityParagraph];
        return;
    }
#endif
    [self.textInputContentView _moveToEndOfParagraph:NO withHistory:nil];
}

- (void)extendSelectionToEndOfParagraph
{
#if USE(BROWSERENGINEKIT)
    if (id<BETextInput> asyncTextInput = self.asyncTextInput) {
        [asyncTextInput extendInStorageDirection:UITextStorageDirectionForward byGranularity:UITextGranularityParagraph];
        return;
    }
#endif
    [self.textInputContentView _moveToEndOfParagraph:YES withHistory:nil];
}

- (void)insertTextSuggestion:(UITextSuggestion *)textSuggestion
{
#if USE(BROWSERENGINEKIT)
    if (id<BETextInput> asyncTextInput = self.asyncTextInput) {
        RetainPtr beTextSuggestion = adoptNS([[BETextSuggestion alloc] _initWithUIKitTextSuggestion:textSuggestion]);
        [asyncTextInput insertTextSuggestion:beTextSuggestion.get()];
        return;
    }
#endif
    [self.textInputContentView insertTextSuggestion:textSuggestion];
}

#if USE(BROWSERENGINEKIT)

- (id<BETextInput>)asyncTextInput
{
    static BOOL conformsToAsyncTextInput = class_conformsToProtocol(NSClassFromString(@"WKContentView"), @protocol(BETextInput));
    if (!conformsToAsyncTextInput)
        return nil;

    return (id<BETextInput>)self.textInputContentView;
}

- (id<BEExtendedTextInputTraits>)extendedTextInputTraits
{
    return self.asyncTextInput.extendedTextInputTraits;
}

#endif // USE(BROWSERENGINEKIT)

- (std::pair<CGRect, CGRect>)autocorrectionRectsForString:(NSString *)string
{
    std::pair<CGRect, CGRect> result;
    bool done = false;
#if USE(BROWSERENGINEKIT)
    if (auto asyncTextInput = self.asyncTextInput) {
        [asyncTextInput requestTextRectsForString:string withCompletionHandler:[&](NSArray<UITextSelectionRect *> *rects) {
            result = { rects.firstObject.rect, rects.lastObject.rect };
            done = true;
        }];
    } else
#endif
    {
        [self.textInputContentView requestAutocorrectionRectsForString:string withCompletionHandler:[&](UIWKAutocorrectionRects *rects) {
            result = { rects.firstRect, rects.lastRect };
            done = true;
        }];
    }
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (TestWebKitAPI::AutocorrectionContext)autocorrectionContext
{
    TestWebKitAPI::AutocorrectionContext result;
    bool done = false;
#if USE(BROWSERENGINEKIT)
    if (auto asyncTextInput = self.asyncTextInput) {
        [asyncTextInput requestTextContextForAutocorrectionWithCompletionHandler:[&](WKBETextDocumentContext *context) {
            auto uiContext = dynamic_objc_cast<UIWKDocumentContext>(context);
            if (auto seContext = dynamic_objc_cast<WKBETextDocumentContext>(context))
                uiContext = seContext._uikitDocumentContext;
            result = {
                dynamic_objc_cast<NSString>(uiContext.contextBefore),
                dynamic_objc_cast<NSString>(uiContext.selectedText),
                dynamic_objc_cast<NSString>(uiContext.contextAfter),
                dynamic_objc_cast<NSString>(uiContext.markedText),
                uiContext.selectedRangeInMarkedText,
            };
            done = true;
        }];
    } else
#endif // USE(BROWSERENGINEKIT)
    {
        [self.textInputContentView requestAutocorrectionContextWithCompletionHandler:[&](UIWKAutocorrectionContext *context) {
            result = { context.contextBeforeSelection, context.selectedText, context.contextAfterSelection, context.markedText, context.rangeInMarkedText };
            done = true;
        }];
    }
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (id<UITextInputTraits_Private>)effectiveTextInputTraits
{
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput)
        return static_cast<id<UITextInputTraits_Private>>(self.extendedTextInputTraits);
#endif
    return static_cast<id<UITextInputTraits_Private>>(self.textInputContentView.textInputTraits);
}

- (void)replaceText:(NSString *)input withText:(NSString *)correction shouldUnderline:(BOOL)shouldUnderline completion:(void(^)())completion
{
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput) {
        auto options = shouldUnderline ? BETextReplacementOptionsAddUnderline : BETextReplacementOptionsNone;
        [self.asyncTextInput replaceText:input withText:correction options:options completionHandler:[completion = makeBlockPtr(completion)](NSArray<UITextSelectionRect *> *) {
            completion();
        }];
        return;
    }
#endif

    [self.textInputContentView applyAutocorrection:correction toString:input shouldUnderline:shouldUnderline withCompletionHandler:[completion = makeBlockPtr(completion)](UIWKAutocorrectionRects *) {
        completion();
    }];
}

- (void)insertText:(NSString *)primaryString alternatives:(NSArray<NSString *> *)alternativeStrings
{
    if (!alternativeStrings.count) {
        [self.textInputContentView insertText:primaryString];
        return;
    }

#if USE(BROWSERENGINEKIT)
    auto nsAlternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:primaryString alternativeStrings:alternativeStrings]);
    auto alternatives = adoptNS([[BETextAlternatives alloc] _initWithNSTextAlternatives:nsAlternatives.get()]);
    [self.asyncTextInput insertTextAlternatives:alternatives.get()];
#else
    [self.textInputContentView insertText:primaryString alternatives:alternativeStrings style:UITextAlternativeStyleNone];
#endif
}

#if USE(BROWSERENGINEKIT)

static RetainPtr<BEKeyEntry> wrap(WebEvent *webEvent)
{
    auto uiKeyEvent = adoptNS([allocUIKeyEventInstance() initWithWebEvent:webEvent]);
    return adoptNS([[BEKeyEntry alloc] _initWithUIKitKeyEvent:uiKeyEvent.get()]);
}

static WebEvent *unwrap(BEKeyEntry *event)
{
    auto uiEvent = retainPtr(event._uikitKeyEvent);
    return [uiEvent webEvent];
}

#endif // USE(BROWSERENGINEKIT)

#if HAVE(UI_WK_DOCUMENT_CONTEXT)

- (void)synchronouslyAdjustSelectionWithDelta:(NSRange)range
{
    __block bool finished = false;
    auto completion = ^{
        finished = true;
    };
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput) {
        BEDirectionalTextRange directionalRange {
            static_cast<NSInteger>(range.location),
            static_cast<NSInteger>(range.length)
        };
        [self.asyncTextInput adjustSelectionByRange:directionalRange completionHandler:completion];
    } else
#endif
        [self.textInputContentView adjustSelectionWithDelta:range completionHandler:completion];
    TestWebKitAPI::Util::run(&finished);
}

#endif // HAVE(UI_WK_DOCUMENT_CONTEXT)

- (void)selectTextForContextMenuWithLocationInView:(CGPoint)locationInView completion:(void(^)(BOOL shouldPresent))completion
{
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput) {
        [self.asyncTextInput selectTextForEditMenuWithLocationInView:locationInView completionHandler:[completion = makeBlockPtr(completion)](BOOL shouldPresent, NSString *, NSRange) {
            completion(shouldPresent);
        }];
        return;
    }
#endif
    [self.textInputContentView prepareSelectionForContextMenuWithLocationInView:locationInView completionHandler:[completion = makeBlockPtr(completion)](BOOL shouldPresent, RVItem *) {
        completion(shouldPresent);
    }];
}

- (void)handleKeyEvent:(WebEvent *)event completion:(void (^)(WebEvent *theEvent, BOOL handled))completion
{
#if USE(BROWSERENGINEKIT)
    if (self.hasAsyncTextInput) {
        [self.asyncTextInput handleKeyEntry:wrap(event).get() withCompletionHandler:[completion = makeBlockPtr(completion)](BEKeyEntry *event, BOOL handled) {
            completion(unwrap(event), handled);
        }];
        return;
    }
#endif
    [self.textInputContentView handleKeyWebEvent:event withCompletionHandler:completion];
}

#endif // PLATFORM(IOS_FAMILY)

- (NSUInteger)gpuToWebProcessConnectionCount
{
    __block bool done = false;
    __block NSUInteger count = 0;
    [self _gpuToWebProcessConnectionCountForTesting:^(NSUInteger result) {
        done = true;
        count = result;
    }];
    TestWebKitAPI::Util::run(&done);
    return count;
}

- (NSString *)contentsAsString
{
    __block bool done = false;
    __block RetainPtr<NSString> result;
    [self _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        result = contents;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (NSData *)contentsAsWebArchive
{
    __block bool done = false;
    __block RetainPtr<NSData> result;
    [self createWebArchiveDataWithCompletionHandler:^(NSData *contents, NSError *error) {
        result = contents;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (NSArray<NSString *> *)tagsInBody
{
    return [self objectByEvaluatingJavaScript:@"Array.from(document.body.getElementsByTagName('*')).map(e => e.tagName)"];
}

- (NSString *)selectedText
{
    return [self stringByEvaluatingJavaScript:@"getSelection().toString()"];
}

- (void)expectElementTagsInOrder:(NSArray<NSString *> *)tagNames
{
    auto remainingTags = adoptNS([tagNames mutableCopy]);
    NSArray<NSString *> *tagsInBody = self.tagsInBody;
    for (NSString *tag in tagsInBody.reverseObjectEnumerator) {
        if ([tag isEqualToString:[remainingTags lastObject]])
            [remainingTags removeLastObject];
        if (![remainingTags count])
            break;
    }
    EXPECT_EQ([remainingTags count], 0U);
    if ([remainingTags count])
        NSLog(@"Expected to find ordered tags: %@ in: %@", tagNames, tagsInBody);
}

- (void)expectElementCount:(NSInteger)count querySelector:(NSString *)querySelector
{
    NSString *script = [NSString stringWithFormat:@"document.querySelectorAll('%@').length", querySelector];
    EXPECT_EQ(count, [self stringByEvaluatingJavaScript:script].integerValue);
}

- (void)expectElementTag:(NSString *)tagName toComeBefore:(NSString *)otherTagName
{
    [self expectElementTagsInOrder:@[tagName, otherTagName]];
}

- (BOOL)evaluateMediaQuery:(NSString *)query
{
    return [[self objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.matchMedia(\"(%@)\").matches", query]] boolValue];
}

- (id)objectByEvaluatingJavaScript:(NSString *)script
{
    bool callbackComplete = false;
    RetainPtr<id> evalResult;
    [self _evaluateJavaScriptWithoutUserGesture:script completionHandler:[&] (id result, NSError *error) {
        evalResult = result;
        callbackComplete = true;
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error: %@ while evaluating script: %@", error, script);
    }];
    TestWebKitAPI::Util::run(&callbackComplete);
    return evalResult.autorelease();
}

- (id)objectByEvaluatingJavaScript:(NSString *)script inFrame:(WKFrameInfo *)frame
{
    bool callbackComplete = false;
    RetainPtr<id> evalResult;
    [self _evaluateJavaScript:script withSourceURL:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld withUserGesture:NO completionHandler:[&](id result, NSError *error) {
        evalResult = result;
        callbackComplete = true;
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error: %@ while evaluating script: %@", error, script);
    }];
    TestWebKitAPI::Util::run(&callbackComplete);
    return evalResult.autorelease();
}

- (id)objectByEvaluatingJavaScriptWithUserGesture:(NSString *)script
{
    bool callbackComplete = false;
    RetainPtr<id> evalResult;
    [self evaluateJavaScript:script completionHandler:[&] (id result, NSError *error) {
        evalResult = result;
        callbackComplete = true;
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error: %@ while evaluating script: %@", error, script);
    }];
    TestWebKitAPI::Util::run(&callbackComplete);
    return evalResult.autorelease();
}

- (id)objectByCallingAsyncFunction:(NSString *)script withArguments:(NSDictionary *)arguments error:(NSError **)errorOut
{
    bool callbackComplete = false;
    if (errorOut)
        *errorOut = nil;

    RetainPtr<id> evalResult;
    RetainPtr<NSError> strongError;
    [self callAsyncJavaScript:script arguments:arguments inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        evalResult = result;
        strongError = error;
        callbackComplete = true;
    }];
    TestWebKitAPI::Util::run(&callbackComplete);

    if (errorOut)
        *errorOut = strongError.autorelease();

    return evalResult.autorelease();
}

- (NSString *)stringByEvaluatingJavaScript:(NSString *)script inFrame:(WKFrameInfo *)frame
{
    return [NSString stringWithFormat:@"%@", [self objectByEvaluatingJavaScript:script inFrame:frame]];
}

- (NSString *)stringByEvaluatingJavaScript:(NSString *)script
{
    return [NSString stringWithFormat:@"%@", [self objectByEvaluatingJavaScript:script]];
}

- (unsigned)waitUntilClientWidthIs:(unsigned)expectedClientWidth
{
    int timeout = 10;
    unsigned clientWidth = 0;
    do {
        if (timeout != 10)
            TestWebKitAPI::Util::runFor(0.1_s);

        id result = [self objectByEvaluatingJavaScript:@"function ___forceLayoutAndGetClientWidth___() { document.body.offsetTop; return document.body.clientWidth; }; ___forceLayoutAndGetClientWidth___();"];
        clientWidth = [result integerValue];

        --timeout;
    } while (clientWidth != expectedClientWidth && timeout >= 0);

    return clientWidth;
}

- (CGRect)elementRectFromSelector:(NSString *)selector
{
    __block CGRect rect;
    __block bool doneEvaluatingScript = false;
    auto script = [NSString stringWithFormat:@"r = document.querySelector('%@').getBoundingClientRect(); [r.left, r.top, r.width, r.height]", selector];
    [self evaluateJavaScript:script completionHandler:^(NSArray<NSNumber *> *result, NSError *error) {
        if (error)
            NSLog(@"Error while getting element rect for '%@': %@", selector, error);
        EXPECT_NULL(error);
        rect = CGRectMake(result[0].floatValue, result[1].floatValue, result[2].floatValue, result[3].floatValue);
        doneEvaluatingScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingScript);
    return rect;
}

- (CGPoint)elementMidpointFromSelector:(NSString *)selector
{
    auto rect = [self elementRectFromSelector:selector];
    return CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
}

- (CGImageRef)snapshotAfterScreenUpdates
{
    __block RetainPtr<CGImage> result;
    __block bool done = false;
    RetainPtr configuration = adoptNS([WKSnapshotConfiguration new]);
    [configuration setAfterScreenUpdates:YES];
    [self takeSnapshotWithConfiguration:configuration.get() completionHandler:^(TestWebKitAPI::Util::PlatformImage *snapshot, NSError *) {
        result = TestWebKitAPI::Util::convertToCGImage(snapshot);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

@implementation TestMessageHandler {
    NSMutableDictionary<NSString *, dispatch_block_t> *_messageHandlers;
}

- (void)addMessage:(NSString *)message withHandler:(dispatch_block_t)handler
{
    if (!_messageHandlers)
        _messageHandlers = [NSMutableDictionary dictionary];

    _messageHandlers[message] = adoptNS([handler copy]).autorelease();
}

- (void)removeMessage:(NSString *)message
{
    [_messageHandlers removeObjectForKey:message];
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (dispatch_block_t handler = _messageHandlers[message.body])
        handler();
    if (_didReceiveScriptMessage)
        _didReceiveScriptMessage(message.body);
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
    __weak TestWKWebView *_webView;
}

#if PLATFORM(MAC)

static int gEventNumber = 1;

NSEventMask __simulated_forceClickAssociatedEventsMask(id self, SEL _cmd)
{
    return NSEventMaskPressure | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView contentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)style backing:(NSBackingStoreType)backingStoreType defer:(BOOL)flag
{
    if (self = [super initWithContentRect:contentRect styleMask:style backing:backingStoreType defer:flag])
        _webView = webView;
    return self;
}

- (void)_mouseDownAtPoint:(NSPoint)point simulatePressure:(BOOL)simulatePressure clickCount:(NSUInteger)clickCount modifierFlags:(NSEventModifierFlags)modifierFlags mouseEventType:(NSEventType)mouseEventType
{
    if (simulatePressure)
        modifierFlags |= NSEventMaskPressure;

    NSEvent *event = [NSEvent mouseEventWithType:mouseEventType location:point modifierFlags:modifierFlags timestamp:_webView.eventTimestamp windowNumber:self.windowNumber context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:clickCount pressure:simulatePressure];
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

- (void)_mouseUpAtPoint:(NSPoint)point clickCount:(NSUInteger)clickCount modifierFlags:(NSEventModifierFlags)modifierFlags eventType:(NSEventType)eventType
{
    [self sendEvent:[NSEvent mouseEventWithType:eventType location:point modifierFlags:modifierFlags timestamp:_webView.eventTimestamp windowNumber:self.windowNumber context:[NSGraphicsContext currentContext] eventNumber:++gEventNumber clickCount:clickCount pressure:0]];
}

- (BOOL)canBecomeKeyWindow
{
    return _webView.forceWindowToBecomeKey || super.canBecomeKeyWindow;
}

#endif

- (BOOL)isKeyWindow
{
    return _forceKeyWindow || [super isKeyWindow];
}

#if PLATFORM(IOS_FAMILY)

static NeverDestroyed<RetainPtr<UIWindow>> gOverriddenApplicationKeyWindow;
static NeverDestroyed<std::unique_ptr<InstanceMethodSwizzler>> gApplicationKeyWindowSwizzler;
static NeverDestroyed<std::unique_ptr<InstanceMethodSwizzler>> gSharedApplicationSwizzler;

static void setOverriddenApplicationKeyWindow(UIWindow *window)
{
    if (gOverriddenApplicationKeyWindow.get() == window)
        return;

    if (!UIApplication.sharedApplication) {
        InstanceMethodSwizzler bundleIdentifierSwizzler(NSBundle.class, @selector(bundleIdentifier), reinterpret_cast<IMP>(overrideBundleIdentifier));
        UIApplicationInitialize();
        UIApplicationInstantiateSingleton(UIApplication.class);
    }

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gApplicationKeyWindowSwizzler.get() = makeUnique<InstanceMethodSwizzler>(UIApplication.class, @selector(keyWindow), reinterpret_cast<IMP>(applicationKeyWindowOverride));
    });
    gOverriddenApplicationKeyWindow.get() = window;
}

static UIWindow *applicationKeyWindowOverride(id, SEL)
{
    return gOverriddenApplicationKeyWindow.get().get();
}

- (instancetype)initWithWebView:(TestWKWebView *)webView frame:(CGRect)frame
{
    if (self = [super initWithFrame:frame])
        _webView = webView;
    return self;
}

#endif

- (void)makeKeyWindow
{
    if (_forceKeyWindow)
        return;

    _forceKeyWindow = YES;
#if PLATFORM(MAC)
    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidBecomeKeyNotification object:self];
#else
    setOverriddenApplicationKeyWindow(self);
    [[NSNotificationCenter defaultCenter] postNotificationName:UIWindowDidBecomeKeyNotification object:self];
#endif
}

- (void)resignKeyWindow
{
    _forceKeyWindow = NO;
    [super resignKeyWindow];
}

@end

#if PLATFORM(IOS_FAMILY)

using InputSessionChangeCount = NSUInteger;
static InputSessionChangeCount nextInputSessionChangeCount()
{
    static InputSessionChangeCount gInputSessionChangeCount = 0;
    return ++gInputSessionChangeCount;
}

#endif

@implementation TestWKWebView {
    RetainPtr<TestWKWebViewHostWindow> _hostWindow;
    RetainPtr<TestMessageHandler> _testHandler;
    RetainPtr<WKUserScript> _onloadScript;
#if PLATFORM(IOS_FAMILY)
    InputSessionChangeCount _inputSessionChangeCount;
    UIEdgeInsets _overrideSafeAreaInset;
#endif
#if PLATFORM(MAC)
    BOOL _forceWindowToBecomeKey;
    NSTimeInterval _eventTimestampOffset;
#endif
}

- (instancetype)initWithFrame:(CGRect)frame
{
    auto defaultConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    return [self initWithFrame:frame configuration:defaultConfiguration.get()];
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

#if PLATFORM(IOS_FAMILY)
    _inputSessionChangeCount = 0;
    // We suppress safe area insets by default in order to ensure consistent results when running against device models
    // that may or may not have safe area insets, have insets with different values (e.g. iOS devices with a notch).
    _overrideSafeAreaInset = UIEdgeInsetsZero;
#endif

    return self;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration processPoolConfiguration:(_WKProcessPoolConfiguration *)processPoolConfiguration
{
    [configuration setProcessPool:adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration]).get()];
    return [self initWithFrame:frame configuration:configuration];
}

- (void)_setUpTestWindow:(NSRect)frame
{
#if PLATFORM(MAC)
    _hostWindow = adoptNS([[TestWKWebViewHostWindow alloc] initWithWebView:self contentRect:frame styleMask:(NSWindowStyleMaskBorderless | NSWindowStyleMaskMiniaturizable) backing:NSBackingStoreBuffered defer:NO]);
    [_hostWindow setHasShadow:NO];
    [_hostWindow setFrameOrigin:frame.origin];
    [_hostWindow setIsVisible:YES];
    [_hostWindow contentView].wantsLayer = YES;
    [[_hostWindow contentView] addSubview:self];
    [_hostWindow makeKeyAndOrderFront:self];
#else
    _hostWindow = adoptNS([[TestWKWebViewHostWindow alloc] initWithWebView:self frame:frame]);
    [_hostWindow setHidden:NO];
    [_hostWindow addSubview:self];
#endif
}

- (void)addToTestWindow
{
    if (!_hostWindow) {
        [self _setUpTestWindow:self.frame];
        return;
    }

#if PLATFORM(MAC)
    [[_hostWindow contentView] addSubview:self];
#else
    [_hostWindow addSubview:self];
#endif
}

- (void)removeFromTestWindow
{
    if (_hostWindow)
        [self removeFromSuperview];
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

- (void)performAfterReceivingAnyMessage:(void (^)(NSString *))action
{
    if (!_testHandler) {
        _testHandler = adoptNS([[TestMessageHandler alloc] init]);
        [[[self configuration] userContentController] addScriptMessageHandler:_testHandler.get() name:@"testHandler"];
    }
    [_testHandler setDidReceiveScriptMessage:action];
}

- (void)synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:(NSString *)html
{
    bool didFireDOMLoadEvent = false;
    [self performAfterLoading:[&] { didFireDOMLoadEvent = true; }];
    [self loadHTMLString:html baseURL:NSBundle.test_resourcesBundle.resourceURL];
    TestWebKitAPI::Util::run(&didFireDOMLoadEvent);
    [self waitForNextPresentationUpdate];
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

- (void)waitForMessages:(NSArray<NSString *> *)expectedMessages
{
    __block Deque<RetainPtr<NSString>> receivedMessages;
    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    [messageHandler setDidReceiveScriptMessage:^(NSString *message) {
        receivedMessages.append(message);
    }];
    [self.configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    for (NSString *expectedMessage in expectedMessages) {
        while (receivedMessages.isEmpty())
            TestWebKitAPI::Util::spinRunLoop();
        EXPECT_WK_STREQ(receivedMessages.takeFirst().get(), expectedMessage);
    }
    [self.configuration.userContentController removeScriptMessageHandlerForName:@"testHandler"];
}

- (void)performAfterLoading:(dispatch_block_t)actions
{
    NSString *const viewDidLoadMessage = @"TestWKWebViewDidLoad";
    if (!_onloadScript) {
        NSString *onloadScript = [NSString stringWithFormat:@"window.addEventListener('load', () => window.webkit.messageHandlers.testHandler.postMessage('%@'), true /* useCapture */)", viewDidLoadMessage];
        _onloadScript = adoptNS([[WKUserScript alloc] initWithSource:onloadScript injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
        [self.configuration.userContentController addUserScript:_onloadScript.get()];
    }
    [self performAfterReceivingMessage:viewDidLoadMessage action:actions];
}

- (void)waitForNextPresentationUpdate
{
    __block bool done = false;
    [self _doAfterNextPresentationUpdate:^{
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

- (void)waitForNextVisibleContentRectUpdate
{
    __block bool done = false;
    [self _doAfterNextVisibleContentRectUpdate:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)waitUntilActivityStateUpdateDone
{
    __block bool done = false;
    [self _doAfterActivityStateUpdate:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

- (void)forceDarkMode
{
#if HAVE(OS_DARK_MODE_SUPPORT)
#if USE(APPKIT)
    [self setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
#else
    [self setOverrideUserInterfaceStyle:UIUserInterfaceStyleDark];
#endif
#endif
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

- (BOOL)selectionRangeHasStartOffset:(int)start endOffset:(int)end
{
    __block bool isDone = false;
    __block bool matches = true;
    [self evaluateJavaScript:@"window.getSelection().getRangeAt(0).startOffset" completionHandler:^(id result, NSError *error) {
        if ([(NSNumber *)result intValue] != start)
            matches = false;
    }];
    [self evaluateJavaScript:@"window.getSelection().getRangeAt(0).endOffset" completionHandler:^(id result, NSError *error) {
        if ([(NSNumber *)result intValue] != end)
            matches = false;
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    return matches;
}

- (BOOL)selectionRangeHasStartOffset:(int)start endOffset:(int)end inFrame:(WKFrameInfo *)frameInfo
{
    __block bool isDone = false;
    __block bool matches = true;
    [self evaluateJavaScript:@"window.getSelection().getRangeAt(0).startOffset" inFrame:frameInfo inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        if ([(NSNumber *)result intValue] != start)
            matches = false;
    }];
    [self evaluateJavaScript:@"window.getSelection().getRangeAt(0).endOffset" inFrame:frameInfo inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        if ([(NSNumber *)result intValue] != end)
            matches = false;
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    return matches;
}

- (void)clickOnElementID:(NSString *)elementID
{
    [self evaluateJavaScript:[NSString stringWithFormat:@"document.getElementById('%@').click();", elementID] completionHandler:nil];
}

- (void)waitForPendingMouseEvents
{
    __block bool doneProcessingMouseEvents = false;
    [self _doAfterProcessingAllPendingMouseEvents:^{
        doneProcessingMouseEvents = true;
    }];
    TestWebKitAPI::Util::run(&doneProcessingMouseEvents);
}

- (void)focus
{
#if PLATFORM(MAC)
    [_hostWindow makeFirstResponder:self];
#else
    [super becomeFirstResponder];
#endif
}

- (std::optional<CGPoint>)getElementMidpoint:(NSString *)selector
{
    NSArray<NSNumber *> *midpoint = [self objectByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => {"
        "    let element = document.querySelector('%@');"
        "    if (!element)"
        "        return [];"
        "    const rect = element.getBoundingClientRect();"
        "    return [rect.left + (rect.width / 2), rect.top + (rect.height / 2)];"
        "})()", selector]];
    if (midpoint.count != 2)
        return std::nullopt;
    return CGPointMake(midpoint.firstObject.doubleValue, midpoint.lastObject.doubleValue);
}

#if PLATFORM(IOS_FAMILY)

- (void)didStartFormControlInteraction
{
    _inputSessionChangeCount = nextInputSessionChangeCount();
}

- (void)didEndFormControlInteraction
{
    _inputSessionChangeCount = 0;
}

#endif // PLATFORM(IOS_FAMILY)

@end

#if PLATFORM(IOS_FAMILY)

@implementation TestWKWebView (IOSOnly)

- (void)evaluateJavaScriptAndWaitForInputSessionToChange:(NSString *)script
{
    auto initialChangeCount = _inputSessionChangeCount;
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];

    [self objectByEvaluatingJavaScript:script];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if (_inputSessionChangeCount != initialChangeCount)
            break;

        if (hasEmittedWarning || startTime + secondsToWaitUntilWarning >= [NSDate timeIntervalSinceReferenceDate])
            continue;

        NSLog(@"Warning: expecting input session change count to differ from %lu", static_cast<unsigned long>(initialChangeCount));
        hasEmittedWarning = YES;
    }
}

- (UIEdgeInsets)overrideSafeAreaInset
{
    return _overrideSafeAreaInset;
}

- (void)setOverrideSafeAreaInset:(UIEdgeInsets)inset
{
    _overrideSafeAreaInset = inset;
}

- (UIEdgeInsets)_safeAreaInsetsForFrame:(CGRect)frame inSuperview:(UIView *)view
{
    return _overrideSafeAreaInset;
}

- (CGRect)caretViewRectInContentCoordinates
{
    UIView *caretView = nil;
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (auto view = self.textSelectionDisplayInteraction.cursorView; !view.hidden)
        caretView = view;
#endif

    return [caretView convertRect:caretView.bounds toView:self.textInputContentView];
}

- (NSArray<NSValue *> *)selectionViewRectsInContentCoordinates
{
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    RetainPtr contentView = [self textInputContentView];
    if (auto view = self.textSelectionDisplayInteraction.highlightView; !view.hidden) {
        RetainPtr uiTextSelectionRects = [view selectionRects];
        NSMutableArray *selectionRects = [NSMutableArray arrayWithCapacity:[uiTextSelectionRects count]];
        for (UITextSelectionRect *rect in uiTextSelectionRects.get()) {
            CGRect rectInContentView = [view convertRect:rect.rect toView:contentView.get()];
            [selectionRects addObject:[NSValue valueWithCGRect:rectInContentView]];
        }
        return selectionRects;
    }
#endif
    return @[ ];
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

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

- (UITextSelectionDisplayInteraction *)textSelectionDisplayInteraction
{
    return dynamic_objc_cast<UITextSelectionDisplayInteraction>([self.textInputContentView valueForKeyPath:@"interactionAssistant._selectionViewManager"]);
}

#endif

static WKContentView *recursiveFindWKContentView(UIView *view)
{
    if ([view isKindOfClass:NSClassFromString(@"WKContentView")])
        return (WKContentView *)view;

    for (UIView *subview in view.subviews) {
        WKContentView *contentView = recursiveFindWKContentView(subview);
        if (contentView)
            return contentView;
    }

    return nil;
}

- (WKContentView *)wkContentView
{
    return recursiveFindWKContentView(self);
}

@end

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

@implementation TestWKWebView (MacOnly)

- (void)setEventTimestampOffset:(NSTimeInterval)offset
{
    _eventTimestampOffset += offset;
}

- (NSTimeInterval)eventTimestamp
{
    return GetCurrentEventTime() + _eventTimestampOffset;
}

- (BOOL)forceWindowToBecomeKey
{
    return _forceWindowToBecomeKey;
}

- (void)setForceWindowToBecomeKey:(BOOL)forceWindowToBecomeKey
{
    _forceWindowToBecomeKey = forceWindowToBecomeKey;
}

- (void)mouseDownAtPoint:(NSPoint)pointInWindow simulatePressure:(BOOL)simulatePressure
{
    [self mouseDownAtPoint:pointInWindow simulatePressure:simulatePressure withFlags:0 eventType:NSEventTypeLeftMouseDown];
}

- (void)mouseDownAtPoint:(NSPoint)pointInWindow simulatePressure:(BOOL)simulatePressure withFlags:(NSEventModifierFlags)flags eventType:(NSEventType)eventType
{
    [_hostWindow _mouseDownAtPoint:pointInWindow simulatePressure:simulatePressure clickCount:1 modifierFlags:flags mouseEventType:eventType];
}

- (void)mouseUpAtPoint:(NSPoint)pointInWindow
{
    [self mouseUpAtPoint:pointInWindow withFlags:0 eventType:NSEventTypeLeftMouseUp];
}

- (void)mouseUpAtPoint:(NSPoint)pointInWindow withFlags:(NSEventModifierFlags)flags eventType:(NSEventType)eventType
{
    [_hostWindow _mouseUpAtPoint:pointInWindow clickCount:1 modifierFlags:flags eventType:eventType];
}

- (void)mouseMoveToPoint:(NSPoint)pointInWindow withFlags:(NSEventModifierFlags)flags
{
    [self _simulateMouseMove:[self _mouseEventWithType:NSEventTypeMouseMoved atLocation:pointInWindow flags:flags timestamp:self.eventTimestamp clickCount:0]];
}

- (void)sendClicksAtPoint:(NSPoint)pointInWindow numberOfClicks:(NSUInteger)numberOfClicks
{
    for (NSUInteger clickCount = 1; clickCount <= numberOfClicks; ++clickCount) {
        [_hostWindow _mouseDownAtPoint:pointInWindow simulatePressure:NO clickCount:clickCount modifierFlags:0 mouseEventType:NSEventTypeLeftMouseDown];
        [_hostWindow _mouseUpAtPoint:pointInWindow clickCount:clickCount modifierFlags:0 eventType:NSEventTypeLeftMouseUp];
    }
}

- (void)sendClickAtPoint:(NSPoint)pointInWindow
{
    [self sendClicksAtPoint:pointInWindow numberOfClicks:1];
}

- (void)rightClickAtPoint:(NSPoint)pointInWindow
{
    [self mouseDownAtPoint:pointInWindow simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [self mouseUpAtPoint:pointInWindow withFlags:0 eventType:NSEventTypeRightMouseUp];
}

- (BOOL)acceptsFirstMouseAtPoint:(NSPoint)pointInWindow
{
    return [self acceptsFirstMouse:[self _mouseEventWithType:NSEventTypeLeftMouseDown atLocation:pointInWindow]];
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
    return [self _mouseEventWithType:type atLocation:pointInWindow flags:0 timestamp:self.eventTimestamp clickCount:0];
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

- (void)wheelEventAtPoint:(CGPoint)pointInWindow wheelDelta:(CGSize)delta
{
    RetainPtr<CGEventRef> cgScrollEvent = adoptCF(CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2, delta.height, delta.width, 0));

    CGPoint locationInGlobalScreenCoordinates = [[self window] convertPointToScreen:pointInWindow];
    locationInGlobalScreenCoordinates.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - locationInGlobalScreenCoordinates.y;
    CGEventSetLocation(cgScrollEvent.get(), locationInGlobalScreenCoordinates);
    
    NSEvent* event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];
    [self scrollWheel:event];
}

- (NSWindow *)hostWindow
{
    return _hostWindow.get();
}

- (void)typeCharacter:(char)character
{
    [self typeCharacter:character modifiers:0];
}

- (void)sendKey:(NSString *)characters code:(unsigned short)keyCode isDown:(BOOL)isDown modifiers:(NSEventModifierFlags)modifiers
{
    NSEvent *event = [NSEvent keyEventWithType:isDown ? NSEventTypeKeyDown : NSEventTypeKeyUp
        location:NSZeroPoint
        modifierFlags:modifiers
        timestamp:self.eventTimestamp
        windowNumber:[_hostWindow windowNumber]
        context:nil
        characters:characters
        charactersIgnoringModifiers:characters
        isARepeat:NO
        keyCode:keyCode];

    if (isDown)
        [self keyDown:event];
    else
        [self keyUp:event];
}

- (void)typeCharacter:(char)character modifiers:(NSEventModifierFlags)modifiers
{
    NSString *characters = [NSString stringWithFormat:@"%c", character];
    for (auto isDown : std::array { YES, NO })
        [self sendKey:characters code:character isDown:isDown modifiers:modifiers];
}

// Note: this testing strategy makes a couple of assumptions:
// 1. The network process hasn't already died and allowed the system to reuse the same PID.
// 2. The API test did not take more than ~120 seconds to run.
- (NSArray<NSString *> *)collectLogsForNewConnections
{
    auto predicate = [NSString stringWithFormat:@"subsystem == 'com.apple.network'"
        " AND category == 'connection'"
        " AND eventMessage endswith 'start'"
        " AND processIdentifier == %d", self._networkProcessIdentifier];
    RetainPtr pipe = [NSPipe pipe];
    // FIXME: This is currently reliant on `NSTask`, which is absent on iOS. We should find a way to
    // make this helper work on both platforms.
    auto task = adoptNS([NSTask new]);
    [task setLaunchPath:@"/usr/bin/log"];
    [task setArguments:@[ @"show", @"--last", @"2m", @"--style", @"json", @"--predicate", predicate ]];
    [task setStandardOutput:pipe.get()];
    [task launch];
    [task waitUntilExit];

    auto rawData = [pipe fileHandleForReading].availableData;
    auto messages = [NSMutableArray<NSString *> array];
    for (id messageData in dynamic_objc_cast<NSArray>([NSJSONSerialization JSONObjectWithData:rawData options:0 error:nil]))
        [messages addObject:dynamic_objc_cast<NSString>([messageData objectForKey:@"eventMessage"])];
    return messages;
}

@end

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
@implementation UIView (WKTestingUIViewUtilities)

- (UIView *)wkFirstSubviewWithClass:(Class)targetClass
{
    for (UIView *view in self.subviews) {
        if ([view isKindOfClass:targetClass])
            return view;
    
        UIView *foundSubview = [view wkFirstSubviewWithClass:targetClass];
        if (foundSubview)
            return foundSubview;
    }
    
    return nil;
}

- (UIView *)wkFirstSubviewWithBoundsSize:(CGSize)size
{
    for (UIView *view in self.subviews) {
        if (CGSizeEqualToSize([view bounds].size, size))
            return view;
    
        UIView *foundSubview = [view wkFirstSubviewWithBoundsSize:size];
        if (foundSubview)
            return foundSubview;
    }
    
    return nil;
}

@end

#endif // PLATFORM(IOS_FAMILY)

@implementation TestWKWebView (SiteIsolation)

- (_WKFrameTreeNode *)mainFrame
{
    __block RetainPtr<_WKFrameTreeNode> frame;
    [self _frames:^(_WKFrameTreeNode *mainFrame) {
        frame = mainFrame;
    }];
    while (!frame)
        TestWebKitAPI::Util::spinRunLoop();
    return frame.autorelease();
}

- (WKFrameInfo *)firstChildFrame
{
    return [self mainFrame].childFrames.firstObject.info;
}

- (void)evaluateJavaScript:(NSString *)string inFrame:(WKFrameInfo *)frame completionHandler:(void(^)(id, NSError *))completionHandler
{
    [self evaluateJavaScript:string inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:completionHandler];
}

- (WKFindResult *)findStringAndWait:(NSString *)string withConfiguration:(WKFindConfiguration *)configuration
{
    __block RetainPtr<WKFindResult> findResult;
    [self findString:string withConfiguration:configuration completionHandler:^(WKFindResult *result) {
        findResult = result;
    }];
    while (!findResult)
        TestWebKitAPI::Util::spinRunLoop();
    return findResult.autorelease();
}

@end
