/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "BidiNetworkAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebPageProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;
using CacheBehavior = Inspector::Protocol::BidiNetwork::CacheBehavior;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiNetworkAgent);

BidiNetworkAgent::BidiNetworkAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_networkDomainDispatcher(BidiNetworkBackendDispatcher::create(backendDispatcher, this))
{
}

BidiNetworkAgent::~BidiNetworkAgent() = default;

void BidiNetworkAgent::didCreatePage(WebPageProxy& page)
{
    updateCacheBehavior(page);
}

void BidiNetworkAgent::sessionWillEnd()
{
    // https://w3c.github.io/webdriver-bidi/#cleanup-remote-end-state
    m_defaultCacheBehavior = CacheBehavior::Default;
    m_cacheBehaviorsForBrowsingContexts.clear();

    if (RefPtr session = m_session) {
        for (Ref page : session->pagesControlledByAutomation())
            updateCacheBehavior(page);
    }
}

CacheBehavior BidiNetworkAgent::cacheBehaviorForPage(const WebPageProxy& page) const
{
    // https://w3c.github.io/webdriver-bidi/#navigable-cache-behavior
    return m_cacheBehaviorsForBrowsingContexts.getOptional(page).value_or(m_defaultCacheBehavior);
}

void BidiNetworkAgent::updateCacheBehavior(WebPageProxy& page)
{
    // Bypassing uses the same mechanism as Web Inspector's "Disable Caches": every load in the page's frames
    // ignores the memory cache and the HTTP cache, and the page doesn't go into the back/forward cache.
    page.setResourceCachingDisabledByAutomation(cacheBehaviorForPage(page) == CacheBehavior::Bypass);
}

CommandResult<void> BidiNetworkAgent::setCacheBehavior(CacheBehavior cacheBehavior, RefPtr<JSON::Array>&& optionalContexts)
{
    // https://w3c.github.io/webdriver-bidi/#command-network-setCacheBehavior
    RefPtr session = m_session;
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    if (!optionalContexts) {
        m_defaultCacheBehavior = cacheBehavior;
        m_cacheBehaviorsForBrowsingContexts.clear();
        for (Ref page : session->pagesControlledByAutomation())
            updateCacheBehavior(page);
        return { };
    }

    auto pages = session->pagesForTopLevelBrowsingContexts(*optionalContexts);
    if (!pages)
        return makeUnexpected(pages.error().toProtocolString());

    for (Ref page : *pages) {
        if (cacheBehavior == m_defaultCacheBehavior)
            m_cacheBehaviorsForBrowsingContexts.remove(page.get());
        else
            m_cacheBehaviorsForBrowsingContexts.set(page.get(), cacheBehavior);
        updateCacheBehavior(page);
    }
    return { };
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
