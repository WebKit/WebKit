/*
 * Copyright (C) 2013 University of Szeged
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurlMultipartHandle.h"

#if USE(CURL)

#include "CurlMultipartHandleClient.h"
#include "CurlResponse.h"
#include "HTTPParsers.h"
#include "ParsedContentType.h"
#include "SharedBuffer.h"
#include <wtf/StringExtras.h>

namespace WebCore {

static std::optional<CString> extractBoundary(const CurlResponse& response)
{
    static const auto contentTypeLength = strlen("content-type:");

    for (auto header : response.headers) {
        if (!header.startsWithIgnoringASCIICase("content-type:"_s))
            continue;

        auto contentType = ParsedContentType::create(header.substring(contentTypeLength));
        if (!contentType)
            return std::nullopt;

        if (!equalLettersIgnoringASCIICase(contentType->mimeType(), "multipart/x-mixed-replace"_s))
            return std::nullopt;

        auto boundary = contentType->parameterValueForName("boundary"_s);
        if (boundary.isEmpty())
            return std::nullopt;

        return makeString("--", boundary).latin1();
    }

    return std::nullopt;
}

std::unique_ptr<CurlMultipartHandle> CurlMultipartHandle::createIfNeeded(CurlMultipartHandleClient& client, const CurlResponse& response)
{
    auto boundary = extractBoundary(response);
    if (!boundary)
        return nullptr;

    return makeUnique<CurlMultipartHandle>(client, WTFMove(*boundary));
}

CurlMultipartHandle::CurlMultipartHandle(CurlMultipartHandleClient& client, CString&& boundary)
    : m_client(client)
    , m_boundary(WTFMove(boundary))
{
}

void CurlMultipartHandle::didReceiveMessage(const SharedBuffer& buffer)
{
    if (m_state == State::WaitingForTerminate || m_state == State::End || m_didCompleteMessage)
        return; // The handler is closed down so ignore everything.

    m_buffer.append(buffer.data(), buffer.size());

    while (processContent()) { }
}

void CurlMultipartHandle::completeHeaderProcessing()
{
    if (m_state == State::WaitingForTerminate || m_state == State::End)
        return; // The handler is closed down so ignore everything.

    RELEASE_ASSERT(m_state == State::WaitingForHeaderProcessing);

    m_state = State::InBody;

    while (processContent()) { }
}

void CurlMultipartHandle::didCompleteMessage()
{
    if (m_state == State::End)
        return; // The handler is closed down so ignore everything.

    m_didCompleteMessage = true;

    if (m_state == State::WaitingForTerminate)
        m_state = State::Terminating;

    while (processContent()) { }
}

bool CurlMultipartHandle::processContent()
{
    switch (m_state) {
    case State::FindBoundaryStart:
        FALLTHROUGH;
    case State::InBody: {
        auto result = findBoundary();
        if (result.isSyntaxError) {
            m_hasError = true;
            m_state = State::End;
            return false;
        }

        if (m_state == State::InBody && result.dataEnd)
            m_client->didReceiveDataFromMultipart(SharedBuffer::create(m_buffer.data(), result.dataEnd));

        if (result.processed)
            m_buffer.remove(0, result.processed);

        if (!result.hasBoundary || result.hasCloseDelimiter) {
            if (m_didCompleteMessage) {
                m_state = State::Terminating;
                return true;
            }

            if (result.hasCloseDelimiter)
                m_state = State::WaitingForTerminate;

            return false;
        }

        m_headers.clear();
        m_state = State::InHeader;
        return true;
    }

    case State::InHeader: {
        switch (parseHeadersIfPossible()) {
        case ParseHeadersResult::Success:
            m_client->didReceiveHeaderFromMultipart(WTFMove(m_headers));
            m_state = State::WaitingForHeaderProcessing;
            return true;

        case ParseHeadersResult::NeedMoreData:
            if (m_didCompleteMessage) {
                m_state = State::Terminating;
                return true;
            }
            return false;

        case ParseHeadersResult::HeaderSizeTooLarge:
            m_hasError = true;
            m_state = State::End;
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    case State::WaitingForHeaderProcessing: {
        // Wait until completeHeaderProcessing() is called
        return false;
    }

    case State::WaitingForTerminate: {
        // Wait until didCompleteMessage() is called
        return false;
    }

    case State::Terminating: {
        m_client->didCompleteFromMultipart();
        m_state = State::End;
        return false;
    }

    case State::End:
        return false;
    }

    return false;
}

CurlMultipartHandle::FindBoundaryResult CurlMultipartHandle::findBoundary()
{
    FindBoundaryResult result;

    auto contentLength = m_buffer.size();
    const auto contentStartPtr = m_buffer.data();
    const auto contentEndPtr = contentStartPtr + contentLength;

    auto boundaryLength = m_boundary.length();
    auto boundaryStartPtr = m_boundary.data();

    auto matchedStartPtr = static_cast<const uint8_t*>(memmem(contentStartPtr, contentLength, boundaryStartPtr, boundaryLength));
    if (!matchedStartPtr) {
        if (!m_didCompleteMessage) {
            // Not enough data to check the boundary (Temporarily retain "Initial CRLF + (boundary - 1)" bytes for the next search.)
            result.dataEnd = std::max(int(contentLength) - int(boundaryLength) + 1 - 2, 0);
        } else
            result.dataEnd = contentLength;

        result.processed = result.dataEnd;
        return result;
    }

    auto matchedEndPtr = matchedStartPtr + boundaryLength;

    // The initial CRLF is considered to be attached to the boundary delimiter line rather than
    // part of the preceding part. (See RFC2046 [5.1.1. Common Syntax])
    if (matchedStartPtr - contentStartPtr >= 2 && !memcmp(matchedStartPtr - 2, "\r\n", 2))
        matchedStartPtr -= 2;
    else if (matchedStartPtr - contentStartPtr >= 1 && !memcmp(matchedStartPtr - 1, "\n", 1))
        matchedStartPtr--;

    result.dataEnd = matchedStartPtr - contentStartPtr;
    result.processed = result.dataEnd;

    // Check the Close Delimiter
    if (contentEndPtr - matchedEndPtr < 2) {
        if (!m_didCompleteMessage) {
            // Not enough data to check the Close Delimiter.
            return result;
        }
    } else if (!memcmp(matchedEndPtr, "--", 2)) {
        result.hasBoundary = true;
        result.hasCloseDelimiter = true;
        result.processed = matchedEndPtr + 2 - contentStartPtr;
        return result;
    }

    // Skip transport-padding
    for (; matchedEndPtr < contentEndPtr && isTabOrSpace(*matchedEndPtr); ++matchedEndPtr) { }

    // There should be a \r and a \n but it seems that's not the case.
    // So we'll check for a simple \n. Not really RFC compatible but servers do tricky things.
    if (contentEndPtr - matchedEndPtr >= 2  && !memcmp(matchedEndPtr, "\r\n", 2))
        matchedEndPtr += 2;
    else if (contentEndPtr - matchedEndPtr >= 1 && !memcmp(matchedEndPtr, "\n", 1))
        matchedEndPtr++;
    else if (matchedEndPtr >= contentEndPtr) {
        // Not enough data to check the boundary
        return result;
    } else {
        result.isSyntaxError = true;
        return result;
    }

    result.hasBoundary = true;
    result.processed = matchedEndPtr - contentStartPtr;
    return result;
}

CurlMultipartHandle::ParseHeadersResult CurlMultipartHandle::parseHeadersIfPossible()
{
    static const auto maxHeaderSize = 300 * 1024;

    auto contentLength = m_buffer.size();
    const auto contentStartPtr = m_buffer.data();

    // Check if we have the header closing strings.
    const uint8_t* end = nullptr;
    if ((end = static_cast<const uint8_t*>(memmem(contentStartPtr, contentLength, "\r\n\r\n", 4))))
        end += 4;
    else if ((end = static_cast<const uint8_t*>(memmem(contentStartPtr, contentLength, "\n\n", 2))))
        end += 2;
    else if (contentLength > maxHeaderSize)
        return ParseHeadersResult::HeaderSizeTooLarge;
    else {
        // Don't have the header closing string. Wait for more data.
        return ParseHeadersResult::NeedMoreData;
    }

    if (end - contentStartPtr > maxHeaderSize)
        return ParseHeadersResult::HeaderSizeTooLarge;

    // Parse the HTTP headers.
    String failureReason;
    StringView name;
    String value;

    for (auto p = contentStartPtr; p < end; ++p) {
        size_t consumedLength = parseHTTPHeader(p, end - p, failureReason, name, value, false);
        if (!consumedLength)
            break; // No more header to parse.

        p += consumedLength;

        // The name should not be empty, but the value could be empty.
        if (name.isEmpty())
            break;

        m_headers.append(makeString(name, ": ", value, "\r\n"));
    }

    m_buffer.remove(0, end - contentStartPtr);
    return ParseHeadersResult::Success;
}

} // namespace WebCore

#endif
