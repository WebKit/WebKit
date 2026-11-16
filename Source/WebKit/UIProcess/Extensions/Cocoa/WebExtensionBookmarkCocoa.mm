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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionBookmark.h"

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import "WebExtensionContext.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionBookmarkConfigurationInternal.h"
#import "_WKWebExtensionBookmarks.h"
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionBookmark);

WebExtensionBookmark::WebExtensionBookmark(const WebExtensionContext& context, NSArray *topLevelNodes)
    : m_extensionContext(context)
    , m_topLevelNodes(topLevelNodes)
    , m_isRoot(true)
{
}

WebExtensionBookmark::WebExtensionBookmark(const WebExtensionContext& context, NSObject *delegate, WebExtensionBookmark* parent, std::optional<size_t> index)
    : m_extensionContext(context)
    , m_delegate(delegate)
    , m_parent(parent)
    , m_index(index)
    , m_respondsToIdentifier([delegate respondsToSelector:@selector(identifierForWebExtensionContext:)])
    , m_respondsToTitle([delegate respondsToSelector:@selector(titleForWebExtensionContext:)])
    , m_respondsToURL([delegate respondsToSelector:@selector(urlForWebExtensionContext:)])
    , m_respondsToType([delegate respondsToSelector:@selector(bookmarkTypeForWebExtensionContext:)])
    , m_respondsToChildren([delegate respondsToSelector:@selector(childrenForWebExtensionContext:)])
    , m_respondsToDateAdded([delegate respondsToSelector:@selector(dateAddedForWebExtensionContext:)])
    , m_respondsToFolderType([delegate respondsToSelector:@selector(folderTypeForWebExtensionContext:)])
    , m_respondsToIsManaged([delegate respondsToSelector:@selector(isManagedForWebExtensionContext:)])
    , m_respondsToIsSynced([delegate respondsToSelector:@selector(isSyncedForWebExtensionContext:)])
    , m_respondsToCreateChild([delegate respondsToSelector:@selector(createChildUsingConfiguration:forWebExtensionContext:completionHandler:)])
    , m_respondsToSetTitle([delegate respondsToSelector:@selector(setTitle:forWebExtensionContext:completionHandler:)])
    , m_respondsToSetURL([delegate respondsToSelector:@selector(setURL:forWebExtensionContext:completionHandler:)])
    , m_respondsToMoveToParent([delegate respondsToSelector:@selector(moveToParent:atIndex:forWebExtensionContext:completionHandler:)])
    , m_respondsToRemove([delegate respondsToSelector:@selector(removeForWebExtensionContext:completionHandler:)])
{
}

WebExtensionContext* WebExtensionBookmark::extensionContext() const
{
    return m_extensionContext.get();
}

bool WebExtensionBookmark::isValid() const
{
    return m_extensionContext && (m_isRoot || m_delegate);
}

String WebExtensionBookmark::identifier() const
{
    if (m_isRoot)
        return rootIdentifier();

    if (!isValid() || !m_respondsToIdentifier)
        return emptyString();

    String identifier = [(id<_WKWebExtensionBookmark>)m_delegate identifierForWebExtensionContext:m_extensionContext->wrapper()];
    return identifier.isNull() ? emptyString() : identifier;
}

String WebExtensionBookmark::title() const
{
    if (m_isRoot || !isValid() || !m_respondsToTitle)
        return emptyString();

    String title = [(id<_WKWebExtensionBookmark>)m_delegate titleForWebExtensionContext:m_extensionContext->wrapper()];
    return title.isNull() ? emptyString() : title;
}

std::optional<URL> WebExtensionBookmark::url() const
{
    if (m_isRoot || !isValid() || !m_respondsToURL)
        return std::nullopt;

    auto *nsURL = [(id<_WKWebExtensionBookmark>)m_delegate urlForWebExtensionContext:m_extensionContext->wrapper()];
    if (!nsURL)
        return std::nullopt;

    URL url { nsURL };
    if (!url.isValid())
        return std::nullopt;

    return url;
}

WallTime WebExtensionBookmark::dateAdded() const
{
    if (m_isRoot || !isValid() || !m_respondsToDateAdded)
        return WallTime::fromRawSeconds(0);

    auto *dateAdded = [(id<_WKWebExtensionBookmark>)m_delegate dateAddedForWebExtensionContext:m_extensionContext->wrapper()];
    if (!dateAdded)
        return WallTime::fromRawSeconds(0);

    return WallTime::fromRawSeconds(dateAdded.timeIntervalSince1970);
}

bool WebExtensionBookmark::isFolder() const
{
    if (m_isRoot)
        return true;

    if (!isValid())
        return false;

    if (m_respondsToType)
        return [(id<_WKWebExtensionBookmark>)m_delegate bookmarkTypeForWebExtensionContext:m_extensionContext->wrapper()] == _WKWebExtensionBookmarkTypeFolder;

    return !url();
}

WebExtensionBookmark::FolderType WebExtensionBookmark::folderType() const
{
    if (m_isRoot || !isValid() || !m_respondsToFolderType)
        return FolderType::None;

    switch ([(id<_WKWebExtensionBookmark>)m_delegate folderTypeForWebExtensionContext:m_extensionContext->wrapper()]) {
    case _WKWebExtensionBookmarkFolderTypeNone:
        return FolderType::None;
    case _WKWebExtensionBookmarkFolderTypeBookmarksBar:
        return FolderType::BookmarksBar;
    case _WKWebExtensionBookmarkFolderTypeManaged:
        return FolderType::Managed;
    case _WKWebExtensionBookmarkFolderTypeOther:
        return FolderType::Other;
    }

    return FolderType::None;
}

bool WebExtensionBookmark::isManaged() const
{
    if (m_isRoot || !isValid() || !m_respondsToIsManaged)
        return false;

    return [(id<_WKWebExtensionBookmark>)m_delegate isManagedForWebExtensionContext:m_extensionContext->wrapper()];
}

bool WebExtensionBookmark::isSynced() const
{
    // Managed nodes are never synced to the account.
    if (m_isRoot || !isValid() || !m_respondsToIsSynced || isManaged())
        return false;

    return [(id<_WKWebExtensionBookmark>)m_delegate isSyncedForWebExtensionContext:m_extensionContext->wrapper()];
}

RefPtr<WebExtensionBookmark> WebExtensionBookmark::parent() const
{
    return m_parent.get();
}

std::optional<size_t> WebExtensionBookmark::index() const
{
    return m_index;
}

const Vector<Ref<WebExtensionBookmark>>& WebExtensionBookmark::children() const
{
    if (m_children)
        return *m_children;

    Vector<Ref<WebExtensionBookmark>> children;

    if (RefPtr extensionContext = m_extensionContext) {
        NSArray *nodes = nil;
        if (m_isRoot)
            nodes = m_topLevelNodes.get();
        else if (isValid() && m_respondsToChildren)
            nodes = [(id<_WKWebExtensionBookmark>)m_delegate childrenForWebExtensionContext:extensionContext->wrapper()];

        for (NSObject *node in nodes) {
            Ref child = WebExtensionBookmark::create(*extensionContext, node, const_cast<WebExtensionBookmark*>(this), children.size());
            if (child->identifier().isEmpty())
                continue;

            children.append(WTF::move(child));
        }
    }

    m_children = WTF::move(children);
    return *m_children;
}

RefPtr<WebExtensionBookmark> WebExtensionBookmark::find(const String& identifier)
{
    if (this->identifier() == identifier)
        return this;

    for (auto& child : children()) {
        if (RefPtr found = child->find(identifier))
            return found;
    }

    return nullptr;
}

Vector<Ref<WebExtensionBookmark>> WebExtensionBookmark::descendants() const
{
    Vector<Ref<WebExtensionBookmark>> result;

    for (auto& child : children()) {
        result.append(child.copyRef());
        result.appendVector(child->descendants());
    }

    return result;
}

RefPtr<WebExtensionBookmark> WebExtensionBookmark::defaultParent() const
{
    if (!m_isRoot)
        return nullptr;

    for (auto& child : children()) {
        if (child->folderType() == FolderType::Other)
            return child.ptr();
    }

    return nullptr;
}

bool WebExtensionBookmark::isDescendantOf(const WebExtensionBookmark& ancestor) const
{
    auto ancestorIdentifier = ancestor.identifier();
    for (RefPtr bookmark = parent(); bookmark; bookmark = bookmark->parent()) {
        if (bookmark->identifier() == ancestorIdentifier)
            return true;
    }

    return false;
}

std::optional<WebExtensionError> WebExtensionBookmark::modificationError(const String& apiName) const
{
    if (!isValid())
        return toWebExtensionError(apiName, nullString(), @"the extension is not loaded").error();

    if (m_isRoot)
        return toWebExtensionError(apiName, nullString(), @"the bookmark root cannot be modified").error();

    if (isManaged())
        return toWebExtensionError(apiName, nullString(), @"managed bookmarks cannot be modified").error();

    return std::nullopt;
}

void WebExtensionBookmark::createChild(const String& apiName, std::optional<URL> url, const String& title, std::optional<size_t> index, CompletionHandler<void(std::expected<Ref<WebExtensionBookmark>, WebExtensionError>&&)>&& completionHandler)
{
    if (auto error = modificationError(apiName)) {
        completionHandler(std::unexpected(WTF::move(*error)));
        return;
    }

    if (!isFolder()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the parent is not a folder"));
        return;
    }

    size_t resolvedIndex = index.value_or(children().size());
    if (resolvedIndex > children().size()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"index out of bounds"));
        return;
    }

    if (!m_respondsToCreateChild) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto *configuration = [[_WKWebExtensionBookmarkConfiguration alloc] _init];
    configuration.type = url ? _WKWebExtensionBookmarkTypeBookmark : _WKWebExtensionBookmarkTypeFolder;
    configuration.title = title.createNSString().get();
    configuration.url = url ? url->createNSURL().get() : nil;
    configuration.index = resolvedIndex;

    [(id<_WKWebExtensionBookmark>)m_delegate createChildUsingConfiguration:configuration forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, apiName, resolvedIndex, completionHandler = WTF::move(completionHandler)](id<_WKWebExtensionBookmark> newBookmark, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        RefPtr extensionContext = protectedThis->extensionContext();
        if (!newBookmark || !extensionContext) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(WebExtensionBookmark::create(*extensionContext, (NSObject *)newBookmark, protectedThis.ptr(), resolvedIndex));
    }).get()];
}

void WebExtensionBookmark::setTitle(const String& apiName, const String& title, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (auto error = modificationError(apiName)) {
        completionHandler(std::unexpected(WTF::move(*error)));
        return;
    }

    if (!m_respondsToSetTitle) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented for 'title'"));
        return;
    }

    [(id<_WKWebExtensionBookmark>)m_delegate setTitle:title.createNSString().get() forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, apiName, completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionBookmark::setURL(const String& apiName, const URL& url, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (auto error = modificationError(apiName)) {
        completionHandler(std::unexpected(WTF::move(*error)));
        return;
    }

    if (isFolder()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the URL of a folder cannot be set"));
        return;
    }

    if (!m_respondsToSetURL) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented for 'url'"));
        return;
    }

    [(id<_WKWebExtensionBookmark>)m_delegate setURL:url.createNSURL().get() forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, apiName, completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionBookmark::moveTo(const String& apiName, WebExtensionBookmark& newParent, std::optional<size_t> index, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (auto error = modificationError(apiName)) {
        completionHandler(std::unexpected(WTF::move(*error)));
        return;
    }

    if (auto error = newParent.modificationError(apiName)) {
        completionHandler(std::unexpected(WTF::move(*error)));
        return;
    }

    if (!newParent.isFolder()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the new parent is not a folder"));
        return;
    }

    if (newParent.identifier() == identifier() || newParent.isDescendantOf(*this)) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"a folder cannot be moved into itself or its descendants"));
        return;
    }

    size_t resolvedIndex = index.value_or(newParent.children().size());
    if (resolvedIndex > newParent.children().size()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"index out of bounds"));
        return;
    }

    if (!m_respondsToMoveToParent) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [(id<_WKWebExtensionBookmark>)m_delegate moveToParent:(id<_WKWebExtensionBookmark>)newParent.delegate() atIndex:resolvedIndex forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, protectedNewParent = Ref { newParent }, apiName, completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionBookmark::remove(const String& apiName, Recursively recursively, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (auto error = modificationError(apiName)) {
        completionHandler(std::unexpected(WTF::move(*error)));
        return;
    }

    if (recursively == Recursively::No && isFolder() && !children().isEmpty()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the folder is not empty, use removeTree() to remove it"));
        return;
    }

    if (!m_respondsToRemove) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [(id<_WKWebExtensionBookmark>)m_delegate removeForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, apiName, completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

WebExtensionBookmarkParameters WebExtensionBookmark::parameters(IncludeChildren includeChildren) const
{
    WebExtensionBookmarkParameters parameters;
    parameters.identifier = identifier();
    parameters.title = title();
    parameters.url = url();
    parameters.dateAdded = dateAdded();
    parameters.managed = isManaged();
    parameters.synced = isSynced();

    if (RefPtr parent = this->parent())
        parameters.parentIdentifier = parent->identifier();

    if (auto index = this->index())
        parameters.index = *index;

    parameters.folderType = folderType();

    if (includeChildren == IncludeChildren::Yes && isFolder()) {
        Vector<WebExtensionBookmarkParameters> childrenParameters;
        childrenParameters.reserveInitialCapacity(children().size());
        for (auto& child : children())
            childrenParameters.append(child->parameters(IncludeChildren::Yes));

        parameters.children = WTF::move(childrenParameters);
    }

    return parameters;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
