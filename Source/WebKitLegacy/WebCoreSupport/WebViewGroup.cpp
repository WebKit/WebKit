/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "WebViewGroup.h"

#include "WebStorageNamespaceProvider.h"
#include "WebView.h"
#include "WebVisitedLinkStore.h"
#include <WebCore/UserContentController.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringHash.h>

using namespace WebCore;

// Any named groups will live for the lifetime of the process, thanks to the reference held by the RefPtr.
static HashMap<String, RefPtr<WebViewGroup>>& webViewGroups()
{
    static NeverDestroyed<HashMap<String, RefPtr<WebViewGroup>>> webViewGroups;

    return webViewGroups;
}

Ref<WebViewGroup> WebViewGroup::getOrCreate(const String& name, const String& localStorageDatabasePath)
{
    if (name.isEmpty())
        return adoptRef(*new WebViewGroup(String(), localStorageDatabasePath));

    auto& webViewGroup = webViewGroups().add(name, nullptr).iterator->value;
    if (!webViewGroup) {
        auto result = adoptRef(*new WebViewGroup(name, localStorageDatabasePath));
        webViewGroup = result.copyRef();
        return result;
    }

    if (!webViewGroup->m_storageNamespaceProvider && webViewGroup->m_localStorageDatabasePath.isEmpty() && !localStorageDatabasePath.isEmpty())
        webViewGroup->m_localStorageDatabasePath = localStorageDatabasePath;

    return *webViewGroup;
}

WebViewGroup* WebViewGroup::get(const String& name)
{
    ASSERT(!name.isEmpty());

    return webViewGroups().get(name);
}

WebViewGroup::WebViewGroup(const String& name, const String& localStorageDatabasePath)
    : m_name(name)
    , m_localStorageDatabasePath(localStorageDatabasePath)
    , m_userContentController(UserContentController::create())
    , m_visitedLinkStore(WebVisitedLinkStore::create())
{
}

WebViewGroup::~WebViewGroup()
{
    ASSERT(m_name.isEmpty());
    ASSERT(m_webViews.isEmpty());
}

void WebViewGroup::addWebView(WebView *webView)
{
    ASSERT(!m_webViews.contains(webView));

    m_webViews.add(webView);
}

void WebViewGroup::removeWebView(WebView *webView)
{
    ASSERT(m_webViews.contains(webView));

    m_webViews.remove(webView);
}

StorageNamespaceProvider& WebViewGroup::storageNamespaceProvider()
{
    if (!m_storageNamespaceProvider)
        m_storageNamespaceProvider = WebKit::WebStorageNamespaceProvider::create(m_localStorageDatabasePath);

    return *m_storageNamespaceProvider;
}
