/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkDataTaskBlob.h"

#include "AuthenticationManager.h"
#include "Download.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "PrivateRelayed.h"
#include "WebErrors.h"
#include <WebCore/AsyncFileStream.h>
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/ParsedContentRange.h>
#include <WebCore/PolicyContainer.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

static const unsigned bufferSize = 512 * 1024;

static const int httpOK = 200;
static const int httpPartialContent = 206;
static constexpr auto httpOKText = "OK"_s;
static constexpr auto httpPartialContentText = "Partial Content"_s;

static constexpr auto webKitBlobResourceDomain = "WebKitBlobResource"_s;

static RefPtr<BlobData> blobDataFrom(NetworkSession& session, const WebCore::ResourceRequest& request, const SecurityOrigin* topOrigin)
{
    // We use request.firstPartyForCookies() to indicate if the request originated from the DOM or WebView API.
    ASSERT(topOrigin || request.firstPartyForCookies().isEmpty());

    std::optional<SecurityOriginData> topOriginData = topOrigin ? std::optional { topOrigin->data() } : std::nullopt;
    if (!topOriginData && !request.firstPartyForCookies().isEmpty() && request.firstPartyForCookies().isValid()) {
        RELEASE_LOG(Network, "Got request for blob without topOrigin but request specifies firstPartyForCookies");
        topOriginData = SecurityOriginData::fromURLWithoutStrictOpaqueness(request.firstPartyForCookies());
    }
    return session.blobRegistry().blobDataFromURL(request.url(), topOriginData);
}

NetworkDataTaskBlob::NetworkDataTaskBlob(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& request, const Vector<Ref<WebCore::BlobDataFileReference>>& fileReferences, const RefPtr<SecurityOrigin>& topOrigin)
    : NetworkDataTask(session, client, request, StoredCredentialsPolicy::DoNotUse, false, false, false)
    , BlobResourceHandleBase(true /* async */, blobDataFrom(session, request, topOrigin.get()))
    , m_fileReferences(fileReferences)
    , m_networkProcess(session.networkProcess())
{
    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    LOG(NetworkSession, "%p - Created NetworkDataTaskBlob for %s", this, request.url().string().utf8().data());
}

NetworkDataTaskBlob::~NetworkDataTaskBlob()
{
    for (auto& fileReference : m_fileReferences)
        fileReference->revokeFileAccess();

    clearStream();
}

void NetworkDataTaskBlob::clearStream()
{
    if (m_state == State::Completed)
        return;

    m_state = State::Completed;

    closeFileIfOpen();
    clearAsyncStream();
}

void NetworkDataTaskBlob::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    start();
}

void NetworkDataTaskBlob::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

    closeFileIfOpen();

    if (isDownload())
        cleanDownloadFiles();
}

void NetworkDataTaskBlob::invalidateAndCancel()
{
    cancel();
    clearStream();
}

bool NetworkDataTaskBlob::erroredOrAborted() const
{
    return m_state == State::Canceling || m_state == State::Completed || (!m_client && !isDownload());
}

void NetworkDataTaskBlob::didReceiveResponse(ResourceResponse&& response)
{
    NetworkDataTask::didReceiveResponse(WTF::move(response), NegotiatedLegacyTLS::No, PrivateRelayed::No, std::nullopt, [this, protectedThis = Ref { *this }](PolicyAction policyAction) {
        LOG(NetworkSession, "%p - NetworkDataTaskBlob::didReceiveResponse completionHandler (%s)", this, toString(policyAction).characters());

        if (m_state == State::Canceling || m_state == State::Completed) {
            clearStream();
            return;
        }

        switch (policyAction) {
        case PolicyAction::Use:
            buffer().resize(bufferSize);
            readAsync();
            break;
        case PolicyAction::LoadWillContinueInAnotherProcess:
            ASSERT_NOT_REACHED();
            break;
        case PolicyAction::Ignore:
            break;
        case PolicyAction::Download:
            download();
            break;
        }
    });
}

bool NetworkDataTaskBlob::didReceiveData(std::span<const uint8_t> data)
{
    ASSERT(!data.empty());
    if (m_downloadFile) {
        if (!writeDownload(data))
            return false;
    } else {
        ASSERT(m_client);
        protect(client())->didReceiveData(SharedBuffer::create(data));
    }
    return true;
}

void NetworkDataTaskBlob::setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, { }, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTF::move(sandboxExtensionHandle));
    if (RefPtr extension = m_sandboxExtension)
        extension->consume();

    if (allowOverwrite && FileSystem::fileExists(m_pendingDownloadLocation))
        FileSystem::deleteFile(m_pendingDownloadLocation);
}

String NetworkDataTaskBlob::suggestedFilename() const
{
    return m_suggestedFilename;
}

void NetworkDataTaskBlob::download()
{
    ASSERT(isDownload());
    ASSERT(m_pendingDownloadLocation);
    ASSERT(m_session);

    LOG(NetworkSession, "%p - NetworkDataTaskBlob::download to %s", this, m_pendingDownloadLocation.utf8().data());

    m_downloadFile = FileSystem::openFile(m_pendingDownloadLocation, FileSystem::FileOpenMode::Truncate);
    if (!m_downloadFile) {
        didFailDownload(cancelledError(m_firstRequest));
        return;
    }

    CheckedRef downloadManager = m_networkProcess->downloadManager();
    Ref download = Download::create(downloadManager, *m_pendingDownloadID, *this, *checkedNetworkSession(), suggestedFilename());
    downloadManager->dataTaskBecameDownloadTask(*m_pendingDownloadID, download.copyRef());
    download->didCreateDestination(m_pendingDownloadLocation);

    ASSERT(!m_client);

    buffer().resize(bufferSize);
    readAsync();
}

bool NetworkDataTaskBlob::writeDownload(std::span<const uint8_t> data)
{
    ASSERT(isDownload());
    auto bytesWritten = m_downloadFile.write(data);
    if (bytesWritten != data.size()) {
        didFailDownload(cancelledError(m_firstRequest));
        return false;
    }

    m_downloadBytesWritten += *bytesWritten;
    RefPtr download = m_networkProcess->checkedDownloadManager()->download(*m_pendingDownloadID);
    ASSERT(download);
    download->didReceiveData(*bytesWritten, m_downloadBytesWritten, totalSize());
    return true;
}

void NetworkDataTaskBlob::cleanDownloadFiles()
{
    m_downloadFile = { };
    FileSystem::deleteFile(m_pendingDownloadLocation);
}

void NetworkDataTaskBlob::didFailDownload(const ResourceError& error)
{
    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFailDownload", this);

    clearStream();
    cleanDownloadFiles();

    if (RefPtr extension = std::exchange(m_sandboxExtension, nullptr))
        extension->revoke();

    if (RefPtr client = m_client.get())
        client->didCompleteWithError(error);
    else {
        RefPtr download = m_networkProcess->checkedDownloadManager()->download(*m_pendingDownloadID);
        ASSERT(download);
        download->didFail(error, { });
    }
}

void NetworkDataTaskBlob::didFinishDownload()
{
    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFinishDownload", this);

    ASSERT(isDownload());
    m_downloadFile = { };

#if !HAVE(MODERN_DOWNLOADPROGRESS)
    if (RefPtr extension = std::exchange(m_sandboxExtension, nullptr))
        extension->revoke();
#endif

    clearStream();
    RefPtr download = m_networkProcess->checkedDownloadManager()->download(*m_pendingDownloadID);
    ASSERT(download);

#if HAVE(MODERN_DOWNLOADPROGRESS)
    if (m_sandboxExtension)
        download->setSandboxExtension(std::exchange(m_sandboxExtension, nullptr));
#endif

    download->didFinish();
}

void NetworkDataTaskBlob::didFail(Error errorCode)
{
    ASSERT(!m_sandboxExtension);

    Ref<NetworkDataTaskBlob> protectedThis(*this);
    if (isDownload()) {
        didFailDownload(ResourceError(webKitBlobResourceDomain, static_cast<int>(errorCode), m_firstRequest.url(), String()));
        return;
    }

    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFail", this);

    clearStream();
    ASSERT(m_client);
    protect(client())->didCompleteWithError(ResourceError(webKitBlobResourceDomain, static_cast<int>(errorCode), m_firstRequest.url(), String()));
}

void NetworkDataTaskBlob::didFinish()
{
    if (m_downloadFile) {
        didFinishDownload();
        return;
    }

    ASSERT(!m_sandboxExtension);

    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFinish", this);

    clearStream();
    ASSERT(m_client);
    protect(client())->didCompleteWithError({ });
}

} // namespace WebKit
