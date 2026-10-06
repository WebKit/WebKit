/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

// Split from SiteIsolation.mm. This file is unified with SiteIsolation.mm
// and its neighbors, so it must not rely on their file-scope helpers or reuse their names.
// Helpers shared between these files live in Helpers/cocoa/SiteIsolationTestUtilities.h.

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "FrameTreeChecks.h"
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/DragAndDropSimulator.h"
#import "Helpers/cocoa/FindInPageUtilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "InstanceMethodSwizzler.h"
#import "TestInputDelegate.h"
#import "UIKitSPIForTesting.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/DOMPasteAccess.h>
#import <WebCore/FrameIdentifier.h>
#import <WebCore/IntRect.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationPrivateForTesting.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKTextInputContext.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>

@interface WKContentView ()
- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point;
- (BOOL)screenIsBeingCaptured;
- (void)_sceneCaptureStateDidChange;
@end

#if HAVE(UIFINDINTERACTION)
// Forward declare UITextSearching methods
@interface WKWebView () <UITextSearching>
- (void)didBeginTextSearchOperation;
- (void)didEndTextSearchOperation;
@end
#endif

@interface UIView (SiteIsolationDictationStreamingOpacity)
- (void)_setDictationStreamingOpacity:(CGFloat)opacity forHypothesisText:(NSString *)hypothesisText streamingRange:(NSRange)streamingRange;
- (void)_clearDictationStreamingOpacity;
@end

// UIWKGestureType stands in for WKBEGestureType, which is BEGestureType with BrowserEngineKit and
// UIWKGestureType without; both are NSInteger-backed, and these handlers ignore the value.
@interface UIView (SiteIsolationPointBasedSelection)
- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState;
- (void)selectPositionAtBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction fromPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler;
@end

namespace TestWebKitAPI {

#if ENABLE(DRAG_SUPPORT) && !PLATFORM(MACCATALYST)

TEST(SiteIsolation, DragAndDrop)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"link-and-target-div" withExtension:@"html"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSArray *registeredTypes = [[simulator sourceItemProviders].firstObject registeredTypeIdentifiers];
    EXPECT_WK_STREQ(UTTypeURL.identifier, [registeredTypes firstObject]);
}

#endif

TEST(SiteIsolation, SelectMultiplePickerLocationInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><select multiple style='margin: 50px; width: 100px; height: 50px; appearance: none; border: none; padding: 0;'><option>A</option><option>B</option></select></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool pickerPresented = false;

    InstanceMethodSwizzler swizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        imp_implementationWithBlock(^(UIViewController *, UIViewController *, BOOL, id) {
            pickerPresented = true;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    RetainPtr hostViewController = adoptNS([[UIViewController alloc] init]);
    [[webView window] setRootViewController:hostViewController.get()];
    [[hostViewController view] addSubview:webView.get()];

    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [webView focusInWindow];
    [webView evaluateJavaScript:@"document.querySelector('select').focus()" inFrame:[webView firstChildFrame] completionHandler:nil];

    Util::run(&pickerPresented);

    // The select element is at (50, 50) in iframe coordinates with size (100, 50).
    // The iframe is at (100, 100) in main frame coordinates (margin: 100px, border: none).
    // After conversion, the focused element's interaction rect should be (150, 150, 100, 50) in main frame coordinates.
    EXPECT_EQ([webView _focusedElementInteractionRect], CGRectMake(150, 150, 100, 50));
}

} // namespace TestWebKitAPI

namespace SiteIsolationDOMPaste {

static CGRect capturedElementRect;
static bool receivedRequest;

static void swizzledRequestDOMPasteAccess(id, SEL,
    WebCore::DOMPasteAccessCategory,
    WebCore::DOMPasteRequiresInteraction,
    WebCore::FrameIdentifier,
    const WebCore::IntRect& elementRect,
    const String&,
    CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    capturedElementRect = CGRectMake(elementRect.x(), elementRect.y(), elementRect.width(), elementRect.height());
    receivedRequest = true;
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

}

namespace TestWebKitAPI {

static ASCIILiteral defaultDOMPasteMainframeHTML = "<body style='margin: 0'><iframe style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;
static ASCIILiteral defaultDOMPasteSubframeHTML = "<!DOCTYPE html><body style='margin: 0'><textarea style='margin: 50px; width: 100px; height: 50px; border: none; padding: 0;'></textarea></body>"_s;
static ASCIILiteral tallDOMPasteSubframeHTML = "<!DOCTYPE html><body style='margin: 0; min-height: 1000px'><textarea style='margin: 50px; width: 100px; height: 50px; border: none; padding: 0;'></textarea></body>"_s;

static CGPoint checkDOMPasteAccessRectInCrossOriginIframe(const String& mainframeHTML, const String& subframeHTML, void (^prepareBeforeFocusing)(TestWKWebView *, WKFrameInfo *) = nil)
{
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/iframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    SiteIsolationDOMPaste::capturedElementRect = CGRectZero;
    SiteIsolationDOMPaste::receivedRequest = false;

    InstanceMethodSwizzler pasteSwizzler {
        NSClassFromString(@"WKContentView"),
        NSSelectorFromString(@"_requestDOMPasteAccessForCategory:requiresInteraction:frameID:elementRect:originIdentifier:completionHandler:"),
        reinterpret_cast<IMP>(SiteIsolationDOMPaste::swizzledRequestDOMPasteAccess)
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrameInfo = [webView firstChildFrame];

    if (prepareBeforeFocusing)
        prepareBeforeFocusing(webView.get(), childFrameInfo.get());

    [webView evaluateJavaScript:@"document.querySelector('textarea').focus(); document.execCommand('paste')" inFrame:childFrameInfo.get() completionHandler:nil];
    Util::run(&SiteIsolationDOMPaste::receivedRequest);

    // Without a real touch event, the reported rect comes from a hit-test at the origin of the
    // subframe's viewport (i.e. the enclosing <body>), converted to main-frame coordinates -- not
    // from the focused <textarea>'s own bounds.
    return SiteIsolationDOMPaste::capturedElementRect.origin;
}

TEST(SiteIsolation, DOMPasteAccessRectInCrossOriginIframe)
{
    CGPoint origin = checkDOMPasteAccessRectInCrossOriginIframe(defaultDOMPasteMainframeHTML, defaultDOMPasteSubframeHTML);
    EXPECT_EQ(origin.x, 100);
    EXPECT_EQ(origin.y, 100);
}

TEST(SiteIsolation, DOMPasteAccessRectInScrolledCrossOriginIframe)
{
    CGPoint origin = checkDOMPasteAccessRectInCrossOriginIframe(defaultDOMPasteMainframeHTML, tallDOMPasteSubframeHTML,
        ^(TestWKWebView *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        });
    EXPECT_EQ(origin.x, 100);
    EXPECT_EQ(origin.y, -400);
}

TEST(SiteIsolation, ApplyAutocorrectionInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable>teh</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Focus the cross-origin iframe so the UI process tracks it as the focused frame, then select the
    // misspelled word inside it.
    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"teh"])
        Util::spinRunLoop();

    // Apply the autocorrection. Under site isolation this IPC must reach the iframe's process; if it
    // is routed to the main frame instead the iframe's content is never corrected.
    __block bool didApplyAutocorrection = false;
    [webView replaceText:@"teh" withText:@"the" shouldUnderline:NO completion:^{
        didApplyAutocorrection = true;
    }];
    Util::run(&didApplyAutocorrection);

    EXPECT_WK_STREQ("the", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);
}

TEST(SiteIsolation, InsertDictatedTextWithAlternativesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // Use the internals-enabled plug-in so window.internals is reachable inside the cross-origin
    // subframe, and point the data store at the test HTTPS proxy (as the media-in-remote-frame tests
    // do), since _test_configurationWithTestPlugInClassName: does not set one up.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    // Allow an input session to begin when the subframe's editable element is focused.
    bool didStartInputSession = false;
    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id<_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    // Focus the cross-origin iframe's contenteditable body with a user gesture (Element::focus() is a
    // no-op for cross-origin non-main-frame iframes without one), then wait until the UI process tracks
    // the subframe as the focused frame and the input session has started.
    RetainPtr childFrame = [webView firstChildFrame];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:childFrame.get()];
    Util::run(&didStartInputSession);
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    // Drive the non-empty dictationAlternatives path of insertDictatedTextAsync. Under site isolation
    // this IPC must reach the iframe's process; if it is routed to the main frame instead (which has no
    // local focused frame) the text is never inserted into the subframe.
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];

    // Primary assertion: the text reaches the subframe (proves cross-process routing).
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"hello"];
    }));
    EXPECT_WK_STREQ("hello", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);

    // Secondary assertion: it went through the alternatives branch specifically, so a dictation
    // alternatives marker was added around the inserted text in the subframe.
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)" inFrame:childFrame.get()] boolValue];
    }));
}

TEST(SiteIsolation, ReplaceDictatedTextInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    bool didStartInputSession = false;
    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id<_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:childFrame.get()];
    Util::run(&didStartInputSession);
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    // Seed the subframe with dictated text, then replace it. replaceDictatedText (iOS) must likewise
    // route to the focused frame's process under site isolation; otherwise the subframe keeps "hello".
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"hello"];
    }));

    [[webView textInputContentView] replaceDictatedText:@"hello" withText:@"world"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"world"];
    }));
    EXPECT_WK_STREQ("world", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);
}

TEST(SiteIsolation, SelectAllAndCopyInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'>main frame text<iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    [webView selectAll:nil];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"subframe text"];
    }));
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    auto changeCountBeforeCopy = [UIPasteboard.generalPasteboard changeCount];
    [webView copy:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [UIPasteboard.generalPasteboard changeCount] != changeCountBeforeCopy;
    }));
    EXPECT_WK_STREQ("subframe text", [UIPasteboard.generalPasteboard string]);
}

// Selection commands are sent to the process containing the focused frame. These tests put the
// selection in a cross-origin iframe and check that each command takes effect there; if the message
// is routed to the main frame's process instead it finds no selection and silently does nothing.

static constexpr auto mainFrameWithCrossOriginIframe = "<body style='margin: 0'>main frame text<iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;

struct SiteIsolatedViewWithFocusedIframe {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<WKFrameInfo> childFrame;
};

static SiteIsolatedViewWithFocusedIframe siteIsolatedViewWithFocusedIframe(const HTTPServer& server)
{
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    return { WTF::move(webView), WTF::move(navigationDelegate), WTF::move(childFrame) };
}

TEST(SiteIsolation, SelectWordInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframe } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = siteIsolatedViewWithFocusedIframe(server);

    // Put the caret inside "subframe" so that selecting a word has to extend the selection in the
    // iframe's process.
    [webView objectByEvaluatingJavaScript:@"getSelection().collapse(document.body.firstChild, 3)" inFrame:childFrame.get()];

    [webView select:nil];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"subframe"];
    }));
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(SiteIsolation, SelectWordBackwardInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframe } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = siteIsolatedViewWithFocusedIframe(server);

    // Caret at the end of "subframe text"; selecting the word backward must select "text".
    [webView objectByEvaluatingJavaScript:@"getSelection().collapse(document.body.firstChild, 13)" inFrame:childFrame.get()];

    [webView selectWordBackwardForTesting];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"text"];
    }));
}

TEST(SiteIsolation, SelectWordForReplacementInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframe } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = siteIsolatedViewWithFocusedIframe(server);

    [webView objectByEvaluatingJavaScript:@"getSelection().collapse(document.body.firstChild, 3)" inFrame:childFrame.get()];

    [[webView textInputContentView] selectWordForReplacement];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"subframe"];
    }));
}

TEST(SiteIsolation, MoveSelectionByOffsetInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframe } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = siteIsolatedViewWithFocusedIframe(server);

    [webView objectByEvaluatingJavaScript:@"getSelection().collapse(document.body.firstChild, 3)" inFrame:childFrame.get()];

    [[webView textInputContentView] moveByOffset:-1];

    EXPECT_TRUE(Util::waitFor([&] {
        return [webView selectionRangeHasStartOffset:2 endOffset:2 inFrame:childFrame.get()];
    }));
}

TEST(SiteIsolation, MoveSelectionAtBoundaryInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframe } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = siteIsolatedViewWithFocusedIframe(server);

    // Caret in the middle of "subframe"; moving to the word boundary to the left lands at offset 0.
    [webView objectByEvaluatingJavaScript:@"getSelection().collapse(document.body.firstChild, 3)" inFrame:childFrame.get()];

    __block bool didMoveSelection = false;
    [[webView textInputContentView] moveSelectionAtBoundary:UITextGranularityWord inDirection:UITextLayoutDirectionLeft completionHandler:^{
        didMoveSelection = true;
    }];
    Util::run(&didMoveSelection);

    EXPECT_TRUE(Util::waitFor([&] {
        return [webView selectionRangeHasStartOffset:0 endOffset:0 inFrame:childFrame.get()];
    }));
}

TEST(SiteIsolation, ReplaceSelectedTextInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframe } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0' contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = siteIsolatedViewWithFocusedIframe(server);

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"subframe text"];
    }));

    [[webView textInputContentView] replaceText:@"subframe text" withText:@"replaced"];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"replaced"];
    }));
}

TEST(SiteIsolation, SelectionBoundingRectInCrossOriginIframeUsesMainFrameCoordinates)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();

    // The iframe is at (100, 100) in the main frame, so the subframe selection rect must be
    // converted to main-frame coordinates; without the fix it would be near the origin.
    __block CGRect rect = CGRectZero;
    while (true) {
        __block bool didReceiveRect = false;
        [webView _selectionBoundingRectInMainFrameCoordinatesForTesting:^(CGRect receivedRect) {
            rect = receivedRect;
            didReceiveRect = true;
        }];
        Util::run(&didReceiveRect);
        if (!CGRectIsEmpty(rect))
            break;
        Util::spinRunLoop();
    }

    EXPECT_GE(CGRectGetMinX(rect), 100);
    EXPECT_GE(CGRectGetMinY(rect), 100);
}

TEST(SiteIsolation, SelectionBoundingRectInNestedCrossOriginIframesUsesMainFrameCoordinates)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/middle'></iframe></body>"_s } },
        { "/middle"_s, { "<!DOCTYPE html><body style='margin: 0'><iframe id='inner' style='margin: 50px; width: 200px; height: 150px; border: none;' src='https://domain3.com/inner'></iframe></body>"_s } },
        { "/inner"_s, { "<!DOCTYPE html><body style='margin: 0'>test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // Wait for the deepest cross-origin iframe (domain3) to appear in the frame tree, then focus it
    // from its parent (domain2) and select its text.
    while (![webView mainFrame].childFrames.firstObject.childFrames.firstObject)
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"document.getElementById('inner').focus()" inFrame:[webView mainFrame].childFrames.firstObject.info completionHandler:nil];
    while (![[webView mainFrame].childFrames.firstObject.childFrames.firstObject.info _isFocused])
        Util::spinRunLoop();

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:[webView mainFrame].childFrames.firstObject.childFrames.firstObject.info])
        Util::spinRunLoop();

    // The domain2 iframe is at (100, 100) in the main frame and the domain3 iframe is at (50, 50)
    // within it, so the selection's bounding rect must be converted through both cross-process hops
    // to land at >= (150, 150) in main-frame coordinates.
    __block CGRect rect = CGRectZero;
    while (true) {
        __block bool didReceiveRect = false;
        [webView _selectionBoundingRectInMainFrameCoordinatesForTesting:^(CGRect receivedRect) {
            rect = receivedRect;
            didReceiveRect = true;
        }];
        Util::run(&didReceiveRect);
        if (!CGRectIsEmpty(rect))
            break;
        Util::spinRunLoop();
    }

    EXPECT_GE(CGRectGetMinX(rect), 150);
    EXPECT_GE(CGRectGetMinY(rect), 150);
}

TEST(SiteIsolation, SelectionBoundingRectInMainFrameIsNotOffset)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'>test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4])
        Util::spinRunLoop();

    // The selection is in the main frame, so no conversion is needed and the rect must stay near the
    // top-left where the text is laid out (margin: 0). A spurious conversion would push it past 100.
    __block CGRect rect = CGRectZero;
    while (true) {
        __block bool didReceiveRect = false;
        [webView _selectionBoundingRectInMainFrameCoordinatesForTesting:^(CGRect receivedRect) {
            rect = receivedRect;
            didReceiveRect = true;
        }];
        Util::run(&didReceiveRect);
        if (!CGRectIsEmpty(rect))
            break;
        Util::spinRunLoop();
    }

    EXPECT_LT(CGRectGetMinX(rect), 50);
    EXPECT_LT(CGRectGetMinY(rect), 50);
}

TEST(SiteIsolation, SelectionInCrossOriginIframeTracksMainFrameScroll)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0; height: 3000px'>"
            "<iframe id='sameorigin' style='display: block; position: absolute; top: 100px; left: 100px; width: 400px; height: 200px; border: none;' src='https://example.com/iframe'></iframe>"
            "<iframe id='crossorigin' style='display: block; position: absolute; top: 500px; left: 100px; width: 400px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe>"
            "</body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"SelectionHonorsOverflowScrolling"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // Locate the same-origin and cross-origin iframes in the frame tree.
    auto childInfoForHost = [&](NSString *host) -> RetainPtr<WKFrameInfo> {
        for (_WKFrameTreeNode *child in [webView mainFrame].childFrames) {
            if ([child.info.securityOrigin.host isEqualToString:host])
                return child.info;
        }
        return nil;
    };
    while (!childInfoForHost(@"example.com") || !childInfoForHost(@"webkit.org"))
        Util::spinRunLoop();

    auto readSelectionRect = [&] -> CGRect {
        __block CGRect rect = CGRectZero;
        __block bool didReceiveRect = false;
        [webView _selectionBoundingRectInMainFrameCoordinatesForTesting:^(CGRect receivedRect) {
            rect = receivedRect;
            didReceiveRect = true;
        }];
        Util::run(&didReceiveRect);
        return rect;
    };

    // Focus the given iframe and select all of its text so the editor state is recomputed with the
    // main frame at its current scroll offset, then report the selection rect. WKFrameInfo's focus
    // state is a snapshot, so re-fetch it each spin. Clear the frame's selection first (so the
    // subsequent SelectAll always recomputes) and key the waits off the specific frame.
    auto selectTextAndReadRect = [&](NSString *elementID, NSString *host) -> CGRect {
        [webView evaluateJavaScript:[NSString stringWithFormat:@"document.getElementById('%@').focus()", elementID] completionHandler:nil];
        RetainPtr<WKFrameInfo> frame;
        while (!(frame = childInfoForHost(host)) || ![frame _isFocused])
            Util::spinRunLoop();
        [webView evaluateJavaScript:@"getSelection().removeAllRanges()" inFrame:frame.get() completionHandler:nil];
        while ([webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:frame.get()])
            Util::spinRunLoop();
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:frame.get()])
            Util::spinRunLoop();
        [webView waitForNextPresentationUpdate];
        return readSelectionRect();
    };

    auto scrollDelta = [&](NSString *elementID, NSString *host) -> CGFloat {
        [[webView scrollView] setContentOffset:CGPointZero animated:NO];
        [webView waitForNextPresentationUpdate];
        CGRect before = selectTextAndReadRect(elementID, host);

        [[webView scrollView] setContentOffset:CGPointMake(0, 200) animated:NO];
        [webView waitForNextPresentationUpdate];
        CGRect after = selectTextAndReadRect(elementID, host);

        return CGRectGetMinY(after) - CGRectGetMinY(before);
    };

    // A same-origin iframe shares the main frame's process, so its selection rect uses the ordinary
    // (known-correct) conversion; a cross-origin iframe's rect goes through the site-isolation
    // conversion. The two must respond to main-frame scrolling identically -- otherwise the selection
    // (and caret) in the cross-origin iframe is drawn at the wrong location once the main frame is
    // scrolled, because the main frame's RemoteFrameView proxy subtracts its scroll offset while the
    // real main frame (which delegates scrolling to the UIScrollView) does not.
    CGFloat sameOriginDelta = scrollDelta(@"sameorigin", @"example.com");
    CGFloat crossOriginDelta = scrollDelta(@"crossorigin", @"webkit.org");
    EXPECT_EQ(crossOriginDelta, sameOriginDelta);
}

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
TEST(SiteIsolation, SelectionInCrossOriginIframeIsContainedByContentView)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'>test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // The selection container is only ever the enclosing overflow-scroll layer when
    // SelectionHonorsOverflowScrolling is enabled, so enable it to exercise the code path this test
    // guards.
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"SelectionHonorsOverflowScrolling"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    // The container is the enclosing layer only when that layer is in a window, so host the view.
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();
    [webView waitForNextPresentationUpdate];

    // The selection lives in a cross-origin subframe, whose rects are reported in main-frame
    // coordinates, so it must be contained by the WKContentView itself -- not the subframe's
    // overflow-scroll layer (which is offset by the subframe's position and scroll). Otherwise the
    // selection loupe/handles are positioned in the wrong coordinate space.
    EXPECT_EQ(webView.get().selectionHighlightView.superview, webView.get().textInputContentView);
}

TEST(SiteIsolation, SelectionInCrossOriginIframeIsClippedToIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0; font: 50px/60px monospace'>test test test test test test test test test test test test test test test test test test test test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"SelectionHonorsOverflowScrolling"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (CGRectIsNull([webView selectionClipRect]))
        Util::spinRunLoop();
    [webView waitForNextPresentationUpdate];

    // The selected text extends below the bottom of the iframe, so the selection must be clipped to
    // the iframe's bounds (in main-frame coordinates) as it would be for a same-process iframe.
    EXPECT_GT([[webView objectByEvaluatingJavaScript:@"document.body.scrollHeight" inFrame:childFrame.get()] intValue], 300);
    auto selectionClipRect = [webView selectionClipRect];
    EXPECT_EQ(100, selectionClipRect.origin.x);
    EXPECT_EQ(100, selectionClipRect.origin.y);
    EXPECT_EQ(400, selectionClipRect.size.width);
    EXPECT_EQ(300, selectionClipRect.size.height);

    // UIKit clips the highlight to the selection clip rect.
    CGRect selectionBounds = CGRectNull;
    for (NSValue *rect in [webView selectionViewRectsInContentCoordinates])
        selectionBounds = CGRectUnion(selectionBounds, rect.CGRectValue);
    EXPECT_FALSE(CGRectIsNull(selectionBounds));
    EXPECT_TRUE(CGRectContainsRect(selectionClipRect, selectionBounds));
}

TEST(SiteIsolation, SelectionInOverflowScrollerInCrossOriginIframeIsClippedToScroller)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><div id='scroller' style='margin: 20px; width: 200px; height: 100px; overflow: scroll; font: 50px/60px monospace'>test test test test test test test test test test</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"SelectionHonorsOverflowScrolling"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView objectByEvaluatingJavaScript:@"const text = document.getElementById('scroller').firstChild; getSelection().setBaseAndExtent(text, 0, text, text.length); true" inFrame:childFrame.get()];
    while (![webView selectionRangeHasStartOffset:0 endOffset:49 inFrame:childFrame.get()])
        Util::spinRunLoop();
    [webView waitForNextPresentationUpdate];

    // The selected text overflows the scroller, which is 200x100 at (120, 120) in main-frame
    // coordinates, so the selection must be clipped to the scroller rather than to the iframe.
    EXPECT_GT([[webView objectByEvaluatingJavaScript:@"document.getElementById('scroller').scrollHeight" inFrame:childFrame.get()] intValue], 100);
    auto selectionClipRect = [webView selectionClipRect];
    EXPECT_EQ(120, selectionClipRect.origin.x);
    EXPECT_EQ(120, selectionClipRect.origin.y);
    EXPECT_EQ(200, selectionClipRect.size.width);
    EXPECT_EQ(100, selectionClipRect.size.height);

    CGRect selectionBounds = CGRectNull;
    for (NSValue *rect in [webView selectionViewRectsInContentCoordinates])
        selectionBounds = CGRectUnion(selectionBounds, rect.CGRectValue);
    EXPECT_FALSE(CGRectIsNull(selectionBounds));
    EXPECT_TRUE(CGRectContainsRect(selectionClipRect, selectionBounds));
}

TEST(SiteIsolation, SelectionInSameSiteIframeNestedInCrossOriginIframeIsClippedToInnerIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/child'></iframe></body>"_s } },
        { "/child"_s, { "<body style='margin: 0'><iframe style='margin: 50px; width: 200px; height: 100px; border: none;' src='/grandchild'></iframe></body>"_s } },
        { "/grandchild"_s, { "<!DOCTYPE html><body style='margin: 0; font: 50px/60px monospace'>test test test test test test test test test test test test test test test test test test test test</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"SelectionHonorsOverflowScrolling"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    auto grandchildFrame = [&] {
        return [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
    };
    [webView evaluateJavaScript:@"document.querySelector('iframe').focus()" inFrame:childFrame.get() completionHandler:nil];
    while (![grandchildFrame() _isFocused])
        Util::spinRunLoop();

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:99 inFrame:grandchildFrame()])
        Util::spinRunLoop();
    [webView waitForNextPresentationUpdate];

    CGRect selectionBounds = CGRectNull;
    for (NSValue *rect in [webView selectionViewRectsInContentCoordinates])
        selectionBounds = CGRectUnion(selectionBounds, rect.CGRectValue);
    EXPECT_EQ(150, CGRectGetMinX(selectionBounds));
    EXPECT_NEAR(150, CGRectGetMinY(selectionBounds), 2);

    // The inner iframe is 200x100 at (150, 150) in main-frame coordinates, and the selected text
    // extends below it, so the selection must be clipped to the inner iframe rather than the outer one.
    auto selectionClipRect = [webView selectionClipRect];
    EXPECT_EQ(150, selectionClipRect.origin.x);
    EXPECT_EQ(150, selectionClipRect.origin.y);
    EXPECT_EQ(200, selectionClipRect.size.width);
    EXPECT_EQ(100, selectionClipRect.size.height);
}
#endif // HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

#if HAVE(UI_EDIT_MENU_INTERACTION)
TEST(SiteIsolation, EditMenuSelectsWordInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='display: block; position: absolute; top: 100px; left: 100px; width: 400px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0; font: 50px/60px monospace'>Hello world</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(140, 130) completion:^(BOOL shouldPresentMenu) {
        EXPECT_TRUE(shouldPresentMenu);
        done = true;
    }];
    Util::run(&done);

    EXPECT_WK_STREQ("Hello", [webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}
#endif // HAVE(UI_EDIT_MENU_INTERACTION)

} // namespace TestWebKitAPI

@interface SiteIsolationInputSessionWebView : TestWKWebView
@property (nonatomic, readonly) BOOL hasActiveInputSession;
@end

@implementation SiteIsolationInputSessionWebView {
    BOOL _hasActiveInputSession;
}

- (BOOL)hasActiveInputSession
{
    return _hasActiveInputSession;
}

- (void)didStartFormControlInteraction
{
    _hasActiveInputSession = YES;
    [super didStartFormControlInteraction];
}

- (void)didEndFormControlInteraction
{
    _hasActiveInputSession = NO;
    [super didEndFormControlInteraction];
}

@end

namespace TestWebKitAPI {

TEST(SiteIsolation, FocusingMainFrameFieldKeepsFocusAfterCrossOriginIframeField)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><input id='mainInput'><iframe id='iframe' style='display: block; width: 300px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><input id='iframeInput'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[SiteIsolationInputSessionWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    RetainPtr childFrame = [webView firstChildFrame];

    // Focus the text field in the cross-origin iframe and wait (via the didStartFormControlInteraction
    // hook) for its input session to begin. A cross-origin frame needs a user gesture to take focus, so
    // this can't use -evaluateJavaScriptAndWaitForInputSessionToChange, which evaluates without one.
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('iframeInput').focus()" inFrame:childFrame.get()];
    while (![webView hasActiveInputSession])
        Util::spinRunLoop();

    // Focus the text field in the main frame.
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('mainInput').focus()"];

    // Once the main frame takes focus, the subframe process blurs its now-defocused field and sends an
    // ElementDidBlur that can arrive afterwards. Round-trip through the subframe process so that blur has
    // been delivered to and processed by the UI process (messages from a process are ordered) before we
    // assert. The main frame's input session must remain active -- previously the stale blur cleared it.
    [webView objectByEvaluatingJavaScript:@"0" inFrame:childFrame.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([webView hasActiveInputSession]);
}

TEST(SiteIsolation, RefocusingCrossOriginIframeFieldStartsInputSessionAgain)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><input id='mainInput' placeholder='main'><iframe id='iframe' style='display: block; width: 300px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><input id='iframeInput' placeholder='iframe'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    RetainPtr focusedElements = adoptNS([NSMutableArray new]);
    RetainPtr inputSessionElements = adoptNS([NSMutableArray new]);

    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id<_WKFocusedElementInfo> info) {
        [focusedElements addObject:info.placeholder];
        // Disallowing an input session for the main frame's field is essential to reproducing the bug:
        // -_elementDidFocus bails before storing the new focused element information, so the UI process
        // keeps the iframe field's information. Refocusing the iframe field then looks like a refocus of
        // the element that is already focused and is dropped. Allowing an input session here would
        // overwrite that information, and the refocus would start a new session even without the fix.
        return [info.placeholder isEqualToString:@"main"] ? _WKFocusStartsInputSessionPolicyDisallow : _WKFocusStartsInputSessionPolicyAllow;
    }];
    [inputDelegate setWillStartInputSessionHandler:[&](WKWebView *, id<_WKFormInputSession>) {
        [inputSessionElements addObject:[focusedElements lastObject]];
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    RetainPtr childFrame = [webView firstChildFrame];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('iframeInput').focus()" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [inputSessionElements count] > 0;
    }));

    auto focusCountBeforeMainFrameFocus = [focusedElements count];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('mainInput').focus()"];
    EXPECT_TRUE(Util::waitFor([&] { return [focusedElements count] > focusCountBeforeMainFrameFocus; }));

    EXPECT_TRUE(Util::waitFor([&] {
        return ![[webView stringByEvaluatingJavaScript:@"document.activeElement.id" inFrame:childFrame.get()] isEqualToString:@"iframeInput"];
    }));

    auto focusCountBeforeRefocus = [focusedElements count];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('iframeInput').focus()" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::waitFor([&] { return [focusedElements count] > focusCountBeforeRefocus; }));
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ("iframe,main,iframe", [focusedElements componentsJoinedByString:@","]);
    EXPECT_WK_STREQ("iframe,iframe", [inputSessionElements componentsJoinedByString:@","]);
}

static void testZoomToRevealFocusedElementRect(unsigned mainFrameScrollY, unsigned subframeScrollY)
{
    // The iframe and the input are offset by the scroll amounts so that they appear at the same place in the view after scrolling.
    auto mainframeHTML = makeString("<body style='margin: 0; height: 3000px;'><iframe style='position: absolute; top: "_s, 100 + mainFrameScrollY, "px; left: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s);
    auto subframeHTML = makeString("<!DOCTYPE html><body style='margin: 0; height: 3000px;'>"
        "<input id='input' value='hello world' style='position: absolute; top: "_s, 50 + subframeScrollY, "px; left: 50px; width: 200px; height: 20px; border: none; padding: 0;'>"
        "</body>"_s);

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/iframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    // Allow an input session to begin when the input inside the subframe is focused.
    bool didStartInputSession = false;
    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id<_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"scrollTo(0, %u); true", mainFrameScrollY]];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"scrollTo(0, %u); true", subframeScrollY] inFrame:[webView firstChildFrame]];
    [webView waitForNextPresentationUpdate];
    [webView focusInWindow];

    // Focus and select the input inside the cross-origin iframe with a user gesture (Element::focus() is a
    // no-op for cross-origin non-main-frame iframes without one), then wait until the input session has
    // started and the UI process tracks the subframe as the focused frame.
    RetainPtr childFrame = [webView firstChildFrame];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"input.focus(); input.select()" inFrame:childFrame.get()];
    Util::run(&didStartInputSession);
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    // The input is at (50, 50) in the iframe's view, and the iframe is at (100, 100) in the main frame's
    // view, so the interaction rect is at (150, 150) plus the main frame's scroll offset in main-frame
    // document coordinates. The subframe's scroll offset must only be applied once.
    EXPECT_EQ([webView _focusedElementInteractionRect], CGRectMake(150, 150 + mainFrameScrollY, 200, 20));

    // -[WKContentView _zoomToRevealFocusedElement] reveals the selection's bounding rect intersected with
    // the focused element's interaction rect. Both are in main-frame coordinates, so the reveal rect lands
    // on the input rather than near the page origin, and stays within the interaction rect the zoom is
    // anchored to.
    CGRect revealRect = CGRectZero;
    EXPECT_TRUE(Util::waitFor([&] {
        revealRect = [webView _rectToRevealWhenZoomingToFocusedElementForTesting];
        return !CGRectIsEmpty(revealRect);
    }));

    EXPECT_GE(CGRectGetMinX(revealRect), 150);
    EXPECT_GE(CGRectGetMinY(revealRect), 150 + mainFrameScrollY);
    EXPECT_TRUE(CGRectContainsRect([webView _focusedElementInteractionRect], revealRect));
}

TEST(SiteIsolation, ZoomToRevealFocusedElementRect)
{
    testZoomToRevealFocusedElementRect(0, 0);
    testZoomToRevealFocusedElementRect(500, 0);
    testZoomToRevealFocusedElementRect(0, 500);
    testZoomToRevealFocusedElementRect(500, 500);
}

#if HAVE(UICONTEXTMENU_LOCATION)

// UIKit anchors the menu to a hidden control whose frame is the anchor rect.
static CGRect presentedMenuAnchorRect = CGRectNull;

static CGRect menuAnchorRectAfterOpeningFilePicker(TestWKWebView *webView, NSString *script, WKFrameInfo *frame)
{
    // Wait for the frame to commit the document containing the input.
    while (![[webView objectByEvaluatingJavaScript:@"!!document.querySelector('input')" inFrame:frame] boolValue])
        Util::spinRunLoop();

    InstanceMethodSwizzler menuPresentationSwizzler { UIContextMenuInteraction.class, @selector(_presentMenuAtLocation:), imp_implementationWithBlock(^(UIContextMenuInteraction *interaction, CGPoint) {
        presentedMenuAnchorRect = interaction.view.frame;
    }) };

    presentedMenuAnchorRect = CGRectNull;
    [webView objectByEvaluatingJavaScriptWithUserGesture:script inFrame:frame];
    EXPECT_TRUE(Util::waitFor([&] {
        return !CGRectIsNull(presentedMenuAnchorRect);
    }));

    [webView _dismissFilePicker];
    return presentedMenuAnchorRect;
}

TEST(SiteIsolation, FileUploadPanelAnchorRectInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><input type='file' style='display: block; margin: 50px; width: 100px; height: 50px; border: none; padding: 0;'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://domain2.com"_s } }
        },
    });

    // The input is at (50, 50) in the iframe, which is at (100, 100) in the main frame.
    EXPECT_EQ(menuAnchorRectAfterOpeningFilePicker(webView.get(), @"document.querySelector('input').showPicker()", [webView firstChildFrame]), CGRectMake(150, 150, 100, 50));
}

TEST(SiteIsolation, FileUploadPanelAnchorRectInNestedCrossOriginIframes)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/middle'></iframe></body>"_s } },
        { "/middle"_s, { "<!DOCTYPE html><body style='margin: 0'><iframe style='display: block; margin: 50px; width: 200px; height: 150px; border: none;' src='https://domain3.com/inner'></iframe></body>"_s } },
        { "/inner"_s, { "<!DOCTYPE html><body style='margin: 0'><input type='file' style='display: block; margin: 25px; width: 100px; height: 50px; border: none; padding: 0;'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    while (![webView mainFrame].childFrames.firstObject.childFrames.firstObject)
        Util::spinRunLoop();

    // Two nested frame offsets: 100 + 50 + 25.
    EXPECT_EQ(menuAnchorRectAfterOpeningFilePicker(webView.get(), @"document.querySelector('input').showPicker()", [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info), CGRectMake(175, 175, 100, 50));
}

TEST(SiteIsolation, FileUploadPanelAnchorRectWithScrolledMainFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0; height: 2000px'><iframe style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><input type='file' style='display: block; margin: 50px; width: 100px; height: 50px; border: none; padding: 0;'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
    while ([[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] != 400)
        Util::spinRunLoop();

    // The main frame's scroll offset must not shift the anchor.
    EXPECT_EQ(menuAnchorRectAfterOpeningFilePicker(webView.get(), @"document.querySelector('input').showPicker()", [webView firstChildFrame]), CGRectMake(150, 550, 100, 50));
}

TEST(SiteIsolation, FileUploadPanelAnchorRectInMainFrameIsNotOffset)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><input type='file' style='display: block; margin: 60px; width: 100px; height: 50px; border: none; padding: 0;'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // A spurious conversion would move the anchor off the element.
    EXPECT_EQ(menuAnchorRectAfterOpeningFilePicker(webView.get(), @"document.querySelector('input').showPicker()", nil), CGRectMake(60, 60, 100, 50));
}

TEST(SiteIsolation, FileUploadPanelAnchorRectForHiddenInputInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><input type='file' style='display: none'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // No box, so the menu opens anchored to nothing rather than being cancelled.
    CGRect anchorRect = menuAnchorRectAfterOpeningFilePicker(webView.get(), @"document.querySelector('input').click()", [webView firstChildFrame]);
    EXPECT_FALSE(CGRectIsNull(anchorRect));
    EXPECT_TRUE(CGRectIsEmpty(anchorRect));
}

#endif // HAVE(UICONTEXTMENU_LOCATION)

TEST(SiteIsolation, PositionInformationForImageInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0'><img style='display: block; margin: 50px; width: 100px; height: 100px;' src='https://webkit.org/image.png'></body>"_s } },
        { "/image.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    __block RetainPtr<_WKActivatedElementInfo> elementInfo;
    __block bool done = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(200, 200) completionBlock:^(_WKActivatedElementInfo *info) {
        elementInfo = info;
        done = true;
    }];
    Util::run(&done);

    EXPECT_EQ(_WKActivatedElementTypeImage, [elementInfo type]);
    EXPECT_WK_STREQ(@"https://webkit.org/image.png", [elementInfo imageURL].absoluteString);

    CGRect bounds = [elementInfo boundingRect];
    EXPECT_NEAR(150, bounds.origin.x, 1);
    EXPECT_NEAR(150, bounds.origin.y, 1);
    EXPECT_NEAR(100, bounds.size.width, 1);
    EXPECT_NEAR(100, bounds.size.height, 1);
}

TEST(SiteIsolation, SynchronousPositionInformationForTextInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='display: block; margin: 100px; width: 400px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body style='margin: 0; font: 50px/60px monospace'>Hello world</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView wkContentView] hasSelectablePositionAtPoint:CGPointMake(140, 130)]);
}

static bool screenIsBeingCapturedInProcessForFrame(TestWKWebView *webView, WKFrameInfo *frame)
{
    __block bool done = false;
    __block bool result = false;
    [webView _screenIsBeingCapturedForFrame:frame._handle completionHandler:^(BOOL captured) {
        result = captured;
        done = true;
    }];
    Util::run(&done);
    return result;
}

TEST(SiteIsolation, ScreenCaptureStateReachesCrossSiteIframeProcesses)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<input type='password'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block BOOL screenIsBeingCaptured = NO;
    InstanceMethodSwizzler screenCaptureSwizzler {
        NSClassFromString(@"WKContentView"),
        @selector(screenIsBeingCaptured),
        imp_implementationWithBlock(^BOOL(id) {
            return screenIsBeingCaptured;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr<WKFrameInfo> mainFrame = [webView mainFrame].info;
    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_NE([mainFrame _processIdentifier], [childFrame _processIdentifier]);
    EXPECT_FALSE(screenIsBeingCapturedInProcessForFrame(webView.get(), childFrame.get()));

    screenIsBeingCaptured = YES;
    [[webView wkContentView] _sceneCaptureStateDidChange];
    EXPECT_TRUE(screenIsBeingCapturedInProcessForFrame(webView.get(), mainFrame.get()));
    EXPECT_TRUE(screenIsBeingCapturedInProcessForFrame(webView.get(), childFrame.get()));

    [webView evaluateJavaScript:@"document.querySelector('iframe').src = 'https://c.com/subframe'" completionHandler:nil];
    while (![[webView firstChildFrame].securityOrigin.host isEqualToString:@"c.com"])
        Util::spinRunLoop();
    RetainPtr newChildFrame = [webView firstChildFrame];
    EXPECT_NE([newChildFrame _processIdentifier], [childFrame _processIdentifier]);
    EXPECT_TRUE(screenIsBeingCapturedInProcessForFrame(webView.get(), newChildFrame.get()));
}

#if HAVE(UIFINDINTERACTION)

TEST(SiteIsolation, FindStringInFrameIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Hello world", searchOptions.get(), 1UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Missing string", searchOptions.get(), 0UL);
}

TEST(SiteIsolation, FindStringInNestedFrameIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Hello world", searchOptions.get(), 1UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Missing string", searchOptions.get(), 0UL);
}

TEST(SiteIsolation, FindStringAcrossMultipleFramesIOS)
{
    const auto mainFrameSrc = "<iframe src='https://domain2.com/subframe'></iframe>"
    "<iframe src='https://domain3.com/subframe2'></iframe>"
    "<p>foobar</p>"
    "<iframe src='https://domain4.com/subframe3'></iframe>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameSrc } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>I am going to write a bunch of words and the word foobar will be somewhere in the middle.</p>"_s } },
        { "/subframe2"_s, { "<iframe src='https://domain5.com/nested_subframe2'></iframe>"_s } },
        { "/nested_subframe2"_s, { "<p>nested foobarfoobar</p>"_s } },
        { "/subframe3"_s, { "<p>foobar</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"foobar", searchOptions.get(), 5UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"nothing", searchOptions.get(), 0UL);
}

TEST(SiteIsolation, FindStringInFrameAndReplaceIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable><p>foobar</p><p>shoebar foobar</p></body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto ranges = textRangesForQueryString(webView.get(), @"foobar");

    EXPECT_EQ([ranges count], (NSUInteger)2);

    [webView _setEditable:YES];

    // replace first instance of "foobar" with "here"
    auto range = [ranges firstObject];
    [webView replaceFoundTextInRange:range inDocument:nil withText:@"here"];

    [webView waitForNextPresentationUpdate];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"here", searchOptions.get(), 1UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"foobar", searchOptions.get(), 1UL);
}

TEST(SiteIsolation, FindStringAcrossMultipleFramesOrderIOS)
{
    const auto mainFrameSrc = "<p>match1</p>"
    "<iframe src='https://domain2.com/frame1'></iframe>"
    "<p>match2</p>"
    "<iframe src='https://domain3.com/frame2'></iframe>"
    "<p>match3</p>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameSrc } },
        { "/frame1"_s, { "<p>match4</p><p>match5</p>"_s } },
        { "/frame2"_s, { "<p>match6</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr foundRanges = textRangesForQueryString(webView.get(), @"match");

    EXPECT_EQ([foundRanges count], 6UL);

    for (NSUInteger i = 0; i < [foundRanges count] - 1; i++) {
        UITextRange *current = foundRanges.get()[i];
        UITextRange *next = foundRanges.get()[i + 1];

        NSComparisonResult result = [webView compareFoundRange:current toRange:next inDocument:nil];
        EXPECT_EQ(result, NSOrderedAscending); // current < next
    }
}

TEST(SiteIsolation, FindStringNestedFramesOrderIOS)
{
    HTTPServer server({
        { "/main"_s, { "<p>resultA</p><iframe src='https://d2.com/f1'></iframe><p>resultB</p>"_s } },
        { "/f1"_s, { "<p>resultC</p><iframe src='https://d3.com/f2'></iframe><p>resultD</p>"_s } },
        { "/f2"_s, { "<p>resultE</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://d1.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr foundRanges = textRangesForQueryString(webView.get(), @"result");

    EXPECT_EQ([foundRanges count], 5UL);

    for (NSUInteger i = 0; i < [foundRanges count] - 1; i++) {
        NSComparisonResult result = [webView compareFoundRange:foundRanges.get()[i] toRange:foundRanges.get()[i + 1] inDocument:nil];
        EXPECT_EQ(result, NSOrderedAscending);
    }
}

TEST(SiteIsolation, DecorateFoundTextRangeIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    RetainPtr findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto ranges = textRangesForQueryString(webView.get(), @"world");

    EXPECT_EQ([ranges count], (NSUInteger)1);
    UITextRange *range = [ranges objectAtIndex:0];

    // Start text search operation to create find overlay
    __block bool didAddOverlay = false;
    [findDelegate setDidAddLayerForFindOverlayHandler:^{
        didAddOverlay = true;
    }];

    [webView didBeginTextSearchOperation];

    TestWebKitAPI::Util::run(&didAddOverlay);
    EXPECT_NOT_NULL([webView _layerForFindOverlay]);

    // Test all decoration styles - Normal, Found, and Highlighted
    [webView decorateFoundTextRange:range inDocument:nil usingStyle:(UITextSearchFoundTextStyle)_UIFoundTextStyleNormal];
    [webView decorateFoundTextRange:range inDocument:nil usingStyle:(UITextSearchFoundTextStyle)_UIFoundTextStyleFound];

    // Verify we can still get a rect for the decorated range
    __block bool didReceiveRect = false;
    __block CGRect receivedRect = CGRectZero;
    [webView _requestRectForFoundTextRange:range completionHandler:^(CGRect rect) {
        receivedRect = rect;
        didReceiveRect = true;
    }];
    TestWebKitAPI::Util::run(&didReceiveRect);
    EXPECT_FALSE(CGRectIsEmpty(receivedRect));

    [webView decorateFoundTextRange:range inDocument:nil usingStyle:(UITextSearchFoundTextStyle)_UIFoundTextStyleHighlighted];

    // Verify the range is still valid after highlighting and overlay still exists
    didReceiveRect = false;
    receivedRect = CGRectZero;
    [webView _requestRectForFoundTextRange:range completionHandler:^(CGRect rect) {
        receivedRect = rect;
        didReceiveRect = true;
    }];
    TestWebKitAPI::Util::run(&didReceiveRect);
    EXPECT_FALSE(CGRectIsEmpty(receivedRect));
    EXPECT_NOT_NULL([webView _layerForFindOverlay]);
}

TEST(SiteIsolation, ScrollTextRangeToVisibleIOS)
{
    // Position the iframe far down the page so the main frame needs to scroll
    const auto mainframeSrc = "<div style='height: 2000px;'>Spacer content at top</div>"
    "<iframe src='https://domain2.com/subframe' style='width: 100%; height: 400px;'></iframe>"_s;

    const auto subframeSrc = "<p>Target text in iframe</p>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeSrc } },
        { "/subframe"_s, { subframeSrc } },
    }, HTTPServer::Protocol::HttpsProxy);

    // Create webView with explicit size so it can scroll
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto ranges = textRangesForQueryString(webView.get(), @"Target text");

    EXPECT_EQ([ranges count], (NSUInteger)1);
    UITextRange *range = [ranges firstObject];

    // Scroll the found text range into view
    // This primarily tests that scrollTextRangeToVisible works with FrameIdentifier
    // tracking and doesn't crash with site-isolated iframes
    [webView scrollRangeToVisible:range inDocument:nil];
}

TEST(SiteIsolation, ClearAllDecoratedFoundTextIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe><iframe src='https://domain3.com/subframe2'></iframe>"_s } },
        { "/subframe"_s, { "<p>foobar</p>"_s } },
        { "/subframe2"_s, { "<p>foobar</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    RetainPtr findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto ranges = textRangesForQueryString(webView.get(), @"foobar");
    EXPECT_EQ([ranges count], (NSUInteger)2);

    __block bool didAddOverlay = false;
    [findDelegate setDidAddLayerForFindOverlayHandler:^{
        didAddOverlay = true;
    }];

    [webView didBeginTextSearchOperation];
    TestWebKitAPI::Util::run(&didAddOverlay);
    EXPECT_NOT_NULL([webView _layerForFindOverlay]);

    // Decorate ranges across multiple site-isolated iframes
    for (UITextRange *range in ranges.get())
        [webView decorateFoundTextRange:range inDocument:nil usingStyle:(UITextSearchFoundTextStyle)_UIFoundTextStyleHighlighted];

    [webView clearAllDecoratedFoundText];

    // Verify we can still find and decorate text after clearing
    auto rangesAfterClear = textRangesForQueryString(webView.get(), @"foobar");
    EXPECT_EQ([rangesAfterClear count], (NSUInteger)2);

    for (UITextRange *range in rangesAfterClear.get())
        [webView decorateFoundTextRange:range inDocument:nil usingStyle:(UITextSearchFoundTextStyle)_UIFoundTextStyleFound];

    // Verify ranges work after re-decoration
    for (UITextRange *range in rangesAfterClear.get()) {
        __block bool didReceiveRect = false;
        __block CGRect receivedRect = CGRectZero;
        [webView _requestRectForFoundTextRange:range completionHandler:^(CGRect rect) {
            receivedRect = rect;
            didReceiveRect = true;
        }];
        TestWebKitAPI::Util::run(&didReceiveRect);
        EXPECT_FALSE(CGRectIsEmpty(receivedRect));
    }
}

TEST(SiteIsolation, RequestRectForFoundTextRangeIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto ranges = textRangesForQueryString(webView.get(), @"world");

    EXPECT_EQ([ranges count], (NSUInteger)1);
    UITextRange *range = [ranges objectAtIndex:0];

    __block bool didReceiveRect = false;
    __block CGRect receivedRect = CGRectZero;
    [webView _requestRectForFoundTextRange:range completionHandler:^(CGRect rect) {
        receivedRect = rect;
        didReceiveRect = true;
    }];

    TestWebKitAPI::Util::run(&didReceiveRect);

    // Verify we got a non-zero rect
    EXPECT_FALSE(CGRectIsEmpty(receivedRect));
    EXPECT_GT(CGRectGetWidth(receivedRect), 0);
    EXPECT_GT(CGRectGetHeight(receivedRect), 0);
}

#endif

#if ENABLE(DEVICE_ORIENTATION)

TEST(SiteIsolation, CrossSiteIFrameCanReceiveDeviceOrientationEvents)
{
    auto mainframeHTML = "<iframe src='https://examplesubframe.com/subframe' allow='accelerometer;gyroscope;magnetometer'></iframe>"_s;

    auto subframeHTML = "<script>"
        "    window.addEventListener('deviceorientation', function(event) {"
        "        alert('deviceOrientationEvent received');"
        "    });"
        "    "
        "    window.addEventListener('message', function(event) {"
        "        if (event.data === 'requestPermission') {"
        "            DeviceOrientationEvent.requestPermission().then(function(result) {"
        "                alert('permission granted');"
        "            }).catch(function(error) {"
        "                alert('error getting permission');"
        "            });"
        "        }"
        "    });"
        "    "
        "    "
        "    alert('iframe loaded');"
        "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    __block bool askedClientForPermission = false;

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [uiDelegate setRequestDeviceOrientationAndMotionPermissionForOrigin:^(WKSecurityOrigin *, WKFrameInfo *, void (^completion)(WKPermissionDecision)) {
        askedClientForPermission = true;
        completion(WKPermissionDecisionGrant);
    }];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://examplemainframe.com/mainframe"]]];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "iframe loaded");

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission()" inFrame:[webView firstChildFrame] completionHandler:nil];
    TestWebKitAPI::Util::run(&askedClientForPermission);
    askedClientForPermission = false;

    [webView _simulateDeviceOrientationChangeWithAlpha:45.0 beta:90.0 gamma:180.0];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "deviceOrientationEvent received");
}

TEST(SiteIsolation, CrossSiteIFrameCanReceiveDeviceMotionEvents)
{
    auto mainframeHTML = "<iframe src='https://examplesubframe.com/subframe' allow='accelerometer;gyroscope;magnetometer'></iframe>"_s;

    auto subframeHTML = "<script>"
        "    window.addEventListener('devicemotion', function(event) {"
        "        alert('deviceMotionEvent received');"
        "    });"
        "    "
        "    window.addEventListener('message', function(event) {"
        "        if (event.data === 'requestPermission') {"
        "            DeviceMotionEvent.requestPermission().then(function(result) {"
        "                alert('permission granted');"
        "            }).catch(function(error) {"
        "                alert('error getting permission');"
        "            });"
        "        }"
        "    });"
        "    "
        "    "
        "    alert('iframe loaded');"
        "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    __block bool askedClientForPermission = false;

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [uiDelegate setRequestDeviceOrientationAndMotionPermissionForOrigin:^(WKSecurityOrigin *, WKFrameInfo *, void (^completion)(WKPermissionDecision)) {
        askedClientForPermission = true;
        completion(WKPermissionDecisionGrant);
    }];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://examplemainframe.com/mainframe"]]];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "iframe loaded");

    [webView evaluateJavaScript:@"DeviceMotionEvent.requestPermission()" inFrame:[webView firstChildFrame] completionHandler:nil];
    TestWebKitAPI::Util::run(&askedClientForPermission);
    askedClientForPermission = false;

    [webView _simulateDeviceMotionChangeWithXAcceleration:1.0 yAcceleration:2.0 zAcceleration:3.0 xAccelerationIncludingGravity:1.0 yAccelerationIncludingGravity:2.0 zAccelerationIncludingGravity:3.0 xRotationRate:1.0 yRotationRate:2.0 zRotationRate:3.0];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "deviceMotionEvent received");
}

#endif // ENABLE(DEVICE_ORIENTATION)

TEST(SiteIsolation, NoRedundantFocusPolicyCallbackAfterBlurAndRefocusInCrossOriginIframe)
{
    auto mainHTML = "<iframe src='https://webkit.org/iframe' style='width: 300px; height: 300px;'></iframe>"_s;
    auto iframeHTML = "<input id='input' type='text' style='width: 200px; font-size: 20px;'>"_s;

    HTTPServer server({
        { "/example"_s, { mainHTML } },
        { "/iframe"_s, { { { "Content-Type"_s, "text/html"_s } }, iframeHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 320, 500));

    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    int focusPolicyCallCount = 0;
    bool didFocusPolicy = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id<_WKFocusedElementInfo>) {
        focusPolicyCallCount++;
        didFocusPolicy = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Wait for the cross-origin iframe's content to load.
    EXPECT_TRUE(Util::waitFor([&] {
        auto frame = [webView firstChildFrame];
        return frame && [[webView objectByEvaluatingJavaScript:@"!!document.getElementById('input')" inFrame:frame] boolValue];
    }));

    // Focus the input in the cross-origin iframe. Use WithUserGesture because Element::focus()
    // is a no-op for cross-origin non-main-frame iframes without a user gesture.
    [webView focusInWindow];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('input').focus()" inFrame:[webView firstChildFrame]];
    Util::run(&didFocusPolicy);
    EXPECT_EQ(1, focusPolicyCallCount);

    // Blur and immediately refocus the same element. The focus policy handler should not be
    // called again because the refocus of the same element should be suppressed.
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"var i = document.getElementById('input'); i.blur(); i.focus();" inFrame:[webView firstChildFrame]];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1, focusPolicyCallCount);
}

static constexpr auto iframeContentForCrossOriginDblclickWindowListener = "<!DOCTYPE html>"
    "<html>"
    "<body style='margin: 0; padding: 0;'>"
    "<script>"
    "window.addEventListener('dblclick', function(e) {"
    "    parent.postMessage({"
    "        type: 'dblclick',"
    "        clientX: e.clientX,"
    "        clientY: e.clientY"
    "    }, '*');"
    "});"
    "requestAnimationFrame(() => parent.postMessage({ type: 'iframeReady' }, '*'));"
    "</script>"
    "</body>"
    "</html>"_s;

static constexpr auto iframeContentForCrossOriginDblclickDocumentListener = "<!DOCTYPE html>"
    "<html>"
    "<body style='margin: 0; padding: 0;'>"
    "<script>"
    "document.addEventListener('dblclick', function(e) {"
    "    parent.postMessage({"
    "        type: 'dblclick',"
    "        clientX: e.clientX,"
    "        clientY: e.clientY"
    "    }, '*');"
    "});"
    "requestAnimationFrame(() => parent.postMessage({ type: 'iframeReady' }, '*'));"
    "</script>"
    "</body>"
    "</html>"_s;

static constexpr auto mainHTMLForCrossOriginDblclick = "<!DOCTYPE html>"
    "<html>"
    "<body style='margin: 0; padding: 0;'>"
    "<iframe id='frame' src='https://webkit.org/iframe' style='width: 100px; height: 100px; position: absolute; border: none;'></iframe>"
    "<script>"
    "window.iframeReady = new Promise(resolve => {"
    "    window.addEventListener('message', function(e) {"
    "        if (e.data.type === 'iframeReady')"
    "            resolve();"
    "    });"
    "});"
    "window.dblclickReceived = new Promise(resolve => {"
    "    window.addEventListener('message', function(e) {"
    "        if (e.data.type === 'dblclick') {"
    "            window.clientX = e.data.clientX;"
    "            window.clientY = e.data.clientY;"
    "            resolve();"
    "        }"
    "    });"
    "});"
    "</script>"
    "</body>"
    "</html>"_s;

static void testDblclickInCrossOriginIFrame(TestWKWebView *webView, CGFloat tapX, CGFloat tapY, NSString *expectedX, NSString *expectedY, NSString *jsTransform = nil)
{
    [webView objectByCallingAsyncFunction:@"return await window.iframeReady;" withArguments:@{ }];
    [webView waitForNextPresentationUpdate];

    if (jsTransform) {
        __block bool done = false;
        [webView evaluateJavaScript:jsTransform completionHandler:^(id, NSError *) {
            done = true;
        }];
        Util::run(&done);
        [webView waitForNextPresentationUpdate];
    }

    [webView _simulateDoubleClickAtLocation:CGPointMake(tapX, tapY)];
    [webView objectByCallingAsyncFunction:@"return await window.dblclickReceived;" withArguments:@{ }];

    EXPECT_WK_STREQ(expectedX, [webView stringByEvaluatingJavaScript:@"window.clientX"]);
    EXPECT_WK_STREQ(expectedY, [webView stringByEvaluatingJavaScript:@"window.clientY"]);
}

TEST(SiteIsolation, DblclickWithWindowListenerInSimpleIFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { mainHTMLForCrossOriginDblclick } },
        { "/iframe"_s, { iframeContentForCrossOriginDblclickWindowListener } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    testDblclickInCrossOriginIFrame(webView.get(), 50, 50, @"50", @"50");
}

TEST(SiteIsolation, DblclickWithWindowListenerInRotatedIFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { mainHTMLForCrossOriginDblclick } },
        { "/iframe"_s, { iframeContentForCrossOriginDblclickWindowListener } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    testDblclickInCrossOriginIFrame(webView.get(), 10, 10, @"90", @"90", @"frame.style.rotate = \"180deg\";");
}

TEST(SiteIsolation, DblclickWithWindowListenerInScaledIFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { mainHTMLForCrossOriginDblclick } },
        { "/iframe"_s, { iframeContentForCrossOriginDblclickWindowListener } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    testDblclickInCrossOriginIFrame(webView.get(), 50, 50, @"25", @"25", @"frame.style.transformOrigin = \"top left\"; frame.style.scale = \"2\";");
}

TEST(SiteIsolation, DblclickWithDocumentListenerInSimpleIFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { mainHTMLForCrossOriginDblclick } },
        { "/iframe"_s, { iframeContentForCrossOriginDblclickDocumentListener } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    testDblclickInCrossOriginIFrame(webView.get(), 50, 50, @"50", @"50");
}

TEST(SiteIsolation, DblclickWithDocumentListenerInRotatedIFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { mainHTMLForCrossOriginDblclick } },
        { "/iframe"_s, { iframeContentForCrossOriginDblclickDocumentListener } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    testDblclickInCrossOriginIFrame(webView.get(), 10, 10, @"90", @"90", @"frame.style.rotate = \"180deg\";");
}

TEST(SiteIsolation, DblclickWithDocumentListenerInScaledIFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { mainHTMLForCrossOriginDblclick } },
        { "/iframe"_s, { iframeContentForCrossOriginDblclickDocumentListener } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    testDblclickInCrossOriginIFrame(webView.get(), 50, 50, @"25", @"25", @"frame.style.transformOrigin = \"top left\"; frame.style.scale = \"2\";");
}

TEST(SiteIsolation, BaseWritingDirectionInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><p id='paragraph'>Hello world</p></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(paragraph.firstChild, 0)", _WKSelectionAttributeIsCaret);

    [webView makeTextWritingDirectionRightToLeft:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getComputedStyle(paragraph).direction" inFrame:childFrame.get()] isEqualToString:@"rtl"];
    }));
}

TEST(SiteIsolation, ChangeFontSizeInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><span id='target'>subframe</span></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setBaseAndExtent(target.firstChild, 0, target.firstChild, 8)", _WKSelectionAttributeIsRange);

    [webView _setFontSize:20 sender:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getComputedStyle(getSelection().getRangeAt(0).startContainer.parentElement).fontSize" inFrame:childFrame.get()] isEqualToString:@"20px"];
    }));
}

TEST(SiteIsolation, SpeakSelectionInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // If the request reaches the main frame's process, it finds no selection there and returns the main
    // frame's contents ("main frame text") instead.
    EXPECT_WK_STREQ("subframe text", [webView textForSpeakSelection]);
}

TEST(SiteIsolation, SelectionChangesInCrossOriginIframeAreIgnoredDuringTextInteraction)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><textarea style='display: block; width: 200px; height: 50px;'></textarea><iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 3)", _WKSelectionAttributeIsCaret);

    RetainPtr contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    ASSERT_GE([contexts count], 1U);
    RetainPtr context = [contexts firstObject];

    // While a text interaction is in progress, every web process must report its selection changes as
    // ignorable, so the UI process doesn't update its selection UI in the middle of the interaction.
    [webView _willBeginTextInteractionInTextInputContext:context.get()];
    [webView objectByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)" inFrame:childFrame.get()];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [webView _selectionAttributes]);

    // Finishing the interaction makes the web processes report their current selection again.
    [webView _didFinishTextInteractionInTextInputContext:context.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [webView _selectionAttributes] == _WKSelectionAttributeIsRange;
    }));
}

static NSUInteger markerCountInFrame(TestWKWebView *webView, WKFrameInfo *frame, NSString *markerType)
{
    RetainPtr script = [NSString stringWithFormat:@"internals.markerCountForNode(document.body.firstChild, '%@')", markerType];
    return [[webView objectByEvaluatingJavaScript:script.get() inFrame:frame] unsignedIntegerValue];
}

TEST(SiteIsolation, DictationAlternativesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>hello world&nbsp;</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 11)", _WKSelectionAttributeIsCaret);

    RetainPtr alternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:@"hello world" alternativeStrings:@[ @"👋🌎" ]]);
    [[webView textInputContentView] addTextAlternatives:alternatives.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return markerCountInFrame(webView.get(), childFrame.get(), @"dictationalternatives") == 1;
    }));

    [[webView textInputContentView] removeEmojiAlternatives];
    EXPECT_TRUE(Util::waitFor([&] {
        return !markerCountInFrame(webView.get(), childFrame.get(), @"dictationalternatives");
    }));
}

TEST(SiteIsolation, DictationStreamingOpacityInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>hello world</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 11)", _WKSelectionAttributeIsCaret);

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(6, 5)];
    EXPECT_TRUE(Util::waitFor([&] {
        return markerCountInFrame(webView.get(), childFrame.get(), @"dictationstreamingopacity") == 1;
    }));

    [[webView textInputContentView] _clearDictationStreamingOpacity];
    EXPECT_TRUE(Util::waitFor([&] {
        return !markerCountInFrame(webView.get(), childFrame.get(), @"dictationstreamingopacity");
    }));
}

TEST(SiteIsolation, InsertFinalDictationResultInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body, 0)", _WKSelectionAttributeIsCaret);

    // Typing right after a dictated word removes its alternatives, unless it's part of inserting the final dictation result.
    [[webView textInputContentView] willInsertFinalDictationResult];
    [webView insertText:@"wanna" alternatives:@[ @"want to" ]];
    [webView insertText:@"." alternatives:@[ ]];
    [[webView textInputContentView] didInsertFinalDictationResult];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"wanna."];
    }));
    EXPECT_EQ(1U, markerCountInFrame(webView.get(), childFrame.get(), @"dictationalternatives"));
}

// UIKit reads text and geometry around the insertion point to drive autocorrection, predictive text, and
// accessibility. With the caret in a cross-origin iframe, those requests must go to the iframe's process, and
// any rects in the reply must be in the main frame's coordinates rather than the iframe's.

static constexpr auto mainFrameWithPositionedCrossOriginIframe = "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<body style='margin: 0'><iframe id='iframe' style='position: absolute; left: 100px; top: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;
static constexpr auto editableIframeWithText = "<body contenteditable style='margin: 0; font-size: 20px;'>hello world</body>"_s;

// The iframe in mainFrameWithPositionedCrossOriginIframe covers (100, 100) to (500, 400) in the main frame.
static void expectRectInPositionedCrossOriginIframe(CGRect rect)
{
    EXPECT_FALSE(CGRectIsEmpty(rect));
    EXPECT_GE(CGRectGetMinX(rect), 100);
    EXPECT_GE(CGRectGetMinY(rect), 100);
    EXPECT_LE(CGRectGetMaxX(rect), 500);
    EXPECT_LE(CGRectGetMaxY(rect), 400);
}

// Focuses the iframe's editable body with a user gesture, so that it becomes the focused element and starts an
// input session, then puts the caret after "hello world". Focusing already leaves a caret, so the UI process may not
// have seen the caret move by the time this returns; callers that depend on UI-side editor state must wait for more.
static RetainPtr<TestInputDelegate> startInputSessionInCrossOriginIframe(TestWKWebView *webView, WKFrameInfo *frame)
{
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    __block bool didStartInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:^_WKFocusStartsInputSessionPolicy(WKWebView *, id<_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:frame];
    Util::run(&didStartInputSession);
    setSelectionInFrame(webView, frame, @"getSelection().setPosition(document.body.firstChild, 11)", _WKSelectionAttributeIsCaret);
    return inputDelegate;
}

TEST(SiteIsolation, AutocorrectionContextInCrossOriginIframe)
{
    // With the out-of-process keyboard, UIKit doesn't ask the web process for autocorrection context.
    if ([UIKeyboard usesInputSystemUI])
        return;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr inputDelegate = startInputSessionInCrossOriginIframe(webView.get(), childFrame.get());

    // The UI process caches the context the iframe sent when its body was focused, and only asks a web process
    // again after it sees the selection change. Switching from a caret to a range gives us something to wait for.
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setBaseAndExtent(document.body.firstChild, 6, document.body.firstChild, 11)", _WKSelectionAttributeIsRange);

    // The request is answered by a separate message that the UI process waits for, so the wait has to be on the
    // process that got the request. Otherwise the wait times out and the context comes back empty.
    auto context = [webView autocorrectionContext];
    EXPECT_WK_STREQ("world", context.selectedText);
    EXPECT_TRUE(context.contextBeforeSelection.startsWith("hello"_s));
}

TEST(SiteIsolation, AutocorrectionRectsInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr inputDelegate = startInputSessionInCrossOriginIframe(webView.get(), childFrame.get());

    auto [firstRect, lastRect] = [webView autocorrectionRectsForString:@"world"];
    expectRectInPositionedCrossOriginIframe(firstRect);
    expectRectInPositionedCrossOriginIframe(lastRect);
}

TEST(SiteIsolation, AccessibilityRectsAtSelectionOffsetInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    __block bool done = false;
    __block RetainPtr<NSArray<NSValue *>> rects;
    [webView _accessibilityRetrieveRectsAtSelectionOffset:0 withText:@"hello" completionHandler:^(NSArray<NSValue *> *result) {
        rects = result;
        done = true;
    }];
    Util::run(&done);

    ASSERT_GE([rects count], 1U);
    expectRectInPositionedCrossOriginIframe([rects firstObject].CGRectValue);
}

#if HAVE(UI_WK_DOCUMENT_CONTEXT)

TEST(SiteIsolation, DocumentEditingContextInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr inputDelegate = startInputSessionInCrossOriginIframe(webView.get(), childFrame.get());

    RetainPtr request = adoptNS([[UIWKDocumentRequest alloc] init]);
    [request setFlags:UIWKDocumentRequestText | UIWKDocumentRequestRects];
    [request setSurroundingGranularity:UITextGranularityParagraph];
    [request setGranularityCount:1];
    RetainPtr context = [webView synchronouslyRequestDocumentContext:request.get()];

    EXPECT_TRUE([[context contextBefore] isKindOfClass:NSString.class]);
    EXPECT_WK_STREQ("hello world", (NSString *)[context contextBefore]);
    RetainPtr<NSArray<NSValue *>> characterRects = [context characterRectsForCharacterRange:NSMakeRange(0, 1)];
    ASSERT_GE([characterRects count], 1U);
    expectRectInPositionedCrossOriginIframe([characterRects firstObject].CGRectValue);
}

#endif // HAVE(UI_WK_DOCUMENT_CONTEXT)

// Point-based selection. The UI process hands these messages a point in web-view coordinates. The iOS
// selection gestures hit-test it and re-dispatch into the cross-origin iframe under it, so they work whether
// or not that iframe is focused; `CharacterIndexForPointAsync` resolves it in the focused frame's process.

static int selectionAnchorOffsetInFrame(TestWKWebView *webView, WKFrameInfo *frame)
{
    return [[webView objectByEvaluatingJavaScript:@"getSelection().anchorOffset" inFrame:frame] intValue];
}

TEST(SiteIsolation, SelectPositionAtBoundaryInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);
    ASSERT_EQ(0, selectionAnchorOffsetInFrame(webView.get(), childFrame.get()));

    // Starting from a point at "h", the next word boundary forward is the end of "hello".
    __block bool done = false;
    [[webView textInputContentView] selectPositionAtBoundary:UITextGranularityWord inDirection:UITextStorageDirectionForward fromPoint:pointAtCharacterInIframe(webView.get(), childFrame.get(), 0) completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    EXPECT_TRUE(Util::waitFor([&] {
        return selectionAnchorOffsetInFrame(webView.get(), childFrame.get()) == 5;
    }));
    EXPECT_EQ(5, selectionAnchorOffsetInFrame(webView.get(), childFrame.get()));
}

TEST(SiteIsolation, SelectWithTwoTouchesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);

    // Two touches at "h" and at "w" must select everything between them, in the iframe.
    CGPoint from = pointAtCharacterInIframe(webView.get(), childFrame.get(), 0);
    CGPoint to = pointAtCharacterInIframe(webView.get(), childFrame.get(), 6);
    [[webView textInputContentView] changeSelectionWithTouchesFrom:from to:to withGesture:UIWKGestureLoupe withState:UIGestureRecognizerStateEnded];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"hello "];
    }));
    EXPECT_WK_STREQ("hello ", [webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()]);
}

static NSArray<_WKTextInputContext *> *synchronouslyRequestTextInputContextsInRect(WKWebView *webView, CGRect rect)
{
    __block RetainPtr<NSArray<_WKTextInputContext *>> result;
    __block bool done = false;
    [webView _requestTextInputContextsInRect:rect completionHandler:^(NSArray<_WKTextInputContext *> *contexts) {
        result = contexts;
        done = true;
    }];
    Util::run(&done);
    return result.autorelease();
}

static UIResponder<UITextInput> *synchronouslyFocusTextInputContext(WKWebView *webView, _WKTextInputContext *context, CGPoint point)
{
    __block UIResponder<UITextInput> *result = nil;
    __block bool done = false;
    [webView _focusTextInputContext:context placeCaretAt:point completionHandler:^(UIResponder<UITextInput> *responder) {
        result = responder;
        done = true;
    }];
    Util::run(&done);
    return result;
}

TEST(SiteIsolation, RequestTextInputContextsInRectCoveringCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body><input type='password'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);

    EXPECT_EQ(1UL, [contexts count]);
}

static RetainPtr<NSArray<_WKTextInputContext *>> textInputContextsSortedByX(TestWKWebView *webView, CGRect rect)
{
    RetainPtr contexts = [webView synchronouslyRequestTextInputContextsInRect:rect];
    return [contexts sortedArrayUsingComparator:^NSComparisonResult(_WKTextInputContext *a, _WKTextInputContext *b) {
        if (CGRectGetMinX(a.boundingRect) == CGRectGetMinX(b.boundingRect))
            return NSOrderedSame;
        return CGRectGetMinX(a.boundingRect) < CGRectGetMinX(b.boundingRect) ? NSOrderedAscending : NSOrderedDescending;
    }];
}

TEST(SiteIsolation, RequestTextInputContextsInRectCoveringOffsetCrossOriginIframes)
{
    static constexpr auto mainFrameHTML = "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body { margin: 0; } iframe { position: absolute; top: 200px; width: 300px; height: 150px; border: none; }</style>"
        "<iframe style='left: 0' src='https://a.com/iframe'></iframe>"
        "<iframe style='left: 400px' src='https://b.com/iframe'></iframe>"_s;
    static constexpr auto iframeHTML = "<style>body { margin: 0; } input { position: absolute; left: 20px; top: 30px; width: 100px; height: 40px; box-sizing: border-box; }</style>"
        "<input type='text'>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr contexts = textInputContextsSortedByX(webView.get(), [webView bounds]);
    ASSERT_EQ(2U, [contexts count]);
    EXPECT_EQ(CGRectMake(20, 230, 100, 40), [contexts objectAtIndex:0].boundingRect);
    EXPECT_EQ(CGRectMake(420, 230, 100, 40), [contexts objectAtIndex:1].boundingRect);

    contexts = textInputContextsSortedByX(webView.get(), CGRectMake(410, 220, 120, 60));
    ASSERT_EQ(1U, [contexts count]);
    EXPECT_EQ(CGRectMake(420, 230, 100, 40), [contexts objectAtIndex:0].boundingRect);
}

TEST(SiteIsolation, FocusTextInputContextInCrossOriginIframeMovesCaret)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><input id='iframeInput' value='hello world'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);
    ASSERT_EQ(1UL, contexts.count);

    RetainPtr<_WKTextInputContext> iframeField = contexts[0];
    EXPECT_NOT_NULL(synchronouslyFocusTextInputContext(webView.get(), iframeField.get(), [iframeField boundingRect].origin));

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName" inFrame:childFrame.get()]);
    EXPECT_WK_STREQ("iframeInput", [webView stringByEvaluatingJavaScript:@"document.activeElement.id" inFrame:childFrame.get()]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, FocusTextInputContextInOffsetCrossOriginIframeMovesCaret)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe style='margin-left: 100px; margin-top: 50px;' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><input id='iframeInput' value='hello world'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);
    ASSERT_EQ(1UL, contexts.count);

    RetainPtr<_WKTextInputContext> iframeField = contexts[0];
    EXPECT_NOT_NULL(synchronouslyFocusTextInputContext(webView.get(), iframeField.get(), [iframeField boundingRect].origin));

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName" inFrame:childFrame.get()]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, SetCanShowPlaceholderForElementInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body><input id='iframeInput' placeholder='ph''></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrame = [webView firstChildFrame];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('iframeInput').focus()" inFrame:childFrame.get()];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);
    ASSERT_EQ(1UL, contexts.count);

    RetainPtr<_WKTextInputContext> iframeField = contexts[0];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"document.activeElement.matches(':placeholder-shown')" inFrame:childFrame.get()] boolValue]);
    [webView _willBeginTextInteractionInTextInputContext:iframeField.get()];
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"document.activeElement.matches(':placeholder-shown')" inFrame:childFrame.get()] boolValue]);
    [webView _didFinishTextInteractionInTextInputContext:iframeField.get()];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"document.activeElement.matches(':placeholder-shown')" inFrame:childFrame.get()] boolValue]);
}

TEST(SiteIsolation, SelectPositionAtBoundaryInUnfocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrame = [webView firstChildFrame];

    __block bool done = false;
    [[webView textInputContentView] selectPositionAtBoundary:UITextGranularityWord inDirection:UITextStorageDirectionForward fromPoint:pointAtCharacterInIframe(webView.get(), childFrame.get(), 0) completionHandler:^{
        done = true;
    }];
    EXPECT_TRUE(Util::runFor(&done, 5_s));

    EXPECT_TRUE(Util::waitFor([&] {
        return selectionAnchorOffsetInFrame(webView.get(), childFrame.get()) == 5;
    }));
}

TEST(SiteIsolation, SelectWithTwoTouchesInUnfocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrame = [webView firstChildFrame];

    CGPoint from = pointAtCharacterInIframe(webView.get(), childFrame.get(), 0);
    CGPoint to = pointAtCharacterInIframe(webView.get(), childFrame.get(), 6);
    [[webView textInputContentView] changeSelectionWithTouchesFrom:from to:to withGesture:UIWKGestureLoupe withState:UIGestureRecognizerStateEnded];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"hello "];
    }));
}

#if ENABLE(ORIENTATION_EVENTS)

TEST(SiteIsolation, CrossSiteIFrameReceivesOrientationChangeEvent)
{
    auto mainFrameHTML = "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<script>window.addEventListener('orientationchange', () => { window.gotOrientationChange = true; });</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameHTML } },
        { "/subframe"_s, { subFrameHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView _setInterfaceOrientationOverride:UIInterfaceOrientationLandscapeRight];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.gotOrientationChange === true" inFrame:childFrame.get()] boolValue];
    }));
}

#endif // ENABLE(ORIENTATION_EVENTS)

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
