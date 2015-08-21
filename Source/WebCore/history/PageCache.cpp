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
#include "FrameLoaderStateMachine.h"
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
    HasDatabaseHandles, // FIXME: Remove.
    HasSharedWorkers, // FIXME: Remove.
    NoHistoryItem,
    QuickRedirectComing,
    IsLoading,
    IsStopping,
    CannotSuspendActiveDOMObjects,
    DocumentLoaderUsesApplicationCache,
    ClientDeniesCaching,
    NumberOfReasonsFramesCannotBeInPageCache,
    IsInProvisionalLoadStage,
};
COMPILE_ASSERT(NumberOfReasonsFramesCannotBeInPageCache <= sizeof(unsigned)*8, ReasonFrameCannotBeInPageCacheDoesNotFitInBitmap);

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

static unsigned logCanCacheFrameDecision(Frame& frame, DiagnosticLoggingClient& diagnosticLoggingClient, unsigned indentLevel)
{
    PCLOG("+---");
    if (!frame.isMainFrame() && frame.loader().state() == FrameStateProvisional) {
        PCLOG("   -Frame is in provisional load stage");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::provisionalLoadKey());
        return 1 << IsInProvisionalLoadStage;
    }
    if (!frame.loader().documentLoader()) {
        PCLOG("   -There is no DocumentLoader object");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::noDocumentLoaderKey());
        return 1 << NoDocumentLoader;
    }

    URL currentURL = frame.loader().documentLoader()->url();
    URL newURL = frame.loader().provisionalDocumentLoader() ? frame.loader().provisionalDocumentLoader()->url() : URL();
    if (!newURL.isEmpty())
        PCLOG(" Determining if frame can be cached navigating from (", currentURL.string(), ") to (", newURL.string(), "):");
    else
        PCLOG(" Determining if subframe with URL (", currentURL.string(), ") can be cached:");
     
    unsigned rejectReasons = 0;
    if (!frame.loader().documentLoader()->mainDocumentError().isNull()) {
        PCLOG("   -Main document has an error");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::mainDocumentErrorKey());

        if (frame.loader().documentLoader()->mainDocumentError().isCancellation() && frame.loader().documentLoader()->subresourceLoadersArePageCacheAcceptable())
            PCLOG("    -But, it was a cancellation and all loaders during the cancelation were loading images or XHR.");
        else
            rejectReasons |= 1 << MainDocumentError;
    }
    if (frame.loader().documentLoader()->substituteData().isValid() && frame.loader().documentLoader()->substituteData().failingURL().isEmpty()) {
        PCLOG("   -Frame is an error page");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isErrorPageKey());
        rejectReasons |= 1 << IsErrorPage;
    }
    if (frame.loader().subframeLoader().containsPlugins() && !frame.page()->settings().pageCacheSupportsPlugins()) {
        PCLOG("   -Frame contains plugins");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::hasPluginsKey());
        rejectReasons |= 1 << HasPlugins;
    }
    if (frame.isMainFrame() && frame.document()->url().protocolIs("https") && frame.loader().documentLoader()->response().cacheControlContainsNoStore()) {
        PCLOG("   -Frame is HTTPS, and cache control prohibits storing");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::httpsNoStoreKey());
        rejectReasons |= 1 << IsHttpsAndCacheControlled;
    }
    if (!frame.loader().history().currentItem()) {
        PCLOG("   -No current history item");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::noCurrentHistoryItemKey());
        rejectReasons |= 1 << NoHistoryItem;
    }
    if (frame.loader().quickRedirectComing()) {
        PCLOG("   -Quick redirect is coming");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::quirkRedirectComingKey());
        rejectReasons |= 1 << QuickRedirectComing;
    }
    if (frame.loader().documentLoader()->isLoading()) {
        PCLOG("   -DocumentLoader is still loading");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isLoadingKey());
        rejectReasons |= 1 << IsLoading;
    }
    if (frame.loader().documentLoader()->isStopping()) {
        PCLOG("   -DocumentLoader is in the middle of stopping");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::documentLoaderStoppingKey());
        rejectReasons |= 1 << IsStopping;
    }

    Vector<ActiveDOMObject*> unsuspendableObjects;
    if (!frame.document()->canSuspendActiveDOMObjectsForPageCache(&unsuspendableObjects)) {
        PCLOG("   -The document cannot suspend its active DOM Objects");
        for (auto* activeDOMObject : unsuspendableObjects) {
            PCLOG("    - Unsuspendable: ", activeDOMObject->activeDOMObjectName());
            diagnosticLoggingClient.logDiagnosticMessageWithValue(DiagnosticLoggingKeys::pageCacheKey(), DiagnosticLoggingKeys::unsuspendableDOMObjectKey(), activeDOMObject->activeDOMObjectName(), ShouldSample::Yes);
            UNUSED_PARAM(activeDOMObject);
        }
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey());
        rejectReasons |= 1 << CannotSuspendActiveDOMObjects;
    }
    if (!frame.loader().documentLoader()->applicationCacheHost()->canCacheInPageCache()) {
        PCLOG("   -The DocumentLoader uses an application cache");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::applicationCacheKey());
        rejectReasons |= 1 << DocumentLoaderUsesApplicationCache;
    }
    if (!frame.loader().client().canCachePage()) {
        PCLOG("   -The client says this frame cannot be cached");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deniedByClientKey());
        rejectReasons |= 1 << ClientDeniesCaching;
    }

    for (Frame* child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        rejectReasons |= logCanCacheFrameDecision(*child, diagnosticLoggingClient, indentLevel + 1);
    
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

static void logCanCachePageDecision(Page& page)
{
    // Only bother logging for main frames that have actually loaded and have content.
    if (page.mainFrame().loader().stateMachine().creatingInitialEmptyDocument())
        return;
    URL currentURL = page.mainFrame().loader().documentLoader() ? page.mainFrame().loader().documentLoader()->url() : URL();
    if (currentURL.isEmpty())
        return;
    
    unsigned indentLevel = 0;
    PCLOG("--------\n Determining if page can be cached:");
    
    unsigned rejectReasons = 0;
    MainFrame& mainFrame = page.mainFrame();
    DiagnosticLoggingClient& diagnosticLoggingClient = mainFrame.diagnosticLoggingClient();
    unsigned frameRejectReasons = logCanCacheFrameDecision(mainFrame, diagnosticLoggingClient, indentLevel + 1);
    if (frameRejectReasons)
        rejectReasons |= 1 << FrameCannotBeInPageCache;
    
    if (!page.settings().usesPageCache()) {
        PCLOG("   -Page settings says b/f cache disabled");
        rejectReasons |= 1 << DisabledPageCache;
    }
#if ENABLE(DEVICE_ORIENTATION) && !PLATFORM(IOS)
    if (DeviceMotionController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceMotion");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deviceMotionKey());
        rejectReasons |= 1 << UsesDeviceMotion;
    }
    if (DeviceOrientationController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceOrientation");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deviceOrientationKey());
        rejectReasons |= 1 << UsesDeviceOrientation;
    }
#endif
#if ENABLE(PROXIMITY_EVENTS)
    if (DeviceProximityController::isActiveAt(page)) {
        PCLOG("   -Page is using DeviceProximity");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, deviceProximityKey);
        rejectReasons |= 1 << UsesDeviceMotion;
    }
#endif
    FrameLoadType loadType = page.mainFrame().loader().loadType();
    if (loadType == FrameLoadType::Reload) {
        PCLOG("   -Load type is: Reload");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadKey());
        rejectReasons |= 1 << IsReload;
    }
    if (loadType == FrameLoadType::ReloadFromOrigin) {
        PCLOG("   -Load type is: Reload from origin");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadFromOriginKey());
        rejectReasons |= 1 << IsReloadFromOrigin;
    }
    if (loadType == FrameLoadType::Same) {
        PCLOG("   -Load type is: Same");
        logPageCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::sameLoadKey());
        rejectReasons |= 1 << IsSameLoad;
    }
    
    if (rejectReasons)
        PCLOG(" Page CANNOT be cached\n--------");
    else
        PCLOG(" Page CAN be cached\n--------");

    diagnosticLoggingClient.logDiagnosticMessageWithResult(DiagnosticLoggingKeys::pageCacheKey(), emptyString(), rejectReasons ? DiagnosticLoggingResultFail : DiagnosticLoggingResultPass, ShouldSample::Yes);
}

PageCache& PageCache::singleton()
{
    static NeverDestroyed<PageCache> globalPageCache;
    return globalPageCache;
}
    
bool PageCache::canCachePageContainingThisFrame(Frame& frame)
{
    for (Frame* child = frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!canCachePageContainingThisFrame(*child))
            return false;
    }
    
    FrameLoader& frameLoader = frame.loader();

    // Prevent page caching if a subframe is still in provisional load stage.
    // We only do this check for subframes because the main frame is reused when navigating to a new page.
    if (!frame.isMainFrame() && frameLoader.state() == FrameStateProvisional)
        return false;

    DocumentLoader* documentLoader = frameLoader.documentLoader();
    Document* document = frame.document();
    
    return documentLoader
        && (documentLoader->mainDocumentError().isNull() || (documentLoader->mainDocumentError().isCancellation() && documentLoader->subresourceLoadersArePageCacheAcceptable()))
        // Do not cache error pages (these can be recognized as pages with substitute data or unreachable URLs).
        && !(documentLoader->substituteData().isValid() && !documentLoader->substituteData().failingURL().isEmpty())
        && (!frameLoader.subframeLoader().containsPlugins() || frame.page()->settings().pageCacheSupportsPlugins())
        && !(frame.isMainFrame() && document->url().protocolIs("https") && documentLoader->response().cacheControlContainsNoStore())
        && frameLoader.history().currentItem()
        && !frameLoader.quickRedirectComing()
        && !documentLoader->isLoading()
        && !documentLoader->isStopping()
        && document->canSuspendActiveDOMObjectsForPageCache()
        // FIXME: We should investigating caching frames that have an associated
        // application cache. <rdar://problem/5917899> tracks that work.
        && documentLoader->applicationCacheHost()->canCacheInPageCache()
        && frameLoader.client().canCachePage();
}
    
bool PageCache::canCache(Page* page) const
{
    if (!page)
        return false;
    
    logCanCachePageDecision(*page);

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return false;

    // Cache the page, if possible.
    // Don't write to the cache if in the middle of a redirect, since we will want to
    // store the final page we end up on.
    // No point writing to the cache on a reload or loadSame, since we will just write
    // over it again when we leave that page.
    FrameLoadType loadType = page->mainFrame().loader().loadType();
    
    return m_maxSize > 0
        && canCachePageContainingThisFrame(page->mainFrame())
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
    std::unique_ptr<CachedPage> cachedPage = WTF::move(item.m_cachedPage);

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
