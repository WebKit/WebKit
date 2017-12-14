/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#include "DownloadManager.h"

#include "Download.h"
#include "NetworkBlobRegistry.h"
#include "NetworkLoad.h"
#include "NetworkSession.h"
#include "PendingDownload.h"
#include "SessionTracker.h"
#include <WebCore/NotImplemented.h>
#include <pal/SessionID.h>
#include <wtf/StdLibExtras.h>

using namespace WebCore;

namespace WebKit {

DownloadManager::DownloadManager(Client& client)
    : m_client(client)
{
}

void DownloadManager::startDownload(NetworkConnectionToWebProcess* connection, PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedName)
{
#if USE(NETWORK_SESSION)
    auto* networkSession = SessionTracker::networkSession(sessionID);
    if (!networkSession)
        return;

    NetworkLoadParameters parameters;
    parameters.sessionID = sessionID;
    parameters.request = request;
    parameters.clientCredentialPolicy = ClientCredentialPolicy::MayAskClientForCredentials;
    if (request.url().protocolIsBlob() && connection)
        parameters.blobFileReferences = NetworkBlobRegistry::singleton().filesInBlob(*connection, request.url());
    parameters.storedCredentialsPolicy = sessionID.isEphemeral() ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;

    m_pendingDownloads.add(downloadID, std::make_unique<PendingDownload>(WTFMove(parameters), downloadID, *networkSession, suggestedName));
#else
    auto download = std::make_unique<Download>(*this, downloadID, request, suggestedName);
    if (request.url().protocolIsBlob() && connection) {
        auto blobFileReferences = NetworkBlobRegistry::singleton().filesInBlob(*connection, request.url());
        for (auto& fileReference : blobFileReferences)
            fileReference->prepareForFileAccess();
        download->setBlobFileReferences(WTFMove(blobFileReferences));
    }
    download->start();

    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
#endif
}

#if USE(NETWORK_SESSION)
void DownloadManager::dataTaskBecameDownloadTask(DownloadID downloadID, std::unique_ptr<Download>&& download)
{
    ASSERT(m_pendingDownloads.contains(downloadID));
    m_pendingDownloads.remove(downloadID);
    ASSERT(!m_downloads.contains(downloadID));
    m_downloadsAfterDestinationDecided.remove(downloadID);
    m_downloads.add(downloadID, WTFMove(download));
}

void DownloadManager::continueWillSendRequest(DownloadID downloadID, WebCore::ResourceRequest&& request)
{
    auto* pendingDownload = m_pendingDownloads.get(downloadID);
    ASSERT(pendingDownload);
    if (pendingDownload)
        pendingDownload->continueWillSendRequest(WTFMove(request));
}

void DownloadManager::willDecidePendingDownloadDestination(NetworkDataTask& networkDataTask, ResponseCompletionHandler&& completionHandler)
{
    auto downloadID = networkDataTask.pendingDownloadID();
    auto addResult = m_downloadsWaitingForDestination.set(downloadID, std::make_pair<RefPtr<NetworkDataTask>, ResponseCompletionHandler>(&networkDataTask, WTFMove(completionHandler)));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}
#endif // USE(NETWORK_SESSION)

void DownloadManager::convertNetworkLoadToDownload(DownloadID downloadID, std::unique_ptr<NetworkLoad>&& networkLoad, Vector<RefPtr<WebCore::BlobDataFileReference>>&& blobFileReferences, const ResourceRequest& request, const ResourceResponse& response)
{
#if USE(NETWORK_SESSION)
    ASSERT(!m_pendingDownloads.contains(downloadID));
    m_pendingDownloads.add(downloadID, std::make_unique<PendingDownload>(WTFMove(networkLoad), downloadID, request, response));
#else
    auto download = std::make_unique<Download>(*this, downloadID, request);
    download->setBlobFileReferences(WTFMove(blobFileReferences));

    auto* handle = networkLoad->handle();
    ASSERT(handle);
    download->startWithHandle(handle, response);
    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));

    // Unblock the URL connection operation queue.
    handle->continueDidReceiveResponse();
#endif
}

void DownloadManager::continueDecidePendingDownloadDestination(DownloadID downloadID, String destination, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
#if USE(NETWORK_SESSION)
    if (m_downloadsWaitingForDestination.contains(downloadID)) {
        auto pair = m_downloadsWaitingForDestination.take(downloadID);
        auto networkDataTask = WTFMove(pair.first);
        auto completionHandler = WTFMove(pair.second);
        ASSERT(networkDataTask);
        ASSERT(completionHandler);
        ASSERT(m_pendingDownloads.contains(downloadID));

        networkDataTask->setPendingDownloadLocation(destination, WTFMove(sandboxExtensionHandle), allowOverwrite);
        completionHandler(PolicyAction::Download);
        if (networkDataTask->state() == NetworkDataTask::State::Canceling || networkDataTask->state() == NetworkDataTask::State::Completed)
            return;

        if (m_downloads.contains(downloadID)) {
            // The completion handler already called dataTaskBecameDownloadTask().
            return;
        }

        ASSERT(!m_downloadsAfterDestinationDecided.contains(downloadID));
        m_downloadsAfterDestinationDecided.set(downloadID, networkDataTask);
    }
#else
    if (auto* waitingDownload = download(downloadID))
        waitingDownload->didDecideDownloadDestination(destination, WTFMove(sandboxExtensionHandle), allowOverwrite);
#endif
}

void DownloadManager::resumeDownload(PAL::SessionID sessionID, DownloadID downloadID, const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&& sandboxExtensionHandle)
{
#if USE(NETWORK_SESSION) && !PLATFORM(COCOA)
    notImplemented();
#else
#if USE(NETWORK_SESSION)
    auto download = std::make_unique<Download>(*this, downloadID, nullptr, sessionID);
#else
    // Download::resume() is responsible for setting the Download's resource request.
    auto download = std::make_unique<Download>(*this, downloadID, ResourceRequest());
#endif

    download->resume(resumeData, path, WTFMove(sandboxExtensionHandle));
    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
#endif
}

void DownloadManager::cancelDownload(DownloadID downloadID)
{
    if (Download* download = m_downloads.get(downloadID)) {
#if USE(NETWORK_SESSION)
        ASSERT(!m_downloadsWaitingForDestination.contains(downloadID));
        ASSERT(!m_pendingDownloads.contains(downloadID));
#endif
        download->cancel();
        return;
    }
#if USE(NETWORK_SESSION)
    auto pendingDownload = m_pendingDownloads.take(downloadID);
    if (m_downloadsWaitingForDestination.contains(downloadID)) {
        auto pair = m_downloadsWaitingForDestination.take(downloadID);
        auto networkDataTask = WTFMove(pair.first);
        auto completionHandler = WTFMove(pair.second);
        ASSERT(networkDataTask);
        ASSERT(completionHandler);

        networkDataTask->cancel();
        completionHandler(PolicyAction::Ignore);
        m_client.pendingDownloadCanceled(downloadID);
        return;
    }

    if (pendingDownload)
        pendingDownload->cancel();
#endif
}

void DownloadManager::downloadFinished(Download* download)
{
    ASSERT(m_downloads.contains(download->downloadID()));
    m_downloads.remove(download->downloadID());
}

void DownloadManager::didCreateDownload()
{
    m_client.didCreateDownload();
}

void DownloadManager::didDestroyDownload()
{
    m_client.didDestroyDownload();
}

IPC::Connection* DownloadManager::downloadProxyConnection()
{
    return m_client.downloadProxyConnection();
}

AuthenticationManager& DownloadManager::downloadsAuthenticationManager()
{
    return m_client.downloadsAuthenticationManager();
}

} // namespace WebKit
