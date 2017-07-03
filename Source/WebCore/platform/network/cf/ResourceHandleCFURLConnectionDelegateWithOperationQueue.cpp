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

#include "config.h"
#include "ResourceHandleCFURLConnectionDelegateWithOperationQueue.h"

#if USE(CFURLCONNECTION)

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "CFNetworkSPI.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "WebCoreSystemInterface.h"
#include "WebCoreURLResponse.h"
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

ResourceHandleCFURLConnectionDelegateWithOperationQueue::ResourceHandleCFURLConnectionDelegateWithOperationQueue(ResourceHandle* handle)
    : ResourceHandleCFURLConnectionDelegate(handle)
    , m_queue(dispatch_queue_create("com.apple.WebCore/CFNetwork", DISPATCH_QUEUE_SERIAL))
    , m_semaphore(dispatch_semaphore_create(0))
{
    dispatch_queue_t backgroundQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
    dispatch_set_target_queue(m_queue, backgroundQueue);
}

ResourceHandleCFURLConnectionDelegateWithOperationQueue::~ResourceHandleCFURLConnectionDelegateWithOperationQueue()
{
    dispatch_release(m_semaphore);
    dispatch_release(m_queue);
}

bool ResourceHandleCFURLConnectionDelegateWithOperationQueue::hasHandle() const
{
    return !!m_handle;
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::releaseHandle()
{
    ResourceHandleCFURLConnectionDelegate::releaseHandle();
    m_requestResult = nullptr;
    m_cachedResponseResult = nullptr;
    m_boolResult = false;
    dispatch_semaphore_signal(m_semaphore);
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::setupRequest(CFMutableURLRequestRef request)
{
#if PLATFORM(IOS)
    CFURLRequestSetShouldStartSynchronously(request, 1);
#endif
    CFURLRef requestURL = CFURLRequestGetURL(request);
    if (!requestURL)
        return;
    m_originalScheme = adoptCF(CFURLCopyScheme(requestURL));
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::setupConnectionScheduling(CFURLConnectionRef connection)
{
    CFURLConnectionSetDelegateDispatchQueue(connection, m_queue);
}

CFURLRequestRef ResourceHandleCFURLConnectionDelegateWithOperationQueue::willSendRequest(CFURLRequestRef cfRequest, CFURLResponseRef originalRedirectResponse)
{
    // If the protocols of the new request and the current request match, this is not an HSTS redirect and we don't need to synthesize a redirect response.
    if (!originalRedirectResponse) {
        RetainPtr<CFStringRef> newScheme = adoptCF(CFURLCopyScheme(CFURLRequestGetURL(cfRequest)));
        if (CFStringCompare(newScheme.get(), m_originalScheme.get(), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            CFRetain(cfRequest);
            return cfRequest;
        }
    }

    ASSERT(!isMainThread());
    
    struct ProtectedParameters {
        Ref<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protectedThis;
        RetainPtr<CFURLRequestRef> cfRequest;
        RetainPtr<CFURLResponseRef> originalRedirectResponse;
    };
    
    auto work = [] (void* context) {
        auto& parameters = *reinterpret_cast<ProtectedParameters*>(context);
        auto& protectedThis = parameters.protectedThis;
        auto& handle = protectedThis->m_handle;
        auto& cfRequest = parameters.cfRequest;
        
        if (!protectedThis->hasHandle()) {
            protectedThis->continueWillSendRequest(nullptr);
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::willSendRequest(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        RetainPtr<CFURLResponseRef> redirectResponse = protectedThis->synthesizeRedirectResponseIfNecessary(cfRequest.get(), parameters.originalRedirectResponse.get());
        ASSERT(redirectResponse);

        ResourceRequest request = protectedThis->createResourceRequest(cfRequest.get(), redirectResponse.get());
        handle->willSendRequest(WTFMove(request), redirectResponse.get());
    };
    
    ProtectedParameters parameters { makeRef(*this), cfRequest, originalRedirectResponse };
    dispatch_async_f(dispatch_get_main_queue(), &parameters, work);
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);

    return m_requestResult.leakRef();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveResponse(CFURLConnectionRef connection, CFURLResponseRef cfResponse)
{
    struct ProtectedParameters {
        Ref<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protectedThis;
        RetainPtr<CFURLConnectionRef> connection;
        RetainPtr<CFURLResponseRef> cfResponse;
    };
    
    auto work = [] (void* context) {
        auto& parameters = *reinterpret_cast<ProtectedParameters*>(context);
        auto& protectedThis = parameters.protectedThis;
        auto& handle = protectedThis->m_handle;
        auto& cfResponse = parameters.cfResponse;
        
        if (!protectedThis->hasHandle() || !handle->client()) {
            protectedThis->continueDidReceiveResponse();
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveResponse(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        auto msg = CFURLResponseGetHTTPResponse(cfResponse.get());
        int statusCode = msg ? CFHTTPMessageGetResponseStatusCode(msg) : 0;

        if (statusCode != 304) {
            bool isMainResourceLoad = handle->firstRequest().requester() == ResourceRequest::Requester::Main;
            adjustMIMETypeIfNecessary(cfResponse.get(), isMainResourceLoad);
        }

#if !PLATFORM(IOS)
        if (_CFURLRequestCopyProtocolPropertyForKey(handle->firstRequest().cfURLRequest(DoNotUpdateHTTPBody), CFSTR("ForceHTMLMIMEType")))
            CFURLResponseSetMIMEType(cfResponse.get(), CFSTR("text/html"));
#endif // !PLATFORM(IOS)
        
        ResourceResponse resourceResponse(cfResponse.get());
#if ENABLE(WEB_TIMING)
        ResourceHandle::getConnectionTimingData(parameters.connection.get(), resourceResponse.deprecatedNetworkLoadMetrics());
#endif
        
        handle->didReceiveResponse(WTFMove(resourceResponse));
    };
    
    ProtectedParameters parameters { makeRef(*this), connection, cfResponse };
    dispatch_async_f(dispatch_get_main_queue(), &parameters, work);
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveData(CFDataRef data, CFIndex originalLength)
{
    callOnMainThread([protectedThis = makeRef(*this), data = RetainPtr<CFDataRef>(data), originalLength = originalLength] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client())
            return;
        
        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveData(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didReceiveBuffer(handle, SharedBuffer::create(data.get()), originalLength);
    });
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFinishLoading()
{
    callOnMainThread([protectedThis = makeRef(*this)] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client())
            return;

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFinishLoading(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didFinishLoading(handle);
    });
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFail(CFErrorRef error)
{
    callOnMainThread([protectedThis = makeRef(*this), error = RetainPtr<CFErrorRef>(error)] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client())
            return;
        
        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFail(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didFail(handle, ResourceError(error.get()));
    });
}

CFCachedURLResponseRef ResourceHandleCFURLConnectionDelegateWithOperationQueue::willCacheResponse(CFCachedURLResponseRef cachedResponse)
{
    struct ProtectedParameters {
        Ref<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protectedThis;
        RetainPtr<CFCachedURLResponseRef> cachedResponse;
    };
    
    auto work = [] (void* context) {
        auto& parameters = *reinterpret_cast<ProtectedParameters*>(context);
        auto& protectedThis = parameters.protectedThis;
        auto& handle = protectedThis->m_handle;
        
        if (!protectedThis->hasHandle() || !handle->client()) {
            protectedThis->continueWillCacheResponse(nullptr);
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::willCacheResponse(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->willCacheResponseAsync(handle, parameters.cachedResponse.get());
    };
    
    ProtectedParameters parameters { makeRef(*this), cachedResponse };
    dispatch_async_f(dispatch_get_main_queue(), &parameters, work);
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
    return m_cachedResponseResult.leakRef();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveChallenge(CFURLAuthChallengeRef challenge)
{
    callOnMainThread([protectedThis = makeRef(*this), challenge = RetainPtr<CFURLAuthChallengeRef>(challenge)] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle())
            return;
        
        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveChallenge(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(challenge.get(), handle));
    });
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite)
{
    callOnMainThread([protectedThis = makeRef(*this), totalBytesWritten, totalBytesExpectedToWrite] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client())
            return;

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didSendData(handle, totalBytesWritten, totalBytesExpectedToWrite);
    });
}

Boolean ResourceHandleCFURLConnectionDelegateWithOperationQueue::shouldUseCredentialStorage()
{
    return NO;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
Boolean ResourceHandleCFURLConnectionDelegateWithOperationQueue::canRespondToProtectionSpace(CFURLProtectionSpaceRef protectionSpace)
{
    struct ProtectedParameters {
        Ref<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protectedThis;
        RetainPtr<CFURLProtectionSpaceRef> protectionSpace;
    };
    
    auto work = [] (void* context) {
        auto& parameters = *reinterpret_cast<ProtectedParameters*>(context);
        auto& protectedThis = parameters.protectedThis;
        auto& handle = protectedThis->m_handle;
        
        if (!protectedThis->hasHandle()) {
            protectedThis->continueCanAuthenticateAgainstProtectionSpace(false);
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::canRespondToProtectionSpace(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        ProtectionSpace coreProtectionSpace = ProtectionSpace(parameters.protectionSpace.get());
#if PLATFORM(IOS)
        if (coreProtectionSpace.authenticationScheme() == ProtectionSpaceAuthenticationSchemeUnknown) {
            m_boolResult = false;
            dispatch_semaphore_signal(m_semaphore);
            return;
        }
#endif // PLATFORM(IOS)
        handle->canAuthenticateAgainstProtectionSpace(coreProtectionSpace);
    };
    
    ProtectedParameters parameters { makeRef(*this), protectionSpace };
    dispatch_async_f(dispatch_get_main_queue(), &parameters, work);
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
    return m_boolResult;
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::continueWillSendRequest(CFURLRequestRef request)
{
    m_requestResult = request;
    dispatch_semaphore_signal(m_semaphore);
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::continueDidReceiveResponse()
{
    dispatch_semaphore_signal(m_semaphore);
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::continueWillCacheResponse(CFCachedURLResponseRef response)
{
    m_cachedResponseResult = response;
    dispatch_semaphore_signal(m_semaphore);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void ResourceHandleCFURLConnectionDelegateWithOperationQueue::continueCanAuthenticateAgainstProtectionSpace(bool canAuthenticate)
{
    m_boolResult = canAuthenticate;
    dispatch_semaphore_signal(m_semaphore);
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)
} // namespace WebCore

#endif // USE(CFURLCONNECTION)
