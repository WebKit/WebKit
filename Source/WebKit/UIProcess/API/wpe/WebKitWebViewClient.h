/*
 * Copyright (C) 2020 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "APIViewClient.h"
#include <wtf/CompletionHandler.h>

typedef struct _WebKitWebView WebKitWebView;

namespace WKWPE {
class View;
}

namespace WebCore {
class IntRect;
}

namespace WebKit {
class DownloadProxy;
class WebKitPopupMenu;
class WebKitWebResourceLoadManager;
struct WebPopupItem;
struct UserMessage;
}

class WebKitWebViewClient final : public API::ViewClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebKitWebViewClient(WebKitWebView*);

    GRefPtr<WebKitOptionMenu> showOptionMenu(WebKit::WebKitPopupMenu&, const WebCore::IntRect&, const Vector<WebKit::WebPopupItem>&, int32_t selectedIndex);

private:
    bool isGLibBasedAPI() override { return true; }

    void frameDisplayed(WKWPE::View&) override;
    void handleDownloadRequest(WKWPE::View&, WebKit::DownloadProxy&) override;
    void willStartLoad(WKWPE::View&) override;
    void didChangePageID(WKWPE::View&) override;
    void didReceiveUserMessage(WKWPE::View&, WebKit::UserMessage&&, CompletionHandler<void(WebKit::UserMessage&&)>&&) override;
    WebKit::WebKitWebResourceLoadManager* webResourceLoadManager() override;

    WebKitWebView* m_webView;
};
