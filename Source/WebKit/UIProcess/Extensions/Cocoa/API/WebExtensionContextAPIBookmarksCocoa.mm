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
#import <wtf/Vector.h>

namespace WebKit {

bool WebExtensionContext::isBookmarksMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::bookmarks());
}

static Vector<WebExtensionBookmarkParameters> toParameters(NSArray<id<_WKWebExtensionBookmark>> *, WKWebExtensionContext *);

static std::optional<WebExtensionBookmarkParameters> toParameters(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    if (!bookmark)
        return std::nullopt;

    WebExtensionBookmarkParameters parameters;

    auto *identifier = [bookmark identifierForWebExtensionContext:context];
    if (!identifier)
        return std::nullopt;

    parameters.identifier = identifier;
    parameters.parentIdentifier = [bookmark parentIdentifierForWebExtensionContext:context];
    parameters.index = [bookmark indexForWebExtensionContext:context];

    if (auto *title = [bookmark titleForWebExtensionContext:context])
        parameters.title = String(title);
    parameters.url = [bookmark urlStringForWebExtensionContext:context];

    if (auto *children = [bookmark childrenForWebExtensionContext:context])
        parameters.children = toParameters(children, context);

    if (auto *dateAdded = [bookmark dateAddedForWebExtensionContext:context])
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
        WebExtensionBookmarkParameters parameters;
        parameters.identifier = [bookmark identifierForWebExtensionContext:context];
        parameters.parentIdentifier = [bookmark parentIdentifierForWebExtensionContext:context];
        parameters.index = [bookmark indexForWebExtensionContext:context];

        if (auto *title = [bookmark titleForWebExtensionContext:context])
            parameters.title = String(title);

        parameters.url = [bookmark urlStringForWebExtensionContext:context];

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

        if ([bookmark bookmarkTypeForWebExtensionContext:context] == _WKWebExtensionBookmarkTypeFolder) {
            if (auto *children = [bookmark childrenForWebExtensionContext:context])
                result.appendVector(flattenedParameters(children, context));
        }
    }

    return result;
}

static id<_WKWebExtensionBookmark> findBookmark(NSArray<id<_WKWebExtensionBookmark>> *bookmarks, const String& identifier, WKWebExtensionContext *context)
{
    for (id<_WKWebExtensionBookmark> bookmark in bookmarks) {
        if (String([bookmark identifierForWebExtensionContext:context]) == identifier)
            return bookmark;

        if (auto *children = [bookmark childrenForWebExtensionContext:context]) {
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

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:createBookmarkWithParentIdentifier:index:url:title:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto *parentIdentifierString = bookmark.parentIdentifier ? bookmark.parentIdentifier->createNSString().get() : nil;
    auto *indexNumber = bookmark.index ? @(bookmark.index.value()) : nil;
    auto *urlString = bookmark.url ? bookmark.url->createNSString().get() : nil;
    auto *titleString = bookmark.title ? bookmark.title->createNSString().get() : @"";

    [controllerDelegate _webExtensionController:controllerWrapper createBookmarkWithParentIdentifier:parentIdentifierString index:indexNumber url:urlString title:titleString forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *newBookmark, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getTree()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelBookmarks) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(toParameters(topLevelBookmarks, contextWrapper));
    }];
}

void WebExtensionContext::bookmarksGetSubTree(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getSubTree()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksGet(const Vector<String>& bookmarkIdentifiers, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.get()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksGetChildren(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getChildren()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) {
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

        if ([parentBookmark bookmarkTypeForWebExtensionContext:contextWrapper] != _WKWebExtensionBookmarkTypeFolder) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark is not a folder"));
            return;
        }

        auto *children = [parentBookmark childrenForWebExtensionContext:contextWrapper];
        if (!children) {
            completionHandler({ });
            return;
        }

        completionHandler(toShallowParameters(children, contextWrapper));
    }];
}

void WebExtensionContext::bookmarksGetRecent(uint64_t numberOfItems, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getRecent()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksSearch(std::optional<String> query, std::optional<String> url, std::optional<String> title, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.search()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *topLevelBookmarks, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksUpdate(const WebExtensionBookmarkParameters& bookmark, CompletionHandler<void(std::expected<WebExtensionBookmarkParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.update()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:updateBookmarkWithIdentifier:title:url:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto *titleString = bookmark.title ? bookmark.title->createNSString().get() : nil;
    auto *urlString = bookmark.url ? bookmark.url->createNSString().get() : nil;

    [controllerDelegate _webExtensionController:controllerWrapper updateBookmarkWithIdentifier:bookmark.identifier.createNSString().get() title:titleString url:urlString forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *updatedBookmark, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksMove(const WebExtensionBookmarkParameters& bookmark, CompletionHandler<void(std::expected<WebExtensionBookmarkParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.move()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:moveBookmarkWithIdentifier:toParent:atIndex:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto *parentIdentifierString = bookmark.parentIdentifier ? bookmark.parentIdentifier->createNSString().get() : nil;
    auto *indexNumber = bookmark.index ? @(bookmark.index.value()) : nil;

    [controllerDelegate _webExtensionController:controllerWrapper moveBookmarkWithIdentifier:bookmark.identifier.createNSString().get() toParent:parentIdentifierString atIndex:indexNumber forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *movedBookmark, NSError *error) {
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
    }];
}

void WebExtensionContext::bookmarksRemove(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.remove()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:removeBookmarkWithIdentifier:removeFolderWithChildren:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper removeBookmarkWithIdentifier:bookmarkIdentifier.createNSString().get() removeFolderWithChildren:NO forExtensionContext:contextWrapper completionHandler:^(NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }];
}

void WebExtensionContext::bookmarksRemoveTree(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.removeTree()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    auto *controllerDelegate = extensionController->delegate();
    auto *controllerWrapper = extensionController->wrapper();
    auto *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:removeBookmarkWithIdentifier:removeFolderWithChildren:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper removeBookmarkWithIdentifier:bookmarkIdentifier.createNSString().get() removeFolderWithChildren:YES forExtensionContext:contextWrapper completionHandler:^(NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        completionHandler({ });
    }];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
