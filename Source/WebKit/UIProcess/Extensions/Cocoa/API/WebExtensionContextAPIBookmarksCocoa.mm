// Copyright © 2025  All rights reserved.
#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#include <wtf/Vector.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import "WKWebExtensionControllerDelegatePrivate.h"

namespace WebKit {

bool WebExtensionContext::isBookmarksMessageAllowed(IPC::Decoder& message)
{
    // FIXME: fix
    return false;
}

void WebExtensionContext::bookmarksCreate(const std::optional<String>& parentId, const std::optional<uint64_t>& index, const std::optional<String>& url, const std::optional<String>& title, CompletionHandler<void(Expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksGetTree(CompletionHandler<void(Expected<WebExtensionBookmarksParameters, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksGetSubTree(const String& bookmarkId, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksGet(const Vector<String>& bookmarkId, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{

}
void WebExtensionContext::bookmarksGetChildren(const String& bookmarkId, CompletionHandler<void(Expected<Vector<WebExtensionBookmarksParameters>, WebExtensionError>&&)>&& completionHandler)
{

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
