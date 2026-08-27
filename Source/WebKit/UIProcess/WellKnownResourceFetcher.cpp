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
#include "WellKnownResourceFetcher.h"

#include "APIDataTask.h"
#include "APIDataTaskClient.h"
#include "NetworkProcessProxy.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/WellKnownOriginList.h>
#include <wtf/RunLoop.h>

namespace WebKit {

ASCIILiteral wellKnownFetchStatusDescription(WellKnownFetchResult::Status status)
{
    switch (status) {
    case WellKnownFetchResult::Status::Success: return "success"_s;
    case WellKnownFetchResult::Status::NetworkError: return "network error"_s;
    case WellKnownFetchResult::Status::UnacceptableResponse: return "unacceptable status or content type"_s;
    case WellKnownFetchResult::Status::InsecureRedirect: return "redirect left the https scheme"_s;
    case WellKnownFetchResult::Status::TooLarge: return "resource too large"_s;
    case WellKnownFetchResult::Status::TimedOut: return "timed out"_s;
    }
    return "unknown"_s;
}

class WellKnownResourceFetcherClient final : public API::DataTaskClient {
public:
    static Ref<WellKnownResourceFetcherClient> create(size_t maxResourceSize, CompletionHandler<void(WellKnownFetchResult&&)>&& completionHandler)
    {
        return adoptRef(*new WellKnownResourceFetcherClient(maxResourceSize, WTF::move(completionHandler)));
    }

    void finish(WellKnownFetchResult::Status status) const
    {
        if (!m_completionHandler)
            return;
        auto handler = std::exchange(m_completionHandler, nullptr);
        handler({ status, status == WellKnownFetchResult::Status::Success ? std::exchange(m_body, { }) : Vector<uint8_t> { } });
    }

    bool isFinished() const { return !m_completionHandler; }

private:
    WellKnownResourceFetcherClient(size_t maxResourceSize, CompletionHandler<void(WellKnownFetchResult&&)>&& completionHandler)
        : m_maxResourceSize(maxResourceSize)
        , m_completionHandler(WTF::move(completionHandler))
    {
    }

    ~WellKnownResourceFetcherClient() final
    {
        finish(m_failure == WellKnownFetchResult::Status::Success ? WellKnownFetchResult::Status::NetworkError : m_failure);
    }

    void willPerformHTTPRedirection(API::DataTask&, WebCore::ResourceResponse&&, WebCore::ResourceRequest&& request, CompletionHandler<void(bool)>&& decisionHandler) const final
    {
        if (!WebCore::isWellKnownRedirectAllowed(request.url())) {
            m_failure = WellKnownFetchResult::Status::InsecureRedirect;
            decisionHandler(false);
            return;
        }
        decisionHandler(true);
    }

    void didReceiveResponse(API::DataTask&, WebCore::ResourceResponse&& response, CompletionHandler<void(bool)>&& decisionHandler) const final
    {
        if (!WebCore::isWellKnownResponseAcceptable(response.httpStatusCode(), response.mimeType())) {
            if (m_failure == WellKnownFetchResult::Status::Success)
                m_failure = WellKnownFetchResult::Status::UnacceptableResponse;
            decisionHandler(false);
            return;
        }

        auto expected = response.expectedContentLength();
        if (expected > 0 && static_cast<size_t>(expected) > m_maxResourceSize) {
            m_failure = WellKnownFetchResult::Status::TooLarge;
            decisionHandler(false);
            return;
        }

        decisionHandler(true);
    }

    void didReceiveData(API::DataTask& task, std::span<const uint8_t> data) const final
    {
        if (m_body.size() + data.size() > m_maxResourceSize) {
            m_failure = WellKnownFetchResult::Status::TooLarge;
            m_body.clear();
            task.cancel();
            return;
        }
        m_body.append(data);
    }

    void didCompleteWithError(API::DataTask&, WebCore::ResourceError&& error) const final
    {
        if (m_failure != WellKnownFetchResult::Status::Success)
            finish(m_failure);
        else if (!error.isNull())
            finish(WellKnownFetchResult::Status::NetworkError);
        else
            finish(WellKnownFetchResult::Status::Success);
    }

    size_t m_maxResourceSize;
    mutable Vector<uint8_t> m_body;
    mutable WellKnownFetchResult::Status m_failure { WellKnownFetchResult::Status::Success };
    mutable CompletionHandler<void(WellKnownFetchResult&&)> m_completionHandler;
};

void WellKnownResourceFetcher::fetch(WebPageProxy& page, URL&& url, const WellKnownFetchLimits& limits, CompletionHandler<void(WellKnownFetchResult&&)>&& completionHandler)
{
    if (!WebCore::isWellKnownRedirectAllowed(url)) {
        completionHandler(WellKnownFetchResult { WellKnownFetchResult::Status::InsecureRedirect, { } });
        return;
    }

    WebCore::ResourceRequest request { WTF::move(url) };
    request.setTimeoutInterval(limits.timeout.seconds());
    request.setAllowCookies(false);
    request.setHTTPReferrer({ });

    Ref client = WellKnownResourceFetcherClient::create(limits.maxResourceSize, WTF::move(completionHandler));

    Ref protectedPage = page;
    Ref websiteDataStore = page.websiteDataStore();
    Ref networkProcess = websiteDataStore->networkProcess();
    networkProcess->dataTaskWithRequest(protectedPage, websiteDataStore->sessionID(), WTF::move(request), std::nullopt, true, [client, timeout = limits.timeout](API::DataTask& task) mutable {
        task.setClient(client.copyRef());

        RunLoop::mainSingleton().dispatchAfter(timeout, [client = WTF::move(client), task = Ref { task }] {
            if (client->isFinished())
                return;
            task->cancel();
            client->finish(WellKnownFetchResult::Status::TimedOut);
        });
    });
}

} // namespace WebKit
