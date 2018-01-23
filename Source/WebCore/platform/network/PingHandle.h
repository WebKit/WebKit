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

#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "Timer.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

// This class triggers asynchronous loads independent of the networking context staying alive (i.e., auditing pingbacks).
// The object just needs to live long enough to ensure the message was actually sent.
// As soon as any callback is received from the ResourceHandle, this class will cancel the load and delete itself.

class PingHandle final : private ResourceHandleClient {
    WTF_MAKE_NONCOPYABLE(PingHandle); WTF_MAKE_FAST_ALLOCATED;
public:
    PingHandle(NetworkingContext* networkingContext, const ResourceRequest& request, bool shouldUseCredentialStorage, bool shouldFollowRedirects, WTF::CompletionHandler<void(const ResourceError&, const ResourceResponse&)>&& completionHandler)
        : m_currentRequest(request)
        , m_timeoutTimer(*this, &PingHandle::timeoutTimerFired)
        , m_shouldUseCredentialStorage(shouldUseCredentialStorage)
        , m_shouldFollowRedirects(shouldFollowRedirects)
        , m_completionHandler(WTFMove(completionHandler))
    {
        bool defersLoading = false;
        bool shouldContentSniff = false;
        bool shouldContentEncodingSniff = true;
        m_handle = ResourceHandle::create(networkingContext, request, this, defersLoading, shouldContentSniff, shouldContentEncodingSniff);

        // If the server never responds, this object will hang around forever.
        // Set a very generous timeout, just in case.
        m_timeoutTimer.startOneShot(60000_s);
    }

private:
    void willSendRequestAsync(ResourceHandle*, ResourceRequest&& request, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) final
    {
        m_currentRequest = WTFMove(request);
        if (m_shouldFollowRedirects) {
            completionHandler(ResourceRequest { m_currentRequest });
            return;
        }
        completionHandler({ });
        pingLoadComplete(ResourceError { String(), 0, m_currentRequest.url(), ASCIILiteral("Not allowed to follow redirects"), ResourceError::Type::AccessControl });
    }
    void didReceiveResponseAsync(ResourceHandle*, ResourceResponse&& response, CompletionHandler<void()>&& completionHandler) final
    {
        completionHandler();
        pingLoadComplete({ }, response);
    }
    void didReceiveBuffer(ResourceHandle*, Ref<SharedBuffer>&&, int) final { pingLoadComplete(); }
    void didFinishLoading(ResourceHandle*) final { pingLoadComplete(); }
    void didFail(ResourceHandle*, const ResourceError& error) final { pingLoadComplete(error); }
    bool shouldUseCredentialStorage(ResourceHandle*) final { return m_shouldUseCredentialStorage; }
    void timeoutTimerFired() { pingLoadComplete(ResourceError { String(), 0, m_currentRequest.url(), ASCIILiteral("Load timed out"), ResourceError::Type::Timeout }); }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&)
    {
        m_handle->continueCanAuthenticateAgainstProtectionSpace(false);
        pingLoadComplete(ResourceError { String { }, 0, m_currentRequest.url(), ASCIILiteral("Not allowed to authenticate"), ResourceError::Type::AccessControl });
    }
#endif

    void pingLoadComplete(const ResourceError& error = { }, const ResourceResponse& response = { })
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

    RefPtr<ResourceHandle> m_handle;
    ResourceRequest m_currentRequest;
    Timer m_timeoutTimer;
    bool m_shouldUseCredentialStorage;
    bool m_shouldFollowRedirects;
    WTF::CompletionHandler<void(const ResourceError&, const ResourceResponse&)> m_completionHandler;
};

} // namespace WebCore
