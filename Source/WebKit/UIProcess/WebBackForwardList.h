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
#include "LoadedWebArchive.h"
#include "MessageReceiver.h"
#include "WebBackForwardListItem.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <wtf/Ref.h>
#include <wtf/ThreadGroup.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace API {
class Array;
}

namespace WebKit {

class FrameState;
class WebPageProxy;

struct BackForwardListState;
struct WebBackForwardListCounts;

enum class AllowSkippingBackForwardItems : bool { No, Yes };

#if !ENABLE(BACK_FORWARD_LIST_SWIFT)

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

    WebBackForwardListItem* NODELETE currentItem() const;
    RefPtr<WebBackForwardListItem> backItem() const;
    RefPtr<WebBackForwardListItem> forwardItem() const;

    RefPtr<WebBackForwardListItem> itemAtDeltaFromCurrentIndex(int, AllowSkippingBackForwardItems = AllowSkippingBackForwardItems::Yes) const;

    RefPtr<WebBackForwardListItem> goBackItemSkippingItemsWithoutUserGesture() const;
    RefPtr<WebBackForwardListItem> goForwardItemSkippingItemsWithoutUserGesture() const;
    unsigned backListCountForAPI() const;
    unsigned forwardListCountForAPI() const;

    Ref<API::Array> backList() const;
    Ref<API::Array> forwardList() const;

    Ref<API::Array> backListAsAPIArrayWithLimit(unsigned limit) const;
    Ref<API::Array> forwardListAsAPIArrayWithLimit(unsigned limit) const;

    BackForwardListState backForwardListState(WTF::Function<bool (WebBackForwardListItem&)>&&) const;
    void restoreFromState(BackForwardListState);

    void setItemsAsRestoredFromSession();
    void setItemsAsRestoredFromSessionIf(NOESCAPE Function<bool(WebBackForwardListItem&)>&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveProvisionalMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void backForwardAddItemShared(IPC::Connection&, Ref<FrameState>&&, LoadedWebArchive);
    void backForwardGoToItemShared(WebCore::BackForwardItemIdentifier);

    FrameState* findFrameStateInItem(WebCore::BackForwardItemIdentifier, WebCore::FrameIdentifier parentFrameID, WebCore::FrameIdentifier childFrameID, uint64_t childFrameIndex, const String& childFrameName);
    void updateFrameIdentifier(WebCore::FrameIdentifier oldFrameID, WebCore::FrameIdentifier newFrameID);

    void replaceFrameStateForChild(WebBackForwardListItem&, WebCore::FrameIdentifier, Ref<FrameState>&& newFrameState);

    String loggingString() const;

    enum class MakeAPIArray : bool { No, Yes };

private:
    // Couples the entries vector with the current index, making it structurally impossible
    // to have a non-empty list without a valid current index, or vice versa.
    class ActiveList {
    public:
        ActiveList(BackForwardListItemVector&& entries, size_t currentIndex)
            : m_entries(WTF::move(entries))
            , m_currentIndex(currentIndex)
        {
            ASSERT(!m_entries.isEmpty() && m_currentIndex < m_entries.size());
        }

        const BackForwardListItemVector& entries() const { return m_entries; }
        size_t currentIndex() const { return m_currentIndex; }

        void assertValid() const { ASSERT(m_currentIndex < m_entries.size()); }

        // Removes and returns the last entry. Caller must ensure forward entries exist.
        Ref<WebBackForwardListItem> removeLast()
        {
            Ref<WebBackForwardListItem> removed = WTF::move(m_entries.last());
            m_entries.removeLast();
            assertValid();
            return removed;
        }

        // Removes and returns the first entry, adjusting currentIndex.
        // Caller must ensure currentIndex > 0. Since currentIndex > 0 implies
        // entries.size() >= 2, entries cannot become empty after this call.
        Ref<WebBackForwardListItem> removeFirst()
        {
            ASSERT(m_currentIndex > 0);
            Ref<WebBackForwardListItem> removed = WTF::move(m_entries[0]);
            m_entries.removeAt(0);
            --m_currentIndex;
            assertValid();
            return removed;
        }

        // Replaces the entry at currentIndex with newItem; returns the replaced entry.
        Ref<WebBackForwardListItem> replaceCurrentEntry(Ref<WebBackForwardListItem>&& newItem)
        {
            Ref<WebBackForwardListItem> removed = WTF::move(m_entries[m_currentIndex]);
            m_entries[m_currentIndex] = WTF::move(newItem);
            assertValid();
            return removed;
        }

        // Advances currentIndex by 1 and inserts newItem there.
        void advanceAndInsert(Ref<WebBackForwardListItem>&& newItem)
        {
            ++m_currentIndex;
            m_entries.insert(m_currentIndex, WTF::move(newItem));
            assertValid();
        }

        // Removes the current entry and moves currentIndex to point at the item
        // that was at preRemovalTargetIndex in the pre-removal list.
        // preRemovalTargetIndex must not equal currentIndex.
        Ref<WebBackForwardListItem> removeCurrentEntry(size_t preRemovalTargetIndex)
        {
            ASSERT(preRemovalTargetIndex != m_currentIndex);
            Ref<WebBackForwardListItem> removed = WTF::move(m_entries[m_currentIndex]);
            m_entries.removeAt(m_currentIndex);
            m_currentIndex = preRemovalTargetIndex > m_currentIndex ? preRemovalTargetIndex - 1 : preRemovalTargetIndex;
            assertValid();
            return removed;
        }

        // Moves currentIndex to newIndex without modifying entries.
        void moveToIndex(size_t newIndex)
        {
            ASSERT(newIndex < m_entries.size());
            m_currentIndex = newIndex;
            assertValid();
        }

        // Returns all entries, leaving this object in an invalid state.
        // Only call immediately before destroying or resetting the containing optional.
        BackForwardListItemVector takeEntries() { return WTF::move(m_entries); }

    private:
        BackForwardListItemVector m_entries;
        size_t m_currentIndex;
    };

    explicit WebBackForwardList(WebPageProxy&);

    enum class NavigationDirection { Backward, Forward };
    std::pair<RefPtr<WebBackForwardListItem>, size_t> itemStartingAtIndexSkippingItemsAddedByJSWithoutUserGesture(NavigationDirection, size_t startingIndex) const;
    std::pair<RefPtr<WebBackForwardListItem>, size_t> itemAtIndexWithoutSkipping(size_t) const;

    std::pair<unsigned, RefPtr<API::Array>> backListWithLimitInternal(unsigned limit, MakeAPIArray) const;
    std::pair<unsigned, RefPtr<API::Array>> forwardListWithLimitInternal(unsigned limit, MakeAPIArray) const;

    unsigned NODELETE rawBackListEntryCount() const;
    unsigned NODELETE rawForwardListEntryCount() const;

    void addItem(Ref<WebBackForwardListItem>&&);
    void addChildItem(WebCore::FrameIdentifier, Ref<FrameState>&&);
    void didRemoveItem(WebBackForwardListItem&);
    const BackForwardListItemVector& activeEntries() const LIFETIME_BOUND;
    WebBackForwardListCounts NODELETE rawCounts() const;
    Ref<FrameState> completeFrameStateForNavigation(Ref<FrameState>&&);

    // IPC messages
    void backForwardAddItem(IPC::Connection&, Ref<FrameState>&&);
    void backForwardSetChildItem(IPC::Connection&, WebCore::BackForwardFrameItemIdentifier, Ref<FrameState>&&);
    void backForwardClearChildren(WebCore::BackForwardItemIdentifier, WebCore::BackForwardFrameItemIdentifier);
    void backForwardUpdateItem(IPC::Connection&, Ref<FrameState>&&);
    void backForwardGoToItem(WebCore::BackForwardItemIdentifier);
    void backForwardAllItems(WebCore::FrameIdentifier, CompletionHandler<void(Vector<Ref<FrameState>>&&)>&&);
    void backForwardItemAtIndexForWebContent(IPC::Connection&, int32_t index, WebCore::FrameIdentifier, CompletionHandler<void(RefPtr<FrameState>&&)>&&);
    void backForwardListContainsItem(WebCore::BackForwardItemIdentifier, CompletionHandler<void(bool)>&&);
    void backForwardListCounts(CompletionHandler<void(WebBackForwardListCounts&&)>&&);

    WeakPtr<WebPageProxy> m_page;
    std::optional<ActiveList> m_activeList;
    bool m_handlingProvisionalMessage { false };

};

using WebBackForwardListWrapper = WebBackForwardList;

#else // ENABLE(BACK_FORWARD_LIST_SWIFT)

// Avoid including WebKit-Swift.h in header files to avoid dependency loops.
class WebBackForwardList;
class WebBackForwardListMessageForwarder;

// This C++ stub object exists to forward API calls through to the Swift implementation.
// Although the BackForwardList is in Swift, we retain a C++
// API::Object subclass because Swift can't yet inherit from C++ -
// rdar://163102366
class WebBackForwardListWrapper : public API::ObjectImpl<API::Object::Type::BackForwardList> {
public:
    static Ref<WebBackForwardListWrapper> create(WebPageProxy& webPageProxy)
    {
        return adoptRef(*new WebBackForwardListWrapper(webPageProxy));
    }

    virtual ~WebBackForwardListWrapper();

    void removeAllItems();
    void clear();

    WebBackForwardListItem* WTF_NULLABLE currentItem() const;

    RefPtr<WebBackForwardListItem> itemAtDeltaFromCurrentIndex(int, AllowSkippingBackForwardItems = AllowSkippingBackForwardItems::Yes) const;
    RefPtr<WebBackForwardListItem> backItem() const;
    RefPtr<WebBackForwardListItem> forwardItem() const;

    Ref<API::Array> backList() const;
    Ref<API::Array> forwardList() const;

    unsigned backListCountForAPI() const;
    unsigned forwardListCountForAPI() const;

    Ref<API::Array> backListAsAPIArrayWithLimit(unsigned limit) const;
    Ref<API::Array> forwardListAsAPIArrayWithLimit(unsigned limit) const;

    String loggingString();

    WebBackForwardList& getImpl() { return *m_impl; }
    WebBackForwardListMessageForwarder& messageReceiver() const;

private:
    explicit WebBackForwardListWrapper(WebPageProxy&);

    std::unique_ptr<WebBackForwardList> m_impl;
    Ref<WebBackForwardListMessageForwarder> m_messageForwarder;
};

#endif // ENABLE(BACK_FORWARD_LIST_SWIFT)

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebBackForwardListWrapper)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BackForwardList; }
SPECIALIZE_TYPE_TRAITS_END()
