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
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/StringExtras.h>

namespace WebCore {

std::unique_ptr<CurlMultipartHandle> CurlMultipartHandle::createIfNeeded(CurlMultipartHandleClient& client, const CurlResponse& response)
{
    auto boundary = extractBoundary(response);
    if (!boundary)
        return nullptr;

    return std::make_unique<CurlMultipartHandle>(client, *boundary);
}

Optional<String> CurlMultipartHandle::extractBoundary(const CurlResponse& response)
{
    for (auto header : response.headers) {
        auto splitPosistion = header.find(":");
        if (splitPosistion == notFound)
            continue;

        auto key = header.left(splitPosistion).stripWhiteSpace();
        if (!equalIgnoringASCIICase(key, "Content-Type"))
            continue;

        auto contentType = header.substring(splitPosistion + 1).stripWhiteSpace();
        auto mimeType = extractMIMETypeFromMediaType(contentType);
        if (!equalIgnoringASCIICase(mimeType, "multipart/x-mixed-replace"))
            continue;

        auto boundary = extractBoundaryFromContentType(contentType);
        if (!boundary)
            continue;

        return String("--" + *boundary);
    }

    return WTF::nullopt;
}

Optional<String> CurlMultipartHandle::extractBoundaryFromContentType(const String& contentType)
{
    static const size_t length = strlen("boundary=");

    auto boundaryStart = contentType.findIgnoringASCIICase("boundary=");
    if (boundaryStart == notFound)
        return WTF::nullopt;

    boundaryStart += length;
    size_t boundaryEnd = 0;

    if (contentType[boundaryStart] == '"') {
        // Boundary value starts with a " quote. Search for the closing one.
        ++boundaryStart;
        boundaryEnd = contentType.find('"', boundaryStart);
        if (boundaryEnd == notFound)
            return WTF::nullopt;
    } else if (contentType[boundaryStart] == '\'') {
        // Boundary value starts with a ' quote. Search for the closing one.
        ++boundaryStart;
        boundaryEnd = contentType.find('\'', boundaryStart);
        if (boundaryEnd == notFound)
            return WTF::nullopt;
    } else {
        // Check for the end of the boundary. That can be a semicolon or a newline.
        boundaryEnd = contentType.find(';', boundaryStart);
        if (boundaryEnd == notFound)
            boundaryEnd = contentType.length();
    }

    // The boundary end should not be before the start
    if (boundaryEnd <= boundaryStart)
        return WTF::nullopt;

    return contentType.substring(boundaryStart, boundaryEnd - boundaryStart);
}

CurlMultipartHandle::CurlMultipartHandle(CurlMultipartHandleClient& client, const String& boundary)
    : m_client(client)
    , m_boundary(boundary)
{

}

void CurlMultipartHandle::didReceiveData(const SharedBuffer& buffer)
{
    if (m_state == State::End)
        return; // The handler is closed down so ignore everything.

    m_buffer.append(buffer.data(), buffer.size());

    while (processContent()) { }
}

void CurlMultipartHandle::didComplete()
{
    // Process the leftover data.
    while (processContent()) { }

    if (m_state != State::End) {
        // It seems we are still not at the end of the processing.
        // Push out the remaining data.
        m_client.didReceiveDataFromMultipart(SharedBuffer::create(m_buffer.data(), m_buffer.size()));
        m_state = State::End;
    }

    m_buffer.clear();
}

bool CurlMultipartHandle::processContent()
{
/*
    The allowed transitions between the states:
         Check Boundary
               |
      /-- In Boundary <----\
     |         |            |
     |     In Header        |
     |         |            |
     |     In Content       |
     |         |            |
     |    End Boundary ----/
     |         |
      \-----> End

*/
    switch (m_state) {
    case State::CheckBoundary: {
        if (m_buffer.size() < m_boundary.length()) {
            // We don't have enough data, so just skip.
            return false;
        }

        // Check for the boundary string.
        size_t boundaryStart;
        size_t lastPartialMatch;

        if (!checkForBoundary(boundaryStart, lastPartialMatch) && boundaryStart == notFound) {
            // Did not find the boundary start in this chunk.
            // Skip ahead to the last valid looking boundary character and start again.
            m_buffer.remove(0, lastPartialMatch);
            return false;
        }

        // Found the boundary start.
        // Consume everything before that and also the boundary
        m_buffer.remove(0, boundaryStart + m_boundary.length());
        m_state = State::InBoundary;
    }
    // Fallthrough.
    case State::InBoundary: {
        // Now the first two characters should be: \r\n
        if (m_buffer.size() < 2)
            return false;

        const char* content = m_buffer.data();
        // By default we'll remove 2 characters at the end.
        // The \r and \n as stated in the multipart RFC.
        size_t removeCount = 2;

        if (content[0] != '\r' || content[1] != '\n') {
            // There should be a \r and a \n but it seems that's not the case.
            // So we'll check for a simple \n. Not really RFC compatible but servers do tricky things.
            if (content[0] != '\n') {
                // Also no \n so just go to the end.
                m_state = State::End;
                return false;
            }

            // Found a simple \n so remove just that.
            removeCount = 1;
        }

        // Consume the characters.
        m_buffer.remove(0, removeCount);
        m_headers.clear();
        m_state = State::InHeader;
    }
    // Fallthrough.
    case State::InHeader: {
        // Process the headers.
        if (!parseHeadersIfPossible()) {
            // Parsing of headers failed, try again later.
            return false;
        }

        m_client.didReceiveHeaderFromMultipart(m_headers);
        m_state = State::InContent;
    }
    // Fallthrough.
    case State::InContent: {
        if (m_buffer.isEmpty())
            return false;

        size_t boundaryStart;
        size_t lastPartialMatch;

        if (!checkForBoundary(boundaryStart, lastPartialMatch) && boundaryStart == notFound) {
            // Did not find the boundary start, all data up to the lastPartialMatch is ok.
            m_client.didReceiveDataFromMultipart(SharedBuffer::create(m_buffer.data(), lastPartialMatch));
            m_buffer.remove(0, lastPartialMatch);
            return false;
        }

        // There was a boundary start (or end we'll check that later), push out part of the data.
        m_client.didReceiveDataFromMultipart(SharedBuffer::create(m_buffer.data(), boundaryStart));
        m_buffer.remove(0, boundaryStart + m_boundary.length());
        m_state = State::EndBoundary;
    }
    // Fallthrough.
    case State::EndBoundary: {
        if (m_buffer.size() < 2)
            return false; // Not enough data to check. Return later when there is more data.

        // We'll decide if this is a closing boundary or an opening one.
        const char* content = m_buffer.data();

        if (content[0] == '-' && content[1] == '-') {
            // This is a closing boundary. Close down the handler.
            m_state = State::End;
            return false;
        }

        // This was a simple content separator not a closing one.
        // Go to before the content processing.
        m_state = State::InBoundary;
        break;
    }
    case State::End:
        // We are done. Nothing to do anymore.
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true; // There are still things to process, so go for it.
}

bool CurlMultipartHandle::checkForBoundary(size_t& boundaryStartPosition, size_t& lastPartialMatchPosition)
{
    auto contentLength = m_buffer.size();

    boundaryStartPosition = notFound;
    lastPartialMatchPosition = contentLength;

    if (contentLength < m_boundary.length())
        return false;

    const auto content = m_buffer.data();

    for (size_t i = 0; i < contentLength - m_boundary.length(); ++i) {
        auto length = matchedLength(content + i);
        if (length == m_boundary.length()) {
            boundaryStartPosition = i;
            return true;
        }

        if (length)
            lastPartialMatchPosition = i;
    }

    return false;
}

size_t CurlMultipartHandle::matchedLength(const char* data)
{
    auto length = m_boundary.length();
    for (size_t i = 0; i < length; ++i) {
        if (data[i] != m_boundary[i])
            return i;
    }

    return length;
}

bool CurlMultipartHandle::parseHeadersIfPossible()
{
    auto contentLength = m_buffer.size();

    if (!contentLength)
        return false;

    const auto content = m_buffer.data();

    // Check if we have the header closing strings.
    if (!strnstr(content, "\r\n\r\n", contentLength)) {
        // Some servers closes the headers with only \n-s.
        if (!strnstr(content, "\n\n", contentLength)) {
            // Don't have the header closing string. Wait for more data.
            return false;
        }
    }

    // Parse the HTTP headers.
    String value;
    StringView name;
    auto p = content;
    const auto end = content + contentLength;
    size_t totalConsumedLength = 0;
    for (; p < end; ++p) {
        String failureReason;
        size_t consumedLength = parseHTTPHeader(p, end - p, failureReason, name, value, false);
        if (!consumedLength)
            break; // No more header to parse.

        p += consumedLength;
        totalConsumedLength += consumedLength;

        // The name should not be empty, but the value could be empty.
        if (name.isEmpty())
            break;

        m_headers.append(name.toString() + ": " + value + "\r\n");
    }

    m_buffer.remove(0, totalConsumedLength + 1);
    return true;
}

} // namespace WebCore

#endif
