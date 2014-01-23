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

#ifndef CurlCacheManager_h
#define CurlCacheManager_h

#include "CurlCacheEntry.h"
#include "ResourceHandle.h"
#include "ResourceResponse.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CurlCacheManager {

public:
    static CurlCacheManager& getInstance();

    void setCacheDirectory(const String&);
    const String& cacheDirectory() { return m_cacheDir; }
    void setStorageSizeLimit(size_t);

    bool isCached(const String&);
    HTTPHeaderMap& requestHeaders(const String&); // Load headers

    void didReceiveResponse(ResourceHandle*, ResourceResponse&);
    void didReceiveData(const String&, const char*, size_t); // Save data
    void didFinishLoading(const String&);
    void didFail(const String&);

private:
    CurlCacheManager();
    ~CurlCacheManager();
    CurlCacheManager(CurlCacheManager const&);
    void operator=(CurlCacheManager const&);

    bool m_disabled;
    String m_cacheDir;
    HashMap<String, std::unique_ptr<CurlCacheEntry>> m_index;

    ListHashSet<String> m_LRUEntryList;
    size_t m_currentStorageSize;
    size_t m_storageSizeLimit;

    void saveIndex();
    void loadIndex();
    void makeRoomForNewEntry();

    void saveResponseHeaders(const String&, ResourceResponse&);
    void invalidateCacheEntry(const String&);
    void readCachedData(const String&, ResourceHandle*, ResourceResponse&);
};

}

#endif // CurlCacheManager_h
