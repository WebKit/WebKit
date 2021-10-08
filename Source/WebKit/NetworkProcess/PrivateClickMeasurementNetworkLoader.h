/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "NetworkLoad.h"
#include "NetworkLoadClient.h"
#include "NetworkLoadParameters.h"
#include <WebCore/TextResourceDecoder.h>
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class NetworkSession;

class PrivateClickMeasurementNetworkLoader final : public NetworkLoadClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Callback = CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&, const RefPtr<JSON::Object>&)>;
    static void start(NetworkSession&, NetworkLoadParameters&&, Callback&&);

    ~PrivateClickMeasurementNetworkLoader();

private:
    PrivateClickMeasurementNetworkLoader(NetworkSession&, NetworkLoadParameters&&, Callback&&);

    // NetworkLoadClient.
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final { }
    bool isSynchronous() const final { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(Ref<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;

    void fail(WebCore::ResourceError&&);
    void cancel();
    void didComplete();

    WeakPtr<NetworkSession> m_session;
    std::unique_ptr<NetworkLoad> m_networkLoad;
    Callback m_completionHandler;

    WebCore::ResourceResponse m_response;
    RefPtr<WebCore::TextResourceDecoder> m_decoder;
    StringBuilder m_jsonString;
};

} // namespace WebKit
