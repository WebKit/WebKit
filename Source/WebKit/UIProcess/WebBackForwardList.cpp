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
#include <wtf/Borrow.h>
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=306415
#include "WebKit-Swift.h"

namespace WebKit {
using namespace WebCore;

static inline void setBackForwardItemIdentifiers(FrameState& frameState, BackForwardItemIdentifier itemID)
{
    frameState.itemID = itemID;
    frameState.frameItemID = BackForwardFrameItemIdentifier::generate();
    for (auto& child : frameState.children)
        setBackForwardItemIdentifiers(child, itemID);
}

#if !ENABLE(BACK_FORWARD_LIST_SWIFT)

static bool shouldSkipItemsWithoutUserGestureForWebKitAPI()
{
#if PLATFORM(COCOA)
    return linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::AllBackForwardItemsWithoutUserGestureInvisibleToUI);
#endif
    return false;
}

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

    RefPtr parentItem = currentItem->mainFrameItem().childItemForFrameID(parentFrameID);
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

RefPtr<WebBackForwardListItem> WebBackForwardList::backItem() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());
    if (!m_page || !m_currentIndex)
        return nullptr;

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI())
        return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Backward, *m_currentIndex).first;

    return *m_currentIndex ? m_entries[*m_currentIndex - 1].ptr() : nullptr;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::forwardItem() const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());
    if (!m_page || !m_currentIndex)
        return nullptr;

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI())
        return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Forward, *m_currentIndex).first;

    return m_entries.size() && *m_currentIndex < m_entries.size() - 1 ? m_entries[*m_currentIndex + 1].ptr() : nullptr;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::itemAtDeltaFromCurrentIndex(int delta, AllowSkippingBackForwardItems allowSkippingBackForwardItems) const
{
    if (!m_currentIndex || (int)*m_currentIndex + delta < 0)
        return nullptr;

    // API requests to get the current item will always get the current item without any skipping logic.
    if (!delta)
        return itemAtIndexWithoutSkipping(*m_currentIndex).first;

    if (allowSkippingBackForwardItems == AllowSkippingBackForwardItems::No || !shouldSkipItemsWithoutUserGestureForWebKitAPI())
        return itemAtIndexWithoutSkipping(*m_currentIndex + delta).first;

    auto direction = delta < 0 ? NavigationDirection::Backward : NavigationDirection::Forward;
    size_t stepsLeft = abs(delta);
    size_t nextIndex = *m_currentIndex;
    while (stepsLeft) {
        auto item = itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(direction, nextIndex);
        if (!item.first || !--stepsLeft)
            return item.first;
        nextIndex = item.second;
    }

    return nullptr;
}

std::pair<RefPtr<WebBackForwardListItem>, size_t> WebBackForwardList::itemAtIndexWithoutSkipping(size_t index) const
{
    if (!m_page)
        return { nullptr, index };

    if (index >= m_entries.size())
        return { nullptr, index };

    return { { m_entries[index] }, index };
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

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI()) {
        Vector<RefPtr<API::Object>> vector;

        size_t nextStartingIndex = *m_currentIndex;
        while (backListSize) {
            auto item = itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Backward, nextStartingIndex);
            if (item.first)
                vector.append(item.first);

            if (!item.first || !--backListSize || !item.second)
                break;
            nextStartingIndex = item.second;
        }
        vector.reverse();

        return API::Array::create(WTF::move(vector));
    }

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

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI()) {
        Vector<RefPtr<API::Object>> vector;

        size_t nextStartingIndex = *m_currentIndex;
        while (size) {
            auto item = itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Forward, nextStartingIndex);
            if (item.first)
                vector.append(item.first);

            if (!item.first || !--size || !item.second)
                break;
            nextStartingIndex = item.second;
        }

        return API::Array::create(WTF::move(vector));
    }

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

    Borrow entries = m_entries;
    for (size_t i = 0; i < entries->size(); ++i) {
        auto& entry = entries.get()[i];

        if (filter && !filter(entry)) {
            auto& currentIndex = backForwardListState.currentIndex;
            if (currentIndex && i <= currentIndex.value() && currentIndex.value())
                --currentIndex.value();

            continue;
        }

        backForwardListState.items.append({ entry->copyMainFrameStateWithChildren(), entry->navigatedFrameID() });
    }

    if (backForwardListState.items.isEmpty())
        backForwardListState.currentIndex = std::nullopt;
    else if (backForwardListState.items.size() <= backForwardListState.currentIndex.value())
        backForwardListState.currentIndex = backForwardListState.items.size() - 1;

    return backForwardListState;
}

void WebBackForwardList::restoreFromState(BackForwardListState backForwardListState)
{
    if (!m_page)
        return;

    // FIXME: Enable restoring resourceDirectoryURL.
    m_entries = WTF::map(WTF::move(backForwardListState.items), [this](auto&& itemState) {
        Ref stateCopy = itemState.frameState->copy();
        setBackForwardItemIdentifiers(stateCopy, BackForwardItemIdentifier::generate());
        m_currentIndex = m_entries.isEmpty() ? std::nullopt : std::optional(m_entries.size() - 1);
        return WebBackForwardListItem::create(WTF::move(stateCopy), m_page->identifier(), itemState.navigatedFrameID);
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

std::pair<RefPtr<WebBackForwardListItem>, size_t> WebBackForwardList::itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection direction, size_t startingIndex) const
{
    if (direction == NavigationDirection::Backward && !startingIndex)
        return { nullptr, 0 };

    auto delta = direction == NavigationDirection::Backward ? -1 : 1;
    size_t itemIndex = startingIndex + delta;

    if (itemIndex >= m_entries.size())
        return { nullptr, 0 };

    auto startingItem = itemAtIndexWithoutSkipping(startingIndex);
    RELEASE_ASSERT(startingItem.first);

    auto item = itemAtIndexWithoutSkipping(itemIndex);

#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::UIBackForwardSkipsHistoryItemsWithoutUserGesture))
        return item;
#endif

    if (!item.first)
        return item;

    // For example:
    // A -> A#a (no userInteraction) -> B -> B#a (no user interaction) -> B#b (no user interaction)
    // If we're on B and navigate back, we don't want to skip anything and load A#a.
    // However, if we're on A and navigate forward, we do want to skip items and end up on B#b.
    // The forward logic comes later.
    if (direction == NavigationDirection::Backward && !startingItem.first->wasCreatedByJSWithoutUserInteraction())
        return item;

    // If every item from this point back to the start of the list was created by JS without user interaction,
    // we ignore them all.
    if (direction == NavigationDirection::Backward && startingItem.first->wasCreatedByJSWithoutUserInteraction()) {
        auto innerItem = item;
        while (innerItem.first->wasCreatedByJSWithoutUserInteraction()) {
            if (innerItem.second)
                innerItem = itemAtIndexWithoutSkipping(innerItem.second - 1);
            else
                return { };
            ASSERT(innerItem.first);
        }
    }

    // For example:
    // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
    // If we are on Google#b and navigate backwards, we want to skip over Google#a and Google, to end up on Yahoo#a.
    // If we are on Yahoo#a and navigate forwards, we want to skip over Google and Google#a, to end up on Google#b.
    auto originalitem = item;
    while (item.first->wasCreatedByJSWithoutUserInteraction()) {
        itemIndex += delta;
        item = itemAtIndexWithoutSkipping(itemIndex);
        if (!item.first) {
            // If there are no more back items that ever had a user gesture, then we should not enable going back.
            // This happens when e.g. a new window is created by JavaScript then client redirects occur that create
            // a sequence of history items, each without user interaction.
            RELEASE_LOG(Loading, "UI Navigation is disabling going back because no more WebBackForwardListItem items in the back list had user interaction");
            return { };
        }

        RELEASE_LOG(Loading, "UI Navigation is skipping a WebBackForwardListItem because it was added by JavaScript without user interaction");
    }

    // We are now on the next item that has user interaction.
    ASSERT(!item.first->wasCreatedByJSWithoutUserInteraction());

    if (direction == NavigationDirection::Backward) {
        // If going backwards, skip over next item with user iteraction since this is the one the user
        // thinks they're on.
        --itemIndex;
        item = itemAtIndexWithoutSkipping(itemIndex);
        if (!item.first)
            return originalitem;
        RELEASE_LOG(Loading, "UI Navigation is skipping a WebBackForwardListItem that has user interaction because we started on an item that didn't have interaction");
    } else {
        // If going forward and there are items that we created by JS without user interaction, move forward to the last
        // one in the series.
        auto nextItem = itemAtIndexWithoutSkipping(itemIndex + 1);
        while (nextItem.first && nextItem.first->wasCreatedByJSWithoutUserInteraction())
            item = std::exchange(nextItem, itemAtIndexWithoutSkipping(++itemIndex));
    }
    return item;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::goBackItemSkippingItemsWithoutUserGesture() const
{
    if (!m_currentIndex || !*m_currentIndex)
        return nullptr;
    return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Backward, *m_currentIndex).first;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::goForwardItemSkippingItemsWithoutUserGesture() const
{
    if (!m_currentIndex || *m_currentIndex >= m_entries.size())
        return nullptr;
    return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Forward, *m_currentIndex).first;
}

static inline void NODELETE setBackForwardItemIdentifier(FrameState& frameState, BackForwardItemIdentifier itemID)
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

    Ref frameState = currentItem->copyMainFrameStateWithChildren();
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
            item->setDataStoreForWebArchive(protect(process->websiteDataStore()));
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
            updateFrameIdentifier(*oldFrameID, *newFrameID);

        webPageProxy->updateCanGoBackAndForward();
    }
}

void WebBackForwardList::updateFrameIdentifier(FrameIdentifier oldFrameID, FrameIdentifier newFrameID)
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
        MESSAGE_CHECK_COMPLETION(Ref { webPageProxy->legacyMainFrameProcess() }, !WebKit::isInspectorPage(*webPageProxy), completionHandler(counts()));

    RefPtr item = itemForID(itemID);
    if (!item)
        return completionHandler(counts());

    goToItem(*item);
    completionHandler(counts());
}

void WebBackForwardList::backForwardAllItems(FrameIdentifier frameID, CompletionHandler<void(Vector<Ref<FrameState>>&&)>&& completionHandler)
{
    auto frameItems = WTF::compactMap(entries(), [frameID](const auto& item) -> RefPtr<WebBackForwardListFrameItem> {
        return item->mainFrameItem().childItemForFrameID(frameID);
    });

    completionHandler(WTF::map(WTF::move(frameItems), [](const auto& frameItem) {
        return frameItem->copyFrameStateWithChildren();
    }));
}

void WebBackForwardList::backForwardItemAtIndexForWebContent(int32_t delta, FrameIdentifier frameID, CompletionHandler<void(RefPtr<FrameState>&&)>&& completionHandler)
{
    // FIXME: This should verify that the web process requesting the item hosts the specified frame.
    if (RefPtr item = itemAtDeltaFromCurrentIndex(delta, AllowSkippingBackForwardItems::No)) {
        if (RefPtr frameItem = item->mainFrameItem().childItemForFrameID(frameID))
            return completionHandler(frameItem->copyFrameStateWithChildren());
        completionHandler(item->copyMainFrameStateWithChildren());
    } else
        completionHandler(nullptr);
}

void WebBackForwardList::backForwardListCounts(CompletionHandler<void(WebBackForwardListCounts&&)>&& completionHandler)
{
    completionHandler(counts());
}

FrameState* WebBackForwardList::findFrameStateInItem(WebCore::BackForwardItemIdentifier itemID, WebCore::FrameIdentifier parentFrameID, uint64_t childFrameIndex)
{
    RefPtr targetItem = itemForID(itemID);
    if (!targetItem)
        return nullptr;

    RefPtr parentFrameItem = targetItem->mainFrameItem().childItemForFrameID(parentFrameID);
    if (!parentFrameItem) {
        // FIXME: After session restore, the back/forward list's frame identifiers don't match
        // the current WebView's frames because the original identifiers are unavailable.
        // Fall back to the mainFrameItem if the parentFrameID isn't found.
        // This only works correctly for direct children of the main frame; nested frames
        // (e.g., subframe > nestedframe) will get the wrong FrameState.
        parentFrameItem = &targetItem->mainFrameItem();
    }

    RefPtr childFrameItem = parentFrameItem->childItemAtIndex(childFrameIndex);
    if (!childFrameItem)
        return nullptr;

    return &childFrameItem->frameState();
}

String WebBackForwardList::loggingString() const
{
    StringBuilder builder;

    String currentIndexString = m_currentIndex ? String::number(*m_currentIndex) : String::number(-1);
    builder.append("\nWebBackForwardList 0x"_s, hex(reinterpret_cast<uintptr_t>(this)), " - "_s, m_entries.size(), " entries, currentIndex is "_s, currentIndexString, "\n"_s);

    for (size_t i = 0; i < m_entries.size(); ++i) {
        Ref entry = m_entries[i];
        String itemIdentifier = entry->identifier().loggingString();
        auto entryString = entry->loggingString();
        builder.append(String::number(i), " - ItemID:"_s, itemIdentifier, ", "_s, entryString);
    }

    return builder.toString();
}

#else // ENABLE(BACK_FORWARD_LIST_SWIFT)

WebBackForwardListWrapper::WebBackForwardListWrapper(WebPageProxy& webPageProxy)
    : m_impl(WTF::makeUniqueWithoutFastMallocCheck<WebBackForwardList>(WebBackForwardList::init(webPageProxy)))
{
}

WebBackForwardListWrapper::~WebBackForwardListWrapper() = default;

WebBackForwardListItem* WebBackForwardListWrapper::currentItem() const
{
    return m_impl->currentItem();
}

RefPtr<WebBackForwardListItem> WebBackForwardListWrapper::backItem() const
{
    return m_impl->backItem();
}

RefPtr<WebBackForwardListItem> WebBackForwardListWrapper::forwardItem() const
{
    return m_impl->forwardItem();
}

RefPtr<WebBackForwardListItem> WebBackForwardListWrapper::itemAtDeltaFromCurrentIndex(int index, AllowSkippingBackForwardItems allowSkipping) const
{
    return m_impl->itemAtDeltaFromCurrentIndex(index, allowSkipping == AllowSkippingBackForwardItems::Yes ? true : false);
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

// rdar://168139823 is the task of doing a productionized version of WebKit Swift logging
void doLog(const WTF::String& msg)
{
    LOG(BackForward, "%s", msg.utf8().data());
}

void doLoadingReleaseLog(const WTF::String& msg)
{
    RELEASE_LOG(Loading, "%s", msg.utf8().data());
}
// rdar://168139740 is the task of doing a productionized Swift MESSAGE_CHECK
void messageCheckFailed(Ref<WebKit::WebProcessProxy> process)
{
    MESSAGE_CHECK_BASE(false, process->connection());
}

// Workarounds for rdar://171011011
void appendToBackForwardStateItems(Vector<WebKit::BackForwardListItemState>& items, const WebKit::WebBackForwardListItem& entry)
{
    items.append({ entry.copyMainFrameStateWithChildren(), entry.navigatedFrameID() });
}

Ref<WebKit::WebBackForwardListItem> createItemFromState(const WebKit::BackForwardListItemState& itemState, WebKit::WebPageProxyIdentifier pageIdentifier)
{
    Ref stateCopy = itemState.frameState->copy();
    setBackForwardItemIdentifiers(stateCopy, WebCore::BackForwardItemIdentifier::generate());
    return WebKit::WebBackForwardListItem::create(WTF::move(stateCopy), pageIdentifier, itemState.navigatedFrameID);
}


#endif // ENABLE(BACK_FORWARD_LIST_SWIFT)
