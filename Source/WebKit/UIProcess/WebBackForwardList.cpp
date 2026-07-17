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
#include "WebBackForwardListSwiftUtilities.h"
#include "WebPageProxy.h"

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=306415
#include "WebKit-Swift.h"

namespace WebKit {
using namespace WebCore;

static inline void setBackForwardItemIdentifiers(FrameState& frameState, BackForwardItemIdentifier itemID)
{
    frameState.itemID = itemID;
    frameState.frameItemID = BackForwardFrameItemIdentifier::generate();
    for (auto& child : frameState.children)
        setBackForwardItemIdentifiers(child, itemID);
}

WebBackForwardListWrapper::WebBackForwardListWrapper(WebPageProxy& webPageProxy)
    : m_impl(WTF::makeUniqueWithoutFastMallocCheck<WebBackForwardList>(WebBackForwardList::init(webPageProxy)))
    , m_messageForwarder(m_impl->getMessageReceiver())
{
}

WebBackForwardListWrapper::~WebBackForwardListWrapper() = default;

WebBackForwardListMessageForwarder& WebBackForwardListWrapper::messageReceiver() const
{
    return m_messageForwarder.get();
}

WebBackForwardListItem* WebBackForwardListWrapper::currentItem() const
{
    return m_impl->currentItem();
}

RefPtr<WebBackForwardListItem> WebBackForwardListWrapper::backItem() const
{
    return m_impl->backItem();
}

RefPtr<WebBackForwardListItem> WebBackForwardListWrapper::forwardItem() const
{
    return m_impl->forwardItem();
}

RefPtr<WebBackForwardListItem> WebBackForwardListWrapper::itemAtDeltaFromCurrentIndex(int index, AllowSkippingBackForwardItems allowSkipping) const
{
    return m_impl->itemAtDeltaFromCurrentIndex(index, allowSkipping == AllowSkippingBackForwardItems::Yes ? true : false);
}

unsigned WebBackForwardListWrapper::backListCountForAPI() const
{
    return m_impl->backListCountForAPI();
}

unsigned WebBackForwardListWrapper::forwardListCountForAPI() const
{
    return m_impl->forwardListCountForAPI();
}

Ref<API::Array> WebBackForwardListWrapper::backList() const
{
    return backListAsAPIArrayWithLimit(backListCountForAPI());
}

Ref<API::Array> WebBackForwardListWrapper::forwardList() const
{
    return forwardListAsAPIArrayWithLimit(forwardListCountForAPI());
}

Ref<API::Array> WebBackForwardListWrapper::backListAsAPIArrayWithLimit(unsigned limit) const
{
    return m_impl->backListAsAPIArrayWithLimit(limit);
}

Ref<API::Array> WebBackForwardListWrapper::forwardListAsAPIArrayWithLimit(unsigned limit) const
{
    return m_impl->forwardListAsAPIArrayWithLimit(limit);
}

void WebBackForwardListWrapper::removeAllItems()
{
    m_impl->removeAllItems();
}

void WebBackForwardListWrapper::clear()
{
    m_impl->clear();
}

String WebBackForwardListWrapper::loggingString()
{
    return String::fromUTF8WithLatin1Fallback(std::string(m_impl->loggingString()));
}

} // namespace WebKit

WebCore::BackForwardFrameItemIdentifier generateBackForwardFrameItemIdentifier()
{
    return WebCore::BackForwardFrameItemIdentifier::generate();
}

// rdar://168139823 is the task of doing a productionized version of WebKit Swift logging
void doLog(const WTF::String& msg)
{
    LOG(BackForward, "%s", msg.utf8().data());
}

void doLoadingReleaseLog(const WTF::String& msg)
{
    RELEASE_LOG(Loading, "%s", msg.utf8().data());
}
// rdar://168139740 is the task of doing a productionized Swift MESSAGE_CHECK
void messageCheckFailed(Ref<WebKit::WebProcessProxy> process)
{
    MESSAGE_CHECK_BASE(false, process->connection());
}

// Workarounds for rdar://171011011
void appendToBackForwardStateItems(Vector<WebKit::BackForwardListItemState>& items, const WebKit::WebBackForwardListItem& entry)
{
    items.append({ entry.copyMainFrameStateWithChildren(), entry.navigatedFrameID() });
}

void setFrameStateBackForwardItemIdentifier(WebKit::FrameState& frameState, const WebCore::BackForwardItemIdentifier& itemID)
{
    frameState.itemID = itemID;
    for (auto& child : frameState.children)
        setFrameStateBackForwardItemIdentifier(child, itemID);
}

Ref<WebKit::WebBackForwardListItem> createItemFromState(const WebKit::BackForwardListItemState& itemState, WebKit::WebPageProxyIdentifier pageIdentifier)
{
    Ref stateCopy = itemState.frameState->copy();
    setBackForwardItemIdentifiers(stateCopy, WebCore::BackForwardItemIdentifier::generate());
    return WebKit::WebBackForwardListItem::create(WTF::move(stateCopy), pageIdentifier, itemState.navigatedFrameID);
}

Vector<Ref<WebKit::WebBackForwardListItem>> createItemsFromState(const WebKit::BackForwardListState& state, WebKit::WebPageProxyIdentifier pageIdentifier)
{
    Vector<Ref<WebKit::WebBackForwardListItem>> items;
    items.reserveInitialCapacity(state.items.size());
    for (auto& itemState : state.items)
        items.append(createItemFromState(itemState, pageIdentifier));
    return items;
}

WebKit::WebBackForwardListItem* itemAtIndexInBackForwardListItemVector(const Vector<Ref<WebKit::WebBackForwardListItem>>& items, size_t index)
{
    return items[index].ptr();
}
