/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import <dispatch/dispatch.h>
#import <wtf/Box.h>
#import <wtf/Function.h>
#import <wtf/Lock.h>
#import <wtf/MessageQueue.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/SchedulePair.h>
#import <wtf/threads/BinarySemaphore.h>

namespace WebCore {
class NetworkLoadMetrics;
class ResourceHandle;
class SynchronousLoaderMessageQueue;
}

@interface WebCoreResourceHandleAsOperationQueueDelegate : NSObject <NSURLConnectionDelegate> {
    Lock m_lock;
    WebCore::ResourceHandle* m_handle WTF_GUARDED_BY_LOCK(m_lock);

    // Synchronous delegates on operation queue wait until main thread sends an asynchronous response.
    BinarySemaphore m_semaphore;
    RefPtr<WebCore::SynchronousLoaderMessageQueue> m_messageQueue;
    RetainPtr<NSURLRequest> m_requestResult;
    RetainPtr<NSCachedURLResponse> m_cachedResponseResult;
    std::optional<SchedulePairHashSet> m_scheduledPairs;
    BOOL m_boolResult;
}

- (void)detachHandle;
- (id)initWithHandle:(WebCore::ResourceHandle*)handle messageQueue:(RefPtr<WebCore::SynchronousLoaderMessageQueue>&&)messageQueue;
@end

@interface WebCoreResourceHandleWithCredentialStorageAsOperationQueueDelegate : WebCoreResourceHandleAsOperationQueueDelegate

@end
