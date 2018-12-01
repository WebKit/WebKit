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

#include "DataReference.h"
#include "Download.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "WebErrors.h"
#include <WebCore/AsyncFileStream.h>
#include <WebCore/BlobData.h>
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/FileStream.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/ParsedContentRange.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

static const unsigned bufferSize = 512 * 1024;

static const int httpOK = 200;
static const int httpPartialContent = 206;
static const int httpNotAllowed = 403;
static const int httpRequestedRangeNotSatisfiable = 416;
static const int httpInternalError = 500;
static const char* httpOKText = "OK";
static const char* httpPartialContentText = "Partial Content";
static const char* httpNotAllowedText = "Not Allowed";
static const char* httpRequestedRangeNotSatisfiableText = "Requested Range Not Satisfiable";
static const char* httpInternalErrorText = "Internal Server Error";

static const char* const webKitBlobResourceDomain = "WebKitBlobResource";

NetworkDataTaskBlob::NetworkDataTaskBlob(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& request, ContentSniffingPolicy shouldContentSniff, const Vector<RefPtr<WebCore::BlobDataFileReference>>& fileReferences)
    : NetworkDataTask(session, client, request, StoredCredentialsPolicy::DoNotUse, false, false)
    , m_stream(std::make_unique<AsyncFileStream>(*this))
    , m_fileReferences(fileReferences)
{
    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    m_blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(request.url());

    m_session->registerNetworkDataTask(*this);
    LOG(NetworkSession, "%p - Created NetworkDataTaskBlob for %s", this, request.url().string().utf8().data());
}

NetworkDataTaskBlob::~NetworkDataTaskBlob()
{
    for (auto& fileReference : m_fileReferences)
        fileReference->revokeFileAccess();

    clearStream();
    m_session->unregisterNetworkDataTask(*this);
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

    if (m_scheduledFailureType != NoFailure) {
        ASSERT(m_failureTimer.isActive());
        return;
    }

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] {
        if (m_state == State::Canceling || m_state == State::Completed || !m_client) {
            clearStream();
            return;
        }

        if (!equalLettersIgnoringASCIICase(m_firstRequest.httpMethod(), "get")) {
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
        if (!range.isEmpty() && !parseRange(range, m_rangeOffset, m_rangeEnd, m_rangeSuffixLength)) {
            dispatchDidReceiveResponse(Error::RangeError);
            return;
        }

        getSizeForNext();
    });
}

void NetworkDataTaskBlob::suspend()
{
    // FIXME: can this happen?
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
        seek();
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

void NetworkDataTaskBlob::seek()
{
    ASSERT(RunLoop::isMain());

    // Convert from the suffix length to the range.
    if (m_rangeSuffixLength != kPositionNotSpecified) {
        m_rangeOffset = m_totalRemainingSize - m_rangeSuffixLength;
        m_rangeEnd = m_rangeOffset + m_rangeSuffixLength - 1;
    }

    // Bail out if the range is not provided.
    if (m_rangeOffset == kPositionNotSpecified)
        return;

    // Skip the initial items that are not in the range.
    long long offset = m_rangeOffset;
    for (m_readItemCount = 0; m_readItemCount < m_blobData->items().size() && offset >= m_itemLengthList[m_readItemCount]; ++m_readItemCount)
        offset -= m_itemLengthList[m_readItemCount];

    // Set the offset that need to jump to for the first item in the range.
    m_currentItemReadSize = offset;

    // Adjust the total remaining size in order not to go beyond the range.
    if (m_rangeEnd != kPositionNotSpecified) {
        long long rangeSize = m_rangeEnd - m_rangeOffset + 1;
        if (m_totalRemainingSize > rangeSize)
            m_totalRemainingSize = rangeSize;
    } else
        m_totalRemainingSize -= m_rangeOffset;
}

void NetworkDataTaskBlob::dispatchDidReceiveResponse(Error errorCode)
{
    LOG(NetworkSession, "%p - NetworkDataTaskBlob::dispatchDidReceiveResponse(%u)", this, static_cast<unsigned>(errorCode));

    Ref<NetworkDataTaskBlob> protectedThis(*this);
    ResourceResponse response(m_firstRequest.url(), errorCode != Error::NoError ? "text/plain" : m_blobData->contentType(), errorCode != Error::NoError ? 0 : m_totalRemainingSize, String());
    switch (errorCode) {
    case Error::NoError: {
        bool isRangeRequest = m_rangeOffset != kPositionNotSpecified;
        response.setHTTPStatusCode(isRangeRequest ? httpPartialContent : httpOK);
        response.setHTTPStatusText(isRangeRequest ? httpPartialContentText : httpOKText);

        response.setHTTPHeaderField(HTTPHeaderName::ContentType, m_blobData->contentType());
        response.setHTTPHeaderField(HTTPHeaderName::ContentLength, String::number(m_totalRemainingSize));

        if (isRangeRequest)
            response.setHTTPHeaderField(HTTPHeaderName::ContentRange, ParsedContentRange(m_rangeOffset, m_rangeEnd, m_totalSize).headerValue());
        // FIXME: If a resource identified with a blob: URL is a File object, user agents must use that file's name attribute,
        // as if the response had a Content-Disposition header with the filename parameter set to the File's name attribute.
        // Notably, this will affect a name suggested in "File Save As".
        break;
    }
    case Error::RangeError:
        response.setHTTPStatusCode(httpRequestedRangeNotSatisfiable);
        response.setHTTPStatusText(httpRequestedRangeNotSatisfiableText);
        break;
    case Error::SecurityError:
        response.setHTTPStatusCode(httpNotAllowed);
        response.setHTTPStatusText(httpNotAllowedText);
        break;
    default:
        response.setHTTPStatusCode(httpInternalError);
        response.setHTTPStatusText(httpInternalErrorText);
        break;
    }

    didReceiveResponse(WTFMove(response), [this, protectedThis = WTFMove(protectedThis), errorCode](PolicyAction policyAction) {
        LOG(NetworkSession, "%p - NetworkDataTaskBlob::didReceiveResponse completionHandler (%u)", this, static_cast<unsigned>(policyAction));

        if (m_state == State::Canceling || m_state == State::Completed) {
            clearStream();
            return;
        }

        if (errorCode != Error::NoError) {
            didFinish();
            return;
        }

        switch (policyAction) {
        case PolicyAction::Use:
            m_buffer.resize(bufferSize);
            read();
            break;
        case PolicyAction::Suspend:
            LOG_ERROR("PolicyAction::Suspend encountered - Treating as PolicyAction::Ignore for now");
            FALLTHROUGH;
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
    ASSERT(item.data().data());

    long long bytesToRead = item.length() - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = m_totalRemainingSize;
    consumeData(reinterpret_cast<const char*>(item.data().data()->data()) + item.offset() + m_currentItemReadSize, static_cast<int>(bytesToRead));
    m_currentItemReadSize = 0;
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
    consumeData(m_buffer.data(), bytesRead);
}

void NetworkDataTaskBlob::consumeData(const char* data, int bytesRead)
{
    m_totalRemainingSize -= bytesRead;

    if (bytesRead) {
        if (m_downloadFile != FileSystem::invalidPlatformFileHandle) {
            if (!writeDownload(data, bytesRead))
                return;
        } else {
            ASSERT(m_client);
            m_client->didReceiveData(SharedBuffer::create(data, bytesRead));
        }
    }

    if (m_fileOpened) {
        // When the current item is a file item, the reading is completed only if bytesRead is 0.
        if (!bytesRead) {
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
    if (!m_suggestedFilename.isEmpty())
        return m_suggestedFilename;

    return "unknown"_s;
}

void NetworkDataTaskBlob::download()
{
    ASSERT(isDownload());
    ASSERT(m_pendingDownloadLocation);

    LOG(NetworkSession, "%p - NetworkDataTaskBlob::download to %s", this, m_pendingDownloadLocation.utf8().data());

    m_downloadFile = FileSystem::openFile(m_pendingDownloadLocation, FileSystem::FileOpenMode::Write);
    if (m_downloadFile == FileSystem::invalidPlatformFileHandle) {
        didFailDownload(cancelledError(m_firstRequest));
        return;
    }

    auto& downloadManager = NetworkProcess::singleton().downloadManager();
    auto download = std::make_unique<Download>(downloadManager, m_pendingDownloadID, *this, m_session->sessionID(), suggestedFilename());
    auto* downloadPtr = download.get();
    downloadManager.dataTaskBecameDownloadTask(m_pendingDownloadID, WTFMove(download));
    downloadPtr->didCreateDestination(m_pendingDownloadLocation);

    ASSERT(!m_client);

    m_buffer.resize(bufferSize);
    read();
}

bool NetworkDataTaskBlob::writeDownload(const char* data, int bytesRead)
{
    ASSERT(isDownload());
    int bytesWritten = FileSystem::writeToFile(m_downloadFile, data, bytesRead);
    if (bytesWritten == -1) {
        didFailDownload(cancelledError(m_firstRequest));
        return false;
    }

    ASSERT(bytesWritten == bytesRead);
    auto* download = NetworkProcess::singleton().downloadManager().download(m_pendingDownloadID);
    ASSERT(download);
    download->didReceiveData(bytesWritten);
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
        auto* download = NetworkProcess::singleton().downloadManager().download(m_pendingDownloadID);
        ASSERT(download);
        download->didFail(error, IPC::DataReference());
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
    auto* download = NetworkProcess::singleton().downloadManager().download(m_pendingDownloadID);
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
