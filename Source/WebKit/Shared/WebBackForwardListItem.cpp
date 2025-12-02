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

#include "BrowsingContextGroup.h"
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

Ref<WebBackForwardListItem> WebBackForwardListItem::create(Ref<FrameState>&& mainFrameState, WebPageProxyIdentifier pageID, std::optional<FrameIdentifier> navigatedFrameID, BrowsingContextGroup* browsingContextGroup)
{
    RELEASE_ASSERT(RunLoop::isMain());
    return adoptRef(*new WebBackForwardListItem(WTFMove(mainFrameState), pageID, navigatedFrameID, browsingContextGroup));
}

WebBackForwardListItem::WebBackForwardListItem(Ref<FrameState>&& mainFrameState, WebPageProxyIdentifier pageID, std::optional<FrameIdentifier> navigatedFrameID, BrowsingContextGroup* browsingContextGroup)
    : m_identifier(*mainFrameState->itemID)
    , m_mainFrameItem(WebBackForwardListFrameItem::create(*this, nullptr, WTFMove(mainFrameState)))
    , m_navigatedFrameID(navigatedFrameID)
    , m_pageID(pageID)
    , m_lastProcessIdentifier(navigatedFrameItem().identifier().processIdentifier())
    , m_browsingContextGroup(browsingContextGroup)
{
    auto result = allItems().add(m_identifier, *this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

WebBackForwardListItem::~WebBackForwardListItem()
{
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(allItems().get(m_identifier) == this);
    allItems().remove(m_identifier);
    removeFromBackForwardCache();
}

HashMap<BackForwardItemIdentifier, WeakRef<WebBackForwardListItem>>& WebBackForwardListItem::allItems()
{
    RELEASE_ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<BackForwardItemIdentifier, WeakRef<WebBackForwardListItem>>> items;
    return items;
}

WebBackForwardListItem* WebBackForwardListItem::itemForID(BackForwardItemIdentifier identifier)
{
    return allItems().get(identifier);
}

static const FrameState* childItemWithTarget(const FrameState& frameState, const String& target)
{
    for (auto& child : frameState.children) {
        if (child->target == target)
            return child.ptr();
    }

    return nullptr;
}

bool WebBackForwardListItem::itemIsInSameDocument(const WebBackForwardListItem& other) const
{
    if (m_pageID != other.m_pageID)
        return false;

    // The following logic must be kept in sync with WebCore::HistoryItem::shouldDoSameDocumentNavigationTo().
    Ref mainFrameState = this->mainFrameState();
    Ref otherMainFrameState = other.mainFrameState();

    return mainFrameState->documentSequenceNumber == otherMainFrameState->documentSequenceNumber;
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

    Ref mainFrameState = this->mainFrameState();
    Ref otherMainFrameState = other.mainFrameState();

    if (mainFrameState->itemSequenceNumber != otherMainFrameState->itemSequenceNumber)
        return false;

    return hasSameFrames(mainFrameState, otherMainFrameState);
}

void WebBackForwardListItem::wasRemovedFromBackForwardList()
{
    removeFromBackForwardCache();
}

void WebBackForwardListItem::removeFromBackForwardCache()
{
    if (RefPtr backForwardCacheEntry = m_backForwardCacheEntry) {
        if (RefPtr backForwardCache = backForwardCacheEntry->backForwardCache())
            backForwardCache->removeEntry(*this);
    }
    ASSERT(!m_backForwardCacheEntry);
}

RefPtr<WebBackForwardCacheEntry> WebBackForwardListItem::protectedBackForwardCacheEntry() const
{
    return m_backForwardCacheEntry;
}

void WebBackForwardListItem::setBackForwardCacheEntry(RefPtr<WebBackForwardCacheEntry>&& backForwardCacheEntry)
{
    m_backForwardCacheEntry = WTFMove(backForwardCacheEntry);
}

SuspendedPageProxy* WebBackForwardListItem::suspendedPage() const
{
    return m_backForwardCacheEntry ? m_backForwardCacheEntry->suspendedPage() : nullptr;
}

Ref<FrameState> WebBackForwardListItem::navigatedFrameState() const
{
    return protectedNavigatedFrameItem()->copyFrameStateWithChildren();
}

Ref<FrameState> WebBackForwardListItem::mainFrameState() const
{
    return m_mainFrameItem->copyFrameStateWithChildren();
}

const String& WebBackForwardListItem::originalURL() const
{
    if (m_isRemoteFrameNavigation)
        return emptyString();
    return mainFrameItem().frameState().originalURLString;
}

const String& WebBackForwardListItem::url() const
{
    if (m_isRemoteFrameNavigation)
        return emptyString();
    return mainFrameItem().frameState().urlString;
}

const String& WebBackForwardListItem::title() const
{
    if (m_isRemoteFrameNavigation)
        return emptyString();
    return mainFrameItem().frameState().title;
}

bool WebBackForwardListItem::wasCreatedByJSWithoutUserInteraction() const
{
    return navigatedFrameItem().frameState().wasCreatedByJSWithoutUserInteraction;
}

void WebBackForwardListItem::setWasRestoredFromSession()
{
    m_mainFrameItem->setWasRestoredFromSession();
}

WebBackForwardListFrameItem& WebBackForwardListItem::navigatedFrameItem() const
{
    if (m_navigatedFrameID) {
        if (auto* childItem = m_mainFrameItem->childItemForFrameID(*m_navigatedFrameID))
            return *childItem;
    }
    return m_mainFrameItem;
}

Ref<WebBackForwardListFrameItem> WebBackForwardListItem::protectedNavigatedFrameItem() const
{
    return navigatedFrameItem();
}

WebBackForwardListFrameItem& WebBackForwardListItem::mainFrameItem() const
{
    return m_mainFrameItem;
}

Ref<WebBackForwardListFrameItem> WebBackForwardListItem::protectedMainFrameItem() const
{
    return m_mainFrameItem;
}

String WebBackForwardListItem::loggingString()
{
    return m_mainFrameItem->loggingString();
}

} // namespace WebKit
