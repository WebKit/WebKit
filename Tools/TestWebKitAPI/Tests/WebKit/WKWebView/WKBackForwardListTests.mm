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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/SiteIsolationUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKBackForwardListItemPrivate.h>
#import <WebKit/WKBackForwardListPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKSessionState.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

static NSString *loadableURL1 = @"data:text/html,no%20error%20A";
static NSString *loadableURL2 = @"data:text/html,no%20error%20B";
static NSString *loadableURL3 = @"data:text/html,no%20error%20C";

TEST(WKBackForwardList, RemoveCurrentItem)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL3]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL3.UTF8String);

    _WKSessionState *sessionState = [webView _sessionStateWithFilter:^BOOL(WKBackForwardListItem *item)
    {
        return [item.URL isEqual:[NSURL URLWithString:loadableURL2]];
    }];

    [webView _restoreSessionState:sessionState andNavigate:NO];

    WKBackForwardList *newList = [webView backForwardList];

    EXPECT_EQ((NSUInteger)0, newList.backList.count);
    EXPECT_EQ((NSUInteger)0, newList.forwardList.count);
    EXPECT_STREQ([[newList.currentItem URL] absoluteString].UTF8String, loadableURL2.UTF8String);
}

TEST(WKBackForwardList, RemoveCurrentItemWithNeighbors)
{
    // Regression test: when the current item is filtered out, the currentIndex should
    // be decremented (so the predecessor becomes current), not left unchanged (which
    // would make the successor current).
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL3]]];
    [webView _test_waitForDidFinishNavigation];

    [webView goBack];
    [webView _test_waitForDidFinishNavigation];

    // entries = [A, B(current), C], currentIndex = 1
    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)1, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL2.UTF8String);

    // Filter out B (the current item), keeping A and C.
    _WKSessionState *sessionState = [webView _sessionStateWithFilter:^BOOL(WKBackForwardListItem *item) {
        return ![item.URL isEqual:[NSURL URLWithString:loadableURL2]];
    }];

    [webView _restoreSessionState:sessionState andNavigate:NO];

    // Restored list should be [A, C] with A as current (predecessor), not C (successor).
    WKBackForwardList *newList = [webView backForwardList];
    EXPECT_EQ((NSUInteger)0, newList.backList.count);
    EXPECT_EQ((NSUInteger)1, newList.forwardList.count);
    EXPECT_STREQ([[newList.currentItem URL] absoluteString].UTF8String, loadableURL1.UTF8String);
}

TEST(WKBackForwardList, CanNotGoBackAfterRestoringEmptySessionState)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ(NO, [webView canGoForward]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);

    RetainPtr singlePageWebView = adoptNS([[WKWebView alloc] init]);

    [singlePageWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [singlePageWebView _test_waitForDidFinishNavigation];

    [webView _restoreSessionState:[singlePageWebView _sessionState] andNavigate:NO];

    WKBackForwardList *newList = [webView backForwardList];

    EXPECT_EQ(NO, [webView canGoBack]);
    EXPECT_EQ(NO, [webView canGoForward]);
    EXPECT_EQ((NSUInteger)0, newList.backList.count);
    EXPECT_EQ((NSUInteger)0, newList.forwardList.count);
}

TEST(WKBackForwardList, RestoringNilSessionState)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ(NO, [webView canGoForward]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);

    RetainPtr singlePageWebView = adoptNS([[WKWebView alloc] init]);

    [singlePageWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [singlePageWebView _test_waitForDidFinishNavigation];

    [webView _restoreSessionState:nil andNavigate:NO];

    WKBackForwardList *newList = [webView backForwardList];

    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ(NO, [webView canGoForward]);
    EXPECT_EQ((NSUInteger)1, newList.backList.count);
    EXPECT_EQ((NSUInteger)0, newList.forwardList.count);
}

static bool done;
static size_t navigations;

@interface AsyncPolicyDecisionDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation AsyncPolicyDecisionDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation
{
    if (navigations++)
        done = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    dispatch_async(mainDispatchQueueSingleton(), ^{
        decisionHandler(WKNavigationActionPolicyAllow);
    });
}

@end

TEST(WKBackForwardList, WindowLocationAsyncPolicyDecision)
{
    NSURL *simple = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    NSURL *simple2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr delegate = adoptNS([[AsyncPolicyDecisionDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>window.location='simple.html'</script>" baseURL:simple2];
    TestWebKitAPI::Util::run(&done);
    EXPECT_STREQ(webView.get().backForwardList.currentItem.URL.absoluteString.UTF8String, simple.absoluteString.UTF8String);
}

TEST(WKBackForwardList, InteractionStateRestoration)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr url3 = [NSBundle.test_resourcesBundle URLForResource:@"simple3" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url3.get()]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);

    id interactionState = [webView interactionState];
    RetainPtr<NSURL> temporaryFile = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSUUID UUID].UUIDString] isDirectory:NO];
    NSError *error = nil;
    RetainPtr<NSData> archivedInteractionState = [NSKeyedArchiver archivedDataWithRootObject:interactionState requiringSecureCoding:YES error:&error];
    EXPECT_TRUE(!error);
    interactionState = nil;
    [archivedInteractionState writeToURL:temporaryFile.get() options:NSDataWritingAtomic error:&error];
    archivedInteractionState = nil;
    EXPECT_TRUE(!error);

    webView = adoptNS([[WKWebView alloc] init]);

    archivedInteractionState = [NSData dataWithContentsOfURL:temporaryFile.get()];
    interactionState = [NSKeyedUnarchiver unarchivedObjectOfClass:[(id)[webView interactionState] class] fromData:archivedInteractionState.get() error:&error];
    EXPECT_TRUE(!error);

    [webView setInteractionState:interactionState];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *newList = [webView backForwardList];

    EXPECT_EQ((NSUInteger)2, newList.backList.count);
    EXPECT_EQ((NSUInteger)0, newList.forwardList.count);
    EXPECT_STREQ([[newList.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);

    done = false;
    [webView evaluateJavaScript:@"document.body.innerText" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        NSString* bodyText = result;
        EXPECT_WK_STREQ(@"Third simple HTML file.", bodyText);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView goBack];
    [webView _test_waitForDidFinishNavigation];

    done = false;
    [webView evaluateJavaScript:@"document.body.innerText" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        NSString* bodyText = result;
        EXPECT_WK_STREQ(@"Second simple HTML file.", bodyText);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView goBack];
    [webView _test_waitForDidFinishNavigation];

    done = false;
    [webView evaluateJavaScript:@"document.body.innerText" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        NSString* bodyText = result;
        EXPECT_WK_STREQ(@"Simple HTML file.", bodyText);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKBackForwardList, InteractionStateRestorationNil)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr url3 = [NSBundle.test_resourcesBundle URLForResource:@"simple3" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url3.get()]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);

    [webView setInteractionState:nil];

    list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);
}

TEST(WKBackForwardList, InteractionStateRestorationInvalid)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr url3 = [NSBundle.test_resourcesBundle URLForResource:@"simple3" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url3.get()]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);

    NSString *invalidState = @"foo";
    [webView setInteractionState:invalidState];

    list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);
}

// Restoring state with multiple items causes the Swift restoreFromState loop to iterate more than
// once, which can trigger an ASAN false positive if Swift reuses a stack slot that C++ poisoned
// via leakRef in the previous iteration.
TEST(WKBackForwardList, InteractionStateRestorationMultipleItems)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [webView _test_waitForDidFinishNavigation];

    id interactionState = [webView interactionState];

    webView = adoptNS([[WKWebView alloc] init]);
    [webView setInteractionState:interactionState];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)1, list.get().backList.count);
    EXPECT_EQ((NSUInteger)0, list.get().forwardList.count);
    EXPECT_STREQ([[list.get().currentItem URL] absoluteString].UTF8String, url2.get().absoluteString.UTF8String);
    EXPECT_STREQ([[list.get().backList.firstObject URL] absoluteString].UTF8String, url1.get().absoluteString.UTF8String);
}

@interface WKBackForwardNavigationDelegate : NSObject <WKNavigationDelegatePrivate>
- (void)waitForDidFinishNavigationOrDidSameDocumentNavigation;
@end

static RetainPtr<WKNavigation> lastNavigation;

@implementation WKBackForwardNavigationDelegate {
    bool _navigated;
    bool _didFinishNavigation;
}

- (instancetype) init
{
    self = [super init];
    return self;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _navigated = true;
    _didFinishNavigation = true;
    lastNavigation = navigation;
}

- (void)_webView:(WKWebView *)webView navigation:(WKNavigation *)navigation didSameDocumentNavigation:(_WKSameDocumentNavigationType)navigationType
{
    if (navigationType == _WKSameDocumentNavigationTypeSessionStatePush || navigationType == _WKSameDocumentNavigationTypeSessionStatePop) {
        _navigated = true;
        lastNavigation = navigation;
    }
}

- (void)waitForDidFinishNavigationOrDidSameDocumentNavigation
{
    _navigated = false;
    TestWebKitAPI::Util::run(&_navigated);
}

- (void)waitForDidFinishNavigation
{
    _didFinishNavigation = false;
    TestWebKitAPI::Util::run(&_didFinishNavigation);
}

@end

TEST(WKBackForwardList, BackSwipeNavigationSkipsItemsWithoutUserGesture)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#b');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#c');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // 4 items in the back list, but most should be invisible because of the user gesture flag.
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // Navigating back via a swipe gesture should skip those back/forward list items without a user gesture.
    [webView _beginBackSwipeForTesting];
    [webView _completeBackSwipeForTesting];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);

    // 4 items in the forward list, but most should be invisible because of the user gesture flag.
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);
    EXPECT_EQ([webView backForwardList].backList.count, 0U);
}

TEST(WKBackForwardList, BackSwipeNavigationDoesNotSkipItemsWithUserGesture)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list item with a user gesture.
    [webView evaluateJavaScript:@"history.pushState(null, document.title, location.pathname + '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, 2U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // Navigating back via a swipe gesture should skip those back/forward list items without a user gesture.
    [webView _beginBackSwipeForTesting];
    [webView _completeBackSwipeForTesting];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url2 absoluteString].UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);
}

static void runBackForwardNavigationSkipsItemsWithoutUserGestureTest(Function<void(WKWebView *, ASCIILiteral destination)>&& navigate)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr url3 = [NSBundle.test_resourcesBundle URLForResource:@"simple3" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Test case:
    // url1 -> url2 -> url2#a (no user gesture) -> url2#b (no user gesture) -> url2#c (no user gesture) -> url3.

    // Add back/forward list items without user gestures.
    navigate(webView.get(), "location.pathname + '#a'"_s);
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    EXPECT_TRUE(webView.get().backForwardList.currentItem._wasCreatedByJSWithoutUserInteraction);
    RetainPtr expectedURLString = makeString(String([url2 absoluteString]), "#a"_s).createNSString();
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.get().UTF8String);

    navigate(webView.get(), "location.pathname + '#b'"_s);
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    EXPECT_TRUE(webView.get().backForwardList.currentItem._wasCreatedByJSWithoutUserInteraction);
    expectedURLString = makeString(String([url2 absoluteString]), "#b"_s).createNSString();
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.get().UTF8String);

    navigate(webView.get(), "location.pathname + '#c'"_s);
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    EXPECT_TRUE(webView.get().backForwardList.currentItem._wasCreatedByJSWithoutUserInteraction);
    expectedURLString = makeString(String([url2 absoluteString]), "#c"_s).createNSString();
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.get().UTF8String);

    [webView loadRequest:[NSURLRequest requestWithURL:url3.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE(webView.get().backForwardList.currentItem._wasCreatedByJSWithoutUserInteraction);

    // url1 -> url2 -> url2#a (no user gesture) -> url2#b (no user gesture) -> url2#c (no user gesture) -> url3 **
    EXPECT_EQ([webView backForwardList].backList.count, 2U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // We are now on url3. Let's go back.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // url1 -> url2 -> url2#a (no user gesture) -> url2#b (no user gesture) -> url2#c (no user gesture) ** -> url3
    expectedURLString = makeString(String([url2 absoluteString]), "#c"_s).createNSString();
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.get().UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);

    // Let's go back again.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // We should have skipped over url2#b, url2#a and url2, to end up on url1.
    // url1 ** -> url2 -> url2#a (no user gesture) -> url2#b (no user gesture) -> url2#c (no user gesture) -> url3
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 2U);

    // Now let's go forward.
    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // We should get to the latest url2 URL, that is url2#c.
    // url1 -> url2 -> url2#a (no user gesture) -> url2#b (no user gesture) -> url2#c (no user gesture) ** -> url3
    expectedURLString = makeString(String([url2 absoluteString]), "#c"_s).createNSString();
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.get().UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);

    // Let's go forward again.
    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // We should now be on url3.
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url3 absoluteString].UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 2U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // Navigating via the JS API shouldn't skip those back/forward list items.
    [webView _evaluateJavaScriptWithoutUserGesture:@"history.back();" completionHandler:^(id, NSError *) { }];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    expectedURLString = makeString(String([url2 absoluteString]), "#c"_s).createNSString();
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.get().UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.back();" completionHandler:^(id, NSError *) { }];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String([url2 absoluteString]), "#b"_s).createNSString();
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.get().UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);
}

TEST(WKBackForwardList, BackForwardNavigationSkipsItemsWithoutUserGesturePushState)
{
    runBackForwardNavigationSkipsItemsWithoutUserGestureTest([](WKWebView* webView, ASCIILiteral destination) {
        [webView _evaluateJavaScriptWithoutUserGesture:makeString("history.pushState(null, document.title, "_s, destination, ");"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationSkipsItemsWithoutUserGestureFragment)
{
    runBackForwardNavigationSkipsItemsWithoutUserGestureTest([](WKWebView* webView, ASCIILiteral destination) {
        [webView _evaluateJavaScriptWithoutUserGesture:makeString("location.href = "_s, destination, ";"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationSkipsItemsWithoutUserGesturePushStateAfterEvaluateJS)
{
    runBackForwardNavigationSkipsItemsWithoutUserGestureTest([](WKWebView* webView, ASCIILiteral destination) {
        // Do a call to evaluateJavaScript (with user gesture) *BEFORE* the pushState and make sure it doesn't count
        // as a user gesture for the pushState().
        __block bool didRunScript = false;
        [webView evaluateJavaScript:@"window.foo = 1;" completionHandler:^(id, NSError *) {
            didRunScript = true;
        }];
        TestWebKitAPI::Util::run(&didRunScript);
        [webView _evaluateJavaScriptWithoutUserGesture:makeString("history.pushState(null, document.title, "_s, destination, ");"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationSkipsItemsWithoutUserGestureSubframe)
{
    TestWebKitAPI::HTTPServer server({
        { "/source.html"_s, { "foo"_s } },
        { "/destination.html"_s, { "<iframe src='iframe.html'></iframe>"_s } },
        { "/iframe.html"_s, { "<script>onload = () => { setTimeout(() => { history.pushState(null, document.title, '#'); }, 0); };</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] init]);

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    // After this load, the bf list is:
    // (source)*
    [webView loadRequest:server.request("/source.html"_s)];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // After this load, the bf list is:
    // (source) - (destination)*
    [webView loadRequest:server.request("/destination.html"_s)];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Wait for the subframe to call pushState(). The API-visible backList is filtered
    // so we poll history.length which reflects the unfiltered count.
    while ([[webView objectByEvaluatingJavaScript:@"history.length"] integerValue] < 3)
        TestWebKitAPI::Util::spinRunLoop();

    // After the push state without user gesture in the iframe, the bf list is:
    // (source) - (destination) - [destination`]*
    // Because the current item has no user gesture, going back would skip the previous item
    // that *does* have a user gesture. So the back list count should be 1, not 2.
    EXPECT_EQ([webView backForwardList].backList.count, 1U);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // We should be back to source.html since we would have ignored the history item
    // added by the subframe without user interaction.
    // The bf list should look like:
    // (source)* - (destination) - [destination`]
    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, server.request("/source.html"_s).URL.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Going forward skips the middle item (which has a user gesture) to the final item.
    // So the list is once again:
    // (source) - (destination) - [destination`]*
    // Like before, vecause the current item has no user gesture, going back would skip the previous item
    // that *does* have a user gesture. So the back list count should be 1, not 2.
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, server.request("/destination.html"_s).URL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationSkipsClientSideRedirectWithCOOP)
{
    TestWebKitAPI::HTTPServer server({
        { "/source.html"_s, { "<a id='testLink' href='form.html'>click me</a>"_s } },
        { "/form.html"_s, { "<body><form id='testForm' method='POST' action='redirect.html'><input type='submit' value='submit'></form><script>document.getElementById('testForm').submit()</script></body>"_s } },
        { "/redirect.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "cross-origin-opener-policy"_s, "same-origin"_s } }, "<head><meta http-equiv='refresh' content='0; url=destination.html'></head>"_s } },
        { "/destination.html"_s, { "foo"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https);

    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.request("/source.html"_s)];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView evaluateJavaScript:@"document.getElementById('testLink').click()" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, server.request("/form.html"_s).URL.absoluteString.UTF8String);

    // Wait for form submission to happen.
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, server.request("/redirect.html"_s).URL.absoluteString.UTF8String);

    // Wait for redirect to finish.
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, server.request("/destination.html"_s).URL.absoluteString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, server.request("/source.html"_s).URL.absoluteString.UTF8String);
}

static void runBackForwardNavigationDoesNotSkipItemsWithUserGestureTest(Function<void(WKWebView *, ASCIILiteral fragment)>&& navigate)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    // Test case: url1 -> url2 -> url2#a (with user gesture)
    // No item should be skipped when navigating backwards or forwards.

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    navigate(webView.get(), "#a"_s);
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    RetainPtr expectedURLString = makeString(String([url2 absoluteString]), "#a"_s).createNSString();
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.get().UTF8String);

    RetainPtr lastURL = [webView URL];
    EXPECT_FALSE([lastURL isEqual:url2]);

    EXPECT_FALSE(webView.get().backForwardList.backItem._wasCreatedByJSWithoutUserInteraction);
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, [url2 absoluteString].UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url2 absoluteString].UTF8String);

    EXPECT_FALSE(webView.get().backForwardList.backItem._wasCreatedByJSWithoutUserInteraction);
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url2 absoluteString].UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String([url2 absoluteString]), "#a"_s).createNSString();
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.get().UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [lastURL absoluteString].UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithUserGesturePushState)
{
    runBackForwardNavigationDoesNotSkipItemsWithUserGestureTest([](WKWebView *webView, ASCIILiteral fragment) {
        [webView evaluateJavaScript:makeString("history.pushState(null, document.title, location.pathname + '"_s, fragment, "');"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithUserGestureFragment)
{
    runBackForwardNavigationDoesNotSkipItemsWithUserGestureTest([](WKWebView *webView, ASCIILiteral fragment) {
        [webView evaluateJavaScript:makeString("location.href = location.pathname + '"_s, fragment, "';"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsFromLoadRequest)
{
    runBackForwardNavigationDoesNotSkipItemsWithUserGestureTest([](WKWebView *webView, ASCIILiteral fragment) {
        auto newURLString = makeString(String([webView URL].absoluteString), fragment);
        [webView loadRequest:adoptNS([[NSURLRequest alloc] initWithURL:adoptNS([[NSURL alloc] initWithString:newURLString.createNSString().get()]).get()]).get()];
    });
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithRecentUserGesturePushState)
{
    runBackForwardNavigationDoesNotSkipItemsWithUserGestureTest([](WKWebView *webView, ASCIILiteral fragment) {
        // Call pushState() in a setTimeout() so that it has a recent user gesture but not a current one.
        [webView evaluateJavaScript:makeString("setTimeout(() => { history.pushState(null, document.title, location.pathname + '"_s, fragment, "'); }, 0);"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithRecentUserGestureFragment)
{
    runBackForwardNavigationDoesNotSkipItemsWithUserGestureTest([](WKWebView *webView, ASCIILiteral fragment) {
        // Do fragment navigation in a setTimeout() so that it has a recent user gesture but not a current one.
        [webView evaluateJavaScript:makeString("setTimeout(() => { location.href = location.pathname + '"_s, fragment, "'; }, 0);"_s).createNSString().get() completionHandler:nil];
    });
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipUpdatedItemWithRecentUserGesture)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"fragment-navigation-before-load-event" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    // Page navigated to #fragment before the load event.
    RetainPtr expectedURLString = makeString(String([url2 absoluteString]), "#fragment"_s).createNSString();
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.get().UTF8String);

    // Navigate with a user gesture.
    [webView evaluateJavaScript:@"location.href = location.pathname + '#otherFragment';" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Should go back to #fragment.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.get().UTF8String);
}

TEST(WKBackForwardList, BackNavigationHijacking)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, null, '');" completionHandler:nil];
    __block bool ranJS = false;
    [webView _evaluateJavaScriptWithoutUserGesture:@"onpopstate = (e) => { history.forward(); };false" completionHandler:^(id, NSError *) {
        ranJS = true;
    }];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    TestWebKitAPI::Util::run(&ranJS);

    // At this point, the bf list is:
    // url1 - url1` (no user gesture)*

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url2 absoluteString].UTF8String);

    // At this point, the bf list is:
    // url1 - url1` (no user gesture) - url2*

    EXPECT_TRUE(webView.get().backForwardList.backItem._wasCreatedByJSWithoutUserInteraction);
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);

    // At this point, the bf list is:
    // url1* - url1` (no user gesture) - url2

    TestWebKitAPI::Util::spinRunLoop(10);
    usleep(100000);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);
}

TEST(WKBackForwardList, BackForwardListRemoveAndAddSubframes)
{
    auto indexHTML = "<iframe id='1' src='/frame1'></iframe>"
        "<iframe id='2' src='/frame2'></iframe>"_s;
    TestWebKitAPI::HTTPServer server({
        { "/index"_s, { indexHTML } },
        { "/frame1"_s, { ""_s } },
        { "/frame2"_s, { "<script> alert('frame2'); </script>"_s } },
        { "/frame3"_s, { "<script> alert('frame3'); </script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https);
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:server.request("/index"_s)];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "frame2");

    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        [webView evaluateJavaScript:@"location.href = '/frame3'" inFrame:mainFrame.childFrames[1].info inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    }];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "frame3");

    auto removeAndAddFrame = @"let frame = document.getElementById('1');"
        "frame.parentNode.removeChild(frame);"
        "let newFrame = document.createElement('iframe');"
        "newFrame.src = '/frame1';"
        "document.body.appendChild(newFrame);";
    __block bool done = false;
    [webView evaluateJavaScript:removeAndAddFrame completionHandler:^(id, NSError *) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "frame2");

    __block auto expectedFrameURL = server.request("/frame2"_s).URL.absoluteString.UTF8String;
    [webView evaluateJavaScript:@"document.getElementById('2').contentWindow.location.href" completionHandler:^(id result, NSError *) {
        EXPECT_WK_STREQ(expectedFrameURL, [result UTF8String]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKBackForwardList, SessionStateTitleTruncation)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "<script>document.title='a'.repeat(10000);window.history.pushState({}, '', window.location+'?a=b');</script>"_s } }
    });

    RetainPtr webView = adoptNS([WKWebView new]);
    [webView loadRequest:server.request()];
    while (!webView.get().canGoBack)
        TestWebKitAPI::Util::spinRunLoop();
    while (webView.get()._sessionState.data.length < 1000u)
        TestWebKitAPI::Util::spinRunLoop();
    _WKSessionState *sessionState = webView.get()._sessionState;
    NSData *stateData = sessionState.data;
    EXPECT_LT(stateData.length, 2000u);
}

TEST(WKBackForwardList, RestoreSessionStateResetProvisionalItem)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] init]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView synchronouslyGoBack];
    [webView synchronouslyGoForward];

    RetainPtr sessionState = [webView _sessionStateWithFilter:^BOOL(WKBackForwardListItem *item) {
        return [item.URL isEqual:[NSURL URLWithString:loadableURL1]];
    }];
    [webView _restoreSessionState:sessionState.get() andNavigate:NO];
    [[webView backForwardList] currentItem];
}

TEST(WKBackForwardList, GoBackToPageAfterNavigatingIframeAndRestoringSession)
{
    TestWebKitAPI::HTTPServer server({
        { "/example"_s, { "<iframe src='/a'></iframe>"_s } },
        { "/a"_s, { "<script>alert('a');</script>"_s } },
        { "/b"_s, { "<script>alert('b');</script>"_s } },
    });
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    [webView loadRequest:server.request("/example"_s)];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    [webView evaluateJavaScript:@"document.querySelector('iframe').src = '/b';" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");

    [webView loadRequest:server.requestWithLocalhost("/example"_s)];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    [webView _restoreSessionState:[webView _sessionState] andNavigate:NO];
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    EXPECT_WK_STREQ([webView URL].absoluteString, server.request("/example"_s).URL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, RestoreSessionForSiteWithCOOP)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { { { "Content-Type"_s, "text/html"_s }, { "cross-origin-opener-policy"_s, "same-origin"_s } }, "<p>main</p><iframe src='/frame'></iframe>"_s } },
        { "/frame"_s, { "<p>iframe</p><script>alert(location.href + ' is loaded');</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://example.com/frame is loaded");

    RetainPtr webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView2.get().navigationDelegate = navigationDelegate.get();
    [webView2 _restoreSessionState:[webView _sessionState] andNavigate:YES];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "https://example.com/frame is loaded");
}

enum class ShouldEnablePageCache : bool { No, Yes };
static void runGoBackAfterNavigatingSameSiteIframe(ShouldEnablePageCache shouldEnablePageCache)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { "<iframe id='iframe' src='/frame'></iframe>"_s } },
        { "/frame"_s, { "hi"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().pageCacheEnabled = shouldEnablePageCache == ShouldEnablePageCache::Yes;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    [configuration setProcessPool:processPool.get()];
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    // FIXME: Remove once the back-forward cache is enabled for site isolation: rdar://161762363.
    const bool backForwardCacheEnabled = isUsingBackForwardCache(webView.get());
    webView.get().navigationDelegate = navigationDelegate.get();
    static bool didCommitLoadForAllFrames = false;
    static unsigned expectedCommittedFrameSize = 2;
    static Vector<RetainPtr<WKFrameInfo>> committedFrames;
    navigationDelegate.get().didCommitLoadWithRequestInFrame = makeBlockPtr([&](WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo) {
        committedFrames.append(frameInfo);
        if (committedFrames.size() == expectedCommittedFrameSize)
            didCommitLoadForAllFrames = true;
    }).get();

    // After initial loading, page has a main frame and a same-site iframe.
    didCommitLoadForAllFrames = false;
    expectedCommittedFrameSize = 2;
    committedFrames.clear();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_TRUE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://example.com/main");
    EXPECT_FALSE([committedFrames[1] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[1] request].URL.absoluteString, "https://example.com/frame");
    committedFrames.clear();
    didCommitLoadForAllFrames = false;

    // Navigate iframe cross-site.
    expectedCommittedFrameSize = 1;
    [webView evaluateJavaScript:@"document.getElementById('iframe').src = 'https://frame.com/frame'" completionHandler:nil];
    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_FALSE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://frame.com/frame");
    committedFrames.clear();
    didCommitLoadForAllFrames = false;

    // Navigate main frame cross-site.
    expectedCommittedFrameSize = 1;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example2.com/frame"]]];
    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_TRUE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://example2.com/frame");
    committedFrames.clear();
    didCommitLoadForAllFrames = false;

    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ([webView backForwardList].backList.count, 2U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // Navigate main frame back.
    // For page cache case, iframe is not reloaded, so there is only one commit.
    expectedCommittedFrameSize = backForwardCacheEnabled ? 1 : 2;
    [webView goBack];

    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_WK_STREQ([webView URL].absoluteString, @"https://example.com/main");
    EXPECT_TRUE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://example.com/main");
    if (expectedCommittedFrameSize == 1) {
        EXPECT_FALSE([[webView firstChildFrame] isMainFrame]);
        EXPECT_WK_STREQ([[webView firstChildFrame] request].URL.absoluteString, "https://frame.com/frame");
    } else {
        EXPECT_FALSE([committedFrames[1] isMainFrame]);
        EXPECT_WK_STREQ([committedFrames[1] request].URL.absoluteString, "https://frame.com/frame");
    }
}

TEST(WKBackForwardList, PageCacheGoBackAfterNavigatingSameSiteIframe)
{
    runGoBackAfterNavigatingSameSiteIframe(ShouldEnablePageCache::Yes);
}

TEST(WKBackForwardList, NoPageCacheGoBackAfterNavigatingSameSiteIframe)
{
    runGoBackAfterNavigatingSameSiteIframe(ShouldEnablePageCache::No);
}

TEST(WKBackForwardList, RemoveAllItems)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL3]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(list.backList.count, (NSUInteger)2);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)0);

    [list _removeAllItems];

    EXPECT_EQ(list.backList.count, (NSUInteger)0);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)0);
    EXPECT_EQ(list.currentItem, nil);
}

TEST(WKBackForwardList, ClearKeepsCurrentItem)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL3]]];
    [webView _test_waitForDidFinishNavigation];

    [webView goBack];
    [webView _test_waitForDidFinishNavigation];

    // entries = [A, B(current), C]
    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(list.backList.count, (NSUInteger)1);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)1);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL2.UTF8String);

    [list _clear];

    // Only the current item should remain.
    EXPECT_EQ(list.backList.count, (NSUInteger)0);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)0);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL2.UTF8String);
}

TEST(WKBackForwardList, ClearWithSingleItemDoesNothing)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(list.backList.count, (NSUInteger)0);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)0);

    [list _clear];

    // A single-item list should be unchanged.
    EXPECT_EQ(list.backList.count, (NSUInteger)0);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)0);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL1.UTF8String);
}

TEST(WKBackForwardList, LoggingString)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    NSString *description = [[webView backForwardList] _loggingStringForTesting];
    EXPECT_GT(description.length, 0U);
    EXPECT_TRUE([description containsString:@"no%20error"]);
}

TEST(WKBackForwardList, PageClosed)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)1);

    [webView _close];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)0);
    EXPECT_EQ([webView backForwardList].currentItem, nil);

    webView = nil;
    // If the deinit assertion fires this test will crash.
}

TEST(WKBackForwardList, RestoreFromStateAndSetItemsAsRestoredFromSession)
{
    // Exercise restoreFromState and setItemsAsRestoredFromSession (without filter)
    // by restoring session state via _restoreSessionState:andNavigate:YES.
    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr url3 = [NSBundle.test_resourcesBundle URLForResource:@"simple3" withExtension:@"html"];

    RetainPtr source = adoptNS([[WKWebView alloc] init]);
    [source loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [source _test_waitForDidFinishNavigation];
    [source loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [source _test_waitForDidFinishNavigation];
    [source loadRequest:[NSURLRequest requestWithURL:url3.get()]];
    [source _test_waitForDidFinishNavigation];

    _WKSessionState *sessionState = [source _sessionState];

    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    [webView _restoreSessionState:sessionState andNavigate:YES];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(list.backList.count, (NSUInteger)2);
    EXPECT_EQ(list.forwardList.count, (NSUInteger)0);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, [url3 absoluteString].UTF8String);
}

TEST(WKBackForwardList, BackwardSkipIteratesThroughConsecutiveJSItems)
{
    // Covers the while-loop in itemSkippingBackForwardItemsAddedByJSWithoutUserGesture
    // for the backward direction, plus the backward post-loop step.
    //
    // State: url1 -> url2 -> url2#a(JS) -> url2#b(JS, current)
    //
    // goBack: current is JS, item at -1 (url2#a) is also JS, so the while loop
    // iterates past it to url2 (non-JS). The backward post-loop then steps one
    // further to url1, which is the final navigation target.
    //
    // goForward from url1: url2 is immediately forward and is non-JS (main while
    // skipped), but url2#a and url2#b beyond it are JS, so the forward inner while
    // advances to url2#b.

    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#b');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, 1U);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    RetainPtr expectedURL = makeString(String([url2 absoluteString]), "#b"_s).createNSString();
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURL.get().UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
}

TEST(WKBackForwardList, ForwardSkipIteratesThroughLeadingConsecutiveJSItems)
{
    // Covers the while-loop in itemSkippingBackForwardItemsAddedByJSWithoutUserGesture
    // for the forward direction when the immediately-forward item is JS.
    //
    // State: url1 -> url1#a(JS) -> url1#b(JS) -> url2
    //
    // Navigate directly to url1, then goForward: item at +1 is url1#a (JS), so the
    // main while iterates past url1#a and url1#b, reaching url2 (non-JS). Nothing
    // follows url2, so the inner while is skipped and we land on url2.

    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#b');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // The state of the list right now is:
    // url1 -> url1#a(JS) -> url1#b(JS) -> url2*

    // backList is ordered oldest-first; firstObject is url1 (the original loadRequest item).
    WKBackForwardListItem *url1Item = [webView backForwardList].backList.firstObject;
    [webView goToBackForwardListItem:url1Item];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url2 absoluteString].UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 3U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
}

TEST(WKBackForwardList, BackwardSkipReturnsNonJSItemWhenNothingFurtherBack)
{
    // Covers the guard in the backward post-loop of
    // itemSkippingBackForwardItemsAddedByJSWithoutUserGesture (line 660-662).
    //
    // State: url1 -> url1#a(JS, current)
    //
    // goBack: current is JS so we don't early-return. Item at -1 is url1 (non-JS)
    // so the main while is skipped. The backward post-loop tries to step one further
    // back (itemAtIndex(-2)) which is nil, so it returns originalItem (url1).

    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_TRUE([webView backForwardList].currentItem._wasCreatedByJSWithoutUserInteraction);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);
    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
}

TEST(WKBackForwardList, ForwardSkipReturnsFirstJSItemWhenAllForwardItemsAreJS)
{
    // Create a back/forward list with the following state:
    // url1 -> url1#a(JS) -> url1#b(JS) -> url1#c(JS)
    //
    // Navigate to url1's item, and verify the API visible forward list is empty.

    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr url1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1.get()]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#b');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, '', '#c');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    WKBackForwardListItem *url1Item = [webView backForwardList].backList.firstObject;
    [webView goToBackForwardListItem:url1Item];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, [url1 absoluteString].UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);
}

static void runGoBackAfterNavigatingSameSiteIframe2(ShouldEnablePageCache shouldEnablePageCache)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { "<iframe id='iframe' src='/frame'></iframe>"_s } },
        { "/frame"_s, { "hi"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().pageCacheEnabled = shouldEnablePageCache == ShouldEnablePageCache::Yes;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    [configuration setProcessPool:processPool.get()];
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    // FIXME: Remove once the back-forward cache is enabled for site isolation: rdar://161762363.
    const bool backForwardCacheEnabled = isUsingBackForwardCache(webView.get());
    webView.get().navigationDelegate = navigationDelegate.get();
    static bool didCommitLoadForAllFrames = false;
    static unsigned expectedCommittedFrameSize = 2;
    static Vector<RetainPtr<WKFrameInfo>> committedFrames;
    navigationDelegate.get().didCommitLoadWithRequestInFrame = makeBlockPtr([&](WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo) {
        committedFrames.append(frameInfo);
        if (committedFrames.size() == expectedCommittedFrameSize)
            didCommitLoadForAllFrames = true;
    }).get();

    // After initial loading, page has a main frame and a same-site iframe.
    didCommitLoadForAllFrames = false;
    expectedCommittedFrameSize = 2;
    committedFrames.clear();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_TRUE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://example.com/main");
    EXPECT_FALSE([committedFrames[1] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[1] request].URL.absoluteString, "https://example.com/frame");
    committedFrames.clear();
    didCommitLoadForAllFrames = false;

    // Navigate iframe cross-site.
    expectedCommittedFrameSize = 1;
    [webView evaluateJavaScript:@"document.getElementById('iframe').src = 'https://frame.com/frame'" completionHandler:nil];
    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_FALSE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://frame.com/frame");
    committedFrames.clear();
    didCommitLoadForAllFrames = false;

    // Navigate main frame same-site.
    expectedCommittedFrameSize = 1;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/frame"]]];
    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_TRUE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://example.com/frame");
    committedFrames.clear();
    didCommitLoadForAllFrames = false;

    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ([webView backForwardList].backList.count, 2U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // Navigate main frame back.
    // For page cache case, iframe is not reloaded, so there is only one commit.
    expectedCommittedFrameSize = backForwardCacheEnabled ? 1 : 2;
    [webView goBack];

    TestWebKitAPI::Util::run(&didCommitLoadForAllFrames);
    EXPECT_WK_STREQ([webView URL].absoluteString, @"https://example.com/main");
    EXPECT_TRUE([committedFrames[0] isMainFrame]);
    EXPECT_WK_STREQ([committedFrames[0] request].URL.absoluteString, "https://example.com/main");
    if (expectedCommittedFrameSize == 1) {
        EXPECT_FALSE([[webView firstChildFrame] isMainFrame]);
        EXPECT_WK_STREQ([[webView firstChildFrame] request].URL.absoluteString, "https://frame.com/frame");
    } else {
        EXPECT_FALSE([committedFrames[1] isMainFrame]);
        EXPECT_WK_STREQ([committedFrames[1] request].URL.absoluteString, "https://frame.com/frame");
    }
}

TEST(WKBackForwardList, PageCacheGoBackAfterNavigatingSameSiteIframe2)
{
    runGoBackAfterNavigatingSameSiteIframe2(ShouldEnablePageCache::Yes);
}

TEST(WKBackForwardList, NoPageCacheGoBackAfterNavigatingSameSiteIframe2)
{
    runGoBackAfterNavigatingSameSiteIframe2(ShouldEnablePageCache::No);
}

TEST(WKBackForwardList, BackForwardNavigationSkipsPromiseRedirectWithoutUserInteraction)
{
    // When a web page asynchronously adds items to the session history after the load event,
    // the JavaScript visible session history should see the new entries... But the browser-visible
    // session history (like what the user sees when looking at the back/forward list buttons) now
    // hides these entries.
    //
    // This test uses both JavaScript and the WKBackForwardList API to verify both views of the
    // session history are as expected.

    TestWebKitAPI::HTTPServer server({
        { "/opener"_s, { "<a id='link' href='/pageA' target='_blank'>Open</a>"
            "<script>function clickLink() { document.getElementById('link').click(); }</script>"_s } },
        { "/pageA"_s, { "<script>"
            "window.onload = function() {"
            "  var data = new TextEncoder().encode('test data for hashing');"
            "  crypto.subtle.digest('SHA-256', data).then(function(hash) {"
            "    return crypto.subtle.importKey('raw', hash, { name: 'AES-CBC', length: 256 }, false, ['encrypt']);"
            "  }).then(function(key) {"
            "    var iv = new Uint8Array(16);"
            "    return crypto.subtle.encrypt({ name: 'AES-CBC', iv: iv }, key, data);"
            "  }).then(function(encrypted) {"
            "    window.location.href = '/pageB';"
            "  });"
            "};"
            "</script>"_s } },
        { "/pageB"_s, { "Page B"_s } },
        { "/pageC"_s, { "Page C"_s } },
        { "/pageD"_s, { "Page D"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    [openerWebView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<TestWKWebView> popupWebView;
    __block bool popupCreated = false;
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        popupWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        [popupWebView setNavigationDelegate:navigationDelegate.get()];
        popupCreated = true;
        return popupWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    RetainPtr openerNavigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    [openerWebView setNavigationDelegate:openerNavigationDelegate.get()];
    [openerWebView loadRequest:server.request("/opener"_s)];
    [openerNavigationDelegate waitForDidFinishNavigation];

    [openerWebView evaluateJavaScript:@"clickLink()" completionHandler:nil];
    TestWebKitAPI::Util::run(&popupCreated);

    // Wait for two navigations to finish: To page A, then to Page B.
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];

    // Page A did some crypto operations in a promise chain.
    // At the end of that promise chain it did a JS redirect to page B.
    // So the session history looks like this:
    // (A) -> (B)*
    // Parenthesis indicates the page had no user interaction.
    // Asterisk indicates the current item.
    // JavaScript will always see the full session history, so the length there is 2.
    EXPECT_STREQ([[popupWebView stringByEvaluatingJavaScript:@"history.length"] UTF8String], "2");

    // But the API back/forward list ignores entries without user interaction.
    // Therefore both the backList and forwardList should be empty.
    EXPECT_EQ([popupWebView backForwardList].backList.count, 0U);
    EXPECT_EQ([popupWebView backForwardList].forwardList.count, 0U);

    [popupWebView loadRequest:server.request("/pageC"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    // The history now looks like this:
    // (A) -> (B) -> C*
    // JavaScript sees all 3, but the API back/forward list only sees (B), which is where the
    // user presumably clicked a link to get to C.
    EXPECT_STREQ([[popupWebView stringByEvaluatingJavaScript:@"history.length"] UTF8String], "3");
    EXPECT_EQ([popupWebView backForwardList].backList.count, 1U);
    EXPECT_EQ([popupWebView backForwardList].forwardList.count, 0U);
    EXPECT_STREQ([popupWebView URL].absoluteString.UTF8String, server.request("/pageC"_s).URL.absoluteString.UTF8String);

    [popupWebView loadRequest:server.request("/pageD"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    // The history now looks like this:
    // (A) -> (B) -> C -> D*
    // JavaScript sees all 4, the API back/forward list should see only 3.
    // C and D because they both have a user gesture, and (B) because it's where the
    // user presumably clicked a link to get to C.
    EXPECT_STREQ([[popupWebView stringByEvaluatingJavaScript:@"history.length"] UTF8String], "4");
    EXPECT_EQ([popupWebView backForwardList].backList.count, 2U);
    EXPECT_EQ([popupWebView backForwardList].forwardList.count, 0U);
    EXPECT_STREQ([popupWebView URL].absoluteString.UTF8String, server.request("/pageD"_s).URL.absoluteString.UTF8String);

    [popupWebView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    // The history now looks like this:
    // (A) -> (B) -> C* -> D
    EXPECT_EQ([popupWebView backForwardList].backList.count, 1U);
    EXPECT_EQ([popupWebView backForwardList].forwardList.count, 1U);
    EXPECT_STREQ([popupWebView URL].absoluteString.UTF8String, server.request("/pageC"_s).URL.absoluteString.UTF8String);

    [popupWebView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    // The history now looks like this:
    // (A) -> (B)* -> C -> D
    EXPECT_EQ([popupWebView backForwardList].backList.count, 0U);
    EXPECT_EQ([popupWebView backForwardList].forwardList.count, 2U);
    EXPECT_STREQ([popupWebView URL].absoluteString.UTF8String, server.request("/pageB"_s).URL.absoluteString.UTF8String);

    // Attempting to goBack now should simply fail.
    auto *navigation = [popupWebView goBack];
    EXPECT_NULL(navigation);
}

TEST(WKBackForwardList, BackButtonWorksAfterUserClickFromJSCreatedPage)
{
    TestWebKitAPI::HTTPServer server({
        { "/opener"_s, { "<a id='link' href='/pageA' target='_blank'>Open</a>"
            "<script>function clickLink() { document.getElementById('link').click(); }</script>"_s } },
        { "/pageA"_s, { "<a id='nextLink' href='/pageB'>Go to B</a>"
            "<script>function clickNextLink() { document.getElementById('nextLink').click(); }</script>"_s } },
        { "/pageB"_s, { "Page B"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    [openerWebView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<TestWKWebView> popupWebView;
    __block bool popupCreated = false;
    RetainPtr navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        popupWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        [popupWebView setNavigationDelegate:navigationDelegate.get()];
        popupCreated = true;
        return popupWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    RetainPtr openerNavigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    [openerWebView setNavigationDelegate:openerNavigationDelegate.get()];
    [openerWebView loadRequest:server.request("/opener"_s)];
    [openerNavigationDelegate waitForDidFinishNavigation];

    [openerWebView evaluateJavaScript:@"clickLink()" completionHandler:nil];
    TestWebKitAPI::Util::run(&popupCreated);
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_STREQ([[popupWebView stringByEvaluatingJavaScript:@"history.length"] UTF8String], "1");
    EXPECT_EQ([popupWebView backForwardList].backList.count, 0U);

    // User clicks a link on page A to navigate to page B.
    [popupWebView evaluateJavaScript:@"clickNextLink()" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_STREQ([[popupWebView stringByEvaluatingJavaScript:@"history.length"] UTF8String], "2");
    EXPECT_STREQ([popupWebView URL].absoluteString.UTF8String, server.request("/pageB"_s).URL.absoluteString.UTF8String);
    EXPECT_EQ([popupWebView backForwardList].backList.count, 1U);
    EXPECT_EQ([popupWebView backForwardList].forwardList.count, 0U);

    [popupWebView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_STREQ([popupWebView URL].absoluteString.UTF8String, server.request("/pageA"_s).URL.absoluteString.UTF8String);
}
