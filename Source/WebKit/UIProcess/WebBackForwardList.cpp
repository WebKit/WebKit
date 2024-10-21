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
#include "WebBackForwardListFrameItem.h"
#include "WebFrameProxy.h"
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
    ASSERT((!m_page && !provisionalOrCurrentIndex()) || !m_page->hasRunningProcess());
}

WebBackForwardListItem* WebBackForwardList::itemForID(const BackForwardItemIdentifier& identifier)
{
    if (!m_page)
        return nullptr;

    RefPtr frameItem = WebBackForwardListFrameItem::itemForID(identifier);
    if (!frameItem)
        return nullptr;

    auto* item = frameItem->backForwardListItem();
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

    m_page.clear();
    m_entries.clear();
    m_currentIndex = std::nullopt;
    m_provisionalIndex = std::nullopt;
}

void WebBackForwardList::addItem(Ref<WebBackForwardListItem>&& newItem)
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    RefPtr page = m_page.get();
    if (!page)
        return;

    Vector<Ref<WebBackForwardListItem>> removedItems;
    
    if (provisionalOrCurrentIndex()) {
        page->recordAutomaticNavigationSnapshot();

        // Toss everything in the forward list.
        unsigned targetSize = *provisionalOrCurrentIndex() + 1;
        removedItems.reserveInitialCapacity(m_entries.size() - targetSize);
        while (m_entries.size() > targetSize) {
            didRemoveItem(m_entries.last());
            removedItems.append(WTFMove(m_entries.last()));
            m_entries.removeLast();
        }

        // Toss the first item if the list is getting too big, as long as we're not using it
        // (or even if we are, if we only want 1 entry).
        if (m_entries.size() >= DefaultCapacity && (*provisionalOrCurrentIndex())) {
            didRemoveItem(m_entries[0]);
            removedItems.append(WTFMove(m_entries[0]));
            m_entries.remove(0);

            if (m_entries.isEmpty()) {
                m_currentIndex = std::nullopt;
                m_provisionalIndex = std::nullopt;
            } else
                setProvisionalOrCurrentIndex(*provisionalOrCurrentIndex() - 1);
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

    if (!provisionalOrCurrentIndex()) {
        ASSERT(m_entries.isEmpty());
        m_currentIndex = 0;
    } else {
        shouldKeepCurrentItem = page->shouldKeepCurrentBackForwardListItemInList(m_entries[*provisionalOrCurrentIndex()]);
        if (shouldKeepCurrentItem)
            setProvisionalOrCurrentIndex(*provisionalOrCurrentIndex() + 1);
    }

    auto* newItemPtr = newItem.ptr();
    if (!shouldKeepCurrentItem) {
        // m_current should never be pointing past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to swap in the new item.
        ASSERT(*provisionalOrCurrentIndex() < m_entries.size());

        removedItems.append(m_entries[*provisionalOrCurrentIndex()].copyRef());
        m_entries[*provisionalOrCurrentIndex()] = WTFMove(newItem);
    } else {
        // m_current should never be pointing more than 1 past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to insert the new item.
        ASSERT(*provisionalOrCurrentIndex() <= m_entries.size());

        if (*provisionalOrCurrentIndex() <= m_entries.size())
            m_entries.insert(*provisionalOrCurrentIndex(), WTFMove(newItem));
    }

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p added an item. Current size %zu, current index %zu, threw away %zu items", this, m_entries.size(), *provisionalOrCurrentIndex(), removedItems.size());
    page->didChangeBackForwardList(newItemPtr, WTFMove(removedItems));
}

void WebBackForwardList::goToItem(WebBackForwardListItem& item)
{
    if (m_provisionalIndex)
        m_currentIndex = std::exchange(m_provisionalIndex, std::nullopt);

    goToItemInternal(item, m_currentIndex);
}

void WebBackForwardList::goToProvisionalItem(WebBackForwardListItem& item)
{
    goToItemInternal(item, m_provisionalIndex);
}

void WebBackForwardList::goToItemInternal(WebBackForwardListItem& item, std::optional<size_t>& indexToUpdate)
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    RefPtr page = m_page.get();
    if (!m_entries.size() || !page || !provisionalOrCurrentIndex())
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

    if (targetIndex < *provisionalOrCurrentIndex()) {
        unsigned delta = m_entries.size() - targetIndex - 1;
        String deltaValue = delta > 10 ? "over10"_s : String::number(delta);
        page->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::backNavigationDeltaKey(), deltaValue, ShouldSample::No);
    }

    // If we're going to an item different from the current item, ask the client if the current
    // item should remain in the list.
    auto& currentItem = m_entries[*provisionalOrCurrentIndex()];
    bool shouldKeepCurrentItem = true;
    if (currentItem.ptr() != &item) {
        page->recordAutomaticNavigationSnapshot();
        shouldKeepCurrentItem = page->shouldKeepCurrentBackForwardListItemInList(m_entries[*provisionalOrCurrentIndex()]);
    }

    // If the client said to remove the current item, remove it and then update the target index.
    Vector<Ref<WebBackForwardListItem>> removedItems;
    if (!shouldKeepCurrentItem) {
        removedItems.append(currentItem.copyRef());
        m_entries.remove(*provisionalOrCurrentIndex());
        targetIndex = notFound;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].ptr() == &item) {
                targetIndex = i;
                break;
            }
        }
        ASSERT(targetIndex != notFound);
    }

    indexToUpdate = targetIndex;

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p going to item %s, is now at index %zu", this, item.itemID().toString().utf8().data(), targetIndex);
    page->didChangeBackForwardList(nullptr, WTFMove(removedItems));
}

void WebBackForwardList::clearProvisionalItem(WebBackForwardListFrameItem& frameItem)
{
    RefPtr item = frameItem.backForwardListItem();
    if (!item)
        return;

    if ((frameItem.parent() || item->isRemoteFrameNavigation()) && frameItem.frameID() != item->navigatedFrameID())
        return;

    m_provisionalIndex = std::nullopt;
}

WebBackForwardListItem* WebBackForwardList::currentItem() const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    return m_page && provisionalOrCurrentIndex() ? m_entries[*provisionalOrCurrentIndex()].ptr() : nullptr;
}

RefPtr<WebBackForwardListItem> WebBackForwardList::protectedCurrentItem() const
{
    return currentItem();
}

WebBackForwardListItem* WebBackForwardList::backItem() const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    return m_page && provisionalOrCurrentIndex() && *provisionalOrCurrentIndex() ? m_entries[*provisionalOrCurrentIndex() - 1].ptr() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::forwardItem() const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    return m_page && provisionalOrCurrentIndex() && m_entries.size() && *provisionalOrCurrentIndex() < m_entries.size() - 1 ? m_entries[*provisionalOrCurrentIndex() + 1].ptr() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::itemAtIndex(int index) const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    if (!provisionalOrCurrentIndex() || !m_page)
        return nullptr;
    
    // Do range checks without doing math on index to avoid overflow.
    if (index < 0 && static_cast<unsigned>(-index) > backListCount())
        return nullptr;
    
    if (index > 0 && static_cast<unsigned>(index) > forwardListCount())
        return nullptr;

    return m_entries[index + *provisionalOrCurrentIndex()].ptr();
}

unsigned WebBackForwardList::backListCount() const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    return m_page && provisionalOrCurrentIndex() ? *provisionalOrCurrentIndex() : 0;
}

unsigned WebBackForwardList::forwardListCount() const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    return m_page && provisionalOrCurrentIndex() ? m_entries.size() - (*provisionalOrCurrentIndex() + 1) : 0;
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
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    if (!m_page || !provisionalOrCurrentIndex())
        return API::Array::create();

    unsigned backListSize = static_cast<unsigned>(backListCount());
    unsigned size = std::min(backListSize, limit);
    if (!size)
        return API::Array::create();

    ASSERT(backListSize >= size);
    size_t startIndex = backListSize - size;
    Vector<RefPtr<API::Object>> vector(size, [&](size_t i) -> RefPtr<API::Object> {
        return m_entries[startIndex + i].ptr();
    });

    return API::Array::create(WTFMove(vector));
}

Ref<API::Array> WebBackForwardList::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    if (!m_page || !provisionalOrCurrentIndex())
        return API::Array::create();

    unsigned size = std::min(static_cast<unsigned>(forwardListCount()), limit);
    if (!size)
        return API::Array::create();

    size_t startIndex = *provisionalOrCurrentIndex() + 1;
    Vector<RefPtr<API::Object>> vector(size, [&](size_t i) -> RefPtr<API::Object> {
        return m_entries[startIndex + i].ptr();
    });

    return API::Array::create(WTFMove(vector));
}

void WebBackForwardList::removeAllItems()
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p removeAllItems (has %zu of them)", this, m_entries.size());

    for (auto& entry : m_entries)
        didRemoveItem(entry);

    m_currentIndex = std::nullopt;
    m_provisionalIndex = std::nullopt;
    protectedPage()->didChangeBackForwardList(nullptr, std::exchange(m_entries, { }));
}

void WebBackForwardList::clear()
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    LOG(BackForward, "(Back/Forward) WebBackForwardList %p clear (has %zu of them)", this, m_entries.size());

    RefPtr page = m_page.get();
    size_t size = m_entries.size();
    if (!page || size <= 1)
        return;

    RefPtr<WebBackForwardListItem> currentItem = this->currentItem();

    if (!currentItem) {
        // We should only ever have no current item if we also have no current item index.
        ASSERT(!provisionalOrCurrentIndex());

        // But just in case it does happen in practice we should get back into a consistent state now.
        for (auto& entry : m_entries)
            didRemoveItem(entry);

        m_currentIndex = std::nullopt;
        m_provisionalIndex = std::nullopt;
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
        if (provisionalOrCurrentIndex() && i != *provisionalOrCurrentIndex())
            removedItems.append(WTFMove(m_entries[i]));
    }

    m_currentIndex = 0;
    m_provisionalIndex = std::nullopt;

    m_entries.clear();
    if (currentItem)
        m_entries.append(currentItem.releaseNonNull());
    else {
        m_currentIndex = std::nullopt;
        m_provisionalIndex = std::nullopt;
    }
    page->didChangeBackForwardList(nullptr, WTFMove(removedItems));
}

BackForwardListState WebBackForwardList::backForwardListState(WTF::Function<bool (WebBackForwardListItem&)>&& filter) const
{
    ASSERT(!provisionalOrCurrentIndex() || *provisionalOrCurrentIndex() < m_entries.size());

    BackForwardListState backForwardListState;
    if (provisionalOrCurrentIndex())
        backForwardListState.currentIndex = *provisionalOrCurrentIndex();

    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];

        if (filter && !filter(entry)) {
            auto& currentIndex = backForwardListState.currentIndex;
            if (currentIndex && i <= currentIndex.value() && currentIndex.value())
                --currentIndex.value();

            continue;
        }

        backForwardListState.items.append(entry->rootFrameState());
    }

    if (backForwardListState.items.isEmpty())
        backForwardListState.currentIndex = std::nullopt;
    else if (backForwardListState.items.size() <= backForwardListState.currentIndex.value())
        backForwardListState.currentIndex = backForwardListState.items.size() - 1;

    return backForwardListState;
}

static inline void setBackForwardItemIdentifiers(FrameState& frameState)
{
    frameState.identifier = BackForwardItemIdentifier::generate();
    for (auto& child : frameState.children)
        setBackForwardItemIdentifiers(child);
}

void WebBackForwardList::restoreFromState(BackForwardListState backForwardListState)
{
    if (!m_page)
        return;

    // FIXME: Enable restoring resourceDirectoryURL.
    m_entries = WTF::map(WTFMove(backForwardListState.items), [this](auto&& state) {
        Ref stateCopy = state->copy();
        setBackForwardItemIdentifiers(stateCopy);
        return WebBackForwardListItem::create(WTFMove(stateCopy), m_page->identifier());
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

void WebBackForwardList::setItemsAsRestoredFromSessionIf(Function<bool(WebBackForwardListItem&)>&& functor)
{
    for (auto& entry : m_entries) {
        if (functor(entry))
            entry->setWasRestoredFromSession();
    }
}

void WebBackForwardList::didRemoveItem(WebBackForwardListItem& backForwardListItem)
{
    backForwardListItem.wasRemovedFromBackForwardList();

    protectedPage()->backForwardRemovedItem(backForwardListItem.itemID());

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

RefPtr<WebPageProxy> WebBackForwardList::protectedPage()
{
    return m_page.get();
}

void WebBackForwardList::setProvisionalOrCurrentIndex(size_t index)
{
    if (m_provisionalIndex) {
        m_provisionalIndex = index;
        return;
    }
    m_currentIndex = index;
}

#if !LOG_DISABLED

String WebBackForwardList::loggingString()
{
    StringBuilder builder;

    builder.append("\nWebBackForwardList 0x"_s, hex(reinterpret_cast<uintptr_t>(this)), " - "_s, m_entries.size(), " entries, has current index "_s, provisionalOrCurrentIndex() ? "YES"_s : "NO"_s, " ("_s, provisionalOrCurrentIndex() ? *provisionalOrCurrentIndex() : 0, ')');

    for (size_t i = 0; i < m_entries.size(); ++i) {
        ASCIILiteral prefix;
        if (provisionalOrCurrentIndex() && *provisionalOrCurrentIndex() == i)
            prefix = " * "_s;
        else
            prefix = " - "_s;
        builder.append('\n', prefix, m_entries[i]->loggingString());
    }

    return builder.toString();
}

#endif // !LOG_DISABLED

} // namespace WebKit
