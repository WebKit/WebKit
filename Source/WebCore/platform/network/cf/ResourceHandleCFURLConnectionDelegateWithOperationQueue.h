/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(CFURLCONNECTION)

#include "ResourceHandleCFURLConnectionDelegate.h"
#include <CFNetwork/CFNetwork.h>
#include <dispatch/queue.h>
#include <wtf/Function.h>
#include <wtf/MessageQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

class ResourceHandleCFURLConnectionDelegateWithOperationQueue final : public ResourceHandleCFURLConnectionDelegate {
public:
    ResourceHandleCFURLConnectionDelegateWithOperationQueue(ResourceHandle*, MessageQueue<Function<void()>>*);
    virtual ~ResourceHandleCFURLConnectionDelegateWithOperationQueue();

private:
    bool hasHandle() const;

    void setupRequest(CFMutableURLRequestRef) override;
    void setupConnectionScheduling(CFURLConnectionRef) override;
    void releaseHandle() override;

    CFURLRequestRef willSendRequest(CFURLRequestRef, CFURLResponseRef) override;
    void didReceiveResponse(CFURLConnectionRef, CFURLResponseRef) override;
    void didReceiveData(CFDataRef, CFIndex originalLength) override;
    void didFinishLoading() override;
    void didFail(CFErrorRef) override;
    CFCachedURLResponseRef willCacheResponse(CFCachedURLResponseRef) override;
    void didReceiveChallenge(CFURLAuthChallengeRef) override;
    void didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite) override;
    Boolean shouldUseCredentialStorage() override;

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    Boolean canRespondToProtectionSpace(CFURLProtectionSpaceRef) override;
#endif

    BinarySemaphore m_semaphore;
    MessageQueue<Function<void()>>* m_messageQueue { nullptr };

    RetainPtr<CFURLRequestRef> m_requestResult;
    RetainPtr<CFCachedURLResponseRef> m_cachedResponseResult;
    bool m_boolResult { false };
};

} // namespace WebCore.

#endif // USE(CFURLCONNECTION)
