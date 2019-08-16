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

#pragma once

#include "HTTPHeaderMap.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/FileSystem.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CurlCacheEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CurlCacheEntry(const String& url, ResourceHandle* job, const String& cacheDir);
    ~CurlCacheEntry();

    bool isCached();
    bool isLoading() const;
    size_t entrySize();
    HTTPHeaderMap& requestHeaders() { return m_requestHeaders; }

    bool saveCachedData(const char* data, size_t);
    bool readCachedData(ResourceHandle*);

    bool saveResponseHeaders(const ResourceResponse&);
    void setResponseFromCachedHeaders(ResourceResponse&);

    void invalidate();
    void didFail();
    void didFinishLoading();

    bool parseResponseHeaders(const ResourceResponse&);

    void setIsLoading(bool);

    void addClient(ResourceHandle* job) { m_clients.add(job); }
    void removeClient(ResourceHandle* job) { m_clients.remove(job); }
    int hasClients() const { return m_clients.size() > 0; }

    const ResourceHandle* getJob() const { return m_job; }

private:
    String m_basename;
    String m_headerFilename;
    String m_contentFilename;

    FileSystem::PlatformFileHandle m_contentFile;

    size_t m_entrySize;
    WallTime m_expireDate;
    bool m_headerParsed;
    bool m_isLoading;
    ListHashSet<ResourceHandle*> m_clients;

    ResourceResponse m_cachedResponse;
    HTTPHeaderMap m_requestHeaders;

    ResourceHandle* m_job;

    void generateBaseFilename(const CString& url);
    bool loadFileToBuffer(const String& filepath, Vector<char>& buffer);
    bool loadResponseHeaders();

    bool openContentFile();
    bool closeContentFile();
};

} // namespace WebCore
