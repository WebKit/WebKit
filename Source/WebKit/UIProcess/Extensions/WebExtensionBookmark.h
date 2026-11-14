/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#include "WebExtensionBookmarkParameters.h"
#include "WebExtensionError.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSObject;

namespace WebKit {

class WebExtensionContext;

class WebExtensionBookmark : public RefCountedAndCanMakeWeakPtr<WebExtensionBookmark> {
    WTF_MAKE_NONCOPYABLE(WebExtensionBookmark);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionBookmark);

public:
    using FolderType = WebExtensionBookmarkFolderType;

    enum class IncludeChildren : bool { No, Yes };
    enum class Recursively : bool { No, Yes };

    static String rootIdentifier() { return webExtensionBookmarkRootIdentifier; }

    template<typename... Args>
    static Ref<WebExtensionBookmark> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionBookmark(std::forward<Args>(args)...));
    }

    NODELETE WebExtensionContext* extensionContext() const;

    bool isRoot() const { return m_isRoot; }
    bool isValid() const;

    String identifier() const;
    String title() const;
    std::optional<URL> url() const;
    WallTime dateAdded() const;
    bool isFolder() const;
    FolderType folderType() const;
    bool isManaged() const;
    bool isSynced() const;

    RefPtr<WebExtensionBookmark> parent() const;
    std::optional<size_t> index() const;
    const Vector<Ref<WebExtensionBookmark>>& children() const;

    // Returns this bookmark or the descendant with the identifier.
    RefPtr<WebExtensionBookmark> find(const String& identifier);

    // Returns every descendant in pre-order, without this bookmark.
    Vector<Ref<WebExtensionBookmark>> descendants() const;

    // Returns the folder that new bookmarks go in when the extension does not name one. Only the root has one.
    RefPtr<WebExtensionBookmark> defaultParent() const;

    bool isDescendantOf(const WebExtensionBookmark&) const;

    WebExtensionBookmarkParameters parameters(IncludeChildren = IncludeChildren::No) const;

    // Each of these checks the rules every browser shares before it calls the delegate node.
    void createChild(const String& apiName, std::optional<URL>, const String& title, std::optional<size_t> index, CompletionHandler<void(std::expected<Ref<WebExtensionBookmark>, WebExtensionError>&&)>&&);
    void setTitle(const String& apiName, const String& title, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&&);
    void setURL(const String& apiName, const URL&, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&&);
    void moveTo(const String& apiName, WebExtensionBookmark& newParent, std::optional<size_t> index, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&&);
    void remove(const String& apiName, Recursively, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&&);

#ifdef __OBJC__
    NSObject *delegate() const { return m_delegate.get(); }
#endif

private:
    // Creates the root, which is not backed by a delegate node and has the top-level nodes as its children.
    explicit WebExtensionBookmark(const WebExtensionContext&, NSArray *topLevelNodes);

    explicit WebExtensionBookmark(const WebExtensionContext&, NSObject *delegate, WebExtensionBookmark* parent = nullptr, std::optional<size_t> index = std::nullopt);

    std::optional<WebExtensionError> modificationError(const String& apiName) const;

    WeakPtr<WebExtensionContext> m_extensionContext;
    RetainPtr<NSObject> m_delegate;
    RetainPtr<NSArray> m_topLevelNodes;
    WeakPtr<WebExtensionBookmark> m_parent;
    std::optional<size_t> m_index;
    mutable std::optional<Vector<Ref<WebExtensionBookmark>>> m_children;
    bool m_isRoot : 1 { false };
    bool m_respondsToIdentifier : 1 { false };
    bool m_respondsToTitle : 1 { false };
    bool m_respondsToURL : 1 { false };
    bool m_respondsToType : 1 { false };
    bool m_respondsToChildren : 1 { false };
    bool m_respondsToDateAdded : 1 { false };
    bool m_respondsToFolderType : 1 { false };
    bool m_respondsToIsManaged : 1 { false };
    bool m_respondsToIsSynced : 1 { false };
    bool m_respondsToCreateChild : 1 { false };
    bool m_respondsToSetTitle : 1 { false };
    bool m_respondsToSetURL : 1 { false };
    bool m_respondsToMoveToParent : 1 { false };
    bool m_respondsToRemove : 1 { false };
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
