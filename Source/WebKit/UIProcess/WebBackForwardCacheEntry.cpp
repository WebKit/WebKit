/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebBackForwardCacheEntry.h"

#include "Logging.h"
#include "SuspendedPageProxy.h"
#include "WebBackForwardCache.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

static const Seconds expirationDelay { 30_min };

WebBackForwardCacheEntry::WebBackForwardCacheEntry(WebBackForwardCache& backForwardCache, WebCore::BackForwardItemIdentifier backForwardItemID, WebCore::ProcessIdentifier processIdentifier, std::unique_ptr<SuspendedPageProxy>&& suspendedPage)
    : m_backForwardCache(backForwardCache)
    , m_processIdentifier(processIdentifier)
    , m_backForwardItemID(backForwardItemID)
    , m_suspendedPage(WTFMove(suspendedPage))
    , m_expirationTimer(RunLoop::main(), this, &WebBackForwardCacheEntry::expirationTimerFired)
{
    m_expirationTimer.startOneShot(expirationDelay);
}

WebBackForwardCacheEntry::~WebBackForwardCacheEntry()
{
    if (m_backForwardItemID && !m_suspendedPage) {
        auto& process = this->process();
        process.sendWithAsyncReply(Messages::WebProcess::ClearCachedPage(m_backForwardItemID), [activity = process.throttler().backgroundActivity("Clearing back/forward cache entry"_s)] { });
    }
}

std::unique_ptr<SuspendedPageProxy> WebBackForwardCacheEntry::takeSuspendedPage()
{
    ASSERT(m_suspendedPage);
    m_backForwardItemID = { };
    m_expirationTimer.stop();
    return std::exchange(m_suspendedPage, nullptr);
}

WebProcessProxy& WebBackForwardCacheEntry::process() const
{
    auto* process = WebProcessProxy::processForIdentifier(m_processIdentifier);
    ASSERT(process);
    ASSERT(!m_suspendedPage || process == &m_suspendedPage->process());
    return *process;
}

void WebBackForwardCacheEntry::expirationTimerFired()
{
    RELEASE_LOG(BackForwardCache, "%p - WebBackForwardCacheEntry::expirationTimerFired backForwardItemID: %s, hasSuspendedPage? %d", this, m_backForwardItemID.string().utf8().data(), !!m_suspendedPage);
    ASSERT(m_backForwardItemID);
    auto* item = WebBackForwardListItem::itemForID(m_backForwardItemID);
    ASSERT(item);
    m_backForwardCache.removeEntry(*item); // Will destroy |this|.
}

} // namespace WebKit
