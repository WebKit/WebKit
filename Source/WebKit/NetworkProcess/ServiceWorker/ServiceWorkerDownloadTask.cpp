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

#include "config.h"
#include "ServiceWorkerDownloadTask.h"

#if ENABLE(SERVICE_WORKER)

#include "DownloadManager.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "SharedBufferReference.h"
#include "WebErrors.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include <wtf/WorkQueue.h>

namespace WebKit {

using namespace WebCore;

static WorkQueue& sharedServiceWorkerDownloadTaskQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("Shared ServiceWorkerDownloadTask Queue"));
    return queue.get();
}

ServiceWorkerDownloadTask::ServiceWorkerDownloadTask(NetworkSession& session, NetworkDataTaskClient& client, WebSWServerToContextConnection& serviceWorkerConnection, ServiceWorkerIdentifier serviceWorkerIdentifier, SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const WebCore::ResourceRequest& request, DownloadID downloadID)
    : NetworkDataTask(session, client, request, StoredCredentialsPolicy::DoNotUse, false, false)
    , m_serviceWorkerConnection(serviceWorkerConnection)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_fetchIdentifier(fetchIdentifier)
    , m_downloadID(downloadID)
    , m_networkProcess(serviceWorkerConnection.networkProcess())
{
    serviceWorkerConnection.registerDownload(*this);
}

ServiceWorkerDownloadTask::~ServiceWorkerDownloadTask()
{
    ASSERT(!m_serviceWorkerConnection);
}

void ServiceWorkerDownloadTask::close()
{
    ASSERT(isMainRunLoop());

    if (m_serviceWorkerConnection) {
        m_serviceWorkerConnection->unregisterDownload(*this);
        m_serviceWorkerConnection = nullptr;
    }
}

template<typename Message> bool ServiceWorkerDownloadTask::sendToServiceWorker(Message&& message)
{
    return m_serviceWorkerConnection ? m_serviceWorkerConnection->ipcConnection().send(std::forward<Message>(message), 0) : false;
}

void ServiceWorkerDownloadTask::dispatchToThread(Function<void()>&& function)
{
    sharedServiceWorkerDownloadTaskQueue().dispatch([protectedThis = Ref { *this }, function = WTFMove(function)] {
        function();
    });
}

void ServiceWorkerDownloadTask::cancel()
{
    ASSERT(isMainRunLoop());

    sharedServiceWorkerDownloadTaskQueue().dispatch([this, protectedThis = Ref { *this }] {
        if (m_downloadFile != FileSystem::invalidPlatformFileHandle) {
            FileSystem::closeFile(m_downloadFile);
            m_downloadFile = FileSystem::invalidPlatformFileHandle;
        }
    });

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }

    sendToServiceWorker(Messages::WebSWContextManagerConnection::CancelFetch { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier });

    m_state = State::Completed;
    close();
}

void ServiceWorkerDownloadTask::resume()
{
    ASSERT(isMainRunLoop());

    m_state = State::Running;
}

void ServiceWorkerDownloadTask::invalidateAndCancel()
{
    ASSERT(isMainRunLoop());

    cancel();
}

void ServiceWorkerDownloadTask::setPendingDownloadLocation(const WTF::String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    ASSERT(isMainRunLoop());

    if (!networkSession()) {
        sharedServiceWorkerDownloadTaskQueue().dispatch([this, protectedThis = Ref { *this }]() mutable {
            didFailDownload();
        });
        return;
    }

    NetworkDataTask::setPendingDownloadLocation(filename, { }, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    sharedServiceWorkerDownloadTaskQueue().dispatch([this, protectedThis = Ref { *this }, allowOverwrite , filename = filename.isolatedCopy()]() mutable {
        if (allowOverwrite && FileSystem::fileExists(filename)) {
            if (!FileSystem::deleteFile(filename)) {
                didFailDownload();
                return;
            }
        }

        m_downloadFile = FileSystem::openFile(m_pendingDownloadLocation, FileSystem::FileOpenMode::Write);
        if (m_downloadFile == FileSystem::invalidPlatformFileHandle)
            didFailDownload();
    });
}

void ServiceWorkerDownloadTask::start()
{
    ASSERT(m_state != State::Completed);

    if (!sendToServiceWorker(Messages::WebSWContextManagerConnection::ConvertFetchToDownload { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier })) {
        sharedServiceWorkerDownloadTaskQueue().dispatch([this, protectedThis = Ref { *this }]() mutable {
            didFailDownload();
        });
        return;
    }

    m_state = State::Running;

    auto& manager = m_networkProcess->downloadManager();
    auto download = makeUnique<Download>(manager, m_downloadID, *this, *networkSession());
    auto* downloadPtr = download.get();
    manager.dataTaskBecameDownloadTask(m_downloadID, WTFMove(download));
    downloadPtr->didCreateDestination(m_pendingDownloadLocation);
}

void ServiceWorkerDownloadTask::didReceiveData(const IPC::SharedBufferReference& data, int64_t encodedDataLength)
{
    ASSERT(!isMainRunLoop());

    if (m_downloadFile == FileSystem::invalidPlatformFileHandle)
        return;

    size_t bytesWritten = FileSystem::writeToFile(m_downloadFile, data.data(), data.size());

    if (bytesWritten != data.size()) {
        didFailDownload();
        return;
    }

    callOnMainRunLoop([this, protectedThis = Ref { *this }, bytesWritten] {
        m_downloadBytesWritten += bytesWritten;
        if (auto* download = m_networkProcess->downloadManager().download(m_pendingDownloadID))
            download->didReceiveData(bytesWritten, m_downloadBytesWritten, 0);
    });
}

void ServiceWorkerDownloadTask::didReceiveFormData(const IPC::FormDataReference& formData)
{
    ASSERT(!isMainRunLoop());

    // FIXME: Support writing formData in downloads.
    RELEASE_LOG_ERROR(ServiceWorker, "ServiceWorkerDownloadTask::didReceiveFormData not implemented");
    didFailDownload();
}

void ServiceWorkerDownloadTask::didFinish()
{
    ASSERT(!isMainRunLoop());

    FileSystem::closeFile(m_downloadFile);
    m_downloadFile = FileSystem::invalidPlatformFileHandle;

    callOnMainRunLoop([this, protectedThis = Ref { *this }] {
        m_state = State::Completed;
        close();

        if (m_sandboxExtension) {
            m_sandboxExtension->revoke();
            m_sandboxExtension = nullptr;
        }

        if (auto download = m_networkProcess->downloadManager().download(m_pendingDownloadID))
            download->didFinish();

        if (m_client)
            m_client->didCompleteWithError({ });
    });
}

void ServiceWorkerDownloadTask::didFail(ResourceError&& error)
{
    ASSERT(!isMainRunLoop());

    didFailDownload(WTFMove(error));
}

void ServiceWorkerDownloadTask::didFailDownload(std::optional<ResourceError>&& error)
{
    ASSERT(!isMainRunLoop());

    if (m_downloadFile != FileSystem::invalidPlatformFileHandle) {
        FileSystem::closeFile(m_downloadFile);
        m_downloadFile = FileSystem::invalidPlatformFileHandle;
    }

    callOnMainRunLoop([this, protectedThis = Ref { *this }, error = crossThreadCopy(WTFMove(error))] {
        if (m_state == State::Completed)
            return;

        m_state = State::Completed;
        close();

        if (m_sandboxExtension) {
            m_sandboxExtension->revoke();
            m_sandboxExtension = nullptr;
        }

        auto resourceError = error.value_or(cancelledError(firstRequest()));
        if (auto download = m_networkProcess->downloadManager().download(m_pendingDownloadID))
            download->didFail(resourceError, IPC::DataReference());

        if (m_client)
            m_client->didCompleteWithError(resourceError);
    });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
