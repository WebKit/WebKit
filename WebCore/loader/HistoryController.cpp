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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "BackForwardList.h"
#include "CachedPage.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderStateMachine.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "Page.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "Settings.h"
#include <wtf/text/CString.h>

#if USE(PLATFORM_STRATEGIES)
#include "PlatformStrategies.h"
#include "VisitedLinkStrategy.h"
#endif

namespace WebCore {

static inline void addVisitedLink(Page* page, const KURL& url)
{
#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->visitedLinkStrategy()->addVisitedLink(page, visitedLinkHash(url.string().characters(), url.string().length()));
#else
    page->group().addVisitedLink(url);
#endif
}

HistoryController::HistoryController(Frame* frame)
    : m_frame(frame)
{
}

HistoryController::~HistoryController()
{
}

void HistoryController::saveScrollPositionAndViewStateToItem(HistoryItem* item)
{
    if (!item || !m_frame->view())
        return;
        
    item->setScrollPoint(m_frame->view()->scrollPosition());
    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling through to the client.
    m_frame->loader()->client()->saveViewStateToItem(item);
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
    if (!m_frame->loader()->stateMachine()->committedFirstRealDocumentLoad())
        return;

    ASSERT(m_currentItem);
    
    // FIXME: As the ASSERT attests, it seems we should always have a currentItem here.
    // One counterexample is <rdar://problem/4917290>
    // For now, to cover this issue in release builds, there is no technical harm to returning
    // early and from a user standpoint - as in the above radar - the previous page load failed 
    // so there *is* no scroll or view state to restore!
    if (!m_currentItem)
        return;
    
    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling
    // through to the client. It's currently used only for the PDF view on Mac.
    m_frame->loader()->client()->restoreViewState();
    
    if (FrameView* view = m_frame->view())
        if (!view->wasScrolledByUser())
            view->setScrollPosition(m_currentItem->scrollPoint());
}

void HistoryController::updateBackForwardListForFragmentScroll()
{
    updateBackForwardListClippedAtTarget(false);
}

void HistoryController::saveDocumentState()
{
    // FIXME: Reading this bit of FrameLoader state here is unfortunate.  I need to study
    // this more to see if we can remove this dependency.
    if (m_frame->loader()->stateMachine()->creatingInitialEmptyDocument())
        return;

    // For a standard page load, we will have a previous item set, which will be used to
    // store the form state.  However, in some cases we will have no previous item, and
    // the current item is the right place to save the state.  One example is when we
    // detach a bunch of frames because we are navigating from a site with frames to
    // another site.  Another is when saving the frame state of a frame that is not the
    // target of the current navigation (if we even decide to save with that granularity).

    // Because of previousItem's "masking" of currentItem for this purpose, it's important
    // that previousItem be cleared at the end of a page transition.  We leverage the
    // checkLoadComplete recursion to achieve this goal.

    HistoryItem* item = m_previousItem ? m_previousItem.get() : m_currentItem.get();
    if (!item)
        return;

    Document* document = m_frame->document();
    ASSERT(document);
    
    if (item->isCurrentDocument(document)) {
        LOG(Loading, "WebCoreLoading %s: saving form state to %p", m_frame->tree()->name().string().utf8().data(), item);
        item->setDocumentState(document->formElementsState());
    }
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
void HistoryController::saveDocumentAndScrollState()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame)) {
        frame->loader()->history()->saveDocumentState();
        frame->loader()->history()->saveScrollPositionAndViewStateToItem(frame->loader()->history()->currentItem());
    }
}

void HistoryController::restoreDocumentState()
{
    Document* doc = m_frame->document();
        
    HistoryItem* itemToRestore = 0;
    
    switch (m_frame->loader()->loadType()) {
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            break;
        case FrameLoadTypeBack:
        case FrameLoadTypeBackWMLDeckNotAccessible:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
        case FrameLoadTypeRedirectWithLockedBackForwardList:
        case FrameLoadTypeStandard:
            itemToRestore = m_currentItem.get(); 
    }
    
    if (!itemToRestore)
        return;

    LOG(Loading, "WebCoreLoading %s: restoring form state from %p", m_frame->tree()->name().string().utf8().data(), itemToRestore);
    doc->setStateForNewFormElements(itemToRestore->documentState());
}

void HistoryController::invalidateCurrentItemCachedPage()
{
    // When we are pre-commit, the currentItem is where the pageCache data resides    
    CachedPage* cachedPage = pageCache()->get(currentItem());

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the PageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    
    ASSERT(!cachedPage || cachedPage->document() == m_frame->document());
    if (cachedPage && cachedPage->document() == m_frame->document()) {
        cachedPage->document()->setInPageCache(false);
        cachedPage->clear();
    }
    
    if (cachedPage)
        pageCache()->remove(currentItem());
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
void HistoryController::goToItem(HistoryItem* targetItem, FrameLoadType type)
{
    ASSERT(!m_frame->tree()->parent());
    
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    Page* page = m_frame->page();
    if (!page)
        return;
    if (!m_frame->loader()->client()->shouldGoToHistoryItem(targetItem))
        return;

    // Set the BF cursor before commit, which lets the user quickly click back/forward again.
    // - plus, it only makes sense for the top level of the operation through the frametree,
    // as opposed to happening for some/one of the page commits that might happen soon
    BackForwardList* bfList = page->backForwardList();
    HistoryItem* currentItem = bfList->currentItem();    
    bfList->goToItem(targetItem);
    Settings* settings = m_frame->settings();
    page->setGlobalHistoryItem((!settings || settings->privateBrowsingEnabled()) ? 0 : targetItem);
    recursiveGoToItem(targetItem, currentItem, type);
}

void HistoryController::updateForBackForwardNavigation()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for back/forward navigation in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif

    // Must grab the current scroll position before disturbing it
    saveScrollPositionAndViewStateToItem(m_previousItem.get());
}

void HistoryController::updateForReload()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for reload in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif

    if (m_currentItem) {
        pageCache()->remove(m_currentItem.get());
    
        if (m_frame->loader()->loadType() == FrameLoadTypeReload || m_frame->loader()->loadType() == FrameLoadTypeReloadFromOrigin)
            saveScrollPositionAndViewStateToItem(m_currentItem.get());
    
        // Sometimes loading a page again leads to a different result because of cookies. Bugzilla 4072
        if (m_frame->loader()->documentLoader()->unreachableURL().isEmpty())
            m_currentItem->setURL(m_frame->loader()->documentLoader()->requestURL());
    }
}

// There are 3 things you might think of as "history", all of which are handled by these functions.
//
//     1) Back/forward: The m_currentItem is part of this mechanism.
//     2) Global history: Handled by the client.
//     3) Visited links: Handled by the PageGroup.

void HistoryController::updateForStandardLoad(HistoryUpdateType updateType)
{
    LOG(History, "WebCoreHistory: Updating History for Standard Load in frame %s", m_frame->loader()->documentLoader()->url().string().ascii().data());

    FrameLoader* frameLoader = m_frame->loader();

    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = frameLoader->documentLoader()->urlForHistory();

    if (!frameLoader->documentLoader()->isClientRedirect()) {
        if (!historyURL.isEmpty()) {
            if (updateType != UpdateAllExceptBackForwardList)
                updateBackForwardListClippedAtTarget(true);
            if (!needPrivacy) {
                frameLoader->client()->updateGlobalHistory();
                frameLoader->documentLoader()->setDidCreateGlobalHistoryEntry(true);
                if (frameLoader->documentLoader()->unreachableURL().isEmpty())
                    frameLoader->client()->updateGlobalHistoryRedirectLinks();
            }
            if (Page* page = m_frame->page())
                page->setGlobalHistoryItem(needPrivacy ? 0 : page->backForwardList()->currentItem());
        }
    } else if (frameLoader->documentLoader()->unreachableURL().isEmpty() && m_currentItem) {
        m_currentItem->setURL(frameLoader->documentLoader()->url());
        m_currentItem->setFormInfoFromRequest(frameLoader->documentLoader()->request());
    }

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            addVisitedLink(page, historyURL);

        if (!frameLoader->documentLoader()->didCreateGlobalHistoryEntry() && frameLoader->documentLoader()->unreachableURL().isEmpty() && !frameLoader->url().isEmpty())
            frameLoader->client()->updateGlobalHistoryRedirectLinks();
    }
}

void HistoryController::updateForRedirectWithLockedBackForwardList()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for redirect load in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif
    
    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = m_frame->loader()->documentLoader()->urlForHistory();

    if (m_frame->loader()->documentLoader()->isClientRedirect()) {
        if (!m_currentItem && !m_frame->tree()->parent()) {
            if (!historyURL.isEmpty()) {
                updateBackForwardListClippedAtTarget(true);
                if (!needPrivacy) {
                    m_frame->loader()->client()->updateGlobalHistory();
                    m_frame->loader()->documentLoader()->setDidCreateGlobalHistoryEntry(true);
                    if (m_frame->loader()->documentLoader()->unreachableURL().isEmpty())
                        m_frame->loader()->client()->updateGlobalHistoryRedirectLinks();
                }
                if (Page* page = m_frame->page())
                    page->setGlobalHistoryItem(needPrivacy ? 0 : page->backForwardList()->currentItem());
            }
        }
        if (m_currentItem) {
            m_currentItem->setURL(m_frame->loader()->documentLoader()->url());
            m_currentItem->setFormInfoFromRequest(m_frame->loader()->documentLoader()->request());
        }
    } else {
        Frame* parentFrame = m_frame->tree()->parent();
        if (parentFrame && parentFrame->loader()->history()->m_currentItem)
            parentFrame->loader()->history()->m_currentItem->setChildItem(createItem(true));
    }

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            addVisitedLink(page, historyURL);

        if (!m_frame->loader()->documentLoader()->didCreateGlobalHistoryEntry() && m_frame->loader()->documentLoader()->unreachableURL().isEmpty() && !m_frame->loader()->url().isEmpty())
            m_frame->loader()->client()->updateGlobalHistoryRedirectLinks();
    }
}

void HistoryController::updateForClientRedirect()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for client redirect in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif

    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // webcore has closed the URL and saved away the form state.
    if (m_currentItem) {
        m_currentItem->clearDocumentState();
        m_currentItem->clearScrollPoint();
    }

    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = m_frame->loader()->documentLoader()->urlForHistory();

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            addVisitedLink(page, historyURL);
    }
}

void HistoryController::updateForCommit()
{
    FrameLoader* frameLoader = m_frame->loader();
#if !LOG_DISABLED
    if (frameLoader->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for commit in frame %s", frameLoader->documentLoader()->title().utf8().data());
#endif
    FrameLoadType type = frameLoader->loadType();
    if (isBackForwardLoadType(type) ||
        ((type == FrameLoadTypeReload || type == FrameLoadTypeReloadFromOrigin) && !frameLoader->provisionalDocumentLoader()->unreachableURL().isEmpty())) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        m_previousItem = m_currentItem;
        ASSERT(m_provisionalItem);
        m_currentItem = m_provisionalItem;
        m_provisionalItem = 0;
    }
}

void HistoryController::updateForSameDocumentNavigation()
{
    if (m_frame->loader()->url().isEmpty())
        return;

    Settings* settings = m_frame->settings();
    if (!settings || settings->privateBrowsingEnabled())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    addVisitedLink(page, m_frame->loader()->url());
}

void HistoryController::updateForFrameLoadCompleted()
{
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to clear these out.
    m_previousItem = 0;
}

void HistoryController::setCurrentItem(HistoryItem* item)
{
    m_currentItem = item;
}

void HistoryController::setCurrentItemTitle(const String& title)
{
    if (m_currentItem)
        m_currentItem->setTitle(title);
}

bool HistoryController::currentItemShouldBeReplaced() const
{
    // From the HTML5 spec for location.assign():
    //  "If the browsing context's session history contains only one Document,
    //   and that was the about:blank Document created when the browsing context
    //   was created, then the navigation must be done with replacement enabled."
    return m_currentItem && !m_previousItem && equalIgnoringCase(m_currentItem->urlString(), blankURL());
}

void HistoryController::setProvisionalItem(HistoryItem* item)
{
    m_provisionalItem = item;
}

PassRefPtr<HistoryItem> HistoryController::createItem(bool useOriginal)
{
    DocumentLoader* docLoader = m_frame->loader()->documentLoader();
    
    KURL unreachableURL = docLoader ? docLoader->unreachableURL() : KURL();
    
    KURL url;
    KURL originalURL;

    if (!unreachableURL.isEmpty()) {
        url = unreachableURL;
        originalURL = unreachableURL;
    } else {
        originalURL = docLoader ? docLoader->originalURL() : KURL();
        if (useOriginal)
            url = originalURL;
        else if (docLoader)
            url = docLoader->requestURL();
    }

    LOG(History, "WebCoreHistory: Creating item for %s", url.string().ascii().data());
    
    // Frames that have never successfully loaded any content
    // may have no URL at all. Currently our history code can't
    // deal with such things, so we nip that in the bud here.
    // Later we may want to learn to live with nil for URL.
    // See bug 3368236 and related bugs for more information.
    if (url.isEmpty()) 
        url = blankURL();
    if (originalURL.isEmpty())
        originalURL = blankURL();
    
    Frame* parentFrame = m_frame->tree()->parent();
    String parent = parentFrame ? parentFrame->tree()->name() : "";
    String title = docLoader ? docLoader->title() : "";

    RefPtr<HistoryItem> item = HistoryItem::create(url, m_frame->tree()->name(), parent, title);
    item->setOriginalURLString(originalURL.string());

    if (!unreachableURL.isEmpty() || !docLoader || docLoader->response().httpStatusCode() >= 400)
        item->setLastVisitWasFailure(true);

    // Save form state if this is a POST
    if (docLoader) {
        if (useOriginal)
            item->setFormInfoFromRequest(docLoader->originalRequest());
        else
            item->setFormInfoFromRequest(docLoader->request());
    }
    
    // Set the item for which we will save document state
    m_previousItem = m_currentItem;
    m_currentItem = item;
    
    return item.release();
}

PassRefPtr<HistoryItem> HistoryController::createItemTree(Frame* targetFrame, bool clipAtTarget)
{
    RefPtr<HistoryItem> bfItem = createItem(m_frame->tree()->parent() ? true : false);
    if (m_previousItem)
        saveScrollPositionAndViewStateToItem(m_previousItem.get());

    if (!clipAtTarget || m_frame != targetFrame) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        saveDocumentState();

        // clipAtTarget is false for navigations within the same document, so
        // we should copy the documentSequenceNumber over to the newly create
        // item.  Non-target items are just clones, and they should therefore
        // preserve the same itemSequenceNumber.
        if (m_previousItem) {
            if (m_frame != targetFrame)
                bfItem->setItemSequenceNumber(m_previousItem->itemSequenceNumber());
            bfItem->setDocumentSequenceNumber(m_previousItem->documentSequenceNumber());
        }

        for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
            FrameLoader* childLoader = child->loader();
            bool hasChildLoaded = childLoader->frameHasLoaded();

            // If the child is a frame corresponding to an <object> element that never loaded,
            // we don't want to create a history item, because that causes fallback content
            // to be ignored on reload.
            
            if (!(!hasChildLoaded && childLoader->isHostedByObjectElement()))
                bfItem->addChildItem(childLoader->history()->createItemTree(targetFrame, clipAtTarget));
        }
    }
    // FIXME: Eliminate the isTargetItem flag in favor of itemSequenceNumber.
    if (m_frame == targetFrame)
        bfItem->setIsTargetItem(true);
    return bfItem;
}

// The general idea here is to traverse the frame tree and the item tree in parallel,
// tracking whether each frame already has the content the item requests.  If there is
// a match (by URL), we just restore scroll position and recurse.  Otherwise we must
// reload that frame, and all its kids.
void HistoryController::recursiveGoToItem(HistoryItem* item, HistoryItem* fromItem, FrameLoadType type)
{
    ASSERT(item);
    ASSERT(fromItem);

    // If the item we're going to is a clone of the item we're at, then do
    // not load it again, and continue history traversal to its children.
    // The current frame tree and the frame tree snapshot in the item have
    // to match.
    // Note: If item and fromItem are the same, then we need to create a new
    // document.
    if (item != fromItem 
        && item->itemSequenceNumber() == fromItem->itemSequenceNumber()
        && currentFramesMatchItem(item)
        && fromItem->hasSameFrames(item))
    {
        // This content is good, so leave it alone and look for children that need reloading
        // Save form state (works from currentItem, since prevItem is nil)
        ASSERT(!m_previousItem);
        saveDocumentState();
        saveScrollPositionAndViewStateToItem(m_currentItem.get());

        if (FrameView* view = m_frame->view())
            view->setWasScrolledByUser(false);

        m_currentItem = item;
                
        // Restore form state (works from currentItem)
        restoreDocumentState();
        
        // Restore the scroll position (we choose to do this rather than going back to the anchor point)
        restoreScrollPositionAndViewState();
        
        const HistoryItemVector& childItems = item->children();
        
        int size = childItems.size();
        for (int i = 0; i < size; ++i) {
            String childFrameName = childItems[i]->target();
            HistoryItem* fromChildItem = fromItem->childItemWithTarget(childFrameName);
            ASSERT(fromChildItem);
            Frame* childFrame = m_frame->tree()->child(childFrameName);
            ASSERT(childFrame);
            childFrame->loader()->history()->recursiveGoToItem(childItems[i].get(), fromChildItem, type);
        }
    } else {
        m_frame->loader()->loadItem(item, type);
    }
}

// Helper method that determines whether the current frame tree matches given history item's.
bool HistoryController::currentFramesMatchItem(HistoryItem* item) const
{
    if ((!m_frame->tree()->name().isEmpty() || !item->target().isEmpty()) && m_frame->tree()->name() != item->target())
        return false;
        
    const HistoryItemVector& childItems = item->children();
    if (childItems.size() != m_frame->tree()->childCount())
        return false;
    
    unsigned size = childItems.size();
    for (unsigned i = 0; i < size; ++i) {
        if (!m_frame->tree()->child(childItems[i]->target()))
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

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame->loader()->documentLoader()->urlForHistory().isEmpty())
        return;

    Frame* mainFrame = page->mainFrame();
    ASSERT(mainFrame);
    FrameLoader* frameLoader = mainFrame->loader();

    frameLoader->checkDidPerformFirstNavigation();

    RefPtr<HistoryItem> topItem = frameLoader->history()->createItemTree(m_frame, doClip);
    LOG(BackForward, "WebCoreBackForward - Adding backforward item %p for frame %s", topItem.get(), m_frame->loader()->documentLoader()->url().string().ascii().data());
    page->backForwardList()->addItem(topItem.release());
}

void HistoryController::pushState(PassRefPtr<SerializedScriptValue> stateObject, const String& title, const String& urlString)
{
    if (!m_currentItem)
        return;

    Page* page = m_frame->page();
    ASSERT(page);

    // Get a HistoryItem tree for the current frame tree.
    RefPtr<HistoryItem> topItem = page->mainFrame()->loader()->history()->createItemTree(m_frame, false);
    
    // Override data in the current item (created by createItemTree) to reflect
    // the pushState() arguments.
    m_currentItem->setTitle(title);
    m_currentItem->setStateObject(stateObject);
    m_currentItem->setURLString(urlString);

    page->backForwardList()->addItem(topItem.release());
}

void HistoryController::replaceState(PassRefPtr<SerializedScriptValue> stateObject, const String& title, const String& urlString)
{
    if (!m_currentItem)
        return;

    if (!urlString.isEmpty())
        m_currentItem->setURLString(urlString);
    m_currentItem->setTitle(title);
    m_currentItem->setStateObject(stateObject);
}

} // namespace WebCore
