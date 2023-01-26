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
#include "Logging.h"
#include "SessionState.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardListCounts.h"
#include "WebPageProxy.h"
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebKit {
using namespace WebCore;

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

WebBackForwardListItem* WebBackForwardList::itemForID(const BackForwardItemIdentifier& identifier)
{
    if (!m_page)
        return nullptr;

    auto* item = WebBackForwardListItem::itemForID(identifier);
    if (!item)
        return nullptr;

    ASSERT(item->pageID() == m_page->identifier());
    return item;
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

    m_page = nullptr;
    m_entries.clear();
    m_currentIndex = std::nullopt;
}

void WebBackForwardList::addItem(Ref<WebBackForwardListItem>&& newItem)
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    if (!m_page)
        return;

    Vector<Ref<WebBackForwardListItem>> removedItems;
    
    if (m_currentIndex) {
        m_page->recordAutomaticNavigationSnapshot();

        // Toss everything in the forward list.
        unsigned targetSize = *m_currentIndex + 1;
        removedItems.reserveInitialCapacity(m_entries.size() - targetSize);
        while (m_entries.size() > targetSize) {
            didRemoveItem(m_entries.last());
            removedItems.uncheckedAppend(WTFMove(m_entries.last()));
            m_entries.removeLast();
        }

        // Toss the first item if the list is getting too big, as long as we're not using it
        // (or even if we are, if we only want 1 entry).
        if (m_entries.size() >= DefaultCapacity && (*m_currentIndex)) {
            didRemoveItem(m_entries[0]);
            removedItems.append(WTFMove(m_entries[0]));
            m_entries.remove(0);

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
            removedItems.append(WTFMove(m_entries[i]));
        }
        m_entries.clear();
    }

    bool shouldKeepCurrentItem = true;

    if (!m_currentIndex) {
        ASSERT(m_entries.isEmpty());
        m_currentIndex = 0;
    } else {
        shouldKeepCurrentItem = m_page->shouldKeepCurrentBackForwardListItemInList(m_entries[*m_currentIndex]);
        if (shouldKeepCurrentItem)
            ++*m_currentIndex;
    }

    auto* newItemPtr = newItem.ptr();
    if (!shouldKeepCurrentItem) {
        // m_current should never be pointing past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to swap in the new item.
        ASSERT(*m_currentIndex < m_entries.size());

        removedItems.append(m_entries[*m_currentIndex].copyRef());
        m_entries[*m_currentIndex] = WTFMove(newItem);
    } else {
        // m_current should never be pointing more than 1 past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to insert the new item.
        ASSERT(*m_currentIndex <= m_entries.size());

        if (*m_currentIndex <= m_entries.size())
            m_entries.insert(*m_currentIndex, WTFMove(newItem));
    }

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p added an item. Current size %zu, current index %zu, threw away %zu items", this, m_entries.size(), *m_currentIndex, removedItems.size());
    m_page->didChangeBackForwardList(newItemPtr, WTFMove(removedItems));
}

void WebBackForwardList::goToItem(WebBackForwardListItem& item)
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    if (!m_entries.size() || !m_page || !m_currentIndex)
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
        LOG(BackForward, "(Back/Forward) WebBackForwardList %p could not go to item %s (%s) because it was not found", this, item.itemID().toString().utf8().data(), item.url().utf8().data());
        return;
    }

    if (targetIndex < *m_currentIndex) {
        unsigned delta = m_entries.size() - targetIndex - 1;
        String deltaValue = delta > 10 ? "over10"_s : String::number(delta);
        m_page->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::backNavigationDeltaKey(), deltaValue, ShouldSample::No);
    }

    // If we're going to an item different from the current item, ask the client if the current
    // item should remain in the list.
    auto& currentItem = m_entries[*m_currentIndex];
    bool shouldKeepCurrentItem = true;
    if (currentItem.ptr() != &item) {
        m_page->recordAutomaticNavigationSnapshot();
        shouldKeepCurrentItem = m_page->shouldKeepCurrentBackForwardListItemInList(m_entries[*m_currentIndex]);
    }

    // If the client said to remove the current item, remove it and then update the target index.
    Vector<Ref<WebBackForwardListItem>> removedItems;
    if (!shouldKeepCurrentItem) {
        removedItems.append(currentItem.copyRef());
        m_entries.remove(*m_currentIndex);
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

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p going to item %s, is now at index %zu", this, item.itemID().toString().utf8().data(), targetIndex);
    m_page->didChangeBackForwardList(nullptr, WTFMove(removedItems));
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

    Vector<RefPtr<API::Object>> vector;
    vector.reserveInitialCapacity(size);

    ASSERT(backListSize >= size);
    for (unsigned i = backListSize - size; i < backListSize; ++i)
        vector.uncheckedAppend(m_entries[i].ptr());

    return API::Array::create(WTFMove(vector));
}

Ref<API::Array> WebBackForwardList::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    if (!m_page || !m_currentIndex)
        return API::Array::create();

    unsigned size = std::min(static_cast<unsigned>(forwardListCount()), limit);
    if (!size)
        return API::Array::create();

    Vector<RefPtr<API::Object>> vector;
    vector.reserveInitialCapacity(size);

    size_t last = *m_currentIndex + size;
    ASSERT(last < m_entries.size());
    for (size_t i = *m_currentIndex + 1; i <= last; ++i)
        vector.uncheckedAppend(m_entries[i].ptr());

    return API::Array::create(WTFMove(vector));
}

void WebBackForwardList::removeAllItems()
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p removeAllItems (has %zu of them)", this, m_entries.size());

    for (auto& entry : m_entries)
        didRemoveItem(entry);

    m_currentIndex = std::nullopt;
    m_page->didChangeBackForwardList(nullptr, std::exchange(m_entries, { }));
}

void WebBackForwardList::clear()
{
    ASSERT(!m_currentIndex || *m_currentIndex < m_entries.size());

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p clear (has %zu of them)", this, m_entries.size());

    size_t size = m_entries.size();
    if (!m_page || size <= 1)
        return;

    RefPtr<WebBackForwardListItem> currentItem = this->currentItem();

    if (!currentItem) {
        // We should only ever have no current item if we also have no current item index.
        ASSERT(!m_currentIndex);

        // But just in case it does happen in practice we should get back into a consistent state now.
        for (auto& entry : m_entries)
            didRemoveItem(entry);

        m_currentIndex = std::nullopt;
        m_page->didChangeBackForwardList(nullptr, std::exchange(m_entries, { }));

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
            removedItems.uncheckedAppend(WTFMove(m_entries[i]));
    }

    m_currentIndex = 0;

    m_entries.clear();
    if (currentItem)
        m_entries.append(currentItem.releaseNonNull());
    else
        m_currentIndex = std::nullopt;
    m_page->didChangeBackForwardList(nullptr, WTFMove(removedItems));
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

        backForwardListState.items.append(entry->itemState());
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
    m_entries = WTF::map(WTFMove(backForwardListState.items), [this](auto&& state) {
        state.identifier = BackForwardItemIdentifier::generate();
        return WebBackForwardListItem::create(WTFMove(state), m_page->identifier());
    });
    m_currentIndex = backForwardListState.currentIndex ? std::optional<size_t>(*backForwardListState.currentIndex) : std::nullopt;

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p restored from state (has %zu entries)", this, m_entries.size());
}

Vector<BackForwardListItemState> WebBackForwardList::filteredItemStates(Function<bool(WebBackForwardListItem&)>&& functor) const
{
    Vector<BackForwardListItemState> itemStates;
    itemStates.reserveInitialCapacity(m_entries.size());

    for (const auto& entry : m_entries) {
        if (functor(entry))
            itemStates.uncheckedAppend(entry->itemState());
    }

    return itemStates;
}

Vector<BackForwardListItemState> WebBackForwardList::itemStates() const
{
    return filteredItemStates([](WebBackForwardListItem&) {
        return true;
    });
}

void WebBackForwardList::didRemoveItem(WebBackForwardListItem& backForwardListItem)
{
    backForwardListItem.wasRemovedFromBackForwardList();

    m_page->backForwardRemovedItem(backForwardListItem.itemID());

#if PLATFORM(COCOA) || PLATFORM(GTK)
    backForwardListItem.setSnapshot(nullptr);
#endif
}

enum class NavigationDirection { Backward, Forward };
static WebBackForwardListItem* itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(const WebBackForwardList& backForwardList, NavigationDirection direction)
{
    auto delta = direction == NavigationDirection::Backward ? -1 : 1;
    int itemIndex = delta;
    auto* item = backForwardList.itemAtIndex(itemIndex);
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
    if (direction == NavigationDirection::Backward && !backForwardList.currentItem()->wasCreatedByJSWithoutUserInteraction())
        return item;

    // For example:
    // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
    // If we are on Google#b and navigate backwards, we want to skip over Google#a and Google, to end up on Yahoo#a.
    // If we are on Yahoo#a and navigate forwards, we want to skip over Google and Google#a, to end up on Google#b.

    auto* originalItem = item;
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
        auto* nextItem = backForwardList.itemAtIndex(itemIndex + 1);
        while (nextItem && nextItem->wasCreatedByJSWithoutUserInteraction()) {
            item = nextItem;
            nextItem = backForwardList.itemAtIndex(++itemIndex);
        }
    }
    return item;
}

WebBackForwardListItem* WebBackForwardList::goBackItemSkippingItemsWithoutUserGesture() const
{
    return itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(*this, NavigationDirection::Backward);
}

WebBackForwardListItem* WebBackForwardList::goForwardItemSkippingItemsWithoutUserGesture() const
{
    return itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(*this, NavigationDirection::Forward);
}

#if !LOG_DISABLED

const char* WebBackForwardList::loggingString()
{
    StringBuilder builder;

    builder.append("WebBackForwardList 0x", hex(reinterpret_cast<uintptr_t>(this)), " - ", m_entries.size(), " entries, has current index ", m_currentIndex ? "YES" : "NO", " (", m_currentIndex ? *m_currentIndex : 0, ')');

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const char* prefix;
        if (m_currentIndex && *m_currentIndex == i)
            prefix = " * ";
        else
            prefix = " - ";
        builder.append('\n', prefix, m_entries[i]->loggingString());
    }

    return debugString("\n", builder.toString());
}

#endif // !LOG_DISABLED

} // namespace WebKit
