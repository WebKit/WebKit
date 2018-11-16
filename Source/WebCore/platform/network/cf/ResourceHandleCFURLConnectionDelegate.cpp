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

#include "config.h"
#include "ResourceHandleCFURLConnectionDelegate.h"

#if USE(CFURLCONNECTION)

#include "FormDataStreamCFNet.h"
#include "NetworkingContext.h"
#include "ResourceHandle.h"
#include <pal/spi/cf/CFNetworkSPI.h>

namespace WebCore {

ResourceHandleCFURLConnectionDelegate::ResourceHandleCFURLConnectionDelegate(ResourceHandle* handle)
    : m_handle(handle)
{
}

ResourceHandleCFURLConnectionDelegate::~ResourceHandleCFURLConnectionDelegate() = default;

void ResourceHandleCFURLConnectionDelegate::releaseHandle()
{
    m_handle = nullptr;
}

const void* ResourceHandleCFURLConnectionDelegate::retain(const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->ref();
    return clientInfo;
}

void ResourceHandleCFURLConnectionDelegate::release(const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->deref();
}

CFURLRequestRef ResourceHandleCFURLConnectionDelegate::willSendRequestCallback(CFURLConnectionRef, CFURLRequestRef cfRequest, CFURLResponseRef originalRedirectResponse, const void* clientInfo)
{
    return static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->willSendRequest(cfRequest, originalRedirectResponse);
}

void ResourceHandleCFURLConnectionDelegate::didReceiveResponseCallback(CFURLConnectionRef connection, CFURLResponseRef cfResponse, const void* clientInfo)
{
    static_cast<ResourceHandleCFURLConnectionDelegate*>(const_cast<void*>(clientInfo))->didReceiveResponse(connection, cfResponse);
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

RetainPtr<CFURLResponseRef> ResourceHandleCFURLConnectionDelegate::synthesizeRedirectResponseIfNecessary(CFURLRequestRef newRequest, CFURLResponseRef cfRedirectResponse)
{
    if (cfRedirectResponse)
        return cfRedirectResponse;

    CFURLRef newURL = CFURLRequestGetURL(newRequest);
    RetainPtr<CFStringRef> newScheme = adoptCF(CFURLCopyScheme(newURL));

    // If the protocols of the new request and the current request match, this is not an HSTS redirect and we shouldn't synthesize a redirect response.
    const ResourceRequest& currentRequest = m_handle->currentRequest();
    if (currentRequest.url().protocol() == String(newScheme.get()))
        return nullptr;

    RetainPtr<CFURLRef> currentURL = currentRequest.url().createCFURL();
    RetainPtr<CFHTTPMessageRef> responseMessage = adoptCF(CFHTTPMessageCreateResponse(0, 302, 0, kCFHTTPVersion1_1));
    RetainPtr<CFURLRef> newAbsoluteURL = adoptCF(CFURLCopyAbsoluteURL(newURL));
    CFHTTPMessageSetHeaderFieldValue(responseMessage.get(), CFSTR("Location"), CFURLGetString(newAbsoluteURL.get()));
    CFHTTPMessageSetHeaderFieldValue(responseMessage.get(), CFSTR("Cache-Control"), CFSTR("no-store"));

    RetainPtr<CFURLResponseRef> newResponse = adoptCF(CFURLResponseCreateWithHTTPResponse(0, currentURL.get(), responseMessage.get(), kCFURLCacheStorageNotAllowed));
    return newResponse;
}

ResourceRequest ResourceHandleCFURLConnectionDelegate::createResourceRequest(CFURLRequestRef cfRequest, CFURLResponseRef redirectResponse)
{
    ResourceRequest request;
    CFHTTPMessageRef httpMessage = CFURLResponseGetHTTPResponse(redirectResponse);
    if (httpMessage && CFHTTPMessageGetResponseStatusCode(httpMessage) == 307) {
        RetainPtr<CFStringRef> lastHTTPMethod = m_handle->lastHTTPMethod().createCFString();
        RetainPtr<CFStringRef> newMethod = adoptCF(CFURLRequestCopyHTTPRequestMethod(cfRequest));
        if (CFStringCompareWithOptions(lastHTTPMethod.get(), newMethod.get(), CFRangeMake(0, CFStringGetLength(lastHTTPMethod.get())), kCFCompareCaseInsensitive)) {
            auto mutableRequest = adoptCF(CFURLRequestCreateMutableCopy(kCFAllocatorDefault, cfRequest));
            if (auto storageSession = m_handle->storageSession())
                _CFURLRequestSetStorageSession(mutableRequest.get(), storageSession);
            CFURLRequestSetHTTPRequestMethod(mutableRequest.get(), lastHTTPMethod.get());

            FormData* body = m_handle->firstRequest().httpBody();
            if (!equalLettersIgnoringASCIICase(m_handle->firstRequest().httpMethod(), "get") && body && !body->isEmpty())
                WebCore::setHTTPBody(mutableRequest.get(), body);

            String originalContentType = m_handle->firstRequest().httpContentType();
            if (!originalContentType.isEmpty())
                CFURLRequestSetHTTPHeaderFieldValue(mutableRequest.get(), CFSTR("Content-Type"), originalContentType.createCFString().get());

            request = mutableRequest.get();
        }
    }

    if (request.isNull())
        request = cfRequest;

    if (!request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https") && m_handle->context()->shouldClearReferrerOnHTTPSToHTTPRedirect())
        request.clearHTTPReferrer();
    return request;
}

CFURLConnectionClient_V6 ResourceHandleCFURLConnectionDelegate::makeConnectionClient() const
{
    CFURLConnectionClient_V6 client = { 6, this,
        &ResourceHandleCFURLConnectionDelegate::retain,
        &ResourceHandleCFURLConnectionDelegate::release,
        0, // copyDescription
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
        0
    };
    return client;
}

} // namespace WebCore.

#endif // USE(CFURLCONNECTION)
