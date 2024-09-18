/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include <WebCore/HTTPParsers.h>
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

NetworkDataTaskBlob::NetworkDataTaskBlob(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& request, const Vector<RefPtr<WebCore::BlobDataFileReference>>& fileReferences, const RefPtr<SecurityOrigin>& topOrigin)
    : NetworkDataTask(session, client, request, StoredCredentialsPolicy::DoNotUse, false, false)
    , m_stream(makeUnique<AsyncFileStream>(*this))
    , m_fileReferences(fileReferences)
    , m_networkProcess(session.networkProcess())
{
    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    // We use request.firstPartyForCookies() to indicate if the request originated from the DOM or WebView API.
    ASSERT(topOrigin || request.firstPartyForCookies().isEmpty());
    std::optional<SecurityOriginData> topOriginData = topOrigin ? std::optional { topOrigin->data() } : std::nullopt;
    if (!topOriginData && !request.firstPartyForCookies().isEmpty() && request.firstPartyForCookies().isValid()) {
        RELEASE_LOG(Network, "Got request for blob without topOrigin but request specifies firstPartyForCookies");
        topOriginData = SecurityOriginData::fromURLWithoutStrictOpaqueness(request.firstPartyForCookies());
    }
    m_blobData = session.blobRegistry().getBlobDataFromURL(request.url(), topOriginData);

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

    if (m_fileOpened) {
        m_fileOpened = false;
        m_stream->close();
    }
    m_stream = nullptr;
}

void NetworkDataTaskBlob::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    RunLoop::main().dispatch([this, protectedThis = Ref { *this }] {
        if (m_state == State::Canceling || m_state == State::Completed || !m_client) {
            clearStream();
            return;
        }

        if (!equalLettersIgnoringASCIICase(m_firstRequest.httpMethod(), "get"_s)) {
            didFail(Error::MethodNotAllowed);
            return;
        }

        // If the blob data is not found, fail now.
        if (!m_blobData) {
            didFail(Error::NotFoundError);
            return;
        }

        // Parse the "Range" header we care about.
        String range = m_firstRequest.httpHeaderField(HTTPHeaderName::Range);
        m_isRangeRequest = !range.isNull();
        if (m_isRangeRequest && !parseRange(range, RangeAllowWhitespace::Yes, m_rangeStart, m_rangeEnd)) {
            didFail(Error::RangeError);
            return;
        }

        getSizeForNext();
    });
}

void NetworkDataTaskBlob::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

    if (m_fileOpened) {
        m_fileOpened = false;
        m_stream->close();
    }

    if (isDownload())
        cleanDownloadFiles();
}

void NetworkDataTaskBlob::invalidateAndCancel()
{
    cancel();
    clearStream();
}

void NetworkDataTaskBlob::getSizeForNext()
{
    ASSERT(RunLoop::isMain());

    // Do we finish validating and counting size for all items?
    if (m_sizeItemCount >= m_blobData->items().size()) {
        if (auto error = seek()) {
            didFail(*error);
            return;
        }
        dispatchDidReceiveResponse();
        return;
    }

    const BlobDataItem& item = m_blobData->items().at(m_sizeItemCount);
    switch (item.type()) {
    case BlobDataItem::Type::Data:
        didGetSize(item.length());
        break;
    case BlobDataItem::Type::File:
        // Files know their sizes, but asking the stream to verify that the file wasn't modified.
        m_stream->getSize(item.file()->path(), item.file()->expectedModificationTime());
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void NetworkDataTaskBlob::didGetSize(long long size)
{
    ASSERT(RunLoop::isMain());
    Ref protectedThis { *this };

    if (m_state == State::Canceling || m_state == State::Completed || (!m_client && !isDownload())) {
        clearStream();
        return;
    }

    // If the size is -1, it means the file has been moved or changed. Fail now.
    if (size == -1) {
        didFail(Error::NotFoundError);
        return;
    }

    // The size passed back is the size of the whole file. If the underlying item is a sliced file, we need to use the slice length.
    const BlobDataItem& item = m_blobData->items().at(m_sizeItemCount);
    size = item.length();

    // Cache the size.
    m_itemLengthList.append(size);

    // Count the size.
    m_totalSize += size;
    m_totalRemainingSize += size;
    m_sizeItemCount++;

    // Continue with the next item.
    getSizeForNext();
}

auto NetworkDataTaskBlob::seek() -> std::optional<Error>
{
    ASSERT(RunLoop::isMain());

    // Bail out if the range is not provided.
    if (!m_isRangeRequest)
        return std::nullopt;

    // Adjust m_rangeStart / m_rangeEnd
    if (m_rangeStart == kPositionNotSpecified) {
        m_rangeStart = m_totalSize - m_rangeEnd;
        m_rangeEnd = m_rangeStart + m_rangeEnd - 1;
    } else {
        if (m_rangeStart >= m_totalSize)
            return Error::RangeError;
        if (m_rangeEnd == kPositionNotSpecified || m_rangeEnd >= m_totalSize)
            m_rangeEnd = m_totalSize - 1;
    }

    // Skip the initial items that are not in the range.
    long long offset = m_rangeStart;
    for (m_readItemCount = 0; m_readItemCount < m_blobData->items().size() && offset >= m_itemLengthList[m_readItemCount]; ++m_readItemCount)
        offset -= m_itemLengthList[m_readItemCount];

    // Set the offset that need to jump to for the first item in the range.
    m_currentItemReadSize = offset;

    // Adjust the total remaining size in order not to go beyond the range.
    long long rangeSize = m_rangeEnd - m_rangeStart + 1;
    if (m_totalRemainingSize > rangeSize)
        m_totalRemainingSize = rangeSize;
    return std::nullopt;
}

void NetworkDataTaskBlob::dispatchDidReceiveResponse()
{
    LOG(NetworkSession, "%p - NetworkDataTaskBlob::dispatchDidReceiveResponse()", this);

    Ref<NetworkDataTaskBlob> protectedThis(*this);
    ResourceResponse response(m_firstRequest.url(), extractMIMETypeFromMediaType(m_blobData->contentType()), m_totalRemainingSize, String());
    response.setHTTPStatusCode(m_isRangeRequest ? httpPartialContent : httpOK);
    response.setHTTPStatusText(m_isRangeRequest ? httpPartialContentText : httpOKText);

    response.setHTTPHeaderField(HTTPHeaderName::ContentType, m_blobData->contentType());
    response.setTextEncodingName(extractCharsetFromMediaType(m_blobData->contentType()).toString());
    response.setHTTPHeaderField(HTTPHeaderName::ContentLength, String::number(m_totalRemainingSize));
    addPolicyContainerHeaders(response, m_blobData->policyContainer());

    if (m_isRangeRequest)
        response.setHTTPHeaderField(HTTPHeaderName::ContentRange, ParsedContentRange(m_rangeStart, m_rangeEnd, m_totalSize).headerValue());

    // FIXME: If a resource identified with a blob: URL is a File object, user agents must use that file's name attribute,
    // as if the response had a Content-Disposition header with the filename parameter set to the File's name attribute.
    // Notably, this will affect a name suggested in "File Save As".

    didReceiveResponse(WTFMove(response), NegotiatedLegacyTLS::No, PrivateRelayed::No, std::nullopt, [this, protectedThis = WTFMove(protectedThis)](PolicyAction policyAction) {
        LOG(NetworkSession, "%p - NetworkDataTaskBlob::didReceiveResponse completionHandler (%u)", this, static_cast<unsigned>(policyAction));

        if (m_state == State::Canceling || m_state == State::Completed) {
            clearStream();
            return;
        }

        switch (policyAction) {
        case PolicyAction::Use:
            m_buffer.resize(bufferSize);
            read();
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

void NetworkDataTaskBlob::read()
{
    ASSERT(RunLoop::isMain());

    // If there is no more remaining data to read, we are done.
    if (!m_totalRemainingSize || m_readItemCount >= m_blobData->items().size()) {
        didFinish();
        return;
    }

    const BlobDataItem& item = m_blobData->items().at(m_readItemCount);
    if (item.type() == BlobDataItem::Type::Data)
        readData(item);
    else if (item.type() == BlobDataItem::Type::File)
        readFile(item);
    else
        ASSERT_NOT_REACHED();
}

void NetworkDataTaskBlob::readData(const BlobDataItem& item)
{
    ASSERT(item.data());

    long long bytesToRead = item.length() - m_currentItemReadSize;
    ASSERT(bytesToRead >= 0);
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = m_totalRemainingSize;

    auto data = item.data()->span().subspan(item.offset() + m_currentItemReadSize, static_cast<size_t>(bytesToRead));
    m_currentItemReadSize = 0;

    consumeData(data);
}

void NetworkDataTaskBlob::readFile(const BlobDataItem& item)
{
    ASSERT(m_stream);

    if (m_fileOpened) {
        m_stream->read(m_buffer.data(), m_buffer.size());
        return;
    }

    long long bytesToRead = m_itemLengthList[m_readItemCount] - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = static_cast<int>(m_totalRemainingSize);
    m_stream->openForRead(item.file()->path(), item.offset() + m_currentItemReadSize, bytesToRead);
    m_fileOpened = true;
    m_currentItemReadSize = 0;
}

void NetworkDataTaskBlob::didOpen(bool success)
{
    if (m_state == State::Canceling || m_state == State::Completed || (!m_client && !isDownload())) {
        clearStream();
        return;
    }

    if (!success) {
        didFail(Error::NotReadableError);
        return;
    }

    Ref<NetworkDataTaskBlob> protectedThis(*this);
    read();
}

void NetworkDataTaskBlob::didRead(int bytesRead)
{
    if (m_state == State::Canceling || m_state == State::Completed || (!m_client && !isDownload())) {
        clearStream();
        return;
    }

    if (bytesRead < 0) {
        didFail(Error::NotReadableError);
        return;
    }

    Ref<NetworkDataTaskBlob> protectedThis(*this);
    consumeData(m_buffer.subspan(0, bytesRead));
}

void NetworkDataTaskBlob::consumeData(std::span<const uint8_t> data)
{
    m_totalRemainingSize -= data.size();

    if (!data.empty()) {
        if (m_downloadFile != FileSystem::invalidPlatformFileHandle) {
            if (!writeDownload(data))
                return;
        } else {
            ASSERT(m_client);
            m_client->didReceiveData(SharedBuffer::create(data));
        }
    }

    if (m_fileOpened) {
        // When the current item is a file item, the reading is completed only if bytesRead is 0.
        if (data.empty()) {
            // Close the file.
            m_fileOpened = false;
            m_stream->close();

            // Move to the next item.
            m_readItemCount++;
        }
    } else {
        // Otherwise, we read the current text item as a whole and move to the next item.
        m_readItemCount++;
    }

    read();
}

void NetworkDataTaskBlob::setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, { }, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

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
    if (m_downloadFile == FileSystem::invalidPlatformFileHandle) {
        didFailDownload(cancelledError(m_firstRequest));
        return;
    }

    auto& downloadManager = m_networkProcess->downloadManager();
    auto download = makeUnique<Download>(downloadManager, *m_pendingDownloadID, *this, *m_session, suggestedFilename());
    auto* downloadPtr = download.get();
    downloadManager.dataTaskBecameDownloadTask(*m_pendingDownloadID, WTFMove(download));
    downloadPtr->didCreateDestination(m_pendingDownloadLocation);

    ASSERT(!m_client);

    m_buffer.resize(bufferSize);
    read();
}

bool NetworkDataTaskBlob::writeDownload(std::span<const uint8_t> data)
{
    ASSERT(isDownload());
    int bytesWritten = FileSystem::writeToFile(m_downloadFile, data);
    if (static_cast<size_t>(bytesWritten) != data.size()) {
        didFailDownload(cancelledError(m_firstRequest));
        return false;
    }

    m_downloadBytesWritten += bytesWritten;
    auto* download = m_networkProcess->downloadManager().download(*m_pendingDownloadID);
    ASSERT(download);
    download->didReceiveData(bytesWritten, m_downloadBytesWritten, m_totalSize);
    return true;
}

void NetworkDataTaskBlob::cleanDownloadFiles()
{
    if (m_downloadFile != FileSystem::invalidPlatformFileHandle) {
        FileSystem::closeFile(m_downloadFile);
        m_downloadFile = FileSystem::invalidPlatformFileHandle;
    }
    FileSystem::deleteFile(m_pendingDownloadLocation);
}

void NetworkDataTaskBlob::didFailDownload(const ResourceError& error)
{
    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFailDownload", this);

    clearStream();
    cleanDownloadFiles();

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }

    if (m_client)
        m_client->didCompleteWithError(error);
    else {
        auto* download = m_networkProcess->downloadManager().download(*m_pendingDownloadID);
        ASSERT(download);
        download->didFail(error, { });
    }
}

void NetworkDataTaskBlob::didFinishDownload()
{
    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFinishDownload", this);

    ASSERT(isDownload());
    FileSystem::closeFile(m_downloadFile);
    m_downloadFile = FileSystem::invalidPlatformFileHandle;

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }

    clearStream();
    auto* download = m_networkProcess->downloadManager().download(*m_pendingDownloadID);
    ASSERT(download);
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
    m_client->didCompleteWithError(ResourceError(webKitBlobResourceDomain, static_cast<int>(errorCode), m_firstRequest.url(), String()));
}

void NetworkDataTaskBlob::didFinish()
{
    if (m_downloadFile != FileSystem::invalidPlatformFileHandle) {
        didFinishDownload();
        return;
    }

    ASSERT(!m_sandboxExtension);

    LOG(NetworkSession, "%p - NetworkDataTaskBlob::didFinish", this);

    clearStream();
    ASSERT(m_client);
    m_client->didCompleteWithError({ });
}

} // namespace WebKit
