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
#import <WebKit2/_WKDownloadDelegate.h>
#import <WebKit2/WKNavigationDelegate.h>
#import <WebKit2/WKProcessPoolPrivate.h>
#import <WebKit2/WKWebView.h>
#import <WebKit2/WKWebViewConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static bool isDone;

@interface DownloadDelegate : NSObject <_WKDownloadDelegate>
- (id)initWithSourceURL:(NSURL *)sourceURL;
@property (nonatomic, readonly) NSURL *sourceURL;
@end

@implementation DownloadDelegate {
    RetainPtr<_WKDownload> _download;
    RetainPtr<NSURL> _sourceURL;
    String _destinationPath;
    long long _expectedContentLength;
    uint64_t _receivedContentLength;
}

- (id)initWithSourceURL:(NSURL *)sourceURL
{
    if (!(self = [super init]))
        return nil;
    _sourceURL = sourceURL;
    _expectedContentLength = 0;
    _receivedContentLength = 0;
    return self;
}

- (NSURL *)sourceURL
{
    return _sourceURL.get();
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_NULL(_download);
    EXPECT_NOT_NULL(download);
    _download = download;
}

- (void)_download:(_WKDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    EXPECT_EQ(_download, download);
    EXPECT_TRUE(_expectedContentLength == 0);
    EXPECT_TRUE(_receivedContentLength == 0);
    EXPECT_TRUE([[[response URL] path] isEqualToString:[[self sourceURL] path]]);
    _expectedContentLength = [response expectedContentLength];
}

- (void)_download:(_WKDownload *)download didReceiveData:(uint64_t)length
{
    NSLog(@"didReceiveData: %llu\n", length);
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
    NSLog(@"_expectedContentLength: %lld\n", _expectedContentLength);
    NSLog(@"_receivedContentLength: %llu\n", _receivedContentLength);
    EXPECT_EQ(_download, download);
    EXPECT_TRUE(_expectedContentLength == NSURLResponseUnknownLength || static_cast<uint64_t>(_expectedContentLength) == _receivedContentLength);
    EXPECT_TRUE([[NSFileManager defaultManager] contentsEqualAtPath:_destinationPath andPath:[_sourceURL path]]);
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

static void runTestWithNavigationDelegate(id <WKNavigationDelegate> navigationDelegate)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate];

    RetainPtr<DownloadDelegate> downloadDelegate = adoptNS([[DownloadDelegate alloc] initWithSourceURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]);
    [[[webView configuration] processPool] _setDownloadDelegate:downloadDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[downloadDelegate sourceURL]]];
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
    runTestWithNavigationDelegate(adoptNS([[DownloadNavigationDelegate alloc] init]).get());
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
    runTestWithNavigationDelegate(adoptNS([[ConvertResponseToDownloadNavigationDelegate alloc] init]).get());
}

#endif
