/*
 * Copyright (C) 2007, 2014, 2015 Apple Inc. All rights reserved.
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
#include "PageCache.h"

#include "ApplicationCacheHost.h"
#include "BackForwardController.h"
#include "MemoryCache.h"
#include "CachedPage.h"
#include "DOMWindow.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "HistoryController.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryPressureHandler.h"
#include "Page.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

#if ENABLE(PROXIMITY_EVENTS)
#include "DeviceProximityController.h"
#endif

namespace WebCore {

#define PCLOG(...) LOG(PageCache, "%*s%s", indentLevel*4, "", makeString(__VA_ARGS__).utf8().data())

static inline void logPageCacheFailureDiagnosticMessage(DiagnosticLoggingClient& client, const String& reason)
{
    client.logDiagnosticMessageWithValue(DiagnosticLoggingKeys::pageCacheKey(), DiagnosticLoggingKeys::failureKey(), reason, ShouldSample::Yes);
}

static inline void logPageCacheFailureDiagnosticMessage(Page* page, const String& reason)
{
    if (!page)
        return;

    logPageCacheFailureDiagnosticMessage(page->mainFrame().diagnosticLoggingClient(), reason);
}

static bool canCacheFrame(Frame& frame, DiagnosticLoggingClient& diagnosticLoggingClient, unsigned indentLevel)
{
    PCLOG("+---");
    FrameLoader& frameLoader = frame.loader();

    // Prevent page caching if a subframe is still in provisional load stage.
    // We only do this check for subframes because the main frame is reused when navigating to a new page.
    if (!frame.isMainFrame() && frameLoader.state() == FrameStateProvisional) {
        PCLOG("   -Frame is in provisional load stage");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::provisionalLoadKey());
        return false;
    }

    DocumentLoader* documentLoader = frameLoader.documentLoader();
    if (!documentLoader) {
        PCLOG("   -There is no DocumentLoader object");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::noDocumentLoaderKey());
        return false;
    }

    URL currentURL = documentLoader->url();
    URL newURL = frameLoader.provisionalDocumentLoader() ? frameLoader.provisionalDocumentLoader()->url() : URL();
    if (!newURL.isEmpty())
        PCLOG(" Determining if frame can be cached navigating from (", currentURL.string(), ") to (", newURL.string(), "):");
    else
        PCLOG(" Determining if subframe with URL (", currentURL.string(), ") can be cached:");
     
    bool isCacheable = true;
    if (!documentLoader->mainDocumentError().isNull()) {
        PCLOG("   -Main document has an error");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::mainDocumentErrorKey());

        if (documentLoader->mainDocumentError().isCancellation() && documentLoader->subresourceLoadersArePageCacheAcceptable())
            PCLOG("    -But, it was a cancellation and all loaders during the cancelation were loading images or XHR.");
        else
            isCacheable = false;
    }
    // Do not cache error pages (these can be recognized as pages with substitute data or unreachable URLs).
    if (documentLoader->substituteData().isValid() && !documentLoader->substituteData().failingURL().isEmpty()) {
        PCLOG("   -Frame is an error page");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isErrorPageKey());
        isCacheable = false;
    }
    if (frameLoader.subframeLoader().containsPlugins() && !frame.page()->settings().pageCacheSupportsPlugins()) {
        PCLOG("   -Frame contains plugins");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::hasPluginsKey());
        isCacheable = false;
    }
    if (frame.isMainFrame() && frame.document() && frame.document()->url().protocolIs("https") && documentLoader->response().cacheControlContainsNoStore()) {
        PCLOG("   -Frame is HTTPS, and cache control prohibits storing");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::httpsNoStoreKey());
        isCacheable = false;
    }
    if (frame.isMainFrame() && !frameLoader.history().currentItem()) {
        PCLOG("   -Main frame has no current history item");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::noCurrentHistoryItemKey());
        isCacheable = false;
    }
    if (frameLoader.quickRedirectComing()) {
        PCLOG("   -Quick redirect is coming");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::quirkRedirectComingKey());
        isCacheable = false;
    }
    if (documentLoader->isLoading()) {
        PCLOG("   -DocumentLoader is still loading");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isLoadingKey());
        isCacheable = false;
    }
    if (documentLoader->isStopping()) {
        PCLOG("   -DocumentLoader is in the middle of stopping");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::documentLoaderStoppingKey());
        isCacheable = false;
    }

    Vector<ActiveDOMObject*> unsuspendableObjects;
    if (frame.document() && !frame.document()->canSuspendActiveDOMObjectsForDocumentSuspension(&unsuspendableObjects)) {
        PCLOG("   -The document cannot suspend its active DOM Objects");
        for (auto* activeDOMObject : unsuspendableObjects) {
            PCLOG("    - Unsuspendable: ", activeDOMObject->activeDOMObjectName());
            diagnosticLoggingClient.logDiagnosticMessageWithValue(DiagnosticLoggingKeys::pageCacheKey(), DiagnosticLoggingKeys::unsuspendableDOMObjectKey(), activeDOMObject->activeDOMObjectName(), ShouldSample::Yes);
            UNUSED_PARAM(activeDOMObject);
        }
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey());
        isCacheable = false;
    }
    // FIXME: We should investigating caching frames that have an associated
    // application cache. <rdar://problem/5917899> tracks that work.
    if (!documentLoader->applicationCacheHost()->canCacheInPageCache()) {
        PCLOG("   -The DocumentLoader uses an application cache");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::applicationCacheKey());
        isCacheable = false;
    }
    if (!frameLoader.client().canCachePage()) {
        PCLOG("   -The client says this frame cannot be cached");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deniedByClientKey());
        isCacheable = false;
    }

    for (Frame* child = frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!canCacheFrame(*child, diagnosticLoggingClient, indentLevel + 1))
            isCacheable = false;
    }
    
    PCLOG(isCacheable ? " Frame CAN be cached" : " Frame CANNOT be cached");
    PCLOG("+---");
    
    return isCacheable;
}

static bool canCachePage(Page& page)
{
    unsigned indentLevel = 0;
    PCLOG("--------\n Determining if page can be cached:");
    
    MainFrame& mainFrame = page.mainFrame();
    DiagnosticLoggingClient& diagnosticLoggingClient = mainFrame.diagnosticLoggingClient();
    bool isCacheable = canCacheFrame(mainFrame, diagnosticLoggingClient, indentLevel + 1);
    
    if (!page.settings().usesPageCache()) {
        PCLOG("   -Page settings says b/f cache disabled");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isDisabledKey());
        isCacheable = false;
    }
#if ENABLE(DEVICE_ORIENTATION) && !PLATFORM(IOS)
    if (DeviceMotionController::isActiveAt(&page)) {
        PCLOG("   -Page is using DeviceMotion");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deviceMotionKey());
        isCacheable = false;
    }
    if (DeviceOrientationController::isActiveAt(&page)) {
        PCLOG("   -Page is using DeviceOrientation");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deviceOrientationKey());
        isCacheable = false;
    }
#endif
#if ENABLE(PROXIMITY_EVENTS)
    if (DeviceProximityController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceProximity");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, deviceProximityKey);
        isCacheable = false;
    }
#endif
    FrameLoadType loadType = page.mainFrame().loader().loadType();
    switch (loadType) {
    case FrameLoadType::Reload:
        // No point writing to the cache on a reload, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: Reload");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadKey());
        isCacheable = false;
        break;
    case FrameLoadType::Same: // user loads same URL again (but not reload button)
        // No point writing to the cache on a same load, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: Same");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::sameLoadKey());
        isCacheable = false;
        break;
    case FrameLoadType::RedirectWithLockedBackForwardList:
        // Don't write to the cache if in the middle of a redirect, since we will want to store the final page we end up on.
        PCLOG("   -Load type is: RedirectWithLockedBackForwardList");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::redirectKey());
        isCacheable = false;
        break;
    case FrameLoadType::Replace:
        // No point writing to the cache on a replace, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: Replace");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::replaceKey());
        isCacheable = false;
        break;
    case FrameLoadType::ReloadFromOrigin: {
        // No point writing to the cache on a reload, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: ReloadFromOrigin");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadFromOriginKey());
        isCacheable = false;
        break;
    }
    case FrameLoadType::Standard:
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward: // a multi-item hop in the backforward list
        // Cacheable.
        break;
    }
    
    if (isCacheable)
        PCLOG(" Page CAN be cached\n--------");
    else
        PCLOG(" Page CANNOT be cached\n--------");

    diagnosticLoggingClient.logDiagnosticMessageWithResult(DiagnosticLoggingKeys::pageCacheKey(), DiagnosticLoggingKeys::canCacheKey(), isCacheable ? DiagnosticLoggingResultPass : DiagnosticLoggingResultFail, ShouldSample::Yes);
    return isCacheable;
}

PageCache& PageCache::singleton()
{
    static NeverDestroyed<PageCache> globalPageCache;
    return globalPageCache;
}
    
bool PageCache::canCache(Page* page) const
{
    if (!page)
        return false;

    if (!m_maxSize) {
        logPageCacheFailureDiagnosticMessage(page, DiagnosticLoggingKeys::isDisabledKey());
        return false;
    }

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        logPageCacheFailureDiagnosticMessage(page, DiagnosticLoggingKeys::underMemoryPressureKey());
        return false;
    }
    
    return canCachePage(*page);
}

void PageCache::pruneToSizeNow(unsigned size, PruningReason pruningReason)
{
    TemporaryChange<unsigned> change(m_maxSize, size);
    prune(pruningReason);
}

void PageCache::setMaxSize(unsigned maxSize)
{
    m_maxSize = maxSize;
    prune(PruningReason::None);
}

unsigned PageCache::frameCount() const
{
    unsigned frameCount = m_items.size();
    for (auto& item : m_items) {
        ASSERT(item->m_cachedPage);
        frameCount += item->m_cachedPage->cachedMainFrame()->descendantFrameCount();
    }
    
    return frameCount;
}

void PageCache::markPagesForVisitedLinkStyleRecalc()
{
    for (auto& item : m_items) {
        ASSERT(item->m_cachedPage);
        item->m_cachedPage->markForVisitedLinkStyleRecalc();
    }
}

void PageCache::markPagesForFullStyleRecalc(Page& page)
{
    for (auto& item : m_items) {
        CachedPage& cachedPage = *item->m_cachedPage;
        if (&page.mainFrame() == &cachedPage.cachedMainFrame()->view()->frame())
            cachedPage.markForFullStyleRecalc();
    }
}

void PageCache::markPagesForDeviceOrPageScaleChanged(Page& page)
{
    for (auto& item : m_items) {
        CachedPage& cachedPage = *item->m_cachedPage;
        if (&page.mainFrame() == &cachedPage.cachedMainFrame()->view()->frame())
            cachedPage.markForDeviceOrPageScaleChanged();
    }
}

void PageCache::markPagesForContentsSizeChanged(Page& page)
{
    for (auto& item : m_items) {
        CachedPage& cachedPage = *item->m_cachedPage;
        if (&page.mainFrame() == &cachedPage.cachedMainFrame()->view()->frame())
            cachedPage.markForContentsSizeChanged();
    }
}

#if ENABLE(VIDEO_TRACK)
void PageCache::markPagesForCaptionPreferencesChanged()
{
    for (auto& item : m_items) {
        ASSERT(item->m_cachedPage);
        item->m_cachedPage->markForCaptionPreferencesChanged();
    }
}
#endif

static String pruningReasonToDiagnosticLoggingKey(PruningReason pruningReason)
{
    switch (pruningReason) {
    case PruningReason::MemoryPressure:
        return DiagnosticLoggingKeys::prunedDueToMemoryPressureKey();
    case PruningReason::ProcessSuspended:
        return DiagnosticLoggingKeys::prunedDueToProcessSuspended();
    case PruningReason::ReachedMaxSize:
        return DiagnosticLoggingKeys::prunedDueToMaxSizeReached();
    case PruningReason::None:
        break;
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

void PageCache::add(HistoryItem& item, Page& page)
{
    ASSERT(canCache(&page));

    // Remove stale cache entry if necessary.
    remove(item);

    item.m_cachedPage = std::make_unique<CachedPage>(page);
    item.m_pruningReason = PruningReason::None;
    m_items.add(&item);
    
    prune(PruningReason::ReachedMaxSize);
}

std::unique_ptr<CachedPage> PageCache::take(HistoryItem& item, Page* page)
{
    if (!item.m_cachedPage) {
        if (item.m_pruningReason != PruningReason::None)
            logPageCacheFailureDiagnosticMessage(page, pruningReasonToDiagnosticLoggingKey(item.m_pruningReason));
        return nullptr;
    }

    m_items.remove(&item);
    std::unique_ptr<CachedPage> cachedPage = WTFMove(item.m_cachedPage);

    if (cachedPage->hasExpired()) {
        LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", item.url().string().ascii().data());
        logPageCacheFailureDiagnosticMessage(page, DiagnosticLoggingKeys::expiredKey());
        return nullptr;
    }

    return cachedPage;
}

CachedPage* PageCache::get(HistoryItem& item, Page* page)
{
    CachedPage* cachedPage = item.m_cachedPage.get();
    if (!cachedPage) {
        if (item.m_pruningReason != PruningReason::None)
            logPageCacheFailureDiagnosticMessage(page, pruningReasonToDiagnosticLoggingKey(item.m_pruningReason));
        return nullptr;
    }

    if (cachedPage->hasExpired()) {
        LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", item.url().string().ascii().data());
        logPageCacheFailureDiagnosticMessage(page, DiagnosticLoggingKeys::expiredKey());
        remove(item);
        return nullptr;
    }
    return cachedPage;
}

void PageCache::remove(HistoryItem& item)
{
    // Safely ignore attempts to remove items not in the cache.
    if (!item.m_cachedPage)
        return;

    m_items.remove(&item);
    item.m_cachedPage = nullptr;
}

void PageCache::prune(PruningReason pruningReason)
{
    while (pageCount() > maxSize()) {
        auto oldestItem = m_items.takeFirst();
        oldestItem->m_cachedPage = nullptr;
        oldestItem->m_pruningReason = pruningReason;
    }
}

} // namespace WebCore
