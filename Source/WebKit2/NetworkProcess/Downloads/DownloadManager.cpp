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
#include "NetworkLoad.h"
#include "NetworkSession.h"
#include "PendingDownload.h"
#include "SessionTracker.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/SessionID.h>
#include <wtf/StdLibExtras.h>

using namespace WebCore;

namespace WebKit {

DownloadManager::DownloadManager(Client& client)
    : m_client(client)
{
}

void DownloadManager::startDownload(SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedName)
{
#if USE(NETWORK_SESSION)
    if (!request.url().protocolIsBlob()) {
        auto* networkSession = SessionTracker::networkSession(sessionID);
        if (!networkSession)
            return;
        NetworkLoadParameters parameters;
        parameters.sessionID = sessionID;
        parameters.request = request;
        parameters.clientCredentialPolicy = ClientCredentialPolicy::MayAskClientForCredentials;
        m_pendingDownloads.add(downloadID, std::make_unique<PendingDownload>(WTFMove(parameters), downloadID, *networkSession, suggestedName));
        return;
    }
#endif
    auto download = std::make_unique<Download>(*this, downloadID, request, suggestedName);
    download->start();

    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
}

#if USE(NETWORK_SESSION)
void DownloadManager::dataTaskBecameDownloadTask(DownloadID downloadID, std::unique_ptr<Download>&& download)
{
    ASSERT(!m_pendingDownloads.contains(downloadID));
    ASSERT(!m_downloads.contains(downloadID));
    m_downloadsAfterDestinationDecided.remove(downloadID);
    m_downloads.add(downloadID, WTFMove(download));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void DownloadManager::continueCanAuthenticateAgainstProtectionSpace(DownloadID downloadID, bool canAuthenticate)
{
    auto* pendingDownload = m_pendingDownloads.get(downloadID);
    ASSERT(pendingDownload);
    if (pendingDownload)
        pendingDownload->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
}
#endif

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
#endif

void DownloadManager::convertHandleToDownload(DownloadID downloadID, ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response)
{
    auto download = std::make_unique<Download>(*this, downloadID, request);

    download->startWithHandle(handle, response);
    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
}

void DownloadManager::continueDecidePendingDownloadDestination(DownloadID downloadID, String destination, const SandboxExtension::Handle& sandboxExtensionHandle, bool allowOverwrite)
{
#if USE(NETWORK_SESSION)
    if (m_downloadsWaitingForDestination.contains(downloadID)) {
        auto pendingDownload = m_pendingDownloads.take(downloadID);
        auto pair = m_downloadsWaitingForDestination.take(downloadID);
        auto networkDataTask = WTFMove(pair.first);
        auto completionHandler = WTFMove(pair.second);
        ASSERT(networkDataTask);
        ASSERT(completionHandler);
        ASSERT(!pendingDownload || pendingDownload.get() == networkDataTask->pendingDownload());

        networkDataTask->setPendingDownloadLocation(destination, sandboxExtensionHandle, allowOverwrite);
        completionHandler(PolicyDownload);
        if (networkDataTask->state() == NetworkDataTask::State::Canceling || networkDataTask->state() == NetworkDataTask::State::Completed)
            return;

        if (m_downloads.contains(downloadID)) {
            // The completion handler already called dataTaskBecameDownloadTask().
            return;
        }

        ASSERT(!m_downloadsAfterDestinationDecided.contains(downloadID));
        m_downloadsAfterDestinationDecided.set(downloadID, networkDataTask);
        return;
    }
#endif
    if (auto* waitingDownload = download(downloadID))
        waitingDownload->didDecideDownloadDestination(destination, sandboxExtensionHandle, allowOverwrite);
}

void DownloadManager::resumeDownload(SessionID, DownloadID downloadID, const IPC::DataReference& resumeData, const String& path, const SandboxExtension::Handle& sandboxExtensionHandle)
{
#if USE(NETWORK_SESSION)
    notImplemented();
#else
    // Download::resume() is responsible for setting the Download's resource request.
    auto download = std::make_unique<Download>(*this, downloadID, ResourceRequest());

    download->resume(resumeData, path, sandboxExtensionHandle);
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
        completionHandler(PolicyIgnore);
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
