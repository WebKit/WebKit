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
#include "WebResourceCacheManager.h"

#include "Connection.h"
#include "MessageID.h"
#include "SecurityOriginData.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceCacheManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/MemoryCache.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginHash.h>

using namespace WebCore;

namespace WebKit {

WebResourceCacheManager& WebResourceCacheManager::shared()
{
    static WebResourceCacheManager& shared = *new WebResourceCacheManager;
    return shared;
}

WebResourceCacheManager::WebResourceCacheManager()
{
}

WebResourceCacheManager::~WebResourceCacheManager()
{
}

void WebResourceCacheManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebResourceCacheManagerMessage(connection, messageID, arguments);
}


void WebResourceCacheManager::getCacheOrigins(uint64_t callbackID) const
{
    MemoryCache::SecurityOriginSet origins;
    memoryCache()->getOriginsWithCache(origins);

    Vector<SecurityOriginData> identifiers;
    identifiers.reserveCapacity(origins.size());

    MemoryCache::SecurityOriginSet::iterator end = origins.end();
    for (MemoryCache::SecurityOriginSet::iterator it = origins.begin(); it != end; ++it) {
        RefPtr<SecurityOrigin> origin = *it;
        
        SecurityOriginData originData;
        originData.protocol = origin->protocol();
        originData.host = origin->host();
        originData.port = origin->port();

        identifiers.uncheckedAppend(originData);
    }

    WebProcess::shared().connection()->send(Messages::WebResourceCacheManagerProxy::DidGetCacheOrigins(identifiers, callbackID), 0);
}

void WebResourceCacheManager::clearCacheForOrigin(SecurityOriginData originData) const
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(originData.protocol, originData.host, originData.port);
    if (!origin)
        return;

    memoryCache()->removeResourcesWithOrigin(origin.get());
}

void WebResourceCacheManager::clearCacheForAllOrigins() const
{
    WebProcess::shared().clearResourceCaches();
}

} // namespace WebKit
