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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#include "JSWebExtensionAPIBookmarks.h"
#include "WebExtensionAPIObject.h"

namespace WebKit {

class WebExtensionAPIBookmarks : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIBookmarks, bookmarks, bookmarks);

public:
    enum class BookmarkTreeNodeType : uint8_t {
        Bookmark,
        Folder
    };


#if PLATFORM(COCOA)
    void createBookmark(NSDictionary *bookmark, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void getChildren(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getRecent(long long numberOfItems, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getSubTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getTree(Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void get(NSObject *idOrIdList, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void remove(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void removeTree(NSString *bookmarkIdentifier, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void search(NSObject *query, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void update(NSString *bookmarkIdentifier, NSDictionary *changes, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void move(NSString *bookmarkIdentifier, NSDictionary *destination, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
#endif // PLATFORM(COCOA)
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_WEB_EXTENSION(WebExtensionAPIBookmarks, bookmarks);

#endif
