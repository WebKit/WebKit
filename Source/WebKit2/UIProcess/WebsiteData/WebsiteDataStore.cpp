/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebsiteDataStore.h"

#include <wtf/RunLoop.h>

namespace WebKit {

static WebCore::SessionID generateNonPersistentSessionID()
{
    // FIXME: We count backwards here to not conflict with API::Session.
    static uint64_t sessionID = std::numeric_limits<uint64_t>::max();

    return WebCore::SessionID(--sessionID);
}

RefPtr<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(new WebsiteDataStore(generateNonPersistentSessionID()));
}

RefPtr<WebsiteDataStore> WebsiteDataStore::create(Configuration configuration)
{
    return adoptRef(new WebsiteDataStore(WTF::move(configuration)));
}

WebsiteDataStore::WebsiteDataStore(Configuration)
    : m_sessionID(WebCore::SessionID::defaultSessionID())
{
}

WebsiteDataStore::WebsiteDataStore(WebCore::SessionID sessionID)
    : m_sessionID(sessionID)
{
}

WebsiteDataStore::~WebsiteDataStore()
{
    ASSERT(m_webPages.isEmpty());
}

void WebsiteDataStore::addWebPage(WebPageProxy& webPageProxy)
{
    ASSERT(!m_webPages.contains(&webPageProxy));

    m_webPages.add(&webPageProxy);
}

void WebsiteDataStore::removeWebPage(WebPageProxy& webPageProxy)
{
    ASSERT(m_webPages.contains(&webPageProxy));

    m_webPages.remove(&webPageProxy);
}

void WebsiteDataStore::removeDataModifiedSince(WebsiteDataTypes dataTypes, std::chrono::system_clock::time_point, std::function<void ()> completionHandler)
{
    // FIXME: Actually remove data.
    RunLoop::main().dispatch(WTF::move(completionHandler));
}

}
