/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceLoaderIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/PlatformMediaResourceLoader.h>
#include <WebCore/SharedMemory.h>
#include <wtf/Expected.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace IPC {
class Connection;
class Decoder;
class SharedBufferReference;
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
}

namespace WebKit {

class RemoteMediaResourceLoaderProxy final
    : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaResourceLoaderProxy);
public:
    static Ref<RemoteMediaResourceLoaderProxy> create(Ref<IPC::Connection>&&, WebCore::PlatformMediaResourceLoader&, RemoteMediaResourceLoaderIdentifier);
    virtual ~RemoteMediaResourceLoaderProxy();

    static Ref<WorkQueue> defaultQueue()
    {
        // FIXME: Move this object and MediaResourceLoader off the main thread
        return WorkQueue::mainSingleton();
    }

    // Disambiguate ref/deref:
    void ref() const final { return IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::ref(); }
    void deref() const final { return IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::deref(); }

    // IPC::Connection::WorkQueueMessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages from RemoteMediaResourceLoader:
    void requestResource(RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, WebCore::PlatformMediaResourceLoader::LoadOptions);
    void removeResource(RemoteMediaResourceIdentifier, CompletionHandler<void()>&&);
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);

    // Messages to RemoteMediaResourceLoader:
    void responseReceived(RemoteMediaResourceIdentifier, const WebCore::ResourceResponse&, bool, CompletionHandler<void(WebCore::ShouldContinuePolicyCheck)>&&);
    void redirectReceived(RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&);
    void dataSent(RemoteMediaResourceIdentifier, uint64_t, uint64_t);
    void dataReceived(RemoteMediaResourceIdentifier, const WebCore::SharedBuffer&);
    void accessControlCheckFailed(RemoteMediaResourceIdentifier, const WebCore::ResourceError&);
    void loadFailed(RemoteMediaResourceIdentifier, const WebCore::ResourceError&);
    void loadFinished(RemoteMediaResourceIdentifier, const WebCore::NetworkLoadMetrics&);

private:
    RemoteMediaResourceLoaderProxy(Ref<IPC::Connection>&&, WebCore::PlatformMediaResourceLoader&, RemoteMediaResourceLoaderIdentifier);

    void initializeConnection();

    const Ref<IPC::Connection> m_connection;
    const Ref<WebCore::PlatformMediaResourceLoader> m_platformLoader;
    RemoteMediaResourceLoaderIdentifier m_identifier;
    HashMap<RemoteMediaResourceIdentifier, RefPtr<WebCore::PlatformMediaResource>> m_mediaResources;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
