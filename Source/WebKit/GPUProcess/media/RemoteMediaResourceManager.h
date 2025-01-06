/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#include "RemoteMediaResourceIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/PolicyChecker.h>
#include <WebCore/SharedMemory.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
class SharedBufferReference;
}

namespace WebCore {
class NetworkLoadMetrics;
class ResourceRequest;
}

namespace WebKit {

class RemoteMediaResource;

class RemoteMediaResourceManager
    : public IPC::WorkQueueMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaResourceManager);
public:
    static Ref<RemoteMediaResourceManager> create() { return adoptRef(*new RemoteMediaResourceManager()); }
    ~RemoteMediaResourceManager();

    void ref() const final { IPC::WorkQueueMessageReceiver::ref(); }
    void deref() const final { IPC::WorkQueueMessageReceiver::deref(); }

    void initializeConnection(IPC::Connection*);
    void stopListeningForIPC();

    void addMediaResource(RemoteMediaResourceIdentifier, RemoteMediaResource&);
    void removeMediaResource(RemoteMediaResourceIdentifier);

    // IPC::Connection::WorkQueueMessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    RemoteMediaResourceManager();
    void responseReceived(RemoteMediaResourceIdentifier, const WebCore::ResourceResponse&, bool, CompletionHandler<void(WebCore::ShouldContinuePolicyCheck)>&&);
    void redirectReceived(RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&);
    void dataSent(RemoteMediaResourceIdentifier, uint64_t, uint64_t);
    void dataReceived(RemoteMediaResourceIdentifier, IPC::SharedBufferReference&&, CompletionHandler<void(std::optional<WebCore::SharedMemory::Handle>&&)>&&);
    void accessControlCheckFailed(RemoteMediaResourceIdentifier, const WebCore::ResourceError&);
    void loadFailed(RemoteMediaResourceIdentifier, const WebCore::ResourceError&);
    void loadFinished(RemoteMediaResourceIdentifier, const WebCore::NetworkLoadMetrics&);

    RefPtr<RemoteMediaResource> resourceForId(RemoteMediaResourceIdentifier);

    Lock m_lock;
    HashMap<RemoteMediaResourceIdentifier, ThreadSafeWeakPtr<RemoteMediaResource>> m_remoteMediaResources WTF_GUARDED_BY_LOCK(m_lock);

    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
