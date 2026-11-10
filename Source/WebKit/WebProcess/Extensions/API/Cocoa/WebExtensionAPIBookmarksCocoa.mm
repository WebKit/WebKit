/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#import "WebExtensionAPIBookmarks.h"
#import "CocoaHelpers.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIKeys.h"
#import "WebExtensionBookmarksParameters.h"
#import "WebExtensionContextMessages.h"
#import "WebProcess.h"

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

namespace WebKit {


static NSDictionary *toAPI(const WebExtensionBookmarksParameters& node)
{
    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];

    dictionary[idKey] = createNSString(node.nodeId).get();
    dictionary[indexKey] = @(node.index);
    dictionary[titleKey] = createNSString(node.title).get();

    if (node.parentId)
        dictionary[parentIdKey] = createNSString(*node.parentId).get();
    if (node.url && !isEmpty(*node.url)) {
        dictionary[urlKey] = createNSString(*node.url).get();
        dictionary[typeKey] = @"bookmark";
        double seconds = node.dateAdded.secondsSinceEpoch().value();
        dictionary[dateAddedKey] = @(seconds * 1000.0);
    } else
        dictionary[typeKey] = @"folder";

    if (node.children) {
        NSMutableArray *childrenArray = [NSMutableArray array];
        for (const auto& childNode : *node.children)
            [childrenArray addObject:toAPI(childNode)];
        dictionary[childrenKey] = childrenArray;
    }

    return dictionary;
}


static std::optional<WebExtensionAPIBookmarks::BookmarkTreeNodeType> toTypeImpl(NSString *typeString)
{
    if (!typeString)
        return std::nullopt;

    if ([typeString isEqualToString:bookmarkKey])
        return WebExtensionAPIBookmarks::BookmarkTreeNodeType::Bookmark;
    if ([typeString isEqualToString:folderKey])
        return WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder;

    return std::nullopt;
}

void WebExtensionAPIBookmarks::createBookmark(NSDictionary *bookmark, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        indexKey: NSNumber.class,
        parentIdKey: NSString.class,
        typeKey: NSString.class,
        urlKey: NSString.class,
        titleKey: NSString.class
    };

    if (!validateDictionary(bookmark, @"bookmark", nil, types, outExceptionString))
        return;

    if (bookmark[typeKey]) {
        auto parsedType = toTypeImpl(dynamic_objc_cast<NSString>(bookmark[typeKey]));
        if (!parsedType) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(typeKey).get(), StringImpl::create(@"it must specify either 'bookmark' or 'folder'").get())).autorelease();
            return;
        }
    }

    NSString *parentMockIdString = bookmark[parentIdKey];
    NSString *titleString = bookmark[titleKey];
    NSString *urlString = bookmark[urlKey];
    const std::optional<RefPtr<StringImpl>> parentMockId = StringImpl::create(parentMockIdString);
    const std::optional<RefPtr<StringImpl>> title = StringImpl::create(titleString);
    const std::optional<RefPtr<StringImpl>> url = StringImpl::create(urlString);

    std::optional<uint64_t> index = objectForKey<NSNumber>(bookmark, indexKey).unsignedLongValue;
    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksCreate(parentMockId, index, url, title),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarksParameters, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }
            auto createdNode = result.value();
            NSDictionary* newNodeDictionary = toAPI(createdNode);
            callback->call(newNodeDictionary);
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::getChildren(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksGetChildren(StringImpl::create(bookmarkIdentifier).get()),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }
            const Vector<WebExtensionBookmarksParameters>& resultVector = result.value();

            NSMutableArray *resultArray = [NSMutableArray arrayWithCapacity:resultVector.size()];
            for (const auto& node : resultVector)
                [resultArray addObject:toAPI(node)];

            callback->call(resultArray);
        },

        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::getRecent(long long numberOfItems, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    if (numberOfItems < 1) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"numberOfItems").get(), StringImpl::create(@"it must be at least 1").get())).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksGetRecent(static_cast<uint64_t>(numberOfItems)),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            Vector<WebExtensionBookmarksParameters> recentBookmarksParams = result.value();
            NSMutableArray *resultArray = [NSMutableArray arrayWithCapacity:recentBookmarksParams.size()];

            for (const auto& bookmarkParams : recentBookmarksParams)
                [resultArray addObject:toAPI(bookmarkParams)];

            callback->call(resultArray);
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::getSubTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksGetSubTree(StringImpl::create(bookmarkIdentifier).get()),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            const Vector<WebExtensionBookmarksParameters>& resultVector = result.value();
            const WebExtensionBookmarksParameters& subtreeRootNode = resultVector[0];
            if (!subtreeRootNode.children) {
                callback->call(@[], nil);
                return;
            }

            NSMutableArray *childrenArray = [NSMutableArray arrayWithCapacity:subtreeRootNode.children->size()];
            for (const auto& childNode : *subtreeRootNode.children)
                [childrenArray addObject:toAPI(childNode)];

            callback->call(childrenArray);
        },

        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::getTree(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksGetTree(),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            const Vector<WebExtensionBookmarksParameters>& resultVector = result.value();

            NSMutableArray *resultArray = [NSMutableArray arrayWithCapacity:resultVector.size()];
            for (const auto& topLevelNode : resultVector)
                [resultArray addObject:toAPI(topLevelNode)];

            callback->call(resultArray);
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::get(NSObject *idOrIdList, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    Vector<RefPtr<StringImpl>> bookmarkIds;

    if ([idOrIdList isKindOfClass:[NSString class]]) {
        bookmarkIds.append(StringImpl::create(static_cast<NSString *>(idOrIdList)));
    } else if ([idOrIdList isKindOfClass:[NSArray class]]) {
        for (NSString *bookmarkId in static_cast<NSArray<NSString *> *>(idOrIdList)) {
            if (![bookmarkId isKindOfClass:[NSString class]] || !bookmarkId.length) {
                *outExceptionString = @"Each item in the ID list must be a non-empty string.";
                return;
            }
            bookmarkIds.append(StringImpl::create(bookmarkId));
        }
    } else {
        *outExceptionString = @"The first argument must be a string or an array of strings.";
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksGet(bookmarkIds),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            const Vector<WebExtensionBookmarksParameters>& resultVector = result.value();

            NSMutableArray *resultArray = [NSMutableArray arrayWithCapacity:resultVector.size()];
            for (const auto& node : resultVector)
                [resultArray addObject:toAPI(node)];

            callback->call(resultArray);
        },

        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::move(NSString *bookmarkIdentifier, NSDictionary *destination, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    std::optional<RefPtr<StringImpl>> ipcParentId;
    std::optional<uint64_t> ipcIndex;

    if (![destination isKindOfClass:[NSDictionary class]]) {
        if (outExceptionString)
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"destination").get(), StringImpl::create(@"property must be an object.").get())).autorelease();
        return;
    }

    static NSDictionary<NSString *, id> *types = @{
        indexKey: NSNumber.class,
        parentIdKey: NSString.class,
    };

    if (!validateDictionary(destination, @"destination", nil, types, outExceptionString))
        return;

    id parentIdObj = destination[@"parentId"];
    if (parentIdObj)
        ipcParentId = StringImpl::create(dynamic_objc_cast<NSString>(parentIdObj));

    id indexObj = destination[@"index"];
    if (indexObj)
        ipcIndex = dynamic_objc_cast<NSNumber>(indexObj).unsignedLongLongValue;

    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksMove(StringImpl::create(bookmarkIdentifier).get(), ipcParentId, ipcIndex),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarksParameters, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            NSDictionary *movedNodeDictionary = toAPI(result.value());
            callback->call(movedNodeDictionary);
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::remove(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksRemove(StringImpl::create(bookmarkIdentifier).get()),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }
            callback->call({ });
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::removeTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksRemoveTree(StringImpl::create(bookmarkIdentifier).get()),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }
            callback->call({ });
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::search(NSObject *query, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    std::optional<RefPtr<StringImpl>> ipcQueryTerms;
    std::optional<RefPtr<StringImpl>> ipcTitle;
    std::optional<RefPtr<StringImpl>> ipcURL;

    if ([query isKindOfClass:NSString.class])
        ipcQueryTerms = StringImpl::create(dynamic_objc_cast<NSString>(query));
    else if ([query isKindOfClass:NSDictionary.class]) {
        NSDictionary *queryDict = dynamic_objc_cast<NSDictionary>(query);

        static NSDictionary<NSString *, id> *types = @{
            urlKey: NSString.class,
            titleKey: NSString.class,
            queryKey: NSString.class
        };

        if (!validateDictionary(queryDict, @"query", nil, types, outExceptionString))
            return;

        id queryTermsObj = queryDict[@"query"];
        if (queryTermsObj)
            ipcQueryTerms = StringImpl::create(dynamic_objc_cast<NSString>(queryTermsObj));

        id titleObj = queryDict[@"title"];
        if (titleObj)
            ipcTitle = StringImpl::create(dynamic_objc_cast<NSString>(titleObj));

        id urlObj = queryDict[@"url"];
        if (urlObj)
            ipcURL = StringImpl::create(dynamic_objc_cast<NSString>(urlObj));

    } else {
        if (outExceptionString)
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"query").get(), StringImpl::create(@"property must be a string or object.").get())).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksSearch(ipcQueryTerms, ipcURL, ipcTitle),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            const Vector<WebExtensionBookmarksParameters>& resultVector = result.value();
            NSMutableArray *resultArray = [NSMutableArray arrayWithCapacity:resultVector.size()];
            for (const auto& node : resultVector)
                [resultArray addObject:toAPI(node)];

            callback->call(resultArray);
        },
        extensionContext().identifier()
    );
}

void WebExtensionAPIBookmarks::update(NSString *bookmarkIdentifier, NSDictionary *changes, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    if (!bookmarkIdentifier || ![bookmarkIdentifier isKindOfClass:[NSString class]] || !bookmarkIdentifier.length) {
        if (outExceptionString)
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"bookmarkID").get(), StringImpl::create(@"property must be a non-empty string.").get())).autorelease();
        return;
    }

    if (!changes || ![changes isKindOfClass:[NSDictionary class]]) {
        if (outExceptionString)
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"changes").get(), StringImpl::create(@"property must be an object (dictionary).").get())).autorelease();
        return;
    }

    static NSDictionary<NSString *, id> *types = @{
        urlKey: NSString.class,
        titleKey: NSString.class
    };

    if (!validateDictionary(changes, @"changes", nil, types, outExceptionString))
        return;

    std::optional<RefPtr<StringImpl>> newTitle;
    std::optional<RefPtr<StringImpl>> newURL;

    id titleObj = changes[@"title"];
    if (titleObj)
        newTitle = StringImpl::create(dynamic_objc_cast<NSString>(titleObj));

    id urlStringObj = changes[@"url"];
    if (urlStringObj)
        newURL = StringImpl::create(dynamic_objc_cast<NSString>(urlStringObj));

    if (!newTitle.has_value() && !newURL.has_value()) {
        if (outExceptionString)
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"title or url").get(), StringImpl::create(@"must be specified.").get())).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(
        Messages::WebExtensionContext::BookmarksUpdate(StringImpl::create(bookmarkIdentifier).get(), newURL, newTitle),
        [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarksParameters, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().get());
                return;
            }

            NSDictionary* updatedNodeDictionary = toAPI(result.value());
            callback->call(updatedNodeDictionary);
        },
        extensionContext().identifier()
    );
}
} // namespace WebKit

#endif

