/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ApplicationCacheStorage.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCacheGroup.h"
#include "ApplicationCache.h"
#include "FileSystem.h"
#include "KURL.h"

namespace WebCore {

static unsigned urlHostHash(const KURL& url)
{
    unsigned hostStart = url.hostStart();
    unsigned hostEnd = url.hostEnd();
    
    return AlreadyHashed::avoidDeletedValue(StringImpl::computeHash(url.string().characters() + hostStart, hostEnd - hostStart));
}

ApplicationCacheGroup* ApplicationCacheStorage::findOrCreateCacheGroup(const KURL& manifestURL)
{
    std::pair<CacheGroupMap::iterator, bool> result = m_cachesInMemory.add(manifestURL, 0);
    
    if (!result.second) {
        ASSERT(result.first->second);
        
        return result.first->second;
    }

    result.first->second = new ApplicationCacheGroup(manifestURL);
    m_cacheHostSet.add(urlHostHash(manifestURL));
    
    return result.first->second;
}

ApplicationCacheGroup* ApplicationCacheStorage::cacheGroupForURL(const KURL& url)
{
    // Hash the host name and see if there's a manifest with the same host.
    if (!m_cacheHostSet.contains(urlHostHash(url)))
        return 0;

    // Check if a cache already exists in memory.
    CacheGroupMap::const_iterator end = m_cachesInMemory.end();
    for (CacheGroupMap::const_iterator it = m_cachesInMemory.begin(); it != end; ++it) {
        ApplicationCacheGroup* group = it->second;
        
        if (!protocolHostAndPortAreEqual(url, group->manifestURL()))
            continue;
        
        if (ApplicationCache* cache = group->newestCache()) {
            if (cache->resourceForURL(url))
                return group;
        }
    }
    
    return 0;
}

void ApplicationCacheStorage::cacheGroupDestroyed(ApplicationCacheGroup* group)
{
    ASSERT(m_cachesInMemory.get(group->manifestURL()) == group);

    m_cachesInMemory.remove(group->manifestURL());
    if (!group->newestCache())
        m_cacheHostSet.remove(urlHostHash(group->manifestURL()));
}

void ApplicationCacheStorage::setCacheDirectory(const String& cacheDirectory)
{
    ASSERT(m_cacheDirectory.isNull());
    ASSERT(!cacheDirectory.isNull());
    
    m_cacheDirectory = cacheDirectory;
}

void ApplicationCacheStorage::openDatabase(bool createIfDoesNotExist)
{
    if (m_database.isOpen())
        return;

    // The cache directory should never be null, but if it for some weird reason is we bail out.
    if (m_cacheDirectory.isNull())
        return;
    
    String applicationCachePath = pathByAppendingComponent(m_cacheDirectory, "ApplicationCache.db");
    if (!createIfDoesNotExist && !fileExists(applicationCachePath))
        return;

    makeAllDirectories(m_cacheDirectory);
    m_database.open(applicationCachePath);
}

ApplicationCacheStorage& cacheStorage()
{
    static ApplicationCacheStorage storage;
    
    return storage;
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
