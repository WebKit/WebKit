/*
 * Copyright (C) 2007, 2014 Apple Inc.  All rights reserved.
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
#include "DatabaseManager.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FeatureCounter.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderStateMachine.h"
#include "FrameView.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryPressureHandler.h"
#include "Page.h"
#include "Settings.h"
#include "SharedWorkerRepository.h"
#include "SubframeLoader.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

#if ENABLE(PROXIMITY_EVENTS)
#include "DeviceProximityController.h"
#endif

#if PLATFORM(IOS)
#include "MemoryPressureHandler.h"
#endif

namespace WebCore {

#if !defined(NDEBUG)

#define PCLOG(...) LOG(PageCache, "%*s%s", indentLevel*4, "", makeString(__VA_ARGS__).utf8().data())

#else

#define PCLOG(...) ((void)0)

#endif // !defined(NDEBUG)
    
// Used in histograms, please only add at the end, and do not remove elements (renaming e.g. to "FooEnumUnused1" is fine).
// This is because statistics may be gathered from histograms between versions over time, and re-using values causes collisions.
enum ReasonFrameCannotBeInPageCache {
    NoDocumentLoader = 0,
    MainDocumentError,
    IsErrorPage,
    HasPlugins,
    IsHttpsAndCacheControlled,
    HasUnloadListener,
    HasDatabaseHandles,
    HasSharedWorkers,
    NoHistoryItem,
    QuickRedirectComing,
    IsLoadingInAPISense,
    IsStopping,
    CannotSuspendActiveDOMObjects,
    DocumentLoaderUsesApplicationCache,
    ClientDeniesCaching,
    NumberOfReasonsFramesCannotBeInPageCache,
};
COMPILE_ASSERT(NumberOfReasonsFramesCannotBeInPageCache <= sizeof(unsigned)*8, ReasonFrameCannotBeInPageCacheDoesNotFitInBitmap);

static unsigned logCanCacheFrameDecision(Frame* frame, int indentLevel)
{
    PCLOG("+---");
    if (!frame->loader().documentLoader()) {
        PCLOG("   -There is no DocumentLoader object");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureNoDocumentLoaderKey);
        return 1 << NoDocumentLoader;
    }

    URL currentURL = frame->loader().documentLoader()->url();
    URL newURL = frame->loader().provisionalDocumentLoader() ? frame->loader().provisionalDocumentLoader()->url() : URL();
    if (!newURL.isEmpty())
        PCLOG(" Determining if frame can be cached navigating from (", currentURL.string(), ") to (", newURL.string(), "):");
    else
        PCLOG(" Determining if subframe with URL (", currentURL.string(), ") can be cached:");
     
    unsigned rejectReasons = 0;
    if (!frame->loader().documentLoader()->mainDocumentError().isNull()) {
        PCLOG("   -Main document has an error");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureMainDocumentErrorKey);
#if !PLATFORM(IOS)
        rejectReasons |= 1 << MainDocumentError;
#else
        if (frame->loader().documentLoader()->mainDocumentError().isCancellation() && frame->loader().documentLoader()->subresourceLoadersArePageCacheAcceptable())
            PCLOG("    -But, it was a cancellation and all loaders during the cancel were loading images.");
        else
            rejectReasons |= 1 << MainDocumentError;
#endif
    }
    if (frame->loader().documentLoader()->substituteData().isValid() && frame->loader().documentLoader()->substituteData().failingURL().isEmpty()) {
        PCLOG("   -Frame is an error page");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureIsErrorPageKey);
        rejectReasons |= 1 << IsErrorPage;
    }
    if (frame->loader().subframeLoader().containsPlugins() && !frame->page()->settings().pageCacheSupportsPlugins()) {
        PCLOG("   -Frame contains plugins");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureHasPlugins);
        rejectReasons |= 1 << HasPlugins;
    }
    if (frame->document()->url().protocolIs("https")
        && (frame->loader().documentLoader()->response().cacheControlContainsNoCache()
            || frame->loader().documentLoader()->response().cacheControlContainsNoStore())) {
        if (frame->loader().documentLoader()->response().cacheControlContainsNoCache())
            FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureHTTPSNoCacheKey);
        if (frame->loader().documentLoader()->response().cacheControlContainsNoStore())
            FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureHTTPSNoStoreKey);
        PCLOG("   -Frame is HTTPS, and cache control prohibits caching or storing");
        rejectReasons |= 1 << IsHttpsAndCacheControlled;
    }
    if (frame->document()->domWindow() && frame->document()->domWindow()->hasEventListeners(eventNames().unloadEvent)) {
        PCLOG("   -Frame has an unload event listener");
#if !PLATFORM(IOS)
        rejectReasons |= 1 << HasUnloadListener;
#else
        // iOS allows pages with unload event listeners to enter the page cache.
        PCLOG("    -BUT iOS allows these pages to be cached.");
#endif
    }
#if ENABLE(SQL_DATABASE)
    if (DatabaseManager::manager().hasOpenDatabases(frame->document())) {
        PCLOG("   -Frame has open database handles");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureHasOpenDatabasesKey);
        rejectReasons |= 1 << HasDatabaseHandles;
    }
#endif
#if ENABLE(SHARED_WORKERS)
    if (SharedWorkerRepository::hasSharedWorkers(frame->document())) {
        PCLOG("   -Frame has associated SharedWorkers");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureHasSharedWorkersKey);
        rejectReasons |= 1 << HasSharedWorkers;
    }
#endif
    if (!frame->loader().history().currentItem()) {
        PCLOG("   -No current history item");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureNoCurrentHistoryItemKey);
        rejectReasons |= 1 << NoHistoryItem;
    }
    if (frame->loader().quickRedirectComing()) {
        PCLOG("   -Quick redirect is coming");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureQuirkRedirectComingKey);
        rejectReasons |= 1 << QuickRedirectComing;
    }
    if (frame->loader().documentLoader()->isLoadingInAPISense()) {
        PCLOG("   -DocumentLoader is still loading in API sense");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureLoadingAPISenseKey);
        rejectReasons |= 1 << IsLoadingInAPISense;
    }
    if (frame->loader().documentLoader()->isStopping()) {
        PCLOG("   -DocumentLoader is in the middle of stopping");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureDocumentLoaderStoppingKey);
        rejectReasons |= 1 << IsStopping;
    }
    if (!frame->document()->canSuspendActiveDOMObjects()) {
        PCLOG("   -The document cannot suspend its active DOM Objects");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureCannotSuspendActiveDOMObjectsKey);
        rejectReasons |= 1 << CannotSuspendActiveDOMObjects;
    }
    if (!frame->loader().documentLoader()->applicationCacheHost()->canCacheInPageCache()) {
        PCLOG("   -The DocumentLoader uses an application cache");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureApplicationCacheKey);
        rejectReasons |= 1 << DocumentLoaderUsesApplicationCache;
    }
    if (!frame->loader().client().canCachePage()) {
        PCLOG("   -The client says this frame cannot be cached");
        FEATURE_COUNTER_INCREMENT_KEY(frame->page(), FeatureCounterPageCacheFailureDeniedByClientKey);
        rejectReasons |= 1 << ClientDeniesCaching;
    }

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        rejectReasons |= logCanCacheFrameDecision(child, indentLevel + 1);
    
    PCLOG(rejectReasons ? " Frame CANNOT be cached" : " Frame CAN be cached");
    PCLOG("+---");
    
    return rejectReasons;
}

// Used in histograms, please only add at the end, and do not remove elements (renaming e.g. to "FooEnumUnused1" is fine).
// This is because statistics may be gathered from histograms between versions over time, and re-using values causes collisions.
enum ReasonPageCannotBeInPageCache {
    FrameCannotBeInPageCache = 0,
    DisabledBackForwardList,
    DisabledPageCache,
    UsesDeviceMotion,
    UsesDeviceOrientation,
    IsReload,
    IsReloadFromOrigin,
    IsSameLoad,
    NumberOfReasonsPagesCannotBeInPageCache,
};
COMPILE_ASSERT(NumberOfReasonsPagesCannotBeInPageCache <= sizeof(unsigned)*8, ReasonPageCannotBeInPageCacheDoesNotFitInBitmap);

static void logCanCachePageDecision(Page* page)
{
    // Only bother logging for main frames that have actually loaded and have content.
    if (page->mainFrame().loader().stateMachine().creatingInitialEmptyDocument())
        return;
    URL currentURL = page->mainFrame().loader().documentLoader() ? page->mainFrame().loader().documentLoader()->url() : URL();
    if (currentURL.isEmpty())
        return;
    
    int indentLevel = 0;    
    PCLOG("--------\n Determining if page can be cached:");
    
    unsigned rejectReasons = 0;
    unsigned frameRejectReasons = logCanCacheFrameDecision(&page->mainFrame(), indentLevel+1);
    if (frameRejectReasons)
        rejectReasons |= 1 << FrameCannotBeInPageCache;
    
    if (!page->settings().usesPageCache()) {
        PCLOG("   -Page settings says b/f cache disabled");
        rejectReasons |= 1 << DisabledPageCache;
    }
#if ENABLE(DEVICE_ORIENTATION) && !PLATFORM(IOS)
    if (DeviceMotionController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceMotion");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureDeviceMotionKey);
        rejectReasons |= 1 << UsesDeviceMotion;
    }
    if (DeviceOrientationController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceOrientation");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureDeviceOrientationKey);
        rejectReasons |= 1 << UsesDeviceOrientation;
    }
#endif
#if ENABLE(PROXIMITY_EVENTS)
    if (DeviceProximityController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceProximity");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureDeviceProximityKey);
        rejectReasons |= 1 << UsesDeviceMotion;
    }
#endif
    FrameLoadType loadType = page->mainFrame().loader().loadType();
    if (loadType == FrameLoadType::Reload) {
        PCLOG("   -Load type is: Reload");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureReloadKey);
        rejectReasons |= 1 << IsReload;
    }
    if (loadType == FrameLoadType::ReloadFromOrigin) {
        PCLOG("   -Load type is: Reload from origin");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureReloadFromOriginKey);
        rejectReasons |= 1 << IsReloadFromOrigin;
    }
    if (loadType == FrameLoadType::Same) {
        PCLOG("   -Load type is: Same");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureSameLoadKey);
        rejectReasons |= 1 << IsSameLoad;
    }
    
    if (rejectReasons) {
        PCLOG(" Page CANNOT be cached\n--------");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureKey);
    } else {
        PCLOG(" Page CAN be cached\n--------");
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheSuccessKey);
    }
}

PageCache* pageCache()
{
    static PageCache* staticPageCache = new PageCache;
    return staticPageCache;
}

PageCache::PageCache()
    : m_capacity(0)
    , m_size(0)
    , m_head(0)
    , m_tail(0)
    , m_shouldClearBackingStores(false)
{
}
    
bool PageCache::canCachePageContainingThisFrame(Frame* frame)
{
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!canCachePageContainingThisFrame(child))
            return false;
    }
    
    FrameLoader& frameLoader = frame->loader();
    DocumentLoader* documentLoader = frameLoader.documentLoader();
    Document* document = frame->document();
    
    return documentLoader
#if !PLATFORM(IOS)
        && documentLoader->mainDocumentError().isNull()
#else
        && (documentLoader->mainDocumentError().isNull() || (documentLoader->mainDocumentError().isCancellation() && documentLoader->subresourceLoadersArePageCacheAcceptable()))
#endif
        // Do not cache error pages (these can be recognized as pages with substitute data or unreachable URLs).
        && !(documentLoader->substituteData().isValid() && !documentLoader->substituteData().failingURL().isEmpty())
        && (!frameLoader.subframeLoader().containsPlugins() || frame->page()->settings().pageCacheSupportsPlugins())
        && (!document->url().protocolIs("https") || (!documentLoader->response().cacheControlContainsNoCache() && !documentLoader->response().cacheControlContainsNoStore()))
#if !PLATFORM(IOS)
        && (!document->domWindow() || !document->domWindow()->hasEventListeners(eventNames().unloadEvent))
#endif
#if ENABLE(SQL_DATABASE)
        && !DatabaseManager::manager().hasOpenDatabases(document)
#endif
#if ENABLE(SHARED_WORKERS)
        && !SharedWorkerRepository::hasSharedWorkers(document)
#endif
        && frameLoader.history().currentItem()
        && !frameLoader.quickRedirectComing()
        && !documentLoader->isLoadingInAPISense()
        && !documentLoader->isStopping()
        && document->canSuspendActiveDOMObjects()
        // FIXME: We should investigating caching frames that have an associated
        // application cache. <rdar://problem/5917899> tracks that work.
        && documentLoader->applicationCacheHost()->canCacheInPageCache()
        && frameLoader.client().canCachePage();
}
    
bool PageCache::canCache(Page* page) const
{
    if (!page)
        return false;
    
    logCanCachePageDecision(page);

    if (memoryPressureHandler().isUnderMemoryPressure())
        return false;

    // Cache the page, if possible.
    // Don't write to the cache if in the middle of a redirect, since we will want to
    // store the final page we end up on.
    // No point writing to the cache on a reload or loadSame, since we will just write
    // over it again when we leave that page.
    FrameLoadType loadType = page->mainFrame().loader().loadType();
    
    return m_capacity > 0
        && canCachePageContainingThisFrame(&page->mainFrame())
        && page->settings().usesPageCache()
#if ENABLE(DEVICE_ORIENTATION) && !PLATFORM(IOS)
        && !DeviceMotionController::isActiveAt(page)
        && !DeviceOrientationController::isActiveAt(page)
#endif
#if ENABLE(PROXIMITY_EVENTS)
        && !DeviceProximityController::isActiveAt(page)
#endif
        && (loadType == FrameLoadType::Standard
            || loadType == FrameLoadType::Back
            || loadType == FrameLoadType::Forward
            || loadType == FrameLoadType::IndexedBackForward);
}

void PageCache::pruneToCapacityNow(int capacity)
{
    int savedCapacity = m_capacity;
    setCapacity(capacity);
    setCapacity(savedCapacity);
}

void PageCache::setCapacity(int capacity)
{
    ASSERT(capacity >= 0);
    m_capacity = std::max(capacity, 0);

    prune();
}

int PageCache::frameCount() const
{
    int frameCount = 0;
    for (HistoryItem* current = m_head; current; current = current->m_next) {
        ++frameCount;
        ASSERT(current->m_cachedPage);
        frameCount += current->m_cachedPage ? current->m_cachedPage->cachedMainFrame()->descendantFrameCount() : 0;
    }
    
    return frameCount;
}

void PageCache::markPagesForVistedLinkStyleRecalc()
{
    for (HistoryItem* current = m_head; current; current = current->m_next) {
        if (current->m_cachedPage)
            current->m_cachedPage->markForVistedLinkStyleRecalc();
    }
}

void PageCache::markPagesForFullStyleRecalc(Page* page)
{
    for (HistoryItem* current = m_head; current; current = current->m_next) {
        CachedPage* cachedPage = current->m_cachedPage.get();
        if (cachedPage && &page->mainFrame() == &cachedPage->cachedMainFrame()->view()->frame())
            cachedPage->markForFullStyleRecalc();
    }
}

void PageCache::markPagesForDeviceScaleChanged(Page* page)
{
    for (HistoryItem* current = m_head; current; current = current->m_next) {
        CachedPage* cachedPage = current->m_cachedPage.get();
        if (cachedPage && &page->mainFrame() == &cachedPage->cachedMainFrame()->view()->frame())
            cachedPage->markForDeviceScaleChanged();
    }
}

#if ENABLE(VIDEO_TRACK)
void PageCache::markPagesForCaptionPreferencesChanged()
{
    for (HistoryItem* current = m_head; current; current = current->m_next) {
        if (current->m_cachedPage)
            current->m_cachedPage->markForCaptionPreferencesChanged();
    }
}
#endif

void PageCache::add(PassRefPtr<HistoryItem> prpItem, Page& page)
{
    ASSERT(prpItem);
    ASSERT(canCache(&page));
    
    HistoryItem* item = prpItem.leakRef(); // Balanced in remove().

    // Remove stale cache entry if necessary.
    if (item->m_cachedPage)
        remove(item);

    item->m_cachedPage = std::make_unique<CachedPage>(page);
    item->m_wasPruned = false;
    addToLRUList(item);
    ++m_size;
    
    prune();
}

std::unique_ptr<CachedPage> PageCache::take(HistoryItem* item, Page* page)
{
    if (!item)
        return nullptr;

    std::unique_ptr<CachedPage> cachedPage = WTF::move(item->m_cachedPage);

    removeFromLRUList(item);
    --m_size;

    item->deref(); // Balanced in add().

    if (!cachedPage) {
        if (item->m_wasPruned)
            FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureWasPrunedKey);
        return nullptr;
    }

    if (cachedPage->hasExpired()) {
        LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", item->url().string().ascii().data());
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureExpiredKey);
        return nullptr;
    }

    return cachedPage;
}

CachedPage* PageCache::get(HistoryItem* item, Page* page)
{
    if (!item)
        return nullptr;

    if (CachedPage* cachedPage = item->m_cachedPage.get()) {
        if (!cachedPage->hasExpired())
            return cachedPage;
        
        LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", item->url().string().ascii().data());
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureExpiredKey);
        pageCache()->remove(item);
    } else if (item->m_wasPruned)
        FEATURE_COUNTER_INCREMENT_KEY(page, FeatureCounterPageCacheFailureWasPrunedKey);

    return nullptr;
}

void PageCache::remove(HistoryItem* item)
{
    // Safely ignore attempts to remove items not in the cache.
    if (!item || !item->m_cachedPage)
        return;

    item->m_cachedPage = nullptr;
    removeFromLRUList(item);
    --m_size;

    item->deref(); // Balanced in add().
}

void PageCache::prune()
{
    while (m_size > m_capacity) {
        ASSERT(m_tail && m_tail->m_cachedPage);
        m_tail->m_wasPruned = true;
        remove(m_tail);
    }
}

void PageCache::addToLRUList(HistoryItem* item)
{
    item->m_next = m_head;
    item->m_prev = 0;

    if (m_head) {
        ASSERT(m_tail);
        m_head->m_prev = item;
    } else {
        ASSERT(!m_tail);
        m_tail = item;
    }

    m_head = item;
}

void PageCache::removeFromLRUList(HistoryItem* item)
{
    if (!item->m_next) {
        ASSERT(item == m_tail);
        m_tail = item->m_prev;
    } else {
        ASSERT(item != m_tail);
        item->m_next->m_prev = item->m_prev;
    }

    if (!item->m_prev) {
        ASSERT(item == m_head);
        m_head = item->m_next;
    } else {
        ASSERT(item != m_head);
        item->m_prev->m_next = item->m_next;
    }
}

} // namespace WebCore
