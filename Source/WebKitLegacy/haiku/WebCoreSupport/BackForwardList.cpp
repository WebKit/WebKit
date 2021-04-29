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

#include "WebCore/Frame.h"
#include "WebCore/FrameLoader.h"
#include "WebCore/FrameLoaderClient.h"
#include "WebCore/HistoryItem.h"
#include "WebCore/SerializedScriptValue.h"

static const unsigned DefaultCapacity = 100;
static const unsigned NoCurrentItemIndex = UINT_MAX;

using namespace WebCore;

BackForwardList::BackForwardList()
    : m_current(NoCurrentItemIndex)
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
            RefPtr<HistoryItem> item = m_entries.takeLast();
            m_entryHash.remove(item);
        }
    }

    // Toss the first item if the list is getting too big, as long as we're not using it
    // (or even if we are, if we only want 1 entry).
    if (m_entries.size() == m_capacity && (m_current || m_capacity == 1)) {
        RefPtr<HistoryItem> item = WTFMove(m_entries[0]);
        m_entries.remove(0);
        m_entryHash.remove(item);
        --m_current;
    }

    m_entryHash.add(newItem.ptr());
    m_entries.insert(m_current + 1, WTFMove(newItem));
    ++m_current;
}

void BackForwardList::goToItem(HistoryItem& item)
{
    if (!m_entries.size())
        return;

    unsigned int index = 0;
    for (; index < m_entries.size(); ++index)
        if (m_entries[index] == &item)
            break;
    if (index < m_entries.size()) {
        m_current = index;
    }
}


unsigned int BackForwardList::backListCount() const
{
    return m_current == NoCurrentItemIndex ? 0 : m_current;
}

unsigned int BackForwardList::forwardListCount() const
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

    return m_entries[index + m_current];
}


void BackForwardList::close()
{
    m_entries.clear();
    m_entryHash.clear();
    m_closed = true;
}

