/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "DownloadManager.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "ServiceWorkerDownloadTaskMessages.h"
#include "SharedBufferReference.h"
#include "WebErrors.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

using namespace WebCore;

static WorkQueue& serviceWorkerDownloadTaskQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("Shared ServiceWorkerDownloadTask Queue"_s));
    return queue.get();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ServiceWorkerDownloadTask);

ServiceWorkerDownloadTask::ServiceWorkerDownloadTask(NetworkSession& session, NetworkDataTaskClient& client, WebSWServerToContextConnection& serviceWorkerConnection, ServiceWorkerIdentifier serviceWorkerIdentifier, SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const WebCore::ResourceRequest& request, const ResourceResponse& response, DownloadID downloadID)
    : NetworkDataTask(session, client, request, StoredCredentialsPolicy::DoNotUse, false, false, false)
    , m_serviceWorkerConnection(serviceWorkerConnection)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_fetchIdentifier(fetchIdentifier)
    , m_downloadID(downloadID)
    , m_networkProcess(*serviceWorkerConnection.networkProcess())
    , m_sharedPreferences(serviceWorkerConnection.sharedPreferencesForWebProcess())
{
    auto expectedContentLength = response.expectedContentLength();
    if (expectedContentLength != -1)
        m_expectedContentLength = expectedContentLength;
    serviceWorkerConnection.registerDownload(*this);
}

ServiceWorkerDownloadTask::~ServiceWorkerDownloadTask()
{
    ASSERT(!m_serviceWorkerConnection);
}

void ServiceWorkerDownloadTask::startListeningForIPC()
{
    protect(RefPtr { m_serviceWorkerConnection.get() }->ipcConnection())->addMessageReceiver(*this, *this, Messages::ServiceWorkerDownloadTask::messageReceiverName(), fetchIdentifier().toUInt64());
}

void ServiceWorkerDownloadTask::close()
{
    ASSERT(isMainRunLoop());

    if (RefPtr serviceWorkerConnection = m_serviceWorkerConnection.get()) {
        protect(serviceWorkerConnection->ipcConnection())->removeMessageReceiver(Messages::ServiceWorkerDownloadTask::messageReceiverName(), fetchIdentifier().toUInt64());
        serviceWorkerConnection->unregisterDownload(*this);
        m_serviceWorkerConnection = nullptr;
    }
}

template<typename Message> bool ServiceWorkerDownloadTask::sendToServiceWorker(Message&& message)
{
    RefPtr serviceWorkerConnection = m_serviceWorkerConnection.get();
    if (!serviceWorkerConnection)
        return false;

    return protect(serviceWorkerConnection->ipcConnection())->send(std::forward<Message>(message), 0) == IPC::Error::NoError;
}

void ServiceWorkerDownloadTask::dispatch(Function<void()>&& function)
{
    serviceWorkerDownloadTaskQueueSingleton().dispatch([protectedThis = Ref { *this }, function = WTF::move(function)] {
        function();
    });
}

void ServiceWorkerDownloadTask::cancel()
{
    ASSERT(isMainRunLoop());

    serviceWorkerDownloadTaskQueueSingleton().dispatch([this, protectedThis = Ref { *this }] {
        m_downloadFile = { };
    });

    if (RefPtr sandboxExtension = std::exchange(m_sandboxExtension, nullptr))
        sandboxExtension->revoke();

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
        serviceWorkerDownloadTaskQueueSingleton().dispatch([this, protectedThis = Ref { *this }]() mutable {
            didFailDownload();
        });
        return;
    }

    NetworkDataTask::setPendingDownloadLocation(filename, { }, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTF::move(sandboxExtensionHandle));
    if (RefPtr sandboxExtension = m_sandboxExtension)
        sandboxExtension->consume();

    serviceWorkerDownloadTaskQueueSingleton().dispatch([this, protectedThis = Ref { *this }, allowOverwrite, filename = filename.isolatedCopy()]() mutable {
        if (allowOverwrite && FileSystem::fileExists(filename)) {
            if (!FileSystem::deleteFile(filename)) {
                didFailDownload();
                return;
            }
        }

        m_downloadFile = FileSystem::openFile(m_pendingDownloadLocation, FileSystem::FileOpenMode::Truncate);
        if (!m_downloadFile)
            didFailDownload();
    });
}

void ServiceWorkerDownloadTask::start()
{
    ASSERT(m_state != State::Completed);

    if (!sendToServiceWorker(Messages::WebSWContextManagerConnection::ConvertFetchToDownload { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier })) {
        serviceWorkerDownloadTaskQueueSingleton().dispatch([this, protectedThis = Ref { *this }]() mutable {
            didFailDownload();
        });
        return;
    }

    m_state = State::Running;

    CheckedRef manager = m_networkProcess->downloadManager();
    Ref download = Download::create(manager.get(), m_downloadID, *this, CheckedRef { *networkSession() });
    manager->dataTaskBecameDownloadTask(m_downloadID, download.copyRef());
    download->didCreateDestination(m_pendingDownloadLocation);
}

void ServiceWorkerDownloadTask::didReceiveData(const IPC::SharedBufferReference& data)
{
    ASSERT(!isMainRunLoop());

    if (!m_downloadFile)
        return;

    auto bytesWritten = m_downloadFile.write(data.span());

    if (bytesWritten != data.size()) {
        didFailDownload();
        return;
    }

    callOnMainRunLoop([this, protectedThis = Ref { *this }, bytesWritten = *bytesWritten] {
        m_downloadBytesWritten += bytesWritten;
        if (RefPtr download = m_networkProcess->checkedDownloadManager()->download(*m_pendingDownloadID))
            download->didReceiveData(bytesWritten, m_downloadBytesWritten, std::max(m_expectedContentLength.value_or(0), m_downloadBytesWritten));
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

    m_downloadFile = { };

    callOnMainRunLoop([this, protectedThis = Ref { *this }] {
        m_state = State::Completed;
        close();

#if !HAVE(MODERN_DOWNLOADPROGRESS)
        if (RefPtr sandboxExtension = std::exchange(m_sandboxExtension, nullptr))
            sandboxExtension->revoke();
#endif

        if (RefPtr download = m_networkProcess->checkedDownloadManager()->download(*m_pendingDownloadID)) {
#if HAVE(MODERN_DOWNLOADPROGRESS)
            if (RefPtr sandboxExtension = std::exchange(m_sandboxExtension, nullptr))
                download->setSandboxExtension(WTF::move(sandboxExtension));
#endif
            download->didFinish();
        }

        if (RefPtr client = m_client.get())
            client->didCompleteWithError({ });
    });
}

void ServiceWorkerDownloadTask::didFail(ResourceError&& error)
{
    ASSERT(!isMainRunLoop());

    didFailDownload(WTF::move(error));
}

void ServiceWorkerDownloadTask::didFailDownload(std::optional<ResourceError>&& error)
{
    ASSERT(!isMainRunLoop());

    m_downloadFile = { };

    callOnMainRunLoop([this, protectedThis = Ref { *this }, error = crossThreadCopy(WTF::move(error))] {
        if (m_state == State::Completed)
            return;

        m_state = State::Completed;
        close();

        if (RefPtr sandboxExtension = std::exchange(m_sandboxExtension, nullptr))
            sandboxExtension->revoke();

        auto resourceError = error.value_or(cancelledError(firstRequest()));
        if (RefPtr download = m_networkProcess->checkedDownloadManager()->download(*m_pendingDownloadID))
            download->didFail(resourceError, { });

        if (RefPtr client = m_client.get())
            client->didCompleteWithError(resourceError);
    });
}

std::optional<SharedPreferencesForWebProcess> ServiceWorkerDownloadTask::sharedPreferencesForWebProcess(const IPC::Connection& connection) const
{
    return m_sharedPreferences;
}

} // namespace WebKit
