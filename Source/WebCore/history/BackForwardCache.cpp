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
#include "BackForwardCache.h"

#include "ApplicationCacheHost.h"
#include "BackForwardController.h"
#include "CachedPage.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DiagnosticLoggingResultType.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "FocusController.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTTPParsers.h"
#include "HistoryController.h"
#include "IgnoreOpensDuringUnloadCountIncrementer.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "Page.h"
#include "Quirks.h"
#include "ScriptDisallowedScope.h"
#include "SecurityOriginHash.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include <pal/Logging.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

#define PCLOG(...) LOG(BackForwardCache, "%*s%s", indentLevel*4, "", makeString(__VA_ARGS__).utf8().data())

static inline void logBackForwardCacheFailureDiagnosticMessage(DiagnosticLoggingClient& client, const String& reason)
{
    client.logDiagnosticMessage(DiagnosticLoggingKeys::backForwardCacheFailureKey(), reason, ShouldSample::No);
}

static inline void logBackForwardCacheFailureDiagnosticMessage(Page* page, const String& reason)
{
    if (!page)
        return;

    logBackForwardCacheFailureDiagnosticMessage(page->diagnosticLoggingClient(), reason);
}

static bool canCacheFrame(LocalFrame& frame, DiagnosticLoggingClient& diagnosticLoggingClient, unsigned indentLevel)
{
    PCLOG("+---");
    CheckedRef frameLoader = frame.loader();

    // Prevent page caching if a subframe is still in provisional load stage.
    // We only do this check for subframes because the main frame is reused when navigating to a new page.
    if (!frame.isMainFrame() && frameLoader->state() == FrameState::Provisional) {
        PCLOG("   -Frame is in provisional load stage");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::provisionalLoadKey());
        return false;
    }

    if (frame.isMainFrame() && frameLoader->stateMachine().isDisplayingInitialEmptyDocument()) {
        PCLOG("   -MainFrame is displaying initial empty document");
        return false;
    }

    RefPtr document = frame.document();
    if (!document) {
        PCLOG("   -Frame has no document");
        return false;
    }

    if (document->shouldPreventEnteringBackForwardCacheForTesting()) {
        PCLOG("   -Back/Forward caching is disabled for testing");
        return false;
    }

    if (!document->frame()) {
        PCLOG("   -Document is detached from frame");
        ASSERT_NOT_REACHED();
        return false;
    }

    RefPtr documentLoader = frameLoader->documentLoader();
    if (!documentLoader) {
        PCLOG("   -There is no DocumentLoader object");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::noDocumentLoaderKey());
        return false;
    }

    URL currentURL = documentLoader->url();
    URL newURL = frameLoader->provisionalDocumentLoader() ? frameLoader->provisionalDocumentLoader()->url() : URL();
    if (!newURL.isEmpty())
        PCLOG(" Determining if frame can be cached navigating from (", currentURL.string(), ") to (", newURL.string(), "):");
    else
        PCLOG(" Determining if subframe with URL (", currentURL.string(), ") can be cached:");

    bool isCacheable = true;

    if (frame.isMainFrame() && document->quirks().shouldBypassBackForwardCache()) {
        PCLOG("   -Disabled by site-specific quirk");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::siteSpecificQuirkKey());
        isCacheable = false;
    }

    // Do not cache error pages (these can be recognized as pages with substitute data or unreachable URLs).
    if (documentLoader->substituteData().isValid() && !documentLoader->substituteData().failingURL().isEmpty()) {
        PCLOG("   -Frame is an error page");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isErrorPageKey());
        isCacheable = false;
    }
    if (frame.isMainFrame() && document && document->url().protocolIs("https"_s) && documentLoader->response().cacheControlContainsNoStore()) {
        PCLOG("   -Frame is HTTPS, and cache control prohibits storing");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::httpsNoStoreKey());
        isCacheable = false;
    }
    if (frame.isMainFrame() && !frameLoader->history().currentItem()) {
        PCLOG("   -Main frame has no current history item");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::noCurrentHistoryItemKey());
        isCacheable = false;
    }
    if (frame.isMainFrame() && frame.view() && !frame.view()->isVisuallyNonEmpty()) {
        PCLOG("   -Main frame is visually empty");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::visuallyEmptyKey());
        isCacheable = false;
    }
    if (frameLoader->quickRedirectComing()) {
        PCLOG("   -Quick redirect is coming");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::quirkRedirectComingKey());
        isCacheable = false;
    }
    if (documentLoader->isLoading()) {
        PCLOG("   -DocumentLoader is still loading");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isLoadingKey());
        isCacheable = false;
    }
    if (documentLoader->isStopping()) {
        PCLOG("   -DocumentLoader is in the middle of stopping");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::documentLoaderStoppingKey());
        isCacheable = false;
    }

    // FIXME: We should investigating caching frames that have an associated
    // application cache. <rdar://problem/5917899> tracks that work.
    if (!documentLoader->applicationCacheHost().canCacheInBackForwardCache()) {
        PCLOG("   -The DocumentLoader uses an application cache");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::applicationCacheKey());
        isCacheable = false;
    }
    if (!frameLoader->client().canCachePage()) {
        PCLOG("   -The client says this frame cannot be cached");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deniedByClientKey());
        isCacheable = false;
    }

    for (auto* child = frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        if (!canCacheFrame(*localChild, diagnosticLoggingClient, indentLevel + 1))
            isCacheable = false;
    }
    
    PCLOG(isCacheable ? " Frame CAN be cached" : " Frame CANNOT be cached");
    PCLOG("+---");
    
    return isCacheable;
}

static bool canCachePage(Page& page)
{
    RELEASE_ASSERT(!page.isRestoringCachedPage());

    unsigned indentLevel = 0;
    PCLOG("--------\n Determining if page can be cached:");

    CheckedRef diagnosticLoggingClient = page.diagnosticLoggingClient();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!localMainFrame)
        return false;
    bool isCacheable = canCacheFrame(*localMainFrame, diagnosticLoggingClient, indentLevel + 1);

    if (!page.settings().usesBackForwardCache() || page.isResourceCachingDisabledByWebInspector() || page.settings().siteIsolationEnabled()) {
        PCLOG("   -Page settings says b/f cache disabled");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::isDisabledKey());
        isCacheable = false;
    }
#if ENABLE(DEVICE_ORIENTATION) && !PLATFORM(IOS_FAMILY)
    if (DeviceMotionController::isActiveAt(&page)) {
        PCLOG("   -Page is using DeviceMotion");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deviceMotionKey());
        isCacheable = false;
    }
    if (DeviceOrientationController::isActiveAt(&page)) {
        PCLOG("   -Page is using DeviceOrientation");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::deviceOrientationKey());
        isCacheable = false;
    }
#endif

    FrameLoadType loadType = localMainFrame->loader().loadType();
    switch (loadType) {
    case FrameLoadType::Reload:
        // No point writing to the cache on a reload, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: Reload");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadKey());
        isCacheable = false;
        break;
    case FrameLoadType::Same: // user loads same URL again (but not reload button)
        // No point writing to the cache on a same load, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: Same");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::sameLoadKey());
        isCacheable = false;
        break;
    case FrameLoadType::RedirectWithLockedBackForwardList:
        // Don't write to the cache if in the middle of a redirect, since we will want to store the final page we end up on.
        PCLOG("   -Load type is: RedirectWithLockedBackForwardList");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::redirectKey());
        isCacheable = false;
        break;
    case FrameLoadType::Replace:
        // No point writing to the cache on a replace, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: Replace");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::replaceKey());
        isCacheable = false;
        break;
    case FrameLoadType::ReloadFromOrigin: {
        // No point writing to the cache on a reload, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: ReloadFromOrigin");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadFromOriginKey());
        isCacheable = false;
        break;
    }
    case FrameLoadType::ReloadExpiredOnly: {
        // No point writing to the cache on a reload, since we will just write over it again when we leave that page.
        PCLOG("   -Load type is: ReloadRevalidatingExpired");
        logBackForwardCacheFailureDiagnosticMessage(diagnosticLoggingClient, DiagnosticLoggingKeys::reloadRevalidatingExpiredKey());
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

    // If this is a same-origin navigation and the navigated-to main resource serves the
    // `Clear-Site-Data: "cache"` HTTP header, then we shouldn't cache the current page.
    if (RefPtr provisionalDocumentLoader = localMainFrame->loader().provisionalDocumentLoader()) {
        if (provisionalDocumentLoader->responseClearSiteDataValues().contains(ClearSiteDataValue::Cache)) {
            if (RefPtr topDocument = localMainFrame->document()) {
                if (topDocument->securityOrigin().isSameOriginAs(SecurityOrigin::create(provisionalDocumentLoader->response().url()))) {
                    PCLOG("   -`Clear-Site-Data: cache` HTTP header is present");
                    isCacheable = false;
                }
            }
        }
    }
    
    if (isCacheable)
        PCLOG(" Page CAN be cached\n--------");
    else
        PCLOG(" Page CANNOT be cached\n--------");

    diagnosticLoggingClient->logDiagnosticMessageWithResult(DiagnosticLoggingKeys::backForwardCacheKey(), DiagnosticLoggingKeys::canCacheKey(), isCacheable ? DiagnosticLoggingResultPass : DiagnosticLoggingResultFail, ShouldSample::No);
    return isCacheable;
}

BackForwardCache& BackForwardCache::singleton()
{
    static NeverDestroyed<BackForwardCache> globalBackForwardCache;
    return globalBackForwardCache;
}

BackForwardCache::BackForwardCache()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showBackForwardCache"_s, [] {
            BackForwardCache::singleton().dump();
        });
    });
}

void BackForwardCache::dump() const
{
    WTFLogAlways("Back/Forward Cache:");
    for (auto& item : m_items) {
        CachedPage& cachedPage = *item->m_cachedPage;
        WTFLogAlways("  Page %p, document %p %s", &cachedPage.page(), cachedPage.document(), cachedPage.document() ? cachedPage.document()->url().string().utf8().data() : "");
    }
}

bool BackForwardCache::canCache(Page& page) const
{
    if (!m_maxSize) {
        logBackForwardCacheFailureDiagnosticMessage(&page, DiagnosticLoggingKeys::isDisabledKey());
        return false;
    }

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        logBackForwardCacheFailureDiagnosticMessage(&page, DiagnosticLoggingKeys::underMemoryPressureKey());
        return false;
    }
    
    return canCachePage(page);
}

void BackForwardCache::pruneToSizeNow(unsigned size, PruningReason pruningReason)
{
    SetForScope change(m_maxSize, size);
    prune(pruningReason);
}

void BackForwardCache::setMaxSize(unsigned maxSize)
{
    m_maxSize = maxSize;
    prune(PruningReason::None);
}

unsigned BackForwardCache::frameCount() const
{
    unsigned frameCount = m_items.size();
    for (auto& item : m_items) {
        ASSERT(item->m_cachedPage);
        frameCount += item->m_cachedPage->cachedMainFrame()->descendantFrameCount();
    }
    
    return frameCount;
}

void BackForwardCache::markPagesForDeviceOrPageScaleChanged(Page& page)
{
    for (auto& item : m_items) {
        CheckedRef cachedPage = *item->m_cachedPage;
        auto* localMainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
        if (localMainFrame == &cachedPage->cachedMainFrame()->view()->frame())
            cachedPage->markForDeviceOrPageScaleChanged();
    }
}

void BackForwardCache::markPagesForContentsSizeChanged(Page& page)
{
    for (auto& item : m_items) {
        CheckedRef cachedPage = *item->m_cachedPage;
        auto* localMainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
        if (localMainFrame == &cachedPage->cachedMainFrame()->view()->frame())
            cachedPage->markForContentsSizeChanged();
    }
}

#if ENABLE(VIDEO)
void BackForwardCache::markPagesForCaptionPreferencesChanged()
{
    for (auto& item : m_items) {
        ASSERT(item->m_cachedPage);
        CheckedRef { *item->m_cachedPage }->markForCaptionPreferencesChanged();
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

static void setBackForwardCacheState(Page& page, Document::BackForwardCacheState BackForwardCacheState)
{
    page.forEachDocument([&] (Document& document) {
        document.setBackForwardCacheState(BackForwardCacheState);
    });
}

// When entering back/forward cache, tear down the render tree before setting the in-cache flag.
// This maintains the invariant that render trees are never present in the back/forward cache.
// Note that destruction happens bottom-up so that the main frame's tree dies last.
static void destroyRenderTree(LocalFrame& mainFrame)
{
    for (auto* abstractFrame = mainFrame.tree().traversePrevious(CanWrap::Yes); abstractFrame; abstractFrame = abstractFrame->tree().traversePrevious(CanWrap::No)) {
        auto* frame = dynamicDowncast<LocalFrame>(abstractFrame);
        if (!frame)
            continue;
        if (!frame->document())
            continue;
        Ref document = *frame->document();
        if (document->hasLivingRenderTree())
            document->destroyRenderTree();
    }
}

static void firePageHideEventRecursively(LocalFrame& frame)
{
    RefPtr document = frame.document();
    if (!document)
        return;

    // stopLoading() will fire the pagehide event in each subframe and the HTML specification states
    // that the parent document's ignore-opens-during-unload counter should be incremented while the
    // pagehide event is being fired in its subframes:
    // https://html.spec.whatwg.org/multipage/browsers.html#unload-a-document
    IgnoreOpensDuringUnloadCountIncrementer ignoreOpensDuringUnloadCountIncrementer(document.get());

    frame.loader().stopLoading(UnloadEventPolicy::UnloadAndPageHide);

    for (auto* child = frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localChild = dynamicDowncast<LocalFrame>(*child))
            firePageHideEventRecursively(*localChild);
    }
}

std::unique_ptr<CachedPage> BackForwardCache::trySuspendPage(Page& page, ForceSuspension forceSuspension)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!localMainFrame)
        return nullptr;

    localMainFrame->checkedLoader()->stopForBackForwardCache();

    if (forceSuspension == ForceSuspension::No && !canCache(page))
        return nullptr;

    ASSERT_WITH_MESSAGE(!page.isUtilityPage(), "Utility pages such as SVGImage pages should never go into BackForwardCache");

    setBackForwardCacheState(page, Document::AboutToEnterBackForwardCache);

    // Focus the main frame, defocusing a focused subframe (if we have one). We do this here,
    // before the page enters the back/forward cache, while we still can dispatch DOM blur/focus events.
    if (CheckedRef focusController = page.focusController(); focusController->focusedLocalFrame())
        focusController->setFocusedFrame(localMainFrame.get());

    // Fire the pagehide event in all frames.
    firePageHideEventRecursively(*localMainFrame);

    destroyRenderTree(*localMainFrame);

    // Stop all loads again before checking if we can still cache the page after firing the pagehide
    // event, since the page may have started ping loads in its pagehide event handler.
    localMainFrame->checkedLoader()->stopForBackForwardCache();

    // Check that the page is still page-cacheable after firing the pagehide event. The JS event handlers
    // could have altered the page in a way that could prevent caching.
    if (forceSuspension == ForceSuspension::No && !canCache(page)) {
        setBackForwardCacheState(page, Document::NotInBackForwardCache);
        return nullptr;
    }

    setBackForwardCacheState(page, Document::InBackForwardCache);

    // Make sure we don't fire any JS events in this scope.
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    return makeUnique<CachedPage>(page);
}

bool BackForwardCache::addIfCacheable(HistoryItem& item, Page* page)
{
    if (item.isInBackForwardCache())
        return false;

    if (!page)
        return false;

    auto cachedPage = trySuspendPage(*page, ForceSuspension::No);
    if (!cachedPage)
        return false;

    {
        // Make sure we don't fire any JS events in this scope.
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

        item.setCachedPage(WTFMove(cachedPage));
        item.m_pruningReason = PruningReason::None;
        m_items.add(&item);
    }
    prune(PruningReason::ReachedMaxSize);

    RELEASE_LOG(BackForwardCache, "BackForwardCache::addIfCacheable item: %s, size: %u / %u", item.identifier().toString().utf8().data(), pageCount(), maxSize());

    return true;
}

std::unique_ptr<CachedPage> BackForwardCache::suspendPage(Page& page)
{
    RELEASE_LOG(BackForwardCache, "BackForwardCache::suspendPage()");

    return trySuspendPage(page, ForceSuspension::Yes);
}

std::unique_ptr<CachedPage> BackForwardCache::take(HistoryItem& item, Page* page)
{
    if (!item.m_cachedPage) {
        if (item.m_pruningReason != PruningReason::None)
            logBackForwardCacheFailureDiagnosticMessage(page, pruningReasonToDiagnosticLoggingKey(item.m_pruningReason));
        return nullptr;
    }

    m_items.remove(&item);
    auto cachedPage = item.takeCachedPage();

    RELEASE_LOG(BackForwardCache, "BackForwardCache::take item: %s, size: %u / %u", item.identifier().toString().utf8().data(), pageCount(), maxSize());

    if (cachedPage->hasExpired() || (page && page->isResourceCachingDisabledByWebInspector())) {
        LOG(BackForwardCache, "Not restoring page for %s from back/forward cache because cache entry has expired", item.url().string().ascii().data());
        logBackForwardCacheFailureDiagnosticMessage(page, DiagnosticLoggingKeys::expiredKey());
        return nullptr;
    }

    return cachedPage;
}

void BackForwardCache::removeAllItemsForPage(Page& page)
{
#if ASSERT_ENABLED
    ASSERT_WITH_MESSAGE(!m_isInRemoveAllItemsForPage, "We should not reenter this method");
    SetForScope inRemoveAllItemsForPageScope { m_isInRemoveAllItemsForPage, true };
#endif

    for (auto it = m_items.begin(); it != m_items.end();) {
        // Increment iterator first so it stays valid after the removal.
        auto current = it;
        ++it;
        if (&(*current)->m_cachedPage->page() == &page) {
            RELEASE_LOG(BackForwardCache, "BackForwardCache::removeAllItemsForPage removing item: %s, size: %u / %u", (*current)->identifier().toString().utf8().data(), pageCount() - 1, maxSize());
            RefPtr { *current }->setCachedPage(nullptr);
            m_items.remove(current);
        }
    }
}

CachedPage* BackForwardCache::get(HistoryItem& item, Page* page)
{
    CheckedPtr cachedPage = item.m_cachedPage.get();
    if (!cachedPage) {
        if (item.m_pruningReason != PruningReason::None)
            logBackForwardCacheFailureDiagnosticMessage(page, pruningReasonToDiagnosticLoggingKey(item.m_pruningReason));
        return nullptr;
    }

    if (cachedPage->hasExpired() || (page && page->isResourceCachingDisabledByWebInspector())) {
        LOG(BackForwardCache, "Not restoring page for %s from back/forward cache because cache entry has expired", item.url().string().ascii().data());
        logBackForwardCacheFailureDiagnosticMessage(page, DiagnosticLoggingKeys::expiredKey());
        remove(item);
        return nullptr;
    }
    return cachedPage.get();
}

void BackForwardCache::remove(HistoryItem& item)
{
    // Safely ignore attempts to remove items not in the cache.
    if (!item.m_cachedPage)
        return;

    m_items.remove(&item);
    item.setCachedPage(nullptr);

    RELEASE_LOG(BackForwardCache, "BackForwardCache::remove item: %s, size: %u / %u", item.identifier().toString().utf8().data(), pageCount(), maxSize());

}

void BackForwardCache::prune(PruningReason pruningReason)
{
    while (pageCount() > maxSize()) {
        RefPtr oldestItem = m_items.takeFirst();
        oldestItem->setCachedPage(nullptr);
        oldestItem->m_pruningReason = pruningReason;
        RELEASE_LOG(BackForwardCache, "BackForwardCache::prune removing item: %s, size: %u / %u", oldestItem->identifier().toString().utf8().data(), pageCount(), maxSize());
    }
}

void BackForwardCache::clearEntriesForOrigins(const HashSet<RefPtr<SecurityOrigin>>& origins)
{
    for (auto it = m_items.begin(); it != m_items.end();) {
        // Increment iterator first so it stays valid after the removal.
        auto current = it;
        ++it;
        auto itemOrigin = SecurityOrigin::create((*current)->url());
        if (origins.contains(itemOrigin.ptr())) {
            RELEASE_LOG(BackForwardCache, "BackForwardCache::clearEntriesForOrigins removing item: %s, size: %u / %u", (*current)->identifier().toString().utf8().data(), pageCount() - 1, maxSize());
            RefPtr { *current }->setCachedPage(nullptr);
            m_items.remove(current);
        }
    }
}

} // namespace WebCore
