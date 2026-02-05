/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebBackForwardList.h"

#include "APIArray.h"
#include "BrowsingContextGroup.h"
#include "LoadedWebArchive.h"
#include "Logging.h"
#include "SessionState.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardListCounts.h"
#include "WebBackForwardListFrameItem.h"
#include "WebBackForwardListSwiftUtilities.h"
#include "WebFrameProxy.h"
#include "WebInspectorUtilities.h"
#include "WebPageProxy.h"
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=306415
#if ENABLE(BACK_FORWARD_LIST_SWIFT)
#include "WebKit-Swift.h"
#endif

namespace WebKit {
using namespace WebCore;

#if !ENABLE(BACK_FORWARD_LIST_SWIFT)

static const unsigned DefaultCapacity = 100;

WebBackForwardList::WebBackForwardList(WebPageProxy& page)
    : m_page(&page)
{
    LOG(BackForward, "(Back/Forward) Created WebBackForwardList %p", this);
}

WebBackForwardList::~WebBackForwardList()
{
    LOG(BackForward, "(Back/Forward) Destroying WebBackForwardList %p", this);

    // A WebBackForwardList should never be destroyed unless it's associated page has been closed or is invalid.
    ASSERT((!m_page && !m_currentIndex) || !m_page->hasRunningProcess());
}

WebBackForwardListItem* WebBackForwardList::itemForID(BackForwardItemIdentifier identifier)
{
    if (!m_page)
        return nullptr;

    ASSERT(!WebBackForwardListItem::itemForID(identifier) || WebBackForwardListItem::itemForID(identifier)->pageID() == m_page->identifier());
    return WebBackForwardListItem::itemForID(identifier);
}

void WebBackForwardList::pageClosed()
{
    LOG(BackForward, "(Back/Forward) WebBackForwardList %p had its page closed with current size %zu", this, m_entries.size());

    // We should have always started out with an m_page and we should never close the page twice.
    ASSERT(m_page);

    if (m_page) {
        size_t size = m_entries.size();
        for (size_t i = 0; i < size; ++i)
            didRemoveItem(m_entries[i]);
    }

    m_page.clear();
    m_entries.clear();
    m_currentIndex = std::nullopt;
}

void WebBackForwardList::addItem(Ref<WebBackForwardListItem>&& newItem)
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    RefPtr page = m_page.get();
    if (!page)
        return;

    Vector<Ref<WebBackForwardListItem>> removedItems;
    
    if (m_currentIndex) {
        page->recordAutomaticNavigationSnapshot();

        // Toss everything in the forward list.
        unsigned targetSize = *m_currentIndex + 1;
        removedItems.reserveInitialCapacity(m_entries.size() - targetSize);
        while (m_entries.size() > targetSize) {
            didRemoveItem(m_entries.last());
            removedItems.append(WTF::move(m_entries.last()));
            m_entries.removeLast();
        }

        // Toss the first item if the list is getting too big, as long as we're not using it
        // (or even if we are, if we only want 1 entry).
        if (m_entries.size() >= DefaultCapacity && (*m_currentIndex)) {
            didRemoveItem(m_entries[0]);
            removedItems.append(WTF::move(m_entries[0]));
            m_entries.removeAt(0);

            if (m_entries.isEmpty())
                m_currentIndex = std::nullopt;
            else
                --*m_currentIndex;
        }
    } else {
        // If we have no current item index we should also not have any entries.
        ASSERT(m_entries.isEmpty());

        // But just in case it does happen in practice we'll get back in to a consistent state now before adding the new item.
        size_t size = m_entries.size();
        for (size_t i = 0; i < size; ++i) {
            didRemoveItem(m_entries[i]);
            removedItems.append(WTF::move(m_entries[i]));
        }
        m_entries.clear();
    }

    bool shouldKeepCurrentItem = true;

    if (!m_currentIndex) {
        ASSERT(m_entries.isEmpty());
        m_currentIndex = 0;
    } else {
        shouldKeepCurrentItem = page->shouldKeepCurrentBackForwardListItemInList(m_entries[*m_currentIndex]);
        if (shouldKeepCurrentItem)
            ++*m_currentIndex;
    }

    auto* newItemPtr = newItem.ptr();
    if (!shouldKeepCurrentItem) {
        // m_current should never be pointing past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to swap in the new item.
        ASSERT(*m_currentIndex < m_entries.size());

        removedItems.append(m_entries[*m_currentIndex].copyRef());
        m_entries[*m_currentIndex] = WTF::move(newItem);
    } else {
        // m_current should never be pointing more than 1 past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to insert the new item.
        ASSERT(*m_currentIndex <= m_entries.size());

        if (*m_currentIndex <= m_entries.size())
            m_entries.insert(*m_currentIndex, WTF::move(newItem));
    }

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p added an item. Current size %zu, current index %zu, threw away %zu items", this, m_entries.size(), *m_currentIndex, removedItems.size());
    page->didChangeBackForwardList(newItemPtr, WTF::move(removedItems));
}

void WebBackForwardList::addChildItem(FrameIdentifier parentFrameID, Ref<FrameState>&& frameState)
{
    RefPtr currentItem = this->currentItem();
    if (!currentItem)
        return;

    RefPtr parentItem = protect(currentItem->mainFrameItem())->childItemForFrameID(parentFrameID);
    if (!parentItem)
        return;

    parentItem->setChild(WTF::move(frameState));
}

void WebBackForwardList::goToItem(WebBackForwardListItem& item)
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    RefPtr page = m_page.get();
    if (!m_entries.size() || !page || !m_currentIndex)
        return;

    size_t targetIndex = notFound;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].ptr() == &item) {
            targetIndex = i;
            break;
        }
    }

    // If the target item wasn't even in the list, there's nothing else to do.
    if (targetIndex == notFound) {
        LOG(BackForward, "(Back/Forward) WebBackForwardList %p could not go to item %s (%s) because it was not found", this, item.identifier().toString().utf8().data(), item.url().utf8().data());
        return;
    }

    if (targetIndex < *m_currentIndex) {
        unsigned delta = m_entries.size() - targetIndex - 1;
        String deltaValue = delta > 10 ? "over10"_s : String::number(delta);
        page->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::backNavigationDeltaKey(), deltaValue, ShouldSample::No);
    }

    // If we're going to an item different from the current item, ask the client if the current
    // item should remain in the list.
    auto& currentItem = m_entries[*m_currentIndex];
    bool shouldKeepCurrentItem = true;
    if (currentItem.ptr() != &item) {
        page->recordAutomaticNavigationSnapshot();
        shouldKeepCurrentItem = page->shouldKeepCurrentBackForwardListItemInList(m_entries[*m_currentIndex]);
    }

    // If the client said to remove the current item, remove it and then update the target index.
    Vector<Ref<WebBackForwardListItem>> removedItems;
    if (!shouldKeepCurrentItem) {
        removedItems.append(currentItem.copyRef());
        m_entries.removeAt(*m_currentIndex);
        targetIndex = notFound;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].ptr() == &item) {
                targetIndex = i;
                break;
            }
        }
        ASSERT(targetIndex != notFound);
    }

    m_currentIndex = targetIndex;

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p going to item %s, is now at index %zu", this, item.identifier().toString().utf8().data(), targetIndex);
    page->didChangeBackForwardList(nullptr, WTF::move(removedItems));
}

WebBackForwardListItem* WebBackForwardList::currentItem() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    return m_page && m_currentIndex ? m_entries[*m_currentIndex].ptr() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::backItem() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    return m_page && m_currentIndex && *m_currentIndex ? m_entries[*m_currentIndex - 1].ptr() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::forwardItem() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    return m_page && m_currentIndex && m_entries.size() && *m_currentIndex < m_entries.size() - 1 ? m_entries[*m_currentIndex + 1].ptr() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::itemAtIndex(int index) const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    if (!m_currentIndex || !m_page)
        return nullptr;
    
    // Do range checks without doing math on index to avoid overflow.
    if (index < 0 && static_cast<unsigned>(-index) > backListCount())
        return nullptr;
    
    if (index > 0 && static_cast<unsigned>(index) > forwardListCount())
        return nullptr;

    return m_entries[index + *m_currentIndex].ptr();
}

unsigned WebBackForwardList::backListCount() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    return m_page && m_currentIndex ? *m_currentIndex : 0;
}

unsigned WebBackForwardList::forwardListCount() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    return m_page && m_currentIndex ? m_entries.size() - (*m_currentIndex + 1) : 0;
}

WebBackForwardListCounts WebBackForwardList::counts() const
{
    return WebBackForwardListCounts { backListCount(), forwardListCount() };
}

Ref<API::Array> WebBackForwardList::backList() const
{
    return backListAsAPIArrayWithLimit(backListCount());
}

Ref<API::Array> WebBackForwardList::forwardList() const
{
    return forwardListAsAPIArrayWithLimit(forwardListCount());
}

Ref<API::Array> WebBackForwardList::backListAsAPIArrayWithLimit(unsigned limit) const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    if (!m_page || !m_currentIndex)
        return API::Array::create();

    unsigned backListSize = static_cast<unsigned>(backListCount());
    unsigned size = std::min(backListSize, limit);
    if (!size)
        return API::Array::create();

    ASSERT(backListSize >= size);
    size_t startIndex = backListSize - size;
    Vector<RefPtr<API::Object>> vector(size, [&](size_t i) -> RefPtr<API::Object> {
        // FIXME: Remove SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE when the false positive
        // in the static analyzer is fixed.
        SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE return m_entries[startIndex + i].ptr();
    });

    return API::Array::create(WTF::move(vector));
}

Ref<API::Array> WebBackForwardList::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    if (!m_page || !m_currentIndex)
        return API::Array::create();

    unsigned size = std::min(static_cast<unsigned>(forwardListCount()), limit);
    if (!size)
        return API::Array::create();

    size_t startIndex = *m_currentIndex + 1;
    Vector<RefPtr<API::Object>> vector(size, [&](size_t i) -> RefPtr<API::Object> {
        return m_entries[startIndex + i].ptr();
    });

    return API::Array::create(WTF::move(vector));
}

void WebBackForwardList::removeAllItems()
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p removeAllItems (has %zu of them)", this, m_entries.size());

    for (auto& entry : m_entries)
        didRemoveItem(entry);

    m_currentIndex = std::nullopt;
    protect(m_page)->didChangeBackForwardList(nullptr, std::exchange(m_entries, { }));
}

void WebBackForwardList::clear()
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p clear (has %zu of them)", this, m_entries.size());

    RefPtr page = m_page.get();
    size_t size = m_entries.size();
    if (!page || size <= 1)
        return;

    RefPtr<WebBackForwardListItem> currentItem = this->currentItem();

    if (!currentItem) {
        // We should only ever have no current item if we also have no current item index.
        ASSERT(!m_currentIndex);

        // But just in case it does happen in practice we should get back into a consistent state now.
        for (auto& entry : m_entries)
            didRemoveItem(entry);

        m_currentIndex = std::nullopt;
        page->didChangeBackForwardList(nullptr, std::exchange(m_entries, { }));

        return;
    }

    for (size_t i = 0; i < size; ++i) {
        if (m_entries[i].ptr() != currentItem)
            didRemoveItem(m_entries[i]);
    }

    Vector<Ref<WebBackForwardListItem>> removedItems;
    removedItems.reserveInitialCapacity(size - 1);
    for (size_t i = 0; i < size; ++i) {
        if (m_currentIndex && i != *m_currentIndex)
            removedItems.append(WTF::move(m_entries[i]));
    }

    m_currentIndex = 0;

    m_entries.clear();
    if (currentItem)
        m_entries.append(currentItem.releaseNonNull());
    else
        m_currentIndex = std::nullopt;
    page->didChangeBackForwardList(nullptr, WTF::move(removedItems));
}

BackForwardListState WebBackForwardList::backForwardListState(WTF::Function<bool (WebBackForwardListItem&)>&& filter) const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    BackForwardListState backForwardListState;
    if (m_currentIndex)
        backForwardListState.currentIndex = *m_currentIndex;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];

        if (filter && !filter(entry)) {
            auto& currentIndex = backForwardListState.currentIndex;
            if (currentIndex && i <= currentIndex.value() && currentIndex.value())
                --currentIndex.value();

            continue;
        }

        backForwardListState.items.append(entry->mainFrameState());
    }

    if (backForwardListState.items.isEmpty())
        backForwardListState.currentIndex = std::nullopt;
    else if (backForwardListState.items.size() <= backForwardListState.currentIndex.value())
        backForwardListState.currentIndex = backForwardListState.items.size() - 1;

    return backForwardListState;
}

static inline void setBackForwardItemIdentifiers(FrameState& frameState, BackForwardItemIdentifier itemID)
{
    frameState.itemID = itemID;
    frameState.frameItemID = BackForwardFrameItemIdentifier::generate();
    for (auto& child : frameState.children)
        setBackForwardItemIdentifiers(child, itemID);
}

void WebBackForwardList::restoreFromState(BackForwardListState backForwardListState)
{
    if (!m_page)
        return;

    // FIXME: Enable restoring resourceDirectoryURL.
    m_entries = WTF::map(WTF::move(backForwardListState.items), [this](auto&& state) {
        Ref stateCopy = state->copy();
        setBackForwardItemIdentifiers(stateCopy, BackForwardItemIdentifier::generate());
        m_currentIndex = m_entries.isEmpty() ? std::nullopt : std::optional(m_entries.size() - 1);
        // FIXME: navigatedFrameID will always be the main frame ID, causing the restored session state to be sent to an incorrect process when going back or forward with site isolation enabled.
        auto navigatedFrameID = stateCopy->frameID;
        return WebBackForwardListItem::create(WTF::move(stateCopy), m_page->identifier(), navigatedFrameID);
    });
    m_currentIndex = backForwardListState.currentIndex ? std::optional<size_t>(*backForwardListState.currentIndex) : std::nullopt;

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p restored from state (has %zu entries)", this, m_entries.size());
}

void WebBackForwardList::setItemsAsRestoredFromSession()
{
    setItemsAsRestoredFromSessionIf([](WebBackForwardListItem&) {
        return true;
    });
}

void WebBackForwardList::setItemsAsRestoredFromSessionIf(NOESCAPE Function<bool(WebBackForwardListItem&)>&& functor)
{
    for (auto& entry : m_entries) {
        if (functor(entry))
            entry->setWasRestoredFromSession();
    }
}

void WebBackForwardList::didRemoveItem(WebBackForwardListItem& backForwardListItem)
{
    backForwardListItem.wasRemovedFromBackForwardList();

    protect(m_page)->backForwardRemovedItem(backForwardListItem.mainFrameItem().identifier());

#if PLATFORM(COCOA) || PLATFORM(GTK)
    backForwardListItem.setSnapshot(nullptr);
#endif
}

enum class NavigationDirection { Backward, Forward };
static RefPtr<WebBackForwardListItem> itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(const WebBackForwardList& backForwardList, NavigationDirection direction)
{
    auto delta = direction == NavigationDirection::Backward ? -1 : 1;
    int itemIndex = delta;
    RefPtr item = backForwardList.itemAtIndex(itemIndex);
    if (!item)
        return nullptr;

#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::UIBackForwardSkipsHistoryItemsWithoutUserGesture))
        return item;
#endif

    // For example:
    // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
    // If we're on Google and navigate back, we don't want to skip anything and load Yahoo#a.
    // However, if we're on Yahoo and navigate forward, we do want to skip items and end up on Google#b.
    if (direction == NavigationDirection::Backward && !protect(backForwardList.currentItem())->wasCreatedByJSWithoutUserInteraction())
        return item;

    // For example:
    // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
    // If we are on Google#b and navigate backwards, we want to skip over Google#a and Google, to end up on Yahoo#a.
    // If we are on Yahoo#a and navigate forwards, we want to skip over Google and Google#a, to end up on Google#b.

    RefPtr originalItem = item;
    while (item->wasCreatedByJSWithoutUserInteraction()) {
        itemIndex += delta;
        item = backForwardList.itemAtIndex(itemIndex);
        if (!item)
            return originalItem;
        RELEASE_LOG(Loading, "UI Navigation is skipping a WebBackForwardListItem because it was added by JavaScript without user interaction");
    }

    // We are now on the next item that has user interaction.
    ASSERT(!item->wasCreatedByJSWithoutUserInteraction());

    if (direction == NavigationDirection::Backward) {
        // If going backwards, skip over next item with user iteraction since this is the one the user
        // thinks they're on.
        --itemIndex;
        item = backForwardList.itemAtIndex(itemIndex);
        if (!item)
            return originalItem;
        RELEASE_LOG(Loading, "UI Navigation is skipping a WebBackForwardListItem that has user interaction because we started on an item that didn't have interaction");
    } else {
        // If going forward and there are items that we created by JS without user interaction, move forward to the last
        // one in the series.
        RefPtr nextItem = backForwardList.itemAtIndex(itemIndex + 1);
        while (nextItem && nextItem->wasCreatedByJSWithoutUserInteraction())
            item = std::exchange(nextItem, backForwardList.itemAtIndex(++itemIndex));
    }
    return item;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::goBackItemSkippingItemsWithoutUserGesture() const
{
    return itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(*this, NavigationDirection::Backward);
}

RefPtr<WebBackForwardListItem> WebBackForwardList::goForwardItemSkippingItemsWithoutUserGesture() const
{
    return itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(*this, NavigationDirection::Forward);
}

static inline void setBackForwardItemIdentifier(FrameState& frameState, BackForwardItemIdentifier itemID)
{
    frameState.itemID = itemID;
    for (auto& child : frameState.children)
        setBackForwardItemIdentifier(child, itemID);
}

Ref<FrameState> WebBackForwardList::completeFrameStateForNavigation(Ref<FrameState>&& navigatedFrameState)
{
    RefPtr currentItem = this->currentItem();
    if (!currentItem)
        return navigatedFrameState;

    auto navigatedFrameID = navigatedFrameState->frameID;
    if (!navigatedFrameID)
        return navigatedFrameState;

    Ref mainFrameItem = currentItem->mainFrameItem();
    if (mainFrameItem->frameID() == navigatedFrameID)
        return navigatedFrameState;

    if (!mainFrameItem->childItemForFrameID(*navigatedFrameID))
        return navigatedFrameState;

    Ref frameState = currentItem->mainFrameState();
    setBackForwardItemIdentifier(frameState, *navigatedFrameState->itemID);
    frameState->replaceChildFrameState(WTF::move(navigatedFrameState));
    return frameState;
}

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())
#define MESSAGE_CHECK_COMPLETION(process, assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, process->connection(), completion)

void WebBackForwardList::backForwardAddItem(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState)
{
    if (RefPtr webPageProxy = m_page.get())
        backForwardAddItemShared(connection, WTF::move(navigatedFrameState), webPageProxy->didLoadWebArchive() ? LoadedWebArchive::Yes : LoadedWebArchive::No);
}

void WebBackForwardList::backForwardAddItemShared(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState, LoadedWebArchive loadedWebArchive)
{
    Ref process = WebProcessProxy::fromConnection(connection);

    URL itemURL { navigatedFrameState->urlString };
    URL itemOriginalURL { navigatedFrameState->originalURLString };
#if PLATFORM(COCOA)
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PushStateFilePathRestriction)
#if PLATFORM(MAC)
        && !WTF::MacApplication::isMimeoPhotoProject() // rdar://112445672.
#endif // PLATFORM(MAC)
    ) {
#endif // PLATFORM(COCOA)
        ASSERT(!itemURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemURL));
        MESSAGE_CHECK(process, !itemURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemURL));
        MESSAGE_CHECK(process, !itemOriginalURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemOriginalURL));
#if PLATFORM(COCOA)
    }
#endif

    if (RefPtr targetFrame = WebFrameProxy::webFrame(navigatedFrameState->frameID)) {
        if (targetFrame->isPendingInitialHistoryItem()) {
            targetFrame->setIsPendingInitialHistoryItem(false);
            if (RefPtr parent = targetFrame->parentFrame())
                addChildItem(parent->frameID(), WTF::move(navigatedFrameState));
            return;
        }
    } else
        return;

    if (RefPtr webPageProxy = m_page.get()) {

        auto navigatedFrameID = navigatedFrameState->frameID;
        Ref item = WebBackForwardListItem::create(completeFrameStateForNavigation(WTF::move(navigatedFrameState)), webPageProxy->identifier(), navigatedFrameID, protect(webPageProxy->browsingContextGroup()).ptr());
        item->setResourceDirectoryURL(webPageProxy->currentResourceDirectoryURL());
        item->setEnhancedSecurity(process->enhancedSecurity());
        if (loadedWebArchive == LoadedWebArchive::Yes)
            item->setDataStoreForWebArchive(process->websiteDataStore());
        addItem(WTF::move(item));
    }
}

void WebBackForwardList::backForwardSetChildItem(BackForwardFrameItemIdentifier frameItemID, Ref<FrameState>&& frameState)
{
    RefPtr item = currentItem();
    if (!item)
        return;

    if (RefPtr frameItem = WebBackForwardListFrameItem::itemForID(item->identifier(), frameItemID))
        frameItem->setChild(WTF::move(frameState));
}

void WebBackForwardList::backForwardClearChildren(BackForwardItemIdentifier itemID, BackForwardFrameItemIdentifier frameItemID)
{
    if (RefPtr frameItem = WebBackForwardListFrameItem::itemForID(itemID, frameItemID))
        frameItem->clearChildren();
}

void WebBackForwardList::backForwardUpdateItem(IPC::Connection& connection, Ref<FrameState>&& frameState)
{
    RefPtr frameItem = frameState->itemID && frameState->frameItemID ? WebBackForwardListFrameItem::itemForID(*frameState->itemID, *frameState->frameItemID) : nullptr;
    if (!frameItem)
        return;

    RefPtr item = frameItem->backForwardListItem();
    if (!item)
        return;

    if (RefPtr webPageProxy = m_page.get()) {
        ASSERT(webPageProxy->identifier() == item->pageID() && frameState->itemID == item->identifier());

        Ref process = *downcast<WebProcessProxy>(AuxiliaryProcessProxy::fromConnection(connection));
        if (!!item->backForwardCacheEntry() != frameState->hasCachedPage) {
            if (frameState->hasCachedPage)
                protect(webPageProxy->backForwardCache())->addEntry(*item, process->coreProcessIdentifier());
            else if (!item->suspendedPage())
                protect(webPageProxy->backForwardCache())->removeEntry(*item);
        }

        auto oldFrameID = frameItem->frameID();
        frameItem->setFrameState(WTF::move(frameState));
        auto newFrameID = frameItem->frameID();

        if (oldFrameID && newFrameID && oldFrameID != newFrameID)
            updateAllFrameIDs(*oldFrameID, *newFrameID);
    }
}

void WebBackForwardList::updateAllFrameIDs(FrameIdentifier oldFrameID, FrameIdentifier newFrameID)
{
    for (auto& entry : m_entries)
        entry->updateFrameID(oldFrameID, newFrameID);
}

void WebBackForwardList::backForwardGoToItem(BackForwardItemIdentifier itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    // On process swap, we tell the previous process to ignore the load, which causes it to restore its current back forward item to its previous
    // value. Since the load is really going on in a new provisional process, we want to ignore such requests from the committed process.
    // Any real new load in the committed process would have cleared m_provisionalPage.
    if (RefPtr webPageProxy = m_page.get()) {
        if (webPageProxy->hasProvisionalPage())
            return completionHandler(counts());
    }

    backForwardGoToItemShared(itemID, WTF::move(completionHandler));
}

void WebBackForwardList::backForwardListContainsItem(WebCore::BackForwardItemIdentifier itemID, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(itemForID(itemID));
}

void WebBackForwardList::backForwardGoToItemShared(BackForwardItemIdentifier itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    if (RefPtr webPageProxy = m_page.get())
        MESSAGE_CHECK_COMPLETION(protect(webPageProxy->legacyMainFrameProcess()), !WebKit::isInspectorPage(*webPageProxy), completionHandler(counts()));

    RefPtr item = itemForID(itemID);
    if (!item)
        return completionHandler(counts());

    goToItem(*item);
    completionHandler(counts());
}

void WebBackForwardList::backForwardAllItems(FrameIdentifier frameID, CompletionHandler<void(Vector<Ref<FrameState>>&&)>&& completionHandler)
{
    auto frameItems = WTF::compactMap(entries(), [frameID](const auto& item) -> RefPtr<WebBackForwardListFrameItem> {
        return protect(item->mainFrameItem())->childItemForFrameID(frameID);
    });

    completionHandler(WTF::map(WTF::move(frameItems), [](const auto& frameItem) {
        return frameItem->copyFrameStateWithChildren();
    }));
}

void WebBackForwardList::backForwardItemAtIndex(int32_t index, FrameIdentifier frameID, CompletionHandler<void(RefPtr<FrameState>&&)>&& completionHandler)
{
    // FIXME: This should verify that the web process requesting the item hosts the specified frame.
    if (RefPtr item = itemAtIndex(index)) {
        if (RefPtr frameItem = protect(item->mainFrameItem())->childItemForFrameID(frameID))
            return completionHandler(frameItem->copyFrameStateWithChildren());
        completionHandler(item->mainFrameState());
    } else
        completionHandler(nullptr);
}

void WebBackForwardList::backForwardListCounts(CompletionHandler<void(WebBackForwardListCounts&&)>&& completionHandler)
{
    completionHandler(counts());
}

String WebBackForwardList::loggingString()
{
    StringBuilder builder;

    builder.append("\nWebBackForwardList 0x"_s, hex(reinterpret_cast<uintptr_t>(this)), " - "_s, m_entries.size(), " entries, has current index "_s, m_currentIndex ? "YES"_s : "NO"_s, " ("_s, m_currentIndex ? *m_currentIndex : 0, ")\n"_s);

    for (size_t i = 0; i < m_entries.size(); ++i) {
        Ref entry = m_entries[i];
        ASCIILiteral prefix = (m_currentIndex && *m_currentIndex == i) ? " * "_s : " - "_s;
        auto entryString = entry->loggingString();
        builder.append(prefix, entryString);
    }

    return builder.toString();
}

#else // ENABLE(BACK_FORWARD_LIST_SWIFT)

WebBackForwardListWrapper::WebBackForwardListWrapper(WebPageProxy& webPageProxy)
    : m_impl(WTF::makeUniqueWithoutFastMallocCheck<WebBackForwardList>(WebBackForwardList::init(webPageProxy)))
{
}

WebBackForwardListWrapper::~WebBackForwardListWrapper()
{
}

WebBackForwardListItem* WebBackForwardListWrapper::currentItem() const
{
    return m_impl->currentItem();
}

WebBackForwardListItem* WebBackForwardListWrapper::backItem() const
{
    return m_impl->backItem();
}

WebBackForwardListItem* WebBackForwardListWrapper::forwardItem() const
{
    return m_impl->forwardItem();
}

WebBackForwardListItem* WebBackForwardListWrapper::itemAtIndex(int index) const
{
    return m_impl->itemAtIndex(index);
}

unsigned WebBackForwardListWrapper::backListCount() const
{
    return m_impl->backListCount();
}

unsigned WebBackForwardListWrapper::forwardListCount() const
{
    return m_impl->forwardListCount();
}

Ref<API::Array> WebBackForwardListWrapper::backList() const
{
    return backListAsAPIArrayWithLimit(backListCount());
}

Ref<API::Array> WebBackForwardListWrapper::forwardList() const
{
    return forwardListAsAPIArrayWithLimit(forwardListCount());
}

Ref<API::Array> WebBackForwardListWrapper::backListAsAPIArrayWithLimit(unsigned limit) const
{
    return m_impl->backListAsAPIArrayWithLimit(limit);
}

Ref<API::Array> WebBackForwardListWrapper::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    return m_impl->forwardListAsAPIArrayWithLimit(limit);
}

void WebBackForwardListWrapper::removeAllItems()
{
    m_impl->removeAllItems();
}

void WebBackForwardListWrapper::clear()
{
    m_impl->clear();
}

String WebBackForwardListWrapper::loggingString()
{
    return String::fromUTF8WithLatin1Fallback(std::string(m_impl->loggingString()));
}

#endif // ENABLE(BACK_FORWARD_LIST_SWIFT)

} // namespace WebKit

#if ENABLE(BACK_FORWARD_LIST_SWIFT)

WebCore::BackForwardFrameItemIdentifier generateBackForwardFrameItemIdentifier()
{
    return WebCore::BackForwardFrameItemIdentifier::generate();
}

WebCore::BackForwardItemIdentifier generateBackForwardItemIdentifier()
{
    return WebCore::BackForwardItemIdentifier::generate();
}

// rdar://168139823 is the task of doing a productionized version of WebKit Swift logging
void doLog(const char* WTF_NONNULL msg)
{
    LOG(BackForward, "%s", msg);
}

void doLoadingReleaseLog(const char* WTF_NONNULL msg)
{
    RELEASE_LOG(Loading, "%s", msg);
}
// rdar://168139740 is the task of doing a productionized Swift MESSAGE_CHECK
void messageCheckFailed(Ref<WebKit::WebProcessProxy> process)
{
    MESSAGE_CHECK_BASE(false, process->connection());
}

#endif // ENABLE(BACK_FORWARD_LIST_SWIFT)
