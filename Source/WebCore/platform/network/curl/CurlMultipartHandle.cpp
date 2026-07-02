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
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CurlMultipartHandle);

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

        return makeString("--"_s, boundary).latin1();
    }

    return std::nullopt;
}

std::unique_ptr<CurlMultipartHandle> CurlMultipartHandle::createIfNeeded(CurlMultipartHandleClient& client, const CurlResponse& response)
{
    auto boundary = extractBoundary(response);
    if (!boundary)
        return nullptr;

    return makeUnique<CurlMultipartHandle>(client, WTF::move(*boundary));
}

CurlMultipartHandle::CurlMultipartHandle(CurlMultipartHandleClient& client, CString&& boundary)
    : m_client(client)
    , m_boundary(WTF::move(boundary))
{
}

void CurlMultipartHandle::didReceiveMessage(std::span<const uint8_t> receivedData)
{
    if (m_state == State::WaitingForTerminate || m_state == State::End || m_didCompleteMessage)
        return; // The handler is closed down so ignore everything.

    m_buffer.append(receivedData);

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
        [[fallthrough]];
    case State::InBody: {
        auto result = findBoundary();
        if (result.isSyntaxError) {
            m_hasError = true;
            m_state = State::End;
            return false;
        }

        if (m_state == State::InBody && result.dataEnd)
            m_client->didReceiveDataFromMultipart(m_buffer.span().first(result.dataEnd));

        if (result.processed)
            m_buffer.removeAt(0, result.processed);

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
            m_client->didReceiveHeaderFromMultipart(WTF::move(m_headers));
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

    auto contentSpan = m_buffer.span();
    auto contentLength = contentSpan.size();
    const auto contentStartPtr = contentSpan.data();
    const auto contentEndPtr = contentStartPtr + contentLength;

    auto boundarySpan = byteCast<uint8_t>(m_boundary.span());

    auto matchedIndex = find(contentSpan, boundarySpan);
    if (matchedIndex == notFound) {
        if (!m_didCompleteMessage) {
            // Not enough data to check the boundary (Temporarily retain "Initial CRLF + (boundary - 1)" bytes for the next search.)
            result.dataEnd = std::max(static_cast<int>(contentLength) - static_cast<int>(boundarySpan.size()) + 1 - 2, 0);
        } else
            result.dataEnd = contentLength;

        result.processed = result.dataEnd;
        return result;
    }

    auto matchedStartPtr = contentStartPtr + matchedIndex;

    auto matchedEndPtr = matchedStartPtr + boundarySpan.size();

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
    static constexpr std::array<uint8_t, 4> crlfcrlf = { '\r', '\n', '\r', '\n' };
    static constexpr std::array<uint8_t, 2> lflf = { '\n', '\n' };

    auto contentSpan = m_buffer.span();
    auto contentLength = contentSpan.size();
    const auto contentStartPtr = contentSpan.data();

    // Check if we have the header closing strings.
    const uint8_t* end = nullptr;
    auto crlfcrlfIndex = find(contentSpan, std::span { crlfcrlf });
    if (crlfcrlfIndex != notFound)
        end = contentStartPtr + crlfcrlfIndex + 4;
    else {
        auto lflIndex = find(contentSpan, std::span { lflf });
        if (lflIndex != notFound)
            end = contentStartPtr + lflIndex + 2;
    }

    if (!end) {
        if (contentLength > maxHeaderSize)
            return ParseHeadersResult::HeaderSizeTooLarge;
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
        size_t consumedLength = parseHTTPHeader(std::span { p, static_cast<size_t>(end - p) }, failureReason, name, value, false);
        if (!consumedLength)
            break; // No more header to parse.

        p += consumedLength;

        // The name should not be empty, but the value could be empty.
        if (name.isEmpty())
            break;

        m_headers.append(makeString(name, ": "_s, value, "\r\n"_s));
    }

    m_buffer.removeAt(0, end - contentStartPtr);
    return ParseHeadersResult::Success;
}

} // namespace WebCore

#endif
