/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class StorageNamespaceProvider;
class UserContentController;
}

class WebVisitedLinkStore;

OBJC_CLASS WebView;

class WebViewGroup : public RefCounted<WebViewGroup> {
public:
    static Ref<WebViewGroup> getOrCreate(const String& name, const String& localStorageDatabasePath);
    ~WebViewGroup();

    static WebViewGroup* get(const String& name);

    void addWebView(WebView *);
    void removeWebView(WebView *);

    WebCore::StorageNamespaceProvider& storageNamespaceProvider();
    WebCore::UserContentController& userContentController() { return m_userContentController.get(); }
    WebVisitedLinkStore& visitedLinkStore() { return m_visitedLinkStore.get(); }

private:
    WebViewGroup(const String& name, const String& localStorageDatabasePath);

    String m_name;
    HashSet<WebView *> m_webViews;

    String m_localStorageDatabasePath;
    RefPtr<WebCore::StorageNamespaceProvider> m_storageNamespaceProvider;

    Ref<WebCore::UserContentController> m_userContentController;
    Ref<WebVisitedLinkStore> m_visitedLinkStore;
};
