/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import <WebKit/WKBackForwardListItemPrivate.h>
#import <WebKit/WKContentRuleListStore.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferences.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/Deque.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringConcatenateNumbers.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

@interface WKProcessPool ()
- (WKContextRef)_contextForTesting;
@end

static bool didStartProvisionalLoad;
static bool failed;
static int numberOfDecidePolicyCalls;
static bool didRepondToPolicyDecisionCall;

#if PLATFORM(IOS_FAMILY)
static bool requestedQuickLookPassword;
static bool didStartQuickLookLoad;
static bool didFinishQuickLookLoad;
#endif

bool didReceiveAlert;
static bool receivedMessage;
static bool serverRedirected;
static HashSet<pid_t> seenPIDs;
static bool willPerformClientRedirect;
static bool didPerformClientRedirect;
static bool shouldConvertToDownload;
static bool didCloseWindow;
static RetainPtr<NSURL> clientRedirectSourceURL;
static RetainPtr<NSURL> clientRedirectDestinationURL;

static bool didChangeLockdownMode;
static bool lockdownModeBeforeChange;
static bool lockdownModeAfterChange;
static unsigned crashCount = 0;

@interface LockdownModeKVO : NSObject {
@public
}
@end

@implementation LockdownModeKVO

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:@"lockdownModeEnabled"]);
    ASSERT([[object class] isEqual:[WKWebpagePreferences class]]);

    lockdownModeBeforeChange = [[change objectForKey:NSKeyValueChangeOldKey] boolValue];
    lockdownModeAfterChange = [[change objectForKey:NSKeyValueChangeNewKey] boolValue];
    didChangeLockdownMode = true;
}

@end

@interface PSONMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation PSONMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message body])
        [receivedMessages addObject:[message body]];
    else
        [receivedMessages addObject:@""];

    receivedMessage = true;
    if ([message.webView _webProcessIdentifier])
        seenPIDs.add([message.webView _webProcessIdentifier]);
}
@end

@interface PSONNavigationDelegate : NSObject <WKNavigationDelegatePrivate> {
    @public void (^decidePolicyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
    @public void (^didStartProvisionalNavigationHandler)();
    @public void (^didCommitNavigationHandler)();
}
@end

@implementation PSONNavigationDelegate

- (instancetype) init
{
    self = [super init];
    return self;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    seenPIDs.add([webView _webProcessIdentifier]);
    failed = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    seenPIDs.add([webView _webProcessIdentifier]);
    done = true;
}

- (void)_webView:(WKWebView *)webView navigation:(WKNavigation *)navigation didSameDocumentNavigation:(_WKSameDocumentNavigationType)navigationType
{
    if (navigationType != _WKSameDocumentNavigationTypeAnchorNavigation)
        return;

    seenPIDs.add([webView _webProcessIdentifier]);
    done = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    didStartProvisionalLoad = true;
    if (didStartProvisionalNavigationHandler)
        didStartProvisionalNavigationHandler();
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    if (didCommitNavigationHandler)
        didCommitNavigationHandler();
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    ++numberOfDecidePolicyCalls;
    seenPIDs.add([webView _webProcessIdentifier]);
    if (decidePolicyForNavigationAction)
        decidePolicyForNavigationAction(navigationAction, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow);
    didRepondToPolicyDecisionCall = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(shouldConvertToDownload ? WKNavigationResponsePolicyDownload : WKNavigationResponsePolicyAllow);
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    seenPIDs.add([webView _webProcessIdentifier]);
    serverRedirected = true;
}

- (void)_webView:(WKWebView *)webView willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)delay
{
    clientRedirectDestinationURL = URL;
    willPerformClientRedirect = true;
}

- (void)_webView:(WKWebView *)webView didPerformClientRedirectFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL
{
    EXPECT_TRUE(willPerformClientRedirect);
    EXPECT_WK_STREQ([clientRedirectDestinationURL absoluteString], [destinationURL absoluteString]);
    clientRedirectSourceURL = sourceURL;
    didPerformClientRedirect = true;
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    ++crashCount;
    [webView reload];
}

#if PLATFORM(IOS_FAMILY)

- (void)_webViewDidRequestPasswordForQuickLookDocument:(WKWebView *)webView
{
    requestedQuickLookPassword = true;
}

- (void)_webView:(WKWebView *)webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:(NSString *)fileName uti:(NSString *)uti
{
    didStartQuickLookLoad = true;
}

- (void)_webView:(WKWebView *)webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)documentData
{
    didFinishQuickLookLoad = true;
}

#endif

@end

static RetainPtr<WKWebView> createdWebView;

@interface PSONUIDelegate : NSObject <WKUIDelegatePrivate>
- (instancetype)initWithNavigationDelegate:(PSONNavigationDelegate *)navigationDelegate;
@end

@implementation PSONUIDelegate {
    RetainPtr<PSONNavigationDelegate> _navigationDelegate;
}

- (instancetype)initWithNavigationDelegate:(PSONNavigationDelegate *)navigationDelegate
{
    if (!(self = [super init]))
        return nil;

    _navigationDelegate = navigationDelegate;
    return self;
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    createdWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [createdWebView setNavigationDelegate:_navigationDelegate.get()];
    [createdWebView setUIDelegate:self];
    didCreateWebView = true;
    return createdWebView.get();
}

- (void)webViewDidClose:(WKWebView *)webView
{
    EXPECT_EQ(createdWebView.get(), webView);
    createdWebView = nullptr;
    didCloseWindow = true;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    didReceiveAlert = true;
    completionHandler();
}

@end

@interface PSONScheme : NSObject <WKURLSchemeHandler> {
    const char* _bytes;
    HashMap<String, String> _redirects;
    HashMap<String, RetainPtr<NSData>> _dataMappings;
    HashSet<id <WKURLSchemeTask>> _runningTasks;
    bool _shouldRespondAsynchronously;
}
- (instancetype)initWithBytes:(const char*)bytes;
- (void)addRedirectFromURLString:(NSString *)sourceURLString toURLString:(NSString *)destinationURLString;
- (void)addMappingFromURLString:(NSString *)urlString toData:(const char*)data;
@end

@implementation PSONScheme

- (instancetype)initWithBytes:(const char*)bytes
{
    self = [super init];
    _bytes = bytes;
    return self;
}

- (void)addRedirectFromURLString:(NSString *)sourceURLString toURLString:(NSString *)destinationURLString
{
    _redirects.set(sourceURLString, destinationURLString);
}

- (void)addMappingFromURLString:(NSString *)urlString toData:(const char*)data
{
    _dataMappings.set(urlString, [NSData dataWithBytesNoCopy:(void*)data length:strlen(data) freeWhenDone:NO]);
}

- (void)setShouldRespondAsynchronously:(BOOL)value
{
    _shouldRespondAsynchronously = value;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([(id<WKURLSchemeTaskPrivate>)task _requestOnlyIfCached]) {
        [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
        return;
    }

    _runningTasks.add(task);

    auto doAsynchronouslyIfNecessary = [self, strongSelf = retainPtr(self), task = retainPtr(task)](Function<void(id <WKURLSchemeTask>)>&& f, double delay) {
        if (!_shouldRespondAsynchronously)
            return f(task.get());
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay * NSEC_PER_SEC), dispatch_get_main_queue(), makeBlockPtr([self, strongSelf, task, f = WTFMove(f)] {
            if (_runningTasks.contains(task.get()))
                f(task.get());
        }).get());
    };

    NSURL *finalURL = task.request.URL;
    auto target = _redirects.get(task.request.URL.absoluteString);
    if (!target.isEmpty()) {
        auto redirectResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:nil expectedContentLength:0 textEncodingName:nil]);

        finalURL = [NSURL URLWithString:(NSString *)target];
        auto request = adoptNS([[NSURLRequest alloc] initWithURL:finalURL]);

        [(id<WKURLSchemeTaskPrivate>)task _didPerformRedirection:redirectResponse.get() newRequest:request.get()];
    }

    doAsynchronouslyIfNecessary([finalURL = retainPtr(finalURL)](id <WKURLSchemeTask> task) {
        NSMutableDictionary* headerDictionary = [NSMutableDictionary dictionary];
        [headerDictionary setObject:@"text/html" forKey:@"Content-Type"];
        [headerDictionary setObject:@"1" forKey:@"Content-Length"];

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:finalURL.get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:headerDictionary]);
        [task didReceiveResponse:response.get()];
    }, 0.1);

    doAsynchronouslyIfNecessary([self, finalURL = retainPtr(finalURL)](id <WKURLSchemeTask> task) {
        if (auto data = _dataMappings.get([finalURL absoluteString]))
            [task didReceiveData:data.get()];
        else if (_bytes) {
            RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytesNoCopy:(void *)_bytes length:strlen(_bytes) freeWhenDone:NO]);
            [task didReceiveData:data.get()];
        } else
            [task didReceiveData:[@"Hello" dataUsingEncoding:NSUTF8StringEncoding]];
    }, 0.2);

    doAsynchronouslyIfNecessary([self](id <WKURLSchemeTask> task) {
        [task didFinish];
        _runningTasks.remove(task);
    }, 0.3);
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    _runningTasks.remove(task);
}

@end

static const char* testBytes = R"PSONRESOURCE(
<head>
<script>

function log(msg)
{
    window.webkit.messageHandlers.pson.postMessage(msg);
}

window.onload = function(evt) {
    if (window.history.state != "onloadCalled")
        setTimeout('window.history.replaceState("onloadCalled", "");', 0);
}

window.onpageshow = function(evt) {
    log("PageShow called. Persisted: " + evt.persisted + ", and window.history.state is: " + window.history.state);
}

</script>
</head>
)PSONRESOURCE";

static const char* linkToCrossSiteClientSideRedirectBytes = R"PSONRESOURCE(
<body>
  <a id="testLink" href="pson://www.google.com/clientSideRedirect.html">Link to cross-site client-side redirect</a>
</body>
)PSONRESOURCE";

static const char* crossSiteClientSideRedirectBytes = R"PSONRESOURCE(
<body>
<script>
onload = () => {
  location = "pson://www.apple.com/main.html";
};
</script>
</body>
)PSONRESOURCE";

static const char* navigationWithLockedHistoryBytes = R"PSONRESOURCE(
<script>
let shouldNavigate = true;
window.addEventListener('pageshow', function(event) {
    if (event.persisted) {
        window.webkit.messageHandlers.pson.postMessage("Was persisted");
        shouldNavigate = false;
    }
});

onload = function()
{
    if (!shouldNavigate)
        return;

    // JS navigation via window.location
    setTimeout(() => {
        location = "pson://www.apple.com/main.html";
    }, 10);
}
</script>
)PSONRESOURCE";

static const char* pageCache1Bytes = R"PSONRESOURCE(
<script>
window.addEventListener('pageshow', function(event) {
    if (event.persisted)
        window.webkit.messageHandlers.pson.postMessage("Was persisted");
});
</script>
)PSONRESOURCE";

static const char* windowOpenCrossSiteNoOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    window.open("pson://www.apple.com/main.html", "_blank", "noopener");
}
</script>
)PSONRESOURCE";

static const char* windowOpenCrossOriginButSameSiteNoOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    window.open("pson://www.webkit.org:8080/main.html", "_blank", "noopener");
}
</script>
)PSONRESOURCE";

static const char* windowOpenCrossSiteWithOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    window.open("pson://www.apple.com/main.html");
}
</script>
)PSONRESOURCE";

static const char* windowOpenSameSiteWithOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    w = window.open("pson://www.webkit.org/main2.html");
}
</script>
)PSONRESOURCE";

static const char* windowOpenSameSiteNoOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    if (!opener)
        window.open("pson://www.webkit.org/popup.html", "_blank", "noopener");
}
</script>
)PSONRESOURCE";

static const char* windowOpenWithNameSameSiteNoOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    if (!opener)
        window.open("pson://www.webkit.org/popup.html", "foo", "noopener");
}
</script>
)PSONRESOURCE";

static const char* targetBlankCrossSiteWithExplicitOpenerTestBytes = R"PSONRESOURCE(
<a id="testLink" target="_blank" href="pson://www.apple.com/main.html" rel="opener">Link</a>
<script>
window.onload = function() {
    testLink.click();
}
</script>
)PSONRESOURCE";

static const char* targetBlankCrossSiteWithImplicitNoOpenerTestBytes = R"PSONRESOURCE(
<a id="testLink" target="_blank" href="pson://www.apple.com/main.html">Link</a>
<script>
window.onload = function() {
    testLink.click();
}
</script>
)PSONRESOURCE";

static const char* targetBlankCrossSiteNoOpenerTestBytes = R"PSONRESOURCE(
<a id="testLink" target="_blank" href="pson://www.apple.com/main.html" rel="noopener">Link</a>
<script>
window.onload = function() {
    testLink.click();
}
</script>
)PSONRESOURCE";

static const char* targetBlankSameSiteNoOpenerTestBytes = R"PSONRESOURCE(
<a id="testLink" target="_blank" href="pson://www.webkit.org/main2.html" rel="noopener">Link</a>
<script>
window.onload = function() {
    testLink.click();
}
</script>
)PSONRESOURCE";

#if PLATFORM(MAC)
static const char* linkToAppleTestBytes = R"PSONRESOURCE(
<script>
window.addEventListener('pageshow', function(event) {
    if (event.persisted)
        window.webkit.messageHandlers.pson.postMessage("Was persisted");
});
</script>
<a id="testLink" href="pson://www.apple.com/main.html">Navigate</a>
)PSONRESOURCE";
#endif

static RetainPtr<_WKProcessPoolConfiguration> psonProcessPoolConfiguration()
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    return processPoolConfiguration;
}

enum class SchemeHandlerShouldBeAsync { No, Yes };
static void runBasicTest(SchemeHandlerShouldBeAsync schemeHandlerShouldBeAsync)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler setShouldRespondAsynchronously:(schemeHandlerShouldBeAsync == SchemeHandlerShouldBeAsync::Yes)];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];

    EXPECT_EQ(pid1, pid2);
    EXPECT_FALSE(pid2 == pid3);

    // 3 loads, 3 decidePolicy calls (e.g. the load that did perform a process swap should not have generated an additional decidePolicy call)
    EXPECT_EQ(numberOfDecidePolicyCalls, 3);
}

TEST(ProcessSwap, Basic)
{
    runBasicTest(SchemeHandlerShouldBeAsync::No);
}

TEST(ProcessSwap, BasicWithAsyncSchemeHandler)
{
    runBasicTest(SchemeHandlerShouldBeAsync::Yes);
}

TEST(ProcessSwap, NoProcessSwappingWithinSameNonHTTPFamilyProtocol)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"CUSTOM"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"custom://abc/main1.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"custom://def/main2.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(pid1, [webView _webProcessIdentifier]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"custom://ghi/main3.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(pid1, [webView _webProcessIdentifier]);

    // Switch to the file protocol.
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(pid2, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, LoadAfterPolicyDecision)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);

        // Synchronously navigate again right after answering the policy delegate for the previous navigation.
        navigationDelegate->decidePolicyForNavigationAction = nil;
        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
        [webView loadRequest:request];
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    while (![[[webView URL] absoluteString] isEqualToString:@"pson://www.webkit.org/main2.html"])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(ProcessSwap, KillWebContentProcessAfterServerRedirectPolicyDecision)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addRedirectFromURLString:@"pson://www.webkit.org/main2.html" toURLString:@"pson://www.apple.com/main.html"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    __block BOOL isRedirection = NO;
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction * action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        if (!isRedirection) {
            isRedirection = YES;
            return;
        }

        navigationDelegate->decidePolicyForNavigationAction = nil;
        [webView _killWebContentProcessAndResetState];
        done = true;
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::spinRunLoop(10);
    [webView reload];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, PSONRedirectionToExternal)
{
    TestWebKitAPI::HTTPServer server(std::initializer_list<std::pair<String, TestWebKitAPI::HTTPResponse>> { }, TestWebKitAPI::HTTPServer::Protocol::Https);

    HashMap<String, String> redirectHeaders;
    redirectHeaders.add("location"_s, "other://test"_s);
    TestWebKitAPI::HTTPResponse redirectResponse(301, WTFMove(redirectHeaders));

    server.addResponse("/popup.html"_s, WTFMove(redirectResponse));
    auto popupURL = makeString("https://localhost:", server.port(), "/popup.html");

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:popupURL]];
    [webView loadRequest:request];
    done = false;

    __block BOOL isRedirection = NO;
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction * action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        if (!isRedirection) {
            isRedirection = YES;
            return;
        }

        EXPECT_TRUE(!action._canHandleRequest);
        done = true;
    };

    TestWebKitAPI::Util::run(&done);
}

TEST(ProcessSwap, KillProvisionalWebContentProcessThenStartNewLoad)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
    
    // When the provisional load starts in the provisional process, kill the WebView's processes.
    navigationDelegate->didStartProvisionalNavigationHandler = ^{
        [webView _killWebContentProcessAndResetState];
        done = true;
    };
    
    // Start a new cross-site load, which should happen in a new provisional process.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];
    
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    navigationDelegate->didStartProvisionalNavigationHandler = nil;
    
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, NoSwappingForeTLDPlus2)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www1.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www2.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    EXPECT_EQ(pid1, pid2);

    EXPECT_EQ(numberOfDecidePolicyCalls, 2);
}

TEST(ProcessSwap, Back)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:testBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:testBytes];
    [handler addMappingFromURLString:@"pson://www.google.com/main.html" toData:testBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    RetainPtr<PSONMessageHandler> messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_GT([processPool _maximumSuspendedPageCount], 0U);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto googlePID = [webView _webProcessIdentifier];
    EXPECT_NE(applePID, googlePID);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.bing.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto bingPID = [webView _webProcessIdentifier];
    EXPECT_NE(googlePID, bingPID);

    [webView goBack]; // Back to google.com.

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterFirstBackNavigation = [webView _webProcessIdentifier];
    EXPECT_EQ(googlePID, pidAfterFirstBackNavigation);

    [webView goBack]; // Back to apple.com.

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterSecondBackNavigation = [webView _webProcessIdentifier];
    if ([processPool _maximumSuspendedPageCount] > 1)
        EXPECT_EQ(applePID, pidAfterSecondBackNavigation);
    else {
        EXPECT_NE(applePID, pidAfterSecondBackNavigation);
        EXPECT_NE(googlePID, pidAfterSecondBackNavigation);
    }


    // 6 loads, 6 decidePolicy calls (e.g. any load that performs a process swap should not have generated an
    // additional decidePolicy call as a result of the process swap)
    EXPECT_EQ(6, numberOfDecidePolicyCalls);

    EXPECT_EQ(5u, [receivedMessages count]);
    EXPECT_WK_STREQ(@"PageShow called. Persisted: false, and window.history.state is: null", receivedMessages.get()[0]);
    EXPECT_WK_STREQ(@"PageShow called. Persisted: false, and window.history.state is: null", receivedMessages.get()[1]);
    EXPECT_WK_STREQ(@"PageShow called. Persisted: false, and window.history.state is: null", receivedMessages.get()[2]);
    EXPECT_WK_STREQ(@"PageShow called. Persisted: true, and window.history.state is: onloadCalled", receivedMessages.get()[3]);

    // The number of suspended pages we keep around is determined at runtime.
    if ([processPool _maximumSuspendedPageCount] > 1) {
        EXPECT_WK_STREQ(@"PageShow called. Persisted: true, and window.history.state is: onloadCalled", receivedMessages.get()[4]);
        EXPECT_EQ(4u, seenPIDs.size());
    } else
        EXPECT_EQ(5u, seenPIDs.size());
}

static const char* pageWithFragmentTestBytes = R"PSONRESOURCE(
<div id="foo">TEST</div>
)PSONRESOURCE";

TEST(ProcessSwap, HistoryNavigationToFragmentURL)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html#foo" toData:pageWithFragmentTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html#foo"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    [webView goForward];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);

    bool finishedRunningScript = false;
    [webView evaluateJavaScript:@"document.getElementById('foo').innerText" completionHandler: [&] (id result, NSError *error) {
        NSString *innerText = (NSString *)result;
        EXPECT_WK_STREQ(@"TEST", innerText);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
}

TEST(ProcessSwap, SuspendedPageDiesAfterBackForwardListItemIsGone)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_GT([processPool _maximumSuspendedPageCount], 0U);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);

    // webkit.org + apple.com processes.
    EXPECT_EQ(2U, [processPool _webProcessCountIgnoringPrewarmedAndCached]);

    [webView goBack]; // Back to webkit.org.
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    // webkit.org + apple.com processes.
    EXPECT_EQ(2U, [processPool _webProcessCountIgnoringPrewarmedAndCached]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    // apple.com is not longer present in the back/forward list and there should therefore be no-suspended page for it.
    while ([processPool _webProcessCountIgnoringPrewarmedAndCached] > 1u)
        TestWebKitAPI::Util::spinRunLoop();
}

#if PLATFORM(MAC)
TEST(ProcessSwap, SuspendedPagesInActivityMonitor)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:testBytes];
    [handler addMappingFromURLString:@"pson://www.google.com/main.html" toData:testBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    RetainPtr<PSONMessageHandler> messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];
    [processPool _getActivePagesOriginsInWebProcessForTesting:webkitPID completionHandler:^(NSArray<NSString *> *activeDomains) {
        EXPECT_EQ(1u, activeDomains.count);
        EXPECT_WK_STREQ(@"pson://www.webkit.org", activeDomains[0]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto googlePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, googlePID);

    [processPool _getActivePagesOriginsInWebProcessForTesting:googlePID completionHandler:^(NSArray<NSString *> *activeDomains) {
        EXPECT_EQ(1u, activeDomains.count);
        EXPECT_WK_STREQ(@"pson://www.google.com", activeDomains[0]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [processPool _getActivePagesOriginsInWebProcessForTesting:webkitPID completionHandler:^(NSArray<NSString *> *activeDomains) {
        EXPECT_EQ(1u, activeDomains.count);
        EXPECT_WK_STREQ(@"pson://www.webkit.org", activeDomains[0]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack]; // Back to webkit.org.

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterBackNavigation = [webView _webProcessIdentifier];
    EXPECT_EQ(webkitPID, pidAfterBackNavigation);

    [processPool _getActivePagesOriginsInWebProcessForTesting:googlePID completionHandler:^(NSArray<NSString *> *activeDomains) {
        EXPECT_EQ(1u, activeDomains.count);
        EXPECT_WK_STREQ(@"pson://www.google.com", activeDomains[0]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [processPool _getActivePagesOriginsInWebProcessForTesting:webkitPID completionHandler:^(NSArray<NSString *> *activeDomains) {
        EXPECT_EQ(1u, activeDomains.count);
        EXPECT_WK_STREQ(@"pson://www.webkit.org", activeDomains[0]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

#endif // PLATFORM(MAC)

TEST(ProcessSwap, BackWithoutSuspendedPage)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:testBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    RetainPtr<PSONMessageHandler> messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView1 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView1 _webProcessIdentifier];
    RetainPtr<_WKSessionState> sessionState = [webView1 _sessionState];
    webView1 = nullptr;

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView2 setNavigationDelegate:delegate.get()];

    [webView2 _restoreSessionState:sessionState.get() andNavigate:NO];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView2 _webProcessIdentifier];

    [webView2 goBack];

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView2 _webProcessIdentifier];

    EXPECT_FALSE(pid1 == pid2);
    EXPECT_FALSE(pid2 == pid3);
}

TEST(ProcessSwap, BackNavigationAfterSessionRestore)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView1 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView1 _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView1 _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    RetainPtr<_WKSessionState> sessionState = [webView1 _sessionState];
    webView1 = nullptr;

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView2 setNavigationDelegate:delegate.get()];

    [webView2 _restoreSessionState:sessionState.get() andNavigate:YES];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView2 URL] absoluteString]);
    auto pid3 = [webView2 _webProcessIdentifier];

    [webView2 goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView2 URL] absoluteString]);
    auto pid4 = [webView2 _webProcessIdentifier];
    EXPECT_NE(pid3, pid4);
}

TEST(ProcessSwap, CrossSiteWindowOpenNoOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:windowOpenCrossSiteNoOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_NE(pid1, pid2);
}

TEST(ProcessSwap, CrossOriginButSameSiteWindowOpenNoOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:windowOpenCrossOriginButSameSiteNoOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    // Since there is no opener, we process-swap, even though the navigation is same-site.
    EXPECT_NE(pid1, pid2);
}

TEST(ProcessSwap, CrossSiteWindowOpenWithOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnWindowOpenWithOpener = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:windowOpenCrossSiteWithOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_NE(pid1, pid2);
}

enum class ExpectSwap { No, Yes };
enum class WindowHasName : bool { No, Yes };
static void runSameSiteWindowOpenNoOpenerTest(WindowHasName windowHasName, ExpectSwap expectSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    if (windowHasName == WindowHasName::Yes)
        [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:windowOpenWithNameSameSiteNoOpenerTestBytes];
    else
        [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:windowOpenSameSiteNoOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    // Since there is no opener, we process-swap, even though the navigation is same-site.
    if (expectSwap == ExpectSwap::Yes)
        EXPECT_NE(pid1, pid2);
    else
        EXPECT_EQ(pid1, pid2);

    done = false;
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/popup2.html"]];
    [createdWebView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_EQ(pid2, [createdWebView _webProcessIdentifier]);

    // Since the window was opened via JS, it should be able to close itself.
    didCloseWindow = false;
    [createdWebView evaluateJavaScript:@"window.close()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didCloseWindow);
}

TEST(ProcessSwap, SameSiteWindowOpenNoOpener)
{
    // We process-swap even though the navigation is same-site, because the popup has no opener.
    runSameSiteWindowOpenNoOpenerTest(WindowHasName::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, SameSiteWindowOpenWithNameNoOpener)
{
    // We currently do no process-swap when navigating same-site a popup without opener if the window
    // has a name. We should be able to support this but we would need to pass the window name over
    // to the new process.
    runSameSiteWindowOpenNoOpenerTest(WindowHasName::Yes, ExpectSwap::No);
}

TEST(ProcessSwap, CrossSiteBlankTargetWithOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:targetBlankCrossSiteWithExplicitOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(3, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_EQ(pid1, pid2);
}

TEST(ProcessSwap, CrossSiteBlankTargetImplicitNoOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:targetBlankCrossSiteWithImplicitNoOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(3, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_NE(pid1, pid2);
}

TEST(ProcessSwap, CrossSiteBlankTargetNoOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:targetBlankCrossSiteNoOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(3, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_NE(pid1, pid2);
}

TEST(ProcessSwap, SameSiteBlankTargetNoOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:targetBlankSameSiteNoOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(3, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    // Since there is no opener, we process-swap, even though the navigation is same-site.
    EXPECT_NE(pid1, pid2);
}

TEST(ProcessSwap, ServerRedirectFromNewWebView)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addRedirectFromURLString:@"pson://www.webkit.org/main.html" toURLString:@"pson://www.apple.com/main.html"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&serverRedirected);
    serverRedirected = false;

    seenPIDs.add([webView _webProcessIdentifier]);

    TestWebKitAPI::Util::run(&done);
    done = false;

    seenPIDs.add([webView _webProcessIdentifier]);

    EXPECT_FALSE(serverRedirected);
    EXPECT_EQ(2, numberOfDecidePolicyCalls);
    EXPECT_EQ(1u, seenPIDs.size());
}

TEST(ProcessSwap, ServerRedirect)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addRedirectFromURLString:@"pson://www.webkit.org/main.html" toURLString:@"pson://www.apple.com/main.html"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterFirstLoad = [webView _webProcessIdentifier];

    EXPECT_EQ(1, numberOfDecidePolicyCalls);
    EXPECT_EQ(1u, seenPIDs.size());
    EXPECT_TRUE(*seenPIDs.begin() == pidAfterFirstLoad);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&serverRedirected);
    serverRedirected = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    TestWebKitAPI::Util::run(&done);
    done = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    EXPECT_FALSE(serverRedirected);
    EXPECT_EQ(3, numberOfDecidePolicyCalls);
    EXPECT_EQ(2u, seenPIDs.size());
}

TEST(ProcessSwap, ServerRedirect2)
{
    // This tests a load that *starts out* to the same origin as the previous load, but then redirects to a new origin.
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler1 = adoptNS([[PSONScheme alloc] init]);
    [handler1 addRedirectFromURLString:@"pson://www.webkit.org/main2.html" toURLString:@"pson://www.apple.com/main.html"];
    [webViewConfiguration setURLSchemeHandler:handler1.get() forURLScheme:@"pson"];
    auto handler2 = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler2.get() forURLScheme:@"psonredirected"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterFirstLoad = [webView _webProcessIdentifier];

    EXPECT_FALSE(serverRedirected);
    EXPECT_EQ(1, numberOfDecidePolicyCalls);
    EXPECT_EQ(1u, seenPIDs.size());
    EXPECT_TRUE(*seenPIDs.begin() == pidAfterFirstLoad);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&serverRedirected);
    serverRedirected = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    TestWebKitAPI::Util::run(&done);
    done = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    EXPECT_FALSE(serverRedirected);
    EXPECT_EQ(3, numberOfDecidePolicyCalls);
    EXPECT_EQ(2u, seenPIDs.size());

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    [webView goBack];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", [[webView URL] absoluteString]);
}

enum class ShouldCacheProcessFirst { No, Yes };
static void runSameOriginServerRedirectTest(ShouldCacheProcessFirst shouldCacheProcessFirst)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:crossSiteClientSideRedirectBytes];
    [handler addRedirectFromURLString:@"pson://www.apple.com/main.html" toURLString:@"pson://www.apple.com/main2.html"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request;

    if (shouldCacheProcessFirst == ShouldCacheProcessFirst::Yes) {
        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main3.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;
    }

    delegate->didStartProvisionalNavigationHandler = ^{
        seenPIDs.add([webView _webProcessIdentifier]);
        if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
            seenPIDs.add(provisionalPID);
    };

    willPerformClientRedirect = false;
    didPerformClientRedirect = false;
    serverRedirected = false;
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&willPerformClientRedirect);

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    TestWebKitAPI::Util::run(&didPerformClientRedirect);
    didPerformClientRedirect = false;
    willPerformClientRedirect = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    TestWebKitAPI::Util::run(&serverRedirected);
    serverRedirected = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    TestWebKitAPI::Util::run(&done);
    done = false;

    seenPIDs.add([webView _webProcessIdentifier]);
    if (auto provisionalPID = [webView _provisionalWebProcessIdentifier])
        seenPIDs.add(provisionalPID);

    EXPECT_EQ(2u, seenPIDs.size());
}

TEST(ProcessSwap, SameOriginServerRedirect)
{
    runSameOriginServerRedirectTest(ShouldCacheProcessFirst::No);
}

TEST(ProcessSwap, SameOriginServerRedirectFromCachedProcess)
{
    runSameOriginServerRedirectTest(ShouldCacheProcessFirst::Yes);
}

TEST(ProcessSwap, TerminateProcessRightAfterSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    delegate->didStartProvisionalNavigationHandler = ^{
        EXPECT_NE(0, [webView _provisionalWebProcessIdentifier]);
        kill([webView _provisionalWebProcessIdentifier], 9);
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::runFor(0.5_s);
}

static const char* linkToWebKitBytes = R"PSONRESOURCE(
<body>
  <a id="testLink" href="pson://www.webkit.org/main.html">Link</a>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, PolicyCancelAfterServerRedirect)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.google.com/main.html" toData:linkToWebKitBytes];
    [handler addRedirectFromURLString:@"pson://www.webkit.org/main.html" toURLString:@"pson://www.apple.com/ignore.html"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.absoluteString hasSuffix:@"ignore.html"]) {
            decisionHandler(WKNavigationActionPolicyCancel);
            return;
        }
        decisionHandler(WKNavigationActionPolicyAllow);
    };

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pidAfterFirstLoad = [webView _webProcessIdentifier];

    EXPECT_EQ(1, numberOfDecidePolicyCalls);

    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::run(&failed);
    failed = false;
    done = false;

    EXPECT_EQ(3, numberOfDecidePolicyCalls);

    // We should still be on google.com.
    EXPECT_EQ(pidAfterFirstLoad, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.google.com/main.html", [[webView URL] absoluteString]);

    [webView evaluateJavaScript:@"testLink.innerText" completionHandler: [&] (id innerText, NSError *error) {
        EXPECT_WK_STREQ(@"Link", innerText);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, CrossSiteDownload)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.google.com/main.html" toData:linkToWebKitBytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:"Hello"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pidAfterFirstLoad = [webView _webProcessIdentifier];

    shouldConvertToDownload = true;
    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::run(&failed);
    failed = false;
    shouldConvertToDownload = false;

    // We should still be on google.com.
    EXPECT_EQ(pidAfterFirstLoad, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.google.com/main.html", [[webView URL] absoluteString]);

    [webView evaluateJavaScript:@"testLink.innerText" completionHandler: [&] (id innerText, NSError *error) {
        EXPECT_WK_STREQ(@"Link", innerText);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

#if USE(SYSTEM_PREVIEW)

static const char* systemPreviewSameOriginTestBytes = R"PSONRESOURCE(
<body>
    <a id="testLink" rel="ar" href="pson://www.webkit.org/whatever">
        <img src="pson://www.webkit.org/image">
    </a>
</body>
)PSONRESOURCE";

static const char* systemPreviewCrossOriginTestBytes = R"PSONRESOURCE(
<body>
    <a id="testLink" rel="ar" href="pson://www.apple.com/whatever">
        <img src="pson://www.webkit.org/image">
    </a>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, SameOriginSystemPreview)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:systemPreviewSameOriginTestBytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/whatever" toData:"Fake USDZ data"];
    [handler addMappingFromURLString:@"pson://www.webkit.org/image" toData:"Fake image data"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    [webViewConfiguration _setSystemPreviewEnabled:YES];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pidAfterFirstLoad = [webView _webProcessIdentifier];

    didStartProvisionalLoad = false;
    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::runFor(0.5_s);

    // We should still be on webkit.org.
    EXPECT_EQ(pidAfterFirstLoad, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);
    EXPECT_FALSE(didStartProvisionalLoad);
}

TEST(ProcessSwap, CrossOriginSystemPreview)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:systemPreviewCrossOriginTestBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/whatever" toData:"Fake USDZ data"];
    [handler addMappingFromURLString:@"pson://www.webkit.org/image" toData:"Fake image data"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    [webViewConfiguration _setSystemPreviewEnabled:YES];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pidAfterFirstLoad = [webView _webProcessIdentifier];

    didStartProvisionalLoad = false;
    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::runFor(0.5_s);

    // We should still be on webkit.org.
    EXPECT_EQ(pidAfterFirstLoad, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);
    EXPECT_FALSE(didStartProvisionalLoad);
}

#endif

enum class ShouldEnablePSON { No, Yes };
static void runClientSideRedirectTest(ShouldEnablePSON shouldEnablePSON)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnNavigation = shouldEnablePSON == ShouldEnablePSON::Yes ? YES : NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:linkToCrossSiteClientSideRedirectBytes];
    [handler addMappingFromURLString:@"pson://www.google.com/clientSideRedirect.html" toData:crossSiteClientSideRedirectBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    // Navigate to the page doing a client-side redirect to apple.com.
    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.google.com/clientSideRedirect.html", [[webView URL] absoluteString]);
    auto googlePID = [webView _webProcessIdentifier];
    if (shouldEnablePSON == ShouldEnablePSON::Yes)
        EXPECT_NE(webkitPID, googlePID);
    else
        EXPECT_EQ(webkitPID, googlePID);

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    auto applePID = [webView _webProcessIdentifier];
    if (shouldEnablePSON == ShouldEnablePSON::Yes) {
        EXPECT_NE(webkitPID, applePID);
        EXPECT_NE(webkitPID, googlePID);
    } else {
        EXPECT_EQ(webkitPID, applePID);
        EXPECT_EQ(webkitPID, googlePID);
    }

    EXPECT_TRUE(willPerformClientRedirect);
    EXPECT_TRUE(didPerformClientRedirect);
    EXPECT_WK_STREQ(@"pson://www.google.com/clientSideRedirect.html", [clientRedirectSourceURL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [clientRedirectDestinationURL absoluteString]);

    willPerformClientRedirect = false;
    didPerformClientRedirect = false;
    clientRedirectSourceURL = nullptr;
    clientRedirectDestinationURL = nullptr;

    // Validate Back/Forward list.
    auto* backForwardList = [webView backForwardList];
    auto* currentItem = backForwardList.currentItem;
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [currentItem.URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [currentItem.initialURL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);

    EXPECT_EQ(1U, backForwardList.backList.count);

    auto* backItem = backForwardList.backItem;
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backItem.URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backItem.initialURL absoluteString]);

    // Navigate back.
    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);
    EXPECT_FALSE(willPerformClientRedirect);
    EXPECT_FALSE(didPerformClientRedirect);

    auto pidAfterBackNavigation = [webView _webProcessIdentifier];
    if ([processPool _maximumSuspendedPageCount] > 1)
        EXPECT_EQ(webkitPID, pidAfterBackNavigation);

    // Validate Back/Forward list.
    currentItem = backForwardList.currentItem;
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [currentItem.URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [currentItem.initialURL absoluteString]);

    EXPECT_TRUE(!backForwardList.backItem);
    EXPECT_EQ(1U, backForwardList.forwardList.count);

    auto* forwardItem = backForwardList.forwardItem;
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [forwardItem.URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [forwardItem.initialURL absoluteString]);

    // Navigate forward.
    [webView goForward];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
    EXPECT_FALSE(willPerformClientRedirect);
    EXPECT_FALSE(didPerformClientRedirect);

    auto pidAfterForwardNavigation = [webView _webProcessIdentifier];
    EXPECT_EQ(applePID, pidAfterForwardNavigation);

    // Validate Back/Forward list.
    currentItem = backForwardList.currentItem;
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [currentItem.URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [currentItem.initialURL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);

    EXPECT_EQ(1U, backForwardList.backList.count);

    backItem = backForwardList.backItem;
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backItem.URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backItem.initialURL absoluteString]);
}

TEST(ProcessSwap, CrossSiteClientSideRedirectWithoutPSON)
{
    runClientSideRedirectTest(ShouldEnablePSON::No);
}

TEST(ProcessSwap, CrossSiteClientSideRedirectWithPSON)
{
    runClientSideRedirectTest(ShouldEnablePSON::Yes);
}

TEST(ProcessSwap, CrossSiteClientSideRedirectFromFileURL)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    willPerformClientRedirect = false;
    didPerformClientRedirect = false;

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"client-side-redirect" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    EXPECT_EQ(1U, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_TRUE(willPerformClientRedirect);
    EXPECT_TRUE(didPerformClientRedirect);
}

TEST(ProcessSwap, NavigateBackAfterClientSideRedirect)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:linkToCrossSiteClientSideRedirectBytes];
    [handler addMappingFromURLString:@"pson://www.google.com/clientSideRedirect.html" toData:crossSiteClientSideRedirectBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    willPerformClientRedirect = false;
    didPerformClientRedirect = false;

    // Navigate to the page doing a client-side redirect to apple.com.
    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.google.com/clientSideRedirect.html", [[webView URL] absoluteString]);
    auto googlePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, googlePID);

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);
    EXPECT_NE(webkitPID, googlePID);

    EXPECT_TRUE(willPerformClientRedirect);
    EXPECT_TRUE(didPerformClientRedirect);
    EXPECT_WK_STREQ(@"pson://www.google.com/clientSideRedirect.html", [clientRedirectSourceURL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [clientRedirectDestinationURL absoluteString]);

    willPerformClientRedirect = false;
    didPerformClientRedirect = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);

    [webView goBack];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    EXPECT_FALSE(willPerformClientRedirect);
    EXPECT_FALSE(didPerformClientRedirect);
}

static void runNavigationWithLockedHistoryTest(ShouldEnablePSON shouldEnablePSON)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnNavigation = shouldEnablePSON == ShouldEnablePSON::Yes ? YES : NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:navigationWithLockedHistoryBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    // Page redirects to apple.com.
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    if (shouldEnablePSON == ShouldEnablePSON::Yes)
        EXPECT_NE(webkitPID, applePID);
    else
        EXPECT_EQ(webkitPID, applePID);

    auto* backForwardList = [webView backForwardList];
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);
    EXPECT_EQ(1U, backForwardList.backList.count);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.backItem.URL absoluteString]);

    receivedMessage = false;
    [webView goBack];
    TestWebKitAPI::Util::run(&receivedMessage); // Should be restored from PageCache.
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.backItem);
    EXPECT_EQ(1U, backForwardList.forwardList.count);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.forwardItem.URL absoluteString]);

    [webView goForward];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&receivedMessage); // Should be restored from PageCache.
    receivedMessage = false;
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);
    EXPECT_EQ(1U, backForwardList.backList.count);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.backItem.URL absoluteString]);
}

TEST(ProcessSwap, NavigationWithLockedHistoryWithPSON)
{
    runNavigationWithLockedHistoryTest(ShouldEnablePSON::Yes);
}

static void runQuickBackForwardNavigationTest(ShouldEnablePSON shouldEnablePSON)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnNavigation = shouldEnablePSON == ShouldEnablePSON::Yes ? YES : NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    if (shouldEnablePSON == ShouldEnablePSON::Yes)
        EXPECT_NE(webkitPID, applePID);
    else
        EXPECT_EQ(webkitPID, applePID);

    for (unsigned i = 0; i < 10; ++i) {
        [webView goBack];
        TestWebKitAPI::Util::runFor(0.1_s);
        [webView goForward];
        TestWebKitAPI::Util::spinRunLoop();
    }

    TestWebKitAPI::Util::run(&done);
    done = false;

    Vector<String> backForwardListURLs;
    auto* backForwardList = [webView backForwardList];
    for (unsigned i = 0; i < backForwardList.backList.count; ++i)
        backForwardListURLs.append([backForwardList.backList[i].URL absoluteString]);
    backForwardListURLs.append([backForwardList.currentItem.URL absoluteString]);
    for (unsigned i = 0; i < backForwardList.forwardList.count; ++i)
        backForwardListURLs.append([backForwardList.forwardList[i].URL absoluteString]);
    RELEASE_ASSERT(backForwardListURLs.size() == 3u);
    EXPECT_WK_STREQ("pson://www.webkit.org/main1.html", backForwardListURLs[0]);
    EXPECT_WK_STREQ("pson://www.webkit.org/main2.html", backForwardListURLs[1]);
    EXPECT_WK_STREQ("pson://www.apple.com/main.html", backForwardListURLs[2]);
}

TEST(ProcessSwap, QuickBackForwardNavigationWithoutPSON)
{
    runQuickBackForwardNavigationTest(ShouldEnablePSON::No);
}

TEST(ProcessSwap, QuickBackForwardNavigationWithPSON)
{
    runQuickBackForwardNavigationTest(ShouldEnablePSON::Yes);
}

TEST(ProcessSwap, NavigationWithLockedHistoryWithoutPSON)
{
    runNavigationWithLockedHistoryTest(ShouldEnablePSON::No);
}

static const char* sessionStorageTestBytes = R"PSONRESOURCE(
<head>
<script>

function log(msg)
{
    window.webkit.messageHandlers.pson.postMessage(msg);
}

window.onload = function(evt) {
    log(sessionStorage.psonKey);
    sessionStorage.psonKey = "I exist!";
}

</script>
</head>
)PSONRESOURCE";

TEST(ProcessSwap, SessionStorage)
{
    for (int i = 0; i < 5; ++i) {
        [receivedMessages removeAllObjects];
        auto processPoolConfiguration = psonProcessPoolConfiguration();
        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration setProcessPool:processPool.get()];
        auto handler = adoptNS([[PSONScheme alloc] init]);
        [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:sessionStorageTestBytes];
        [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

        auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
        [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:delegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&receivedMessage);
        receivedMessage = false;
        TestWebKitAPI::Util::run(&done);
        done = false;

        auto webkitPID = [webView _webProcessIdentifier];

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        auto applePID = [webView _webProcessIdentifier];

        // Verify the web pages are in different processes
        EXPECT_NE(webkitPID, applePID);

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&receivedMessage);
        receivedMessage = false;
        TestWebKitAPI::Util::run(&done);
        done = false;

        // We should have gone back to the webkit.org process for this load since we reuse SuspendedPages' process when possible.
        EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

        // Verify the sessionStorage values were as expected
        EXPECT_EQ([receivedMessages count], 2u);
        EXPECT_TRUE([receivedMessages.get()[0] isEqualToString:@""]);
        EXPECT_TRUE([receivedMessages.get()[1] isEqualToString:@"I exist!"]);
    }
}

TEST(ProcessSwap, ReuseSuspendedProcess)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().usesWebProcessCache = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];

    EXPECT_NE(webkitPID, applePID);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // We should have gone back to the webkit.org process for this load since we reuse SuspendedPages' process when possible.
    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // We should have gone back to the apple.com process for this load since we reuse SuspendedPages' process when possible.
    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
}

static const char* failsToEnterPageCacheTestBytes = R"PSONRESOURCE(
<body>
<script>
// Pages with dedicated workers do not go into back/forward cache.
var myWorker = new Worker('worker.js');
</script>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, ReuseSuspendedProcessEvenIfPageCacheFails)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:failsToEnterPageCacheTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];

    EXPECT_NE(webkitPID, applePID);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // We should have gone back to the webkit.org process for this load since we reuse SuspendedPages' process when possible.
    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // We should have gone back to the apple.com process for this load since we reuse SuspendedPages' process when possible.
    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, ReuseSuspendedProcessOnBackEvenIfPageCacheFails)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:failsToEnterPageCacheTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];

    EXPECT_NE(webkitPID, applePID);

    [webView goBack];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
}

static const char* withSubframesTestBytes = R"PSONRESOURCE(
<body>
<iframe src="about:blank"></iframe>
<iframe src="about:blank"></iframe>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, HistoryItemIDConfusion)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:failsToEnterPageCacheTestBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:withSubframesTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto googlePID = [webView _webProcessIdentifier];
    EXPECT_NE(applePID, googlePID);
    EXPECT_NE(webkitPID, googlePID);

    [webView goBack];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);

    [webView goBack];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    auto* backForwardList = [webView backForwardList];
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_EQ(2U, backForwardList.forwardList.count);
    EXPECT_EQ(0U, backForwardList.backList.count);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.forwardList[0].URL absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.google.com/main.html", [backForwardList.forwardList[1].URL absoluteString]);
}

TEST(ProcessSwap, GoToSecondItemInBackHistory)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:withSubframesTestBytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/main2.html" toData:withSubframesTestBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:withSubframesTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);

    [webView goToBackForwardListItem:webView.get().backForwardList.backList[0]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", [[webView URL] absoluteString]);

    [webView goToBackForwardListItem:webView.get().backForwardList.forwardList[1]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
}

TEST(ProcessSwap, PrivateAndRegularSessionsShouldGetDifferentProcesses)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto privateWebViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [privateWebViewConfiguration setProcessPool:processPool.get()];
    [privateWebViewConfiguration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [privateWebViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];
    auto regularWebViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [regularWebViewConfiguration setProcessPool:processPool.get()];
    [regularWebViewConfiguration setWebsiteDataStore:[WKWebsiteDataStore defaultDataStore]];
    [regularWebViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);

    auto regularWebView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:regularWebViewConfiguration.get()]);
    [regularWebView1 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [regularWebView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [regularWebView1 _close];
    regularWebView1 = nil;

    auto privateWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:privateWebViewConfiguration.get()]);
    [privateWebView setNavigationDelegate:delegate.get()];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [privateWebView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto privateSessionWebkitPID = [privateWebView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [privateWebView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto privateSessionApplePID = [privateWebView _webProcessIdentifier];
    EXPECT_NE(privateSessionWebkitPID, privateSessionApplePID);

    [privateWebView _close];
    privateWebView = nil;

    auto regularWebView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:regularWebViewConfiguration.get()]);
    [regularWebView2 setNavigationDelegate:delegate.get()];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [regularWebView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto regularSessionGooglePID = [regularWebView2 _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [regularWebView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto regularSessionWebkitPID = [regularWebView2 _webProcessIdentifier];
    EXPECT_NE(regularSessionGooglePID, regularSessionWebkitPID);
    EXPECT_NE(privateSessionWebkitPID, regularSessionWebkitPID);
}

static const char* keepNavigatingFrameBytes = R"PSONRESOURCE(
<body>
<iframe id="testFrame1" src="about:blank"></iframe>
<iframe id="testFrame2" src="about:blank"></iframe>
<iframe id="testFrame3" src="about:blank"></iframe>
<script>
window.addEventListener('pagehide', function(event) {
    for (var j = 0; j < 10000; j++);
});

var i = 0;
function navigateFrames()
{
    testFrame1.src = "pson://www.google.com/main" + i + ".html";
    testFrame2.src = "pson://www.google.com/main" + i + ".html";
    testFrame3.src = "pson://www.google.com/main" + i + ".html";
    i++;
}

navigateFrames();
setInterval(() => {
    navigateFrames();
}, 0);
</script>
</body>
)PSONRESOURCE";

enum class RetainPageInBundle { No, Yes };

void testReuseSuspendedProcessForRegularNavigation(RetainPageInBundle retainPageInBundle)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    if (retainPageInBundle == RetainPageInBundle::Yes)
        [processPoolConfiguration setInjectedBundleURL:[[NSBundle mainBundle] URLForResource:@"TestWebKitAPI" withExtension:@"wkbundle"]];
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    if (retainPageInBundle == RetainPageInBundle::Yes)
        [processPool _setObject:@"BundleRetainPagePlugIn" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:keepNavigatingFrameBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, ReuseSuspendedProcessForRegularNavigationRetainBundlePage)
{
    testReuseSuspendedProcessForRegularNavigation(RetainPageInBundle::Yes);
}

TEST(ProcessSwap, ReuseSuspendedProcessForRegularNavigation)
{
    testReuseSuspendedProcessForRegularNavigation(RetainPageInBundle::No);
}

static const char* mainFramesOnlyMainFrame = R"PSONRESOURCE(
<script>
function loaded() {
    setTimeout('window.frames[0].location.href = "pson://www.apple.com/main.html"', 0);
}
</script>
<body onload="loaded();">
Some text
<iframe src="pson://www.webkit.org/iframe.html"></iframe>
</body>
)PSONRESOURCE";

static const char* mainFramesOnlySubframe = R"PSONRESOURCE(
Some content
)PSONRESOURCE";


static const char* mainFramesOnlySubframe2 = R"PSONRESOURCE(
<script>
    window.webkit.messageHandlers.pson.postMessage("Done");
</script>
)PSONRESOURCE";

TEST(ProcessSwap, MainFramesOnly)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:mainFramesOnlyMainFrame];
    [handler addMappingFromURLString:@"pson://www.webkit.org/iframe" toData:mainFramesOnlySubframe];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:mainFramesOnlySubframe2];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;

    EXPECT_EQ(1u, seenPIDs.size());
}

#if PLATFORM(MAC)

static const char* getClientWidthBytes = R"PSONRESOURCE(
<body>
TEST
<script>
function getClientWidth()
{
    document.body.offsetTop; // Force layout.
    return document.body.clientWidth;
}
</script>
</body>
)PSONRESOURCE";

static unsigned waitUntilClientWidthIs(WKWebView *webView, unsigned expectedClientWidth)
{
    int timeout = 10;
    unsigned clientWidth = 0;
    do {
        if (timeout != 10)
            TestWebKitAPI::Util::runFor(0.1_s);

        [webView evaluateJavaScript:@"getClientWidth()" completionHandler: [&] (id result, NSError *error) {
            clientWidth = [result integerValue];
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
        --timeout;
    } while (clientWidth != expectedClientWidth && timeout >= 0);

    return clientWidth;
}

TEST(ProcessSwap, PageZoomLevelAfterSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:getClientWidthBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:getClientWidthBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    delegate->didCommitNavigationHandler = ^ {
        [webView _setPageZoomFactor:2.0];
        delegate->didCommitNavigationHandler = nil;
    };

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    unsigned clientWidth = waitUntilClientWidthIs(webView.get(), 400);
    EXPECT_EQ(400U, clientWidth);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    clientWidth = waitUntilClientWidthIs(webView.get(), 400);
    EXPECT_EQ(400U, clientWidth);

    // Kill the WebProcess, the page should reload automatically and the page zoom level should be maintained.
    kill([webView _webProcessIdentifier], 9);

    TestWebKitAPI::Util::run(&done);
    done = false;

    clientWidth = waitUntilClientWidthIs(webView.get(), 400);
    EXPECT_EQ(400U, clientWidth);
}

#endif // PLATFORM(MAC)

static const char* mediaTypeBytes = R"PSONRESOURCE(
<style>
@media screen {
.print{
    visibility: hidden;
}
}

@media print {
.screen{
    visibility: hidden;
}
}
</style>
<body>
<div class="screen">Screen</div>
<div class="print">Print</div>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, MediaTypeAfterSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:mediaTypeBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:mediaTypeBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    NSString *innerText = [[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    EXPECT_TRUE([innerText isEqualToString:@"Screen"]);

    webView.get().mediaType = @"print";

    innerText = [[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    EXPECT_TRUE([innerText isEqualToString:@"Print"]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    innerText = [[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    EXPECT_TRUE([innerText isEqualToString:@"Print"]);

    // Kill the WebProcess, the page should reload automatically and the media type should be maintained.
    kill([webView _webProcessIdentifier], 9);

    TestWebKitAPI::Util::run(&done);
    done = false;

    innerText = [[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    EXPECT_TRUE([innerText isEqualToString:@"Print"]);
}

static const char* navigateBeforePageLoadEndBytes = R"PSONRESOURCE(
<body>
<a id="testLink" href="pson://www.apple.com/main.html">Link</a>
<script>
    testLink.click();
</script>
<p>TEST</p>
<script src="test.js"></script>
<script src="test2.js"></script>
<iframe src="subframe1.html></iframe>
<iframe src="subframe2.html></iframe>
<iframe src="subframe3.html></iframe>
<iframe src="subframe4.html></iframe>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, NavigateCrossSiteBeforePageLoadEnd)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:navigateBeforePageLoadEndBytes];
    [handler setShouldRespondAsynchronously:YES];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    failed = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_FALSE(failed);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
}

static void runCancelCrossSiteProvisionalLoadTest(ShouldEnablePSON shouldEnablePSON)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnNavigation = shouldEnablePSON == ShouldEnablePSON::Yes;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    failed = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_FALSE(failed);

    navigationDelegate->didStartProvisionalNavigationHandler = ^{
        [webView stopLoading];
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&failed);
    failed = false;
}

TEST(ProcessSwap, CancelCrossSiteProvisionalLoadWithoutPSON)
{
    runCancelCrossSiteProvisionalLoadTest(ShouldEnablePSON::No);
}

TEST(ProcessSwap, CancelCrossSiteProvisionalLoadWithPSON)
{
    runCancelCrossSiteProvisionalLoadTest(ShouldEnablePSON::Yes);
}

TEST(ProcessSwap, DoSameSiteNavigationAfterCrossSiteProvisionalLoadStarted)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    delegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);

        delegate->decidePolicyForNavigationAction = nil;
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]]];
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main2.html", [[webView URL] absoluteString]);
    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, SuspendedPageLimit)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    auto maximumSuspendedPageCount = [processPool _maximumSuspendedPageCount];
    EXPECT_GT(maximumSuspendedPageCount, 0U);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.bing.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.yahoo.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // Navigations to 5 different domains, we expect to have seen 5 different PIDs
    EXPECT_EQ(5u, seenPIDs.size());

    // But not all of those processes should still be alive (1 visible, maximumSuspendedPageCount suspended).
    auto expectedProcessCount = 1 + maximumSuspendedPageCount;
    int timeout = 20;
    while ([processPool _webProcessCountIgnoringPrewarmedAndCached] != expectedProcessCount && timeout >= 0) {
        TestWebKitAPI::Util::runFor(0.1_s);
        --timeout;
    }

    EXPECT_EQ(expectedProcessCount, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
}

TEST(ProcessSwap, PageCache1)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:pageCache1Bytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad1 = [webView _webProcessIdentifier];

    EXPECT_EQ(1u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad2 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_NE(pidAfterLoad1, pidAfterLoad2);

    [webView goBack];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad3 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_EQ(pidAfterLoad1, pidAfterLoad3);
    EXPECT_EQ(1u, [receivedMessages count]);
    EXPECT_TRUE([receivedMessages.get()[0] isEqualToString:@"Was persisted" ]);
    EXPECT_EQ(2u, seenPIDs.size());

    [webView goForward];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad4 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_EQ(pidAfterLoad2, pidAfterLoad4);
    EXPECT_EQ(2u, [receivedMessages count]);
    EXPECT_TRUE([receivedMessages.get()[1] isEqualToString:@"Was persisted" ]);
    EXPECT_EQ(2u, seenPIDs.size());
}

TEST(ProcessSwap, NavigateBackAfterCrossOriginClientRedirect)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://webkit.org/navigated_from" toData:"<a href='pson://apple.com/'>hello</a>"];
    [handler addMappingFromURLString:@"pson://apple.com/" toData:"<script>window.location.href='pson://webkit.org/redirected_to'</script>redirecting..."];
    [handler addMappingFromURLString:@"pson://webkit.org/redirected_to" toData:"<p>hello again</p>"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://webkit.org/navigated_from"]]];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.querySelector('a').click()" completionHandler:nil];
    [webView _test_waitForDidFinishNavigation];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.location.href"], "pson://webkit.org/redirected_to");
    [webView goBack];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.location.href"], "pson://webkit.org/navigated_from");
}

TEST(ProcessSwap, BackForwardCacheSkipBackForwardListItem)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:pageCache1Bytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html#foo"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_EQ(1U, [[[webView backForwardList] backList] count]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    // Go 2 items back.
    EXPECT_EQ(2U, [[[webView backForwardList] backList] count]);
    [webView goToBackForwardListItem:[[webView backForwardList] itemAtIndex:-2]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);

    // Go 2 items forward.
    EXPECT_EQ(2U, [[[webView backForwardList] forwardList] count]);
    [webView goToBackForwardListItem:[[webView backForwardList] itemAtIndex:2]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    // Go back.
    EXPECT_EQ(2U, [[[webView backForwardList] backList] count]);
    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html#foo", [[webView URL] absoluteString]);
}

TEST(ProcessSwap, ClearWebsiteDataWithSuspendedPage)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().usesWebProcessCache = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:pageCache1Bytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:pageCache1Bytes];
    [handler addMappingFromURLString:@"pson://www.google.com/main.html" toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad1 = [webView _webProcessIdentifier];

    EXPECT_EQ(1u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad2 = [webView _webProcessIdentifier];
    EXPECT_NE(pidAfterLoad1, pidAfterLoad2);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad3 = [webView _webProcessIdentifier];
    EXPECT_NE(pidAfterLoad2, pidAfterLoad3);

    __block bool readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(ProcessSwap, PageCacheAfterProcessSwapByClient)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:pageCache1Bytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/main2.html" toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad1 = [webView _webProcessIdentifier];

    EXPECT_EQ(1u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);

    // We force a proces-swap via client API.
    delegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowInNewProcess);
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad2 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_NE(pidAfterLoad1, pidAfterLoad2);

    delegate->decidePolicyForNavigationAction = nil;

    [webView goBack];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad3 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_EQ(pidAfterLoad1, pidAfterLoad3);
    EXPECT_EQ(1u, [receivedMessages count]);
    EXPECT_TRUE([receivedMessages.get()[0] isEqualToString:@"Was persisted" ]);
    EXPECT_EQ(2u, seenPIDs.size());

    [webView goForward];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad4 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_EQ(pidAfterLoad2, pidAfterLoad4);
    EXPECT_EQ(2u, [receivedMessages count]);
    EXPECT_TRUE([receivedMessages.get()[1] isEqualToString:@"Was persisted" ]);
    EXPECT_EQ(2u, seenPIDs.size());
}

TEST(ProcessSwap, PageCacheWhenNavigatingFromJS)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:pageCache1Bytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad1 = [webView _webProcessIdentifier];

    EXPECT_EQ(1u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad2 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_NE(pidAfterLoad1, pidAfterLoad2);

    [webView evaluateJavaScript:@"history.back()" completionHandler: nil];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad3 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_EQ(pidAfterLoad1, pidAfterLoad3);
    EXPECT_EQ(1u, [receivedMessages count]);
    EXPECT_TRUE([receivedMessages.get()[0] isEqualToString:@"Was persisted" ]);
    EXPECT_EQ(2u, seenPIDs.size());

    [webView evaluateJavaScript:@"history.forward()" completionHandler: nil];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pidAfterLoad4 = [webView _webProcessIdentifier];

    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_EQ(pidAfterLoad2, pidAfterLoad4);
    EXPECT_EQ(2u, [receivedMessages count]);
    EXPECT_TRUE([receivedMessages.get()[1] isEqualToString:@"Was persisted" ]);
    EXPECT_EQ(2u, seenPIDs.size());
}

#if PLATFORM(MAC) // WebProcessCache is disabled on devices with too little RAM.

TEST(ProcessSwap, UseWebProcessCacheAfterTermination)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    int webkitPID = 0;

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;
        webkitPID = [webView _webProcessIdentifier];
    }

    EXPECT_EQ(1U, [processPool _webProcessCountIgnoringPrewarmed]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto applePID = [webView _webProcessIdentifier];

    EXPECT_NE(webkitPID, applePID);

    EXPECT_EQ(2U, [processPool _webProcessCountIgnoringPrewarmed]);

    __block bool webProcessTerminated = false;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *) {
        webProcessTerminated = true;
    }];

    kill(applePID, 9);
    TestWebKitAPI::Util::run(&webProcessTerminated);
    webProcessTerminated = false;

    EXPECT_EQ(0, [webView _webProcessIdentifier]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_EQ(1U, [processPool _webProcessCountIgnoringPrewarmed]);
}

TEST(ProcessSwap, ProcessCrashedWhileInTheCache)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    int webkitPID = 0;

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;
        webkitPID = [webView _webProcessIdentifier];
    }

    while ([processPool _processCacheSize] != 1)
        TestWebKitAPI::Util::runFor(0.1_s);

    kill(webkitPID, 9);

    while ([processPool _processCacheSize])
        TestWebKitAPI::Util::runFor(0.1_s);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_NE(webkitPID, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, ProcessTerminatedWhileInTheCache)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    int webkitPID = 0;

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;
        webkitPID = [webView _webProcessIdentifier];
    }

    while ([processPool _processCacheSize] != 1)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE([processPool _requestWebProcessTermination:webkitPID]);
    TestWebKitAPI::Util::spinRunLoop(100);

    EXPECT_EQ(0U, [processPool _processCacheSize]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_NE(webkitPID, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, UseWebProcessCacheForLoadInNewView)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);

    int webkitPID = 0;

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];

        // Process launch should be delayed until a load actually happens.
        EXPECT_EQ(0, [webView _webProcessIdentifier]);
        TestWebKitAPI::Util::runFor(0.1_s);
        EXPECT_EQ(0, [webView _webProcessIdentifier]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;
        webkitPID = [webView _webProcessIdentifier];
        EXPECT_NE(0, webkitPID);
    }

    while ([processPool _processCacheSize] != 1)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_EQ(1U, [processPool _webProcessCountIgnoringPrewarmed]);

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;

        EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
        EXPECT_EQ(1U, [processPool _webProcessCountIgnoringPrewarmed]);
    }

    while ([processPool _processCacheSize] != 1)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_EQ(1U, [processPool _webProcessCountIgnoringPrewarmed]);

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;

        EXPECT_NE(webkitPID, [webView _webProcessIdentifier]);
    }

    EXPECT_EQ(2U, [processPool _webProcessCountIgnoringPrewarmed]);
}

#endif // PLATFORM(MAC)

TEST(ProcessSwap, NumberOfPrewarmedProcesses)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(2u, [processPool _webProcessCount]);
    EXPECT_EQ(1u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_TRUE([processPool _hasPrewarmedWebProcess]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(3u, [processPool _webProcessCount]);
    EXPECT_EQ(2u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_TRUE([processPool _hasPrewarmedWebProcess]);
}

#if PLATFORM(MAC)

TEST(ProcessSwap, NumberOfCachedProcesses)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().prewarmsProcessesAutomatically = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    EXPECT_GT([processPool _maximumSuspendedPageCount], 0u);
    EXPECT_GT([processPool _processCacheCapacity], 0u);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);

    const unsigned maxSuspendedPageCount = [processPool _maximumSuspendedPageCount];
    for (unsigned i = 0; i < maxSuspendedPageCount + 2; i++)
        [handler addMappingFromURLString:makeString("pson://www.domain-", i, ".com") toData:pageCache1Bytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    for (unsigned i = 0; i < maxSuspendedPageCount + 1; i++) {
        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:makeString("pson://www.domain-", i, ".com")]];
        [webView loadRequest:request];
        TestWebKitAPI::Util::run(&done);
        done = false;

        EXPECT_EQ(i + 1, [processPool _webProcessCount]);
        EXPECT_EQ(i + 1, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
        EXPECT_FALSE([processPool _hasPrewarmedWebProcess]);
    }

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:makeString("pson://www.domain-", maxSuspendedPageCount + 1, ".com")]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    int timeout = 100;
    while (([processPool _webProcessCount] > (maxSuspendedPageCount + 2) &&  [processPool _webProcessCountIgnoringPrewarmedAndCached] > (maxSuspendedPageCount + 1)) && timeout > 0) {
        TestWebKitAPI::Util::runFor(0.1_s);
        --timeout;
    }

    EXPECT_EQ(maxSuspendedPageCount + 2, [processPool _webProcessCount]);
    EXPECT_EQ(maxSuspendedPageCount + 1, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_FALSE([processPool _hasPrewarmedWebProcess]);

    static bool readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    timeout = 100;
    while ([processPool _webProcessCount] > 1 && timeout > 0) {
        TestWebKitAPI::Util::runFor(0.1_s);
        --timeout;
    }

    EXPECT_EQ(1u, [processPool _webProcessCount]);
    EXPECT_EQ(1u, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    EXPECT_FALSE([processPool _hasPrewarmedWebProcess]);
}

#endif // PLATFORM(MAC)

static const char* visibilityBytes = R"PSONRESOURCE(
<script>
window.addEventListener('pageshow', function(event) {
    var msg = window.location.href + " - pageshow ";
    msg += event.persisted ? "persisted" : "NOT persisted";
    window.webkit.messageHandlers.pson.postMessage(msg);
});

window.addEventListener('pagehide', function(event) {
    var msg = window.location.href + " - pagehide ";
    msg += event.persisted ? "persisted" : "NOT persisted";
    window.webkit.messageHandlers.pson.postMessage(msg);
});
</script>
)PSONRESOURCE";

TEST(ProcessSwap, PageShowHide)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:visibilityBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:visibilityBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goForward];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    while ([receivedMessages count] < 7)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_EQ(7u, [receivedMessages count]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pageshow NOT persisted", receivedMessages.get()[0]);
    if ([receivedMessages.get()[1] hasPrefix:@"pson://www.webkit.org/main.html"]) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pagehide persisted", receivedMessages.get()[1]);
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - pageshow NOT persisted", receivedMessages.get()[2]);
    } else {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - pageshow NOT persisted", receivedMessages.get()[1]);
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pagehide persisted", receivedMessages.get()[2]);
    }
    if ([receivedMessages.get()[3] hasPrefix:@"pson://www.apple.com/main.html"]) {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - pagehide persisted", receivedMessages.get()[3]);
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pageshow persisted", receivedMessages.get()[4]);
    } else {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pageshow persisted", receivedMessages.get()[3]);
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - pagehide persisted", receivedMessages.get()[4]);
    }
    if ([receivedMessages.get()[5] hasPrefix:@"pson://www.webkit.org/main.html"]) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pagehide persisted", receivedMessages.get()[5]);
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - pageshow persisted", receivedMessages.get()[6]);
    } else {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - pageshow persisted", receivedMessages.get()[5]);
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - pagehide persisted", receivedMessages.get()[6]);
    }
}

// Disabling the back/forward cache explicitly is (for some reason) not available on iOS.
#if !TARGET_OS_IPHONE
static const char* loadUnloadBytes = R"PSONRESOURCE(
<script>
window.addEventListener('unload', function(event) {
    var msg = window.location.href + " - unload";
    window.webkit.messageHandlers.pson.postMessage(msg);
});

window.addEventListener('load', function(event) {
    var msg = window.location.href + " - load";
    window.webkit.messageHandlers.pson.postMessage(msg);
});
</script>
)PSONRESOURCE";

TEST(ProcessSwap, LoadUnload)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:loadUnloadBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:loadUnloadBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];
    [[webViewConfiguration preferences] _setUsesPageCache:NO];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goForward];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    while ([receivedMessages count] < 7)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_EQ(7u, [receivedMessages count]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - load", receivedMessages.get()[0]);
    if ([receivedMessages.get()[1] hasPrefix:@"pson://www.webkit.org/main.html"]) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - unload", receivedMessages.get()[1]);
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - load", receivedMessages.get()[2]);
    } else {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - load", receivedMessages.get()[1]);
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - unload", receivedMessages.get()[2]);
    }
    if ([receivedMessages.get()[3] hasPrefix:@"pson://www.apple.com/main.html"]) {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - unload", receivedMessages.get()[3]);
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - load", receivedMessages.get()[4]);
    } else {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - load", receivedMessages.get()[3]);
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - unload", receivedMessages.get()[4]);
    }
    if ([receivedMessages.get()[5] hasPrefix:@"pson://www.webkit.org/main.html"]) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - unload", receivedMessages.get()[5]);
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - load", receivedMessages.get()[6]);
    } else {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html - load", receivedMessages.get()[5]);
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html - unload", receivedMessages.get()[6]);
    }
}

TEST(ProcessSwap, WebInspector)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    [[webView _inspector] show];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    [[webView _inspector] close];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];

    EXPECT_NE(pid1, pid2); // We should have swapped.
    EXPECT_NE(pid2, pid3); // We should have swapped again.
    EXPECT_EQ(numberOfDecidePolicyCalls, 3);
}

TEST(ProcessSwap, WebInspectorDelayedProcessLaunch)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    EXPECT_EQ(0, [webView _webProcessIdentifier]);
    TestWebKitAPI::Util::spinRunLoop(100);
    EXPECT_EQ(0, [webView _webProcessIdentifier]);

    [[webView _inspector] show];
    EXPECT_TRUE([[webView _inspector] isConnected]);

    // Trying to inspect the view should launch a WebProcess.
    while (![webView _webProcessIdentifier])
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_NE(0, [webView _webProcessIdentifier]);

    [[webView _inspector] close];
}

#endif // !TARGET_OS_IPHONE

static const char* sameOriginBlobNavigationTestBytes = R"PSONRESOURCE(
<!DOCTYPE html>
<html>
<body>
<p><a id="link">Click here</a></p>
<script>
const blob = new Blob(['<!DOCTYPE html><html><p>PASS</p></html>'], {type: 'text/html'});
link.href = URL.createObjectURL(blob);
</script>
)PSONRESOURCE";

TEST(ProcessSwap, SameOriginBlobNavigation)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:sameOriginBlobNavigationTestBytes]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);

    [webView _evaluateJavaScriptWithoutUserGesture:@"document.getElementById('link').click()" completionHandler: nil];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);
    EXPECT_EQ(2, numberOfDecidePolicyCalls);
    EXPECT_EQ(pid1, pid2);
}

TEST(ProcessSwap, CrossOriginBlobNavigation)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:sameOriginBlobNavigationTestBytes]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);

    bool finishedRunningScript = false;
    String blobURL;
    [webView _evaluateJavaScriptWithoutUserGesture:@"document.getElementById('link').href" completionHandler: [&] (id result, NSError *error) {
        blobURL = String([NSString stringWithFormat:@"%@", result]);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    finishedRunningScript = false;
    String script = "document.getElementById('link').href = '" + blobURL + "'";
    [webView _evaluateJavaScriptWithoutUserGesture:(NSString *)script completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    // This navigation will fail.
    [webView _evaluateJavaScriptWithoutUserGesture:@"document.getElementById('link').click()" completionHandler: [&] (id result, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid3 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid3);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);
    EXPECT_NE(pid1, pid2);
    EXPECT_EQ(pid2, pid3);
}

TEST(ProcessSwap, NavigateToAboutBlank)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:sameOriginBlobNavigationTestBytes]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);
    EXPECT_EQ(pid1, pid2);
}

TEST(ProcessSwap, NavigateToDataURL)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:sameOriginBlobNavigationTestBytes]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/plain,PASS"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);
    EXPECT_EQ(pid1, pid2);
}

TEST(ProcessSwap, ProcessReuse)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];

    // Two process swaps have occurred, but we should only have ever seen 2 pids.
    EXPECT_EQ(2u, seenPIDs.size());
    EXPECT_NE(pid1, pid2);
    EXPECT_NE(pid2, pid3);
    EXPECT_EQ(pid1, pid3);
}

TEST(ProcessSwap, ProcessReuseeTLDPlus2)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www1.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www2.webkit.org/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];

    // Two process swaps have occurred, but we should only have ever seen 2 pids.
    EXPECT_EQ(2u, seenPIDs.size());
    EXPECT_NE(pid1, pid2);
    EXPECT_NE(pid2, pid3);
    EXPECT_EQ(pid1, pid3);
}

TEST(ProcessSwap, ConcurrentHistoryNavigations)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView _webProcessIdentifier];

    EXPECT_NE(webkitPID, applePID);

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);

    auto* backForwardList = [webView backForwardList];
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.backItem);
    EXPECT_EQ(1U, backForwardList.forwardList.count);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.forwardItem.URL absoluteString]);

    // Concurrent requests to go forward, which process swaps.
    [webView goForward];
    [webView goForward];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);
    EXPECT_EQ(1U, backForwardList.backList.count);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.backItem.URL absoluteString]);

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_NE(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.backItem);
    EXPECT_EQ(1U, backForwardList.forwardList.count);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.forwardItem.URL absoluteString]);
}

TEST(ProcessSwap, NavigateToInvalidURL)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);

    EXPECT_EQ(1, numberOfDecidePolicyCalls);

    __block bool evaluated = false;
    [webView evaluateJavaScript:@"var err = 'no error'; try { location.href = 'http://A=a%B=b' } catch(e) { err=e; }; err.message" completionHandler:^(id result, NSError *error) {
        EXPECT_WK_STREQ(result, "Invalid URL");
        EXPECT_NULL(error);
        evaluated = true;
    }];

    TestWebKitAPI::Util::run(&evaluated);

    TestWebKitAPI::Util::spinRunLoop(1);

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_EQ(1, numberOfDecidePolicyCalls);
    EXPECT_EQ(pid1, pid2);
}

static const char* navigateToDataURLThenBackBytes = R"PSONRESOURCE(
<script>
onpageshow = function(event) {
    if (sessionStorage.getItem('navigated') == 'true') {
        sessionStorage.clear();
        return;
    }
    sessionStorage.setItem('navigated', 'true');

    // Location changes need to happen outside the onload handler to generate history entries.
    setTimeout(function() {
      window.location.href = "data:text/html,<body onload='history.back()'></body>";
    }, 0);
}

</script>
)PSONRESOURCE";

TEST(ProcessSwap, NavigateToDataURLThenBack)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().pageCacheEnabled = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get()._allowTopNavigationToDataURLs = YES;
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:navigateToDataURLThenBackBytes]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid2 = [webView _webProcessIdentifier];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid3 = [webView _webProcessIdentifier];

    EXPECT_EQ(3, numberOfDecidePolicyCalls);
    EXPECT_EQ(1u, seenPIDs.size());
    EXPECT_EQ(pid1, pid2);
    EXPECT_EQ(pid2, pid3);
}

TEST(ProcessSwap, NavigateCrossSiteWithPageCacheDisabled)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().pageCacheEnabled = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto webkitPID = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto applePID = [webView _webProcessIdentifier];

    EXPECT_NE(webkitPID, applePID);

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_NE(applePID, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, APIControlledProcessSwapping)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:"Hello World!"]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/1"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];

    // Navigating from the above URL to this URL normally should not process swap.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/2"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid2 = [webView _webProcessIdentifier];

    EXPECT_EQ(1u, seenPIDs.size());
    EXPECT_EQ(pid1, pid2);

    // Navigating from the above URL to this URL normally should not process swap,
    // but we'll explicitly ask for a swap.
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowInNewProcess);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/3"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid3 = [webView _webProcessIdentifier];

    EXPECT_EQ(3, numberOfDecidePolicyCalls);
    EXPECT_EQ(2u, seenPIDs.size());
    EXPECT_NE(pid1, pid3);
}

enum class WithDelay : bool { No, Yes };
static void runAPIControlledProcessSwappingThenBackTest(WithDelay withDelay)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[PSONScheme alloc] initWithBytes:"Hello World!"]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];
    
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    auto pid1 = [webView _webProcessIdentifier];
    
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowInNewProcess);
    };
    
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);
    
    // Give time to the suspended WebPage to close.
    if (withDelay == WithDelay::Yes)
        TestWebKitAPI::Util::runFor(0.1_s);
    
    navigationDelegate->decidePolicyForNavigationAction = nil;
    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    EXPECT_EQ(pid1, [webView _webProcessIdentifier]);
}

TEST(ProcessSwap, APIControlledProcessSwappingThenBackWithDelay)
{
    runAPIControlledProcessSwappingThenBackTest(WithDelay::Yes);
}

TEST(ProcessSwap, APIControlledProcessSwappingThenBackWithoutDelay)
{
    runAPIControlledProcessSwappingThenBackTest(WithDelay::No);
}

TEST(ProcessSwap, NavigateToCrossSiteThenBackFromJS)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().pageCacheEnabled = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto webkitPID = [webView _webProcessIdentifier];

    [webView evaluateJavaScript:@"location.href = 'pson://www.apple.com/main.html';" completionHandler:nil];

    TestWebKitAPI::Util::run(&done);
    done = false;
    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID); // Should have process-swapped when going from webkit.org to apple.com.

    [webView evaluateJavaScript:@"history.back();" completionHandler:nil];

    // Page now calls history.back() to navigate back to webkit.org.
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(3, numberOfDecidePolicyCalls);

    // Should have process-swapped when going from apple.com to webkit.org.
    // PID is not necessarily webkitPID because PageCache is disabled.
    EXPECT_NE(applePID, [webView _webProcessIdentifier]);
}

static const char* crossSiteFormSubmissionBytes = R"PSONRESOURCE(
<body>
<form action="pson://www.apple.com/main.html" method="post">
Name: <input type="text" name="name" placeholder="Name">
<input id="submitButton" type="submit">
</form>
</body>
)PSONRESOURCE";

TEST(ProcessSwap, SwapOnFormSubmission)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:crossSiteFormSubmissionBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto webkitPID = [webView _webProcessIdentifier];
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);

    [webView evaluateJavaScript:@"submitButton.click()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto applePID = [webView _webProcessIdentifier];
    EXPECT_NE(webkitPID, applePID);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    [webView reload];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);

    [webView goForward];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_EQ(applePID, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;
#if !PLATFORM(IOS_FAMILY)
    // This is not guaranteed on iOS because the WebProcess cache is disabled on devices with too little RAM.
    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
#else
    EXPECT_NE(applePID, [webView _webProcessIdentifier]);
#endif
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [[webView URL] absoluteString]);
}

TEST(ProcessSwap, ClosePageAfterCrossSiteProvisionalLoad)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    didStartProvisionalLoad = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);

        [webView _close];
        done = true;
    };

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::runFor(0.5_s);
}

static Vector<bool> loadingStateChanges;
static unsigned urlChangeCount;

@interface PSONLoadingObserver : NSObject
@end

@implementation PSONLoadingObserver

-(void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    WKWebView *webView = (WKWebView *)context;

    if ([keyPath isEqualToString:@"loading"])
        loadingStateChanges.append([webView isLoading]);
    else if ([keyPath isEqualToString:@"URL"])
        ++urlChangeCount;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
}

@end

TEST(ProcessSwap, LoadingStateAfterPolicyDecision)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto loadObserver = adoptNS([[PSONLoadingObserver alloc] init]);
    [webView addObserver:loadObserver.get() forKeyPath:@"loading" options:0 context:webView.get()];
    [webView addObserver:loadObserver.get() forKeyPath:@"URL" options:0 context:webView.get()];

    urlChangeCount = 0;
    didStartProvisionalLoad = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]]];
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
    EXPECT_TRUE([webView isLoading]);

    EXPECT_EQ(1U, loadingStateChanges.size());
    EXPECT_TRUE(loadingStateChanges[0]);
    EXPECT_EQ(1U, urlChangeCount);

    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
        EXPECT_TRUE([webView isLoading]);

        decisionHandler(WKNavigationActionPolicyAllow);

        EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
        EXPECT_TRUE([webView isLoading]);
    };

    EXPECT_EQ(1U, loadingStateChanges.size());

    TestWebKitAPI::Util::run(&didStartProvisionalLoad);
    didStartProvisionalLoad = false;

    EXPECT_EQ(1U, loadingStateChanges.size());
    EXPECT_EQ(1U, urlChangeCount);

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
    EXPECT_TRUE([webView isLoading]);

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);
    EXPECT_FALSE([webView isLoading]);

    EXPECT_EQ(2U, loadingStateChanges.size());
    EXPECT_FALSE(loadingStateChanges[1]);
    EXPECT_EQ(1U, urlChangeCount);

    [webView removeObserver:loadObserver.get() forKeyPath:@"loading" context:webView.get()];
    [webView removeObserver:loadObserver.get() forKeyPath:@"URL" context:webView.get()];
}

static const char* saveOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    savedOpener = opener;
}
</script>
)PSONRESOURCE";

TEST(ProcessSwap, OpenerLinkAfterAPIControlledProcessSwappingOfOpener)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:windowOpenSameSiteWithOpenerTestBytes]; // Opens "pson://www.webkit.org/main2.html".
    [handler addMappingFromURLString:@"pson://www.webkit.org/main2.html" toData:saveOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(pid1, [createdWebView _webProcessIdentifier]);

    // Auxiliary window should have an opener.
    [createdWebView evaluateJavaScript:@"window.opener.location.href" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // We force a proces-swap via client API.
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowInNewProcess);
    };

    // Navigating from the above URL to this URL normally should not process swap.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main3.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);
    
    bool hasOpener = true;
    int timeout = 50;
    do {
        if (timeout != 50)
            TestWebKitAPI::Util::runFor(0.1_s);
        
        // Auxiliary window's opener should no longer have an opener.
        [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpenerString, NSError *error) {
            hasOpener = [hasOpenerString isEqualToString:@"true"];
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
    } while (hasOpener && (--timeout));
    EXPECT_FALSE(hasOpener);

    [createdWebView evaluateJavaScript:@"savedOpener.closed ? 'true' : 'false'" completionHandler: [&] (id savedOpenerIsClosed, NSError *error) {
        EXPECT_WK_STREQ(@"true", savedOpenerIsClosed);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, OpenerLinkAfterAPIControlledProcessSwappingOfOpenee)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:windowOpenSameSiteWithOpenerTestBytes]; // Opens "pson://www.webkit.org/main2.html".
    [handler addMappingFromURLString:@"pson://www.webkit.org/main2.html" toData:saveOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    auto pid1 = [webView _webProcessIdentifier];

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(pid1, [createdWebView _webProcessIdentifier]);

    // Auxiliary window should have an opener.
    [webView evaluateJavaScript:@"w.opener.location.href" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // We force a proces-swap via client API.
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowInNewProcess);
    };

    // Navigating from the above URL to this URL normally should not process swap.
    [createdWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main3.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    // Auxiliary window's opener should no longer have an opener.
    [webView evaluateJavaScript:@"w.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"false", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static void runProcessSwapDueToRelatedWebViewTest(NSURL* relatedViewURL, NSURL* targetURL, ExpectSwap expectSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webView1Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView1Configuration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webView1Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView1Configuration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView1 setNavigationDelegate:delegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:relatedViewURL];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView1 _webProcessIdentifier];

    auto webView2Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView2Configuration setProcessPool:processPool.get()];
    [webView2Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];
    webView2Configuration.get()._relatedWebView = webView1.get(); // webView2 will be related to webView1 and webView1's URL will be used for process swap decision.
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView2Configuration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    request = [NSURLRequest requestWithURL:targetURL];
    [webView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView2 _webProcessIdentifier];

    if (expectSwap == ExpectSwap::No)
        EXPECT_TRUE(pid1 == pid2);
    else
        EXPECT_FALSE(pid1 == pid2);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);
}

TEST(ProcessSwap, ProcessSwapDueToRelatedView)
{
    runProcessSwapDueToRelatedWebViewTest([NSURL URLWithString:@"pson://www.webkit.org/main1.html"], [NSURL URLWithString:@"pson://www.apple.com/main2.html"], ExpectSwap::Yes);
}

TEST(ProcessSwap, NoProcessSwapDueToRelatedView)
{
    runProcessSwapDueToRelatedWebViewTest([NSURL URLWithString:@"pson://www.webkit.org/main1.html"], [NSURL URLWithString:@"pson://www.webkit.org/main2.html"], ExpectSwap::No);
}

TEST(ProcessSwap, RelatedWebViewBeforeWebProcessLaunch)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webView1Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView1Configuration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webView1Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView1Configuration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView1 setNavigationDelegate:delegate.get()];

    auto webView2Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView2Configuration setProcessPool:processPool.get()];
    [webView2Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];
    webView2Configuration.get()._relatedWebView = webView1.get(); // webView2 will be related to webView1 and webView1's URL will be used for process swap decision.
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView2Configuration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView1 _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView2 _webProcessIdentifier];

    EXPECT_EQ(pid1, pid2); // WebViews are related so they should share the same process.
}

TEST(ProcessSwap, ReloadRelatedWebViewAfterCrash)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webView1Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView1Configuration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webView1Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView1Configuration.get()]);
    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didCrash = false;
    [delegate setWebContentProcessDidTerminate:^(WKWebView *view) {
        [view reload];
        didCrash = true;
    }];
    [delegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    [webView1 setNavigationDelegate:delegate.get()];

    auto webView2Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView2Configuration setProcessPool:processPool.get()];
    [webView2Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];
    webView2Configuration.get()._relatedWebView = webView1.get(); // webView2 will be related to webView1 and webView1's URL will be used for process swap decision.
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView2Configuration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView1 _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView2 _webProcessIdentifier];

    EXPECT_EQ(pid1, pid2); // WebViews are related so they should share the same process.

    [webView1 _close];
    webView1 = nullptr;

    kill(pid1, 9);

    TestWebKitAPI::Util::run(&didCrash);
    didCrash = false;

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, TerminatedSuspendedPageProcess)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    @autoreleasepool {
        auto webViewConfiguration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration2 setProcessPool:processPool.get()];
        [webViewConfiguration2 _setRelatedWebView:webView.get()]; // Make sure it uses the same process.
        auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration2.get()]);
        [webView2 setNavigationDelegate:delegate.get()];

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]];
        [webView2 loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        auto pid2 = [webView2 _webProcessIdentifier];
        EXPECT_EQ(pid1, pid2);

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main2.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        [webView2 _killWebContentProcessAndResetState];
        webView2 = nullptr;
        webViewConfiguration2 = nullptr;
    }

    auto pid3 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid3);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid4 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid4);
    EXPECT_NE(pid3, pid4);
}

TEST(ProcessSwap, CommittedProcessCrashDuringCrossSiteNavigation)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    done = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    static bool didKill = false;
    navigationDelegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow); // Will ask the load to proceed in a new provisional WebProcess since the navigation is cross-site.

        // Simulate a crash of the committed WebProcess while the provisional navigation starts in the new provisional WebProcess.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            kill(pid1, 9);
            didKill = true;
        });
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&didKill);

    TestWebKitAPI::Util::runFor(0.5_s);
}

TEST(ProcessSwap, NavigateBackAndForth)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto* backForwardList = [webView backForwardList];
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);
    EXPECT_EQ(1U, backForwardList.backList.count);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.backItem.URL absoluteString]);

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.backItem);
    EXPECT_EQ(1U, backForwardList.forwardList.count);
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.forwardItem.URL absoluteString]);

    [webView goForward];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [backForwardList.currentItem.URL absoluteString]);
    EXPECT_TRUE(!backForwardList.forwardItem);
    EXPECT_EQ(1U, backForwardList.backList.count);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main.html", [backForwardList.backItem.URL absoluteString]);
}

TEST(ProcessSwap, SwapOnLoadHTMLString)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    numberOfDecidePolicyCalls = 0;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    NSString *htmlString = @"<html><body>substituteData</body></html>";
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://example.com"]];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    [webView evaluateJavaScript:@"document.body.innerText" completionHandler:^(id innerText, NSError *error) {
        EXPECT_WK_STREQ(@"substituteData", (NSString *)innerText);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, EphemeralWebStorage)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/iframe.html" toData:"<script>window.localStorage.setItem('c','d')</script>"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];
    
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://webkit.org/"]]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.localStorage.setItem('a','b')" completionHandler:^(id, NSError *) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.sessionStorage.setItem('b','a')" completionHandler:^(id, NSError *) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://example.com/"]]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://webkit.org/"]]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.localStorage.getItem('a')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"b" isEqualToString:result]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.sessionStorage.getItem('b')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"a" isEqualToString:result]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    done = false;
    [webView loadHTMLString:@"<html><iframe src='pson://www.webkit.org/iframe.html'></iframe></html>" baseURL:[NSURL URLWithString:@"http://www.example.com/"]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.localStorage.getItem('a')" completionHandler:^(id result, NSError *) {
        EXPECT_FALSE([@"b" isEqualToString:result]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.sessionStorage.getItem('b')" completionHandler:^(id result, NSError *) {
        EXPECT_FALSE([@"a" isEqualToString:result]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ProcessSwap, UsePrewarmedProcessAfterTerminatingNetworkProcess)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::spinRunLoop(1);

    [webViewConfiguration.get().websiteDataStore _terminateNetworkProcess];

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView2 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, UseSessionCookiesAfterProcessSwapInPrivateBrowsing)
{
    auto originalCookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr<WKWebsiteDataStore> ephemeralStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().websiteDataStore = ephemeralStore.get();

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool setPolicy = false;
    [webView.get().configuration.websiteDataStore.httpCookieStore _setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways completionHandler:^{
        setPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setPolicy);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"SetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    // Should process-swap.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    // Should process-swap.
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];
    EXPECT_NE(pid2, pid3);

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;

    EXPECT_EQ(1u, [receivedMessages count]);
    EXPECT_WK_STREQ(@"foo=bar", receivedMessages.get()[0]);
    
    setPolicy = false;
    [webView.get().configuration.websiteDataStore.httpCookieStore _setCookieAcceptPolicy:originalCookieAcceptPolicy completionHandler:^{
        setPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setPolicy);
}

TEST(ProcessSwap, UseSessionCookiesAfterProcessSwapInNonDefaultPersistentSession)
{
    auto originalCookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    auto processPoolConfiguration = psonProcessPoolConfiguration();

    // Prevent WebProcess reuse.
    processPoolConfiguration.get().usesWebProcessCache = NO;
    processPoolConfiguration.get().pageCacheEnabled = NO;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = NO;

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    auto customDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);

    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().websiteDataStore = customDataStore.get();

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool setPolicy = false;
    [webView.get().configuration.websiteDataStore.httpCookieStore _setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways completionHandler:^{
        setPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setPolicy);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"SetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    // Should process-swap.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    // Should process-swap.
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];
    EXPECT_NE(pid2, pid3);
    EXPECT_NE(pid1, pid3);

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;

    EXPECT_EQ(1u, [receivedMessages count]);
    EXPECT_WK_STREQ(@"foo=bar", receivedMessages.get()[0]);

    setPolicy = false;
    [webView.get().configuration.websiteDataStore.httpCookieStore _setCookieAcceptPolicy:originalCookieAcceptPolicy completionHandler:^{
        setPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setPolicy);
}

TEST(ProcessSwap, ProcessSwapInRelatedView)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webView1Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView1Configuration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webView1Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView1Configuration.get()]);

    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView1 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView1 loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto applePID = [webView1 _webProcessIdentifier];

    auto webView2Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webView2Configuration setProcessPool:processPool.get()];
    [webView2Configuration _setRelatedWebView:webView1.get()];
    [webView2Configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView2Configuration.get()]);
    [webView2 _restoreSessionState:webView1.get()._sessionState andNavigate:NO];

    [webView2 setNavigationDelegate:delegate.get()];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView2 _webProcessIdentifier];
    EXPECT_NE(applePID, webkitPID);
}

TEST(ProcessSwap, TerminateProcessAfterProcessSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setAllowsBackForwardNavigationGestures:YES];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    __block bool webProcessTerminated = false;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *) {
        webProcessTerminated = true;
    }];
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    // Make sure there is a gesture controller.
#if PLATFORM(MAC)
    [webView _setCustomSwipeViewsTopContentInset:2.];
#endif

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webkitPID = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_NE(webkitPID, [webView _webProcessIdentifier]);

    webProcessTerminated = false;
    kill([webView _webProcessIdentifier], 9);

    TestWebKitAPI::Util::run(&webProcessTerminated);

    TestWebKitAPI::Util::spinRunLoop(1);

    [webView reload];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

#if PLATFORM(IOS_FAMILY)
static bool viewHasSwipeGestures(UIView *view)
{
    unsigned swipeGestureCount = 0;
    for (UIGestureRecognizer *recognizer in view.gestureRecognizers) {
        if ([recognizer isKindOfClass:NSClassFromString(@"UIScreenEdgePanGestureRecognizer")])
            swipeGestureCount++;
    }

    return swipeGestureCount == 2;
}
#endif

TEST(ProcessSwap, SwapWithGestureController)
{
    @autoreleasepool {
        auto processPoolConfiguration = psonProcessPoolConfiguration();
        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration setProcessPool:processPool.get()];
        auto handler = adoptNS([[PSONScheme alloc] init]);
        [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

        auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:delegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        // Ensure a ViewGestureController is created.
        [webView setAllowsBackForwardNavigationGestures:YES];
#if PLATFORM(MAC)
        [webView _setCustomSwipeViewsTopContentInset:2.];
#endif

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

#if PLATFORM(IOS_FAMILY)
        EXPECT_TRUE(viewHasSwipeGestures(webView.get()));
#endif
    }
}

TEST(ProcessSwap, CrashWithGestureController)
{
    @autoreleasepool {
        auto processPoolConfiguration = psonProcessPoolConfiguration();
        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration setProcessPool:processPool.get()];
        auto handler = adoptNS([[PSONScheme alloc] init]);
        [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

        auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        __block bool webProcessTerminated = false;
        [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *) {
            webProcessTerminated = true;
        }];
        [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
            done = true;
        }];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        // Ensure a ViewGestureController is created.
        [webView setAllowsBackForwardNavigationGestures:YES];
#if PLATFORM(MAC)
        [webView _setCustomSwipeViewsTopContentInset:2.];
#endif

        webProcessTerminated = false;
        kill([webView _webProcessIdentifier], 9);

        TestWebKitAPI::Util::run(&webProcessTerminated);

        TestWebKitAPI::Util::spinRunLoop(1);

        [webView reload];
        TestWebKitAPI::Util::run(&done);
        done = false;

#if PLATFORM(IOS_FAMILY)
        EXPECT_TRUE(viewHasSwipeGestures(webView.get()));
#endif
    }
}

#if PLATFORM(MAC)

TEST(ProcessSwap, NavigateCrossOriginWithOpenee)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:windowOpenSameSiteWithOpenerTestBytes]; // Opens "pson://www.webkit.org/main2.html".
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ([webView _webProcessIdentifier], [createdWebView _webProcessIdentifier]);
    auto webkitPID = [webView _webProcessIdentifier];

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", [[webView URL] absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main2.html", [[createdWebView URL] absoluteString]);

    // New window should have an opener.
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"true", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Navigate cross-origin.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[webView URL] absoluteString]);

    // Auxiliary window should still have an opener.
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"true", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // We should not have process-swapped since an auxiliary window has an opener link to us.
    EXPECT_EQ(webkitPID, [webView _webProcessIdentifier]);
}

static const char* crossSiteLinkWithOpenerTestBytes = R"PSONRESOURCE(
<script>
function saveOpenee()
{
    openee = window.open("", "foo");
}
</script>
<a id="testLink" target="foo" href="pson://www.webkit.org/main2.html">Link</a>
)PSONRESOURCE";

TEST(ProcessSwap, NavigateCrossOriginWithOpener)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:crossSiteLinkWithOpenerTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Opens "pson://www.webkit.org/main2.html" in an auxiliary window.
    [webView evaluateJavaScript:@"testLink.click()" completionHandler:nil];

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ([webView _webProcessIdentifier], [createdWebView _webProcessIdentifier]);
    auto webkitPID = [webView _webProcessIdentifier];

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", [[webView URL] absoluteString]);
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main2.html", [[createdWebView URL] absoluteString]);

    [webView evaluateJavaScript:@"saveOpenee()" completionHandler: [&] (id, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"openee.location.href" completionHandler: [&] (id openeeURL, NSError *error) {
        EXPECT_WK_STREQ(@"pson://www.webkit.org/main2.html", openeeURL);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // New window should have an opener.
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"true", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Navigate auxiliary window cross-origin.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [createdWebView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[createdWebView URL] absoluteString]);

    // Auxiliary window should still have an opener.
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"true", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    [createdWebView evaluateJavaScript:@"window.opener.closed ? 'true' : 'false'" completionHandler: [&] (id openerIsClosed, NSError *error) {
        EXPECT_WK_STREQ(@"false", openerIsClosed);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // We should not have process-swapped since the auxiliary window has an opener.
    EXPECT_EQ(webkitPID, [createdWebView _webProcessIdentifier]);

    // Have the openee disown its opener.
    [createdWebView evaluateJavaScript:@"window.opener = null" completionHandler: [&] (id, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Auxiliary window should not have an opener.
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"false", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Navigate openee cross-origin again.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
    [createdWebView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Auxiliary window should not have an opener.
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id hasOpener, NSError *error) {
        EXPECT_WK_STREQ(@"false", hasOpener);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.google.com/main.html", [[createdWebView URL] absoluteString]);
    // We still should not have process-swapped since the auxiliary window's opener still has a handle to its openee.
    EXPECT_EQ(webkitPID, [createdWebView _webProcessIdentifier]);

    [webView evaluateJavaScript:@"openee.closed ? 'true' : 'false'" completionHandler: [&] (id openeeIsClosed, NSError *error) {
        EXPECT_WK_STREQ(@"false", openeeIsClosed);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, GoBackToSuspendedPageWithMainFrameIDThatIsNotOne)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main1.html" toData:targetBlankSameSiteNoOpenerTestBytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/main2.html" toData:linkToAppleTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView1 setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView1 setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html"]];

    [webView1 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html", [[webView1 URL] absoluteString]);
    auto pid1 = [webView1 _webProcessIdentifier];

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);
    done = false;

    // New WKWebView has now navigated to webkit.org.
    EXPECT_WK_STREQ(@"pson://www.webkit.org/main2.html", [[createdWebView URL] absoluteString]);
    auto pid2 = [createdWebView _webProcessIdentifier];

    // We process-swap since there is no opener relationship.
    EXPECT_NE(pid1, pid2);

    // Click link in new WKWebView so that it navigates cross-site to apple.com.
    [createdWebView evaluateJavaScript:@"testLink.click()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // New WKWebView has now navigated to apple.com.
    EXPECT_WK_STREQ(@"pson://www.apple.com/main.html", [[createdWebView URL] absoluteString]);
    auto pid3 = [createdWebView _webProcessIdentifier];
    EXPECT_NE(pid1, pid3); // Should have process-swapped.

    // Navigate back to the suspended page (should use the back/forward cache).
    [createdWebView goBack];
    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main2.html", [[createdWebView URL] absoluteString]);
    auto pid4 = [createdWebView _webProcessIdentifier];
    EXPECT_EQ(pid2, pid4); // Should have process-swapped to the original "suspended" process.

    // Do a fragment navigation in the original WKWebView and make sure this does not crash.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main1.html#testLink"]];
    [webView1 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ(@"pson://www.webkit.org/main1.html#testLink", [[webView1 URL] absoluteString]);
    auto pid5 = [createdWebView _webProcessIdentifier];
    EXPECT_EQ(pid2, pid5);
}

#endif // PLATFORM(MAC)

static const char* tallPageBytes = R"PSONRESOURCE(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body {
    margin: 0;
    width: 100%;
    height: 10000px;
}
</style>
</head>
<body>
<script>
// Pages with dedicated workers do not go into back/forward cache.
var myWorker = new Worker('worker.js');
</script>
<a id="testLink" href="pson://www.apple.com/main.html">Test</a>
</body>
</html>
)PSONRESOURCE";

static unsigned waitUntilScrollPositionIsRestored(WKWebView *webView)
{
    unsigned scrollPosition = 0;
    do {
        [webView evaluateJavaScript:@"window.scrollY" completionHandler: [&] (id result, NSError *error) {
            scrollPosition = [result integerValue];
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
    } while (!scrollPosition);

    return scrollPosition;
}

TEST(ProcessSwap, ScrollPositionRestoration)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:tallPageBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"scroll(0, 5000)" completionHandler: [&] (id result, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    do {
        TestWebKitAPI::Util::runFor(0.05_s);
    } while (lroundf([[[webView backForwardList] currentItem] _scrollPosition].y) != 5000);

    [webView evaluateJavaScript:@"testLink.click()" completionHandler: nil];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"window.scrollY" completionHandler: [&] (id result, NSError *error) {
        EXPECT_EQ(0, [result integerValue]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto scrollPosition = waitUntilScrollPositionIsRestored(webView.get());
    EXPECT_EQ(5000U, scrollPosition);

    [webView evaluateJavaScript:@"scroll(0, 4000)" completionHandler: [&] (id result, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    do {
        TestWebKitAPI::Util::runFor(0.05_s);
    } while (lroundf([[[webView backForwardList] currentItem] _scrollPosition].y) != 4000);

    [webView evaluateJavaScript:@"testLink.click()" completionHandler: nil];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"window.scrollY" completionHandler: [&] (id result, NSError *error) {
        EXPECT_EQ(0, [result integerValue]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    scrollPosition = waitUntilScrollPositionIsRestored(webView.get());
    EXPECT_EQ(4000U, scrollPosition);
}

static NSString *blockmeFilter = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*blockme.html\"}}]";

static const char* contentBlockingAfterProcessSwapTestBytes = R"PSONRESOURCE(
<body>
<script>
let wasSubframeLoaded = false;
// Pages with dedicated workers do not go into back/forward cache.
var myWorker = new Worker('worker.js');
</script>
<iframe src="blockme.html"></iframe>
</body>
)PSONRESOURCE";

static const char* markSubFrameAsLoadedTestBytes = R"PSONRESOURCE(
<script>
top.wasSubframeLoaded = true;
</script>
)PSONRESOURCE";

TEST(ProcessSwap, ContentBlockingAfterProcessSwap)
{
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" completionHandler:^(NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" encodedContentRuleList:blockmeFilter completionHandler:^(WKContentRuleList *ruleList, NSError *error) {

        EXPECT_NOT_NULL(ruleList);
        EXPECT_NULL(error);

        [webViewConfiguration.get().userContentController addContentRuleList:ruleList];

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:contentBlockingAfterProcessSwapTestBytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/blockme.html" toData:markSubFrameAsLoadedTestBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:contentBlockingAfterProcessSwapTestBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/blockme.html" toData:markSubFrameAsLoadedTestBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"window.wasSubframeLoaded ? 'FAIL' : 'PASS'" completionHandler: [&] (id result, NSError *error) {
        NSString *blockSuccess = (NSString *)result;
        EXPECT_WK_STREQ(@"PASS", blockSuccess);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"window.wasSubframeLoaded ? 'FAIL' : 'PASS'" completionHandler: [&] (id result, NSError *error) {
        NSString *blockSuccess = (NSString *)result;
        EXPECT_WK_STREQ(@"PASS", blockSuccess);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"window.wasSubframeLoaded ? 'FAIL' : 'PASS'" completionHandler: [&] (id result, NSError *error) {
        NSString *blockSuccess = (NSString *)result;
        EXPECT_WK_STREQ(@"PASS", blockSuccess);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView goForward];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"window.wasSubframeLoaded ? 'FAIL' : 'PASS'" completionHandler: [&] (id result, NSError *error) {
        NSString *blockSuccess = (NSString *)result;
        EXPECT_WK_STREQ(@"PASS", blockSuccess);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" completionHandler:^(NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static const char* notifyLoadedBytes = R"PSONRESOURCE(
<script>
    window.webkit.messageHandlers.pson.postMessage("Loaded");
</script>
)PSONRESOURCE";

TEST(ProcessSwap, ContentExtensionBlocksMainLoadThenReloadWithoutExtensions)
{
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" completionHandler:^(NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get()._allowTopNavigationToDataURLs = YES;

    RetainPtr<PSONMessageHandler> messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" encodedContentRuleList:blockmeFilter completionHandler:^(WKContentRuleList *ruleList, NSError *error) {

        EXPECT_NOT_NULL(ruleList);
        EXPECT_NULL(error);

        [webViewConfiguration.get().userContentController addContentRuleList:ruleList];

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.apple.com/blockme.html" toData:notifyLoadedBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    receivedMessage = false;
    failed = false;
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/blockme.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&failed);
    failed = false;
    EXPECT_FALSE(receivedMessage);

    [webView _loadAlternateHTMLString:@"Blocked" baseURL:[NSURL URLWithString:@"data:text/html,"] forUnreachableURL:[NSURL URLWithString:@"pson://www.apple.com/blockme.html"]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView _reloadWithoutContentBlockers];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_FALSE(failed);
    EXPECT_TRUE(receivedMessage);
    EXPECT_WK_STREQ(@"pson://www.apple.com/blockme.html", [[webView URL] absoluteString]);

    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" completionHandler:^(NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ProcessSwap, LoadAlternativeHTML)
{
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" completionHandler:^(NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get()._allowTopNavigationToDataURLs = YES;

    RetainPtr<PSONMessageHandler> messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"ContentBlockingAfterProcessSwapExtension" encodedContentRuleList:blockmeFilter completionHandler:^(WKContentRuleList *ruleList, NSError *error) {

        EXPECT_NOT_NULL(ruleList);
        EXPECT_NULL(error);

        [webViewConfiguration.get().userContentController addContentRuleList:ruleList];

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.apple.com/blockme.html" toData:notifyLoadedBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    receivedMessage = false;
    failed = false;
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/blockme.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&failed);
    failed = false;
    EXPECT_FALSE(receivedMessage);

    [webView _loadAlternateHTMLString:@"Blocked" baseURL:[NSURL URLWithString:@"foo:blockedWarning.html"] forUnreachableURL:[NSURL URLWithString:@"pson://www.apple.com/blockme.html"]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

#if ENABLE(MEDIA_STREAM)

static bool isCapturing = false;
static bool isNotCapturing = false;
@interface GetUserMediaUIDelegate : UserMediaCaptureUIDelegate
- (void)_webView:(WKWebView *)webView mediaCaptureStateDidChange:(_WKMediaCaptureStateDeprecated)state;
@end

@implementation GetUserMediaUIDelegate
- (void)_webView:(WKWebView *)webView mediaCaptureStateDidChange:(_WKMediaCaptureStateDeprecated)state
{
    isCapturing = state == _WKMediaCaptureStateDeprecatedActiveCamera;
    isNotCapturing = !state;
}
@end

static const char* getUserMediaBytes = R"PSONRESOURCE(
<head>
<body>
<script>
navigator.mediaDevices.getUserMedia({video: true});
</script>
</body>
</head>
)PSONRESOURCE";

TEST(ProcessSwap, GetUserMediaCaptureState)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().pageCacheEnabled = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [webViewConfiguration.get() preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    webViewConfiguration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._getUserMediaRequiresFocus = NO;

    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/getUserMedia.html" toData:getUserMediaBytes];
    [handler addMappingFromURLString:@"pson://www.apple.org/test.html" toData:""];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    webView.get()._mediaCaptureReportingDelayForTesting = 1;

    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto uiDelegate = adoptNS([[GetUserMediaUIDelegate alloc] init]);
    [webView setUIDelegate: uiDelegate.get()];

    auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/getUserMedia.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&isCapturing);

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.org/test.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    TestWebKitAPI::Util::run(&isNotCapturing);

    EXPECT_FALSE(isCapturing);
    EXPECT_FALSE(pid1 == pid2);

    isCapturing = false;
    [webView goBack];

    TestWebKitAPI::Util::run(&isCapturing);
    isCapturing = false;
    isNotCapturing = true;
}

#endif

#if !PLATFORM(MAC)
static void traverseLayerTree(CALayer *layer, void(^block)(CALayer *))
{
    for (CALayer *child in layer.sublayers)
        traverseLayerTree(child, block);
    block(layer);
}

static bool hasOverlay(CALayer *layer)
{
    __block bool hasViewOverlay = false;
    traverseLayerTree(layer, ^(CALayer *layer) {
        if ([layer.name containsString:@"View overlay container"])
            hasViewOverlay = true;
    });
    return hasViewOverlay;
}
#endif

TEST(ProcessSwap, PageOverlayLayerPersistence)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    [processPoolConfiguration setInjectedBundleURL:[[NSBundle mainBundle] URLForResource:@"TestWebKitAPI" withExtension:@"wkbundle"]];
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    [processPool _setObject:@"PageOverlayPlugIn" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/page-overlay" toData:""];
    [handler addMappingFromURLString:@"pson://www.apple.com/page-overlay" toData:""];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/page-overlay"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView waitForNextPresentationUpdate];

    // We can only look for the overlay layer in the UI-side layer tree on platforms
    // that use UI-side compositing.
#if !PLATFORM(MAC)
    EXPECT_TRUE(hasOverlay([webView layer]));
#endif

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/page-overlay"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView waitForNextPresentationUpdate];

    [webView goBack]; // Back to webkit.org.

    [webView waitForNextPresentationUpdate];

#if !PLATFORM(MAC)
    EXPECT_TRUE(hasOverlay([webView layer]));
#endif
}

#if PLATFORM(IOS)

#if __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
TEST(ProcessSwap, QuickLookRequestsPasswordAfterSwap)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto* request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"password-protected" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&didStartQuickLookLoad);
    didStartQuickLookLoad = false;

    TestWebKitAPI::Util::run(&requestedQuickLookPassword);
    requestedQuickLookPassword = false;

    TestWebKitAPI::Util::run(&didFinishQuickLookLoad);
    didFinishQuickLookLoad = false;
}
#endif

static const char* minimumWidthPageBytes = R"PSONRESOURCE(
<!DOCTYPE html>
<html>
<head>
<style>
div {
    margin: 0;
    width: 100%;
    height: 10000px;
}
</style>
</head>
<body>
<div>Test</a>
</body>
</html>
)PSONRESOURCE";

TEST(ProcessSwap, PassMinimumDeviceWidthOnNewWebView)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:minimumWidthPageBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto preferences = [[webView configuration] preferences];
    [preferences _setShouldIgnoreMetaViewport:YES];
    [webView _setMinimumEffectiveDeviceWidth:1024];

    auto* request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    bool finishedRunningScript = false;
    [webView evaluateJavaScript:@"window.innerWidth" completionHandler: [&] (id result, NSError *error) {
        NSNumber *width = (NSNumber *)result;
        EXPECT_EQ(1024, [width intValue]);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
}

#endif

TEST(ProcessSwap, SuspendAllMediaPlayback)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
#if TARGET_OS_IPHONE
    configuration.get().allowsInlineMediaPlayback = YES;
#endif
    [configuration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    __block bool loaded = false;
    [webView performAfterLoading:^{ loaded = true; }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];

    TestWebKitAPI::Util::run(&loaded);

    [webView _suspendAllMediaPlayback];

    __block bool notPlaying = false;
    [webView performAfterReceivingMessage:@"not playing" action:^() { notPlaying = true; }];
    [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];
    TestWebKitAPI::Util::run(&notPlaying);
}

TEST(ProcessSwap, PassSandboxExtension)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
#if TARGET_OS_IPHONE
    configuration.get().allowsInlineMediaPlayback = YES;
#endif
    [configuration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    NSURL *file = [[NSBundle mainBundle] URLForResource:@"autoplay-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadFileURL:file allowingReadAccessToURL:file.URLByDeletingLastPathComponent];
    [webView waitForMessage:@"loaded"];

    EXPECT_WK_STREQ(webView.get()._resourceDirectoryURL.path, file.URLByDeletingLastPathComponent.path);
}

#if PLATFORM(MAC)

static const char* pageThatOpensBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    window.open("pson://www.webkit.org/window.html", "_blank");
}
</script>
)PSONRESOURCE";

static const char* openedPage = "Hello World";

TEST(ProcessSwap, SameSiteWindowWithOpenerNavigateToFile)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    processPoolConfiguration.get().processSwapsOnWindowOpenWithOpener = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:pageThatOpensBytes];
    [handler addMappingFromURLString:@"pson://www.webkit.org/window.html" toData:openedPage];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_EQ(pid1, pid2);

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"blinking-div" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([url.scheme isEqualToString:@"file"]);

    [createdWebView loadRequest:[NSURLRequest requestWithURL:url]];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(3, numberOfDecidePolicyCalls);
    auto pid3 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid3);
    EXPECT_NE(pid2, pid3);

    [createdWebView goBack];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(4, numberOfDecidePolicyCalls);
    auto pid4 = [createdWebView _webProcessIdentifier];
    EXPECT_NE(pid3, pid4);

    [createdWebView goForward];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(5, numberOfDecidePolicyCalls);
    auto pid5 = [createdWebView _webProcessIdentifier];
    EXPECT_NE(pid4, pid5);
}

#endif // PLATFORM(MAC)


static const char* responsivePageBytes = R"PSONRESOURCE(
<meta name="viewport" content="width=device-width, initial-scale=1">
)PSONRESOURCE";

TEST(ProcessSwap, ResizeWebViewDuringCrossSiteProvisionalNavigation)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [handler addMappingFromURLString:@"pson://www.webkit.org/main.html" toData:responsivePageBytes];
    [handler addMappingFromURLString:@"pson://www.apple.com/main.html" toData:responsivePageBytes];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    __block bool finishedRunningScript = false;
    [webView evaluateJavaScript:@"window.innerWidth" completionHandler:^(id result, NSError *error) {
        NSNumber *width = (NSNumber *)result;
        EXPECT_EQ(800, [width intValue]);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;

    delegate->didStartProvisionalNavigationHandler = ^{
        EXPECT_NE(0, [webView _provisionalWebProcessIdentifier]);
        [webView setFrame:CGRectMake(0, 0, 200, 200)];
    };

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView _doAfterNextPresentationUpdate:^{
        [webView evaluateJavaScript:@"window.innerWidth" completionHandler:^(id result, NSError *error) {
            NSNumber *width = (NSNumber *)result;
            EXPECT_EQ(200, [width intValue]);
            finishedRunningScript = true;
        }];
        TestWebKitAPI::Util::run(&finishedRunningScript);
    }];
}

TEST(WebProcessCache, ClearWhenEnteringCache)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    @autoreleasepool {
        auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);
        auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);
        auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);

        auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
        [webView1 setNavigationDelegate:delegate.get()];
        [webView2 setNavigationDelegate:delegate.get()];
        [webView3 setNavigationDelegate:delegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];
        [webView1 loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.apple.com/main.html"]];
        [webView2 loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;

        request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.google.com/main.html"]];
        [webView3 loadRequest:request];

        TestWebKitAPI::Util::run(&done);
        done = false;
    }

    TestWebKitAPI::Util::spinRunLoop();

    // Clear the WebProcess cache while the processes are being checked for responsiveness.
    [processPool _clearWebProcessCache];
}

TEST(ProcessSwap, ResponsePolicyDownloadAfterCOOPProcessSwap)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/source.html"_s, { "foo"_s } },
        { "/destination.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "bar"_s } },
    }, HTTPServer::Protocol::Https);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"CrossOriginOpenerPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
        else if ([feature.key isEqualToString:@"CrossOriginEmbedderPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    done = false;
    [webView loadRequest:server.request("/source.html"_s)];
    Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    // The next navigation will get converted into a download via decidePolicyForNavigationResponse.
    shouldConvertToDownload = true;

    done = false;
    failed = false;
    [webView loadRequest:server.request("/destination.html"_s)];
    Util::run(&failed);
    failed = false;
    shouldConvertToDownload = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_EQ(pid1, pid2);

    // The layer tree should no longer be frozen since the navigation didn't happen.
    __block bool isFrozen = true;
    do {
        Util::runFor(0.1_s);
        done = false;
        [webView _isLayerTreeFrozenForTesting:^(BOOL frozen) {
            isFrozen = frozen;
            done = true;
        }];
        Util::run(&done);
    } while (isFrozen);
}

TEST(ProcessSwap, NavigateBackAfterNavigatingAwayFromCOOP)
{
    using namespace TestWebKitAPI;
    auto verifyCookieAccessDoesNotAssert = "<script>document.cookie='key=value'</script>"_s;

    HTTPServer server({
        { "/source.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin-allow-popups"_s }, { "cross-origin-embedder-policy"_s, "unsafe-none"_s } }, verifyCookieAccessDoesNotAssert } },
        { "/destination.html"_s, { "bar"_s } },
    }, HTTPServer::Protocol::Https);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"CrossOriginOpenerPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
        else if ([feature.key isEqualToString:@"CrossOriginEmbedderPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    done = false;
    [webView loadRequest:server.request("/source.html"_s)];
    Util::run(&done);
    done = false;

    [webView loadRequest:server.request("/destination.html"_s)];
    Util::run(&done);
    done = false;

    [webView goBack];
    Util::run(&done);
    done = false;
}

TEST(ProcessSwap, CommittedURLAfterNavigatingBackToCOOP)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/source.html"_s, { "foo"_s } },
        { "/destination1.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "foo"_s } },
        { "/destination2.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "foo"_s } },
    }, HTTPServer::Protocol::Https);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"CrossOriginOpenerPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
        else if ([feature.key isEqualToString:@"CrossOriginEmbedderPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    done = false;
    [webView loadRequest:server.request("/source.html"_s)];
    Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_WK_STREQ([webView _committedURL].absoluteString, server.request("/source.html"_s).URL.absoluteString);

    done = false;
    [webView loadRequest:server.request("/destination1.html"_s)];
    Util::run(&done);
    done = false;

    // Process swap due to COOP.
    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);
    EXPECT_WK_STREQ([webView _committedURL].absoluteString, server.request("/destination1.html"_s).URL.absoluteString);

    done = false;
    [webView loadRequest:server.request("/destination2.html"_s)];
    Util::run(&done);
    done = false;

    EXPECT_EQ([webView _webProcessIdentifier], pid2);
    EXPECT_WK_STREQ([webView _committedURL].absoluteString, server.request("/destination2.html"_s).URL.absoluteString);

    done = false;
    [webView goBack];
    Util::run(&done);
    done = false;

    EXPECT_EQ([webView _webProcessIdentifier], pid2);
    EXPECT_WK_STREQ([webView _committedURL].absoluteString, server.request("/destination1.html"_s).URL.absoluteString);
}

enum class IsSameOrigin : bool { No, Yes };
enum class DoServerSideRedirect : bool { No, Yes };
static void runCOOPProcessSwapTest(ASCIILiteral sourceCOOP, ASCIILiteral sourceCOEP, ASCIILiteral destinationCOOP, ASCIILiteral destinationCOEP, IsSameOrigin isSameOrigin, DoServerSideRedirect doServerSideRedirect, ExpectSwap expectSwap)
{
    using namespace TestWebKitAPI;

    HashMap<String, String> sourceHeaders;
    sourceHeaders.add("Content-Type"_s, "text/html"_s);
    if (sourceCOOP)
        sourceHeaders.add("Cross-Origin-Opener-Policy"_s, sourceCOOP);
    if (sourceCOEP)
        sourceHeaders.add("Cross-Origin-Embedder-Policy"_s, sourceCOEP);

    HashMap<String, String> destinationHeaders;
    destinationHeaders.add("Content-Type"_s, "text/html"_s);
    if (destinationCOOP)
        destinationHeaders.add("Cross-Origin-Opener-Policy"_s, destinationCOOP);
    if (destinationCOEP)
        destinationHeaders.add("Cross-Origin-Embedder-Policy"_s, destinationCOEP);
    HTTPResponse destinationResponse(WTFMove(destinationHeaders), "popup"_s);

    HTTPServer server(std::initializer_list<std::pair<String, HTTPResponse>> { }, HTTPServer::Protocol::Https);

    auto popupURL = isSameOrigin == IsSameOrigin::Yes ? "popup.html"_str : makeString("https://localhost:", server.port(), "/popup.html");
    auto popupSource = makeString("<script>onload = () => { w = open('", popupURL, "', 'foo'); };</script>");
    server.addResponse("/main.html"_s, HTTPResponse { WTFMove(sourceHeaders), WTFMove(popupSource) });

    if (doServerSideRedirect == DoServerSideRedirect::Yes) {
        HashMap<String, String> redirectHeaders;
        String redirectionURL = isSameOrigin == IsSameOrigin::Yes ? makeString("https://127.0.0.1:", server.port(), "/popup-after-redirection.html") : makeString("https://localhost:", server.port(), "/popup-after-redirection.html");
        redirectHeaders.add("location"_s, WTFMove(redirectionURL));
        HTTPResponse redirectResponse(301, WTFMove(redirectHeaders));

        server.addResponse("/popup.html"_s, WTFMove(redirectResponse));
        server.addResponse("/popup-after-redirection.html"_s, WTFMove(destinationResponse));
    } else
        server.addResponse("/popup.html"_s, WTFMove(destinationResponse));

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    bool sourceShouldBeCrossOriginIsolated = sourceCOOP == "same-origin"_s && sourceCOEP == "require-corp"_s;
    bool destinationShouldBeCrossOriginIsolated = destinationCOOP == "same-origin"_s && destinationCOEP == "require-corp"_s;
    EXPECT_TRUE(sourceShouldBeCrossOriginIsolated == destinationShouldBeCrossOriginIsolated || expectSwap == ExpectSwap::Yes);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"CrossOriginOpenerPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
        else if ([feature.key isEqualToString:@"CrossOriginEmbedderPolicyEnabled"])
            [[webViewConfiguration preferences] _setEnabled:YES forExperimentalFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);

    __block unsigned numberOfProvisionalLoads = 0;
    navigationDelegate->didStartProvisionalNavigationHandler = ^{
        ++numberOfProvisionalLoads;
    };

    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    failed = false;
    serverRedirected = false;
    numberOfDecidePolicyCalls = 0;
    [webView loadRequest:server.request("/main.html"_s)];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    if (doServerSideRedirect == DoServerSideRedirect::Yes) {
        EXPECT_EQ(3, numberOfDecidePolicyCalls);
        EXPECT_TRUE(serverRedirected);
    } else {
        EXPECT_EQ(2, numberOfDecidePolicyCalls);
        EXPECT_FALSE(serverRedirected);
    }
    EXPECT_EQ(2U, numberOfProvisionalLoads); // One in each view.
    EXPECT_FALSE(failed); // There should be no didFailProvisionalLoad call.

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    if (expectSwap == ExpectSwap::Yes)
        EXPECT_NE(pid1, pid2);
    else
        EXPECT_EQ(pid1, pid2);

    bool finishedRunningScript = false;
    [webView evaluateJavaScript:@"w.closed ? 'true' : 'false'" completionHandler: [&] (id result, NSError *error) {
        NSString *isClosed = (NSString *)result;
        if (expectSwap == ExpectSwap::Yes)
            EXPECT_WK_STREQ(@"true", isClosed);
        else
            EXPECT_WK_STREQ(@"false", isClosed);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    [webView evaluateJavaScript:@"w.name" completionHandler: [&] (id result, NSError *error) {
        NSString *windowName = (NSString *)result;
        if (expectSwap == ExpectSwap::No && isSameOrigin == IsSameOrigin::Yes)
            EXPECT_WK_STREQ(@"foo", windowName);
        else
            EXPECT_WK_STREQ(@"", windowName);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    [webView evaluateJavaScript:@"self.crossOriginIsolated ? 'isolated' : 'not-isolated'" completionHandler: [&] (id result, NSError *error) {
        NSString *crossOriginIsolated = (NSString *)result;
        if (sourceShouldBeCrossOriginIsolated)
            EXPECT_WK_STREQ(@"isolated", crossOriginIsolated);
        else
            EXPECT_WK_STREQ(@"not-isolated", crossOriginIsolated);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    [webView evaluateJavaScript:@"self.SharedArrayBuffer ? 'has-sab' : 'does-not-have-sab'" completionHandler: [&] (id result, NSError *error) {
        NSString *hasSAB = (NSString *)result;
        if (sourceShouldBeCrossOriginIsolated)
            EXPECT_WK_STREQ(@"has-sab", hasSAB);
        else
            EXPECT_WK_STREQ(@"does-not-have-sab", hasSAB);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    // Openee should not have an opener or a name.
    finishedRunningScript = false;
    [createdWebView evaluateJavaScript:@"window.opener ? 'true' : 'false'" completionHandler: [&] (id result, NSError *error) {
        NSString *hasOpener = (NSString *)result;
        if (expectSwap == ExpectSwap::Yes)
            EXPECT_WK_STREQ(@"false", hasOpener);
        else
            EXPECT_WK_STREQ(@"true", hasOpener);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    [createdWebView evaluateJavaScript:@"window.name" completionHandler: [&] (id result, NSError *error) {
        NSString *windowName = (NSString *)result;
        if (expectSwap == ExpectSwap::Yes)
            EXPECT_WK_STREQ(@"", windowName);
        else
            EXPECT_WK_STREQ(@"foo", windowName);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    finishedRunningScript = false;
    [createdWebView evaluateJavaScript:@"document.body.innerText" completionHandler: [&] (id result, NSError *error) {
        NSString *innerText = (NSString *)result;
        EXPECT_WK_STREQ(@"popup", innerText);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    [createdWebView evaluateJavaScript:@"self.crossOriginIsolated ? 'isolated' : 'not-isolated'" completionHandler: [&] (id result, NSError *error) {
        NSString *crossOriginIsolated = (NSString *)result;
        if (destinationShouldBeCrossOriginIsolated)
            EXPECT_WK_STREQ(@"isolated", crossOriginIsolated);
        else
            EXPECT_WK_STREQ(@"not-isolated", crossOriginIsolated);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    [createdWebView evaluateJavaScript:@"self.SharedArrayBuffer ? 'has-sab' : 'does-not-have-sab'" completionHandler: [&] (id result, NSError *error) {
        NSString *hasSAB = (NSString *)result;
        if (destinationShouldBeCrossOriginIsolated)
            EXPECT_WK_STREQ(@"has-sab", hasSAB);
        else
            EXPECT_WK_STREQ(@"does-not-have-sab", hasSAB);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    createdWebView = nullptr;
}

TEST(ProcessSwap, NavigatingSameOriginToCOOPSameOrigin)
{
    runCOOPProcessSwapTest({ }, { }, "same-origin"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginToCOOPSameOrigin2)
{
    runCOOPProcessSwapTest("unsafe-none"_s, { }, "same-origin"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginToCOOPSameOrigin3)
{
    runCOOPProcessSwapTest("unsafe-none"_s, { }, "same-origin"_s, "unsafe-none"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginToCOOPSameOrigin4)
{
    runCOOPProcessSwapTest("unsafe-none"_s, "unsafe-none"_s, "same-origin"_s, "unsafe-none"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginToCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest({ }, { }, "same-origin"_s, "require-corp"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, { }, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOrigin2)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, "unsafe-none"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOrigin3)
{
    runCOOPProcessSwapTest("same-origin"_s, "unsafe-none"_s, "unsafe-none"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, { }, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginAllowPopup)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, { }, { }, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginAllowPopup2)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, { }, "unsafe-none"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginAllowPopup3)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, "unsafe-none"_s, "unsafe-none"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginAllowPopup4)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, "unsafe-none"_s, "unsafe-none"_s, "unsafe-none"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPAndCOEPSameOriginAllowPopup)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, "require-corp"_s, { }, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginToCOOPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, "same-origin"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginToCOOPSameOrigin2)
{
    runCOOPProcessSwapTest("same-origin"_s, "unsafe-none"_s, "same-origin"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginToCOOPSameOrigin3)
{
    runCOOPProcessSwapTest("same-origin"_s, "unsafe-none"_s, "same-origin"_s, "unsafe-none"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPAndCOEPSameOriginToCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, "same-origin"_s, "require-corp"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPAndCOEPSameOriginToCOOPSameOrigin)
{
    // Should swap because the destination is missing COEP.
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, "same-origin"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginToCOOPAndCOEPSameOrigin)
{
    // Should swap because the source is missing COEP.
    runCOOPProcessSwapTest("same-origin"_s, { }, "same-origin"_s, "require-corp"_s, IsSameOrigin::Yes, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPSameOriginToCOOPSameOriginWithRedirect)
{
    // We expect a swap because the redirect doesn't have COOP=same-origin.
    runCOOPProcessSwapTest("same-origin"_s, { }, "same-origin"_s, { }, IsSameOrigin::Yes, DoServerSideRedirect::Yes, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginFromCOOPAndCOEPSameOriginToCOOPAndCOEPSameOriginWithRedirect)
{
    // We expect a swap because the redirect doesn't have COOP=same-origin and COEP=require-corp.
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, "same-origin"_s, "require-corp"_s, IsSameOrigin::Yes, DoServerSideRedirect::Yes, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingSameOriginWithoutCOOPWithRedirect)
{
    runCOOPProcessSwapTest({ }, { }, { }, { }, IsSameOrigin::Yes, DoServerSideRedirect::Yes, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingCrossOriginToCOOPSameOrigin)
{
    runCOOPProcessSwapTest({ }, { }, "same-origin"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginToCOOPSameOrigin2)
{
    runCOOPProcessSwapTest("unsafe-none"_s, { }, "same-origin"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginToCOOPSameOrigin3)
{
    runCOOPProcessSwapTest("unsafe-none"_s, { }, "same-origin"_s, "unsafe-none"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginToCOOPSameOrigin4)
{
    runCOOPProcessSwapTest("unsafe-none"_s, "unsafe-none"_s, "same-origin"_s, "unsafe-none"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginToCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest({ }, { }, "same-origin"_s, "require-corp"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, { }, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOrigin2)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, "unsafe-none"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOrigin3)
{
    runCOOPProcessSwapTest("same-origin"_s, "unsafe-none"_s, "unsafe-none"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOrigin4)
{
    runCOOPProcessSwapTest("same-origin"_s, "unsafe-none"_s, "unsafe-none"_s, "unsafe-none"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, { }, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOriginToCOOPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, "same-origin"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPAndCOEPSameOriginToCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, "same-origin"_s, "require-corp"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPAndCOEPSameOriginToCOOPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, "require-corp"_s, "same-origin"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOriginToCOOPAndCOEPSameOrigin)
{
    runCOOPProcessSwapTest("same-origin"_s, { }, "same-origin"_s, "require-corp"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::Yes);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOriginAllowPopup)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, { }, { }, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOriginAllowPopup2)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, { }, "unsafe-none"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOriginAllowPopup3)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, "unsafe-none"_s, "unsafe-none"_s, { }, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::No);
}

TEST(ProcessSwap, NavigatingCrossOriginFromCOOPSameOriginAllowPopup4)
{
    runCOOPProcessSwapTest("same-origin-allow-popup"_s, "unsafe-none"_s, "unsafe-none"_s, "unsafe-none"_s, IsSameOrigin::No, DoServerSideRedirect::No, ExpectSwap::No);
}

static bool isJITEnabled(WKWebView *webView)
{
    __block bool gotResponse = false;
    __block bool isJITEnabledResult = false;
    [webView _isJITEnabled:^(BOOL isJITEnabled) {
        isJITEnabledResult = isJITEnabled;
        gotResponse = true;
    }];
    TestWebKitAPI::Util::run(&gotResponse);
    EXPECT_NE([webView _webProcessIdentifier], 0);
    return isJITEnabledResult;
}

enum class ShouldBeEnabled : bool { No, Yes };
enum class IsShowingInitialEmptyDocument : bool { No, Yes };
static void checkSettingsControlledByLockdownMode(WKWebView *webView, ShouldBeEnabled shouldBeEnabled, IsShowingInitialEmptyDocument isShowingInitialEmptyDocument = IsShowingInitialEmptyDocument::No)
{
    auto runJSCheck = [&](const String& js) -> bool {
        bool finishedRunningScript = false;
        bool checkResult = false;
        [webView evaluateJavaScript:js completionHandler:[&] (id result, NSError *error) {
            EXPECT_NULL(error);
            checkResult = [result boolValue];
            finishedRunningScript = true;
        }];
        TestWebKitAPI::Util::run(&finishedRunningScript);
        return checkResult;
    };

    EXPECT_EQ(runJSCheck("!!window.WebGL2RenderingContext"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // WebGL2.
    EXPECT_EQ(runJSCheck("!!window.Gamepad"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // Gamepad API.
    EXPECT_EQ(runJSCheck("!!window.RemotePlayback"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // Remote Playback.
    EXPECT_EQ(runJSCheck("!!window.FileSystemHandle"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // File System Access.
    EXPECT_EQ(runJSCheck("!!window.HTMLModelElement"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // AR (Model)
    EXPECT_EQ(runJSCheck("!!window.PictureInPictureEvent"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // Picture in Picture API.
    EXPECT_EQ(runJSCheck("!!window.SpeechRecognitionEvent"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // Speech recognition.
#if ENABLE(SPEECH_SYNTHESIS)
    EXPECT_EQ(runJSCheck("!!window.SpeechSynthesisEvent"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // Speech synthesis.
#endif
#if ENABLE(NOTIFICATIONS)
    EXPECT_EQ(runJSCheck("!!window.Notification"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // Notification API.
#endif
    EXPECT_EQ(runJSCheck("!!window.WebXRSystem"_s), false); // WebXR (currently always disabled).
    EXPECT_EQ(runJSCheck("!!window.AudioContext"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // WebAudio.
    EXPECT_EQ(runJSCheck("!!window.Cache"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // Cache API.
    EXPECT_EQ(runJSCheck("!!window.CacheStorage"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // Cache API.
    EXPECT_EQ(runJSCheck("!!window.FileReader"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // FileReader API.
    EXPECT_EQ(runJSCheck("!!window.FileSystemFileHandle"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // File System Access API.
    EXPECT_EQ(runJSCheck("!!window.RTCPeerConnection"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // WebRTC Peer Connection.
    EXPECT_EQ(runJSCheck("!!window.RTCRtpScriptTransform"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // WebRTC Script Transform
    EXPECT_EQ(runJSCheck("!!window.indexedDB"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // IndexedDB API
    EXPECT_EQ(runJSCheck("!!navigator.getUserMedia"_s), false); // Legacy GetUserMedia (currently always disabled).
    EXPECT_EQ(runJSCheck("!!window.MathMLElement"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // MathML.
    EXPECT_EQ(runJSCheck("!!window.MathMLMathElement"_s), shouldBeEnabled == ShouldBeEnabled::Yes); // MathML.
    EXPECT_EQ(runJSCheck("!!window.PushManager"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // Push API.
    EXPECT_EQ(runJSCheck("!!window.PushSubscription"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // Push API.
    EXPECT_EQ(runJSCheck("!!window.PushSubscriptionOptions"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // Push API.
    EXPECT_EQ(runJSCheck("!!window.LockManager"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // WebLockManager API.
    String mathMLCheck = makeString("document.createElementNS('http://www.w3.org/1998/Math/MathML','mspace').__proto__ == ", shouldBeEnabled == ShouldBeEnabled::Yes ? "MathMLElement" : "Element", ".prototype");
    EXPECT_EQ(runJSCheck(mathMLCheck), true); // MathML.
    EXPECT_EQ(runJSCheck("!!window.ServiceWorker"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes); // Service Workers API.
    String embedElementCheck = makeString("document.createElement('embed').__proto__ == ", shouldBeEnabled == ShouldBeEnabled::Yes ? "HTMLEmbedElement" : "HTMLUnknownElement", ".prototype");
    EXPECT_EQ(runJSCheck(embedElementCheck), true); // Embed Element.

    EXPECT_EQ(runJSCheck("CSS.supports('contain-intrinsic-size: 10rem')"_s), shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("CSS.supports('content-visibility: visible')"_s), shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("CSS.supports('overflow-anchor:none')"_s), shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("CSS.supports('text-justify: auto')"_s), shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("!!navigator.contacts"_s), isShowingInitialEmptyDocument != IsShowingInitialEmptyDocument::Yes && shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("!!window.CSSCounterStyleRule"_s), shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("!!window.DeprecationReportBody"_s), shouldBeEnabled == ShouldBeEnabled::Yes);
    EXPECT_EQ(runJSCheck("!!window.Highlight"_s), shouldBeEnabled == ShouldBeEnabled::Yes);

    // Confirm unstable settings are always off in Lockdown Mode.
    EXPECT_EQ(runJSCheck("!!navigator.requestCookieConsent"_s), false);
    EXPECT_EQ(runJSCheck("!!window.requestIdleCallback"_s), false);
}

@interface LockdownMessageHandler : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@end

@implementation LockdownMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

@end

static void configureLockdownWKWebViewConfiguration(WKWebViewConfiguration *config)
{
    [config.preferences _setMediaDevicesEnabled:YES];
    config.preferences._mediaCaptureRequiresSecureConnection = NO;
    [config.preferences _setNotificationsEnabled:YES];

    // Turn on all experimental features to confirm they are properly turned off in Lockdown Mode.
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures])
        [config.preferences _setEnabled:YES forFeature:feature];
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"WebRTCEncodedTransformEnabled"])
            [config.preferences _setEnabled:YES forInternalDebugFeature:feature];
    }
}

TEST(ProcessSwap, NavigatingToLockdownMode)
{
    auto messageHandler = adoptNS([[LockdownMessageHandler alloc] init]);

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    configureLockdownWKWebViewConfiguration(webViewConfiguration.get());
    [webViewConfiguration.get().userContentController addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_TRUE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::Yes, IsShowingInitialEmptyDocument::Yes);

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    EXPECT_TRUE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::Yes);

    finishedNavigation = false;
    receivedScriptMessage = false;
    url = [[NSBundle mainBundle] URLForResource:@"LockdownModePDF" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_FALSE(receivedScriptMessage);
    EXPECT_TRUE(scriptMessages.isEmpty());

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_FALSE(preferences.lockdownModeEnabled);
        preferences.lockdownModeEnabled = YES;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    finishedNavigation = false;
    url = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    // We should have process-swap for transitioning to lockdown mode.
    EXPECT_NE(pid1, [webView _webProcessIdentifier]);
    EXPECT_FALSE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::No);

    finishedNavigation = false;
    receivedScriptMessage = false;
    url = [[NSBundle mainBundle] URLForResource:@"LockdownModePDF" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_TRUE(receivedScriptMessage);
    EXPECT_WK_STREQ("Error loading PDF", getNextMessage().body);
}

TEST(ProcessSwap, LockdownModeSystemSettingChange)
{
    auto kvo = adoptNS([LockdownModeKVO new]);

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    [webViewConfiguration.get().defaultWebpagePreferences addObserver:kvo.get() forKeyPath:@"lockdownModeEnabled" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld context:nil];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_TRUE(isJITEnabled(webView.get()));

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    EXPECT_FALSE(didChangeLockdownMode);

    finishedNavigation = false;
    // Now change the global setting.
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    TestWebKitAPI::Util::run(&didChangeLockdownMode);
    EXPECT_FALSE(lockdownModeBeforeChange);
    EXPECT_TRUE(lockdownModeAfterChange);
    EXPECT_TRUE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);

    // This should cause the WebView to reload with a WebProcess in lockdown mode.
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_FALSE(isJITEnabled(webView.get()));

    pid_t pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    didChangeLockdownMode = false;
    finishedNavigation = false;
    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];

    TestWebKitAPI::Util::run(&didChangeLockdownMode);
    EXPECT_TRUE(lockdownModeBeforeChange);
    EXPECT_FALSE(lockdownModeAfterChange);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);

    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    pid_t pid3 = [webView _webProcessIdentifier];
    EXPECT_NE(pid2, pid3);
}

#if PLATFORM(IOS)

TEST(ProcessSwap, CannotDisableLockdownModeWithoutBrowserEntitlement)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    configureLockdownWKWebViewConfiguration(webViewConfiguration.get());
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_FALSE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::No, IsShowingInitialEmptyDocument::Yes);
    pid_t pid1 = [webView _webProcessIdentifier];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_TRUE(preferences.lockdownModeEnabled);
        bool didThrowWhenTryingToDisableLockdownMode = false;
        // TestWebKitAPI doesn't have the web browser entitlement and thus shouldn't be able to disable lockdown mode.
        @try {
            preferences.lockdownModeEnabled = NO;
        } @catch (NSException *exception) {
            didThrowWhenTryingToDisableLockdownMode = true;
        }
        EXPECT_TRUE(didThrowWhenTryingToDisableLockdownMode);
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(pid1, [webView _webProcessIdentifier]); // Shouldn't have process-swapped since we're staying in lockdown mode.
    EXPECT_FALSE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::No);
}

#else

TEST(ProcessSwap, LockdownModeEnabledByDefaultThenOptOut)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    configureLockdownWKWebViewConfiguration(webViewConfiguration.get());
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_FALSE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::No, IsShowingInitialEmptyDocument::Yes);
    pid_t pid1 = [webView _webProcessIdentifier];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_TRUE(preferences.lockdownModeEnabled);
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_FALSE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::No);
    EXPECT_EQ(pid1, [webView _webProcessIdentifier]);

    finishedNavigation = false;
    url = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(pid1, [webView _webProcessIdentifier]); // Shouldn't have process-swapped since we're staying in lockdown mode.
    EXPECT_FALSE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::No);

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_TRUE(preferences.lockdownModeEnabled);
        preferences.lockdownModeEnabled = NO; // Opt out of lockdown mode for this load.
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    finishedNavigation = false;
    url = [[NSBundle mainBundle] URLForResource:@"simple3" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    // We should have process-swapped to get out of lockdown mode.
    EXPECT_NE(pid1, [webView _webProcessIdentifier]);
    EXPECT_TRUE(isJITEnabled(webView.get()));
    checkSettingsControlledByLockdownMode(webView.get(), ShouldBeEnabled::Yes);

    // lockdown mode should be disabled in new WebViews since it is not enabled globally.
    auto webViewConfiguration2 = adoptNS([WKWebViewConfiguration new]);
    configureLockdownWKWebViewConfiguration(webViewConfiguration2.get());
    EXPECT_TRUE(webViewConfiguration2.get().defaultWebpagePreferences.lockdownModeEnabled == NO);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration2.get()]);
    [webView2 setNavigationDelegate:delegate.get()];
    EXPECT_TRUE(isJITEnabled(webView2.get()));
    checkSettingsControlledByLockdownMode(webView2.get(), ShouldBeEnabled::Yes, IsShowingInitialEmptyDocument::Yes);
    pid_t pid2 = [webView2 _webProcessIdentifier];

    delegate.get().decidePolicyForNavigationActionWithPreferences = nil;

    finishedNavigation = false;
    url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView2 loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_TRUE(isJITEnabled(webView2.get()));
    checkSettingsControlledByLockdownMode(webView2.get(), ShouldBeEnabled::Yes);
    EXPECT_EQ(pid2, [webView2 _webProcessIdentifier]);
}

TEST(ProcessSwap, LockdownModeSystemSettingChangeDoesNotReloadViewsWhenModeIsSetExplicitly1)
{
    // Captive portal mode is disabled globally and explicitly opted out via defaultWebpagePreferences.

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = NO; // Explicitly opt out via default WKWebpagePreferences.

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_TRUE(isJITEnabled(webView.get()));

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    finishedNavigation = false;
    // Now change the global setting.
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    // This should not reload the web view since the view has explicitly opted out of lockdown mode.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(finishedNavigation);

    pid_t pid2 = [webView _webProcessIdentifier];
    EXPECT_EQ(pid1, pid2);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
}

TEST(ProcessSwap, LockdownModeSystemSettingChangeDoesNotReloadViewsWhenModeIsSetExplicitly2)
{
    // Captive portal mode is enabled globally but explicitly opted out via defaultWebpagePreferences.
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = NO; // Explicitly opt out via default WKWebpagePreferences.

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_TRUE(isJITEnabled(webView.get()));

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    finishedNavigation = false;
    // Now change the global setting.
    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];

    // This should not reload the web view since the view has explicitly opted out of lockdown mode.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(finishedNavigation);

    pid_t pid2 = [webView _webProcessIdentifier];
    EXPECT_EQ(pid1, pid2);

    EXPECT_TRUE(isJITEnabled(webView.get()));
}

TEST(ProcessSwap, LockdownModeSystemSettingChangeDoesNotReloadViewsWhenModeIsSetExplicitly3)
{
    // Captive portal mode is disabled globally and explicitly opted out for the load.

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_TRUE(isJITEnabled(webView.get()));

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_FALSE(preferences.lockdownModeEnabled);
        preferences.lockdownModeEnabled = NO; // Explicitly Opt out of lockdown mode for this load.
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    finishedNavigation = false;
    // Now change the global setting.
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    // This should not reload the web view since the view has explicitly opted out of lockdown mode.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(finishedNavigation);

    pid_t pid2 = [webView _webProcessIdentifier];
    EXPECT_EQ(pid1, pid2);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
}

TEST(ProcessSwap, LockdownModeSystemSettingChangeDoesNotReloadViewsWhenModeIsSetExplicitly4)
{
    // Captive portal mode is enabled globally but explicitly opted out for the load.
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    EXPECT_FALSE(isJITEnabled(webView.get()));

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_TRUE(preferences.lockdownModeEnabled);
        preferences.lockdownModeEnabled = NO; // Explicitly Opt out of lockdown mode for this load.
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    EXPECT_TRUE(isJITEnabled(webView.get()));

    finishedNavigation = false;
    // Now change the global setting.
    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];

    // This should not reload the web view since the view has explicitly opted out of lockdown mode.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(finishedNavigation);

    pid_t pid2 = [webView _webProcessIdentifier];
    EXPECT_EQ(pid1, pid2);

    EXPECT_TRUE(isJITEnabled(webView.get()));

}

#endif

#if PLATFORM(IOS_FAMILY)
TEST(ProcessSwap, ContentModeInCaseOfCoopProcessSwap)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/source.html"_s, { "foo"_s } },
        { "/destination.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "bar"_s } },
    }, HTTPServer::Protocol::Https);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [webpagePreferences setPreferredContentMode:WKContentModeDesktop];
    [webViewConfiguration setDefaultWebpagePreferences:webpagePreferences.get()];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    done = false;
    [webView loadRequest:server.request("/source.html"_s)];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        done = true;

        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE([userAgent containsString:@"Macintosh; Intel Mac"]);
    }];
    Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    [webView loadRequest:server.request("/destination.html"_s)];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        done = true;

        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE([userAgent containsString:@"Macintosh; Intel Mac"]);
    }];
    Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    [webView goBack]; // Back to source.html.
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        done = true;

        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE([userAgent containsString:@"Macintosh; Intel Mac"]);
    }];
    Util::run(&done);
    done = false;
}

TEST(ProcessSwap, ContentModeInCaseOfPSONThenCoopProcessSwap)
{
    using namespace TestWebKitAPI;

    HTTPServer server1({
        { "/source.html"_s, { "foo"_s } },
    }, HTTPServer::Protocol::Https);

    HTTPServer server2({
        { "/destination.html"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "bar"_s } },
    }, HTTPServer::Protocol::Http);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [webpagePreferences setPreferredContentMode:WKContentModeDesktop];
    [webViewConfiguration setDefaultWebpagePreferences:webpagePreferences.get()];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    done = false;
    [webView loadRequest:server1.request("/source.html"_s)];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        done = true;

        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE([userAgent containsString:@"Macintosh; Intel Mac"]);
    }];
    Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    [webView loadRequest:server2.request("/destination.html"_s)];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        done = true;

        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE([userAgent containsString:@"Macintosh; Intel Mac"]);
    }];
    Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, pid2);

    [webView goBack]; // Back to source.html.
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        done = true;

        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE([userAgent containsString:@"Macintosh; Intel Mac"]);
    }];
    Util::run(&done);
    done = false;
}
#endif // PLATFORM(IOS_FAMILY)

// The WebProcess cache cannot be enabled on devices with too little RAM so we need to disable
// tests relying on it on iOS. The WebProcess cache is disabled by default on iOS anyway.
#if !PLATFORM(IOS_FAMILY)
TEST(WebProcessCache, ReusedCrashedCachedWebProcess)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);
    [webView1 setNavigationDelegate:delegate.get()];
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    done = false;
    [webView1 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    done = false;
    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto cachedProcessPID = [webView1 _webProcessIdentifier];
    EXPECT_NE([webView1 _webProcessIdentifier], [webView2 _webProcessIdentifier]);

    [webView1 _close];
    webView1 = nil;

    // There should now be a process for apple.com in the process cache.
    while ([processPool _processCacheSize] != 1)
        TestWebKitAPI::Util::runFor(0.1_s);

    crashCount = 0;
    done = false;
    kill([webView2 _webProcessIdentifier], 9);
    kill(cachedProcessPID, 9);

    // Intentional hang to delay processing of crash notifications.
    sleep(1);

    // View should reload due to crash.
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(crashCount, 1u);
}
#endif

TEST(WebProcessCache, ReusedCrashedBackForwardSuspendedWebProcess)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 800) configuration:webViewConfiguration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main.html"]];

    done = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto firstWebProcessPID = [webView _webProcessIdentifier];

    // We force a proces-swap via client API.
    delegate->decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowInNewProcess);
    };

    done = false;
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://www.webkit.org/main2.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto secondWebProcessPID = [webView _webProcessIdentifier];
    EXPECT_NE(firstWebProcessPID, secondWebProcessPID);

    crashCount = 0;
    done = false;
    kill(secondWebProcessPID, 9);
    kill(firstWebProcessPID, 9);

    // Intentional hang to delay processing of crash notifications.
    sleep(1);

    // View should reload due to crash.
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(crashCount, 1u);
}
