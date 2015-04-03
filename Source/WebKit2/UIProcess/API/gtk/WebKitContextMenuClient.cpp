/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "config.h"
#include "WebKitContextMenuClient.h"

#include "APIContextMenuClient.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"

using namespace WebKit;

class ContextMenuClient final: public API::ContextMenuClient {
public:
    explicit ContextMenuClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    bool getContextMenuFromProposedMenu(WebPageProxy&, const Vector<WebContextMenuItemData>& proposedMenu, Vector<WebContextMenuItemData>&, const WebHitTestResult::Data& hitTestResultData, API::Object* userData)
    {
        GRefPtr<GVariant> variant;
        if (userData) {
            ASSERT(userData->type() == API::Object::Type::String);
            CString userDataString = static_cast<API::String*>(userData)->string().utf8();
            variant = adoptGRef(g_variant_parse(nullptr, userDataString.data(), userDataString.data() + userDataString.length(), nullptr, nullptr));
        }
        webkitWebViewPopulateContextMenu(m_webView, proposedMenu, hitTestResultData, variant.get());
        return true;
    }

    WebKitWebView* m_webView;
};

void attachContextMenuClientToView(WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    page->setContextMenuClient(std::make_unique<ContextMenuClient>(webView));
}

