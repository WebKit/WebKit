/*
 * Copyright (C) 2004-2013 Apple Inc.  All rights reserved.
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

#ifndef ResourceHandleCFURLConnectionDelegate_h
#define ResourceHandleCFURLConnectionDelegate_h

#if USE(CFNETWORK)

#include "ResourceRequest.h"
#include <CFNetwork/CFURLConnectionPriv.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class ResourceHandle;

class ResourceHandleCFURLConnectionDelegate : public ThreadSafeRefCounted<ResourceHandleCFURLConnectionDelegate> {
public:
    ResourceHandleCFURLConnectionDelegate(ResourceHandle*);
    virtual ~ResourceHandleCFURLConnectionDelegate();

    CFURLConnectionClient_V6 makeConnectionClient() const;
    virtual void setupRequest(CFMutableURLRequestRef) = 0;
    virtual void setupConnectionScheduling(CFURLConnectionRef) = 0;
    virtual void releaseHandle();

    virtual void continueWillSendRequest(CFURLRequestRef) = 0;
    virtual void continueDidReceiveResponse() = 0;
    virtual void continueWillCacheResponse(CFCachedURLResponseRef) = 0;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual void continueCanAuthenticateAgainstProtectionSpace(bool) = 0;
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

protected:
    RetainPtr<CFURLResponseRef> synthesizeRedirectResponseIfNecessary(CFURLRequestRef, CFURLResponseRef);
    ResourceRequest createResourceRequest(CFURLRequestRef, CFURLResponseRef);

private:
    static CFURLRequestRef willSendRequestCallback(CFURLConnectionRef, CFURLRequestRef, CFURLResponseRef, const void* clientInfo);
    static void didReceiveResponseCallback(CFURLConnectionRef, CFURLResponseRef, const void* clientInfo);
    static void didReceiveDataCallback(CFURLConnectionRef, CFDataRef, CFIndex originalLength, const void* clientInfo);
    static void didFinishLoadingCallback(CFURLConnectionRef, const void* clientInfo);
    static void didFailCallback(CFURLConnectionRef, CFErrorRef, const void* clientInfo);
    static CFCachedURLResponseRef willCacheResponseCallback(CFURLConnectionRef, CFCachedURLResponseRef, const void* clientInfo);
    static void didReceiveChallengeCallback(CFURLConnectionRef, CFURLAuthChallengeRef, const void* clientInfo);
    static void didSendBodyDataCallback(CFURLConnectionRef, CFIndex, CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite, const void *clientInfo);
    static Boolean shouldUseCredentialStorageCallback(CFURLConnectionRef, const void* clientInfo);
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    static Boolean canRespondToProtectionSpaceCallback(CFURLConnectionRef, CFURLProtectionSpaceRef, const void* clientInfo);
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    static void didReceiveDataArrayCallback(CFURLConnectionRef, CFArrayRef, const void* clientInfo);
#endif // USE(NETWORK_CFDATA_ARRAY_CALLBACK)

    virtual CFURLRequestRef willSendRequest(CFURLRequestRef, CFURLResponseRef) = 0;
    virtual void didReceiveResponse(CFURLConnectionRef, CFURLResponseRef) = 0;
    virtual void didReceiveData(CFDataRef, CFIndex originalLength) = 0;
    virtual void didFinishLoading() = 0;
    virtual void didFail(CFErrorRef) = 0;
    virtual CFCachedURLResponseRef willCacheResponse(CFCachedURLResponseRef) = 0;
    virtual void didReceiveChallenge(CFURLAuthChallengeRef) = 0;
    virtual void didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite) = 0;
    virtual Boolean shouldUseCredentialStorage() = 0;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual Boolean canRespondToProtectionSpace(CFURLProtectionSpaceRef) = 0;
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual void didReceiveDataArray(CFArrayRef dataArray) = 0;
#endif // USE(NETWORK_CFDATA_ARRAY_CALLBACK)

protected:
    ResourceHandle* m_handle;
    RetainPtr<CFStringRef> m_originalScheme;
};

} // namespace WebCore.

#endif // USE(CFNETWORK)

#endif // ResourceHandleCFURLConnectionDelegate_h
