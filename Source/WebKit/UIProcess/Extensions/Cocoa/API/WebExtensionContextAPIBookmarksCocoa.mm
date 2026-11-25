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
#import "WebExtensionBookmark.h"
#import "WebExtensionBookmarkChanges.h"
#import "WebExtensionBookmarkParameters.h"
#import "WebExtensionController.h"
#import "WebExtensionPermission.h"
#import "_WKWebExtensionBookmarks.h"
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>

namespace WebKit {

using BookmarkTreeResult = std::expected<Ref<WebExtensionBookmark>, WebExtensionError>;
using BookmarkResult = std::expected<WebExtensionBookmarkParameters, WebExtensionError>;
using BookmarksResult = std::expected<Vector<WebExtensionBookmarkParameters>, WebExtensionError>;

bool WebExtensionContext::isBookmarksMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::bookmarks());
}

static Vector<WebExtensionBookmarkParameters> toVector(WebExtensionBookmarkParameters&& parameters)
{
    Vector<WebExtensionBookmarkParameters> result;
    result.append(WTF::move(parameters));
    return result;
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

void WebExtensionContext::bookmarkTree(StringImpl* apiName, CompletionHandler<void(std::expected<Ref<WebExtensionBookmark>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler(toWebExtensionError(apiName, nullString(), StringImpl::create(@"the extension is not loaded").get()));
        return;
    }

    auto *controllerDelegate = extensionController->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    [controllerDelegate _webExtensionController:extensionController->wrapper() bookmarksForExtensionContext:wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, apiName = RefPtr { apiName }, completionHandler = WTF::move(completionHandler)](NSArray<id<_WKWebExtensionBookmark>> *topLevelNodes, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        if (!topLevelNodes) {
            completionHandler(toWebExtensionError(apiName, nullString(), StringImpl::create(@"an unknown error occurred").get()));
            return;
        }

        completionHandler(WebExtensionBookmark::create(protectedThis.get(), topLevelNodes));
    }).get()];
}

// Reads a bookmark back from a fresh tree after a change, so its parent and index are accurate.
static void bookmarkResult(WebExtensionContext& extensionContext, StringImpl* apiName, Ref<WebExtensionBookmark>&& bookmark, CompletionHandler<void(BookmarkResult&&)>&& completionHandler)
{
    extensionContext.bookmarkTree(apiName, [bookmark = WTF::move(bookmark), completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (root) {
            if (RefPtr found = root.value()->find(bookmark->identifier())) {
                completionHandler(found->parameters());
                return;
            }
        }

        completionHandler(bookmark->parameters());
    });
}

void WebExtensionContext::bookmarksCreate(const WebExtensionBookmarkChanges& bookmark, CompletionHandler<void(BookmarkResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.create()";

    bookmarkTree(StringImpl::create(apiName).get(), [protectedThis = Ref { *this }, bookmark, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr<WebExtensionBookmark> parent;
        if (bookmark.parentIdentifier) {
            parent = root.value()->find(*bookmark.parentIdentifier);
            if (!parent) {
                completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"parent bookmark not found").get()));
                return;
            }
        } else {
            parent = root.value()->defaultParent();
            if (!parent) {
                completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"there is no default folder for new bookmarks").get()));
                return;
            }
        }

        parent->createChild(StringImpl::create(apiName).get(), bookmark.url, bookmark.title.value_or(emptyString()), bookmark.index, [protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)](std::expected<Ref<WebExtensionBookmark>, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(std::unexpected(WTF::move(result.error())));
                return;
            }

            bookmarkResult(protectedThis.get(), StringImpl::create(apiName).get(), WTF::move(result.value()), WTF::move(completionHandler));
        });
    });
}

void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getTree()";

    bookmarkTree(StringImpl::create(apiName).get(), [completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        completionHandler(toVector(root.value()->parameters(WebExtensionBookmark::IncludeChildren::Yes)));
    });
}

void WebExtensionContext::bookmarksGetSubTree(StringImpl* bookmarkIdentifier, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getSubTree()";

    bookmarkTree(StringImpl::create(apiName).get(), [bookmarkIdentifier = RefPtr { bookmarkIdentifier }, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr bookmark = root.value()->find(bookmarkIdentifier);
        if (!bookmark) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
            return;
        }

        completionHandler(toVector(bookmark->parameters(WebExtensionBookmark::IncludeChildren::Yes)));
    });
}

void WebExtensionContext::bookmarksGet(const Vector<RefPtr<StringImpl>>& bookmarkIdentifiers, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.get()";

    bookmarkTree(StringImpl::create(apiName).get(), [bookmarkIdentifiers = bookmarkIdentifiers, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        Vector<WebExtensionBookmarkParameters> result;
        result.reserveInitialCapacity(bookmarkIdentifiers.size());

        for (auto& bookmarkIdentifier : bookmarkIdentifiers) {
            RefPtr bookmark = root.value()->find(bookmarkIdentifier);
            if (!bookmark) {
                completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
                return;
            }

            result.append(bookmark->parameters());
        }

        completionHandler(WTF::move(result));
    });
}

void WebExtensionContext::bookmarksGetChildren(StringImpl* bookmarkIdentifier, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getChildren()";

    bookmarkTree(StringImpl::create(apiName).get(), [bookmarkIdentifier = RefPtr { bookmarkIdentifier }, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr bookmark = root.value()->find(bookmarkIdentifier);
        if (!bookmark) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
            return;
        }

        Vector<WebExtensionBookmarkParameters> result;
        if (bookmark->isFolder()) {
            result.reserveInitialCapacity(bookmark->children().size());
            for (auto& child : bookmark->children())
                result.append(child->parameters());
        }

        completionHandler(WTF::move(result));
    });
}

void WebExtensionContext::bookmarksGetRecent(uint64_t numberOfItems, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getRecent()";

    bookmarkTree(StringImpl::create(apiName).get(), [numberOfItems, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        Vector<std::pair<WallTime, Ref<WebExtensionBookmark>>> candidates;
        for (auto& bookmark : root.value()->descendants()) {
            auto url = bookmark->url();
            if (url && !url->isEmpty())
                candidates.append({ bookmark->dateAdded(), bookmark.copyRef() });
        }

        std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });

        Vector<WebExtensionBookmarkParameters> result;
        for (auto& candidate : candidates) {
            if (result.size() >= numberOfItems)
                break;

            result.append(candidate.second->parameters());
        }

        completionHandler(WTF::move(result));
    });
}

void WebExtensionContext::bookmarksSearch(std::optional<RefPtr<StringImpl>> query, std::optional<RefPtr<StringImpl>> url, std::optional<RefPtr<StringImpl>> title, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.search()";

    bookmarkTree(StringImpl::create(apiName).get(), [query = WTF::move(query), url = WTF::move(url), title = WTF::move(title), completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        Vector<RefPtr<StringImpl>> queryTerms;
        if (query && !isEmpty(*query))
            queryTerms = split(*query, ' ');

        std::optional<RefPtr<StringImpl>> normalizedURL;
        if (url)
            normalizedURL = normalizeURLStringForComparison(url->get());

        Vector<WebExtensionBookmarkParameters> result;

        for (auto& bookmark : root.value()->descendants()) {
            if (bookmark->folderType() != WebExtensionBookmark::FolderType::None)
                continue;

            auto bookmarkTitle = bookmark->title();

            std::optional<RefPtr<StringImpl>> bookmarkURL;
            if (auto bookmarkURLValue = bookmark->url())
                bookmarkURL = bookmarkURLValue->string();

            if (query) {
                bool allTermsMatch = true;
                for (const auto& term : queryTerms) {
                    if (!bookmarkContainsIgnoringASCIICase(bookmarkTitle.get(), term.get()) && (!bookmarkURL || !bookmarkContainsIgnoringASCIICase(bookmarkURL->get(), term.get()))) {
                        allTermsMatch = false;
                        break;
                    }
                }

                if (!allTermsMatch)
                    continue;
            }

            if (title && !equal(bookmarkTitle, *title))
                continue;

            if (url && (!bookmarkURL || !equal(normalizeURLStringForComparison(bookmarkURL->get()), *normalizedURL)))
                continue;

            result.append(bookmark->parameters());
        }

        completionHandler(WTF::move(result));
    });
}

void WebExtensionContext::bookmarksUpdate(const WebExtensionBookmarkChanges& bookmark, CompletionHandler<void(BookmarkResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.update()";

    bookmarkTree(StringImpl::create(apiName).get(), [protectedThis = Ref { *this }, bookmark, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr found = root.value()->find(bookmark.identifier);
        if (!found) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
            return;
        }

        // Like tabs.update(), each property has its own setter, and the first failure ends the update.
        auto setURL = [found, url = bookmark.url](CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler) mutable {
            if (!url) {
                completionHandler({ });
                return;
            }

            found->setURL(StringImpl::create(apiName).get(), *url, WTF::move(completionHandler));
        };

        auto finish = [protectedThis = WTF::move(protectedThis), found, completionHandler = WTF::move(completionHandler)](std::expected<void, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(std::unexpected(WTF::move(result.error())));
                return;
            }

            bookmarkResult(protectedThis.get(), StringImpl::create(apiName).get(), found.releaseNonNull(), WTF::move(completionHandler));
        };

        if (!bookmark.title) {
            setURL(WTF::move(finish));
            return;
        }

        found->setTitle(StringImpl::create(apiName).get(), *bookmark.title, [setURL = WTF::move(setURL), finish = WTF::move(finish)](std::expected<void, WebExtensionError>&& result) mutable {
            if (!result) {
                finish(WTF::move(result));
                return;
            }

            setURL(WTF::move(finish));
        });
    });
}

void WebExtensionContext::bookmarksMove(const WebExtensionBookmarkChanges& bookmark, CompletionHandler<void(BookmarkResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.move()";

    bookmarkTree(StringImpl::create(apiName).get(), [protectedThis = Ref { *this }, bookmark, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr found = root.value()->find(bookmark.identifier);
        if (!found) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
            return;
        }

        // Without a new parent the bookmark stays in its folder and only moves within it.
        RefPtr<WebExtensionBookmark> newParent;
        if (bookmark.parentIdentifier) {
            newParent = root.value()->find(*bookmark.parentIdentifier);
            if (!newParent) {
                completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"parent bookmark not found").get()));
                return;
            }
        } else
            newParent = found->parent();

        if (!newParent) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"the bookmark root cannot be modified").get()));
            return;
        }

        found->moveTo(StringImpl::create(apiName).get(), *newParent, bookmark.index, [protectedThis = WTF::move(protectedThis), found, completionHandler = WTF::move(completionHandler)](std::expected<void, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(std::unexpected(WTF::move(result.error())));
                return;
            }

            bookmarkResult(protectedThis.get(), StringImpl::create(apiName).get(), found.releaseNonNull(), WTF::move(completionHandler));
        });
    });
}

// Removes each bookmark in turn and stops at the first failure from the app, naming the bookmark it stopped at.
// The next removal starts from the run loop, so an app that completes synchronously does not nest one call inside the previous one.
static void removeBookmarks(Vector<Ref<WebExtensionBookmark>>&& bookmarks, size_t index, WebExtensionBookmark::Recursively recursively, NSString *apiName, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (index >= bookmarks.size()) {
        completionHandler({ });
        return;
    }

    Ref bookmark = bookmarks[index].copyRef();
    bookmark->remove(StringImpl::create(apiName).get(), recursively, [bookmarks = WTF::move(bookmarks), index, recursively, apiName, completionHandler = WTF::move(completionHandler)](std::expected<void, WebExtensionError>&& result) mutable {
        if (!result) {
            if (index)
                completionHandler(std::unexpected(makeString(result.error(), " (stopped at bookmark '"_s, bookmarks[index]->identifier(), "'; the bookmarks before it in the list were removed)"_s)));
            else
                completionHandler(std::unexpected(WTF::move(result.error())));

            return;
        }

        RunLoop::mainSingleton().dispatch([bookmarks = WTF::move(bookmarks), index, recursively, apiName, completionHandler = WTF::move(completionHandler)] mutable {
            removeBookmarks(WTF::move(bookmarks), index + 1, recursively, apiName, WTF::move(completionHandler));
        });
    });
}

void WebExtensionContext::bookmarksRemove(const Vector<RefPtr<StringImpl>>& bookmarkIdentifiers, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.remove()";

    bookmarkTree(StringImpl::create(apiName).get(), [bookmarkIdentifiers, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        // Check every bookmark against the rules WebKit enforces before removing any of them.
        HashMap<RefPtr<StringImpl>, Ref<WebExtensionBookmark>> bookmarksByIdentifier;
        for (auto& descendant : root.value()->descendants())
            bookmarksByIdentifier.add(descendant->identifier(), descendant.copyRef());

        HashSet<RefPtr<StringImpl>> seenIdentifiers;
        Vector<Ref<WebExtensionBookmark>> bookmarks;
        bookmarks.reserveInitialCapacity(bookmarkIdentifiers.size());
        for (auto& bookmarkIdentifier : bookmarkIdentifiers) {
            if (!seenIdentifiers.add(bookmarkIdentifier).isNewEntry)
                continue;

            RefPtr<WebExtensionBookmark> bookmark;
            if (equal(bookmarkIdentifier, WebExtensionBookmark::rootIdentifier()))
                bookmark = root.value().ptr();
            else if (auto found = bookmarksByIdentifier.find(bookmarkIdentifier); found != bookmarksByIdentifier.end())
                bookmark = found->value.ptr();

            if (!bookmark) {
                completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
                return;
            }

            if (auto error = bookmark->removalError(StringImpl::create(apiName).get(), WebExtensionBookmark::Recursively::No)) {
                completionHandler(std::unexpected(WTF::move(*error)));
                return;
            }

            bookmarks.append(bookmark.releaseNonNull());
        }

        removeBookmarks(WTF::move(bookmarks), 0, WebExtensionBookmark::Recursively::No, apiName, WTF::move(completionHandler));
    });
}

void WebExtensionContext::bookmarksRemoveTree(StringImpl* bookmarkIdentifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.removeTree()";

    bookmarkTree(StringImpl::create(apiName).get(), [bookmarkIdentifier = RefPtr { bookmarkIdentifier }, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr found = root.value()->find(bookmarkIdentifier);
        if (!found) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"bookmark not found").get()));
            return;
        }

        Vector<Ref<WebExtensionBookmark>> bookmarks;
        bookmarks.append(found.releaseNonNull());

        removeBookmarks(WTF::move(bookmarks), 0, WebExtensionBookmark::Recursively::Yes, apiName, WTF::move(completionHandler));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
