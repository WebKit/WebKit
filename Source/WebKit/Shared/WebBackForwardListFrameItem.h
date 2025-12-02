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

#pragma once

#include <WebCore/BackForwardFrameItemIdentifier.h>
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FrameIdentifier.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainReleaseSwift.h>

namespace WebKit {

class FrameState;
class WebBackForwardListItem;

class WebBackForwardListFrameItem : public RefCountedAndCanMakeWeakPtr<WebBackForwardListFrameItem> {
public:
    static Ref<WebBackForwardListFrameItem> create(WebBackForwardListItem&, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&&);
    ~WebBackForwardListFrameItem();

    static WebBackForwardListFrameItem* itemForID(WebCore::BackForwardItemIdentifier, WebCore::BackForwardFrameItemIdentifier);

    FrameState& frameState() const { return m_frameState; }
    Ref<FrameState> protectedFrameState() const { return m_frameState; }
    void setFrameState(Ref<FrameState>&&);

    Ref<FrameState> copyFrameStateWithChildren();

    std::optional<WebCore::FrameIdentifier> frameID() const;
    WebCore::BackForwardFrameItemIdentifier identifier() const { return m_identifier; }
    const String& url() const;

    WebBackForwardListFrameItem* parent() const { return m_parent.get(); }
    RefPtr<WebBackForwardListFrameItem> protectedParent() const { return m_parent.get(); }
    void setParent(WebBackForwardListFrameItem* parent) { m_parent = parent; }
    bool sharesAncestor(WebBackForwardListFrameItem&) const;

    Ref<WebBackForwardListFrameItem> rootFrame();
    Ref<WebBackForwardListFrameItem> mainFrame();
    Ref<WebBackForwardListFrameItem> protectedMainFrame();
    WebBackForwardListFrameItem* childItemForFrameID(WebCore::FrameIdentifier);
    RefPtr<WebBackForwardListFrameItem> protectedChildItemForFrameID(WebCore::FrameIdentifier);

    WebBackForwardListItem* backForwardListItem() const;
    RefPtr<WebBackForwardListItem> protectedBackForwardListItem() const;

    void setChild(Ref<FrameState>&&);
    void clearChildren() { m_children.clear(); }

    void setWasRestoredFromSession();

    String loggingString();

private:
    WebBackForwardListFrameItem(WebBackForwardListItem&, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&&);

    String loggingStringAtIndent(size_t);

    static HashMap<std::pair<WebCore::BackForwardFrameItemIdentifier, WebCore::BackForwardItemIdentifier>, WeakRef<WebBackForwardListFrameItem>>& allItems();

    WeakPtr<WebBackForwardListItem> m_backForwardListItem;
    const WebCore::BackForwardFrameItemIdentifier m_identifier;
    Ref<FrameState> m_frameState;
    WeakPtr<WebBackForwardListFrameItem> m_parent;
    Vector<Ref<WebBackForwardListFrameItem>> m_children;

} SWIFT_SHARED_REFERENCE(refWebBackForwardListFrameItem, derefWebBackForwardListFrameItem);

} // namespace WebKit

inline void refWebBackForwardListFrameItem(WebKit::WebBackForwardListFrameItem* obj)
{
    WTF::ref(obj);
}

inline void derefWebBackForwardListFrameItem(WebKit::WebBackForwardListFrameItem* obj)
{
    WTF::deref(obj);
}
