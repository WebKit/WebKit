// Copyright © 2025  All rights reserved.
#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#include "WebExtensionBookmarksParameters.h"
#include "WebExtensionController.h"
#include "_WKWebExtensionBookmarks.h"

#include <wtf/Vector.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import "WKWebExtensionControllerDelegatePrivate.h"

namespace WebKit {

bool WebExtensionContext::isBookmarksMessageAllowed(IPC::Decoder& message)
{
    // FIXME: fix
    return false;
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
    parameters.nodeId = nodeId;

    parameters.parentId = [bookmark parentIdentifierForWebExtensionContext:context];
    parameters.index = [bookmark indexForWebExtensionContext:context];
    parameters.title = [bookmark titleForWebExtensionContext:context];
    parameters.url = [bookmark urlForWebExtensionContext:context];

    if (NSArray *children = [bookmark childrenForWebExtensionContext:context])
        parameters.children = createParametersFromProtocolObjects(children, context);

    return parameters;
}

static Vector<WebExtensionBookmarksParameters> createParametersFromProtocolObjects(NSArray<id<_WKWebExtensionBookmark>> *bookmarkNodes, WKWebExtensionContext *context)
{
    if (!bookmarkNodes)
        return { };

    Vector<WebExtensionBookmarksParameters> parameters;
    parameters.reserveInitialCapacity(bookmarkNodes.count);

    for (id<_WKWebExtensionBookmark> bookmark in bookmarkNodes) {
        auto parametersOptional = createParametersFromProtocolObject(bookmark, context);
        if (parametersOptional)
            parameters.append(WTFMove(*parametersOptional));
    }

    return parameters;
}

static id<_WKWebExtensionBookmark> findBookmarkNodeInTree(NSArray<id<_WKWebExtensionBookmark>> *nodes, const String& targetId, WKWebExtensionContext *context)
{
    for (id<_WKWebExtensionBookmark> node in nodes) {
        if (String([node identifierForWebExtensionContext:context]) == targetId)
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

    Vector<WebExtensionBookmarksParameters> parameters;
    parameters.reserveInitialCapacity(bookmarkNodes.count);

    for (id<_WKWebExtensionBookmark> bookmark in bookmarkNodes) {
        WebExtensionBookmarksParameters node;

        node.nodeId = [bookmark identifierForWebExtensionContext:context];
        node.parentId = [bookmark parentIdentifierForWebExtensionContext:context];
        node.index = [bookmark indexForWebExtensionContext:context];
        node.title = [bookmark titleForWebExtensionContext:context];
        node.url = [bookmark urlForWebExtensionContext:context];

        parameters.append(WTFMove(node));
    }

    return parameters;
}

void WebExtensionContext::bookmarksCreate(const std::optional<String>& parentId, const std::optional<uint64_t>& index, const std::optional<String>& url, const std::optional<String>& title, CompletionHandler<void(Expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
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
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    NSString *parentIdString = parentId ? parentId->createNSString().get() : nil;
    NSNumber *indexNumber = index ? @(index.value()) : nil;
    NSString *urlString = url ? url->createNSString().get() : nil;
    NSString *titleString = title ? title->createNSString().get() : @"";

    [controllerDelegate _webExtensionController:controllerWrapper createBookmarkWithParentIdentifier:parentIdString index:indexNumber url:urlString title:titleString forExtensionContext:contextWrapper completionHandler:^(NSObject<_WKWebExtensionBookmark> *newBookmark, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"error was reported"));
            return;
        }
        auto parametersOptional = createParametersFromProtocolObject(newBookmark, contextWrapper);
        if (!parametersOptional.has_value()) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark was null or invalid"));
            return;
        }

        completionHandler(Expected<WebExtensionBookmarksParameters, WebExtensionError> { WTFMove(parametersOptional.value()) });
    }];
}
void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(Expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
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

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:getBookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:controllerWrapper getBookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *bookmarkNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"failed because it returned an error"));
            return;
        }

        if (!bookmarkNodes) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"returned array of bookmarks was invalid"));
            return;
        }
        Vector<WebExtensionBookmarksParameters> topLevelNodes = createParametersFromProtocolObjects(bookmarkNodes, contextWrapper);
        WebExtensionBookmarksParameters rootNode;

        rootNode.nodeId = @"testBookmarksRoot";
        rootNode.children = WTFMove(topLevelNodes);
        completionHandler(Expected<WebExtensionBookmarksParameters, WebExtensionError> { WTFMove(rootNode) });
    }];
}
void WebExtensionContext::bookmarksGetSubTree(const String& bookmarkId, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
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

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:getBookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }


    [controllerDelegate _webExtensionController:controllerWrapper getBookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"failed because it returned an error"));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"returned array of bookmarks was invalid"));
            return;
        }
        id<_WKWebExtensionBookmark> foundNode = findBookmarkNodeInTree(allTopLevelNodes, bookmarkId, contextWrapper);

        std::optional<WebExtensionBookmarksParameters> singleNodeParameters = createParametersFromProtocolObject(foundNode, contextWrapper);

        if (!singleNodeParameters.has_value()) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark was null or invalid"));
            return;
        }

        Vector<WebExtensionBookmarksParameters> resultVector;
        resultVector.append(WTFMove(singleNodeParameters.value()));

        completionHandler(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTFMove(resultVector) });
    }];
}
void WebExtensionContext::bookmarksGet(const Vector<String>& bookmarkId, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
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

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:getBookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }
    Vector<String> capturedBookmarkIds = bookmarkId;

    [controllerDelegate _webExtensionController:controllerWrapper getBookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"failed because it returned an error"));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"returned array of bookmarks was invalid"));
            return;
        }
        NSMutableArray<id<_WKWebExtensionBookmark>> *foundNodes = [NSMutableArray arrayWithCapacity:capturedBookmarkIds.size()];

        for (const String& targetId : capturedBookmarkIds) {
            NSString *targetIdNSString = targetId.createNSString().autorelease();
            id<_WKWebExtensionBookmark> foundNode = findBookmarkNodeInTree(allTopLevelNodes, targetIdNSString, contextWrapper);

            if (!foundNode) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"A Bookmark ID was not found"));
                return;
            }
            [foundNodes addObject:foundNode];
        }

        Vector<WebExtensionBookmarksParameters> foundNodeParameters = createShallowParametersFromProtocolObjects(foundNodes, contextWrapper);
        completionHandler(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTFMove(foundNodeParameters) });
    }];
}
void WebExtensionContext::bookmarksGetChildren(const String& bookmarkId, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
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

    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:getBookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    NSString *bookmarkIdNSString = bookmarkId.createNSString().autorelease();

    [controllerDelegate _webExtensionController:controllerWrapper getBookmarksForExtensionContext:contextWrapper completionHandler:^(NSArray<id<_WKWebExtensionBookmark>> *allTopLevelNodes, NSError *error) {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"failed because it returned an error"));
            return;
        }

        if (!allTopLevelNodes) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"returned array of bookmarks was invalid"));
            return;
        }

        id<_WKWebExtensionBookmark> parentNode = findBookmarkNodeInTree(allTopLevelNodes, bookmarkIdNSString, contextWrapper);

        if (!parentNode) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark Id was not found"));
            return;
        }

        if ([parentNode bookmarkTypeForWebExtensionContext:contextWrapper] != _WKWebExtensionBookmarkTypeFolder) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark Id is not a folder"));
            return;
        }

        NSArray<id<_WKWebExtensionBookmark>> *directChildren = [parentNode childrenForWebExtensionContext:contextWrapper];

        if (!directChildren) {
            completionHandler(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { Vector<WebExtensionBookmarksParameters>() });
            return;
        }

        Vector<WebExtensionBookmarksParameters> childrenParameters = createShallowParametersFromProtocolObjects(directChildren, contextWrapper);
        completionHandler(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError> { WTFMove(childrenParameters) });
    }];
}
void WebExtensionContext::bookmarksGetRecent(uint64_t count, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksSearch(const std::optional<String>& query, const std::optional<String>& url, const std::optional<String>& title, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksUpdate(const String& bookmarkId, const std::optional<String>& url, const std::optional<String>& title, CompletionHandler<void(Expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksMove(const String& bookmarkId, const std::optional<String>& parentId, const std::optional<uint64_t>& index, CompletionHandler<void(Expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksRemove(const String& bookmarkId, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksRemoveTree(const String& bookmarkId, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{

}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
