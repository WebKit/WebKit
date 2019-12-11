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

#pragma once

#include "APIObject.h"
#include "SessionState.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace API {
class Data;
}

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class SuspendedPageProxy;
class WebBackForwardCache;
class WebBackForwardCacheEntry;

class WebBackForwardListItem : public API::ObjectImpl<API::Object::Type::BackForwardListItem> {
public:
    static Ref<WebBackForwardListItem> create(BackForwardListItemState&&, WebPageProxyIdentifier);
    virtual ~WebBackForwardListItem();

    static WebBackForwardListItem* itemForID(const WebCore::BackForwardItemIdentifier&);
    static HashMap<WebCore::BackForwardItemIdentifier, WebBackForwardListItem*>& allItems();

    const WebCore::BackForwardItemIdentifier& itemID() const { return m_itemState.identifier; }
    const BackForwardListItemState& itemState() { return m_itemState; }
    WebPageProxyIdentifier pageID() const { return m_pageID; }

    WebCore::ProcessIdentifier lastProcessIdentifier() const { return m_lastProcessIdentifier; }
    void setLastProcessIdentifier(const WebCore::ProcessIdentifier& identifier) { m_lastProcessIdentifier = identifier; }

    void setPageState(PageState&& pageState) { m_itemState.pageState = WTFMove(pageState); }
    const PageState& pageState() const { return m_itemState.pageState; }

    const String& originalURL() const { return m_itemState.pageState.mainFrameState.originalURLString; }
    const String& url() const { return m_itemState.pageState.mainFrameState.urlString; }
    const String& title() const { return m_itemState.pageState.title; }

    const URL& resourceDirectoryURL() const { return m_resourceDirectoryURL; }
    void setResourceDirectoryURL(URL&& url) { m_resourceDirectoryURL = WTFMove(url); }

    bool itemIsInSameDocument(const WebBackForwardListItem&) const;
    bool itemIsClone(const WebBackForwardListItem&);

#if PLATFORM(COCOA) || PLATFORM(GTK)
    ViewSnapshot* snapshot() const { return m_itemState.snapshot.get(); }
    void setSnapshot(RefPtr<ViewSnapshot>&& snapshot) { m_itemState.snapshot = WTFMove(snapshot); }
#endif

    void wasRemovedFromBackForwardList();

    WebBackForwardCacheEntry* backForwardCacheEntry() const { return m_backForwardCacheEntry.get(); }
    SuspendedPageProxy* suspendedPage() const;

#if !LOG_DISABLED
    const char* loggingString();
#endif

private:
    WebBackForwardListItem(BackForwardListItemState&&, WebPageProxyIdentifier);

    void removeFromBackForwardCache();

    // WebBackForwardCache.
    friend class WebBackForwardCache;
    void setBackForwardCacheEntry(std::unique_ptr<WebBackForwardCacheEntry>&&);

    BackForwardListItemState m_itemState;
    URL m_resourceDirectoryURL;
    WebPageProxyIdentifier m_pageID;
    WebCore::ProcessIdentifier m_lastProcessIdentifier;
    std::unique_ptr<WebBackForwardCacheEntry> m_backForwardCacheEntry;
};

typedef Vector<Ref<WebBackForwardListItem>> BackForwardListItemVector;

} // namespace WebKit
