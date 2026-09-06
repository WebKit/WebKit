/*
 * Copyright (C) 2026 John Cardullo <john@jcarmedia.org>
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
#include "WebKitUserAgent.h"

#include <WebCore/UserAgent.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

/**
 * WebKitUserAgent: (ref-func webkit_user_agent_ref) (unref-func webkit_user_agent_unref)
 *
 * Helper to construct user agent strings.
 *
 * #WebKitUserAgent is a boxed type that allows configuring the user agent type
 * (such as mobile or desktop) and optional application name and version details,
 * and generating a standard user agent string for use in #WebKitSettings or #WebKitWebsitePolicies.
 *
 * Construct it using webkit_user_agent_new() or webkit_user_agent_new_with_details(),
 * then call webkit_user_agent_to_string(), and pass the result to webkit_settings_set_user_agent().
 *
 * Since: 2.56
 */

struct _WebKitUserAgent {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(_WebKitUserAgent);
public:
    WebKitUserAgentType type { WEBKIT_USER_AGENT_TYPE_DEFAULT };
    CString applicationName;
    CString applicationVersion;
    CString userAgentString;
    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitUserAgent, webkit_user_agent, webkit_user_agent_ref, webkit_user_agent_unref)

static WebCore::UserAgentType toWebCoreUserAgentType(WebKitUserAgentType type)
{
    switch (type) {
    case WEBKIT_USER_AGENT_TYPE_DEFAULT:
        return WebCore::UserAgentType::Default;
    case WEBKIT_USER_AGENT_TYPE_DESKTOP:
        return WebCore::UserAgentType::Desktop;
    case WEBKIT_USER_AGENT_TYPE_MOBILE:
        return WebCore::UserAgentType::Mobile;
    }
    return WebCore::UserAgentType::Default;
}

/**
 * webkit_user_agent_new: (constructor)
 * @type: a #WebKitUserAgentType
 *
 * Creates a new #WebKitUserAgent initialized with the given @type.
 *
 * Returns: (transfer full): the newly created #WebKitUserAgent.
 *
 * Since: 2.56
 */
WebKitUserAgent* webkit_user_agent_new(WebKitUserAgentType type)
{
    WebKitUserAgent* userAgent = new WebKitUserAgent();
    userAgent->type = type;
    return userAgent;
}

/**
 * webkit_user_agent_new_with_details: (constructor)
 * @type: a #WebKitUserAgentType
 * @application_name: (allow-none): application name or %NULL
 * @application_version: (allow-none): application version or %NULL
 *
 * Creates a new #WebKitUserAgent with the given @type, @application_name, and @application_version.
 *
 * Returns: (transfer full): the newly created #WebKitUserAgent.
 *
 * Since: 2.56
 */
WebKitUserAgent* webkit_user_agent_new_with_details(WebKitUserAgentType type, const gchar* applicationName, const gchar* applicationVersion)
{
    WebKitUserAgent* userAgent = webkit_user_agent_new(type);
    if (applicationName)
        userAgent->applicationName = applicationName;
    if (applicationVersion)
        userAgent->applicationVersion = applicationVersion;
    return userAgent;
}

/**
 * webkit_user_agent_ref:
 * @user_agent: a #WebKitUserAgent
 *
 * Atomically increments the reference count of @user_agent by one.
 *
 * Returns: The passed in #WebKitUserAgent
 *
 * Since: 2.56
 */
WebKitUserAgent* webkit_user_agent_ref(WebKitUserAgent* userAgent)
{
    g_return_val_if_fail(userAgent, nullptr);
    g_atomic_int_inc(&userAgent->referenceCount);
    return userAgent;
}

/**
 * webkit_user_agent_unref:
 * @user_agent: a #WebKitUserAgent
 *
 * Atomically decrements the reference count of @user_agent by one.
 *
 * If the reference count drops to 0, all memory allocated by the #WebKitUserAgent is
 * released.
 *
 * Since: 2.56
 */
void webkit_user_agent_unref(WebKitUserAgent* userAgent)
{
    g_return_if_fail(userAgent);
    if (g_atomic_int_dec_and_test(&userAgent->referenceCount))
        delete userAgent;
}

/**
 * webkit_user_agent_get_user_agent_type:
 * @user_agent: a #WebKitUserAgent
 *
 * Get the #WebKitUserAgentType of @user_agent.
 *
 * Returns: the #WebKitUserAgentType of @user_agent.
 *
 * Since: 2.56
 */
WebKitUserAgentType webkit_user_agent_get_user_agent_type(WebKitUserAgent* userAgent)
{
    g_return_val_if_fail(userAgent, WEBKIT_USER_AGENT_TYPE_DEFAULT);
    return userAgent->type;
}

/**
 * webkit_user_agent_set_user_agent_type:
 * @user_agent: a #WebKitUserAgent
 * @type: a #WebKitUserAgentType
 *
 * Set the #WebKitUserAgentType of @user_agent.
 *
 * Since: 2.56
 */
void webkit_user_agent_set_user_agent_type(WebKitUserAgent* userAgent, WebKitUserAgentType type)
{
    g_return_if_fail(userAgent);
    if (userAgent->type == type)
        return;
    userAgent->type = type;
    userAgent->userAgentString = { };
}

/**
 * webkit_user_agent_get_application_name:
 * @user_agent: a #WebKitUserAgent
 *
 * Get the application name of @user_agent.
 *
 * Returns: (allow-none): the application name, or %NULL.
 *
 * Since: 2.56
 */
const gchar* webkit_user_agent_get_application_name(WebKitUserAgent* userAgent)
{
    g_return_val_if_fail(userAgent, nullptr);
    return userAgent->applicationName.isNull() ? nullptr : userAgent->applicationName.data();
}

/**
 * webkit_user_agent_set_application_name:
 * @user_agent: a #WebKitUserAgent
 * @application_name: (allow-none): application name or %NULL
 *
 * Set the application name of @user_agent.
 *
 * Since: 2.56
 */
void webkit_user_agent_set_application_name(WebKitUserAgent* userAgent, const gchar* applicationName)
{
    g_return_if_fail(userAgent);
    CString newName = applicationName;
    if (userAgent->applicationName == newName)
        return;
    userAgent->applicationName = WTF::move(newName);
    userAgent->userAgentString = { };
}

/**
 * webkit_user_agent_get_application_version:
 * @user_agent: a #WebKitUserAgent
 *
 * Get the application version of @user_agent.
 *
 * Returns: (allow-none): the application version, or %NULL.
 *
 * Since: 2.56
 */
const gchar* webkit_user_agent_get_application_version(WebKitUserAgent* userAgent)
{
    g_return_val_if_fail(userAgent, nullptr);
    return userAgent->applicationVersion.isNull() ? nullptr : userAgent->applicationVersion.data();
}

/**
 * webkit_user_agent_set_application_version:
 * @user_agent: a #WebKitUserAgent
 * @application_version: (allow-none): application version or %NULL
 *
 * Set the application version of @user_agent.
 *
 * Since: 2.56
 */
void webkit_user_agent_set_application_version(WebKitUserAgent* userAgent, const gchar* applicationVersion)
{
    g_return_if_fail(userAgent);
    CString newVersion = applicationVersion;
    if (userAgent->applicationVersion == newVersion)
        return;
    userAgent->applicationVersion = WTF::move(newVersion);
    userAgent->userAgentString = { };
}

/**
 * webkit_user_agent_to_string:
 * @user_agent: a #WebKitUserAgent
 *
 * Construct and return the user agent string for @user_agent.
 *
 * Returns: (transfer none): the user agent string.
 *
 * Since: 2.56
 */
const gchar* webkit_user_agent_to_string(WebKitUserAgent* userAgent)
{
    g_return_val_if_fail(userAgent, nullptr);
    if (userAgent->userAgentString.isNull()) {
        String appName = userAgent->applicationName.isNull() ? String { } : String::fromUTF8(userAgent->applicationName.span());
        String appVer = userAgent->applicationVersion.isNull() ? String { } : String::fromUTF8(userAgent->applicationVersion.span());
        userAgent->userAgentString = WebCore::standardUserAgent(appName, appVer, toWebCoreUserAgentType(userAgent->type)).utf8();
    }
    return userAgent->userAgentString.data();
}
