/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "BlobResourceHandle.h"

#include "AsyncFileStream.h"
#include "BlobData.h"
#include "FileStream.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ParsedContentRange.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>
#include <wtf/URL.h>

namespace WebCore {

static const unsigned bufferSize = 512 * 1024;

static const int httpOK = 200;
static const int httpPartialContent = 206;
static constexpr auto httpOKText = "OK"_s;
static constexpr auto httpPartialContentText = "Partial Content"_s;

static constexpr auto webKitBlobResourceDomain = "WebKitBlobResource"_s;

///////////////////////////////////////////////////////////////////////////////
// BlobResourceSynchronousLoader

namespace {

class BlobResourceSynchronousLoader : public ResourceHandleClient {
public:
    BlobResourceSynchronousLoader(ResourceError&, ResourceResponse&, Vector<uint8_t>&);

    void didReceiveResponseAsync(ResourceHandle*, ResourceResponse&&, CompletionHandler<void()>&&) final;
    void didFail(ResourceHandle*, const ResourceError&) final;
    void willSendRequestAsync(ResourceHandle*, ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&&) final;
#endif

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<uint8_t>& m_data;
};

BlobResourceSynchronousLoader::BlobResourceSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
    : m_error(error)
    , m_response(response)
    , m_data(data)
{
}

void BlobResourceSynchronousLoader::willSendRequestAsync(ResourceHandle*, ResourceRequest&& request, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(WTFMove(request));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void BlobResourceSynchronousLoader::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(false);
}
#endif

void BlobResourceSynchronousLoader::didReceiveResponseAsync(ResourceHandle* handle, ResourceResponse&& response, CompletionHandler<void()>&& completionHandler)
{
    // We cannot handle the size that is more than maximum integer.
    if (response.expectedContentLength() > INT_MAX) {
        m_error = ResourceError(webKitBlobResourceDomain, static_cast<int>(BlobResourceHandle::Error::NotReadableError), response.url(), "File is too large"_s);
        completionHandler();
        return;
    }

    m_response = response;

    // Read all the data.
    m_data.resize(static_cast<size_t>(response.expectedContentLength()));
    static_cast<BlobResourceHandle*>(handle)->readSync(m_data.data(), static_cast<int>(m_data.size()));
    completionHandler();
}

void BlobResourceSynchronousLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    m_error = error;
}

}

///////////////////////////////////////////////////////////////////////////////
// BlobResourceHandle

Ref<BlobResourceHandle> BlobResourceHandle::createAsync(BlobData* blobData, const ResourceRequest& request, ResourceHandleClient* client)
{
    return adoptRef(*new BlobResourceHandle(blobData, request, client, true));
}

void BlobResourceHandle::loadResourceSynchronously(BlobData* blobData, const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get"_s)) {
        error = ResourceError(webKitBlobResourceDomain, static_cast<int>(Error::MethodNotAllowed), response.url(), "Request method must be GET"_s);
        return;
    }

    BlobResourceSynchronousLoader loader(error, response, data);
    RefPtr<BlobResourceHandle> handle = adoptRef(new BlobResourceHandle(blobData, request, &loader, false));
    handle->start();
}

BlobResourceHandle::BlobResourceHandle(BlobData* blobData, const ResourceRequest& request, ResourceHandleClient* client, bool async)
    : ResourceHandle { nullptr, request, client, false /* defersLoading */, false /* shouldContentSniff */, ContentEncodingSniffingPolicy::Default, nullptr /* sourceOrigin */, false /* isMainFrameNavigation */ }
    , m_blobData { blobData }
    , m_async { async }
{
    if (m_async)
        m_asyncStream = makeUnique<AsyncFileStream>(*this);
    else
        m_stream = makeUnique<FileStream>();
}

BlobResourceHandle::~BlobResourceHandle() = default;

void BlobResourceHandle::cancel()
{
    m_asyncStream = nullptr;
    m_fileOpened = false;

    m_aborted = true;

    ResourceHandle::cancel();
}

void BlobResourceHandle::start()
{
    if (!m_async) {
        doStart();
        return;
    }

    // Finish this async call quickly and return.
    callOnMainThread([protectedThis = Ref { *this }]() mutable {
        protectedThis->doStart();
    });
}

void BlobResourceHandle::doStart()
{
    ASSERT(isMainThread());

    // Do not continue if the request is aborted or an error occurs.
    if (erroredOrAborted())
        return;

    if (!equalLettersIgnoringASCIICase(firstRequest().httpMethod(), "get"_s)) {
        notifyFail(Error::MethodNotAllowed);
        return;
    }

    // If the blob data is not found, fail now.
    if (!m_blobData) {
        notifyFail(Error::NotFoundError);
        return;
    }

    // Parse the "Range" header we care about.
    String range = firstRequest().httpHeaderField(HTTPHeaderName::Range);
    m_isRangeRequest = !range.isNull();
    if (m_isRangeRequest && !parseRange(range, RangeAllowWhitespace::Yes, m_rangeStart, m_rangeEnd)) {
        notifyFail(Error::RangeError);
        return;
    }

    if (m_async)
        getSizeForNext();
    else {
        Ref<BlobResourceHandle> protectedThis(*this); // getSizeForNext calls the client
        for (size_t i = 0; i < m_blobData->items().size() && !erroredOrAborted(); ++i)
            getSizeForNext();
        notifyResponse();
    }
}

void BlobResourceHandle::getSizeForNext()
{
    ASSERT(isMainThread());

    // Do we finish validating and counting size for all items?
    if (m_sizeItemCount >= m_blobData->items().size()) {
        seek();

        // Start reading if in asynchronous mode.
        if (m_async) {
            Ref<BlobResourceHandle> protectedThis(*this);
            notifyResponse();
        }
        return;
    }

    const BlobDataItem& item = m_blobData->items().at(m_sizeItemCount);
    switch (item.type()) {
    case BlobDataItem::Type::Data:
        didGetSize(item.length());
        break;
    case BlobDataItem::Type::File:
        // Files know their sizes, but asking the stream to verify that the file wasn't modified.
        if (m_async)
            m_asyncStream->getSize(item.file()->path(), item.file()->expectedModificationTime());
        else
            didGetSize(m_stream->getSize(item.file()->path(), item.file()->expectedModificationTime()));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void BlobResourceHandle::didGetSize(long long size)
{
    ASSERT(isMainThread());

    // Do not continue if the request is aborted or an error occurs.
    if (erroredOrAborted())
        return;

    // If the size is -1, it means the file has been moved or changed. Fail now.
    if (size == -1) {
        notifyFail(Error::NotFoundError);
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

void BlobResourceHandle::seek()
{
    ASSERT(isMainThread());

    // Bail out if the range is not provided.
    if (!m_isRangeRequest)
        return;

    // Adjust m_rangeStart / m_rangeEnd
    if (m_rangeStart == kPositionNotSpecified) {
        m_rangeStart = m_totalSize - m_rangeEnd;
        m_rangeEnd = m_rangeStart + m_rangeEnd - 1;
    } else {
        if (m_rangeStart >= m_totalSize) {
            notifyFail(Error::RangeError);
            return;
        }
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
}

int BlobResourceHandle::readSync(uint8_t* buf, int length)
{
    ASSERT(isMainThread());

    ASSERT(!m_async);
    Ref<BlobResourceHandle> protectedThis(*this);

    int offset = 0;
    int remaining = length;
    while (remaining) {
        // Do not continue if the request is aborted or an error occurs.
        if (erroredOrAborted())
            break;

        // If there is no more remaining data to read, we are done.
        if (!m_totalRemainingSize || m_readItemCount >= m_blobData->items().size())
            break;

        const BlobDataItem& item = m_blobData->items().at(m_readItemCount);
        int bytesRead = 0;
        if (item.type() == BlobDataItem::Type::Data)
            bytesRead = readDataSync(item, buf + offset, remaining);
        else if (item.type() == BlobDataItem::Type::File)
            bytesRead = readFileSync(item, buf + offset, remaining);
        else
            ASSERT_NOT_REACHED();

        if (bytesRead > 0) {
            offset += bytesRead;
            remaining -= bytesRead;
        }
    }

    int result;
    if (erroredOrAborted())
        result = -1;
    else
        result = length - remaining;

    if (result > 0)
        notifyReceiveData(buf, result);

    if (!result)
        notifyFinish();

    return result;
}

int BlobResourceHandle::readDataSync(const BlobDataItem& item, void* buf, int length)
{
    ASSERT(isMainThread());

    ASSERT(!m_async);

    long long remaining = item.length() - m_currentItemReadSize;
    int bytesToRead = (length > remaining) ? static_cast<int>(remaining) : length;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = static_cast<int>(m_totalRemainingSize);
    memcpy(buf, item.data()->data() + item.offset() + m_currentItemReadSize, bytesToRead);
    m_totalRemainingSize -= bytesToRead;

    m_currentItemReadSize += bytesToRead;
    if (m_currentItemReadSize == item.length()) {
        m_readItemCount++;
        m_currentItemReadSize = 0;
    }

    return bytesToRead;
}

int BlobResourceHandle::readFileSync(const BlobDataItem& item, void* buf, int length)
{
    ASSERT(isMainThread());

    ASSERT(!m_async);

    if (!m_fileOpened) {
        long long bytesToRead = m_itemLengthList[m_readItemCount] - m_currentItemReadSize;
        if (bytesToRead > m_totalRemainingSize)
            bytesToRead = m_totalRemainingSize;
        bool success = m_stream->openForRead(item.file()->path(), item.offset() + m_currentItemReadSize, bytesToRead);
        m_currentItemReadSize = 0;
        if (!success) {
            m_errorCode = Error::NotReadableError;
            return 0;
        }

        m_fileOpened = true;
    }

    int bytesRead = m_stream->read(buf, length);
    if (bytesRead < 0) {
        m_errorCode = Error::NotReadableError;
        return 0;
    }
    if (!bytesRead) {
        m_stream->close();
        m_fileOpened = false;
        m_readItemCount++;
    } else
        m_totalRemainingSize -= bytesRead;

    return bytesRead;
}

void BlobResourceHandle::readAsync()
{
    ASSERT(isMainThread());

    // Do not continue if the request is aborted or an error occurs.
    if (erroredOrAborted())
        return;

    // If there is no more remaining data to read, we are done.
    if (!m_totalRemainingSize || m_readItemCount >= m_blobData->items().size()) {
        notifyFinish();
        return;
    }

    const BlobDataItem& item = m_blobData->items().at(m_readItemCount);
    if (item.type() == BlobDataItem::Type::Data)
        readDataAsync(item);
    else if (item.type() == BlobDataItem::Type::File)
        readFileAsync(item);
    else
        ASSERT_NOT_REACHED();
}

void BlobResourceHandle::readDataAsync(const BlobDataItem& item)
{
    ASSERT(isMainThread());
    ASSERT(item.data());

    Ref<BlobResourceHandle> protectedThis(*this);

    long long bytesToRead = item.length() - m_currentItemReadSize;
    ASSERT(bytesToRead >= 0);
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = m_totalRemainingSize;

    auto* data = item.data()->data() + item.offset() + m_currentItemReadSize;
    m_currentItemReadSize = 0;

    consumeData(data, static_cast<int>(bytesToRead));
}

void BlobResourceHandle::readFileAsync(const BlobDataItem& item)
{
    ASSERT(isMainThread());

    if (m_fileOpened) {
        m_asyncStream->read(m_buffer.data(), m_buffer.size());
        return;
    }

    long long bytesToRead = m_itemLengthList[m_readItemCount] - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = static_cast<int>(m_totalRemainingSize);
    m_asyncStream->openForRead(item.file()->path(), item.offset() + m_currentItemReadSize, bytesToRead);
    m_fileOpened = true;
    m_currentItemReadSize = 0;
}

void BlobResourceHandle::didOpen(bool success)
{
    ASSERT(m_async);

    if (!success) {
        failed(Error::NotReadableError);
        return;
    }

    // Continue the reading.
    readAsync();
}

void BlobResourceHandle::didRead(int bytesRead)
{
    if (bytesRead < 0) {
        failed(Error::NotReadableError);
        return;
    }

    consumeData(m_buffer.data(), bytesRead);
}

void BlobResourceHandle::consumeData(const uint8_t* data, int bytesRead)
{
    ASSERT(m_async);
    Ref<BlobResourceHandle> protectedThis(*this);

    m_totalRemainingSize -= bytesRead;

    // Notify the client.
    if (bytesRead)
        notifyReceiveData(data, bytesRead);

    if (m_fileOpened) {
        // When the current item is a file item, the reading is completed only if bytesRead is 0.
        if (!bytesRead) {
            // Close the file.
            m_fileOpened = false;
            m_asyncStream->close();

            // Move to the next item.
            m_readItemCount++;
        }
    } else {
        // Otherwise, we read the current text item as a whole and move to the next item.
        m_readItemCount++;
    }

    // Continue the reading.
    readAsync();
}

void BlobResourceHandle::failed(Error errorCode)
{
    ASSERT(m_async);
    Ref<BlobResourceHandle> protectedThis(*this);

    // Notify the client.
    notifyFail(errorCode);

    // Close the file if needed.
    if (m_fileOpened) {
        m_fileOpened = false;
        m_asyncStream->close();
    }
}

void BlobResourceHandle::notifyResponse()
{
    if (!client())
        return;

    if (m_errorCode != Error::NoError) {
        notifyFail(m_errorCode);
        return;
    }

    notifyResponseOnSuccess();
}

void BlobResourceHandle::notifyResponseOnSuccess()
{
    ASSERT(isMainThread());

    ResourceResponse response(firstRequest().url(), extractMIMETypeFromMediaType(m_blobData->contentType()), m_totalRemainingSize, String());
    response.setHTTPStatusCode(m_isRangeRequest ? httpPartialContent : httpOK);
    response.setHTTPStatusText(m_isRangeRequest ? httpPartialContentText : httpOKText);

    response.setHTTPHeaderField(HTTPHeaderName::ContentType, m_blobData->contentType());
    response.setTextEncodingName(extractCharsetFromMediaType(m_blobData->contentType()).toAtomString());
    response.setHTTPHeaderField(HTTPHeaderName::ContentLength, String::number(m_totalRemainingSize));
    addPolicyContainerHeaders(response, m_blobData->policyContainer());

    if (m_isRangeRequest)
        response.setHTTPHeaderField(HTTPHeaderName::ContentRange, ParsedContentRange(m_rangeStart, m_rangeEnd, m_totalSize).headerValue());

    // FIXME: If a resource identified with a blob: URL is a File object, user agents must use that file's name attribute,
    // as if the response had a Content-Disposition header with the filename parameter set to the File's name attribute.
    // Notably, this will affect a name suggested in "File Save As".

    client()->didReceiveResponseAsync(this, WTFMove(response), [this, protectedThis = Ref { *this }] {
        m_buffer.resize(bufferSize);
        readAsync();
    });
}

void BlobResourceHandle::notifyReceiveData(const uint8_t* data, int bytesRead)
{
    if (client())
        client()->didReceiveBuffer(this, SharedBuffer::create(data, bytesRead), bytesRead);
}

void BlobResourceHandle::notifyFail(Error errorCode)
{
    if (client())
        client()->didFail(this, ResourceError(webKitBlobResourceDomain, static_cast<int>(errorCode), firstRequest().url(), String()));
}

static void doNotifyFinish(BlobResourceHandle& handle)
{
    if (handle.aborted())
        return;

    if (!handle.client())
        return;

    handle.client()->didFinishLoading(&handle, { });
}

void BlobResourceHandle::notifyFinish()
{
    if (!m_async) {
        doNotifyFinish(*this);
        return;
    }

    // Schedule to notify the client from a standalone function because the client might dispose the handle immediately from the callback function
    // while we still have BlobResourceHandle calls in the stack.
    callOnMainThread([protectedThis = Ref { *this }]() mutable {
        doNotifyFinish(protectedThis);
    });

}

} // namespace WebCore
