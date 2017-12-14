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
#include "Download.h"

#include "AuthenticationManager.h"
#include "BlobDownloadClient.h"
#include "Connection.h"
#include "DataReference.h"
#include "DownloadManager.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "NetworkDataTask.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "SandboxExtension.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/NotImplemented.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#endif

using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - Download::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

#if USE(NETWORK_SESSION)
Download::Download(DownloadManager& downloadManager, DownloadID downloadID, NetworkDataTask& download, const PAL::SessionID& sessionID, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_download(&download)
    , m_sessionID(sessionID)
    , m_suggestedName(suggestedName)
{
    ASSERT(m_downloadID.downloadID());

    m_downloadManager.didCreateDownload();
}
#if PLATFORM(COCOA)
Download::Download(DownloadManager& downloadManager, DownloadID downloadID, NSURLSessionDownloadTask* download, const PAL::SessionID& sessionID, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_downloadTask(download)
    , m_sessionID(sessionID)
    , m_suggestedName(suggestedName)
{
    ASSERT(m_downloadID.downloadID());

    m_downloadManager.didCreateDownload();
}
#endif
#else
Download::Download(DownloadManager& downloadManager, DownloadID downloadID, const ResourceRequest& request, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_request(request)
    , m_suggestedName(suggestedName)
{
    ASSERT(m_downloadID.downloadID());

    m_downloadManager.didCreateDownload();
}
#endif // USE(NETWORK_SESSION)

Download::~Download()
{
#if !USE(NETWORK_SESSION)
    for (auto& fileReference : m_blobFileReferences)
        fileReference->revokeFileAccess();

    if (m_resourceHandle) {
        m_resourceHandle->clearClient();
        m_resourceHandle->cancel();
        m_resourceHandle = nullptr;
    }
    m_downloadClient = nullptr;

    platformInvalidate();
#endif

    m_downloadManager.didDestroyDownload();
}

#if !USE(NETWORK_SESSION)
void Download::start()
{
    if (m_request.url().protocolIsBlob()) {
        m_downloadClient = std::make_unique<BlobDownloadClient>(*this);
        bool defersLoading = false;
        bool shouldContentSniff = false;
        bool shouldContentEncodingSniff = true;
        m_resourceHandle = ResourceHandle::create(nullptr, m_request, m_downloadClient.get(), defersLoading, shouldContentSniff, shouldContentEncodingSniff);
        didStart();
        return;
    }

    startNetworkLoad();
}

void Download::startWithHandle(ResourceHandle* handle, const ResourceResponse& response)
{
    if (m_request.url().protocolIsBlob()) {
        m_downloadClient = std::make_unique<BlobDownloadClient>(*this);
        bool defersLoading = false;
        bool shouldContentSniff = false;
        bool shouldContentEncodingSniff = true;
        m_resourceHandle = ResourceHandle::create(nullptr, m_request, m_downloadClient.get(), defersLoading, shouldContentSniff, shouldContentEncodingSniff);
        didStart();
        return;
    }

    startNetworkLoadWithHandle(handle, response);
}
#endif

void Download::cancel()
{
#if USE(NETWORK_SESSION)
    if (m_download) {
        m_download->cancel();
        didCancel({ });
        return;
    }
#else
    if (m_request.url().protocolIsBlob()) {
        auto resourceHandle = WTFMove(m_resourceHandle);
        resourceHandle->cancel();
        static_cast<BlobDownloadClient*>(m_downloadClient.get())->didCancel();
        return;
    }
#endif
    platformCancelNetworkLoad();
}

#if USE(NETWORK_SESSION)
void Download::didReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler&& completionHandler)
{
    if (challenge.protectionSpace().isPasswordBased() && !challenge.proposedCredential().isEmpty() && !challenge.previousFailureCount()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, challenge.proposedCredential());
        return;
    }

    NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(*this, challenge, WTFMove(completionHandler));
}
#endif // USE(NETWORK_SESSION)

#if !USE(NETWORK_SESSION)
void Download::didStart()
{
    send(Messages::DownloadProxy::DidStart(m_request, m_suggestedName));
}

void Download::willSendRedirectedRequest(WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse)
{
    send(Messages::DownloadProxy::WillSendRequest(WTFMove(redirectRequest), WTFMove(redirectResponse)));
}

void Download::didReceiveAuthenticationChallenge(const AuthenticationChallenge& authenticationChallenge)
{
    m_downloadManager.downloadsAuthenticationManager().didReceiveAuthenticationChallenge(*this, authenticationChallenge);
}

void Download::didReceiveResponse(const ResourceResponse& response)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Created (id = %" PRIu64 ")", downloadID().downloadID());

    m_responseMIMEType = response.mimeType();
    send(Messages::DownloadProxy::DidReceiveResponse(response));
}

bool Download::shouldDecodeSourceDataOfMIMEType(const String& mimeType)
{
    bool result;
    if (!sendSync(Messages::DownloadProxy::ShouldDecodeSourceDataOfMIMEType(mimeType), Messages::DownloadProxy::ShouldDecodeSourceDataOfMIMEType::Reply(result)))
        return true;

    return result;
}

String Download::decideDestinationWithSuggestedFilename(const String& filename, bool& allowOverwrite)
{
    String destination;
    SandboxExtension::Handle sandboxExtensionHandle;
    if (!sendSync(Messages::DownloadProxy::DecideDestinationWithSuggestedFilename(filename, m_responseMIMEType), Messages::DownloadProxy::DecideDestinationWithSuggestedFilename::Reply(destination, allowOverwrite, sandboxExtensionHandle)))
        return String();

    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    return destination;
}

void Download::decideDestinationWithSuggestedFilenameAsync(const String& suggestedFilename)
{
    send(Messages::DownloadProxy::DecideDestinationWithSuggestedFilenameAsync(downloadID(), suggestedFilename));
}

void Download::didDecideDownloadDestination(const String& destinationPath, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    if (m_request.url().protocolIsBlob()) {
        static_cast<BlobDownloadClient*>(m_downloadClient.get())->didDecideDownloadDestination(destinationPath, allowOverwrite);
        return;
    }

    // For now, only Blob URL downloads go through this code path.
    ASSERT_NOT_REACHED();
}

void Download::continueDidReceiveResponse()
{
    m_resourceHandle->continueDidReceiveResponse();
}
#endif

void Download::didCreateDestination(const String& path)
{
    send(Messages::DownloadProxy::DidCreateDestination(path));
}

void Download::didReceiveData(uint64_t length)
{
    if (!m_hasReceivedData) {
        RELEASE_LOG_IF_ALLOWED("didReceiveData: Started receiving data (id = %" PRIu64 ")", downloadID().downloadID());
        m_hasReceivedData = true;
    }

    send(Messages::DownloadProxy::DidReceiveData(length));
}

void Download::didFinish()
{
    RELEASE_LOG_IF_ALLOWED("didFinish: (id = %" PRIu64 ")", downloadID().downloadID());

#if !USE(NETWORK_SESSION)
    platformDidFinish();
#endif

    send(Messages::DownloadProxy::DidFinish());

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }

    m_downloadManager.downloadFinished(this);
}

void Download::didFail(const ResourceError& error, const IPC::DataReference& resumeData)
{
    RELEASE_LOG_IF_ALLOWED("didFail: (id = %" PRIu64 ", isTimeout = %d, isCancellation = %d, errCode = %d)",
        downloadID().downloadID(), error.isTimeout(), error.isCancellation(), error.errorCode());

    send(Messages::DownloadProxy::DidFail(error, resumeData));

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }
    m_downloadManager.downloadFinished(this);
}

void Download::didCancel(const IPC::DataReference& resumeData)
{
    RELEASE_LOG_IF_ALLOWED("didCancel: (id = %" PRIu64 ")", downloadID().downloadID());

    send(Messages::DownloadProxy::DidCancel(resumeData));

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }
    m_downloadManager.downloadFinished(this);
}

IPC::Connection* Download::messageSenderConnection()
{
    return m_downloadManager.downloadProxyConnection();
}

uint64_t Download::messageSenderDestinationID()
{
    return m_downloadID.downloadID();
}

bool Download::isAlwaysOnLoggingAllowed() const
{
#if USE(NETWORK_SESSION) && PLATFORM(COCOA)
    return m_sessionID.isAlwaysOnLoggingAllowed();
#else
    return false;
#endif
}

#if !PLATFORM(COCOA)
void Download::platformCancelNetworkLoad()
{
}
#endif

} // namespace WebKit
