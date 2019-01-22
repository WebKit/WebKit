/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
 * Copyright (C) 2017 NAVER Corp.
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
#include "CurlResourceHandleDelegate.h"

#include "CurlCacheManager.h"
#include "CurlRequest.h"
#include "NetworkStorageSession.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"

#include <wtf/CompletionHandler.h>

#if USE(CURL)

namespace WebCore {

CurlResourceHandleDelegate::CurlResourceHandleDelegate(ResourceHandle& handle)
    : m_handle(handle)
{

}

void CurlResourceHandleDelegate::ref()
{
    m_handle.ref();
}

void CurlResourceHandleDelegate::deref()
{
    m_handle.deref();
}

bool CurlResourceHandleDelegate::cancelledOrClientless()
{
    return m_handle.cancelledOrClientless();
}

ResourceHandleClient* CurlResourceHandleDelegate::client() const
{
    return m_handle.client();
}

ResourceHandleInternal* CurlResourceHandleDelegate::d()
{
    return m_handle.getInternal();
}

void CurlResourceHandleDelegate::curlDidSendData(CurlRequest& request, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    UNUSED_PARAM(request);
    ASSERT(isMainThread());

    if (cancelledOrClientless())
        return;

    client()->didSendData(&m_handle, bytesSent, totalBytesToBeSent);
}

static void handleCookieHeaders(const CurlResponse& response)
{
    static const auto setCookieHeader = "set-cookie: ";

    const auto& storageSession = *d()->m_context->storageSession(PAL::SessionID::defaultSessionID());
    const auto& cookieJar = storageSession.cookieStorage();
    for (const auto& header : response.headers) {
        if (header.startsWithIgnoringASCIICase(setCookieHeader)) {
            const auto contents = header.right(header.length() - strlen(setCookieHeader));
            cookieJar.setCookiesFromHTTPResponse(storageSession, response.url, contents);
        }
    }
}

void CurlResourceHandleDelegate::curlDidReceiveResponse(CurlRequest& request, const CurlResponse& receivedResponse)
{
    ASSERT(isMainThread());
    ASSERT(!d()->m_defersLoading);

    if (cancelledOrClientless())
        return;

    m_response = ResourceResponse(receivedResponse);

    m_response.setCertificateInfo(request.certificateInfo().isolatedCopy());
    m_response.setDeprecatedNetworkLoadMetrics(request.networkLoadMetrics().isolatedCopy());

    handleCookieHeaders(receivedResponse);

    if (m_response.shouldRedirect()) {
        m_handle.willSendRequest();
        return;
    }

    if (m_response.isUnauthorized() && receivedResponse.availableHttpAuth) {
        AuthenticationChallenge challenge(receivedResponse, d()->m_authFailureCount, m_response, &m_handle);
        m_handle.didReceiveAuthenticationChallenge(challenge);
        d()->m_authFailureCount++;
        return;
    }

    if (m_response.isNotModified()) {
        URL cacheUrl = m_response.url();
        cacheUrl.removeFragmentIdentifier();

        if (CurlCacheManager::singleton().getCachedResponse(cacheUrl, m_response)) {
            if (d()->m_addedCacheValidationHeaders) {
                m_response.setHTTPStatusCode(200);
                m_response.setHTTPStatusText("OK");
            }
        }
    }

    CurlCacheManager::singleton().didReceiveResponse(m_handle, m_response);

    m_handle.didReceiveResponse(ResourceResponse(m_response), [this, protectedHandle = makeRef(m_handle)] {
        m_handle.continueAfterDidReceiveResponse();
    });
}

void CurlResourceHandleDelegate::curlDidReceiveBuffer(CurlRequest& request, Ref<SharedBuffer>&& buffer)
{
    UNUSED_PARAM(request);
    ASSERT(isMainThread());

    if (cancelledOrClientless())
        return;

    CurlCacheManager::singleton().didReceiveData(m_handle, buffer->data(), buffer->size());
    client()->didReceiveBuffer(&m_handle, WTFMove(buffer), buffer->size());
}

void CurlResourceHandleDelegate::curlDidComplete(CurlRequest& request)
{
    ASSERT(isMainThread());

    if (cancelledOrClientless())
        return;

    m_response.setDeprecatedNetworkLoadMetrics(request.networkLoadMetrics().isolatedCopy());

    CurlCacheManager::singleton().didFinishLoading(m_handle);
    client()->didFinishLoading(&m_handle);
}

void CurlResourceHandleDelegate::curlDidFailWithError(CurlRequest& request, const ResourceError& resourceError)
{
    UNUSED_PARAM(request);
    ASSERT(isMainThread());

    if (cancelledOrClientless())
        return;

    CurlCacheManager::singleton().didFail(m_handle);
    client()->didFail(&m_handle, resourceError);
}


} // namespace WebCore

#endif
