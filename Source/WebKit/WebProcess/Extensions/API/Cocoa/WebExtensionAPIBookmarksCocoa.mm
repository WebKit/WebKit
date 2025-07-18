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

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

namespace WebKit {

static NSString * const idKey = @"id";
static NSString * const urlKey = @"url";
static NSString * const titleKey = @"title";
static NSString * const indexKey = @"index";
static NSString * const parentIdKey = @"parentId";
static NSString * const dateAddedKey = @"dateAdded";
static NSString * const typeKey = @"type";
static NSString * const childrenKey = @"children";
static NSString * const bookmarkKey = @"bookmark";
static NSString * const folderKey = @"folder";
// FIXME: (152505488) rename this when we aren't mocking bookmarks anymore.
static NSString * const bookmarksRootId = @"testBookmarksRoot";

static NSString *toWebAPI(WebExtensionAPIBookmarks::BookmarkTreeNodeType type, NSString *inputURL)
{
    if (inputURL.length)
        return bookmarkKey;

    switch (type) {
    case WebExtensionAPIBookmarks::BookmarkTreeNodeType::Bookmark:
        return bookmarkKey;
    case WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder:
        return folderKey;
    }

    return folderKey;
}

static NSDictionary *toWebAPI(const WebExtensionAPIBookmarks::MockBookmarkNode& node)
{
    NSDictionary *baseNodeDictionary = @{
        idKey: node.id.createNSString().get(),
        parentIdKey: node.parentId.createNSString().get(),
        titleKey: node.title.createNSString().get(),
        urlKey: node.url.createNSString().get(),
        indexKey: @(node.index),
        dateAddedKey: @(node.dateAdded.secondsSinceEpoch().milliseconds()),
        typeKey: toWebAPI(node.type, node.url.createNSString().get())
    };

    NSMutableDictionary *tempNode = baseNodeDict.mutableCopy;

    NSMutableArray *childrenArray = [NSMutableArray array];
    if (node.type == WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder) {
        for (const auto& child : node.children)
            [childrenArray addObject:toWebAPI(child)];
        [tempNode setObject:childrenArray forKey:childrenKey];
    }

    return tempNode;
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

void WebExtensionAPIBookmarks::initializeMockBookmarksInternal()
{
    Ref rootNode = MockBookmarkNode::create();
    rootNode->id = bookmarksRootId;
    rootNode->parentId = ""_s;
    rootNode->title = ""_s;
    rootNode->type = WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder;
    rootNode->dateAdded = WTF::WallTime::fromRawSeconds(0);
    rootNode->index = 0;

    m_mockBookmarks.add(rootNode->id, WTFMove(rootNode));

    auto addStandardFolder = [&](const String& identifier, const String& title, int index) {
        Ref folderRef = MockBookmarkNode::create();
        folderRef->id = identifier;
        folderRef->parentId = bookmarksRootId;
        folderRef->title = title;
        folderRef->type = WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder;
        folderRef->dateAdded = WTF::WallTime::now();
        folderRef->index = index;

        m_mockBookmarks.add(folderRef->id, WTFMove(folderRef));

        auto rootOptional = m_mockBookmarks.getOptional(bookmarksRootId);
        auto folderOptional = m_mockBookmarks.getOptional(identifier);
        if (rootOptional && folderOptional)
            rootOptional.value()->children.append(folderOptional.value());
    };

    addStandardFolder(@"testFavorites", @"Favorites", 0);

    if (auto rootOptional = m_mockBookmarks.getOptional(bookmarksRootId)) {
        std::sort((*rootOptional)->children.begin(), (*rootOptional)->children.end(),
            [](const MockBookmarkNode& a, const MockBookmarkNode& b) {
                return a.index < b.index;
            });
    }
}

void WebExtensionAPIBookmarks::createBookmark(NSDictionary *bookmark, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    if (m_mockBookmarks.isEmpty())
        initializeMockBookmarksInternal();

    WebExtensionAPIBookmarks::CreateDetails parsedDetails;

    static NSDictionary<NSString *, id> *types = @{
        idKey: NSString.class,
        indexKey: NSNumber.class,
        parentIdKey: NSString.class,
        typeKey: NSString.class,
        urlKey: NSString.class,
        dateAddedKey: NSNumber.class,
        titleKey: NSString.class
    };

    if (!validateDictionary(bookmark, @"bookmark", nil, types, outExceptionString))
        return;

    parsedDetails.index = objectForKey<NSNumber>(bookmark, indexKey).unsignedLongValue;
    parsedDetails.parentId = objectForKey<NSString>(bookmark, parentIdKey);
    parsedDetails.title = objectForKey<NSString>(bookmark, titleKey);
    parsedDetails.url = objectForKey<NSString>(bookmark, urlKey);

    if (bookmark[typeKey]) {
        auto parsedType = toTypeImpl(dynamic_objc_cast<NSString>(bookmark[typeKey]));
        if (!parsedType) {
            *outExceptionString = toErrorString(nullString(), typeKey, @"it must specify either 'bookmark' or 'folder'").createNSString().autorelease();
            return;
        }

        parsedDetails.type = parsedType;
    }

    Ref newNode = MockBookmarkNode::create();
    newNode->id = bookmark[idKey];
    newNode->url = bookmark[urlKey];
    newNode->title = bookmark[titleKey];
    newNode->parentId = bookmark[parentIdKey] ? bookmark[parentIdKey] : bookmarksRootId;
    newNode->index = parsedDetails.index.value();
    newNode->type = bookmark[urlKey] ? WebExtensionAPIBookmarks::BookmarkTreeNodeType::Bookmark : WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder;

    double msSinceEpoch = dynamic_objc_cast<NSNumber>(bookmark[dateAddedKey]).doubleValue;
    newNode->dateAdded = WTF::WallTime::fromRawSeconds(msSinceEpoch / 1000.0);

    String parentId = newNode->parentId;
    String newNodeId = newNode->id;

    auto parentOptional = m_mockBookmarks.getOptional(parentId);
    if (!parentOptional) {
        *outExceptionString = toErrorString(nullString(), parentIdKey, @"it could not be mapped to a node").createNSString().autorelease();
        return;
    }
    Ref parentNode = parentOptional.value();

    if (parentNode->type != WebExtensionAPIBookmarks::BookmarkTreeNodeType::Folder) {
        *outExceptionString = toErrorString(nullString(), parentIdKey, @"it must specify a node which is a folder").createNSString().autorelease();
        return;
    }

    m_mockBookmarks.add(newNodeId, newNode);

    parentNode->children.append(newNode);
    std::sort(parentNode->children.begin(), parentNode->children.end(),
        [](const Ref<MockBookmarkNode>& a, const Ref<MockBookmarkNode>& b) {
            return a->index < b->index;
        });
    callback->call(toWebAPI(newNode));
}

void WebExtensionAPIBookmarks::getChildren(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::getRecent(long long numberOfItems, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    if (numberOfItems < 1) {
        *outExceptionString = toErrorString(nullString(), @"numberOfItems", @"it must be at least 1").createNSString().autorelease();
        return;
    }

    std::vector<Ref<MockBookmarkNode>> allBookmarks;
    allBookmarks.reserve(m_mockBookmarks.size());

    for (const auto& pair : m_mockBookmarks)
        allBookmarks.push_back(pair.value);

    std::sort(allBookmarks.begin(), allBookmarks.end(), [](const Ref<MockBookmarkNode>& a, const Ref<MockBookmarkNode>& b) {
        return a->dateAdded > b->dateAdded;
    });

    NSMutableArray *resultArray = [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(numberOfItems)];

    for (const auto& nodeRef : allBookmarks) {
        if (resultArray.count >= (NSUInteger)numberOfItems)
            break;
        if (nodeRef->type == WebExtensionAPIBookmarks::BookmarkTreeNodeType::Bookmark)
            [resultArray addObject:toWebAPI(nodeRef)];
    }
    callback->call(resultArray);
}

void WebExtensionAPIBookmarks::getSubTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::getTree(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    if (m_mockBookmarks.isEmpty())
        initializeMockBookmarksInternal();

    auto rootNodeOptional = m_mockBookmarks.getOptional(bookmarksRootId);
    if (!rootNodeOptional) {
        if (outExceptionString)
            *outExceptionString = toErrorString(nullString(), nullString(), @"root not found").createNSString().autorelease();
        return;
    }

    if (auto rootOptional = m_mockBookmarks.getOptional(bookmarksRootId)) {
        Ref finalTreeRoot = rootOptional.value();
        NSDictionary *rootDict = toWebAPI(finalTreeRoot);
        NSArray* resultArray = [NSArray arrayWithObject:rootDict];
        callback->call(resultArray);
    }
}

void WebExtensionAPIBookmarks::get(NSObject *idOrIdList, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::move(NSString *bookmarkIdentifier, NSDictionary *destination, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::remove(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::removeTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::search(NSObject *query, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}

void WebExtensionAPIBookmarks::update(NSString *bookmarkIdentifier, NSDictionary *changes, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    callback->reportError(@"unimplemented");
}
} // namespace WebKit

#endif

