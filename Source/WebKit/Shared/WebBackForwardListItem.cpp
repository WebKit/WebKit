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
#include "WebBackForwardListFrameItem.h"
#include "WebFrameProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <wtf/DebugUtilities.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

Ref<WebBackForwardListItem> WebBackForwardListItem::create(Ref<FrameState>&& mainFrameState, WebPageProxyIdentifier pageID)
{
    RELEASE_ASSERT(RunLoop::isMain());
    return adoptRef(*new WebBackForwardListItem(WTFMove(mainFrameState), pageID));
}

WebBackForwardListItem::WebBackForwardListItem(Ref<FrameState>&& mainFrameState, WebPageProxyIdentifier pageID)
    : m_rootFrameItem(WebBackForwardListFrameItem::create(this, nullptr, WTFMove(mainFrameState)))
    , m_pageID(pageID)
    , m_lastProcessIdentifier(m_rootFrameItem->frameState().identifier->processIdentifier())
{
    auto result = allItems().add(*m_rootFrameItem->frameState().identifier, *this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

WebBackForwardListItem::~WebBackForwardListItem()
{
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(allItems().get(*m_rootFrameItem->frameState().identifier) == this);
    allItems().remove(*m_rootFrameItem->frameState().identifier);
    removeFromBackForwardCache();
}

HashMap<BackForwardItemIdentifier, WeakRef<WebBackForwardListItem>>& WebBackForwardListItem::allItems()
{
    RELEASE_ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<BackForwardItemIdentifier, WeakRef<WebBackForwardListItem>>> items;
    return items;
}

WebBackForwardListItem* WebBackForwardListItem::itemForID(const BackForwardItemIdentifier& identifier)
{
    return allItems().get(identifier);
}

static const FrameState* childItemWithDocumentSequenceNumber(const FrameState& frameState, int64_t number)
{
    for (auto& child : frameState.children) {
        if (child->documentSequenceNumber == number)
            return child.ptr();
    }

    return nullptr;
}

static const FrameState* childItemWithTarget(const FrameState& frameState, const String& target)
{
    for (auto& child : frameState.children) {
        if (child->target == target)
            return child.ptr();
    }

    return nullptr;
}

static bool documentTreesAreEqual(const FrameState& a, const FrameState& b)
{
    if (a.documentSequenceNumber != b.documentSequenceNumber)
        return false;

    if (a.children.size() != b.children.size())
        return false;

    for (auto& child : a.children) {
        const FrameState* otherChild = childItemWithDocumentSequenceNumber(b, child->documentSequenceNumber);
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

    Ref mainFrameState = m_rootFrameItem->frameState();
    Ref otherMainFrameState = other.m_rootFrameItem->frameState();

    if (mainFrameState->stateObjectData || otherMainFrameState->stateObjectData)
        return mainFrameState->documentSequenceNumber == otherMainFrameState->documentSequenceNumber;

    URL url = URL({ }, mainFrameState->urlString);
    URL otherURL = URL({ }, otherMainFrameState->urlString);

    if ((url.hasFragmentIdentifier() || otherURL.hasFragmentIdentifier()) && equalIgnoringFragmentIdentifier(url, otherURL))
        return mainFrameState->documentSequenceNumber == otherMainFrameState->documentSequenceNumber;

    return documentTreesAreEqual(mainFrameState, otherMainFrameState);
}

static bool hasSameFrames(const FrameState& a, const FrameState& b)
{
    if (a.target != b.target)
        return false;

    if (a.children.size() != b.children.size())
        return false;

    for (auto& child : a.children) {
        if (!childItemWithTarget(b, child->target))
            return false;
    }

    return true;
}

bool WebBackForwardListItem::itemIsClone(const WebBackForwardListItem& other)
{
    // The following logic must be kept in sync with WebCore::HistoryItem::itemsAreClones().

    if (this == &other)
        return false;

    const auto& mainFrameState = m_rootFrameItem->frameState();
    const auto& otherMainFrameState = other.m_rootFrameItem->frameState();

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

WebCore::BackForwardItemIdentifier WebBackForwardListItem::itemID() const
{
    return *m_rootFrameItem->frameState().identifier;
}

void WebBackForwardListItem::setRootFrameState(Ref<FrameState>&& mainFrameState)
{
    protectedRootFrameItem()->setFrameState(WTFMove(mainFrameState));
}

FrameState& WebBackForwardListItem::rootFrameState() const
{
    return m_rootFrameItem->frameState();
}

const String& WebBackForwardListItem::originalURL() const
{
    if (m_isRemoteFrameNavigation)
        return emptyString();
    return m_rootFrameItem->frameState().originalURLString;
}

const String& WebBackForwardListItem::url() const
{
    if (m_isRemoteFrameNavigation)
        return emptyString();
    return m_rootFrameItem->frameState().urlString;
}

const String& WebBackForwardListItem::title() const
{
    if (m_isRemoteFrameNavigation)
        return emptyString();
    return m_rootFrameItem->frameState().title;
}

bool WebBackForwardListItem::wasCreatedByJSWithoutUserInteraction() const
{
    return m_rootFrameItem->frameState().wasCreatedByJSWithoutUserInteraction;
}

void WebBackForwardListItem::setWasRestoredFromSession()
{
    protectedRootFrameItem()->setWasRestoredFromSession();
}

Ref<WebBackForwardListFrameItem> WebBackForwardListItem::protectedRootFrameItem()
{
    return m_rootFrameItem.get();
}

#if !LOG_DISABLED
String WebBackForwardListItem::loggingString()
{
    return makeString("Back/forward item ID "_s, itemID().toString(), ", original URL "_s, originalURL(), ", current URL "_s, url(), m_backForwardCacheEntry ? "(has a back/forward cache entry)"_s : ""_s);
}
#endif // !LOG_DISABLED

} // namespace WebKit
