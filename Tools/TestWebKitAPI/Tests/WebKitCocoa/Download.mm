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

#if PLATFORM(MAC) || PLATFORM(IOS)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestDownloadDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
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

static bool isDone;
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
    _destinationPath = FileSystem::openTemporaryFile("TestWebKitAPI", fileHandle);
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
    DownloadDelegate *downloadDelegate = [[DownloadDelegate alloc] init];
    [processPool _setDownloadDelegate:downloadDelegate];

    @autoreleasepool {
        EXPECT_EQ(downloadDelegate, [processPool _downloadDelegate]);
    }

    [downloadDelegate release];
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
    decisionHandler(_WKNavigationResponsePolicyBecomeDownload);
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
    [webView setNavigationDelegate:[[DownloadRequestOriginalURLNavigationDelegate alloc] init]];
    [[[webView configuration] processPool] _setDownloadDelegate:[[DownloadRequestOriginalURLDelegate alloc] initWithExpectedOriginalURL:sourceURL]];

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
    _destinationPath = FileSystem::openTemporaryFile("TestWebKitAPI", fileHandle);
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
    _destinationPath = FileSystem::openTemporaryFile("TestWebKitAPI", fileHandle);
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
    _destinationPath = FileSystem::openTemporaryFile(filename, fileHandle);
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

- (void)_downloadDidCancel:(_WKDownload *)download
{
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
static bool didCancel;
static RetainPtr<NSString> destination;

@interface DownloadMonitorTestDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation DownloadMonitorTestDelegate

- (void)_downloadDidStart:(_WKDownload *)download
{
    didDownloadStart = true;
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    didCancel = true;
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

void respondSlowly(int socket, double kbps, bool& terminateServer)
{
    EXPECT_FALSE(isMainThread());
    char readBuffer[1000];
    auto bytesRead = ::read(socket, readBuffer, sizeof(readBuffer));
    EXPECT_GT(bytesRead, 0);
    EXPECT_TRUE(static_cast<size_t>(bytesRead) < sizeof(readBuffer));
    
    const char* responseHeader =
    "HTTP/1.1 200 OK\r\n"
    "Content-Disposition: attachment; filename=\"filename.dat\"\r\n"
    "Content-Length: 100000000\r\n\r\n";
    auto bytesWritten = ::write(socket, responseHeader, strlen(responseHeader));
    EXPECT_EQ(static_cast<size_t>(bytesWritten), strlen(responseHeader));
    
    const double writesPerSecond = 100;
    Vector<char> writeBuffer(static_cast<size_t>(1024 * kbps / writesPerSecond));
    while (!terminateServer) {
        auto before = MonotonicTime::now();
        ::write(socket, writeBuffer.data(), writeBuffer.size());
        double writeDuration = (MonotonicTime::now() - before).seconds();
        double desiredSleep = 1.0 / writesPerSecond;
        if (writeDuration < desiredSleep)
            usleep(USEC_PER_SEC * (desiredSleep - writeDuration));
    }
}

RetainPtr<WKWebView> webViewWithDownloadMonitorSpeedMultiplier(size_t multiplier)
{
    static auto navigationDelegate = adoptNS([DownloadNavigationDelegate new]);
    static auto downloadDelegate = adoptNS([DownloadMonitorTestDelegate new]);
    auto processPoolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[_WKWebsiteDataStoreConfiguration new] autorelease];
    dataStoreConfiguration.testSpeedMultiplier = multiplier;
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [webViewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration] autorelease]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView configuration].processPool._downloadDelegate = downloadDelegate.get();
    return webView;
}

enum class AppReturnsToForeground { No, Yes };
    
void downloadAtRate(double desiredKbps, unsigned speedMultiplier, AppReturnsToForeground returnToForeground = AppReturnsToForeground::No)
{
    bool terminateServer = false;
    TCPServer server([&](int socket) {
        respondSlowly(socket, desiredKbps, terminateServer);
    });
    
    auto webView = webViewWithDownloadMonitorSpeedMultiplier(speedMultiplier);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    receivedData = false;
    Util::run(&receivedData);
    // Start the DownloadMonitor's timer.
    [[webView configuration].processPool _synthesizeAppIsBackground:YES];
    if (returnToForeground == AppReturnsToForeground::Yes)
        [[webView configuration].processPool _synthesizeAppIsBackground:NO];
    didCancel = false;
    Util::run(&didCancel);
    terminateServer = true;
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
        EXPECT_FALSE(didCancel);
        didCancel = true;
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
        EXPECT_FALSE(didCancel);
        didCancel = true;
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
    if (_responsePolicy != _WKNavigationResponsePolicyBecomeDownload || _didStartDownload)
        isDone = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    EXPECT_FALSE(_didFailProvisionalNavigation);
    _didFinishNavigation = YES;
    if (_responsePolicy != _WKNavigationResponsePolicyBecomeDownload || _didStartDownload)
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
    [delegate setResponsePolicy:_WKNavigationResponsePolicyBecomeDownload];
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

    [delegate setResponsePolicy:_WKNavigationResponsePolicyBecomeDownload];
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
    _path = FileSystem::openTemporaryFile("TestWebKitAPI", fileHandle);
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

    std::atomic<bool> receivedFirstConnection { false };

    TCPServer server([&](int socket) {
        if (!receivedFirstConnection.exchange(true)) {
            TCPServer::read(socket);

            const char* responseHeader =
            "HTTP/1.1 200 OK\r\n"
            "ETag: test\r\n"
            "Content-Length: 10000\r\n\r\n";
            TCPServer::write(socket, responseHeader, strlen(responseHeader));

            char data[5000];
            memset(data, 0, 5000);
            TCPServer::write(socket, data, 5000);

            // Wait for the client to cancel the download before closing the connection.
            Util::run(&isDone);
        } else {
            TCPServer::read(socket);
            const char* challengeHeader =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
            "Content-Length: 0\r\n"
            "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n";
            TCPServer::write(socket, challengeHeader, strlen(challengeHeader));

            TCPServer::read(socket);

            const char* responseHeader =
            "HTTP/1.1 206 Partial Content\r\n"
            "ETag: test\r\n"
            "Content-Range: bytes 5000-9999/10000\r\n"
            "Content-Length: 5000\r\n\r\n";
            TCPServer::write(socket, responseHeader, strlen(responseHeader));

            char data[5000];
            memset(data, 1, 5000);
            TCPServer::write(socket, data, 5000);
        }
    }, 2);

    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    auto websiteDataStore = adoptNS([WKWebsiteDataStore defaultDataStore]);

    auto delegate1 = adoptNS([[DownloadCancelingDelegate alloc] init]);
    [processPool _setDownloadDelegate:delegate1.get()];

    isDone = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]];
    [processPool _downloadURLRequest:request websiteDataStore:websiteDataStore.get() originatingWebView:nil];

    Util::run(&isDone);

    isDone = false;
    auto delegate2 = adoptNS([[AuthenticationChallengeHandlingDelegate alloc] init]);
    [processPool _setDownloadDelegate:delegate2.get()];
    [processPool _resumeDownloadFromData:[delegate1 resumeData].get() websiteDataStore:websiteDataStore.get() path:[delegate1 path].get() originatingWebView:nil];

    Util::run(&isDone);
}

#if HAVE(NETWORK_FRAMEWORK)

template<size_t length>
String longString(LChar c)
{
    Vector<LChar> vector(length, c);
    return String(vector.data(), length);
}

TEST(_WKDownload, Resume)
{
    using namespace TestWebKitAPI;
    HTTPServer server([connectionCount = 0](Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([connection](Vector<char>&&) {
                connection.send(makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "ETag: test\r\n"
                    "Content-Length: 10000\r\n"
                    "Content-Disposition: attachment; filename=\"example.txt\"\r\n"
                    "\r\n", longString<5000>('a')
                ));
            });
            break;
        case 2:
            connection.receiveHTTPRequest([connection](Vector<char>&& request) {
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
    });

    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"DownloadResumeTest"] isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:tempDir withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *expectedDownloadFile = [tempDir URLByAppendingPathComponent:@"example.txt"];
    [[NSFileManager defaultManager] removeItemAtURL:expectedDownloadFile error:nil];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        completionHandler(_WKNavigationResponsePolicyBecomeDownload);
    };

    enum class Callback : uint8_t { Start, WriteData, DecideDestination, CreateDestination, Cancel, Finish };
    __block Vector<Callback> callbacks;
    __block bool didCancel = false;
    __block bool didFinish = false;
    __block bool receivedData = false;
    __block RetainPtr<_WKDownload> download;
    __block RetainPtr<NSData> resumeData;

    auto downloadDelegate = adoptNS([TestDownloadDelegate new]);
    downloadDelegate.get().decideDestinationWithSuggestedFilename = ^(_WKDownload *, NSString *suggestedFilename, void (^completionHandler)(BOOL, NSString *)) {
        callbacks.append(Callback::DecideDestination);
        completionHandler(YES, [tempDir URLByAppendingPathComponent:suggestedFilename].path);
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
        EXPECT_WK_STREQ(destination, [tempDir URLByAppendingPathComponent:@"example.txt"].path);
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

    NSData *fileContents = [NSData dataWithContentsOfURL:expectedDownloadFile];
    EXPECT_EQ(fileContents.length, 10000u);
    EXPECT_TRUE(fileContents.bytes);
    if (fileContents.bytes) {
        for (size_t i = 0; i < 5000; i++)
            EXPECT_EQ(static_cast<const char*>(fileContents.bytes)[i], 'a');
        for (size_t i = 5000; i < 10000; i++)
            EXPECT_EQ(static_cast<const char*>(fileContents.bytes)[i], 'b');
    }
}

#endif // HAVE(NETWORK_FRAMEWORK)

@interface DownloadTestSchemeDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DownloadTestSchemeDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    if ([navigationResponse.response.URL.absoluteString hasSuffix:@"/download"])
        decisionHandler(_WKNavigationResponsePolicyBecomeDownload);
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

static const char* documentText = R"DOCDOCDOC(
<script>
function loaded()
{
    document.getElementById("thelink").click();
}
</script>
<body onload="loaded();">
<a id="thelink" href="download">Click me</a>
</body>
)DOCDOCDOC";

TEST(_WKDownload, SubframeSecurityOrigin)
{
    auto navigationDelegate = adoptNS([[DownloadTestSchemeDelegate alloc] init]);
    auto downloadDelegate = adoptNS([[DownloadSecurityOriginDelegate alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    TestWebKitAPI::HTTPServer server({
        { "/page", { documentText } },
        { "/download", { documentText } },
    });
    downloadDelegate->_serverPort = server.port();
    downloadDelegate->_webView = webView.get();

    isDone = false;
    [webView loadHTMLString:[NSString stringWithFormat:@"<body><iframe src='http://127.0.0.1:%d/page'></iframe></body>", server.port()] baseURL:nil];
    TestWebKitAPI::Util::run(&isDone);
}
#endif // PLATFORM(MAC) || PLATFORM(IOS)
