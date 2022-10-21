/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestDownloadDelegate.h"
#import "TestLegacyDownloadDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import <Foundation/NSURLResponse.h>
#import <WebKit/WKDownload.h>
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationResponsePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebpagePreferences.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKDownload.h>
#import <WebKit/_WKDownloadDelegate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/MainThread.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include <pal/spi/mac/QuarantineSPI.h>
#endif

static unsigned redirectCount = 0;
static bool hasReceivedResponse;
static NSURL *sourceURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
static WKWebView* expectedOriginatingWebView;
static bool expectedUserInitiatedState = false;

@interface DownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation DownloadDelegate {
    RetainPtr<_WKDownload> _download;
    String _destinationPath;
    long long _expectedContentLength;
    uint64_t _receivedContentLength;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_NULL(_download);
    EXPECT_NOT_NULL(download);
    EXPECT_TRUE([[[[download request] URL] path] isEqualToString:[sourceURL path]]);
    EXPECT_EQ(expectedUserInitiatedState, download.wasUserInitiated);
    _download = download;
}

- (void)_download:(_WKDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    hasReceivedResponse = true;
    EXPECT_EQ(_download, download);
    EXPECT_TRUE(_expectedContentLength == 0);
    EXPECT_TRUE(_receivedContentLength == 0);
    EXPECT_TRUE([[[response URL] path] isEqualToString:[sourceURL path]]);
    _expectedContentLength = [response expectedContentLength];
}

- (void)_download:(_WKDownload *)download didReceiveData:(uint64_t)length
{
    EXPECT_EQ(_download, download);
    _receivedContentLength += length;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSString *)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename allowOverwrite:(BOOL *)allowOverwrite
IGNORE_WARNINGS_END
{
    EXPECT_TRUE(hasReceivedResponse);
    EXPECT_EQ(_download, download);

    FileSystem::PlatformFileHandle fileHandle;
    _destinationPath = FileSystem::openTemporaryFile("TestWebKitAPI"_s, fileHandle);
    EXPECT_TRUE(fileHandle != FileSystem::invalidPlatformFileHandle);
    FileSystem::closeFile(fileHandle);

    *allowOverwrite = YES;
    return _destinationPath;
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_EQ(_download, download);
    EXPECT_EQ(expectedUserInitiatedState, download.wasUserInitiated);
    EXPECT_TRUE(_expectedContentLength == NSURLResponseUnknownLength || static_cast<uint64_t>(_expectedContentLength) == _receivedContentLength);
    EXPECT_TRUE([[NSFileManager defaultManager] contentsEqualAtPath:_destinationPath andPath:[sourceURL path]]);
    FileSystem::deleteFile(_destinationPath);
    isDone = true;
}

@end

TEST(_WKDownload, DownloadDelegate)
{
    RetainPtr<WKProcessPool> processPool = adoptNS([[WKProcessPool alloc] init]);
    auto downloadDelegate = adoptNS([[DownloadDelegate alloc] init]);
    [processPool _setDownloadDelegate:downloadDelegate.get()];

    @autoreleasepool {
        EXPECT_EQ(downloadDelegate.get(), [processPool _downloadDelegate]);
    }

    downloadDelegate = nil;
    EXPECT_NULL([processPool _downloadDelegate]);
}

static void runTest(id <WKNavigationDelegate> navigationDelegate, id <_WKDownloadDelegate> downloadDelegate, NSURL *url)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate];
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate];

    isDone = false;
    hasReceivedResponse = false;
    expectedUserInitiatedState = false;
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&isDone);
}

@interface DownloadNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DownloadNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(_WKNavigationActionPolicyDownload);
}
@end

TEST(_WKDownload, DownloadRequest)
{
    runTest(adoptNS([[DownloadNavigationDelegate alloc] init]).get(), adoptNS([[DownloadDelegate alloc] init]).get(), sourceURL);
}

@interface ConvertResponseToDownloadNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ConvertResponseToDownloadNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(WKNavigationResponsePolicyDownload);
}
@end

TEST(_WKDownload, ConvertResponseToDownload)
{
    runTest(adoptNS([[ConvertResponseToDownloadNavigationDelegate alloc] init]).get(), adoptNS([[DownloadDelegate alloc] init]).get(), sourceURL);
}

@interface FailingDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation FailingDownloadDelegate

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_TRUE(false);
    isDone = true;
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    isDone = true;
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    EXPECT_TRUE(false);
    isDone = true;
}

@end

TEST(_WKDownload, DownloadMissingResource)
{
    runTest(adoptNS([[DownloadNavigationDelegate alloc] init]).get(), adoptNS([[FailingDownloadDelegate alloc] init]).get(), [NSURL URLWithString:@"non-existant-scheme://"]);
}

@interface CancelledDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation CancelledDownloadDelegate

- (void)_downloadDidStart:(_WKDownload *)download
{
    [download cancel];
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_TRUE(false);
    isDone = true;
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    EXPECT_TRUE(false);
    isDone = true;
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    isDone = true;
}

@end

TEST(_WKDownload, CancelDownload)
{
    runTest(adoptNS([[DownloadNavigationDelegate alloc] init]).get(), adoptNS([[CancelledDownloadDelegate alloc] init]).get(), sourceURL);
}

@interface OriginatingWebViewDownloadDelegate : NSObject <_WKDownloadDelegate>
- (instancetype)initWithWebView:(WKWebView *)webView;
@end

@implementation OriginatingWebViewDownloadDelegate {
    RetainPtr<WKWebView> _webView;
}

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    return self;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_EQ([download originatingWebView], _webView);
    [_webView _killWebContentProcessAndResetState];
    _webView = nullptr;

    WTF::callOnMainThread([download = retainPtr(download)] {
        EXPECT_NULL([download originatingWebView]);
        isDone = true;
    });
}

@end

TEST(_WKDownload, OriginatingWebView)
{
    RetainPtr<DownloadNavigationDelegate> navigationDelegate = adoptNS([[DownloadNavigationDelegate alloc] init]);                 
    RetainPtr<OriginatingWebViewDownloadDelegate> downloadDelegate;
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        downloadDelegate = adoptNS([[OriginatingWebViewDownloadDelegate alloc] initWithWebView:webView.get()]);
        [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:sourceURL]];
    }

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

@interface DownloadRequestOriginalURLDelegate : NSObject <_WKDownloadDelegate>
- (instancetype)initWithExpectedOriginalURL:(NSURL *)expectOriginalURL;
@end

@implementation DownloadRequestOriginalURLDelegate {
    NSURL *_expectedOriginalURL;
}

- (instancetype)initWithExpectedOriginalURL:(NSURL *)expectedOriginalURL
{
    if (!(self = [super init]))
        return nil;

    _expectedOriginalURL = expectedOriginalURL;
    return self;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    if ([_expectedOriginalURL isEqual:sourceURL])
        EXPECT_TRUE(!download.request.mainDocumentURL);
    else
        EXPECT_TRUE([_expectedOriginalURL isEqual:download.request.mainDocumentURL]);
    isDone = true;
}

@end

@interface DownloadRequestOriginalURLNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DownloadRequestOriginalURLNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if ([navigationAction.request.URL isEqual:sourceURL])
        decisionHandler(_WKNavigationActionPolicyDownload);
    else
        decisionHandler(WKNavigationActionPolicyAllow);
}
@end

TEST(_WKDownload, DownloadRequestOriginalURL)
{
    NSURL *originalURL = [[NSBundle mainBundle] URLForResource:@"DownloadRequestOriginalURL" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    runTest(adoptNS([[DownloadRequestOriginalURLNavigationDelegate alloc] init]).get(), adoptNS([[DownloadRequestOriginalURLDelegate alloc] initWithExpectedOriginalURL:originalURL]).get(), originalURL);
}

TEST(_WKDownload, DownloadRequestOriginalURLFrame)
{
    NSURL *originalURL = [[NSBundle mainBundle] URLForResource:@"DownloadRequestOriginalURL2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    runTest(adoptNS([[DownloadRequestOriginalURLNavigationDelegate alloc] init]).get(), adoptNS([[DownloadRequestOriginalURLDelegate alloc] initWithExpectedOriginalURL:originalURL]).get(), originalURL);
}

TEST(_WKDownload, DownloadRequestOriginalURLDirectDownload)
{
    runTest(adoptNS([[DownloadRequestOriginalURLNavigationDelegate alloc] init]).get(), adoptNS([[DownloadRequestOriginalURLDelegate alloc] initWithExpectedOriginalURL:sourceURL]).get(), sourceURL);
}

TEST(_WKDownload, DownloadRequestOriginalURLDirectDownloadWithLoadedContent)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto navigationDelegate = adoptNS([[DownloadRequestOriginalURLNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto downloadDelegate = adoptNS([[DownloadRequestOriginalURLDelegate alloc] initWithExpectedOriginalURL:sourceURL]);
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    expectedUserInitiatedState = false;
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    // Here is to test if the original URL can be set correctly when the current document
    // is completely unrelated to the download.
    [webView loadRequest:[NSURLRequest requestWithURL:contentURL]];
    [webView loadRequest:[NSURLRequest requestWithURL:sourceURL]];
    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

@interface BlobDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation BlobDownloadDelegate {
    RetainPtr<_WKDownload> _download;
    String _destinationPath;
    long long _expectedContentLength;
    uint64_t _receivedContentLength;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_NULL(_download);
    EXPECT_NOT_NULL(download);
    EXPECT_TRUE([[[[download request] URL] scheme] isEqualToString:@"blob"]);
    EXPECT_EQ(expectedUserInitiatedState, download.wasUserInitiated);
    _download = download;
}

- (void)_download:(_WKDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    hasReceivedResponse = true;
    EXPECT_EQ(_download, download);
    EXPECT_EQ(_expectedContentLength, 0U);
    EXPECT_EQ(_receivedContentLength, 0U);
    EXPECT_TRUE([[[response URL] scheme] isEqualToString:@"blob"]);
    _expectedContentLength = [response expectedContentLength];
}

- (void)_download:(_WKDownload *)download didReceiveData:(uint64_t)length
{
    EXPECT_EQ(_download, download);
    _receivedContentLength += length;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSString *)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename allowOverwrite:(BOOL *)allowOverwrite
IGNORE_WARNINGS_END
{
    EXPECT_TRUE(hasReceivedResponse);
    EXPECT_EQ(_download, download);

    FileSystem::PlatformFileHandle fileHandle;
    _destinationPath = FileSystem::openTemporaryFile("TestWebKitAPI"_s, fileHandle);
    EXPECT_TRUE(fileHandle != FileSystem::invalidPlatformFileHandle);
    FileSystem::closeFile(fileHandle);

    *allowOverwrite = YES;
    return _destinationPath;
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_EQ(_download, download);
    EXPECT_EQ(expectedUserInitiatedState, download.wasUserInitiated);
    EXPECT_TRUE(_expectedContentLength == NSURLResponseUnknownLength || static_cast<uint64_t>(_expectedContentLength) == _receivedContentLength);
    NSString* expectedContent = @"{\"x\":42,\"s\":\"hello, world\"}";
    NSData* expectedData = [expectedContent dataUsingEncoding:NSUTF8StringEncoding];
    EXPECT_TRUE([[[NSFileManager defaultManager] contentsAtPath:_destinationPath] isEqualToData:expectedData]);
    FileSystem::deleteFile(_destinationPath);
    isDone = true;
}

@end

@interface DownloadBlobURLNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DownloadBlobURLNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if ([navigationAction.request.URL.scheme isEqualToString:@"blob"])
        decisionHandler(_WKNavigationActionPolicyDownload);
    else
        decisionHandler(WKNavigationActionPolicyAllow);
}
@end

TEST(_WKDownload, DownloadRequestBlobURL)
{
    NSURL *originalURL = [[NSBundle mainBundle] URLForResource:@"DownloadRequestBlobURL" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    runTest(adoptNS([[DownloadBlobURLNavigationDelegate alloc] init]).get(), adoptNS([[BlobDownloadDelegate alloc] init]).get(), originalURL);
}

@interface RedirectedDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation RedirectedDownloadDelegate {
    String _destinationPath;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_NOT_NULL(download);
    EXPECT_EQ(expectedOriginatingWebView, download.originatingWebView);
    EXPECT_EQ(expectedUserInitiatedState, download.wasUserInitiated);
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSString *)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename allowOverwrite:(BOOL *)allowOverwrite
IGNORE_WARNINGS_END
{
    FileSystem::PlatformFileHandle fileHandle;
    _destinationPath = FileSystem::openTemporaryFile("TestWebKitAPI"_s, fileHandle);
    EXPECT_TRUE(fileHandle != FileSystem::invalidPlatformFileHandle);
    FileSystem::closeFile(fileHandle);
    *allowOverwrite = YES;
    return _destinationPath;
}

- (void)_download:(_WKDownload *)download didReceiveServerRedirectToURL:(NSURL *)url
{
    if (!redirectCount)
        EXPECT_STREQ("http://redirect/?pass", [url.absoluteString UTF8String]);
    else
        EXPECT_STREQ("http://pass/", [url.absoluteString UTF8String]);
    ++redirectCount = true;
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_EQ(expectedUserInitiatedState, download.wasUserInitiated);

    NSArray<NSURL *> *redirectChain = download.redirectChain;
    EXPECT_EQ(3U, redirectChain.count);
    if (redirectChain.count > 0)
        EXPECT_STREQ("http://redirect/?redirect/?pass", [redirectChain[0].absoluteString UTF8String]);
    if (redirectChain.count > 1)
        EXPECT_STREQ("http://redirect/?pass", [redirectChain[1].absoluteString UTF8String]);
    if (redirectChain.count > 2)
        EXPECT_STREQ("http://pass/", [redirectChain[2].absoluteString UTF8String]);

    FileSystem::deleteFile(_destinationPath);
    isDone = true;
}

@end

TEST(_WKDownload, RedirectedDownload)
{
    [TestProtocol registerWithScheme:@"http"];

    redirectCount = 0;
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto downloadDelegate = adoptNS([[RedirectedDownloadDelegate alloc] init]);
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    // Do 2 loads in the same view to make sure the redirect chain is properly cleared between loads.
    [webView synchronouslyLoadHTMLString:@"<div>First load</div>"];
    [webView synchronouslyLoadHTMLString:@"<a id='link' href='http://redirect/?redirect/?pass'>test</a>"];

    expectedOriginatingWebView = webView.get();
    expectedUserInitiatedState = true;

    auto navigationDelegate = adoptNS([[DownloadNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('link').click();"];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(1U, redirectCount);

    [TestProtocol unregister];
}

TEST(_WKDownload, RedirectedLoadConvertedToDownload)
{
    [TestProtocol registerWithScheme:@"http"];

    auto navigationDelegate = adoptNS([[ConvertResponseToDownloadNavigationDelegate alloc] init]);
    auto downloadDelegate = adoptNS([[RedirectedDownloadDelegate alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    expectedOriginatingWebView = webView.get();
    expectedUserInitiatedState = false;
    isDone = false;
    redirectCount = 0;
    hasReceivedResponse = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect/?redirect/?pass"]]];
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(0U, redirectCount);

    [TestProtocol unregister];
}

TEST(_WKDownload, RedirectedSubframeLoadConvertedToDownload)
{
    [TestProtocol registerWithScheme:@"http"];

    auto navigationDelegate = adoptNS([[ConvertResponseToDownloadNavigationDelegate alloc] init]);
    auto downloadDelegate = adoptNS([[RedirectedDownloadDelegate alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    expectedOriginatingWebView = webView.get();
    expectedUserInitiatedState = false;
    isDone = false;
    redirectCount = 0;
    hasReceivedResponse = false;
    [webView loadHTMLString:@"<body><iframe src='http://redirect/?redirect/?pass'></iframe></body>" baseURL:nil];
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(0U, redirectCount);

    [TestProtocol unregister];
}

static bool downloadHasDecidedDestination;

@interface CancelDownloadWhileDecidingDestinationDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation CancelDownloadWhileDecidingDestinationDelegate
- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_TRUE(false);
    isDone = true;
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    EXPECT_TRUE(false);
    isDone = true;
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    isDone = true;
}

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL allowOverwrite, NSString *destination))completionHandler
{
    [download cancel];
    TestWebKitAPI::Util::run(&isDone);
    completionHandler(YES, @"/tmp/WebKitAPITest/_WKDownload");
    downloadHasDecidedDestination = true;
}
@end

TEST(_WKDownload, DownloadCanceledWhileDecidingDestination)
{
    [TestProtocol registerWithScheme:@"http"];

    auto navigationDelegate = adoptNS([[DownloadNavigationDelegate alloc] init]);
    auto downloadDelegate = adoptNS([[CancelDownloadWhileDecidingDestinationDelegate alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    isDone = false;
    downloadHasDecidedDestination = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://pass"]]];

    TestWebKitAPI::Util::run(&downloadHasDecidedDestination);

    [TestProtocol unregister];
}

@interface BlobWithUSDZExtensionDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation BlobWithUSDZExtensionDownloadDelegate {
    String _destinationPath;
}

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL allowOverwrite, NSString *destination))completionHandler
{
    EXPECT_TRUE([filename hasSuffix:@".usdz"]);

    FileSystem::PlatformFileHandle fileHandle;
    _destinationPath = FileSystem::openTemporaryFile(String { filename }, fileHandle);
    EXPECT_TRUE(fileHandle != FileSystem::invalidPlatformFileHandle);
    FileSystem::closeFile(fileHandle);

    completionHandler(YES, _destinationPath);
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    FileSystem::deleteFile(_destinationPath);
    isDone = true;
}

@end

TEST(_WKDownload, SystemPreviewUSDZBlobNaming)
{
    NSURL *originalURL = [[NSBundle mainBundle] URLForResource:@"SystemPreviewBlobNaming" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    runTest(adoptNS([[DownloadBlobURLNavigationDelegate alloc] init]).get(), adoptNS([[BlobWithUSDZExtensionDownloadDelegate alloc] init]).get(), originalURL);
}

@interface DownloadAttributeTestDelegate : NSObject <WKNavigationDelegate, _WKDownloadDelegate>
@property (nonatomic, readonly) bool didFinishNavigation;
@property (nonatomic, readonly) bool didStartProvisionalNavigation;
@property (nonatomic, readonly) bool downloadDidStart;
@property (nonatomic) WKNavigationActionPolicy navigationActionPolicy;
@end

@implementation DownloadAttributeTestDelegate

- (instancetype)init
{
    if ((self = [super init]))
        _navigationActionPolicy = WKNavigationActionPolicyAllow;

    return self;
}

- (void)waitForDidFinishNavigation
{
    TestWebKitAPI::Util::run(&_didFinishNavigation);
    _didStartProvisionalNavigation = false;
    _didFinishNavigation = false;
}

- (void)waitForDownloadDidStart
{
    TestWebKitAPI::Util::run(&_downloadDidStart);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _didFinishNavigation = true;
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    _didStartProvisionalNavigation = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(_navigationActionPolicy);
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    _downloadDidStart = true;
}

@end

TEST(_WKDownload, DownloadAttributeDoesNotStartDownloads)
{
    auto delegate = adoptNS([[DownloadAttributeTestDelegate alloc] init]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowTopNavigationToDataURLs = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView setNavigationDelegate:delegate.get()];
    [webView configuration].processPool._downloadDelegate = delegate.get();

    [webView loadHTMLString:@"<a id='link' href='data:,test' download>Click me!</a>" baseURL:nil];
    [delegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE([delegate downloadDidStart]);
}

TEST(_WKDownload, StartDownloadWithDownloadAttribute)
{
    auto delegate = adoptNS([[DownloadAttributeTestDelegate alloc] init]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowTopNavigationToDataURLs = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView setNavigationDelegate:delegate.get()];
    [webView configuration].processPool._downloadDelegate = delegate.get();

    [webView loadHTMLString:@"<a id='link' href='data:,test' download>Click me!</a>" baseURL:nil];
    [delegate waitForDidFinishNavigation];

    [delegate setNavigationActionPolicy:_WKNavigationActionPolicyDownload];
    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    [delegate waitForDownloadDidStart];
    EXPECT_FALSE([delegate didStartProvisionalNavigation]);
}

static bool didDownloadStart;

@interface WaitUntilDownloadCanceledDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation WaitUntilDownloadCanceledDelegate

- (void)_downloadDidStart:(_WKDownload *)download
{
    didDownloadStart = true;
    [download.originatingWebView _killWebContentProcessAndResetState];
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
    EXPECT_EQ(error.code, NSURLErrorCancelled);
    isDone = true;
}

@end

TEST(_WKDownload, CrashAfterDownloadDidFinishWhenDownloadProxyHoldsTheLastRefOnWebProcessPool)
{
    auto navigationDelegate = adoptNS([[DownloadNavigationDelegate alloc] init]);
    auto downloadDelegate = adoptNS([[WaitUntilDownloadCanceledDelegate alloc] init]);
    WeakObjCPtr<WKProcessPool> processPool;
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        processPool = [webView configuration].processPool;
        [webView configuration].processPool._downloadDelegate = downloadDelegate.get();
        [webView loadRequest:[NSURLRequest requestWithURL:sourceURL]];

        didDownloadStart = false;
        TestWebKitAPI::Util::run(&didDownloadStart);
    }

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_NULL(processPool.get());
}

static bool receivedData;
static RetainPtr<NSString> destination;

@interface DownloadMonitorTestDelegate : NSObject <_WKDownloadDelegate>
- (void)waitForDidFail;
- (void)stopWaitingForDidFail;
@end

@implementation DownloadMonitorTestDelegate {
    bool didFail;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    didDownloadStart = true;
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
    EXPECT_EQ(error.code, NSURLErrorCancelled);
    didFail = true;
}

- (void)waitForDidFail
{
    didFail = false;
    while (!didFail)
        TestWebKitAPI::Util::spinRunLoop();
}

- (void)stopWaitingForDidFail
{
    EXPECT_FALSE(didFail);
    didFail = true;
}

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL allowOverwrite, NSString *destination))completionHandler
{
    EXPECT_TRUE([filename isEqualToString:@"filename.dat"]);
    destination = [NSTemporaryDirectory() stringByAppendingPathComponent:filename];
    completionHandler(YES, destination.get());
}

- (void)_download:(_WKDownload *)download didReceiveData:(uint64_t)length
{
    receivedData = true;
}

@end

namespace TestWebKitAPI {

void respondSlowly(const Connection& connection, double kbps)
{
    EXPECT_TRUE(isMainThread());

    const double writesPerSecond = 100;
    Vector<uint8_t> writeBuffer(static_cast<size_t>(1024 * kbps / writesPerSecond));
    auto before = MonotonicTime::now();
    connection.send(WTFMove(writeBuffer), [=] {
        double writeDuration = (MonotonicTime::now() - before).seconds();
        double desiredSleep = 1.0 / writesPerSecond;
        if (writeDuration < desiredSleep)
            usleep(USEC_PER_SEC * (desiredSleep - writeDuration));
        respondSlowly(connection, kbps);
    });
}

static RetainPtr<DownloadMonitorTestDelegate> monitorDelegate()
{
    static auto delegate = adoptNS([DownloadMonitorTestDelegate new]);
    return delegate;
}

RetainPtr<WKWebView> webViewWithDownloadMonitorSpeedMultiplier(size_t multiplier)
{
    static auto navigationDelegate = adoptNS([DownloadNavigationDelegate new]);
    auto processPoolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [dataStoreConfiguration setTestSpeedMultiplier:multiplier];
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [webViewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView configuration].processPool._downloadDelegate = monitorDelegate().get();
    return webView;
}

enum class AppReturnsToForeground { No, Yes };
    
void downloadAtRate(double desiredKbps, unsigned speedMultiplier, AppReturnsToForeground returnToForeground = AppReturnsToForeground::No)
{
    HTTPServer server([=](const Connection& connection) {
        connection.receiveHTTPRequest([=](Vector<char>&&) {
            constexpr auto responseHeader =
            "HTTP/1.1 200 OK\r\n"
            "Content-Disposition: attachment; filename=\"filename.dat\"\r\n"
            "Content-Length: 100000000\r\n\r\n"_s;
            connection.send(responseHeader, [=] {
                respondSlowly(connection, desiredKbps);
            });
        });
    });
    
    auto webView = webViewWithDownloadMonitorSpeedMultiplier(speedMultiplier);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    receivedData = false;
    Util::run(&receivedData);
    // Start the DownloadMonitor's timer.
    [[webView configuration].websiteDataStore _synthesizeAppIsBackground:YES];
    if (returnToForeground == AppReturnsToForeground::Yes)
        [[webView configuration].websiteDataStore _synthesizeAppIsBackground:NO];
    [monitorDelegate() waitForDidFail];
    [[NSFileManager defaultManager] removeItemAtURL:[NSURL fileURLWithPath:destination.get() isDirectory:NO] error:nil];
}

TEST(_WKDownload, DownloadMonitorCancel)
{
    downloadAtRate(0.5, 120); // Should cancel in ~0.5 seconds
    downloadAtRate(1.5, 120); // Should cancel in ~2.5 seconds
}

TEST(_WKDownload, DISABLED_DownloadMonitorSurvive)
{
    __block BOOL timeoutReached = NO;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2.5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [monitorDelegate() stopWaitingForDidFail];
        timeoutReached = YES;
    });

    // Simulates an hour of downloading 150kb/s in 1 second.
    // Timeout should be reached before this is cancelled because the download rate is high enough.
    downloadAtRate(150.0, 3600);
    EXPECT_TRUE(timeoutReached);
}

TEST(_WKDownload, DownloadMonitorReturnToForeground)
{
    __block BOOL timeoutReached = NO;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2.5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [monitorDelegate() stopWaitingForDidFail];
        timeoutReached = YES;
    });
    downloadAtRate(0.5, 120, AppReturnsToForeground::Yes);
    EXPECT_TRUE(timeoutReached);
}

} // namespace TestWebKitAPI

@interface TestDownloadNavigationResponseFromMemoryCacheDelegate : NSObject <WKNavigationDelegate, _WKDownloadDelegate>
@property (nonatomic) WKNavigationResponsePolicy responsePolicy;
@property (nonatomic, readonly) BOOL didFailProvisionalNavigation;
@property (nonatomic, readonly) BOOL didFinishNavigation;
@property (nonatomic, readonly) BOOL didStartDownload;
@end

@implementation TestDownloadNavigationResponseFromMemoryCacheDelegate

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    _didFailProvisionalNavigation = NO;
    _didFinishNavigation = NO;
    _didStartDownload = NO;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    EXPECT_EQ(_WKErrorCodeFrameLoadInterruptedByPolicyChange, error.code);
    EXPECT_FALSE(_didFinishNavigation);
    EXPECT_WK_STREQ(_WKLegacyErrorDomain, error.domain);
    _didFailProvisionalNavigation = YES;
    if (_responsePolicy != WKNavigationResponsePolicyDownload || _didStartDownload)
        isDone = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    EXPECT_FALSE(_didFailProvisionalNavigation);
    _didFinishNavigation = YES;
    if (_responsePolicy != WKNavigationResponsePolicyDownload || _didStartDownload)
        isDone = true;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    _didStartDownload = YES;
    if (_didFailProvisionalNavigation || _didFinishNavigation)
        isDone = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(_responsePolicy);
}

@end

TEST(WebKit, DownloadNavigationResponseFromMemoryCache)
{
    [TestProtocol registerWithScheme:@"http"];
    TestProtocol.additionalResponseHeaders = @{ @"Cache-Control" : @"max-age=3600" };

    auto delegate = adoptNS([[TestDownloadNavigationResponseFromMemoryCacheDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView configuration].processPool._downloadDelegate = delegate.get();

    NSURL *firstURL = [NSURL URLWithString:@"http://bundle-file/simple.html"];
    [delegate setResponsePolicy:WKNavigationResponsePolicyAllow];
    [webView loadRequest:[NSURLRequest requestWithURL:firstURL]];
    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_FALSE([delegate didFailProvisionalNavigation]);
    EXPECT_FALSE([delegate didStartDownload]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_WK_STREQ(firstURL.absoluteString, [webView URL].absoluteString);

    NSURL *secondURL = [NSURL URLWithString:@"http://bundle-file/simple2.html"];
    [delegate setResponsePolicy:WKNavigationResponsePolicyDownload];
    [webView loadRequest:[NSURLRequest requestWithURL:secondURL]];
    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFailProvisionalNavigation]);
    EXPECT_TRUE([delegate didStartDownload]);
    EXPECT_WK_STREQ(firstURL.absoluteString, [webView URL].absoluteString);

    [delegate setResponsePolicy:WKNavigationResponsePolicyAllow];
    [webView loadRequest:[NSURLRequest requestWithURL:secondURL]];
    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_FALSE([delegate didFailProvisionalNavigation]);
    EXPECT_FALSE([delegate didStartDownload]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_WK_STREQ(secondURL.absoluteString, [webView URL].absoluteString);

    [delegate setResponsePolicy:WKNavigationResponsePolicyAllow];
    [webView goBack];
    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_FALSE([delegate didFailProvisionalNavigation]);
    EXPECT_FALSE([delegate didStartDownload]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_WK_STREQ(firstURL.absoluteString, [webView URL].absoluteString);

    [delegate setResponsePolicy:WKNavigationResponsePolicyDownload];
    [webView loadRequest:[NSURLRequest requestWithURL:secondURL]];
    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFailProvisionalNavigation]);
    EXPECT_TRUE([delegate didStartDownload]);
    EXPECT_WK_STREQ(firstURL.absoluteString, [webView URL].absoluteString);

    TestProtocol.additionalResponseHeaders = nil;
    [TestProtocol unregister];
}

@interface DownloadCancelingDelegate : NSObject <_WKDownloadDelegate>
@property (readonly) RetainPtr<NSData> resumeData;
@property (readonly) RetainPtr<NSString> path;
@end

@implementation DownloadCancelingDelegate

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL allowOverwrite, NSString *destination))completionHandler
{
    FileSystem::PlatformFileHandle fileHandle;
    _path = FileSystem::openTemporaryFile("TestWebKitAPI"_s, fileHandle);
    EXPECT_TRUE(fileHandle != FileSystem::invalidPlatformFileHandle);
    FileSystem::closeFile(fileHandle);
    completionHandler(YES, _path.get());
}

- (void)_download:(_WKDownload *)download didReceiveData:(uint64_t)length
{
    EXPECT_EQ(length, 5000ULL);
    [download cancel];
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    EXPECT_NOT_NULL(download.resumeData);
    _resumeData = download.resumeData;
    isDone = true;
}

@end

@interface AuthenticationChallengeHandlingDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation AuthenticationChallengeHandlingDelegate {
    bool _didReceiveAuthenticationChallenge;
}

- (void)_download:(_WKDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completionHandler
{
    _didReceiveAuthenticationChallenge = true;
    completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_TRUE(_didReceiveAuthenticationChallenge);
    isDone = true;
}

@end

TEST(_WKDownload, ResumedDownloadCanHandleAuthenticationChallenge)
{
    using namespace TestWebKitAPI;

    HTTPServer server([receivedFirstConnection = false] (Connection connection) mutable {
        if (!std::exchange(receivedFirstConnection, true)) {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                constexpr auto responseHeader =
                "HTTP/1.1 200 OK\r\n"
                "ETag: test\r\n"
                "Content-Length: 10000\r\n\r\n"_s;
                connection.send(responseHeader, [=] {
                    connection.send(Vector<uint8_t>(5000, 0));
                });
            });
            return;
        }
        connection.receiveHTTPRequest([=](Vector<char>&&) {
            constexpr auto challengeHeader =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
            "Content-Length: 0\r\n"
            "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n"_s;
            connection.send(challengeHeader, [=] {
                connection.receiveHTTPRequest([=](Vector<char>&&) {
                    constexpr auto responseHeader =
                    "HTTP/1.1 206 Partial Content\r\n"
                    "ETag: test\r\n"
                    "Content-Range: bytes 5000-9999/10000\r\n"
                    "Content-Length: 5000\r\n\r\n"_s;
                    connection.send(responseHeader, [=] {
                        connection.send(Vector<uint8_t>(5000, 1));
                    });
                });
            });
        });
    });

    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    auto websiteDataStore = [WKWebsiteDataStore defaultDataStore];

    auto delegate1 = adoptNS([[DownloadCancelingDelegate alloc] init]);
    [processPool _setDownloadDelegate:delegate1.get()];

    isDone = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]];
    [processPool _downloadURLRequest:request websiteDataStore:websiteDataStore originatingWebView:nil];

    Util::run(&isDone);

    isDone = false;
    auto delegate2 = adoptNS([[AuthenticationChallengeHandlingDelegate alloc] init]);
    [processPool _setDownloadDelegate:delegate2.get()];
    [processPool _resumeDownloadFromData:[delegate1 resumeData].get() websiteDataStore:websiteDataStore path:[delegate1 path].get() originatingWebView:nil];

    Util::run(&isDone);
}

template<size_t length>
String longString(LChar c)
{
    Vector<LChar> vector(length, c);
    return String(vector.data(), length);
}

enum class IncludeETag : bool { No, Yes };
enum class TerminateAfterFirstReply : bool { No, Yes };

static TestWebKitAPI::HTTPServer downloadTestServer(IncludeETag includeETag = IncludeETag::Yes, Function<void(TestWebKitAPI::Connection)>&& terminator = nullptr)
{
    return { [includeETag, terminator = WTFMove(terminator), connectionCount = 0](TestWebKitAPI::Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([includeETag, connection, terminator = WTFMove(terminator)] (Vector<char>&&) mutable {
                auto response = makeString(
                    "HTTP/1.1 200 OK\r\n",
                    includeETag == IncludeETag::Yes ? "ETag: test\r\n" : "",
                    "Content-Length: 10000\r\n"
                    "Content-Disposition: attachment; filename=\"example.txt\"\r\n"
                    "\r\n", longString<5000>('a')
                );
                connection.send(WTFMove(response), [connection, terminator = WTFMove(terminator)] () mutable {
                    if (terminator)
                        terminator(connection);
                });
            });
            break;
        case 2:
            connection.receiveHTTPRequest([=](Vector<char>&& request) {
                EXPECT_TRUE(strnstr(request.data(), "Range: bytes=5000-\r\n", request.size()));
                connection.send(makeString(
                    "HTTP/1.1 206 Partial Content\r\n"
                    "ETag: test\r\n"
                    "Content-Length: 5000\r\n"
                    "Content-Range: bytes 5000-9999/10000\r\n"
                    "\r\n", longString<5000>('b')
                ));
            });
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }};
}

static void checkResumedDownloadContents(NSURL *file)
{
    NSData *fileContents = [NSData dataWithContentsOfURL:file];
    EXPECT_EQ(fileContents.length, 10000u);
    EXPECT_TRUE(fileContents.bytes);
    if (fileContents.bytes && fileContents.length == 10000u) {
        for (size_t i = 0; i < 5000; i++)
            EXPECT_EQ(static_cast<const char*>(fileContents.bytes)[i], 'a');
        for (size_t i = 5000; i < 10000; i++)
            EXPECT_EQ(static_cast<const char*>(fileContents.bytes)[i], 'b');
    }
}

static TestWebKitAPI::HTTPServer simpleDownloadTestServer()
{
    return { [](TestWebKitAPI::Connection connection) {
        connection.receiveHTTPRequest([connection](Vector<char>&&) {
            connection.send(makeString(
                "HTTP/1.1 200 OK\r\n"
                "ETag: test\r\n"
                "Content-Length: 5000\r\n"
                "Content-Disposition: attachment; filename=\"example.txt\"\r\n"
                "\r\n", longString<5000>('a')
            ));
        });
    }};
}

static void checkFileContents(NSURL *file, const String& expectedContents)
{
    NSData *fileContents = [NSData dataWithContentsOfURL:file];
    EXPECT_EQ(fileContents.length, expectedContents.length());
    for (size_t i = 0; i < fileContents.length; i++)
        EXPECT_EQ(static_cast<const char*>(fileContents.bytes)[i], expectedContents[i]);
}

static NSURL *tempFileThatDoesNotExist()
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"DownloadTëst"] isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:tempDir withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *file = [tempDir URLByAppendingPathComponent:@"example.txt"];
    [[NSFileManager defaultManager] removeItemAtURL:file error:nil];
    return file;
}

static NSURL *tempPDFThatDoesNotExist()
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"DownloadTest"] isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:tempDir withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *file = [tempDir URLByAppendingPathComponent:@"example.pdf"];
    [[NSFileManager defaultManager] removeItemAtURL:file error:nil];
    return file;
}

static NSURL *tempUSDZThatDoesNotExist()
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"DownloadTest"] isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:tempDir withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *file = [tempDir URLByAppendingPathComponent:@"example.usdz"];
    [[NSFileManager defaultManager] removeItemAtURL:file error:nil];
    return file;
}

TEST(_WKDownload, Resume)
{
    using namespace TestWebKitAPI;
    auto server = downloadTestServer();

    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        completionHandler(WKNavigationResponsePolicyDownload);
    };

    enum class Callback : uint8_t { Start, WriteData, DecideDestination, CreateDestination, Cancel, Finish };
    __block Vector<Callback> callbacks;
    __block bool didCancel = false;
    __block bool didFinish = false;
    __block bool receivedData = false;
    __block RetainPtr<_WKDownload> download;
    __block RetainPtr<NSData> resumeData;

    auto downloadDelegate = adoptNS([TestLegacyDownloadDelegate new]);
    downloadDelegate.get().decideDestinationWithSuggestedFilename = ^(_WKDownload *, NSString *suggestedFilename, void (^completionHandler)(BOOL, NSString *)) {
        callbacks.append(Callback::DecideDestination);
        EXPECT_WK_STREQ("example.txt", suggestedFilename);
        completionHandler(YES, expectedDownloadFile.path);
    };
    downloadDelegate.get().didWriteData = ^(_WKDownload *download, uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite) {
        callbacks.append(Callback::WriteData);
        EXPECT_EQ(bytesWritten, 5000u);
        EXPECT_EQ(totalBytesWritten, didCancel ? 10000u : 5000u);
        EXPECT_EQ(totalBytesExpectedToWrite, 10000u);
        receivedData = true;
    };
    downloadDelegate.get().downloadDidStart = ^(_WKDownload *downloadFromDelegate) {
        callbacks.append(Callback::Start);
        download = downloadFromDelegate;
    };
    downloadDelegate.get().didCreateDestination = ^(_WKDownload *, NSString *destination) {
        callbacks.append(Callback::CreateDestination);
        EXPECT_WK_STREQ(destination, expectedDownloadFile.path);
    };
    downloadDelegate.get().downloadDidCancel = ^(_WKDownload *download) {
        callbacks.append(Callback::Cancel);
        resumeData = download.resumeData;
        didCancel = true;
    };
    downloadDelegate.get().downloadDidFinish = ^(_WKDownload *) {
        callbacks.append(Callback::Finish);
        didFinish = true;
    };

    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView configuration].processPool._downloadDelegate = downloadDelegate.get();
    [webView loadRequest:server.request()];
    Util::run(&receivedData);
    [download cancel];
    Util::run(&didCancel);

    [[webView configuration].processPool _resumeDownloadFromData:resumeData.get() websiteDataStore:[WKWebsiteDataStore defaultDataStore] path:expectedDownloadFile.path originatingWebView:webView.get()];
    Util::run(&didFinish);
    
    EXPECT_EQ(callbacks.size(), 7u);
    EXPECT_EQ(callbacks[0], Callback::Start);
    EXPECT_EQ(callbacks[1], Callback::DecideDestination);
    EXPECT_EQ(callbacks[2], Callback::CreateDestination);
    EXPECT_EQ(callbacks[3], Callback::WriteData);
    EXPECT_EQ(callbacks[4], Callback::Cancel);
    EXPECT_EQ(callbacks[5], Callback::WriteData);
    EXPECT_EQ(callbacks[6], Callback::Finish);

    // Give CFNetwork enough time to unlink the downloaded file if it would have.
    // This makes failures like https://bugs.webkit.org/show_bug.cgi?id=211786 more reliable.
    usleep(10000);
    Util::spinRunLoop(10);
    usleep(10000);

    checkResumedDownloadContents(expectedDownloadFile);
}

@interface DownloadTestSchemeDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DownloadTestSchemeDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    if ([navigationResponse.response.URL.absoluteString hasSuffix:@"/download"])
        decisionHandler(WKNavigationResponsePolicyDownload);
    else
        decisionHandler(WKNavigationResponsePolicyAllow);
}
@end

@interface DownloadSecurityOriginDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation DownloadSecurityOriginDelegate {
@public
    uint16_t _serverPort;
    WKWebView *_webView;
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_TRUE([download.originatingFrame.securityOrigin.protocol isEqualToString:@"http"]);
    EXPECT_TRUE([download.originatingFrame.securityOrigin.host isEqualToString:@"127.0.0.1"]);
    EXPECT_EQ(download.originatingFrame.securityOrigin.port, _serverPort);
    EXPECT_FALSE(download.originatingFrame.mainFrame);
    EXPECT_EQ(download.originatingFrame.webView, _webView);
    isDone = true;
}

@end

static constexpr auto documentText = R"DOCDOCDOC(
<script>
function loaded()
{
    document.getElementById("thelink").click();
}
</script>
<body onload="loaded();">
<a id="thelink" href="download">Click me</a>
</body>
)DOCDOCDOC"_s;

TEST(_WKDownload, SubframeSecurityOrigin)
{
    auto navigationDelegate = adoptNS([[DownloadTestSchemeDelegate alloc] init]);
    auto downloadDelegate = adoptNS([[DownloadSecurityOriginDelegate alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    TestWebKitAPI::HTTPServer server({
        { "/page"_s, { documentText } },
        { "/download"_s, { documentText } },
    });
    downloadDelegate->_serverPort = server.port();
    downloadDelegate->_webView = webView.get();

    isDone = false;
    [webView loadHTMLString:[NSString stringWithFormat:@"<body><iframe src='http://127.0.0.1:%d/page'></iframe></body>", server.port()] baseURL:nil];
    TestWebKitAPI::Util::run(&isDone);
}

@interface DownloadObserver : NSObject
@property (nonatomic, copy) void (^progressChangeCallback)(int64_t, int64_t);
@end

@implementation DownloadObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    if (self.progressChangeCallback) {
        NSProgress *progress = (NSProgress *)object;
        self.progressChangeCallback(progress.completedUnitCount, progress.totalUnitCount);
    }
}

@end

namespace TestWebKitAPI {

static void checkCallbackRecord(TestDownloadDelegate *delegate, Vector<DownloadCallback> expectedCallbacks)
{
    auto actualCallbacks = delegate.takeCallbackRecord;
    EXPECT_EQ(actualCallbacks.size(), expectedCallbacks.size());
    for (size_t i = 0; i < std::min(actualCallbacks.size(), expectedCallbacks.size()); i++)
        EXPECT_EQ(actualCallbacks[i], expectedCallbacks[i]);
}

#if PLATFORM(MAC)
static void expectHardQuarantine(NSURL *url, bool expected)
{
    auto file = std::unique_ptr<_qtn_file, QuarantineFileDeleter>(qtn_file_alloc());
    if (!file) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto error = qtn_file_init_with_path(file.get(), url.fileSystemRepresentation);
    if (error) {
        ASSERT_NOT_REACHED();
        return;
    }

    uint32_t flags = qtn_file_get_flags(file.get());
    EXPECT_EQ(!!(flags & QTN_FLAG_HARD), expected);
}
#else
static void expectHardQuarantine(NSURL *, bool) { }
#endif

TEST(WKDownload, FinishSuccessfully)
{
    auto server = simpleDownloadTestServer();
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *download, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            EXPECT_NULL(download.progress.fileURL);
            completionHandler(expectedDownloadFile);
            EXPECT_NOT_NULL(download.progress.fileURL);
            EXPECT_WK_STREQ(download.progress.fileURL.absoluteString, expectedDownloadFile.absoluteString);
        };
    };
    [webView loadRequest:server.request()];
    [delegate waitForDownloadDidFinish];

    checkFileContents(expectedDownloadFile, longString<5000>('a'));
    expectHardQuarantine(expectedDownloadFile, false);

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

static void resumeAndFinishDownload(NSData *resumeData, NSURL *destination)
{
    __block RetainPtr<WKDownload> retainedDownload;
    @autoreleasepool {
        checkFileContents(destination, longString<5000>('a'));

        auto delegate = adoptNS([TestDownloadDelegate new]);
        auto webView = adoptNS([WKWebView new]);

        [webView resumeDownloadFromResumeData:resumeData completionHandler:^(WKDownload *download) {
            retainedDownload = download;
            EXPECT_NULL(download.delegate);
        }];
        while (!retainedDownload)
            Util::spinRunLoop();
        
        __block bool downloadedSecond5k = false;
        EXPECT_NULL(retainedDownload.get().delegate);
        retainedDownload.get().delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            ASSERT_NOT_REACHED();
        };

        auto observer = adoptNS([DownloadObserver new]);
        observer.get().progressChangeCallback = ^(int64_t bytesWritten, int64_t totalByteCount) {
            if (bytesWritten == 10000) {
                EXPECT_EQ(totalByteCount, 10000);
                downloadedSecond5k = true;
            }
        };
        [retainedDownload.get().progress addObserver:observer.get() forKeyPath:@"completedUnitCount" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:nil];

        __block bool didFinish = false;
        delegate.get().downloadDidFinish = ^(WKDownload *download) {
            EXPECT_EQ(retainedDownload.get(), download);
            EXPECT_TRUE(downloadedSecond5k);
            didFinish = true;
        };

        Util::run(&didFinish);

        [retainedDownload.get().progress removeObserver:observer.get() forKeyPath:@"completedUnitCount" context:nil];

        checkResumedDownloadContents(destination);
        checkCallbackRecord(delegate.get(), {
            DownloadCallback::DidFinish
        });
        EXPECT_EQ(retainedDownload.get().webView, webView.get());
        EXPECT_NOT_NULL(retainedDownload.get().webView);
    }
    EXPECT_NOT_NULL(retainedDownload.get());
    EXPECT_NULL(retainedDownload.get().webView);
}

static void waitForFirst5k(RetainPtr<WKDownload>& download)
{
    __block bool downloadedFirst5k = false;
    
    auto observer = adoptNS([DownloadObserver new]);
    observer.get().progressChangeCallback = ^(int64_t bytesWritten, int64_t totalByteCount) {
        if (bytesWritten == 5000) {
            EXPECT_EQ(totalByteCount, 10000);
            downloadedFirst5k = true;
        }
    };
    while (!download)
        Util::spinRunLoop();
    [download.get().progress addObserver:observer.get() forKeyPath:@"completedUnitCount" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:nil];
    Util::run(&downloadedFirst5k);
    [download.get().progress removeObserver:observer.get() forKeyPath:@"completedUnitCount" context:nil];
}

TEST(WKDownload, CancelAndResume)
{
    auto server = downloadTestServer();
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block RetainPtr<WKDownload> retainedDownload;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
    };

    [webView loadRequest:server.request()];
    waitForFirst5k(retainedDownload);
    expectHardQuarantine(expectedDownloadFile, true);

    __block RetainPtr<NSData> retainedResumeData;
    [retainedDownload cancel:^(NSData *resumeData) {
        retainedResumeData = resumeData;
    }];

    while (!retainedResumeData)
        Util::spinRunLoop();
    resumeAndFinishDownload(retainedResumeData.get(), expectedDownloadFile);
    expectHardQuarantine(expectedDownloadFile, false);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
    });
}

TEST(WKDownload, FailAndResume)
{
    auto delegate = adoptNS([TestDownloadDelegate new]);
    RetainPtr<WKDownload> retainedDownload;
    auto server = downloadTestServer(IncludeETag::Yes, [&] (TestWebKitAPI::Connection connection) {
        waitForFirst5k(retainedDownload);
        connection.terminate();
    });
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    RetainPtr<NSData> retainedResumeData;
    delegate.get().navigationResponseDidBecomeDownload = makeBlockPtr([&](WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
        delegate.get().didFailWithError = ^(WKDownload *download, NSError *error, NSData *resumeData) {
            EXPECT_EQ(download, retainedDownload.get());
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
            retainedResumeData = resumeData;
        };
    }).get();

    [webView loadRequest:server.request()];

    while (!retainedResumeData)
        Util::spinRunLoop();
    resumeAndFinishDownload(retainedResumeData.get(), expectedDownloadFile);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError,
    });
}

TEST(WKDownload, CancelNoResumeData)
{
    auto server = downloadTestServer(IncludeETag::No);
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block RetainPtr<WKDownload> retainedDownload;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
    };
    [webView loadRequest:server.request()];
    waitForFirst5k(retainedDownload);

    __block bool done = false;
    [retainedDownload cancel:^(NSData *resumeData) {
        EXPECT_NULL(resumeData);
        done = true;
    }];
    Util::run(&done);
    checkFileContents(expectedDownloadFile, longString<5000>('a'));
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
    });
}

TEST(WKDownload, FailNoResumeData)
{
    auto delegate = adoptNS([TestDownloadDelegate new]);
    RetainPtr<WKDownload> retainedDownload;
    auto server = downloadTestServer(IncludeETag::No, [&] (TestWebKitAPI::Connection connection) {
        waitForFirst5k(retainedDownload);
        connection.terminate();
    });
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    bool done = false;
    delegate.get().navigationResponseDidBecomeDownload = makeBlockPtr([&](WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
        delegate.get().didFailWithError = makeBlockPtr([&](WKDownload *, NSError *error, NSData *resumeData) {
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
            EXPECT_NULL(resumeData);
            done = true;
        }).get();
    }).get();
    [webView loadRequest:server.request()];
    Util::run(&done);
    checkFileContents(expectedDownloadFile, longString<5000>('a'));
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError
    });
}

// FIXME: Make this work in CFNetwork.
#if HAVE(CFNET_RESPONSE_CALLBACK_WITH_NO_CONTENT)

TEST(WKDownload, ResumeAfterZeroBytesReceived)
{
    std::optional<TestWebKitAPI::Connection> serverConnection;
    HTTPServer server([connectionCount = 0, &serverConnection](TestWebKitAPI::Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            serverConnection = connection;
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "ETag: test\r\n"
                    "Content-Disposition: attachment; filename=\"example.txt\"\r\n"
                    "\r\n"
                );
            });
            break;
        case 2:
            connection.receiveHTTPRequest([=](Vector<char>&& request) {
                EXPECT_TRUE(strnstr(request.data(), "Range: bytes=5000-\r\n", request.size()));
                connection.send(makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "ETag: test\r\n"
                    "Content-Length: 100\r\n"
                    "\r\n", longString<100>('x')
                ));
            });
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    });

    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    webView.navigationDelegate = delegate.get();

    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
    };
    delegate.get().decideDestinationUsingResponse = [&](WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
        completionHandler(expectedDownloadFile);
        serverConnection->terminate();
    };
    __block RetainPtr<NSData> retainedResumeData;
    delegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *resumeData) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
        retainedResumeData = resumeData;
    };
    __block bool downloadFinished = false;
    delegate.get().downloadDidFinish = ^(WKDownload *) {
        downloadFinished = true;
    };

    [webView loadRequest:server.request()];
    while (!retainedResumeData)
        Util::spinRunLoop();
    
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:expectedDownloadFile.path]);
    EXPECT_FALSE(downloadFinished);
    [webView resumeDownloadFromResumeData:retainedResumeData.get() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
    }];
    Util::run(&downloadFinished);
    checkFileContents(expectedDownloadFile, longString<100>('x'));

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError,
        DownloadCallback::DidFinish
    });
}

#endif

void testResumeAfterMutatingDisk(NSURLRequest *serverRequest, NSURL *expectedDownloadFile, void(^mutateFile)(void))
{
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block RetainPtr<WKDownload> retainedDownload;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *download, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
    };

    [webView loadRequest:serverRequest];
    waitForFirst5k(retainedDownload);

    __block RetainPtr<NSData> retainedResumeData;
    [retainedDownload cancel:^(NSData *resumeData) {
        retainedResumeData = resumeData;
    }];

    while (!retainedResumeData)
        Util::spinRunLoop();

    checkFileContents(expectedDownloadFile, longString<5000>('a'));

    mutateFile();

    __block bool didFinish = false;
    [webView resumeDownloadFromResumeData:retainedResumeData.get() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().downloadDidFinish = ^(WKDownload *) {
            didFinish = true;
        };
    }];
    Util::run(&didFinish);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

TEST(WKDownload, ResumeWithoutInitialDataOnDisk)
{
    HTTPServer server([connectionCount = 0](TestWebKitAPI::Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                connection.send(makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "ETag: test\r\n"
                    "Content-Length: 10000\r\n"
                    "Content-Disposition: attachment; filename=\"example.txt\"\r\n"
                    "\r\n",
                    longString<5000>('a')
                ));
            });
            break;
        case 2:
            connection.receiveHTTPRequest([=](Vector<char>&& request) {
                EXPECT_FALSE(strnstr(request.data(), "Range", request.size()));
                connection.send(makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "ETag: test\r\n"
                    "Content-Length: 10000\r\n"
                    "\r\n",
                    longString<10000>('x')
                ));
            });
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    });
    
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    testResumeAfterMutatingDisk(server.request(), expectedDownloadFile, ^{
        NSError *error = nil;
        [[NSFileManager defaultManager] removeItemAtURL:expectedDownloadFile error:&error];
        EXPECT_NULL(error);
    });

    checkFileContents(expectedDownloadFile, longString<10000>('x'));
}

TEST(WKDownload, ResumeWithExtraInitialDataOnDisk)
{
    HTTPServer server([connectionCount = 0](TestWebKitAPI::Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                connection.send(makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "ETag: test\r\n"
                    "Content-Length: 10000\r\n"
                    "Content-Disposition: attachment; filename=\"example.txt\"\r\n"
                    "\r\n",
                    longString<5000>('a')
                ));
            });
            break;
        case 2:
            connection.receiveHTTPRequest([=](Vector<char>&& request) {
                EXPECT_TRUE(strnstr(request.data(), "Range: bytes=5000-\r\n", request.size()));
                connection.send(makeString(
                    "HTTP/1.1 206 Partial Content\r\n"
                    "ETag: test\r\n"
                    "Content-Range: bytes 5000-9999/10000\r\n"
                    "Content-Length: 5000\r\n"
                    "\r\n",
                    longString<10000>('d')
                ));
            });
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    });
    
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    testResumeAfterMutatingDisk(server.request(), expectedDownloadFile, ^{
        NSError *error = nil;
        [[NSFileManager defaultManager] removeItemAtURL:expectedDownloadFile error:&error];
        EXPECT_NULL(error);
        EXPECT_TRUE([[(NSString *)makeString(longString<3000>('b'), longString<3000>('c')) dataUsingEncoding:NSUTF8StringEncoding] writeToURL:expectedDownloadFile atomically:YES]);
    });

    checkFileContents(expectedDownloadFile, makeString(longString<3000>('b'), longString<3000>('c'), longString<5000>('d')));
}

TEST(WKDownload, ResumeWithInvalidResumeData)
{
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    EXPECT_TRUE([[@"initial data on disk" dataUsingEncoding:NSUTF8StringEncoding] writeToURL:expectedDownloadFile atomically:YES]);
    auto webView = adoptNS([WKWebView new]);
    bool caughtException = false;
    @try {
        [webView resumeDownloadFromResumeData:[@"invalid resume data" dataUsingEncoding:NSUTF8StringEncoding] completionHandler:^(WKDownload *download) {
            ASSERT_NOT_REACHED();
        }];
    } @catch (NSException *e) {
        EXPECT_WK_STREQ(e.name, NSInvalidArgumentException);
        caughtException = true;
    }
    EXPECT_TRUE(caughtException);
}

TEST(WKDownload, ResumeCantReconnect)
{
    auto server = downloadTestServer();
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block RetainPtr<WKDownload> retainedDownload;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
    };
    [webView loadRequest:server.request()];
    waitForFirst5k(retainedDownload);

    checkFileContents(expectedDownloadFile, longString<5000>('a'));

    __block RetainPtr<NSData> retainedResumeData;
    [retainedDownload cancel:^(NSData *resumeData) {
        retainedResumeData = resumeData;
    }];
    while (!retainedResumeData)
        Util::spinRunLoop();

    server.cancel();
    
    retainedDownload = nil;
    [webView resumeDownloadFromResumeData:retainedResumeData.get() completionHandler:^(WKDownload *download) {
        retainedDownload = download;
        EXPECT_NULL(download.delegate);
    }];
    while (!retainedDownload)
        Util::spinRunLoop();

    retainedDownload.get().delegate = delegate.get();
    __block bool done = false;
    delegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *resumeData) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorCannotConnectToHost);
        EXPECT_NOT_NULL(resumeData);
        done = true;
    };
    Util::run(&done);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError
    });
}

TEST(WKDownload, UnknownContentLength)
{
    HTTPServer server([](Connection connection) {
        connection.receiveHTTPRequest([=](Vector<char>&&) {
            connection.send(makeString("HTTP/1.1 200 OK\r\n\r\n", longString<5000>('a')));
        });
    });
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool done = false;
    auto observer = adoptNS([DownloadObserver new]);
    observer.get().progressChangeCallback = ^(int64_t bytesWritten, int64_t totalByteCount) {
        EXPECT_EQ(totalByteCount, -1);
        if (bytesWritten == 5000)
            done = true;
    };

    __block RetainPtr<WKDownload> retainedDownload;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        retainedDownload = download;
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
        [download.progress addObserver:observer.get() forKeyPath:@"completedUnitCount" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:nil];
    };
    [webView loadRequest:server.request()];
    Util::run(&done);
    [retainedDownload.get().progress removeObserver:observer.get() forKeyPath:@"completedUnitCount" context:nil];
    checkFileContents(expectedDownloadFile, longString<5000>('a'));

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
    });
}

TEST(WKDownload, InvalidArguments)
{
    auto webView = adoptNS([WKWebView new]);
    __block bool caughtException = false;
    auto server = downloadTestServer();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    [webView startDownloadUsingRequest:server.request() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            @try {
                completionHandler([NSURL URLWithString:@"https://webkit.org/"]);
            } @catch (NSException *e) {
                EXPECT_WK_STREQ(e.name, NSInvalidArgumentException);
                caughtException = true;
            }
        };
    }];
    Util::run(&caughtException);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::DecideDestination,
    });
}

static HTTPServer redirectServer()
{
    return {{
        { "/"_s, { 301, {{ "Location"_s, "/redirectTarget"_s }, { "Custom-Name"_s, "Custom-Value"_s }} } },
        { "/redirectTarget"_s, { "hi"_s } },
    }};
}

TEST(WKDownload, RedirectAllow)
{
    auto server = redirectServer();
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    auto serverRequest = adoptNS([server.request() mutableCopy]);
    [serverRequest setHTTPBody:[@"body" dataUsingEncoding:NSUTF8StringEncoding]];
    
    __block bool finishedDownload = false;
    [webView startDownloadUsingRequest:serverRequest.get() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().willPerformHTTPRedirection = ^(WKDownload *, NSHTTPURLResponse *response, NSURLRequest *request, void (^completionHandler)(WKDownloadRedirectPolicy)) {
            EXPECT_NULL(request.HTTPBody); // FIXME: We probably want to make this non-null.
            EXPECT_WK_STREQ(response.URL.absoluteString, [serverRequest URL].absoluteString);
            EXPECT_EQ(response.statusCode, 301);
            EXPECT_WK_STREQ(response.allHeaderFields[@"Custom-Name"], "Custom-Value");
            EXPECT_WK_STREQ(request.URL.absoluteString, [[serverRequest URL] URLByAppendingPathComponent:@"redirectTarget"].absoluteString);
            completionHandler(WKDownloadRedirectPolicyAllow);
        };
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *download, NSURLResponse *response, NSString *suggestedFilename, void (^completionHandler)(NSURL *)) {
            EXPECT_WK_STREQ(suggestedFilename, "redirectTarget.txt");
            EXPECT_WK_STREQ(response.URL.path, "/redirectTarget");
            EXPECT_WK_STREQ(download.originalRequest.URL.path, "/");
            EXPECT_WK_STREQ(download.originalRequest.URL.absoluteString, [serverRequest URL].absoluteString);
            completionHandler(expectedDownloadFile);
        };
        delegate.get().downloadDidFinish = ^(WKDownload *) {
            finishedDownload = true;
        };
    }];
    Util::run(&finishedDownload);

    checkFileContents(expectedDownloadFile, "hi"_s);
    
    EXPECT_EQ(server.totalRequests(), 2u);

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::WillRedirect,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

TEST(WKDownload, RedirectCancel)
{
    auto server = redirectServer();
    NSURLRequest *serverRequest = server.request();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool cancelled = false;
    [webView startDownloadUsingRequest:server.request() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().willPerformHTTPRedirection = ^(WKDownload *, NSHTTPURLResponse *response, NSURLRequest *request, void (^completionHandler)(WKDownloadRedirectPolicy)) {
            EXPECT_WK_STREQ(response.URL.absoluteString, serverRequest.URL.absoluteString);
            EXPECT_EQ(response.statusCode, 301);
            EXPECT_WK_STREQ(response.allHeaderFields[@"Custom-Name"], "Custom-Value");
            EXPECT_WK_STREQ(request.URL.absoluteString, [serverRequest.URL URLByAppendingPathComponent:@"redirectTarget"].absoluteString);
            completionHandler(WKDownloadRedirectPolicyCancel);
        };
        delegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *resumeData) {
            EXPECT_NULL(resumeData);
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorCancelled);
            cancelled = true;
        };
    }];
    Util::run(&cancelled);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::WillRedirect,
        DownloadCallback::DidFailWithError
    });
    EXPECT_EQ(server.totalRequests(), 1u);
}

TEST(WKDownload, DownloadRequestFailure)
{
    HTTPServer server({ });
    NSURLRequest *serverRequest = server.request();
    server.cancel();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool failed = false;
    [webView startDownloadUsingRequest:serverRequest completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().didFailWithError = ^(WKDownload *download, NSError *error, NSData *resumeData) {
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorCannotConnectToHost);
            failed = true;
        };
    }];
    Util::run(&failed);

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::DidFailWithError,
    });

    failed = false;
    [webView startDownloadUsingRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"ftp:///"]] completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().didFailWithError = ^(WKDownload *download, NSError *error, NSData *resumeData) {
            EXPECT_WK_STREQ(error.domain, WebKitErrorDomain);
            EXPECT_EQ(error.code, 101);
            failed = true;
        };
    }];
    Util::run(&failed);

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::DidFailWithError,
    });
}

TEST(WKDownload, DownloadRequest404)
{
    HTTPServer server({
        { "/"_s, { 404, { }, "http body"_s } }
    });
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool didFinish = false;
    [webView startDownloadUsingRequest:server.request() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
        delegate.get().downloadDidFinish = ^(WKDownload *download) {
            didFinish = true;
        };
    }];
    Util::run(&didFinish);

    checkFileContents(expectedDownloadFile, "http body"_s);

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish,
    });
}

TEST(WKDownload, NetworkProcessCrash)
{
    auto server = downloadTestServer();
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block RetainPtr<WKDownload> retainedDownload;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        retainedDownload = download;
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
    };
    [webView loadRequest:server.request()];
    waitForFirst5k(retainedDownload);
    
    __block bool terminated = false;
    delegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
        terminated = true;
    };
    [[webView configuration].websiteDataStore _terminateNetworkProcess];
    Util::run(&terminated);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError
    });
}

TEST(WKDownload, SuggestedFilenameFromHost)
{
    HTTPServer server({
        { "/"_s, { "download content"_s } }
    });
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *response, NSString *suggestedFilename, void (^completionHandler)(NSURL *)) {
            EXPECT_WK_STREQ(suggestedFilename, "127.0.0.1.txt");
            EXPECT_WK_STREQ(response.suggestedFilename, "127.0.0.1.txt");
            completionHandler(expectedDownloadFile);
        };
    };
    [webView loadRequest:server.request()];
    [delegate waitForDownloadDidFinish];

    checkFileContents(expectedDownloadFile, "download content"_s);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

TEST(WKDownload, RequestHTTPBody)
{
    auto server = downloadTestServer();
    auto webView = adoptNS([WKWebView new]);
    __block bool done = false;
    auto request = adoptNS([server.request() mutableCopy]);
    [request setHTTPBody:[@"body" dataUsingEncoding:NSUTF8StringEncoding]];
    [webView startDownloadUsingRequest:request.get() completionHandler:^(WKDownload *download) {
        EXPECT_NULL(download.originalRequest.HTTPBody); // FIXME: We probably want to make this non-null.
        done = true;
    }];
    Util::run(&done);
}

TEST(WKDownload, PathMustExist)
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"DownloadTest"] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:tempDir error:nil];
    NSURL *expectedDownloadFile = [tempDir URLByAppendingPathComponent:@"example.txt"];

    auto server = downloadTestServer();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool failed = false;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(expectedDownloadFile);
        };
        delegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *) {
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorCannotCreateFile);
            failed = true;
        };
    };
    [webView loadRequest:server.request()];
    Util::run(&failed);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError
    });
}

TEST(WKDownload, FileMustNotExist)
{
    auto server = downloadTestServer();
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    __block auto retainedDelegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:retainedDelegate.get()];

    EXPECT_TRUE([[@"initial data on disk" dataUsingEncoding:NSUTF8StringEncoding] writeToURL:expectedDownloadFile atomically:YES]);

    __block bool failed = false;
    retainedDelegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = retainedDelegate.get();
        retainedDelegate.get().decideDestinationUsingResponse = ^(WKDownload *download, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {

            retainedDelegate = adoptNS([TestDownloadDelegate new]);
            download.delegate = retainedDelegate.get();

            retainedDelegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *) {
                EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
                EXPECT_EQ(error.code, NSURLErrorCannotCreateFile);
                failed = true;
            };

            completionHandler(expectedDownloadFile);
        };
    };
    [webView loadRequest:server.request()];
    Util::run(&failed);

    checkCallbackRecord(retainedDelegate.get(), {
        DownloadCallback::DidFailWithError,
    });
}

TEST(WKDownload, DestinationNullString)
{
    auto server = downloadTestServer();
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool failed = false;
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            completionHandler(nil);
        };
        delegate.get().didFailWithError = ^(WKDownload *, NSError *error, NSData *) {
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorCancelled);
            failed = true;
        };
    };
    [webView loadRequest:server.request()];
    Util::run(&failed);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFailWithError
    });
}

TEST(WKDownload, ChallengeSuccess)
{
    HTTPServer server({{ "/"_s, { "download content"_s }}}, HTTPServer::Protocol::Https);
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    __block bool finished = false;
    delegate.get().downloadDidFinish = ^(WKDownload *download) {
        finished = true;
    };
    __block bool receivedChallenge = false;
    delegate.get().didReceiveAuthenticationChallenge = ^(WKDownload *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        receivedChallenge = true;
    };
    delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
        completionHandler(expectedDownloadFile);
    };
    [webView startDownloadUsingRequest:server.request() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
    }];
    Util::run(&finished);
    EXPECT_TRUE(receivedChallenge);
    checkFileContents(expectedDownloadFile, "download content"_s);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::AuthenticationChallenge,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

TEST(WKDownload, ChallengeFailure)
{
    HTTPServer server({ }, HTTPServer::Protocol::Https);
    auto delegate = adoptNS([TestDownloadDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    __block bool failed = false;
    delegate.get().didFailWithError = ^(WKDownload *download, NSError *error, NSData *resumeData) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorCancelled);
        failed = true;
    };
    __block bool receivedChallenge = false;
    delegate.get().didReceiveAuthenticationChallenge = ^(WKDownload *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        receivedChallenge = true;
    };
    [webView startDownloadUsingRequest:server.request() completionHandler:^(WKDownload *download) {
        download.delegate = delegate.get();
    }];
    Util::run(&failed);
    EXPECT_TRUE(receivedChallenge);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::AuthenticationChallenge,
        DownloadCallback::DidFailWithError
    });
}

void blobTest(bool downloadFromNavigationAction, std::initializer_list<DownloadCallback> expectedCallbacks)
{
    NSString *html = @"<script>"
    "function downloadBlob() {"
    "    var a = document.createElement('a');"
    "    var b = new Blob([1,2,3]);"
    "    a.href = URL.createObjectURL(b);"
    "    a.download = 'downloadFilename';"
    "    a.click();"
    "}"
    "</script><body onload='downloadBlob()'></body>";
    NSURL *expectedDownloadFile = tempFileThatDoesNotExist();
    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:html baseURL:nil];
    auto delegate = adoptNS([TestDownloadDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.absoluteString isEqualToString:@"about:blank"])
            EXPECT_FALSE(action.shouldPerformDownload);
        else {
            EXPECT_WK_STREQ(action.request.URL.scheme, "blob");
            EXPECT_TRUE(action.shouldPerformDownload);
            if (downloadFromNavigationAction) {
                completionHandler(WKNavigationActionPolicyDownload);
                return;
            }
        }
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().navigationActionDidBecomeDownload = ^(WKWebView *, WKNavigationAction *action, WKDownload *download) {
        EXPECT_WK_STREQ(action.request.URL.scheme, "blob");
        EXPECT_WK_STREQ(download.originalRequest.URL.scheme, "blob");
        download.delegate = delegate.get();
    };
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *response, WKDownload *download) {
        EXPECT_WK_STREQ(response.response.URL.scheme, "blob");
        EXPECT_WK_STREQ(download.originalRequest.URL.scheme, "blob");
        download.delegate = delegate.get();
    };
    delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *response, NSString *suggestedFilename, void (^completionHandler)(NSURL *)) {
        EXPECT_WK_STREQ(response.URL.scheme, "blob");
        EXPECT_WK_STREQ(suggestedFilename, "downloadFilename");
        completionHandler(expectedDownloadFile);
    };
    __block bool done = false;
    delegate.get().downloadDidFinish = ^(WKDownload *) {
        done = true;
    };
    Util::run(&done);

    checkFileContents(expectedDownloadFile, "123"_s);
    checkCallbackRecord(delegate.get(), expectedCallbacks);
}

TEST(WKDownload, BlobResponse)
{
    blobTest(false, {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });

    blobTest(true, {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationActionBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

TEST(WKDownload, BlobResponseNoFilename)
{
    NSString *html = @"<script>"
    "function downloadBlob() {"
    "    var a = document.createElement('a');"
    "    var b = new Blob([1,2,3]);"
    "    a.href = URL.createObjectURL(b);"
    "    a.click();"
    "}"
    "</script><body onload='downloadBlob()'></body>";
    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    auto delegate = adoptNS([TestDownloadDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        EXPECT_FALSE(action.shouldPerformDownload);
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *response, WKDownload *download) {
        EXPECT_WK_STREQ(response.response.URL.scheme, "blob");
        EXPECT_WK_STREQ(response._frame.securityOrigin.host, "webkit.org");
        download.delegate = delegate.get();
    };
    __block bool done = false;
    delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *response, NSString *suggestedFilename, void (^completionHandler)(NSURL *)) {
        EXPECT_WK_STREQ(response.URL.scheme, "blob");
        EXPECT_WK_STREQ(suggestedFilename, "Unknown");
        completionHandler(nil);
        done = true;
    };
    Util::run(&done);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination
    });
}

TEST(WKDownload, SubframeOriginator)
{
    constexpr auto grandchildFrameHTML = "<script>"
    "function downloadBlob() {"
    "    var a = document.createElement('a');"
    "    var b = new Blob([1,2,3]);"
    "    a.href = URL.createObjectURL(b);"
    "    a.click();"
    "}"
    "</script><body onload='downloadBlob()'></body>"_s;
    HTTPServer grandchildFrameServer({
        { "/"_s, { grandchildFrameHTML } }
    });
    HTTPServer childFrameServer({
        { "/"_s, { [NSString stringWithFormat:@"<iframe src='http://127.0.0.1:%d/'></iframe>", grandchildFrameServer.port()] } }
    });
    NSURLRequest *grandchildFrameServerRequest = grandchildFrameServer.request();
    NSURLRequest *childFrameServerRequest = childFrameServer.request();
    uint16_t grandchildFrameServerPort = grandchildFrameServer.port();

    NSString *mainHTML = [NSString stringWithFormat:@"<iframe src='http://127.0.0.1:%d/'></iframe>", childFrameServer.port()];

    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:mainHTML baseURL:[NSURL URLWithString:@"http://webkit.org/"]];

    auto delegate = adoptNS([TestDownloadDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    delegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *response, void (^completionHandler)(WKNavigationResponsePolicy)) {
        if ([response.response.URL.absoluteString isEqualToString:@"http://webkit.org/"]
            || [response.response.URL.absoluteString isEqualToString:childFrameServerRequest.URL.absoluteString]
            || [response.response.URL.absoluteString isEqualToString:grandchildFrameServerRequest.URL.absoluteString])
            completionHandler(WKNavigationResponsePolicyAllow);
        else
            completionHandler(WKNavigationResponsePolicyDownload);
    };
    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *response, WKDownload *download) {
        EXPECT_WK_STREQ(response.response.URL.scheme, "blob");
        EXPECT_WK_STREQ(response._frame.securityOrigin.host, "127.0.0.1");
        EXPECT_EQ(response._frame.securityOrigin.port, grandchildFrameServerPort);
        download.delegate = delegate.get();
    };
    __block bool done = false;
    delegate.get().decideDestinationUsingResponse = ^(WKDownload *, NSURLResponse *response, NSString *suggestedFilename, void (^completionHandler)(NSURL *)) {
        EXPECT_WK_STREQ(response.URL.scheme, "blob");
        EXPECT_WK_STREQ(suggestedFilename, "Unknown");
        completionHandler(nil);
        done = true;
    };
    Util::run(&done);
    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination
    });
}


static TestWebKitAPI::HTTPServer simplePDFTestServer()
{
    return { [](TestWebKitAPI::Connection connection) {
        connection.receiveHTTPRequest([connection](Vector<char>&&) {
            connection.send(makeString(
                "HTTP/1.1 200 OK\r\n"
                "content-type: application/pdf\r\n"
                "Content-Length: 5000\r\n"
                "\r\n", longString<5000>('a')
            ));
        });
    } };
}

TEST(WKDownload, LockdownModePDF)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestDownloadDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    auto server = simplePDFTestServer();
    NSURL *expectedDownloadFile = tempPDFThatDoesNotExist();

    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *download, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            EXPECT_NULL(download.progress.fileURL);
            completionHandler(expectedDownloadFile);
            EXPECT_NOT_NULL(download.progress.fileURL);
            EXPECT_WK_STREQ(download.progress.fileURL.absoluteString, expectedDownloadFile.absoluteString);
        };
    };

    [webView loadRequest:server.request()];
    [delegate waitForDownloadDidFinish];

    checkFileContents(expectedDownloadFile, longString<5000>('a'));

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

static TestWebKitAPI::HTTPServer simpleUSDZTestServer()
{
    return { [](TestWebKitAPI::Connection connection) {
        connection.receiveHTTPRequest([connection](Vector<char>&&) {
            connection.send(makeString(
                "HTTP/1.1 200 OK\r\n"
                "content-type: model/vnd.usdz+zip\r\n"
                "Content-Length: 5000\r\n"
                "\r\n", longString<5000>('a')
            ));
        });
    } };
}

TEST(WKDownload, LockdownModeUSDZ)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestDownloadDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    auto server = simpleUSDZTestServer();
    NSURL *expectedDownloadFile = tempUSDZThatDoesNotExist();

    delegate.get().navigationResponseDidBecomeDownload = ^(WKWebView *, WKNavigationResponse *, WKDownload *download) {
        download.delegate = delegate.get();
        delegate.get().decideDestinationUsingResponse = ^(WKDownload *download, NSURLResponse *, NSString *, void (^completionHandler)(NSURL *)) {
            EXPECT_NULL(download.progress.fileURL);
            completionHandler(expectedDownloadFile);
            EXPECT_NOT_NULL(download.progress.fileURL);
            EXPECT_WK_STREQ(download.progress.fileURL.absoluteString, expectedDownloadFile.absoluteString);
        };
    };

    [webView loadRequest:server.request()];
    [delegate waitForDownloadDidFinish];

    checkFileContents(expectedDownloadFile, longString<5000>('a'));

    checkCallbackRecord(delegate.get(), {
        DownloadCallback::NavigationAction,
        DownloadCallback::NavigationResponse,
        DownloadCallback::NavigationResponseBecameDownload,
        DownloadCallback::DecideDestination,
        DownloadCallback::DidFinish
    });
}

}
