/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "BackForwardList.h"

#include <WebCore/PageCache.h>

using namespace WebCore;

static const unsigned DefaultCapacity = 100;
static const unsigned NoCurrentItemIndex = UINT_MAX;

BackForwardList::BackForwardList(WebView *webView)
    : m_webView(webView)
    , m_current(NoCurrentItemIndex)
    , m_capacity(DefaultCapacity)
    , m_closed(true)
    , m_enabled(true)
{
}

BackForwardList::~BackForwardList()
{
    ASSERT(m_closed);
}

void BackForwardList::addItem(Ref<HistoryItem>&& newItem)
{
    if (!m_capacity || !m_enabled)
        return;
    
    // Toss anything in the forward list    
    if (m_current != NoCurrentItemIndex) {
        unsigned targetSize = m_current + 1;
        while (m_entries.size() > targetSize) {
            Ref<HistoryItem> item = m_entries.takeLast();
            m_entryHash.remove(item.ptr());
            PageCache::singleton().remove(item);
        }
    }

    // Toss the first item if the list is getting too big, as long as we're not using it
    // (or even if we are, if we only want 1 entry).
    if (m_entries.size() == m_capacity && (m_current || m_capacity == 1)) {
        Ref<HistoryItem> item = WTFMove(m_entries[0]);
        m_entries.remove(0);
        m_entryHash.remove(item.ptr());
        PageCache::singleton().remove(item);
        --m_current;
    }

    m_entryHash.add(newItem.ptr());
    m_entries.insert(m_current + 1, WTFMove(newItem));
    ++m_current;
}

void BackForwardList::goBack()
{
    ASSERT(m_current > 0);
    if (m_current > 0)
        m_current--;
}

void BackForwardList::goForward()
{
    ASSERT(m_current < m_entries.size() - 1);
    if (m_current < m_entries.size() - 1)
        m_current++;
}

void BackForwardList::goToItem(HistoryItem& item)
{
    if (!m_entries.size())
        return;

    unsigned index = 0;
    for (; index < m_entries.size(); ++index) {
        if (m_entries[index].ptr() == &item)
            break;
    }

    if (index < m_entries.size())
        m_current = index;
}

RefPtr<HistoryItem> BackForwardList::backItem()
{
    if (m_current && m_current != NoCurrentItemIndex)
        return m_entries[m_current - 1].copyRef();
    return nullptr;
}

RefPtr<HistoryItem> BackForwardList::currentItem()
{
    if (m_current != NoCurrentItemIndex)
        return m_entries[m_current].copyRef();
    return nullptr;
}

RefPtr<HistoryItem> BackForwardList::forwardItem()
{
    if (m_entries.size() && m_current < m_entries.size() - 1)
        return m_entries[m_current + 1].copyRef();
    return nullptr;
}

void BackForwardList::backListWithLimit(int limit, Vector<Ref<HistoryItem>>& list)
{
    list.clear();
    if (m_current != NoCurrentItemIndex) {
        unsigned first = std::max(static_cast<int>(m_current) - limit, 0);
        for (; first < m_current; ++first)
            list.append(m_entries[first].get());
    }
}

void BackForwardList::forwardListWithLimit(int limit, Vector<Ref<HistoryItem>>& list)
{
    ASSERT(limit > -1);
    list.clear();
    if (!m_entries.size())
        return;
        
    unsigned lastEntry = m_entries.size() - 1;
    if (m_current < lastEntry) {
        int last = std::min(m_current + limit, lastEntry);
        limit = m_current + 1;
        for (; limit <= last; ++limit)
            list.append(m_entries[limit].get());
    }
}

int BackForwardList::capacity()
{
    return m_capacity;
}

void BackForwardList::setCapacity(int size)
{    
    while (size < static_cast<int>(m_entries.size())) {
        Ref<HistoryItem> item = m_entries.takeLast();
        m_entryHash.remove(item.ptr());
        PageCache::singleton().remove(item);
    }

    if (!size)
        m_current = NoCurrentItemIndex;
    else if (m_current > m_entries.size() - 1)
        m_current = m_entries.size() - 1;

    m_capacity = size;
}

bool BackForwardList::enabled()
{
    return m_enabled;
}

void BackForwardList::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        int capacity = m_capacity;
        setCapacity(0);
        setCapacity(capacity);
    }
}

unsigned BackForwardList::backListCount() const
{
    return m_current == NoCurrentItemIndex ? 0 : m_current;
}

unsigned BackForwardList::forwardListCount() const
{
    return m_current == NoCurrentItemIndex ? 0 : m_entries.size() - m_current - 1;
}

RefPtr<HistoryItem> BackForwardList::itemAtIndex(int index)
{
    // Do range checks without doing math on index to avoid overflow.
    if (index < -static_cast<int>(m_current))
        return nullptr;
    
    if (index > static_cast<int>(forwardListCount()))
        return nullptr;

    return m_entries[index + m_current].copyRef();
}

#if PLATFORM(IOS_FAMILY)
unsigned BackForwardList::current()
{
    return m_current;
}

void BackForwardList::setCurrent(unsigned newCurrent)
{
    m_current = newCurrent;
}
#endif

void BackForwardList::close()
{
    m_entries.clear();
    m_entryHash.clear();
    m_webView = nullptr;
    m_closed = true;
}

bool BackForwardList::closed()
{
    return m_closed;
}

void BackForwardList::removeItem(HistoryItem& item)
{
    for (unsigned i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].ptr() == &item) {
            m_entries.remove(i);
            m_entryHash.remove(&item);
            if (m_current == NoCurrentItemIndex || m_current < i)
                break;
            if (m_current > i)
                m_current--;
            else {
                size_t count = m_entries.size();
                if (m_current >= count)
                    m_current = count ? count - 1 : NoCurrentItemIndex;
            }
            break;
        }
    }
}

bool BackForwardList::containsItem(HistoryItem& entry)
{
    return m_entryHash.contains(&entry);
}
