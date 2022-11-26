/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "WebKitConsoleMessage.h"

#include "WebKitConsoleMessagePrivate.h"

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
G_DEFINE_BOXED_TYPE(WebKitConsoleMessage, webkit_console_message, webkit_console_message_copy, webkit_console_message_free)
ALLOW_DEPRECATED_DECLARATIONS_END

/**
 * webkit_console_message_copy:
 * @console_message: a #WebKitConsoleMessage
 *
 * Make a copy of @console_message.
 *
 * Returns: (transfer full): A copy of passed in #WebKitConsoleMessage
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
WebKitConsoleMessage* webkit_console_message_copy(WebKitConsoleMessage* consoleMessage)
{
    g_return_val_if_fail(consoleMessage, nullptr);
    WebKitConsoleMessage* copy = static_cast<WebKitConsoleMessage*>(fastZeroedMalloc(sizeof(WebKitConsoleMessage)));
    new (copy) WebKitConsoleMessage(consoleMessage);
    return copy;
}

/**
 * webkit_console_message_free:
 * @console_message: a #WebKitConsoleMessage
 *
 * Free the #WebKitConsoleMessage
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
void webkit_console_message_free(WebKitConsoleMessage* consoleMessage)
{
    g_return_if_fail(consoleMessage);
    consoleMessage->~WebKitConsoleMessage();
    fastFree(consoleMessage);
}

/**
 * webkit_console_message_get_source:
 * @console_message: a #WebKitConsoleMessage
 *
 * Gets the source of a #WebKitConsoleMessage
 *
 * Returns: a #WebKitConsoleMessageSource indicating the source of @console_message
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
WebKitConsoleMessageSource webkit_console_message_get_source(WebKitConsoleMessage* consoleMessage)
{
    g_return_val_if_fail(consoleMessage, WEBKIT_CONSOLE_MESSAGE_SOURCE_OTHER);
    switch (consoleMessage->source) {
    case JSC::MessageSource::JS:
        return WEBKIT_CONSOLE_MESSAGE_SOURCE_JAVASCRIPT;
    case JSC::MessageSource::Network:
        return WEBKIT_CONSOLE_MESSAGE_SOURCE_NETWORK;
    case JSC::MessageSource::ConsoleAPI:
        return WEBKIT_CONSOLE_MESSAGE_SOURCE_CONSOLE_API;
    case JSC::MessageSource::Security:
        return WEBKIT_CONSOLE_MESSAGE_SOURCE_SECURITY;
    case JSC::MessageSource::Other:
    default:
        break;
    }

    return WEBKIT_CONSOLE_MESSAGE_SOURCE_OTHER;
}

/**
 * webkit_console_message_get_level:
 * @console_message: a #WebKitConsoleMessage
 *
 * Gets the log level of a #WebKitConsoleMessage
 *
 * Returns: a #WebKitConsoleMessageLevel indicating the log level of @console_message
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
WebKitConsoleMessageLevel webkit_console_message_get_level(WebKitConsoleMessage* consoleMessage)
{
    g_return_val_if_fail(consoleMessage, WEBKIT_CONSOLE_MESSAGE_LEVEL_LOG);
    switch (consoleMessage->level) {
    case JSC::MessageLevel::Log:
        return WEBKIT_CONSOLE_MESSAGE_LEVEL_LOG;
    case JSC::MessageLevel::Warning:
        return WEBKIT_CONSOLE_MESSAGE_LEVEL_WARNING;
    case JSC::MessageLevel::Error:
        return WEBKIT_CONSOLE_MESSAGE_LEVEL_ERROR;
    case JSC::MessageLevel::Debug:
        return WEBKIT_CONSOLE_MESSAGE_LEVEL_DEBUG;
    case JSC::MessageLevel::Info:
        return WEBKIT_CONSOLE_MESSAGE_LEVEL_INFO;
    }

    ASSERT_NOT_REACHED();
    return WEBKIT_CONSOLE_MESSAGE_LEVEL_LOG;
}

/**
 * webkit_console_message_get_text:
 * @console_message: a #WebKitConsoleMessage
 *
 * Gets the text message of a #WebKitConsoleMessage
 *
 * Returns: the text message of @console_message
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
const gchar* webkit_console_message_get_text(WebKitConsoleMessage* consoleMessage)
{
    g_return_val_if_fail(consoleMessage, nullptr);
    return consoleMessage->message.data();
}

/**
 * webkit_console_message_get_line:
 * @console_message: a #WebKitConsoleMessage
 *
 * Gets the line number of a #WebKitConsoleMessage
 *
 * Returns: the line number of @console_message
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
guint webkit_console_message_get_line(WebKitConsoleMessage* consoleMessage)
{
    g_return_val_if_fail(consoleMessage, 0);
    return consoleMessage->lineNumber;
}

/**
 * webkit_console_message_get_source_id:
 * @console_message: a #WebKitConsoleMessage
 *
 * Gets the source identifier of a #WebKitConsoleMessage
 *
 * Returns: the source identifier of @console_message
 *
 * Since: 2.12
 *
 * Deprecated: 2.40
 */
const gchar* webkit_console_message_get_source_id(WebKitConsoleMessage* consoleMessage)
{
    g_return_val_if_fail(consoleMessage, nullptr);
    return consoleMessage->sourceID.data();
}
