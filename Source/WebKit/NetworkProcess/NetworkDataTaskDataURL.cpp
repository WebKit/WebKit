/*
 * Copyright (C) 2023 Microsoft Corporation. All rights reserved.
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
 *     * Neither the name of Microsoft Corporation nor the names of its
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
#include "NetworkDataTaskDataURL.h"

#if USE(CURL) || USE(SOUP)

#include "AuthenticationManager.h"
#include "Download.h"
#include "NetworkLoadParameters.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <pal/text/TextEncoding.h>
#include <wtf/Vector.h>

#if USE(CURL)
#include <curl/curl.h>
#elif USE(SOUP)
#include "WebErrors.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<NetworkDataTask> NetworkDataTaskDataURL::create(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
{
    ASSERT(parameters.request.url().protocolIsData());
    return adoptRef(*new NetworkDataTaskDataURL(session, client, parameters));
}

NetworkDataTaskDataURL::NetworkDataTaskDataURL(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
    : NetworkDataTask(session, client, parameters.request, parameters.storedCredentialsPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation)
{
}

NetworkDataTaskDataURL::~NetworkDataTaskDataURL()
{
    invalidateAndCancel();
}

void NetworkDataTaskDataURL::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    DataURLDecoder::decode(firstRequest().url(), { }, DataURLDecoder::ShouldValidatePadding::Yes, [this, protectedThis = Ref { *this }](auto decodeResult) mutable {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        didDecodeDataURL(WTFMove(decodeResult));
    });
}

void NetworkDataTaskDataURL::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;
}

void NetworkDataTaskDataURL::invalidateAndCancel()
{
    cancel();
    m_state = State::Completed;
}

NetworkDataTask::State NetworkDataTaskDataURL::state() const
{
    return m_state;
}

void NetworkDataTaskDataURL::setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, WTFMove(sandboxExtensionHandle), allowOverwrite);
    m_allowOverwriteDownload = allowOverwrite;
}

String NetworkDataTaskDataURL::suggestedFilename() const
{
    if (!m_suggestedFilename.isEmpty())
        return m_suggestedFilename;

    String suggestedFilename = m_response.suggestedFilename();
    if (!suggestedFilename.isEmpty())
        return suggestedFilename;

    return PAL::decodeURLEscapeSequences(m_response.url().lastPathComponent());
}

void NetworkDataTaskDataURL::didDecodeDataURL(std::optional<WebCore::DataURLDecoder::Result>&& result)
{
    ASSERT(m_state == State::Running);
    if (!result) {
        if (m_client)
            m_client->didCompleteWithError(internalError(firstRequest().url()));
        invalidateAndCancel();
        return;
    }

    m_response = ResourceResponse::dataURLResponse(firstRequest().url(), result.value());

    didReceiveResponse(ResourceResponse(m_response), NegotiatedLegacyTLS::No, PrivateRelayed::No, std::nullopt, [this, protectedThis = Ref { *this }, data = WTFMove(result.value().data)](PolicyAction policyAction) mutable {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        switch (policyAction) {
        case PolicyAction::Use:
            // Should not be reached for data URLs as they are normally handled in the web process.
            invalidateAndCancel();
            ASSERT_NOT_REACHED();
            break;
        case PolicyAction::Ignore:
            invalidateAndCancel();
            break;
        case PolicyAction::Download:
            downloadDecodedData(WTFMove(data));
            break;
        case PolicyAction::LoadWillContinueInAnotherProcess:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

void NetworkDataTaskDataURL::downloadDecodedData(Vector<uint8_t>&& data)
{
    FileSystem::PlatformFileHandle downloadDestinationFile = FileSystem::openFile(m_pendingDownloadLocation, FileSystem::FileOpenMode::Truncate, FileSystem::FileAccessPermission::All, !m_allowOverwriteDownload);
    if (!FileSystem::isHandleValid(downloadDestinationFile)) {
#if USE(CURL)
        ResourceError error(CURLE_WRITE_ERROR, m_response.url());
#elif USE(SOUP)
        ResourceError error(downloadDestinationError(m_response, "Cannot write destination file."_s));
#endif
        if (m_client)
            m_client->didCompleteWithError(error);
        invalidateAndCancel();
        return;
    }

    auto& downloadManager = m_session->networkProcess().downloadManager();
    auto download = makeUnique<Download>(downloadManager, *m_pendingDownloadID, *this, *m_session, suggestedFilename());
    auto* downloadPtr = download.get();
    downloadManager.dataTaskBecameDownloadTask(*m_pendingDownloadID, WTFMove(download));
    downloadPtr->didCreateDestination(m_pendingDownloadLocation);

    if (-1 == FileSystem::writeToFile(downloadDestinationFile, data.span())) {
        FileSystem::closeFile(downloadDestinationFile);
        FileSystem::deleteFile(m_pendingDownloadLocation);
#if USE(CURL)
        ResourceError error(CURLE_WRITE_ERROR, m_response.url());
#elif USE(SOUP)
        ResourceError error(downloadDestinationError(m_response, "Cannot write destination file."_s));
#endif
        downloadPtr->didFail(error, { });
        invalidateAndCancel();
        return;
    }

    downloadPtr->didReceiveData(data.size(), 0, 0);
    FileSystem::closeFile(downloadDestinationFile);
    downloadPtr->didFinish();
    m_state = State::Completed;
}

} // namespace WebKit

#endif // USE(CURL) || USE(SOUP)
