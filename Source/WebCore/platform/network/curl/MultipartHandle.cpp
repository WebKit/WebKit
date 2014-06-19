/*
 * Copyright (C) 2013 University of Szeged
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
#include "MultipartHandle.h"

#if USE(CURL)

#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include <wtf/StringExtras.h>

namespace WebCore {

PassOwnPtr<MultipartHandle> MultipartHandle::create(ResourceHandle* handle, const String& boundary)
{
    return adoptPtr(new MultipartHandle(handle, boundary));
}

bool MultipartHandle::extractBoundary(const String& contentType, String& boundary)
{
    static const size_t length = strlen("boundary=");
    size_t boundaryStart = contentType.findIgnoringCase("boundary=");

    // No boundary found.
    if (boundaryStart == notFound)
        return false;

    boundaryStart += length;
    size_t boundaryEnd = 0;

    if (contentType[boundaryStart] == '"') {
        // Boundary value starts with a " quote. Search for the closing one.
        ++boundaryStart;
        boundaryEnd = contentType.find('"', boundaryStart);
        if (boundaryEnd == notFound)
            return false;
    } else if (contentType[boundaryStart] == '\'') {
        // Boundary value starts with a ' quote. Search for the closing one.
        ++boundaryStart;
        boundaryEnd = contentType.find('\'', boundaryStart);
        if (boundaryEnd == notFound)
            return false;
    } else {
        // Check for the end of the boundary. That can be a semicolon or a newline.
        boundaryEnd = contentType.find(';', boundaryStart);
        if (boundaryEnd == notFound)
            boundaryEnd = contentType.length();
    }

    // The boundary end should not be before the start
    if (boundaryEnd <= boundaryStart)
        return false;

    boundary = contentType.substring(boundaryStart, boundaryEnd - boundaryStart);
    return true;
}


bool MultipartHandle::matchForBoundary(const char* data, size_t position, size_t& matchedLength)
{
    matchedLength = 0;

    for (size_t i = 0; i < m_boundaryLength; ++i) {
        if (data[position + i] != m_boundary[i]) {
            matchedLength = i;
            return false;
        }
    }

    matchedLength = m_boundaryLength;
    return true;
}

bool MultipartHandle::checkForBoundary(size_t& boundaryStartPosition, size_t& lastPartialMatchPosition)
{
    size_t contentLength = m_buffer.size();

    boundaryStartPosition = notFound;
    lastPartialMatchPosition = contentLength;

    if (contentLength < m_boundaryLength)
        return false;

    const char* content = m_buffer.data();
    size_t matched;

    for (size_t i = 0; i < contentLength - m_boundaryLength; ++i) {
        if (matchForBoundary(content, i, matched)) {
            boundaryStartPosition = i;
            return true;
        }

        if (matched)
            lastPartialMatchPosition = i;
    }

    return false;
}

bool MultipartHandle::parseHeadersIfPossible()
{
    size_t contentLength = m_buffer.size();

    if (!contentLength)
        return false;

    const char* content = m_buffer.data();

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
    String name;
    char* p = const_cast<char*>(content);
    const char* end = content + contentLength;
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

        m_headers.add(name, value);
    }

    m_buffer.remove(0, totalConsumedLength + 1);
    return true;
}

void MultipartHandle::contentReceived(const char* data, size_t length)
{
    if (m_state == End)
        return; // The handler is closed down so ignore everything.

    m_buffer.append(data, length);

    while (processContent()) { }
}

bool MultipartHandle::processContent()
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
    case CheckBoundary: {
        if (m_buffer.size() < m_boundaryLength) {
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
        m_buffer.remove(0, boundaryStart + m_boundaryLength);
        m_state = InBoundary;
    }
    // Fallthrough.
    case InBoundary: {
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
                m_state = End;
                return false;
            }

            // Found a simple \n so remove just that.
            removeCount = 1;
        }

        // Consume the characters.
        m_buffer.remove(0, removeCount);
        m_headers.clear();
        m_state = InHeader;
    }
    // Fallthrough.
    case InHeader: {
        // Process the headers.
        if (!parseHeadersIfPossible()) {
            // Parsing of headers failed, try again later.
            return false;
        }

        didReceiveResponse();
        m_state = InContent;
    }
    // Fallthrough.
    case InContent: {
        if (m_buffer.isEmpty())
            return false;

        size_t boundaryStart = notFound;
        size_t lastPartialMatch;

        if (!checkForBoundary(boundaryStart, lastPartialMatch) && boundaryStart == notFound) {
            // Did not find the boundary start, all data up to the lastPartialMatch is ok.
            didReceiveData(lastPartialMatch);
            m_buffer.remove(0, lastPartialMatch);
            return false;
        }

        // There was a boundary start (or end we'll check that later), push out part of the data.
        didReceiveData(boundaryStart);
        m_buffer.remove(0, boundaryStart + m_boundaryLength);
        m_state = EndBoundary;
    }
    // Fallthrough.
    case EndBoundary: {
        if (m_buffer.size() < 2)
            return false; // Not enough data to check. Return later when there is more data.

        // We'll decide if this is a closing boundary or an opening one.
        const char* content = m_buffer.data();

        if (content[0] == '-' && content[1] == '-') {
            // This is a closing boundary. Close down the handler.
            m_state = End;
            return false;
        }

        // This was a simple content separator not a closing one.
        // Go to before the content processing.
        m_state = InBoundary;
        break;
    }
    case End:
        // We are done. Nothing to do anymore.
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true; // There are still things to process, so go for it.
}

void MultipartHandle::contentEnded()
{
    // Process the leftover data.
    while (processContent()) { }

    if (m_state != End) {
        // It seems we are still not at the end of the processing.
        // Push out the remaining data.
        didReceiveData(m_buffer.size());
        m_state = End;
    }

    m_buffer.clear();
}

void MultipartHandle::didReceiveData(size_t length)
{
    ResourceHandleInternal* d = m_resourceHandle->getInternal();

    if (d->m_cancelled) {
        // Request has been canceled, so we'll go to the end state.
        m_state = End;
        return;
    }

    if (d->client()) {
        const char* data = m_buffer.data();
        d->client()->didReceiveData(m_resourceHandle, data, length, length);
    }
}

void MultipartHandle::didReceiveResponse()
{
    ResourceHandleInternal* d = m_resourceHandle->getInternal();
    if (d->client()) {
        OwnPtr<ResourceResponse> response = ResourceResponseBase::adopt(d->m_response.copyData());

        HTTPHeaderMap::const_iterator end = m_headers.end();
        for (HTTPHeaderMap::const_iterator it = m_headers.begin(); it != end; ++it)
            response->setHTTPHeaderField(it->key, it->value);

        String contentType = m_headers.get(HTTPHeaderName::ContentType);
        String mimeType = extractMIMETypeFromMediaType(contentType);

        response->setMimeType(mimeType.lower());
        response->setTextEncodingName(extractCharsetFromMediaType(contentType));
        response->setSuggestedFilename(filenameFromHTTPContentDisposition(response->httpHeaderField(HTTPHeaderName::ContentDisposition)));

        d->client()->didReceiveResponse(m_resourceHandle, *response);
        response->setResponseFired(true);
    }
}

} // namespace WebCore

#endif
