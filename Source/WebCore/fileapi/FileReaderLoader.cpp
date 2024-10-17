/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#include "FileReaderLoader.h"

#include "Blob.h"
#include "BlobURL.h"
#include "ContentSecurityPolicy.h"
#include "ExceptionCode.h"
#include "FileReaderLoaderClient.h"
#include "HTTPHeaderNames.h"
#include "HTTPStatusCodes.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "ThreadableBlobRegistry.h"
#include "ThreadableLoader.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

const int defaultBufferLength = 32768;

FileReaderLoader::FileReaderLoader(ReadType readType, FileReaderLoaderClient* client)
    : m_readType(readType)
    , m_client(client)
    , m_isRawDataConverted(false)
    , m_stringResult(emptyString())
    , m_variableLength(false)
    , m_bytesLoaded(0)
    , m_totalBytes(0)
{
}

FileReaderLoader::~FileReaderLoader()
{
    cancel();
}

void FileReaderLoader::start(ScriptExecutionContext* scriptExecutionContext, Blob& blob)
{
    start(scriptExecutionContext, blob.url());
}

void FileReaderLoader::start(ScriptExecutionContext* scriptExecutionContext, const URL& blobURL)
{
    ASSERT(scriptExecutionContext);

    // The blob is read by routing through the request handling layer given a temporary public url.
    m_urlForReading = { BlobURL::createPublicURL(scriptExecutionContext->securityOrigin()), scriptExecutionContext->topOrigin().data() };
    if (m_urlForReading.isEmpty()) {
        failed(ExceptionCode::SecurityError);
        return;
    }

    CheckedPtr contentSecurityPolicy = scriptExecutionContext->contentSecurityPolicy();
    if (!contentSecurityPolicy)
        return;

    ThreadableBlobRegistry::registerBlobURL(scriptExecutionContext->securityOrigin(), scriptExecutionContext->policyContainer(), m_urlForReading, blobURL);

    // Construct and load the request.
    ResourceRequest request(m_urlForReading);
    request.setHTTPMethod("GET"_s);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.dataBufferingPolicy = DataBufferingPolicy::DoNotBufferData;
    options.credentials = FetchOptions::Credentials::Include;
    options.mode = FetchOptions::Mode::SameOrigin;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    if (m_client) {
        auto loader = ThreadableLoader::create(*scriptExecutionContext, *this, WTFMove(request), options);
        if (!loader)
            return;
        std::exchange(m_loader, loader);
    } else
        ThreadableLoader::loadResourceSynchronously(*scriptExecutionContext, WTFMove(request), *this, options);
}

void FileReaderLoader::cancel()
{
    m_errorCode = ExceptionCode::AbortError;
    terminate();
}

void FileReaderLoader::terminate()
{
    if (m_loader) {
        m_loader->cancel();
        cleanup();
    }
}

void FileReaderLoader::cleanup()
{
    m_loader = nullptr;

    // If we get any error, we do not need to keep a buffer around.
    if (m_errorCode) {
        m_rawData = nullptr;
        m_stringResult = emptyString();
    }
}

bool FileReaderLoader::processResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() != httpStatus200OK) {
        failed(httpStatusCodeToErrorCode(response.httpStatusCode()));
        return false;
    }

    if (m_readType == ReadType::ReadAsBinaryChunks)
        return true;

    long long length = response.expectedContentLength();

    // A negative value means that the content length wasn't specified, so the buffer will need to be dynamically grown.
    if (length < 0) {
        m_variableLength = true;
        length = defaultBufferLength;
    }

    // Check that we can cast to unsigned since we have to do
    // so to call ArrayBuffer's create function.
    // FIXME: Support reading more than the current size limit of ArrayBuffer.
    if (length > std::numeric_limits<unsigned>::max()) {
        failed(ExceptionCode::NotReadableError);
        return false;
    }

    ASSERT(!m_rawData);
    m_rawData = ArrayBuffer::tryCreate(static_cast<unsigned>(length), 1);

    if (!m_rawData) {
        failed(ExceptionCode::NotReadableError);
        return false;
    }

    m_totalBytes = static_cast<unsigned>(length);
    return true;
}

void FileReaderLoader::didReceiveResponse(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const ResourceResponse& response)
{
    if (!processResponse(response))
        return;

    if (m_client)
        m_client->didStartLoading();
}

void FileReaderLoader::didReceiveData(const SharedBuffer& buffer)
{
    ASSERT(buffer.size());

    // Bail out if we already encountered an error.
    if (m_errorCode)
        return;

    if (m_readType == ReadType::ReadAsBinaryChunks) {
        if (m_client)
            m_client->didReceiveBinaryChunk(buffer);
        return;
    }

    int length = buffer.size();
    unsigned remainingBufferSpace = m_totalBytes - m_bytesLoaded;
    if (length > static_cast<long long>(remainingBufferSpace)) {
        // If the buffer has hit maximum size, it can't be grown any more.
        if (m_totalBytes >= std::numeric_limits<unsigned>::max()) {
            failed(ExceptionCode::NotReadableError);
            return;
        }
        if (m_variableLength) {
            unsigned newLength = m_totalBytes + buffer.size();
            if (newLength < m_totalBytes) {
                failed(ExceptionCode::NotReadableError);
                return;
            }
            newLength = std::max(newLength, m_totalBytes + m_totalBytes / 4 + 1);
            auto newData = ArrayBuffer::tryCreate(newLength, 1);
            if (!newData) {
                // Not enough memory.
                failed(ExceptionCode::NotReadableError);
                return;
            }
            memcpy(static_cast<char*>(newData->data()), static_cast<char*>(m_rawData->data()), m_bytesLoaded);

            m_rawData = newData;
            m_totalBytes = static_cast<unsigned>(newLength);
        } else {
            // This can only happen if we get more data than indicated in expected content length (i.e. never, unless the networking layer is buggy).
            length = remainingBufferSpace;
        }
    }

    if (length <= 0)
        return;

    memcpy(static_cast<char*>(m_rawData->data()) + m_bytesLoaded, buffer.span().data(), length);
    m_bytesLoaded += length;

    m_isRawDataConverted = false;

    if (m_client)
        m_client->didReceiveData();
}

void FileReaderLoader::didFinishLoading(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const NetworkLoadMetrics&)
{
    if (m_variableLength && m_totalBytes > m_bytesLoaded) {
        m_rawData = m_rawData->slice(0, m_bytesLoaded);
        m_totalBytes = m_bytesLoaded;
    }
    cleanup();
    if (m_client)
        m_client->didFinishLoading();
}

void FileReaderLoader::didFail(std::optional<ScriptExecutionContextIdentifier>, const ResourceError& error)
{
    // If we're aborting, do not proceed with normal error handling since it is covered in aborting code.
    if (m_errorCode && m_errorCode.value() == ExceptionCode::AbortError)
        return;

    failed(toErrorCode(static_cast<BlobResourceHandle::Error>(error.errorCode())));
}

void FileReaderLoader::failed(ExceptionCode errorCode)
{
    m_errorCode = errorCode;
    cleanup();
    if (m_client)
        m_client->didFail(errorCode);
}

ExceptionCode FileReaderLoader::toErrorCode(BlobResourceHandle::Error error)
{
    switch (error) {
    case BlobResourceHandle::Error::NotFoundError:
        return ExceptionCode::NotFoundError;
    default:
        return ExceptionCode::NotReadableError;
    }
}

ExceptionCode FileReaderLoader::httpStatusCodeToErrorCode(int httpStatusCode)
{
    switch (httpStatusCode) {
    case httpStatus403Forbidden:
        return ExceptionCode::SecurityError;
    default:
        return ExceptionCode::NotReadableError;
    }
}

RefPtr<ArrayBuffer> FileReaderLoader::arrayBufferResult() const
{
    ASSERT(m_readType == ReadAsArrayBuffer);

    // If the loading is not started or an error occurs, return an empty result.
    if (!m_rawData || m_errorCode)
        return nullptr;

    // If completed, we can simply return our buffer.
    if (isCompleted())
        return m_rawData;

    // Otherwise, return a copy.
    return ArrayBuffer::create(*m_rawData);
}

String FileReaderLoader::stringResult()
{
    ASSERT(m_readType != ReadAsArrayBuffer && m_readType != ReadAsBlob);

    // If the loading is not started or an error occurs, return an empty result.
    if (!m_rawData || m_errorCode)
        return m_stringResult;

    // If already converted from the raw data, return the result now.
    if (m_isRawDataConverted)
        return m_stringResult;

    switch (m_readType) {
    case ReadAsArrayBuffer:
        // No conversion is needed.
        break;
    case ReadAsBinaryString:
        m_stringResult = m_rawData->span().first(m_bytesLoaded);
        break;
    case ReadAsText:
        convertToText();
        break;
    case ReadAsDataURL:
        // Partial data is not supported when reading as data URL.
        if (isCompleted())
            convertToDataURL();
        break;
    case ReadAsBlob:
    case ReadAsBinaryChunks:
        ASSERT_NOT_REACHED();
    }
    
    return m_stringResult;
}

void FileReaderLoader::convertToText()
{
    if (!m_bytesLoaded)
        return;

    // Decode the data.
    // The File API spec says that we should use the supplied encoding if it is valid. However, we choose to ignore this
    // requirement in order to be consistent with how WebKit decodes the web content: always has the BOM override the
    // provided encoding.     
    // FIXME: consider supporting incremental decoding to improve the perf.
    if (!m_decoder)
        m_decoder = TextResourceDecoder::create("text/plain"_s, m_encoding.isValid() ? m_encoding : PAL::UTF8Encoding());
    if (isCompleted())
        m_stringResult = m_decoder->decodeAndFlush(m_rawData->span().first(m_bytesLoaded));
    else
        m_stringResult = m_decoder->decode(m_rawData->span().first(m_bytesLoaded));
}

void FileReaderLoader::convertToDataURL()
{
    m_stringResult = makeString("data:"_s, m_dataType.isEmpty() ? "application/octet-stream"_s : m_dataType, ";base64,"_s, base64Encoded(m_rawData ? m_rawData->span().first(m_bytesLoaded) : std::span<const uint8_t>()));
}

bool FileReaderLoader::isCompleted() const
{
    return m_bytesLoaded == m_totalBytes;
}

void FileReaderLoader::setEncoding(StringView encoding)
{
    if (!encoding.isEmpty())
        m_encoding = PAL::TextEncoding(encoding);
}

} // namespace WebCore
