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
#import <pal/spi/cocoa/NSProgressSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakObjCPtr.h>

static void* WKDownloadProgressBytesExpectedToReceiveCountContext = &WKDownloadProgressBytesExpectedToReceiveCountContext;
static void* WKDownloadProgressBytesReceivedContext = &WKDownloadProgressBytesReceivedContext;

static NSString * const countOfBytesExpectedToReceiveKeyPath = @"countOfBytesExpectedToReceive";
static NSString * const countOfBytesReceivedKeyPath = @"countOfBytesReceived";

@implementation WKDownloadProgress {
    RetainPtr<NSURLSessionDownloadTask> m_task;
    WeakPtr<WebKit::Download> m_download;
    RefPtr<WebKit::SandboxExtension> m_sandboxExtension;
}

- (void)performCancel
{
    if (m_download)
        m_download->cancel();
    m_download = nullptr;
}

- (instancetype)initWithDownloadTask:(NSURLSessionDownloadTask *)task download:(WebKit::Download&)download URL:(NSURL *)fileURL sandboxExtension:(RefPtr<WebKit::SandboxExtension>)sandboxExtension
{
    if (!(self = [self initWithParent:nil userInfo:nil]))
        return nil;

    m_task = task;
    m_download = makeWeakPtr(download);

    [task addObserver:self forKeyPath:countOfBytesExpectedToReceiveKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:WKDownloadProgressBytesExpectedToReceiveCountContext];
    [task addObserver:self forKeyPath:countOfBytesReceivedKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:WKDownloadProgressBytesReceivedContext];

    self.kind = NSProgressKindFile;
    self.fileOperationKind = NSProgressFileOperationKindDownloading;
    self.fileURL = fileURL;
    m_sandboxExtension = sandboxExtension;

    self.cancellable = YES;
    self.cancellationHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKDownloadProgress> { self }] () mutable {
        if (!RunLoop::isMain()) {
            RunLoop::main().dispatch([weakSelf = WTFMove(weakSelf)] {
                [weakSelf performCancel];
            });
            return;
        }
        [weakSelf performCancel];
    }).get();

    return self;
}

#if USE(NSPROGRESS_PUBLISHING_SPI)
- (void)_publish
#else
- (void)publish
#endif
{
    BOOL consumedExtension = m_sandboxExtension->consume();
    ASSERT_UNUSED(consumedExtension, consumedExtension);

#if USE(NSPROGRESS_PUBLISHING_SPI)
    [super _publish];
#else
    [super publish];
#endif
}

#if USE(NSPROGRESS_PUBLISHING_SPI)
- (void)_unpublish
#else
- (void)unpublish
#endif
{
#if USE(NSPROGRESS_PUBLISHING_SPI)
    [super _unpublish];
#else
    [super unpublish];
#endif

    m_sandboxExtension->revoke();
    m_sandboxExtension = nullptr;
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
