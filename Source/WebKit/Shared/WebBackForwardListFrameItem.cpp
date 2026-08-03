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
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessProxy.h"
#include <WebCore/SecurityOrigin.h>
#include <wtf/Borrow.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

Ref<WebBackForwardListFrameItem> WebBackForwardListFrameItem::create(WebBackForwardListItem& item, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&& frameState)
{
    return adoptRef(*new WebBackForwardListFrameItem(item, parentItem, WTF::move(frameState)));
}

WebBackForwardListFrameItem::WebBackForwardListFrameItem(WebBackForwardListItem& item, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&& frameState)
    : m_backForwardListItem(item)
    , m_identifier(*frameState->frameItemID)
    , m_frameState(WTF::move(frameState))
    , m_parent(parentItem)
{
    m_frameState->itemID = item.identifier();
    auto result = allItems().add({ *m_frameState->frameItemID, *m_frameState->itemID }, *this);
    ASSERT_UNUSED(result, result.isNewEntry);
    for (auto& child : std::exchange(m_frameState->children, { }))
        m_children.append(WebBackForwardListFrameItem::create(item, this, WTF::move(child)));
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

WebBackForwardListFrameItem* WebBackForwardListFrameItem::childItemForFrameItemID(BackForwardFrameItemIdentifier frameItemID)
{
    if (m_identifier == frameItemID)
        return this;
    for (auto& child : m_children) {
        if (auto* childFrameItem = child->childItemForFrameItemID(frameItemID))
            return childFrameItem;
    }
    return nullptr;
}

bool WebBackForwardListFrameItem::containsItemSequenceNumber(int64_t itemSequenceNumber) const
{
    if (m_frameState->itemSequenceNumber == itemSequenceNumber)
        return true;
    for (auto& child : m_children) {
        if (child->containsItemSequenceNumber(itemSequenceNumber))
            return true;
    }
    return false;
}

WebBackForwardListFrameItem* WebBackForwardListFrameItem::childItemAtIndex(uint64_t index)
{
    if (index >= m_children.size())
        return nullptr;
    return m_children[index].ptr();
}

WebBackForwardListFrameItem* WebBackForwardListFrameItem::childItemForFrameName(const String& frameName)
{
    if (frameName.isEmpty())
        return nullptr;
    for (auto& child : m_children) {
        if (child->frameState().target == frameName)
            return child.ptr();
    }
    return nullptr;
}

WebBackForwardListItem* WebBackForwardListFrameItem::backForwardListItem() const
{
    return m_backForwardListItem.get();
}

void WebBackForwardListFrameItem::setChild(Ref<FrameState>&& frameState)
{
    ASSERT(m_backForwardListItem);
    Ref childItem = WebBackForwardListFrameItem::create(*protect(backForwardListItem()), this, WTF::move(frameState));
    for (size_t i = 0; i < m_children.size(); i++) {
        if (m_children[i]->frameID() == childItem->m_frameState->frameID) {
            borrow(m_children)[i] = WTF::move(childItem);
            return;
        }
    }
    m_children.insert(insertionIndexForChild(childItem->frameID()), WTF::move(childItem));
}

size_t WebBackForwardListFrameItem::insertionIndexForChild(std::optional<WebCore::FrameIdentifier> frameID) const
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    RefPtr parentFrame = frame ? frame->parentFrame() : nullptr;
    if (!parentFrame)
        return m_children.size();

    auto& siblingFrames = parentFrame->childFrames();
    auto iterator = siblingFrames.find(frame.get());
    if (iterator == siblingFrames.end()) {
        ASSERT_NOT_REACHED("This frame should be in its parent's child frames");
        return m_children.size();
    }

    HashSet<WebCore::FrameIdentifier> laterSiblingFrameIDs;
    for (++iterator; iterator != siblingFrames.end(); ++iterator)
        laterSiblingFrameIDs.add((*iterator)->frameID());
    if (laterSiblingFrameIDs.isEmpty())
        return m_children.size();

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (auto childFrameID = m_children[i]->frameID(); childFrameID && laterSiblingFrameIDs.contains(*childFrameID))
            return i;
    }
    return m_children.size();
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

void WebBackForwardListFrameItem::setWasRestoredFromSession()
{
    m_frameState->wasRestoredFromSession = true;
    for (auto& child : m_children)
        child->setWasRestoredFromSession();
}

void WebBackForwardListFrameItem::updateFrameStatePayload(Ref<FrameState>&& frameState)
{
    m_frameState->replacePayloadFrom(WTF::move(frameState));
}

void WebBackForwardListFrameItem::updateFrameID(FrameIdentifier newFrameID)
{
    m_frameState->frameID = newFrameID;
}

Ref<FrameState> WebBackForwardListFrameItem::copyFrameState()
{
    Ref frameState = protect(this->frameState())->copy();
    ASSERT(frameState->children.isEmpty());
    return frameState;
}

Ref<FrameState> WebBackForwardListFrameItem::copyFrameStateWithChildren()
{
    Ref frameState = copyFrameState();
    for (auto& child : borrow(m_children))
        frameState->children.append(child->copyFrameStateWithChildren());
    return frameState;
}

Ref<FrameState> WebBackForwardListFrameItem::copyFrameStateForReplacingDocument(const URL& url)
{
    if (SecurityOrigin::create(URL { m_frameState->urlString })->isSameOriginAs(SecurityOrigin::create(url)))
        return copyFrameState();
    Ref frameState = FrameState::create();
    frameState->itemID = m_frameState->itemID;
    frameState->frameItemID = m_frameState->frameItemID;
    frameState->frameID = m_frameState->frameID;
    // These describe the entry rather than its document, so HistoryItem::reset() keeps them.
    frameState->wasCreatedByJSWithoutUserInteraction = m_frameState->wasCreatedByJSWithoutUserInteraction;
    frameState->wasRestoredFromSession = m_frameState->wasRestoredFromSession;
    return frameState;
}

// When the UI process drives back/forward loads it sends each frame its own state, so other processes only need identifiers.
Ref<FrameState> WebBackForwardListFrameItem::copyFrameStateWithChildrenForProcess(WebPageProxy& page, WebProcessProxy& process)
{
    if (!protect(page.preferences())->shouldUseUIProcessForBackForwardItemLoading())
        return copyFrameStateWithChildren();

    RefPtr frame = m_parent ? WebFrameProxy::webFrame(frameID()) : page.mainFrame();
    bool processHostsFrame = frame && frame->page() == &page && (&frame->process() == &process || &frame->provisionalLoadProcess() == &process);

    Ref frameState = processHostsFrame ? copyFrameState() : m_frameState->copyIdentifiersOnly();
    for (auto& child : borrow(m_children))
        frameState->children.append(child->copyFrameStateWithChildrenForProcess(page, process));
    return frameState;
}

bool WebBackForwardListFrameItem::sharesAncestor(WebBackForwardListFrameItem& frameItem) const
{
    HashSet<WebCore::BackForwardFrameItemIdentifier> currentAncestors;
    for (RefPtr currentAncestor = m_parent.get(); currentAncestor; currentAncestor = currentAncestor->m_parent.get())
        currentAncestors.add(currentAncestor->m_identifier);

    for (auto* frameItemAncestor = frameItem.m_parent.get(); frameItemAncestor; frameItemAncestor = frameItemAncestor->m_parent.get()) {
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
        String frameIDString = String::number(frameID() ? frameID()->toUInt64() : 0);
        String frameItemIDString = m_identifier.loggingString();
        builder.append("FrameItemID:"_s, frameItemIDString, ", URL:"_s, url(), ", FrameID:"_s, frameIDString);
        if (!m_frameState->target.isEmpty())
            builder.append(", FrameUniqueName:"_s, m_frameState->target);
        if (m_frameState->wasCreatedByJSWithoutUserInteraction)
            builder.append(" (no user gesture)"_s);
        builder.append('\n');
        builder.append(indentString);
        builder.append("("_s, m_frameState->title, ")\n"_s);
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        Ref child = m_children[i];
        auto childString = child->loggingStringAtIndent(indent + 1);
        builder.append(indentString, String::number(i), " - "_s, childString);
    }

    return builder.toString();
}

} // namespace WebKit
