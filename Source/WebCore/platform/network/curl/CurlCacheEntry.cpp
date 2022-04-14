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

#if USE(CURL)

#include "CurlCacheEntry.h"

#include "HTTPHeaderMap.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include <wtf/DateMath.h>
#include <wtf/HexNumber.h>
#include <wtf/SHA1.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

CurlCacheEntry::CurlCacheEntry(const String& url, ResourceHandle* job, const String& cacheDir)
    : m_contentFile(FileSystem::invalidPlatformFileHandle)
    , m_entrySize(0)
    , m_expireDate(WallTime::fromRawSeconds(-1))
    , m_headerParsed(false)
    , m_isLoading(false)
    , m_job(job)
{
    generateBaseFilename(url.latin1());

    m_headerFilename = makeString(cacheDir, m_basename, ".header"_s);
    m_contentFilename = makeString(cacheDir, m_basename, ".content"_s);
}

CurlCacheEntry::~CurlCacheEntry()
{
    closeContentFile();
}

bool CurlCacheEntry::isLoading() const
{
    return m_isLoading;
}

// Cache manager should invalidate the entry on false
bool CurlCacheEntry::isCached()
{
    if (!FileSystem::fileExists(m_contentFilename) || !FileSystem::fileExists(m_headerFilename))
        return false;

    if (!m_headerParsed) {
        if (!loadResponseHeaders())
            return false;
    }

    if (m_expireDate < WallTime::now()) {
        m_headerParsed = false;
        return false;
    }

    if (!entrySize())
        return false;

    return true;
}

bool CurlCacheEntry::saveCachedData(const uint8_t* data, size_t size)
{
    if (!openContentFile())
        return false;

    // Append
    FileSystem::writeToFile(m_contentFile, data, size);

    return true;
}

bool CurlCacheEntry::readCachedData(ResourceHandle* job)
{
    ASSERT(job->client());

    auto buffer = FileSystem::readEntireFile(m_contentFilename);
    if (!buffer) {
        LOG(Network, "Cache Error: Could not open %s to read cached content\n", m_contentFilename.latin1().data());
        return false;
    }

    if (auto bufferSize = buffer->size())
        job->getInternal()->client()->didReceiveBuffer(job, SharedBuffer::create(WTFMove(*buffer)), bufferSize);

    return true;
}

bool CurlCacheEntry::saveResponseHeaders(const ResourceResponse& response)
{
    FileSystem::PlatformFileHandle headerFile = FileSystem::openFile(m_headerFilename, FileSystem::FileOpenMode::Write);
    if (!FileSystem::isHandleValid(headerFile)) {
        LOG(Network, "Cache Error: Could not open %s for write\n", m_headerFilename.latin1().data());
        return false;
    }

    // Headers
    HTTPHeaderMap::const_iterator it = response.httpHeaderFields().begin();
    HTTPHeaderMap::const_iterator end = response.httpHeaderFields().end();
    while (it != end) {
        auto headerField = makeString(it->key, ": ", it->value, '\n').latin1();
        FileSystem::writeToFile(headerFile, headerField.data(), headerField.length());
        m_cachedResponse.setHTTPHeaderField(it->key, it->value);
        ++it;
    }

    FileSystem::closeFile(headerFile);
    return true;
}

bool CurlCacheEntry::loadResponseHeaders()
{
    auto buffer = FileSystem::readEntireFile(m_headerFilename);
    if (!buffer) {
        LOG(Network, "Cache Error: Could not open %s to read cached headers\n", m_headerFilename.latin1().data());
        return false;
    }

    String headerContent = String::adopt(WTFMove(*buffer));
    Vector<String> headerFields = headerContent.split('\n');

    Vector<String>::const_iterator it = headerFields.begin();
    Vector<String>::const_iterator end = headerFields.end();
    while (it != end) {
        size_t splitPosition = it->find(':');
        if (splitPosition != notFound)
            m_cachedResponse.setHTTPHeaderField(it->left(splitPosition), it->substring(splitPosition+1).stripWhiteSpace());
        ++it;
    }

    return parseResponseHeaders(m_cachedResponse);
}

// Set response headers from memory
void CurlCacheEntry::setResponseFromCachedHeaders(ResourceResponse& response)
{
    response.setHTTPStatusCode(304);
    response.setSource(ResourceResponseBase::Source::DiskCache);

    // Integrate the headers in the response with the cached ones.
    HTTPHeaderMap::const_iterator it = m_cachedResponse.httpHeaderFields().begin();
    HTTPHeaderMap::const_iterator end = m_cachedResponse.httpHeaderFields().end();
    while (it != end) {
        if (response.httpHeaderField(it->key).isNull())
            response.setHTTPHeaderField(it->key, it->value);
        ++it;
    }

    // Try to parse expected content length
    long long contentLength = -1;
    if (!response.httpHeaderField(HTTPHeaderName::ContentLength).isNull()) {
        if (auto parsedContentLength = parseIntegerAllowingTrailingJunk<long long>(response.httpHeaderField(HTTPHeaderName::ContentLength)))
            contentLength = *parsedContentLength;
    }
    response.setExpectedContentLength(contentLength); // -1 on parse error or null

    response.setMimeType(extractMIMETypeFromMediaType(response.httpHeaderField(HTTPHeaderName::ContentType)));
    response.setTextEncodingName(extractCharsetFromMediaType(response.httpHeaderField(HTTPHeaderName::ContentType)).toString());
}

void CurlCacheEntry::didFail()
{
    // The cache manager will call invalidate()
    setIsLoading(false);
}

void CurlCacheEntry::didFinishLoading()
{
    setIsLoading(false);
}

void CurlCacheEntry::generateBaseFilename(const CString& url)
{
    SHA1 sha1;
    sha1.addBytes(url.dataAsUInt8Ptr(), url.length());

    SHA1::Digest sum;
    sha1.computeHash(sum);
    uint8_t* rawdata = sum.data();

    StringBuilder baseNameBuilder;
    for (size_t i = 0; i < 16; i++)
        baseNameBuilder.append(hex(rawdata[i], Lowercase));
    m_basename = baseNameBuilder.toString();
}

void CurlCacheEntry::invalidate()
{
    closeContentFile();
    FileSystem::deleteFile(m_headerFilename);
    FileSystem::deleteFile(m_contentFilename);
    LOG(Network, "Cache: invalidated %s\n", m_basename.latin1().data());
}

bool CurlCacheEntry::parseResponseHeaders(const ResourceResponse& response)
{
    if (response.cacheControlContainsNoCache() || response.cacheControlContainsNoStore() || !response.hasCacheValidatorFields())
        return false;

    WallTime fileTime;

    if (auto fileTimeFromFile = FileSystem::fileModificationTime(m_headerFilename))
        fileTime = fileTimeFromFile.value();
    else
        fileTime = WallTime::now(); // GMT

    auto maxAge = response.cacheControlMaxAge();
    auto lastModificationDate = response.lastModified();
    auto responseDate = response.date();
    auto expirationDate = response.expires();

    if (maxAge && !response.cacheControlContainsMustRevalidate()) {
        // When both the cache entry and the response contain max-age, the lesser one takes priority
        WallTime expires = fileTime + *maxAge;
        if (m_expireDate == WallTime::fromRawSeconds(-1) || m_expireDate > expires)
            m_expireDate = expires;
    } else if (responseDate && expirationDate) {
        if (*expirationDate >= *responseDate)
            m_expireDate = fileTime + (*expirationDate - *responseDate);
    }
    // If there is no lifetime information
    if (m_expireDate == WallTime::fromRawSeconds(-1)) {
        if (lastModificationDate)
            m_expireDate = fileTime + (fileTime - *lastModificationDate) * 0.1;
        else
            m_expireDate = WallTime::fromRawSeconds(0);
    }

    String etag = response.httpHeaderField(HTTPHeaderName::ETag);
    if (!etag.isNull())
        m_requestHeaders.set(HTTPHeaderName::IfNoneMatch, etag);

    String lastModified = response.httpHeaderField(HTTPHeaderName::LastModified);
    if (!lastModified.isNull())
        m_requestHeaders.set(HTTPHeaderName::IfModifiedSince, lastModified);

    if (etag.isNull() && lastModified.isNull())
        return false;

    m_headerParsed = true;
    return true;
}

void CurlCacheEntry::setIsLoading(bool isLoading)
{
    m_isLoading = isLoading;
    if (m_isLoading)
        openContentFile();
    else
        closeContentFile();
}

size_t CurlCacheEntry::entrySize()
{
    if (!m_entrySize) {
        auto headerFileSize = FileSystem::fileSize(m_headerFilename);
        if (!headerFileSize) {
            LOG(Network, "Cache Error: Could not get file size of %s\n", m_headerFilename.latin1().data());
            return m_entrySize;
        }
        auto contentFileSize = FileSystem::fileSize(m_contentFilename);
        if (!contentFileSize) {
            LOG(Network, "Cache Error: Could not get file size of %s\n", m_contentFilename.latin1().data());
            return m_entrySize;
        }

        m_entrySize = *headerFileSize + *contentFileSize;
    }

    return m_entrySize;
}


bool CurlCacheEntry::openContentFile()
{
    if (FileSystem::isHandleValid(m_contentFile))
        return true;
    
    m_contentFile = FileSystem::openFile(m_contentFilename, FileSystem::FileOpenMode::Write);

    if (FileSystem::isHandleValid(m_contentFile))
        return true;
    
    LOG(Network, "Cache Error: Could not open %s for write\n", m_contentFilename.latin1().data());
    return false;
}

bool CurlCacheEntry::closeContentFile()
{
    if (!FileSystem::isHandleValid(m_contentFile))
        return true;

    FileSystem::closeFile(m_contentFile);
    m_contentFile = FileSystem::invalidPlatformFileHandle;

    return true;
}

}

#endif
