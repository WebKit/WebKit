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

    result[idKey] = bookmark.identifier.createNSString().get();
    result[titleKey] = bookmark.title.createNSString().get();

    if (bookmark.index)
        result[indexKey] = @(*bookmark.index);

    if (bookmark.parentIdentifier)
        result[parentIdKey] = bookmark.parentIdentifier->createNSString().get();

    if (bookmark.url && !bookmark.url->isEmpty()) {
        result[urlKey] = bookmark.url->string().createNSString().get();
        result[typeKey] = bookmarkKey;
        result[dateAddedKey] = @(bookmark.dateAdded.secondsSinceEpoch().milliseconds());
    } else
        result[typeKey] = folderKey;

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
            *outExceptionString = toErrorString(nullString(), typeKey, @"it must specify either 'bookmark' or 'folder'").createNSString().autorelease();
            return;
        }
    }

    WebExtensionBookmarkChanges parameters;

    if (auto *parentIdentifier = objectForKey<NSString>(bookmark, parentIdKey))
        parameters.parentIdentifier = String(parentIdentifier);

    if (auto *title = objectForKey<NSString>(bookmark, titleKey))
        parameters.title = String(title);

    if (auto *urlString = objectForKey<NSString>(bookmark, urlKey); urlString.length) {
        URL url { String(urlString) };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nullString(), urlKey, @"it must be a valid URL").createNSString().autorelease();
            return;
        }

        parameters.url = WTF::move(url);
    }

    if (auto *index = objectForKey<NSNumber>(bookmark, indexKey)) {
        if (index.longLongValue < 0) {
            *outExceptionString = toErrorString(nullString(), indexKey, @"it must not be negative").createNSString().autorelease();
            return;
        }

        parameters.index = index.unsignedLongLongValue;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksCreate(WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarkParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getChildren(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getChildren

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetChildren(bookmarkIdentifier), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getRecent(long long numberOfItems, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getRecent

    if (numberOfItems < 1) {
        *outExceptionString = toErrorString(nullString(), numberOfItemsKey, @"it must be at least 1").createNSString().autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetRecent(static_cast<uint64_t>(numberOfItems)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::getSubTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/getSubTree

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGetSubTree(bookmarkIdentifier), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
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
            callback->reportError(result.error().createNSString().get());
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

    Vector<String> bookmarkIdentifiers;

    if (auto *bookmarkIdentifier = dynamic_objc_cast<NSString>(idOrIdList))
        bookmarkIdentifiers.append(bookmarkIdentifier);
    else if (auto *identifierList = dynamic_objc_cast<NSArray>(idOrIdList)) {
        for (NSString *bookmarkIdentifier in identifierList) {
            if (!bookmarkIdentifier.length) {
                *outExceptionString = toErrorString(nullString(), idOrIdListKey, @"each item in the list must be a non-empty string").createNSString().autorelease();
                return;
            }

            bookmarkIdentifiers.append(bookmarkIdentifier);
        }
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksGet(bookmarkIdentifiers), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
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
    parameters.identifier = bookmarkIdentifier;

    if (auto *parentIdentifier = objectForKey<NSString>(destination, parentIdKey))
        parameters.parentIdentifier = String(parentIdentifier);

    if (auto *index = objectForKey<NSNumber>(destination, indexKey)) {
        if (index.longLongValue < 0) {
            *outExceptionString = toErrorString(nullString(), indexKey, @"it must not be negative").createNSString().autorelease();
            return;
        }

        parameters.index = index.unsignedLongLongValue;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksMove(WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarkParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::remove(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/remove

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksRemove(bookmarkIdentifier), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::removeTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/removeTree

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksRemoveTree(bookmarkIdentifier), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
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

    std::optional<String> queryTerms;
    std::optional<String> title;
    std::optional<String> url;

    if (auto *queryString = dynamic_objc_cast<NSString>(query))
        queryTerms = String(queryString);
    else if (auto *queryDictionary = dynamic_objc_cast<NSDictionary>(query)) {
        static NSDictionary<NSString *, id> *types = @{
            urlKey: NSString.class,
            titleKey: NSString.class,
            queryKey: NSString.class
        };

        if (!validateDictionary(queryDictionary, queryKey, nil, types, outExceptionString))
            return;

        if (auto *queryString = objectForKey<NSString>(queryDictionary, queryKey))
            queryTerms = String(queryString);

        if (auto *titleString = objectForKey<NSString>(queryDictionary, titleKey))
            title = String(titleString);

        if (auto *urlString = objectForKey<NSString>(queryDictionary, urlKey))
            url = String(urlString);
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksSearch(queryTerms, url, title), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIBookmarks::update(NSString *bookmarkIdentifier, NSDictionary *changes, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks/update

    if (!bookmarkIdentifier.length) {
        *outExceptionString = toErrorString(nullString(), idKey, @"it must be a non-empty string").createNSString().autorelease();
        return;
    }

    static NSDictionary<NSString *, id> *types = @{
        urlKey: NSString.class,
        titleKey: NSString.class
    };

    if (!validateDictionary(changes, changesKey, nil, types, outExceptionString))
        return;

    WebExtensionBookmarkChanges parameters;
    parameters.identifier = bookmarkIdentifier;

    if (auto *title = objectForKey<NSString>(changes, titleKey))
        parameters.title = String(title);

    if (auto *urlString = objectForKey<NSString>(changes, urlKey); urlString.length) {
        URL url { String(urlString) };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nullString(), urlKey, @"it must be a valid URL").createNSString().autorelease();
            return;
        }

        parameters.url = WTF::move(url);
    }

    if (!parameters.title && !parameters.url) {
        *outExceptionString = toErrorString(nullString(), changesKey, @"it must contain a title or a url").createNSString().autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::BookmarksUpdate(WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<WebExtensionBookmarkParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
