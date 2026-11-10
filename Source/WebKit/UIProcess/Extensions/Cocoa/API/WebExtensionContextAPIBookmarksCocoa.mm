/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import "CocoaHelpers.h"
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WebExtensionBookmarkParameters.h"
#import "WebExtensionController.h"
#import "WebExtensionPermission.h"
#import "_WKWebExtensionBookmarks.h"
#import <wtf/BlockPtr.h>
#import <wtf/Vector.h>

namespace WebKit {

bool WebExtensionContext::isBookmarksMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::bookmarks());
}

struct BookmarksDelegateContext {
    WKWebExtensionControllerDelegatePrivate *delegate;
    WKWebExtensionController *controller;
    WKWebExtensionContext *context;
};

static std::expected<BookmarksDelegateContext, WebExtensionError> bookmarksDelegateContext(WebExtensionContext& extensionContext, NSString *apiName, SEL selector)
{
    RefPtr extensionController = extensionContext.extensionController();
    if (!extensionController)
        return toWebExtensionError(apiName, nullString(), @"the extension is not loaded");

    auto *delegate = extensionController->delegate();
    if (![delegate respondsToSelector:selector])
        return toWebExtensionError(apiName, nullString(), @"it is not implemented");

    return BookmarksDelegateContext { delegate, extensionController->wrapper(), extensionContext.wrapper() };
}

static inline NSString *identifierOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(identifierForWebExtensionContext:)] ? [bookmark identifierForWebExtensionContext:context] : nil;
}

static inline NSString *parentIdentifierOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(parentIdentifierForWebExtensionContext:)] ? [bookmark parentIdentifierForWebExtensionContext:context] : nil;
}

static inline NSString *titleOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(titleForWebExtensionContext:)] ? [bookmark titleForWebExtensionContext:context] : nil;
}

static inline NSString *urlStringOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(urlStringForWebExtensionContext:)] ? [bookmark urlStringForWebExtensionContext:context] : nil;
}

static inline NSArray<id<_WKWebExtensionBookmark>> *childrenOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(childrenForWebExtensionContext:)] ? [bookmark childrenForWebExtensionContext:context] : nil;
}

static inline NSDate *dateAddedOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(dateAddedForWebExtensionContext:)] ? [bookmark dateAddedForWebExtensionContext:context] : nil;
}

static inline uint64_t indexOf(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    if (![bookmark respondsToSelector:@selector(indexForWebExtensionContext:)])
        return 0;

    return [bookmark indexForWebExtensionContext:context];
}

static inline bool isFolder(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    return [bookmark respondsToSelector:@selector(bookmarkTypeForWebExtensionContext:)] && [bookmark bookmarkTypeForWebExtensionContext:context] == _WKWebExtensionBookmarkTypeFolder;
}

static Vector<WebExtensionBookmarkParameters> toParameters(NSArray<id<_WKWebExtensionBookmark>> *, WKWebExtensionContext *);

static std::optional<WebExtensionBookmarkParameters> toParameters(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    if (!bookmark)
        return std::nullopt;

    WebExtensionBookmarkParameters parameters;

    auto *identifier = identifierOf(bookmark, context);
    if (!identifier)
        return std::nullopt;

    parameters.identifier = identifier;

    if (auto *parentIdentifier = parentIdentifierOf(bookmark, context))
        parameters.parentIdentifier = String(parentIdentifier);

    parameters.index = indexOf(bookmark, context);
    parameters.title = String(titleOf(bookmark, context) ?: @"");
    parameters.url = urlStringOf(bookmark, context);

    if (auto *children = childrenOf(bookmark, context))
        parameters.children = toParameters(children, context);

    if (auto *dateAdded = dateAddedOf(bookmark, context))
        parameters.dateAdded = WallTime::fromRawSeconds(dateAdded.timeIntervalSince1970);
    else
        parameters.dateAdded = WallTime::fromRawSeconds(0);

    return parameters;
}

static Vector<WebExtensionBookmarkParameters> toParameters(NSArray<id<_WKWebExtensionBookmark>> *bookmarks, WKWebExtensionContext *context)
{
    if (!bookmarks.count)
        return { };

    Vector<WebExtensionBookmarkParameters> result(ReserveCapacityTag { }, bookmarks.count);

    for (id<_WKWebExtensionBookmark> bookmark in bookmarks) {
        if (auto parameters = toParameters(bookmark, context))
            result.append(WTF::move(*parameters));
    }

    return result;
}

static Vector<WebExtensionBookmarkParameters> toShallowParameters(NSArray<id<_WKWebExtensionBookmark>> *bookmarks, WKWebExtensionContext *context)
{
    if (!bookmarks)
        return { };

    Vector<WebExtensionBookmarkParameters> result(ReserveCapacityTag { }, bookmarks.count);

    for (id<_WKWebExtensionBookmark> bookmark in bookmarks) {
        auto *identifier = identifierOf(bookmark, context);
        if (!identifier)
            continue;

        WebExtensionBookmarkParameters parameters;
        parameters.identifier = identifier;

        if (auto *parentIdentifier = parentIdentifierOf(bookmark, context))
            parameters.parentIdentifier = String(parentIdentifier);

        parameters.index = indexOf(bookmark, context);
        parameters.title = String(titleOf(bookmark, context) ?: @"");
        parameters.url = urlStringOf(bookmark, context);

        result.append(WTF::move(parameters));
    }

    return result;
}

static Vector<WebExtensionBookmarkParameters> flattenedParameters(NSArray<id<_WKWebExtensionBookmark>> *bookmarks, WKWebExtensionContext *context)
{
    Vector<WebExtensionBookmarkParameters> result;
    if (!bookmarks)
        return result;

    for (id<_WKWebExtensionBookmark> bookmark in bookmarks) {
        if (auto parameters = toParameters(bookmark, context))
            result.append(WTF::move(*parameters));

        if (isFolder(bookmark, context)) {
            if (auto *children = childrenOf(bookmark, context))
                result.appendVector(flattenedParameters(children, context));
        }
    }

    return result;
}

static id<_WKWebExtensionBookmark> findBookmark(NSArray<id<_WKWebExtensionBookmark>> *bookmarks, const String& identifier, WKWebExtensionContext *context)
{
    for (id<_WKWebExtensionBookmark> bookmark in bookmarks) {
        if (String(identifierOf(bookmark, context)) == identifier)
            return bookmark;

        if (auto *children = childrenOf(bookmark, context)) {
            if (auto foundBookmark = findBookmark(children, identifier, context))
                return foundBookmark;
        }
    }

    return nil;
}

static bool containsIgnoringASCIICase(const String& text, const String& substring)
{
    if (substring.isEmpty())
        return true;

    if (text.isEmpty())
        return false;

    return text.containsIgnoringASCIICase(substring);
}

static String normalizeURLStringForComparison(const String& url)
{
    auto normalized = url.convertToASCIILowercase();
    if (normalized.endsWith('/'))
        normalized = normalized.left(normalized.length() - 1);

    return normalized;
}

void WebExtensionContext::bookmarksCreate(const WebExtensionBookmarkParameters& bookmark, CompletionHandler<void(std::expected<WebExtensionBookmarkParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.create()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:createBookmarkWithParentIdentifier:index:url:title:forExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    auto *parentIdentifierString = bookmark.parentIdentifier ? bookmark.parentIdentifier->createNSString().get() : nil;
    auto *indexNumber = bookmark.index ? @(bookmark.index.value()) : nil;
    auto *urlString = bookmark.url ? bookmark.url->createNSString().get() : nil;
    auto *titleString = bookmark.title ? bookmark.title->createNSString().get() : @"";

    [controllerDelegate _webExtensionController:controllerWrapper createBookmarkWithParentIdentifier:parentIdentifierString index:indexNumber url:urlString title:titleString forExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper](NSObject<_WKWebExtensionBookmark> *newBookmark, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        auto parameters = toParameters(newBookmark, contextWrapper);
        if (!parameters) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(WTF::move(*parameters));
    }).get()];
}

void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getTree()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper](NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(toParameters(topLevelBookmarks, contextWrapper));
    }).get()];
}

void WebExtensionContext::bookmarksGetSubTree(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getSubTree()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper, bookmarkIdentifier = bookmarkIdentifier](NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        auto parameters = toParameters(findBookmark(topLevelBookmarks, bookmarkIdentifier, contextWrapper), contextWrapper);
        if (!parameters) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        Vector<WebExtensionBookmarkParameters> result;
        result.append(WTF::move(*parameters));

        completionHandler(WTF::move(result));
    }).get()];
}

void WebExtensionContext::bookmarksGet(const Vector<String>& bookmarkIdentifiers, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.get()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper, bookmarkIdentifiers = bookmarkIdentifiers](NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        auto *foundBookmarks = [NSMutableArray arrayWithCapacity:bookmarkIdentifiers.size()];

        for (auto& bookmarkIdentifier : bookmarkIdentifiers) {
            auto foundBookmark = findBookmark(topLevelBookmarks, bookmarkIdentifier, contextWrapper);
            if (!foundBookmark) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
                return;
            }

            [foundBookmarks addObject:foundBookmark];
        }

        completionHandler(toShallowParameters(foundBookmarks, contextWrapper));
    }).get()];
}

void WebExtensionContext::bookmarksGetChildren(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getChildren()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper, bookmarkIdentifier = bookmarkIdentifier](NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        auto parentBookmark = findBookmark(topLevelBookmarks, bookmarkIdentifier, contextWrapper);
        if (!parentBookmark) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
            return;
        }

        if (!isFolder(parentBookmark, contextWrapper)) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark is not a folder"));
            return;
        }

        auto *children = childrenOf(parentBookmark, contextWrapper);
        if (!children) {
            completionHandler({ });
            return;
        }

        completionHandler(toShallowParameters(children, contextWrapper));
    }).get()];
}

void WebExtensionContext::bookmarksGetRecent(uint64_t numberOfItems, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getRecent()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper, numberOfItems](NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        auto allBookmarks = flattenedParameters(topLevelBookmarks, contextWrapper);

        std::sort(allBookmarks.begin(), allBookmarks.end(), [](const auto& a, const auto& b) {
            return a.dateAdded > b.dateAdded;
        });

        Vector<WebExtensionBookmarkParameters> recentBookmarks(ReserveCapacityTag { }, std::min(static_cast<size_t>(numberOfItems), allBookmarks.size()));

        for (const auto& bookmark : allBookmarks) {
            if (recentBookmarks.size() >= numberOfItems)
                break;

            if (bookmark.url && !bookmark.url->isEmpty())
                recentBookmarks.append(bookmark);
        }

        completionHandler(WTF::move(recentBookmarks));
    }).get()];
}

void WebExtensionContext::bookmarksSearch(std::optional<String> query, std::optional<String> url, std::optional<String> title, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.search()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper, query = WTF::move(query), url = WTF::move(url), title = WTF::move(title)](NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        auto allBookmarks = flattenedParameters(topLevelBookmarks, contextWrapper);

        Vector<String> queryTerms;
        if (query && !query->isEmpty())
            queryTerms = query->split(' ');

        std::optional<String> normalizedURL;
        if (url)
            normalizedURL = normalizeURLStringForComparison(*url);

        Vector<WebExtensionBookmarkParameters> matchingBookmarks;

        for (const auto& bookmark : allBookmarks) {
            if (query) {
                bool allTermsMatch = true;
                for (const auto& term : queryTerms) {
                    if (!containsIgnoringASCIICase(bookmark.title.value_or(emptyString()), term) && (!bookmark.url || !containsIgnoringASCIICase(*bookmark.url, term))) {
                        allTermsMatch = false;
                        break;
                    }
                }

                if (!allTermsMatch)
                    continue;
            }

            if (title && bookmark.title.value_or(emptyString()) != *title)
                continue;

            if (url && (!bookmark.url || normalizeURLStringForComparison(*bookmark.url) != *normalizedURL))
                continue;

            auto match = bookmark;
            match.children = std::nullopt;
            matchingBookmarks.append(WTF::move(match));
        }

        completionHandler(WTF::move(matchingBookmarks));
    }).get()];
}

void WebExtensionContext::bookmarksUpdate(const WebExtensionBookmarkParameters& bookmark, CompletionHandler<void(std::expected<WebExtensionBookmarkParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.update()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:updateBookmarkWithIdentifier:title:url:forExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    auto *titleString = bookmark.title ? bookmark.title->createNSString().get() : nil;
    auto *urlString = bookmark.url ? bookmark.url->createNSString().get() : nil;

    [controllerDelegate _webExtensionController:controllerWrapper updateBookmarkWithIdentifier:bookmark.identifier.createNSString().get() title:titleString url:urlString forExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper](NSObject<_WKWebExtensionBookmark> *updatedBookmark, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!updatedBookmark) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"the bookmark could not be updated"));
            return;
        }

        auto parameters = toParameters(updatedBookmark, contextWrapper);
        if (!parameters) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(WTF::move(*parameters));
    }).get()];
}

void WebExtensionContext::bookmarksMove(const WebExtensionBookmarkParameters& bookmark, CompletionHandler<void(std::expected<WebExtensionBookmarkParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.move()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:moveBookmarkWithIdentifier:toParent:atIndex:forExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    auto *parentIdentifierString = bookmark.parentIdentifier ? bookmark.parentIdentifier->createNSString().get() : nil;
    auto *indexNumber = bookmark.index ? @(bookmark.index.value()) : nil;

    [controllerDelegate _webExtensionController:controllerWrapper moveBookmarkWithIdentifier:bookmark.identifier.createNSString().get() toParent:parentIdentifierString atIndex:indexNumber forExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler), contextWrapper](NSObject<_WKWebExtensionBookmark> *movedBookmark, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!movedBookmark) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"the bookmark could not be moved"));
            return;
        }

        auto parameters = toParameters(movedBookmark, contextWrapper);
        if (!parameters) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(WTF::move(*parameters));
    }).get()];
}

void WebExtensionContext::bookmarksRemove(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.remove()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:removeBookmarkWithIdentifier:removeFolderWithChildren:forExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper removeBookmarkWithIdentifier:bookmarkIdentifier.createNSString().get() removeFolderWithChildren:NO forExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionContext::bookmarksRemoveTree(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.removeTree()";

    auto delegateContext = bookmarksDelegateContext(*this, apiName, @selector(_webExtensionController:removeBookmarkWithIdentifier:removeFolderWithChildren:forExtensionContext:completionHandler:));
    if (!delegateContext) {
        completionHandler(std::unexpected(WTF::move(delegateContext.error())));
        return;
    }

    auto *controllerDelegate = delegateContext->delegate;
    auto *controllerWrapper = delegateContext->controller;
    auto *contextWrapper = delegateContext->context;

    [controllerDelegate _webExtensionController:controllerWrapper removeBookmarkWithIdentifier:bookmarkIdentifier.createNSString().get() removeFolderWithChildren:YES forExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
