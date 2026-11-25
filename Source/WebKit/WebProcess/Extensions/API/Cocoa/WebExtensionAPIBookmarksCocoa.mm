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

#import "config.h"
#import "WebExtensionAPIBookmarks.h"
#import "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import "CocoaHelpers.h"
#import "MessageSenderInlines.h"
#import "WebExtensionBookmarkChanges.h"
#import "WebExtensionBookmarkParameters.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

static inline NSDictionary *toWebAPI(const WebExtensionBookmarkParameters& bookmark)
{
    auto *result = [NSMutableDictionary dictionary];

    result[idKey] = createNSString(bookmark.identifier).get();
    result[titleKey] = createNSString(bookmark.title).get();

    if (bookmark.index)
        result[indexKey] = @(*bookmark.index);

    if (bookmark.parentIdentifier)
        result[parentIdKey] = createNSString(*bookmark.parentIdentifier).get();

    if (bookmark.url && !bookmark.url->isEmpty()) {
        result[urlKey] = createNSString(bookmark.url->string()).get();
        result[typeKey] = bookmarkKey;
        result[dateAddedKey] = @(bookmark.dateAdded.secondsSinceEpoch().milliseconds());
    } else
        result[typeKey] = folderKey;

    switch (bookmark.folderType) {
    case WebExtensionBookmarkFolderType::None:
        break;
    case WebExtensionBookmarkFolderType::BookmarksBar:
        result[folderTypeKey] = @"bookmarks-bar";
        break;
    case WebExtensionBookmarkFolderType::Managed:
        result[folderTypeKey] = @"managed";
        break;
    case WebExtensionBookmarkFolderType::Other:
        result[folderTypeKey] = @"other";
        break;
    }

    if (bookmark.managed)
        result[unmodifiableKey] = @"managed";

    result[syncingKey] = @(bookmark.synced);

    if (bookmark.children) {
        result[childrenKey] = createNSArray(*bookmark.children, [](const auto& child) {
            return toWebAPI(child);
        }).get();
    }

    return [result copy];
}

static inline NSArray *toWebAPI(const Vector<WebExtensionBookmarkParameters>& bookmarks)
{
    return createNSArray(bookmarks, [](const auto& bookmark) {
        return toWebAPI(bookmark);
    }).autorelease();
}

void WebExtensionAPIBookmarks::createBookmark(NSDictionary *bookmark, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/create

    static NSDictionary<NSString *, id> *types = @{
        indexKey: NSNumber.class,
        parentIdKey: NSString.class,
        typeKey: NSString.class,
        urlKey: NSString.class,
        titleKey: NSString.class
    };

    if (!validateDictionary(bookmark, bookmarkKey, nil, types, outExceptionString))
        return;

    if (auto *type = objectForKey<NSString>(bookmark, typeKey)) {
        if (![type isEqualToString:bookmarkKey] && ![type isEqualToString:folderKey]) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(typeKey).get(), StringImpl::create(@"it must specify either 'bookmark' or 'folder'").get())).autorelease();
            return;
        }
    }

    WebExtensionBookmarkChanges parameters;

    if (auto *parentIdentifier = objectForKey<NSString>(bookmark, parentIdKey))
        parameters.parentIdentifier = StringImpl::create(parentIdentifier);

    if (auto *title = objectForKey<NSString>(bookmark, titleKey))
        parameters.title = StringImpl::create(title);

    if (auto *urlString = objectForKey<NSString>(bookmark, urlKey); urlString.length) {
        URL url { StringImpl::create(urlString) };
        if (!url.isValid()) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(urlKey).get(), StringImpl::create(@"it must be a valid URL").get())).autorelease();
            return;
        }

        parameters.url = WTF::move(url);
    }

    if (auto *index = objectForKey<NSNumber>(bookmark, indexKey)) {
        if (index.longLongValue < 0) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(indexKey).get(), StringImpl::create(@"it must not be negative").get())).autorelease();
            return;
        }

        parameters.index = index.unsignedLongLongValue;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksCreate(WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarkParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getChildren(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getChildren

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetChildren(StringImpl::create(bookmarkIdentifier).get()), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getRecent(long long numberOfItems, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getRecent

    if (numberOfItems < 1) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(numberOfItemsKey).get(), StringImpl::create(@"it must be at least 1").get())).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetRecent(static_cast<uint64_t>(numberOfItems)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getSubTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getSubTree

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetSubTree(StringImpl::create(bookmarkIdentifier).get()), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getTree(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getTree

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetTree(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::get(NSObject *idOrIdList, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/get

    if (!validateObject(idOrIdList, idOrIdListKey, [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], nil], outExceptionString))
        return;

    Vector<RefPtr<StringImpl>> bookmarkIdentifiers;

    if (auto *bookmarkIdentifier = dynamic_objc_cast<NSString>(idOrIdList))
        bookmarkIdentifiers.append(StringImpl::create(bookmarkIdentifier));
    else if (auto *identifierList = dynamic_objc_cast<NSArray>(idOrIdList)) {
        for (NSString *bookmarkIdentifier in identifierList) {
            if (!bookmarkIdentifier.length) {
                *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(idOrIdListKey).get(), StringImpl::create(@"each item in the list must be a non-empty string").get())).autorelease();
                return;
            }

            bookmarkIdentifiers.append(StringImpl::create(bookmarkIdentifier));
        }
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGet(bookmarkIdentifiers), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::move(NSString *bookmarkIdentifier, NSDictionary *destination, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/move

    static NSDictionary<NSString *, id> *types = @{
        indexKey: NSNumber.class,
        parentIdKey: NSString.class,
    };

    if (!validateDictionary(destination, destinationKey, nil, types, outExceptionString))
        return;

    WebExtensionBookmarkChanges parameters;
    parameters.identifier = StringImpl::create(bookmarkIdentifier);

    if (auto *parentIdentifier = objectForKey<NSString>(destination, parentIdKey))
        parameters.parentIdentifier = StringImpl::create(parentIdentifier);

    if (auto *index = objectForKey<NSNumber>(destination, indexKey)) {
        if (index.longLongValue < 0) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(indexKey).get(), StringImpl::create(@"it must not be negative").get())).autorelease();
            return;
        }

        parameters.index = index.unsignedLongLongValue;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksMove(WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarkParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::remove(NSObject *idOrIdList, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/remove

    if (!validateObject(idOrIdList, idOrIdListKey, [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], nil], outExceptionString))
        return;

    Vector<RefPtr<StringImpl>> bookmarkIdentifiers;

    if (auto *bookmarkIdentifier = dynamic_objc_cast<NSString>(idOrIdList))
        bookmarkIdentifiers.append(StringImpl::create(bookmarkIdentifier));
    else if (auto *identifierList = dynamic_objc_cast<NSArray>(idOrIdList)) {
        for (NSString *bookmarkIdentifier in identifierList)
            bookmarkIdentifiers.append(StringImpl::create(bookmarkIdentifier));
    }

    if (bookmarkIdentifiers.isEmpty()) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(idOrIdListKey).get(), StringImpl::create(@"it must contain at least one identifier").get())).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksRemove(bookmarkIdentifiers), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::removeTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/removeTree

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksRemoveTree(StringImpl::create(bookmarkIdentifier).get()), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::search(NSObject *query, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/search

    if (!validateObject(query, queryKey, [NSOrderedSet orderedSetWithObjects:NSString.class, NSDictionary.class, nil], outExceptionString))
        return;

    std::optional<RefPtr<StringImpl>> queryTerms;
    std::optional<RefPtr<StringImpl>> title;
    std::optional<RefPtr<StringImpl>> url;

    if (auto *queryString = dynamic_objc_cast<NSString>(query))
        queryTerms = StringImpl::create(queryString);
    else if (auto *queryDictionary = dynamic_objc_cast<NSDictionary>(query)) {
        static NSDictionary<NSString *, id> *types = @{
            urlKey: NSString.class,
            titleKey: NSString.class,
            queryKey: NSString.class
        };

        if (!validateDictionary(queryDictionary, queryKey, nil, types, outExceptionString))
            return;

        if (auto *queryString = objectForKey<NSString>(queryDictionary, queryKey))
            queryTerms = StringImpl::create(queryString);

        if (auto *titleString = objectForKey<NSString>(queryDictionary, titleKey))
            title = StringImpl::create(titleString);

        if (auto *urlString = objectForKey<NSString>(queryDictionary, urlKey))
            url = StringImpl::create(urlString);
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksSearch(queryTerms, url, title), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::update(NSString *bookmarkIdentifier, NSDictionary *changes, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/update

    if (!bookmarkIdentifier.length) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(idKey).get(), StringImpl::create(@"it must be a non-empty string").get())).autorelease();
        return;
    }

    static NSDictionary<NSString *, id> *types = @{
        urlKey: NSString.class,
        titleKey: NSString.class
    };

    if (!validateDictionary(changes, changesKey, nil, types, outExceptionString))
        return;

    WebExtensionBookmarkChanges parameters;
    parameters.identifier = StringImpl::create(bookmarkIdentifier);

    if (auto *title = objectForKey<NSString>(changes, titleKey))
        parameters.title = StringImpl::create(title);

    if (auto *urlString = objectForKey<NSString>(changes, urlKey); urlString.length) {
        URL url { StringImpl::create(urlString) };
        if (!url.isValid()) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(urlKey).get(), StringImpl::create(@"it must be a valid URL").get())).autorelease();
            return;
        }

        parameters.url = WTF::move(url);
    }

    if (!parameters.title && !parameters.url) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(changesKey).get(), StringImpl::create(@"it must contain a title or a url").get())).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksUpdate(WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarkParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
