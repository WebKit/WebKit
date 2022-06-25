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
#include "DownloadProxyMessages.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkLoad.h"
#include "NetworkSession.h"
#include "PendingDownload.h"
#include <WebCore/NotImplemented.h>
#include <pal/SessionID.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

DownloadManager::DownloadManager(Client& client)
    : m_client(client)
{
}

void DownloadManager::startDownload(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, const String& suggestedName)
{
    auto* networkSession = client().networkSession(sessionID);
    if (!networkSession)
        return;

    NetworkLoadParameters parameters;
    parameters.request = request;
    parameters.clientCredentialPolicy = ClientCredentialPolicy::MayAskClientForCredentials;
    parameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    if (request.url().protocolIsBlob())
        parameters.blobFileReferences = client().networkSession(sessionID)->blobRegistry().filesInBlob(request.url());
    parameters.storedCredentialsPolicy = sessionID.isEphemeral() ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;

    m_pendingDownloads.add(downloadID, makeUnique<PendingDownload>(m_client.parentProcessConnectionForDownloads(), WTFMove(parameters), downloadID, *networkSession, &client().networkSession(sessionID)->blobRegistry(), suggestedName));
}

void DownloadManager::dataTaskBecameDownloadTask(DownloadID downloadID, std::unique_ptr<Download>&& download)
{
    ASSERT(m_pendingDownloads.contains(downloadID));
    if (auto pendingDownload = m_pendingDownloads.take(downloadID)) {
#if PLATFORM(COCOA)
        pendingDownload->didBecomeDownload(download);
#endif
    }
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

void DownloadManager::convertNetworkLoadToDownload(DownloadID downloadID, std::unique_ptr<NetworkLoad>&& networkLoad, ResponseCompletionHandler&& completionHandler, Vector<RefPtr<WebCore::BlobDataFileReference>>&& blobFileReferences, const ResourceRequest& request, const ResourceResponse& response)
{
    ASSERT(!m_pendingDownloads.contains(downloadID));
    m_pendingDownloads.add(downloadID, makeUnique<PendingDownload>(m_client.parentProcessConnectionForDownloads(), WTFMove(networkLoad), WTFMove(completionHandler), downloadID, request, response));
}

void DownloadManager::downloadDestinationDecided(DownloadID downloadID, Ref<NetworkDataTask>&& networkDataTask)
{
    ASSERT(!m_downloadsAfterDestinationDecided.contains(downloadID));
    m_downloadsAfterDestinationDecided.set(downloadID, WTFMove(networkDataTask));
}

void DownloadManager::resumeDownload(PAL::SessionID sessionID, DownloadID downloadID, const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&& sandboxExtensionHandle, CallDownloadDidStart callDownloadDidStart)
{
#if !PLATFORM(COCOA)
    notImplemented();
#else
    auto* networkSession = m_client.networkSession(sessionID);
    if (!networkSession)
        return;
    auto download = makeUnique<Download>(*this, downloadID, nullptr, *networkSession);

    download->resume(resumeData, path, WTFMove(sandboxExtensionHandle));

    // For compatibility with the legacy download API, only send DidStart if we're using the new API.
    if (callDownloadDidStart == CallDownloadDidStart::Yes)
        download->send(Messages::DownloadProxy::DidStart({ }, { }));

    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
#endif
}

void DownloadManager::cancelDownload(DownloadID downloadID, CompletionHandler<void(const IPC::DataReference&)>&& completionHandler)
{
    if (auto* download = m_downloads.get(downloadID)) {
        ASSERT(!m_pendingDownloads.contains(downloadID));
        download->cancel(WTFMove(completionHandler), Download::IgnoreDidFailCallback::Yes);
        return;
    }
    if (auto pendingDownload = m_pendingDownloads.take(downloadID)) {
        pendingDownload->cancel(WTFMove(completionHandler));
        return;
    }
    ASSERT_NOT_REACHED();
    completionHandler({ });
}

#if PLATFORM(COCOA)
void DownloadManager::publishDownloadProgress(DownloadID downloadID, const URL& url, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    if (auto* download = m_downloads.get(downloadID))
        download->publishProgress(url, WTFMove(sandboxExtensionHandle));
    else if (auto* pendingDownload = m_pendingDownloads.get(downloadID))
        pendingDownload->publishProgress(url, WTFMove(sandboxExtensionHandle));
}
#endif // PLATFORM(COCOA)

void DownloadManager::downloadFinished(Download& download)
{
    ASSERT(m_downloads.contains(download.downloadID()));
    m_downloads.remove(download.downloadID());
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

void DownloadManager::applicationDidEnterBackground()
{
    for (auto& download : m_downloads.values())
        download->applicationDidEnterBackground();
}

void DownloadManager::applicationWillEnterForeground()
{
    for (auto& download : m_downloads.values())
        download->applicationWillEnterForeground();
}

} // namespace WebKit
