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

void WebExtensionContext::bookmarkTree(const String& apiName, CompletionHandler<void(std::expected<Ref<WebExtensionBookmark>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the extension is not loaded"));
        return;
    }

    auto *controllerDelegate = extensionController->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:bookmarksForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    [controllerDelegate _webExtensionController:extensionController->wrapper() bookmarksForExtensionContext:wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, apiName = apiName, completionHandler = WTF::move(completionHandler)](NSArray<id<_WKWebExtensionBookmark>> *topLevelNodes, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!topLevelNodes) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"an unknown error occurred"));
            return;
        }

        completionHandler(WebExtensionBookmark::create(protectedThis.get(), topLevelNodes));
    }).get()];
}

// Reads a bookmark back from a fresh tree after a change, so its parent and index are accurate.
static void bookmarkResult(WebExtensionContext& extensionContext, const String& apiName, Ref<WebExtensionBookmark>&& bookmark, CompletionHandler<void(BookmarkResult&&)>&& completionHandler)
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

    bookmarkTree(apiName, [protectedThis = Ref { *this }, bookmark, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr<WebExtensionBookmark> parent;
        if (bookmark.parentIdentifier) {
            parent = root.value()->find(*bookmark.parentIdentifier);
            if (!parent) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"parent bookmark not found"));
                return;
            }
        } else {
            parent = root.value()->defaultParent();
            if (!parent) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"there is no default folder for new bookmarks"));
                return;
            }
        }

        parent->createChild(apiName, bookmark.url, bookmark.title.value_or(emptyString()), bookmark.index, [protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)](std::expected<Ref<WebExtensionBookmark>, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(std::unexpected(WTF::move(result.error())));
                return;
            }

            bookmarkResult(protectedThis.get(), apiName, WTF::move(result.value()), WTF::move(completionHandler));
        });
    });
}

void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getTree()";

    bookmarkTree(apiName, [completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        completionHandler(toVector(root.value()->parameters(WebExtensionBookmark::IncludeChildren::Yes)));
    });
}

void WebExtensionContext::bookmarksGetSubTree(const String& bookmarkIdentifier, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getSubTree()";

    bookmarkTree(apiName, [bookmarkIdentifier = bookmarkIdentifier, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr bookmark = root.value()->find(bookmarkIdentifier);
        if (!bookmark) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
            return;
        }

        completionHandler(toVector(bookmark->parameters(WebExtensionBookmark::IncludeChildren::Yes)));
    });
}

void WebExtensionContext::bookmarksGet(const Vector<String>& bookmarkIdentifiers, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.get()";

    bookmarkTree(apiName, [bookmarkIdentifiers = bookmarkIdentifiers, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        Vector<WebExtensionBookmarkParameters> result;
        result.reserveInitialCapacity(bookmarkIdentifiers.size());

        for (auto& bookmarkIdentifier : bookmarkIdentifiers) {
            RefPtr bookmark = root.value()->find(bookmarkIdentifier);
            if (!bookmark) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
                return;
            }

            result.append(bookmark->parameters());
        }

        completionHandler(WTF::move(result));
    });
}

void WebExtensionContext::bookmarksGetChildren(const String& bookmarkIdentifier, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.getChildren()";

    bookmarkTree(apiName, [bookmarkIdentifier = bookmarkIdentifier, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr bookmark = root.value()->find(bookmarkIdentifier);
        if (!bookmark) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
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

    bookmarkTree(apiName, [numberOfItems, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
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

void WebExtensionContext::bookmarksSearch(std::optional<String> query, std::optional<String> url, std::optional<String> title, CompletionHandler<void(BookmarksResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.search()";

    bookmarkTree(apiName, [query = WTF::move(query), url = WTF::move(url), title = WTF::move(title), completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        Vector<String> queryTerms;
        if (query && !query->isEmpty())
            queryTerms = query->split(' ');

        std::optional<String> normalizedURL;
        if (url)
            normalizedURL = normalizeURLStringForComparison(*url);

        Vector<WebExtensionBookmarkParameters> result;

        for (auto& bookmark : root.value()->descendants()) {
            if (bookmark->folderType() != WebExtensionBookmark::FolderType::None)
                continue;

            auto bookmarkTitle = bookmark->title();

            std::optional<String> bookmarkURL;
            if (auto bookmarkURLValue = bookmark->url())
                bookmarkURL = bookmarkURLValue->string();

            if (query) {
                bool allTermsMatch = true;
                for (const auto& term : queryTerms) {
                    if (!containsIgnoringASCIICase(bookmarkTitle, term) && (!bookmarkURL || !containsIgnoringASCIICase(*bookmarkURL, term))) {
                        allTermsMatch = false;
                        break;
                    }
                }

                if (!allTermsMatch)
                    continue;
            }

            if (title && bookmarkTitle != *title)
                continue;

            if (url && (!bookmarkURL || normalizeURLStringForComparison(*bookmarkURL) != *normalizedURL))
                continue;

            result.append(bookmark->parameters());
        }

        completionHandler(WTF::move(result));
    });
}

void WebExtensionContext::bookmarksUpdate(const WebExtensionBookmarkChanges& bookmark, CompletionHandler<void(BookmarkResult&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.update()";

    bookmarkTree(apiName, [protectedThis = Ref { *this }, bookmark, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr found = root.value()->find(bookmark.identifier);
        if (!found) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
            return;
        }

        // Like tabs.update(), each property has its own setter, and the first failure ends the update.
        auto setURL = [found, url = bookmark.url](CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler) mutable {
            if (!url) {
                completionHandler({ });
                return;
            }

            found->setURL(apiName, *url, WTF::move(completionHandler));
        };

        auto finish = [protectedThis = WTF::move(protectedThis), found, completionHandler = WTF::move(completionHandler)](std::expected<void, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(std::unexpected(WTF::move(result.error())));
                return;
            }

            bookmarkResult(protectedThis.get(), apiName, found.releaseNonNull(), WTF::move(completionHandler));
        };

        if (!bookmark.title) {
            setURL(WTF::move(finish));
            return;
        }

        found->setTitle(apiName, *bookmark.title, [setURL = WTF::move(setURL), finish = WTF::move(finish)](std::expected<void, WebExtensionError>&& result) mutable {
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

    bookmarkTree(apiName, [protectedThis = Ref { *this }, bookmark, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr found = root.value()->find(bookmark.identifier);
        if (!found) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
            return;
        }

        // Without a new parent the bookmark stays in its folder and only moves within it.
        RefPtr<WebExtensionBookmark> newParent;
        if (bookmark.parentIdentifier) {
            newParent = root.value()->find(*bookmark.parentIdentifier);
            if (!newParent) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"parent bookmark not found"));
                return;
            }
        } else
            newParent = found->parent();

        if (!newParent) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"the bookmark root cannot be modified"));
            return;
        }

        found->moveTo(apiName, *newParent, bookmark.index, [protectedThis = WTF::move(protectedThis), found, completionHandler = WTF::move(completionHandler)](std::expected<void, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(std::unexpected(WTF::move(result.error())));
                return;
            }

            bookmarkResult(protectedThis.get(), apiName, found.releaseNonNull(), WTF::move(completionHandler));
        });
    });
}

// Removes each bookmark in turn and stops at the first failure. The next removal starts from the run loop, so
// an app that completes synchronously does not nest one call inside the previous one.
static void removeBookmarks(Vector<Ref<WebExtensionBookmark>>&& bookmarks, size_t index, WebExtensionBookmark::Recursively recursively, NSString *apiName, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (index >= bookmarks.size()) {
        completionHandler({ });
        return;
    }

    Ref bookmark = bookmarks[index].copyRef();
    bookmark->remove(apiName, recursively, [bookmarks = WTF::move(bookmarks), index, recursively, apiName, completionHandler = WTF::move(completionHandler)](std::expected<void, WebExtensionError>&& result) mutable {
        if (!result) {
            completionHandler(std::unexpected(WTF::move(result.error())));
            return;
        }

        RunLoop::mainSingleton().dispatch([bookmarks = WTF::move(bookmarks), index, recursively, apiName, completionHandler = WTF::move(completionHandler)] mutable {
            removeBookmarks(WTF::move(bookmarks), index + 1, recursively, apiName, WTF::move(completionHandler));
        });
    });
}

void WebExtensionContext::bookmarksRemove(const Vector<String>& bookmarkIdentifiers, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.remove()";

    bookmarkTree(apiName, [bookmarkIdentifiers, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        // Check that every bookmark exists before removing any of them.
        HashMap<String, Ref<WebExtensionBookmark>> bookmarksByIdentifier;
        for (auto& descendant : root.value()->descendants())
            bookmarksByIdentifier.add(descendant->identifier(), descendant.copyRef());

        Vector<Ref<WebExtensionBookmark>> bookmarks;
        bookmarks.reserveInitialCapacity(bookmarkIdentifiers.size());
        for (auto& bookmarkIdentifier : bookmarkIdentifiers) {
            if (bookmarkIdentifier == WebExtensionBookmark::rootIdentifier()) {
                bookmarks.append(root.value().copyRef());
                continue;
            }

            auto found = bookmarksByIdentifier.find(bookmarkIdentifier);
            if (found == bookmarksByIdentifier.end()) {
                completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
                return;
            }

            bookmarks.append(found->value.copyRef());
        }

        removeBookmarks(WTF::move(bookmarks), 0, WebExtensionBookmark::Recursively::No, apiName, WTF::move(completionHandler));
    });
}

void WebExtensionContext::bookmarksRemoveTree(const String& bookmarkIdentifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"bookmarks.removeTree()";

    bookmarkTree(apiName, [bookmarkIdentifier, completionHandler = WTF::move(completionHandler)](BookmarkTreeResult&& root) mutable {
        if (!root) {
            completionHandler(std::unexpected(WTF::move(root.error())));
            return;
        }

        RefPtr found = root.value()->find(bookmarkIdentifier);
        if (!found) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"bookmark not found"));
            return;
        }

        Vector<Ref<WebExtensionBookmark>> bookmarks;
        bookmarks.append(found.releaseNonNull());

        removeBookmarks(WTF::move(bookmarks), 0, WebExtensionBookmark::Recursively::Yes, apiName, WTF::move(completionHandler));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
