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

#include "config.h"
#include "ResourceHandleCFURLConnectionDelegate.h"

#if USE(CFNETWORK)

namespace WebCore {

ResourceHandleCFURLConnectionDelegate::ResourceHandleCFURLConnectionDelegate(ResourceHandle* handle)
    : m_handle(handle)
{
}

ResourceHandleCFURLConnectionDelegate::~ResourceHandleCFURLConnectionDelegate()
{
}

CFURLRequestRef ResourceHandleCFURLConnectionDelegate::willSendRequestCallback(CFURLConnectionRef, CFURLRequestRef cfRequest, CFURLResponseRef originalRedirectResponse, const void* clientInfo)
{
    return static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->willSendRequest(cfRequest, originalRedirectResponse);
}

void ResourceHandleCFURLConnectionDelegate::didReceiveResponseCallback(CFURLConnectionRef, CFURLResponseRef cfResponse, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didReceiveResponse(cfResponse);
}

void ResourceHandleCFURLConnectionDelegate::didReceiveDataCallback(CFURLConnectionRef, CFDataRef data, CFIndex originalLength, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didReceiveData(data, originalLength);
}

void ResourceHandleCFURLConnectionDelegate::didFinishLoadingCallback(CFURLConnectionRef, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didFinishLoading();
}

void ResourceHandleCFURLConnectionDelegate::didFailCallback(CFURLConnectionRef, CFErrorRef error, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didFail(error);
}

CFCachedURLResponseRef ResourceHandleCFURLConnectionDelegate::willCacheResponseCallback(CFURLConnectionRef, CFCachedURLResponseRef cachedResponse, const void* clientInfo)
{
    return static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->willCacheResponse(cachedResponse);
}

void ResourceHandleCFURLConnectionDelegate::didReceiveChallengeCallback(CFURLConnectionRef, CFURLAuthChallengeRef challenge, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didReceiveChallenge(challenge);
}

void ResourceHandleCFURLConnectionDelegate::didSendBodyDataCallback(CFURLConnectionRef, CFIndex, CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite, const void *clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didSendBodyData(totalBytesWritten, totalBytesExpectedToWrite);
}

Boolean ResourceHandleCFURLConnectionDelegate::shouldUseCredentialStorageCallback(CFURLConnectionRef, const void* clientInfo)
{
    return static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->shouldUseCredentialStorage();

}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
Boolean ResourceHandleCFURLConnectionDelegate::canRespondToProtectionSpaceCallback(CFURLConnectionRef, CFURLProtectionSpaceRef protectionSpace, const void* clientInfo)
{
    return static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->canRespondToProtectionSpace(protectionSpace);
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
void ResourceHandleCFURLConnectionDelegate::didReceiveDataArrayCallback(CFURLConnectionRef, CFArrayRef dataArray, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didReceiveDataArray(dataArray);
}
#endif // USE(NETWORK_CFDATA_ARRAY_CALLBACK)

CFURLConnectionClient_V6 ResourceHandleCFURLConnectionDelegate::makeConnectionClient() const
{
    CFURLConnectionClient_V6 client = { 6, this, 0, 0, 0,
        &ResourceHandleCFURLConnectionDelegate::willSendRequestCallback,
        &ResourceHandleCFURLConnectionDelegate::didReceiveResponseCallback,
        &ResourceHandleCFURLConnectionDelegate::didReceiveDataCallback,
        0,
        &ResourceHandleCFURLConnectionDelegate::didFinishLoadingCallback,
        &ResourceHandleCFURLConnectionDelegate::didFailCallback,
        &ResourceHandleCFURLConnectionDelegate::willCacheResponseCallback,
        &ResourceHandleCFURLConnectionDelegate::didReceiveChallengeCallback,
        &ResourceHandleCFURLConnectionDelegate::didSendBodyDataCallback,
        &ResourceHandleCFURLConnectionDelegate::shouldUseCredentialStorageCallback,
        0,
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
        &ResourceHandleCFURLConnectionDelegate::canRespondToProtectionSpaceCallback,
#else
        0,
#endif
        0,
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
        &ResourceHandleCFURLConnectionDelegate::didReceiveDataArrayCallback
#else
        0
#endif
    };
    return client;
}

} // namespace WebCore.

#endif // USE(CFNETWORK)
