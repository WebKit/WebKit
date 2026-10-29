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

#include "APIError.h"
#include "AuthenticationManager.h"
#include "Download.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
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

DownloadManager::~DownloadManager() = default;

void DownloadManager::startDownload(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, const String& suggestedName, FromDownloadAttribute fromDownloadAttribute, std::optional<WebCore::FrameIdentifier> frameID, std::optional<WebCore::PageIdentifier> pageID, std::optional<WebCore::ProcessIdentifier> webProcessID)
{
    CheckedPtr networkSession = m_client->networkSession(sessionID);
    if (!networkSession)
        return;

    NetworkLoadParameters parameters;
    if (frameID)
        parameters.webFrameID = *frameID;
    if (pageID)
        parameters.webPageID = *pageID;
    parameters.request = request;
    parameters.clientCredentialPolicy = ClientCredentialPolicy::MayAskClientForCredentials;
    parameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    if (request.url().protocolIsBlob()) {
        parameters.topOrigin = topOrigin ? topOrigin->securityOrigin().ptr() : nullptr;
        parameters.blobFileReferences = m_client->networkSession(sessionID)->blobRegistry().filesInBlob(request.url(), topOrigin);
    }
    parameters.storedCredentialsPolicy = sessionID.isEphemeral() ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;

    m_pendingDownloads.add(downloadID, PendingDownload::create(protect(m_client->parentProcessConnectionForDownloads()).get(), WTF::move(parameters), downloadID, *networkSession, suggestedName, fromDownloadAttribute, webProcessID));
}

void DownloadManager::dataTaskBecameDownloadTask(DownloadID downloadID, Ref<Download>&& download)
{
    ASSERT(m_pendingDownloads.contains(downloadID));
    if (RefPtr pendingDownload = m_pendingDownloads.take(downloadID)) {
#if PLATFORM(COCOA)
        pendingDownload->didBecomeDownload(download);
#endif
    }
    ASSERT(!m_downloads.contains(downloadID));
    m_downloadsAfterDestinationDecided.remove(downloadID);
    m_downloads.add(downloadID, WTF::move(download));
}

void DownloadManager::convertNetworkLoadToDownload(DownloadID downloadID, Ref<NetworkLoad>&& networkLoad, ResponseCompletionHandler&& completionHandler, Vector<Ref<WebCore::BlobDataFileReference>>&& blobFileReferences, const ResourceRequest& request, const ResourceResponse& response)
{
    ASSERT(!m_pendingDownloads.contains(downloadID));
    m_pendingDownloads.add(downloadID, PendingDownload::create(protect(m_client->parentProcessConnectionForDownloads()).get(), WTF::move(networkLoad), WTF::move(completionHandler), downloadID, request, response));
}

void DownloadManager::downloadDestinationDecided(DownloadID downloadID, Ref<NetworkDataTask>&& networkDataTask)
{
    ASSERT(!m_downloadsAfterDestinationDecided.contains(downloadID));
    m_downloadsAfterDestinationDecided.set(downloadID, WTF::move(networkDataTask));
}

void DownloadManager::resumeDownload(PAL::SessionID sessionID, DownloadID downloadID, std::span<const uint8_t> resumeData, const String& path, SandboxExtension::Handle&& sandboxExtensionHandle, CallDownloadDidStart callDownloadDidStart, std::span<const uint8_t> activityAccessToken)
{
#if !PLATFORM(COCOA)
    notImplemented();
#else
    CheckedPtr networkSession = m_client->networkSession(sessionID);
    if (!networkSession)
        return;
    // If a download had been private-relayed when it was originally started, we have no way to
    // force it to be private relayed when we resume it.
    // This is especially imporant if advanced privacy protections are set to fail-closed
    // and not allow falling back to non-relayed connections.
    // So instead of resuming, start the download over with those protections applied, writing to
    // the destination it was already using.
    // FIXME: rdar://188118122 Once we have CFNetwork API to configure privacy relay use on a resume,
    // use that instead of this mechanism.
    auto resumeInfo = Download::resumeInfoFromResumeData(resumeData);
    bool mustRestartToApplyProtections = resumeInfo.wasPrivateRelayed
        && resumeInfo.advancedPrivacyProtections.containsAny({ WebCore::AdvancedPrivacyProtections::FailClosedForUnreachableHosts, WebCore::AdvancedPrivacyProtections::FailClosedForAllHosts, WebCore::AdvancedPrivacyProtections::StrictFailClosed });

    if (mustRestartToApplyProtections && resumeInfo.url.isValid()) {
        startDownload(sessionID, downloadID, ResourceRequest { WTF::move(resumeInfo.url) }, std::nullopt, NavigatingToAppBoundDomain::No, resumeInfo.advancedPrivacyProtections);
        if (RefPtr pendingDownload = m_pendingDownloads.get(downloadID))
            pendingDownload->setPendingDownloadLocation(path, WTF::move(sandboxExtensionHandle), true);
#if HAVE(MODERN_DOWNLOADPROGRESS)
        // Short-circuiting the client's destination decision also skips where progress is normally
        // published, so republish it here from what the resume data already carries.
        auto placeholderBookmarkData = Download::placeholderBookmarkDataFromResumeData(resumeData);
        if (!placeholderBookmarkData.isEmpty())
            publishDownloadProgress(downloadID, URL::fileURLWithFileSystemPath(path), placeholderBookmarkData.span(), UseDownloadPlaceholder::Yes, activityAccessToken);
#endif
        return;
    }

    Ref download = Download::create(*this, downloadID, nullptr, *networkSession);

    // For compatibility with the legacy download API, only send DidStart if we're using the new API.
    if (callDownloadDidStart == CallDownloadDidStart::Yes)
        download->send(Messages::DownloadProxy::DidStart({ }, { }));

    ASSERT(!m_downloads.contains(downloadID));
    m_downloads.add(downloadID, download.copyRef());

    // The download needs restarting but its resume blob records no URL to restart from.
    // This should never happen for a resume blob that WebKit recorded, but resume blob data
    // comes from disk and might be inconsistent to what WebKit would write.
    // Handle this case with a log messasge and download failure for the client to address.
    if (mustRestartToApplyProtections) {
        RELEASE_LOG_ERROR(Network, "DownloadManager::resumeDownload: download %" PRIu64 " must fail closed but its resume data records no URL to restart from", downloadID.toUInt64());
        download->didFail(ResourceError { API::Error::webKitErrorDomain(), API::Error::General::Internal, { }, { } }, { });
        return;
    }

    download->resume(resumeData, path, WTF::move(sandboxExtensionHandle), activityAccessToken);
#endif
}

void DownloadManager::cancelDownload(DownloadID downloadID, CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler)
{
    if (RefPtr download = m_downloads.get(downloadID)) {
        ASSERT(!m_pendingDownloads.contains(downloadID));
        download->cancel(WTF::move(completionHandler), Download::IgnoreDidFailCallback::Yes);
        return;
    }
    if (RefPtr pendingDownload = m_pendingDownloads.take(downloadID)) {
        pendingDownload->cancel(WTF::move(completionHandler));
        return;
    }
    // If there is no active or pending download, then the download finished in a short race window after cancellation was requested.
    completionHandler({ });
}

Download* DownloadManager::download(DownloadID downloadID)
{
    return m_downloads.get(downloadID);
}

#if PLATFORM(COCOA)
#if HAVE(MODERN_DOWNLOADPROGRESS)
void DownloadManager::publishDownloadProgress(DownloadID downloadID, const URL& url, std::span<const uint8_t> bookmarkData, WebKit::UseDownloadPlaceholder useDownloadPlaceholder, std::span<const uint8_t> activityAccessToken)
{
    if (RefPtr download = m_downloads.get(downloadID))
        download->publishProgress(url, bookmarkData, useDownloadPlaceholder, activityAccessToken);
    else if (RefPtr pendingDownload = m_pendingDownloads.get(downloadID))
        pendingDownload->publishProgress(url, bookmarkData, useDownloadPlaceholder, activityAccessToken);
}
#else
void DownloadManager::publishDownloadProgress(DownloadID downloadID, const URL& url, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    if (RefPtr download = m_downloads.get(downloadID))
        download->publishProgress(url, WTF::move(sandboxExtensionHandle));
    else if (RefPtr pendingDownload = m_pendingDownloads.get(downloadID))
        pendingDownload->publishProgress(url, WTF::move(sandboxExtensionHandle));
}
#endif
#endif // PLATFORM(COCOA)

void DownloadManager::downloadFinished(Download& download)
{
    ASSERT(m_downloads.get(download.downloadID()) == &download);
    download.clearManager();
    m_downloads.remove(download.downloadID());
}

void DownloadManager::didCreateDownload()
{
    m_client->didCreateDownload();
}

void DownloadManager::didDestroyDownload()
{
    m_client->didDestroyDownload();
}

IPC::Connection* DownloadManager::downloadProxyConnection()
{
    return m_client->downloadProxyConnection();
}

AuthenticationManager& DownloadManager::downloadsAuthenticationManager()
{
    return m_client->downloadsAuthenticationManager();
}

void DownloadManager::applicationDidEnterBackground()
{
    for (Ref download : m_downloads.values())
        download->applicationDidEnterBackground();
}

void DownloadManager::applicationWillEnterForeground()
{
    for (Ref download : m_downloads.values())
        download->applicationWillEnterForeground();
}

} // namespace WebKit
