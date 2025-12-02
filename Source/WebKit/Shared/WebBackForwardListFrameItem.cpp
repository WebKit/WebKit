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
#include "WebBackForwardListItem.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

Ref<WebBackForwardListFrameItem> WebBackForwardListFrameItem::create(WebBackForwardListItem& item, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&& frameState)
{
    return adoptRef(*new WebBackForwardListFrameItem(item, parentItem, WTFMove(frameState)));
}

WebBackForwardListFrameItem::WebBackForwardListFrameItem(WebBackForwardListItem& item, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&& frameState)
    : m_backForwardListItem(item)
    , m_identifier(*frameState->frameItemID)
    , m_frameState(WTFMove(frameState))
    , m_parent(parentItem)
{
    m_frameState->itemID = item.identifier();
    auto result = allItems().add({ *m_frameState->frameItemID, *m_frameState->itemID }, *this);
    ASSERT_UNUSED(result, result.isNewEntry);
    for (auto& child : std::exchange(m_frameState->children, { }))
        m_children.append(WebBackForwardListFrameItem::create(item, this, WTFMove(child)));
}

WebBackForwardListFrameItem::~WebBackForwardListFrameItem()
{
    ASSERT(allItems().get({ *m_frameState->frameItemID, *m_frameState->itemID }) == this);
    allItems().remove({ *m_frameState->frameItemID, *m_frameState->itemID });
}

HashMap<std::pair<BackForwardFrameItemIdentifier, BackForwardItemIdentifier>, WeakRef<WebBackForwardListFrameItem>>& WebBackForwardListFrameItem::allItems()
{
    static MainRunLoopNeverDestroyed<HashMap<std::pair<BackForwardFrameItemIdentifier, BackForwardItemIdentifier>, WeakRef<WebBackForwardListFrameItem>>> items;
    return items;
}

WebBackForwardListFrameItem* WebBackForwardListFrameItem::itemForID(BackForwardItemIdentifier itemID, BackForwardFrameItemIdentifier frameItemID)
{
    return allItems().get({ frameItemID, itemID });
}

std::optional<FrameIdentifier> WebBackForwardListFrameItem::frameID() const
{
    return m_frameState->frameID;
}

const String& WebBackForwardListFrameItem::url() const
{
    return m_frameState->urlString;
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

RefPtr<WebBackForwardListFrameItem> WebBackForwardListFrameItem::protectedChildItemForFrameID(FrameIdentifier frameID)
{
    return childItemForFrameID(frameID);
}

WebBackForwardListItem* WebBackForwardListFrameItem::backForwardListItem() const
{
    return m_backForwardListItem.get();
}

RefPtr<WebBackForwardListItem> WebBackForwardListFrameItem::protectedBackForwardListItem() const
{
    return m_backForwardListItem.get();
}

void WebBackForwardListFrameItem::setChild(Ref<FrameState>&& frameState)
{
    ASSERT(m_backForwardListItem);
    Ref childItem = WebBackForwardListFrameItem::create(*protectedBackForwardListItem(), this, WTFMove(frameState));
    for (size_t i = 0; i < m_children.size(); i++) {
        if (m_children[i]->frameID() == childItem->m_frameState->frameID) {
            m_children[i] = WTFMove(childItem);
            return;
        }
    }
    m_children.append(WTFMove(childItem));
}

Ref<WebBackForwardListFrameItem> WebBackForwardListFrameItem::rootFrame()
{
    Ref rootFrame = *this;
    while (rootFrame->m_parent && rootFrame->m_parent->identifier().processIdentifier() == identifier().processIdentifier())
        rootFrame = *rootFrame->m_parent;
    return rootFrame;
}

Ref<WebBackForwardListFrameItem> WebBackForwardListFrameItem::mainFrame()
{
    Ref mainFrame = *this;
    while (mainFrame->m_parent)
        mainFrame = *mainFrame->m_parent;
    return mainFrame;
}

Ref<WebBackForwardListFrameItem> WebBackForwardListFrameItem::protectedMainFrame()
{
    return mainFrame();
}

void WebBackForwardListFrameItem::setWasRestoredFromSession()
{
    m_frameState->wasRestoredFromSession = true;
    for (auto& child : m_children)
        child->setWasRestoredFromSession();
}

void WebBackForwardListFrameItem::setFrameState(Ref<FrameState>&& frameState)
{
    m_frameState = WTFMove(frameState);
    m_frameState->children.clear();
}

Ref<FrameState> WebBackForwardListFrameItem::copyFrameStateWithChildren()
{
    Ref frameState = protectedFrameState()->copy();
    ASSERT(frameState->children.isEmpty());
    for (auto& child : m_children)
        frameState->children.append(child->copyFrameStateWithChildren());
    return frameState;
}

bool WebBackForwardListFrameItem::sharesAncestor(WebBackForwardListFrameItem& frameItem) const
{
    HashSet<WebCore::BackForwardFrameItemIdentifier> currentAncestors;
    for (RefPtr currentAncestor = m_parent.get(); currentAncestor; currentAncestor = currentAncestor->m_parent.get())
        currentAncestors.add(currentAncestor->m_identifier);

    for (RefPtr frameItemAncestor = frameItem.m_parent.get(); frameItemAncestor; frameItemAncestor = frameItemAncestor->m_parent.get()) {
        if (currentAncestors.contains(frameItemAncestor->m_identifier))
            return true;
    }
    return false;
}

String WebBackForwardListFrameItem::loggingString()
{
    return loggingStringAtIndent(1);
}

String WebBackForwardListFrameItem::loggingStringAtIndent(size_t indent)
{
    StringBuilder indentBuilder;
    for (size_t j = 0; j < indent; ++j)
        indentBuilder.append("   "_s);
    String indentString = indentBuilder.toString();

    StringBuilder builder;
    {
        uint64_t calculatedFrameID = frameID() ? frameID()->toRawValue() : 0;
        builder.append(makeString(url(), " ("_s, String::number(calculatedFrameID), ")"_s));
        if (!m_frameState->target.isEmpty())
            builder.append(makeString(" in "_s, m_frameState->target));
        builder.append('\n');
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        Ref child = m_children[i];
        auto childString = child->loggingStringAtIndent(indent + 1);
        builder.append(makeString(indentString, String::number(i), " - "_s, childString));
    }

    return builder.toString();
}

} // namespace WebKit
