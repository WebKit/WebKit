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

namespace WebCore {

// This class triggers asynchronous loads independent of the networking context staying alive (i.e., auditing pingbacks).
// The object just needs to live long enough to ensure the message was actually sent.
// As soon as any callback is received from the ResourceHandle, this class will cancel the load and delete itself.

class PingHandle final : private ResourceHandleClient {
    WTF_MAKE_NONCOPYABLE(PingHandle); WTF_MAKE_FAST_ALLOCATED;
public:
    enum class UsesAsyncCallbacks {
        Yes,
        No,
    };
    
    PingHandle(NetworkingContext* networkingContext, const ResourceRequest& request, bool shouldUseCredentialStorage, UsesAsyncCallbacks useAsyncCallbacks, bool shouldFollowRedirects)
        : m_timeoutTimer(*this, &PingHandle::timeoutTimerFired)
        , m_shouldUseCredentialStorage(shouldUseCredentialStorage)
        , m_shouldFollowRedirects(shouldFollowRedirects)
        , m_usesAsyncCallbacks(useAsyncCallbacks)
    {
        m_handle = ResourceHandle::create(networkingContext, request, this, false, false);

        // If the server never responds, this object will hang around forever.
        // Set a very generous timeout, just in case.
        m_timeoutTimer.startOneShot(60000);
    }

private:
    ResourceRequest willSendRequest(ResourceHandle*, ResourceRequest&& request, ResourceResponse&&) final
    {
        return m_shouldFollowRedirects ? request : ResourceRequest();
    }
    void willSendRequestAsync(ResourceHandle* handle, ResourceRequest&& request, ResourceResponse&&) final
    {
        if (m_shouldFollowRedirects) {
            handle->continueWillSendRequest(WTFMove(request));
            return;
        }
        delete this;
    }
    void didReceiveResponse(ResourceHandle*, ResourceResponse&&) final { delete this; }
    void didReceiveBuffer(ResourceHandle*, Ref<SharedBuffer>&&, int) final { delete this; };
    void didFinishLoading(ResourceHandle*, double) final { delete this; }
    void didFail(ResourceHandle*, const ResourceError&) final { delete this; }
    bool shouldUseCredentialStorage(ResourceHandle*) final { return m_shouldUseCredentialStorage; }
    bool usesAsyncCallbacks() final { return m_usesAsyncCallbacks == UsesAsyncCallbacks::Yes; }
    void timeoutTimerFired() { delete this; }

    virtual ~PingHandle()
    {
        if (m_handle) {
            ASSERT(m_handle->client() == this);
            m_handle->clearClient();
            m_handle->cancel();
        }
    }

    RefPtr<ResourceHandle> m_handle;
    Timer m_timeoutTimer;
    bool m_shouldUseCredentialStorage;
    bool m_shouldFollowRedirects;
    UsesAsyncCallbacks m_usesAsyncCallbacks;
};

} // namespace WebCore
