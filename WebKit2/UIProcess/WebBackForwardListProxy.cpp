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

#include "WebBackForwardListProxy.h"

namespace WebKit {

static const unsigned NoCurrentItemIndex = UINT_MAX;

WebBackForwardListProxy::WebBackForwardListProxy(WebPageProxy* page)
    : m_page(page)
    , m_current(NoCurrentItemIndex)
{
}

WebBackForwardListProxy::~WebBackForwardListProxy()
{
}

WebBackForwardListItem* WebBackForwardListProxy::currentItem()
{
    if (m_current != NoCurrentItemIndex)
        return m_entries[m_current].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardListProxy::backItem()
{
    if (m_current && m_current != NoCurrentItemIndex)
        return m_entries[m_current - 1].get();
    return 0;
}

WebBackForwardListItem* WebBackForwardListProxy::forwardItem()
{
    if (m_entries.size() && m_current < m_entries.size() - 1)
        return m_entries[m_current + 1].get();
    return 0;
}

unsigned WebBackForwardListProxy::backListCount()
{
    return m_current == NoCurrentItemIndex ? 0 : m_current;
}

unsigned WebBackForwardListProxy::forwardListCount()
{
    return m_current == NoCurrentItemIndex ? 0 : m_entries.size() - (m_current + 1);
}

BackForwardListItemVector WebBackForwardListProxy::backListWithLimit(unsigned limit)
{
    BackForwardListItemVector list;
    unsigned size = std::min(backListCount(), limit);
    if (!size)
        return list;

    list.resize(size);
    for (unsigned i = std::max(m_current - limit, 0U), j = 0; i < m_current; ++i, ++j)
        list[j] = m_entries[i];

    return list;
}

BackForwardListItemVector WebBackForwardListProxy::forwardListWithLimit(unsigned limit)
{
    BackForwardListItemVector list;
    unsigned size = std::min(forwardListCount(), limit);
    if (!size)
        return list;

    list.resize(size);
    unsigned last = std::min(m_current + limit, static_cast<unsigned>(m_entries.size() - 1));
    for (unsigned i = m_current + 1, j = 0; i <= last; ++i, ++j)
        list[j] = m_entries[i];

    return list;
}

// ImmutableArray::ImmutableArrayCallback [for WebBackForwardListItem] callbacks

static void webBackForwardListItemRef(const void* item)
{
    static_cast<WebBackForwardListItem*>(const_cast<void*>(item))->ref();
}

static void webBackForwardListItemDeref(const void* item)
{
    static_cast<WebBackForwardListItem*>(const_cast<void*>(item))->deref();
}

PassRefPtr<ImmutableArray> WebBackForwardListProxy::backListAsImmutableArrayWithLimit(unsigned limit)
{
    unsigned size = std::min(backListCount(), limit);
    if (!size)
        return ImmutableArray::create();

    void** array = new void*[size];
    for (unsigned i = std::max(m_current - limit, 0U), j = 0; i < m_current; ++i, ++j) {
        WebBackForwardListItem* item = m_entries[i].get();
        item->ref();
        array[j] = item;
    }

    ImmutableArray::ImmutableArrayCallbacks callbacks = {
        webBackForwardListItemRef,
        webBackForwardListItemDeref
    };
    return ImmutableArray::adopt(array, size, &callbacks);
}

PassRefPtr<ImmutableArray> WebBackForwardListProxy::forwardListAsImmutableArrayWithLimit(unsigned limit)
{
    unsigned size = std::min(forwardListCount(), limit);
    if (!size)
        return ImmutableArray::create();

    void** array = new void*[size];
    unsigned last = std::min(m_current + limit, static_cast<unsigned>(m_entries.size() - 1));
    for (unsigned i = m_current + 1, j = 0; i <= last; ++i, ++j) {
        WebBackForwardListItem* item = m_entries[i].get();
        item->ref();
        array[j] = item;
    }

    ImmutableArray::ImmutableArrayCallbacks callbacks = {
        webBackForwardListItemRef,
        webBackForwardListItemDeref
    };
    return ImmutableArray::adopt(array, size, &callbacks);
}

} // namespace WebKit
