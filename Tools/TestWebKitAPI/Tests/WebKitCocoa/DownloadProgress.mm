/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Utilities.h"
#import <Foundation/NSProgress.h>
#import <WebCore/FileSystem.h>
#import <WebKit/WKBrowsingContextController.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/_WKDownload.h>
#import <WebKit/_WKDownloadDelegate.h>
#import <pal/spi/cocoa/NSProgressSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if WK_API_ENABLED

@class DownloadProgressTestProtocol;

enum class DownloadStartType {
    ConvertLoadToDownload,
    StartFromNavigationAction,
    StartInProcessPool,
};

@interface DownloadProgressTestRunner : NSObject <WKNavigationDelegate, _WKDownloadDelegate>

@property (nonatomic, readonly) _WKDownload *download;
@property (nonatomic, readonly) NSProgress *progress;

- (void)startLoadingWithProtocol:(DownloadProgressTestProtocol *)protocol;

@end

@interface DownloadProgressTestProtocol : NSURLProtocol
@end

@implementation DownloadProgressTestProtocol

static DownloadProgressTestRunner *currentTestRunner;

+ (void)registerProtocolForTestRunner:(DownloadProgressTestRunner *)testRunner
{
    currentTestRunner = testRunner;
    [NSURLProtocol registerClass:self];
    [WKBrowsingContextController registerSchemeForCustomProtocol:@"http"];
}

+ (void)unregisterProtocol
{
    currentTestRunner = nullptr;
    [WKBrowsingContextController unregisterSchemeForCustomProtocol:@"http"];
    [NSURLProtocol unregisterClass:self];
}

// MARK: NSURLProtocol Methods

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    return [request.URL.scheme caseInsensitiveCompare:@"http"] == NSOrderedSame;
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b
{
    return NO;
}

- (void)startLoading
{
    [currentTestRunner startLoadingWithProtocol:self];
}

- (void)stopLoading
{
}

@end

static void* progressObservingContext = &progressObservingContext;

@implementation DownloadProgressTestRunner {
    RetainPtr<NSURL> m_progressURL;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<id> m_progressSubscriber;
    RetainPtr<NSProgress> m_progress;
    RetainPtr<_WKDownload> m_download;
    RetainPtr<DownloadProgressTestProtocol> m_protocol;
    BlockPtr<void(void)> m_unpublishingBlock;
    DownloadStartType m_startType;
    NSInteger m_expectedLength;
    bool m_hasProgress;
    bool m_lostProgress;
    bool m_downloadStarted;
    bool m_downloadDidCreateDestination;
    bool m_downloadFinished;
    bool m_downloadCanceled;
    bool m_downloadFailed;
    bool m_hasUpdatedCompletedUnitCount;
}

- (instancetype)init
{
    self = [super init];

    [DownloadProgressTestProtocol registerProtocolForTestRunner:self];

    NSString *fileName = [NSString stringWithFormat:@"download-progress-%@", [NSUUID UUID].UUIDString];
    m_progressURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:fileName] isDirectory:NO];

    currentTestRunner = self;

    m_unpublishingBlock = makeBlockPtr([self] {
        [self _didLoseProgress];
    }).get();

    return self;
}

- (_WKDownload *)download
{
    return m_download.get();
}

- (NSProgress *)progress
{
    return m_progress.get();
}

- (void)startLoadingWithProtocol:(DownloadProgressTestProtocol *)protocol
{
    m_protocol = protocol;

    auto response = adoptNS([[NSURLResponse alloc] initWithURL:protocol.request.URL MIMEType:@"application/x-test-file" expectedContentLength:m_expectedLength textEncodingName:nullptr]);
    [m_protocol.get().client URLProtocol:m_protocol.get() didReceiveResponse:response.get() cacheStoragePolicy:NSURLCacheStorageNotAllowed];
}

- (void)tearDown
{
    if (m_webView) {
        m_webView.get().configuration.processPool._downloadDelegate = nullptr;
        [m_webView.get() removeFromSuperview];
        m_webView = nullptr;
    }

    if (m_progressSubscriber) {
#if USE(NSPROGRESS_PUBLISHING_SPI)
        [NSProgress _removeSubscriber:m_progressSubscriber.get()];
#else
        [NSProgress removeSubscriber:m_progressSubscriber.get()];
#endif
        m_progressSubscriber = nullptr;
    }

    m_progress = nullptr;
    m_download = nullptr;
    m_protocol = nullptr;
    m_unpublishingBlock = nullptr;

    [DownloadProgressTestProtocol unregisterProtocol];
}

- (void)_didGetProgress:(NSProgress *)progress
{
    ASSERT(!m_progress);
    m_progress = progress;
    [progress addObserver:self forKeyPath:@"completedUnitCount" options:NSKeyValueObservingOptionNew context:progressObservingContext];
    m_hasProgress = true;
}

- (void)_didLoseProgress
{
    ASSERT(m_progress);
    [m_progress.get() removeObserver:self forKeyPath:@"completedUnitCount"];
    m_progress = nullptr;
    m_lostProgress = true;
}

- (void)subscribeAndWaitForProgress
{
    if (!m_progressSubscriber) {
        auto publishingHandler = makeBlockPtr([weakSelf = WeakObjCPtr<DownloadProgressTestRunner> { self }](NSProgress *progress) {
            if (auto strongSelf = weakSelf.get()) {
                [strongSelf.get() _didGetProgress:progress];
                return strongSelf->m_unpublishingBlock.get();
            }
            return static_cast<NSProgressUnpublishingHandler>(nil);
        });

#if USE(NSPROGRESS_PUBLISHING_SPI)
        m_progressSubscriber = [NSProgress _addSubscriberForFileURL:m_progressURL.get() withPublishingHandler:publishingHandler.get()];
#else
        m_progressSubscriber = [NSProgress addSubscriberForFileURL:m_progressURL.get() withPublishingHandler:publishingHandler.get()];
#endif
    }
    TestWebKitAPI::Util::run(&m_hasProgress);
}

- (void)waitToLoseProgress
{
    TestWebKitAPI::Util::run(&m_lostProgress);
}

- (void)startDownload:(DownloadStartType)startType expectedLength:(NSInteger)expectedLength
{
    m_startType = startType;
    m_expectedLength = expectedLength;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    m_webView.get().navigationDelegate = self;
    m_webView.get().configuration.processPool._downloadDelegate = self;

    auto request = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"http://file"]]);

    switch (startType) {
    case DownloadStartType::ConvertLoadToDownload:
    case DownloadStartType::StartFromNavigationAction:
        [m_webView loadRequest:request.get()];
        break;
    case DownloadStartType::StartInProcessPool:
        [m_webView.get().configuration.processPool _downloadURLRequest:request.get() originatingWebView:nullptr];
        break;
    }

    TestWebKitAPI::Util::run(&m_downloadStarted);
}

- (void)publishProgress
{
    ASSERT(m_download);

    [m_download.get() publishProgressAtURL:m_progressURL.get()];
}

- (void)receiveData:(NSInteger)length
{
    auto data = adoptNS([[NSMutableData alloc] init]);
    while (length-- > 0) {
        const char byte = 'A';
        [data.get() appendBytes:static_cast<const void*>(&byte) length:1];
    }

    [m_protocol.get().client URLProtocol:m_protocol.get() didLoadData:data.get()];
}

- (void)finishDownloadTask
{
    [m_protocol.get().client URLProtocolDidFinishLoading:m_protocol.get()];
}

- (void)failDownloadTask
{
    [m_protocol.get().client URLProtocol:m_protocol.get() didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorNetworkConnectionLost userInfo:nullptr]];
}

- (void)waitForDownloadDidCreateDestination
{
    TestWebKitAPI::Util::run(&m_downloadDidCreateDestination);
}

- (void)waitForDownloadFinished
{
    TestWebKitAPI::Util::run(&m_downloadFinished);
}

- (void)waitForDownloadCanceled
{
    TestWebKitAPI::Util::run(&m_downloadCanceled);
}

- (void)waitForDownloadFailed
{
    TestWebKitAPI::Util::run(&m_downloadFailed);
}

- (int64_t)waitForUpdatedCompletedUnitCount
{
    TestWebKitAPI::Util::run(&m_hasUpdatedCompletedUnitCount);
    m_hasUpdatedCompletedUnitCount = false;

    return m_progress.get().completedUnitCount;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context == progressObservingContext) {
        EXPECT_EQ(object, m_progress.get());
        EXPECT_STREQ(keyPath.UTF8String, "completedUnitCount");
        m_hasUpdatedCompletedUnitCount = true;
    } else
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

// MARK: <WKNavigationDelegate> Methods

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    if (m_startType == DownloadStartType::ConvertLoadToDownload)
        decisionHandler(_WKNavigationResponsePolicyBecomeDownload);
    else
        decisionHandler(WKNavigationResponsePolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (m_startType == DownloadStartType::StartFromNavigationAction)
        decisionHandler(_WKNavigationActionPolicyDownload);
    else
        decisionHandler(WKNavigationActionPolicyAllow);
}

// MARK: <_WKDownloadDelegate> Methods

- (void)_downloadDidStart:(_WKDownload *)download
{
    ASSERT(!m_downloadStarted);
    ASSERT(!m_download);

    m_download = download;
    m_downloadStarted = true;
}

- (void)_download:(_WKDownload *)download didCreateDestination:(NSString *)destination
{
    EXPECT_EQ(download, m_download.get());
    m_downloadDidCreateDestination = true;
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_EQ(download, m_download.get());
    m_downloadFinished = true;
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    EXPECT_EQ(download, m_download.get());
    m_downloadCanceled = true;
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    EXPECT_EQ(download, m_download.get());
    m_downloadFailed = true;
}

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL allowOverwrite, NSString *destination))completionHandler
{
    EXPECT_EQ(download, m_download.get());

    WebCore::FileSystem::PlatformFileHandle fileHandle;
    RetainPtr<NSString> path = (NSString *)WebCore::FileSystem::openTemporaryFile("TestWebKitAPI", fileHandle);
    EXPECT_TRUE(fileHandle != WebCore::FileSystem::invalidPlatformFileHandle);
    WebCore::FileSystem::closeFile(fileHandle);

    completionHandler(YES, path.get());
}

@end

// End-to-end test of subscribing to progress on a successful download. The client
// should be able to receive an NSProgress that is updated as the download makes
// progress, and the NSProgress should be unpublished when the download finishes.
TEST(DownloadProgress, BasicSubscriptionAndProgressUpdates)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];

    [testRunner.get() receiveData:50];
    EXPECT_EQ([testRunner.get() waitForUpdatedCompletedUnitCount], 50);
    EXPECT_EQ(testRunner.get().progress.fractionCompleted, .5);

    [testRunner.get() receiveData:50];
    EXPECT_EQ([testRunner.get() waitForUpdatedCompletedUnitCount], 100);
    EXPECT_EQ(testRunner.get().progress.fractionCompleted, 1);

    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// Similar test as before, but initiating the download before receiving its response.
TEST(DownloadProgress, StartDownloadFromNavigationAction)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::StartFromNavigationAction expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    [testRunner.get() receiveData:100];
    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

TEST(DownloadProgress, StartDownloadInProcessPool)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::StartInProcessPool expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    [testRunner.get() receiveData:100];
    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// If the download is canceled, the progress should be unpublished.
TEST(DownloadProgress, LoseProgressWhenDownloadIsCanceled)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    [testRunner.get() receiveData:50];
    [testRunner.get().download cancel];
    [testRunner.get() waitForDownloadCanceled];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// If the download fails, the progress should be unpublished.
TEST(DownloadProgress, LoseProgressWhenDownloadFails)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    [testRunner.get() receiveData:50];
    [testRunner.get() failDownloadTask];
    [testRunner.get() waitForDownloadFailed];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// Canceling the progress should cancel the download.
TEST(DownloadProgress, CancelDownloadWhenProgressIsCanceled)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    [testRunner.get() receiveData:50];
    [testRunner.get().progress cancel];
    [testRunner.get() waitForDownloadCanceled];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// Publishing progress on a download after it has finished should be a safe no-op.
TEST(DownloadProgress, PublishProgressAfterDownloadFinished)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() receiveData:100];
    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() publishProgress];

    [testRunner.get() tearDown];
}

// Test the behavior of a download of unknown length.
TEST(DownloadProgress, IndeterminateDownloadSize)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:NSURLResponseUnknownLength];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    EXPECT_EQ(testRunner.get().progress.totalUnitCount, -1);

    [testRunner.get() receiveData:50];
    EXPECT_EQ([testRunner.get() waitForUpdatedCompletedUnitCount], 50);
    EXPECT_EQ(testRunner.get().progress.fractionCompleted, 0);
    EXPECT_EQ(testRunner.get().progress.totalUnitCount, -1);

    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// Test the behavior when a download continues returning data beyond its expected length.
TEST(DownloadProgress, ExtraData)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];

    [testRunner.get() receiveData:150];
    EXPECT_EQ([testRunner.get() waitForUpdatedCompletedUnitCount], 150);
    EXPECT_EQ(testRunner.get().progress.fractionCompleted, 1.5);

    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

// Clients should be able to publish progress on a download that has already started.
TEST(DownloadProgress, PublishProgressOnPartialDownload)
{
    auto testRunner = adoptNS([[DownloadProgressTestRunner alloc] init]);

    [testRunner.get() startDownload:DownloadStartType::ConvertLoadToDownload expectedLength:100];
    [testRunner.get() receiveData:50];

    // Ensure that the the data task has become a download task so that we test
    // telling a live Download, not a PendingDownload, to publish its progress.
    [testRunner.get() waitForDownloadDidCreateDestination];

    [testRunner.get() publishProgress];
    [testRunner.get() subscribeAndWaitForProgress];
    EXPECT_EQ(testRunner.get().progress.completedUnitCount, 50);

    [testRunner.get() receiveData:50];
    EXPECT_EQ([testRunner.get() waitForUpdatedCompletedUnitCount], 100);

    [testRunner.get() finishDownloadTask];
    [testRunner.get() waitForDownloadFinished];
    [testRunner.get() waitToLoseProgress];

    [testRunner.get() tearDown];
}

#endif // WK_API_ENABLED
