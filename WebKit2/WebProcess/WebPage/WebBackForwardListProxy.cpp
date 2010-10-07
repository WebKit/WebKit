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

#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessageKinds.h"
#include <WebCore/HistoryItem.h>
#include <wtf/HashMap.h>

using namespace WebCore;

namespace WebKit {

static const unsigned DefaultCapacity = 100;
static const unsigned NoCurrentItemIndex = UINT_MAX;

// FIXME: This leaks all HistoryItems that go into these maps.  We need to clear
// up the life time of these objects.

typedef HashMap<uint64_t, RefPtr<HistoryItem> > IDToHistoryItemMap;
typedef HashMap<RefPtr<HistoryItem>, uint64_t> HistoryItemToIDMap;

static IDToHistoryItemMap& idToHistoryItemMap()
{
    static IDToHistoryItemMap map;
    return map;
} 

static HistoryItemToIDMap& historyItemToIDMap()
{
    static HistoryItemToIDMap map;
    return map;
} 

static uint64_t generateHistoryItemID()
{
    static uint64_t uniqueHistoryItemID = 1;
    return uniqueHistoryItemID++;
}

static uint64_t getIDForHistoryItem(HistoryItem* item)
{
    uint64_t itemID = 0;

    std::pair<HistoryItemToIDMap::iterator, bool> result = historyItemToIDMap().add(item, 0);
    if (result.second) {
        itemID = generateHistoryItemID();
        result.first->second = itemID;
        idToHistoryItemMap().set(itemID, item);
    } else
        itemID = result.first->second;

    ASSERT(itemID);
    return itemID;
}

static void updateBackForwardItem(HistoryItem* item)
{
    uint64_t itemID = getIDForHistoryItem(item);
    const String& originalURLString = item->originalURLString();
    const String& urlString = item->urlString();
    const String& title = item->title();
    WebProcess::shared().connection()->send(WebProcessProxyMessage::AddBackForwardItem, 0, CoreIPC::In(itemID, originalURLString, urlString, title));
}

static void WK2NotifyHistoryItemChanged(HistoryItem* item)
{
    updateBackForwardItem(item);
}

HistoryItem* WebBackForwardListProxy::itemForID(uint64_t itemID)
{
    return idToHistoryItemMap().get(itemID).get();
}

WebBackForwardListProxy::WebBackForwardListProxy(WebPage* page)
    : m_page(page)
    , m_capacity(DefaultCapacity)
    , m_closed(true)
    , m_enabled(true)
{
    WebCore::notifyHistoryItemChanged = WK2NotifyHistoryItemChanged;
}

WebBackForwardListProxy::~WebBackForwardListProxy()
{
}

void WebBackForwardListProxy::addItem(PassRefPtr<HistoryItem> prpItem)
{
    if (!m_capacity || !m_enabled)
        return;

    RefPtr<HistoryItem> item = prpItem;
    uint64_t itemID = historyItemToIDMap().get(item);
    WebProcess::shared().connection()->send(Messages::WebPageProxy::BackForwardAddItem(itemID), m_page->pageID());
}

void WebBackForwardListProxy::goBack()
{
    ASSERT_NOT_REACHED();
}

void WebBackForwardListProxy::goForward()
{
    ASSERT_NOT_REACHED();
}

void WebBackForwardListProxy::goToItem(HistoryItem* item)
{
    uint64_t itemID = historyItemToIDMap().get(item);
    WebProcess::shared().connection()->send(Messages::WebPageProxy::BackForwardGoToItem(itemID), m_page->pageID());

}

HistoryItem* WebBackForwardListProxy::backItem()
{
    uint64_t backItemID = 0;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::BackForwardBackItem(),
             Messages::WebPageProxy::BackForwardBackItem::Reply(backItemID),
             m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!backItemID)
        return 0;

    RefPtr<HistoryItem> item = idToHistoryItemMap().get(backItemID);
    return item.get();
}

HistoryItem* WebBackForwardListProxy::currentItem()
{
    uint64_t currentItemID = 0;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::BackForwardCurrentItem(),
             Messages::WebPageProxy::BackForwardCurrentItem::Reply(currentItemID),
             m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!currentItemID)
        return 0;

    RefPtr<HistoryItem> item = idToHistoryItemMap().get(currentItemID);
    return item.get();
}

HistoryItem* WebBackForwardListProxy::forwardItem()
{
    uint64_t forwardItemID = 0;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::BackForwardForwardItem(),
             Messages::WebPageProxy::BackForwardForwardItem::Reply(forwardItemID),
             m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!forwardItemID)
        return 0;

    RefPtr<HistoryItem> item = idToHistoryItemMap().get(forwardItemID);
    return item.get();
}

HistoryItem* WebBackForwardListProxy::itemAtIndex(int itemIndex)
{
    uint64_t itemID = 0;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::BackForwardItemAtIndex(itemIndex),
             Messages::WebPageProxy::BackForwardItemAtIndex::Reply(itemID),
             m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!itemID)
        return 0;

    RefPtr<HistoryItem> item = idToHistoryItemMap().get(itemID);
    return item.get();
}

void WebBackForwardListProxy::backListWithLimit(int, HistoryItemVector&)
{
    ASSERT_NOT_REACHED();
}

void WebBackForwardListProxy::forwardListWithLimit(int, HistoryItemVector&)
{
    ASSERT_NOT_REACHED();
}

int WebBackForwardListProxy::capacity()
{
    return m_capacity;
}

void WebBackForwardListProxy::setCapacity(int capacity)
{
    m_capacity = capacity;
}

bool WebBackForwardListProxy::enabled()
{
    return m_enabled;
}

void WebBackForwardListProxy::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

int WebBackForwardListProxy::backListCount()
{
    int backListCount = 0;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::BackForwardBackListCount(),
             Messages::WebPageProxy::BackForwardBackListCount::Reply(backListCount),
             m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    return backListCount;
}

int WebBackForwardListProxy::forwardListCount()
{
    int forwardListCount = 0;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::BackForwardForwardListCount(),
             Messages::WebPageProxy::BackForwardForwardListCount::Reply(forwardListCount),
             m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    return forwardListCount;
}

bool WebBackForwardListProxy::containsItem(HistoryItem*)
{
    return false;
}

void WebBackForwardListProxy::close()
{
    m_closed = true;
    m_page = 0;
}

bool WebBackForwardListProxy::closed()
{
    return m_closed;
}

void WebBackForwardListProxy::removeItem(HistoryItem*)
{
}

HistoryItemVector& WebBackForwardListProxy::entries()
{
    static HistoryItemVector noEntries;
    return noEntries;
}

#if ENABLE(WML)
void WebBackForwardListProxy::clearWMLPageHistory()
{
}
#endif

} // namespace WebKit
