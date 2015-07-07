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

#if USE(CFNETWORK)

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
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

    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!protector->hasHandle()) {
            continueWillSendRequest(nullptr);
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::willSendRequest(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        RetainPtr<CFURLResponseRef> redirectResponse = synthesizeRedirectResponseIfNecessary(cfRequest, originalRedirectResponse);
        ASSERT(redirectResponse);

        ResourceRequest request = createResourceRequest(cfRequest, redirectResponse.get());
        m_handle->willSendRequest(request, redirectResponse.get());
    });

    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);

    return m_requestResult.leakRef();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveResponse(CFURLConnectionRef connection, CFURLResponseRef cfResponse)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!protector->hasHandle()) {
            continueDidReceiveResponse();
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveResponse(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        CFHTTPMessageRef msg = wkGetCFURLResponseHTTPResponse(cfResponse);
        int statusCode = msg ? CFHTTPMessageGetResponseStatusCode(msg) : 0;

        if (statusCode != 304)
            adjustMIMETypeIfNecessary(cfResponse);

#if !PLATFORM(IOS)
        if (_CFURLRequestCopyProtocolPropertyForKey(m_handle->firstRequest().cfURLRequest(DoNotUpdateHTTPBody), CFSTR("ForceHTMLMIMEType")))
            wkSetCFURLResponseMIMEType(cfResponse, CFSTR("text/html"));
#endif // !PLATFORM(IOS)
        
        ResourceResponse resourceResponse(cfResponse);
#if ENABLE(WEB_TIMING)
        ResourceHandle::getConnectionTimingData(connection, resourceResponse.resourceLoadTiming());
#else
        UNUSED_PARAM(connection);
#endif
        
        m_handle->client()->didReceiveResponseAsync(m_handle, resourceResponse);
    });
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveData(CFDataRef data, CFIndex originalLength)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);
    CFRetain(data);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (protector->hasHandle()) {
            LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveData(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

            m_handle->client()->didReceiveBuffer(m_handle, SharedBuffer::wrapCFData(data), originalLength);
        }

        CFRelease(data);
    });
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFinishLoading()
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!protector->hasHandle())
            return;

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFinishLoading(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        m_handle->client()->didFinishLoading(m_handle, 0);
    });
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFail(CFErrorRef error)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);
    CFRetain(error);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (protector->hasHandle()) {
            LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFail(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

            m_handle->client()->didFail(m_handle, ResourceError(error));
        }

        CFRelease(error);
    });
}

CFCachedURLResponseRef ResourceHandleCFURLConnectionDelegateWithOperationQueue::willCacheResponse(CFCachedURLResponseRef cachedResponse)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!protector->hasHandle()) {
            continueWillCacheResponse(nullptr);
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::willCacheResponse(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        m_handle->client()->willCacheResponseAsync(m_handle, cachedResponse);
    });
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
    return m_cachedResponseResult.leakRef();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveChallenge(CFURLAuthChallengeRef challenge)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);
    CFRetain(challenge);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (protector->hasHandle()) {
            LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveChallenge(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

            m_handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(challenge, m_handle));
        }

        CFRelease(challenge);
    });
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!protector->hasHandle())
            return;

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        m_handle->client()->didSendData(m_handle, totalBytesWritten, totalBytesExpectedToWrite);
    });
}

Boolean ResourceHandleCFURLConnectionDelegateWithOperationQueue::shouldUseCredentialStorage()
{
    return NO;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
Boolean ResourceHandleCFURLConnectionDelegateWithOperationQueue::canRespondToProtectionSpace(CFURLProtectionSpaceRef protectionSpace)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!protector->hasHandle()) {
            continueCanAuthenticateAgainstProtectionSpace(false);
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::canRespondToProtectionSpace(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        ProtectionSpace coreProtectionSpace = ProtectionSpace(protectionSpace);
#if PLATFORM(IOS)
        if (coreProtectionSpace.authenticationScheme() == ProtectionSpaceAuthenticationSchemeUnknown) {
            m_boolResult = false;
            dispatch_semaphore_signal(m_semaphore);
            return;
        }
#endif // PLATFORM(IOS)
        m_handle->canAuthenticateAgainstProtectionSpace(coreProtectionSpace);
    });
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
    return m_boolResult;
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveDataArray(CFArrayRef dataArray)
{
    RefPtr<ResourceHandleCFURLConnectionDelegateWithOperationQueue> protector(this);
    CFRetain(dataArray);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (protector->hasHandle()) {
            LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

            m_handle->client()->didReceiveBuffer(m_handle, SharedBuffer::wrapCFDataArray(dataArray), -1);
        }
        CFRelease(dataArray);
    });
}
#endif // USE(NETWORK_CFDATA_ARRAY_CALLBACK)

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

#endif // USE(CFNETWORK)
