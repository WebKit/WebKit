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
#include "CurlCacheManager.h"

#include "FileSystem.h"
#include "HTTPHeaderMap.h"
#include "Logging.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

#define IO_BUFFERSIZE 4096

namespace WebCore {

CurlCacheManager& CurlCacheManager::getInstance()
{
    static CurlCacheManager instance;
    return instance;
}

CurlCacheManager::CurlCacheManager()
    : m_disabled(true)
{
    // call setCacheDirectory() to enable
}

CurlCacheManager::~CurlCacheManager()
{
    if (m_disabled)
        return;

    saveIndex();
}

void CurlCacheManager::setCacheDirectory(const String& directory)
{
    m_cacheDir = directory;
    m_cacheDir.append("/");
    if (m_cacheDir.isEmpty()) {
        LOG(Network, "Cache Error: Cache location is not set! CacheManager disabled.\n");
        m_disabled = true;
        return;
    }

    if (!fileExists(m_cacheDir)) {
        if (makeAllDirectories(m_cacheDir)) {
            LOG(Network, "Cache Error: Could not open or create cache directory! CacheManager disabled.\n");
            m_disabled = true;
            return;
        }
    }

    m_disabled = false;
    loadIndex();
}

void CurlCacheManager::loadIndex()
{
    if (m_disabled)
        return;

    String indexFilePath(m_cacheDir);
    indexFilePath.append("index.dat");

    PlatformFileHandle indexFile = openFile(indexFilePath, OpenForRead);
    if (!isHandleValid(indexFile)) {
        LOG(Network, "Cache Warning: Could not open %s for read\n", indexFilePath.latin1().data());
        return;
    }

    long long filesize = -1;
    if (!getFileSize(indexFilePath, filesize)) {
        LOG(Network, "Cache Error: Could not get file size of %s\n", indexFilePath.latin1().data());
        return;
    }

    // load the file content into buffer
    Vector<char> buffer;
    buffer.resize(filesize);
    int bufferPosition = 0;
    int bufferReadSize = IO_BUFFERSIZE;
    while (filesize > bufferPosition) {
        if (filesize - bufferPosition < bufferReadSize)
            bufferReadSize = filesize - bufferPosition;

        readFromFile(indexFile, buffer.data() + bufferPosition, bufferReadSize);
        bufferPosition += bufferReadSize;
    }
    closeFile(indexFile);

    // create strings from buffer
    String headerContent = String(buffer.data());
    Vector<String> indexURLs;
    headerContent.split("\n", indexURLs);
    buffer.clear();

    // add entries to index
    Vector<String>::const_iterator it = indexURLs.begin();
    Vector<String>::const_iterator end = indexURLs.end();
    if (indexURLs.size() > 1)
        --end; // last line is empty
    while (it != end) {
        String url = it->stripWhiteSpace();

        std::unique_ptr<CurlCacheEntry> cacheEntry(new CurlCacheEntry(url, m_cacheDir));
        if (cacheEntry->isCached())
            m_index.set(url, std::move(cacheEntry));

        ++it;
    }
}

void CurlCacheManager::saveIndex()
{
    if (m_disabled)
        return;

    String indexFilePath(m_cacheDir);
    indexFilePath.append("index.dat");

    PlatformFileHandle indexFile = openFile(indexFilePath, OpenForWrite);
    if (!isHandleValid(indexFile)) {
        LOG(Network, "Cache Error: Could not open %s for write\n", indexFilePath.latin1().data());
        return;
    }

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::const_iterator it = m_index.begin();
    HashMap<String, std::unique_ptr<CurlCacheEntry>>::const_iterator end = m_index.end();
    while (it != end) {
        const CString& urlLatin1 = it->key.latin1();
        writeToFile(indexFile, urlLatin1.data(), urlLatin1.length());
        writeToFile(indexFile, "\n", 1);
        ++it;
    }

    closeFile(indexFile);
}

void CurlCacheManager::didReceiveResponse(ResourceHandle* job, ResourceResponse& response)
{
    if (m_disabled)
        return;

    String url = job->firstRequest().url().string();

    if (response.httpStatusCode() == 304)
        loadCachedData(url, job, response);
    else if (response.httpStatusCode() == 200) {
        HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
        if (it != m_index.end())
            invalidateCacheEntry(url);

        std::unique_ptr<CurlCacheEntry> entry(new CurlCacheEntry(url, m_cacheDir));
        bool cacheable = entry->parseResponseHeaders(response);
        if (cacheable) {
            m_index.set(url, std::move(entry));
            saveResponseHeaders(url, response);
        } else
            entry->invalidate();
    } else
        invalidateCacheEntry(url);
}

void CurlCacheManager::didFinishLoading(const String& url)
{
    if (m_disabled)
        return;

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
    if (it != m_index.end())
        it->value->didFinishLoading();
}

bool CurlCacheManager::isCached(const String& url)
{
    if (m_disabled)
        return false;

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
    if (it != m_index.end()) {
        if (it->value->isCached())
            return true;

        invalidateCacheEntry(url);
    }
    return false;
}

HTTPHeaderMap& CurlCacheManager::requestHeaders(const String& url)
{
    ASSERT(isCached(url));
    return m_index.find(url)->value->requestHeaders();
}

void CurlCacheManager::didReceiveData(const String& url, const char* data, size_t size)
{
    if (m_disabled)
        return;

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
    if (it != m_index.end())
        if (!it->value->saveCachedData(data, size))
            invalidateCacheEntry(url);
}

void CurlCacheManager::saveResponseHeaders(const String& url, ResourceResponse& response)
{
    if (m_disabled)
        return;

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
    if (it != m_index.end())
        if (!it->value->saveResponseHeaders(response))
            invalidateCacheEntry(url);
}

void CurlCacheManager::invalidateCacheEntry(const String& url)
{
    if (m_disabled)
        return;

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
    if (it != m_index.end()) {
        it->value->invalidate();
        m_index.remove(url);
    }
}

void CurlCacheManager::didFail(const String& url)
{
    invalidateCacheEntry(url);
}

void CurlCacheManager::loadCachedData(const String& url, ResourceHandle* job, ResourceResponse& response)
{
    if (m_disabled)
        return;

    HashMap<String, std::unique_ptr<CurlCacheEntry>>::iterator it = m_index.find(url);
    if (it != m_index.end()) {
        it->value->setResponseFromCachedHeaders(response);
        if (!it->value->loadCachedData(job))
            invalidateCacheEntry(url);
    }
}

}
