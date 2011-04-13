/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebApplicationCacheManager.h"

#include "MessageID.h"
#include "SecurityOriginData.h"
#include "WebApplicationCacheManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ApplicationCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginHash.h>

using namespace WebCore;

namespace WebKit {

WebApplicationCacheManager& WebApplicationCacheManager::shared()
{
    static WebApplicationCacheManager& shared = *new WebApplicationCacheManager;
    return shared;
}

WebApplicationCacheManager::WebApplicationCacheManager()
{
}

void WebApplicationCacheManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebApplicationCacheManagerMessage(connection, messageID, arguments);
}

void WebApplicationCacheManager::getApplicationCacheOrigins(uint64_t callbackID)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

    HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash> origins;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    cacheStorage().getOriginsWithCache(origins);
#endif

    Vector<SecurityOriginData> identifiers;
    identifiers.reserveCapacity(origins.size());

    HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash>::iterator end = origins.end();
    HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash>::iterator i = origins.begin();
    for (; i != end; ++i) {
        RefPtr<SecurityOrigin> origin = *i;
        
        SecurityOriginData originData;
        originData.protocol = origin->protocol();
        originData.host = origin->host();
        originData.port = origin->port();

        identifiers.uncheckedAppend(originData);
    }

    WebProcess::shared().connection()->send(Messages::WebApplicationCacheManagerProxy::DidGetApplicationCacheOrigins(identifiers, callbackID), 0);
}

void WebApplicationCacheManager::deleteEntriesForOrigin(const SecurityOriginData& originData)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(originData.protocol, originData.host, originData.port);
    if (!origin)
        return;
    
    ApplicationCache::deleteCacheForOrigin(origin.get());
#endif
}

void WebApplicationCacheManager::deleteAllEntries()
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    cacheStorage().deleteAllEntries();
#endif
}

} // namespace WebKit
