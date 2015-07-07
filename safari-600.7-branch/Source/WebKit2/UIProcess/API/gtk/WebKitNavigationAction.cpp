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
#include "WebKitNavigationAction.h"

#include "WebKitNavigationActionPrivate.h"
#include <gdk/gdk.h>
#include <wtf/gobject/GRefPtr.h>

using namespace WebKit;

G_DEFINE_BOXED_TYPE(WebKitNavigationAction, webkit_navigation_action, webkit_navigation_action_copy, webkit_navigation_action_free)

WebKitNavigationAction* webkitNavigationActionCreate(WebKitURIRequest* request, const NavigationActionData& navigationActionData)
{
    WebKitNavigationAction* navigation = g_slice_new0(WebKitNavigationAction);
    new (navigation) WebKitNavigationAction(request, navigationActionData);
    return navigation;
}

/**
 * webkit_navigation_action_copy:
 * @navigation: a #WebKitNavigationAction
 *
 * Make a copy of @navigation.
 *
 * Returns: (transfer full): A copy of passed in #WebKitNavigationAction
 *
 * Since: 2.6
 */
WebKitNavigationAction* webkit_navigation_action_copy(WebKitNavigationAction* navigation)
{
    g_return_val_if_fail(navigation, nullptr);

    WebKitNavigationAction* copy = g_slice_new0(WebKitNavigationAction);
    new (copy) WebKitNavigationAction(navigation);
    return copy;
}

/**
 * webkit_navigation_action_free:
 * @navigation: a #WebKitNavigationAction
 *
 * Free the #WebKitNavigationAction
 *
 * Since: 2.6
 */
void webkit_navigation_action_free(WebKitNavigationAction* navigation)
{
    g_return_if_fail(navigation);

    navigation->~WebKitNavigationAction();
    g_slice_free(WebKitNavigationAction, navigation);
}

/**
 * webkit_navigation_action_get_navigation_type:
 * @navigation: a #WebKitNavigationAction
 *
 * Rtuen the type of action that triggered the navigation.
 *
 * Returns: a #WebKitNavigationType
 *
 * Since: 2.6
 */
WebKitNavigationType webkit_navigation_action_get_navigation_type(WebKitNavigationAction* navigation)
{
    g_return_val_if_fail(navigation, WEBKIT_NAVIGATION_TYPE_OTHER);
    return navigation->type;
}

/**
 * webkit_navigation_action_get_mouse_button:
 * @navigation: a #WebKitNavigationAction
 *
 * Return the number of the mouse button that triggered the navigation, or 0 if
 * the navigation was not started by a mouse event.
 *
 * Returns: the mouse button number or 0
 *
 * Since: 2.6
 */
unsigned webkit_navigation_action_get_mouse_button(WebKitNavigationAction* navigation)
{
    g_return_val_if_fail(navigation, 0);
    return navigation->mouseButton;
}

/**
 * webkit_navigation_action_get_modifiers:
 * @navigation: a #WebKitNavigationAction
 *
 * Return a bitmask of #GdkModifierType values describing the modifier keys that were in effect
 * when the navigation was requested
 *
 * Returns: the modifier keys
 *
 * Since: 2.6
 */
unsigned webkit_navigation_action_get_modifiers(WebKitNavigationAction* navigation)
{
    g_return_val_if_fail(navigation, 0);
    return navigation->modifiers;
}

/**
 * webkit_navigation_action_get_request:
 * @navigation: a #WebKitNavigationAction
 *
 * Return the navigation #WebKitURIRequest
 *
 * Returns: (transfer none): a #WebKitURIRequest
 *
 * Since: 2.6
 */
WebKitURIRequest* webkit_navigation_action_get_request(WebKitNavigationAction* navigation)
{
    g_return_val_if_fail(navigation, nullptr);
    return navigation->request.get();
}

/**
 * webkit_navigation_action_is_user_gesture:
 * @navigation: a #WebKitNavigationAction
 *
 * Return whether the navigation was triggered by a user gesture like a mouse click.
 *
 * Returns: whether navigation action is a user gesture
 *
 * Since: 2.6
 */
gboolean webkit_navigation_action_is_user_gesture(WebKitNavigationAction* navigation)
{
    g_return_val_if_fail(navigation, FALSE);
    return navigation->isUserGesture;
}
