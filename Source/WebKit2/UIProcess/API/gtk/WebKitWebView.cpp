/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebKitWebView.h"

#include "NotImplemented.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <WebKit2/WKBase.h>
#include <WebKit2/WKURL.h>

using namespace WebKit;
using namespace WebCore;

G_DEFINE_TYPE(WebKitWebView, webkit_web_view, WEBKIT_TYPE_WEB_VIEW_BASE)

static void webkit_web_view_init(WebKitWebView* webView)
{
    webkitWebViewBaseCreateWebPage(WEBKIT_WEB_VIEW_BASE(webView), WKContextGetSharedProcessContext(), 0);
}

static void webkit_web_view_class_init(WebKitWebViewClass* webViewClass)
{
}

// Public API.
GtkWidget* webkit_web_view_new()
{
    return GTK_WIDGET(WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, NULL)));
}

void webkit_web_view_load_uri(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageLoadURL(toAPI(page), WKURLCreateWithUTF8CString(uri));
}

void webkit_web_view_go_back(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageGoBack(toAPI(page));
}

void webkit_web_view_go_forward(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageGoForward(toAPI(page));
}
