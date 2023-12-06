/*
 * Copyright (C) 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
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
#include "CachedPage.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Node.h"
#include "Page.h"
#include "PageTransitionEvent.h"
#include "ScriptDisallowedScope.h"
#include "SelectionRestorationMode.h"
#include "Settings.h"
#include "VisitedLinkState.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(IOS_FAMILY)
#include "FrameSelection.h"
#endif


namespace WebCore {
using namespace JSC;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, cachedPageCounter, ("CachedPage"));

CachedPage::CachedPage(Page& page)
    : m_page(page)
    , m_expirationTime(MonotonicTime::now() + page.settings().backForwardCacheExpirationInterval())
    , m_cachedMainFrame(makeUnique<CachedFrame>(page.mainFrame()))
    , m_loadedSubresourceDomains(is<LocalFrame>(page.mainFrame()) ? downcast<LocalFrame>(page.mainFrame()).loader().client().loadedSubresourceDomains() : Vector<RegistrableDomain>())
{
#ifndef NDEBUG
    cachedPageCounter.increment();
#endif
}

CachedPage::~CachedPage()
{
#ifndef NDEBUG
    cachedPageCounter.decrement();
#endif

    if (m_cachedMainFrame)
        m_cachedMainFrame->destroy();
}

static void firePageShowEvent(Page& page)
{
    // Dispatching JavaScript events can cause frame destruction.
    auto& mainFrame = page.mainFrame();

    Vector<Ref<LocalFrame>> childFrames;
    for (auto* child = mainFrame.tree().traverseNextInPostOrder(CanWrap::Yes); child; child = child->tree().traverseNextInPostOrder(CanWrap::No)) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        childFrames.append(*localChild);
    }

    for (auto& child : childFrames) {
        if (!child->tree().isDescendantOf(&mainFrame))
            continue;
        auto* document = child->document();
        if (!document)
            continue;

        // This takes care of firing the visibilitychange event and making sure the document is reported as visible.
        document->setVisibilityHiddenDueToDismissal(false);

        document->dispatchPageshowEvent(PageshowEventPersistence::Persisted);
    }
}

class CachedPageRestorationScope {
public:
    CachedPageRestorationScope(Page& page)
        : m_page(page)
    {
        m_page.setIsRestoringCachedPage(true);
    }

    ~CachedPageRestorationScope()
    {
        m_page.setIsRestoringCachedPage(false);
    }

private:
    Page& m_page;
};

void CachedPage::restore(Page& page)
{
    ASSERT(m_cachedMainFrame);
    ASSERT(m_cachedMainFrame->view());
    ASSERT(m_cachedMainFrame->view()->frame().isMainFrame());
    ASSERT(!page.subframeCount());

    auto& mainFrame = page.mainFrame();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(mainFrame);

    CachedPageRestorationScope restorationScope(page);
    m_cachedMainFrame->open();

    // Restore the focus appearance for the focused element.
    // FIXME: Right now we don't support pages w/ frames in the b/f cache.  This may need to be tweaked when we add support for that.
    RefPtr focusedDocument = CheckedRef(page.focusController())->focusedOrMainFrame().document();
    if (RefPtr element = focusedDocument->focusedElement()) {
#if PLATFORM(IOS_FAMILY)
        // We don't want focused nodes changing scroll position when restoring from the cache
        // as it can cause ugly jumps before we manage to restore the cached position.
        if (localMainFrame)
            localMainFrame->selection().suppressScrolling();

        bool hadProhibitsScrolling = false;
        auto* frameView = mainFrame.virtualView();
        if (frameView) {
            hadProhibitsScrolling = frameView->prohibitsScrolling();
            frameView->setProhibitsScrolling(true);
        }
#endif
        element->updateFocusAppearance(SelectionRestorationMode::RestoreOrSelectAll);
#if PLATFORM(IOS_FAMILY)
        if (frameView)
            frameView->setProhibitsScrolling(hadProhibitsScrolling);
        if (localMainFrame)
            localMainFrame->selection().restoreScrolling();
#endif
    }

    if (m_needsDeviceOrPageScaleChanged) {
        if (auto* localMainFrame = dynamicDowncast<LocalFrame>(mainFrame))
            localMainFrame->deviceOrPageScaleFactorChanged();
    }

    page.setNeedsRecalcStyleInAllFrames();

#if ENABLE(VIDEO)
    if (m_needsCaptionPreferencesChanged)
        page.captionPreferencesChanged();
#endif

    if (m_needsUpdateContentsSize) {
        if (auto* frameView = mainFrame.virtualView())
            frameView->updateContentsSize();
    }

    firePageShowEvent(page);

    for (auto& domain : m_loadedSubresourceDomains) {
        if (auto* localMainFrame = dynamicDowncast<LocalFrame>(mainFrame))
            localMainFrame->loader().client().didLoadFromRegistrableDomain(WTFMove(domain));
    }

    clear();
}

void CachedPage::clear()
{
    ASSERT(m_cachedMainFrame);
    m_cachedMainFrame->clear();
    m_cachedMainFrame = nullptr;
#if ENABLE(VIDEO)
    m_needsCaptionPreferencesChanged = false;
#endif
    m_needsDeviceOrPageScaleChanged = false;
    m_needsUpdateContentsSize = false;
    m_loadedSubresourceDomains.clear();
}

bool CachedPage::hasExpired() const
{
    return MonotonicTime::now() > m_expirationTime;
}

RefPtr<DocumentLoader> CachedPage::protectedDocumentLoader() const
{
    return documentLoader();
}

} // namespace WebCore
