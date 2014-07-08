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
#include "SessionState.h"
#include "WebPageProxy.h"

namespace WebKit {

// FIXME: Make this static once WebBackForwardListCF.cpp is no longer using it.
uint64_t generateWebBackForwardItemID();

uint64_t generateWebBackForwardItemID()
{
    // These IDs exist in the UIProcess for items created by the UIProcess.
    // The IDs generated here need to never collide with the IDs created in WebBackForwardListProxy in the WebProcess.
    // We accomplish this by starting from 2, and only ever using even ids.
    static uint64_t uniqueHistoryItemID = 0;
    uniqueHistoryItemID += 2;
    return uniqueHistoryItemID;
}

static const unsigned DefaultCapacity = 100;

WebBackForwardList::WebBackForwardList(WebPageProxy& page)
    : m_page(&page)
    , m_hasCurrentIndex(false)
    , m_currentIndex(0)
    , m_capacity(DefaultCapacity)
{
}

WebBackForwardList::~WebBackForwardList()
{
    // A WebBackForwardList should never be destroyed unless it's associated page has been closed or is invalid.
    ASSERT((!m_page && !m_hasCurrentIndex) || !m_page->isValid());
}

void WebBackForwardList::pageClosed()
{
    // We should have always started out with an m_page and we should never close the page twice.
    ASSERT(m_page);

    if (m_page) {
        size_t size = m_entries.size();
        for (size_t i = 0; i < size; ++i) {
            ASSERT(m_entries[i]);
            if (!m_entries[i])
                continue;
            m_page->backForwardRemovedItem(m_entries[i]->itemID());
        }
    }

    m_page = 0;
    m_entries.clear();
    m_hasCurrentIndex = false;
}

void WebBackForwardList::addItem(WebBackForwardListItem* newItem)
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    if (!m_capacity || !newItem || !m_page)
        return;

    Vector<RefPtr<WebBackForwardListItem>> removedItems;
    
    if (m_hasCurrentIndex) {
        // Toss everything in the forward list.
        unsigned targetSize = m_currentIndex + 1;
        removedItems.reserveCapacity(m_entries.size() - targetSize);
        while (m_entries.size() > targetSize) {
            m_page->backForwardRemovedItem(m_entries.last()->itemID());
            removedItems.append(m_entries.last().release());
            m_entries.removeLast();
        }

        // Toss the first item if the list is getting too big, as long as we're not using it
        // (or even if we are, if we only want 1 entry).
        if (m_entries.size() == m_capacity && (m_currentIndex || m_capacity == 1)) {
            m_page->backForwardRemovedItem(m_entries[0]->itemID());
            removedItems.append(m_entries[0].release());
            m_entries.remove(0);

            if (m_entries.isEmpty())
                m_hasCurrentIndex = false;
            else
                m_currentIndex--;
        }
    } else {
        // If we have no current item index we should also not have any entries.
        ASSERT(m_entries.isEmpty());

        // But just in case it does happen in practice we'll get back in to a consistent state now before adding the new item.
        size_t size = m_entries.size();
        for (size_t i = 0; i < size; ++i) {
            ASSERT(m_entries[i]);
            if (!m_entries[i])
                continue;
            m_page->backForwardRemovedItem(m_entries[i]->itemID());
            removedItems.append(m_entries[i].release());
        }
        m_entries.clear();
    }

    bool shouldKeepCurrentItem = true;

    if (!m_hasCurrentIndex) {
        ASSERT(m_entries.isEmpty());
        m_currentIndex = 0;
        m_hasCurrentIndex = true;
    } else {
        shouldKeepCurrentItem = m_page->shouldKeepCurrentBackForwardListItemInList(m_entries[m_currentIndex].get());
        if (shouldKeepCurrentItem)
            m_currentIndex++;
    }

    if (!shouldKeepCurrentItem) {
        // m_current should never be pointing past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to swap in the new item.
        ASSERT(m_currentIndex < m_entries.size());

        removedItems.append(m_entries[m_currentIndex]);
        m_entries[m_currentIndex] = newItem;
    } else {
        // m_current should never be pointing more than 1 past the end of the entries Vector.
        // If it is, something has gone wrong and we should not try to insert the new item.
        ASSERT(m_currentIndex <= m_entries.size());

        if (m_currentIndex <= m_entries.size())
            m_entries.insert(m_currentIndex, newItem);
    }

    m_page->didChangeBackForwardList(newItem, WTF::move(removedItems));
}

void WebBackForwardList::goToItem(WebBackForwardListItem* item)
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    if (!m_entries.size() || !item || !m_page || !m_hasCurrentIndex)
        return;

    size_t targetIndex = m_entries.find(item);

    // If the target item wasn't even in the list, there's nothing else to do.
    if (targetIndex == notFound)
        return;

    // If we're going to an item different from the current item, ask the client if the current
    // item should remain in the list.
    WebBackForwardListItem* currentItem = m_entries[m_currentIndex].get();
    bool shouldKeepCurrentItem = true;
    if (currentItem != item)
        shouldKeepCurrentItem = m_page->shouldKeepCurrentBackForwardListItemInList(m_entries[m_currentIndex].get());

    // If the client said to remove the current item, remove it and then update the target index.
    Vector<RefPtr<WebBackForwardListItem>> removedItems;
    if (!shouldKeepCurrentItem) {
        removedItems.append(currentItem);
        m_entries.remove(m_currentIndex);
        targetIndex = m_entries.find(item);

        ASSERT(targetIndex != notFound);
    }

    m_currentIndex = targetIndex;
    m_page->didChangeBackForwardList(nullptr, removedItems);
}

WebBackForwardListItem* WebBackForwardList::currentItem() const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    return m_page && m_hasCurrentIndex ? m_entries[m_currentIndex].get() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::backItem() const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    return m_page && m_hasCurrentIndex && m_currentIndex ? m_entries[m_currentIndex - 1].get() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::forwardItem() const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    return m_page && m_hasCurrentIndex && m_entries.size() && m_currentIndex < m_entries.size() - 1 ? m_entries[m_currentIndex + 1].get() : nullptr;
}

WebBackForwardListItem* WebBackForwardList::itemAtIndex(int index) const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    if (!m_hasCurrentIndex || !m_page)
        return nullptr;
    
    // Do range checks without doing math on index to avoid overflow.
    if (index < -backListCount())
        return nullptr;
    
    if (index > forwardListCount())
        return nullptr;
        
    return m_entries[index + m_currentIndex].get();
}

int WebBackForwardList::backListCount() const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    return m_page && m_hasCurrentIndex ? m_currentIndex : 0;
}

int WebBackForwardList::forwardListCount() const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    return m_page && m_hasCurrentIndex ? m_entries.size() - (m_currentIndex + 1) : 0;
}

PassRefPtr<API::Array> WebBackForwardList::backList() const
{
    return backListAsAPIArrayWithLimit(backListCount());
}

PassRefPtr<API::Array> WebBackForwardList::forwardList() const
{
    return forwardListAsAPIArrayWithLimit(forwardListCount());
}

PassRefPtr<API::Array> WebBackForwardList::backListAsAPIArrayWithLimit(unsigned limit) const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    if (!m_page || !m_hasCurrentIndex)
        return API::Array::create();

    unsigned backListSize = static_cast<unsigned>(backListCount());
    unsigned size = std::min(backListSize, limit);
    if (!size)
        return API::Array::create();

    Vector<RefPtr<API::Object>> vector;
    vector.reserveInitialCapacity(size);

    ASSERT(backListSize >= size);
    for (unsigned i = backListSize - size; i < backListSize; ++i) {
        ASSERT(m_entries[i]);
        vector.uncheckedAppend(m_entries[i].get());
    }

    return API::Array::create(WTF::move(vector));
}

PassRefPtr<API::Array> WebBackForwardList::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    if (!m_page || !m_hasCurrentIndex)
        return API::Array::create();

    unsigned size = std::min(static_cast<unsigned>(forwardListCount()), limit);
    if (!size)
        return API::Array::create();

    Vector<RefPtr<API::Object>> vector;
    vector.reserveInitialCapacity(size);

    unsigned last = m_currentIndex + size;
    ASSERT(last < m_entries.size());
    for (unsigned i = m_currentIndex + 1; i <= last; ++i) {
        ASSERT(m_entries[i]);
        vector.uncheckedAppend(m_entries[i].get());
    }

    return API::Array::create(WTF::move(vector));
}

void WebBackForwardList::removeAllItems()
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    Vector<RefPtr<WebBackForwardListItem>> removedItems;

    for (auto& entry : m_entries) {
        ASSERT(entry);
        if (!entry)
            continue;

        m_page->backForwardRemovedItem(entry->itemID());
        removedItems.append(WTF::move(entry));
    }

    m_entries.clear();
    m_hasCurrentIndex = false;
    m_page->didChangeBackForwardList(nullptr, WTF::move(removedItems));
}

void WebBackForwardList::clear()
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    size_t size = m_entries.size();
    if (!m_page || size <= 1)
        return;

    RefPtr<WebBackForwardListItem> currentItem = this->currentItem();
    Vector<RefPtr<WebBackForwardListItem>> removedItems;

    if (!currentItem) {
        // We should only ever have no current item if we also have no current item index.
        ASSERT(!m_hasCurrentIndex);

        // But just in case it does happen in practice we should get back into a consistent state now.
        for (size_t i = 0; i < size; ++i) {
            ASSERT(m_entries[i]);
            if (!m_entries[i])
                continue;

            m_page->backForwardRemovedItem(m_entries[i]->itemID());
            removedItems.append(m_entries[i].release());
        }

        m_entries.clear();
        m_hasCurrentIndex = false;
        m_page->didChangeBackForwardList(nullptr, WTF::move(removedItems));

        return;
    }

    for (size_t i = 0; i < size; ++i) {
        ASSERT(m_entries[i]);
        if (m_entries[i] && m_entries[i] != currentItem)
            m_page->backForwardRemovedItem(m_entries[i]->itemID());
    }

    removedItems.reserveCapacity(size - 1);
    for (size_t i = 0; i < size; ++i) {
        if (i != m_currentIndex && m_hasCurrentIndex && m_entries[i])
            removedItems.append(m_entries[i].release());
    }

    m_currentIndex = 0;

    if (currentItem) {
        m_entries.shrink(1);
        m_entries[0] = currentItem.release();
    } else {
        m_entries.clear();
        m_hasCurrentIndex = false;
    }

    m_page->didChangeBackForwardList(nullptr, WTF::move(removedItems));
}

BackForwardListState WebBackForwardList::backForwardListState(const std::function<bool (WebBackForwardListItem&)>& filter) const
{
    ASSERT(!m_hasCurrentIndex || m_currentIndex < m_entries.size());

    BackForwardListState backForwardListState;
    if (m_hasCurrentIndex)
        backForwardListState.currentIndex = m_currentIndex;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = *m_entries[i];

        if (filter && !filter(entry)) {
            if (backForwardListState.currentIndex && i <= backForwardListState.currentIndex.value())
                --backForwardListState.currentIndex.value();

            continue;
        }

        backForwardListState.items.append(entry.itemState());
    }

    return backForwardListState;
}

void WebBackForwardList::restoreFromState(BackForwardListState backForwardListState)
{
    Vector<RefPtr<WebBackForwardListItem>> items;
    items.reserveInitialCapacity(backForwardListState.items.size());

    for (auto& backForwardListItemState : backForwardListState.items) {
        backForwardListItemState.identifier = generateWebBackForwardItemID();
        items.uncheckedAppend(WebBackForwardListItem::create(WTF::move(backForwardListItemState), m_page->pageID()));
    }
    m_hasCurrentIndex = !!backForwardListState.currentIndex;
    m_currentIndex = backForwardListState.currentIndex.valueOr(0);
    m_entries = WTF::move(items);
}

Vector<BackForwardListItemState> WebBackForwardList::itemStates() const
{
    Vector<BackForwardListItemState> itemStates;
    itemStates.reserveInitialCapacity(m_entries.size());

    for (const auto& entry : m_entries)
        itemStates.uncheckedAppend(entry->itemState());

    return itemStates;
}

} // namespace WebKit
