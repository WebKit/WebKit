/*
 * Copyright (C) 2014 Collabora Ltd.
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
#include "WebKitNotification.h"

#include "WebKitNotificationPrivate.h"
#include "WebNotification.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

/**
 * SECTION: WebKitNotification
 * @Short_description: Object used to hold information about a notification that should be shown to the user.
 * @Title: WebKitNotification
 *
 * Since: 2.8
 */

enum {
    PROP_0,

    PROP_ID,
    PROP_TITLE,
    PROP_BODY,
    PROP_TAG
};

enum {
    CLOSED,
    CLICKED,

    LAST_SIGNAL
};

struct _WebKitNotificationPrivate {
    CString title;
    CString body;
    CString tag;
    guint64 id;

    WebKitWebView* webView;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitNotification, webkit_notification, G_TYPE_OBJECT)

static void webkitNotificationGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitNotification* notification = WEBKIT_NOTIFICATION(object);

    switch (propId) {
    case PROP_ID:
        g_value_set_uint64(value, webkit_notification_get_id(notification));
        break;
    case PROP_TITLE:
        g_value_set_string(value, webkit_notification_get_title(notification));
        break;
    case PROP_BODY:
        g_value_set_string(value, webkit_notification_get_body(notification));
        break;
    case PROP_TAG:
        g_value_set_string(value, webkit_notification_get_tag(notification));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_notification_class_init(WebKitNotificationClass* notificationClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(notificationClass);
    objectClass->get_property = webkitNotificationGetProperty;

    /**
     * WebKitNotification:id:
     *
     * The unique id for the notification.
     *
     * Since: 2.8
     */
    g_object_class_install_property(objectClass,
        PROP_ID,
        g_param_spec_uint64("id",
            _("ID"),
            _("The unique id for the notification"),
            0, G_MAXUINT64, 0,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitNotification:title:
     *
     * The title for the notification.
     *
     * Since: 2.8
     */
    g_object_class_install_property(objectClass,
        PROP_TITLE,
        g_param_spec_string("title",
            _("Title"),
            _("The title for the notification"),
            nullptr,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitNotification:body:
     *
     * The body for the notification.
     *
     * Since: 2.8
     */
    g_object_class_install_property(objectClass,
        PROP_BODY,
        g_param_spec_string("body",
            _("Body"),
            _("The body for the notification"),
            nullptr,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitNotification:tag:
     *
     * The tag identifier for the notification.
     *
     * Since: 2.16
     */
    g_object_class_install_property(objectClass,
        PROP_TAG,
        g_param_spec_string("tag",
            _("Tag"),
            _("The tag identifier for the notification"),
            nullptr,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitNotification::closed:
     * @notification: the #WebKitNotification on which the signal is emitted
     *
     * Emitted when a notification has been withdrawn.
     *
     * The default handler will close the notification using libnotify, if built with
     * support for it.
     *
     * Since: 2.8
     */
    signals[CLOSED] =
        g_signal_new(
            "closed",
            G_TYPE_FROM_CLASS(notificationClass),
            G_SIGNAL_RUN_LAST,
            0, 0,
            nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitNotification::clicked:
     * @notification: the #WebKitNotification on which the signal is emitted
     *
     * Emitted when a notification has been clicked. See webkit_notification_clicked().
     *
     * Since: 2.12
     */
    signals[CLICKED] =
        g_signal_new(
            "clicked",
            G_TYPE_FROM_CLASS(notificationClass),
            G_SIGNAL_RUN_LAST,
            0, 0,
            nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

WebKitNotification* webkitNotificationCreate(WebKitWebView* webView, const WebKit::WebNotification& webNotification)
{
    WebKitNotification* notification = WEBKIT_NOTIFICATION(g_object_new(WEBKIT_TYPE_NOTIFICATION, nullptr));
    notification->priv->id = webNotification.notificationID();
    notification->priv->title = webNotification.title().utf8();
    notification->priv->body = webNotification.body().utf8();
    notification->priv->tag = webNotification.tag().utf8();
    notification->priv->webView = webView;
    return notification;
}

WebKitWebView* webkitNotificationGetWebView(WebKitNotification* notification)
{
    return notification->priv->webView;
}

/**
 * webkit_notification_get_id:
 * @notification: a #WebKitNotification
 *
 * Obtains the unique id for the notification.
 *
 * Returns: the unique id for the notification
 *
 * Since: 2.8
 */
guint64 webkit_notification_get_id(WebKitNotification* notification)
{
    g_return_val_if_fail(WEBKIT_IS_NOTIFICATION(notification), 0);

    return notification->priv->id;
}

/**
 * webkit_notification_get_title:
 * @notification: a #WebKitNotification
 *
 * Obtains the title for the notification.
 *
 * Returns: the title for the notification
 *
 * Since: 2.8
 */
const gchar* webkit_notification_get_title(WebKitNotification* notification)
{
    g_return_val_if_fail(WEBKIT_IS_NOTIFICATION(notification), nullptr);

    return notification->priv->title.data();
}

/**
 * webkit_notification_get_body:
 * @notification: a #WebKitNotification
 *
 * Obtains the body for the notification.
 *
 * Returns: the body for the notification
 *
 * Since: 2.8
 */
const gchar* webkit_notification_get_body(WebKitNotification* notification)
{
    g_return_val_if_fail(WEBKIT_IS_NOTIFICATION(notification), nullptr);

    return notification->priv->body.data();
}

/**
 * webkit_notification_get_tag:
 * @notification: a #WebKitNotification
 *
 * Obtains the tag identifier for the notification.
 *
 * Returns: (allow-none): the tag for the notification
 *
 * Since: 2.16
 */
const gchar* webkit_notification_get_tag(WebKitNotification* notification)
{
    g_return_val_if_fail(WEBKIT_IS_NOTIFICATION(notification), nullptr);

    const gchar* tag = notification->priv->tag.data();
    return notification->priv->tag.length() ? tag : nullptr;
}

/**
 * webkit_notification_close:
 * @notification: a #WebKitNotification
 *
 * Closes the notification.
 *
 * Since: 2.8
 */
void webkit_notification_close(WebKitNotification* notification)
{
    g_return_if_fail(WEBKIT_IS_NOTIFICATION(notification));

    g_signal_emit(notification, signals[CLOSED], 0);
}

/**
 * webkit_notification_clicked:
 * @notification: a #WebKitNotification
 *
 * Tells WebKit the notification has been clicked. This will emit the
 * #WebKitNotification::clicked signal.
 *
 * Since: 2.12
 */
void webkit_notification_clicked(WebKitNotification* notification)
{
    g_return_if_fail(WEBKIT_IS_NOTIFICATION(notification));

    g_signal_emit(notification, signals[CLICKED], 0);
}
