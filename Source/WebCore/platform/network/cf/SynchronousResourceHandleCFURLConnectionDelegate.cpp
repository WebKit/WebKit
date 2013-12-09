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
#include "SynchronousResourceHandleCFURLConnectionDelegate.h"

#if USE(CFNETWORK)

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "FormDataStreamCFNet.h"
#include "Logging.h"
#include "NetworkingContext.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/RetainPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include "WebCoreSystemInterface.h"
#include "WebCoreURLResponse.h"
#endif // PLATFORM(MAC)

#if PLATFORM(WIN)
#include "MIMETypeRegistry.h"
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif // PLATFORM(WIN)

namespace WebCore {

SynchronousResourceHandleCFURLConnectionDelegate::SynchronousResourceHandleCFURLConnectionDelegate(ResourceHandle* handle)
    : ResourceHandleCFURLConnectionDelegate(handle)
{
}

static RetainPtr<CFURLResponseRef> synthesizeRedirectResponseIfNecessary(const ResourceRequest& currentRequest, CFURLRequestRef newRequest, CFURLResponseRef cfRedirectResponse)
{
    if (cfRedirectResponse)
        return cfRedirectResponse;

    CFURLRef newURL = CFURLRequestGetURL(newRequest);
    RetainPtr<CFStringRef> newScheme = adoptCF(CFURLCopyScheme(newURL));

    // If the protocols of the new request and the current request match, this is not an HSTS redirect and we shouldn't synthesize a redirect response.
    if (currentRequest.url().protocol() == String(newScheme.get()))
        return 0;

    RetainPtr<CFURLRef> currentURL = currentRequest.url().createCFURL();
    RetainPtr<CFHTTPMessageRef> responseMessage = adoptCF(CFHTTPMessageCreateResponse(0, 302, 0, kCFHTTPVersion1_1));
    RetainPtr<CFURLRef> newAbsoluteURL = adoptCF(CFURLCopyAbsoluteURL(newURL));
    CFHTTPMessageSetHeaderFieldValue(responseMessage.get(), CFSTR("Location"), CFURLGetString(newAbsoluteURL.get()));
    CFHTTPMessageSetHeaderFieldValue(responseMessage.get(), CFSTR("Cache-Control"), CFSTR("no-store"));

    RetainPtr<CFURLResponseRef> newResponse = adoptCF(CFURLResponseCreateWithHTTPResponse(0, currentURL.get(), responseMessage.get(), kCFURLCacheStorageNotAllowed));
    return newResponse;
}

CFURLRequestRef SynchronousResourceHandleCFURLConnectionDelegate::willSendRequest(CFURLRequestRef cfRequest, CFURLResponseRef originalRedirectResponse)
{
    RetainPtr<CFURLResponseRef> redirectResponse = synthesizeRedirectResponseIfNecessary(m_handle->currentRequest(), cfRequest, originalRedirectResponse);

    if (!redirectResponse) {
        CFRetain(cfRequest);
        return cfRequest;
    }

    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::willSendRequest(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

    ResourceRequest request;
    CFHTTPMessageRef httpMessage = CFURLResponseGetHTTPResponse(redirectResponse.get());
    if (httpMessage && CFHTTPMessageGetResponseStatusCode(httpMessage) == 307) {
        RetainPtr<CFStringRef> lastHTTPMethod = m_handle->lastHTTPMethod().createCFString();
        RetainPtr<CFStringRef> newMethod = adoptCF(CFURLRequestCopyHTTPRequestMethod(cfRequest));
        if (CFStringCompareWithOptions(lastHTTPMethod.get(), newMethod.get(), CFRangeMake(0, CFStringGetLength(lastHTTPMethod.get())), kCFCompareCaseInsensitive)) {
            RetainPtr<CFMutableURLRequestRef> mutableRequest = adoptCF(CFURLRequestCreateMutableCopy(0, cfRequest));
            wkSetRequestStorageSession(m_handle->storageSession(), mutableRequest.get());
            CFURLRequestSetHTTPRequestMethod(mutableRequest.get(), lastHTTPMethod.get());

            FormData* body = m_handle->firstRequest().httpBody();
            if (!equalIgnoringCase(m_handle->firstRequest().httpMethod(), "GET") && body && !body->isEmpty())
                WebCore::setHTTPBody(mutableRequest.get(), body);

            String originalContentType = m_handle->firstRequest().httpContentType();
            if (!originalContentType.isEmpty())
                CFURLRequestSetHTTPHeaderFieldValue(mutableRequest.get(), CFSTR("Content-Type"), originalContentType.createCFString().get());

            request = mutableRequest.get();
        }
    }
    if (request.isNull())
        request = cfRequest;

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https") && m_handle->context()->shouldClearReferrerOnHTTPSToHTTPRedirect())
        request.clearHTTPReferrer();

    m_handle->willSendRequest(request, redirectResponse.get());

    if (request.isNull())
        return 0;

    cfRequest = request.cfURLRequest(UpdateHTTPBody);

    CFRetain(cfRequest);
    return cfRequest;
}

#if !PLATFORM(MAC)
static void setDefaultMIMEType(CFURLResponseRef response)
{
    static CFStringRef defaultMIMETypeString = defaultMIMEType().createCFString().leakRef();
    
    CFURLResponseSetMIMEType(response, defaultMIMETypeString);
}
#endif // !PLATFORM(MAC)

void SynchronousResourceHandleCFURLConnectionDelegate::didReceiveResponse(CFURLResponseRef cfResponse)
{
    LOG(Network, "CFNet - SynchronousResourceHandleCFURLConnectionDelegate::didReceiveResponse(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

    if (!m_handle->client())
        return;

#if PLATFORM(MAC)
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

} // namespace WebCore.

#endif
