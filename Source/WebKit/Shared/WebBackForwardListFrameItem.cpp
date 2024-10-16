/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WebBackForwardListFrameItem.h"

#include "SessionState.h"

namespace WebKit {
using namespace WebCore;

Ref<WebBackForwardListFrameItem> WebBackForwardListFrameItem::create(WebBackForwardListItem* item, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&& frameState)
{
    return adoptRef(*new WebBackForwardListFrameItem(item, parentItem, WTFMove(frameState)));
}

WebBackForwardListFrameItem::WebBackForwardListFrameItem(WebBackForwardListItem* item, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&& frameState)
    : m_backForwardListItem(item)
    , m_frameState(WTFMove(frameState))
    , m_parent(parentItem)
{
    auto result = allItems().add(*m_frameState->identifier, *this);
    ASSERT_UNUSED(result, result.isNewEntry);
    for (auto& child : m_frameState->children)
        m_children.append(WebBackForwardListFrameItem::create(item, this, child.copyRef()));
}

WebBackForwardListFrameItem::~WebBackForwardListFrameItem()
{
    ASSERT(allItems().get(*m_frameState->identifier) == this);
    allItems().remove(*m_frameState->identifier);
}

UncheckedKeyHashMap<BackForwardItemIdentifier, WeakRef<WebBackForwardListFrameItem>>& WebBackForwardListFrameItem::allItems()
{
    static MainThreadNeverDestroyed<UncheckedKeyHashMap<BackForwardItemIdentifier, WeakRef<WebBackForwardListFrameItem>>> items;
    return items;
}

WebBackForwardListFrameItem* WebBackForwardListFrameItem::itemForID(BackForwardItemIdentifier identifier)
{
    return allItems().get(identifier);
}

std::optional<FrameIdentifier> WebBackForwardListFrameItem::frameID() const
{
    return m_frameState->frameID;
}

BackForwardItemIdentifier WebBackForwardListFrameItem::identifier() const
{
    return *m_frameState->identifier;
}

WebBackForwardListFrameItem* WebBackForwardListFrameItem::childItemForFrameID(FrameIdentifier frameID)
{
    if (m_frameState->frameID == frameID)
        return this;
    for (auto& child : m_children) {
        if (auto* childFrameItem = child->childItemForFrameID(frameID))
            return childFrameItem;
    }
    return nullptr;
}

WebBackForwardListItem* WebBackForwardListFrameItem::backForwardListItem() const
{
    return m_backForwardListItem.get();
}

RefPtr<WebBackForwardListItem> WebBackForwardListFrameItem::protectedBackForwardListItem() const
{
    return m_backForwardListItem.get();
}

void WebBackForwardListFrameItem::addChild(Ref<FrameState>&& frameState)
{
    m_frameState->children.append(frameState.copyRef());
    m_children.append(WebBackForwardListFrameItem::create(protectedBackForwardListItem().get(), this, WTFMove(frameState)));
}

WebBackForwardListFrameItem& WebBackForwardListFrameItem::rootFrame()
{
    Ref rootFrame = *this;
    while (rootFrame->m_parent && rootFrame->m_parent->identifier().processIdentifier() == identifier().processIdentifier())
        rootFrame = *rootFrame->m_parent;
    return rootFrame.get();
}

} // namespace WebKit
