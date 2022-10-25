/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HistoryController.h"

#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "CachedPage.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderStateMachine.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLObjectElement.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "Page.h"
#include "ScrollingCoordinator.h"
#include "SerializedScriptValue.h"
#include "SharedStringHash.h"
#include "ShouldTreatAsContinuingLoad.h"
#include "VisitedLinkStore.h"
#include <wtf/text/CString.h>

namespace WebCore {

static inline void addVisitedLink(Page& page, const URL& url)
{
    page.visitedLinkStore().addVisitedLink(page, computeSharedStringHash(url.string()));
}

FrameLoader::HistoryController::HistoryController(Frame& frame)
    : m_frame(frame)
    , m_frameLoadComplete(true)
    , m_defersLoading(false)
{
}

FrameLoader::HistoryController::~HistoryController() = default;

void FrameLoader::HistoryController::saveScrollPositionAndViewStateToItem(HistoryItem* item)
{
    FrameView* frameView = m_frame.view();
    if (!item || !frameView)
        return;

    if (m_frame.document()->backForwardCacheState() != Document::NotInBackForwardCache)
        item->setScrollPosition(frameView->cachedScrollPosition());
    else
        item->setScrollPosition(frameView->scrollPosition());

#if PLATFORM(IOS_FAMILY)
    item->setExposedContentRect(frameView->exposedContentRect());
    item->setUnobscuredContentRect(frameView->unobscuredContentRect());
#endif

    Page* page = m_frame.page();
    if (page && m_frame.isMainFrame()) {
        item->setPageScaleFactor(page->pageScaleFactor() / page->viewScaleFactor());
#if PLATFORM(IOS_FAMILY)
        item->setObscuredInsets(page->obscuredInsets());
#endif
    }

    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling through to the client.
    m_frame.loader().client().saveViewStateToItem(*item);

    // Notify clients that the HistoryItem has changed.
    item->notifyChanged();
}

void FrameLoader::HistoryController::clearScrollPositionAndViewState()
{
    if (!m_currentItem)
        return;

    m_currentItem->clearScrollPosition();
    m_currentItem->setPageScaleFactor(0);
}

/*
 There is a race condition between the layout and load completion that affects restoring the scroll position.
 We try to restore the scroll position at both the first layout and upon load completion.
 
 1) If first layout happens before the load completes, we want to restore the scroll position then so that the
 first time we draw the page is already scrolled to the right place, instead of starting at the top and later
 jumping down.  It is possible that the old scroll position is past the part of the doc laid out so far, in
 which case the restore silent fails and we will fix it in when we try to restore on doc completion.
 2) If the layout happens after the load completes, the attempt to restore at load completion time silently
 fails.  We then successfully restore it when the layout happens.
*/
void FrameLoader::HistoryController::restoreScrollPositionAndViewState()
{
    if (!m_frame.loader().stateMachine().committedFirstRealDocumentLoad())
        return;

    ASSERT(m_currentItem);
    
    // FIXME: As the ASSERT attests, it seems we should always have a currentItem here.
    // One counterexample is <rdar://problem/4917290>
    // For now, to cover this issue in release builds, there is no technical harm to returning
    // early and from a user standpoint - as in the above radar - the previous page load failed 
    // so there *is* no scroll or view state to restore!
    if (!m_currentItem)
        return;

    RefPtr view = m_frame.view();

    // FIXME: There is some scrolling related work that needs to happen whenever a page goes into the
    // back/forward cache and similar work that needs to occur when it comes out. This is where we do the work
    // that needs to happen when we exit, and the work that needs to happen when we enter is in
    // Document::setIsInBackForwardCache(bool). It would be nice if there was more symmetry in these spots.
    // https://bugs.webkit.org/show_bug.cgi?id=98698
    if (view) {
        Page* page = m_frame.page();
        if (page && m_frame.isMainFrame()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(*view);
        }
    }

    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling
    // through to the client.
    m_frame.loader().client().restoreViewState();

#if !PLATFORM(IOS_FAMILY)
    // Don't restore scroll point on iOS as FrameLoaderClient::restoreViewState() does that.
    if (view && !view->wasScrolledByUser()) {
        view->scrollToFocusedElementImmediatelyIfNeeded();

        Page* page = m_frame.page();
        auto desiredScrollPosition = m_currentItem->shouldRestoreScrollPosition() ? m_currentItem->scrollPosition() : view->scrollPosition();
        LOG(Scrolling, "HistoryController::restoreScrollPositionAndViewState scrolling to %d,%d", desiredScrollPosition.x(), desiredScrollPosition.y());
        if (page && m_frame.isMainFrame() && m_currentItem->pageScaleFactor())
            page->setPageScaleFactor(m_currentItem->pageScaleFactor() * page->viewScaleFactor(), desiredScrollPosition);
        else
            view->setScrollPosition(desiredScrollPosition);

        // If the scroll position doesn't have to be clamped, consider it successfully restored.
        if (m_frame.isMainFrame()) {
            auto adjustedDesiredScrollPosition = view->adjustScrollPositionWithinRange(desiredScrollPosition);
            if (desiredScrollPosition == adjustedDesiredScrollPosition)
                m_frame.loader().client().didRestoreScrollPosition();
        }

    }
#endif
}

void FrameLoader::HistoryController::updateBackForwardListForFragmentScroll()
{
    updateBackForwardListClippedAtTarget(false);
}

void FrameLoader::HistoryController::saveDocumentState()
{
    // FIXME: Reading this bit of FrameLoader state here is unfortunate.  I need to study
    // this more to see if we can remove this dependency.
    if (m_frame.loader().stateMachine().creatingInitialEmptyDocument())
        return;

    // For a standard page load, we will have a previous item set, which will be used to
    // store the form state.  However, in some cases we will have no previous item, and
    // the current item is the right place to save the state.  One example is when we
    // detach a bunch of frames because we are navigating from a site with frames to
    // another site.  Another is when saving the frame state of a frame that is not the
    // target of the current navigation (if we even decide to save with that granularity).

    // Because of previousItem's "masking" of currentItem for this purpose, it's important
    // that we keep track of the end of a page transition with m_frameLoadComplete.  We
    // leverage the checkLoadComplete recursion to achieve this goal.

    HistoryItem* item = m_frameLoadComplete ? m_currentItem.get() : m_previousItem.get();
    if (!item)
        return;

    ASSERT(m_frame.document());
    Document& document = *m_frame.document();
    if (item->isCurrentDocument(document) && document.hasLivingRenderTree()) {
        if (DocumentLoader* documentLoader = document.loader())
            item->setShouldOpenExternalURLsPolicy(documentLoader->shouldOpenExternalURLsPolicyToPropagate());

        LOG(Loading, "WebCoreLoading frame %" PRIu64 ": saving form state to %p", m_frame.frameID().object().toUInt64(), item);
        item->setDocumentState(document.formElementsState());
    }
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
void FrameLoader::HistoryController::saveDocumentAndScrollState()
{
    for (AbstractFrame* frame = &m_frame; frame; frame = frame->tree().traverseNext(&m_frame)) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        localFrame->loader().history().saveDocumentState();
        localFrame->loader().history().saveScrollPositionAndViewStateToItem(localFrame->loader().history().currentItem());
    }
}

void FrameLoader::HistoryController::restoreDocumentState()
{
    switch (m_frame.loader().loadType()) {
    case FrameLoadType::Reload:
    case FrameLoadType::ReloadFromOrigin:
    case FrameLoadType::ReloadExpiredOnly:
    case FrameLoadType::Same:
    case FrameLoadType::Replace:
        // Not restoring the document state.
        return;
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward:
    case FrameLoadType::RedirectWithLockedBackForwardList:
    case FrameLoadType::Standard:
        break;
    }
    
    if (!m_currentItem)
        return;
    if (m_frame.loader().requestedHistoryItem() != m_currentItem.get())
        return;
    if (m_frame.loader().documentLoader()->isClientRedirect())
        return;

    m_frame.loader().documentLoader()->setShouldOpenExternalURLsPolicy(m_currentItem->shouldOpenExternalURLsPolicy());

    LOG(Loading, "WebCoreLoading frame %" PRIu64 ": restoring form state from %p", m_frame.frameID().object().toUInt64(), m_currentItem.get());
    m_frame.document()->setStateForNewFormElements(m_currentItem->documentState());
}

void FrameLoader::HistoryController::invalidateCurrentItemCachedPage()
{
    if (!currentItem())
        return;

    // When we are pre-commit, the currentItem is where any back/forward cache data resides.
    std::unique_ptr<CachedPage> cachedPage = BackForwardCache::singleton().take(*currentItem(), m_frame.page());
    if (!cachedPage)
        return;

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the PageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    
    ASSERT(cachedPage->document() == m_frame.document());
    if (cachedPage->document() == m_frame.document()) {
        cachedPage->document()->setBackForwardCacheState(Document::NotInBackForwardCache);
        cachedPage->clear();
    }
}

bool FrameLoader::HistoryController::shouldStopLoadingForHistoryItem(HistoryItem& targetItem) const
{
    if (!m_currentItem)
        return false;

    // Don't abort the current load if we're navigating within the current document.
    if (m_currentItem->shouldDoSameDocumentNavigationTo(targetItem))
        return false;

    return true;
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
void FrameLoader::HistoryController::goToItem(HistoryItem& targetItem, FrameLoadType type, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    LOG(History, "HistoryController %p goToItem %p type=%d", this, &targetItem, static_cast<int>(type));

    ASSERT(!m_frame.tree().parent());
    
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    Page* page = m_frame.page();
    if (!page)
        return;
    if (!m_frame.loader().client().shouldGoToHistoryItem(targetItem))
        return;
    if (m_defersLoading) {
        m_deferredItem = &targetItem;
        m_deferredFrameLoadType = type;
        return;
    }

    // Set the BF cursor before commit, which lets the user quickly click back/forward again.
    // - plus, it only makes sense for the top level of the operation through the frame tree,
    // as opposed to happening for some/one of the page commits that might happen soon
    RefPtr<HistoryItem> currentItem = page->backForward().currentItem();
    page->backForward().setCurrentItem(targetItem);

    // First set the provisional item of any frames that are not actually navigating.
    // This must be done before trying to navigate the desired frame, because some
    // navigations can commit immediately (such as about:blank).  We must be sure that
    // all frames have provisional items set before the commit.
    recursiveSetProvisionalItem(targetItem, currentItem.get());

    // Now that all other frames have provisional items, do the actual navigation.
    recursiveGoToItem(targetItem, currentItem.get(), type, shouldTreatAsContinuingLoad);
}

void FrameLoader::HistoryController::setDefersLoading(bool defer)
{
    m_defersLoading = defer;
    if (!defer && m_deferredItem) {
        goToItem(*m_deferredItem, m_deferredFrameLoadType, ShouldTreatAsContinuingLoad::No);
        m_deferredItem = nullptr;
    }
}

void FrameLoader::HistoryController::updateForBackForwardNavigation()
{
    LOG(History, "HistoryController %p updateForBackForwardNavigation: Updating History for back/forward navigation in frame %p (main frame %d) %s", this, &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader() ? m_frame.loader().documentLoader()->url().string().utf8().data() : "");

    // Must grab the current scroll position before disturbing it
    if (!m_frameLoadComplete)
        saveScrollPositionAndViewStateToItem(m_previousItem.get());

    // When traversing history, we may end up redirecting to a different URL
    // this time (e.g., due to cookies).  See http://webkit.org/b/49654.
    updateCurrentItem();
}

void FrameLoader::HistoryController::updateForReload()
{
    LOG(History, "HistoryController %p updateForReload: Updating History for reload in frame %p (main frame %d) %s", this, &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader() ? m_frame.loader().documentLoader()->url().string().utf8().data() : "");

    if (m_currentItem) {
        BackForwardCache::singleton().remove(*m_currentItem);
    
        if (m_frame.loader().loadType() == FrameLoadType::Reload || m_frame.loader().loadType() == FrameLoadType::ReloadFromOrigin)
            saveScrollPositionAndViewStateToItem(m_currentItem.get());

        // Rebuild the history item tree when reloading as trying to re-associate everything is too error-prone.
        m_currentItem->clearChildren();
    }

    // When reloading the page, we may end up redirecting to a different URL
    // this time (e.g., due to cookies).  See http://webkit.org/b/4072.
    updateCurrentItem();
}

// There are 3 things you might think of as "history", all of which are handled by these functions.
//
//     1) Back/forward: The m_currentItem is part of this mechanism.
//     2) Global history: Handled by the client.
//     3) Visited links: Handled by the PageGroup.

void FrameLoader::HistoryController::updateForStandardLoad(HistoryUpdateType updateType)
{
    LOG(History, "HistoryController %p updateForStandardLoad: Updating History for standard load in frame %p (main frame %d) %s", this, &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader()->url().string().ascii().data());

    FrameLoader& frameLoader = m_frame.loader();

    bool usesEphemeralSession = m_frame.page() ? m_frame.page()->usesEphemeralSession() : true;
    const URL& historyURL = frameLoader.documentLoader()->urlForHistory();

    if (!frameLoader.documentLoader()->isClientRedirect()) {
        if (!historyURL.isEmpty()) {
            if (updateType != UpdateAllExceptBackForwardList)
                updateBackForwardListClippedAtTarget(true);
            if (!usesEphemeralSession) {
                frameLoader.client().updateGlobalHistory();
                frameLoader.documentLoader()->setDidCreateGlobalHistoryEntry(true);
                if (frameLoader.documentLoader()->unreachableURL().isEmpty())
                    frameLoader.client().updateGlobalHistoryRedirectLinks();
            }
        }
    } else {
        // The client redirect replaces the current history item.
        updateCurrentItem();
    }

    if (!historyURL.isEmpty() && !usesEphemeralSession) {
        if (Page* page = m_frame.page())
            addVisitedLink(*page, historyURL);

        if (!frameLoader.documentLoader()->didCreateGlobalHistoryEntry() && frameLoader.documentLoader()->unreachableURL().isEmpty() && !m_frame.document()->url().isEmpty())
            frameLoader.client().updateGlobalHistoryRedirectLinks();
    }
}

void FrameLoader::HistoryController::updateForRedirectWithLockedBackForwardList()
{
    LOG(History, "HistoryController %p updateForRedirectWithLockedBackForwardList: Updating History for redirect load in frame %p (main frame %d) %s", this, &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader() ? m_frame.loader().documentLoader()->url().string().utf8().data() : "");
    
    bool usesEphemeralSession = m_frame.page() ? m_frame.page()->usesEphemeralSession() : true;
    auto historyURL = m_frame.loader().documentLoader() ? m_frame.loader().documentLoader()->urlForHistory() : URL { };

    if (m_frame.loader().documentLoader() && m_frame.loader().documentLoader()->isClientRedirect()) {
        if (!m_currentItem && !m_frame.tree().parent()) {
            if (!historyURL.isEmpty()) {
                updateBackForwardListClippedAtTarget(true);
                if (!usesEphemeralSession) {
                    m_frame.loader().client().updateGlobalHistory();
                    m_frame.loader().documentLoader()->setDidCreateGlobalHistoryEntry(true);
                    if (m_frame.loader().documentLoader()->unreachableURL().isEmpty())
                        m_frame.loader().client().updateGlobalHistoryRedirectLinks();
                }
            }
        }
        // The client redirect replaces the current history item.
        updateCurrentItem();
    } else {
        auto* parentFrame = dynamicDowncast<LocalFrame>(m_frame.tree().parent());
        if (parentFrame && parentFrame->loader().history().currentItem())
            parentFrame->loader().history().currentItem()->setChildItem(createItem());
    }

    if (!historyURL.isEmpty() && !usesEphemeralSession) {
        if (Page* page = m_frame.page())
            addVisitedLink(*page, historyURL);

        if (!m_frame.loader().documentLoader()->didCreateGlobalHistoryEntry() && m_frame.loader().documentLoader()->unreachableURL().isEmpty())
            m_frame.loader().client().updateGlobalHistoryRedirectLinks();
    }
}

void FrameLoader::HistoryController::updateForClientRedirect()
{
    LOG(History, "HistoryController %p updateForClientRedirect: Updating History for client redirect in frame %p (main frame %d) %s", this, &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader() ? m_frame.loader().documentLoader()->url().string().utf8().data() : "");

    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // webcore has closed the URL and saved away the form state.
    if (m_currentItem) {
        m_currentItem->clearDocumentState();
        m_currentItem->clearScrollPosition();
    }

    bool usesEphemeralSession = m_frame.page() ? m_frame.page()->usesEphemeralSession() : true;
    const URL& historyURL = m_frame.loader().documentLoader()->urlForHistory();

    if (!historyURL.isEmpty() && !usesEphemeralSession) {
        if (Page* page = m_frame.page())
            addVisitedLink(*page, historyURL);
    }
}

void FrameLoader::HistoryController::updateForCommit()
{
    FrameLoader& frameLoader = m_frame.loader();
    LOG(History, "HistoryController %p updateForCommit: Updating History for commit in frame %p (main frame %d) %s", this, &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader() ? m_frame.loader().documentLoader()->url().string().utf8().data() : "");

    FrameLoadType type = frameLoader.loadType();
    if (isBackForwardLoadType(type)
        || isReplaceLoadTypeWithProvisionalItem(type)
        || (isReloadTypeWithProvisionalItem(type) && !frameLoader.provisionalDocumentLoader()->unreachableURL().isEmpty())) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=146842
        // We should always have a provisional item when committing, but we sometimes don't.
        // Not having one leads to us not having a m_currentItem later, which is also a terrible known issue.
        // We should get to the bottom of this.
        ASSERT(m_provisionalItem);
        if (m_provisionalItem)
            setCurrentItem(*m_provisionalItem.get());
        m_provisionalItem = nullptr;

        // Tell all other frames in the tree to commit their provisional items and
        // restore their scroll position.  We'll avoid this frame (which has already
        // committed) and its children (which will be replaced).
        m_frame.mainFrame().loader().history().recursiveUpdateForCommit();
    }
}

bool FrameLoader::HistoryController::isReplaceLoadTypeWithProvisionalItem(FrameLoadType type)
{
    // Going back to an error page in a subframe can trigger a FrameLoadType::Replace
    // while m_provisionalItem is set, so we need to commit it.
    return type == FrameLoadType::Replace && m_provisionalItem;
}

bool FrameLoader::HistoryController::isReloadTypeWithProvisionalItem(FrameLoadType type)
{
    return (type == FrameLoadType::Reload || type == FrameLoadType::ReloadFromOrigin) && m_provisionalItem;
}

void FrameLoader::HistoryController::recursiveUpdateForCommit()
{
    // The frame that navigated will now have a null provisional item.
    // Ignore it and its children.
    if (!m_provisionalItem)
        return;

    // For each frame that already had the content the item requested (based on
    // (a matching URL and frame tree snapshot), just restore the scroll position.
    // Save form state (works from currentItem, since m_frameLoadComplete is true)
    if (m_currentItem && itemsAreClones(*m_currentItem, m_provisionalItem.get())) {
        ASSERT(m_frameLoadComplete);
        saveDocumentState();
        saveScrollPositionAndViewStateToItem(m_currentItem.get());

        if (FrameView* view = m_frame.view())
            view->setWasScrolledByUser(false);

        // Now commit the provisional item
        if (m_provisionalItem)
            setCurrentItem(*m_provisionalItem.get());
        m_provisionalItem = nullptr;

        // Restore form state (works from currentItem)
        restoreDocumentState();

        // Restore the scroll position (we choose to do this rather than going back to the anchor point)
        restoreScrollPositionAndViewState();
    }

    // Iterate over the rest of the tree
    for (auto* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        localChild->loader().history().recursiveUpdateForCommit();
    }
}

void FrameLoader::HistoryController::updateForSameDocumentNavigation()
{
    if (m_frame.document()->url().isEmpty())
        return;

    Page* page = m_frame.page();
    if (!page)
        return;

    bool usesEphemeralSession = page->usesEphemeralSession();
    if (!usesEphemeralSession)
        addVisitedLink(*page, m_frame.document()->url());

    m_frame.mainFrame().loader().history().recursiveUpdateForSameDocumentNavigation();

    if (m_currentItem) {
        m_currentItem->setURL(m_frame.document()->url());
        if (!usesEphemeralSession)
            m_frame.loader().client().updateGlobalHistory();
    }
}

void FrameLoader::HistoryController::recursiveUpdateForSameDocumentNavigation()
{
    // The frame that navigated will now have a null provisional item.
    // Ignore it and its children.
    if (!m_provisionalItem)
        return;

    // The provisional item may represent a different pending navigation.
    // Don't commit it if it isn't a same document navigation.
    if (m_currentItem && !m_currentItem->shouldDoSameDocumentNavigationTo(*m_provisionalItem))
        return;

    // Commit the provisional item.
    if (m_provisionalItem)
        setCurrentItem(*m_provisionalItem.get());
    m_provisionalItem = nullptr;

    // Iterate over the rest of the tree.
    for (auto* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        localChild->loader().history().recursiveUpdateForSameDocumentNavigation();
    }
}

void FrameLoader::HistoryController::updateForFrameLoadCompleted()
{
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to track that
    // the load is complete so that we use the current item instead.
    m_frameLoadComplete = true;
}

void FrameLoader::HistoryController::setCurrentItem(HistoryItem& item)
{
    m_frameLoadComplete = false;
    m_previousItem = m_currentItem;
    m_currentItem = &item;
}

void FrameLoader::HistoryController::setCurrentItemTitle(const StringWithDirection& title)
{
    // FIXME: This ignores the title's direction.
    if (m_currentItem)
        m_currentItem->setTitle(title.string);
}

bool FrameLoader::HistoryController::currentItemShouldBeReplaced() const
{
    // From the HTML5 spec for location.assign():
    //  "If the browsing context's session history contains only one Document,
    //   and that was the about:blank Document created when the browsing context
    //   was created, then the navigation must be done with replacement enabled."
    return m_currentItem && !m_previousItem && equalIgnoringASCIICase(m_currentItem->urlString(), aboutBlankURL().string());
}

void FrameLoader::HistoryController::clearPreviousItem()
{
    m_previousItem = nullptr;
    for (auto* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        localChild->loader().history().clearPreviousItem();
    }
}

void FrameLoader::HistoryController::setProvisionalItem(HistoryItem* item)
{
    m_provisionalItem = item;
}

void FrameLoader::HistoryController::initializeItem(HistoryItem& item)
{
    DocumentLoader* documentLoader = m_frame.loader().documentLoader();
    ASSERT(documentLoader);

    URL unreachableURL = documentLoader->unreachableURL();

    URL url;
    URL originalURL;

    if (!unreachableURL.isEmpty()) {
        url = unreachableURL;
        originalURL = unreachableURL;
    } else {
        url = documentLoader->url();
        originalURL = documentLoader->originalURL();
    }

    // Frames that have never successfully loaded any content
    // may have no URL at all. Currently our history code can't
    // deal with such things, so we nip that in the bud here.
    // Later we may want to learn to live with nil for URL.
    // See bug 3368236 and related bugs for more information.
    if (url.isEmpty()) 
        url = aboutBlankURL();
    if (originalURL.isEmpty())
        originalURL = aboutBlankURL();
    
    StringWithDirection title = documentLoader->title();

    item.setURL(url);
    item.setTarget(m_frame.tree().uniqueName());
    // FIXME: Should store the title direction as well.
    item.setTitle(title.string);
    item.setOriginalURLString(originalURL.string());

    if (!unreachableURL.isEmpty() || documentLoader->response().httpStatusCode() >= 400)
        item.setLastVisitWasFailure(true);

    item.setShouldOpenExternalURLsPolicy(documentLoader->shouldOpenExternalURLsPolicyToPropagate());

    // Save form state if this is a POST
    item.setFormInfoFromRequest(documentLoader->request());
}

Ref<HistoryItem> FrameLoader::HistoryController::createItem()
{
    Ref<HistoryItem> item = HistoryItem::create();
    initializeItem(item);
    
    // Set the item for which we will save document state
    setCurrentItem(item);
    
    return item;
}

Ref<HistoryItem> FrameLoader::HistoryController::createItemTree(Frame& targetFrame, bool clipAtTarget)
{
    Ref<HistoryItem> bfItem = createItem();
    if (!m_frameLoadComplete)
        saveScrollPositionAndViewStateToItem(m_previousItem.get());

    if (!clipAtTarget || &m_frame != &targetFrame) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        saveDocumentState();

        // clipAtTarget is false for navigations within the same document, so
        // we should copy the documentSequenceNumber over to the newly create
        // item.  Non-target items are just clones, and they should therefore
        // preserve the same itemSequenceNumber.
        if (m_previousItem) {
            if (&m_frame != &targetFrame)
                bfItem->setItemSequenceNumber(m_previousItem->itemSequenceNumber());
            bfItem->setDocumentSequenceNumber(m_previousItem->documentSequenceNumber());
        }

        for (auto* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
            auto* localChild = dynamicDowncast<LocalFrame>(child);
            if (!localChild)
                continue;
            FrameLoader& childLoader = localChild->loader();
            bool hasChildLoaded = childLoader.frameHasLoaded();

            // If the child is a frame corresponding to an <object> element that never loaded,
            // we don't want to create a history item, because that causes fallback content
            // to be ignored on reload.
            
            if (!(!hasChildLoaded && localChild->ownerElement() && is<HTMLObjectElement>(localChild->ownerElement())))
                bfItem->addChildItem(childLoader.history().createItemTree(targetFrame, clipAtTarget));
        }
    }
    // FIXME: Eliminate the isTargetItem flag in favor of itemSequenceNumber.
    if (&m_frame == &targetFrame)
        bfItem->setIsTargetItem(true);
    return bfItem;
}

// The general idea here is to traverse the frame tree and the item tree in parallel,
// tracking whether each frame already has the content the item requests.  If there is
// a match, we set the provisional item and recurse.  Otherwise we will reload that
// frame and all its kids in recursiveGoToItem.
void FrameLoader::HistoryController::recursiveSetProvisionalItem(HistoryItem& item, HistoryItem* fromItem)
{
    if (!itemsAreClones(item, fromItem))
        return;

    // Set provisional item, which will be committed in recursiveUpdateForCommit.
    m_provisionalItem = &item;

    for (auto& childItem : item.children()) {
        auto& childFrameName = childItem->target();

        auto* fromChildItem = fromItem->childItemWithTarget(childFrameName);
        if (!fromChildItem)
            continue;

        if (auto* childFrame = dynamicDowncast<LocalFrame>(m_frame.tree().child(childFrameName)))
            childFrame->loader().history().recursiveSetProvisionalItem(childItem, fromChildItem);
    }
}

// We now traverse the frame tree and item tree a second time, loading frames that
// do have the content the item requests.
void FrameLoader::HistoryController::recursiveGoToItem(HistoryItem& item, HistoryItem* fromItem, FrameLoadType type, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    if (!itemsAreClones(item, fromItem)) {
        m_frame.loader().loadItem(item, fromItem, type, shouldTreatAsContinuingLoad);
        return;
    }

    // Just iterate over the rest, looking for frames to navigate.
    for (auto& childItem : item.children()) {
        auto& childFrameName = childItem->target();

        auto* fromChildItem = fromItem->childItemWithTarget(childFrameName);
        if (!fromChildItem)
            continue;

        if (auto* childFrame = dynamicDowncast<LocalFrame>(m_frame.tree().child(childFrameName)))
            childFrame->loader().history().recursiveGoToItem(childItem, fromChildItem, type, shouldTreatAsContinuingLoad);
    }
}

// The following logic must be kept in sync with WebKit::WebBackForwardListItem::itemIsClone().
bool FrameLoader::HistoryController::itemsAreClones(HistoryItem& item1, HistoryItem* item2) const
{
    // If the item we're going to is a clone of the item we're at, then we do
    // not need to load it again.  The current frame tree and the frame tree
    // snapshot in the item have to match.
    // Note: Some clients treat a navigation to the current history item as
    // a reload.  Thus, if item1 and item2 are the same, we need to create a
    // new document and should not consider them clones.
    // (See http://webkit.org/b/35532 for details.)
    return item2
        && &item1 != item2
        && item1.itemSequenceNumber() == item2->itemSequenceNumber();
}

// Helper method that determines whether the current frame tree matches given history item's.
bool FrameLoader::HistoryController::currentFramesMatchItem(HistoryItem& item) const
{
    if ((!m_frame.tree().uniqueName().isEmpty() || !item.target().isEmpty()) && m_frame.tree().uniqueName() != item.target())
        return false;

    const auto& childItems = item.children();
    if (childItems.size() != m_frame.tree().childCount())
        return false;
    
    for (auto& item : childItems) {
        if (!m_frame.tree().child(item->target()))
            return false;
    }
    
    return true;
}

void FrameLoader::HistoryController::updateBackForwardListClippedAtTarget(bool doClip)
{
    // In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  
    // The item that was the target of the user's navigation is designated as the "targetItem".  
    // When this function is called with doClip=true we're able to create the whole tree except for the target's children, 
    // which will be loaded in the future. That part of the tree will be filled out as the child loads are committed.

    Page* page = m_frame.page();
    if (!page)
        return;

    if (m_frame.loader().documentLoader()->urlForHistory().isEmpty())
        return;

    FrameLoader& frameLoader = m_frame.mainFrame().loader();

    Ref<HistoryItem> topItem = frameLoader.history().createItemTree(m_frame, doClip);
    LOG(History, "HistoryController %p updateBackForwardListClippedAtTarget: Adding backforward item %p in frame %p (main frame %d) %s", this, topItem.ptr(), &m_frame, m_frame.isMainFrame(), m_frame.loader().documentLoader()->url().string().utf8().data());

    page->backForward().addItem(WTFMove(topItem));
}

void FrameLoader::HistoryController::updateCurrentItem()
{
    if (!m_currentItem)
        return;

    DocumentLoader* documentLoader = m_frame.loader().documentLoader();

    if (!documentLoader->unreachableURL().isEmpty())
        return;

    if (m_currentItem->url() != documentLoader->url()) {
        // We ended up on a completely different URL this time, so the HistoryItem
        // needs to be re-initialized.  Preserve the isTargetItem flag as it is a
        // property of how this HistoryItem was originally created and is not
        // dependent on the document.
        bool isTargetItem = m_currentItem->isTargetItem();
        m_currentItem->reset();
        initializeItem(*m_currentItem);
        m_currentItem->setIsTargetItem(isTargetItem);
    } else {
        // Even if the final URL didn't change, the form data may have changed.
        m_currentItem->setFormInfoFromRequest(documentLoader->request());
    }
}

void FrameLoader::HistoryController::pushState(RefPtr<SerializedScriptValue>&& stateObject, const String& title, const String& urlString)
{
    if (!m_currentItem)
        return;

    Page* page = m_frame.page();
    ASSERT(page);

    bool shouldRestoreScrollPosition = m_currentItem->shouldRestoreScrollPosition();

    auto* document = m_frame.document();
    if (document && !document->hasRecentUserInteractionForNavigationFromJS())
        m_currentItem->setWasCreatedByJSWithoutUserInteraction(true);

    // Get a HistoryItem tree for the current frame tree.
    Ref<HistoryItem> topItem = m_frame.mainFrame().loader().history().createItemTree(m_frame, false);
    
    // Override data in the current item (created by createItemTree) to reflect
    // the pushState() arguments.
    m_currentItem->setTitle(title);
    m_currentItem->setStateObject(WTFMove(stateObject));
    m_currentItem->setURLString(urlString);
    m_currentItem->setShouldRestoreScrollPosition(shouldRestoreScrollPosition);

    LOG(History, "HistoryController %p pushState: Adding top item %p, setting url of current item %p to %s, scrollRestoration is %s", this, topItem.ptr(), m_currentItem.get(), urlString.ascii().data(), topItem->shouldRestoreScrollPosition() ? "auto" : "manual");

    page->backForward().addItem(WTFMove(topItem));

    if (m_frame.page()->usesEphemeralSession())
        return;

    addVisitedLink(*page, URL({ }, urlString));
    m_frame.loader().client().updateGlobalHistory();
}

void FrameLoader::HistoryController::replaceState(RefPtr<SerializedScriptValue>&& stateObject, const String& title, const String& urlString)
{
    if (!m_currentItem)
        return;

    LOG(History, "HistoryController %p replaceState: Setting url of current item %p to %s scrollRestoration %s", this, m_currentItem.get(), urlString.ascii().data(), m_currentItem->shouldRestoreScrollPosition() ? "auto" : "manual");

    if (!urlString.isEmpty())
        m_currentItem->setURLString(urlString);
    m_currentItem->setTitle(title);
    m_currentItem->setStateObject(WTFMove(stateObject));
    m_currentItem->setFormData(nullptr);
    m_currentItem->setFormContentType(String());

    ASSERT(m_frame.page());
    if (m_frame.page()->usesEphemeralSession())
        return;

    addVisitedLink(*m_frame.page(), URL({ }, urlString));
    m_frame.loader().client().updateGlobalHistory();
}

void FrameLoader::HistoryController::replaceCurrentItem(HistoryItem* item)
{
    if (!item)
        return;

    m_previousItem = nullptr;
    if (m_provisionalItem)
        m_provisionalItem = item;
    else
        m_currentItem = item;
}

} // namespace WebCore
