// Copyright © 2025  All rights reserved.
#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"
#import "WebExtensionPermission.h"

#include "WebExtensionBookmarksParameters.h"
#include "WebExtensionController.h"
#include "_WKWebExtensionBookmarks.h"

#include <wtf/Vector.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import "WKWebExtensionControllerDelegatePrivate.h"

namespace WebKit {

bool WebExtensionContext::isBookmarksMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::bookmarks());
}

static Vector<WebExtensionBookmarksParameters> createParametersFromProtocolObjects(NSArray<id<_WKWebExtensionBookmark>> *, WKWebExtensionContext *);

static std::optional<WebExtensionBookmarksParameters> createParametersFromProtocolObject(id<_WKWebExtensionBookmark> bookmark, WKWebExtensionContext *context)
{
    if (!bookmark)
        return std::nullopt;

    WebExtensionBookmarksParameters parameters;

    NSString *nodeId = [bookmark identifierForWebExtensionContext:context];
    if (!nodeId)
        return std::nullopt;
    parameters.nodeId = StringImpl::create(nodeId);

    parameters.parentId = StringImpl::create([bookmark parentIdentifierForWebExtensionContext:context]);
    parameters.index = [bookmark indexForWebExtensionContext:context];
    parameters.title = StringImpl::create([bookmark titleForWebExtensionContext:context]);
    parameters.url = StringImpl::create([bookmark urlStringForWebExtensionContext:context]);

    if (NSArray *children = [bookmark childrenForWebExtensionContext:context])
        parameters.children = createParametersFromProtocolObjects(children, context);

    NSDate *dateAdded = [bookmark dateAddedForWebExtensionContext:context];
    if (dateAdded)
        parameters.dateAdded = WTF::WallTime::fromRawSeconds(dateAdded.timeIntervalSince1970);
    else
        parameters.dateAdded = WTF::WallTime::fromRawSeconds(0);

    return parameters;
}

static Vector<WebExtensionBookmarksParameters> createParametersFromProtocolObjects(NSArray<id<_WKWebExtensionBookmark>> *bookmarkNodes, WKWebExtensionContext *context)
{
    if (!bookmarkNodes.count)
        return { };

    Vector<WebExtensionBookmarksParameters> parameters(ReserveCapacityTag { }, bookmarkNodes.count);

    for (id<_WKWebExtensionBookmark> bookmark in bookmarkNodes) {
        auto parametersOptional = createParametersFromProtocolObject(bookmark, context);
        if (parametersOptional)
            parameters.append(WTF::move(*parametersOptional));
    }

    return parameters;
}

static id<_WKWebExtensionBookmark> findBookmarkNodeInTree(NSArray<id<_WKWebExtensionBookmark>> *nodes, StringImpl* targetId, WKWebExtensionContext *context)
{
    for (id<_WKWebExtensionBookmark> node in nodes) {
        if (equal(StringImpl::create([node identifierForWebExtensionContext:context]), targetId))
            return node;

        if (NSArray *children = [node childrenForWebExtensionContext:context]) {
            id<_WKWebExtensionBookmark> foundInChild = findBookmarkNodeInTree(children, targetId, context);
            if (foundInChild)
                return foundInChild;
        }
    }

    return nil;
}

static Vector<WebExtensionBookmarksParameters> createShallowParametersFromProtocolObjects(NSArray<id<_WKWebExtensionBookmark>> *bookmarkNodes, WKWebExtensionContext *context)
{
    if (!bookmarkNodes)
        return { };

    Vector<WebExtensionBookmarksParameters> parameters(ReserveCapacityTag { }, bookmarkNodes.count);

    for (id<_WKWebExtensionBookmark> bookmark in bookmarkNodes) {
        WebExtensionBookmarksParameters node;

        node.nodeId = StringImpl::create([bookmark identifierForWebExtensionContext:context]);
        node.parentId = StringImpl::create([bookmark parentIdentifierForWebExtensionContext:context]);
        node.index = [bookmark indexForWebExtensionContext:context];
        node.title = StringImpl::create([bookmark titleForWebExtensionContext:context]);
        node.url = StringImpl::create([bookmark urlStringForWebExtensionContext:context]);

        parameters.append(WTF::move(node));
    }

    return parameters;
}

static Vector<WebExtensionBookmarksParameters> flattenAndConvertAllBookmarks(NSArray<id<_WKWebExtensionBookmark>> *nodes, WKWebExtensionContext *context)
{
    Vector<WebExtensionBookmarksParameters> flattened;
    if (!nodes)
        return flattened;

    for (id<_WKWebExtensionBookmark> node in nodes) {
        auto parametersOptional = createParametersFromProtocolObject(node, context);

        if (parametersOptional.has_value()) {
            WebExtensionBookmarksParameters params = WTF::move(parametersOptional.value());
            flattened.append(WTF::move(params));
        }

        if ([node bookmarkTypeForWebExtensionContext:context] == _WKWebExtensionBookmarkTypeFolder) {
            if (NSArray<id<_WKWebExtensionBookmark>> *children = [node childrenForWebExtensionContext:context]) {
                Vector<WebExtensionBookmarksParameters> childFlattened = flattenAndConvertAllBookmarks(children, context);
                flattened.appendVector(WTF::move(childFlattened));
            }
        }
    }
    return flattened;
}

static bool bookmarkContainsIgnoringASCIICase(StringImpl* text, StringImpl* substring)
{
    if (isEmpty(substring))
        return true;
    if (isEmpty(text))
        return false;
    return WTF::containsIgnoringASCIICase(text, substring);
}

static RefPtr<StringImpl> normalizeURLStringForComparison(StringImpl* url)
{
    RefPtr<StringImpl> normalized = convertToASCIILowercase(url);
    if (endsWith(normalized, '/'))
        normalized = left(normalized, length(normalized) - 1);
    return normalized;
}

void WebExtensionContext::bookmarksCreate(const std::optional<RefPtr<StringImpl>>& parentId, const std::optional<uint64_t>& index, const std::optional<RefPtr<StringImpl>>& url, const std::optional<RefPtr<StringImpl>>& title, CompletionHandler<void(std::expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.create()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:createBookmarkWithParentIdentifier:index:url:title:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    NSString *parentIdString = parentId ? createNSString(*parentId).get() : nil;
    NSNumber *indexNumber = index ? @(index.value()) : nil;
    NSString *urlString = url ? createNSString(*url).get() : nil;
    NSString *titleString = title ? createNSString(*title).get() : @"";

    [controllerDelegate _webExtensionController:controllerWrapper createBookmarkWithParentIdentifier:parentIdString index:indexNumber url:urlString title:titleString forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *newBookmark, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }
        auto parametersOptional = createParametersFromProtocolObject(newBookmark, contextWrapper);
        if (!parametersOptional.has_value()) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark was null or invalid").get()));
            return;
        }

        completionHandler(std::expected<WebExtensionBookmarksParameters, WebExtensionError> { WTF::move(parametersOptional.value()) });
    }];
}
void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getTree()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *bookmarkNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!bookmarkNodes) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"returned array of bookmarks was invalid").get()));
            return;
        }
        Vector<WebExtensionBookmarksParameters> topLevelNodes = createParametersFromProtocolObjects(bookmarkNodes, contextWrapper);
        completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTF::move(topLevelNodes) });
    }];
}
void WebExtensionContext::bookmarksGetSubTree(StringImpl* bookmarkId, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getSubtree()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    id controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }


    RefPtr<StringImpl> capturedBookmarkId = bookmarkId;
    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"returned array of bookmarks was invalid").get()));
            return;
        }
        id<_WKWebExtensionBookmark> foundNode = findBookmarkNodeInTree(allTopLevelNodes, capturedBookmarkId.get(), contextWrapper);

        std::optional<WebExtensionBookmarksParameters> singleNodeParameters = createParametersFromProtocolObject(foundNode, contextWrapper);

        if (!singleNodeParameters.has_value()) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark was null or invalid").get()));
            return;
        }

        Vector<WebExtensionBookmarksParameters> resultVector;
        resultVector.append(WTF::move(singleNodeParameters.value()));

        completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTF::move(resultVector) });
    }];
}
void WebExtensionContext::bookmarksGet(const Vector<RefPtr<StringImpl>>& bookmarkId, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.get()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }
    Vector<RefPtr<StringImpl>> capturedBookmarkIds = bookmarkId;

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"returned array of bookmarks was invalid").get()));
            return;
        }
        NSMutableArray<id<_WKWebExtensionBookmark>> *foundNodes = [NSMutableArray arrayWithCapacity:capturedBookmarkIds.size()];

        for (auto& targetId : capturedBookmarkIds) {
            NSString *targetIdNSString = createNSString(targetId).autorelease();
            id<_WKWebExtensionBookmark> foundNode = findBookmarkNodeInTree(allTopLevelNodes, StringImpl::create(targetIdNSString).get(), contextWrapper);

            if (!foundNode) {
                completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"A Bookmark ID was not found").get()));
                return;
            }
            [foundNodes addObject:foundNode];
        }

        Vector<WebExtensionBookmarksParameters> foundNodeParameters = createShallowParametersFromProtocolObjects(foundNodes, contextWrapper);
        completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTF::move(foundNodeParameters) });
    }];
}
void WebExtensionContext::bookmarksGetChildren(StringImpl* bookmarkId, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.getChildren()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    id controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    NSString *bookmarkIdNSString = createNSString(bookmarkId).autorelease();

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"returned array of bookmarks was invalid").get()));
            return;
        }

        id<_WKWebExtensionBookmark> parentNode = findBookmarkNodeInTree(allTopLevelNodes, StringImpl::create(bookmarkIdNSString).get(), contextWrapper);

        if (!parentNode) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark Id was not found").get()));
            return;
        }

        if ([parentNode bookmarkTypeForWebExtensionContext:contextWrapper] != _WKWebExtensionBookmarkTypeFolder) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark Id is not a folder").get()));
            return;
        }

        NSArray<id<_WKWebExtensionBookmark>> *directChildren = [parentNode childrenForWebExtensionContext:contextWrapper];

        if (!directChildren) {
            completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { Vector<WebExtensionBookmarksParameters>() });
            return;
        }

        Vector<WebExtensionBookmarksParameters> childrenParameters = createShallowParametersFromProtocolObjects(directChildren, contextWrapper);
        completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTF::move(childrenParameters) });
    }];
}
void WebExtensionContext::bookmarksGetRecent(uint64_t count, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.getRecent()";

    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper
        completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"returned array of bookmarks was invalid").get()));
            return;
        }

        Vector<WebExtensionBookmarksParameters> allBookmarksParameters = flattenAndConvertAllBookmarks(allTopLevelNodes, contextWrapper);

        std::sort(allBookmarksParameters.begin(), allBookmarksParameters.end(), [](const WebExtensionBookmarksParameters& a, const WebExtensionBookmarksParameters& b) {
            return a.dateAdded > b.dateAdded;
        });

        Vector<WebExtensionBookmarksParameters> recentBookmarks(ReserveCapacityTag { }, std::min(static_cast<size_t>(count), allBookmarksParameters.size()));

        for (const auto& bookmarkParams : allBookmarksParameters) {
            if (recentBookmarks.size() >= count)
                break;

            if (bookmarkParams.url.has_value() && length(bookmarkParams.url.value()) > 0)
                recentBookmarks.append(bookmarkParams);
        }

        completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTF::move(recentBookmarks) });
    }];

}
void WebExtensionContext::bookmarksSearch(const std::optional<RefPtr<StringImpl>>& query, const std::optional<RefPtr<StringImpl>>& url, const std::optional<RefPtr<StringImpl>>& title, CompletionHandler<void(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.search()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper bookmarksForExtensionContext:contextWrapper
        completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"returned array of bookmarks was invalid").get()));
            return;
        }

        Vector<WebExtensionBookmarksParameters> allBookmarks = flattenAndConvertAllBookmarks(allTopLevelNodes, contextWrapper);

        Vector<WebExtensionBookmarksParameters> matchingBookmarks;

        Vector<RefPtr<StringImpl>> termsForQueryProperty;
        if (query.has_value() && !isEmpty(*query))
            termsForQueryProperty = split(*query, ' ');

        std::optional<RefPtr<StringImpl>> normalizedURLMatch;
        if (url.has_value())
            normalizedURLMatch = normalizeURLStringForComparison(url.value().get());

        for (const auto& bookmarkParams : allBookmarks) {
            bool matches = true;

            if (query.has_value()) {
                bool allTermsMatch = true;
                for (const auto& term : termsForQueryProperty) {
                    if (!bookmarkContainsIgnoringASCIICase(bookmarkParams.title.get(), term.get()) && (!bookmarkParams.url.has_value() || !bookmarkContainsIgnoringASCIICase(bookmarkParams.url.value().get(), term.get()))) {
                        allTermsMatch = false;
                        break;
                    }
                }
                if (!allTermsMatch)
                    matches = false;
            }

            if (!matches)
                continue;

            if (title.has_value()) {
                if (!equal(bookmarkParams.title, title.value()))
                    matches = false;
            }

            if (!matches)
                continue;

            if (url.has_value()) {
                if (!bookmarkParams.url.has_value())
                    matches = false;
                else {
                    RefPtr<StringImpl> bookmarkNormalizedURL = normalizeURLStringForComparison(bookmarkParams.url.value().get());
                    if (!equal(bookmarkNormalizedURL, normalizedURLMatch.value()))
                        matches = false;
                }
            }

            if (matches) {
                WebExtensionBookmarksParameters resultNode = bookmarkParams;
                resultNode.children = std::nullopt;
                matchingBookmarks.append(WTF::move(resultNode));
            }
        }

        completionHandler(std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTF::move(matchingBookmarks) });
    }];

}
void WebExtensionContext::bookmarksUpdate(StringImpl* bookmarkId, const std::optional<RefPtr<StringImpl>>& url, const std::optional<RefPtr<StringImpl>>& title, CompletionHandler<void(std::expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.update()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:updateBookmarkWithIdentifier:title:url:forExtensionContext:completionHandler:)]) {
    completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    NSString *bookmarkIdNSString = createNSString(bookmarkId).autorelease();
    NSString *titleNSString = title.has_value() ? createNSString(*title).autorelease() : nil;
    NSString *urlNSString = url.has_value() ? createNSString(*url).autorelease() : nil;

    [controllerDelegate _webExtensionController:controllerWrapper updateBookmarkWithIdentifier:bookmarkIdNSString title:titleNSString url:urlNSString forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *updatedBookmark, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!updatedBookmark) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"updating bookmark failed").get()));
            return;
        }

        std::optional<WebExtensionBookmarksParameters> updatedBookmarkParams = createParametersFromProtocolObject(updatedBookmark, contextWrapper);

        if (!updatedBookmarkParams.has_value()) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark was null or invalid").get()));
            return;
        }

        completionHandler(std::expected<WebExtensionBookmarksParameters, WebExtensionError> { WTF::move(updatedBookmarkParams.value()) });
    }];
}
void WebExtensionContext::bookmarksMove(StringImpl* bookmarkId, const std::optional<RefPtr<StringImpl>>& parentId, const std::optional<uint64_t>& index, CompletionHandler<void(std::expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.move()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:moveBookmarkWithIdentifier:toParent:atIndex:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    NSString *bookmarkIdNSString = createNSString(bookmarkId).autorelease();
    NSString *parentIdNSString = parentId.has_value() ? createNSString(*parentId).autorelease() : nil;
    NSNumber *indexNSNumber = index.has_value() ? @(index.value()) : nil;

    [controllerDelegate _webExtensionController:controllerWrapper moveBookmarkWithIdentifier:bookmarkIdNSString toParent:parentIdNSString atIndex:indexNSNumber forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *movedBookmark, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }
        if (!movedBookmark) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"moving bookmark failed").get()));
            return;
        }

        std::optional<WebExtensionBookmarksParameters> movedBookmarkParams = createParametersFromProtocolObject(movedBookmark, contextWrapper);

        if (!movedBookmarkParams.has_value()) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark was null or invalid").get()));
            return;
        }

        completionHandler(std::expected<WebExtensionBookmarksParameters, WebExtensionError> { WTF::move(movedBookmarkParams.value()) });
    }];
}
void WebExtensionContext::bookmarksRemove(StringImpl* bookmarkId, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.remove()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:removeBookmarkWithIdentifier:removeFolderWithChildren:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    NSString *bookmarkIdString = createNSString(bookmarkId).get();
    [controllerDelegate _webExtensionController:controllerWrapper removeBookmarkWithIdentifier:bookmarkIdString removeFolderWithChildren:NO forExtensionContext:contextWrapper completionHandler:^(NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }
        completionHandler({ });
    }];
}
void WebExtensionContext::bookmarksRemoveTree(StringImpl* bookmarkId, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString *const apiName = @"bookmarks.removeTree()";
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    WKWebExtensionContext *contextWrapper = wrapper();

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:removeBookmarkWithIdentifier:removeFolderWithChildren:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    NSString *bookmarkIdString = createNSString(bookmarkId).get();

    [controllerDelegate _webExtensionController:controllerWrapper removeBookmarkWithIdentifier:bookmarkIdString removeFolderWithChildren:YES forExtensionContext:contextWrapper completionHandler:^(NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }
        completionHandler({ });
    }];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
