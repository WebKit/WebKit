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

#include "WebBackForwardList.h"

#include "WebPageProxy.h"

namespace WebKit {

static const unsigned DefaultCapacity = 100;
static const unsigned NoCurrentItemIndex = UINT_MAX;

WebBackForwardList::WebBackForwardList(WebPageProxy* page)
    : m_page(page)
    , m_current(NoCurrentItemIndex)
    , m_capacity(DefaultCapacity)
    , m_closed(true)
    , m_enabled(true)
{
}

WebBackForwardList::~WebBackForwardList()
{
}

void WebBackForwardList::addItem(WebBackForwardListItem* newItem)
{
    if (m_capacity == 0 || !m_enabled)
        return;
    
    // Toss anything in the forward list    
    if (m_current != NoCurrentItemIndex) {
        unsigned targetSize = m_current + 1;
        while (m_entries.size() > targetSize) {
            RefPtr<WebBackForwardListItem> item = m_entries.last();
            m_entries.removeLast();
        }
    }

    // Toss the first item if the list is getting too big, as long as we're not using it
    // (or even if we are, if we only want 1 entry).
    if (m_entries.size() == m_capacity && (m_current != 0 || m_capacity == 1)) {
        RefPtr<WebBackForwardListItem> item = m_entries[0];
        m_entries.remove(0);
        m_current--;
        
        if (m_page)
            m_page->didChangeBackForwardList();
    }

    m_entries.insert(m_current + 1, newItem);
    m_current++;

    if (m_page)
        m_page->didChangeBackForwardList();
}

void WebBackForwardList::goToItem(WebBackForwardListItem* item)
{
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
            m_page->didChangeBackForwardList();
    }
}

WebBackForwardListItem* WebBackForwardList::currentItem()
{
    if (m_current != NoCurrentItemIndex)
        return m_entries[m_current].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardList::backItem()
{
    if (m_current && m_current != NoCurrentItemIndex)
        return m_entries[m_current - 1].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardList::forwardItem()
{
    if (m_entries.size() && m_current < m_entries.size() - 1)
        return m_entries[m_current + 1].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardList::itemAtIndex(int index)
{
    // Do range checks without doing math on index to avoid overflow.
    if (index < -static_cast<int>(m_current))
        return 0;
    
    if (index > forwardListCount())
        return 0;
        
    return m_entries[index + m_current].get();
}

int WebBackForwardList::backListCount()
{
    return m_current == NoCurrentItemIndex ? 0 : m_current;
}

int WebBackForwardList::forwardListCount()
{
    return m_current == NoCurrentItemIndex ? 0 : static_cast<int>(m_entries.size()) - (m_current + 1);
}

BackForwardListItemVector WebBackForwardList::backListWithLimit(unsigned limit)
{
    BackForwardListItemVector list;
    unsigned size = std::min(backListCount(), static_cast<int>(limit));
    if (!size)
        return list;

    list.resize(size);
    for (unsigned i = std::max(m_current - limit, 0U), j = 0; i < m_current; ++i, ++j)
        list[j] = m_entries[i];

    return list;
}

BackForwardListItemVector WebBackForwardList::forwardListWithLimit(unsigned limit)
{
    BackForwardListItemVector list;
    unsigned size = std::min(forwardListCount(), static_cast<int>(limit));
    if (!size)
        return list;

    list.resize(size);
    unsigned last = std::min(m_current + limit, static_cast<unsigned>(m_entries.size() - 1));
    for (unsigned i = m_current + 1, j = 0; i <= last; ++i, ++j)
        list[j] = m_entries[i];

    return list;
}

PassRefPtr<ImmutableArray> WebBackForwardList::backListAsImmutableArrayWithLimit(unsigned limit)
{
    unsigned backListSize = static_cast<unsigned>(backListCount());
    unsigned size = std::min(backListSize, limit);
    if (!size)
        return ImmutableArray::create();

    Vector<APIObject*> vector;
    vector.reserveInitialCapacity(size);

    ASSERT(backListSize >= size);
    for (unsigned i = backListSize - size; i < backListSize; ++i) {
        APIObject* item = m_entries[i].get();
        item->ref();
        vector.uncheckedAppend(item);
    }

    return ImmutableArray::adopt(vector);
}

PassRefPtr<ImmutableArray> WebBackForwardList::forwardListAsImmutableArrayWithLimit(unsigned limit)
{
    unsigned size = std::min(static_cast<unsigned>(forwardListCount()), limit);
    if (!size)
        return ImmutableArray::create();

    Vector<APIObject*> vector;
    vector.reserveInitialCapacity(size);

    unsigned last = m_current + size;
    ASSERT(last < m_entries.size());
    for (unsigned i = m_current + 1; i <= last; ++i) {
        APIObject* item = m_entries[i].get();
        item->ref();
        vector.uncheckedAppend(item);
    }

    return ImmutableArray::adopt(vector);
}

} // namespace WebKit
