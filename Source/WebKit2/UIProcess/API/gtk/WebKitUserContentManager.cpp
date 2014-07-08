/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WebKitUserContentManager.h"

#include "WebKitPrivate.h"
#include "WebKitUserContentManagerPrivate.h"
#include "WebKitUserContentPrivate.h"
#include <wtf/gobject/GRefPtr.h>

using namespace WebCore;
using namespace WebKit;

struct _WebKitUserContentManagerPrivate {
    _WebKitUserContentManagerPrivate()
        : userContentController(WebUserContentControllerProxy::create())
    {
    }

    RefPtr<WebUserContentControllerProxy> userContentController;
};

/**
 * SECTION:WebKitUserContentManager
 * @short_description: Manages user-defined content which affects web pages.
 * @title: WebKitUserContentManager
 *
 * Using a #WebKitUserContentManager user CSS style sheets can be set to
 * be injected in the web pages loaded by a #WebKitWebView, by
 * webkit_user_content_manager_add_style_sheet().
 *
 * To use a #WebKitUserContentManager, it must be created using
 * webkit_user_content_manager_new(), and then passed to
 * webkit_web_view_new_with_user_content_manager(). User style
 * sheets can be created with webkit_user_style_sheet_new().
 *
 * User style sheets can be added and removed at any time, but
 * they will affect the web pages loaded afterwards.
 *
 * Since: 2.6
 */

WEBKIT_DEFINE_TYPE(WebKitUserContentManager, webkit_user_content_manager, G_TYPE_OBJECT)

static void webkit_user_content_manager_class_init(WebKitUserContentManagerClass*)
{
}

/**
 * webkit_user_content_manager_new:
 *
 * Creates a new user content manager.
 *
 * Returns: A #WebKitUserContentManager
 *
 * Since: 2.6
 */
WebKitUserContentManager* webkit_user_content_manager_new()
{
    return WEBKIT_USER_CONTENT_MANAGER(g_object_new(WEBKIT_TYPE_USER_CONTENT_MANAGER, nullptr));
}

/**
 * webkit_user_content_manager_add_style_sheet:
 * @manager: A #WebKitUserContentManager
 * @stylesheet: A #WebKitUserStyleSheet
 *
 * Adds a #WebKitUserStyleSheet to the given #WebKitUserContentManager.
 * The same #WebKitUserStyleSheet can be reused with multiple
 * #WebKitUserContentManager instances.
 *
 * Since: 2.6
 */
void webkit_user_content_manager_add_style_sheet(WebKitUserContentManager* manager, WebKitUserStyleSheet* styleSheet)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(styleSheet);
    manager->priv->userContentController->addUserStyleSheet(webkitUserStyleSheetGetUserStyleSheet(styleSheet));
}

/**
 * webkit_user_content_manager_remove_all_style_sheets:
 * @manager: A #WebKitUserContentManager
 *
 * Removes all user style sheets from the given #WebKitUserContentManager.
 *
 * Since: 2.6
 */
void webkit_user_content_manager_remove_all_style_sheets(WebKitUserContentManager* manager)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    manager->priv->userContentController->removeAllUserStyleSheets();
}

WebUserContentControllerProxy* webkitUserContentManagerGetUserContentControllerProxy(WebKitUserContentManager* manager)
{
    return manager->priv->userContentController.get();
}
