/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "WebKitUserMessage.h"

#include "WebKitUserMessagePrivate.h"
#include <gio/gunixfdlist.h>
#include <glib/gi18n-lib.h>
#include <wtf/CompletionHandler.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

enum {
    PROP_0,

    PROP_NAME,
    PROP_PARAMETERS,
    PROP_FD_LIST
};

/**
 * SECTION: WebKitUserMessage
 * @Short_description: A user message
 * @Title: WebKitUserMessage
 * @See_also: #WebKitWebContext, #WebKitWebView, #WebKitWebExtension, #WebKitWebPage
 *
 * A WebKitUserMessage is a message that can be used for the communication between the UI process
 * and web extensions. A WebKitUserMessage always has a name, and it can also include parameters and
 * UNIX file descriptors. Messages can be sent from a #WebKitWebContext to all #WebKitWebExtension<!-- -->s,
 * from a #WebKitWebExtension to its corresponding #WebKitWebContext, and from a #WebKitWebView to its
 * corresponding #WebKitWebPage (and vice versa). One to one messages can be replied to directly with
 * webkit_user_message_send_reply().
 *
 * Since: 2.28
 */
struct _WebKitUserMessagePrivate {
    UserMessage message;
    CompletionHandler<void(UserMessage&&)> replyHandler;
};

WEBKIT_DEFINE_TYPE(WebKitUserMessage, webkit_user_message, G_TYPE_INITIALLY_UNOWNED)

G_DEFINE_QUARK(WebKitUserMessageError, webkit_user_message_error)

static void webkitUserMessageDispose(GObject* object)
{
    WebKitUserMessage* message = WEBKIT_USER_MESSAGE(object);

    if (auto replyHandler = std::exchange(message->priv->replyHandler, nullptr))
        replyHandler(UserMessage(message->priv->message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));

    G_OBJECT_CLASS(webkit_user_message_parent_class)->dispose(object);
}

static void webkitUserMessageGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitUserMessage* message = WEBKIT_USER_MESSAGE(object);

    switch (propId) {
    case PROP_NAME:
        g_value_set_string(value, webkit_user_message_get_name(message));
        break;
    case PROP_PARAMETERS:
        g_value_set_variant(value, webkit_user_message_get_parameters(message));
        break;
    case PROP_FD_LIST:
        g_value_set_object(value, webkit_user_message_get_fd_list(message));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitUserMessageSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitUserMessage* message = WEBKIT_USER_MESSAGE(object);

    switch (propId) {
    case PROP_NAME:
        message->priv->message.name = g_value_get_string(value);
        message->priv->message.type = UserMessage::Type::Message;
        break;
    case PROP_PARAMETERS:
        message->priv->message.parameters = g_value_get_variant(value);
        break;
    case PROP_FD_LIST:
        message->priv->message.fileDescriptors = G_UNIX_FD_LIST(g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_user_message_class_init(WebKitUserMessageClass* klass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);

    gObjectClass->dispose = webkitUserMessageDispose;
    gObjectClass->get_property = webkitUserMessageGetProperty;
    gObjectClass->set_property = webkitUserMessageSetProperty;

    /**
     * WebKitUserMessage:name:
     *
     * The name of the user message.
     *
     * Since: 2.28
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_NAME,
        g_param_spec_string(
            "name",
            _("Name"),
            _("The user message name"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitUserMessage:parameters:
     *
     * The parameters of the user message as a #GVariant, or %NULL
     * if the message doesn't include parameters. Note that only complete types are
     * allowed.
     *
     * Since: 2.28
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_PARAMETERS,
        g_param_spec_variant(
            "parameters",
            _("Parameters"),
            _("The user message parameters"),
            G_VARIANT_TYPE_ANY,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitUserMessage:fd-list:
     *
     * The UNIX file descriptors of the user message.
     *
     * Since: 2.28
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_FD_LIST,
        g_param_spec_object(
            "fd-list",
            _("File Descriptor List"),
            _("The user message list of file descriptors"),
            G_TYPE_UNIX_FD_LIST,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

WebKitUserMessage* webkitUserMessageCreate(UserMessage&& message)
{
    WebKitUserMessage* returnValue = WEBKIT_USER_MESSAGE(g_object_new(WEBKIT_TYPE_USER_MESSAGE, nullptr));
    returnValue->priv->message = WTFMove(message);
    return returnValue;
}

WebKitUserMessage* webkitUserMessageCreate(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& replyHandler)
{
    WebKitUserMessage* returnValue = WEBKIT_USER_MESSAGE(g_object_new(WEBKIT_TYPE_USER_MESSAGE, nullptr));
    returnValue->priv->message = WTFMove(message);
    returnValue->priv->replyHandler = WTFMove(replyHandler);
    return returnValue;
}

UserMessage& webkitUserMessageGetMessage(WebKitUserMessage* message)
{
    return message->priv->message;
}

/**
 * webkit_user_message_new:
 * @name: the message name
 * @parameters: (nullable): the message parameters as a #GVariant, or %NULL
 *
 * Create a new #WebKitUserMessage with @name.
 *
 * Returns: the newly created #WebKitUserMessage object.
 *
 * Since: 2.28
 */
WebKitUserMessage* webkit_user_message_new(const char* name, GVariant* parameters)
{
    return webkit_user_message_new_with_fd_list(name, parameters, nullptr);
}

/**
 * webkit_user_message_new_with_fd_list:
 * @name: the message name
 * @parameters: (nullable): the message parameters as a #GVariant
 * @fd_list: (nullable): the message file descriptors
 *
 * Create a new #WebKitUserMessage including also a list of UNIX file descriptors to be sent.
 *
 * Returns: the newly created #WebKitUserMessage object.
 *
 * Since: 2.28
 */
WebKitUserMessage* webkit_user_message_new_with_fd_list(const char* name, GVariant* parameters, GUnixFDList* fdList)
{
    g_return_val_if_fail(name, nullptr);
    g_return_val_if_fail(!fdList || G_IS_UNIX_FD_LIST(fdList), nullptr);

    return WEBKIT_USER_MESSAGE(g_object_new(WEBKIT_TYPE_USER_MESSAGE, "name", name, "parameters", parameters, "fd-list", fdList, nullptr));
}

/**
 * webkit_user_message_get_name:
 * @message: a #WebKitUserMessage
 *
 * Get the @message name
 *
 * Returns: the message name
 *
 * Since: 2.28
 */
const char* webkit_user_message_get_name(WebKitUserMessage* message)
{
    g_return_val_if_fail(WEBKIT_IS_USER_MESSAGE(message), nullptr);

    return message->priv->message.name.data();
}

/**
 * webkit_user_message_get_parameters:
 * @message: a #WebKitUserMessage
 *
 * Get the @message parameters
 *
 * Returns: (transfer none): the message parameters
 *
 * Since: 2.28
 */
GVariant* webkit_user_message_get_parameters(WebKitUserMessage* message)
{
    g_return_val_if_fail(WEBKIT_IS_USER_MESSAGE(message), nullptr);

    return message->priv->message.parameters.get();
}

/**
 * webkit_user_message_get_fd_list:
 * @message: a #WebKitUserMessage
 *
 * Get the @message list of file descritpor
 *
 * Returns: (transfer none): the message list of file descriptors
 *
 * Since: 2.28
 */
GUnixFDList* webkit_user_message_get_fd_list(WebKitUserMessage* message)
{
    g_return_val_if_fail(WEBKIT_IS_USER_MESSAGE(message), nullptr);

    return message->priv->message.fileDescriptors.get();
}

/**
 * webkit_user_message_send_reply:
 * @message: a #WebKitUserMessage
 * @reply: a #WebKitUserMessage to send as reply
 *
 * Send a reply to @message. If @reply is floating, it's consumed.
 * You can only send a reply to a #WebKitUserMessage that has been
 * received.
 *
 * Since: 2.28
 */
void webkit_user_message_send_reply(WebKitUserMessage* message, WebKitUserMessage* reply)
{
    g_return_if_fail(WEBKIT_IS_USER_MESSAGE(message));
    g_return_if_fail(WEBKIT_IS_USER_MESSAGE(reply));
    g_return_if_fail(message->priv->replyHandler);

    // We sink the reference in case of being floating.
    GRefPtr<WebKitUserMessage> adoptedReply = reply;
    if (auto replyHandler = std::exchange(message->priv->replyHandler, nullptr))
        replyHandler(reply ? reply->priv->message : UserMessage());
}
