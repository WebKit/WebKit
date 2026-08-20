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

#pragma once

#include "NetworkLoadClient.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/Timer.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class NetworkLoad;
class NetworkProcess;
class NetworkSession;

struct NetworkResourceLoadParameters;

class QualifiedServerTrustFetch final : public RefCounted<QualifiedServerTrustFetch>, public NetworkLoadClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(QualifiedServerTrustFetch);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(QualifiedServerTrustFetch);
public:
    template <typename... Args> static void create(Args&&... args)
    {
        keepAliveUntilCompletion(adoptRef(*new QualifiedServerTrustFetch(std::forward<Args>(args)...)));
    }
    ~QualifiedServerTrustFetch();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    QualifiedServerTrustFetch(NetworkSession&, const URL&, const NetworkResourceLoadParameters&, const WebCore::CertificateInfo& serverTrust);

    static void keepAliveUntilCompletion(Ref<QualifiedServerTrustFetch>&&);
    static void didReachCompletion(QualifiedServerTrustFetch&);

    void cancel();

    // NetworkLoadClient.
    bool isSynchronous() const final { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) final;
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;

    const Ref<NetworkProcess> m_networkProcess;
    const Ref<NetworkLoad> m_networkLoad;
    const std::unique_ptr<WebCore::Timer> m_timeoutTimer;
    const WebPageProxyIdentifier m_webPageID;
    const bool m_debugEnabledForTesting { false };
    const WebCore::CertificateInfo m_serverTrust;
    RefPtr<QualifiedServerTrustFetch> m_selfReference;
    WebCore::SharedBufferBuilder m_buffer;
};

} // namespace WebKit
