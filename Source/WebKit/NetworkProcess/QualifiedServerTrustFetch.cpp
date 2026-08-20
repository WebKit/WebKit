/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "QualifiedServerTrustFetch.h"

#include "NetworkLoad.h"
#include "NetworkLoadParameters.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoadParameters.h"

#if PLATFORM(COCOA)
#import "SecuritySoftLink.h"
#endif

namespace WebKit {

using namespace WebCore;

static HashMap<WebPageProxyIdentifier, Ref<QualifiedServerTrustFetch>>& globalQualifiedServerTrustFetchMap()
{
    static NeverDestroyed<HashMap<WebPageProxyIdentifier, Ref<QualifiedServerTrustFetch>>> map;
    return map.get();
}

void QualifiedServerTrustFetch::keepAliveUntilCompletion(Ref<QualifiedServerTrustFetch>&& fetch)
{
    auto webPageID = fetch->m_webPageID;
    if (RefPtr existingFetchForPage = globalQualifiedServerTrustFetchMap().get(webPageID))
        existingFetchForPage->cancel();
    ASSERT(!globalQualifiedServerTrustFetchMap().contains(webPageID));
    globalQualifiedServerTrustFetchMap().set(webPageID, WTF::move(fetch));
}

void QualifiedServerTrustFetch::didReachCompletion(QualifiedServerTrustFetch& fetch)
{
    ASSERT(globalQualifiedServerTrustFetchMap().contains(fetch.m_webPageID));
    globalQualifiedServerTrustFetchMap().remove(fetch.m_webPageID);
}

static NetworkLoadParameters qualifiedServerTrustParameters(URL&& url, const NetworkResourceLoadParameters& parameters)
{
    WebCore::ResourceRequest request(WTF::move(url));
    request.setPriority(ResourceLoadPriority::VeryLow);
    return NetworkLoadParameters {
        parameters.webPageProxyID,
        parameters.webPageID,
        parameters.webFrameID,
        parameters.topOrigin,
        parameters.sourceOrigin,
        parameters.parentPID,
        WTF::move(request)
    };
}

QualifiedServerTrustFetch::QualifiedServerTrustFetch(NetworkSession& session, const URL& url, const NetworkResourceLoadParameters& parameters, const WebCore::CertificateInfo& serverTrust)
    : m_networkProcess(session.networkProcess())
    , m_networkLoad(NetworkLoad::create(*this, qualifiedServerTrustParameters(URL(url), parameters), session))
    , m_timeoutTimer(makeUnique<WebCore::Timer>(*this, &QualifiedServerTrustFetch::cancel))
    , m_webPageID(parameters.webPageProxyID)
    , m_debugEnabledForTesting(session.qualifiedServerTrustDebugEnabledForTesting())
    , m_serverTrust(serverTrust)
{
    m_networkLoad->start();
    m_timeoutTimer->startOneShot(60_s);
}

QualifiedServerTrustFetch::~QualifiedServerTrustFetch() = default;

void QualifiedServerTrustFetch::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    // No redirection will be followed.
    completionHandler({ });
}

void QualifiedServerTrustFetch::didReceiveResponse(ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    completionHandler(PolicyAction::Use);
}

void QualifiedServerTrustFetch::didReceiveBuffer(const FragmentedSharedBuffer& buffer)
{
    constexpr size_t maxBufferSize = 10 * MB;
    if (m_buffer.size() > maxBufferSize)
        return cancel();

    m_buffer.append(buffer);
}

void QualifiedServerTrustFetch::didFinishLoading(const NetworkLoadMetrics&)
{
    WebCore::CertificateInfo qualifiedServerTrust;
    if (m_debugEnabledForTesting) {
        // FIXME: Once implementation of SecQWACTLSBindingVerify is available,
        // use SecTrustSetAnchorCertificates to evaluate without verifying the root trust
        // to get a real 2-QWAC CertificateInfo instead of reusing the TLS trust.
        qualifiedServerTrust = m_serverTrust;
    }
#if PLATFORM(COCOA)
    else if (canLoad_Security_SecQWACTLSBindingVerify()) {
        SUPPRESS_UNRETAINED_LOCAL SecTrustRef trust { nullptr };
        bool success = softLink_Security_SecQWACTLSBindingVerify(m_buffer.takeBuffer()->makeContiguous()->createCFData().get(), m_serverTrust.trust(), &trust, nullptr);
        if (trust) {
            ASSERT_UNUSED(success, success);
            SUPPRESS_RETAINPTR_CTOR_ADOPT qualifiedServerTrust = WebCore::CertificateInfo(adoptCF(trust));
        }
    }
#endif

    if (RefPtr connection = m_networkProcess->parentProcessConnection())
        connection->send(Messages::NetworkProcessProxy::ReceivedQualifiedServerTrust(m_webPageID, m_serverTrust, qualifiedServerTrust), 0);
    didReachCompletion(*this);
}

void QualifiedServerTrustFetch::didFailLoading(const ResourceError&)
{
    didReachCompletion(*this);
}

void QualifiedServerTrustFetch::cancel()
{
    m_networkLoad->cancel();
    didFailLoading({ });
}

void QualifiedServerTrustFetch::didSendData(uint64_t, uint64_t)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebKit
