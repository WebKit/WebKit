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

#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FrameIdentifier.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>

namespace WebKit {

class FrameState;
class WebBackForwardListItem;

class WebBackForwardListFrameItem : public RefCountedAndCanMakeWeakPtr<WebBackForwardListFrameItem> {
public:
    static Ref<WebBackForwardListFrameItem> create(WebBackForwardListItem*, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&&);
    ~WebBackForwardListFrameItem();

    static WebBackForwardListFrameItem* itemForID(WebCore::BackForwardItemIdentifier);
    static HashMap<WebCore::BackForwardItemIdentifier, WeakRef<WebBackForwardListFrameItem>>& allItems();

    FrameState& frameState() const { return m_frameState; }
    void setFrameState(Ref<FrameState>&&);

    std::optional<WebCore::FrameIdentifier> frameID() const;
    WebCore::BackForwardItemIdentifier identifier() const;

    WebBackForwardListFrameItem* parent() const { return m_parent.get(); }
    WebBackForwardListFrameItem& rootFrame();
    WebBackForwardListFrameItem* childItemForFrameID(WebCore::FrameIdentifier);

    WebBackForwardListItem* backForwardListItem() const;
    RefPtr<WebBackForwardListItem> protectedBackForwardListItem() const;

    void setChild(Ref<FrameState>&&);

    void setWasRestoredFromSession();

private:
    WebBackForwardListFrameItem(WebBackForwardListItem*, WebBackForwardListFrameItem* parentItem, Ref<FrameState>&&);

    void updateChildFrameState(Ref<FrameState>&&);

    WeakPtr<WebBackForwardListItem> m_backForwardListItem;
    Ref<FrameState> m_frameState;
    WeakPtr<WebBackForwardListFrameItem> m_parent;
    Vector<Ref<WebBackForwardListFrameItem>> m_children;
};

} // namespace WebKit
