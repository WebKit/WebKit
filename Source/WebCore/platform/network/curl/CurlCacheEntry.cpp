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

#include "FileSystem.h"
#include "HTTPHeaderMap.h"
#include "HTTPParsers.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>
#include <wtf/HexNumber.h>
#include <wtf/MD5.h>

namespace WebCore {

CurlCacheEntry::CurlCacheEntry(const String& url, const String& cacheDir)
    : m_headerFilename(cacheDir)
    , m_contentFilename(cacheDir)
    , m_entrySize(0)
    , m_expireDate(-1)
    , m_headerParsed(false)
{
    generateBaseFilename(url.latin1());

    m_headerFilename.append(m_basename);
    m_headerFilename.append(".header");

    m_contentFilename.append(m_basename);
    m_contentFilename.append(".content");
}

CurlCacheEntry::~CurlCacheEntry()
{
}

// Cache manager should invalidate the entry on false
bool CurlCacheEntry::isCached()
{
    if (!fileExists(m_contentFilename) || !fileExists(m_headerFilename))
        return false;

    if (!m_headerParsed) {
        if (!loadResponseHeaders())
            return false;
    }

    if (m_expireDate < currentTimeMS()) {
        m_headerParsed = false;
        return false;
    }

    if (!entrySize())
        return false;

    return true;
}

bool CurlCacheEntry::saveCachedData(const char* data, size_t size)
{
    PlatformFileHandle contentFile = openFile(m_contentFilename, OpenForWrite);
    if (!isHandleValid(contentFile)) {
        LOG(Network, "Cache Error: Could not open %s for write\n", m_contentFilename.latin1().data());
        return false;
    }

    // Append
    seekFile(contentFile, 0, SeekFromEnd);
    writeToFile(contentFile, data, size);
    closeFile(contentFile);
    return true;
}

bool CurlCacheEntry::readCachedData(ResourceHandle* job)
{
    ASSERT(job->client());

    Vector<char> buffer;
    if (!loadFileToBuffer(m_contentFilename, buffer))
        return false;

    job->getInternal()->client()->didReceiveData(job, buffer.data(), buffer.size(), 0);
    return true;
}

bool CurlCacheEntry::saveResponseHeaders(ResourceResponse& response)
{
    PlatformFileHandle headerFile = openFile(m_headerFilename, OpenForWrite);
    if (!isHandleValid(headerFile)) {
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
        writeToFile(headerFile, headerFieldLatin1.data(), headerFieldLatin1.length());
        ++it;
    }

    closeFile(headerFile);
    return true;
}

bool CurlCacheEntry::loadResponseHeaders()
{
    Vector<char> buffer;
    if (!loadFileToBuffer(m_headerFilename, buffer))
        return false;

    String headerContent = String(buffer.data(), buffer.size());
    Vector<String> headerFields;
    headerContent.split("\n", headerFields);

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
    response.setWasCached(true);

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
    if (!response.httpHeaderField("Content-Length").isNull()) {
        bool success = false;
        long long parsedContentLength = response.httpHeaderField("Content-Length").toInt64(&success);
        if (success)
            contentLength = parsedContentLength;
    }
    response.setExpectedContentLength(contentLength); // -1 on parse error or null

    response.setMimeType(extractMIMETypeFromMediaType(response.httpHeaderField("Content-Type")));
    response.setTextEncodingName(extractCharsetFromMediaType(response.httpHeaderField("Content-Type")));
    response.setSuggestedFilename(filenameFromHTTPContentDisposition(response.httpHeaderField("Content-Disposition")));
}

void CurlCacheEntry::didFail()
{
    // The cache manager will call invalidate()
}

void CurlCacheEntry::didFinishLoading()
{
    // Nothing to do here yet
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
    PlatformFileHandle inputFile = openFile(filepath, OpenForRead);
    if (!isHandleValid(inputFile)) {
        LOG(Network, "Cache Error: Could not open %s for read\n", filepath.latin1().data());
        return false;
    }

    long long filesize = -1;
    if (!getFileSize(filepath, filesize)) {
        LOG(Network, "Cache Error: Could not get file size of %s\n", filepath.latin1().data());
        closeFile(inputFile);
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

        bytesRead = readFromFile(inputFile, buffer.data() + bufferPosition, bufferReadSize);
        if (bytesRead != bufferReadSize) {
            LOG(Network, "Cache Error: Could not read from %s\n", filepath.latin1().data());
            closeFile(inputFile);
            return false;
        }

        bufferPosition += bufferReadSize;
    }
    closeFile(inputFile);
    return true;
}

void CurlCacheEntry::invalidate()
{
    deleteFile(m_headerFilename);
    deleteFile(m_contentFilename);
    LOG(Network, "Cache: invalidated %s\n", m_basename.latin1().data());
}

bool CurlCacheEntry::parseResponseHeaders(ResourceResponse& response)
{
    double fileTime;
    time_t fileModificationDate;

    if (getFileModificationTime(m_headerFilename, fileModificationDate)) {
        fileTime = difftime(fileModificationDate, 0);
        fileTime *= 1000.0;
    } else
        fileTime = currentTimeMS(); // GMT

    if (response.cacheControlContainsNoCache() || response.cacheControlContainsNoStore())
        return false;


    double maxAge = 0;
    bool maxAgeIsValid = false;

    if (response.cacheControlContainsMustRevalidate())
        maxAge = 0;
    else {
        maxAge = response.cacheControlMaxAge();
        if (std::isnan(maxAge))
            maxAge = 0;
        else
            maxAgeIsValid = true;
    }

    if (!response.hasCacheValidatorFields())
        return false;


    double lastModificationDate = 0;
    double responseDate = 0;
    double expirationDate = 0;

    lastModificationDate = response.lastModified();
    if (std::isnan(lastModificationDate))
        lastModificationDate = 0;

    responseDate = response.date();
    if (std::isnan(responseDate))
        responseDate = 0;

    expirationDate = response.expires();
    if (std::isnan(expirationDate))
        expirationDate = 0;


    if (maxAgeIsValid) {
        // When both the cache entry and the response contain max-age, the lesser one takes priority
        double expires = fileTime + maxAge * 1000;
        if (m_expireDate == -1 || m_expireDate > expires)
            m_expireDate = expires;
    } else if (responseDate > 0 && expirationDate >= responseDate)
        m_expireDate = fileTime + (expirationDate - responseDate);

    // If there is no lifetime information
    if (m_expireDate == -1) {
        if (lastModificationDate > 0)
            m_expireDate = fileTime + (fileTime - lastModificationDate) * 0.1;
        else
            m_expireDate = 0;
    }

    String etag = response.httpHeaderField("ETag");
    if (!etag.isNull())
        m_requestHeaders.set("If-None-Match", etag);

    String lastModified = response.httpHeaderField("Last-Modified");
    if (!lastModified.isNull())
        m_requestHeaders.set("If-Modified-Since", lastModified);

    if (etag.isNull() && lastModified.isNull())
        return false;

    m_headerParsed = true;
    return true;
}

size_t CurlCacheEntry::entrySize()
{
    if (!m_entrySize) {
        long long headerFileSize;
        long long contentFileSize;

        if (!getFileSize(m_headerFilename, headerFileSize)) {
            LOG(Network, "Cache Error: Could not get file size of %s\n", m_headerFilename.latin1().data());
            return m_entrySize;
        }
        if (!getFileSize(m_contentFilename, contentFileSize)) {
            LOG(Network, "Cache Error: Could not get file size of %s\n", m_contentFilename.latin1().data());
            return m_entrySize;
        }

        m_entrySize = headerFileSize + contentFileSize;
    }

    return m_entrySize;
}

}

#endif
