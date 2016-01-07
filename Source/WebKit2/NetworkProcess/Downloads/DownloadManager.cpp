/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

void DownloadManager::startDownload(SessionID sessionID, DownloadID downloadID, const ResourceRequest& request)
{
#if USE(NETWORK_SESSION)
    auto* networkSession = SessionTracker::networkSession(sessionID);
    if (!networkSession)
        return;
    NetworkLoadParameters parameters;
    parameters.sessionID = sessionID;
    parameters.request = request;
    m_pendingDownloads.add(downloadID, std::make_unique<PendingDownload>(parameters, downloadID));
#else
    auto download = std::make_unique<Download>(*this, downloadID, request);
    download->start();

    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
#endif
}

#if USE(NETWORK_SESSION)
std::unique_ptr<PendingDownload> DownloadManager::dataTaskBecameDownloadTask(DownloadID downloadID, std::unique_ptr<Download>&& download)
{
    // This is needed for downloads started with startDownload, otherwise it will return nullptr.
    auto pendingDownload = m_pendingDownloads.take(downloadID);

    m_downloads.add(downloadID, WTFMove(download));
    return pendingDownload;
}
#else
void DownloadManager::convertHandleToDownload(DownloadID downloadID, ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response)
{
    auto download = std::make_unique<Download>(*this, downloadID, request);

    download->startWithHandle(handle, response);
    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, WTFMove(download));
}
#endif

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
    Download* download = m_downloads.get(downloadID);
    if (!download)
        return;

    download->cancel();
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
