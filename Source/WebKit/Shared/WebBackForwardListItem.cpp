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

#include <WebCore/URL.h>

namespace WebKit {

static uint64_t highestItemID = 0;

Ref<WebBackForwardListItem> WebBackForwardListItem::create(BackForwardListItemState&& backForwardListItemState, uint64_t pageID)
{
    return adoptRef(*new WebBackForwardListItem(WTFMove(backForwardListItemState), pageID));
}

WebBackForwardListItem::WebBackForwardListItem(BackForwardListItemState&& backForwardListItemState, uint64_t pageID)
    : m_itemState(WTFMove(backForwardListItemState))
    , m_pageID(pageID)
{
    if (m_itemState.identifier > highestItemID)
        highestItemID = m_itemState.identifier;
}

WebBackForwardListItem::~WebBackForwardListItem()
{
}

static const FrameState* childItemWithDocumentSequenceNumber(const FrameState& frameState, int64_t number)
{
    for (const auto& child : frameState.children) {
        if (child.documentSequenceNumber == number)
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

    // The following logic must be kept in sync with WebCore::HistoryItem::shouldDoSameDocumentNavigationTo.

    const FrameState& mainFrameState = m_itemState.pageState.mainFrameState;
    const FrameState& otherMainFrameState = other.m_itemState.pageState.mainFrameState;

    if (mainFrameState.stateObjectData || otherMainFrameState.stateObjectData)
        return mainFrameState.documentSequenceNumber == otherMainFrameState.documentSequenceNumber;

    WebCore::URL url = WebCore::URL(WebCore::ParsedURLString, mainFrameState.urlString);
    WebCore::URL otherURL = WebCore::URL(WebCore::ParsedURLString, otherMainFrameState.urlString);

    if ((url.hasFragmentIdentifier() || otherURL.hasFragmentIdentifier()) && equalIgnoringFragmentIdentifier(url, otherURL))
        return mainFrameState.documentSequenceNumber == otherMainFrameState.documentSequenceNumber;

    return documentTreesAreEqual(mainFrameState, otherMainFrameState);
}

uint64_t WebBackForwardListItem::highestUsedItemID()
{
    return highestItemID;
}

} // namespace WebKit
