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
#include "SynchronousResourceHandleCFURLConnectionDelegate.h"

#if USE(CFNETWORK)

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "LoaderRunLoopCF.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/RetainPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "WebCoreSystemInterface.h"
#include "WebCoreURLResponse.h"
#endif // PLATFORM(COCOA)

#if PLATFORM(IOS)
#include "WebCoreThread.h"
#endif // PLATFORM(IOS)

#if PLATFORM(WIN)
#include "MIMETypeRegistry.h"
#endif // PLATFORM(WIN)

namespace WebCore {

SynchronousResourceHandleCFURLConnectionDelegate::SynchronousResourceHandleCFURLConnectionDelegate(ResourceHandle* handle)
    : ResourceHandleCFURLConnectionDelegate(handle)
{
}

void SynchronousResourceHandleCFURLConnectionDelegate::setupRequest(CFMutableURLRequestRef request)
{
#if PLATFORM(IOS)
    CFURLRequestSetShouldStartSynchronously(request, 1);
#endif
}

void SynchronousResourceHandleCFURLConnectionDelegate::setupConnectionScheduling(CFURLConnectionRef connection)
{
#if PLATFORM(WIN)
    CFURLConnectionScheduleWithCurrentMessageQueue(connection);
#elif PLATFORM(IOS)
    CFURLConnectionScheduleWithRunLoop(connection, WebThreadRunLoop(), kCFRunLoopDefaultMode);
#else
    CFURLConnectionScheduleWithRunLoop(connection, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
#endif
    CFURLConnectionScheduleDownloadWithRunLoop(connection, loaderRunLoop(), kCFRunLoopDefaultMode);
}

CFURLRequestRef SynchronousResourceHandleCFURLConnectionDelegate::willSendRequest(CFURLRequestRef cfRequest, CFURLResponseRef originalRedirectResponse)
{
    RetainPtr<CFURLResponseRef> redirectResponse = synthesizeRedirectResponseIfNecessary(cfRequest, originalRedirectResponse);

    if (!redirectResponse) {
        CFRetain(cfRequest);
        return cfRequest;
    }

    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::willSendRequest(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

    ResourceRequest request = createResourceRequest(cfRequest, redirectResponse.get());
    m_handle->willSendRequest(request, redirectResponse.get());

    if (request.isNull())
        return 0;

    cfRequest = request.cfURLRequest(UpdateHTTPBody);

    CFRetain(cfRequest);
    return cfRequest;
}

#if !PLATFORM(COCOA)
static void setDefaultMIMEType(CFURLResponseRef response)
{
    static CFStringRef defaultMIMETypeString = defaultMIMEType().createCFString().leakRef();
    
    CFURLResponseSetMIMEType(response, defaultMIMETypeString);
}
#endif // !PLATFORM(COCOA)

void SynchronousResourceHandleCFURLConnectionDelegate::didReceiveResponse(CFURLResponseRef cfResponse)
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didReceiveResponse(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

    if (!m_handle->client())
        return;

#if PLATFORM(COCOA)
    // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
    CFHTTPMessageRef msg = wkGetCFURLResponseHTTPResponse(cfResponse);
    int statusCode = msg ? CFHTTPMessageGetResponseStatusCode(msg) : 0;

    if (statusCode != 304)
        adjustMIMETypeIfNecessary(cfResponse);

#if !PLATFORM(IOS)
    if (_CFURLRequestCopyProtocolPropertyForKey(m_handle->firstRequest().cfURLRequest(DoNotUpdateHTTPBody), CFSTR("ForceHTMLMIMEType")))
        wkSetCFURLResponseMIMEType(cfResponse, CFSTR("text/html"));
#endif // !PLATFORM(IOS)
#else
    if (!CFURLResponseGetMIMEType(cfResponse)) {
        // We should never be applying the default MIMEType if we told the networking layer to do content sniffing for handle.
        ASSERT(!m_handle->shouldContentSniff());
        setDefaultMIMEType(cfResponse);
    }
#endif

#if USE(QUICK_LOOK)
    m_handle->setQuickLookHandle(QuickLookHandle::create(m_handle, this, cfResponse));
    if (m_handle->quickLookHandle())
        cfResponse = m_handle->quickLookHandle()->cfResponse();
#endif

    m_handle->client()->didReceiveResponse(m_handle, cfResponse);
}

void SynchronousResourceHandleCFURLConnectionDelegate::didReceiveData(CFDataRef data, CFIndex originalLength)
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didReceiveData(handle=%p, bytes=%ld) (%s)", m_handle, CFDataGetLength(data), m_handle->firstRequest().url().string().utf8().data());

#if USE(QUICK_LOOK)
    if (m_handle->quickLookHandle() && m_handle->quickLookHandle()->didReceiveData(data))
        return;
#endif

    if (ResourceHandleClient* client = m_handle->client())
        client->didReceiveBuffer(m_handle, SharedBuffer::wrapCFData(data), originalLength);
}

void SynchronousResourceHandleCFURLConnectionDelegate::didFinishLoading()
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didFinishLoading(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

#if USE(QUICK_LOOK)
    if (m_handle->quickLookHandle() && m_handle->quickLookHandle()->didFinishLoading())
        return;
#endif

    if (ResourceHandleClient* client = m_handle->client())
        client->didFinishLoading(m_handle, 0);
}

void SynchronousResourceHandleCFURLConnectionDelegate::didFail(CFErrorRef error)
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didFail(handle=%p, error = %p) (%s)", m_handle, error, m_handle->firstRequest().url().string().utf8().data());

#if USE(QUICK_LOOK)
    if (QuickLookHandle* quickLookHandle = m_handle->quickLookHandle())
        quickLookHandle->didFail();
#endif

    if (ResourceHandleClient* client = m_handle->client())
        client->didFail(m_handle, ResourceError(error));
}

CFCachedURLResponseRef SynchronousResourceHandleCFURLConnectionDelegate::willCacheResponse(CFCachedURLResponseRef cachedResponse)
{
#if PLATFORM(WIN)
    // Workaround for <rdar://problem/6300990> Caching does not respect Vary HTTP header.
    // FIXME: WebCore cache has issues with Vary, too (bug 58797, bug 71509).
    CFURLResponseRef wrappedResponse = CFCachedURLResponseGetWrappedResponse(cachedResponse);
    if (CFHTTPMessageRef httpResponse = CFURLResponseGetHTTPResponse(wrappedResponse)) {
        ASSERT(CFHTTPMessageIsHeaderComplete(httpResponse));
        RetainPtr<CFStringRef> varyValue = adoptCF(CFHTTPMessageCopyHeaderFieldValue(httpResponse, CFSTR("Vary")));
        if (varyValue)
            return 0;
    }
#endif // PLATFORM(WIN)

#if PLATFORM(WIN)
    if (m_handle->client() && !m_handle->client()->shouldCacheResponse(m_handle, cachedResponse))
        return 0;
#else
    CFCachedURLResponseRef newResponse = m_handle->client()->willCacheResponse(m_handle, cachedResponse);
    if (newResponse != cachedResponse)
        return newResponse;
#endif

    CFRetain(cachedResponse);
    return cachedResponse;
}

void SynchronousResourceHandleCFURLConnectionDelegate::didReceiveChallenge(CFURLAuthChallengeRef challenge)
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didReceiveChallenge(handle=%p (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

    m_handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(challenge, m_handle));
}

void SynchronousResourceHandleCFURLConnectionDelegate::didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite)
{
    if (!m_handle || !m_handle->client())
        return;
    m_handle->client()->didSendData(m_handle, totalBytesWritten, totalBytesExpectedToWrite);
}

Boolean SynchronousResourceHandleCFURLConnectionDelegate::shouldUseCredentialStorage()
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::shouldUseCredentialStorage(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

    if (!m_handle)
        return false;

    return m_handle->shouldUseCredentialStorage();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
Boolean SynchronousResourceHandleCFURLConnectionDelegate::canRespondToProtectionSpace(CFURLProtectionSpaceRef protectionSpace)
{
    ASSERT(m_handle);

    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::canRespondToProtectionSpace(handle=%p (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

#if PLATFORM(IOS)
    ProtectionSpace coreProtectionSpace = core(protectionSpace);
    if (coreProtectionSpace.authenticationScheme() == ProtectionSpaceAuthenticationSchemeUnknown)
        return false;
    return m_handle->canAuthenticateAgainstProtectionSpace(coreProtectionSpace);
#else
    return m_handle->canAuthenticateAgainstProtectionSpace(core(protectionSpace));
#endif
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
void SynchronousResourceHandleCFURLConnectionDelegate::didReceiveDataArray(CFArrayRef dataArray)
{
    if (!m_handle->client())
        return;

    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didReceiveDataArray(handle=%p, arrayLength=%ld) (%s)", m_handle, CFArrayGetCount(dataArray), m_handle->firstRequest().url().string().utf8().data());

#if USE(QUICK_LOOK)
    if (m_handle->quickLookHandle() && m_handle->quickLookHandle()->didReceiveDataArray(dataArray))
        return;
#endif

    m_handle->handleDataArray(dataArray);
}
#endif // USE(NETWORK_CFDATA_ARRAY_CALLBACK)

void SynchronousResourceHandleCFURLConnectionDelegate::continueWillSendRequest(CFURLRequestRef)
{
    ASSERT_NOT_REACHED();
}

void SynchronousResourceHandleCFURLConnectionDelegate::continueDidReceiveResponse()
{
    ASSERT_NOT_REACHED();
}

void SynchronousResourceHandleCFURLConnectionDelegate::continueShouldUseCredentialStorage(bool)
{
    ASSERT_NOT_REACHED();
}

void SynchronousResourceHandleCFURLConnectionDelegate::continueWillCacheResponse(CFCachedURLResponseRef)
{
    ASSERT_NOT_REACHED();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void SynchronousResourceHandleCFURLConnectionDelegate::continueCanAuthenticateAgainstProtectionSpace(bool)
{
    ASSERT_NOT_REACHED();
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

} // namespace WebCore.

#endif
