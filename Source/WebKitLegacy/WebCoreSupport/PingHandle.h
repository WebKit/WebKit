/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/Timer.h>
#include <wtf/CompletionHandler.h>

// This class triggers asynchronous loads independent of the networking context staying alive (i.e., auditing pingbacks).
// The object just needs to live long enough to ensure the message was actually sent.
// As soon as any callback is received from the ResourceHandle, this class will cancel the load and delete itself.

class PingHandle final : private WebCore::ResourceHandleClient {
    WTF_MAKE_NONCOPYABLE(PingHandle); WTF_MAKE_FAST_ALLOCATED;
public:
    PingHandle(WebCore::NetworkingContext* networkingContext, const WebCore::ResourceRequest& request, bool shouldUseCredentialStorage, bool shouldFollowRedirects, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&& completionHandler)
        : m_currentRequest(request)
        , m_timeoutTimer(*this, &PingHandle::timeoutTimerFired)
        , m_shouldUseCredentialStorage(shouldUseCredentialStorage)
        , m_shouldFollowRedirects(shouldFollowRedirects)
        , m_completionHandler(WTFMove(completionHandler))
    {
        bool defersLoading = false;
        bool shouldContentSniff = false;
        m_handle = WebCore::ResourceHandle::create(networkingContext, request, this, defersLoading, shouldContentSniff, WebCore::ContentEncodingSniffingPolicy::Default, nullptr, false);

        // If the server never responds, this object will hang around forever.
        // Set a very generous timeout, just in case.
        m_timeoutTimer.startOneShot(60000_s);
    }

private:
    void willSendRequestAsync(WebCore::ResourceHandle*, WebCore::ResourceRequest&& request, WebCore::ResourceResponse&&, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler) final
    {
        m_currentRequest = WTFMove(request);
        if (m_shouldFollowRedirects) {
            completionHandler(WebCore::ResourceRequest { m_currentRequest });
            return;
        }
        completionHandler({ });
        pingLoadComplete(WebCore::ResourceError { String(), 0, m_currentRequest.url(), "Not allowed to follow redirects"_s, WebCore::ResourceError::Type::AccessControl });
    }
    void didReceiveResponseAsync(WebCore::ResourceHandle*, WebCore::ResourceResponse&& response, CompletionHandler<void()>&& completionHandler) final
    {
        completionHandler();
        pingLoadComplete({ }, response);
    }
    void didReceiveData(WebCore::ResourceHandle*, const WebCore::SharedBuffer&, int) final { pingLoadComplete(); }
    void didFinishLoading(WebCore::ResourceHandle*, const WebCore::NetworkLoadMetrics&) final { pingLoadComplete(); }
    void didFail(WebCore::ResourceHandle*, const WebCore::ResourceError& error) final { pingLoadComplete(error); }
    bool shouldUseCredentialStorage(WebCore::ResourceHandle*) final { return m_shouldUseCredentialStorage; }
    void timeoutTimerFired() { pingLoadComplete(WebCore::ResourceError { String(), 0, m_currentRequest.url(), "Load timed out"_s, WebCore::ResourceError::Type::Timeout }); }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(WebCore::ResourceHandle*, const WebCore::ProtectionSpace&, CompletionHandler<void(bool)>&& completionHandler)
    {
        completionHandler(false);
        pingLoadComplete(WebCore::ResourceError { String { }, 0, m_currentRequest.url(), "Not allowed to authenticate"_s, WebCore::ResourceError::Type::AccessControl });
    }
#endif

    void pingLoadComplete(const WebCore::ResourceError& error = { }, const WebCore::ResourceResponse& response = { })
    {
        if (auto completionHandler = std::exchange(m_completionHandler, nullptr))
            completionHandler(error, response);
        delete this;
    }

    virtual ~PingHandle()
    {
        ASSERT(!m_completionHandler);
        if (m_handle) {
            ASSERT(m_handle->client() == this);
            m_handle->clearClient();
            m_handle->cancel();
        }
    }

    RefPtr<WebCore::ResourceHandle> m_handle;
    WebCore::ResourceRequest m_currentRequest;
    WebCore::Timer m_timeoutTimer;
    bool m_shouldUseCredentialStorage;
    bool m_shouldFollowRedirects;
    CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)> m_completionHandler;
};
