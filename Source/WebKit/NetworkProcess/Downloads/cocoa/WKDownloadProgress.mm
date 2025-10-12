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
#import "WKDownloadProgress.h"

#import "Download.h"
#import "Logging.h"
#import <pal/spi/cocoa/NSProgressSPI.h>
#import <sys/xattr.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakObjCPtr.h>

#if HAVE(MODERN_DOWNLOADPROGRESS)
#import <BrowserEngineKit/BrowserEngineKit.h>
#import <wtf/cocoa/VectorCocoa.h>
#endif // HAVE(MODERN_DOWNLOADPROGRESS)

static void* WKDownloadProgressBytesExpectedToReceiveCountContext = &WKDownloadProgressBytesExpectedToReceiveCountContext;
static void* WKDownloadProgressBytesReceivedContext = &WKDownloadProgressBytesReceivedContext;

static NSString * const countOfBytesExpectedToReceiveKeyPath = @"countOfBytesExpectedToReceive";
static NSString * const countOfBytesReceivedKeyPath = @"countOfBytesReceived";

#if HAVE(MODERN_DOWNLOADPROGRESS)
bool enableModernDownloadProgress()
{
#if PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR)
    return true;
#else
    return false;
#endif
}

Vector<uint8_t> activityAccessToken()
{
    return makeVector([BEDownloadMonitor createAccessToken]);
}

@implementation WKModernDownloadProgress {
    RetainPtr<NSURLSessionDownloadTask> m_task;
    WeakPtr<WebKit::Download> m_download;
    RetainPtr<BEDownloadMonitor> m_downloadMonitor;
    BlockPtr<void()> m_didFinishCompletionHandler;
    BOOL m_useDownloadPlaceholder;
    BOOL m_fileCreatedHandlerCalled;
}

- (void)performCancel
{
    if (m_download)
        m_download->cancel([](auto) { }, WebKit::Download::IgnoreDidFailCallback::No);
    m_download = nullptr;
}

- (void)begin
{
    [m_downloadMonitor beginMonitoring:makeBlockPtr([weakDownload = WeakPtr { m_download }](BEDownloadMonitorLocation *location, NSError *error) {
        RELEASE_LOG(Network, "Download begin for url %{sensitive}s, error %s", location.url.absoluteString.UTF8String, error.localizedDescription.UTF8String);
        ensureOnMainRunLoop([weakDownload = WTFMove(weakDownload), location = RetainPtr { location }] {
            if (!weakDownload)
                return;
            weakDownload->setPlaceholderURL(location.get().url, location.get().bookmarkData);
        });
    }).get()];
}

- (void)resume:(NSURL *)url
{
    [m_downloadMonitor resumeMonitoring:url completionHandler:^(NSError *) { }];
}

- (instancetype)initWithDownloadTask:(NSURLSessionDownloadTask *)task download:(WebKit::Download&)download URL:(NSURL *)fileURL useDownloadPlaceholder:(BOOL)useDownloadPlaceholder resumePlaceholderURL:(NSURL *)resumePlaceholderURL liveActivityAccessToken:(NSData *)liveActivityAccessToken
{
    if (!(self = [self init]))
        return nil;

    m_task = task;
    m_download = download;
    m_useDownloadPlaceholder = useDownloadPlaceholder;
    m_fileCreatedHandlerCalled = NO;

    self.kind = NSProgressKindFile;
    self.fileOperationKind = NSProgressFileOperationKindDownloading;
    if (!m_useDownloadPlaceholder)
        self.fileURL = fileURL;

    self.cancellable = YES;
    self.cancellationHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKModernDownloadProgress> { self }] () mutable {
        ensureOnMainRunLoop([weakSelf = WTFMove(weakSelf)] {
            [weakSelf performCancel];
        });
    }).get();

    auto fileCreatedHandlerDownloadMonitorLocation = makeBlockPtr([weakDownload = WeakPtr { m_download }, weakSelf = WeakObjCPtr<WKModernDownloadProgress> { self }](BEDownloadMonitorLocation *location) mutable {
        RELEASE_LOG(Network, "File created handler for url %{sensitive}s", location.url.absoluteString.UTF8String);
        ensureOnMainRunLoop([weakDownload = WTFMove(weakDownload), location = RetainPtr { location }, weakSelf = WTFMove(weakSelf)] {
            if (!weakDownload)
                return;
            weakDownload->setFinalURL(location.get().url, location.get().bookmarkData);
            if (!weakSelf)
                return;
            weakSelf.get()->m_fileCreatedHandlerCalled = YES;
            if (!weakSelf.get()->m_didFinishCompletionHandler)
                return;
            weakSelf.get()->m_didFinishCompletionHandler();
        });
    });

    m_downloadMonitor = adoptNS([BEDownloadMonitor alloc]);

    RetainPtr<NSURL> sourceURL = task.currentRequest.URL;
    if (!sourceURL)
        sourceURL = adoptNS([[NSURL alloc] initWithString:@""]);
    [m_downloadMonitor initWithSourceURL:sourceURL.get() destinationURL:fileURL observedProgress:self liveActivityAccessToken:liveActivityAccessToken];

    if (useDownloadPlaceholder)
        [m_downloadMonitor useDownloadsFolderWithPlaceholderType:nil finalFileCreatedHandler:fileCreatedHandlerDownloadMonitorLocation.get()];

    if (resumePlaceholderURL)
        [self resume:resumePlaceholderURL];
    else
        [self begin];

    return self;
}

- (void)startUpdatingDownloadProgress
{
    [m_task addObserver:self forKeyPath:countOfBytesExpectedToReceiveKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:WKDownloadProgressBytesExpectedToReceiveCountContext];
    [m_task addObserver:self forKeyPath:countOfBytesReceivedKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:WKDownloadProgressBytesReceivedContext];
}

- (void)didFinish:(void (^)())completionHandler
{
    if (self.completedUnitCount != self.totalUnitCount)
        self.totalUnitCount = self.completedUnitCount;

    if (m_useDownloadPlaceholder && !m_fileCreatedHandlerCalled)
        m_didFinishCompletionHandler = completionHandler;
    else
        completionHandler();
}

- (void)dealloc
{
    [m_task.get() removeObserver:self forKeyPath:countOfBytesExpectedToReceiveKeyPath];
    [m_task.get() removeObserver:self forKeyPath:countOfBytesReceivedKeyPath];

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context == WKDownloadProgressBytesExpectedToReceiveCountContext) {
        RetainPtr<NSNumber> value = static_cast<NSNumber *>(change[NSKeyValueChangeNewKey]);
        ASSERT([value isKindOfClass:[NSNumber class]]);
        int64_t expectedByteCount = value.get().longLongValue;
        self.totalUnitCount = (expectedByteCount <= 0) ? -1 : expectedByteCount;
    } else if (context == WKDownloadProgressBytesReceivedContext) {
        RetainPtr<NSNumber> value = static_cast<NSNumber *>(change[NSKeyValueChangeNewKey]);
        ASSERT([value isKindOfClass:[NSNumber class]]);
        self.completedUnitCount = value.get().longLongValue;
    } else
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

@end
#endif // HAVE(MODERN_DOWNLOADPROGRESS)

@implementation WKDownloadProgress {
    RetainPtr<NSURLSessionDownloadTask> m_task;
    WeakPtr<WebKit::Download> m_download;
    RefPtr<WebKit::SandboxExtension> m_sandboxExtension;
}

- (void)performCancel
{
    if (RefPtr download = std::exchange(m_download, nullptr).get())
        download->cancel([](auto) { }, WebKit::Download::IgnoreDidFailCallback::No);
}

- (instancetype)initWithDownloadTask:(NSURLSessionDownloadTask *)task download:(WebKit::Download&)download URL:(NSURL *)fileURL sandboxExtension:(RefPtr<WebKit::SandboxExtension>)sandboxExtension
{
    if (!(self = [self initWithParent:nil userInfo:nil]))
        return nil;

    m_task = task;
    m_download = download;

    [task addObserver:self forKeyPath:countOfBytesExpectedToReceiveKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:WKDownloadProgressBytesExpectedToReceiveCountContext];
    [task addObserver:self forKeyPath:countOfBytesReceivedKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:WKDownloadProgressBytesReceivedContext];

    self.kind = NSProgressKindFile;
    self.fileOperationKind = NSProgressFileOperationKindDownloading;
    self.fileURL = fileURL;
    m_sandboxExtension = sandboxExtension;

    self.cancellable = YES;
    self.cancellationHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKDownloadProgress> { self }] () mutable {
        ensureOnMainRunLoop([weakSelf = WTFMove(weakSelf)] {
            if (RetainPtr protectedSelf = weakSelf.get())
                [protectedSelf performCancel];
        });
    }).get();

    return self;
}

#if HAVE(NSPROGRESS_PUBLISHING_SPI)
- (void)_publish
#else
- (void)publish
#endif
{
    if (RefPtr extension = m_sandboxExtension) {
        BOOL consumedExtension = extension->consume();
        ASSERT_UNUSED(consumedExtension, consumedExtension);
    }

#if HAVE(NSPROGRESS_PUBLISHING_SPI)
    [super _publish];
#else
    [super publish];
#endif
}

#if HAVE(NSPROGRESS_PUBLISHING_SPI)
- (void)_unpublish
#else
- (void)unpublish
#endif
{
    [self _updateProgressExtendedAttributeOnProgressFile];

#if HAVE(NSPROGRESS_PUBLISHING_SPI)
    [super _unpublish];
#else
    [super unpublish];
#endif

    if (RefPtr extension = std::exchange(m_sandboxExtension, nullptr))
        extension->revoke();
}

- (void)_updateProgressExtendedAttributeOnProgressFile
{
    int64_t total = self.totalUnitCount;
    int64_t completed = self.completedUnitCount;

    float fraction = (total > 0) ? (float)completed / (float)total : -1;
    auto xattrContents = adoptNS([[NSString alloc] initWithFormat:@"%.3f", fraction]);

    setxattr(self.fileURL.fileSystemRepresentation, "com.apple.progress.fractionCompleted", xattrContents.get().UTF8String, [xattrContents.get() lengthOfBytesUsingEncoding:NSUTF8StringEncoding], 0, 0);
}

- (void)dealloc
{
    [m_task.get() removeObserver:self forKeyPath:countOfBytesExpectedToReceiveKeyPath];
    [m_task.get() removeObserver:self forKeyPath:countOfBytesReceivedKeyPath];

    ASSERT(!m_sandboxExtension);

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context == WKDownloadProgressBytesExpectedToReceiveCountContext) {
        RetainPtr<NSNumber> value = static_cast<NSNumber *>(change[NSKeyValueChangeNewKey]);
        ASSERT([value isKindOfClass:[NSNumber class]]);
        int64_t expectedByteCount = value.get().longLongValue;
        self.totalUnitCount = (expectedByteCount <= 0) ? -1 : expectedByteCount;
    } else if (context == WKDownloadProgressBytesReceivedContext) {
        RetainPtr<NSNumber> value = static_cast<NSNumber *>(change[NSKeyValueChangeNewKey]);
        ASSERT([value isKindOfClass:[NSNumber class]]);
        self.completedUnitCount = value.get().longLongValue;
    } else
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

@end
