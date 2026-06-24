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
#include "WebBackForwardListFrameItem.h"
#include "WebBackForwardListItem.h"
#include "WebFrameProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

static const Seconds expirationDelay { 30_min };

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebBackForwardCacheEntry);

Ref<WebBackForwardCacheEntry> WebBackForwardCacheEntry::create(WebBackForwardCache& backForwardCache, WebCore::BackForwardItemIdentifier backForwardItemID, WebCore::BackForwardFrameItemIdentifier backForwardFrameItemID, WebCore::ProcessIdentifier processIdentifier, RefPtr<SuspendedPageProxy>&& suspendedPage)
{
    return adoptRef(*new WebBackForwardCacheEntry(backForwardCache, backForwardItemID, backForwardFrameItemID, processIdentifier, WTF::move(suspendedPage)));
}

WebBackForwardCacheEntry::WebBackForwardCacheEntry(WebBackForwardCache& backForwardCache, WebCore::BackForwardItemIdentifier backForwardItemID, WebCore::BackForwardFrameItemIdentifier backForwardFrameItemID, WebCore::ProcessIdentifier processIdentifier, RefPtr<SuspendedPageProxy>&& suspendedPage)
    : m_backForwardCache(backForwardCache)
    , m_processIdentifier(processIdentifier)
    , m_backForwardItemID(backForwardItemID)
    , m_backForwardFrameItemID(backForwardFrameItemID)
    , m_suspendedPage(WTF::move(suspendedPage))
    , m_expirationTimer(RunLoop::mainSingleton(), "WebBackForwardCacheEntry::ExpirationTimer"_s, this, &WebBackForwardCacheEntry::expirationTimerFired)
{
    m_expirationTimer.startOneShot(expirationDelay);
}

WebBackForwardCacheEntry::~WebBackForwardCacheEntry()
{
    // m_backForwardFrameItemID is cleared by takeSuspendedPage() and
    // takeForRestoration(), so this skips on the BFCache restore path.
    if (!m_backForwardFrameItemID)
        return;

    for (auto& process : allProcesses())
        process->sendWithAsyncReply(Messages::WebProcess::ClearCachedPage(*m_backForwardFrameItemID), [] { });
}

WebBackForwardCache* WebBackForwardCacheEntry::backForwardCache() const
{
    return m_backForwardCache.get();
}

void WebBackForwardCacheEntry::setSuspendedPage(Ref<SuspendedPageProxy>&& suspendedPage)
{
    m_suspendedPage = WTF::move(suspendedPage);
}

void WebBackForwardCacheEntry::clearSuspendedPage()
{
    m_suspendedPage = nullptr;
}

Ref<SuspendedPageProxy> WebBackForwardCacheEntry::takeSuspendedPage()
{
    ASSERT(m_suspendedPage);
    markAsTakenForRestoration();
    return std::exchange(m_suspendedPage, nullptr).releaseNonNull();
}

RefPtr<WebProcessProxy> WebBackForwardCacheEntry::process() const
{
    auto process = WebProcessProxy::processForIdentifier(m_processIdentifier);
    ASSERT(process);
    ASSERT(!m_suspendedPage || process == &m_suspendedPage->process());
    return process;
}

HashSet<Ref<WebProcessProxy>> WebBackForwardCacheEntry::iframeProcesses() const
{
    HashSet<Ref<WebProcessProxy>> result;
    for (Ref<WebFrameProxy> root : m_cachedChildren) {
        result.add(Ref { root->process() });
        for (RefPtr frame = root->traverseNext().frame; frame; frame = frame->traverseNext().frame)
            result.add(Ref { frame->process() });
    }

    if (RefPtr suspendedPage = m_suspendedPage; suspendedPage && suspendedPage->suspendedFrameItemID() == m_backForwardFrameItemID)
        result.addAll(suspendedPage->iframeProcesses());

    return result;
}

HashSet<Ref<WebProcessProxy>> WebBackForwardCacheEntry::allProcesses() const
{
    HashSet<Ref<WebProcessProxy>> result = iframeProcesses();
    if (RefPtr mainProcess = process())
        result.add(mainProcess.releaseNonNull());
    return result;
}

void WebBackForwardCacheEntry::setCachedChildren(Vector<Ref<WebFrameProxy>>&& children)
{
    ASSERT(m_cachedChildren.isEmpty());
    m_cachedChildren = WTF::move(children);
}

std::pair<Vector<Ref<WebFrameProxy>>, Vector<Ref<WebProcessProxy>>> WebBackForwardCacheEntry::takeForRestoration()
{
    markAsTakenForRestoration();
    auto children = std::exchange(m_cachedChildren, { });
    Vector<Ref<WebProcessProxy>> processes;
    HashSet<WebCore::ProcessIdentifier> visited;
    for (auto& root : children) {
        Ref rootProcess = root->process();
        if (visited.add(rootProcess->coreProcessIdentifier()).isNewEntry)
            processes.append(WTF::move(rootProcess));
        for (RefPtr frame = root->traverseNext().frame; frame; frame = frame->traverseNext().frame) {
            Ref frameProcess = frame->process();
            if (visited.add(frameProcess->coreProcessIdentifier()).isNewEntry)
                processes.append(WTF::move(frameProcess));
        }
    }
    return { WTF::move(children), WTF::move(processes) };
}

bool WebBackForwardCacheEntry::referencesIframeProcess(WebCore::ProcessIdentifier processIdentifier) const
{
    for (Ref<WebFrameProxy> root : m_cachedChildren) {
        if (root->process().coreProcessIdentifier() == processIdentifier)
            return true;
        for (RefPtr frame = root->traverseNext().frame; frame; frame = frame->traverseNext().frame) {
            if (frame->process().coreProcessIdentifier() == processIdentifier)
                return true;
        }
    }
    return false;
}

void WebBackForwardCacheEntry::markAsTakenForRestoration()
{
    m_backForwardItemID = std::nullopt;
    m_backForwardFrameItemID = std::nullopt;
    m_expirationTimer.stop();
}

void WebBackForwardCacheEntry::expirationTimerFired()
{
    ASSERT(m_backForwardItemID);
    ASSERT(m_backForwardFrameItemID);
    RELEASE_LOG(BackForwardCache, "%p - WebBackForwardCacheEntry::expirationTimerFired backForwardItemID=%s backForwardFrameItemID=%s, hasSuspendedPage=%d", this, m_backForwardItemID->toString().utf8().data(), m_backForwardFrameItemID->toString().utf8().data(), !!m_suspendedPage);
    RefPtr item = WebBackForwardListFrameItem::itemForID(*m_backForwardItemID, *m_backForwardFrameItemID);
    ASSERT(item);
    if (RefPtr backForwardCache = m_backForwardCache.get()) {
        if (RefPtr backForwardListItem = item->backForwardListItem())
            backForwardCache->removeEntry(*backForwardListItem);
    }
}

} // namespace WebKit
