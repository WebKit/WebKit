/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import <WebKit2/WKFoundation.h>

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebCore/FileSystem.h>
#import <WebKit2/_WKDownload.h>
#import <WebKit2/_WKDownloadDelegate.h>
#import <WebKit2/WKNavigationDelegate.h>
#import <WebKit2/WKProcessPoolPrivate.h>
#import <WebKit2/WKWebView.h>
#import <WebKit2/WKWebViewConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static bool isDone;
static NSURL *sourceURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

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
    _download = download;
}

- (void)_download:(_WKDownload *)download didReceiveResponse:(NSURLResponse *)response
{
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

- (NSString *)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename allowOverwrite:(BOOL *)allowOverwrite
{
    EXPECT_EQ(_download, download);

    WebCore::PlatformFileHandle fileHandle;
    _destinationPath = WebCore::openTemporaryFile("TestWebKitAPI", fileHandle);
    EXPECT_TRUE(fileHandle != WebCore::invalidPlatformFileHandle);
    WebCore::closeFile(fileHandle);

    *allowOverwrite = YES;
    return _destinationPath;
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == 1080
    // Work around <rdar://problem/7611450> NSURLDownload calls didReceiveDataOfLength: delegate method too many times
    _receivedContentLength /= 2;
#endif
    EXPECT_EQ(_download, download);
    EXPECT_TRUE(_expectedContentLength == NSURLResponseUnknownLength || static_cast<uint64_t>(_expectedContentLength) == _receivedContentLength);
    EXPECT_TRUE([[NSFileManager defaultManager] contentsEqualAtPath:_destinationPath andPath:[sourceURL path]]);
    WebCore::deleteFile(_destinationPath);
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
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&isDone);
}

@interface DownloadNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DownloadNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationPolicyDecision))decisionHandler
{
    decisionHandler(WKNavigationPolicyDecisionDownload);
}
@end

TEST(_WKDownload, DownloadRequest)
{
    runTest(adoptNS([[DownloadNavigationDelegate alloc] init]).get(), adoptNS([[DownloadDelegate alloc] init]).get(), sourceURL);
}

@interface ConvertResponseToDownloadNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ConvertResponseToDownloadNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicyDecision))decisionHandler
{
    decisionHandler(WKNavigationResponsePolicyDecisionBecomeDownload);
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
    @autoreleasepool {
        EXPECT_EQ([download originatingWebView], _webView);
    }

    _webView = nullptr;
    EXPECT_NULL([download originatingWebView]);
    isDone = true;
}

@end

TEST(_WKDownload, OriginatingWebView)
{
    RetainPtr<DownloadNavigationDelegate> navigationDelegate = adoptNS([[DownloadNavigationDelegate alloc] init]);                 
    RetainPtr<OriginatingWebViewDownloadDelegate> downloadDelegate;
    {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        downloadDelegate = adoptNS([[OriginatingWebViewDownloadDelegate alloc] initWithWebView:webView.get()]);
        [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:sourceURL]];
    }

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

#endif
