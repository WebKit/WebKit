/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2009, Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "BackForwardListImpl.h"

#include "HistoryItem.h"
#include "Logging.h"

namespace WebCore {

static const unsigned DefaultCapacity = 100;
static const unsigned NoCurrentItemIndex = UINT_MAX;

BackForwardListImpl::BackForwardListImpl(Page* page)
    : m_page(page)
    , m_client(0)
    , m_capacity(DefaultCapacity)
    , m_closed(true)
    , m_enabled(true)
{
}

BackForwardListImpl::~BackForwardListImpl()
{
    ASSERT(m_closed);
}

void BackForwardListImpl::addItem(PassRefPtr<HistoryItem> prpItem)
{
    ASSERT(prpItem);
    if (m_capacity == 0 || !m_enabled)
        return;
 
    m_client->addItem(prpItem);
}

void BackForwardListImpl::goToItem(HistoryItem* item)
{
    m_client->goToItem(item);
}

int BackForwardListImpl::capacity()
{
    return m_capacity;
}

void BackForwardListImpl::setCapacity(int size)
{
    m_capacity = size;
}

bool BackForwardListImpl::enabled()
{
    return m_enabled;
}

void BackForwardListImpl::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        int capacity = m_capacity;
        setCapacity(0);
        setCapacity(capacity);
    }
}

int BackForwardListImpl::backListCount()
{
    return m_client->backListCount();
}

int BackForwardListImpl::forwardListCount()
{
    return m_client->forwardListCount();
}

HistoryItem* BackForwardListImpl::itemAtIndex(int index)
{
    return m_client->itemAtIndex(index);
}

HistoryItemVector& BackForwardListImpl::entries()
{
    static HistoryItemVector noEntries;
    return noEntries;
}

void BackForwardListImpl::close()
{
    if (m_client)
        m_client->close();
    m_page = 0;
    m_closed = true;
}

bool BackForwardListImpl::closed()
{
    return m_closed;
}

void BackForwardListImpl::goBack()
{
    ASSERT_NOT_REACHED();
}

void BackForwardListImpl::goForward()
{
    ASSERT_NOT_REACHED();
}

void BackForwardListImpl::backListWithLimit(int, HistoryItemVector&)
{
    ASSERT_NOT_REACHED();
}

void BackForwardListImpl::forwardListWithLimit(int, HistoryItemVector&)
{
    ASSERT_NOT_REACHED();
}

bool BackForwardListImpl::containsItem(HistoryItem*)
{
    ASSERT_NOT_REACHED();
    return false;
}

void BackForwardListImpl::removeItem(HistoryItem*)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
