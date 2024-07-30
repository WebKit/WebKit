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
#include "FrameLoader.h"
#include "FrameLoaderStateMachine.h"
#include "FrameTree.h"
#include "HTMLObjectElement.h"
#include "HistoryItem.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
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

HistoryController::HistoryController(LocalFrame& frame)
    : m_frame(frame)
    , m_frameLoadComplete(true)
    , m_defersLoading(false)
{
}

HistoryController::~HistoryController() = default;

void HistoryController::saveScrollPositionAndViewStateToItem(HistoryItem* item)
{
    RefPtr frameView = m_frame->view();
    if (!item || !frameView)
        return;

    if (m_frame->document()->backForwardCacheState() != Document::NotInBackForwardCache) {
        item->setScrollPosition(frameView->cachedScrollPosition());
#if PLATFORM(IOS_FAMILY)
        item->setUnobscuredContentRect(frameView->cachedUnobscuredContentRect());
        item->setExposedContentRect(frameView->cachedExposedContentRect());
#endif
    } else {
        item->setScrollPosition(frameView->scrollPosition());
#if PLATFORM(IOS_FAMILY)
        item->setUnobscuredContentRect(frameView->unobscuredContentRect());
        item->setExposedContentRect(frameView->exposedContentRect());
#endif
    }

    RefPtr page = m_frame->page();
    if (page && m_frame->isMainFrame()) {
        item->setPageScaleFactor(page->pageScaleFactor() / page->viewScaleFactor());
#if PLATFORM(IOS_FAMILY)
        item->setObscuredInsets(page->obscuredInsets());
#endif
    }

    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling through to the client.
    protectedFrame()->checkedLoader()->client().saveViewStateToItem(*item);

    // Notify clients that the HistoryItem has changed.
    item->notifyChanged();
}

Ref<LocalFrame> HistoryController::protectedFrame() const
{
    return m_frame.get();
}

void HistoryController::clearScrollPositionAndViewState()
{
    RefPtr currentItem = m_currentItem;
    if (!currentItem)
        return;

    currentItem->clearScrollPosition();
    currentItem->setPageScaleFactor(0);
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
void HistoryController::restoreScrollPositionAndViewState()
{
    RefPtr currentItem = m_currentItem;
    Ref frame = m_frame.get();
    if (!currentItem || !frame->loader().stateMachine().committedFirstRealDocumentLoad())
        return;

    RefPtr view = frame->view();

    // FIXME: There is some scrolling related work that needs to happen whenever a page goes into the
    // back/forward cache and similar work that needs to occur when it comes out. This is where we do the work
    // that needs to happen when we exit, and the work that needs to happen when we enter is in
    // Document::setIsInBackForwardCache(bool). It would be nice if there was more symmetry in these spots.
    // https://bugs.webkit.org/show_bug.cgi?id=98698
    if (view) {
        RefPtr page = frame->page();
        if (page && frame->isMainFrame()) {
            if (RefPtr scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(*view);
        }
    }

    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling
    // through to the client.
    frame->checkedLoader()->client().restoreViewState();

#if !PLATFORM(IOS_FAMILY)
    // Don't restore scroll point on iOS as LocalFrameLoaderClient::restoreViewState() does that.
    if (view && !view->wasScrolledByUser()) {
        view->scrollToFocusedElementImmediatelyIfNeeded();

        RefPtr page = frame->page();
        auto desiredScrollPosition = currentItem->shouldRestoreScrollPosition() ? currentItem->scrollPosition() : view->scrollPosition();
        LOG(Scrolling, "HistoryController::restoreScrollPositionAndViewState scrolling to %d,%d", desiredScrollPosition.x(), desiredScrollPosition.y());
        if (page && frame->isMainFrame() && currentItem->pageScaleFactor())
            page->setPageScaleFactor(currentItem->pageScaleFactor() * page->viewScaleFactor(), desiredScrollPosition);
        else
            view->setScrollPosition(desiredScrollPosition);

        // If the scroll position doesn't have to be clamped, consider it successfully restored.
        if (frame->isMainFrame()) {
            auto adjustedDesiredScrollPosition = view->adjustScrollPositionWithinRange(desiredScrollPosition);
            if (desiredScrollPosition == adjustedDesiredScrollPosition)
                frame->checkedLoader()->client().didRestoreScrollPosition();
        }

    }
#endif
}

void HistoryController::updateBackForwardListForFragmentScroll()
{
    updateBackForwardListClippedAtTarget(false);
}

void HistoryController::saveDocumentState()
{
    // FIXME: Reading this bit of FrameLoader state here is unfortunate.  I need to study
    // this more to see if we can remove this dependency.
    if (m_frame->checkedLoader()->stateMachine().creatingInitialEmptyDocument())
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

    RefPtr item = m_frameLoadComplete ? m_currentItem.get() : m_previousItem.get();
    if (!item)
        return;

    ASSERT(m_frame->document());
    Ref document = *m_frame->document();
    if (item->isCurrentDocument(document) && document->hasLivingRenderTree()) {
        if (RefPtr documentLoader = document->loader())
            item->setShouldOpenExternalURLsPolicy(documentLoader->shouldOpenExternalURLsPolicyToPropagate());

        LOG(Loading, "WebCoreLoading frame %" PRIu64 ": saving form state to %p", m_frame->frameID().object().toUInt64(), item.get());
        item->setDocumentState(document->formElementsState());
    }
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
void HistoryController::saveDocumentAndScrollState()
{
    for (Frame* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        CheckedRef history = localFrame->loader().history();
        history->saveDocumentState();
        history->saveScrollPositionAndViewStateToItem(history->currentItem());
    }
}

void HistoryController::restoreDocumentState()
{
    switch (m_frame->loader().loadType()) {
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
    
    RefPtr currentItem = m_currentItem;
    if (!currentItem)
        return;
    if (m_frame->loader().requestedHistoryItem() != currentItem.get())
        return;
    RefPtr documentLoader = m_frame->loader().documentLoader();
    if (documentLoader->isClientRedirect())
        return;

    documentLoader->setShouldOpenExternalURLsPolicy(currentItem->shouldOpenExternalURLsPolicy());

    LOG(Loading, "WebCoreLoading frame %" PRIu64 ": restoring form state from %p", m_frame->frameID().object().toUInt64(), currentItem.get());
    m_frame->protectedDocument()->setStateForNewFormElements(currentItem->documentState());
}

void HistoryController::invalidateCurrentItemCachedPage()
{
    RefPtr currentItem = m_currentItem;
    if (!currentItem)
        return;

    // When we are pre-commit, the currentItem is where any back/forward cache data resides.
    auto cachedPage = BackForwardCache::singleton().take(*currentItem, m_frame->protectedPage().get());
    if (!cachedPage)
        return;

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the PageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    
    RefPtr cachedPageDocument = cachedPage->document();
    ASSERT(cachedPageDocument == m_frame->document());
    if (cachedPageDocument == m_frame->document()) {
        cachedPageDocument->setBackForwardCacheState(Document::NotInBackForwardCache);
        cachedPage->clear();
    }
}

bool HistoryController::shouldStopLoadingForHistoryItem(HistoryItem& targetItem) const
{
    RefPtr currentItem = m_currentItem;
    if (!currentItem)
        return false;

    // Don't abort the current load if we're navigating within the current document.
    if (currentItem->shouldDoSameDocumentNavigationTo(targetItem))
        return false;

    return true;
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
void HistoryController::goToItem(HistoryItem& targetItem, FrameLoadType type, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    LOG(History, "HistoryController %p goToItem %p type=%d", this, &targetItem, static_cast<int>(type));

    ASSERT(!m_frame->tree().parent());
    
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    Ref frame = m_frame.get();
    RefPtr page = frame->page();
    if (!page)
        return;
    if (!frame->checkedLoader()->client().shouldGoToHistoryItem(targetItem))
        return;
    if (m_defersLoading) {
        m_deferredItem = &targetItem;
        m_deferredFrameLoadType = type;
        return;
    }

    // Set the BF cursor before commit, which lets the user quickly click back/forward again.
    // - plus, it only makes sense for the top level of the operation through the frame tree,
    // as opposed to happening for some/one of the page commits that might happen soon
    RefPtr currentItem = page->backForward().currentItem();
    page->backForward().setCurrentItem(targetItem);

    // First set the provisional item of any frames that are not actually navigating.
    // This must be done before trying to navigate the desired frame, because some
    // navigations can commit immediately (such as about:blank).  We must be sure that
    // all frames have provisional items set before the commit.
    recursiveSetProvisionalItem(targetItem, currentItem.get());

    // Now that all other frames have provisional items, do the actual navigation.
    recursiveGoToItem(targetItem, currentItem.get(), type, shouldTreatAsContinuingLoad);
}

void HistoryController::setDefersLoading(bool defer)
{
    m_defersLoading = defer;
    if (defer)
        return;

    if (RefPtr deferredItem = m_deferredItem) {
        goToItem(*deferredItem, m_deferredFrameLoadType, ShouldTreatAsContinuingLoad::No);
        m_deferredItem = nullptr;
    }
}

void HistoryController::updateForBackForwardNavigation()
{
    LOG(History, "HistoryController %p updateForBackForwardNavigation: Updating History for back/forward navigation in frame %p (main frame %d) %s", this, &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader() ? m_frame->loader().documentLoader()->url().string().utf8().data() : "");

    // Must grab the current scroll position before disturbing it
    if (!m_frameLoadComplete)
        saveScrollPositionAndViewStateToItem(protectedPreviousItem().get());

    // When traversing history, we may end up redirecting to a different URL
    // this time (e.g., due to cookies).  See http://webkit.org/b/49654.
    updateCurrentItem();
}

void HistoryController::updateForReload()
{
    LOG(History, "HistoryController %p updateForReload: Updating History for reload in frame %p (main frame %d) %s", this, &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader() ? m_frame->loader().documentLoader()->url().string().utf8().data() : "");

    if (RefPtr currentItem = m_currentItem) {
        BackForwardCache::singleton().remove(*currentItem);
    
        if (m_frame->loader().loadType() == FrameLoadType::Reload || m_frame->loader().loadType() == FrameLoadType::ReloadFromOrigin)
            saveScrollPositionAndViewStateToItem(currentItem.get());

        // Rebuild the history item tree when reloading as trying to re-associate everything is too error-prone.
        currentItem->clearChildren();
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

void HistoryController::updateForStandardLoad(HistoryUpdateType updateType)
{
    LOG(History, "HistoryController %p updateForStandardLoad: Updating History for standard load in frame %p (main frame %d) %s", this, &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader()->url().string().ascii().data());

    Ref frame = m_frame.get();
    CheckedRef frameLoader = frame->loader();

    bool usesEphemeralSession = m_frame->page() ? m_frame->page()->usesEphemeralSession() : true;
    const URL& historyURL = frameLoader->documentLoader()->urlForHistory();

    RefPtr documentLoader = frameLoader->documentLoader();
    if (!frameLoader->documentLoader()->isClientRedirect()) {
        if (!historyURL.isEmpty()) {
            if (updateType != UpdateAllExceptBackForwardList)
                updateBackForwardListClippedAtTarget(true);
            if (!usesEphemeralSession) {
                frameLoader->client().updateGlobalHistory();
                documentLoader->setDidCreateGlobalHistoryEntry(true);
                if (documentLoader->unreachableURL().isEmpty())
                    frameLoader->client().updateGlobalHistoryRedirectLinks();
            }
        }
    } else {
        // The client redirect replaces the current history item.
        updateCurrentItem();
    }

    if (!historyURL.isEmpty() && !usesEphemeralSession) {
        if (RefPtr page = m_frame->page())
            addVisitedLink(*page, historyURL);

        if (!documentLoader->didCreateGlobalHistoryEntry() && documentLoader->unreachableURL().isEmpty() && !m_frame->document()->url().isEmpty())
            frameLoader->client().updateGlobalHistoryRedirectLinks();
    }
}

void HistoryController::updateForRedirectWithLockedBackForwardList()
{
    LOG(History, "HistoryController %p updateForRedirectWithLockedBackForwardList: Updating History for redirect load in frame %p (main frame %d) %s", this, &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader() ? m_frame->loader().documentLoader()->url().string().utf8().data() : "");
    
    Ref frame = m_frame.get();
    RefPtr documentLoader = frame->loader().documentLoader();
    bool usesEphemeralSession = frame->page() ? frame->page()->usesEphemeralSession() : true;
    auto historyURL = documentLoader ? documentLoader->urlForHistory() : URL { };

    if (documentLoader && documentLoader->isClientRedirect()) {
        if (!m_currentItem && !frame->tree().parent()) {
            if (!historyURL.isEmpty()) {
                updateBackForwardListClippedAtTarget(true);
                if (!usesEphemeralSession) {
                    CheckedRef frameLoader = frame->loader();
                    frameLoader->client().updateGlobalHistory();
                    documentLoader->setDidCreateGlobalHistoryEntry(true);
                    if (documentLoader->unreachableURL().isEmpty())
                        frameLoader->client().updateGlobalHistoryRedirectLinks();
                }
            }
        }
        // The client redirect replaces the current history item.
        updateCurrentItem();
    } else {
        RefPtr page = m_frame->page();
        RefPtr parentFrame = dynamicDowncast<LocalFrame>(m_frame->tree().parent());
        if (page && parentFrame) {
            if (RefPtr parentCurrentItem = parentFrame->loader().history().currentItem())
                parentCurrentItem->setChildItem(createItem(page->historyItemClient()));
        }
    }

    if (!historyURL.isEmpty() && !usesEphemeralSession) {
        if (RefPtr page = m_frame->page())
            addVisitedLink(*page, historyURL);

        if (!documentLoader->didCreateGlobalHistoryEntry() && documentLoader->unreachableURL().isEmpty())
            frame->checkedLoader()->client().updateGlobalHistoryRedirectLinks();
    }
}

void HistoryController::updateForClientRedirect()
{
    LOG(History, "HistoryController %p updateForClientRedirect: Updating History for client redirect in frame %p (main frame %d) %s", this, &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader() ? m_frame->loader().documentLoader()->url().string().utf8().data() : "");

    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // webcore has closed the URL and saved away the form state.
    if (RefPtr currentItem = m_currentItem) {
        currentItem->clearDocumentState();
        currentItem->clearScrollPosition();
    }

    bool usesEphemeralSession = m_frame->page() ? m_frame->page()->usesEphemeralSession() : true;
    const URL& historyURL = m_frame->loader().documentLoader()->urlForHistory();

    if (!historyURL.isEmpty() && !usesEphemeralSession) {
        if (RefPtr page = m_frame->page())
            addVisitedLink(*page, historyURL);
    }
}

void HistoryController::updateForCommit()
{
    Ref frame = m_frame.get();
    CheckedRef frameLoader = frame->loader();
    LOG(History, "HistoryController %p updateForCommit: Updating History for commit in frame %p (main frame %d) %s", this, &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader() ? m_frame->loader().documentLoader()->url().string().utf8().data() : "");

    FrameLoadType type = frameLoader->loadType();
    if (isBackForwardLoadType(type)
        || isReplaceLoadTypeWithProvisionalItem(type)
        || (isReloadTypeWithProvisionalItem(type) && !frameLoader->provisionalDocumentLoader()->unreachableURL().isEmpty())) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=146842
        // We should always have a provisional item when committing, but we sometimes don't.
        // Not having one leads to us not having a m_currentItem later, which is also a terrible known issue.
        // We should get to the bottom of this.
        ASSERT(m_provisionalItem);
        if (RefPtr provisionalItem = m_provisionalItem) {
            setCurrentItem(provisionalItem.releaseNonNull());
            m_provisionalItem = nullptr;
        }

        // Tell all other frames in the tree to commit their provisional items and
        // restore their scroll position.  We'll avoid this frame (which has already
        // committed) and its children (which will be replaced).
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame()))
            localFrame->loader().checkedHistory()->recursiveUpdateForCommit();
    }
}

bool HistoryController::isReplaceLoadTypeWithProvisionalItem(FrameLoadType type)
{
    // Going back to an error page in a subframe can trigger a FrameLoadType::Replace
    // while m_provisionalItem is set, so we need to commit it.
    return type == FrameLoadType::Replace && m_provisionalItem;
}

bool HistoryController::isReloadTypeWithProvisionalItem(FrameLoadType type)
{
    return (type == FrameLoadType::Reload || type == FrameLoadType::ReloadFromOrigin) && m_provisionalItem;
}

void HistoryController::recursiveUpdateForCommit()
{
    // The frame that navigated will now have a null provisional item.
    // Ignore it and its children.
    if (!m_provisionalItem)
        return;

    // For each frame that already had the content the item requested (based on
    // (a matching URL and frame tree snapshot), just restore the scroll position.
    // Save form state (works from currentItem, since m_frameLoadComplete is true)
    if (m_currentItem && itemsAreClones(*protectedCurrentItem(), protectedProvisionalItem().get())) {
        ASSERT(m_frameLoadComplete);
        saveDocumentState();
        saveScrollPositionAndViewStateToItem(protectedCurrentItem().get());

        if (auto* view = m_frame->view())
            view->setWasScrolledByUser(false);

        // Now commit the provisional item
        if (RefPtr provisionalItem = m_provisionalItem) {
            setCurrentItem(provisionalItem.releaseNonNull());
            m_provisionalItem = nullptr;
        }

        // Restore form state (works from currentItem)
        restoreDocumentState();

        // Restore the scroll position (we choose to do this rather than going back to the anchor point)
        restoreScrollPositionAndViewState();
    }

    // Iterate over the rest of the tree
    for (auto* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localChild = dynamicDowncast<LocalFrame>(child))
            localChild->loader().checkedHistory()->recursiveUpdateForCommit();
    }
}

void HistoryController::updateForSameDocumentNavigation()
{
    Ref frame = m_frame.get();
    if (frame->document()->url().isEmpty())
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    bool usesEphemeralSession = page->usesEphemeralSession();
    if (!usesEphemeralSession)
        addVisitedLink(*page, frame->document()->url());

    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame()))
        localFrame->loader().checkedHistory()->recursiveUpdateForSameDocumentNavigation();

    if (RefPtr currentItem = m_currentItem) {
        currentItem->setURL(frame->document()->url());
        if (!usesEphemeralSession)
            frame->checkedLoader()->client().updateGlobalHistory();
    }
}

void HistoryController::recursiveUpdateForSameDocumentNavigation()
{
    // The frame that navigated will now have a null provisional item.
    // Ignore it and its children.
    if (!m_provisionalItem)
        return;

    // The provisional item may represent a different pending navigation.
    // Don't commit it if it isn't a same document navigation.
    if (m_currentItem && !protectedCurrentItem()->shouldDoSameDocumentNavigationTo(*protectedProvisionalItem()))
        return;

    // Commit the provisional item.
    if (RefPtr provisionalItem = m_provisionalItem) {
        setCurrentItem(provisionalItem.releaseNonNull());
        m_provisionalItem = nullptr;
    }

    // Iterate over the rest of the tree.
    for (auto* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localChild = dynamicDowncast<LocalFrame>(child))
            localChild->loader().checkedHistory()->recursiveUpdateForSameDocumentNavigation();
    }
}

void HistoryController::updateForFrameLoadCompleted()
{
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to track that
    // the load is complete so that we use the current item instead.
    m_frameLoadComplete = true;
}

void HistoryController::setCurrentItem(Ref<HistoryItem>&& item)
{
    m_frameLoadComplete = false;
    m_previousItem = std::exchange(m_currentItem, WTFMove(item));
}

void HistoryController::setCurrentItemTitle(const StringWithDirection& title)
{
    // FIXME: This ignores the title's direction.
    if (RefPtr currentItem = m_currentItem)
        currentItem->setTitle(title.string);
}

bool HistoryController::currentItemShouldBeReplaced() const
{
    // From the HTML5 spec for location.assign():
    //  "If the browsing context's session history contains only one Document,
    //   and that was the about:blank Document created when the browsing context
    //   was created, then the navigation must be done with replacement enabled."
    return m_currentItem && !m_previousItem && equalIgnoringASCIICase(m_currentItem->urlString(), aboutBlankURL().string());
}

void HistoryController::clearPreviousItem()
{
    m_previousItem = nullptr;
    for (auto* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localChild = dynamicDowncast<LocalFrame>(child))
            localChild->loader().checkedHistory()->clearPreviousItem();
    }
}

void HistoryController::setProvisionalItem(RefPtr<HistoryItem>&& item)
{
    m_provisionalItem = WTFMove(item);
}

void HistoryController::initializeItem(HistoryItem& item)
{
    RefPtr documentLoader = m_frame->loader().documentLoader();
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
    item.setTarget(m_frame->tree().uniqueName());
    // FIXME: Should store the title direction as well.
    item.setTitle(title.string);
    item.setOriginalURLString(originalURL.string());

    if (!unreachableURL.isEmpty() || documentLoader->response().httpStatusCode() >= 400)
        item.setLastVisitWasFailure(true);

    item.setShouldOpenExternalURLsPolicy(documentLoader->shouldOpenExternalURLsPolicyToPropagate());

    // Save form state if this is a POST
    item.setFormInfoFromRequest(documentLoader->request());
}

Ref<HistoryItem> HistoryController::createItem(HistoryItemClient& client)
{
    Ref item = HistoryItem::create(client);
    initializeItem(item);
    
    // Set the item for which we will save document state
    setCurrentItem(item.copyRef());
    
    return item;
}

Ref<HistoryItem> HistoryController::createItemTree(HistoryItemClient& client, LocalFrame& targetFrame, bool clipAtTarget)
{
    Ref item = createItem(client);
    if (!m_frameLoadComplete)
        saveScrollPositionAndViewStateToItem(protectedPreviousItem().get());

    if (!clipAtTarget || m_frame.ptr() != &targetFrame) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        saveDocumentState();

        // clipAtTarget is false for navigations within the same document, so
        // we should copy the documentSequenceNumber over to the newly create
        // item.  Non-target items are just clones, and they should therefore
        // preserve the same itemSequenceNumber.
        if (RefPtr previousItem = m_previousItem) {
            if (m_frame.ptr() != &targetFrame)
                item->setItemSequenceNumber(previousItem->itemSequenceNumber());
            item->setDocumentSequenceNumber(previousItem->documentSequenceNumber());
        }

        for (auto* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
            RefPtr localChild = dynamicDowncast<LocalFrame>(child);
            if (!localChild)
                continue;
            CheckedRef childLoader = localChild->loader();
            bool hasChildLoaded = childLoader->frameHasLoaded();

            // If the child is a frame corresponding to an <object> element that never loaded,
            // we don't want to create a history item, because that causes fallback content
            // to be ignored on reload.
            
            if (!(!hasChildLoaded && localChild->ownerElement() && is<HTMLObjectElement>(localChild->ownerElement())))
                item->addChildItem(childLoader->history().createItemTree(client, targetFrame, clipAtTarget));
        }
    }
    // FIXME: Eliminate the isTargetItem flag in favor of itemSequenceNumber.
    if (m_frame.ptr() == &targetFrame)
        item->setIsTargetItem(true);
    return item;
}

// The general idea here is to traverse the frame tree and the item tree in parallel,
// tracking whether each frame already has the content the item requests.  If there is
// a match, we set the provisional item and recurse.  Otherwise we will reload that
// frame and all its kids in recursiveGoToItem.
void HistoryController::recursiveSetProvisionalItem(HistoryItem& item, HistoryItem* fromItem)
{
    if (!itemsAreClones(item, fromItem))
        return;

    // Set provisional item, which will be committed in recursiveUpdateForCommit.
    m_provisionalItem = &item;

    for (Ref childItem : item.children()) {
        auto& childFrameName = childItem->target();

        RefPtr fromChildItem = fromItem->childItemWithTarget(childFrameName);
        if (!fromChildItem)
            continue;

        if (RefPtr childFrame = dynamicDowncast<LocalFrame>(m_frame->tree().childByUniqueName(childFrameName)))
            childFrame->loader().checkedHistory()->recursiveSetProvisionalItem(childItem, fromChildItem.get());
    }
}

// We now traverse the frame tree and item tree a second time, loading frames that
// do have the content the item requests.
void HistoryController::recursiveGoToItem(HistoryItem& item, HistoryItem* fromItem, FrameLoadType type, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    if (!itemsAreClones(item, fromItem)) {
        protectedFrame()->checkedLoader()->loadItem(item, fromItem, type, shouldTreatAsContinuingLoad);
        return;
    }

    // Just iterate over the rest, looking for frames to navigate.
    for (Ref childItem : item.children()) {
        auto& childFrameName = childItem->target();

        RefPtr fromChildItem = fromItem->childItemWithTarget(childFrameName);
        if (!fromChildItem)
            continue;

        if (RefPtr childFrame = dynamicDowncast<LocalFrame>(m_frame->tree().childByUniqueName(childFrameName)))
            childFrame->loader().checkedHistory()->recursiveGoToItem(childItem, fromChildItem.get(), type, shouldTreatAsContinuingLoad);
    }
}

// The following logic must be kept in sync with WebKit::WebBackForwardListItem::itemIsClone().
bool HistoryController::itemsAreClones(HistoryItem& item1, HistoryItem* item2) const
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
bool HistoryController::currentFramesMatchItem(HistoryItem& item) const
{
    if ((!m_frame->tree().uniqueName().isEmpty() || !item.target().isEmpty()) && m_frame->tree().uniqueName() != item.target())
        return false;

    const auto& childItems = item.children();
    if (childItems.size() != m_frame->tree().childCount())
        return false;
    
    for (auto& item : childItems) {
        if (!m_frame->tree().childByUniqueName(item->target()))
            return false;
    }
    
    return true;
}

void HistoryController::updateBackForwardListClippedAtTarget(bool doClip)
{
    // In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  
    // The item that was the target of the user's navigation is designated as the "targetItem".  
    // When this function is called with doClip=true we're able to create the whole tree except for the target's children, 
    // which will be loaded in the future. That part of the tree will be filled out as the child loads are committed.

    RefPtr page = m_frame->page();
    if (!page)
        return;

    if (m_frame->loader().documentLoader()->urlForHistory().isEmpty())
        return;

    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_frame->mainFrame());
    if (!localFrame)
        return;

    CheckedRef frameLoader = localFrame->loader();

    Ref topItem = frameLoader->checkedHistory()->createItemTree(page->historyItemClient(), protectedFrame(), doClip);
    LOG(History, "HistoryController %p updateBackForwardListClippedAtTarget: Adding backforward item %p in frame %p (main frame %d) %s", this, topItem.ptr(), &m_frame, m_frame->isMainFrame(), m_frame->loader().documentLoader()->url().string().utf8().data());

    page->backForward().addItem(WTFMove(topItem));
}

void HistoryController::updateCurrentItem()
{
    RefPtr currentItem = m_currentItem;
    if (!currentItem)
        return;

    RefPtr documentLoader = m_frame->loader().documentLoader();
    if (!documentLoader || !documentLoader->unreachableURL().isEmpty())
        return;

    if (currentItem->url() != documentLoader->url()) {
        // We ended up on a completely different URL this time, so the HistoryItem
        // needs to be re-initialized.  Preserve the isTargetItem flag as it is a
        // property of how this HistoryItem was originally created and is not
        // dependent on the document.
        bool isTargetItem = currentItem->isTargetItem();
        currentItem->reset();
        initializeItem(*currentItem);
        currentItem->setIsTargetItem(isTargetItem);
    } else {
        // Even if the final URL didn't change, the form data may have changed.
        currentItem->setFormInfoFromRequest(documentLoader->request());
    }
}

void HistoryController::pushState(RefPtr<SerializedScriptValue>&& stateObject, const String& title, const String& urlString)
{
    if (!m_currentItem)
        return;

    Ref frame = m_frame.get();
    RefPtr page = frame->page();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    bool shouldRestoreScrollPosition = m_currentItem->shouldRestoreScrollPosition();

    // Get a HistoryItem tree for the current frame tree.
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return;

    Ref topItem = localFrame->loader().history().createItemTree(page->historyItemClient(), frame, false);

    RefPtr document = frame->document();
    if (document && !document->hasRecentUserInteractionForNavigationFromJS())
        topItem->setWasCreatedByJSWithoutUserInteraction(true);

    // Override data in the current item (created by createItemTree) to reflect
    // the pushState() arguments.
    RefPtr currentItem = m_currentItem;
    currentItem->setTitle(title);
    currentItem->setStateObject(WTFMove(stateObject));
    currentItem->setURLString(urlString);
    currentItem->setShouldRestoreScrollPosition(shouldRestoreScrollPosition);

    LOG(History, "HistoryController %p pushState: Adding top item %p, setting url of current item %p to %s, scrollRestoration is %s", this, topItem.ptr(), m_currentItem.get(), urlString.ascii().data(), topItem->shouldRestoreScrollPosition() ? "auto" : "manual");

    page->backForward().addItem(WTFMove(topItem));

    if (page->usesEphemeralSession())
        return;

    addVisitedLink(*page, URL({ }, urlString));
    frame->checkedLoader()->client().updateGlobalHistory();
}

void HistoryController::replaceState(RefPtr<SerializedScriptValue>&& stateObject, const String& title, const String& urlString)
{
    RefPtr currentItem = m_currentItem;
    if (!currentItem)
        return;

    LOG(History, "HistoryController %p replaceState: Setting url of current item %p to %s scrollRestoration %s", this, currentItem.get(), urlString.ascii().data(), currentItem->shouldRestoreScrollPosition() ? "auto" : "manual");

    if (!urlString.isEmpty())
        currentItem->setURLString(urlString);
    currentItem->setTitle(title);
    currentItem->setStateObject(WTFMove(stateObject));
    currentItem->setFormData(nullptr);
    currentItem->setFormContentType(String());

    Ref frame = m_frame.get();
    RefPtr page = frame->page();
    ASSERT(page);
    if (page->usesEphemeralSession())
        return;

    addVisitedLink(*page, URL({ }, urlString));
    frame->checkedLoader()->client().updateGlobalHistory();
}

void HistoryController::replaceCurrentItem(RefPtr<HistoryItem>&& item)
{
    if (!item)
        return;

    m_previousItem = nullptr;
    if (m_provisionalItem)
        m_provisionalItem = WTFMove(item);
    else
        m_currentItem = WTFMove(item);
}

RefPtr<HistoryItem> HistoryController::protectedCurrentItem() const
{
    return m_currentItem;
}

RefPtr<HistoryItem> HistoryController::protectedPreviousItem() const
{
    return m_previousItem;
}

RefPtr<HistoryItem> HistoryController::protectedProvisionalItem() const
{
    return m_provisionalItem;
}

} // namespace WebCore
