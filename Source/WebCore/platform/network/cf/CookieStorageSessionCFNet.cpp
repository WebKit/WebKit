/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CookieStorageSession.h"

#include <wtf/ProcessPrivilege.h>

namespace WebCore {

RetainPtr<CFURLStorageSessionRef> CookieStorageSession::createCFStorageSessionForIdentifier(CFStringRef identifier, ShouldDisableCFURLCache shouldDisableCFURLCache)
{
    RetainPtr<CFURLStorageSessionRef> storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, nullptr));

    if (!storageSession)
        return nullptr;

    if (shouldDisableCFURLCache == ShouldDisableCFURLCache::Yes)
        _CFURLStorageSessionDisableCache(storageSession.get());

    if (shouldDisableCFURLCache == ShouldDisableCFURLCache::No) {
        RetainPtr<CFURLCacheRef> cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
        if (!cache)
            return nullptr;

        CFURLCacheSetDiskCapacity(cache.get(), 0);

        RetainPtr<CFURLCacheRef> sharedCache = adoptCF(CFURLCacheCopySharedURLCache());
        CFURLCacheSetMemoryCapacity(cache.get(), CFURLCacheMemoryCapacity(sharedCache.get()));
    }

    if (!processMayUseCookieAPI())
        return storageSession;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    auto sharedCookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
    auto sharedPolicy = CFHTTPCookieStorageGetCookieAcceptPolicy(sharedCookieStorage);
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), sharedPolicy);

    return storageSession;
}

CookieStorageSession::CookieStorageSession(PAL::SessionID sessionID, RetainPtr<CFURLStorageSessionRef>&& platformSession, RetainPtr<CFHTTPCookieStorageRef>&& platformCookieStorage, IsInMemoryCookieStore isInMemoryCookieStore)
    : m_sessionID(sessionID)
    , m_isInMemoryCookieStore(isInMemoryCookieStore == IsInMemoryCookieStore::Yes)
    , m_platformSession(WTF::move(platformSession))
{
    ASSERT(processMayUseCookieAPI() || !platformCookieStorage || m_isInMemoryCookieStore);
    m_platformCookieStorage = platformCookieStorage ? WTF::move(platformCookieStorage) : cookieStorage();
}

RetainPtr<CFHTTPCookieStorageRef> CookieStorageSession::cookieStorage() const
{
    if (!processMayUseCookieAPI() && !m_isInMemoryCookieStore)
        return nullptr;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    if (m_platformCookieStorage)
        return m_platformCookieStorage;

    if (m_platformSession)
        return adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, m_platformSession.get()));

    // When using NSURLConnection, we also use its shared cookie storage.
    return nullptr;
}

} // namespace WebCore
