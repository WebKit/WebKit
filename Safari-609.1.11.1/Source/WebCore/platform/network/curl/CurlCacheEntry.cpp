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
#include "SharedBuffer.h"
#include <wtf/DateMath.h>
#include <wtf/HexNumber.h>
#include <wtf/MD5.h>

namespace WebCore {

CurlCacheEntry::CurlCacheEntry(const String& url, ResourceHandle* job, const String& cacheDir)
    : m_headerFilename(cacheDir)
    , m_contentFilename(cacheDir)
    , m_contentFile(FileSystem::invalidPlatformFileHandle)
    , m_entrySize(0)
    , m_expireDate(WallTime::fromRawSeconds(-1))
    , m_headerParsed(false)
    , m_isLoading(false)
    , m_job(job)
{
    generateBaseFilename(url.latin1());

    m_headerFilename.append(m_basename);
    m_headerFilename.append(".header");

    m_contentFilename.append(m_basename);
    m_contentFilename.append(".content");
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

bool CurlCacheEntry::saveCachedData(const char* data, size_t size)
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

    Vector<char> buffer;
    if (!loadFileToBuffer(m_contentFilename, buffer))
        return false;

    if (buffer.size())
        job->getInternal()->client()->didReceiveBuffer(job, SharedBuffer::create(buffer.data(), buffer.size()), buffer.size());

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
        String headerField = it->key;
        headerField.append(": ");
        headerField.append(it->value);
        headerField.append("\n");
        CString headerFieldLatin1 = headerField.latin1();
        FileSystem::writeToFile(headerFile, headerFieldLatin1.data(), headerFieldLatin1.length());
        m_cachedResponse.setHTTPHeaderField(it->key, it->value);
        ++it;
    }

    FileSystem::closeFile(headerFile);
    return true;
}

bool CurlCacheEntry::loadResponseHeaders()
{
    Vector<char> buffer;
    if (!loadFileToBuffer(m_headerFilename, buffer))
        return false;

    String headerContent = String(buffer.data(), buffer.size());
    Vector<String> headerFields = headerContent.split('\n');

    Vector<String>::const_iterator it = headerFields.begin();
    Vector<String>::const_iterator end = headerFields.end();
    while (it != end) {
        size_t splitPosition = it->find(":");
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
        bool success = false;
        long long parsedContentLength = response.httpHeaderField(HTTPHeaderName::ContentLength).toInt64(&success);
        if (success)
            contentLength = parsedContentLength;
    }
    response.setExpectedContentLength(contentLength); // -1 on parse error or null

    response.setMimeType(extractMIMETypeFromMediaType(response.httpHeaderField(HTTPHeaderName::ContentType)));
    response.setTextEncodingName(extractCharsetFromMediaType(response.httpHeaderField(HTTPHeaderName::ContentType)));
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
    MD5 md5;
    md5.addBytes(reinterpret_cast<const uint8_t*>(url.data()), url.length());

    MD5::Digest sum;
    md5.checksum(sum);
    uint8_t* rawdata = sum.data();

    for (size_t i = 0; i < MD5::hashSize; i++)
        appendByteAsHex(rawdata[i], m_basename, Lowercase);
}

bool CurlCacheEntry::loadFileToBuffer(const String& filepath, Vector<char>& buffer)
{
    // Open the file
    FileSystem::PlatformFileHandle inputFile = FileSystem::openFile(filepath, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(inputFile)) {
        LOG(Network, "Cache Error: Could not open %s for read\n", filepath.latin1().data());
        return false;
    }

    long long filesize = -1;
    if (!FileSystem::getFileSize(filepath, filesize)) {
        LOG(Network, "Cache Error: Could not get file size of %s\n", filepath.latin1().data());
        FileSystem::closeFile(inputFile);
        return false;
    }

    // Load the file content into buffer
    buffer.resize(filesize);
    int bufferPosition = 0;
    int bufferReadSize = 4096;
    int bytesRead = 0;
    while (filesize > bufferPosition) {
        if (filesize - bufferPosition < bufferReadSize)
            bufferReadSize = filesize - bufferPosition;

        bytesRead = FileSystem::readFromFile(inputFile, buffer.data() + bufferPosition, bufferReadSize);
        if (bytesRead != bufferReadSize) {
            LOG(Network, "Cache Error: Could not read from %s\n", filepath.latin1().data());
            FileSystem::closeFile(inputFile);
            return false;
        }

        bufferPosition += bufferReadSize;
    }
    FileSystem::closeFile(inputFile);
    return true;
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

    if (auto fileTimeFromFile = FileSystem::getFileModificationTime(m_headerFilename))
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
        long long headerFileSize;
        long long contentFileSize;

        if (!FileSystem::getFileSize(m_headerFilename, headerFileSize)) {
            LOG(Network, "Cache Error: Could not get file size of %s\n", m_headerFilename.latin1().data());
            return m_entrySize;
        }
        if (!FileSystem::getFileSize(m_contentFilename, contentFileSize)) {
            LOG(Network, "Cache Error: Could not get file size of %s\n", m_contentFilename.latin1().data());
            return m_entrySize;
        }

        m_entrySize = headerFileSize + contentFileSize;
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
