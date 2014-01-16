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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceHandleCFURLConnectionDelegateWithOperationQueue_h
#define ResourceHandleCFURLConnectionDelegateWithOperationQueue_h

#if USE(CFNETWORK)

#include "ResourceHandleCFURLConnectionDelegate.h"
#include <CFNetwork/CFNetwork.h>
#include <dispatch/queue.h>
#include <dispatch/semaphore.h>

namespace WebCore {

class ResourceHandleCFURLConnectionDelegateWithOperationQueue final : public ResourceHandleCFURLConnectionDelegate {
public:
    ResourceHandleCFURLConnectionDelegateWithOperationQueue(ResourceHandle*);
    virtual ~ResourceHandleCFURLConnectionDelegateWithOperationQueue();

private:
    bool hasHandle() const;

    virtual void setupRequest(CFMutableURLRequestRef) override;
    virtual void setupConnectionScheduling(CFURLConnectionRef) override;

    virtual CFURLRequestRef willSendRequest(CFURLRequestRef, CFURLResponseRef) override;
    virtual void didReceiveResponse(CFURLResponseRef) override;
    virtual void didReceiveData(CFDataRef, CFIndex originalLength) override;
    virtual void didFinishLoading() override;
    virtual void didFail(CFErrorRef) override;
    virtual CFCachedURLResponseRef willCacheResponse(CFCachedURLResponseRef) override;
    virtual void didReceiveChallenge(CFURLAuthChallengeRef) override;
    virtual void didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite) override;
    virtual Boolean shouldUseCredentialStorage() override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual Boolean canRespondToProtectionSpace(CFURLProtectionSpaceRef) override;
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual void didReceiveDataArray(CFArrayRef dataArray) override;
#endif // USE(NETWORK_CFDATA_ARRAY_CALLBACK)

    virtual void continueWillSendRequest(CFURLRequestRef) override;
    virtual void continueDidReceiveResponse() override;
    virtual void continueShouldUseCredentialStorage(bool) override;
    virtual void continueWillCacheResponse(CFCachedURLResponseRef) override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual void continueCanAuthenticateAgainstProtectionSpace(bool) override;
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

    dispatch_queue_t m_queue;
    dispatch_semaphore_t m_semaphore;

    RetainPtr<CFURLRequestRef> m_requestResult;
    RetainPtr<CFCachedURLResponseRef> m_cachedResponseResult;
    bool m_boolResult;
};

} // namespace WebCore.

#endif // USE(CFNETWORK)

#endif // ResourceHandleCFURLConnectionDelegateWithOperationQueue_h
