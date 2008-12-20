/*
 * Copyright (C) 2008 Collabora Ltd.
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

#include <wtf/Assertions.h>
#include "FrameLoaderTypes.h"

#include "webkitwebnavigationaction.h"
#include "webkitprivate.h"
#include "webkitenumtypes.h"

#include <string.h>

extern "C" {

struct _WebKitWebNavigationActionPrivate {
    WebKitWebNavigationReason reason;
    gchar* originalUri;
};

#define WEBKIT_WEB_NAVIGATION_ACTION_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_NAVIGATION_ACTION, WebKitWebNavigationActionPrivate))

enum  {
    PROP_0,

    PROP_REASON,
    PROP_ORIGINAL_URI
};

G_DEFINE_TYPE(WebKitWebNavigationAction, webkit_web_navigation_action, G_TYPE_OBJECT)


static void webkit_web_navigation_action_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitWebNavigationAction* navigationAction = WEBKIT_WEB_NAVIGATION_ACTION(object);

    switch(propertyId) {
    case PROP_REASON:
        g_value_set_enum(value, webkit_web_navigation_action_get_reason(navigationAction));
        break;
    case PROP_ORIGINAL_URI:
        g_value_set_string(value, webkit_web_navigation_action_get_original_uri(navigationAction));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_web_navigation_action_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitWebNavigationAction* navigationAction = WEBKIT_WEB_NAVIGATION_ACTION(object);

    switch(propertyId) {
    case PROP_REASON:
        webkit_web_navigation_action_set_reason(navigationAction, (WebKitWebNavigationReason)g_value_get_enum(value));
        break;
    case PROP_ORIGINAL_URI:
        webkit_web_navigation_action_set_original_uri(navigationAction, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_web_navigation_action_init(WebKitWebNavigationAction* navigationAction)
{
    navigationAction->priv = WEBKIT_WEB_NAVIGATION_ACTION_GET_PRIVATE(navigationAction);

    WebKitWebNavigationActionPrivate* priv = navigationAction->priv;
}

static void webkit_web_navigation_action_finalize(GObject* obj)
{
    WebKitWebNavigationAction* navigationAction = WEBKIT_WEB_NAVIGATION_ACTION(obj);
    WebKitWebNavigationActionPrivate* priv = navigationAction->priv;

    g_free(priv->originalUri);

    G_OBJECT_CLASS(webkit_web_navigation_action_parent_class)->finalize(obj);
}

static void webkit_web_navigation_action_class_init(WebKitWebNavigationActionClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);

    COMPILE_ASSERT(WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED == WebCore::NavigationTypeLinkClicked, navigation_type_link_clicked_enum_match);
    COMPILE_ASSERT(WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED == WebCore::NavigationTypeFormSubmitted, navigation_type_form_submitted_enum_match);
    COMPILE_ASSERT(WEBKIT_WEB_NAVIGATION_REASON_BACK_FORWARD == WebCore::NavigationTypeBackForward, navigation_type_back_forward_enum_match);
    COMPILE_ASSERT(WEBKIT_WEB_NAVIGATION_REASON_RELOAD == WebCore::NavigationTypeReload, navigation_type_reload_enum_match);
    COMPILE_ASSERT(WEBKIT_WEB_NAVIGATION_REASON_FORM_RESUBMITTED == WebCore::NavigationTypeFormResubmitted, navigation_type_form_resubmitted_enum_match);
    COMPILE_ASSERT(WEBKIT_WEB_NAVIGATION_REASON_OTHER == WebCore::NavigationTypeOther, navigation_type_other_enum_match);

    objectClass->get_property = webkit_web_navigation_action_get_property;
    objectClass->set_property = webkit_web_navigation_action_set_property;
    objectClass->dispose = webkit_web_navigation_action_finalize;

    /**
     * WebKitWebNavigationAction:reason:
     *
     * The reason why this navigation is occuring.
     *
     * Since: 1.0.3
     */
    g_object_class_install_property(objectClass, PROP_REASON,
                                    g_param_spec_enum("reason",
                                                      "Reason",
                                                      "The reason why this navigation is occurring",
                                                      WEBKIT_TYPE_WEB_NAVIGATION_REASON,
                                                      WEBKIT_WEB_NAVIGATION_REASON_OTHER,
                                                      (GParamFlags)(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT)));

    /**
     * WebKitWebNavigationAction:original-uri:
     *
     * The URI that was requested as the target for the navigation.
     *
     * Since: 1.0.3
     */
    g_object_class_install_property(objectClass, PROP_ORIGINAL_URI,
                                    g_param_spec_string("original-uri",
                                                        "Original URI",
                                                        "The URI that was requested as the target for the navigation",
                                                        "",
                                                        (GParamFlags)(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT)));

    g_type_class_add_private(requestClass, sizeof(WebKitWebNavigationActionPrivate));
}

/**
 * webkit_web_navigation_action_get_reason:
 * @navigationAction: a #WebKitWebNavigationAction
 *
 * Returns the reason why WebKit is requesting a navigation.
 *
 * Return value: a #WebKitWebNavigationReason
 *
 * Since: 1.0.3
 */
WebKitWebNavigationReason webkit_web_navigation_action_get_reason(WebKitWebNavigationAction* navigationAction)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_NAVIGATION_ACTION(navigationAction), WEBKIT_WEB_NAVIGATION_REASON_OTHER);

    return navigationAction->priv->reason;
}

/**
 * webkit_web_navigation_action_set_reason:
 * @navigationAction: a #WebKitWebNavigationAction
 * @reason: a #WebKitWebNavigationReason
 *
 * Sets the reason why WebKit is requesting a navigation.
 *
 * Since: 1.0.3
 */
void webkit_web_navigation_action_set_reason(WebKitWebNavigationAction* navigationAction, WebKitWebNavigationReason reason)
{
    g_return_if_fail(WEBKIT_IS_WEB_NAVIGATION_ACTION(navigationAction));

    if (navigationAction->priv->reason == reason)
        return;

    navigationAction->priv->reason = reason;
    g_object_notify(G_OBJECT(navigationAction), "reason");
}

/**
 * webkit_web_navigation_action_get_original_uri:
 * @navigationAction: a #WebKitWebNavigationAction
 *
 * Returns the URI that was originally requested. This may differ from the
 * navigation target, for instance because of a redirect.
 *
 * Return value: the originally requested URI
 *
 * Since: 1.0.3
 */
const gchar* webkit_web_navigation_action_get_original_uri(WebKitWebNavigationAction* navigationAction)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_NAVIGATION_ACTION(navigationAction), NULL);

    return navigationAction->priv->originalUri;
}

/**
 * webkit_web_navigation_action_set_original_uri:
 * @navigationAction: a #WebKitWebNavigationAction
 * @originalUri: a URI
 *
 * Sets the URI that was originally requested. This may differ from the
 * navigation target, for instance because of a redirect.
 *
 * Since: 1.0.3
 */
void webkit_web_navigation_action_set_original_uri(WebKitWebNavigationAction* navigationAction, const gchar* originalUri)
{
    g_return_if_fail(WEBKIT_IS_WEB_NAVIGATION_ACTION(navigationAction));
    g_return_if_fail(originalUri);

    if (navigationAction->priv->originalUri &&
        (!strcmp(navigationAction->priv->originalUri, originalUri)))
        return;

    g_free(navigationAction->priv->originalUri);
    navigationAction->priv->originalUri = g_strdup(originalUri);
    g_object_notify(G_OBJECT(navigationAction), "original-uri");
}

}
