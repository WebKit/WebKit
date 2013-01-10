/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitWebPage.h"

#include "WebKitDOMDocumentPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitWebPagePrivate.h"
#include <WebCore/Frame.h>

using namespace WebKit;
using namespace WebCore;

struct _WebKitWebPagePrivate {
    WebPage* webPage;
};

WEBKIT_DEFINE_TYPE(WebKitWebPage, webkit_web_page, G_TYPE_OBJECT)

static void webkit_web_page_class_init(WebKitWebPageClass* klass)
{
}

WebKitWebPage* webkitWebPageCreate(WebPage* webPage)
{
    WebKitWebPage* page = WEBKIT_WEB_PAGE(g_object_new(WEBKIT_TYPE_WEB_PAGE, NULL));
    page->priv->webPage = webPage;
    return page;
}

/**
 * webkit_web_page_get_dom_document:
 * @web_page: a #WebKitWebPage
 *
 * Get the #WebKitDOMDocument currently loaded in @web_page
 *
 * Returns: the #WebKitDOMDocument currently loaded, or %NULL
 *    if no document is currently loaded.
 */
WebKitDOMDocument* webkit_web_page_get_dom_document(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), 0);

    Frame* coreFrame = webPage->priv->webPage->mainFrame();
    if (!coreFrame)
        return 0;

    return kit(coreFrame->document());
}
