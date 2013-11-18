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
#include <wtf/text/WTFString.h>

namespace WebCore {

class CurlCacheManager {

public:
    static CurlCacheManager& getInstance();

    const String& getCacheDirectory() { return m_cacheDir; }
    void setCacheDirectory(const String&);

    bool isCached(const String&);
    HTTPHeaderMap& requestHeaders(const String&); // load headers

    void didReceiveResponse(ResourceHandle*, ResourceResponse&);
    void didReceiveData(const String&, const char*, size_t); // save data
    void didFail(const String&);
    void didFinishLoading(const String&);

private:
    CurlCacheManager();
    ~CurlCacheManager();
    CurlCacheManager(CurlCacheManager const&);
    void operator=(CurlCacheManager const&);

    String m_cacheDir;
    HashMap<String, std::unique_ptr<CurlCacheEntry>> m_index;
    bool m_disabled;

    void saveIndex();
    void loadIndex();

    void saveResponseHeaders(const String&, ResourceResponse&);
    void invalidateCacheEntry(const String&);
    void loadCachedData(const String&, ResourceHandle*, ResourceResponse&);
};

}

#endif // CurlCacheManager_h
