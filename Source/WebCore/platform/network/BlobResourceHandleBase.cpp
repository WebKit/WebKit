/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include "BlobResourceHandleBase.h"

#include "AsyncFileStream.h"
#include "FileStream.h"
#include "HTTPHeaderNames.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/MainThread.h>

namespace WebCore {

static constexpr int httpOK = 200;
static constexpr int httpPartialContent = 206;
static constexpr auto httpOKText = "OK"_s;
static constexpr auto httpPartialContentText = "Partial Content"_s;

BlobResourceHandleBase::BlobResourceHandleBase(bool async, RefPtr<BlobData>&& blobData)
    : m_blobData(WTF::move(blobData))
{
    if (async)
        m_stream = makeUnique<AsyncFileStream>(*this);
    else
        m_stream = makeUnique<FileStream>();
}

BlobResourceHandleBase::~BlobResourceHandleBase() = default;

auto BlobResourceHandleBase::adjustAndValidateRangeBounds() -> std::optional<Error>
{
    if (!m_range->start) {
        if (!m_range->end)
            return Error::RangeError;
        // m_range->end indicates the last bytes to read.
        if (*m_range->end > m_totalSize) {
            m_range->start = 0;
            m_range->end = m_totalSize ? (m_totalSize - 1) : 0;
        } else {
            m_range->start = m_totalSize - *m_range->end;
            m_range->end = *m_range->start + *m_range->end - 1;
        }
    } else {
        if (*m_range->start >= m_totalSize)
            return Error::RangeError;
        if (m_range->end && *m_range->start > *m_range->end)
            return Error::RangeError;
        if (!m_range->end || *m_range->end >= m_totalSize)
            m_range->end = m_totalSize ? (m_totalSize - 1) : 0;
        else
            m_range->end = *m_range->end;
    }
    return { };
}

void BlobResourceHandleBase::closeFileIfOpen()
{
    if (m_isFileOpen) {
        m_isFileOpen = false;
        asyncStream()->close();
    }
}

void BlobResourceHandleBase::clearAsyncStream()
{
    m_stream = std::unique_ptr<AsyncFileStream>();
}

BlobData* BlobResourceHandleBase::blobData() const
{
    return m_blobData.get();
}

FileStream* BlobResourceHandleBase::syncStream() const
{
    return std::get<std::unique_ptr<FileStream>>(m_stream).get();
}

AsyncFileStream* BlobResourceHandleBase::asyncStream() const
{
    return std::get<std::unique_ptr<AsyncFileStream>>(m_stream).get();
}

void BlobResourceHandleBase::start()
{
    if (!async()) {
        doStart();
        return;
    }

    // Finish this async call quickly and return.
    callOnMainThread([protectedThis = Ref { *this }]() mutable {
        protectedThis->doStart();
    });
}

void BlobResourceHandleBase::doStart()
{
    ASSERT(isMainThread());

    // Do not continue if the request is aborted or an error occurs.
    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    if (!equalLettersIgnoringASCIICase(firstRequest().httpMethod(), "get"_s)) {
        didFail(Error::MethodNotAllowed);
        return;
    }

    // If the blob data is not found, fail now.
    if (!m_blobData) {
        didFail(Error::NotFoundError);
        return;
    }

    // Parse the "Range" header we care about.
    if (String range = firstRequest().httpHeaderField(HTTPHeaderName::Range); !range.isNull()) {
        m_range = parseRange(range, RangeAllowWhitespace::Yes);
        if (!m_range) {
            didFail(Error::RangeError);
            return;
        }
        m_isRangeRequest = true;
    }

    if (async())
        getSizeForNext();
    else {
        for (size_t i = 0; i < m_blobData->items().size() && !erroredOrAborted(); ++i)
            getSizeForNext();

        if (auto error = seek()) {
            didFail(*error);
            return;
        }
        dispatchDidReceiveResponse();
    }
}

void BlobResourceHandleBase::getSizeForNext()
{
    ASSERT(isMainThread());

    // Do we finish validating and counting size for all items?
    if (m_sizeItemCount >= m_blobData->items().size()) {
        if (auto error = seek()) {
            didFail(*error);
            return;
        }

        // Start reading if in asynchronous mode.
        if (async())
            dispatchDidReceiveResponse();
        return;
    }

    const BlobDataItem& item = m_blobData->items().at(m_sizeItemCount);
    switch (item.type()) {
    case BlobDataItem::Type::Data:
        didGetSize(item.length());
        break;
    case BlobDataItem::Type::File: {
        // Files know their sizes, but asking the stream to verify that the file wasn't modified.
        Ref file = item.file();
        if (async())
            asyncStream()->getSize(file->path(), file->expectedModificationTime());
        else
            didGetSize(syncStream()->getSize(file->path(), file->expectedModificationTime()));
        break;
    }
    }
}

void BlobResourceHandleBase::didGetSize(long long size)
{
    ASSERT(isMainThread());

    if (erroredOrAborted()) {
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
    uint64_t updatedSize = static_cast<uint64_t>(item.length());

    // Cache the size.
    m_itemLengthList.append(updatedSize);

    // Count the size.
    m_totalSize += updatedSize;
    m_totalRemainingSize += updatedSize;
    ++m_sizeItemCount;

    // Continue with the next item.
    getSizeForNext();
}

auto BlobResourceHandleBase::seek() -> std::optional<Error>
{
    ASSERT(isMainThread());

    // Bail out if the range is not provided.
    if (!m_isRangeRequest)
        return { };

    if (auto error = adjustAndValidateRangeBounds())
        return error;

    // Skip the initial items that are not in the range.
    Checked<uint64_t> offset = *m_range->start;
    for (m_readItemCount = 0; m_readItemCount < m_blobData->items().size() && offset.value() >= lengthOfItemBeingRead(); ++m_readItemCount)
        offset -= lengthOfItemBeingRead();

    // Set the offset that need to jump to for the first item in the range.
    m_currentItemReadSize = offset.value();

    // Adjust the total remaining size in order not to go beyond the range.
    Checked<uint64_t> rangeSize = *m_range->end;
    rangeSize -= *m_range->start;
    rangeSize += 1uz;
    if (m_totalRemainingSize > rangeSize.value())
        m_totalRemainingSize = rangeSize.value();
    return { };
}

void BlobResourceHandleBase::readAsync()
{
    ASSERT(isMainThread());

    if (erroredOrAborted())
        return;

    while (m_totalRemainingSize && m_readItemCount < m_blobData->items().size()) {
        const BlobDataItem& item = m_blobData->items().at(m_readItemCount);
        switch (item.type()) {
        case BlobDataItem::Type::Data:
            if (!readDataAsync(item))
                return; // error occurred
            break;
        case BlobDataItem::Type::File:
            readFileAsync(item);
            return;
        }
    }
    didFinish();
}

bool BlobResourceHandleBase::readDataAsync(const BlobDataItem& item)
{
    ASSERT(isMainThread());

    ASSERT(m_currentItemReadSize <= static_cast<uint64_t>(item.length()));
    uint64_t bytesToRead = static_cast<uint64_t>(item.length()) - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = m_totalRemainingSize;

    auto data = protect(item.data())->span().subspan(item.offset() + m_currentItemReadSize, bytesToRead);
    m_currentItemReadSize = 0;

    return consumeData(data);
}

void BlobResourceHandleBase::readFileAsync(const BlobDataItem& item)
{
    ASSERT(isMainThread());

    if (m_isFileOpen) {
        asyncStream()->read(buffer().mutableSpan());
        return;
    }

    uint64_t bytesToRead = lengthOfItemBeingRead() - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = static_cast<int>(m_totalRemainingSize);
    asyncStream()->openForRead(protect(item.file())->path(), item.offset() + m_currentItemReadSize, bytesToRead);
    m_isFileOpen = true;
    m_currentItemReadSize = 0;
}

void BlobResourceHandleBase::didOpen(bool success)
{
    ASSERT(async());

    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    if (!success) {
        didFail(Error::NotReadableError);
        return;
    }

    // Continue the reading.
    readAsync();
}

void BlobResourceHandleBase::didRead(int bytesRead)
{
    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    if (bytesRead < 0) {
        didFail(Error::NotReadableError);
        return;
    }

    if (consumeData(m_buffer.subspan(0, bytesRead)))
        readAsync();
}

bool BlobResourceHandleBase::consumeData(std::span<const uint8_t> data)
{
    ASSERT(async());

    m_totalRemainingSize -= data.size();

    // Notify the client.
    if (!data.empty()) {
        if (!didReceiveData(data))
            return false;
    }

    if (m_isFileOpen) {
        // When the current item is a file item, the reading is completed only if bytesRead is 0.
        if (data.empty()) {
            closeFileIfOpen();

            // Move to the next item.
            ++m_readItemCount;
        }
    } else {
        // Otherwise, we read the current text item as a whole and move to the next item.
        ++m_readItemCount;
    }

    return true;
}

void BlobResourceHandleBase::dispatchDidReceiveResponse()
{
    ASSERT(isMainThread());

    if (shouldAbortDispatchDidReceiveResponse())
        return;

    ResourceResponse response(URL { firstRequest().url() }, extractMIMETypeFromMediaType(m_blobData->contentType()), m_totalRemainingSize, String());
    response.setHTTPStatusCode(m_isRangeRequest ? httpPartialContent : httpOK);
    response.setHTTPStatusText(m_isRangeRequest ? httpPartialContentText : httpOKText);

    response.setHTTPHeaderField(HTTPHeaderName::ContentType, m_blobData->contentType());
    response.setTextEncodingName(extractCharsetFromMediaType(m_blobData->contentType()).toString());
    response.setHTTPHeaderField(HTTPHeaderName::ContentLength, String::number(m_totalRemainingSize));
    addPolicyContainerHeaders(response, m_blobData->policyContainer());

    if (m_isRangeRequest)
        response.setHTTPHeaderField(HTTPHeaderName::ContentRange, ParsedContentRange(*m_range->start, *m_range->end, m_totalSize).headerValue());

    // FIXME: If a resource identified with a blob: URL is a File object, user agents must use that file's name attribute,
    // as if the response had a Content-Disposition header with the filename parameter set to the File's name attribute.
    // Notably, this will affect a name suggested in "File Save As".

    didReceiveResponse(WTF::move(response));
}

} // namespace WebCore
