/*
 * Copyright (C) 2010-2011, 2016 Apple Inc. All rights reserved.
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
#include "WebBackForwardListItem.h"

#include "SuspendedPageProxy.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardCacheEntry.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <wtf/DebugUtilities.h>
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

Ref<WebBackForwardListItem> WebBackForwardListItem::create(BackForwardListItemState&& backForwardListItemState, WebPageProxyIdentifier pageID)
{
    return adoptRef(*new WebBackForwardListItem(WTFMove(backForwardListItemState), pageID));
}

WebBackForwardListItem::WebBackForwardListItem(BackForwardListItemState&& backForwardListItemState, WebPageProxyIdentifier pageID)
    : m_itemState(WTFMove(backForwardListItemState))
    , m_pageID(pageID)
    , m_lastProcessIdentifier(m_itemState.identifier.processIdentifier)
{
    auto result = allItems().add(m_itemState.identifier, this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

WebBackForwardListItem::~WebBackForwardListItem()
{
    ASSERT(allItems().get(m_itemState.identifier) == this);
    allItems().remove(m_itemState.identifier);
    removeFromBackForwardCache();
}

HashMap<BackForwardItemIdentifier, WebBackForwardListItem*>& WebBackForwardListItem::allItems()
{
    RELEASE_ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<BackForwardItemIdentifier, WebBackForwardListItem*>> items;
    return items;
}

WebBackForwardListItem* WebBackForwardListItem::itemForID(const BackForwardItemIdentifier& identifier)
{
    return allItems().get(identifier);
}

static const FrameState* childItemWithDocumentSequenceNumber(const FrameState& frameState, int64_t number)
{
    for (const auto& child : frameState.children) {
        if (child.documentSequenceNumber == number)
            return &child;
    }

    return nullptr;
}

static const FrameState* childItemWithTarget(const FrameState& frameState, const String& target)
{
    for (const auto& child : frameState.children) {
        if (child.target == target)
            return &child;
    }

    return nullptr;
}

static bool documentTreesAreEqual(const FrameState& a, const FrameState& b)
{
    if (a.documentSequenceNumber != b.documentSequenceNumber)
        return false;

    if (a.children.size() != b.children.size())
        return false;

    for (const auto& child : a.children) {
        const FrameState* otherChild = childItemWithDocumentSequenceNumber(b, child.documentSequenceNumber);
        if (!otherChild || !documentTreesAreEqual(child, *otherChild))
            return false;
    }

    return true;
}

bool WebBackForwardListItem::itemIsInSameDocument(const WebBackForwardListItem& other) const
{
    if (m_pageID != other.m_pageID)
        return false;

    // The following logic must be kept in sync with WebCore::HistoryItem::shouldDoSameDocumentNavigationTo().

    const FrameState& mainFrameState = m_itemState.pageState.mainFrameState;
    const FrameState& otherMainFrameState = other.m_itemState.pageState.mainFrameState;

    if (mainFrameState.stateObjectData || otherMainFrameState.stateObjectData)
        return mainFrameState.documentSequenceNumber == otherMainFrameState.documentSequenceNumber;

    URL url = URL({ }, mainFrameState.urlString);
    URL otherURL = URL({ }, otherMainFrameState.urlString);

    if ((url.hasFragmentIdentifier() || otherURL.hasFragmentIdentifier()) && equalIgnoringFragmentIdentifier(url, otherURL))
        return mainFrameState.documentSequenceNumber == otherMainFrameState.documentSequenceNumber;

    return documentTreesAreEqual(mainFrameState, otherMainFrameState);
}

static bool hasSameFrames(const FrameState& a, const FrameState& b)
{
    if (a.target != b.target)
        return false;

    if (a.children.size() != b.children.size())
        return false;

    for (const auto& child : a.children) {
        if (!childItemWithTarget(b, child.target))
            return false;
    }

    return true;
}

bool WebBackForwardListItem::itemIsClone(const WebBackForwardListItem& other)
{
    // The following logic must be kept in sync with WebCore::HistoryItem::itemsAreClones().

    if (this == &other)
        return false;

    const FrameState& mainFrameState = m_itemState.pageState.mainFrameState;
    const FrameState& otherMainFrameState = other.m_itemState.pageState.mainFrameState;

    if (mainFrameState.itemSequenceNumber != otherMainFrameState.itemSequenceNumber)
        return false;

    return hasSameFrames(mainFrameState, otherMainFrameState);
}

void WebBackForwardListItem::wasRemovedFromBackForwardList()
{
    removeFromBackForwardCache();
}

void WebBackForwardListItem::removeFromBackForwardCache()
{
    if (m_backForwardCacheEntry)
        m_backForwardCacheEntry->backForwardCache().removeEntry(*this);
    ASSERT(!m_backForwardCacheEntry);
}

void WebBackForwardListItem::setBackForwardCacheEntry(std::unique_ptr<WebBackForwardCacheEntry>&& backForwardCacheEntry)
{
    m_backForwardCacheEntry = WTFMove(backForwardCacheEntry);
}

SuspendedPageProxy* WebBackForwardListItem::suspendedPage() const
{
    return m_backForwardCacheEntry ? m_backForwardCacheEntry->suspendedPage() : nullptr;
}

#if !LOG_DISABLED
const char* WebBackForwardListItem::loggingString()
{
    return debugString("Back/forward item ID ", itemID().logString(), ", original URL ", originalURL(), ", current URL ", url(), m_backForwardCacheEntry ? "(has a back/forward cache entry)" : "");
}
#endif // !LOG_DISABLED

} // namespace WebKit
