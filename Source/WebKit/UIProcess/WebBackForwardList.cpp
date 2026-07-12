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
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
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
    ASSERT((!m_page && !m_activeList) || !m_page->hasRunningProcess());
}

const BackForwardListItemVector& WebBackForwardList::activeEntries() const
{
    static NeverDestroyed<BackForwardListItemVector> empty;
    return m_activeList ? m_activeList->entries() : empty.get();
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
    LOG(BackForward, "(Back/Forward) WebBackForwardList %p had its page closed with current size %zu", this, m_activeList ? m_activeList->entries().size() : 0);

    // We should have always started out with an m_page and we should never close the page twice.
    ASSERT(m_page);

    if (m_activeList) {
        for (auto& entry : m_activeList->entries())
            didRemoveItem(entry);
    }

    m_page.clear();
    m_activeList = std::nullopt;
}

void WebBackForwardList::addItem(Ref<WebBackForwardListItem>&& newItem)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto* newItemPtr = newItem.ptr();
    Vector<Ref<WebBackForwardListItem>> removedItems;

    if (m_activeList) {
        page->recordAutomaticNavigationSnapshot();

        // Toss everything in the forward list.
        while (m_activeList->entries().size() > m_activeList->currentIndex() + 1) {
            auto removed = m_activeList->removeLast();
            didRemoveItem(removed);
            removedItems.append(WTF::move(removed));
        }

        // Toss the first item if the list is getting too big, as long as we're not using it
        // (or even if we are, if we only want 1 entry).
        if (m_activeList->entries().size() >= DefaultCapacity && m_activeList->currentIndex()) {
            auto removed = m_activeList->removeFirst();
            didRemoveItem(removed);
            removedItems.append(WTF::move(removed));
        }

        bool shouldKeepCurrentItem = page->shouldKeepCurrentBackForwardListItemInList(m_activeList->entries()[m_activeList->currentIndex()]);
        if (shouldKeepCurrentItem)
            m_activeList->advanceAndInsert(WTF::move(newItem));
        else
            removedItems.append(m_activeList->replaceCurrentEntry(WTF::move(newItem)));
    } else {
        BackForwardListItemVector entries;
        entries.append(WTF::move(newItem));
        m_activeList.emplace(WTF::move(entries), 0);
    }

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p added an item. Current size %zu, current index %zu, threw away %zu items", this, m_activeList->entries().size(), m_activeList->currentIndex(), removedItems.size());
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
    RefPtr page = m_page.get();
    if (!m_activeList || !page)
        return;

    size_t targetIndex = notFound;
    for (size_t i = 0; i < m_activeList->entries().size(); ++i) {
        if (m_activeList->entries()[i].ptr() == &item) {
            targetIndex = i;
            break;
        }
    }

    // If the target item wasn't even in the list, there's nothing else to do.
    if (targetIndex == notFound) {
        LOG(BackForward, "(Back/Forward) WebBackForwardList %p could not go to item %s (%s) because it was not found", this, item.identifier().toString().utf8().data(), item.url().utf8().data());
        return;
    }

    if (targetIndex < m_activeList->currentIndex()) {
        unsigned delta = m_activeList->entries().size() - targetIndex - 1;
        String deltaValue = delta > 10 ? "over10"_s : String::number(delta);
        page->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::backNavigationDeltaKey(), deltaValue, ShouldSample::No);
    }

    // If we're going to an item different from the current item, ask the client if the current
    // item should remain in the list.
    bool shouldKeepCurrentItem = true;
    if (m_activeList->entries()[m_activeList->currentIndex()].ptr() != &item) {
        page->recordAutomaticNavigationSnapshot();
        shouldKeepCurrentItem = page->shouldKeepCurrentBackForwardListItemInList(m_activeList->entries()[m_activeList->currentIndex()]);
    }

    // If the client said to remove the current item, remove it and then update the target index.
    Vector<Ref<WebBackForwardListItem>> removedItems;
    if (!shouldKeepCurrentItem)
        removedItems.append(m_activeList->removeCurrentEntry(targetIndex));
    else
        m_activeList->moveToIndex(targetIndex);

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p going to item %s, is now at index %zu", this, item.identifier().toString().utf8().data(), m_activeList->currentIndex());
    page->didChangeBackForwardList(nullptr, WTF::move(removedItems));
}

WebBackForwardListItem* WebBackForwardList::currentItem() const
{
    return m_page && m_activeList ? m_activeList->entries()[m_activeList->currentIndex()].ptr() : nullptr;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::backItem() const
{
    if (!m_page || !m_activeList)
        return nullptr;

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI())
        return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Backward, m_activeList->currentIndex()).first;

    return m_activeList->currentIndex() ? m_activeList->entries()[m_activeList->currentIndex() - 1].ptr() : nullptr;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::forwardItem() const
{
    if (!m_page || !m_activeList)
        return nullptr;

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI())
        return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Forward, m_activeList->currentIndex()).first;

    return m_activeList->entries().size() && m_activeList->currentIndex() < m_activeList->entries().size() - 1 ? m_activeList->entries()[m_activeList->currentIndex() + 1].ptr() : nullptr;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::itemAtDeltaFromCurrentIndex(int delta, AllowSkippingBackForwardItems allowSkippingBackForwardItems) const
{
    if (!m_activeList)
        return nullptr;

    // Do range checks without doing math on delta to avoid overflow.
    if (delta < 0 && -static_cast<unsigned>(delta) > m_activeList->currentIndex())
        return nullptr;

    // API requests to get the current item will always get the current item without any skipping logic.
    if (!delta)
        return itemAtIndexWithoutSkipping(m_activeList->currentIndex()).first;

    if (allowSkippingBackForwardItems == AllowSkippingBackForwardItems::No || !shouldSkipItemsWithoutUserGestureForWebKitAPI())
        return itemAtIndexWithoutSkipping(m_activeList->currentIndex() + delta).first;

    auto direction = delta < 0 ? NavigationDirection::Backward : NavigationDirection::Forward;
    size_t stepsLeft = abs(delta);
    size_t nextIndex = m_activeList->currentIndex();
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
    if (!m_page || !m_activeList)
        return { nullptr, index };

    if (index >= m_activeList->entries().size())
        return { nullptr, index };

    return { { m_activeList->entries()[index] }, index };
}

unsigned WebBackForwardList::rawBackListEntryCount() const
{
    return m_page && m_activeList ? m_activeList->currentIndex() : 0;
}

unsigned WebBackForwardList::rawForwardListEntryCount() const
{
    return m_page && m_activeList ? m_activeList->entries().size() - (m_activeList->currentIndex() + 1) : 0;
}

unsigned WebBackForwardList::backListCountForAPI() const
{
    auto listInfo = backListWithLimitInternal(rawBackListEntryCount(), MakeAPIArray::No);
    return listInfo.first;
}

unsigned WebBackForwardList::forwardListCountForAPI() const
{
    auto listInfo = forwardListWithLimitInternal(rawForwardListEntryCount(), MakeAPIArray::No);
    return listInfo.first;
}

WebBackForwardListCounts WebBackForwardList::rawCounts() const
{
    return WebBackForwardListCounts { rawBackListEntryCount(), rawForwardListEntryCount() };
}

Ref<API::Array> WebBackForwardList::backList() const
{
    return backListAsAPIArrayWithLimit(rawBackListEntryCount());
}

Ref<API::Array> WebBackForwardList::forwardList() const
{
    return forwardListAsAPIArrayWithLimit(rawForwardListEntryCount());
}

Ref<API::Array> WebBackForwardList::backListAsAPIArrayWithLimit(unsigned limit) const
{
    auto listInfo = backListWithLimitInternal(limit, MakeAPIArray::Yes);
    RELEASE_ASSERT(listInfo.second);
    return listInfo.second.releaseNonNull();
}

Ref<API::Array> WebBackForwardList::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    auto listInfo = forwardListWithLimitInternal(limit, MakeAPIArray::Yes);
    RELEASE_ASSERT(listInfo.second);
    return listInfo.second.releaseNonNull();
}

static std::pair<unsigned, RefPtr<API::Array>> makeListPairResult(Vector<RefPtr<API::Object>>&& vector, WebBackForwardList::MakeAPIArray makeAPIArray)
{
    std::pair<unsigned, RefPtr<API::Array>> result;
    result.first = vector.size();
    if (makeAPIArray == WebBackForwardList::MakeAPIArray::Yes)
        result.second = vector.size() ? API::Array::create(WTF::move(vector)) : API::Array::create();

    return result;
}

std::pair<unsigned, RefPtr<API::Array>> WebBackForwardList::backListWithLimitInternal(unsigned limit, MakeAPIArray makeAPIArray) const
{
    if (!m_page || !m_activeList)
        return makeListPairResult({ }, makeAPIArray);

    unsigned backListSize = rawBackListEntryCount();
    unsigned size = std::min(backListSize, limit);
    if (!size)
        return makeListPairResult({ }, makeAPIArray);

    ASSERT(backListSize >= size);

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI()) {
        Vector<RefPtr<API::Object>> vector;

        size_t nextStartingIndex = m_activeList->currentIndex();
        while (size) {
            auto item = itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Backward, nextStartingIndex);
            if (item.first)
                vector.append(item.first);

            if (!item.first || !--size || !item.second)
                break;
            nextStartingIndex = item.second;
        }
        vector.reverse();

        return makeListPairResult(WTF::move(vector), makeAPIArray);
    }

    size_t startIndex = backListSize - size;
    Vector<RefPtr<API::Object>> vector(size, [&](size_t i) -> RefPtr<API::Object> {
        // FIXME: Remove SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE when the false positive
        // in the static analyzer is fixed.
        SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE return m_activeList->entries()[startIndex + i].ptr();
    });

    return makeListPairResult(WTF::move(vector), makeAPIArray);
}

std::pair<unsigned, RefPtr<API::Array>> WebBackForwardList::forwardListWithLimitInternal(unsigned limit, MakeAPIArray makeAPIArray) const
{
    if (!m_page || !m_activeList)
        return makeListPairResult({ }, makeAPIArray);

    unsigned size = std::min(rawForwardListEntryCount(), limit);
    if (!size)
        return makeListPairResult({ }, makeAPIArray);

    if (shouldSkipItemsWithoutUserGestureForWebKitAPI()) {
        Vector<RefPtr<API::Object>> vector;

        size_t nextStartingIndex = m_activeList->currentIndex();
        while (size) {
            auto item = itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Forward, nextStartingIndex);
            if (item.first)
                vector.append(item.first);

            if (!item.first || !--size || !item.second)
                break;
            nextStartingIndex = item.second;
        }

        return makeListPairResult(WTF::move(vector), makeAPIArray);
    }

    size_t startIndex = m_activeList->currentIndex() + 1;
    Vector<RefPtr<API::Object>> vector(size, [&](size_t i) -> RefPtr<API::Object> {
        return m_activeList->entries()[startIndex + i].ptr();
    });

    return makeListPairResult(WTF::move(vector), makeAPIArray);
}

void WebBackForwardList::removeAllItems()
{
    LOG(BackForward, "(Back/Forward) WebBackForwardList %p removeAllItems (has %zu of them)", this, m_activeList ? m_activeList->entries().size() : 0);

    if (m_activeList) {
        for (auto& entry : m_activeList->entries())
            didRemoveItem(entry);
    }

    auto removedEntries = m_activeList ? m_activeList->takeEntries() : BackForwardListItemVector();
    m_activeList = std::nullopt;
    protect(m_page)->didChangeBackForwardList(nullptr, WTF::move(removedEntries));
}

void WebBackForwardList::clear()
{
    LOG(BackForward, "(Back/Forward) WebBackForwardList %p clear (has %zu of them)", this, m_activeList ? m_activeList->entries().size() : 0);

    RefPtr page = m_page.get();
    if (!m_activeList || !page || m_activeList->entries().size() <= 1)
        return;

    size_t currentIdx = m_activeList->currentIndex();
    size_t size = m_activeList->entries().size();

    for (size_t i = 0; i < size; ++i) {
        if (i != currentIdx)
            didRemoveItem(m_activeList->entries()[i]);
    }

    // Capture current item and build removed list before modifying m_activeList.
    Ref<WebBackForwardListItem> current = m_activeList->entries()[currentIdx].copyRef();
    Vector<Ref<WebBackForwardListItem>> removedItems;
    removedItems.reserveInitialCapacity(size - 1);
    for (size_t i = 0; i < size; ++i) {
        if (i != currentIdx)
            removedItems.append(m_activeList->entries()[i].copyRef());
    }

    BackForwardListItemVector newEntries;
    newEntries.append(WTF::move(current));
    m_activeList.emplace(WTF::move(newEntries), 0);
    page->didChangeBackForwardList(nullptr, WTF::move(removedItems));
}

BackForwardListState WebBackForwardList::backForwardListState(WTF::Function<bool (WebBackForwardListItem&)>&& filter) const
{
    BackForwardListState backForwardListState;
    if (m_activeList)
        backForwardListState.currentIndex = m_activeList->currentIndex();

    auto& entries = activeEntries();
    for (size_t i = 0; i < entries.size(); ++i) {
        auto& entry = entries[i];

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
    auto entries = WTF::map(WTF::move(backForwardListState.items), [this](auto&& itemState) {
        Ref stateCopy = itemState.frameState->copy();
        setBackForwardItemIdentifiers(stateCopy, BackForwardItemIdentifier::generate());
        return WebBackForwardListItem::create(WTF::move(stateCopy), m_page->identifier(), itemState.navigatedFrameID);
    });

    if (entries.isEmpty() || !backForwardListState.currentIndex) {
        // rdar://172071119: If entries exist without a current index, it's unclear whether
        // this is a programming error or valid state from the web process. Discard entries to
        // ensure consistent state.
        m_activeList = std::nullopt;
    } else {
        size_t clampedIndex = std::min(static_cast<size_t>(*backForwardListState.currentIndex), entries.size() - 1);
        m_activeList.emplace(WTF::move(entries), clampedIndex);
    }

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p restored from state (has %zu entries)", this, m_activeList ? m_activeList->entries().size() : 0);
}

void WebBackForwardList::setItemsAsRestoredFromSession()
{
    setItemsAsRestoredFromSessionIf([](WebBackForwardListItem&) {
        return true;
    });
}

void WebBackForwardList::setItemsAsRestoredFromSessionIf(NOESCAPE Function<bool(WebBackForwardListItem&)>&& functor)
{
    for (auto& entry : activeEntries()) {
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

    if (itemIndex >= activeEntries().size())
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
        // thinks they're on. But if the user-gesture item is at the start of history, there is nothing
        // to skip to — the item itself must be the destination.
        if (!itemIndex)
            return item;
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
    if (!m_activeList || !m_activeList->currentIndex())
        return nullptr;
    return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Backward, m_activeList->currentIndex()).first;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::goForwardItemSkippingItemsWithoutUserGesture() const
{
    if (!m_activeList || m_activeList->currentIndex() >= m_activeList->entries().size())
        return nullptr;
    return itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection::Forward, m_activeList->currentIndex()).first;
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
#define MESSAGE_CHECK_WITH_RETURN_VALUE(process, assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, process->connection(), returnValue)

void WebBackForwardList::backForwardAddItem(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState)
{
    if (RefPtr webPageProxy = m_page.get())
        backForwardAddItemShared(connection, WTF::move(navigatedFrameState), webPageProxy->didLoadWebArchive() ? LoadedWebArchive::Yes : LoadedWebArchive::No);
}

static bool messageCheckItemURLs(Ref<FrameState>& frameState, Ref<WebProcessProxy>& process)
{
    URL itemURL { frameState->urlString };
    URL itemOriginalURL { frameState->originalURLString };
#if PLATFORM(COCOA)
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PushStateFilePathRestriction)
#if PLATFORM(MAC)
        && !WTF::MacApplication::isMimeoPhotoProject() // rdar://112445672.
#endif // PLATFORM(MAC)
    ) {
#endif // PLATFORM(COCOA)
        MESSAGE_CHECK_WITH_RETURN_VALUE(process, !itemURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemURL), false);
        MESSAGE_CHECK_WITH_RETURN_VALUE(process, !itemOriginalURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemOriginalURL), false);
#if PLATFORM(COCOA)
    }
#endif
    return true;
}

void WebBackForwardList::backForwardAddItemShared(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState, LoadedWebArchive loadedWebArchive)
{
    Ref process = WebProcessProxy::fromConnection(connection);

    MESSAGE_CHECK(process, !navigatedFrameState->itemID || navigatedFrameState->itemID->processIdentifier() == process->coreProcessIdentifier());
    MESSAGE_CHECK(process, !navigatedFrameState->frameItemID || navigatedFrameState->frameItemID->processIdentifier() == process->coreProcessIdentifier());

    if (!messageCheckItemURLs(navigatedFrameState, process))
        return;

    if (RefPtr targetFrame = WebFrameProxy::webFrame(navigatedFrameState->frameID)) {
        MESSAGE_CHECK(process, targetFrame->page() == m_page.get());
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

void WebBackForwardList::backForwardSetChildItem(IPC::Connection& connection, BackForwardFrameItemIdentifier frameItemID, Ref<FrameState>&& frameState)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    if (!messageCheckItemURLs(frameState, process))
        return;

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
    Ref process = WebProcessProxy::fromConnection(connection);

    // In the case of a process swap, the `backForwardUpdateItem` message can be received from the old process,
    // and therefore present an unexpected file: URL.
    // We can safely skip the message check in these cases.
    if (!m_handlingProvisionalMessage && !messageCheckItemURLs(frameState, process))
        return;

    RefPtr frameItem = frameState->itemID && frameState->frameItemID ? WebBackForwardListFrameItem::itemForID(*frameState->itemID, *frameState->frameItemID) : nullptr;
    if (!frameItem)
        return;

    RefPtr item = frameItem->backForwardListItem();
    if (!item)
        return;

    if (RefPtr webPageProxy = m_page.get()) {
        MESSAGE_CHECK(process, webPageProxy->identifier() == item->pageID() && frameState->itemID == item->identifier());

        auto oldFrameID = frameItem->frameID();
        frameItem->updateFrameStatePayload(WTF::move(frameState));
        auto newFrameID = frameItem->frameID();

        if (oldFrameID && newFrameID && oldFrameID != newFrameID)
            updateFrameIdentifier(*oldFrameID, *newFrameID);

        webPageProxy->updateCanGoBackAndForward();
    }
}

void WebBackForwardList::updateFrameIdentifier(FrameIdentifier oldFrameID, FrameIdentifier newFrameID)
{
    for (auto& entry : activeEntries())
        entry->updateFrameID(oldFrameID, newFrameID);
}

void WebBackForwardList::replaceFrameStateForChild(WebBackForwardListItem& item, WebCore::FrameIdentifier frameID, Ref<FrameState>&& newFrameState)
{
    RefPtr targetFrameItem = item.mainFrameItem().childItemForFrameID(frameID);
    if (!targetFrameItem)
        return;

    targetFrameItem->updateFrameStatePayload(WTF::move(newFrameState));
}

void WebBackForwardList::backForwardGoToItem(BackForwardItemIdentifier itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    // On process swap, we tell the previous process to ignore the load, which causes it to restore its current back forward item to its previous
    // value. Since the load is really going on in a new provisional process, we want to ignore such requests from the committed process.
    // Any real new load in the committed process would have cleared m_provisionalPage.
    if (RefPtr webPageProxy = m_page.get()) {
        if (webPageProxy->hasProvisionalPage())
            return completionHandler(rawCounts());
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
        MESSAGE_CHECK_COMPLETION(Ref { webPageProxy->legacyMainFrameProcess() }, !WebKit::isInspectorPage(*webPageProxy), completionHandler(rawCounts()));

    RefPtr item = itemForID(itemID);
    if (!item)
        return completionHandler(rawCounts());

    goToItem(*item);
    completionHandler(rawCounts());
}

void WebBackForwardList::backForwardAllItems(FrameIdentifier frameID, CompletionHandler<void(Vector<Ref<FrameState>>&&)>&& completionHandler)
{
    auto frameItems = WTF::compactMap(activeEntries(), [frameID](const auto& item) -> RefPtr<WebBackForwardListFrameItem> {
        return item->mainFrameItem().childItemForFrameID(frameID);
    });

    completionHandler(WTF::map(WTF::move(frameItems), [](const auto& frameItem) {
        return frameItem->copyFrameStateWithChildren();
    }));
}

void WebBackForwardList::backForwardItemAtIndexForWebContent(IPC::Connection& connection, int32_t delta, FrameIdentifier frameID, CompletionHandler<void(RefPtr<FrameState>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(delta != std::numeric_limits<int32_t>::min(), connection, completionHandler(nullptr));

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
    completionHandler(rawCounts());
}

FrameState* WebBackForwardList::findFrameStateInItem(WebCore::BackForwardItemIdentifier itemID, WebCore::FrameIdentifier parentFrameID, WebCore::FrameIdentifier childFrameID, uint64_t childFrameIndex)
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

    RefPtr childFrameItem = parentFrameItem->childItemForFrameID(childFrameID);
    if (!childFrameItem) {
        // The identifier is absent after session restore or cross-site child-frame recreation; fall back to position.
        childFrameItem = parentFrameItem->childItemAtIndex(childFrameIndex);
    }
    if (!childFrameItem)
        return nullptr;

    return &childFrameItem->frameState();
}

String WebBackForwardList::loggingString() const
{
    StringBuilder builder;

    String currentIndexString = m_activeList ? String::number(m_activeList->currentIndex()) : String::number(-1);
    auto& entries = activeEntries();
    builder.append("\nWebBackForwardList 0x"_s, hex(reinterpret_cast<uintptr_t>(this)), " - "_s, entries.size(), " entries, currentIndex is "_s, currentIndexString, "\n"_s);

    for (size_t i = 0; i < entries.size(); ++i) {
        Ref entry = entries[i];
        String itemIdentifier = entry->identifier().loggingString();
        auto entryString = entry->loggingString();
        builder.append(String::number(i), " - ItemID:"_s, itemIdentifier, ", "_s, entryString);
    }

    return builder.toString();
}

void WebBackForwardList::didReceiveProvisionalMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    SetForScope scope(m_handlingProvisionalMessage, true);
    didReceiveMessage(connection, decoder);
}

#else // ENABLE(BACK_FORWARD_LIST_SWIFT)

WebBackForwardListWrapper::WebBackForwardListWrapper(WebPageProxy& webPageProxy)
    : m_impl(WTF::makeUniqueWithoutFastMallocCheck<WebBackForwardList>(WebBackForwardList::init(webPageProxy)))
    , m_messageForwarder(m_impl->getMessageReceiver())
{
}

WebBackForwardListWrapper::~WebBackForwardListWrapper() = default;

WebBackForwardListMessageForwarder& WebBackForwardListWrapper::messageReceiver() const
{
    return m_messageForwarder.get();
}

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

unsigned WebBackForwardListWrapper::backListCountForAPI() const
{
    return m_impl->backListCountForAPI();
}

unsigned WebBackForwardListWrapper::forwardListCountForAPI() const
{
    return m_impl->forwardListCountForAPI();
}

Ref<API::Array> WebBackForwardListWrapper::backList() const
{
    return backListAsAPIArrayWithLimit(backListCountForAPI());
}

Ref<API::Array> WebBackForwardListWrapper::forwardList() const
{
    return forwardListAsAPIArrayWithLimit(forwardListCountForAPI());
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

void setFrameStateBackForwardItemIdentifier(WebKit::FrameState& frameState, const WebCore::BackForwardItemIdentifier& itemID)
{
    frameState.itemID = itemID;
    for (auto& child : frameState.children)
        setFrameStateBackForwardItemIdentifier(child, itemID);
}

Ref<WebKit::WebBackForwardListItem> createItemFromState(const WebKit::BackForwardListItemState& itemState, WebKit::WebPageProxyIdentifier pageIdentifier)
{
    Ref stateCopy = itemState.frameState->copy();
    setBackForwardItemIdentifiers(stateCopy, WebCore::BackForwardItemIdentifier::generate());
    return WebKit::WebBackForwardListItem::create(WTF::move(stateCopy), pageIdentifier, itemState.navigatedFrameID);
}

Vector<Ref<WebKit::WebBackForwardListItem>> createItemsFromState(const WebKit::BackForwardListState& state, WebKit::WebPageProxyIdentifier pageIdentifier)
{
    Vector<Ref<WebKit::WebBackForwardListItem>> items;
    items.reserveInitialCapacity(state.items.size());
    for (auto& itemState : state.items)
        items.append(createItemFromState(itemState, pageIdentifier));
    return items;
}

WebKit::WebBackForwardListItem* itemAtIndexInBackForwardListItemVector(const Vector<Ref<WebKit::WebBackForwardListItem>>& items, size_t index)
{
    return items[index].ptr();
}


#endif // ENABLE(BACK_FORWARD_LIST_SWIFT)
