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
#include "EnhancedSecurity.h"
#include "SessionState.h"
#include "WebPageProxyIdentifier.h"
#include "WebsiteDataStore.h"
#include <wtf/CheckedPtr.h>
#include <wtf/Ref.h>
#include <wtf/RetainReleaseSwift.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class BrowsingContextGroup;
class SuspendedPageProxy;
class WebBackForwardCache;
class WebBackForwardCacheEntry;
class WebBackForwardListFrameItem;

class WebBackForwardListItem : public API::ObjectImpl<API::Object::Type::BackForwardListItem>, public CanMakeWeakPtr<WebBackForwardListItem> {
public:
    static Ref<WebBackForwardListItem> create(Ref<FrameState>&&, WebPageProxyIdentifier, std::optional<WebCore::FrameIdentifier>, BrowsingContextGroup* = nullptr);
    virtual ~WebBackForwardListItem();

    static WebBackForwardListItem* itemForID(WebCore::BackForwardItemIdentifier);
    static HashMap<WebCore::BackForwardItemIdentifier, WeakRef<WebBackForwardListItem>>& allItems();

    WebCore::BackForwardItemIdentifier identifier() const { return m_identifier; }
    WebPageProxyIdentifier pageID() const { return m_pageID; }

    WebCore::ProcessIdentifier lastProcessIdentifier() const { return m_lastProcessIdentifier; }
    void setLastProcessIdentifier(const WebCore::ProcessIdentifier& identifier) { m_lastProcessIdentifier = identifier; }

    BrowsingContextGroup* browsingContextGroup() const { return m_browsingContextGroup.get(); }

    Ref<FrameState> navigatedFrameState() const;
    Ref<FrameState> mainFrameState() const;

    const String& originalURL() const;
    const String& url() const;
    const String& title() const;
    bool wasCreatedByJSWithoutUserInteraction() const;

    const URL& resourceDirectoryURL() const { return m_resourceDirectoryURL; }
    void setResourceDirectoryURL(URL&& url) { m_resourceDirectoryURL = WTFMove(url); }
    RefPtr<WebsiteDataStore> dataStoreForWebArchive() const { return m_dataStoreForWebArchive; }
    void setDataStoreForWebArchive(WebsiteDataStore* dataStore) { m_dataStoreForWebArchive = dataStore; }

    bool itemIsInSameDocument(const WebBackForwardListItem&) const;
    bool itemIsClone(const WebBackForwardListItem&);

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && USE(SKIA))
    ViewSnapshot* snapshot() const { return m_snapshot.get(); }
    void setSnapshot(RefPtr<ViewSnapshot>&& snapshot) { m_snapshot = WTFMove(snapshot); }
#endif

    void wasRemovedFromBackForwardList();

    WebBackForwardCacheEntry* backForwardCacheEntry() const { return m_backForwardCacheEntry.get(); }
    RefPtr<WebBackForwardCacheEntry> protectedBackForwardCacheEntry() const;

    SuspendedPageProxy* suspendedPage() const;

    std::optional<WebCore::FrameIdentifier> navigatedFrameID() const { return m_navigatedFrameID; }

    WebBackForwardListFrameItem& navigatedFrameItem() const;
    Ref<WebBackForwardListFrameItem> protectedNavigatedFrameItem() const;

    WebBackForwardListFrameItem& mainFrameItem() const;
    Ref<WebBackForwardListFrameItem> protectedMainFrameItem() const;

    void setIsRemoteFrameNavigation(bool isRemoteFrameNavigation) { m_isRemoteFrameNavigation = isRemoteFrameNavigation; }
    bool isRemoteFrameNavigation() const { return m_isRemoteFrameNavigation; }

    void setWasRestoredFromSession();

    String loggingString();

    void setEnhancedSecurity(EnhancedSecurity state) { m_enhancedSecurity = state; }
    EnhancedSecurity enhancedSecurity() const { return m_enhancedSecurity; }

private:
    WebBackForwardListItem(Ref<FrameState>&&, WebPageProxyIdentifier, std::optional<WebCore::FrameIdentifier>, BrowsingContextGroup*);

    void removeFromBackForwardCache();

    friend class WebBackForwardCache;
    void setBackForwardCacheEntry(RefPtr<WebBackForwardCacheEntry>&&);

    RefPtr<WebsiteDataStore> m_dataStoreForWebArchive;

    const WebCore::BackForwardItemIdentifier m_identifier;
    const Ref<WebBackForwardListFrameItem> m_mainFrameItem;
    const Markable<WebCore::FrameIdentifier> m_navigatedFrameID;
    URL m_resourceDirectoryURL;
    const WebPageProxyIdentifier m_pageID;
    WebCore::ProcessIdentifier m_lastProcessIdentifier;
    RefPtr<WebBackForwardCacheEntry> m_backForwardCacheEntry;
    const RefPtr<BrowsingContextGroup> m_browsingContextGroup;
#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && USE(SKIA))
    RefPtr<ViewSnapshot> m_snapshot;
#endif
    bool m_isRemoteFrameNavigation { false };
    EnhancedSecurity m_enhancedSecurity { EnhancedSecurity::Disabled };
} SWIFT_SHARED_REFERENCE(refBackForwardListItem, derefBackForwardListItem);

typedef Vector<Ref<WebBackForwardListItem>> BackForwardListItemVector;

} // namespace WebKit

inline void refBackForwardListItem(WebKit::WebBackForwardListItem* WTF_NONNULL obj)
{
    WTF::ref(obj);
}

inline void derefBackForwardListItem(WebKit::WebBackForwardListItem* WTF_NONNULL obj)
{
    WTF::deref(obj);
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebBackForwardListItem)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BackForwardListItem; }
SPECIALIZE_TYPE_TRAITS_END()
