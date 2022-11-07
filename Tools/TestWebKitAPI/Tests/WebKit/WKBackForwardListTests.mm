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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKBackForwardListPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKSessionState.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static NSString *loadableURL1 = @"data:text/html,no%20error%20A";
static NSString *loadableURL2 = @"data:text/html,no%20error%20B";
static NSString *loadableURL3 = @"data:text/html,no%20error%20C";

TEST(WKBackForwardList, RemoveCurrentItem)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

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

TEST(WKBackForwardList, CanNotGoBackAfterRestoringEmptySessionState)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ(NO, [webView canGoForward]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);

    auto singlePageWebView = adoptNS([[WKWebView alloc] init]);

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
    auto webView = adoptNS([[WKWebView alloc] init]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ(YES, [webView canGoBack]);
    EXPECT_EQ(NO, [webView canGoForward]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);

    auto singlePageWebView = adoptNS([[WKWebView alloc] init]);

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
    dispatch_async(dispatch_get_main_queue(), ^{
        decisionHandler(WKNavigationActionPolicyAllow);
    });
}

@end

TEST(WKBackForwardList, WindowLocationAsyncPolicyDecision)
{
    NSURL *simple = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *simple2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[AsyncPolicyDecisionDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>window.location='simple.html'</script>" baseURL:simple2];
    TestWebKitAPI::Util::run(&done);
    EXPECT_STREQ(webView.get().backForwardList.currentItem.URL.absoluteString.UTF8String, simple.absoluteString.UTF8String);
}

TEST(WKBackForwardList, InteractionStateRestoration)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url3 = [[NSBundle mainBundle] URLForResource:@"simple3" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url3]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, url3.absoluteString.UTF8String);

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
    EXPECT_STREQ([[newList.currentItem URL] absoluteString].UTF8String, url3.absoluteString.UTF8String);

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
    auto webView = adoptNS([[WKWebView alloc] init]);

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url3 = [[NSBundle mainBundle] URLForResource:@"simple3" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url3]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, url3.absoluteString.UTF8String);

    [webView setInteractionState:nil];

    list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, url3.absoluteString.UTF8String);
}

TEST(WKBackForwardList, InteractionStateRestorationInvalid)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url3 = [[NSBundle mainBundle] URLForResource:@"simple3" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url3]];
    [webView _test_waitForDidFinishNavigation];

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, url3.absoluteString.UTF8String);

    NSString *invalidState = @"foo";
    [webView setInteractionState:invalidState];

    list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, url3.absoluteString.UTF8String);
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

// _beginBackSwipeForTesting / _completeBackSwipeForTesting are not implemented on macOS.
#if !PLATFORM(MAC)

TEST(WKBackForwardList, BackSwipeNavigationSkipsItemsWithoutUserGesture)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#b');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#c');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, 4U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    // Navigating back via a swipe gesture should skip those back/forward list items without a user gesture.
    [webView _beginBackSwipeForTesting];
    [webView _completeBackSwipeForTesting];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 4U);
}

TEST(WKBackForwardList, BackSwipeNavigationDoesNotSkipItemsWithUserGesture)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
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

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 1U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 1U);
}

#endif

TEST(WKBackForwardList, BackForwardNavigationSkipsItemsWithoutUserGesture)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#b');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    expectedURLString = makeString(String(url2.absoluteString), "#b");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.pushState(null, document.title, location.pathname + '#c');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    expectedURLString = makeString(String(url2.absoluteString), "#c");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 4U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    auto* lastURL = [webView URL];

    // Going back should skip the back/forward list items without user gestures.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 4U);

    // Going forward should skip the back/forward list items without user gestures.
    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 4U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    NSString *currentURLString = [webView URL].absoluteString;
    expectedURLString = makeString(String([[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"].absoluteString), "#c");
    EXPECT_WK_STREQ(currentURLString, expectedURLString);

    // Navigating via the JS API shouldn't skip those back/forward list items.
    [webView _evaluateJavaScriptWithoutUserGesture:@"history.back();" completionHandler:^(id, NSError *) { }];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    expectedURLString = makeString(String([[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"].absoluteString), "#b");
    EXPECT_WK_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationSkipsItemsWithoutUserGesture2)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView _evaluateJavaScriptWithoutUserGesture:@"location.href = location.pathname + '#a';" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView _evaluateJavaScriptWithoutUserGesture:@"location.href = location.pathname + '#b';" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    expectedURLString = makeString(String(url2.absoluteString), "#b");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView _evaluateJavaScriptWithoutUserGesture:@"location.href = location.pathname + '#c';" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_FALSE([lastNavigation _isUserInitiated]);
    expectedURLString = makeString(String(url2.absoluteString), "#c");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 4U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    auto* lastURL = [webView URL];

    // Going back should skip the back/forward list items without user gestures.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 0U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 4U);

    // Going forward should skip the back/forward list items without user gestures.
    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);

    EXPECT_EQ([webView backForwardList].backList.count, 4U);
    EXPECT_EQ([webView backForwardList].forwardList.count, 0U);

    NSString *currentURLString = [webView URL].absoluteString;
    expectedURLString = makeString(String(url2.absoluteString), "#c");
    EXPECT_WK_STREQ(currentURLString, expectedURLString);

    // Navigating via the JS API shouldn't skip those back/forward list items.
    [webView _evaluateJavaScriptWithoutUserGesture:@"history.back();" completionHandler:^(id, NSError *) { }];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    expectedURLString = makeString(String([[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"].absoluteString), "#b");
    EXPECT_WK_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithUserGesture)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView evaluateJavaScript:@"history.pushState(null, document.title, location.pathname + '#a');" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_TRUE([lastNavigation _isUserInitiated]);
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    auto* lastURL = [webView URL];
    EXPECT_FALSE([lastURL isEqual:url2]);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, url2.absoluteString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithUserGesture2)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView evaluateJavaScript:@"location.href = location.pathname + '#a';" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_TRUE([lastNavigation _isUserInitiated]);
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    auto* lastURL = [webView URL];
    EXPECT_WK_STREQ(lastURL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, url2.absoluteString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsFromLoadRequest)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Fragment navigation via loadRequest.
    auto newURLString = makeString(String(url2.absoluteString), "#a");
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(NSString *)newURLString]]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    NSString *expectedURLString = newURLString;
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    auto* lastURL = [webView URL];
    EXPECT_WK_STREQ(lastURL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, url2.absoluteString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithRecentUserGesture)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView evaluateJavaScript:@"setTimeout(() => { history.pushState(null, document.title, location.pathname + '#a'); }, 0);" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_TRUE([lastNavigation _isUserInitiated]);
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    auto* lastURL = [webView URL];
    EXPECT_FALSE([lastURL isEqual:url2]);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, url2.absoluteString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipItemsWithRecentUserGesture2)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Add back/forward list items without user gestures.
    [webView evaluateJavaScript:@"setTimeout(() => { location.href = location.pathname + '#a'; }, 0);" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_TRUE([lastNavigation _isUserInitiated]);
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    auto* lastURL = [webView URL];
    EXPECT_WK_STREQ(lastURL.absoluteString.UTF8String, expectedURLString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, url2.absoluteString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url1.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, url2.absoluteString.UTF8String);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];
    expectedURLString = makeString(String(url2.absoluteString), "#a");
    EXPECT_WK_STREQ([lastNavigation _request].URL.absoluteString.UTF8String, expectedURLString.UTF8String);

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, lastURL.absoluteString.UTF8String);
}

TEST(WKBackForwardList, BackForwardNavigationDoesNotSkipUpdatedItemWithRecentUserGesture)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    auto navigationDelegate = adoptNS([WKBackForwardNavigationDelegate new]);
    webView.get().navigationDelegate = navigationDelegate.get();

    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"fragment-navigation-before-load-event" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    [webView loadRequest:[NSURLRequest requestWithURL:url1]];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    [navigationDelegate waitForDidFinishNavigation];

    // Page navigated to #fragment before the load event.
    NSString *expectedURLString = makeString(String(url2.absoluteString), "#fragment");
    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.UTF8String);

    // Navigate with a user gesture.
    [webView evaluateJavaScript:@"location.href = location.pathname + '#otherFragment';" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    // Should go back to #fragment.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigationOrDidSameDocumentNavigation];

    EXPECT_STREQ([webView URL].absoluteString.UTF8String, expectedURLString.UTF8String);
}
