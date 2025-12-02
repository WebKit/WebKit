/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "APIObject.h"
#include "MessageReceiver.h"
#include "WebBackForwardListItem.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace API {
class Array;
}

namespace WebKit {

class WebPageProxy;

struct BackForwardListState;
struct WebBackForwardListCounts;

class WebBackForwardList : public API::ObjectImpl<API::Object::Type::BackForwardList>, public IPC::MessageReceiver {
public:
    static Ref<WebBackForwardList> create(WebPageProxy& page)
    {
        return adoptRef(*new WebBackForwardList(page));
    }

    void ref() const final { API::ObjectImpl<API::Object::Type::BackForwardList>::ref(); }
    void deref() const final { API::ObjectImpl<API::Object::Type::BackForwardList>::deref(); }

    void pageClosed();

    virtual ~WebBackForwardList();

    WebBackForwardListItem* itemForID(WebCore::BackForwardItemIdentifier);

    void goToItem(WebBackForwardListItem&);
    void removeAllItems();
    void clear();

    WebBackForwardListItem* currentItem() const;
    RefPtr<WebBackForwardListItem> protectedCurrentItem() const;
    WebBackForwardListItem* backItem() const;
    RefPtr<WebBackForwardListItem> protectedBackItem() const;
    WebBackForwardListItem* forwardItem() const;
    RefPtr<WebBackForwardListItem> protectedForwardItem() const;
    WebBackForwardListItem* itemAtIndex(int) const;
    RefPtr<WebBackForwardListItem> protectedItemAtIndex(int) const;

    RefPtr<WebBackForwardListItem> goBackItemSkippingItemsWithoutUserGesture() const;
    RefPtr<WebBackForwardListItem> goForwardItemSkippingItemsWithoutUserGesture() const;
    unsigned backListCount() const;
    unsigned forwardListCount() const;

    Ref<API::Array> backList() const;
    Ref<API::Array> forwardList() const;

    Ref<API::Array> backListAsAPIArrayWithLimit(unsigned limit) const;
    Ref<API::Array> forwardListAsAPIArrayWithLimit(unsigned limit) const;

    BackForwardListState backForwardListState(WTF::Function<bool (WebBackForwardListItem&)>&&) const;
    void restoreFromState(BackForwardListState);

    void setItemsAsRestoredFromSession();
    void setItemsAsRestoredFromSessionIf(NOESCAPE Function<bool(WebBackForwardListItem&)>&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void backForwardAddItemShared(IPC::Connection&, Ref<FrameState>&&, LoadedWebArchive);
    void backForwardGoToItemShared(WebCore::BackForwardItemIdentifier, CompletionHandler<void(const WebBackForwardListCounts&)>&&);

    String loggingString();

private:
    explicit WebBackForwardList(WebPageProxy&);

    void addItem(Ref<WebBackForwardListItem>&&);
    void addChildItem(WebCore::FrameIdentifier, Ref<FrameState>&&);
    void didRemoveItem(WebBackForwardListItem&);
    const BackForwardListItemVector& entries() const { return m_entries; }
    BackForwardListItemVector allItems() const { return m_entries; }
    WebBackForwardListCounts counts() const;
    Ref<FrameState> completeFrameStateForNavigation(Ref<FrameState>&&);

    RefPtr<WebPageProxy> protectedPage();

    // IPC messages
    void backForwardAddItem(IPC::Connection&, Ref<FrameState>&&);
    void backForwardSetChildItem(WebCore::BackForwardFrameItemIdentifier, Ref<FrameState>&&);
    void backForwardClearChildren(WebCore::BackForwardItemIdentifier, WebCore::BackForwardFrameItemIdentifier);
    void backForwardUpdateItem(IPC::Connection&, Ref<FrameState>&&);
    void backForwardGoToItem(WebCore::BackForwardItemIdentifier, CompletionHandler<void(const WebBackForwardListCounts&)>&&);
    void backForwardAllItems(WebCore::FrameIdentifier, CompletionHandler<void(Vector<Ref<FrameState>>&&)>&&);
    void backForwardItemAtIndex(int32_t index, WebCore::FrameIdentifier, CompletionHandler<void(RefPtr<FrameState>&&)>&&);
    void backForwardListContainsItem(WebCore::BackForwardItemIdentifier, CompletionHandler<void(bool)>&&);
    void backForwardListCounts(CompletionHandler<void(WebBackForwardListCounts&&)>&&);
    void shouldGoToBackForwardListItem(WebCore::BackForwardItemIdentifier, bool inBackForwardCache, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&&);
    void shouldGoToBackForwardListItemSync(WebCore::BackForwardItemIdentifier, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&&);

    WeakPtr<WebPageProxy> m_page;
    BackForwardListItemVector m_entries;
    std::optional<size_t> m_currentIndex;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebBackForwardList)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BackForwardList; }
SPECIALIZE_TYPE_TRAITS_END()
