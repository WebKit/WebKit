/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "FrameTreeChecks.h"
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/DragAndDropSimulator.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestDownloadDelegate.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestPDFDocument.h"
#import "Helpers/cocoa/TestResourceLoadDelegate.h"
#import "Helpers/cocoa/TestScriptMessageHandler.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/UserMediaCaptureUIDelegate.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "InstanceMethodSwizzler.h"
#import "TestURLSchemeHandler.h"
#import "WKWebViewFindStringFindDelegate.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/SQLiteDatabase.h>
#import <WebCore/SQLiteStatement.h>
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKMediaKeySystemPermissionCallback.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKNavigationPrivateForTesting.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKAppHighlight.h>
#import <WebKit/_WKAppHighlightDelegate.h>
#import <WebKit/_WKAttachment.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKJSHandle.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKResourceLoadInfo.h>
#import <WebKit/_WKSessionState.h>
#import <WebKit/_WKTextManipulationConfiguration.h>
#import <WebKit/_WKTextManipulationDelegate.h>
#import <WebKit/_WKTextManipulationItem.h>
#import <WebKit/_WKTextManipulationToken.h>
#import <WebKit/_WKUserInitiatedAction.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#if PLATFORM(MAC)
#import "Helpers/mac/AppKitSPI.h"

@interface NSApplication ()
- (void)_setKeyWindow:(NSWindow *)newKeyWindow;
@end
#endif

#if ENABLE(IMAGE_ANALYSIS)
#import "Helpers/cocoa/ImageAnalysisTestingUtilities.h"
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import <UIFoundation/NSAdaptiveImageGlyph.h>
#endif

#if ENABLE(IMAGE_ANALYSIS)
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#endif

@interface WKWebView ()
- (void)paste:(id)sender;
- (WKPageRef)_pageForTesting;
@end

@interface SiteIsolationTextManipulationDelegate : NSObject <_WKTextManipulationDelegate>
- (void)_webView:(WKWebView *)webView didFindTextManipulationItems:(NSArray<_WKTextManipulationItem *> *)items;
@property (nonatomic, readonly, copy) NSArray<_WKTextManipulationItem *> *items;
@end

@implementation SiteIsolationTextManipulationDelegate {
    RetainPtr<NSMutableArray> _items;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _items = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView didFindTextManipulationItems:(NSArray<_WKTextManipulationItem *> *)items
{
    [_items addObjectsFromArray:items];
}

- (NSArray<_WKTextManipulationItem *> *)items
{
    return _items.get();
}
@end

@interface NavigationDelegateAllowingAllTLS : NSObject<WKNavigationDelegate>
- (void)waitForDidFinishNavigation;
@end

@implementation NavigationDelegateAllowingAllTLS {
    bool _finishedNavigation;
}
- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}
- (void)waitForDidFinishNavigation
{
    _finishedNavigation = false;
    TestWebKitAPI::Util::run(&_finishedNavigation);
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _finishedNavigation = true;
}
@end

@interface TestObserver : NSObject

@property (nonatomic, copy) void (^observeValueForKeyPath)(NSString *, id);

@end

@implementation TestObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    _observeValueForKeyPath(keyPath, object);
}

@end

#if ENABLE(IMAGE_ANALYSIS)

static unsigned gDidProcessRequestCount = 0;

static void processRequestWithResults(id, SEL, VKImageAnalyzerRequest *, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    gDidProcessRequestCount++;
    completion(TestWebKitAPI::createImageAnalysisWithSimpleFixedResults().get(), nil);
}

static VKImageAnalyzerRequest *makeFakeRequest(id, SEL, CGImageRef image, VKImageOrientation orientation, VKAnalysisTypes requestTypes)
{
    return TestWebKitAPI::createRequest(image, orientation, requestTypes).leakRef();
}

template <typename FunctionType>
std::pair<std::unique_ptr<InstanceMethodSwizzler>, std::unique_ptr<InstanceMethodSwizzler>> makeImageAnalysisRequestSwizzler(FunctionType function)
{
    return std::pair {
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerClassSingleton(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(function)),
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerRequestClassSingleton(), @selector(initWithCGImage:orientation:requestType:), reinterpret_cast<IMP>(makeFakeRequest))
    };
}

@interface TestWKWebViewImageAnalysisTests : TestWKWebView
- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests;
@end

@implementation TestWKWebViewImageAnalysisTests

- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests
{
    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return gDidProcessRequestCount == numberOfRequests;
    }, 3, @"Timed out waiting for %u image analysis requests to complete, got %u", numberOfRequests, gDidProcessRequestCount);

    [self waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, numberOfRequests);
}

@end

#endif // ENABLE(IMAGE_ANALYSIS)

@interface TrackingURLSchemeHandler : NSObject <WKURLSchemeHandler>
@property (nonatomic, copy) void (^startURLSchemeTaskHandler)(TrackingURLSchemeHandler *, id<WKURLSchemeTask>);
- (BOOL)deliveredSameTaskTwice;
- (BOOL)raisedException;
- (NSUInteger)stopCountForURLPathPrefix:(NSString *)prefix;
- (void)park:(id<WKURLSchemeTask>)task;
- (NSUInteger)parkedCountForURLPathPrefix:(NSString *)prefix;
- (void)respond:(id<WKURLSchemeTask>)task text:(const char *)text mimeType:(NSString *)mimeType;
- (void)respondToParkedTasksWithURLPathPrefix:(NSString *)prefix text:(const char *)text;
@end

@implementation TrackingURLSchemeHandler {
    BlockPtr<void(TrackingURLSchemeHandler *, id<WKURLSchemeTask>)> _startURLSchemeTaskHandler;
    RetainPtr<NSMutableArray> _liveTasks;
    RetainPtr<NSMutableArray> _parkedTasks;
    RetainPtr<NSMutableArray<NSString *>> _stoppedURLStrings;
    BOOL _deliveredSameTaskTwice;
    BOOL _raisedException;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _liveTasks = adoptNS([NSMutableArray new]);
    _parkedTasks = adoptNS([NSMutableArray new]);
    _stoppedURLStrings = adoptNS([NSMutableArray new]);
    return self;
}

- (void)setStartURLSchemeTaskHandler:(void (^)(TrackingURLSchemeHandler *, id<WKURLSchemeTask>))block
{
    _startURLSchemeTaskHandler = makeBlockPtr(block);
}

- (void (^)(TrackingURLSchemeHandler *, id<WKURLSchemeTask>))startURLSchemeTaskHandler
{
    return _startURLSchemeTaskHandler.get();
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)task
{
    if ([_liveTasks indexOfObjectIdenticalTo:task] != NSNotFound) {
        _deliveredSameTaskTwice = YES;
        return;
    }
    [_liveTasks addObject:task];

    if (_startURLSchemeTaskHandler)
        _startURLSchemeTaskHandler(self, task);
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id<WKURLSchemeTask>)task
{
    [_stoppedURLStrings addObject:task.request.URL.absoluteString];
    [_liveTasks removeObjectIdenticalTo:task];
    [_parkedTasks removeObjectIdenticalTo:task];
}

- (BOOL)deliveredSameTaskTwice
{
    return _deliveredSameTaskTwice;
}

- (BOOL)raisedException
{
    return _raisedException;
}

- (NSUInteger)stopCountForURLPathPrefix:(NSString *)prefix
{
    NSUInteger count = 0;
    for (NSString *urlString in _stoppedURLStrings.get()) {
        if ([[NSURL URLWithString:urlString].path hasPrefix:prefix])
            ++count;
    }
    return count;
}

- (void)park:(id<WKURLSchemeTask>)task
{
    [_parkedTasks addObject:task];
}

- (NSUInteger)parkedCountForURLPathPrefix:(NSString *)prefix
{
    NSUInteger count = 0;
    for (id<WKURLSchemeTask> task in _parkedTasks.get()) {
        if ([task.request.URL.path hasPrefix:prefix])
            ++count;
    }
    return count;
}

- (void)respond:(id<WKURLSchemeTask>)task text:(const char *)text mimeType:(NSString *)mimeType
{
    RetainPtr data = [NSData dataWithBytes:text length:strlen(text)];
    RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:[data length] textEncodingName:nil]);
    @try {
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data.get()];
        [task didFinish];
    } @catch (NSException *exception) {
        _raisedException = YES;
    }
    [_liveTasks removeObjectIdenticalTo:task];
    [_parkedTasks removeObjectIdenticalTo:task];
}

- (void)respondToParkedTasksWithURLPathPrefix:(NSString *)prefix text:(const char *)text
{
    RetainPtr<NSArray> tasks = [NSArray arrayWithArray:_parkedTasks.get()];
    for (id<WKURLSchemeTask> task in tasks.get()) {
        if ([task.request.URL.path hasPrefix:prefix])
            [self respond:task text:text mimeType:@"text/plain"];
    }
}

@end

// Unlike -_test_waitForAlert, this waits with a bound, so a wedged load fails the test instead of hanging it.
@interface BoundedAlertRecorder : NSObject <WKUIDelegate>
- (NSString *)waitForAlert;
@end

@implementation BoundedAlertRecorder {
    RetainPtr<NSString> _message;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    _message = message;
    completionHandler();
}

- (NSString *)waitForAlert
{
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return !!_message;
    }));
    return _message.get();
}

@end

@interface SiteIsolationFontAttributesListener : NSObject <WKUIDelegatePrivate>
- (NSDictionary<NSString *, id> *)lastFontAttributes;
@end

@implementation SiteIsolationFontAttributesListener {
    RetainPtr<NSDictionary> _lastFontAttributes;
}

- (void)_webView:(WKWebView *)webView didChangeFontAttributes:(NSDictionary<NSString *, id> *)fontAttributes
{
    _lastFontAttributes = fontAttributes;
}

- (NSDictionary<NSString *, id> *)lastFontAttributes
{
    return _lastFontAttributes.get();
}

@end

#if ENABLE(ATTACHMENT_ELEMENT)
@interface SiteIsolationAttachmentObserver : NSObject <WKUIDelegatePrivate>
- (NSArray<_WKAttachment *> *)insertedAttachments;
@end

@implementation SiteIsolationAttachmentObserver {
    RetainPtr<NSMutableArray<_WKAttachment *>> _insertedAttachments;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _insertedAttachments = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_insertedAttachments addObject:attachment];
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return _insertedAttachments.get();
}

@end
#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(APP_HIGHLIGHTS)
@interface SiteIsolationAppHighlightDelegate : NSObject <_WKAppHighlightDelegate>
- (NSArray<_WKAppHighlight *> *)storedHighlights;
@end

@implementation SiteIsolationAppHighlightDelegate {
    RetainPtr<NSMutableArray<_WKAppHighlight *>> _storedHighlights;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _storedHighlights = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView storeAppHighlight:(_WKAppHighlight *)highlight inNewGroup:(BOOL)inNewGroup requestOriginatedInApp:(BOOL)requestOriginatedInApp
{
    [_storedHighlights addObject:highlight];
}

- (NSArray<_WKAppHighlight *> *)storedHighlights
{
    return _storedHighlights.get();
}

@end
#endif // ENABLE(APP_HIGHLIGHTS)

namespace TestWebKitAPI {

// WKPreferences._usesPageCache is macOS-only, so disable BFCache via the
// cross-platform _WKProcessPoolConfiguration.pageCacheEnabled property
// instead (sets the process pool's BFCache capacity to 0).
static RetainPtr<WKProcessPool> processPoolWithBackForwardCacheDisabled()
{
    RetainPtr poolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    poolConfiguration.get().pageCacheEnabled = NO;
    return adoptNS([[WKProcessPool alloc] _initWithConfiguration:poolConfiguration.get()]);
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> viewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect = CGRectZero)
{
    return siteIsolatedViewAndDelegate(configuration, rect, false);
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> viewAndDelegate(const HTTPServer& server, CGRect rect = CGRectZero)
{
    return viewAndDelegate(server.httpsProxyConfiguration(), rect);
}

static bool frameTreesMatch(_WKFrameTreeNode *actualRoot, ExpectedFrameTree&& expectedRoot)
{
    WKFrameInfo *info = actualRoot.info;
    if (info._isLocalFrame != std::holds_alternative<String>(expectedRoot.remoteOrOrigin))
        return false;

    if (auto* expectedOrigin = std::get_if<String>(&expectedRoot.remoteOrOrigin)) {
        WKSecurityOrigin *origin = info.securityOrigin;
        auto actualOrigin = makeString(String(origin.protocol), "://"_s, String(origin.host), origin.port ? makeString(':', origin.port) : String());
        if (actualOrigin != *expectedOrigin)
            return false;
    }

    if (actualRoot.childFrames.count != expectedRoot.children.size())
        return false;
    for (_WKFrameTreeNode *actualChild in actualRoot.childFrames) {
        auto index = expectedRoot.children.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(actualChild, ExpectedFrameTree { expectedFrameTree });
        });
        if (index == WTF::notFound)
            return false;
        expectedRoot.children.removeAt(index);
    }
    return expectedRoot.children.isEmpty();
}

static bool frameTreesMatch(NSSet<_WKFrameTreeNode *> *actualFrameTrees, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    if (actualFrameTrees.count != expectedFrameTrees.size())
        return false;

    for (_WKFrameTreeNode *root in actualFrameTrees) {
        auto index = expectedFrameTrees.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(root, ExpectedFrameTree { expectedFrameTree });
        });
        if (index == WTF::notFound)
            return false;
        expectedFrameTrees.removeAt(index);
    }
    return expectedFrameTrees.isEmpty();
}

static void checkTopDocumentURLsInBackForwardCacheAtIndex(WKWebView *webView, NSInteger relativeIndex, NSUInteger expectedProcessCount, NSString *expectedTopDocumentURL)
{
    __block bool done = false;
    __block RetainPtr<NSArray<NSURL *>> result;
    [webView _topDocumentURLsInBackForwardCacheAtIndexForTesting:relativeIndex completionHandler:^(NSArray<NSURL *> *topDocumentURLs) {
        result = topDocumentURLs;
        done = true;
    }];
    Util::run(&done);
    EXPECT_EQ([result count], expectedProcessCount);
    for (NSURL *url in result.get())
        EXPECT_WK_STREQ(expectedTopDocumentURL, url.absoluteString);
}

static void checkProcessesTopDocumentURL(NSSet<_WKFrameTreeNode *> *trees, NSString *mainFrameTopDocumentURL, NSString *subframeTopDocumentURL)
{
    for (_WKFrameTreeNode *root in trees) {
        if (root.info._isLocalFrame)
            EXPECT_WK_STREQ(mainFrameTopDocumentURL, root._topDocumentURLForTesting.absoluteString);
        else
            EXPECT_WK_STREQ(subframeTopDocumentURL, root._topDocumentURLForTesting.absoluteString);
    }
}

static ASCIICString indentation(size_t count)
{
    std::span<char> characters;
    auto result = ASCIICString::newUninitialized(count, characters);
    std::ranges::fill(characters, ' ');
    return result;
}

static void printTree(_WKFrameTreeNode *n, size_t indent = 0)
{
    if (n.info._isLocalFrame)
        SAFE_WTFLOGALWAYS("%s%@://%@ (pid %d)", indentation(indent), n.info.securityOrigin.protocol, n.info.securityOrigin.host, n.info._processIdentifier);
    else
        SAFE_WTFLOGALWAYS("%s(remote) (pid %d)", indentation(indent), n.info._processIdentifier);
    for (_WKFrameTreeNode *c in n.childFrames)
        printTree(c, indent + 1);
}

static void printTree(const ExpectedFrameTree& n, size_t indent = 0)
{
    if (auto* s = std::get_if<String>(&n.remoteOrOrigin))
        SAFE_WTFLOGALWAYS("%s%s", indentation(indent), s->utf8());
    else
        SAFE_WTFLOGALWAYS("%s(remote)", indentation(indent));
    for (const auto& c : n.children)
        printTree(c, indent + 1);
}

static void checkFrameTreesInProcesses(NSSet<_WKFrameTreeNode *> *actualTrees, const Vector<ExpectedFrameTree>& expectedFrameTrees)
{
    bool result = frameTreesMatch(actualTrees, Vector<ExpectedFrameTree> { expectedFrameTrees });
    if (!result) {
        WTFLogAlways("ACTUAL");
        for (_WKFrameTreeNode *n in actualTrees)
            printTree(n);
        WTFLogAlways("EXPECTED");
        for (const auto& e : expectedFrameTrees)
            printTree(e);
        WTFLogAlways("END");
    }
    EXPECT_TRUE(result);
}

void checkFrameTreesInProcesses(WKWebView *webView, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    checkFrameTreesInProcesses(frameTrees(webView).get(), WTF::move(expectedFrameTrees));
}

static unsigned countWebPages(const RetainPtr<WKWebView>& webView)
{
    __block bool done { false };
    __block unsigned result { 0 };
    [webView.get().configuration.processPool _countWebPagesInAllProcessesForTesting:^(unsigned count) {
        result = count;
        done = true;
    }];
    Util::run(&done);
    return result;
}

static void startCountingAnimationFrames(TestWKWebView *webView, WKFrameInfo *frame)
{
    [webView objectByEvaluatingJavaScript:@"window.__rafCount = 0; (function tick() { window.__rafCount++; requestAnimationFrame(tick); })();" inFrame:frame];
}

static long long animationFrameCount(TestWKWebView *webView, WKFrameInfo *frame)
{
    return [[webView objectByEvaluatingJavaScript:@"window.__rafCount" inFrame:frame] longLongValue];
}

static void expectAnimationFrameCountToIncrease(TestWKWebView *webView, WKFrameInfo *frame)
{
    auto initialCount = animationFrameCount(webView, frame);
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_GT(animationFrameCount(webView, frame), initialCount);
}

TEST(SiteIsolation, LoadingCallbacksAndPostMessage)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('parent frame received ' + event.data)"
    "    }, false);"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        parent.window.postMessage(event.data + 'pong', { 'targetOrigin' : '*' });"
    "    }, false)"
    "</script>"_s;

    bool finishedLoading { false };
    size_t framesCommitted { 0 };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse(exampleHTML).serialize());
                continue;
            }
            if (path == "/webkit"_s) {
                size_t contentLength = 2000000 + webkitHTML.length();
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Length: "_s, contentLength, "\r\n\r\n"_s));

                co_await connection.awaitableSend(webkitHTML);
                co_await connection.awaitableSend(Vector<uint8_t>(FillWith { }, 1000000, ' '));

                while (framesCommitted < 2)
                    Util::spinRunLoop();
                Util::runFor(Seconds(0.5));
                EXPECT_EQ(framesCommitted, 2u);

                EXPECT_FALSE(finishedLoading);
                co_await connection.awaitableSend(Vector<uint8_t>(FillWith { }, 1000000, ' '));
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didCommitLoadWithRequestInFrame = makeBlockPtr([&](WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo) {
        NSString *url = frameInfo.request.URL.absoluteString;
        switch (++framesCommitted) {
        case 1:
            EXPECT_WK_STREQ(url, "https://example.com/example");
            EXPECT_TRUE(frameInfo.isMainFrame);
            break;
        case 2:
            EXPECT_WK_STREQ(url, "https://webkit.org/webkit");
            EXPECT_FALSE(frameInfo.isMainFrame);
            break;
        default:
            EXPECT_FALSE(true);
            break;
        }
    }).get();
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "parent frame received pingpong");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, CancelNavigationResponseCleansUpProvisionalFrame)
{
    HTTPServer server({
        { "/main"_s, { "hi"_s } },
        { "/iframe1"_s, { "<script>alert('loaded iframe1')</script>"_s } },
        { "/iframe2"_s, { "shouldn't actually load"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:
        @"var iframe1 = document.createElement('iframe');"
        "document.body.appendChild(iframe1);"
        "iframe1.src = 'https://apple.com/iframe1';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded iframe1");

    __block uint32_t failureCount = 0;
    navigationDelegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView* webView, WKFrameInfo* frame, NSError* error) {
        EXPECT_WK_STREQ(error.domain, WebKitErrorDomain);
        EXPECT_EQ(error.code, WebKitErrorFrameLoadInterruptedByPolicyChange);
        failureCount++;
    };

    navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse* navigationResponse, void (^completionHandler)(WKNavigationResponsePolicy)) {
        completionHandler(WKNavigationResponsePolicyCancel);
    };

    [webView evaluateJavaScript:
        @"var iframe2 = document.createElement('iframe');"
        "document.body.appendChild(iframe2);"
        "iframe2.src = 'https://apple.com/iframe2';"
    completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 1; }
    ));

    // Make sure second navigation doesn't assert in WebFrame::createProvisionalFrame()
    [webView evaluateJavaScript:@"iframe2.src = 'https://apple.com/iframe2';" completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 2; }
    ));
}

TEST(SiteIsolation, CancelNavigationActionCleansUpProvisionalFrame)
{
    HTTPServer server({
        { "/main"_s, { "hi"_s } },
        { "/iframe1"_s, { "<script>alert('loaded iframe1')</script>"_s } },
        { "/iframe2"_s, { 302, { { "Location"_s, "https://example.org/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { "this should not be loaded"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:
        @"var iframe1 = document.createElement('iframe');"
        "document.body.appendChild(iframe1);"
        "iframe1.src = 'https://apple.com/iframe1';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded iframe1");

    __block uint32_t failureCount = 0;
    navigationDelegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView* webView, WKFrameInfo* frame, NSError* error) {
        EXPECT_WK_STREQ(error.domain, WebKitErrorDomain);
        EXPECT_EQ(error.code, WebKitErrorFrameLoadInterruptedByPolicyChange);
        failureCount++;
    };

    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction* action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.absoluteString containsString:@"/redirected"])
            completionHandler(WKNavigationActionPolicyCancel);
        else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    [webView evaluateJavaScript:
        @"var iframe2 = document.createElement('iframe');"
        "document.body.appendChild(iframe2);"
        "iframe2.src = 'https://apple.com/iframe2';"
    completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 1; }
    ));

    // Make sure second navigation doesn't assert in WebFrame::createProvisionalFrame()
    [webView evaluateJavaScript:@"iframe2.src = 'https://apple.com/iframe2';" completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 2; }
    ));
}

TEST(SiteIsolation, BasicPostMessageWindowOpen)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        w.postMessage('pong', '*');"
    "    }, false);"
    "</script>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('opened page received ' + event.data);"
    "    }, false);"
    "</script>"_s;

    __block bool openerFinishedLoading { false };
    __block bool openedFinishedLoading { false };
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block RetainPtr<WKWebView> openerWebView;
    __block RetainPtr<WKWebView> openedWebView;

    RetainPtr openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    openerNavigationDelegate.get().didFinishNavigation = ^(WKWebView *opener, WKNavigation *navigation) {
        EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
        checkFrameTreesInProcesses(opener, { { "https://example.com"_s } });
        openerFinishedLoading = true;
    };

    __block RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    openedNavigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://webkit.org/webkit");
        checkFrameTreesInProcesses(openerWebView.get(), { { "https://example.com"_s }, { RemoteFrame } });
        checkFrameTreesInProcesses(openedWebView.get(), { { "https://webkit.org"_s }, { RemoteFrame } });
        auto openerFrames = frameTrees(openerWebView.get());
        auto openedFrames = frameTrees(openedWebView.get());
        EXPECT_NE([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Remote), [openedWebView _webProcessIdentifier]);
        EXPECT_EQ(findFramePID(openedFrames.get(), FrameType::Remote), [openerWebView _webProcessIdentifier]);
        openedFinishedLoading = true;
    };
    openedNavigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        auto openerFrames = frameTrees(openerWebView.get());
        checkFrameTreesInProcesses(openerFrames.get(), { { "https://example.com"_s }, { RemoteFrame } });
        checkFrameTreesInProcesses(openedWebView.get(), { { "https://example.com"_s } });
        EXPECT_EQ([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
        EXPECT_NE([openedWebView _webProcessIdentifier], [openedWebView _provisionalWebProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Remote), [openedWebView _provisionalWebProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Local), [openerWebView _webProcessIdentifier]);
        completionHandler(WKNavigationResponsePolicyAllow);
    };

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    __block RetainPtr<NSString> alert;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().UIDelegate = uiDelegate.get();
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        return openedWebView.get();
    };

    openerWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    openerWebView.get().navigationDelegate = openerNavigationDelegate.get();
    openerWebView.get().UIDelegate = uiDelegate.get();
    openerWebView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&openerFinishedLoading);

    [openerWebView evaluateJavaScript:@"w = window.open('https://webkit.org/webkit')" completionHandler:nil];

    Util::run(&openedFinishedLoading);

    [openedWebView evaluateJavaScript:@"try { window.opener.postMessage('ping', '*'); } catch(e) { alert('error ' + e) }" completionHandler:nil];

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "opened page received pong");
}

TEST(SiteIsolation, NavigationAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/example_opened_after_navigation"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);

    [opened.webView evaluateJavaScript:@"window.location = 'https://example.com/example_opened_after_navigation'" completionHandler:nil];
    [opened.navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://example.com"_s } });

    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
}

TEST(SiteIsolation, CrossSiteIFrameWindowOpensMainFrameSite)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script>w = window.open('https://example.com/opened')</script>"_s } },
        { "/opened"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);

    checkFrameTreesInProcesses(opener.webView.get(), {
        { // example.com process
            "https://example.com"_s, // Main frame
            Vector<ExpectedFrameTree> { { RemoteFrame } } // Child frame
        },
        { // webkit.org process
            RemoteFrame, // Main frame
            Vector<ExpectedFrameTree> { { "https://webkit.org"_s } } // Child frame
        }
    });

    checkFrameTreesInProcesses(opened.webView.get(), {
        { RemoteFrame },
        { "https://example.com"_s }
    });
}

TEST(SiteIsolation, OpenBeforeInitialLoad)
{
    HTTPServer server({
        { "/webkit"_s, { "<script>alert('loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebView> opened;
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = navigationDelegate.get();
        opened.get().UIDelegate = uiDelegate.get();
        return opened.get();
    };
    [webView setUIDelegate:uiDelegate.get()];
    [webView evaluateJavaScript:@"window.open('https://webkit.org/webkit')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "loaded");
}

TEST(SiteIsolation, OpenWithNoopener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit', '_blank', 'noopener')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    __block RetainPtr openerView = opener.webView;
    __block RetainPtr openedView = opened.webView;
    opened.navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
        checkFrameTreesInProcesses(openedView.get(), { { "://"_s } });
        EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
        completionHandler(WKNavigationActionPolicyAllow);
    };
    opened.navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
        checkFrameTreesInProcesses(openedView.get(), { { "://"_s } });
        EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
        completionHandler(WKNavigationResponsePolicyAllow);
    };
    [opened.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(openedView.get(), { { "https://webkit.org"_s } });
    EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
}

TEST(SiteIsolation, OpenWithNoopenerFromWindowOpenedWithNoopener)
{
    HTTPServer server({
        { "/example"_s, { "<script>window.open('https://example.com/example2', '_blank', 'noopener')</script>"_s } },
        { "/example2"_s, { "<script>window.open('https://webkit.org/webkit', '_blank', 'noopener')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, sameSiteOpened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    __block WebViewAndDelegates crossSiteOpened;
    __block pid_t crossSiteOpenedCreationPID { 0 };
    sameSiteOpened.uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        enableSiteIsolation(configuration);
        crossSiteOpened.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        crossSiteOpened.navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [crossSiteOpened.navigationDelegate allowAnyTLSCertificate];
        crossSiteOpened.webView.get().navigationDelegate = crossSiteOpened.navigationDelegate.get();
        crossSiteOpenedCreationPID = [crossSiteOpened.webView _webProcessIdentifier];
        return crossSiteOpened.webView.get();
    };
    while (!crossSiteOpened.webView)
        Util::spinRunLoop();
    [crossSiteOpened.navigationDelegate waitForDidFinishNavigation];

    EXPECT_NE(crossSiteOpenedCreationPID, [opener.webView _webProcessIdentifier]);

    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(sameSiteOpened.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(crossSiteOpened.webView.get(), { { "https://webkit.org"_s } });
    EXPECT_EQ([sameSiteOpened.webView _webProcessIdentifier], [opener.webView _webProcessIdentifier]);
    EXPECT_NE([crossSiteOpened.webView _webProcessIdentifier], [opener.webView _webProcessIdentifier]);
}

static void testOpenWithOpenerFromNoopenerWindow(bool siteIsolationEnabled)
{
    HTTPServer server({
        { "/example"_s, { "<script>window.open('https://example.com/example2', '_blank', 'noopener')</script>"_s } },
        { "/example2"_s, { "<script>location = 'https://webkit.org/webkit'</script>"_s } },
        { "/webkit"_s, { "<script>window.open('https://webkit.org/opened')</script>"_s } },
        { "/opened"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    __block WebViewAndDelegates noopenerOpened;
    __block WebViewAndDelegates withOpenerOpened;
    __block pid_t withOpenerOpenedCreationPID { 0 };

    auto rect = NSMakeRect(0, 0, 800, 600);
    auto [opener, openerNavigationDelegate] = siteIsolationEnabled ? siteIsolatedViewAndDelegate(server, rect) : viewAndDelegate(server, rect);

    noopenerOpened.uiDelegate = adoptNS([TestUIDelegate new]);
    noopenerOpened.uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        if (siteIsolationEnabled)
            enableSiteIsolation(configuration);
        withOpenerOpened.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        withOpenerOpened.navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [withOpenerOpened.navigationDelegate allowAnyTLSCertificate];
        withOpenerOpened.webView.get().navigationDelegate = withOpenerOpened.navigationDelegate.get();
        withOpenerOpenedCreationPID = [withOpenerOpened.webView _webProcessIdentifier];
        return withOpenerOpened.webView.get();
    };

    RetainPtr openerUIDelegate = adoptNS([TestUIDelegate new]);
    openerUIDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        if (siteIsolationEnabled)
            enableSiteIsolation(configuration);
        noopenerOpened.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        noopenerOpened.navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [noopenerOpened.navigationDelegate allowAnyTLSCertificate];
        noopenerOpened.webView.get().navigationDelegate = noopenerOpened.navigationDelegate.get();
        noopenerOpened.webView.get().UIDelegate = noopenerOpened.uiDelegate.get();
        return noopenerOpened.webView.get();
    };
    opener.get().UIDelegate = openerUIDelegate.get();
    opener.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!withOpenerOpened.webView)
        Util::spinRunLoop();

    EXPECT_EQ(withOpenerOpenedCreationPID, [noopenerOpened.webView _webProcessIdentifier]);
    EXPECT_NE(withOpenerOpenedCreationPID, [opener _webProcessIdentifier]);
}

TEST(SiteIsolation, OpenWithOpenerFromNoopenerWindow)
{
    testOpenWithOpenerFromNoopenerWindow(true);
}

TEST(SiteIsolation, OpenWithOpenerFromNoopenerWindowWithoutSiteIsolation)
{
    testOpenWithOpenerFromNoopenerWindow(false);
}

TEST(SiteIsolation, ConcurrentPopupNavigationsToSameSiteShareProcessWhenOneFails)
{
    HTTPServer server({
        { "/example"_s, { "hi"_s } },
        { "/webkit-failing"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } },
        { "/webkit-first"_s, { "hi"_s } },
        { "/webkit-second"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    __block Vector<RetainPtr<TestWKWebView>> openedViews;
    __block Vector<RetainPtr<TestNavigationDelegate>> openedDelegates;
    __block unsigned failedPopups { 0 };
    __block unsigned finishedPopups { 0 };
    RetainPtr openerUIDelegate = adoptNS([TestUIDelegate new]);
    openerUIDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        enableSiteIsolation(configuration);
        RetainPtr openedDelegate = adoptNS([TestNavigationDelegate new]);
        [openedDelegate allowAnyTLSCertificate];
        openedDelegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *) {
            failedPopups++;
        };
        openedDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
            finishedPopups++;
        };
        RetainPtr opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedDelegate.get();
        openedDelegates.append(WTF::move(openedDelegate));
        openedViews.append(WTF::move(opened));
        return openedViews.last().get();
    };

    RetainPtr openerDelegate = adoptNS([TestNavigationDelegate new]);
    [openerDelegate allowAnyTLSCertificate];
    RetainPtr opener = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    opener.get().navigationDelegate = openerDelegate.get();
    opener.get().UIDelegate = openerUIDelegate.get();
    opener.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [openerDelegate waitForDidFinishNavigation];

    [opener evaluateJavaScript:@"w1 = window.open(); w2 = window.open(); w3 = window.open();"
        "w1.location = 'https://webkit.org/webkit-failing';"
        "w2.location = 'https://webkit.org/webkit-first';"
        "w3.location = 'https://webkit.org/webkit-second';" completionHandler:nil];
    while (openedViews.size() < 3 || !failedPopups || finishedPopups < 2)
        Util::spinRunLoop();

    pid_t webKitPid = [openedViews[1] _webProcessIdentifier];
    EXPECT_NE(webKitPid, 0);
    EXPECT_EQ(webKitPid, [openedViews[2] _webProcessIdentifier]);
    EXPECT_NE(webKitPid, [opener _webProcessIdentifier]);

    for (auto& view : openedViews)
        [view _close];
    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
}

TEST(SiteIsolation, PreferencesUpdatesToAllProcesses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } },
        { "/opened"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = NO;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    __block bool opened { false };
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures)
    {
        opened = true;
        return nil;
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open('https://example.com/opened')" inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&opened);
}

TEST(SiteIsolation, ParentOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);

    [opened.webView evaluateJavaScript:@"try { opener.postMessage('test1', '*'); alert('posted message 1') } catch(e) { alert(e) }" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 1");

    [opened.webView evaluateJavaScript:@"try { top.opener.postMessage('test2', '*'); alert('posted message 2') } catch(e) { alert(e) }" inFrame:[opened.webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 2");
}

TEST(SiteIsolation, LoadStringAfterOpen)
{
    NSString *alertOpener = @"<script>alert(!!window.opener)</script>";
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/apple"_s, { alertOpener } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);

    [opened.webView evaluateJavaScript:@"window.location = 'https://apple.com/apple'" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opener.webView evaluateJavaScript:@"w.location = 'https://other.com/apple'" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opened.webView loadHTMLString:alertOpener baseURL:[NSURL URLWithString:@"https://example.org/"]];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opened.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.net/apple"]]];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");
}

TEST(SiteIsolation, LoadDuringOpen)
{
    HTTPServer server({
        { "/example"_s, { "window.open('https://webkit.org/webkit')"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) -> WKWebView * {
        RetainPtr auxiliary = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:[webView configuration]]);
        auxiliary.get().navigationDelegate = navigationDelegate.get();
        [auxiliary loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [navigationDelegate waitForDidFinishNavigation];
        return nil;
    };
    [webView setUIDelegate:uiDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.open('https://webkit.org/webkit');alert('done')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "done");
}

TEST(SiteIsolation, WindowOpenRedirect)
{
    HTTPServer server({
        { "/example1"_s, { "<script>w = window.open('https://webkit.org/webkit1')</script>"_s } },
        { "/webkit1"_s, { 302, { { "Location"_s, "/webkit2"_s } }, "redirecting..."_s } },
        { "/webkit2"_s, { "loaded!"_s } },
        { "/example2"_s, { "<script>w = window.open('https://webkit.org/webkit3')</script>"_s } },
        { "/webkit3"_s, { 302, { { "Location"_s, "https://example.com/example3"_s } }, "redirecting..."_s } },
        { "/example3"_s, { "loaded!"_s } },
        { "/example4"_s, { "<script>w = window.open('https://webkit.org/webkit4')</script>"_s } },
        { "/webkit4"_s, { 302, { { "Location"_s, "https://apple.com/apple"_s } }, "redirecting..."_s } },
        { "/apple"_s, { "loaded!"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example1");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://webkit.org/webkit2");
    }
    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example2");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://example.com/example3");
    }
    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example4");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://apple.com/apple");
    }
}

TEST(SiteIsolation, InitialNavigationRedirect)
{
    HTTPServer server({
        { "/example"_s, { 302, { { "Location"_s, "https://webkit.org/webkit"_s } }, "redirecting..."_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, ReuseUncommittedProcessForInitialNavigation)
{
    HTTPServer server({
        { "/example"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _launchInitialProcessIfNecessary];
    while (![webView _webProcessIdentifier])
        Util::spinRunLoop();
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ(pidBefore, [webView _webProcessIdentifier]);
}

TEST(SiteIsolation, ReuseUncommittedProcessForSameSiteRedirect)
{
    HTTPServer server({
        { "/redirect"_s, { 302, { { "Location"_s, "https://www.example.com/destination"_s } }, "redirecting..."_s } },
        { "/destination"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _launchInitialProcessIfNecessary];
    while (![webView _webProcessIdentifier])
        Util::spinRunLoop();
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/redirect"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ(pidBefore, [webView _webProcessIdentifier]);
}

TEST(SiteIsolation, ReuseUncommittedProcessForCrossSiteRedirect)
{
    HTTPServer server({
        { "/redirect"_s, { 302, { { "Location"_s, "https://webkit.org/destination"_s } }, "redirecting..."_s } },
        { "/destination"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _launchInitialProcessIfNecessary];
    while (![webView _webProcessIdentifier])
        Util::spinRunLoop();
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/redirect"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ(pidBefore, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, @"https://webkit.org/destination");
}

TEST(SiteIsolation, ReuseUncommittedProcessForMultipleRedirects)
{
    HTTPServer server({
        { "/redirect1"_s, { 302, { { "Location"_s, "https://webkit.org/redirect2"_s } }, "redirecting..."_s } },
        { "/redirect2"_s, { 302, { { "Location"_s, "https://apple.com/destination"_s } }, "redirecting..."_s } },
        { "/destination"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _launchInitialProcessIfNecessary];
    while (![webView _webProcessIdentifier])
        Util::spinRunLoop();
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/redirect1"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ(pidBefore, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, @"https://apple.com/destination");
}

static HTTPServer::ResponseMap suspendedReusedMainFrameResponses()
{
    HTTPServer::ResponseMap responses;
    responses.add("/example"_s, HTTPResponse("<script>w = window.open('https://webkit.org/webkit')</script>"_s));
    responses.add("/webkit"_s, HTTPResponse("hi"_s));
    responses.add("/coop"_s, HTTPResponse({ { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "coop"_s));
    responses.add("/destination"_s, HTTPResponse("destination"_s));
    return responses;
}

static void checkSameDocumentNavigationAfterSuspendingReusedMainFrame(NSString *sameDocumentNavigationScript)
{
    HTTPServer server(suspendedReusedMainFrameResponses(), HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    RetainPtr openedWebView = opened.webView;
    RetainPtr openedNavigationDelegate = opened.navigationDelegate;
    EXPECT_WK_STREQ([openedWebView _mainFrameURL].absoluteString, @"https://webkit.org/webkit");

    [openedWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/coop"]]];
    [openedNavigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ([openedWebView _mainFrameURL].absoluteString, @"https://apple.com/coop");

    [openedWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/destination"]]];
    [openedNavigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ([openedWebView _mainFrameURL].absoluteString, @"https://example.com/destination");
    EXPECT_WK_STREQ(openedWebView.get().URL.absoluteString, @"https://example.com/destination");
    auto pidAfterNavigation = [openedWebView _webProcessIdentifier];

    [openedWebView objectByEvaluatingJavaScript:sameDocumentNavigationScript];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [[openedWebView _mainFrameURL].absoluteString isEqualToString:@"https://example.com/same_document"];
    }));
    EXPECT_WK_STREQ([openedWebView _mainFrameURL].absoluteString, @"https://example.com/same_document");
    EXPECT_WK_STREQ(openedWebView.get().URL.absoluteString, @"https://example.com/same_document");

    EXPECT_EQ(pidAfterNavigation, [openedWebView _webProcessIdentifier]);
}

TEST(SiteIsolation, ReplaceStateAfterSuspendingReusedMainFrame)
{
    checkSameDocumentNavigationAfterSuspendingReusedMainFrame(@"history.replaceState(null, null, '/same_document')");
}

TEST(SiteIsolation, PushStateAfterSuspendingReusedMainFrame)
{
    checkSameDocumentNavigationAfterSuspendingReusedMainFrame(@"history.pushState(null, null, '/same_document')");
}

void pollUntilOpenedWindowIsClosed(RetainPtr<WKWebView> webView, bool& finished)
{
    [webView evaluateJavaScript:@"openedWindow.closed" completionHandler:makeBlockPtr([webView, &finished](id result, NSError *error) {
        if ([result boolValue])
            finished = true;
        else
            pollUntilOpenedWindowIsClosed(webView, finished);
    }).get()];
}

TEST(SiteIsolation, ClosedStatePropagation)
{
    HTTPServer server({
        { "/example"_s, { "<script>let openedWindow = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    {
        bool openerSawClosedState = false;
        auto [opener, opened] = openerAndOpenedViews(server);
        [opened.webView evaluateJavaScript:@"window.close()" completionHandler:nil];
        pollUntilOpenedWindowIsClosed(opener.webView, openerSawClosedState);
        Util::run(&openerSawClosedState);
    }

    {
        bool openerSawClosedState = false;
        auto [opener, opened] = openerAndOpenedViews(server);
        [opened.webView _close];
        pollUntilOpenedWindowIsClosed(opener.webView, openerSawClosedState);
        Util::run(&openerSawClosedState);
    }
}

TEST(SiteIsolation, CloseAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);

    EXPECT_FALSE([[opener.webView objectByEvaluatingJavaScript:@"w.closed"] boolValue]);
    [opener.webView evaluateJavaScript:@"w.close()" completionHandler:nil];
    [opened.uiDelegate waitForDidClose];
    [opened.webView _close];
    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    EXPECT_TRUE([[opener.webView objectByEvaluatingJavaScript:@"w.closed"] boolValue]);
}

// FIXME: <rdar://117383420> Add a test that deallocates the opened WKWebView without being asked to by JS.
// Check state using native *and* JS APIs. Make sure processes are torn down as expected.
// Same with the opener WKWebView. We would probably need to set remotePageProxyInOpenerProcess
// to null manually to make the process terminate.
//
// Also test when the opener frame (if it's an iframe) is removed from the tree and garbage collected.
// That should probably do some teardown that should be visible from the API.

TEST(SiteIsolation, PostMessageWithMessagePorts)
{
    auto exampleHTML = "<script>"
    "    const channel = new MessageChannel();"
    "    channel.port1.onmessage = function() {"
    "        alert('parent frame received ' + event.data)"
    "    };"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        event.ports[0].postMessage('got port and message ' + event.data);"
    "    }, false)"
    "</script>"_s;

    auto example2HTML = "<script>"
    "    onload = () => {"
    "        const channel = new MessageChannel();"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);"
    "        channel.port1.postMessage('sent message after sending port');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit2'></iframe>"_s;

    auto webkit2HTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        event.ports[0].onmessage = (e)=>{ alert('port received message ' + event.data); }"
    "    }, false)"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } },
        { "/example2"_s, { example2HTML } },
        { "/webkit2"_s, { webkit2HTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    // This test exercises MessagePort delivery across site-isolated processes, not BFCache.
    // Disable BFCache so the same-site navigation does not cache the page holding the port.
    auto *configuration = server.httpsProxyConfiguration();
    configuration.processPool = processPoolWithBackForwardCacheDisabled().get();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "parent frame received got port and message ping");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example2"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "port received message ping");
}

TEST(SiteIsolation, PostMessageWithMessagePortsFromIFrameToMainFrame)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('main frame received ' + event.data + ' with ' + event.ports.length + ' ports');"
    "        if (event.ports.length)"
    "            event.ports[0].postMessage('pong');"
    "    }, false)"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    onload = () => {"
    "        const channel = new MessageChannel();"
    "        channel.port1.onmessage = (event) => {"
    "            parent.postMessage('port message ' + event.data, '*');"
    "        };"
    "        parent.postMessage('ping', '*', [channel.port2]);"
    "        parent.postMessage('message sent after transferring port', '*');"
    "        parent.postMessage('another message sent after transferring port', '*');"
    "    }"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    // The message transferring the port must be delivered before the messages sent right after it.
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received ping with 1 ports");
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received message sent after transferring port with 0 ports");
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received another message sent after transferring port with 0 ports");
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received port message pong with 0 ports");

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
}

TEST(SiteIsolation, PostMessageWithNotAllowedTargetOrigin)
{
    auto exampleHTML = "<script>"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'https://foo.org');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "    setTimeout(() => { alert('child did not receive message'); }, 1000);"
    "</script>"_s;

    bool finishedLoading { false };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse(exampleHTML).serialize());
                continue;
            }
            if (path == "/webkit"_s) {
                co_await connection.awaitableSend(HTTPResponse(webkitHTML).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "child did not receive message");

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
}

TEST(SiteIsolation, PostMessageToIFrameWithOpaqueOrigin)
{
    auto exampleHTML = "<script>"
    "    onload = () => {"
    "        try {"
    "           document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'data:');"
    "        } catch (error) {"
    "           alert(error);"
    "        }"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "</script>"_s;

    bool finishedLoading { false };
    
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "SyntaxError: The string did not match the expected pattern.");

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
}

TEST(SiteIsolation, QueryFramesStateAfterNavigating)
{
    HTTPServer server({
        { "/page2.html"_s, { "<iframe src='subframe4.html'></iframe>"_s } },
        { "/subframe1.html"_s, { "SubFrame1"_s } },
        { "/subframe2.html"_s, { "SubFrame2"_s } },
        { "/subframe3.html"_s, { "SubFrame3"_s } },
        { "/subframe4.html"_s, { "SubFrame4"_s } }
    }, HTTPServer::Protocol::Http);
    server.addResponse("/page1.html"_s, { makeString("<iframe src='subframe1.html'></iframe><iframe src='subframe2.html'></iframe><iframe src='http://localhost:"_s, server.port(), "/subframe3.html'></iframe>"_s) });

    auto runTest = [&] (bool withSiteIsolation) {
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        if (withSiteIsolation)
            enableSiteIsolation(configuration.get());
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        [webView synchronouslyLoadRequest:server.request("/page1.html"_s)];
        EXPECT_EQ(3u, [webView mainFrame].childFrames.count);

        [webView synchronouslyLoadRequest:server.request("/page2.html"_s)];
        EXPECT_EQ(1u, [webView mainFrame].childFrames.count);

        [webView synchronouslyGoBack];
        EXPECT_EQ(3u, [webView mainFrame].childFrames.count);
    };
    runTest(true);
    runTest(false);
}

TEST(SiteIsolation, QueryFramesStateAfterGoingBackToCachedPageWithIframe)
{
    HTTPServer server({
        { "/page1.html"_s, { "<iframe src='subframe.html'></iframe>"_s } },
        { "/page2.html"_s, { ""_s } },
        { "/subframe.html"_s, { "SubFrame"_s } }
    }, HTTPServer::Protocol::Http);

    auto runTest = [&] (bool withSiteIsolation) {
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        if (withSiteIsolation) {
            enableSiteIsolation(configuration.get());
            setFeatureEnabled(configuration.get(), @"MultiProcessBackForwardCacheEnabled", true);
        }
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        [webView synchronouslyLoadRequest:server.request("/page1.html"_s)];
        EXPECT_EQ(1u, [webView mainFrame].childFrames.count);
        RetainPtr<WKFrameInfo> childFrame = [webView mainFrame].childFrames.firstObject.info;
        [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];
        [webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker = true" inFrame:childFrame.get()];

        [webView synchronouslyLoadRequest:server.request("/page2.html"_s)];
        EXPECT_EQ(0u, [webView mainFrame].childFrames.count);

        [webView synchronouslyGoBack];
        EXPECT_EQ(1u, [webView mainFrame].childFrames.count);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView mainFrame].childFrames.firstObject.info] boolValue]);

        [webView synchronouslyGoForward];
        EXPECT_EQ(0u, [webView mainFrame].childFrames.count);

        [webView evaluateJavaScript:@"history.back()" completionHandler:nil];
        [webView _test_waitForDidFinishNavigation];
        EXPECT_EQ(1u, [webView mainFrame].childFrames.count);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView mainFrame].childFrames.firstObject.info] boolValue]);
    };
    runTest(true);
    runTest(false);
}

TEST(SiteIsolation, NavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://example.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
}

TEST(SiteIsolation, ParentNavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://example.com/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>onload = ()=>{ alert('done') }</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, IframeNavigatesSelfWithoutChangingOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>window.location='/webkit_second'</script>"_s } },
        { "/webkit_second"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, IframeWithConfirm)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>confirm('confirm message')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForConfirm], "confirm message");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, IframeWithPrompt)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>prompt('prompt message', 'default input')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForPromptWithReply:@"default input"], "prompt message");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

// Make sure the main frame remains responsive even while a cross origin iframe is
// displaying an alert.
TEST(SiteIsolation, MainFrameHeartbeatContinuesWhileCrossSiteIframeDialogShown)
{
    HTTPServer server({
        { "/example"_s, { "<script>"
            "window.__heartbeat = 0;"
            "setInterval(() => { window.__heartbeat++; }, 10);"
            "</script>"
            "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('subframe alert')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr protectedWebView = webView;

    // Hold the subframe's alert open; its WebContent process is now blocked in synchronous IPC.
    __block bool receivedSubframeAlert = false;
    __block BlockPtr<void()> heldCompletion;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        EXPECT_WK_STREQ(message, "subframe alert");
        EXPECT_WK_STREQ(frameInfo.securityOrigin.host, "webkit.org");
        heldCompletion = makeBlockPtr(completionHandler);
        receivedSubframeAlert = true;
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    TestWebKitAPI::Util::run(&receivedSubframeAlert);

    // Read window.__heartbeat from the main frame. Bounded wait: if the main frame's process were
    // blocked by the subframe's dialog, evaluateJavaScript would not respond and this reports
    // didRespond == false (a clean failure) rather than hanging.
    auto readHeartbeat = [&](bool& didRespond) -> int {
        __block bool done = false;
        __block int value = -1;
        [protectedWebView evaluateJavaScript:@"window.__heartbeat" completionHandler:^(id result, NSError *error) {
            if (!error && [result isKindOfClass:[NSNumber class]])
                value = [result intValue];
            done = true;
        }];
        for (unsigned attempt = 0; attempt < 100 && !done; ++attempt)
            TestWebKitAPI::Util::runFor(Seconds(0.05));
        didRespond = done;
        return value;
    };

    bool firstResponded = false;
    int firstHeartbeat = readHeartbeat(firstResponded);
    EXPECT_TRUE(firstResponded);

    // The main frame's timer must keep firing while the dialog is held.
    int laterHeartbeat = firstHeartbeat;
    for (unsigned attempt = 0; laterHeartbeat <= firstHeartbeat && attempt < 40; ++attempt) {
        TestWebKitAPI::Util::runFor(Seconds(0.05));
        bool responded = false;
        laterHeartbeat = readHeartbeat(responded);
        if (!responded) {
            EXPECT_TRUE(responded);
            break;
        }
    }
    EXPECT_GT(laterHeartbeat, firstHeartbeat);

    if (heldCompletion)
        heldCompletion();
}

// With site isolation, a single WKWebView can have more than one WebContent process at once hosting it.
// Each of those processes can make a JS dialog request to the UI process simultaneously.
// As far as WKWebView API is concerned, the client would only expect to see one dialog at a time.
// This test verifies that WebKit queues the requests and sends them to the API client serially.
TEST(SiteIsolation, SimultaneousDialogsFromMultipleCrossOriginFrames)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"
            "<iframe src='https://apple.com/apple'></iframe>"
            "<script>setTimeout(() => alert('example dialog'), 0)</script>"_s } },
        { "/webkit"_s, { "<script>alert('webkit dialog')</script>"_s } },
        { "/apple"_s, { "<script>alert('apple dialog')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    // The three frames each request a dialog at roughly the same time, but WebKit must deliver
    // them to the client one at a time: the next dialog should only appear after the previous
    // one's completion handler has been called.
    __block unsigned outstandingDialogs = 0;
    __block unsigned totalDialogs = 0;
    __block bool sawMoreThanOneAtOnce = false;
    __block BlockPtr<void()> pendingCompletion;
    RetainPtr receivedHosts = adoptNS([NSMutableSet new]);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        [receivedHosts addObject:frameInfo.securityOrigin.host];
        totalDialogs++;
        if (++outstandingDialogs > 1)
            sawMoreThanOneAtOnce = true;
        // Hold this dialog open; the test's run loop will release it, and only then should the
        // next queued dialog be delivered.
        pendingCompletion = makeBlockPtr(completionHandler);
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    // Release dialogs one at a time until all three have been delivered serially.
    for (unsigned attempt = 0; attempt < 100 && totalDialogs < 3; ++attempt) {
        TestWebKitAPI::Util::runFor(Seconds(0.05));
        if (pendingCompletion) {
            auto completion = std::exchange(pendingCompletion, nullptr);
            --outstandingDialogs;
            completion();
        }
    }

    EXPECT_EQ(totalDialogs, 3u);
    EXPECT_FALSE(sawMoreThanOneAtOnce);
    EXPECT_TRUE([receivedHosts containsObject:@"example.com"]);
    EXPECT_TRUE([receivedHosts containsObject:@"webkit.org"]);
    EXPECT_TRUE([receivedHosts containsObject:@"apple.com"]);

    if (pendingCompletion) {
        auto completion = std::exchange(pendingCompletion, nullptr);
        completion();
    }
}

// Has two cross-origin iframes request a modal dialog, then navigates the main frame while one
// of those requests is still queued. Makes sure the still-queued request cancels cleanly.
TEST(SiteIsolation, QueuedDialogPurgedByMainFrameNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"
            "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('webkit dialog')</script>"_s } },
        { "/apple"_s, { "<script>alert('apple dialog')</script>"_s } },
        { "/next"_s, { "<p>next</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    __block unsigned totalDialogs = 0;
    __block BlockPtr<void()> firstCompletion;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        if (!totalDialogs++) {
            // Hold the first dialog open so the second stays queued in the UI process.
            firstCompletion = makeBlockPtr(completionHandler);
            return;
        }
        // Reaching here means a queued dialog was delivered after the navigation, which is the bug.
        completionHandler();
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    // Wait for the first dialog to be shown and held.
    while (!firstCompletion)
        TestWebKitAPI::Util::runFor(Seconds(0.05));

    // Give the second cross-origin frame time to get its dialog request queued behind the first. It
    // cannot be shown while the first is held, so a longer wait here is harmless.
    TestWebKitAPI::Util::runFor(Seconds(0.5));

    // Waiting for the provisional load would be racy: the old frames are the page's tree until the commit.
    __block bool navigationCommitted = false;
    navigationDelegate.get().didCommitNavigation = ^(WKWebView *, WKNavigation *) {
        navigationCommitted = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/next"]]];
    while (!navigationCommitted)
        TestWebKitAPI::Util::runFor(Seconds(0.05));

    // Release the first dialog. If the queued dialog had not been purged, dismissing the first would
    // let it flow to the API client now.
    std::exchange(firstCompletion, nullptr)();

    // Let anything still in flight settle, then confirm only the first dialog was ever delivered.
    TestWebKitAPI::Util::runFor(Seconds(0.5));
    EXPECT_EQ(totalDialogs, 1u);
}

TEST(SiteIsolation, QueuedSameProcessDialogPurgedByMainFrameNavigation)
{
    // Same-site iframes share a process, so the first frame's modal run loop blocks the second frame's script.
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/first'></iframe>"
            "<iframe src='https://webkit.org/second'></iframe>"_s } },
        { "/first"_s, { "<script>alert('first dialog')</script>"_s } },
        { "/second"_s, { "<script>alert('second dialog')</script>"_s } },
        { "/next"_s, { "<p>next</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    __block unsigned totalDialogs = 0;
    __block BlockPtr<void()> firstCompletion;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        if (!totalDialogs++) {
            firstCompletion = makeBlockPtr(completionHandler);
            return;
        }
        completionHandler();
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    while (!firstCompletion)
        TestWebKitAPI::Util::runFor(Seconds(0.05));

    TestWebKitAPI::Util::runFor(Seconds(0.5));

    __block bool navigationCommitted = false;
    navigationDelegate.get().didCommitNavigation = ^(WKWebView *, WKNavigation *) {
        navigationCommitted = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/next"]]];
    while (!navigationCommitted)
        TestWebKitAPI::Util::runFor(Seconds(0.05));

    std::exchange(firstCompletion, nullptr)();

    TestWebKitAPI::Util::runFor(Seconds(0.5));
    EXPECT_EQ(totalDialogs, 1u);
}

TEST(SiteIsolation, GrandchildIframe)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe onload='alert(\"grandchild loaded successfully\")' srcdoc=\"<script>window.location='https://apple.com/apple'</script>\">"_s } },
        { "/apple"_s, { ""_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");
}

TEST(SiteIsolation, GrandchildIframeSameOriginAsGrandparent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe src='https://example.com/example_grandchild'></iframe>\">"_s } },
        { "/example_grandchild"_s, { "<script>alert('grandchild loaded successfully')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame, { { "https://example.com"_s } } } } },
        { RemoteFrame, { { "https://webkit.org"_s, { { RemoteFrame } } } } }
    });
}

TEST(SiteIsolation, ChildNavigatingToNewDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://foo.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "foo.com");
}

TEST(SiteIsolation, ChildNavigatingToMainFrameDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://example.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
}

TEST(SiteIsolation, ChildNavigatingToSameDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://webkit.org/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, ChildNavigatingToDomainLoadedOnADifferentPage)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('done')</script>"_s } },
        { "/foo"_s, { "<iframe id='foo'><html><body><p>Hello world.</p></body></html></iframe>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [firstWebView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [firstWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/foo"]]];
    
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:firstWebView.get().configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto firstWebViewMainFrame = [firstWebView mainFrame];
    EXPECT_NE(firstWebViewMainFrame.info._processIdentifier, 0);
    pid_t firstFramePID = firstWebViewMainFrame.info._processIdentifier;
    EXPECT_WK_STREQ(firstWebViewMainFrame.info.securityOrigin.host, "webkit.org");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_NE(firstFramePID, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, MainFrameWithTwoIFramesInTheSameProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame_1' src='https://webkit.org/a'></iframe><iframe id='webkit_frame_2' src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { "<script>alert('donea')</script>"_s } },
        { "/b"_s, { "<script>alert('doneb')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    NSString* alert1 = [webView _test_waitForAlert];
    NSString* alert2 = [webView _test_waitForAlert];
    if ([alert1 isEqualToString:@"donea"])
        EXPECT_WK_STREQ(alert2, "doneb");
    else if ([alert1 isEqualToString:@"doneb"])
        EXPECT_WK_STREQ(alert2, "donea");
    else
        EXPECT_TRUE(false);

    auto mainFrame = [webView mainFrame];
    EXPECT_EQ(mainFrame.childFrames.count, 2u);
    _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
    _WKFrameTreeNode *otherChildFrame = mainFrame.childFrames[1];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    pid_t otherChildFramePid = otherChildFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(otherChildFramePid, 0);
    EXPECT_EQ(childFramePid, otherChildFramePid);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
    EXPECT_WK_STREQ(otherChildFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, ChildBeingNavigatedToMainFrameDomainByParent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto mainFrame = [webView mainFrame];
    auto childFrame = [webView firstChildFrame];
    EXPECT_NE(mainFrame.info._processIdentifier, childFrame._processIdentifier);

    [webView evaluateJavaScript:@"document.getElementById('webkit_frame').src = 'https://example.com/example_subframe'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });

    while (processStillRunning(childFrame._processIdentifier))
        Util::spinRunLoop();
}

TEST(SiteIsolation, ChildBeingNavigatedToSameDomainByParent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://webkit.org/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = [webView firstChildFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.securityOrigin.host, "webkit.org");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, ChildBeingNavigatedToNewDomainByParent)
{
    auto appleHTML = "<script>"
        "window.addEventListener('message', (event) => {"
        "    parent.window.postMessage(event.data + 'pong', { 'targetOrigin' : '*' });"
        "}, false);"
        "alert('apple iframe loaded')"
        "</script>"_s;

    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://apple.com/apple' }</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } },
        { "/apple"_s, { appleHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "apple iframe loaded");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s } }
        }
    });

    NSString *jsCheckingPostMessageRoundTripAfterIframeProcessChange = @""
    "window.addEventListener('message', (event) => {"
    "    alert('parent frame received ' + event.data)"
    "}, false);"
    "document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*');";
    [webView evaluateJavaScript:jsCheckingPostMessageRoundTripAfterIframeProcessChange completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "parent frame received pingpong");
}

TEST(SiteIsolation, IframeRedirectSameSite)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { 302, { { "Location"_s, "https://www.webkit.org/www_webkit"_s } }, "redirecting..."_s } },
        { "/www_webkit"_s, { "arrived!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://www.webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, IframeRedirectCrossSite)
{
    HTTPServer server({
        { "/example1"_s, { "<iframe src='https://webkit.org/webkit1'></iframe>"_s } },
        { "/webkit1"_s, { 302, { { "Location"_s, "https://apple.com/apple1"_s } }, "redirecting..."_s } },
        { "/apple1"_s, { "arrived!"_s } },
        { "/example2"_s, { "<iframe src='https://webkit.org/webkit2'></iframe>"_s } },
        { "/webkit2"_s, { 302, { { "Location"_s, "https://webkit.org/webkit3"_s } }, "redirecting..."_s } },
        { "/webkit3"_s, { 302, { { "Location"_s, "https://example.com/example3"_s } }, "redirecting..."_s } },
        { "/example3"_s, { "arrived!"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s } }
        }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        },
        // example1 is BFCached on the same-site nav to example2; the apple.com
        // iframe process stays alive as a suspended cached iframe and surfaces
        // here as a remote tree (remote main with one remote child).
        { RemoteFrame, { { RemoteFrame } } }
    });
}

TEST(SiteIsolation, CrossOriginOpenerPolicy)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "iframe content"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CrossOriginPopupWithCOOPValueSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { { { "Content-Type"_s, "text/html"_s }, { "cross-origin-opener-policy"_s, "same-origin"_s } }, "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    EXPECT_NE([opener.webView _webProcessIdentifier], [opened.webView _webProcessIdentifier]);

    [opened.webView evaluateJavaScript:@"alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");

    [opener.webView evaluateJavaScript:@"alert(w.closed)" completionHandler:nil];
    EXPECT_WK_STREQ([opener.uiDelegate waitForAlert], "true");
}

TEST(SiteIsolation, CrossOriginPopupWithOpenerCOOPValueSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { { { "Content-Type"_s, "text/html"_s }, { "cross-origin-opener-Policy"_s, "same-origin"_s } }, "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    EXPECT_NE([opener.webView _webProcessIdentifier], [opened.webView _webProcessIdentifier]);

    [opened.webView evaluateJavaScript:@"alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");

    [opener.webView evaluateJavaScript:@"alert(w.closed)" completionHandler:nil];
    EXPECT_WK_STREQ([opener.uiDelegate waitForAlert], "true");
}

static void testCrossOriginOpenerPolicyMainFrame(bool useSharedProcess)
{
    HTTPServer server({
        { "/example"_s, { { { "cross-origin-opener-policy"_s, "same-origin-allow-popups"_s } }, "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "iframe content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;

    if (useSharedProcess)
        std::tie(webView, navigationDelegate) = siteIsolatedViewWithSharedProcess(server);
    else
        std::tie(webView, navigationDelegate) = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CrossOriginOpenerPolicyMainFrame)
{
    testCrossOriginOpenerPolicyMainFrame(false);
}

TEST(SiteIsolation, CrossOriginOpenerPolicyMainFrameWithSharedProcess)
{
    testCrossOriginOpenerPolicyMainFrame(true);
}

TEST(SiteIsolation, NavigationWithIFrames)
{
    HTTPServer server({
        { "/1"_s, { "<iframe src='https://domain2.com/2'></iframe>"_s } },
        { "/2"_s, { "hi!"_s } },
        { "/3"_s, { "<iframe src='https://domain4.com/4'></iframe>"_s } },
        { "/4"_s, { "<iframe src='https://domain5.com/5'></iframe>"_s } },
        { "/5"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/3"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain3.com"_s, { { RemoteFrame, { { RemoteFrame } } } } },
        { RemoteFrame, { { "https://domain4.com"_s, { { RemoteFrame } } } } },
        { RemoteFrame, { { RemoteFrame, { { "https://domain5.com"_s } } } } }
    });

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    };
    // BFCache restore does not fire didFinishLoad in the subframe, so spin
    // until the cross-process tree is fully reconstructed.
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));
}

TEST(SiteIsolation, NavigationWithIFramesWithSharedProcess)
{
    HTTPServer server({
        { "/1"_s, { "<iframe src='https://domain2.com/2'></iframe>"_s } },
        { "/2"_s, { "hi!"_s } },
        { "/3"_s, { "<iframe src='https://domain4.com/4'></iframe>"_s } },
        { "/4"_s, { "<iframe src='https://domain5.com/5'></iframe>"_s } },
        { "/5"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/3"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain3.com"_s, { { RemoteFrame, { { RemoteFrame } } } } },
        { RemoteFrame, { { "https://domain4.com"_s, { { "https://domain5.com"_s } } } } }
    });

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    };
    // BFCache restore does not fire didFinishLoad in the subframe, so spin
    // until the cross-process tree is fully reconstructed.
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));
}

TEST(SiteIsolation, RemoveFrames)
{
    HTTPServer server({
        { "/webkit_main"_s, { "<iframe src='https://webkit.org/webkit_iframe' id='wk'></iframe><iframe src='https://example.com/example_iframe' id='ex'></iframe>"_s } },
        { "/webkit_iframe"_s, { "hi!"_s } },
        { "/example_iframe"_s, { "<iframe src='example_grandchild_frame'></iframe>"_s } },
        { "/example_grandchild_frame"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit_main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { "https://webkit.org"_s }, { RemoteFrame, { { RemoteFrame } } } }
        }, { RemoteFrame,
            { { RemoteFrame }, { "https://example.com"_s, { { "https://example.com"_s } } } }
        }
    });

    __block bool removedLocalFrame { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('wk');frame.parentNode.removeChild(frame);" completionHandler:^(id, NSError *error) {
        removedLocalFrame = true;
    }];
    Util::run(&removedLocalFrame);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame, { { RemoteFrame } } } }
        }, { RemoteFrame,
            { { "https://example.com"_s, { { "https://example.com"_s } } } }
        }
    });

    __block bool removedRemoteFrame { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('ex');frame.parentNode.removeChild(frame);" completionHandler:^(id, NSError *error) {
        removedRemoteFrame = true;
    }];
    Util::run(&removedRemoteFrame);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s }
    });
}

TEST(SiteIsolation, RemoveFrameFromRemoteFrame)
{
    HTTPServer server({
        { "/main"_s, { "<iframe src='https://webkit.org/child'></iframe>"_s } },
        { "/child"_s, { "<iframe src='https://example.com/grandchild' id=grandchildframe></iframe>"_s } },
        { "/grandchild"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame, { { "https://example.com"_s } } } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s, { { RemoteFrame } } } }
        }
    });

    [webView objectByEvaluatingJavaScript:@"grandchildframe.parentNode.removeChild(grandchildframe);1" inFrame:[webView firstChildFrame]];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, ProvisionalLoadFailure)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } },
        { "/apple"_s,  { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { "https://example.com"_s } } }
    });

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://apple.com/apple'" completionHandler:nil];
    Vector<ExpectedFrameTree> expectedFrameTreesAfterAddingApple { {
        "https://example.com"_s, { { "https://example.com"_s }, { RemoteFrame } }
    }, {
        RemoteFrame, { { RemoteFrame }, { "https://apple.com"_s } }
    } };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedFrameTreesAfterAddingApple }))
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"iframe.onload = alert('done');iframe.src = 'https://webkit.org/webkit'" completionHandler:nil];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedFrameTreesAfterAddingApple));
}

TEST(SiteIsolation, MultipleReloads)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s,  { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView reload];
    Util::runFor(0.1_s);
    [webView reload];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
}

#if ENABLE(DRAG_SUPPORT) && !PLATFORM(MACCATALYST)
TEST(SiteIsolation, DragAndDropWithoutNavigation)
{
    auto mainframeHTML = "<!DOCTYPE html>"
    "<html>"
    "    <head>"
    "        <meta charset='utf8'>"
    "        <meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'>"
    "        <style>"
    "            body {"
    "                width: 100%;"
    "                height: 100%;"
    "                margin: 0;"
    "                background-color: antiquewhite;"
    "            }"
    "        </style>"
    "    </head>"
    "    <body>"
    "        <iframe src='https://domain2.com/subframe' style='width: 600px; height: 600px;'></iframe>"
    "    </body>"
    "</html>"_s;

    auto subframeHTML = "<!DOCTYPE html>"
    "<html>"
    "    <head>"
    "    <meta charset='utf8'>"
    "    <meta name='viewport' content='width=device-width, initial-scale=1' />"
    "    <style>"
    "        body {"
    "            margin: 0;"
    "        }"
    "       #draggable {"
    "           background-color: cyan;"
    "           width: 200px;"
    "           height: 200px;"
    "           border: 1px black dotted;"
    "       }"
    "       #dropzone {"
    "           background-color: pink;"
    "           width: 200px;"
    "           height: 200px;"
    "           border: 1px black dotted;"
    "       }"
    "    </style>"
    "    </head>"
    "    <body>"
    "       <div draggable='true' id='draggable'>Hello World</div>"
    "       <div id='dropzone'></div>"
    "    <script>"
    "        window.dropCount = 0;"
    "        const draggable = document.getElementById('draggable');"
    "        draggable.addEventListener('dragstart', function(e) {"
    "               e.dataTransfer.setData('text/plain', 'hello world');"
    "           });"
    "       const dropzone = document.getElementById('dropzone');"
    "       dropzone.addEventListener('dragenter', e => e.preventDefault());"
    "       dropzone.addEventListener('dragover', e => e.preventDefault());"
    "       dropzone.addEventListener('drop', function(e) {"
    "           e.preventDefault();"
    "           const data = e.dataTransfer.getData('text/plain');"
    "           window.dropCount = 1;"
    "       });"
    "    </script>"
    "    </body>"
    "</html>"_s;

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html "_s } }, mainframeHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html "_s } }, subframeHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(72, 92) to:CGPointMake(86, 274)];

    __block bool didDecideNavigationPolicy = false;
    [navigationDelegate setDecidePolicyForNavigationAction:^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        didDecideNavigationPolicy = true;
    }];

    __block bool done = false;
    __block int windowDropCount = 0;
    [webView evaluateJavaScript:@"window.dropCount" inFrame:[webView firstChildFrame] completionHandler:^(id resultValue, NSError *error) {
        EXPECT_NULL(error);
        done = true;
        windowDropCount = [resultValue intValue];
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(didDecideNavigationPolicy);
    EXPECT_EQ(windowDropCount, 1);
}
#endif

TEST(SiteIsolation, ShutDownFrameProcessesAfterNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hello"_s } },
        { "/apple"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // The test verifies that the iframe process is torn down on cross-site
    // navigation. With BFCache the iframe page would be kept alive as a
    // suspended cached iframe, so disable BFCache to keep the original
    // shutdown semantics under test.
    auto *configuration = server.httpsProxyConfiguration();
    configuration.processPool = processPoolWithBackForwardCacheDisabled().get();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), { { "https://apple.com"_s } });

    while (processStillRunning(iframePID))
        Util::spinRunLoop();
}

TEST(SiteIsolation, ShutDownFrameProcessesAfterNavigationBFCacheEnabled)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hello"_s } },
        { "/apple"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // With BFCache enabled (the default when Site Isolation is active), the
    // suspended iframe process is kept alive until the BFCache entry is
    // evicted. Verify that clearing the cache releases the process.
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // BFCache keeps the iframe process alive after navigation.
    EXPECT_TRUE(processStillRunning(iframePID));

    // Evicting the BFCache entry must release the suspended iframe process.
    [webView _clearBackForwardCache];
    while (processStillRunning(iframePID))
        Util::spinRunLoop();
}

TEST(SiteIsolation, OpenerProcessSharing)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/opener_iframe'></iframe>"_s } },
        { "/opened"_s, { "<iframe src='https://webkit.org/opened_iframe'></iframe>"_s } },
        { "/opener_iframe"_s, { "hello"_s } },
        { "/opened_iframe"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server);

    __block RetainPtr<TestWKWebView> openedWebView;
    __block RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        static RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [openedNavigationDelegate allowAnyTLSCertificate];
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"w = window.open('/opened')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "done");

    auto openerMainFrame = [webView mainFrame];
    auto openedMainFrame = [openedWebView mainFrame];
    pid_t openerMainFramePid = openerMainFrame.info._processIdentifier;
    pid_t openedMainFramePid = openedMainFrame.info._processIdentifier;
    pid_t openerIframePid = openerMainFrame.childFrames.firstObject.info._processIdentifier;
    pid_t openedIframePid = openedMainFrame.childFrames.firstObject.info._processIdentifier;

    EXPECT_EQ(openerMainFramePid, openedMainFramePid);
    EXPECT_NE(openerMainFramePid, 0);
    EXPECT_EQ(openerIframePid, openedIframePid);
    EXPECT_NE(openerIframePid, 0);
}

TEST(SiteIsolation, OpenerProcessSharingWithSharedProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/opener_iframe'></iframe>"_s } },
        { "/opened"_s, { "<iframe src='https://webkit.org/opened_iframe'></iframe>"_s } },
        { "/opener_iframe"_s, { "hello"_s } },
        { "/opened_iframe"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewWithSharedProcess(server);

    __block RetainPtr<TestWKWebView> openedWebView;
    __block RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        static RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [openedNavigationDelegate allowAnyTLSCertificate];
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"w = window.open('/opened')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "done");

    auto openerMainFrame = [webView mainFrame];
    auto openedMainFrame = [openedWebView mainFrame];
    pid_t openerMainFramePid = openerMainFrame.info._processIdentifier;
    pid_t openedMainFramePid = openedMainFrame.info._processIdentifier;
    pid_t openerIframePid = openerMainFrame.childFrames.firstObject.info._processIdentifier;
    pid_t openedIframePid = openedMainFrame.childFrames.firstObject.info._processIdentifier;

    EXPECT_EQ(openerMainFramePid, openedMainFramePid);
    EXPECT_NE(openerMainFramePid, 0);
    EXPECT_EQ(openerIframePid, openedIframePid);
    EXPECT_NE(openerIframePid, 0);
}

TEST(SiteIsolation, ConvertRectToMainFrameCoordinatesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<body style='margin: 0; min-height: 1000px'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowAutofill = YES;
    RetainPtr autofillWorld = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    EXPECT_WK_STREQ("undefined", [webView objectByEvaluatingJavaScript:@"typeof window.convertRectToMainFrameCoordinates" inFrame:childFrameInfo.get()]);
    EXPECT_WK_STREQ("undefined", [webView objectByEvaluatingJavaScript:@"typeof window.convertRectToMainFrameCoordinates" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.defaultClientWorld]);
    EXPECT_WK_STREQ("function", [webView objectByEvaluatingJavaScript:@"typeof window.convertRectToMainFrameCoordinates" inFrame:childFrameInfo.get() inContentWorld:autofillWorld.get()]);

    auto convertRect = [&] {
        NSArray *result = [webView objectByEvaluatingJavaScript:@"(() => { let r = window.convertRectToMainFrameCoordinates({ x: 20, y: 530, width: 10, height: 15 }); return [r.x, r.y, r.width, r.height]; })()" inFrame:childFrameInfo.get() inContentWorld:autofillWorld.get()];
        EXPECT_EQ(result.count, 4u);
        return CGRectMake([result[0] doubleValue], [result[1] doubleValue], [result[2] doubleValue], [result[3] doubleValue]);
    };

    // Unscrolled, the rect is offset only by the iframe's position in the main frame.
    // The iframe's position is synced asynchronously from the main frame's process after layout.
    CGRect rect;
    EXPECT_TRUE(Util::waitFor([&] {
        rect = convertRect();
        return rect.origin.x == 120;
    }));
    EXPECT_EQ(rect.origin.x, 120);
    EXPECT_EQ(rect.origin.y, 630);
    EXPECT_EQ(rect.size.width, 10);
    EXPECT_EQ(rect.size.height, 15);

    // Scrolling the iframe moves its contents up relative to the main frame.
    [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo.get()] intValue] == 500;
    }));
    [webView waitForNextPresentationUpdate];

    rect = convertRect();
    EXPECT_EQ(rect.origin.x, 120);
    EXPECT_EQ(rect.origin.y, 130);
    EXPECT_EQ(rect.size.width, 10);
    EXPECT_EQ(rect.size.height, 15);
}

TEST(SiteIsolation, ConvertRectToMainFrameCoordinatesInCrossOriginIframeWithPageZoom)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<body style='margin: 0; min-height: 1000px'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowAutofill = YES;
    RetainPtr autofillWorld = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    auto convertRect = [&] {
        NSArray *result = [webView objectByEvaluatingJavaScript:@"(() => { let r = window.convertRectToMainFrameCoordinates({ x: 20, y: 30, width: 10, height: 15 }); return [r.x, r.y, r.width, r.height]; })()" inFrame:childFrameInfo.get() inContentWorld:autofillWorld.get()];
        EXPECT_EQ(result.count, 4u);
        return CGRectMake([result[0] doubleValue], [result[1] doubleValue], [result[2] doubleValue], [result[3] doubleValue]);
    };

    CGRect rect;
    EXPECT_TRUE(Util::waitFor([&] {
        rect = convertRect();
        return rect.origin.x == 120;
    }));
    EXPECT_EQ(rect.origin.x, 120);
    EXPECT_EQ(rect.origin.y, 130);
    EXPECT_EQ(rect.size.width, 10);
    EXPECT_EQ(rect.size.height, 15);

    // Zooming the page scales both the position and the size of the rect.
    webView.get().pageZoom = 2;
    EXPECT_TRUE(Util::waitFor([&] {
        rect = convertRect();
        return rect.origin.x != 120;
    }));
    [webView waitForNextPresentationUpdate];

    rect = convertRect();
    EXPECT_EQ(rect.origin.x, 240);
    EXPECT_EQ(rect.origin.y, 260);
    EXPECT_EQ(rect.size.width, 20);
    EXPECT_EQ(rect.size.height, 30);
}

TEST(SiteIsolation, SetFocusedFrame)
{
    auto mainframeHTML = "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView mainFrame].info._isFocused);
    EXPECT_FALSE([webView firstChildFrame]._isFocused);

    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while ([webView mainFrame].info._isFocused || ![webView firstChildFrame]._isFocused)
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"window.focus()" completionHandler:nil];
    while (![webView mainFrame].info._isFocused || [webView firstChildFrame]._isFocused)
        Util::spinRunLoop();
}

TEST(SiteIsolation, EvaluateJavaScriptInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<script>test = 'abc';</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ("abc", [webView objectByEvaluatingJavaScript:@"window.test" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, MainFrameURLAfterFragmentNavigation)
{
    NSString *json = @"["
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"blocked_when_fragment_in_top_url\", \"if-top-url\":[\"fragment\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"always_blocked\", \"if-top-url\":[\"http\"]}}"
    "]";
    __block bool doneRemoving { false };
    [WKContentRuleListStore.defaultStore removeContentRuleListForIdentifier:@"Identifier" completionHandler:^(NSError *error) {
        doneRemoving = true;
    }];
    Util::run(&doneRemoving);
    __block RetainPtr<WKContentRuleList> list;
    [WKContentRuleListStore.defaultStore compileContentRuleListForIdentifier:@"Identifier" encodedContentRuleList:json completionHandler:^(WKContentRuleList *ruleList, NSError *error) {
        list = ruleList;
    }];
    while (!list)
        Util::spinRunLoop();

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/blocked_when_fragment_in_top_url"_s, { "loaded successfully"_s } },
        { "/always_blocked"_s, { "loaded successfully"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView.get().configuration.userContentController addContentRuleList:list.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto canLoadURLInIFrame = [childFrame = RetainPtr { [webView firstChildFrame] }, webView = RetainPtr { webView }] (NSString *path) -> bool {
        __block std::optional<bool> loadedSuccessfully;
        [webView callAsyncJavaScript:[NSString stringWithFormat:@"try { let response = await fetch('%@'); return await response.text() } catch (e) { return 'load failed' }", path] arguments:nil inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
            if ([result isEqualToString:@"loaded successfully"])
                loadedSuccessfully = true;
            else if ([result isEqualToString:@"load failed"])
                loadedSuccessfully = false;
            else
                EXPECT_FALSE(true);
        }];
        while (!loadedSuccessfully)
            Util::spinRunLoop();
        return *loadedSuccessfully;
    };
    EXPECT_TRUE(canLoadURLInIFrame(@"/blocked_when_fragment_in_top_url"));
    EXPECT_FALSE(canLoadURLInIFrame(@"/always_blocked"));

    [webView evaluateJavaScript:@"window.location = '#fragment'" completionHandler:nil];
    while (![webView.get().URL.fragment isEqualToString:@"fragment"])
        Util::spinRunLoop();

    EXPECT_FALSE(canLoadURLInIFrame(@"/blocked_when_fragment_in_top_url"));
    EXPECT_FALSE(canLoadURLInIFrame(@"/always_blocked"));
}

TEST(SiteIsolation, LoadRequestOnOpenerWebView)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    [opener.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/webkit"]]];
    [opener.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://apple.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });
}

TEST(SiteIsolation, LoadRequestOnOpenedWebView)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/webkit"]]];
    [opened.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://apple.com"_s } });
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
}

TEST(SiteIsolation, CancelNavigationResponseForLoadRequestOnOpenerWebView)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/apple"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().pageCacheEnabled = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration setProcessPool:processPool.get()];
    enableSiteIsolation(configuration.get());
    configuration.get().preferences.javaScriptCanOpenWindowsAutomatically = YES;

    RetainPtr openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];

    __block RetainPtr<TestWKWebView> opened;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedNavigationDelegate.get();
        return opened.get();
    };

    RetainPtr opener = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    opener.get().navigationDelegate = openerNavigationDelegate.get();
    opener.get().UIDelegate = uiDelegate.get();
    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!opened)
        Util::spinRunLoop();
    [openedNavigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(opener.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    __block pid_t provisionalProcessIdentifier = 0;
    openerNavigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        provisionalProcessIdentifier = [opener _provisionalWebProcessIdentifier];
        completionHandler(WKNavigationResponsePolicyCancel);
    };
    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [openerNavigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(provisionalProcessIdentifier, 0);
    EXPECT_NE(provisionalProcessIdentifier, [opener _webProcessIdentifier]);

    EXPECT_TRUE(Util::waitFor(^{
        return [processPool _processCacheSize] == 1;
    }));
    checkFrameTreesInProcesses(opener.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    openerNavigationDelegate.get().decidePolicyForNavigationResponse = nil;
    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [openerNavigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([opener _webProcessIdentifier], provisionalProcessIdentifier);
    checkFrameTreesInProcesses(opener.get(), { { "https://apple.com"_s } });
    checkFrameTreesInProcesses(opened.get(), { { "https://webkit.org"_s } });
}

TEST(SiteIsolation, FocusOpenedWindow)
{
    auto openerHTML = "<script>"
    "    let w = window.open('https://domain2.com/opened');"
    "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    EXPECT_FALSE([[[opener.webView mainFrame] info] _isFocused]);
    EXPECT_FALSE([[[opened.webView mainFrame] info] _isFocused]);

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];
    while (![[[opened.webView mainFrame] info] _isFocused])
        Util::spinRunLoop();
    EXPECT_FALSE([[[opener.webView mainFrame] info] _isFocused]);
}

TEST(SiteIsolation, FindStringInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_FALSE([[webView findStringAndWait:@"Missing string" withConfiguration:findConfiguration.get()] matchFound]);
}

TEST(SiteIsolation, FindStringInNestedFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_FALSE([[webView findStringAndWait:@"Missing string" withConfiguration:findConfiguration.get()] matchFound]);
}

TEST(SiteIsolation, FindStringSelection)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionWithEmptyFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/empty_subframe'></iframe>"
        "<iframe src='https://domain4.com/subframe'></iframe>"
        "<iframe src='https://domain5.com/empty_subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } },
        { "/empty_subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[2].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionNoWrap)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    findConfiguration.get().wraps = NO;
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        [[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound];
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
    };

    std::array<SelectionOffsets, 3> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionBackwards)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    findConfiguration.get().backwards = YES;
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
}

TEST(SiteIsolation, FindStringSelectionSameOriginFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionNestedFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    auto subframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain4.com/nested_subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 5>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[3].first endOffset:offsets[3].second inFrame:mainFrame.childFrames[0].childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[4].first endOffset:offsets[4].second inFrame:mainFrame.childFrames[1].childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 11 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionMultipleMatchesInMainFrame)
{
    auto mainframeHTML = "<p>Hello world Hello world Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 12, 23 }, { 0, 0 } } },
        { { { 24, 35 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionMultipleMatchesInChildFrame)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world Hello world Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 12, 23 } } },
        { { { 0, 0 }, { 24, 35 } } },
        { { { 0, 11 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionSameOriginFrameBeforeWrap)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain1.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

static void checkFindStringMatchCount(TestWKWebView *webView, TestNavigationDelegate *navigationDelegate)
{
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    // Without a counting option this only reports that the string was found.
    EXPECT_EQ(1ul, [findDelegate matchesCount]);

    isDone = false;
    // The main frame's only match is the one the find above selected, and StartInSelection searches
    // past it, so WrapAround is what finds it again. Without it that process reports no match and
    // the total is 2.
    [webView _findString:@"Hello World" options:_WKFindOptionsCaseInsensitive | _WKFindOptionsWrapAround | _WKFindOptionsDetermineMatchIndex maxCount:100];
    Util::run(&isDone);
    EXPECT_EQ(3ul, [findDelegate matchesCount]);
}

static HTTPServer findStringMatchCountServer()
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    return HTTPServer({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
}

TEST(SiteIsolation, FindStringMatchCount)
{
    auto server = findStringMatchCountServer();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server);
    checkFindStringMatchCount(webView.get(), navigationDelegate.get());
}

TEST(SiteIsolation, FindStringMatchCountWithSharedProcess)
{
    auto server = findStringMatchCountServer();
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    checkFindStringMatchCount(webView.get(), navigationDelegate.get());
}

TEST(SiteIsolation, CountStringMatches)
{
    HTTPServer server({
        { "/mainframe"_s, { "<p>Hello world</p><iframe src='https://webkit.org/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView _countStringMatches:@"Hello world" options:0 maxCount:100];
    while ([findDelegate matchesCount] != 2)
        Util::spinRunLoop();
}

TEST(SiteIsolation, HideFindUIClearsTextMatchMarkersInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<p>Hello world</p><iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWebsiteDataStore:server.httpsProxyConfiguration().websiteDataStore];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto* mainFrameInfo = [webView mainFrame].info;
    RetainPtr childFrame = [webView firstChildFrame];
    auto textMatchMarkerCount = [&](WKFrameInfo *frame) {
        return [[webView objectByEvaluatingJavaScript:@"internals.markerCountForNode(document.querySelector('p').firstChild, 'textmatch')" inFrame:frame] unsignedIntValue];
    };

    [webView _countStringMatches:@"Hello world" options:_WKFindOptionsShowOverlay maxCount:100];
    while ([findDelegate matchesCount] != 2)
        Util::spinRunLoop();
    EXPECT_EQ(1u, textMatchMarkerCount(mainFrameInfo));
    EXPECT_EQ(1u, textMatchMarkerCount(childFrame.get()));

    [webView _hideFindUI];
    EXPECT_TRUE(Util::waitFor([&] {
        return !textMatchMarkerCount(mainFrameInfo) && !textMatchMarkerCount(childFrame.get());
    }));
}

TEST(SiteIsolation, FindStringMatchIndexAcrossFrames)
{
    auto mainframeHTML = "<p>word word</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>word word</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    [webView loadURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findNextWord = [&] {
        isDone = false;
        [webView _findString:@"word" options:_WKFindOptionsWrapAround | _WKFindOptionsDetermineMatchIndex maxCount:100];
        Util::run(&isDone);
    };

    findNextWord();
    EXPECT_EQ(4u, [findDelegate matchesCount]);
    EXPECT_EQ(0, [findDelegate matchIndex]);

    findNextWord();
    EXPECT_EQ(4u, [findDelegate matchesCount]);
    EXPECT_EQ(1, [findDelegate matchIndex]);

    findNextWord();
    EXPECT_EQ(4u, [findDelegate matchesCount]);
    EXPECT_EQ(2, [findDelegate matchIndex]);

    findNextWord();
    EXPECT_EQ(4u, [findDelegate matchesCount]);
    EXPECT_EQ(3, [findDelegate matchIndex]);
}

TEST(SiteIsolation, CountStringMatchesOverflowSaturates)
{
    HTTPServer server({
        { "/mainframe"_s, { "<p>word word word</p><iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>word</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    isDone = false;
    [webView _countStringMatches:@"word" options:0 maxCount:2];
    Util::run(&isDone);

    EXPECT_EQ(static_cast<uint32_t>(kWKMoreThanMaximumMatchCount), [findDelegate matchesCount]);

    isDone = false;
    [webView _countStringMatches:@"word" options:0 maxCount:3];
    Util::run(&isDone);

    EXPECT_EQ(static_cast<uint32_t>(kWKMoreThanMaximumMatchCount), [findDelegate matchesCount]);
}

TEST(SiteIsolation, NavigateOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/webkit2"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView evaluateJavaScript:@"const originalOpener = window.opener;" completionHandler:nil];
    [opened.webView evaluateJavaScript:@"opener.location = '/webkit2'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://webkit.org"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });

    __block bool done { false };
    [opened.webView evaluateJavaScript:@"originalOpener === window.opener" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    Util::run(&done);

    [opened.webView evaluateJavaScript:@"opener.location = '/webkit'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://webkit.org"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });

    done = false;
    [opened.webView evaluateJavaScript:@"originalOpener === window.opener" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, NavigateOpenerWindowCrossSite)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://example.com/text')</script>"_s } },
        { "/text"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example");
    [opened.webView evaluateJavaScript:@"alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opener.webView evaluateJavaScript:@"window.location = 'https://webkit.org/text'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFinishNavigation];

    [opened.webView evaluateJavaScript:@"alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");
}

TEST(SiteIsolation, NavigateOpenedWindowCrossSiteAfterDisowningOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://example.com/text')</script>"_s } },
        { "/text"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example");
    [opened.webView evaluateJavaScript:@"alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    // Opened window disowns opener.
    [opened.webView evaluateJavaScript:@"window.opener = null; alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");

    [opener.webView evaluateJavaScript:@"alert(!!w.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opener.uiDelegate waitForAlert], "false");

    // Opened window performs cross-site navigation.
    [opened.webView evaluateJavaScript:@"window.location = 'https://webkit.org/text'" completionHandler:nil];
    [opened.navigationDelegate waitForDidFinishNavigation];

    [opened.webView evaluateJavaScript:@"alert(!!window.opener)" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");
}

TEST(SiteIsolation, NavigateOpenerToProvisionalNavigationFailure)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://webkit.org/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://example.com/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://apple.com/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });
}

TEST(SiteIsolation, OpenProvisionalFailure)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    [opened.navigationDelegate waitForDidFailProvisionalNavigation];
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://example.com"_s } });
}

TEST(SiteIsolation, NavigateIframeToProvisionalNavigationFailure)
{
    HTTPServer server({
        { "/webkit"_s, { "<iframe id='testiframe' src='https://example.com/example'></iframe>"_s } },
        { "/example"_s, { "hi"_s } },
        { "/redirect_to_example_terminate"_s, { 302, { { "Location"_s, "https://example.com/terminate"_s } }, "redirecting..."_s } },
        { "/redirect_to_webkit_terminate"_s, { 302, { { "Location"_s, "https://webkit.org/terminate"_s } }, "redirecting..."_s } },
        { "/redirect_to_apple_terminate"_s, { 302, { { "Location"_s, "https://apple.com/terminate"_s } }, "redirecting..."_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s } }
        },
    });

    __block bool provisionalLoadFailed { false };
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo, NSError *error) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
        EXPECT_FALSE(frameInfo.isMainFrame);
        provisionalLoadFailed = true;
    };

    __block RetainPtr blockScopeWebView { webView };
    auto checkProvisionalLoadFailure = ^(NSString *url) {
        provisionalLoadFailed = false;
        [blockScopeWebView evaluateJavaScript:[NSString stringWithFormat:@"document.getElementById('testiframe').src = '%@'", url] completionHandler:nil];
        while (!provisionalLoadFailed)
            Util::spinRunLoop();
        checkFrameTreesInProcesses(blockScopeWebView.get(), {
            { "https://webkit.org"_s,
                { { RemoteFrame } }
            }, { RemoteFrame,
                { { "https://example.com"_s } }
            },
        });
    };
    checkProvisionalLoadFailure(@"https://example.com/terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/terminate");
    checkProvisionalLoadFailure(@"https://apple.com/terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_example_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_example_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_example_terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_webkit_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_webkit_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_webkit_terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_apple_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_apple_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_apple_terminate");
}

TEST(SiteIsolation, DrawAfterNavigateToDomainAgain)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/b'></iframe>"_s } },
        { "/b"_s, { "hi"_s } },
        { "/c"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://b.com"_s } }
        }
    });

    [webView evaluateJavaScript:@"window.location = 'https://c.com/c'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    // c.com is unrelated to the back/forward-cached a.com and gets a fresh group, so it is a lone tree here.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://c.com"_s }
    });

    [webView evaluateJavaScript:@"window.location = 'https://a.com/a'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://b.com"_s } }
        }
    });

    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, NavigateToUnrelatedDomainDoesNotShareBCGWithSuspendedPage)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/b'></iframe>"_s } },
        { "/b"_s, { "hi"_s } },
        { "/c"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    while (![webView mainFrame].childFrames.count)
        Util::spinRunLoop();
    EXPECT_WK_STREQ([webView mainFrame].info.securityOrigin.host, "a.com");
    _WKFrameTreeNode *bFrameForA = [webView mainFrame].childFrames.firstObject;
    EXPECT_WK_STREQ(bFrameForA.info.securityOrigin.host, "b.com");
    pid_t bProcessForA = bFrameForA.info._processIdentifier;
    EXPECT_NE(bProcessForA, 0);

    [webView evaluateJavaScript:@"window.location = 'https://c.com/c'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView mainFrame].info.securityOrigin.host, "c.com");

    // a.com's iframe process stays alive in the back/forward cache, but c.com is in a fresh group, so it is a lone
    // tree not linked to that suspended process.
    EXPECT_TRUE(processStillRunning(bProcessForA));
    checkFrameTreesInProcesses(webView.get(), {
        { "https://c.com"_s }
    });
}

TEST(SiteIsolation, CancelProvisionalLoad)
{
    HTTPServer server({
        { "/main"_s, {
            "<iframe id='testiframe' src='https://example.com/respond_quickly'></iframe>"
            "<iframe src='https://example.com/respond_quickly'></iframe>"_s
        } },
        { "/respond_quickly"_s, { "hi"_s } },
        { "/never_respond"_s, { HTTPResponse::Behavior::NeverSendResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    auto checkStateAfterSequentialFrameLoads = [webView = RetainPtr { webView }, navigationDelegate = RetainPtr { navigationDelegate }](NSString *first, NSString *second, Vector<ExpectedFrameTree>&& expectedTrees) {
        [webView evaluateJavaScript:[NSString stringWithFormat:@"i = document.getElementById('testiframe'); i.addEventListener('load', () => { alert('iframe loaded') }); i.src = '%@'; setTimeout(()=>{ i.src = '%@' }, Math.random() * 100)", first, second] completionHandler:nil];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "iframe loaded");
        checkFrameTreesInProcesses(webView.get(), WTF::move(expectedTrees));
    };

    checkStateAfterSequentialFrameLoads(@"https://webkit.org/never_respond", @"https://example.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://example.com/never_respond", @"https://webkit.org/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { "https://webkit.org"_s } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://webkit.org/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { "https://webkit.org"_s } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://example.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://apple.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        }, { RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s } }
        }
    });
}

TEST(SiteIsolation, CancelProvisionalLoadWithSharedProcess)
{
    HTTPServer server({
        { "/main"_s, {
            "<iframe id='testiframe' src='https://example.com/respond_quickly'></iframe>"
            "<iframe src='https://example.com/respond_quickly'></iframe>"_s
        } },
        { "/respond_quickly"_s, { "hi"_s } },
        { "/never_respond"_s, { HTTPResponse::Behavior::NeverSendResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });
    auto sharedProcessPID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);

    [webView evaluateJavaScript:@"i = document.getElementById('testiframe'); i.addEventListener('load', () => { alert('iframe loaded') }); i.src = 'https://apple.com/never_respond'; setTimeout(()=>{ i.src = 'https://apple.com/respond_quickly' }, Math.random() * 100)" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "iframe loaded");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s }, { "https://example.com"_s } }
        },
    });
    EXPECT_EQ(sharedProcessPID, findFramePID(frameTrees(webView.get()).get(), FrameType::Remote));
}

// FIXME: If a provisional load happens in a RemoteFrame with frame children, does anything clear out those
// child frames when the load commits? Probably not. Needs a test.

// FIXME: Add a test that verifies that provisional frames are not accessible via DOMWindow.frames.

// FIXME: Make a test that tries to access its parent that used to be remote during a provisional navigation of
// the parent to that domain to verify that even the main frame uses provisional frames.

TEST(SiteIsolation, OpenThenClose)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<WKWebView> retainOpener;
    @autoreleasepool {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
        retainOpener = opener.webView;
    }
}

TEST(SiteIsolation, CustomUserAgent)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView setCustomUserAgent:@"Custom UserAgent"];
    EXPECT_WK_STREQ(@"Custom UserAgent", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, ApplicationNameForUserAgent)
{
    auto mainframeHTML = "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain3.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex].endsWith(" Custom UserAgent"_s));
                receivedRequestFromSubframe = true;
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _setApplicationNameForUserAgent:@"Custom UserAgent"];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]] hasSuffix:@" Custom UserAgent"]);
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgent)
{
    auto mainframeHTML = "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain3.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    bool firstRequest = true;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                if (firstRequest)
                    EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                else
                    EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent2"_s);
                receivedRequestFromSubframe = true;
                firstRequest = false;
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    EXPECT_WK_STREQ("Custom UserAgent", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);
    EXPECT_WK_STREQ("Custom UserAgent2", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgentDuringCrossSiteProvisionalNavigation)
{
    auto mainframeHTML = "<iframe id='frame' src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain2.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                receivedRequestFromSubframe = true;
                continue;
            }
            if (path == "/missing"_s)
                continue;
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *webView, WKNavigation *) {
        [webView evaluateJavaScript:@"document.getElementById('frame').src = 'https://domain4.com/subframe';" completionHandler:nil];
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/missing"]]];
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, WebsitePoliciesCustomNavigatorPlatform)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomNavigatorPlatform:@"Custom Navigator Platform"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ("Custom Navigator Platform", [webView objectByEvaluatingJavaScript:@"navigator.platform" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, LoadHTMLString)
{
    HTTPServer server({
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    NSString *html = @"<iframe src='https://webkit.org/webkit'></iframe>";
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com"]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgentDuringSameSiteProvisionalNavigation)
{
    auto mainframeHTML = "<iframe id='frame' src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain2.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                receivedRequestFromSubframe = true;
                continue;
            }
            if (path == "/missing"_s)
                continue;
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *webView, WKNavigation *) {
        [webView evaluateJavaScript:@"document.getElementById('frame').src = 'https://domain3.com/subframe';" completionHandler:nil];
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/missing"]]];
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, ProvisionalLoadFailureOnCrossSiteRedirect)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { ""_s } },
        { "/redirect"_s, { 302, { { "Location"_s, "https://example.com/terminate"_s } }, "redirecting..."_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *request, WKFrameInfo *, NSError *) {
        EXPECT_WK_STREQ(request.URL.absoluteString, "https://example.com/terminate");
        done = true;
    };
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/redirect'" inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    Util::run(&done);
}

TEST(SiteIsolation, SynchronouslyExecuteEditCommandSelectAll)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe' src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "test"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();
}

TEST(SiteIsolation, PresentationUpdateAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/source"_s, { "<script> location.href = 'https://webkit.org/destination'; </script>"_s } },
        { "/destination"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CanGoBackAfterLoadingAndNavigatingFrame)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='frame' src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { ""_s } },
        { "/destination"_s, { "<script> alert('done'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView canGoBack]);

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    EXPECT_TRUE([webView canGoBack]);
}

TEST(SiteIsolation, CanGoBackAfterNavigatingFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='frame' src='https://domain1.com/source'></iframe>"_s } },
        { "/source"_s, { ""_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"location.href = 'https://domain2.com/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");
    EXPECT_TRUE([webView canGoBack]);
}

TEST(SiteIsolation, RestoreSessionFromAnotherWebView)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "<script> alert('done'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView1, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "done");

    auto [webView2, navigationDelegate2] = siteIsolatedViewAndDelegate(server);
    [webView2 _restoreSessionState:[webView1 _sessionState] andNavigate:YES];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "done");
}

enum class SessionRestoreMethod : uint8_t { None, InPlace, NewWebView };

static void testNavigateIframeBackForward(NSString *navigationURL, SessionRestoreMethod restoreMethod)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "source");

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");

    switch (restoreMethod) {
    case SessionRestoreMethod::None:
        break;
    case SessionRestoreMethod::InPlace:
        [webView _restoreSessionState:[webView _sessionState] andNavigate:NO];
        break;
    case SessionRestoreMethod::NewWebView: {
        RetainPtr sessionState = [webView _sessionState];
        auto [newWebView, newNavigationDelegate] = siteIsolatedViewAndDelegate(server);
        [newWebView _restoreSessionState:sessionState.get() andNavigate:YES];
        EXPECT_WK_STREQ([newWebView _test_waitForAlert], "destination");
        webView = WTF::move(newWebView);
        navigationDelegate = WTF::move(newNavigationDelegate);
        break;
    }
    }

    childFrame = [webView firstChildFrame];

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);

    [webView goForward];
    EXPECT_WK_STREQ("destination", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ(navigationURL, [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);
}

TEST(SiteIsolation, NavigateIframeSameOriginBackForward)
{
    testNavigateIframeBackForward(@"https://webkit.org/destination", SessionRestoreMethod::None);
}

TEST(SiteIsolation, NavigateIframeSameOriginBackForwardAfterSessionRestore)
{
    testNavigateIframeBackForward(@"https://webkit.org/destination", SessionRestoreMethod::InPlace);
}

TEST(SiteIsolation, NavigateIframeSameOriginBackForwardAfterSessionRestoreToNewWebView)
{
    testNavigateIframeBackForward(@"https://webkit.org/destination", SessionRestoreMethod::NewWebView);
}

TEST(SiteIsolation, NavigateIframeCrossOriginBackForward)
{
    testNavigateIframeBackForward(@"https://apple.com/destination", SessionRestoreMethod::None);
}

TEST(SiteIsolation, NavigateIframeCrossOriginBackForwardAfterSessionRestore)
{
    testNavigateIframeBackForward(@"https://apple.com/destination", SessionRestoreMethod::InPlace);
}

TEST(SiteIsolation, NavigateIframeCrossOriginBackForwardAfterSessionRestoreToNewWebView)
{
    testNavigateIframeBackForward(@"https://apple.com/destination", SessionRestoreMethod::NewWebView);
}

static void testCrossSiteIframeBackForwardEntryThenMainFrameBack(bool siteIsolationEnabled)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/form'></iframe>"_s } },
        { "/form"_s, { "<script>alert('form')</script><form method='GET' action='https://webkit.org/form'><input name='q' value='hello'></form>"_s } },
        { "/form?q=hello"_s, { "<script>alert('result')</script><p>result q=hello</p>"_s } },
        { "/page2"_s, { "<script>alert('page2')</script><p>page2</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolationEnabled ? siteIsolatedViewAndDelegate(server) : viewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ("form", [webView _test_waitForAlert]);

    // The cross-site iframe submits a GET form, creating a back/forward entry in the iframe's process.
    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.forms[0].submit()" inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ("result", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/form?q=hello", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]]);

    // The main frame navigates away, stacking a main-frame entry on top of the iframe entry.
    [webView evaluateJavaScript:@"location.href = 'https://example.com/page2'" completionHandler:nil];
    EXPECT_WK_STREQ("page2", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://example.com/page2", [webView objectByEvaluatingJavaScript:@"location.href"]);

    // Poll instead of waiting for an alert so the bug (a no-op Back) fails fast rather than hanging.
    [webView goBack];
    RetainPtr<NSString> mainURL;
    for (int i = 0; i < 50; i++) {
        mainURL = [webView objectByEvaluatingJavaScript:@"location.href"];
        if ([mainURL isEqualToString:@"https://example.com/example"])
            break;
        Util::runFor(0.1_s);
    }
    EXPECT_WK_STREQ("https://example.com/example", mainURL.get());
    EXPECT_WK_STREQ("https://webkit.org/form?q=hello", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, CrossSiteIframeBackForwardEntryThenMainFrameBackTraverses)
{
    testCrossSiteIframeBackForwardEntryThenMainFrameBack(true);
}

TEST(SiteIsolation, CrossSiteIframeBackForwardEntryThenMainFrameBackTraversesWithoutSiteIsolation)
{
    testCrossSiteIframeBackForwardEntryThenMainFrameBack(false);
}

TEST(SiteIsolation, NestedCrossSiteIframeBackForwardEntryThenMainFrameBackTraverses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/original'></iframe>"_s } },
        { "/original"_s, { "<script>alert('original')</script>"_s } },
        { "/navigated"_s, { "<script>alert('navigated')</script>"_s } },
        { "/page2"_s, { "<script>alert('page2')</script><p>page2</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    // Without this, goBack resurrects the suspended page instead of rebuilding the frame tree.
    RetainPtr configuration = server.httpsProxyConfiguration();
    configuration.get().processPool = processPoolWithBackForwardCacheDisabled().get();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ("original", [webView _test_waitForAlert]);

    RetainPtr<_WKFrameTreeNode> nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/navigated'" inFrame:[nestedChildFrame info] completionHandler:nil];
    EXPECT_WK_STREQ("navigated", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://a.com/navigated", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[nestedChildFrame info]]);

    [webView evaluateJavaScript:@"location.href = 'https://example.com/page2'" completionHandler:nil];
    EXPECT_WK_STREQ("page2", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://example.com/page2", [webView objectByEvaluatingJavaScript:@"location.href"]);

    // Poll rather than wait for an alert, so a regression fails fast instead of hanging.
    [webView goBack];
    RetainPtr<NSString> mainURL;
    for (int i = 0; i < 50; i++) {
        mainURL = [webView objectByEvaluatingJavaScript:@"location.href"];
        if ([mainURL isEqualToString:@"https://example.com/example"])
            break;
        Util::runFor(0.1_s);
    }
    EXPECT_WK_STREQ("https://example.com/example", mainURL.get());

    RetainPtr<NSString> nestedChildURL;
    for (int i = 0; i < 50; i++) {
        nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
        nestedChildURL = [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[nestedChildFrame info]];
        if ([nestedChildURL isEqualToString:@"https://a.com/navigated"])
            break;
        Util::runFor(0.1_s);
    }
    EXPECT_WK_STREQ("https://a.com/navigated", nestedChildURL.get());
}

static void testNestedIframeBackForwardAfterSessionRestore(NSString *navigationURL, SessionRestoreMethod restoreMethod)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "source");

    RetainPtr<_WKFrameTreeNode> nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
    [webView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:[nestedChildFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");

    switch (restoreMethod) {
    case SessionRestoreMethod::None:
        break;
    case SessionRestoreMethod::InPlace:
        [webView _restoreSessionState:[webView _sessionState] andNavigate:NO];
        break;
    case SessionRestoreMethod::NewWebView: {
        RetainPtr sessionState = [webView _sessionState];
        auto [newWebView, newNavigationDelegate] = siteIsolatedViewAndDelegate(server);
        [newWebView _restoreSessionState:sessionState.get() andNavigate:YES];
        EXPECT_WK_STREQ([newWebView _test_waitForAlert], "destination");
        webView = WTF::move(newWebView);
        navigationDelegate = WTF::move(newNavigationDelegate);
        break;
    }
    }

    nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://a.com/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[nestedChildFrame info]]);

    [webView goForward];
    EXPECT_WK_STREQ("destination", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ(navigationURL, [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[nestedChildFrame info]]);

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://a.com/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[nestedChildFrame info]]);
}

TEST(SiteIsolation, NestedIframeCrossOriginBackForwardAfterSessionRestore)
{
    testNestedIframeBackForwardAfterSessionRestore(@"https://apple.com/destination", SessionRestoreMethod::InPlace);
}

TEST(SiteIsolation, NestedIframeCrossOriginBackForwardAfterSessionRestoreToNewWebView)
{
    testNestedIframeBackForwardAfterSessionRestore(@"https://apple.com/destination", SessionRestoreMethod::NewWebView);
}

TEST(SiteIsolation, CancelledChildAsyncBackForwardNotifiesParent)
{
    HTTPServer server({
        { "/page0"_s, { "<p>page0</p><iframe src='https://example.com/child0'></iframe><script>window.onload=()=>alert('page0-loaded')</script>"_s } },
        { "/page1"_s, { "<p>page1</p><iframe src='https://example.com/child1'></iframe><script>window.onload=()=>alert('page1-loaded')</script>"_s } },
        { "/child0"_s, { "<p>child0</p>"_s } },
        { "/child1"_s, { "<p>child1</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    // Disabling BFCache so going back will perform a fresh load of page0.
    processPoolConfiguration.get().pageCacheEnabled = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration setProcessPool:processPool.get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/page0"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "page0-loaded");
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/page1"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "page1-loaded");

    __block bool interceptedChildPolicy = false;
    RetainPtr capturedWebView = webView;
    [navigationDelegate setDecidePolicyForNavigationAction:^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (!action.targetFrame.isMainFrame && action.navigationType == WKNavigationTypeBackForward && !interceptedChildPolicy) {
            interceptedChildPolicy = true;
            // Inject a cross-document navigation while the child is Pending.
            [capturedWebView evaluateJavaScript:@"frames[0].location = 'https://example.com/injected'" completionHandler:^(id, NSError *) {
                decisionHandler(WKNavigationActionPolicyAllow);
            }];
            return;
        }
        // Deny the injected navigation so it never commits. This prevents didBeginDocument()
        // from accidentally unblocking the parent.
        if (!action.targetFrame.isMainFrame && interceptedChildPolicy) {
            decisionHandler(WKNavigationActionPolicyCancel);
            return;
        }
        decisionHandler(WKNavigationActionPolicyAllow);
    }];

    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "page0-loaded");
}

TEST(SiteIsolation, ValidateSessionRestoreWithoutNavigating)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [normalView, normalNavigationDelegate] = viewAndDelegate(server);
    auto [isolatedView, isolatedNavigationDelegate] = siteIsolatedViewAndDelegate(server);

    [normalView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([normalView _test_waitForAlert], "source");

    [isolatedView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([isolatedView _test_waitForAlert], "source");

    RetainPtr childFrame = [normalView firstChildFrame];
    NSString *navigationURL = @"https://apple.com/destination";
    [normalView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([normalView _test_waitForAlert], "destination");

    childFrame = [isolatedView firstChildFrame];
    [isolatedView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([isolatedView _test_waitForAlert], "destination");

    RetainPtr normalSessionState = [normalView _sessionState];
    RetainPtr isolatedSessionState = [isolatedView _sessionState];

    // FIXME: These two session states should be equal, but are not.
    // This is because the fidelity of the back/forward list in the UI process is wrong with site isolation on.
    // You should also be able to do a deep comparison of the WKBackForwardListItems here for equality, but cannot for the same reason.
    // Covered by https://bugs.webkit.org/show_bug.cgi?id=300832
    //
    // EXPECT_TRUE([isolatedSessionState isEqualForTesting:normalSessionState.get()]);

    // Meanwhile, we can verify that the the session state generated by a normal web view successfully restores into an isolated view,
    // and that the isolated view will succesfully recreate that same session state.
    [isolatedView _restoreSessionState:normalSessionState.get() andNavigate:NO];
    RetainPtr newIsolatedSessionState = [isolatedView _sessionState];

    EXPECT_TRUE([newIsolatedSessionState isEqualForTesting:normalSessionState.get()]);
}

TEST(SiteIsolation, BackNavigationOverCrossSiteIframeWithoutBFCache)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().pageCacheEnabled = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ("a", [webView _test_waitForAlert]);

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/b'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ("b", [webView _test_waitForAlert]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/c"]]];
    EXPECT_WK_STREQ("c", [webView _test_waitForAlert]);

    [webView goBack];
    EXPECT_WK_STREQ("b", [webView _test_waitForAlert]);
}

TEST(SiteIsolation, ProtocolProcessSeparation)
{
    HTTPServer secureServer({
        { "/subdomain"_s, { "hi"_s } },
        { "/no_subdomain"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer plaintextServer({
        { "http://a.com/"_s, {
            "<iframe src='https://a.com/no_subdomain'></iframe>"
            "<iframe src='https://subdomain.a.com/subdomain'></iframe>"_s
        } }
    });

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer.port()]]];
    [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", plaintextServer.port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://a.com/"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "http://a.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://subdomain.a.com"_s }, { "https://a.com"_s } }
        },
    });
}

TEST(SiteIsolation, GoBackToPageWithIframe)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/b"_s, { ""_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://frame.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));
}

TEST(SiteIsolation, GoBackToPageWithIframeBFCache)
{
    // Same scenario as GoBackToPageWithIframe but with BFCache active. The
    // goBack should restore a.com from BFCache (verified via __bfcacheMarker)
    // rather than reload it. The cross-site frame.com iframe process is
    // suspended across the cross-site navigation and restored alongside a.com,
    // so the post-restore tree matches the pre-BFCache shape (a.com tree +
    // frame.com tree). The cross-site navigation broadcasts b.com's top document
    // sync data to the suspended frame.com iframe process; without re-establishing
    // the authoritative main-frame URL/origin on restore, the iframe's first
    // party for cookies resolves to b.com, the NetworkProcess denies cookie
    // access, and the iframe process is terminated. The test reads the iframe's
    // marker and document.cookie after restore to confirm the iframe document
    // survived and its first-party-for-cookies access still works.
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/b"_s, { ""_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];
    // Mark the cross-site frame.com iframe document before navigating away. After restore we read
    // it back from the iframe scope: the marker proves the iframe document survived in BFCache
    // (rather than being reloaded), which requires the iframe process to still be alive.
    [webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker = true" inFrame:[webView firstChildFrame]];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://frame.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
    // The iframe document was restored from BFCache (not reloaded), so its marker is still set.
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView firstChildFrame]] boolValue]);
    // Reading document.cookie from the iframe scope exercises the first-party-for-cookies path that
    // previously terminated the restored iframe process. With the fix the iframe's first party
    // resolves to the a.com main frame, so the NetworkProcess allows the access and the read
    // returns a (non-nil) value instead of terminating the process.
    EXPECT_NOT_NULL([webView objectByEvaluatingJavaScript:@"String(document.cookie)" inFrame:[webView firstChildFrame]]);
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));
}

TEST(SiteIsolation, BFCacheRestoredIframeIsVisibleAndFiresPageShow)
{
    auto iframeHTML = "<script>"
        "  window.__pageshowCount = 0;"
        "  window.__pageshowPersisted = null;"
        "  window.addEventListener('pageshow', (event) => {"
        "    window.__pageshowCount++;"
        "    window.__pageshowPersisted = event.persisted;"
        "  });"
        "</script>"_s;
    auto mainHTML = "<script>"
        "  window.__pageshowCount = 0;"
        "  window.addEventListener('pageshow', () => { window.__pageshowCount++ });"
        "</script>"
        "<iframe src='https://frame.com/frame'></iframe>"_s;

    HTTPServer server({
        { "/a"_s, { mainHTML } },
        { "/b"_s, { ""_s } },
        { "/frame"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    [webView objectByEvaluatingJavaScript:@"window.__marker = true" inFrame:[webView firstChildFrame]];
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"window.__pageshowCount" inFrame:[webView firstChildFrame]] intValue]);
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.visibilityState" inFrame:[webView firstChildFrame]], "visible");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://frame.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__marker ? true : false" inFrame:[webView firstChildFrame]] boolValue]);

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.__pageshowCount" inFrame:[webView firstChildFrame]] intValue] == 2;
    }));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__pageshowPersisted === true" inFrame:[webView firstChildFrame]] boolValue]);

    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.visibilityState" inFrame:[webView firstChildFrame]], "visible");
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"document.hidden" inFrame:[webView firstChildFrame]] boolValue]);

    EXPECT_EQ(2, [[webView objectByEvaluatingJavaScript:@"window.__pageshowCount"] intValue]);
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.visibilityState"], "visible");
}

TEST(SiteIsolation, BFCacheSameSitePageChangesTopDocumentURL)
{
    HTTPServer server({
        { "/withframe"_s, { "<iframe src='https://b.com/text'></iframe>"_s } },
        { "/text"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/withframe"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    checkProcessesTopDocumentURL(frameTrees(webView.get()).get(), @"https://a.com/withframe", @"https://a.com/withframe");

    // Same-site navigation reuses the main frame process.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/text"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // The live a.com/text page has no subframe of its own, but the cached b.com iframe process
    // stays in the (unswapped, same-site) BrowsingContextGroup as a childless remote-rooted tree.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { } },
        { RemoteFrame, { } },
    });

    checkTopDocumentURLsInBackForwardCacheAtIndex(webView.get(), -1, 2, @"https://a.com/text");
}

TEST(SiteIsolation, BFCacheCrossSitePageKeepsTopDocumentURL)
{
    HTTPServer server({
        { "/withframe"_s, { "<iframe src='https://b.com/text'></iframe>"_s } },
        { "/text"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/withframe"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    checkProcessesTopDocumentURL(frameTrees(webView.get()).get(), @"https://a.com/withframe", @"https://a.com/withframe");

    // Perform cross-site navigation that swaps process.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://c.com/text"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkTopDocumentURLsInBackForwardCacheAtIndex(webView.get(), -1, 2, @"https://a.com/withframe");
}

TEST(SiteIsolation, NavigateNestedIframeSameOriginBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://a.com/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    RetainPtr<WKFrameInfo> childFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/b'" inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, GoBackToNestedIframeCreatedAfterNavigatingSibling)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    auto createIframe = @"var iframe = document.createElement('iframe');"
        "iframe.src = 'https://apple.com/c';"
        "document.body.appendChild(iframe);";
    [webView evaluateJavaScript:createIframe completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/b'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");

    [webView evaluateJavaScript:createIframe inFrame:[webView secondChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");

    RetainPtr<WKFrameInfo> nestedChildFrame = [webView mainFrame].childFrames[1].childFrames.firstObject.info;
    [webView evaluateJavaScript:@"location.href = 'https://apple.com/a'" inFrame:nestedChildFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
}

TEST(SiteIsolation, GoBackReloadsDynamicallyCreatedCrossSiteIframe)
{
    HTTPServer server({
        { "/example"_s, { "<body><script>"
            "var reloaded = sessionStorage.getItem('loaded');"
            "sessionStorage.setItem('loaded', '1');"
            "var iframe = document.createElement('iframe');"
            "iframe.name = reloaded ? 'frame2' : 'frame1';"
            "iframe.src = reloaded ? 'https://webkit.org/a2' : 'https://webkit.org/a';"
            "document.body.appendChild(iframe);"
            "</script></body>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/a2"_s, { "<script> alert('a2'); </script>"_s } },
        { "/b"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto *configuration = server.httpsProxyConfiguration();
    configuration.processPool = processPoolWithBackForwardCacheDisabled().get();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ("a", [webView _test_waitForAlert]);
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    EXPECT_WK_STREQ("a2", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/a2", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]]);
}

static void testGoBackToCrossSiteIframeCommittedAfterSameSiteSibling(SessionRestoreMethod restoreMethod)
{
    auto mainHTML = "<script>"
        "let messages = [];"
        "onmessage = (event) => {"
        "    let index = [0, 1].find((i) => event.source === frames[i]) ?? '?';"
        "    messages.push(index + ':' + event.data);"
        "    if (messages.length == 2)"
        "        alert(messages.sort().join(' '));"
        "};"
        "</script>"
        "<iframe src='https://webkit.org/cross'></iframe>"
        "<iframe src='https://example.com/same'></iframe>"_s;
    auto subframeHTML = "<script>parent.postMessage(location.href, '*')</script>"_s;

    bool sameSiteIframeCommitted = false;
    std::optional<Connection> delayedCrossSiteConnection;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/main"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainHTML).serialize());
                continue;
            }
            if (path == "/cross"_s) {
                // Make the cross-site iframe commit after its later same-site sibling, so
                // the UI process adds their back/forward items in reverse frame tree order.
                if (!sameSiteIframeCommitted) {
                    delayedCrossSiteConnection = connection;
                    continue;
                }
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/same"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/other"_s) {
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration setProcessPool:processPoolWithBackForwardCacheDisabled().get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    navigationDelegate.get().didCommitLoadWithRequestInFrame = makeBlockPtr([&](WKWebView *, NSURLRequest *request, WKFrameInfo *) {
        if (![request.URL.absoluteString isEqualToString:@"https://example.com/same"])
            return;
        sameSiteIframeCommitted = true;
        if (auto connection = std::exchange(delayedCrossSiteConnection, std::nullopt))
            connection->send(HTTPResponse(subframeHTML).serialize());
    }).get();

    // The main frame can finish loading before or after the alert, so listen for it before loading.
    __block bool finishedLoadingMainPage = false;
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedLoadingMainPage = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    EXPECT_WK_STREQ("0:https://webkit.org/cross 1:https://example.com/same", [webView _test_waitForAlert]);
    Util::run(&finishedLoadingMainPage);
    navigationDelegate.get().didFinishNavigation = nil;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/other"]]];
    [navigationDelegate waitForDidFinishNavigation];

    if (restoreMethod == SessionRestoreMethod::NewWebView) {
        RetainPtr sessionState = [webView _sessionState];
        auto [newWebView, newNavigationDelegate] = siteIsolatedViewAndDelegate(configuration);
        [newWebView _restoreSessionState:sessionState.get() andNavigate:YES];
        [newNavigationDelegate waitForDidFinishNavigation];
        webView = WTF::move(newWebView);
        navigationDelegate = WTF::move(newNavigationDelegate);
    }

    [webView goBack];
    EXPECT_WK_STREQ("0:https://webkit.org/cross 1:https://example.com/same", [webView _test_waitForAlert]);

    RetainPtr mainFrame = [webView mainFrame];
    pid_t mainFramePID = [mainFrame info]._processIdentifier;
    EXPECT_WK_STREQ("webkit.org", [mainFrame childFrames][0].info.securityOrigin.host);
    EXPECT_NE(mainFramePID, [mainFrame childFrames][0].info._processIdentifier);
    EXPECT_WK_STREQ("example.com", [mainFrame childFrames][1].info.securityOrigin.host);
    EXPECT_EQ(mainFramePID, [mainFrame childFrames][1].info._processIdentifier);
}

TEST(SiteIsolation, GoBackToCrossSiteIframeCommittedAfterSameSiteSibling)
{
    testGoBackToCrossSiteIframeCommittedAfterSameSiteSibling(SessionRestoreMethod::None);
}

TEST(SiteIsolation, GoBackToCrossSiteIframeCommittedAfterSameSiteSiblingAfterSessionRestoreToNewWebView)
{
    testGoBackToCrossSiteIframeCommittedAfterSameSiteSibling(SessionRestoreMethod::NewWebView);
}

TEST(SiteIsolation, GoBackToCrossSiteIframeAfterPersistedSessionRestore)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "source");

    [webView evaluateJavaScript:@"location.href = 'https://apple.com/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");

    RetainPtr sessionState = [webView _sessionState];
    RetainPtr persistedSessionState = adoptNS([[_WKSessionState alloc] initWithData:[sessionState data]]);

    auto [newWebView, newNavigationDelegate] = siteIsolatedViewAndDelegate(server);
    [newWebView _restoreSessionState:persistedSessionState.get() andNavigate:YES];
    EXPECT_WK_STREQ([newWebView _test_waitForAlert], "destination");

    RetainPtr childFrame = [newWebView firstChildFrame];

    [newWebView goBack];
    EXPECT_WK_STREQ("source", [newWebView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/source", [newWebView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);

    [newWebView goForward];
    EXPECT_WK_STREQ("destination", [newWebView _test_waitForAlert]);
}

TEST(SiteIsolation, GoBackToNestedCrossSiteIframeAfterPersistedSessionRestore)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "source");

    RetainPtr<_WKFrameTreeNode> nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
    [webView evaluateJavaScript:@"location.href = 'https://apple.com/destination'" inFrame:[nestedChildFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");

    RetainPtr sessionState = [webView _sessionState];
    RetainPtr persistedSessionState = adoptNS([[_WKSessionState alloc] initWithData:[sessionState data]]);

    auto [newWebView, newNavigationDelegate] = siteIsolatedViewAndDelegate(server);
    [newWebView _restoreSessionState:persistedSessionState.get() andNavigate:YES];
    EXPECT_WK_STREQ([newWebView _test_waitForAlert], "destination");

    nestedChildFrame = [newWebView mainFrame].childFrames.firstObject.childFrames.firstObject;

    [newWebView goBack];
    EXPECT_WK_STREQ("source", [newWebView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://a.com/source", [newWebView objectByEvaluatingJavaScript:@"location.href" inFrame:[nestedChildFrame info]]);

    [newWebView goForward];
    EXPECT_WK_STREQ("destination", [newWebView _test_waitForAlert]);
}

TEST(SiteIsolation, AdvancedPrivacyProtectionsHideScreenMetricsFromBindings)
{
    auto frameHTML = [NSString stringWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"simple" ofType:@"html"] encoding:NSUTF8StringEncoding error:NULL];
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { frameHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry | _WKWebsiteNetworkConnectionIntegrityPolicyEnabled];
    [configuration setDefaultWebpagePreferences:preferences.get()];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenX" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenY" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availLeft" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availTop" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, UpdateWebpagePreferences)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setCustomUserAgent:@"Custom UserAgent"];
    [webView _updateWebpagePreferences:preferences.get()];
    while (![[webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]] isEqualToString:@"Custom UserAgent"])
        Util::spinRunLoop();
}

TEST(SiteIsolation, MainFrameRedirectBetweenExistingProcesses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/webkit_redirect"_s, { 302, { { "Location"_s, "https://example.com/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.length"] intValue], 1);
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org/webkit_redirect"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.length"] intValue], 0);
    EXPECT_EQ([webView _webProcessIdentifier], pidBefore);
}

TEST(SiteIsolation, URLSchemeTask)
{
    HTTPServer server({
        { "/example"_s, { ""_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/example"])
            respond(task, "<iframe src='customscheme://webkit.org/webkit'></iframe>");
        else if ([task.request.URL.path isEqualToString:@"/webkit"]) {
            respond(task, "<script>"
                "var xhr = new XMLHttpRequest();"
                "xhr.open('GET', '/fetched');"
                "xhr.onreadystatechange = function () {"
                    "if (xhr.readyState == xhr.DONE) { alert(xhr.responseURL + ' ' + xhr.responseText) }"
                "};"
                "xhr.send();"
            "</script>");
        } else if ([task.request.URL.path isEqualToString:@"/fetched"]) {
            RetainPtr newRequest = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"customscheme://webkit.org/redirected"]]);
            [(id<WKURLSchemeTaskPrivate>)task _willPerformRedirection:adoptNS([NSURLResponse new]).get() newRequest:newRequest.get() completionHandler:^(NSURLRequest *request) {
                respond(task, "hi");
            }];
        } else
            EXPECT_TRUE(false);
    };
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"customscheme"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "customscheme://webkit.org/redirected hi");
    checkFrameTreesInProcesses(webView.get(), {
        { "customscheme://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "customscheme://webkit.org"_s } }
        },
    });
}

// Wide enough that two processes parking a block each are practically guaranteed overlapping identifier ranges.
constexpr NSUInteger parkedLoadsPerFrame = 20;

static NSString *loadParkingHTML(NSString *pathPrefix, NSUInteger count)
{
    return [NSString stringWithFormat:@"<body><script>"
        "window.results = { };"
        "for (let i = 0; i < %lu; ++i) {"
            "let path = '%@' + i;"
            "let xhr = new XMLHttpRequest();"
            "xhr.open('GET', path);"
            "xhr.onload = function () { window.results[path] = xhr.responseText; };"
            "xhr.send();"
        "}"
        "</script>", static_cast<unsigned long>(count), pathPrefix];
}

// A nil frame targets the main frame without -mainFrame, which needs a reply from every process in the tree.
static NSUInteger resultCount(TestWKWebView *webView, WKFrameInfo *frame)
{
    if (!frame)
        return [[webView objectByEvaluatingJavaScript:@"Object.keys(window.results).length"] unsignedLongValue];
    return [[webView objectByEvaluatingJavaScript:@"Object.keys(window.results).length" inFrame:frame] unsignedLongValue];
}

static bool waitForParkedLoads(TrackingURLSchemeHandler *handler, NSString *pathPrefix, NSUInteger count)
{
    return Util::waitFor([&] {
        return [handler parkedCountForURLPathPrefix:pathPrefix] >= count;
    });
}

static bool waitForParkedLoadsOrDuplicateTask(TrackingURLSchemeHandler *handler, NSString *pathPrefix, NSUInteger count)
{
    return Util::waitFor([&] {
        return [handler parkedCountForURLPathPrefix:pathPrefix] >= count || [handler deliveredSameTaskTwice];
    });
}

static bool waitForStoppedLoads(TrackingURLSchemeHandler *handler, NSString *pathPrefix, NSUInteger count)
{
    return Util::waitFor([&] {
        return [handler stopCountForURLPathPrefix:pathPrefix] >= count;
    });
}

static bool waitForResults(TestWKWebView *webView, WKFrameInfo *frame, NSUInteger count)
{
    return Util::waitFor([&] {
        return resultCount(webView, frame) >= count;
    });
}

static void addCrossSiteIframe(TestWKWebView *webView, NSString *url)
{
    [webView evaluateJavaScript:[NSString stringWithFormat:@"const f = document.createElement('iframe'); f.id = 'child'; f.src = '%@'; document.body.appendChild(f)", url] completionHandler:nil];
}

static RetainPtr<TrackingURLSchemeHandler> parkingSchemeHandler()
{
    RetainPtr handler = adoptNS([TrackingURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(TrackingURLSchemeHandler *trackingHandler, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main"])
            [trackingHandler respond:task text:loadParkingHTML(@"/aparked", parkedLoadsPerFrame).UTF8String mimeType:@"text/html"];
        else if ([path isEqualToString:@"/iframe"])
            [trackingHandler respond:task text:loadParkingHTML(@"/bparked", parkedLoadsPerFrame).UTF8String mimeType:@"text/html"];
        else if ([path hasPrefix:@"/aparked"] || [path hasPrefix:@"/bparked"])
            [trackingHandler park:task];
        else
            EXPECT_TRUE(false);
    };
    return handler;
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> viewAndDelegateWithSchemeHandler(TrackingURLSchemeHandler *handler, bool siteIsolationEnabled)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler forURLScheme:@"customscheme"];
    return siteIsolatedViewAndDelegate(configuration, CGRectZero, siteIsolationEnabled);
}

TEST(SiteIsolation, URLSchemeTaskIdentifierCollisionAcrossProcesses)
{
    RetainPtr handler = parkingSchemeHandler();
    auto [webView, navigationDelegate] = viewAndDelegateWithSchemeHandler(handler.get(), true);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/aparked", parkedLoadsPerFrame));

    addCrossSiteIframe(webView.get(), @"customscheme://webkit.org/iframe");
    EXPECT_TRUE(waitForParkedLoadsOrDuplicateTask(handler.get(), @"/bparked", parkedLoadsPerFrame));

    EXPECT_FALSE([handler deliveredSameTaskTwice]);
    EXPECT_EQ([handler parkedCountForURLPathPrefix:@"/bparked"], parkedLoadsPerFrame);
    EXPECT_NE([webView mainFrame].info._processIdentifier, [webView firstChildFrame]._processIdentifier);

    [handler respondToParkedTasksWithURLPathPrefix:@"/aparked" text:"a-data"];
    [handler respondToParkedTasksWithURLPathPrefix:@"/bparked" text:"b-data"];
    EXPECT_FALSE([handler raisedException]);

    EXPECT_TRUE(waitForResults(webView.get(), nil, parkedLoadsPerFrame));
    EXPECT_TRUE(waitForResults(webView.get(), [webView firstChildFrame], parkedLoadsPerFrame));
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.results['/aparked0']"], "a-data");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.results['/bparked0']" inFrame:[webView firstChildFrame]], "b-data");
}

TEST(SiteIsolation, URLSchemeTaskIdentifierCollisionWithoutSiteIsolation)
{
    RetainPtr handler = parkingSchemeHandler();
    auto [webView, navigationDelegate] = viewAndDelegateWithSchemeHandler(handler.get(), false);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/aparked", parkedLoadsPerFrame));

    addCrossSiteIframe(webView.get(), @"customscheme://webkit.org/iframe");
    EXPECT_TRUE(waitForParkedLoadsOrDuplicateTask(handler.get(), @"/bparked", parkedLoadsPerFrame));

    EXPECT_FALSE([handler deliveredSameTaskTwice]);
    EXPECT_EQ([handler parkedCountForURLPathPrefix:@"/bparked"], parkedLoadsPerFrame);

    [handler respondToParkedTasksWithURLPathPrefix:@"/aparked" text:"a-data"];
    [handler respondToParkedTasksWithURLPathPrefix:@"/bparked" text:"b-data"];
    EXPECT_FALSE([handler raisedException]);
    EXPECT_TRUE(waitForResults(webView.get(), nil, parkedLoadsPerFrame));
}

TEST(SiteIsolation, URLSchemeTaskCancellationDoesNotCrossProcesses)
{
    RetainPtr handler = parkingSchemeHandler();
    auto [webView, navigationDelegate] = viewAndDelegateWithSchemeHandler(handler.get(), true);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/aparked", parkedLoadsPerFrame));

    addCrossSiteIframe(webView.get(), @"customscheme://webkit.org/iframe");
    EXPECT_TRUE(waitForParkedLoadsOrDuplicateTask(handler.get(), @"/bparked", parkedLoadsPerFrame));
    EXPECT_FALSE([handler deliveredSameTaskTwice]);

    [webView objectByEvaluatingJavaScript:@"document.getElementById('child').remove()"];
    EXPECT_TRUE(waitForStoppedLoads(handler.get(), @"/bparked", parkedLoadsPerFrame));

    EXPECT_EQ([handler stopCountForURLPathPrefix:@"/aparked"], 0u);
    EXPECT_EQ([handler parkedCountForURLPathPrefix:@"/aparked"], parkedLoadsPerFrame);

    [handler respondToParkedTasksWithURLPathPrefix:@"/aparked" text:"a-data"];
    EXPECT_FALSE([handler raisedException]);
    EXPECT_TRUE(waitForResults(webView.get(), nil, parkedLoadsPerFrame));
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.results['/aparked0']"], "a-data");
}

TEST(SiteIsolation, SynchronousURLSchemeTaskFromCrossSiteIframe)
{
    RetainPtr handler = adoptNS([TrackingURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(TrackingURLSchemeHandler *trackingHandler, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main"])
            [trackingHandler respond:task text:loadParkingHTML(@"/aparked", parkedLoadsPerFrame).UTF8String mimeType:@"text/html"];
        else if ([path isEqualToString:@"/iframe"]) {
            [trackingHandler respond:task text:"<script>"
                "let xhr = new XMLHttpRequest();"
                "xhr.open('GET', '/syncsubresource', false);"
                "try { xhr.send(null); alert('sync:' + xhr.responseText); }"
                "catch (e) { alert('sync-failed:' + e); }"
                "</script>" mimeType:@"text/html"];
        } else if ([path isEqualToString:@"/syncsubresource"])
            [trackingHandler respond:task text:"sync-data" mimeType:@"text/plain"];
        else if ([path hasPrefix:@"/aparked"])
            [trackingHandler park:task];
        else
            EXPECT_TRUE(false);
    };
    auto [webView, navigationDelegate] = viewAndDelegateWithSchemeHandler(handler.get(), true);
    RetainPtr alertRecorder = adoptNS([BoundedAlertRecorder new]);
    webView.get().UIDelegate = alertRecorder.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/aparked", parkedLoadsPerFrame));

    addCrossSiteIframe(webView.get(), @"customscheme://webkit.org/iframe");

    EXPECT_WK_STREQ([alertRecorder waitForAlert], "sync:sync-data");
    EXPECT_FALSE([handler deliveredSameTaskTwice]);
    EXPECT_FALSE([handler raisedException]);
    EXPECT_EQ([handler stopCountForURLPathPrefix:@"/aparked"], 0u);
    EXPECT_EQ([handler parkedCountForURLPathPrefix:@"/aparked"], parkedLoadsPerFrame);

    [handler respondToParkedTasksWithURLPathPrefix:@"/aparked" text:"a-data"];
    EXPECT_FALSE([handler raisedException]);
    EXPECT_TRUE(waitForResults(webView.get(), nil, parkedLoadsPerFrame));
    for (NSUInteger i = 0; i < parkedLoadsPerFrame; ++i) {
        NSString *script = [NSString stringWithFormat:@"window.results['/aparked%lu']", static_cast<unsigned long>(i)];
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:script], "a-data");
    }
}

TEST(SiteIsolation, URLSchemeTasksStoppedWhenIframeProcessTerminates)
{
    RetainPtr handler = adoptNS([TrackingURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(TrackingURLSchemeHandler *trackingHandler, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main"])
            [trackingHandler respond:task text:loadParkingHTML(@"/aparked", 1).UTF8String mimeType:@"text/html"];
        else if ([path isEqualToString:@"/iframe"])
            [trackingHandler respond:task text:loadParkingHTML(@"/bparked", 1).UTF8String mimeType:@"text/html"];
        else if ([path hasPrefix:@"/aparked"] || [path hasPrefix:@"/bparked"])
            [trackingHandler park:task];
        else
            EXPECT_TRUE(false);
    };
    auto [webView, navigationDelegate] = viewAndDelegateWithSchemeHandler(handler.get(), true);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/aparked", 1));

    addCrossSiteIframe(webView.get(), @"customscheme://webkit.org/iframe");
    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/bparked", 1));

    // Query the frame tree before the kill; -mainFrame waits for a reply from every process with no timeout.
    pid_t mainFramePID = [webView mainFrame].info._processIdentifier;
    pid_t iframePID = [webView firstChildFrame]._processIdentifier;
    ASSERT_NE(iframePID, 0);
    ASSERT_NE(iframePID, mainFramePID);

    kill(iframePID, 9);

    EXPECT_TRUE(waitForStoppedLoads(handler.get(), @"/bparked", 1));

    EXPECT_EQ([handler stopCountForURLPathPrefix:@"/aparked"], 0u);
    EXPECT_EQ([handler parkedCountForURLPathPrefix:@"/aparked"], 1u);
    [handler respondToParkedTasksWithURLPathPrefix:@"/aparked" text:"a-data"];
    EXPECT_FALSE([handler raisedException]);
}

TEST(SiteIsolation, URLSchemeTaskRedirectFromCrossSiteIframeWithCollidingIdentifiers)
{
    RetainPtr handler = adoptNS([TrackingURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(TrackingURLSchemeHandler *trackingHandler, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main"])
            [trackingHandler respond:task text:loadParkingHTML(@"/aparked", parkedLoadsPerFrame).UTF8String mimeType:@"text/html"];
        else if ([path isEqualToString:@"/iframe"]) {
            [trackingHandler respond:task text:"<script>"
                "let xhr = new XMLHttpRequest();"
                "xhr.open('GET', '/beforeredirect');"
                "xhr.onload = function () { alert(xhr.responseURL + ' ' + xhr.responseText); };"
                "xhr.send();"
                "</script>" mimeType:@"text/html"];
        } else if ([path isEqualToString:@"/beforeredirect"]) {
            RetainPtr newRequest = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"customscheme://webkit.org/afterredirect"]]);
            [(id<WKURLSchemeTaskPrivate>)task _willPerformRedirection:adoptNS([NSURLResponse new]).get() newRequest:newRequest.get() completionHandler:^(NSURLRequest *) {
                [trackingHandler respond:task text:"redirected-data" mimeType:@"text/plain"];
            }];
        } else if ([path hasPrefix:@"/aparked"])
            [trackingHandler park:task];
        else
            EXPECT_TRUE(false);
    };
    auto [webView, navigationDelegate] = viewAndDelegateWithSchemeHandler(handler.get(), true);
    RetainPtr alertRecorder = adoptNS([BoundedAlertRecorder new]);
    webView.get().UIDelegate = alertRecorder.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForParkedLoads(handler.get(), @"/aparked", parkedLoadsPerFrame));

    addCrossSiteIframe(webView.get(), @"customscheme://webkit.org/iframe");

    EXPECT_WK_STREQ([alertRecorder waitForAlert], "customscheme://webkit.org/afterredirect redirected-data");
    EXPECT_FALSE([handler deliveredSameTaskTwice]);
    EXPECT_FALSE([handler raisedException]);
}

TEST(SiteIsolation, StorageSiteValidationCustomScheme)
{
    HTTPServer server({
        { "/main"_s, { ""_s } },
        { "/iframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main"])
            respond(task, "<iframe src='customscheme://webkit.org/iframe'></iframe>");
        else if ([task.request.URL.path isEqualToString:@"/iframe"])
            respond(task, "<script>sessionStorage.setItem('key', 'value'); alert(sessionStorage.getItem('key'))</script>");
        else
            EXPECT_TRUE(false);
    };
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"customscheme"];
    [[configuration websiteDataStore] _setStorageSiteValidationEnabled:YES];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/main"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "value");
}

TEST(SiteIsolation, ThemeColor)
{
    HTTPServer server({
        { "/example"_s, {
            "<style> html { background-color: blue } </style>"
            "<meta name='theme-color' content='red'><iframe src='https://webkit.org/webkit'></iframe>"_s
        } },
        { "/webkit"_s, {
            "<style> html { background-color: red } </style>"
            "<meta name='theme-color' content='blue'>"_s
        } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server);
    EXPECT_FALSE([webView themeColor]);
    EXPECT_TRUE([webView underPageBackgroundColor]);

    __block bool observedThemeColor { false };
    __block bool observedUnderPageBackgroundColor { false };
    RetainPtr observer = adoptNS([TestObserver new]);
    observer.get().observeValueForKeyPath = ^(NSString *path, id view) {
        RetainPtr sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        if ([path isEqualToString:@"themeColor"]) {
            RetainPtr redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
            EXPECT_TRUE(CGColorEqualToColor([[view themeColor] CGColor], redColor.get()));
            observedThemeColor = true;
        } else {
            EXPECT_WK_STREQ(path, "underPageBackgroundColor");
            RetainPtr blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));
            EXPECT_TRUE(CGColorEqualToColor([[view underPageBackgroundColor] CGColor], blueColor.get()));
            observedUnderPageBackgroundColor = true;
        }
    };
    [webView.get() addObserver:observer.get() forKeyPath:@"themeColor" options:NSKeyValueObservingOptionNew context:nil];
    [webView.get() addObserver:observer.get() forKeyPath:@"underPageBackgroundColor" options:NSKeyValueObservingOptionNew context:nil];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    Util::run(&observedThemeColor);
    Util::run(&observedUnderPageBackgroundColor);
    Util::runFor(0.1_s);

    [webView.get() removeObserver:observer.get() forKeyPath:@"themeColor"];
    [webView.get() removeObserver:observer.get() forKeyPath:@"underPageBackgroundColor"];
}

static WebViewAndDelegates makeWebViewAndDelegates(HTTPServer& server, bool enable = true)
{
    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, enable);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    return {
        WTF::move(webView),
        WTF::move(messageHandler),
        WTF::move(navigationDelegate),
        WTF::move(uiDelegate)
    };
};

TEST(SiteIsolation, SandboxFlags)
{
    NSString *checkAlertJS = @"alert('alerted');window.open('https://example.com/opened');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    HTTPServer server({
        { "/example"_s, { "<iframe sandbox='allow-scripts' id='testiframe' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/check-when-loaded"_s, { [NSString stringWithFormat:@"<script>onload = ()=>{ %@ }</script>", checkAlertJS] } },
        { "/csp-forbids-alert"_s, { { { "Content-Security-Policy"_s, "sandbox allow-scripts"_s } }, "<script>alert('alerted');window.webkit.messageHandlers.testHandler.postMessage('testHandler')</script>"_s } },
        { "/opened"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    bool receivedMessage { false };
    bool receivedAlert { false };
    bool receivedOpen { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedOpen = false;
    };

    WebViewAndDelegates openedWebViewAndDelegates;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *alert, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    auto returnNilOpenedView = [&](WKWebViewConfiguration *, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        receivedOpen = true;
        return nil;
    };
    auto returnNonNilOpenedView = [&](WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        EXPECT_FALSE(openedWebViewAndDelegates.webView);
        openedWebViewAndDelegates = WebViewAndDelegates {
            adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]),
            nil,
            adoptNS([TestNavigationDelegate new]),
            uiDelegate
        };
        openedWebViewAndDelegates.webView.get().UIDelegate = uiDelegate.get();
        openedWebViewAndDelegates.webView.get().navigationDelegate = openedWebViewAndDelegates.navigationDelegate.get();
        [openedWebViewAndDelegates.navigationDelegate allowAnyTLSCertificate];
        receivedOpen = true;
        return openedWebViewAndDelegates.webView.get();
    };
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.sandbox = 'allow-scripts allow-modals'" completionHandler:^(id, NSError *) {
        [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    }];
    Util::run(&receivedMessage);
    // The second warning of https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox
    // says we shouldn't change the effective sandbox until an iframe navigates.
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://example.org/csp-forbids-alert'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://example.org/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.removeAttribute('sandbox'); i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNonNilOpenedView;
    [webView evaluateJavaScript:@"i.sandbox = 'allow-scripts allow-popups'; i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    while (!openedWebViewAndDelegates.webView)
        Util::spinRunLoop();
    [openedWebViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;
    [openedWebViewAndDelegates.webView evaluateJavaScript:checkAlertJS completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNonNilOpenedView;
    openedWebViewAndDelegates.webView = nil;
    [webView evaluateJavaScript:@"i.sandbox = 'allow-scripts allow-popups allow-popups-to-escape-sandbox'; i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    while (!openedWebViewAndDelegates.webView)
        Util::spinRunLoop();
    [openedWebViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;
    [openedWebViewAndDelegates.webView evaluateJavaScript:checkAlertJS completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);
}

TEST(SiteIsolation, SandboxFlagsDuringNavigation)
{
    bool receivedIframe2Request { false };
    HTTPServer server { HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto path = HTTPServer::parsePath(co_await connection.awaitableReceiveHTTPRequest());
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse("<iframe sandbox='allow-scripts' id='testiframe' src='https://webkit.org/iframe1'></iframe>"_s).serialize());
                continue;
            }
            if (path == "/iframe1"_s) {
                co_await connection.awaitableSend(HTTPResponse("hi"_s).serialize());
                continue;
            }
            if (path == "/iframe2"_s) {
                receivedIframe2Request = true;
                // Never respond.
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy };

    NSString *checkAlertJS = @"alert('alerted');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    bool receivedMessage { false };
    bool receivedAlert { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedIframe2Request = false;
    };

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webViewAndDelegates.uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *alert, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);

    reset();
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.sandbox = 'allow-scripts allow-modals'; i.src='https://webkit.org/iframe2'" completionHandler:nil];
    Util::run(&receivedIframe2Request);
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
}

TEST(SiteIsolation, SandboxFlagsRemovedBeforeSameSiteNavigation)
{
    NSString *checkAlertJS = @"alert('alerted');window.open('https://example.com/opened');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    HTTPServer server({
        { "/example"_s, { "<iframe sandbox='allow-scripts allow-modals' id='testiframe' src='https://webkit.org/iframe1'></iframe>"_s } },
        { "/iframe1"_s, { "hi"_s } },
        { "/iframe2"_s, { [NSString stringWithFormat:@"<script>onload = ()=>{ %@ }</script>", checkAlertJS] } },
        { "/opened"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    bool receivedMessage { false };
    bool receivedAlert { false };
    bool receivedOpen { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedOpen = false;
    };

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        receivedOpen = true;
        return nil;
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    // iframe2 is same-site with iframe1, so the frame stays in the process it is already in and the
    // now-empty sandbox flags have to be delivered by the load itself rather than by frame creation.
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.removeAttribute('sandbox'); i.src = 'https://webkit.org/iframe2'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);
}

TEST(SiteIsolation, NavigateNestedRootFramesBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    RetainPtr<_WKFrameTreeNode> nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/b'" inFrame:[nestedChildFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, NavigateFrameWithSiblingsBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe> <iframe src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { ""_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");

    RetainPtr<_WKFrameTreeNode> secondRootFrame = [webView mainFrame].childFrames[1];
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/c'" inFrame:[secondRootFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
}

TEST(SiteIsolation, IntentionalAboutBlankIframeBackForwardNotSkipped)
{
    // Regression test for the URL-heuristic gap in isStaleInitialAboutBlankIframeTarget
    // (https://bugs.webkit.org/show_bug.cgi?id=317458). A child frame that navigates
    // *intentionally* to about:blank after its first real load produces a legitimate,
    // traversable back/forward entry — distinct from the frame's initial empty document.
    // A URL-string guard would wrongly skip the traversal into that entry; the
    // authoritative isInitialAboutBlank flag lets it through.
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    auto childURLIs = [webView = RetainPtr { webView }](NSString *expected) {
        for (int i = 0; i < 100; ++i) {
            RetainPtr value = [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]];
            if ([value isKindOfClass:[NSString class]] && [(NSString *)value.get() isEqualToString:expected])
                return true;
            TestWebKitAPI::Util::runFor(0.05_s);
        }
        return false;
    };

    // Intentional about:blank navigation in the child frame, after its first real load.
    [webView evaluateJavaScript:@"location.href = 'about:blank'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_TRUE(childURLIs(@"about:blank"));

    // Navigate the child to a real URL so the live frame is on a real document.
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/c'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");

    // Going back must traverse the child back to the intentional about:blank entry,
    // not leave it stranded on /c. With the old URL heuristic this was skipped.
    [webView goBack];
    EXPECT_TRUE(childURLIs(@"about:blank"));
}

TEST(SiteIsolation, RedirectToCSP)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/initial'></iframe>"_s } },
        { "/initial"_s, { 302, { { "Location"_s, "https://example.org/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { { { "Content-Type"_s, "text/html"_s }, { "Content-Security-Policy"_s, "frame-ancestors 'none'"_s } }, "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, IframeWithCSPHeaderForFrameAncestors)
{
    auto html = "<script>"
    "let origins = location.ancestorOrigins;"
    "let array = [];"
    "for (var i = 0; i < origins.length; i = i + 1) { array.push(origins.item(i)); };"
    "alert(array)"
    "</script>"_s;

    HTTPServer server({
        { "/"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { { { "Content-Type"_s, "text/html"_s }, { "Content-Security-Policy"_s, "frame-ancestors https://example.com"_s } }, html } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://example.com");
}

TEST(SiteIsolation, MultipleWebViewsWithSameOpenedConfiguration)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='/iframe'></iframe>"_s } },
        { "/iframe"_s, {
            "<script>onload = () => { document.getElementById('mylink').click() }</script>"
            "<a href='/popup' target='_blank' id='mylink'>link</a>"_s
        } },
        { "/popup"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    RetainPtr webView2 = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:opened.webView.get().configuration]);
    [opened.navigationDelegate waitForDidFinishNavigation];
    [webView2 loadURL:[NSURL URLWithString:@"https://example.com/popup"]];
    [webView2 _test_waitForDidFinishNavigation];
}

TEST(SiteIsolation, RecoverFromCrash)
{
    HTTPServer server({
        { "/crash"_s, { "<script>window.internals.terminateWebContentProcess()</script>"_s } },
        { "/dontcrash"_s, { "hi"_s } },
        { "/iframecrash"_s, { "<iframe src='https://webkit.org/crash'></iframe>"_s } },
        { "/iframedontcrash"_s, { "<iframe src='https://webkit.org/dontcrash'></iframe>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration);

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/crash"]]];
    [navigationDelegate waitForWebContentProcessDidTerminate];
    [webView reload];
    [navigationDelegate waitForWebContentProcessDidTerminate];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/iframecrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/iframedontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, IframeOpener)
{
    auto mainFrameHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('main frame received ' + event.data)"
    "    }, false);"
    "    onload = () => { window.open('https://example.com/iframe', 'myframename') }"
    "</script>"
    "<iframe name='myframename'></iframe>"_s;

    auto iframeHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "    try { window.opener.postMessage('hello', '*') } catch (e) { alert('error ' + e) }"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { mainFrameHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    auto verifyThatOpenerIsParent = [webView = RetainPtr { webView }](bool openerShouldBeParent) {
        auto value = openerShouldBeParent ? "1" : "0";
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.frames[0].opener == self"], value);
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.opener == window.parent" inFrame:[webView firstChildFrame]], value);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "child frame received hello");
    verifyThatOpenerIsParent(false);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);
}

TEST(SiteIsolation, CrossProtocolNavigationWithAboutURL)
{
    HTTPServer secureServer({
        { "/example"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer plaintextServer({
        { "http://example.com/example"_s, { "hi"_s } },
    });

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer.port()]]];
    [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", plaintextServer.port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto processIdentifier1 = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto processIdentifier2 = [webView _webProcessIdentifier];
    EXPECT_EQ(processIdentifier1, processIdentifier2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto processIdentifier3 = [webView _webProcessIdentifier];
    // Process should not be reused as protocols are different.
    EXPECT_NE(processIdentifier2, processIdentifier3);
}

TEST(SiteIsolation, ProcessReuse)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe' id='onlyiframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/iframe_with_alert"_s, { "<script>alert('loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    // The test is about WebProcessCache-driven process reuse; turning off
    // BFCache keeps cached WebPages from inflating countWebPages and
    // confusing the reuse assertions.
    processPoolConfiguration.get().pageCacheEnabled = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView objectByEvaluatingJavaScript:@"var frame = document.getElementById('onlyiframe'); frame.parentNode.removeChild(frame);1"];
    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');iframe.src = 'https://webkit.org/iframe_with_alert';document.body.appendChild(iframe)" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");

    EXPECT_EQ(countWebPages(webView), 2u);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(countWebPages(webView), 2u);
}

TEST(SiteIsolation, ProcessReuseWithBFCache)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe' id='onlyiframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/iframe_with_alert"_s, { "<script>alert('loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // Same as ProcessReuse but with BFCache enabled. BFCache keeps suspended
    // WebPages alive during back/forward navigation, so countWebPages can
    // temporarily exceed 2. After evicting the BFCache the count must drop
    // back to 2, confirming that WebProcessCache reuse still works correctly.
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView objectByEvaluatingJavaScript:@"var frame = document.getElementById('onlyiframe'); frame.parentNode.removeChild(frame);1"];
    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');iframe.src = 'https://webkit.org/iframe_with_alert';document.body.appendChild(iframe)" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Evict BFCache entries so that suspended pages from prior navigations
    // are released before checking the final WebPage count.
    [webView _clearBackForwardCache];
    while (countWebPages(webView) != 2u)
        Util::spinRunLoop();
    EXPECT_EQ(countWebPages(webView), 2u);
}

TEST(SiteIsolation, ProcessTerminationReason)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='onlyiframe' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/iframe2"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateAllowingAllTLS new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(server.totalRequests(), 2u);

    kill([webView firstChildFrame]._processIdentifier, 9);
    Util::runFor(0.1_s);
    EXPECT_EQ(server.totalRequests(), 2u);

    [webView evaluateJavaScript:@"onlyiframe.src='https://webkit.org/iframe2'" completionHandler:nil];
    while (server.totalRequests() < 3u)
        Util::spinRunLoop();

    kill([webView mainFrame].info._processIdentifier, 9);
    [navigationDelegate waitForDidFinishNavigation];
    while (server.totalRequests() < 5u)
        Util::spinRunLoop();
    EXPECT_EQ(server.totalRequests(), 5u);
}

TEST(SiteIsolation, RemoteProcessTerminationAfterDisablingSiteIsolation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    pid_t iframePID = [webView firstChildFrame]._processIdentifier;
    EXPECT_NE(iframePID, 0);
    EXPECT_NE(iframePID, [webView mainFrame].info._processIdentifier);

    // The remote page for the iframe process outlives the preference change, so its
    // termination must not try to broadcast FrameTreeSyncData.
    setFeatureEnabled(configuration, @"SiteIsolationEnabled", false);

    kill(iframePID, 9);
    while (processStillRunning(iframePID))
        Util::spinRunLoop();

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"location.host"], "example.com");
}

#if ENABLE(THREADED_ANIMATIONS)
static NSUInteger arrayCountInJSONString(NSString *json, NSString *key)
{
    if (!json.length)
        return 0;
    id object = [NSJSONSerialization JSONObjectWithData:[json dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
    if (![object isKindOfClass:NSDictionary.class])
        return 0;
    id array = [object objectForKey:key];
    return [array isKindOfClass:NSArray.class] ? [array count] : 0;
}

static NSUInteger animationCountForLayer(TestWKWebView *webView, uint64_t layerID, uint64_t processID)
{
    return arrayCountInJSONString([webView _animationStackForLayerWithID:layerID processID:processID], @"animations");
}

static NSUInteger progressBasedTimelineCount(TestWKWebView *webView, uint64_t scrollingNodeID, uint64_t processID)
{
    return arrayCountInJSONString([webView _progressBasedTimelinesForScrollingNodeID:scrollingNodeID processID:processID], @"timelines");
}

static NSUInteger monotonicTimelineCount(TestWKWebView *webView, uint64_t processID)
{
    return arrayCountInJSONString([webView _monotonicTimelinesForProcessID:processID], @"timelines");
}

TEST(SiteIsolation, RemoteTimelinesAndAnimationsClearedWhenIframeProcessCrashes)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe width='400' height='400' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html>"
            "<style>"
            "body { margin: 0 }"
            "#scroller { width: 200px; height: 200px; overflow: scroll }"
            "#scroller > div { height: 2000px }"
            ".target { width: 100px; height: 100px; background-color: green }"
            "</style>"
            "<div id='scroller'><div></div></div>"
            "<div id='progressTarget' class='target'></div>"
            "<div id='monotonicTarget' class='target'></div>"
            "<script>"
            "document.getElementById('progressTarget').animate({ translate: ['0px', '100px'] }, { timeline: new ScrollTimeline({ source: document.getElementById('scroller') }) });"
            "document.getElementById('monotonicTarget').animate({ opacity: [1, 0] }, { duration: 1000000, iterations: Infinity });"
            "</script>"_s } }
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

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // Query the frame tree before the kill; -mainFrame waits for a reply from every process with no
    // timeout. The subframe load can still be in flight when the main frame finishes navigating.
    pid_t mainFramePID = [webView mainFrame].info._processIdentifier;
    RetainPtr<WKFrameInfo> childFrame;
    EXPECT_TRUE(Util::waitFor([&] {
        childFrame = [webView firstChildFrame];
        return [childFrame _processIdentifier] && [childFrame _processIdentifier] != mainFramePID;
    }));
    pid_t iframePID = [childFrame _processIdentifier];

    // The scroller and the animation targets only acquire a scrolling node and composited layers once
    // the animations have started, so poll until the subframe can report all four identifiers.
    RetainPtr<NSDictionary> identifiers = [webView objectByCallingAsyncFunction:
        @"let progressLayerID = 0;"
        @"let monotonicLayerID = 0;"
        @"let scrollingNodeID = 0;"
        @"let processID = 0;"
        @"for (let i = 0; i < 100 && !processID; ++i) {"
        @"    await new Promise(resolve => setTimeout(resolve, 16));"
        @"    try {"
        @"        const scroller = internals.scrollingNodeIDForNode(document.getElementById('scroller'));"
        @"        progressLayerID = internals.layerIDForElement(document.getElementById('progressTarget'));"
        @"        monotonicLayerID = internals.layerIDForElement(document.getElementById('monotonicTarget'));"
        @"        scrollingNodeID = scroller.nodeIdentifier;"
        @"        processID = scroller.processIdentifier;"
        @"    } catch (e) { }"
        @"}"
        @"return { progressLayerID, monotonicLayerID, scrollingNodeID, processID };"
        withArguments:@{ } inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld];

    uint64_t progressLayerID = [[identifiers objectForKey:@"progressLayerID"] unsignedLongLongValue];
    uint64_t monotonicLayerID = [[identifiers objectForKey:@"monotonicLayerID"] unsignedLongLongValue];
    uint64_t scrollingNodeID = [[identifiers objectForKey:@"scrollingNodeID"] unsignedLongLongValue];
    uint64_t processID = [[identifiers objectForKey:@"processID"] unsignedLongLongValue];
    EXPECT_NE(progressLayerID, 0ull);
    EXPECT_NE(monotonicLayerID, 0ull);
    EXPECT_NE(scrollingNodeID, 0ull);
    EXPECT_NE(processID, 0ull);

    // Nothing below would be meaningful if the UI process never took on the subframe's threaded
    // animations in the first place.
    EXPECT_TRUE(Util::waitFor([&] {
        return progressBasedTimelineCount(webView.get(), scrollingNodeID, processID)
            && monotonicTimelineCount(webView.get(), processID)
            && animationCountForLayer(webView.get(), progressLayerID, processID)
            && animationCountForLayer(webView.get(), monotonicLayerID, processID);
    }));

    kill(iframePID, SIGKILL);
    while (processStillRunning(iframePID))
        Util::spinRunLoop();

    Util::waitFor([&] {
        return !progressBasedTimelineCount(webView.get(), scrollingNodeID, processID)
            && !monotonicTimelineCount(webView.get(), processID)
            && !animationCountForLayer(webView.get(), progressLayerID, processID)
            && !animationCountForLayer(webView.get(), monotonicLayerID, processID);
    });

    // Asserted one at a time so a failure names the state that outlived the crashed process.
    EXPECT_EQ(0u, progressBasedTimelineCount(webView.get(), scrollingNodeID, processID));
    EXPECT_EQ(0u, monotonicTimelineCount(webView.get(), processID));
    EXPECT_EQ(0u, animationCountForLayer(webView.get(), progressLayerID, processID));
    EXPECT_EQ(0u, animationCountForLayer(webView.get(), monotonicLayerID, processID));
}
#endif // ENABLE(THREADED_ANIMATIONS)

TEST(SiteIsolation, FormSubmit)
{
    auto mainHTML = "<script>onload=()=>{onlyform.submit()}</script>"
    "<iframe name='onlyiframe' src='https://webkit.org/iframe'></iframe>"
    "<form action='alert_when_loaded' method='get' target='onlyiframe' id='onlyform'><input type='hidden' name='textname' value='textvalue'>"_s;

    HTTPServer server({
        { "/example"_s, { mainHTML } },
        { "/iframe"_s, { "hi"_s } },
        { "/alert_when_loaded?textname=textvalue"_s, { "<script>alert(window.location.search)</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "?textname=textvalue");
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, ContentRuleListFrameURL)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script>fetch('/example')</script>"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded second iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    __block WKWebViewConfiguration *configuration = [webView configuration];

    __block bool doneClearing { false };
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"Test" completionHandler:^(NSError *error) {
        doneClearing = true;
    }];
    TestWebKitAPI::Util::run(&doneClearing);

    __block bool doneCompiling = false;
    static NSString *filterSource = @"["
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"should_not_match\", \"if-frame-url\":[\"should_not_match\"]}}"
    "]";
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"Test" encodedContentRuleList:filterSource completionHandler:^(WKContentRuleList *ruleList, NSError *error) {
        [configuration.userContentController addContentRuleList:ruleList];
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://webkit.org/alert_when_loaded'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded second iframe");
}

TEST(SiteIsolation, ReuseConfiguration)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();

    auto [webView1, navigationDelegate1] = siteIsolatedViewAndDelegate(configuration);
    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate1 waitForDidFinishNavigation];

    auto [webView2, navigationDelegate2] = siteIsolatedViewAndDelegate(configuration);
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate2 waitForDidFinishNavigation];
}

TEST(SiteIsolation, ReuseConfigurationLoadHTMLString)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    enableSiteIsolation(configuration.get());
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    RetainPtr webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 loadHTMLString:@"hi!" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView1 _test_waitForDidFinishNavigation];

    RetainPtr webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadHTMLString:@"hi!" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView2 _test_waitForDidFinishNavigation];

    EXPECT_NE([webView1 _webProcessIdentifier], [webView2 _webProcessIdentifier]);
}

static void callMethodOnFirstVideoElementInFrame(WKWebView *webView, NSString *methodName, WKFrameInfo *frame)
{
    __block RetainPtr<NSError> error;
    __block bool done = false;

    NSString *source = [NSString stringWithFormat:@"document.getElementsByTagName('video')[0].%@()", methodName];
    [webView callAsyncJavaScript:source arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
        error = callError;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(!!error) << [error description].UTF8String;
}

static void expectPlayingAudio(WKWebView *webView, bool expected, ASCIILiteral reason)
{
    bool success = TestWebKitAPI::Util::waitFor([webView, expected] {
        return [webView _isPlayingAudio] == expected;
    });
    EXPECT_TRUE(success) << reason.characters();
}

// FIXME: Re-enable once the audio session is activated per web process. This GPU-activation change
// still keys activation on the process-global AudioSession::isActive()/m_becameActive latch, so only
// the first playing frame's process activates its audio session; a second frame in another process is
// not independently sustained on iOS after the first pauses. Fixed by the per-process activation
// follow-up: https://bugs.webkit.org/show_bug.cgi?id=320600
TEST(SiteIsolation, DISABLED_PlayAudioInMultipleFrames)
{
    auto mainFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"
    "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"_s;

    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.setHeaderField("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", nil);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in main frame"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in remote frame"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"pause", nil);
    expectPlayingAudio(webView.get(), true, "Should still be playing audio after pausing one of the two frames"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"pause", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), false, "Should not be playing audio after pausing in both frames"_s);
}

TEST(SiteIsolation, PlayAudioInRemoteFrameThenRemove)
{
    auto mainFrameHTML = "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"_s;

    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.setHeaderField("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in main frame"_s);

    __block bool done = false;
    __block RetainPtr<NSError> error;
    [webView evaluateJavaScript:@"document.querySelectorAll('iframe').forEach(iframe => iframe.remove())" completionHandler:^(id result, NSError *scriptError) {
        error = scriptError;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(!!error) << [error description].UTF8String;
    done = false;

    expectPlayingAudio(webView.get(), false, "Should not be playing audio after removing iframe"_s);
}

TEST(SiteIsolation, MutesAndSetsAudioInMultipleFrames)
{
    auto mainFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"
        "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"_s;

    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.setHeaderField("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", nil);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in main frame"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in remote frame"_s);

    auto expectMuted = [&](bool expectedMuted, WKFrameInfo *frame, ASCIILiteral reason) {
        bool success = TestWebKitAPI::Util::waitFor([&] {
            id actuallyMuted = [webView objectByEvaluatingJavaScript:@"window.internals.isEffectivelyMuted(document.getElementsByTagName('video')[0])" inFrame:frame];
            return [actuallyMuted boolValue] == expectedMuted;
        });
        EXPECT_TRUE(success) << reason.characters();
    };

    auto expectMediaVolume = [&](float expectedMediaVolume, WKFrameInfo *frame, ASCIILiteral reason) {
        bool success = TestWebKitAPI::Util::waitFor([&] {
            id actualMediaVolume = [webView objectByEvaluatingJavaScript:@"window.internals.pageMediaVolume()" inFrame:frame];
            return [actualMediaVolume floatValue] == expectedMediaVolume;
        });
        EXPECT_TRUE(success) << reason.characters();
    };

    expectMuted(false, nil, "Should not be muted in main frame"_s);
    expectMuted(false, [webView firstChildFrame], "Should not be muted in remote frame"_s);

    [webView _setPageMuted:_WKMediaAudioMuted];
    [webView _setMediaVolumeForTesting:0.125f];

    expectMuted(true, nil, "Should be muted in main frame"_s);
    expectMediaVolume(0.125f, nil, "Should set volume in main frame"_s);
    expectMuted(true, [webView firstChildFrame], "Should be muted in remote frame"_s);
    expectMediaVolume(0.125f, nil, "Should set volume in remote frame"_s);

    auto addFrameToBody = @""
        "return new Promise((resolve, reject) => {"
        "    let frame = document.createElement('iframe');"
        "    frame.onload = () => resolve(true);"
        "    frame.setAttribute('src', 'https://webkit.org/subframe');"
        "    document.body.appendChild(frame);"
        "})";
    __block RetainPtr<NSError> error;
    __block bool done = false;
    [webView callAsyncJavaScript:addFrameToBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
        error = callError;
        done = true;
    }];
    Util::run(&done);
    EXPECT_FALSE(!!error) << "Failed to add iframe: " << [error description].UTF8String;

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView secondChildFrame]);
    expectMuted(true, [webView secondChildFrame], "Should be muted in newly created remote frame"_s);
    expectMediaVolume(0.125f, [webView secondChildFrame], "Should initialize newly created remote frame with previously set media volume"_s);
}

#if ENABLE(MEDIA_STREAM)

TEST(SiteIsolation, StopsMediaCaptureInRemoteFrame)
{
    auto mainFrameHTML = "<video id='video' controlsplaysinline autoplay></video>"
        "<script>var didStartStream = new Promise(resolve => { video.onplay = resolve; })</script>"
        "<script>var didEndStream = new Promise(resolve => { video.onended = resolve; })</script>"
        "<iframe allow='camera *' src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video id='video' controlsplaysinline autoplay></video>"
        "<script>var didStartStream = new Promise(resolve => { video.onplay = resolve; })</script>"
        "<script>var didEndStream = new Promise(resolve => { video.onended = resolve; })</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration _setMediaCaptureEnabled:YES];

    RetainPtr preferences = [configuration preferences];
    [preferences _setMediaCaptureRequiresSecureConnection:NO];
    [preferences _setMockCaptureDevicesEnabled:YES];
    [preferences _setGetUserMediaRequiresFocus:NO];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectZero, false);
    RetainPtr delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto assertStartCaptureSucceedsInFrameWithPrompt = [&](WKFrameInfo *frame) {
        __block RetainPtr<NSError> error;
        __block bool done = false;

        NSString *source = @"return navigator.mediaDevices.getUserMedia({ audio: false, video: true }).then(stream => { video.srcObject = stream; })";
        [webView callAsyncJavaScript:source arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
            error = callError;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        ASSERT_FALSE(!!error) << "Failed to start capture: " << [error description].UTF8String;

        [delegate waitUntilPrompted];
    };

    // FIXME: the mock media stream doesn't seem to start or stop producing frames for the video
    // element in the sim. This doesn't seem to be related to site isolation.
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    auto assertVideoStreamStartedOrEndedInFrame = [&](bool, WKFrameInfo*) { };
#else
    auto assertVideoStreamStartedOrEndedInFrame = [&](bool started, WKFrameInfo* frame) {
        __block RetainPtr<NSError> error;
        __block bool done = false;

        NSString *source = started ? @"return didStartStream.then(() => true)" : @"return didEndStream.then(() => true)";
        [webView callAsyncJavaScript:source arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
            error = callError;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        ASSERT_FALSE(!!error) << "Capture failed to " << (started ? "start" : "end") << " frames for video element: " << [error description].UTF8String;
    };
#endif

    auto assertVideoStreamStartedInFrame = [&](WKFrameInfo *frame) {
        assertVideoStreamStartedOrEndedInFrame(true, frame);
    };
    auto assertVideoStreamEndedInFrame = [&](WKFrameInfo *frame) {
        assertVideoStreamStartedOrEndedInFrame(false, frame);
    };

    auto assertCaptureState = [&](_WKMediaCaptureStateDeprecated expected) {
        _WKMediaCaptureStateDeprecated actual;
        TestWebKitAPI::Util::waitFor([webView, expected, &actual] {
            actual = [webView _mediaCaptureState];
            return actual == expected;
        });
        ASSERT_EQ(actual, expected);
    };

    assertStartCaptureSucceedsInFrameWithPrompt(nil);
    assertStartCaptureSucceedsInFrameWithPrompt([webView firstChildFrame]);
    assertVideoStreamStartedInFrame(nil);
    assertVideoStreamStartedInFrame([webView firstChildFrame]);
    assertCaptureState(_WKMediaCaptureStateDeprecatedActiveCamera);

    [webView _stopMediaCapture];

    assertVideoStreamEndedInFrame(nil);
    assertVideoStreamEndedInFrame([webView firstChildFrame]);
    assertCaptureState(_WKMediaCaptureStateDeprecatedNone);

    assertStartCaptureSucceedsInFrameWithPrompt([webView firstChildFrame]);
    assertStartCaptureSucceedsInFrameWithPrompt(nil);
    assertVideoStreamStartedInFrame(nil);
    assertVideoStreamStartedInFrame([webView firstChildFrame]);
    assertCaptureState(_WKMediaCaptureStateDeprecatedActiveCamera);
}

TEST(SiteIsolation, MediaCapturePermissionUsesRemoteFrameOrigin)
{
    auto mainFrameHTML = "<iframe allow='camera *' src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<script>"
        "async function captureVideo() {"
        "    try {"
        "        const stream = await navigator.mediaDevices.getUserMedia({ audio: false, video: true });"
        "        stream.getTracks().forEach(track => track.stop());"
        "        return 'granted';"
        "    } catch (error) {"
        "        return error.name === 'NotAllowedError' ? 'denied' : `fail (${error.name})`;"
        "    }"
        "}"
        "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration _setMediaCaptureEnabled:YES];

    RetainPtr preferences = [configuration preferences];
    [preferences _setMediaCaptureRequiresSecureConnection:NO];
    [preferences _setMockCaptureDevicesEnabled:YES];
    [preferences _setGetUserMediaRequiresFocus:NO];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectZero, false);
    RetainPtr delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [delegate setDecision:WKPermissionDecisionDeny forFrameHost:@"webkit.org"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto captureResultInFrame = [&](WKFrameInfo *frame) -> RetainPtr<NSString> {
        __block RetainPtr<NSString> result;
        __block bool done = false;
        [webView callAsyncJavaScript:@"return captureVideo()" arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id value, NSError *error) {
            result = (NSString *)value;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        return result;
    };

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ([captureResultInFrame(childFrame.get()) UTF8String], "denied");
    EXPECT_WK_STREQ([delegate lastRequestFrameHost], "webkit.org");
}

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(ENCRYPTED_MEDIA)
TEST(SiteIsolation, RequestMediaKeySystemAccessInCrossSiteIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto requestAccess = [&] {
        return [webView objectByCallingAsyncFunction:@"return navigator.requestMediaKeySystemAccess('org.w3.clearkey', [{ initDataTypes: ['cenc'], videoCapabilities: [{ contentType: 'video/mp4; codecs=\"avc1.64001F\"' }] }]).then(() => 'granted', (error) => error.name)" withArguments:@{ } inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld];
    };

    EXPECT_WK_STREQ(requestAccess(), "granted");

    WKPageUIClientV16 uiClient;
    zeroBytes(uiClient);
    uiClient.base.version = 16;
    uiClient.decidePolicyForMediaKeySystemPermissionRequest = [](WKPageRef, WKSecurityOriginRef, WKStringRef, WKMediaKeySystemPermissionCallbackRef callback) {
        WKMediaKeySystemPermissionCallbackComplete(callback, false);
    };
    WKPageSetPageUIClient([webView _pageForTesting], &uiClient.base);
    EXPECT_WK_STREQ(requestAccess(), "NotSupportedError");
}
#endif

TEST(SiteIsolation, AutoplayPolicyInRemoteFrameFollowsMainFrame)
{
    auto mainFrameHTML = "<script>"
        "window.onmessage = (event) => window.webkit.messageHandlers.testHandler.postMessage('iframe:' + event.data);"
        "function playMainVideo() {"
        "    var video = document.getElementById('video');"
        "    video.addEventListener('play', () => window.webkit.messageHandlers.testHandler.postMessage('main:autoplayed'));"
        "    video.play().catch((error) => { if (error.name === 'NotAllowedError') window.webkit.messageHandlers.testHandler.postMessage('main:did-not-play'); });"
        "}"
        "</script>"
        "<body onload='playMainVideo()'>"
        "<video id='video' webkit-playsinline src='/video-with-audio.mp4'></video>"
        "<iframe src='https://webkit.org/subframe'></iframe>"
        "</body>"_s;
    auto subFrameHTML = "<script>"
        "function playSubframeVideo() {"
        "    var video = document.getElementById('video');"
        "    video.addEventListener('play', () => window.parent.postMessage('autoplayed', '*'));"
        "    video.play().catch((error) => { if (error.name === 'NotAllowedError') window.parent.postMessage('did-not-play', '*'); });"
        "}"
        "</script>"
        "<body onload='playSubframeVideo()'>"
        "<video id='video' webkit-playsinline src='/video-with-audio.mp4'></video>"
        "</body>"_s;

    RetainPtr videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:NO];
#endif

    __block _WKWebsiteAutoplayPolicy mainFramePolicy = _WKWebsiteAutoplayPolicyDeny;
    __block _WKWebsiteAutoplayPolicy subframePolicy = _WKWebsiteAutoplayPolicyAllow;

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
#if PLATFORM(MAC)
    [webView _setWindowOcclusionDetectionEnabled:NO];
#endif
    [navigationDelegate setDecidePolicyForNavigationActionWithPreferences:^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        [preferences _setAutoplayPolicy:[action.request.URL.host isEqualToString:@"webkit.org"] ? subframePolicy : mainFramePolicy];
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    }];

    RetainPtr<NSMutableSet<NSString *>> received = adoptNS([[NSMutableSet alloc] init]);
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        [received addObject:message];
    }];
    auto waitForBoth = [&](NSString *mainResult, NSString *iframeResult) {
        while (![received containsObject:mainResult] || ![received containsObject:iframeResult])
            TestWebKitAPI::Util::spinRunLoop(10);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    waitForBoth(@"main:did-not-play", @"iframe:did-not-play");

    mainFramePolicy = _WKWebsiteAutoplayPolicyAllow;
    subframePolicy = _WKWebsiteAutoplayPolicyDeny;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    waitForBoth(@"main:autoplayed", @"iframe:autoplayed");
}

TEST(SiteIsolation, FrameServerTrust)
{
    HTTPServer plaintextServer({
        { "/"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
    });
    HTTPServer secureServer({
        { "/iframe"_s, { "<script>alert('iframe loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool receivedAlert { false };
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        EXPECT_WK_STREQ(message, "iframe loaded");
        EXPECT_NOT_NULL(frameInfo._serverTrust);
        verifyCertificateAndPublicKey(frameInfo._serverTrust);
        completionHandler();
        receivedAlert = true;
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(secureServer);
    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:plaintextServer.request()];
    Util::run(&receivedAlert);
    EXPECT_NULL([webView mainFrame].info._serverTrust);
    verifyCertificateAndPublicKey([webView firstChildFrame]._serverTrust);
}

TEST(SiteIsolation, CoordinateTransformation)
{
    HTTPServer server({
        { "/example"_s, { "<br><iframe id='wk' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    auto convertRect = [](TestWKWebView *webView, CGRect rect) {
        __block CGRect result;
        __block bool done { false };
        [webView _convertRect:rect fromFrame:[webView firstChildFrame] toMainFrameCoordinates:^(CGRect transformedRect, NSError *error) {
            EXPECT_NULL(error);
            result = transformedRect;
            done = true;
        }];
        Util::run(&done);
        return result;
    };

    constexpr auto expectedTransformedY = 38;
    {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [navigationDelegate waitForDidFinishNavigation];
        auto transformedRect = convertRect(webView.get(), { { 11, 10 }, { 9, 8 } });
        EXPECT_EQ(transformedRect.origin.x, 21);
        EXPECT_EQ(transformedRect.origin.y, expectedTransformedY);
        EXPECT_EQ(transformedRect.size.height, 8);
        EXPECT_EQ(transformedRect.size.width, 9);
    }

    {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/example"]]];
        [navigationDelegate waitForDidFinishNavigation];
        auto transformedRect = convertRect(webView.get(), { { 11, 10 }, { 9, 8 } });
        EXPECT_EQ(transformedRect.origin.x, 21);
        EXPECT_EQ(transformedRect.origin.y, expectedTransformedY);
        EXPECT_EQ(transformedRect.size.height, 8);
        EXPECT_EQ(transformedRect.size.width, 9);
    }

    RetainPtr frameInfoOfRemovedFrame = [webView firstChildFrame];
    __block bool removedIframe { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('wk');frame.parentNode.removeChild(frame)" completionHandler:^(id, NSError *error) {
        removedIframe = true;
    }];
    Util::run(&removedIframe);
    __block bool done { false };
    [webView _convertRect:CGRect { { 11, 10 }, { 9, 8 } } fromFrame:frameInfoOfRemovedFrame.get() toMainFrameCoordinates:^(CGRect, NSError *error) {
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    Util::run(&done);
}

RetainPtr<_WKTextManipulationToken> createToken(NSString *identifier, NSString *content)
{
    RetainPtr<_WKTextManipulationToken> token = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token setIdentifier: identifier];
    [token setContent: content];
    return token;
}

static RetainPtr<_WKTextManipulationItem> createItem(NSString *itemIdentifier, const Vector<RetainPtr<_WKTextManipulationToken>>& tokens)
{
    RetainPtr<NSMutableArray> wkTokens = adoptNS([[NSMutableArray alloc] init]);
    for (auto& token : tokens)
        [wkTokens addObject:token.get()];

    return adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:itemIdentifier tokens:wkTokens.get()]);
}

TEST(SiteIsolation, CompleteTextManipulation)
{
    static constexpr auto mainFrameBytes = R"TESTRESOURCE(
    <div id='text'>mainframe content</div>
    <script>
        function getTextContent() {
            window.webkit.messageHandlers.testHandler.postMessage(document.getElementById('text').innerHTML);
        }
        function getIframeTextContent() {
            document.getElementById('iframe').contentWindow.postMessage('print', '*');
        }
        function postResult(event) {
            window.webkit.messageHandlers.testHandler.postMessage(event.data);
        }
        addEventListener('message', postResult, false);
    </script>
    <iframe id='iframe' src='https://apple.com/apple'></iframe>
    )TESTRESOURCE"_s;

    static constexpr auto iframeBytes = R"TESTRESOURCE(
    <div id='text'>iframe content</div>
    <script>
        addEventListener('message', () => {
            let textElement = document.getElementById('text');
            parent.postMessage(textElement.innerHTML, '*');
        }, false);
        parent.postMessage('loaded', '*');
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/example"_s, { mainFrameBytes } },
        { "/apple"_s, { iframeBytes } },
    }, HTTPServer::Protocol::HttpsProxy);

    bool didLoad = false;
    bool didReceiveMainFrameContent = false;
    bool didReceiveIframeContent = false;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    [webViewAndDelegates.messageHandler addMessage:@"loaded" withHandler:[&] {
        didLoad = true;
    }];
    [webViewAndDelegates.messageHandler addMessage:@"MAINFRAME CONTENT" withHandler:[&] {
        didReceiveMainFrameContent = true;
    }];
    [webViewAndDelegates.messageHandler addMessage:@"IFRAME CONTENT" withHandler:[&] {
        didReceiveIframeContent = true;
    }];
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr textManipulationDelegate = adoptNS([[SiteIsolationTextManipulationDelegate alloc] init]);
    [webView _setTextManipulationDelegate:textManipulationDelegate.get()];
    RetainPtr manipulationConfiguration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    manipulationConfiguration.get().includeSubframes = YES;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&didLoad);

    [webView _startTextManipulationsWithConfiguration:manipulationConfiguration.get() completion:^{ }];
    while ([textManipulationDelegate items].count < 2)
        Util::spinRunLoop();

    RetainPtr items = [textManipulationDelegate items];
    auto sortFunction = ^(_WKTextManipulationItem *item1, _WKTextManipulationItem *item2) {
        auto value1 = [NSNumber numberWithBool:item1.isSubframe];
        auto value2 = [NSNumber numberWithBool:item2.isSubframe];
        return [value1 compare:value2];
    };
    RetainPtr sortedItems = [items.get() sortedArrayUsingComparator:sortFunction];
    EXPECT_EQ(items.get().count, 2UL);
    auto firstItem = [sortedItems objectAtIndex:0];
    auto secondItem = [sortedItems objectAtIndex:1];
    EXPECT_EQ(firstItem.isSubframe, NO);
    EXPECT_EQ(firstItem.isCrossSiteSubframe, NO);
    EXPECT_EQ(firstItem.tokens.count, 1UL);
    EXPECT_STREQ("mainframe content", firstItem.tokens[0].content.UTF8String);
    EXPECT_EQ(secondItem.isSubframe, YES);
    EXPECT_EQ(secondItem.isCrossSiteSubframe, YES);
    EXPECT_EQ(secondItem.tokens.count, 1UL);
    EXPECT_STREQ("iframe content", secondItem.tokens[0].content.UTF8String);

    __block bool done = false;
    [webView _completeTextManipulationForItems:@[
        (_WKTextManipulationItem *)createItem(firstItem.identifier, { createToken(firstItem.tokens[0].identifier, @"MAINFRAME CONTENT") }),
        (_WKTextManipulationItem *)createItem(secondItem.identifier, { createToken(secondItem.tokens[0].identifier, @"IFRAME CONTENT") })
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    Util::run(&done);

    [webView evaluateJavaScript:@"getTextContent()" completionHandler:nil];
    Util::run(&didReceiveMainFrameContent);

    [webView evaluateJavaScript:@"getIframeTextContent()" completionHandler:nil];
    Util::run(&didReceiveIframeContent);
}

TEST(SiteIsolation, CompleteTextManipulationFailsInSomeFrame)
{
    static constexpr auto mainFrameBytes = R"TESTRESOURCE(
    <div>mainframe content</div>
    <script>
        function removeIframe() {
            let element = document.getElementById('iframe');
            element.parentNode.removeChild(element);
        }
        let messageCount = 0;
        addEventListener('message', () => {
            if (++messageCount == 2)
                window.webkit.messageHandlers.testHandler.postMessage('loaded');
        }, false);
    </script>
    <iframe id='iframe' src='https://apple.com/iframe'></iframe>
    <iframe src='https://webkit.org/iframe'></iframe>
    )TESTRESOURCE"_s;

    static constexpr auto iframeBytes = R"TESTRESOURCE(
    <div>iframe content</div>
    <script>
        parent.postMessage('loaded', '*');
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameBytes } },
        { "/iframe"_s, { iframeBytes } }
    }, HTTPServer::Protocol::HttpsProxy);

    bool receivedMessage = false;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    [webViewAndDelegates.messageHandler addMessage:@"loaded" withHandler:[&] {
        receivedMessage = true;
    }];
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr textManipulationDelegate = adoptNS([[SiteIsolationTextManipulationDelegate alloc] init]);
    [webView _setTextManipulationDelegate:textManipulationDelegate.get()];
    RetainPtr manipulationConfiguration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    manipulationConfiguration.get().includeSubframes = YES;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    Util::run(&receivedMessage);

    [webView _startTextManipulationsWithConfiguration:manipulationConfiguration.get() completion:^{ }];
    while ([textManipulationDelegate items].count < 3)
        Util::spinRunLoop();

    RetainPtr items = [textManipulationDelegate items];
    auto sortFunction = ^(_WKTextManipulationItem *item1, _WKTextManipulationItem *item2) {
        auto value1 = [NSNumber numberWithBool:item1.isSubframe];
        auto value2 = [NSNumber numberWithBool:item2.isSubframe];
        return [value1 compare:value2];
    };

    RetainPtr sortedItems = [items.get() sortedArrayUsingComparator:sortFunction];
    EXPECT_EQ(items.get().count, 3UL);
    auto item1 = [sortedItems objectAtIndex:0];
    auto item2 = [sortedItems objectAtIndex:1];
    auto item3 = [sortedItems objectAtIndex:2];
    EXPECT_EQ(item1.isSubframe, NO);
    EXPECT_EQ(item1.isCrossSiteSubframe, NO);
    EXPECT_EQ(item1.tokens.count, 1UL);
    EXPECT_STREQ("mainframe content", item1.tokens[0].content.UTF8String);
    EXPECT_EQ(item2.isSubframe, YES);
    EXPECT_EQ(item2.isCrossSiteSubframe, YES);
    EXPECT_EQ(item2.tokens.count, 1UL);
    EXPECT_STREQ("iframe content", item2.tokens[0].content.UTF8String);
    EXPECT_EQ(item3.isSubframe, YES);
    EXPECT_EQ(item3.isCrossSiteSubframe, YES);
    EXPECT_EQ(item3.tokens.count, 1UL);
    EXPECT_STREQ("iframe content", item3.tokens[0].content.UTF8String);

    __block bool done = false;
    [webView evaluateJavaScript:@"removeIframe()" completionHandler:^(id, NSError *) {
        done = true;
    }];
    Util::run(&done);

    __block RetainPtr newItem1 = createItem(item1.identifier, { createToken(item1.tokens[0].identifier, @"MAINFRAME CONTENT") });
    __block RetainPtr newItem2 = createItem(item2.identifier, { createToken(item2.tokens[0].identifier, @"IFRAME CONTENT") });
    __block RetainPtr newItem3 = createItem(item3.identifier, { createToken(item3.tokens[0].identifier, @"IFRAME CONTENT") });
    [webView _completeTextManipulationForItems:@[ newItem1.get(), newItem2.get(), newItem3.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_NOT_NULL(errors);
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorContentChanged);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], newItem2.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(SiteIsolation, TextManipulationInIframeIfIframeIsAddedAfterTranslationCall)
{
    HTTPServer server({
        { "/example"_s, {
        "<script>"
        "    function addCrossDomainIframe() {"
        "        const iframe = document.createElement('iframe');"
        "        iframe.src = 'https://apple.com/iframe.html';"
        "        document.body.appendChild(iframe);"
        "    }"
        "</script>"_s } },

        { "/iframe.html"_s, { "<html><body>hello<br>world<div>WebKit</div></body></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;

    RetainPtr textManipulationDelegate = adoptNS([[SiteIsolationTextManipulationDelegate alloc] init]);
    [webView _setTextManipulationDelegate:textManipulationDelegate.get()];
    RetainPtr manipulationConfiguration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    manipulationConfiguration.get().includeSubframes = YES;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [webView _startTextManipulationsWithConfiguration:manipulationConfiguration.get() completion:^{
        done = true;
    }];
    Util::run(&done);

    [webView evaluateJavaScript:@"addCrossDomainIframe();" completionHandler:nil];

    while ([textManipulationDelegate items].count < 3)
        Util::spinRunLoop();

    RetainPtr items = [textManipulationDelegate items];
    EXPECT_EQ(items.get().count, 3UL);
}

TEST(SiteIsolation, CreateWebArchive)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div>mainframe content</div><iframe src='https://apple.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div>iframe content</div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><div>mainframe content</div><iframe src=\"https://apple.com/iframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://example.com/mainframe"
            },
            @"WebSubframeArchives" : @[ @{
                @"WebMainResource" : @{
                    @"WebResourceData" : [@"<html><head></head><body><div>iframe content</div></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                    @"WebResourceFrameName" : @"<!--frame1-->",
                    @"WebResourceMIMEType" : @"text/html",
                    @"WebResourceTextEncodingName" : @"UTF-8",
                    @"WebResourceURL" : @"https://apple.com/iframe"
                }
            } ],
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

TEST(SiteIsolation, CreateWebArchiveNestedFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nestedframe'></iframe>"_s } },
        { "/nestedframe"_s, { "<p>hello</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><iframe src=\"https://domain2.com/subframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://domain1.com/mainframe"
            },
            @"WebSubframeArchives" : @[ @{
                @"WebMainResource" : @{
                    @"WebResourceData" : [@"<html><head></head><body><iframe src=\"https://domain3.com/nestedframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                    @"WebResourceFrameName" : @"<!--frame1-->",
                    @"WebResourceMIMEType" : @"text/html",
                    @"WebResourceTextEncodingName" : @"UTF-8",
                    @"WebResourceURL" : @"https://domain2.com/subframe"
                },
                @"WebSubframeArchives" : @[ @{
                    @"WebMainResource" : @{
                        @"WebResourceData" : [@"<html><head></head><body><p>hello</p></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                        @"WebResourceFrameName" : @"<!--frame2-->",
                        @"WebResourceMIMEType" : @"text/html",
                        @"WebResourceTextEncodingName" : @"UTF-8",
                        @"WebResourceURL" : @"https://domain3.com/nestedframe"
                    }
                } ]
            } ],
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

TEST(SiteIsolation, CreateWebArchiveForFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div>mainframe content</div><iframe src='https://apple.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div>iframe content</div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    __block RetainPtr<NSArray<_WKFrameTreeNode *>> childFrames;
    [webView _frames:^(_WKFrameTreeNode *result) {
        EXPECT_NOT_NULL(result);
        EXPECT_NOT_NULL(result.childFrames);
        childFrames = result.childFrames;
        done = true;
    }];
    Util::run(&done);
    done = false;

    EXPECT_EQ([childFrames count], 1u);
    [webView _createWebArchiveForFrame:childFrames.get().firstObject.info completionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><div>iframe content</div></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"<!--frame1-->",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://apple.com/iframe"
            },
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

TEST(SiteIsolation, CreateWebArchiveForFrames)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div>mainframe content</div><iframe src='https://apple.com/iframe'></iframe><iframe src='https://example.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div>iframe content</div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    __block RetainPtr<NSMutableArray<WKFrameInfo *>> frames;
    [webView _frames:^(_WKFrameTreeNode *rootFrame) {
        frames = adoptNS([[NSMutableArray alloc] init]);
        EXPECT_NOT_NULL(rootFrame);
        // Include main frame.
        [frames addObject:rootFrame.info];

        EXPECT_EQ([rootFrame.childFrames count], 2u);
        for (_WKFrameTreeNode *frame in rootFrame.childFrames) {
            if (!frame || !frame.info || !frame.info.securityOrigin || !frame.info.securityOrigin.host)
                continue;
            // Include frames that load example.com.
            if ([frame.info.securityOrigin.host containsString:@"example.com"])
                [frames addObject:frame.info];
        }
        done = true;
    }];
    Util::run(&done);
    done = false;

    EXPECT_EQ([frames count], 2u);
    [webView _createWebArchiveForFrames:[frames copy] rootFrame:frames.get().firstObject completionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><div>mainframe content</div><iframe src=\"https://apple.com/iframe\"></iframe><iframe src=\"https://example.com/iframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://example.com/mainframe"
            },
            @"WebSubframeArchives" : @[ @{
                @"WebMainResource" : @{
                    @"WebResourceData" : [@"<html><head></head><body><div>iframe content</div></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                    @"WebResourceFrameName" : @"<!--frame2-->",
                    @"WebResourceMIMEType" : @"text/html",
                    @"WebResourceTextEncodingName" : @"UTF-8",
                    @"WebResourceURL" : @"https://example.com/iframe"
                }
            } ],
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

static void validateWebArchiveMainResource(NSDictionary *actualResource, NSDictionary *expectedResource)
{
    if (!actualResource)
        return;

    for (id key in expectedResource) {
        NSString *actualValue = [actualResource objectForKey:key];
        if (!actualValue)
            actualValue = @"NULL";
        EXPECT_WK_STREQ([actualValue UTF8String], [[expectedResource objectForKey:key] UTF8String]);
    }
}

TEST(SiteIsolation, CreateWebArchiveForCopy)
{
    static constexpr auto mainframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    mainframecontent
    <iframe id='subframe' src='https://example2.com/subframe'></iframe>
    <script>
        function alertSubframe() { document.getElementById('subframe').contentWindow.postMessage('alert', '*'); }
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto subframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    subframecontent
    <script>
        window.addEventListener('message', function(event) {
            alert('hi');
        });
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeBytes } },
        { "/subframe"_s, { subframeBytes } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    static bool alerted = false;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alerted = true;
        completionHandler();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];

    [webView copy:nil];

    // Ensure there is enough time to collect webarchive from subframe process.
    [webView evaluateJavaScript:@"alertSubframe()" completionHandler:nil];
    Util::run(&alerted);

#if PLATFORM(IOS_FAMILY)
    RetainPtr archiveData = [UIPasteboard.generalPasteboard dataForPasteboardType:UTTypeWebArchive.identifier];
#else
    RetainPtr archiveData = [NSPasteboard.generalPasteboard dataForType:UTTypeWebArchive.identifier];
#endif
    NSDictionary* expectedMainFrameResource = @{
        @"WebResourceFrameName" : @"",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example.com/mainframe"
    };
    NSDictionary* expectedSubframeResource = @{
        @"WebResourceFrameName" : @"<!--frame1-->",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example2.com/subframe"
    };
    NSDictionary* actualMainframeArchive = [NSPropertyListSerialization propertyListWithData:archiveData.get() options:0 format:nil error:nil];
    EXPECT_NOT_NULL(actualMainframeArchive);
    validateWebArchiveMainResource([actualMainframeArchive objectForKey:@"WebMainResource"], expectedMainFrameResource);
    NSArray *subframeArchives = [actualMainframeArchive objectForKey:@"WebSubframeArchives"];
    EXPECT_NOT_NULL(subframeArchives);
    EXPECT_EQ(subframeArchives.count, 1u);
    validateWebArchiveMainResource([subframeArchives.firstObject objectForKey:@"WebMainResource"], expectedSubframeResource);
}

TEST(SiteIsolation, CreateWebArchiveNestedFrameForCopy)
{
    static constexpr auto mainframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    mainframecontent
    <iframe id='subframe' src='https://example2.com/subframe'></iframe>
    <script>
        function alertSubframe() { document.getElementById('subframe').contentWindow.postMessage('alert', '*'); }
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto subframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    subframecontent
    <iframe src='https://example.com/nestedframe'></iframe>
    <script>
        window.addEventListener('message', function(event) {
            alert('hi');
        });
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeBytes } },
        { "/subframe"_s, { subframeBytes } },
        { "/nestedframe"_s, { "nestedframecontent"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    static bool alerted = false;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alerted = true;
        completionHandler();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];

    [webView copy:nil];

    // Ensure there is enough time to collect webarchive from subframe process.
    [webView evaluateJavaScript:@"alertSubframe()" completionHandler:nil];
    Util::run(&alerted);

#if PLATFORM(IOS_FAMILY)
    RetainPtr archiveData = [UIPasteboard.generalPasteboard dataForPasteboardType:UTTypeWebArchive.identifier];
#else
    RetainPtr archiveData = [NSPasteboard.generalPasteboard dataForType:UTTypeWebArchive.identifier];
#endif
    NSDictionary* expectedMainFrameResource = @{
        @"WebResourceFrameName" : @"",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example.com/mainframe"
    };
    NSDictionary* expectedSubframeResource = @{
        @"WebResourceFrameName" : @"<!--frame1-->",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example2.com/subframe"
    };
    NSDictionary* expectedNestedFrameResource = @{
        @"WebResourceFrameName" : @"<!--frame2-->",
        @"WebResourceMIMEType" : @"text/plain",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example.com/nestedframe"
    };
    NSDictionary* actualMainframeArchive = [NSPropertyListSerialization propertyListWithData:archiveData.get() options:0 format:nil error:nil];
    EXPECT_NOT_NULL(actualMainframeArchive);
    validateWebArchiveMainResource([actualMainframeArchive objectForKey:@"WebMainResource"], expectedMainFrameResource);
    NSArray *actualSubframeArchives = [actualMainframeArchive objectForKey:@"WebSubframeArchives"];
    EXPECT_NOT_NULL(actualSubframeArchives);
    EXPECT_EQ(actualSubframeArchives.count, 1u);
    NSDictionary* actualSubframeArchive = actualSubframeArchives.firstObject;
    validateWebArchiveMainResource([actualSubframeArchive objectForKey:@"WebMainResource"], expectedSubframeResource);
    NSArray *actualNestedFrameArchives = [actualSubframeArchive objectForKey:@"WebSubframeArchives"];
    EXPECT_NOT_NULL(actualNestedFrameArchives);
    EXPECT_EQ(actualNestedFrameArchives.count, 1u);
    validateWebArchiveMainResource([actualNestedFrameArchives.firstObject objectForKey:@"WebMainResource"], expectedNestedFrameResource);
}

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

static RetainPtr<NSAttributedString> attributedStringFromGeneralPasteboard()
{
#if PLATFORM(MAC)
    RetainPtr objects = [NSPasteboard.generalPasteboard readObjectsForClasses:@[NSAttributedString.class] options:@{ }];
    EXPECT_EQ([objects count], 1u);
    return [objects firstObject];
#else
    RetainPtr itemProvider = [[UIPasteboard.generalPasteboard itemProviders] firstObject];
    __block bool doneLoading = false;
    __block RetainPtr<NSAttributedString> result;
    [itemProvider loadObjectOfClass:NSAttributedString.class completionHandler:^(NSAttributedString *string, NSError *) {
        result = string;
        doneLoading = true;
    }];
    Util::run(&doneLoading);
    return result;
#endif
}

TEST(SiteIsolation, ReadAttributedStringFromPasteboardAfterCopyWithCrossSiteIframe)
{
    static constexpr auto mainframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    mainframecontent
    <iframe id='subframe' src='https://example2.com/subframe'></iframe>
    <script>
        function alertSubframe() { document.getElementById('subframe').contentWindow.postMessage('alert', '*'); }
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto subframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    subframecontent
    <script>
        window.addEventListener('message', function(event) {
            alert('hi');
        });
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeBytes } },
        { "/subframe"_s, { subframeBytes } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    WKPreferencesSetWriteRichTextDataWhenCopyingOrDragging((__bridge WKPreferencesRef)[[webView configuration] preferences], true);
    static bool alerted = false;
    [uiDelegate setRunJavaScriptAlertPanelWithMessage:^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)()) {
        alerted = true;
        completionHandler();
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView, {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example2.com"_s } }
        },
    });

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];

    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

    [webView evaluateJavaScript:@"alertSubframe()" completionHandler:nil];
    Util::run(&alerted);

    RetainPtr result = attributedStringFromGeneralPasteboard();
    EXPECT_TRUE([[result string] containsString:@"mainframecontent"]);
    EXPECT_TRUE([[result string] containsString:@"subframecontent"]);
}

TEST(SiteIsolation, ReadAttributedStringFromPasteboardAfterCopyWithNestedCrossSiteIframes)
{
    static constexpr auto mainframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    mainframecontent
    <iframe id='subframe' src='https://example2.com/subframe'></iframe>
    <iframe src='https://example.com/samesiteframe'></iframe>
    <script>
        function alertSubframe() { document.getElementById('subframe').contentWindow.postMessage('alert', '*'); }
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto subframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    subframecontent
    <iframe src='https://example3.com/nestedframe'></iframe>
    <iframe src='https://example.com/nestedframeinmainframeprocess'></iframe>
    <script>
        window.addEventListener('message', function(event) {
            alert('hi');
        });
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto samesiteframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    samesiteframecontent
    <iframe src='https://example4.com/nestedframeundersamesiteframe'></iframe>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeBytes } },
        { "/subframe"_s, { subframeBytes } },
        { "/samesiteframe"_s, { samesiteframeBytes } },
        { "/nestedframe"_s, { "nestedframecontent"_s } },
        { "/nestedframeinmainframeprocess"_s, { "nestedframeinmainframeprocesscontent"_s } },
        { "/nestedframeundersamesiteframe"_s, { "nestedframeundersamesiteframecontent"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    WKPreferencesSetWriteRichTextDataWhenCopyingOrDragging((__bridge WKPreferencesRef)[[webView configuration] preferences], true);
    static bool alerted = false;
    [uiDelegate setRunJavaScriptAlertPanelWithMessage:^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)()) {
        alerted = true;
        completionHandler();
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];

    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

    [webView evaluateJavaScript:@"alertSubframe()" completionHandler:nil];
    Util::run(&alerted);

    RetainPtr result = attributedStringFromGeneralPasteboard();
    EXPECT_TRUE([[result string] containsString:@"mainframecontent"]);
    EXPECT_TRUE([[result string] containsString:@"subframecontent"]);

    // Nested inside a cross-site frame, in a third process.
    EXPECT_TRUE([[result string] containsString:@"nestedframecontent"]);

    // Nested inside a same-site frame, which the copying process converts inline.
    EXPECT_TRUE([[result string] containsString:@"samesiteframecontent"]);
    EXPECT_TRUE([[result string] containsString:@"nestedframeundersamesiteframecontent"]);

    // Nested inside a cross-site frame, but back in the main frame's process.
    EXPECT_TRUE([[result string] containsString:@"nestedframeinmainframeprocesscontent"]);
}

#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

TEST(SiteIsolation, LoadWebArchive)
{
    RetainPtr<NSURL> archiveURL = [NSBundle.test_resourcesBundle URLForResource:@"SiteIsolationLoadWebArchive" withExtension:@"webarchive"];
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    setFeatureEnabled(configuration.get(), @"SiteIsolationSharedProcessEnabled", false);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, true);
    [webView loadRequest:[NSURLRequest requestWithURL:archiveURL.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { "https://apple.com"_s } }
        },
    });
}

TEST(SiteIsolation, LoadWebArchiveWithSharedProcess)
{
    RetainPtr<NSURL> archiveURL = [NSBundle.test_resourcesBundle URLForResource:@"SiteIsolationLoadWebArchive" withExtension:@"webarchive"];
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    setFeatureEnabled(configuration.get(), @"SiteIsolationSharedProcessEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, true);
    [webView loadRequest:[NSURLRequest requestWithURL:archiveURL.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { "https://apple.com"_s } }
        },
    });
}

TEST(SiteIsolation, LoadWebArchiveNestedFrame)
{
    RetainPtr<NSURL> archiveURL = [NSBundle.test_resourcesBundle URLForResource:@"SiteIsolationLoadWebArchiveNestedFrame" withExtension:@"webarchive"];
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, true);
    [webView loadRequest:[NSURLRequest requestWithURL:archiveURL.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://domain1.com"_s,
            { { "https://domain2.com"_s, { { "https://domain3.com"_s } } } }
        },
    });
}

TEST(SiteIsolation, Events)
{
    auto eventListeners = "<script>"
    "addEventListener('resize', ()=>{ alert('resize') });"
    "addEventListener('load', ()=>{ alert('load') });"
    "addEventListener('beforeunload', ()=>{ alert('beforeunload') });"
    "addEventListener('unload', ()=>{ alert('unload') });"
    "addEventListener('pageswap', ()=>{ alert('pageswap') });"
    "addEventListener('pageshow', ()=>{ alert('pageshow') });"
    "addEventListener('pagehide', ()=>{ alert('pagehide') });"
    "addEventListener('pagereveal', ()=>{ alert('pagereveal') });"
    "addEventListener('focus', ()=>{ alert('focus') });"
    "addEventListener('blur', ()=>{ alert('blur') });"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { makeString(eventListeners, "<br><iframe id='wk' src='https://webkit.org/iframe'></iframe>"_s) } },
        { "/iframe"_s, { eventListeners } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool receivedLastExpectedMessage = false;
    __block bool receivedResize = false;
    __block RetainPtr<NSMutableArray<NSString *>> webkitMessages = adoptNS([NSMutableArray new]);
    __block RetainPtr<NSMutableArray<NSString *>> exampleMessages = adoptNS([NSMutableArray new]);
    __block RetainPtr<NSMutableArray<NSString *>> appleMessages = adoptNS([NSMutableArray new]);
    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    delegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frame, void (^completionHandler)(void)) {
        NSString *host = frame.securityOrigin.host;
        if ([host isEqualToString:@"apple.com"])
            [appleMessages addObject:message];
        else if ([host isEqualToString:@"webkit.org"])
            [webkitMessages addObject:message];
        else if ([host isEqualToString:@"example.com"])
            [exampleMessages addObject:message];
        else
            EXPECT_FALSE(true);
        completionHandler();
        if ([message isEqualToString:@"resize"] && [host isEqualToString:@"webkit.org"])
            receivedResize = true;
        if ([message isEqualToString:@"pageshow"] && [frame.securityOrigin.host isEqualToString:@"apple.com"])
            receivedLastExpectedMessage = true;
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().UIDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"wk.height = 75" completionHandler:nil];
    Util::run(&receivedResize);
    [webView evaluateJavaScript:@"window.location = 'https://apple.com/iframe'" inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedLastExpectedMessage);
    Util::runFor(Seconds(0.1));

    NSArray *expectedExampleMessages = @[
#if PLATFORM(IOS_FAMILY)
        @"pagereveal",
#endif
        @"load",
        @"pageshow",
    ];
    if (![exampleMessages isEqualToArray:expectedExampleMessages]) {
        NSLog(@"Actual example messages: %@", exampleMessages.get());
        EXPECT_TRUE(false);
    }

    NSArray *expectedWebKitMessages = @[
        @"load",
        @"pageshow",
#if PLATFORM(IOS_FAMILY)
        @"pagereveal",
#endif
        @"resize",
        @"pageswap",
    ];
    if (![webkitMessages isEqualToArray:expectedWebKitMessages]) {
        NSLog(@"Actual webkit messages: %@", webkitMessages.get());
        EXPECT_TRUE(false);
    }

    NSArray *expectedAppleMessages = @[
        @"load",
        @"pageshow",
#if PLATFORM(IOS_FAMILY)
        @"pagereveal",
#endif
    ];
    if (![appleMessages isEqualToArray:expectedAppleMessages]) {
        NSLog(@"Actual apple messages: %@", appleMessages.get());
        EXPECT_TRUE(false);
    }
}

TEST(SiteIsolation, FramesDuringProvisionalNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/second_iframe"_s, { TestWebKitAPI::HTTPResponse::Behavior::NeverSendResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(server.totalRequests(), 2u);

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://webkit.org/second_iframe'" completionHandler:nil];
    while (server.totalRequests() < 3)
        Util::spinRunLoop();
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.parent.length" inFrame:[webView firstChildFrame]] intValue], 2);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame }, { "https://example.com"_s } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, DoAfterNextPresentationUpdate)
{
    HTTPServer server({
        { "/main"_s, { "<iframe src='https://webkit2.org/text'></iframe><iframe src='https://webkit3.org/text'></iframe>"_s } },
        { "/navigatefrom"_s, { "<script>window.location='https://webkit2.org/navigateto'</script>"_s } },
        { "/navigateto"_s, { "<iframe src='https://webkit1.org/alert'></iframe>"_s } },
        { "/alert"_s, { "<script>alert('loaded')</script>"_s } },
        { "/text"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    RetainPtr<WKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().navigationDelegate = navigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit1.org/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.open('https://webkit1.org/navigatefrom')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "loaded");

    __block bool done = false;
    [openedWebView _doAfterNextPresentationUpdate:^{
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, UserScript)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSString *js = @"if (window.parent != window.self) { alert('script ran in iframe') }";
    RetainPtr script = adoptNS([[WKUserScript alloc] initWithSource:js injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    [[webView configuration].userContentController _addUserScriptImmediately:script.get()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "script ran in iframe");
}

// A window opened with no URL has an about:blank main frame that inherits the opener frame's origin.
// Processes seeded with that origin must be told the new one once the window navigates away.
TEST(SiteIsolation, TopOriginInRemoteProcessesAfterMainFrameProcessSwap)
{
    HTTPServer server({
        { "/top"_s, { "<!DOCTYPE html><iframe src='https://widget.com/idle'></iframe><iframe src='https://opener.com/frame'></iframe>"_s } },
        { "/idle"_s, { "<!DOCTYPE html><p>idle</p>"_s } },
        { "/frame"_s, { "<!DOCTYPE html><script>const w = window.open(); setTimeout(() => { w.location = 'https://landing.com/landing' }, 0)</script>"_s } },
        { "/landing"_s, { "<!DOCTYPE html><iframe src='https://widget.com/widget'></iframe>"_s } },
        { "/widget"_s, { "<!DOCTYPE html><script>navigator.serviceWorker.register('/sw.js').then(() => { alert('registered') })</script>"_s } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, "self.addEventListener('message', () => { });"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    // The NetworkProcess terminating a Web process is reported as _WKProcessTerminationReasonCrash.
    __block bool done = false;
    __block bool terminatedByNetworkProcess = false;
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason reason) {
        terminatedByNetworkProcess = reason == _WKProcessTerminationReasonCrash;
        done = true;
    };

    __block auto *sharedNavigationDelegate = navigationDelegate.get();
    __block RetainPtr<TestWKWebView> openedWebView;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *, WKFrameInfo *, void (^completionHandler)(void)) {
        done = true;
        completionHandler();
    };
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 200) configuration:configuration]);
        openedWebView.get().navigationDelegate = sharedNavigationDelegate;
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://top.com/top"]]];
    [navigationDelegate waitForDidFinishNavigation];

    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(terminatedByNetworkProcess);
}

TEST(SiteIsolation, SharedProcessNotReusedAfterCrash)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('webkit')</script>"_s } },
        { "/w3c"_s, { "<script>alert('w3c')</script>"_s } },
        { "/apple"_s, { "<script>alert('apple')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "webkit");
    pid_t crashedSharedProcess = [webView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_NE(crashedSharedProcess, [webView mainFrame].info._processIdentifier);

    kill(crashedSharedProcess, SIGKILL);
    while (processStillRunning(crashedSharedProcess))
        Util::spinRunLoop();

    // The dead shared process must not be handed to this load.
    [webView evaluateJavaScript:
        @"window.f2 = document.createElement('iframe');"
        "document.body.appendChild(window.f2);"
        "window.f2.src = 'https://w3.org/w3c';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "w3c");

    pid_t newSharedProcess = [webView mainFrame].childFrames.lastObject.info._processIdentifier;
    EXPECT_NE(newSharedProcess, crashedSharedProcess);
    EXPECT_TRUE(processStillRunning(newSharedProcess));

    // Wait for the removal to reach the UI process, so the dead shared process's FrameProcess is
    // really gone before the next load asks for a shared process.
    [webView evaluateJavaScript:@"document.querySelector('iframe').remove();" completionHandler:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [webView mainFrame].childFrames.count == 1;
    }));

    [webView evaluateJavaScript:
        @"window.f3 = document.createElement('iframe');"
        "document.body.appendChild(window.f3);"
        "window.f3.src = 'https://apple.com/apple';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "apple");
    EXPECT_EQ([webView mainFrame].childFrames.lastObject.info._processIdentifier, newSharedProcess);
}

TEST(SiteIsolation, SharedProcessMainFrameNotReusedAfterCrash)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>window.open('https://webkit.org/opened')</script>"_s } },
        { "/opened"_s, { "<!DOCTYPE html>opened"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [openerView, openerNavigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    openerView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<TestWKWebView> openedView;
    __block RetainPtr<TestNavigationDelegate> openedNavigationDelegate;
    RetainPtr openerUIDelegate = adoptNS([TestUIDelegate new]);
    openerUIDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [openedNavigationDelegate allowAnyTLSCertificate];
        openedView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        openedView.get().navigationDelegate = openedNavigationDelegate.get();
        return openedView.get();
    };
    [openerView setUIDelegate:openerUIDelegate.get()];

    [openerView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!openedView)
        Util::spinRunLoop();
    [openedNavigationDelegate waitForDidFinishNavigation];

    // The iframe put webkit.org in the shared process, so the window it opened has its main frame there too.
    pid_t crashedSharedProcess = [openerView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_NE(crashedSharedProcess, [openerView mainFrame].info._processIdentifier);
    EXPECT_EQ(crashedSharedProcess, [openedView mainFrame].info._processIdentifier);

    kill(crashedSharedProcess, SIGKILL);
    while (processStillRunning(crashedSharedProcess))
        Util::spinRunLoop();

    // The dead shared process must not be handed to the main frame of the reloaded window.
    [openedView reload];
    [openedNavigationDelegate waitForDidFinishNavigation];

    pid_t reloadedProcess = [openedView mainFrame].info._processIdentifier;
    EXPECT_NE(reloadedProcess, crashedSharedProcess);
    EXPECT_TRUE(processStillRunning(reloadedProcess));
}

TEST(SiteIsolation, SharedProcessShutsDownWhenLastRemoteFrameIsRemoved)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><body></body>"_s } },
        { "/webkit"_s, { "<script>alert('webkit')</script>"_s } },
        { "/w3c"_s, { "<script>alert('w3c')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);

    EXPECT_EQ(0U, [webView.get().configuration.processPool _processCacheCapacity]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto addTwoRemoteFrames = [&] {
        [webView evaluateJavaScript:
            @"window.f1 = document.createElement('iframe');"
            "document.body.appendChild(window.f1);"
            "window.f1.src = 'https://webkit.org/webkit';"
        completionHandler:nil];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "webkit");
        [webView evaluateJavaScript:
            @"window.f2 = document.createElement('iframe');"
            "document.body.appendChild(window.f2);"
            "window.f2.src = 'https://w3.org/w3c';"
        completionHandler:nil];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "w3c");
    };

    addTwoRemoteFrames();
    pid_t sharedProcess = [webView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_EQ(sharedProcess, [webView mainFrame].childFrames[1].info._processIdentifier);
    EXPECT_NE(sharedProcess, [webView mainFrame].info._processIdentifier);

    [webView evaluateJavaScript:@"window.f1.remove(); window.f2.remove();" completionHandler:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return !processStillRunning(sharedProcess);
    }));

    addTwoRemoteFrames();
    pid_t newSharedProcess = [webView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_EQ(newSharedProcess, [webView mainFrame].childFrames[1].info._processIdentifier);
    EXPECT_NE(newSharedProcess, [webView mainFrame].info._processIdentifier);
    EXPECT_NE(newSharedProcess, sharedProcess);
}

TEST(SiteIsolation, SharedProcessMostBasic)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame, { { RemoteFrame } } } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s, { { "https://apple.com"_s } } } }
        },
    });
}

TEST(SiteIsolation, SharedProcessSameOrigin)
{
    HTTPServer server({
        { "/top"_s, { "<!DOCTYPE html><iframe src='./subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><p>hi</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/top"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { "https://example.com"_s } },
        },
    });
}

TEST(SiteIsolation, SharedProcessExcludesLoopback)
{
    HTTPServer localServer({
        { "/local"_s, { "hi"_s } },
    }, HTTPServer::Protocol::Https);

    HTTPServer server({
        { "/example"_s, { makeString("<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://127.0.0.1:"_s, localServer.port(), "/local'></iframe>"_s) } },
        { "/webkit"_s, { "hi"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    setFeatureEnabled(viewConfiguration.get(), @"SiteIsolationSharedProcessEnabled", true);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { RemoteFrame }, { makeString("https://127.0.0.1:"_s, localServer.port()) } }
        },
    });
}

TEST(SiteIsolation, SharedProcessLoadIsolatedSiteInSubframeOfNewWindow)
{
    HTTPServer server({
        { "/opener"_s, { "<!DOCTYPE html><a href='javascript:window.open(`https://b.com/top`, `newWindow`);'>opener</a>"_s } },
        { "/top"_s, { "<!DOCTYPE html><iframe src='https://a.com/content'></iframe>"_s } },
        { "/content"_s, { "<!DOCTYPE html><p>hi</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block auto *sharedNavigationDelegate = navigationDelegate.get();
    __block RetainPtr<TestWKWebView> opendWebView;
    RetainPtr uiDelegate = adoptNS([[TestUIDelegate new] init]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opendWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 200) configuration:configuration]);
        opendWebView.get().navigationDelegate = sharedNavigationDelegate;
        return opendWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/opener"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), { { "https://a.com"_s, } });

    [webView evaluateJavaScript:@"document.querySelector('a').click()" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opendWebView.get(), {
        {
            "https://b.com"_s,
            { { RemoteFrame } },
        },
        {
            RemoteFrame,
            { { "https://a.com"_s } },
        },
    });

    auto *mainFrameInfo = [webView mainFrame].info;
    auto *subFrameInfo = [opendWebView mainFrame].childFrames[0].info;
    EXPECT_STREQ("a.com", mainFrameInfo.request.URL.host.UTF8String);
    EXPECT_STREQ("b.com", [opendWebView mainFrame].info.request.URL.host.UTF8String);
    EXPECT_STREQ("a.com", subFrameInfo.request.URL.host.UTF8String);
    EXPECT_EQ(mainFrameInfo._processIdentifier, subFrameInfo._processIdentifier);
}

TEST(SiteIsolation, SharedProcessBasicNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script>fetch('/example')</script>"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded second iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe'); document.body.appendChild(iframe); iframe.src = 'https://webkit.org/alert_when_loaded'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded second iframe");

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, SharedProcessWithWebsitePolicies)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded alert iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([navigationAction.request.URL.host isEqual:@"apple.com"] || [navigationAction.request.URL.path isEqual:@"alert_when_loaded"])
            preferences._prefersIsolatedProcess = YES;
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame } }
        },
    });

    [webView evaluateJavaScript:@"document.body.appendChild(document.createElement('iframe')).src = 'https://w3.org/alert_when_loaded'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded alert iframe");

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, SharedProcessBasicWebProcessCache)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/w3c"_s, { "w3c"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://w3.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://w3.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1B = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2B = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);
    EXPECT_NE(childFrameProcess1B, childFrameProcess1);
    EXPECT_EQ(childFrameProcess1B, childFrameProcess2B);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcessC = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1C = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2C = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_EQ(mainFrameProcessC, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1C, childFrameProcess1);
    EXPECT_EQ(childFrameProcess2C, childFrameProcess2);
}

TEST(SiteIsolation, SharedProcessInProcessCacheAfterNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/other"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/plain"_s, { "<!DOCTYPE html><p>plain"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/apple"_s, { "apple"_s } },
        { "/w3c"_s, { "w3c"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes, nil, nil, nil, EnableBackForwardCache::Yes);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool finished = false;
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finished = true;
    };

    for (unsigned i = 0; i < 25; ++i) {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://other.com/plain"]]];
        TestWebKitAPI::Util::run(&finished);
        finished = false;

        [webView goBack];
        TestWebKitAPI::Util::run(&finished);
        finished = false;

        [webView reload];
        Util::runFor(0.1_s);
        [webView reload];
        TestWebKitAPI::Util::run(&finished);
        finished = false;
    }
}

#if USE(RUNNINGBOARD)
TEST(SiteIsolation, SharedProcessDropsPageLoadActivityAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/safari"_s, { "<body>safari</body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, nil, nil, nil, EnableBackForwardCache::Yes);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });
    auto sharedProcess = [webView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_NE(sharedProcess, [webView mainFrame].info._processIdentifier);

    // Cross-site navigation. The example.com page enters the back/forward cache, which keeps the
    // old BrowsingContextGroup and webkit.org remote page alive.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/safari"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // The shared process is still alive so the cached example.com page can be restored, but it
    // shouldn't be holding on to any page load activity.
    EXPECT_TRUE(processStillRunning(sharedProcess));
    EXPECT_EQ([WKWebView _suspendedRemotePageNetworkActivityCountForTesting], 0u);
}
#endif // USE(RUNNINGBOARD)

TEST(SiteIsolation, WebProcessCacheCrashWithZeroSharedProcess)
{
    HTTPServer server({
        { "/page"_s, { "<!DOCTYPE html><p>page"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    pid_t previousPID = 0;
    unsigned cacheSize = webView.get().configuration.processPool._processCacheCapacity;
    for (unsigned i = 0; i <= cacheSize + 1; ++i) {
        auto url = makeString("https://domain-"_s, i, ".com/page"_s);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url.createNSString().get()]]];
        [navigationDelegate waitForDidFinishNavigation];
        pid_t currentPID = [webView mainFrame].info._processIdentifier;
        EXPECT_NE(currentPID, previousPID);
        previousPID = currentPID;
    }
}

TEST(SiteIsolation, SharedProcessWebProcessCacheCanEvict)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);

    RetainPtr<WKProcessPool> processPool = webView.get().configuration.processPool;
    [processPool _setCachedProcessLifetimeForTesting:0];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://w3.org/empty"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), { { "https://w3.org"_s, } });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);

    // No processes should be in the cache due to eviction timeout of 0.
    bool cacheIsEmpty = Util::waitFor([&] {
        return ![processPool _processCacheSize];
    });
    EXPECT_TRUE(cacheIsEmpty);
}

TEST(SiteIsolation, SharedProcessBasicWebProcessCacheCrash)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/first"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/second"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/w3c"_s, { "w3c"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/first"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://w3.org"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/second"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://w3.org"_s } }
        },
    });

    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1B = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2B = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_EQ(mainFrameProcessB, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1B, childFrameProcess1);
    EXPECT_EQ(childFrameProcess2B, childFrameProcess2);
}

TEST(SiteIsolation, SharedProcessWithResourceLoadStatistics)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded alert iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessWithResourceLoadStatisticsTestDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessWithResourceLoadStatisticsTestITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    [defaultFileManager removeItemAtPath:itpRoot.path error:nil];
    // Its recorded import would make a second run of this binary skip the import entirely.
    [defaultFileManager removeItemAtPath:dataStoreRoot.path error:nil];

    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    auto database = makeUniqueRef<WebCore::SQLiteDatabase>();
    EXPECT_TRUE(database->open(itpDatabaseFile.path));
    EXPECT_TRUE(database->executeCommand("UPDATE ObservedDomains SET hadUserInteraction = 1 WHERE registrableDomain = 'webkit.org'"_s));
    database->close();

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, dataStoreRoot, itpRoot);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, PartitionWebProcessCache)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/first"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/second"_s, { "<!DOCTYPE html><iframe src='https://example.com/empty'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/first"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess = [webView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_NE(childFrameProcess, mainFrameProcess);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example2.com/second"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example2.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://example.com"_s } }
        },
    });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    auto childFrameProcessB = [webView mainFrame].childFrames[0].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);
    EXPECT_NE(childFrameProcessB, childFrameProcess);

    EXPECT_NE(mainFrameProcess, childFrameProcessB);
}

static auto advanceFocusAcrossFramesMainFrame = R"FOCUSRESOURCE(
<script>

function sendResult(msg) {
    window.webkit.messageHandlers.testHandler.postMessage(msg);
}

function postResult(event) {
    sendResult(event.data)
}

addEventListener('message', postResult, false);

</script>
<div id="div1" tabindex="1">Main 1</div><br>
<div id="div2" tabindex="2">Main 2</div><br>
<iframe id="iframe1" src="https://webkit.org/iframe"></iframe><br>
<script>
document.body.addEventListener("focus", (event) => {
    sendResult('main - focus body', '*');
});
document.getElementById("div1").addEventListener("focus", (event) => {
    sendResult('main - focus div1', '*');
});
document.getElementById("div2").addEventListener("focus", (event) => {
    sendResult('main - focus div2', '*');
});
document.getElementById("iframe1").addEventListener("focus", (event) => {
    sendResult('main - focus iframe element', '*');
});
</script>
)FOCUSRESOURCE"_s;

static auto advanceFocusAcrossFramesChildFrame = R"FOCUSRESOURCE(
<div id="div1" tabindex="1">Child 1</div><br>
<div id="div2" tabindex="2">Child 2</div><br>
<div id="log">Initial logging</div>
<script>
document.body.addEventListener("focus", (event) => {
    parent.postMessage('iframe - focus body', '*');
});
document.getElementById("div1").addEventListener("focus", (event) => {
    parent.postMessage('iframe - focus div1', '*');
});
document.getElementById("div2").addEventListener("focus", (event) => {
    parent.postMessage('iframe - focus div2', '*');
});
</script>
)FOCUSRESOURCE"_s;

TEST(SiteIsolation, AdvanceFocusAcrossFrames)
{
    HTTPServer server({
        { "/example"_s, { advanceFocusAcrossFramesMainFrame } },
        { "/iframe"_s, { advanceFocusAcrossFramesChildFrame } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    auto webView = WTF::move(webViewAndDelegates.webView);
    auto messageHandler = WTF::move(webViewAndDelegates.messageHandler);
    auto navigationDelegate = WTF::move(webViewAndDelegates.navigationDelegate);
    auto uiDelegate = WTF::move(webViewAndDelegates.uiDelegate);

    __block RetainPtr<NSString> mostRecentMessage;
    __block bool messageReceived = false;
    [messageHandler setDidReceiveScriptMessage:^(NSString *message) {
        mostRecentMessage = message;
        messageReceived = true;
    }];

    uiDelegate.get().takeFocus = ^(WKWebView *, _WKFocusDirection) {
        mostRecentMessage = @"Chrome focus taken";
        messageReceived = true;
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [[webView window] makeKeyWindow];
#if PLATFORM(MAC)
    [NSApp _setKeyWindow:[webView window]];
    [[webView window] makeFirstResponder:webView.get()];
#else
    [webView becomeFirstResponder];
#endif
    [webView waitForNextPresentationUpdate];

    NSArray *expectedMessages = @[
        @"main - focus div1",
        @"main - focus div2",
        @"iframe - focus div1",
        @"iframe - focus div2",
        @"Chrome focus taken"
    ];
    size_t currentExpected = 0;

    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);
    messageReceived = false;

    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);
}

TEST(SiteIsolation, HitTesting)
{
    auto text = "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum "_s;

    HTTPServer server({
        { "/example"_s, { makeString(
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<iframe id=iframeid1 src='https://webkit.org/webkitframe'></iframe>"
            "<iframe id=iframeid2 src='/exampleframe'></iframe>"
            "<div id=mainframediv>"_s, text, text, "</div>"_s
        ) } },
        { "/webkitframe"_s, { makeString("<div id=webkitiframediv>"_s, text, text, "</div>"_s) } },
        { "/exampleframe"_s, { makeString("<div id=exampleiframediv>"_s, text, text, "</div>"_s) } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto hitTestResult = [](RetainPtr<WKWebView> webView, CGPoint point, WKFrameInfo *coordinateFrame = nil) -> RetainPtr<_WKJSHandle> {
        __block bool done { false };
        __block RetainPtr<_WKJSHandle> result;
        [webView _hitTestAtPoint:point inFrameCoordinateSpace:coordinateFrame inContentWorld:WKContentWorld.pageWorld completionHandler:^(_WKJSHandle *node, NSError *error) {
            done = true;
            EXPECT_NE(!node, !error);
            result = node;
        }];
        Util::run(&done);
        return result;
    };

    auto hitNodePrototypeAndParentElement = [&](RetainPtr<TestWKWebView> webView, CGPoint point, WKFrameInfo *coordinateFrame = nil) -> NSString * {
        auto node = hitTestResult(webView, point, coordinateFrame);
        EXPECT_EQ([node world], WKContentWorld.pageWorld);
        if (!node)
            return @"(error)";
        return [webView objectByCallingAsyncFunction:@"return Object.getPrototypeOf(n).toString() + ' ' + n.id + ', child of ' + n.parentElement?.id" withArguments:@{ @"n" : node.get() } inFrame:node.get().frame inContentWorld:WKContentWorld.pageWorld];
    };

    auto runTest = [&](bool withSiteIsolation) {
        RetainPtr configuration = server.httpsProxyConfiguration();
        if (withSiteIsolation)
            enableSiteIsolation(configuration.get());

        constexpr size_t widthWiderThanTwoIframes { 650 };
        constexpr size_t heightShorterThanHitTestCoordinates { 100 };
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, widthWiderThanTwoIframes, heightShorterThanHitTestCoordinates) configuration:configuration.get()]);
#if PLATFORM(MAC)
        // on iOS this is a race condition because iOS proactively launches the web content process,
        // which sometimes makes a main frame before the hit test request and sometimes does not.
        EXPECT_FALSE(hitTestResult(webView, CGPointMake(100, 100)));
#endif

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [webView _test_waitForDidFinishNavigationWhileIgnoringSSLErrors];

        auto hitTestPointInMainFrame = [=](size_t x, size_t y, const char* expected) {
            EXPECT_WK_STREQ(hitNodePrototypeAndParentElement(webView, CGPointMake(x, y)), expected);
        };
        hitTestPointInMainFrame(40, 40, "[object Text] undefined, child of webkitiframediv");
        hitTestPointInMainFrame(340, 40, "[object Text] undefined, child of exampleiframediv");
        hitTestPointInMainFrame(40, 240, "[object Text] undefined, child of mainframediv");
        hitTestPointInMainFrame(300, 240, "[object Text] undefined, child of mainframediv");
        hitTestPointInMainFrame(340, 300, "[object Text] undefined, child of mainframediv");

        RetainPtr iframe = [webView firstChildFrame];
        auto hitTestPointInIFrame = [=](size_t x, size_t y, const char* expected) {
            EXPECT_WK_STREQ(hitNodePrototypeAndParentElement(webView, CGPointMake(x, y), iframe.get()), expected);
        };
        hitTestPointInIFrame(10, 10, "[object Text] undefined, child of webkitiframediv");
        hitTestPointInIFrame(260, 160, "[object HTMLDivElement] webkitiframediv, child of ");
        hitTestPointInIFrame(300, 220, "[object HTMLHtmlElement] , child of undefined");
    };
    runTest(true);
    runTest(false);
}

TEST(SiteIsolation, HitTestingInContentWorld)
{
    auto text = "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum "_s;

    HTTPServer server({
        { "/example"_s, { makeString(
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<div id=mainframediv>"_s, text, text, "</div>"_s
        ) } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<WKContentWorld> contentWorld = [WKContentWorld worldWithName:@"HitTestingContentWorld"];
    EXPECT_NE(contentWorld.get(), WKContentWorld.pageWorld);

    auto hitTestResult = [&](RetainPtr<WKWebView> webView, CGPoint point) -> RetainPtr<_WKJSHandle> {
        __block bool done { false };
        __block RetainPtr<_WKJSHandle> result;
        [webView _hitTestAtPoint:point inFrameCoordinateSpace:nil inContentWorld:contentWorld.get() completionHandler:^(_WKJSHandle *node, NSError *error) {
            done = true;
            EXPECT_NE(!node, !error);
            result = node;
        }];
        Util::run(&done);
        return result;
    };

    auto runTest = [&](bool withSiteIsolation) {
        RetainPtr configuration = server.httpsProxyConfiguration();
        if (withSiteIsolation)
            enableSiteIsolation(configuration.get());

        constexpr size_t widthWiderThanTwoIframes { 650 };
        constexpr size_t heightShorterThanHitTestCoordinates { 100 };
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, widthWiderThanTwoIframes, heightShorterThanHitTestCoordinates) configuration:configuration.get()]);

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [webView _test_waitForDidFinishNavigationWhileIgnoringSSLErrors];

        auto node = hitTestResult(webView, CGPointMake(40, 40));
        EXPECT_NOT_NULL(node.get());
        if (!node)
            return;

        // The handle is associated with the content world it was requested for, not the page world.
        EXPECT_EQ([node world], contentWorld.get());

        // The handle can be used to reference the hit node when calling JavaScript in that same content world.
        NSString *result = [webView objectByCallingAsyncFunction:@"return Object.getPrototypeOf(n).toString() + ', child of ' + n.parentElement?.id" withArguments:@{ @"n" : node.get() } inFrame:node.get().frame inContentWorld:contentWorld.get()];
        EXPECT_WK_STREQ(result, "[object Text], child of mainframediv");
    };
    runTest(true);
    runTest(false);
}

TEST(SiteIsolation, HitTestingInScrolledCrossOriginIframe)
{
    auto runTest = [](int iframeTop) {
        HTTPServer server({
            { "/example"_s, { makeString(
                "<body style='margin: 0; height: 2000px'><iframe style='position: absolute; left: 10px; top: "_s, iframeTop,
                "px; width: 200px; height: 200px; border: none' src='https://webkit.org/iframe'></iframe></body>"_s) } },
            { "/iframe"_s, { "<body style='margin: 0'>"
                "<div id=first style='height: 200px'></div>"
                "<div id=second style='height: 200px'></div>"
                "<div id=third style='height: 200px'></div>"
                "</body>"_s } },
        }, HTTPServer::Protocol::HttpsProxy);

        RetainPtr configuration = server.httpsProxyConfiguration();
        enableSiteIsolation(configuration.get());

        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [webView _test_waitForDidFinishNavigationWhileIgnoringSSLErrors];

        auto hitElementID = [&] {
            __block bool done { false };
            __block RetainPtr<_WKJSHandle> node;
            [webView _hitTestAtPoint:CGPointMake(110, iframeTop + 100) inFrameCoordinateSpace:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(_WKJSHandle *result, NSError *) {
                node = result;
                done = true;
            }];
            Util::run(&done);
            if (!node)
                return RetainPtr<NSString> { @"(none)" };
            return RetainPtr<NSString> { [webView objectByCallingAsyncFunction:@"return n.id" withArguments:@{ @"n" : node.get() } inFrame:node.get().frame inContentWorld:WKContentWorld.pageWorld] };
        };

        RetainPtr childFrame = [webView firstChildFrame];
        EXPECT_TRUE(Util::waitFor([&] {
            return [hitElementID() isEqualToString:@"first"];
        })) << "iframeTop=" << iframeTop;

        // Ensure that hit testing in scrolled cross-origin frames still works even after enabling FrameViewportInfo update throttling.
        for (auto [scrollY, expectedID] : { std::pair { 200, @"second" }, std::pair { 400, @"third" } }) {
            RetainPtr script = [NSString stringWithFormat:@"window.scrollTo(0, %d)", scrollY];
            [webView objectByEvaluatingJavaScript:script.get() inFrame:childFrame.get()];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrame.get()] intValue] == scrollY;
            }));
            [webView waitForNextPresentationUpdate];

            EXPECT_TRUE(Util::waitFor([&] {
                return [hitElementID() isEqualToString:expectedID];
            })) << "iframeTop=" << iframeTop << " scrollY=" << scrollY;
        }
    };

    for (int iframeTop : { 20, 600 })
        runTest(iframeTop);
}

TEST(SiteIsolation, WKFrameInfo_isSameFrame)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "hello"_s } },
        { "/apple"_s, { "world"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> mainFrameInfo;
    [webView _frames:^(_WKFrameTreeNode *result) {
        mainFrameInfo = result.info;
    }];
    while (!mainFrameInfo)
        Util::spinRunLoop();

    __block RetainPtr<_WKFrameTreeNode> frames;
    [webView _frames:^(_WKFrameTreeNode *result) {
        frames = result;
    }];
    while (!frames)
        Util::spinRunLoop();

    EXPECT_TRUE([mainFrameInfo _isSameFrame:[frames info]]);
    EXPECT_FALSE([mainFrameInfo _isSameFrame:[[[frames childFrames]objectAtIndex:0] info]]);
    EXPECT_FALSE([mainFrameInfo _isSameFrame:[[[frames childFrames]objectAtIndex:1] info]]);
}

TEST(SiteIsolation, AlternateRequest)
{
    auto alertLocation = "<script>alert(window.location)</script>"_s;
    HTTPServer server({
        { "/example1"_s, { "<script>window.location = 'https://example.com/example2'</script>"_s } },
        { "/example2"_s, { alertLocation } },
        { "/example3"_s, { alertLocation } },
        { "/webkit1"_s, { "<script>window.location = 'https://example.com/example4'</script>"_s } },
        { "/webkit2"_s, { alertLocation } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        NSString *url = action.request.URL.absoluteString;
        if ([url isEqualToString:@"https://example.com/example2"])
            preferences._alternateRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example3"]];
        if ([url isEqualToString:@"https://example.com/example3"])
            EXPECT_FALSE(true);
        if ([url isEqualToString:@"https://example.com/example4"])
            preferences._alternateRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit2"]];
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example1"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://example.com/example3");
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit1"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://webkit.org/webkit2");
}

TEST(SiteIsolation, StatusBarVisibility)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit', '_blank', 'popup=no,menubar=no,status=no,toolbar=no,resizable=no,location=no,scrollbars=no,fullscreen=no')</script>"_s } },
        { "/webkit"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    NSString *statusBarVisible = @"window.statusbar.visible";
    EXPECT_TRUE([[opener.webView objectByEvaluatingJavaScript:statusBarVisible] boolValue]);
    EXPECT_TRUE([[opened.webView objectByEvaluatingJavaScript:statusBarVisible] boolValue]);
    EXPECT_TRUE([[opened.webView objectByEvaluatingJavaScript:statusBarVisible inFrame:[opened.webView firstChildFrame]] boolValue]);
}

TEST(SiteIsolation, LocalIframeOpensBlobURLFromFileMainFrame)
{
    auto iframeHTML = "<script>"
    "const htmlContent = ` <!DOCTYPE html> <html> <h1>Blob URL Loaded</h1> <script>window.webkit.messageHandlers.testHandler.postMessage('blob url loaded');<\\/script> </html> `;"
    "const blob = new Blob([htmlContent], { type: 'text/html' });"
    "const blobUrl = URL.createObjectURL(blob);"
    "const newWindow = window.open(blobUrl, '_blank', 'width=800,height=600,scrollbars=yes,resizable=yes');"
    "window.parent.postMessage('ping', '*');"
    "</script>"
    "<h1>blob-popup-local-iframe</h1>"_s;

    HTTPServer server({
    { "/blob-popup-local-iframe.html"_s, { iframeHTML } },
    }, HTTPServer::Protocol::Http, nullptr, nullptr, 8001);

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

    __block RetainPtr<WKWebView> blobWindow;
    __block bool blobWindowOpened = false;
    __block bool blobContentChecked = false;

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        EXPECT_WK_STREQ([action.request.URL scheme], @"blob");
        blobWindowOpened = true;
        blobWindow = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        return blobWindow.get();
    };

    [webView setUIDelegate:uiDelegate.get()];
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    [messageHandler addMessage:@"blob url loaded" withHandler:^{
        blobContentChecked = true;
    }];

    NSURL *file = [NSBundle.test_resourcesBundle URLForResource:@"blob-popup-file-mainframe" withExtension:@"html"];
    [webView loadFileURL:file allowingReadAccessToURL:file.URLByDeletingLastPathComponent];

    TestWebKitAPI::Util::run(&blobContentChecked);

    EXPECT_TRUE(blobWindowOpened);
    EXPECT_NOT_NULL(blobWindow.get());
}

TEST(SiteIsolation, CrossSiteIframeOpenWindowWithBlobURL)
{
    auto iframeHTML = "<script>"
    "   const blob = new Blob(['<script>function alertOpener() { alert(!!window.opener); }<\\/script>'], { type: 'text/html' });"
    "   const blobURL = URL.createObjectURL(blob);"
    "   window.open(blobURL);"
    "</script>"_s;

    HTTPServer server({
        { "/main"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { iframeHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/main");
    [opened.webView evaluateJavaScript:@"alertOpener()" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");

    // LocalDOMWindow applies the noopener window feature itself for a blob URL opened from a
    // document cross-origin with its top origin, so this window gets a browsing context group of
    // its own. The iframe is in the group's shared process, which only hosts subframes, so the
    // window loads in a process of its own too.
    pid_t openerMainFramePID = [opener.webView mainFrame].info._processIdentifier;
    pid_t openerIframePID = [opener.webView firstChildFrame]._processIdentifier;
    pid_t openedMainFramePID = [opened.webView mainFrame].info._processIdentifier;
    EXPECT_NE(openerMainFramePID, openerIframePID);
    EXPECT_NE(openerMainFramePID, openedMainFramePID);
    EXPECT_NE(openerIframePID, openedMainFramePID);
}

static IMP originalAddSublayer;
static unsigned redundantAddSublayerCount;

static void addSublayerCountingRedundantInsertions(CALayer *self, SEL selector, CALayer *layer)
{
    if (layer.superlayer == self)
        ++redundantAddSublayerCount;
    reinterpret_cast<void (*)(CALayer *, SEL, CALayer *)>(originalAddSublayer)(self, selector, layer);
}

TEST(SiteIsolation, CommitsFromCrossSiteIframeDoNotReparentItsLayers)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='width: 300px; height: 200px; border: none' src='https://domain2.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<body style='background-color: green'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrame = [webView firstChildFrame];

    originalAddSublayer = class_getMethodImplementation(CALayer.class, @selector(addSublayer:));
    redundantAddSublayerCount = 0;
    InstanceMethodSwizzler swizzler { CALayer.class, @selector(addSublayer:), reinterpret_cast<IMP>(addSublayerCountingRedundantInsertions) };

    [webView objectByEvaluatingJavaScript:@"window.ticks = 0;"
        "(function tick() {"
        "    document.body.style.backgroundColor = window.ticks % 2 ? 'green' : 'blue';"
        "    if (++window.ticks < 10)"
        "        requestAnimationFrame(tick);"
        "})();" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.ticks" inFrame:childFrame.get()] intValue] >= 10;
    }));
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(redundantAddSublayerCount, 0u);
}

#if ENABLE(IMAGE_ANALYSIS)

static RetainPtr<WKWebViewConfiguration> createWebViewConfigurationWithTextRecognitionEnhancements()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"TextRecognitionInVideosEnabled"] || [key isEqualToString:@"VisualTranslationEnabled"] || [key isEqualToString:@"RemoveBackgroundEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }
    [configuration _setAttachmentElementEnabled:YES];
#if ENABLE(SERVICE_CONTROLS)
    [configuration _setImageControlsEnabled:YES];
#endif
    return configuration;
}

TEST(SiteIsolation, IframeImageTranslation)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://apple.com/multiple-images.html'></iframe>"_s } },
        { "/multiple-images.html"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-images" withExtension:@"html"]] } },
        { "/large-red-square.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-200px.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-200px" withExtension:@"png"]] } },
        { "/test.jpg"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"jpg"]] } },
        { "/400x400-green.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"400x400-green" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-100px.tiff"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-100px" withExtension:@"tiff"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = createWebViewConfigurationWithTextRecognitionEnhancements();

    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebViewImageAnalysisTests alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:5];
    gDidProcessRequestCount = 0;
}

TEST(SiteIsolation, IframeImageTranslationIfIframeIsAddedAfterTranslationCall)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    HTTPServer server({
        { "/example"_s, {
        "<script>"
        "    function addCrossDomainIframe() {"
        "        const iframe = document.createElement('iframe');"
        "        iframe.src = 'https://apple.com/multiple-images.html';"
        "        iframe.width = '100%';"
        "        iframe.height = '700';"
        "        iframe.style='border:none';"
        "        document.body.appendChild(iframe);"
        "    }"
        "</script>"_s } },

        { "/multiple-images.html"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-images" withExtension:@"html"]] } },
        { "/large-red-square.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-200px.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-200px" withExtension:@"png"]] } },
        { "/test.jpg"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"jpg"]] } },
        { "/400x400-green.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"400x400-green" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-100px.tiff"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-100px" withExtension:@"tiff"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = createWebViewConfigurationWithTextRecognitionEnhancements();

    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebViewImageAnalysisTests alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView _startImageAnalysis:nil target:nil];

    [webView evaluateJavaScript: @"addCrossDomainIframe();" completionHandler:nil];
    [webView waitForImageAnalysisRequests:5];

    NSArray<NSString *> *overlaysAsText = [webView objectByEvaluatingJavaScript:@"imageOverlaysAsText();" inFrame:[webView firstChildFrame]];
    EXPECT_EQ(overlaysAsText.count, 5U);
    for (NSString *overlayText in overlaysAsText)
        EXPECT_WK_STREQ(overlayText, @"Foo bar");

    gDidProcessRequestCount = 0;
}

#if ENABLE(SERVICE_CONTROLS)

const ASCIILiteral imageServiceControlTestMainPage =
    "<body style='margin: 0'>"_s
    "  <iframe style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe>"_s
    "</body>"_s;

const ASCIILiteral imageServiceControlTestScrolledMainPage =
    "<body style='margin: 0'>"_s
    "  <div style='height: 1000px'></div>"_s
    "  <iframe style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe>"_s
    "  <div style='height: 1000px'></div>"_s
    "</body>"_s;

const ASCIILiteral imageServiceControlTestIframePage =
    "<!DOCTYPE html>"_s
    "<body style='margin: 0'>"_s
    "  <img style='margin: 50px; width: 100px; height: 100px;' src='https://webkit.org/image.png'>"_s
    "</body>"_s;

const ASCIILiteral imageServiceControlTestScrolledIframePage =
    "<!DOCTYPE html>"_s
    "<body style='margin: 0'>"_s
    "  <div style='height: 1000px'></div>"_s
    "  <img style='margin: 50px; width: 100px; height: 100px;' src='https://webkit.org/image.png'>"_s
    "  <div style='height: 1000px'></div>"_s
    "</body>"_s;

static void testImageServiceControlledImageBounds(const ASCIILiteral& mainFrameHTML, const ASCIILiteral& iframeHTML, int mainFrameScrollY = 0, int iframeScrollY = 0)
{
    HTTPServer server({
        { "/mainframe"_s, mainFrameHTML },
        { "/iframe"_s, iframeHTML },
        { "/image.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = createWebViewConfigurationWithTextRecognitionEnhancements();

    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [[webView window] orderFrontRegardless];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    if (mainFrameScrollY)
        scrollFrameAndWait(webView, nil, mainFrameScrollY);
    if (iframeScrollY)
        scrollFrameAndWait(webView, [webView firstChildFrame], iframeScrollY);
    [webView waitForNextPresentationUpdate];

    // Capture the screen-space rect that controlledImageBounds is converted to in setupServicesMenu().
    __block bool sourceFrameSet = false;
    __block NSRect capturedSourceFrame = NSZeroRect;
    InstanceMethodSwizzler sourceFrameSwizzler {
        NSClassFromString(@"WKSharingServicePickerDelegate"),
        NSSelectorFromString(@"setSourceFrame:"),
        imp_implementationWithBlock(^(id, NSRect frame) {
            capturedSourceFrame = frame;
            sourceFrameSet = true;
        })
    };

    // getAttachmentIdentifier triggers setImageMenuEnabled(true), which causes the
    // image-controls button to appear in the shadow root.
    NSString *clickScript =
        @"const img = document.querySelector('img');"
        @"HTMLAttachmentElement.getAttachmentIdentifier(img);"
        @"let button;"
        @"do {"
        @"    await new Promise(requestAnimationFrame);"
        @"    const root = internals.shadowRoot(img);"
        @"    button = root && root.getElementById('image-controls-button');"
        @"} while (!button);"
        @"button.click();";
    [webView callAsyncJavaScript:clickScript arguments:nil inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];

    // If the picker menu pops up (machines with registered image sharing services), dismiss it
    // so we don't block in event-tracking mode.
    RetainPtr cancelMenuTimer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *) {
        if (NSMenu *menu = [webView _activeMenu])
            [menu cancelTracking];
    }];
    [NSRunLoop.mainRunLoop addTimer:cancelMenuTimer.get() forMode:NSEventTrackingRunLoopMode];

    // setSourceFrame: is called before popUpMenuPositioningItem:, so capturing it is sufficient.
    bool *sourceFrameSetPtr = &sourceFrameSet;
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([sourceFrameSetPtr] {
        return *sourceFrameSetPtr;
    }, 100));
    [cancelMenuTimer invalidate];

    // Image at (50, 50) in iframe coords + iframe at (100, 100) → (150, 150) in main frame.
    NSRect expectedInWebView = NSMakeRect(150, 150, 100, 100);
    NSRect expectedInWindow = [webView convertRect:expectedInWebView toView:nil];
    NSRect expectedOnScreen = [[webView window] convertRectToScreen:expectedInWindow];
    // Use a tolerance rather than exact equality: the coordinates round-trip through cross-process
    // ContentsToRootViewRect IPC and window→screen conversion, which can introduce sub-pixel error.
    EXPECT_NEAR(capturedSourceFrame.origin.x, expectedOnScreen.origin.x, 1);
    EXPECT_NEAR(capturedSourceFrame.origin.y, expectedOnScreen.origin.y, 1);
    EXPECT_NEAR(capturedSourceFrame.size.width, expectedOnScreen.size.width, 1);
    EXPECT_NEAR(capturedSourceFrame.size.height, expectedOnScreen.size.height, 1);
}

TEST(SiteIsolation, ImageServiceControlledImageBoundsInCrossOriginIframe)
{
    testImageServiceControlledImageBounds(imageServiceControlTestMainPage, imageServiceControlTestIframePage);
}

TEST(SiteIsolation, ImageServiceControlledImageBoundsInScrolledCrossOriginIframe)
{
    testImageServiceControlledImageBounds(imageServiceControlTestMainPage, imageServiceControlTestScrolledIframePage, 0, 1000);
}

TEST(SiteIsolation, ImageServiceControlledImageBoundsInCrossOriginIframeWithScrolledMainPage)
{
    testImageServiceControlledImageBounds(imageServiceControlTestScrolledMainPage, imageServiceControlTestIframePage, 1000, 0);
}

TEST(SiteIsolation, ImageServiceControlledImageBoundsInScrolledCrossOriginIframeWithScrolledMainPage)
{
    testImageServiceControlledImageBounds(imageServiceControlTestScrolledMainPage, imageServiceControlTestScrolledIframePage, 1000, 1000);
}

#endif // ENABLE(SERVICE_CONTROLS)

#endif // ENABLE(IMAGE_ANALYSIS)

TEST(SiteIsolation, MainPageNavigatesCrossOriginIframeToAboutBlank)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe1' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('loaded iframe1');</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    // Load main page
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    // Wait until cross-origin iframe is loaded
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded iframe1");

    // Before the main page navigates the iframe, check that
    // the iframe is in a separate process.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        },
        { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });

    // Have the main frame navigate a cross-origin child iframe
    // to about:blank.
    // The about:blank iframe should inherit the origin of it's parent,
    // or it's opener if the parent doesn't exist.
    // https://dev.w3.org/html5/spec-LC/origin-0.html
    // https://dev.w3.org/html5/spec-LC/browsers.html#about-blank-origin
    //
    // iframe goes from "https://example.com/example" -> "about:blank"
    // and inherits the origin of the origin which initiated navigation.
    [webView evaluateJavaScript:
        @"let iframe1 = document.getElementById('iframe1');"
        "iframe1.onload = () => { alert('loaded about:blank'); };"
        "iframe1.src = 'about:blank';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded about:blank");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");

    // After navigation, the cross-origin iframe has now become about:blank
    // and should have the same origin as the main page.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        },
    });
}

TEST(SiteIsolation, ChildIframeNavigatesCrossOriginGrandchildIframeToAboutBlank)
{
    HTTPServer server({
        { "/main"_s, { "<iframe id='child' src='https://example.com/child_iframe'></iframe>"_s } },
        { "/child_iframe"_s, { "<iframe id='grandchild' src='https://webkit.org/grandchild_iframe'></iframe>"_s } },
        { "/grandchild_iframe"_s, { "<script>alert('loaded webkit.org');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];

    // wait for cross-origin grandchild iframe to be loaded
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded webkit.org");

    // ensure that the cross-origin grandchild iframe is in a separate process
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s, { { RemoteFrame } } } }
        },
        { RemoteFrame,
            { { RemoteFrame, { { "https://webkit.org"_s } } } }
        },
    });

    // note that this JavaScript gets executed in the context of the
    // child iframe (which is at example.com).
    //
    // The example.com child iframe navigates the webkit.org grandchild iframe.
    // to about:blank
    [webView evaluateJavaScript:
        @"let grandchild = document.getElementById('grandchild');"
        "grandchild.onload = () => { alert('loaded about:blank'); };"
        "grandchild.src = 'about:blank';"
    inFrame:[webView firstChildFrame]
    completionHandler:nil];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded about:blank");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    auto grandChildFrame = childFrame.childFrames.firstObject;
    pid_t childFramePid = childFrame.info._processIdentifier;
    pid_t grandChildFramePid = grandChildFrame.info._processIdentifier;
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(grandChildFramePid, 0);
    EXPECT_EQ(childFramePid, grandChildFramePid);
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(grandChildFrame.info.securityOrigin.host, "example.com");

    // After navigation, the cross-origin iframe has now become about:blank
    // and should have the same origin as the main page.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s,  { { "https://example.com"_s } } } }
        },
    });
}

TEST(SiteIsolation, BrowsingContextGroupSwitchForIncompatibleCrossOriginOpenerPolicy)
{
    HTTPServer server({
        { "/coop-unsafe-none"_s, { { { "Cross-Origin-Opener-Policy"_s, "unsafe-none"_s } }, "<script>w = window.open('https://webkit.org/coop-same-origin-allow-popups')</script>"_s } },
        { "/coop-same-origin-allow-popups"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin-allow-popups"_s } }, "child"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/coop-unsafe-none");

    EXPECT_WK_STREQ(opened.webView.get().URL.host, "webkit.org");
    checkFrameTreesInProcesses(opener.webView.get(), {
        { "https://example.com"_s },
    });
    checkFrameTreesInProcesses(opened.webView.get(), {
        { "https://webkit.org"_s },
    });
}

static HTTPServer crossOriginIsolationServer()
{
    return HTTPServer({
        { "/isolated"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s } }, "<iframe src='https://webkit.org/isolated-subframe' allow='cross-origin-isolated'></iframe>"_s } },
        { "/isolated-two-subframes"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s } }, "<iframe src='https://webkit.org/isolated-subframe'></iframe><iframe src='https://apple.com/isolated-subframe'></iframe>"_s } },
        { "/isolated-nested"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s } }, "<iframe src='https://webkit.org/isolated-grandparent'></iframe>"_s } },
        { "/isolated-grandparent"_s, { { { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s }, { "Cross-Origin-Resource-Policy"_s, "cross-origin"_s } }, "<iframe src='https://apple.com/isolated-subframe'></iframe>"_s } },
        { "/isolated-opener"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s } }, "<script>onload = () => { w = window.open('https://example.com/isolated'); }</script>"_s } },
        { "/isolated-noopener-opener"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s } }, "<script>onload = () => { window.open('https://example.com/shared', '_blank', 'noopener'); }</script>"_s } },
        { "/isolated-subframe"_s, { { { "Cross-Origin-Embedder-Policy"_s, "require-corp"_s }, { "Cross-Origin-Resource-Policy"_s, "cross-origin"_s } }, "subframe"_s } },
        { "/shared"_s, { "<iframe src='https://webkit.org/shared-subframe'></iframe>"_s } },
        { "/shared-two-subframes"_s, { "<iframe src='https://webkit.org/shared-subframe'></iframe><iframe src='https://apple.com/shared-subframe'></iframe>"_s } },
        { "/shared-subframe"_s, { "subframe"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
}

enum class ExpectCrossOriginIsolated : bool { No, Yes };

static bool isCrossOriginIsolatedProcess(WKProcessPool *processPool, pid_t pid)
{
    return pid && [[processPool _crossOriginIsolatedProcessIdentifiersForTesting] containsObject:@(pid)];
}

static void expectAllProcessesCrossOriginIsolated(WKProcessPool *processPool, NSSet<_WKFrameTreeNode *> *trees, ExpectCrossOriginIsolated expected, unsigned expectedProcessCount)
{
    EXPECT_EQ(expectedProcessCount, static_cast<unsigned>([trees count]));
    RetainPtr isolatedPIDs = [processPool _crossOriginIsolatedProcessIdentifiersForTesting];
    for (_WKFrameTreeNode *root in trees) {
        pid_t pid = root.info._processIdentifier;
        EXPECT_NE(pid, 0);
        EXPECT_EQ(expected == ExpectCrossOriginIsolated::Yes, !![isolatedPIDs containsObject:@(pid)]);
    }
}

static HashSet<pid_t> processIdentifiers(NSSet<_WKFrameTreeNode *> *trees)
{
    HashSet<pid_t> pids;
    for (_WKFrameTreeNode *root in trees) {
        pid_t pid = root.info._processIdentifier;
        EXPECT_NE(pid, 0);
        if (pid)
            pids.add(pid);
    }
    return pids;
}

static void expectSharedArrayBuffer(TestWKWebView *webView, WKFrameInfo *frame, ExpectCrossOriginIsolated expected)
{
    EXPECT_WK_STREQ(expected == ExpectCrossOriginIsolated::Yes ? "has-sab" : "does-not-have-sab", [webView stringByEvaluatingJavaScript:@"self.SharedArrayBuffer ? 'has-sab' : 'does-not-have-sab'" inFrame:frame]);
}

static void expectSharedArrayBuffer(TestWKWebView *webView, ExpectCrossOriginIsolated expected)
{
    EXPECT_WK_STREQ(expected == ExpectCrossOriginIsolated::Yes ? "has-sab" : "does-not-have-sab", [webView stringByEvaluatingJavaScript:@"self.SharedArrayBuffer ? 'has-sab' : 'does-not-have-sab'"]);
}

static void expectCrossOriginIsolated(TestWKWebView *webView, ExpectCrossOriginIsolated expected)
{
    EXPECT_WK_STREQ(expected == ExpectCrossOriginIsolated::Yes ? "isolated" : "not-isolated", [webView stringByEvaluatingJavaScript:@"self.crossOriginIsolated ? 'isolated' : 'not-isolated'"]);
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupUsesIsolatedProcesses)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPool = processPoolWithBackForwardCacheDisabled();
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::No);

    RetainPtr sharedTrees = frameTrees(webView.get());
    expectAllProcessesCrossOriginIsolated(processPool.get(), sharedTrees.get(), ExpectCrossOriginIsolated::No, 2);
    pid_t sharedMainFramePID = findFramePID(sharedTrees.get(), FrameType::Local);
    pid_t sharedSubframePID = findFramePID(sharedTrees.get(), FrameType::Remote);
    expectSharedArrayBuffer(webView.get(), [webView firstChildFrame], ExpectCrossOriginIsolated::No);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::Yes);

    RetainPtr isolatedTrees = frameTrees(webView.get());
    expectAllProcessesCrossOriginIsolated(processPool.get(), isolatedTrees.get(), ExpectCrossOriginIsolated::Yes, 2);
    pid_t isolatedMainFramePID = findFramePID(isolatedTrees.get(), FrameType::Local);
    pid_t isolatedSubframePID = findFramePID(isolatedTrees.get(), FrameType::Remote);

    // Same sites, so equal PIDs would mean a reused process.
    EXPECT_NE(sharedMainFramePID, isolatedMainFramePID);
    EXPECT_NE(sharedSubframePID, isolatedSubframePID);

    expectSharedArrayBuffer(webView.get(), [webView firstChildFrame], ExpectCrossOriginIsolated::Yes);
    EXPECT_WK_STREQ("isolated", [webView stringByEvaluatingJavaScript:@"self.crossOriginIsolated ? 'isolated' : 'not-isolated'" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupUsesIsolatedProcessesForNestedFrames)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPool = processPoolWithBackForwardCacheDisabled();
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated-nested"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::Yes);
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(webView.get()).get(), ExpectCrossOriginIsolated::Yes, 3);
    expectSharedArrayBuffer(webView.get(), [webView firstChildFrame], ExpectCrossOriginIsolated::Yes);
}

TEST(SiteIsolation, ProcessesAreNotReusedAfterCrossOriginIsolatedBrowsingContextGroupSwitch)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
    processPoolConfiguration.get().pageCacheEnabled = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::Yes);

    RetainPtr isolatedTrees = frameTrees(webView.get());
    expectAllProcessesCrossOriginIsolated(processPool.get(), isolatedTrees.get(), ExpectCrossOriginIsolated::Yes, 2);
    pid_t isolatedMainFramePID = findFramePID(isolatedTrees.get(), FrameType::Local);
    pid_t isolatedSubframePID = findFramePID(isolatedTrees.get(), FrameType::Remote);

    RetainPtr prewarmedPIDs = [processPool _prewarmedProcessIdentifiersForTesting];
    EXPECT_FALSE([prewarmedPIDs containsObject:@(isolatedMainFramePID)]);
    EXPECT_FALSE([prewarmedPIDs containsObject:@(isolatedSubframePID)]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::No);

    RetainPtr sharedTrees = frameTrees(webView.get());
    expectAllProcessesCrossOriginIsolated(processPool.get(), sharedTrees.get(), ExpectCrossOriginIsolated::No, 2);
    pid_t sharedMainFramePID = findFramePID(sharedTrees.get(), FrameType::Local);
    pid_t sharedSubframePID = findFramePID(sharedTrees.get(), FrameType::Remote);
    EXPECT_NE(isolatedMainFramePID, sharedMainFramePID);
    EXPECT_NE(isolatedSubframePID, sharedSubframePID);
    expectSharedArrayBuffer(webView.get(), [webView firstChildFrame], ExpectCrossOriginIsolated::No);
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupDoesNotUseSharedProcess)
{
    auto server = crossOriginIsolationServer();
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    RetainPtr processPool = [[webView configuration] processPool];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared-two-subframes"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame }, { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s }, { "https://apple.com"_s } } }
    });
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(webView.get()).get(), ExpectCrossOriginIsolated::No, 2);
    expectSharedArrayBuffer(webView.get(), [webView firstChildFrame], ExpectCrossOriginIsolated::No);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated-two-subframes"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::Yes);
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame }, { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s }, { RemoteFrame } } },
        { RemoteFrame, { { RemoteFrame }, { "https://apple.com"_s } } }
    });
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(webView.get()).get(), ExpectCrossOriginIsolated::Yes, 3);
    expectSharedArrayBuffer(webView.get(), [webView firstChildFrame], ExpectCrossOriginIsolated::Yes);
    expectSharedArrayBuffer(webView.get(), [webView secondChildFrame], ExpectCrossOriginIsolated::Yes);
}

TEST(SiteIsolation, RelatedWebViewDoesNotJoinCrossOriginIsolatedBrowsingContextGroup)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPool = processPoolWithBackForwardCacheDisabled();
    RetainPtr isolatedConfiguration = server.httpsProxyConfiguration();
    [isolatedConfiguration setProcessPool:processPool.get()];
    auto [isolatedWebView, isolatedNavigationDelegate] = siteIsolatedViewAndDelegate(isolatedConfiguration.get());

    [isolatedWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [isolatedNavigationDelegate waitForDidFinishNavigation];
    expectCrossOriginIsolated(isolatedWebView.get(), ExpectCrossOriginIsolated::Yes);

    // A related web view must use the same website data store.
    RetainPtr relatedConfiguration = adoptNS([isolatedConfiguration copy]);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [relatedConfiguration _setRelatedWebView:isolatedWebView.get()];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto [relatedWebView, relatedNavigationDelegate] = siteIsolatedViewAndDelegate(relatedConfiguration.get());

    [relatedWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared"]]];
    [relatedNavigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(relatedWebView.get(), ExpectCrossOriginIsolated::No);
    expectSharedArrayBuffer(relatedWebView.get(), ExpectCrossOriginIsolated::No);
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(relatedWebView.get()).get(), ExpectCrossOriginIsolated::No, 2);
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupWindowOpen)
{
    auto server = crossOriginIsolationServer();
    auto [openerWebView, openedWebView] = openerAndOpenedViews(server, @"https://example.com/isolated-opener");
    RetainPtr processPool = [[openerWebView.webView configuration] processPool];

    expectCrossOriginIsolated(openerWebView.webView.get(), ExpectCrossOriginIsolated::Yes);
    expectCrossOriginIsolated(openedWebView.webView.get(), ExpectCrossOriginIsolated::Yes);
    expectSharedArrayBuffer(openedWebView.webView.get(), ExpectCrossOriginIsolated::Yes);
    expectSharedArrayBuffer(openedWebView.webView.get(), [openedWebView.webView firstChildFrame], ExpectCrossOriginIsolated::Yes);
    EXPECT_TRUE(isCrossOriginIsolatedProcess(processPool.get(), [openedWebView.webView _webProcessIdentifier]));
    EXPECT_WK_STREQ("has-opener", [openedWebView.webView stringByEvaluatingJavaScript:@"opener ? 'has-opener' : 'no-opener'"]);
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupNoopenerWindowOpen)
{
    auto server = crossOriginIsolationServer();
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    [openerWebView setNavigationDelegate:openerNavigationDelegate.get()];
    [openerWebView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<TestWKWebView> openedWebView;
    __block RetainPtr<TestNavigationDelegate> openedNavigationDelegate;
    __block pid_t openedCreationPID = 0;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        enableSiteIsolation(configuration);
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [openedNavigationDelegate allowAnyTLSCertificate];
        [openedWebView setNavigationDelegate:openedNavigationDelegate.get()];
        openedCreationPID = [openedWebView _webProcessIdentifier];
        return openedWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated-noopener-opener"]]];
    while (!openedWebView)
        Util::spinRunLoop();
    [openedNavigationDelegate waitForDidFinishNavigation];

    RetainPtr processPool = [configuration processPool];
    expectCrossOriginIsolated(openerWebView.get(), ExpectCrossOriginIsolated::Yes);
    EXPECT_NE(openedCreationPID, [openerWebView _webProcessIdentifier]);
    expectCrossOriginIsolated(openedWebView.get(), ExpectCrossOriginIsolated::No);
    expectSharedArrayBuffer(openedWebView.get(), ExpectCrossOriginIsolated::No);
    EXPECT_FALSE(isCrossOriginIsolatedProcess(processPool.get(), [openedWebView _webProcessIdentifier]));
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupNotAdoptedWhenNavigationIsCancelled)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPool = processPoolWithBackForwardCacheDisabled();
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [navigationDelegate setDecidePolicyForNavigationResponse:^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        completionHandler(WKNavigationResponsePolicyCancel);
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [navigationDelegate waitForDidFailProvisionalNavigation];
    [navigationDelegate setDecidePolicyForNavigationResponse:nil];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::No);
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(webView.get()).get(), ExpectCrossOriginIsolated::No, 2);
}

TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupAfterCrash)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPool = processPoolWithBackForwardCacheDisabled();
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [navigationDelegate waitForDidFinishNavigation];
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(webView.get()).get(), ExpectCrossOriginIsolated::Yes, 2);

    // The termination is reported synchronously, so the callback must be set before killing the process.
    __block bool didTerminate = false;
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        didTerminate = true;
    };
    [webView _killWebContentProcessAndResetState];
    Util::run(&didTerminate);
    navigationDelegate.get().webContentProcessDidTerminate = nil;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [navigationDelegate waitForDidStartProvisionalNavigation];
    EXPECT_TRUE(isCrossOriginIsolatedProcess(processPool.get(), [webView _webProcessIdentifier]));
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::Yes);
    expectAllProcessesCrossOriginIsolated(processPool.get(), frameTrees(webView.get()).get(), ExpectCrossOriginIsolated::Yes, 2);
}

// FIXME: Also test with the back/forward cache enabled, where the back/forward item's group is the only source of the mode.
TEST(SiteIsolation, CrossOriginIsolatedBrowsingContextGroupAfterBackForwardNavigation)
{
    auto server = crossOriginIsolationServer();

    RetainPtr processPool = processPoolWithBackForwardCacheDisabled();
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/isolated"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/shared"]]];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::No);

    RetainPtr sharedTrees = frameTrees(webView.get());
    expectAllProcessesCrossOriginIsolated(processPool.get(), sharedTrees.get(), ExpectCrossOriginIsolated::No, 2);
    auto sharedPIDs = processIdentifiers(sharedTrees.get());

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    expectCrossOriginIsolated(webView.get(), ExpectCrossOriginIsolated::Yes);

    // The cross-site subframe can be restored after the main frame finishes loading, so wait for its process.
    RetainPtr restoredTrees = frameTrees(webView.get());
    while ([restoredTrees count] < 2) {
        Util::spinRunLoop();
        restoredTrees = frameTrees(webView.get());
    }
    expectAllProcessesCrossOriginIsolated(processPool.get(), restoredTrees.get(), ExpectCrossOriginIsolated::Yes, 2);
    for (pid_t pid : processIdentifiers(restoredTrees.get()))
        EXPECT_FALSE(sharedPIDs.contains(pid));
}

TEST(SiteIsolation, ProcessActivityGroup)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    bool finishedLoading { false };
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
#if PLATFORM(IOS_SIMULATOR)
        // In the Simulator, we are never taking an activity when the view is hidden and we are calling
        // -[WKWebViewConfiguration _setClientNavigationsRunAtForegroundPriority:YES], so we disable this check.
        EXPECT_EQ([navigation _processActivityGroupSizeForTesting], 0u);
#else
        EXPECT_EQ([navigation _processActivityGroupSizeForTesting], 2u);
#endif
        finishedLoading = true;
    }).get();
    [webView.get().configuration _setClientNavigationsRunAtForegroundPriority:YES];
    [webView setHidden:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);
}

TEST(SiteIsolation, OpenAboutBlankFromAboutBlank)
{
    HTTPServer server({
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebView> opened;
    __block bool openedFinishedLoading { false };
    RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    openedNavigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        openedFinishedLoading = true;
    };
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedNavigationDelegate.get();
        opened.get().UIDelegate = uiDelegate.get();
        return opened.get();
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open()" completionHandler:nil];
    Util::run(&openedFinishedLoading);
}

TEST(SiteIsolation, OpenNonEmptySiteFromAboutBlank)
{
    HTTPServer server({
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebView> opened;
    __block bool openedFinishedLoading { false };
    RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    openedNavigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        openedFinishedLoading = true;
    };
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedNavigationDelegate.get();
        opened.get().UIDelegate = uiDelegate.get();
        return opened.get();
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open('https://webkit.org/webkit')" completionHandler:nil];
    Util::run(&openedFinishedLoading);
}

TEST(SiteIsolation, OpenEmptySiteFromProcessWithNonEmptySite)
{
    HTTPServer server({
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebView> opened;
    __block bool openedFinishedLoading { false };
    RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    openedNavigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        openedFinishedLoading = true;
    };
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedNavigationDelegate.get();
        opened.get().UIDelegate = uiDelegate.get();
        return opened.get();
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open()" completionHandler:nil];
    Util::run(&openedFinishedLoading);
}

TEST(SiteIsolation, MultiProcessBFCacheIframeProcessSurvival)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
        { "/c"_s, { "page c"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://c.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Iframe process must survive. Without multi-process suspension,
    // removeChildFrames() sends WebPage::Close() and kills it.
    EXPECT_TRUE(processStillRunning(iframePID));
}

// DEBUG-assert regression guard: without the fix the iframe process trips
// ASSERT(m_rootFrames.isEmpty()) (Page.cpp:577) on the eviction Close. Release has no assert,
// so the process-alive EXPECT below is only a weak secondary signal there.
TEST(SiteIsolation, MultiProcessBFCacheCrossSiteEvictionDoesNotCrashIframe)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
        { "/c"_s, { "page c"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // The web process cache keeps the b.com process alive past the navigation, so eviction's
    // Close reaches a live process and runs ~Page() instead of force-killing it.
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration setProcessPool:processPool.get()];
    setFeatureEnabled(configuration.get(), @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);

    // Cross-site navigation suspends a.com into the BFCache via SuspendedPageProxy; b.com keeps
    // a CachedPage and stays alive in the web process cache.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://c.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(processStillRunning(iframePID));

    [webView _clearBackForwardCache];

    // Let the eviction IPC (ClearCachedPage then Close) drain so a crash, if any, lands in-window.
    Util::runFor(0.5_s);
    EXPECT_TRUE(processStillRunning(iframePID));
}

// FIXME: Use openerAndOpenedViews() once MultiProcessBackForwardCacheEnabled is on by default.
TEST(SiteIsolation, MultiProcessBFCacheOpenerSkipsBFCache)
{
    HTTPServer server({
        { "/a"_s, { "<script>window.open('https://a.com/child');</script>"_s } },
        { "/child"_s, { "child page"_s } },
        { "/b"_s, { "page b"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<WKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *config, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:config]);
        return openedWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Set a BFCache marker on the opener page.
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    // Navigate to a different site to trigger PSON.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Go back.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    // Verify the marker is gone (full reload, not BFCache restore).
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteCaching)
{
    HTTPServer server({
        { "/a1"_s, { "page a1"_s } },
        { "/a2"_s, { "page a2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/a2", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteWithCrossSiteIframe)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
        { "/a2"_s, { "page a2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];
    [webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker = true" inFrame:[webView firstChildFrame]];

    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    EXPECT_NE(iframePID, 0);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
    // BFCache marker IS preserved — page was cached with iframe coordination
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
    // Iframe process was suspended, not killed
    EXPECT_TRUE(processStillRunning(iframePID));

    // Wait for the cross-process iframe's frame tree to be fully reconstructed
    // before asserting on iframe state. waitForDidFinishNavigation only waits
    // for the main frame, and waitForDidFinishLoadInSubframe does not fire
    // during BFCache restore.
    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();

    // Iframe-scope BFCache marker survives — proves iframe WebPage was actually
    // suspended and restored via UIProcess coordination, not reloaded.
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView firstChildFrame]] boolValue]);

    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));
}

TEST(SiteIsolation, MultiProcessBFCacheGoBackToIntermediateEntryDoesNotHang)
{
    // Same-site main-frame chain a -> b -> c, each hosting the same cross-site iframe. The iframe
    // process is suspended when the first entry (a) is cached, so it cannot also cache the
    // intermediate entry (b): its single live page is already suspended. Going back to b must
    // therefore fall back to a normal load instead of attempting a back/forward-cache restore of
    // iframe children that were never cached — which used to hang on a reload that never fired
    // didFinishNavigation.
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/b"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/c"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/b"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/c"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)2);

    // Back to the intermediate entry b. Must complete (not hang) and land on b.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/b", [webView URL].absoluteString);

    // The cross-site iframe subtree must be reconstructed after the fallback load — a regression
    // that completed the main-frame navigation but dropped the iframe would otherwise pass.
    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteWithCrossSiteIframeMultipleCycles)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
        { "/a2"_s, { "page a2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];
    [webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker = true" inFrame:[webView firstChildFrame]];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    Vector<ExpectedFrameTree> expectedOnA1 = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };

    for (int i = 0; i < 3; i++) {
        [webView goBack];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);

        while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedOnA1 }))
            TestWebKitAPI::Util::spinRunLoop();
        // Iframe-scope marker must survive every suspend/restore cycle.
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView firstChildFrame]] boolValue]);

        [webView goForward];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://a.com/a2", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
    }
}

// Cross-site navigation A → data: URL, then back to A. Without the SI carve-out in
// WebProcessPool::processForNavigationInternal, data: URLs are treated as same-origin
// (NavigationAction::shouldTreatAsSameOriginNavigation hard-codes that), so no swap,
// no SuspendedPageProxy, and goBack falls through to a fresh load.
TEST(SiteIsolation, MultiProcessBFCacheCrossSiteToDataURL)
{
    HTTPServer server({
        { "/a"_s, { "page a"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    [configuration _setAllowTopNavigationToDataURLs:YES];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    [webView evaluateJavaScript:@"window.location.href = \"data:text/html,<body>data url</body>\"" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://a.com/a", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
}

// Multiple sequential data: URL navigations: A -> data:1 -> data:2 -> back -> back.
// Each cross-origin navigation lands on an empty-Site target, so each step exercises
// the relaxed (non-isEmpty) suspended-page lookup. Both back-navigations must restore
// from the correct SuspendedPageProxy.
TEST(SiteIsolation, MultiProcessBFCacheCrossSiteToMultipleDataURLs)
{
    HTTPServer server({
        { "/a"_s, { "page a"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    [configuration _setAllowTopNavigationToDataURLs:YES];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__marker = 'A'"];

    [webView evaluateJavaScript:@"window.location.href = \"data:text/html,<body>data1</body><script>window.__marker='D1'</script>\"" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"D1", [webView objectByEvaluatingJavaScript:@"window.__marker"]);

    [webView evaluateJavaScript:@"window.location.href = \"data:text/html,<body>data2</body><script>window.__marker='D2'</script>\"" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"D2", [webView objectByEvaluatingJavaScript:@"window.__marker"]);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE([[webView URL].absoluteString hasPrefix:@"data:text/html,"]);
    EXPECT_WK_STREQ(@"D1", [webView objectByEvaluatingJavaScript:@"window.__marker"]);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/a", [webView URL].absoluteString);
    EXPECT_WK_STREQ(@"A", [webView objectByEvaluatingJavaScript:@"window.__marker"]);
}

// Cross-origin navigation through a string-returning javascript: URL. Top-level
// navigation to javascript: replaces the document content but does not change the
// URL or create a back-forward entry; verify that the carve-out (keyed on
// protocolIsData()) does not accidentally process-swap for this scheme.
TEST(SiteIsolation, MultiProcessBFCacheCrossSiteToJavaScriptURL)
{
    HTTPServer server({
        { "/a"_s, { "page a"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    [configuration _setAllowTopNavigationToDataURLs:YES];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    NSUInteger bfListBefore = [[[webView backForwardList] backList] count];
    pid_t pidBefore = [webView _webProcessIdentifier];

    __block bool jsRan = false;
    [webView evaluateJavaScript:@"window.location.href = \"javascript:'<body>js url</body>'\"" completionHandler:^(id, NSError *) {
        jsRan = true;
    }];
    TestWebKitAPI::Util::run(&jsRan);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_WK_STREQ(@"https://a.com/a", [webView URL].absoluteString);
    EXPECT_EQ(bfListBefore, [[[webView backForwardList] backList] count]);
    EXPECT_EQ(pidBefore, [webView _webProcessIdentifier]);
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteWithDifferentCrossSiteIframes)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/bframe'></iframe>"_s } },
        { "/bframe"_s, { "b iframe content"_s } },
        { "/a2"_s, { "<iframe src='https://c.com/cframe'></iframe>"_s } },
        { "/cframe"_s, { "c iframe content"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    EXPECT_WK_STREQ(@"https://b.com/bframe", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]]);
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 = true"];
    [webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker = true" inFrame:[webView firstChildFrame]];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    EXPECT_WK_STREQ(@"https://c.com/cframe", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]]);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 ? true : false"] boolValue]);

    // The iframe subtree is reattached after the main frame commits, so c.com is still the child frame for a moment after the navigation finishes.
    while (![[webView firstChildFrame].securityOrigin.host isEqualToString:@"b.com"])
        Util::spinRunLoop();

    EXPECT_WK_STREQ(@"https://b.com/bframe", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:[webView firstChildFrame]]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView firstChildFrame]] boolValue]);
}

TEST(SiteIsolation, IframePushStateBackForwardRoutesToIframe)
{
    HTTPServer server({
        { "/main"_s, { "<iframe src='https://a.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/main"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    WKFrameInfo *iframe = [webView firstChildFrame];

    [webView objectByEvaluatingJavaScript:@"history.pushState(null, '', '?1')" inFrame:iframe];
    [webView objectByEvaluatingJavaScript:@"history.pushState(null, '', '?2')" inFrame:iframe];
    [webView objectByEvaluatingJavaScript:@"history.pushState(null, '', '?3')" inFrame:iframe];

    EXPECT_WK_STREQ(@"?3", [webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe]);
    EXPECT_WK_STREQ(@"https://a.com/main", [webView URL].absoluteString);

    [webView objectByEvaluatingJavaScript:@"history.back()" inFrame:iframe];
    while (![[webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe] isEqualToString:@"?2"])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_WK_STREQ(@"?2", [webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe]);
    EXPECT_WK_STREQ(@"https://a.com/main", [webView URL].absoluteString);

    [webView objectByEvaluatingJavaScript:@"history.back()" inFrame:iframe];
    while (![[webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe] isEqualToString:@"?1"])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_WK_STREQ(@"?1", [webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe]);
    EXPECT_WK_STREQ(@"https://a.com/main", [webView URL].absoluteString);

    [webView objectByEvaluatingJavaScript:@"history.back()" inFrame:iframe];
    while (![[webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe] isEqualToString:@""])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_WK_STREQ(@"", [webView objectByEvaluatingJavaScript:@"location.search" inFrame:iframe]);
    EXPECT_WK_STREQ(@"https://a.com/main", [webView URL].absoluteString);
}

TEST(SiteIsolation, ClearSiteDataClearsRemoteProcessMemoryCache)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script src='https://example.com/resource'></script>"
            "<script>window.onmessage = e => {"
            "   var s = document.createElement('script');"
            "   s.onload = function() { alert('reloaded'); };"
            "   s.src = 'https://example.com/resource';"
            "   document.body.appendChild(s);"
            "};</script>"_s } },
        { "/resource"_s, { { { "Content-Type"_s, "application/javascript"_s }, { "Cache-Control"_s, "max-age=3600"_s } }, "/* script */"_s } },
        { "/clear"_s, { { { "Content-Type"_s, "text/html"_s }, { "Clear-Site-Data"_s, "\"cache\""_s } }, "cleared"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });

    auto requestCountAfterLoad = server.totalRequests();

    [webView callAsyncJavaScript:@"await fetch('/clear'); document.querySelector('iframe').contentWindow.postMessage('reload', '*');" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "reloaded");

    EXPECT_EQ(server.totalRequests(), requestCountAfterLoad + 2u);
}

TEST(SiteIsolation, MultiProcessBFCacheGoForwardSimple)
{
    HTTPServer server({
        { "/a"_s, { "page a"_s } },
        { "/b"_s, { "page b"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker = true"];

    // Cycle through back/forward 5 times to exercise BFCache eviction and
    // re-suspension. Without fixes for stale RemotePageProxy, wrong
    // frameItemID in SetIsSuspended, and SuspendedPageProxy eviction
    // killing the live WebPage, this loop fails on cycle 3+.
    for (int i = 0; i < 5; i++) {
        [webView goBack];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://a.com/a", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);

        [webView goForward];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://b.com/b", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker ? true : false"] boolValue]);
    }
}

TEST(SiteIsolation, MultiProcessBFCacheGoForward)
{
    // The bug requires C to have an iframe from a.com so that when C is
    // suspended during goBack, SetSubframesSuspended(true) is sent to
    // a.com's process — the same process that just restored A from BFCache.
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } },
        { "/c"_s, { "<iframe src='https://a.com/aframe'></iframe>"_s } },
        { "/aframe"_s, { "a.com iframe in c"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    // Step 1: Load A (a.com) with cross-site iframe (b.com).
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a = true"];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    // Step 2: Navigate to C (c.com with iframe from a.com).
    // This triggers BFCache suspension of A.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://c.com/c"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_c = true"];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://c.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://a.com"_s } } },
    });

    // Step 3: Go back (BFCache restore of A).
    // During commit, C is suspended and SuspendWithFrameItem is
    // sent to a.com's process for C's iframe — same process as A's main.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a ? true : false"] boolValue]);

    // Step 4: Go forward (navigate to C again) — this triggers the bug.
    // goToBackForwardItem is sent to a.com's process but
    // m_mainFrame->coreLocalFrame() is null, causing a silent bail-out.
    [webView goForward];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://c.com/c", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_c ? true : false"] boolValue]);
}

TEST(SiteIsolation, MultiProcessBFCacheRestoreWithCrossSiteIframeDoesNotCrash)
{
    // Regression test for bug 318179. /a1 and /a2 are same-site so the main frame is not
    // process-swapped: the back/forward item gets a BFCache entry but no SuspendedPageProxy,
    // so goBack restores via RestoreWithFrameItem dispatched straight to the iframe process
    // while /a2's subframe is still attached there.
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/a2"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { "iframe content"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 = true"];
    [webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker = true" inFrame:[webView firstChildFrame]];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    EXPECT_NE(iframePID, 0);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 ? true : false"] boolValue]);

    // Drain the RestoreWithFrameItem IPC so an iframe-process crash lands in-window.
    Util::runFor(0.5_s);
    EXPECT_TRUE(processStillRunning(iframePID));

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));

    // Reading the iframe-scope marker round-trips to the iframe process, proving it restored rather than crashed and reloaded.
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__iframeBfcacheMarker ? true : false" inFrame:[webView firstChildFrame]] boolValue]);
}

TEST(SiteIsolation, ProvisionalSubframeCommitAfterMainFrameSwapDoesNotCrash)
{
    // A cross-site subframe navigation is still provisional when the main frame commits cross-site:
    // didCommitProvisionalPage() wipes the page's frame targets, so the subframe's later commit finds
    // none. Without the fix the UI process aborts, so surviving to the end of this test is the point.
    // No inspector frontend is needed: frame targets are tracked whenever site isolation is enabled.
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "subframe content"_s } },
        { "/held"_s, { "held subframe content"_s } },
        { "/c"_s, { "page c"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // MultiProcessBackForwardCacheEnabled is load-bearing: without it the main-frame commit runs
    // removeChildFrames() and tears the provisional subframe down instead of suspending it, so no
    // late commit could ever arrive.
    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    pid_t subframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    EXPECT_NE(subframePID, 0);

    // Hold the subframe's response so its ProvisionalFrameProxy and provisional inspector target
    // exist but cannot commit until we release it.
    bool didHoldSubframeResponse { false };
    BlockPtr<void(WKNavigationResponsePolicy)> releaseSubframeResponse;
    navigationDelegate.get().decidePolicyForNavigationResponse = makeBlockPtr([&](WKNavigationResponse *navigationResponse, void (^completionHandler)(WKNavigationResponsePolicy)) {
        if (!navigationResponse.forMainFrame && [navigationResponse.response.URL.absoluteString isEqualToString:@"https://d.com/held"]) {
            releaseSubframeResponse = makeBlockPtr(completionHandler);
            didHoldSubframeResponse = true;
            return;
        }
        completionHandler(WKNavigationResponsePolicyAllow);
    }).get();

    [webView evaluateJavaScript:@"location.href = 'https://d.com/held'" inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&didHoldSubframeResponse);

    // Commit a cross-site main-frame navigation while the subframe is still provisional. This is what
    // removes every frame target belonging to the page, including the held subframe's.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://c.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Guard against passing vacuously: if the outgoing page were torn down rather than suspended,
    // there would be no subframe left to commit and the release below would be a no-op.
    EXPECT_TRUE(processStillRunning(subframePID));

    // Let the subframe commit into a page whose frame targets are gone.
    releaseSubframeResponse(WKNavigationResponsePolicyAllow);

    // Drain the commit IPC so a UI-process crash lands in-window.
    Util::runFor(0.5_s);

    EXPECT_WK_STREQ(@"https://c.com/c", [webView URL].absoluteString);
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteReusedIframeNotFrozen)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame1'></iframe>"_s } },
        { "/a2"_s, { "<iframe src='https://b.com/frame2'></iframe>"_s } },
        { "/frame1"_s, { "frame1"_s } },
        { "/frame2"_s, { "frame2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    pid_t iframePID1 = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    EXPECT_NE(iframePID1, 0);

    // frame1 renders normally before ever being cached.
    startCountingAnimationFrames(webView.get(), [webView firstChildFrame]);
    expectAnimationFrameCountToIncrease(webView.get(), [webView firstChildFrame]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    // frame2's rendering must not have been frozen by frame1's earlier caching.
    startCountingAnimationFrames(webView.get(), [webView firstChildFrame]);
    expectAnimationFrameCountToIncrease(webView.get(), [webView firstChildFrame]);
}

TEST(SiteIsolation, MultiProcessBFCacheRestoreRerendersReattachedIframe)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame1'></iframe>"_s } },
        { "/a2"_s, { "<iframe src='https://b.com/frame2'></iframe>"_s } },
        { "/frame1"_s, { "frame1"_s } },
        { "/frame2"_s, { "frame2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 = true"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 ? true : false"] boolValue]);

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));

    // Verify rendering is resumed.
    startCountingAnimationFrames(webView.get(), [webView firstChildFrame]);
    expectAnimationFrameCountToIncrease(webView.get(), [webView firstChildFrame]);
}

TEST(SiteIsolation, MultiProcessBFCacheRepeatedSameSiteSuspendCachesEachEntry)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame1'></iframe>"_s } },
        { "/a2"_s, { "<iframe src='https://b.com/frame2'></iframe>"_s } },
        { "/a3"_s, { "<iframe src='https://b.com/frame3'></iframe>"_s } },
        { "/frame1"_s, { "frame1"_s } },
        { "/frame2"_s, { "frame2"_s } },
        { "/frame3"_s, { "frame3"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    pid_t iframePID1 = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    EXPECT_NE(iframePID1, 0);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a2 = true" inFrame:[webView firstChildFrame]];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a3"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    // frame3 must render normally regardless of how many times this WebPage was suspended before.
    startCountingAnimationFrames(webView.get(), [webView firstChildFrame]);
    expectAnimationFrameCountToIncrease(webView.get(), [webView firstChildFrame]);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/a2", [webView URL].absoluteString);

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));

    // Confirm page is restored from cache.
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a2 ? true : false" inFrame:[webView firstChildFrame]] boolValue]);
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteEvictionDoesNotCrashIframe)
{
    HTTPServer server({
        { "/a1"_s, { "<iframe src='https://b.com/frame1'></iframe>"_s } },
        { "/a2"_s, { "<iframe src='https://b.com/frame2'></iframe>"_s } },
        { "/frame1"_s, { "frame1"_s } },
        { "/frame2"_s, { "frame2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 = true" inFrame:[webView firstChildFrame]];

    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    EXPECT_NE(iframePID, 0);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    [webView _clearBackForwardCache];

    startCountingAnimationFrames(webView.get(), [webView firstChildFrame]);
    // Verify process does not crash after processing ClearCachedPage message.
    EXPECT_TRUE(processStillRunning(iframePID));
    expectAnimationFrameCountToIncrease(webView.get(), [webView firstChildFrame]);

    // Confirm the entry was actually evicted and page is loaded from network.
    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 ? true : false" inFrame:[webView firstChildFrame]] boolValue]);
}

TEST(SiteIsolation, MultiProcessBFCacheSameSiteNavAfterRestore)
{
    // Regression test for stale process in processForTheFrameItem.
    // History: a.com/a1 → a.com/a2 → b.com/b
    // Cycling back through same-site pages (a2→a1) and then forward
    // to b.com exercises the code path where processForTheFrameItem
    // could return a stale cross-site process with no RemotePageProxy.
    HTTPServer server({
        { "/a1"_s, { "page a1"_s } },
        { "/a2"_s, { "page a2"_s } },
        { "/b"_s, { "page b"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 = true"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a2 = true"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_b = true"];

    for (int i = 0; i < 3; i++) {
        [webView goBack];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://a.com/a2", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a2 ? true : false"] boolValue]);

        [webView goBack];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://a.com/a1", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a1 ? true : false"] boolValue]);

        [webView goForward];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://a.com/a2", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_a2 ? true : false"] boolValue]);

        [webView goForward];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"https://b.com/b", [webView URL].absoluteString);
        EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.__bfcacheMarker_b ? true : false"] boolValue]);
    }
}

TEST(SiteIsolation, ScriptMessageHandlerDocumentIdentifierOnPageHide)
{
    HTTPServer server({
        { "/main"_s, { "<iframe id='child' src='https://webkit.org/iframe'></iframe><iframe id='keeper' src='https://webkit.org/keeper'></iframe>"_s } },
        { "/iframe"_s, { "<input type='text'>"_s } },
        { "/keeper"_s, { "keepalive"_s } },
        { "/next"_s, { "navigated"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr handler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView mainFrame].info._processIdentifier, [webView firstChildFrame]._processIdentifier);

    [webView evaluateJavaScript:@"window.addEventListener('pagehide', () => { window.webkit.messageHandlers.testHandler.postMessage('pagehide'); })" inFrame:[webView firstChildFrame] completionHandler:nil];
    [webView evaluateJavaScript:@"document.getElementById('child').src = 'https://example.com/next'" completionHandler:nil];
    WKScriptMessage *message = [handler waitForMessage];
    EXPECT_WK_STREQ(@"pagehide", message.body);
    EXPECT_NOT_NULL(message.frameInfo._documentIdentifier);
}

TEST(SiteIsolation, NonMainFrameProcessCrash)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        // Test passes if this delegate is not called for iframe process crashes.
        EXPECT_FALSE(true);
    };
    [webView setNavigationDelegate:delegate.get()];

    auto html = "<script>onload = () => {"
    "var iframe = document.createElement('iframe');"
    "document.body.appendChild(iframe);"
    "iframe.src = 'http://localhost:' + window.location.port + '/iframe';"
    "}</script>"_s;

    HTTPServer server({
        { "/"_s, { html } },
        { "/iframe"_s, { "<script>alert(internals.getpid())</script>"_s } },
    });

    [webView loadRequest:server.request()];
    NSString *iframeProcessPort = [webView _test_waitForAlert];
    kill([iframeProcessPort intValue], 9);
    Util::runFor(0.1_s);
}

TEST(SiteIsolation, PasteboardReading)
{
    auto iframehtml = "<script>function doPasteboardStuff() {"
    "navigator.clipboard.writeText('hello');"
    "navigator.clipboard.readText()"
    "    .then(text => alert(text))"
    "    .catch(()=> alert('fail'))"
    "}</script>"
    "<button onclick='doPasteboardStuff()' id='testbutton'>Click</button>"_s;

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { iframehtml } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server.httpsProxyConfiguration());
    [webView loadURL:[NSURL URLWithString:@"https://example.com/example"]];
    [delegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('testbutton').click();" inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld completionHandler:nil];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "hello");
}

TEST(SiteIsolation, DOMPasteAccessGrantedInCrossOriginFrame)
{
    auto subframeMarkup = "<script>function tryToReadPasteboard() {"
        "navigator.clipboard.readText()"
        "    .then(text => { window.readTextResult = 'PASS: ' + text; })"
        "    .catch(error => { window.readTextResult = 'FAIL: ' + error; })"
        "}</script>"
        "<button onclick='tryToReadPasteboard()' id='testbutton'>Click</button>"_s;

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { subframeMarkup } },
    }, HTTPServer::Protocol::HttpsProxy);

#if PLATFORM(MAC)
    [NSPasteboard.generalPasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
    [NSPasteboard.generalPasteboard setString:@"hello" forType:NSPasteboardTypeString];
#else
    [UIPasteboard generalPasteboard].string = @"hello";
#endif

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server.httpsProxyConfiguration());
#if PLATFORM(MAC)
    [[webView window] orderFrontRegardless];
#endif
    [webView loadURL:[NSURL URLWithString:@"https://example.com/example"]];
    [delegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"document.getElementById('testbutton').click();" inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld completionHandler:nil];

#if PLATFORM(MAC)
    bool selectedPasteItem = false;
    BlockPtr allowPasteHandler = makeBlockPtr([webView, &selectedPasteItem](NSTimer *timer) {
        RetainPtr activeMenu = [webView _activeMenu];
        if (!activeMenu)
            return;

        for (NSMenuItem *item in [activeMenu itemArray]) {
            if ([item.title isEqualToString:@"Paste"]) {
                [activeMenu performActionForItemAtIndex:[activeMenu indexOfItem:item]];
                [activeMenu cancelTracking];
                [timer invalidate];
                selectedPasteItem = true;
                break;
            }
        }
    });

    RetainPtr selectPasteItemTimer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:allowPasteHandler.get()];
    [NSRunLoop.mainRunLoop addTimer:selectPasteItemTimer forMode:NSEventTrackingRunLoopMode];
    Util::run(&selectedPasteItem);
#else
    __block bool shownMenu = false;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    InstanceMethodSwizzler showMenuSwizzler {
        UIMenuController.class,
        @selector(showMenuFromView:rect:),
        imp_implementationWithBlock(^(UIMenuController *, UIView *, CGRect) {
            shownMenu = true;
        })
    };
    Util::run(&shownMenu);

    [[webView textInputContentView] paste:UIMenuController.sharedMenuController];
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        RetainPtr readTextResult = [webView stringByEvaluatingJavaScript:@"window.readTextResult" inFrame:[webView firstChildFrame]];
        return [readTextResult isEqualToString:@"PASS: hello"];
    }, 5, @"Timed out waiting for subframe to finish paste.");
}

TEST(SiteIsolation, UserGesture)
{
    auto mainFrameHTML = "<!doctype html>"
    "<button id='testbutton' onclick='testiframe.contentWindow.postMessage(\"hi\", \"*\")'>Click!</button><br>"
    "<iframe id='testiframe' src='https://webkit.org/iframe' allow='payment'></iframe>"_s;

    auto iframeHTML = "<script>"
    "function validRequest() {"
    "    return {"
    "          countryCode: 'US',"
    "          currencyCode: 'USD',"
    "          supportedNetworks: ['visa', 'masterCard', 'carteBancaire'],"
    "          merchantCapabilities: ['supports3DS'],"
    "          total: { label: 'Your Label', amount: '10.00' },"
    "    }"
    "}"
    "window.addEventListener('message', (event) => {"
    "    try {"
    "        new ApplePaySession(4, validRequest());"
    "        alert('did not throw');"
    "    } catch (e) { alert('threw ' + e) }"
    "}, false)"
    "</script>"_s;

    HTTPServer server({
        { "/main"_s, { mainFrameHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"ApplePayEnabled", true);
    auto [webView, delegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadURL:[NSURL URLWithString:@"https://example.com/main"]];
    [delegate waitForDidFinishNavigation];

    [webView clickOnElementID:@"testbutton"];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "did not throw");
}

TEST(SiteIsolation, CrossProcessHistoryTraversalCoalesce)
{
    constexpr auto pageWithIframes = "<iframe src='https://webkit.org/x'></iframe><iframe src='https://apple.com/x'></iframe>"_s;
    constexpr auto iframeBody = "x"_s;

    HTTPServer server({
        { "/a"_s, { pageWithIframes } },
        { "/b"_s, { pageWithIframes } },
        { "/c"_s, { pageWithIframes } },
        { "/x"_s, { iframeBody } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)2);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)0);

    auto childFrames = [webView mainFrame].childFrames;
    EXPECT_EQ(childFrames.count, 2u);
    pid_t mainPid = [[webView mainFrame] info]._processIdentifier;
    pid_t pidWk = childFrames[0].info._processIdentifier;
    pid_t pidAp = childFrames[1].info._processIdentifier;
    EXPECT_NE(pidWk, mainPid);
    EXPECT_NE(pidAp, mainPid);
    EXPECT_NE(pidWk, pidAp);

    __block unsigned didFinishCount = 0;
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        ++didFinishCount;
    };

    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[0].info completionHandler:nil];
    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[1].info completionHandler:nil];

    // A split traversal settles in two navigations, so wait for the deterministic destination.
    int spins = 0;
    while (![[[webView URL] absoluteString] isEqualToString:@"https://example.com/a"] && spins++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE(didFinishCount == 1u || didFinishCount == 2u);
    EXPECT_WK_STREQ(@"https://example.com/a", [[webView URL] absoluteString]);
    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)0);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)2);
}

TEST(SiteIsolation, CrossProcessHistoryTraversalCoalesceWithSharedProcess)
{
    constexpr auto pageWithIframes = "<iframe src='https://webkit.org/x'></iframe><iframe src='https://apple.com/x'></iframe>"_s;
    constexpr auto iframeBody = "x"_s;

    HTTPServer server({
        { "/a"_s, { pageWithIframes } },
        { "/b"_s, { pageWithIframes } },
        { "/c"_s, { pageWithIframes } },
        { "/x"_s, { iframeBody } },
    }, HTTPServer::Protocol::HttpsProxy);

    // apple.com is excluded from the shared process so the two subframes still land in
    // different processes, which is the topology this traversal coalescing needs.
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, nil, nil, @"apple.com");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)2);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)0);

    auto childFrames = [webView mainFrame].childFrames;
    EXPECT_EQ(childFrames.count, 2u);
    pid_t mainPid = [[webView mainFrame] info]._processIdentifier;
    pid_t pidWk = childFrames[0].info._processIdentifier;
    pid_t pidAp = childFrames[1].info._processIdentifier;
    EXPECT_NE(pidWk, mainPid);
    EXPECT_NE(pidAp, mainPid);
    EXPECT_NE(pidWk, pidAp);

    __block unsigned didFinishCount = 0;
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        ++didFinishCount;
    };

    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[0].info completionHandler:nil];
    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[1].info completionHandler:nil];

    // A split traversal settles in two navigations, so wait for the deterministic destination.
    int spins = 0;
    while (![[[webView URL] absoluteString] isEqualToString:@"https://example.com/a"] && spins++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE(didFinishCount == 1u || didFinishCount == 2u);
    EXPECT_WK_STREQ(@"https://example.com/a", [[webView URL] absoluteString]);
    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)0);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)2);
}

TEST(SiteIsolation, CrossProcessHistoryTraversalGoMinus2)
{
    constexpr auto pageWithIframes = "<iframe src='https://webkit.org/x'></iframe><iframe src='https://apple.com/x'></iframe>"_s;
    constexpr auto iframeBody = "x"_s;

    HTTPServer server({
        { "/a"_s, { pageWithIframes } },
        { "/b"_s, { pageWithIframes } },
        { "/c"_s, { pageWithIframes } },
        { "/x"_s, { iframeBody } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)2);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)0);
    EXPECT_EQ([webView mainFrame].childFrames.count, 2u);

    [webView evaluateJavaScript:@"history.go(-2)" completionHandler:nil];

    int spins = 0;
    while (![[[webView URL] absoluteString] isEqualToString:@"https://example.com/a"] && spins++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_WK_STREQ(@"https://example.com/a", [[webView URL] absoluteString]);
    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)0);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)2);
}

TEST(SiteIsolation, CrossProcessHistoryTraversalForwardInSubframeAfterGoMinus2)
{
    constexpr auto pageWithIframes = "<iframe src='https://webkit.org/x'></iframe><iframe src='https://apple.com/x'></iframe>"_s;
    constexpr auto iframeBody = "x"_s;

    HTTPServer server({
        { "/a"_s, { pageWithIframes } },
        { "/b"_s, { pageWithIframes } },
        { "/c"_s, { pageWithIframes } },
        { "/x"_s, { iframeBody } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)2);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)0);
    EXPECT_EQ([webView mainFrame].childFrames.count, 2u);

    [webView evaluateJavaScript:@"history.go(-2)" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://example.com/a", [[webView URL] absoluteString]);

    // A subframe's history.forward() resolves to the main frame item, so the UIProcess delivers the
    // GoToBackForwardItem to the main frame's process, not to this subframe's.
    auto childFrames = [webView mainFrame].childFrames;
    EXPECT_EQ(childFrames.count, 2u);
    [webView evaluateJavaScript:@"history.forward()" inFrame:childFrames[1].info completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://example.com/b", [[webView URL] absoluteString]);
    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)1);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)1);
}

TEST(SiteIsolation, CrossProcessHistoryTraversalSameFrameBackTwice)
{
    constexpr auto pageWithIframes = "<iframe src='https://webkit.org/x'></iframe><iframe src='https://apple.com/x'></iframe>"_s;
    constexpr auto iframeBody = "x"_s;

    HTTPServer server({
        { "/a"_s, { pageWithIframes } },
        { "/b"_s, { pageWithIframes } },
        { "/c"_s, { pageWithIframes } },
        { "/x"_s, { iframeBody } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/c"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto childFrames = [webView mainFrame].childFrames;
    EXPECT_EQ(childFrames.count, 2u);

    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[0].info completionHandler:nil];
    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[0].info completionHandler:nil];

    int spins = 0;
    while (![[[webView URL] absoluteString] isEqualToString:@"https://example.com/a"] && spins++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_WK_STREQ(@"https://example.com/a", [[webView URL] absoluteString]);
    EXPECT_EQ([webView backForwardList].backList.count, (NSUInteger)0);
    EXPECT_EQ([webView backForwardList].forwardList.count, (NSUInteger)2);
}

TEST(SiteIsolation, CrossProcessSameDocumentHistoryTraversalDoesNotStall)
{
    HTTPServer server({
        { "/page"_s, { "<iframe src='https://iframe.com/child'></iframe>"_s } },
        { "/child"_s, { "child"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/page"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    [webView objectByEvaluatingJavaScript:@"location.hash = 'a'"];
    [webView objectByEvaluatingJavaScript:@"location.hash = 'b'"];
    EXPECT_WK_STREQ(@"https://example.com/page#b", [[webView URL] absoluteString]);

    auto childFrames = [webView mainFrame].childFrames;
    EXPECT_EQ(childFrames.count, 1u);
    EXPECT_NE(childFrames[0].info._processIdentifier, [[webView mainFrame] info]._processIdentifier);

    auto waitForURL = [&](NSString *expected) {
        int spins = 0;
        while (![[[webView URL] absoluteString] isEqualToString:expected] && spins++ < 100)
            TestWebKitAPI::Util::runFor(0.1_s);
    };

    // The first back (#b -> #a) is same-document; the second only runs if that settled the queue.
    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[0].info completionHandler:nil];
    waitForURL(@"https://example.com/page#a");
    EXPECT_WK_STREQ(@"https://example.com/page#a", [[webView URL] absoluteString]);

    [webView evaluateJavaScript:@"history.back()" inFrame:childFrames[0].info completionHandler:nil];
    waitForURL(@"https://example.com/page");
    EXPECT_WK_STREQ(@"https://example.com/page", [[webView URL] absoluteString]);
}

TEST(SiteIsolation, CrossSiteTargetBlankDownloadDoesNotCrashNetworkProcess)
{
    HTTPServer server({
        { "/opener"_s, { "<a id='dl' href='https://s3.amazonaws.com/file.txt' target='_blank' rel='noreferrer' style='display:block;width:100%;height:100%'>Full logs</a>"_s } },
        { "/file.txt"_s, { { { "Content-Type"_s, "text/plain"_s } }, "download content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 400));
    RetainPtr navDelegate = navigationDelegate;

    RetainPtr downloadDelegate = adoptNS([TestDownloadDelegate new]);
    __block bool done = false;
    __block bool downloadStarted = false;
    __block bool processCrashed = false;

    downloadDelegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
        downloadStarted = true;
        done = true;
        completionHandler(nil);
    };
    downloadDelegate.get().didFailWithError = ^(WKDownload *, NSError *, NSData *) {
        // A clean download failure is not a network process crash.
        downloadStarted = true;
        done = true;
    };

    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.host isEqualToString:@"s3.amazonaws.com"])
            completionHandler(WKNavigationActionPolicyDownload);
        else
            completionHandler(WKNavigationActionPolicyAllow);
    };
    navigationDelegate.get().navigationActionDidBecomeDownload = ^(WKNavigationAction *, WKDownload *download) {
        download.delegate = downloadDelegate.get();
    };
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        processCrashed = true;
        done = true;
    };

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    __block RetainPtr<TestWKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().navigationDelegate = navDelegate.get();
        return openedWebView.get();
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://ews-build.webkit.org/opener"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView clickOnElementID:@"dl"];

    Util::run(&done);
    EXPECT_TRUE(downloadStarted);
    EXPECT_FALSE(processCrashed);
}

// Per-navigation website policies must be applied to a cross-origin subframe's document loader under
// site isolation, matching non-site-isolation (where the subframe's own DocumentLoader receives them
// via the navigation policy decision). Here we use allowsJSHandleCreationInPageWorld: without applying
// it in the subframe's process, window.webkit.createJSHandle (and therefore testRunner.runUIScript from
// a subframe) is unavailable.
TEST(SiteIsolation, WebsitePoliciesAppliedToCrossOriginSubframeDocumentLoader)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html>subframe"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    // Enable JSHandle creation in the page world for every navigation, main frame and subframe alike,
    // as WebKitTestRunner does for all navigations.
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._allowsJSHandleCreationInPageWorld = YES;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSString *check = @"window.webkit && typeof window.webkit.createJSHandle === 'function' ? 'available' : 'unavailable'";

    // The main frame's process receives the policy via ProvisionalPageProxy.
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check], "available");

    // Wait for the cross-origin subframe (a separate process under site isolation) to commit.
    while (![[webView firstChildFrame].securityOrigin.host isEqualToString:@"b.com"])
        Util::spinRunLoop();

    // The subframe's process must also have the policy applied to its document loader.
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check inFrame:[webView firstChildFrame]], "available");
}

TEST(SiteIsolation, LockdownModeSettingsInheritedByCrossSiteIframe)
{
    HTTPServer server(mainAndSubframeResponses(), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences.lockdownModeEnabled = YES;
    });

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    NSString *check = @"(!!window.WebGL2RenderingContext) + ',' + (!!window.Gamepad)";
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check], "false,false");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check inFrame:childFrame.get()], "false,false");
}

TEST(SiteIsolation, GlobalPrivacyControlInheritedByCrossSiteIframe)
{
    constexpr auto mainframeHTML = "<!DOCTYPE html><iframe src='https://b.com/subframe'></iframe><script>fetch('/mainframe-subresource')</script>"_s;
    constexpr auto subframeHTML = "<!DOCTYPE html><script>fetch('/subframe-subresource')</script>"_s;

    bool receivedMainFrameSubresource { false };
    bool receivedSubframeSubresource { false };
    bool mainFrameSubresourceHadGPCHeader { false };
    bool subframeSubresourceHadGPCHeader { false };

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            bool hasGPCHeader = contains(request.span(), "Sec-GPC"_span);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/mainframe-subresource"_s) {
                mainFrameSubresourceHadGPCHeader = hasGPCHeader;
                receivedMainFrameSubresource = true;
            } else if (path == "/subframe-subresource"_s) {
                subframeSubresourceHadGPCHeader = hasGPCHeader;
                receivedSubframeSubresource = true;
            }
            co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
        }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences.globalPrivacyControlEnabled = YES;
    });

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    NSString *check = @"String(navigator.globalPrivacyControl)";
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check], "true");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check inFrame:childFrame.get()], "true");

    Util::run(&receivedMainFrameSubresource);
    Util::run(&receivedSubframeSubresource);
    EXPECT_TRUE(mainFrameSubresourceHadGPCHeader);
    EXPECT_TRUE(subframeSubresourceHadGPCHeader);
}

// With noise injection active, two identical OfflineAudioContext renders produce different results.
TEST(SiteIsolation, AdvancedPrivacyProtectionsInheritedByCrossSiteIframeDocument)
{
    constexpr auto renderScript =
        "<script>"
        "async function renderOscillatorThroughCompressor() {"
        "    const context = new OfflineAudioContext(1, 5000, 44100);"
        "    const oscillator = context.createOscillator();"
        "    oscillator.type = 'triangle';"
        "    oscillator.frequency.value = 1000;"
        "    const compressor = context.createDynamicsCompressor();"
        "    compressor.threshold.value = -50;"
        "    compressor.knee.value = 40;"
        "    compressor.ratio.value = 12;"
        "    compressor.attack.value = 0;"
        "    compressor.release.value = 0.2;"
        "    oscillator.connect(compressor);"
        "    compressor.connect(context.destination);"
        "    oscillator.start();"
        "    const rendered = await context.startRendering();"
        "    let sum = 0;"
        "    for (const value of rendered.getChannelData(0)) {"
        "        if (isFinite(value))"
        "            sum += value;"
        "    }"
        "    return sum;"
        "}"
        "</script>"_s;

    HTTPServer::ResponseMap responses;
    responses.add("/mainframe"_s, HTTPResponse(makeString("<!DOCTYPE html>"_s, renderScript, "<iframe src='https://b.com/subframe'></iframe>"_s)));
    responses.add("/subframe"_s, HTTPResponse(makeString("<!DOCTYPE html>"_s, renderScript)));
    HTTPServer server(WTF::move(responses), HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry;
    });

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    auto renderedSum = [&](WKFrameInfo *frame) {
        return [[webView objectByCallingAsyncFunction:@"return await renderOscillatorThroughCompressor()" withArguments:@{ } inFrame:frame inContentWorld:WKContentWorld.pageWorld] doubleValue];
    };

    EXPECT_NE(renderedSum(nil), renderedSum(nil));
    EXPECT_NE(renderedSum(childFrame.get()), renderedSum(childFrame.get()));
}

TEST(SiteIsolation, PushAndNotificationAPIPolicyInheritedByCrossSiteIframe)
{
    HTTPServer server(mainAndSubframeResponses(), HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration preferences] _setPushAPIEnabled:YES];
    [[configuration preferences] _setNotificationsEnabled:YES];

    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences._pushAndNotificationAPIEnabled = NO;
    }, configuration.get());

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    NSString *check = @"String('PushManager' in window || 'Notification' in window)";
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check], "false");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check inFrame:childFrame.get()], "false");
}

TEST(SiteIsolation, ColorSchemePreferenceInheritedByCrossSiteIframe)
{
    HTTPServer server(mainAndSubframeResponses(), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences._colorSchemePreference = _WKWebsiteColorSchemePreferenceDark;
    });
    // Without this the subframe falls back to the system appearance, which reports dark for the
    // wrong reason on a machine running in Dark Mode.
    [webView forceLightMode];

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    NSString *check = @"String(matchMedia('(prefers-color-scheme: dark)').matches)";
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check], "true");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:check inFrame:childFrame.get()], "true");
}

static bool hasProcessExited(pid_t pid)
{
    return kill(pid, 0) == -1 && errno == ESRCH;
}

TEST(SiteIsolation, ProcessLimitUnloadsAllProcessesOfLeastRecentlyVisiblePage)
{
    // Each view loads a page in to two processes.
    constexpr unsigned maxProcessCount = 10;
    unsigned maxViewsToCreate = maxProcessCount / 2;

    [WKProcessPool _setWebProcessCountLimit:maxProcessCount];

    HTTPServer::ResponseMap responses;
    responses.add("/subframe"_s, HTTPResponse("<body>subframe</body>"_s));
    for (unsigned i = 0; i < maxViewsToCreate; ++i)
        responses.add(makeString("/mainframe"_s, i), HTTPResponse(makeString("<iframe src='https://frame"_s, i, ".com/subframe'></iframe>"_s)));
    HTTPServer server(WTF::move(responses), HTTPServer::Protocol::HttpsProxy);

    auto loadCrossSitePage = [&](TestWKWebView *webView, TestNavigationDelegate *navigationDelegate, unsigned index) {
        [webView loadRequest:[NSURLRequest requestWithURL:adoptNS([[NSURL alloc] initWithString:[NSString stringWithFormat:@"https://main%u.com/mainframe%u", index, index]]).get()]];
        [navigationDelegate waitForDidFinishNavigation];
        RetainPtr childFrameHost = [NSString stringWithFormat:@"frame%u.com", index];
        EXPECT_TRUE(Util::waitFor([&] {
            return [[webView firstChildFrame].securityOrigin.host isEqualToString:childFrameHost.get()];
        }));
    };

    struct PageProcesses {
        pid_t mainFrame { 0 };
        pid_t remoteFrame { 0 };
    };
    auto processesForPage = [](TestWKWebView *webView) {
        PageProcesses processes { [webView _webProcessIdentifier], [webView firstChildFrame]._processIdentifier };
        EXPECT_NE(processes.mainFrame, 0);
        EXPECT_NE(processes.remoteFrame, 0);
        EXPECT_NE(processes.mainFrame, processes.remoteFrame);
        return processes;
    };

    // mainFrameOnlyPolicyViewAndDelegate() puts the view in a window, so this is visible.
    auto [visibleWebView, visibleNavigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *) { });
    loadCrossSitePage(visibleWebView.get(), visibleNavigationDelegate.get(), 0);
    auto visibleProcesses = processesForPage(visibleWebView.get());

    __block RetainPtr<WKWebView> unloadedView;
    auto recordUnloadedView = ^(WKWebView *view, _WKProcessTerminationReason) {
        unloadedView = view;
    };
    [visibleNavigationDelegate setWebContentProcessDidTerminate:recordUnloadedView];

    RetainPtr nonVisibleNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [nonVisibleNavigationDelegate allowAnyTLSCertificate];
    [nonVisibleNavigationDelegate setWebContentProcessDidTerminate:recordUnloadedView];
    nonVisibleNavigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    RetainPtr nonVisibleConfiguration = server.httpsProxyConfiguration();
    enableSiteIsolation(nonVisibleConfiguration.get());

    Vector<RetainPtr<TestWKWebView>> nonVisibleViews;
    Vector<PageProcesses> nonVisibleProcesses;
    while (!unloadedView && nonVisibleViews.size() + 1 < maxViewsToCreate) {
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:nonVisibleConfiguration.get() addToWindow:NO]);
        webView.get().navigationDelegate = nonVisibleNavigationDelegate.get();
        loadCrossSitePage(webView.get(), nonVisibleNavigationDelegate.get(), nonVisibleViews.size() + 1);
        nonVisibleProcesses.append(processesForPage(webView.get()));
        nonVisibleViews.append(WTF::move(webView));
    }

    // The least recently visible page was the one unloaded, not the visible one. All of its
    // processes should be dead.
    EXPECT_NOT_NULL(unloadedView.get());
    EXPECT_EQ(unloadedView.get(), nonVisibleViews[0].get());
    EXPECT_EQ([nonVisibleViews[0] _webProcessIdentifier], 0);
    EXPECT_TRUE(Util::waitFor([&] {
        return hasProcessExited(nonVisibleProcesses[0].mainFrame);
    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return hasProcessExited(nonVisibleProcesses[0].remoteFrame);
    }));

    // The visible page should still be running.
    EXPECT_EQ([visibleWebView _webProcessIdentifier], visibleProcesses.mainFrame);
    EXPECT_EQ([visibleWebView firstChildFrame]._processIdentifier, visibleProcesses.remoteFrame);
    EXPECT_FALSE(hasProcessExited(visibleProcesses.mainFrame));
    EXPECT_FALSE(hasProcessExited(visibleProcesses.remoteFrame));

    // Every page newer than the victim kept both of its processes too.
    for (size_t i = 1; i < nonVisibleViews.size(); ++i) {
        EXPECT_EQ([nonVisibleViews[i] _webProcessIdentifier], nonVisibleProcesses[i].mainFrame);
        EXPECT_FALSE(hasProcessExited(nonVisibleProcesses[i].mainFrame));
        EXPECT_FALSE(hasProcessExited(nonVisibleProcesses[i].remoteFrame));
    }

    [WKProcessPool _setWebProcessCountLimit:0];
}

TEST(SiteIsolation, RestoredPageIsRenderedAfterCrossSiteBFCacheRoundTrip)
{
    HTTPServer server({
        { "/a"_s, { "a"_s } },
        { "/b"_s, { "b"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), { { "https://a.com"_s } });

    [webView waitForNextPresentationUpdate];

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), { { "https://b.com"_s } });

    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, RestoredPageWithIframeIsRenderedAfterCrossSiteBFCacheRoundTrip)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { "frame"_s } },
        { "/b"_s, { "b"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto *configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration, @"MultiProcessBackForwardCacheEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://frame.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), Vector<ExpectedFrameTree> { expectedAfterGoBack });
    [webView waitForNextPresentationUpdate];

    [webView goForward];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    checkFrameTreesInProcesses(webView.get(), { { "https://b.com"_s } });
}

static Vector<double> preferredRenderingUpdateIntervals(TestWKWebView *webView)
{
    Vector<double> result;
    __block bool done { false };
    __block Vector<double>* values = &result;
    [webView _preferredRenderingUpdateIntervalsForTesting:^(NSArray<NSNumber *> *intervals) {
        for (NSNumber *interval in intervals)
            values->append(interval.doubleValue);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

TEST(SiteIsolation, DisplayRefreshRateReachesSubframeProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<p>hi</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView _setDisplayForTesting:1 nominalFramesPerSecond:120];
    auto fastIntervals = preferredRenderingUpdateIntervals(webView.get());
    EXPECT_EQ(fastIntervals.size(), 2u);
    for (auto interval : fastIntervals)
        EXPECT_EQ(interval, fastIntervals[0]);

    [webView _setDisplayForTesting:2 nominalFramesPerSecond:30];
    auto slowIntervals = preferredRenderingUpdateIntervals(webView.get());
    EXPECT_EQ(slowIntervals.size(), 2u);
    for (auto interval : slowIntervals)
        EXPECT_EQ(interval, slowIntervals[0]);

    EXPECT_NE(slowIntervals[0], fastIntervals[0]);
}

enum class SiteIsolationHighValueFraudTargetDomainsEnabled : bool { No, Yes };

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewWithHighValueFraudTargetDomains(const HTTPServer& server, NSArray<NSString *> *domains, SiteIsolationHighValueFraudTargetDomainsEnabled enabled)
{
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);

    // Stand in for the list WebPrivacy would have delivered, which is unavailable in tests.
    [dataStore _setHighValueFraudTargetDomainsForTesting:domains];

    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];
    enableSiteIsolation(viewConfiguration.get());
    setFeatureEnabled(viewConfiguration.get(), @"SiteIsolationSharedProcessEnabled", true);
    if (enabled == SiteIsolationHighValueFraudTargetDomainsEnabled::Yes)
        setFeatureEnabled(viewConfiguration.get(), @"SiteIsolationHighValueFraudTargetDomainsEnabled", true);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

static HTTPServer serverWithTwoCrossSiteFrames()
{
    return HTTPServer({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://b.com/frame'></iframe><iframe src='https://c.com/frame'></iframe>"_s } },
        { "/frame"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
}

TEST(SiteIsolation, SharedProcessExcludesHighValueFraudTargetDomains)
{
    auto server = serverWithTwoCrossSiteFrames();
    auto [webView, navigationDelegate] = siteIsolatedViewWithHighValueFraudTargetDomains(server, @[@"b.com"], SiteIsolationHighValueFraudTargetDomainsEnabled::Yes);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // b.com is on the high-value fraud target domains list, so it gets its own process rather than
    // joining c.com in the shared process.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame }, { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s }, { RemoteFrame } } },
        { RemoteFrame, { { RemoteFrame }, { "https://c.com"_s } } },
    });
}

TEST(SiteIsolation, SharedProcessIgnoresHighValueFraudTargetDomainsWhenDisabled)
{
    auto server = serverWithTwoCrossSiteFrames();
    auto [webView, navigationDelegate] = siteIsolatedViewWithHighValueFraudTargetDomains(server, @[@"b.com"], SiteIsolationHighValueFraudTargetDomainsEnabled::No);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // The list is populated but the preference is off, so b.com is still allowed to share a process.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame }, { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s }, { "https://c.com"_s } } },
    });
}

// Page-wide commands must reach every web content process backing the page, not just the main
// frame's. Each test below asserts the command took effect *inside* the cross-site iframe; a
// version that only checks the main frame passes without the fix.

static String notifyScriptForTag(ASCIILiteral tag)
{
    return makeString("<script>"
        "const tag = '"_s, tag, "';"
        "function notify(suffix) { window.webkit.messageHandlers.testHandler.postMessage(tag + ':' + suffix); }"
        "</script>"_s);
}

TEST(SiteIsolation, StopLoadingStopsCrossSiteIframe)
{
    // Each frame commits, starts a subresource load that never completes, then blocks its parser
    // on a script that never arrives. stopLoading must abort the outstanding load in both.
    auto pendingLoadScript = "<script>"
        "fetch('/hang').then(() => notify('fetch-completed'), () => notify('fetch-aborted'));"
        "notify('fetch-started');"
        "</script>"_s;

    HTTPServer::ResponseMap responses;
    responses.add("/mainframe"_s, HTTPResponse(makeString(notifyScriptForTag("main"_s), pendingLoadScript,
        "<iframe src='https://b.com/subframe'></iframe><script src='/hang'></script>"_s)));
    responses.add("/subframe"_s, HTTPResponse(makeString(notifyScriptForTag("iframe"_s), pendingLoadScript,
        "<script src='/hang'></script>"_s)));
    responses.add("/hang"_s, HTTPResponse(HTTPResponse::Behavior::NeverSendResponse));
    HTTPServer server(WTF::move(responses), HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    RetainPtr<NSMutableSet<NSString *>> received = adoptNS([[NSMutableSet alloc] init]);
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        [received addObject:message];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];

    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"main:fetch-started"] && [received containsObject:@"iframe:fetch-started"];
    }));
    while (![[webView firstChildFrame].securityOrigin.host isEqualToString:@"b.com"])
        Util::spinRunLoop();

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.readyState" inFrame:childFrame.get()], "loading");

    [webView stopLoading];

    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"main:fetch-aborted"];
    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"iframe:fetch-aborted"];
    }));
    EXPECT_FALSE([received containsObject:@"iframe:fetch-completed"]);
}

enum class MainFrameVideo : bool { No, Yes };

static HTTPServer::ResponseMap crossSiteVideoResponses(MainFrameVideo mainFrameVideo)
{
    auto playVideoScript = "<script>"
        "function playVideo() {"
        "    let video = document.getElementById('video');"
        "    video.addEventListener('playing', () => notify('playing'));"
        "    video.addEventListener('pause', () => notify('paused'));"
        "    video.play().catch(() => notify('play-rejected'));"
        "}"
        "</script>"_s;
    auto videoElement = "<video id='video' webkit-playsinline src='/video-with-audio.mp4'></video>"_s;
    auto iframeElement = "<iframe src='https://b.com/subframe'></iframe>"_s;

    RetainPtr videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];

    HTTPServer::ResponseMap responses;
    if (mainFrameVideo == MainFrameVideo::Yes) {
        responses.add("/mainframe"_s, HTTPResponse(makeString(notifyScriptForTag("main"_s), playVideoScript,
            "<body onload='playVideo()'>"_s, videoElement, iframeElement, "</body>"_s)));
    } else {
        responses.add("/mainframe"_s, HTTPResponse(makeString(notifyScriptForTag("main"_s),
            "<body>"_s, iframeElement, "</body>"_s)));
    }
    responses.add("/subframe"_s, HTTPResponse(makeString(notifyScriptForTag("iframe"_s), playVideoScript,
        "<body onload='playVideo()'>"_s, videoElement, "</body>"_s)));
    responses.add("/video-with-audio.mp4"_s, HTTPResponse(videoData.get()));
    return responses;
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> autoplayingCrossSiteVideoView(const HTTPServer& server)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:NO];
#endif

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [navigationDelegate setDecidePolicyForNavigationActionWithPreferences:^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._autoplayPolicy = _WKWebsiteAutoplayPolicyAllow;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    }];
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

static NSString *videoPausedInFrame(TestWKWebView *webView, WKFrameInfo *frame)
{
    return [webView stringByEvaluatingJavaScript:@"String(document.getElementById('video').paused)" inFrame:frame];
}

TEST(SiteIsolation, PauseAllMediaPlaybackPausesCrossSiteIframe)
{
    HTTPServer server(crossSiteVideoResponses(MainFrameVideo::Yes), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = autoplayingCrossSiteVideoView(server);

    RetainPtr<NSMutableSet<NSString *>> received = adoptNS([[NSMutableSet alloc] init]);
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        [received addObject:message];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"main:playing"] && [received containsObject:@"iframe:playing"];
    }));

    __block bool done = false;
    [webView pauseAllMediaPlaybackWithCompletionHandler:^{
        done = true;
    }];
    Util::run(&done);

    // The pause reached each process before its reply, so by the time this evaluation is
    // dispatched to the iframe's process the video there is already paused.
    EXPECT_WK_STREQ(videoPausedInFrame(webView.get(), nil), "true");
    EXPECT_WK_STREQ(videoPausedInFrame(webView.get(), [webView firstChildFrame]), "true");
}

TEST(SiteIsolation, SetAllMediaPlaybackSuspendedSuspendsCrossSiteIframe)
{
    HTTPServer server(crossSiteVideoResponses(MainFrameVideo::Yes), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = autoplayingCrossSiteVideoView(server);

    RetainPtr<NSMutableSet<NSString *>> received = adoptNS([[NSMutableSet alloc] init]);
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        [received addObject:message];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"main:playing"] && [received containsObject:@"iframe:playing"];
    }));

    __block bool done = false;
    [webView setAllMediaPlaybackSuspended:YES completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ(videoPausedInFrame(webView.get(), nil), "true");
    EXPECT_WK_STREQ(videoPausedInFrame(webView.get(), childFrame.get()), "true");

    // Suspension must also block the page from resuming itself. play() returns a promise, which
    // evaluateJavaScript cannot serialize, so discard it.
    [webView objectByEvaluatingJavaScript:@"document.getElementById('video').play().catch(() => { }); undefined" inFrame:childFrame.get()];
    Util::runFor(0.1_s);
    EXPECT_WK_STREQ(videoPausedInFrame(webView.get(), childFrame.get()), "true");
}

TEST(SiteIsolation, ResumeAllMediaPlaybackResumesCrossSiteIframe)
{
    // Playback is suspended before the cross-site iframe exists, so its process is created with
    // mediaPlaybackIsSuspended already set. Only a resume that reaches that process can let the
    // video play, which is what makes this a test of the resume path rather than the suspend path.
    auto videoScript = "<script>"
        "function playVideo() {"
        "    let video = document.getElementById('video');"
        "    video.addEventListener('playing', () => notify('playing'));"
        "    video.play().catch(() => { });"
        "}"
        "</script>"_s;

    RetainPtr videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];

    HTTPServer::ResponseMap responses;
    responses.add("/mainframe"_s, HTTPResponse(makeString(notifyScriptForTag("main"_s), "<body></body>"_s)));
    responses.add("/subframe"_s, HTTPResponse(makeString(notifyScriptForTag("iframe"_s), videoScript,
        "<body onload='notify(\"loaded\"); playVideo()'>"
        "<video id='video' webkit-playsinline src='/video-with-audio.mp4'></video></body>"_s)));
    responses.add("/video-with-audio.mp4"_s, HTTPResponse(videoData.get()));
    HTTPServer server(WTF::move(responses), HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = autoplayingCrossSiteVideoView(server);

    RetainPtr<NSMutableSet<NSString *>> received = adoptNS([[NSMutableSet alloc] init]);
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        [received addObject:message];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool suspended = false;
    [webView setAllMediaPlaybackSuspended:YES completionHandler:^{
        suspended = true;
    }];
    Util::run(&suspended);

    [webView evaluateJavaScript:@"let iframe = document.createElement('iframe');"
        "iframe.src = 'https://b.com/subframe';"
        "document.body.appendChild(iframe);" completionHandler:nil];

    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"iframe:loaded"];
    }));
    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ([childFrame securityOrigin].host, "b.com");

    // The iframe's process started suspended, so its autoplay never began.
    EXPECT_FALSE([received containsObject:@"iframe:playing"]);
    EXPECT_WK_STREQ(videoPausedInFrame(webView.get(), childFrame.get()), "true");

    __block bool resumed = false;
    [webView setAllMediaPlaybackSuspended:NO completionHandler:^{
        resumed = true;
    }];
    Util::run(&resumed);

    [webView objectByEvaluatingJavaScript:@"document.getElementById('video').play().catch(() => { }); undefined" inFrame:childFrame.get()];

    EXPECT_TRUE(Util::waitFor([&] {
        return [videoPausedInFrame(webView.get(), childFrame.get()) isEqualToString:@"false"];
    }));
}

TEST(SiteIsolation, RequestMediaPlaybackStateReflectsCrossSiteIframe)
{
    // Only the iframe has media, so the main frame's process alone reports no playback at all.
    HTTPServer server(crossSiteVideoResponses(MainFrameVideo::No), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = autoplayingCrossSiteVideoView(server);

    RetainPtr<NSMutableSet<NSString *>> received = adoptNS([[NSMutableSet alloc] init]);
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        [received addObject:message];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    EXPECT_TRUE(Util::waitFor([&] {
        return [received containsObject:@"iframe:playing"];
    }));

    __block bool done = false;
    __block WKMediaPlaybackState state = WKMediaPlaybackStateNone;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState result) {
        state = result;
        done = true;
    }];
    Util::run(&done);

    EXPECT_EQ(state, WKMediaPlaybackStatePlaying);
}

TEST(SiteIsolation, SuspendPageSuspendsCrossSiteIframeProcess)
{
    auto tickScript = "<script>setInterval(() => notify('tick'), 10);</script>"_s;

    HTTPServer::ResponseMap responses;
    responses.add("/mainframe"_s, HTTPResponse(makeString(notifyScriptForTag("main"_s), tickScript,
        "<iframe src='https://b.com/subframe'></iframe>"_s)));
    responses.add("/subframe"_s, HTTPResponse(makeString(notifyScriptForTag("iframe"_s), tickScript)));
    HTTPServer server(WTF::move(responses), HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    __block unsigned mainTicks = 0;
    __block unsigned iframeTicks = 0;
    __block bool bothFramesTicking = false;
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        if ([message isEqualToString:@"main:tick"])
            mainTicks++;
        else if ([message isEqualToString:@"iframe:tick"])
            iframeTicks++;
        if (mainTicks > 2 && iframeTicks > 2)
            bothFramesTicking = true;
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    EXPECT_TRUE(Util::runFor(&bothFramesTicking, 10_s));

    __block bool done = false;
    [webView _suspendPage:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    Util::run(&done);

    // Timers must have stopped in both processes. Evaluating JavaScript throws once the page is
    // suspended, so the tick counters are the only usable probe here.
    auto mainTicksWhenSuspended = mainTicks;
    auto iframeTicksWhenSuspended = iframeTicks;
    Util::runFor(0.5_s);
    EXPECT_EQ(mainTicksWhenSuspended, mainTicks);
    EXPECT_EQ(iframeTicksWhenSuspended, iframeTicks);

    __block bool resumeDone = false;
    [webView _resumePage:^(BOOL success) {
        EXPECT_TRUE(success);
        resumeDone = true;
    }];
    Util::run(&resumeDone);

    // Both processes must start ticking again. The counters are __block, so they cannot be read
    // from a C++ lambda and Util::waitFor is unavailable here.
    bool bothFramesTickingAgain = false;
    for (unsigned attempt = 0; attempt < 100 && !bothFramesTickingAgain; ++attempt) {
        Util::runFor(0.1_s);
        bothFramesTickingAgain = mainTicks > mainTicksWhenSuspended && iframeTicks > iframeTicksWhenSuspended;
    }
    EXPECT_TRUE(bothFramesTickingAgain);
}

TEST(SiteIsolation, SuspendedPageDoesNotLetANewProcessJoin)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='child' src='https://a.com/samesite'></iframe>"_s } },
        { "/samesite"_s, { "same site subframe"_s } },
        { "/crosssite"_s, { "cross site subframe"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    // The subframe starts same-site, so one process backs the page.
    checkFrameTreesInProcesses(webView.get(), { { "https://a.com"_s, { { "https://a.com"_s } } } });

    // Hold the subframe's cross-site navigation. The process for b.com is not chosen until the
    // decision is allowed, so nothing has joined the page yet.
    bool didHoldCrossSiteNavigation { false };
    BlockPtr<void(WKNavigationActionPolicy)> releaseCrossSiteNavigation;
    navigationDelegate.get().decidePolicyForNavigationAction = makeBlockPtr([&](WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.host isEqualToString:@"b.com"]) {
            releaseCrossSiteNavigation = makeBlockPtr(completionHandler);
            didHoldCrossSiteNavigation = true;
            return;
        }
        completionHandler(WKNavigationActionPolicyAllow);
    }).get();

    [webView evaluateJavaScript:@"document.getElementById('child').src = 'https://b.com/crosssite'" completionHandler:nil];
    Util::run(&didHoldCrossSiteNavigation);

    __block bool suspended = false;
    [webView _suspendPage:^(BOOL success) {
        EXPECT_TRUE(success);
        suspended = true;
    }];
    Util::run(&suspended);

    releaseCrossSiteNavigation(WKNavigationActionPolicyAllow);

    // The decision is refused because the page is suspended, so no process for b.com joins and
    // there is nothing running that _suspendPage: failed to freeze.
    Util::runFor(0.5_s);
    EXPECT_EQ([frameTrees(webView.get()) count], 1u);

    __block bool resumed = false;
    __block BOOL resumeSucceeded = NO;
    [webView _resumePage:^(BOOL success) {
        resumeSucceeded = success;
        resumed = true;
    }];
    Util::run(&resumed);
    EXPECT_TRUE(resumeSucceeded);

    // The refused navigation left the iframe where it was.
    checkFrameTreesInProcesses(webView.get(), { { "https://a.com"_s, { { "https://a.com"_s } } } });
}

TEST(SiteIsolation, EndPrintingIsRoutedToTheFrameThatStartedPrinting)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<script>"
            "window.printEvents = [];"
            "for (const event of ['beforeprint', 'afterprint'])"
            "    window.addEventListener(event, () => window.printEvents.push(event));"
            "</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    RetainPtr<WKFrameInfo> mainFrame = [webView mainFrame].info;
    RetainPtr<WKFrameInfo> subframe = [webView mainFrame].childFrames.firstObject.info;
    EXPECT_NE([mainFrame _processIdentifier], [subframe _processIdentifier]);

    __block bool computedPages = false;
    [webView _computePagesForPrinting:[subframe _handle] completionHandler:^{
        computedPages = true;
    }];
    Util::run(&computedPages);

    // Printing began in the subframe's process, leaving the main frame's process untouched.
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.printEvents.join(',')" inFrame:subframe.get()], "beforeprint");
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.matchMedia('print').matches" inFrame:subframe.get()] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.matchMedia('print').matches" inFrame:mainFrame.get()] boolValue]);

    __block bool endedPrinting = false;
    [webView _endPrintingForTesting:^{
        endedPrinting = true;
    }];
    Util::run(&endedPrinting);

    // EndPrinting must reach the process that began printing. When it went to the main frame's
    // process instead, the subframe stayed paginated and never fired afterprint.
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.matchMedia('print').matches" inFrame:subframe.get()] boolValue]);

    // afterprint is queued on the subframe document's event loop, so it can arrive after the
    // EndPrinting reply.
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.printEvents.join(',')" inFrame:subframe.get()] isEqualToString:@"beforeprint,afterprint"];
    }));
}

#if HAVE(PDFKIT)

// UIKit prints on the main thread and blocks it until the document has been drawn. The frames hosted in
// other processes are asked to record by way of the UI process, including one only found by painting
// another, so the blocked UI process still has to route those requests.
TEST(SiteIsolation, DrawPagesToPDFSynchronouslyIncludesCrossSiteFrames)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin:0'>Mainframe<br><iframe style='border:0;width:400px;height:200px' src='https://b.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<body style='margin:0'>Subframe<br><iframe style='border:0;width:300px;height:100px' src='https://c.com/nested'></iframe></body>"_s } },
        { "/nested"_s, { "<body style='margin:0'>Nested</body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(Util::waitFor([&] {
        RetainPtr<WKFrameInfo> nested = [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
        return nested && [[webView objectByEvaluatingJavaScript:@"document.readyState" inFrame:nested.get()] isEqualToString:@"complete"];
    }));

    RetainPtr<WKFrameInfo> mainFrame = [webView mainFrame].info;
    __block bool computedPages = false;
    [webView _computePagesForPrinting:[mainFrame _handle] completionHandler:^{
        computedPages = true;
    }];
    Util::run(&computedPages);

    RetainPtr data = [webView _drawPagesToPDFSynchronouslyForTesting:[mainFrame _handle]];
    ASSERT_NOT_NULL(data.get());

    RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:data.get()]);
    EXPECT_EQ([document pageCount], 1);
    RetainPtr text = [[document pageAtIndex:0] text];
    EXPECT_TRUE([text containsString:@"Mainframe"]);
    EXPECT_TRUE([text containsString:@"Subframe"]);
    EXPECT_TRUE([text containsString:@"Nested"]);
}

#endif // HAVE(PDFKIT)

TEST(SiteIsolation, MultiProcessBFCacheIframeRendersAfterBackNavigation)
{
    HTTPServer server({
        { "/main"_s, { "<iframe src='https://b.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<body style='margin:0'><div style='width:137px;height:59px;background:magenta;transform:translateZ(0)'></div><a id='link' href='https://b.com/destination' target='_top'>click me</a></body>"_s } },
        { "/destination"_s, { "<body>destination page</body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes, nil, nil, nil, EnableBackForwardCache::Yes);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/main"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    });

    [webView evaluateJavaScript:@"document.getElementById('link').click()" inFrame:[webView firstChildFrame] completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://b.com/destination", [webView URL].absoluteString);

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(@"https://a.com/main", [webView URL].absoluteString);

    Vector<ExpectedFrameTree> expectedAfterGoBack = {
        { "https://a.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://b.com"_s } } },
    };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedAfterGoBack }))
        TestWebKitAPI::Util::spinRunLoop();
    checkFrameTreesInProcesses(webView.get(), WTF::move(expectedAfterGoBack));

    __block bool done = false;
    __block BOOL frozen = YES;
    [webView _isLayerTreeFrozenForTesting:^(BOOL isFrozen) {
        frozen = isFrozen;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(frozen);

    // The iframe composites a 137x59 layer, a size nothing in a.com's process can produce, so those
    // bounds appearing in the hosted CALayer tree detect b.com's contribution alone.
    [webView waitForNextPresentationUpdate];
    RetainPtr layerTree = [webView _caLayerTreeAsText];
    EXPECT_TRUE([layerTree containsString:@"width: 137 height: 59"]) << [layerTree UTF8String];

    startCountingAnimationFrames(webView.get(), [webView firstChildFrame]);
    expectAnimationFrameCountToIncrease(webView.get(), [webView firstChildFrame]);
}

TEST(SiteIsolation, UndoAndRedoEditInCrossOriginIframeFromMainFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    insertTextInFrame(webView.get(), childFrame.get(), @"document.body", @"hello");
    EXPECT_WK_STREQ("hello", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.queryCommandEnabled('undo')"] boolValue];
    }));

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"document.execCommand('undo')"] boolValue]);
    EXPECT_TRUE(waitForTextContentInFrame(webView.get(), childFrame.get(), @"document.body", @""));

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.queryCommandEnabled('redo')"] boolValue];
    }));

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"document.execCommand('redo')"] boolValue]);
    EXPECT_TRUE(waitForTextContentInFrame(webView.get(), childFrame.get(), @"document.body", @"hello"));
}

TEST(SiteIsolation, UndoEditsRegisteredByMultipleProcesses)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div id='editor' contenteditable></div><iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    insertTextInFrame(webView.get(), nil, @"editor", @"main");
    EXPECT_WK_STREQ("main", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.queryCommandEnabled('undo')"] boolValue];
    }));

    RetainPtr childFrame = [webView firstChildFrame];
    insertTextInFrame(webView.get(), childFrame.get(), @"document.body", @"sub");
    EXPECT_WK_STREQ("sub", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"document.execCommand('undo')"] boolValue]);
    EXPECT_TRUE(waitForTextContentInFrame(webView.get(), childFrame.get(), @"document.body", @""));
    EXPECT_WK_STREQ("main", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"document.execCommand('undo')"] boolValue]);
    EXPECT_TRUE(waitForTextContentInFrame(webView.get(), nil, @"editor", @""));
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);
}

TEST(SiteIsolation, MainFrameFinishesLoadWithCrossOriginAndSameOriginIframes)
{
    HTTPServer server({
        { "/main"_s, {
            "<body>"
            "<h2>Cross Origin</h2>"
            "<iframe width='500' height='500' src='https://webkit.org/cross-origin-iframe'></iframe>"
            "<h2>Same Origin</h2>"
            "<iframe width='500' height='500' src='https://example.com/same-origin-iframe'></iframe>"
            "</body>"_s
        } },
        { "/cross-origin-iframe"_s, { "<body><p>Cross-origin content</p></body>"_s } },
        { "/same-origin-iframe"_s, { "<body><p>Same-origin content</p></body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame }, { "https://example.com"_s } } },
        { RemoteFrame, { { "https://webkit.org"_s }, { RemoteFrame } } }
    });

    EXPECT_WK_STREQ([webView mainFrame].info.securityOrigin.host, "example.com");
    EXPECT_EQ([webView mainFrame].childFrames.count, 2u);

    auto crossOriginFrame = [webView mainFrame].childFrames[0];
    auto sameOriginFrame = [webView mainFrame].childFrames[1];
    EXPECT_WK_STREQ(crossOriginFrame.info.securityOrigin.host, "webkit.org");
    EXPECT_WK_STREQ(sameOriginFrame.info.securityOrigin.host, "example.com");
    EXPECT_NE(crossOriginFrame.info._processIdentifier, [webView mainFrame].info._processIdentifier);
    EXPECT_EQ(sameOriginFrame.info._processIdentifier, [webView mainFrame].info._processIdentifier);
}

// A page with _shouldRelaxThirdPartyCookieBlocking must keep that relaxation for its cross-origin,
// out-of-process (site-isolated) subframes.
static void runRelaxThirdPartyCookieBlockingSubframeTest(bool shouldRelax)
{
    bool thirdPartySubframeRequestSawCookie = false;
    bool sawSubframeResourceRequest = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/main"_s) {
                // Top document (example.com) embeds a cross-origin webkit.org subframe (separate process).
                co_await connection.awaitableSend(HTTPResponse("<iframe src='https://webkit.org/subframe'></iframe>"_s).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                // The subframe (webkit.org) issues a credentialed request back to webkit.org. Relative to the
                // example.com top document this is a third-party cookie context, blocked by ITP unless relaxed.
                co_await connection.awaitableSend(HTTPResponse("<script>fetch('https://webkit.org/resource', { credentials: 'include' }).then(() => { alert('fetched'); }).catch(() => { alert('error'); });</script>"_s).serialize());
                continue;
            }
            if (path == "/resource"_s) {
                sawSubframeResourceRequest = true;
                thirdPartySubframeRequestSawCookie = contains(request.span(), "Cookie: a=b"_span);
                co_await connection.awaitableSend(HTTPResponse({ { { "Access-Control-Allow-Origin"_s, "https://webkit.org"_s } }, "hi"_s }).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr dataStore = [configuration websiteDataStore];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    if (shouldRelax)
        [configuration _setShouldRelaxThirdPartyCookieBlocking:YES];

    // Seed webkit.org's cross-origin cookie.
    __block bool setCookie = false;
    RetainPtr cookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"a",
        NSHTTPCookieValue: @"b",
        NSHTTPCookieDomain: @"webkit.org",
        NSHTTPCookiePath: @"/",
        NSHTTPCookieSecure: @YES,
    }];
    [dataStore.get().httpCookieStore setCookie:cookie.get() completionHandler:^{
        setCookie = true;
    }];
    Util::run(&setCookie);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");

    // The webkit.org subframe must be in a different process than the example.com main frame.
    EXPECT_NE(findFramePID(frameTrees(webView.get()).get(), FrameType::Remote), [webView _webProcessIdentifier]);

    EXPECT_TRUE(sawSubframeResourceRequest);
    // With relaxation, the out-of-process subframe keeps third-party cookie access (no over-block). Without
    // it, the cookie is blocked as before.
    EXPECT_EQ(thirdPartySubframeRequestSawCookie, shouldRelax);
}

TEST(SiteIsolation, RelaxThirdPartyCookieBlockingSubframe)
{
    runRelaxThirdPartyCookieBlockingSubframeTest(true);
    runRelaxThirdPartyCookieBlockingSubframeTest(false);
}

// A compromised WebContent must not read another page's relaxed third-party cookies by spoofing that page's
// WebPageProxyIdentifier over IPC. Victim WKWebView has _setShouldRelaxThirdPartyCookieBlocking:YES; attacker
// WKWebView (separate WebContent, IPCTestingAPI enabled) uses CoreIPC to send GetRawCookies carrying the
// victim's WebPageProxyIdentifier. The NetworkProcess must reject via the per-process allow-list and return
// no cookies.
TEST(SiteIsolation, ThirdPartyCookieBlockingSpoofedWebPageProxyID)
{
    RetainPtr coreIPCURL = [NSBundle.test_resourcesBundle URLForResource:@"coreipc" withExtension:@"js"];
    RetainPtr coreIPCData = [NSData dataWithContentsOfURL:coreIPCURL.get()];
    RetainPtr coreIPCString = adoptNS([[NSString alloc] initWithData:coreIPCData.get() encoding:NSUTF8StringEncoding]);
    String coreIPC { coreIPCString.get() };

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/victim"_s) {
                co_await connection.awaitableSend(HTTPResponse("<iframe src='https://webkit.org/sub'></iframe>"_s).serialize());
                continue;
            }
            if (path == "/sub"_s) {
                co_await connection.awaitableSend(HTTPResponse("<script>fetch('https://webkit.org/ping', { credentials: 'include' }).then(() => alert('victim-fetched')).catch(() => alert('victim-error'));</script>"_s).serialize());
                continue;
            }
            if (path == "/ping"_s) {
                co_await connection.awaitableSend(HTTPResponse({ { { "Access-Control-Allow-Origin"_s, "https://webkit.org"_s } }, "hi"_s }).serialize());
                continue;
            }
            if (path == "/attacker"_s) {
                co_await connection.awaitableSend(HTTPResponse("<!DOCTYPE html><script src='/coreipc.js'></script><body></body>"_s).serialize());
                continue;
            }
            if (path == "/coreipc.js"_s) {
                co_await connection.awaitableSend(HTTPResponse({ { { "Content-Type"_s, "text/javascript"_s } }, coreIPC }).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr victimConfiguration = server.httpsProxyConfiguration();
    RetainPtr dataStore = [victimConfiguration websiteDataStore];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [victimConfiguration _setShouldRelaxThirdPartyCookieBlocking:YES];

    __block bool setCookie = false;
    RetainPtr cookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"a",
        NSHTTPCookieValue: @"b",
        NSHTTPCookieDomain: @"webkit.org",
        NSHTTPCookiePath: @"/",
        NSHTTPCookieSecure: @YES,
    }];
    [dataStore.get().httpCookieStore setCookie:cookie.get() completionHandler:^{
        setCookie = true;
    }];
    Util::run(&setCookie);

    auto [victimView, victimDelegate] = siteIsolatedViewAndDelegate(victimConfiguration);
    [victimView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/victim"]]];
    EXPECT_WK_STREQ([victimView _test_waitForAlert], "victim-fetched");
    uint64_t victimPageProxyID = [victimView _webPageProxyIdentifierForTesting];

    RetainPtr attackerConfiguration = adoptNS([victimConfiguration copy]);
    [attackerConfiguration _setShouldRelaxThirdPartyCookieBlocking:NO];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[attackerConfiguration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
    auto [attackerView, attackerDelegate] = siteIsolatedViewAndDelegate(attackerConfiguration);
    [attackerView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://attacker.example/attacker"]]];
    [attackerDelegate waitForDidFinishNavigation];

    EXPECT_NE([attackerView _webProcessIdentifier], [victimView _webProcessIdentifier]);

    // firstParty must be in the attacker WebContent's allowed-first-parties set, or the earlier
    // `allowsFirstPartyForCookies` MESSAGE_CHECK terminates the process before our WebPageProxyID
    // check runs. Use attacker.example (the attacker's own origin) as firstParty and webkit.org as
    // the target URL - a third-party cookie context that is only unlocked by relaxation from the
    // spoofed victim WebPageProxyIdentifier.
    NSString *attackScript = [NSString stringWithFormat:
        @"const CoreIPC = new CoreIPCClass();"
        "const reply = await new Promise(resolve => CoreIPC.Networking.NetworkConnectionToWebProcess.GetRawCookies(0, {"
        "  firstParty: { string: 'https://attacker.example/' },"
        "  sameSiteInfo: { isSameSite: false, isTopSite: false, isSafeHTTPMethod: true },"
        "  url: { string: 'https://webkit.org/' },"
        "  frameID: { optionalValue: BigInt(IPC.frameID) },"
        "  pageID: { optionalValue: BigInt(IPC.pageID) },"
        "  webPageProxyID: { optionalValue: %llun },"
        "}, resolve));"
        "const cookies = reply && reply.cookies ? reply.cookies : [];"
        "return Array.isArray(cookies) ? cookies.length : Object.keys(cookies).length;", (unsigned long long)victimPageProxyID];
    __block bool completed = false;
    __block RetainPtr<NSNumber> cookieCount;
    [attackerView callAsyncJavaScript:attackScript arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *) {
        cookieCount = [result isKindOfClass:NSNumber.class] ? result : nil;
        completed = true;
    }];
    Util::run(&completed);
    EXPECT_EQ([cookieCount unsignedIntegerValue], 0u);
}

// Editing, font, and spelling commands act on the focused frame's selection, so they must be sent to
// the process containing the focused frame. These tests put the selection in a cross-origin iframe and
// check that each command takes effect there (or that its reply describes the iframe). If the command is
// sent to the main frame's process instead, it finds the main frame's empty selection and does nothing.

TEST(SiteIsolation, ListCommandsInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><ul><li>One</li><li id='item'>Two</li></ul></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(item.firstChild, 1)", _WKSelectionAttributeIsCaret);

    auto listDepth = [&] {
        return [[webView objectByEvaluatingJavaScript:@"(() => { let depth = 0; for (let element = item.parentElement; element; element = element.parentElement) { if (element.matches('ol, ul')) ++depth; } return depth; })()" inFrame:childFrame.get()] intValue];
    };
    EXPECT_EQ(1, listDepth());

    [webView _increaseListLevel:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return listDepth() == 2;
    }));

    [webView _decreaseListLevel:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return listDepth() == 1;
    }));

    [webView _changeListType:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"item.closest('ol, ul').tagName" inFrame:childFrame.get()] isEqualToString:@"OL"];
    }));
}

// Page-wide state set on the web view after load must reach every web content process, not just the
// main frame's. A cross-origin iframe's process that already exists never hears about the change, even
// though a process created later would get it from the page's creation parameters.

TEST(SiteIsolation, SetEditableAfterCrossOriginIframeLoads)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto mainFrameIsEditable = [&] {
        return [[webView objectByEvaluatingJavaScript:@"document.body.isContentEditable"] boolValue];
    };
    auto childFrameIsEditable = [&] {
        return [[webView objectByEvaluatingJavaScript:@"document.body.isContentEditable" inFrame:childFrame.get()] boolValue];
    };
    EXPECT_FALSE(mainFrameIsEditable());
    EXPECT_FALSE(childFrameIsEditable());

    [webView _setEditable:YES];
    EXPECT_TRUE(Util::waitFor([&] {
        return mainFrameIsEditable();
    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameIsEditable();
    }));

    [webView _setEditable:NO];
    EXPECT_TRUE(Util::waitFor([&] {
        return !mainFrameIsEditable() && !childFrameIsEditable();
    }));
}

TEST(SiteIsolation, SetMediaTypeAfterCrossOriginIframeLoads)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto mainFrameMatchesPrint = [&] {
        return [[webView objectByEvaluatingJavaScript:@"matchMedia('print').matches"] boolValue];
    };
    auto childFrameMatchesPrint = [&] {
        return [[webView objectByEvaluatingJavaScript:@"matchMedia('print').matches" inFrame:childFrame.get()] boolValue];
    };
    EXPECT_FALSE(mainFrameMatchesPrint());
    EXPECT_FALSE(childFrameMatchesPrint());

    webView.get().mediaType = @"print";
    EXPECT_TRUE(Util::waitFor([&] {
        return mainFrameMatchesPrint();
    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameMatchesPrint();
    }));

    webView.get().mediaType = nil;
    EXPECT_TRUE(Util::waitFor([&] {
        return !mainFrameMatchesPrint() && !childFrameMatchesPrint();
    }));
}

TEST(SiteIsolation, SetResourceLoadDelegateAfterCrossOriginIframeLoads)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } },
        { "/subresource"_s, { "subresource"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];

    // The subresource load below happens after the cross-origin iframe's process already exists, so the
    // delegate reaches it only if setResourceLoadClient() is sent to every content process, not just the
    // main frame's.
    RetainPtr resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    __block bool sawSubresourceRequest = false;
    [resourceLoadDelegate setDidSendRequest:^(WKWebView *, _WKResourceLoadInfo *, NSURLRequest *request) {
        if ([request.URL.path isEqualToString:@"/subresource"])
            sawSubresourceRequest = true;
    }];
    webView.get()._resourceLoadDelegate = resourceLoadDelegate.get();

    [webView objectByEvaluatingJavaScript:@"fetch('/subresource'); true" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::runFor(&sawSubresourceRequest, 5_s));

    webView.get()._resourceLoadDelegate = nil;
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

TEST(SiteIsolation, PauseAllAnimationsAfterCrossOriginIframeLoads)
{
    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"test-mse" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.setHeaderField("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body><img id='img' src='/test-mse.mp4'></body>"_s } },
        { "/test-mse.mp4"_s, videoResponse }
    }, HTTPServer::Protocol::HttpsProxy);

    // Use the internals-enabled plug-in to observe image animation state inside the cross-origin iframe, and
    // point the data store at the test HTTPS proxy, since _test_configurationWithTestPlugInClassName: doesn't set one up.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto childFrameImageIsAnimating = [&] {
        return [[webView objectByEvaluatingJavaScript:@"window.internals.isImageAnimating(document.getElementById('img'))" inFrame:childFrame.get()] boolValue];
    };
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameImageIsAnimating();
    }));

    // Pausing only takes effect when the system allows animation controls, so turn that on in the iframe's process.
    [webView objectByEvaluatingJavaScript:@"window.internals.setImageAnimationEnabled(false); true" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return !childFrameImageIsAnimating();
    }));

    __block bool done = false;
    [webView _playAllAnimationsWithCompletionHandler:^{
        done = true;
    }];
    EXPECT_TRUE(Util::runFor(&done, 5_s));
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameImageIsAnimating();
    }));

    done = false;
    [webView _pauseAllAnimationsWithCompletionHandler:^{
        done = true;
    }];
    EXPECT_TRUE(Util::runFor(&done, 5_s));
    EXPECT_TRUE(Util::waitFor([&] {
        return !childFrameImageIsAnimating();
    }));
}

#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

TEST(SiteIsolation, FontAttributesDelegateSetAfterFocusingCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable style='font-size: 37px'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 3)", _WKSelectionAttributeIsCaret);

    // Let any editor state updates for the new selection arrive before the delegate is set, so that the
    // only thing that can report font attributes is the web process learning that they are now needed.
    [webView waitForNextPresentationUpdate];

    // Setting a delegate that wants font attributes tells the web processes to start computing them and
    // to send a fresh editor state. Only the focused iframe's process can report its font.
    RetainPtr listener = adoptNS([SiteIsolationFontAttributesListener new]);
    [webView setUIDelegate:listener.get()];
    EXPECT_TRUE(Util::waitFor([&] {
#if PLATFORM(MAC)
        NSFont *font = [listener lastFontAttributes][NSFontAttributeName];
#else
        UIFont *font = [listener lastFontAttributes][NSFontAttributeName];
#endif
        return font.pointSize == 37;
    }));
}

// Pasteboard, Services, and content insertion act on the focused frame's selection, so they must be sent to
// the process containing the focused frame, and any pasteboard access must be granted to that process.

#if ENABLE(MULTI_REPRESENTATION_HEIC)

TEST(SiteIsolation, InsertAdaptiveImageGlyphInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 8)", _WKSelectionAttributeIsCaret);

    RetainPtr data = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];
    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:data.get()]);
#if PLATFORM(MAC)
    [(id<NSTextInputClient>)webView.get() insertAdaptiveImageGlyph:adaptiveImageGlyph.get() replacementRange:NSMakeRange(0, 0)];
#else
    RetainPtr range = adoptNS([[UITextRange alloc] init]);
    [[webView textInputContentView] insertAdaptiveImageGlyph:adaptiveImageGlyph.get() replacementRange:range.get()];
#endif

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"!!document.querySelector('picture')" inFrame:childFrame.get()] boolValue];
    }));
}

#endif // ENABLE(MULTI_REPRESENTATION_HEIC)

// Dictation acts on the focused frame's selection, so it must be sent to the process containing the focused frame.
// A text placeholder must be removed by the process whose document contains it, even if focus has moved since.

TEST(SiteIsolation, TextPlaceholderInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><input id='input'><iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 8)", _WKSelectionAttributeIsCaret);

#if PLATFORM(MAC)
    using TextPlaceholder = NSTextPlaceholder;
    id<NSTextInputClient_Async> textInput = (id<NSTextInputClient_Async>)webView.get();
#else
    using TextPlaceholder = UITextPlaceholder;
    auto textInput = [webView textInputContentView];
#endif

    __block RetainPtr<TextPlaceholder> placeholder;
    __block bool done = false;
    [textInput insertTextPlaceholderWithSize:CGSizeMake(50, 100) completionHandler:^(TextPlaceholder *insertedPlaceholder) {
        placeholder = insertedPlaceholder;
        done = true;
    }];
    Util::run(&done);
    ASSERT_NOT_NULL(placeholder.get());
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"!!document.querySelector('div')" inFrame:childFrame.get()] boolValue]);

    [webView objectByEvaluatingJavaScript:@"input.focus()"];
    EXPECT_TRUE(Util::waitFor([&] {
        return ![[webView firstChildFrame] _isFocused];
    }));

    done = false;
    [textInput removeTextPlaceholder:placeholder.get() willInsertText:NO completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!document.querySelector('div')" inFrame:childFrame.get()] boolValue]);
}

#if ENABLE(ATTACHMENT_ELEMENT)

// Inserting an attachment acts on the focused frame's selection, so it must go to the focused frame's process.
// Updates and icons for an existing attachment must go to the process whose document contains it, and that
// process must be able to find the element even when it isn't in the main frame's document.

static RetainPtr<WKWebViewConfiguration> attachmentEnabledConfiguration(const HTTPServer& server)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration _setAttachmentElementEnabled:YES];
    return configuration;
}

static RetainPtr<NSFileWrapper> textFileWrapper(NSString *filename)
{
    RetainPtr fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:[@"Hello world" dataUsingEncoding:NSUTF8StringEncoding]]);
    [fileWrapper setPreferredFilename:filename];
    return fileWrapper;
}

static NSString * const attachmentTitleScript = @"document.querySelector('attachment')?.getAttribute('title') ?? ''";

TEST(SiteIsolation, InsertAttachmentInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = attachmentEnabledConfiguration(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body, 0)", _WKSelectionAttributeIsCaret);

    // The completion handler runs whether or not anything was inserted, so check the iframe's document.
    __block bool done = false;
    [webView _insertAttachmentWithFileWrapper:textFileWrapper(@"hello.txt").get() contentType:@"text/plain" completion:^(BOOL) {
        done = true;
    }];
    Util::run(&done);

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:attachmentTitleScript inFrame:childFrame.get()] isEqualToString:@"hello.txt"];
    }));
}

TEST(SiteIsolation, SetFileWrapperForAttachmentInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body><attachment title='original.txt' type='text/plain'></attachment></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(attachmentEnabledConfiguration(server), CGRectMake(0, 0, 800, 600));
    RetainPtr observer = adoptNS([SiteIsolationAttachmentObserver new]);
    [webView setUIDelegate:observer.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // The iframe's process reports the attachment when its element is connected.
    EXPECT_TRUE(Util::waitFor([&] {
        return [observer insertedAttachments].count == 1;
    }));
    RetainPtr<_WKAttachment> attachment = [observer insertedAttachments].firstObject;

    __block bool done = false;
    [attachment setFileWrapper:textFileWrapper(@"updated.txt").get() contentType:@"text/plain" completion:^(NSError *) {
        done = true;
    }];
    Util::run(&done);

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:attachmentTitleScript inFrame:childFrame.get()] isEqualToString:@"updated.txt"];
    }));
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(APP_HIGHLIGHTS)

// Creating an app highlight acts on the focused frame's selection, so it must go to the focused frame's process.
// The request must also complete even when there's nothing to highlight: under site isolation the UI process
// crashes on a failed message check, and a request that's never answered is eventually cancelled with an empty
// highlight, which fails the check.

TEST(SiteIsolation, AddAppHighlightInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr delegate = adoptNS([SiteIsolationAppHighlightDelegate new]);
    [webView _setAppHighlightDelegate:delegate.get()];
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    [webView _addAppHighlight];
    EXPECT_TRUE(Util::waitFor([&] {
        return [delegate storedHighlights].count == 1;
    }));
    EXPECT_WK_STREQ("subframe text", [delegate storedHighlights].firstObject.text);
}

TEST(SiteIsolation, AddAppHighlightWithoutSelectionDoesNotCrashWhenWebProcessesExit)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr delegate = adoptNS([SiteIsolationAppHighlightDelegate new]);
    [webView _setAppHighlightDelegate:delegate.get()];

    // Nothing is selected in either frame, so whichever process gets the request has nothing to highlight.
    [webView _addAppHighlight];
    [webView waitForNextPresentationUpdate];

    // Any request that was never answered is cancelled when its process exits. That must not crash the UI process.
    pid_t mainFramePID = [webView mainFrame].info._processIdentifier;
    pid_t childFramePID = [webView firstChildFrame]._processIdentifier;
    EXPECT_NE(mainFramePID, childFramePID);
    kill(childFramePID, SIGKILL);
    kill(mainFramePID, SIGKILL);
    while (!kill(childFramePID, 0) || !kill(mainFramePID, 0))
        Util::spinRunLoop();
    Util::runFor(0.5_s);

    EXPECT_EQ(0U, [delegate storedHighlights].count);
}

#endif // ENABLE(APP_HIGHLIGHTS)

} // namespace TestWebKitAPI
