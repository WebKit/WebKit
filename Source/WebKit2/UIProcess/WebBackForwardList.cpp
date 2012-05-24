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

#include "WebPageProxy.h"

namespace WebKit {

static const unsigned DefaultCapacity = 100;

WebBackForwardList::WebBackForwardList(WebPageProxy* page)
    : m_page(page)
    , m_current(NoCurrentItemIndex)
    , m_capacity(DefaultCapacity)
    , m_closed(true)
    , m_enabled(true)
{
    ASSERT(m_page);
}

WebBackForwardList::~WebBackForwardList()
{
}

void WebBackForwardList::pageClosed()
{
    if (m_page) {
        size_t size = m_entries.size();
        for (size_t i = 0; i < size; ++i)
            m_page->backForwardRemovedItem(m_entries[i]->itemID());
    }

    m_page = 0;
}

void WebBackForwardList::addItem(WebBackForwardListItem* newItem)
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    if (!m_capacity || !m_enabled || !newItem || !m_page)
        return;

    Vector<RefPtr<APIObject> > removedItems;
    
    if (m_current != NoCurrentItemIndex) {
        // Toss everything in the forward list.
        unsigned targetSize = m_current + 1;
        removedItems.reserveCapacity(m_entries.size() - targetSize);
        while (m_entries.size() > targetSize) {
            m_page->backForwardRemovedItem(m_entries.last()->itemID());
            removedItems.append(m_entries.last().release());
            m_entries.removeLast();
        }

        // Toss the first item if the list is getting too big, as long as we're not using it
        // (or even if we are, if we only want 1 entry).
        if (m_entries.size() == m_capacity && (m_current || m_capacity == 1)) {
            m_page->backForwardRemovedItem(m_entries[0]->itemID());
            removedItems.append(m_entries[0].release());
            m_entries.remove(0);
            m_current--;
        }
    } else {
        // If we have no current item index, we should have no other entries before adding this new item.
        size_t size = m_entries.size();
        for (size_t i = 0; i < size; ++i) {
            m_page->backForwardRemovedItem(m_entries[i]->itemID());
            removedItems.append(m_entries[i].release());
        }
        m_entries.clear();
    }
    
    if (m_current == NoCurrentItemIndex)
        m_current = 0;
    else
        m_current++;

    // m_current never be pointing more than 1 past the end of the entries Vector.
    // If it is, something has gone wrong and we should not try to insert the new item.
    ASSERT(m_current <= m_entries.size());
    if (m_current <= m_entries.size())
        m_entries.insert(m_current, newItem);

    m_page->didChangeBackForwardList(newItem, &removedItems);
}

void WebBackForwardList::goToItem(WebBackForwardListItem* item)
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    if (!m_entries.size() || !item)
        return;
        
    unsigned index = 0;
    for (; index < m_entries.size(); ++index) {
        if (m_entries[index] == item)
            break;
    }
    if (index < m_entries.size()) {
        m_current = index;
        if (m_page)
            m_page->didChangeBackForwardList(0, 0);
    }
}

WebBackForwardListItem* WebBackForwardList::currentItem()
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    if (m_current != NoCurrentItemIndex)
        return m_entries[m_current].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardList::backItem()
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    if (m_current && m_current != NoCurrentItemIndex)
        return m_entries[m_current - 1].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardList::forwardItem()
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    if (m_entries.size() && m_current < m_entries.size() - 1)
        return m_entries[m_current + 1].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardList::itemAtIndex(int index)
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    if (m_current == NoCurrentItemIndex)
        return 0;
    
    // Do range checks without doing math on index to avoid overflow.
    if (index < -backListCount())
        return 0;
    
    if (index > forwardListCount())
        return 0;
        
    return m_entries[index + m_current].get();
}

int WebBackForwardList::backListCount()
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    return m_current == NoCurrentItemIndex ? 0 : m_current;
}

int WebBackForwardList::forwardListCount()
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    return m_current == NoCurrentItemIndex ? 0 : static_cast<int>(m_entries.size()) - (m_current + 1);
}

PassRefPtr<ImmutableArray> WebBackForwardList::backListAsImmutableArrayWithLimit(unsigned limit)
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    unsigned backListSize = static_cast<unsigned>(backListCount());
    unsigned size = std::min(backListSize, limit);
    if (!size)
        return ImmutableArray::create();

    Vector<RefPtr<APIObject> > vector;
    vector.reserveInitialCapacity(size);

    ASSERT(backListSize >= size);
    for (unsigned i = backListSize - size; i < backListSize; ++i)
        vector.uncheckedAppend(m_entries[i].get());

    return ImmutableArray::adopt(vector);
}

PassRefPtr<ImmutableArray> WebBackForwardList::forwardListAsImmutableArrayWithLimit(unsigned limit)
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    unsigned size = std::min(static_cast<unsigned>(forwardListCount()), limit);
    if (!size)
        return ImmutableArray::create();

    Vector<RefPtr<APIObject> > vector;
    vector.reserveInitialCapacity(size);

    unsigned last = m_current + size;
    ASSERT(last < m_entries.size());
    for (unsigned i = m_current + 1; i <= last; ++i)
        vector.uncheckedAppend(m_entries[i].get());

    return ImmutableArray::adopt(vector);
}

void WebBackForwardList::clear()
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    size_t size = m_entries.size();
    if (size <= 1)
        return;

    RefPtr<WebBackForwardListItem> currentItem = this->currentItem();

    if (m_page) {
        for (size_t i = 0; i < size; ++i) {
            if (m_entries[i] != currentItem)
                m_page->backForwardRemovedItem(m_entries[i]->itemID());
        }
    }

    Vector<RefPtr<APIObject> > removedItems;
    removedItems.reserveCapacity(m_entries.size() - 1);
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (i != m_current)
            removedItems.append(m_entries[i].release());
    }

    m_current = 0;

    if (currentItem) {
        m_entries.shrink(1);
        m_entries[0] = currentItem.release();
    } else {
        m_entries.clear();
        m_current = NoCurrentItemIndex;
    }

    if (m_page)
        m_page->didChangeBackForwardList(0, &removedItems);
}

} // namespace WebKit
