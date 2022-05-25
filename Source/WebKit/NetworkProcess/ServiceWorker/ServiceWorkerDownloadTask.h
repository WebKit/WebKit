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

#if ENABLE(SERVICE_WORKER)

#include "Connection.h"
#include "DataReference.h"
#include "Download.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkDataTask.h"
#include <WebCore/FetchIdentifier.h>
#include <wtf/FileSystem.h>

namespace IPC {
class FormDataReference;
class SharedBufferReference;
}

namespace WebKit {

class NetworkLoad;
class NetworkProcess;
class SandboxExtension;
class WebSWServerToContextConnection;

class ServiceWorkerDownloadTask : public NetworkDataTask, public IPC::Connection::ThreadMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ServiceWorkerDownloadTask> create(NetworkSession& session, NetworkDataTaskClient& client, WebSWServerToContextConnection& connection, WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, WebCore::FetchIdentifier fetchIdentifier, const WebCore::ResourceRequest& request, DownloadID downloadID) { return adoptRef(* new ServiceWorkerDownloadTask(session, client, connection, serviceWorkerIdentifier, serverConnectionIdentifier, fetchIdentifier, request, downloadID)); }
    ~ServiceWorkerDownloadTask();

    WebCore::FetchIdentifier fetchIdentifier() const { return m_fetchIdentifier; }
    void contextClosed() { cancel(); }
    void start();
    void stop() { cancel(); }

    using NetworkDataTask::ref;
    using NetworkDataTask::deref;
    using NetworkDataTask::weakPtrFactory;
    using NetworkDataTask::WeakValueType;

private:
    ServiceWorkerDownloadTask(NetworkSession&, NetworkDataTaskClient&, WebSWServerToContextConnection&, WebCore::ServiceWorkerIdentifier, WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, const WebCore::ResourceRequest&, DownloadID);

    // IPC Message
    void didReceiveData(const IPC::SharedBufferReference&, int64_t encodedDataLength);
    void didReceiveFormData(const IPC::FormDataReference&);
    void didFinish();
    void didFail(WebCore::ResourceError&&);

    // NetworkDataTask
    void cancel() final;
    void resume() final;
    void invalidateAndCancel() final;
    State state() const final { return m_state; }
    void setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&&, bool /*allowOverwrite*/) final;

    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void refMessageReceiver() final { ref(); }
    void derefMessageReceiver() final { deref(); }

    template<typename Message> bool sendToServiceWorker(Message&&);
    void didFailDownload(std::optional<WebCore::ResourceError>&& = { });
    void close();

    WeakPtr<WebSWServerToContextConnection> m_serviceWorkerConnection;
    WebCore::ServiceWorkerIdentifier m_serviceWorkerIdentifier;
    WebCore::SWServerConnectionIdentifier m_serverConnectionIdentifier;
    WebCore::FetchIdentifier m_fetchIdentifier;
    DownloadID m_downloadID;
    Ref<NetworkProcess> m_networkProcess;
    RefPtr<SandboxExtension> m_sandboxExtension;
    FileSystem::PlatformFileHandle m_downloadFile { FileSystem::invalidPlatformFileHandle };
    int64_t m_downloadBytesWritten { 0 };
    State m_state { State::Suspended };
};

}

#endif // ENABLE(SERVICE_WORKER)
