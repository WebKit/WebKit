/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2008 Luca Bruno <lethalman88@gmail.com>
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
#include "WebKitError.h"

#include "APIError.h"
#include "WebKitPrivate.h"

using namespace WebCore;

/**
 * webkit_network_error_quark:
 *
 * Gets the quark for the domain of networking errors.
 *
 * Returns: network error domain.
 */
GQuark webkit_network_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitNetworkErrorDomain().span8().data()));
}

/**
 * webkit_policy_error_quark:
 *
 * Gets the quark for the domain of policy errors.
 *
 * Returns: policy error domain.
 */
GQuark webkit_policy_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitPolicyErrorDomain().span8().data()));
}

/**
 * webkit_plugin_error_quark:
 *
 * Gets the quark for the domain of plug-in errors.
 *
 * Returns: plug-in error domain.
 */
GQuark webkit_plugin_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitPluginErrorDomain().span8().data()));
}

/**
 * webkit_download_error_quark:
 *
 * Gets the quark for the domain of download errors.
 *
 * Returns: download error domain.
 */
GQuark webkit_download_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitDownloadErrorDomain().span8().data()));
}

#if PLATFORM(GTK)
/**
 * webkit_print_error_quark:
 *
 * Gets the quark for the domain of printing errors.
 *
 * Returns: print error domain.
 */
GQuark webkit_print_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitPrintErrorDomain().span8().data()));
}
#endif

/**
 * webkit_javascript_error_quark:
 *
 * Gets the quark for the domain of JavaScript errors.
 *
 * Returns: JavaScript error domain.
 */
GQuark webkit_javascript_error_quark()
{
    return g_quark_from_static_string("WebKitJavascriptError");
}

/**
 * webkit_snapshot_error_quark:
 *
 * Gets the quark for the domain of page snapshot errors.
 *
 * Returns: snapshot error domain.
 */
GQuark webkit_snapshot_error_quark()
{
    return g_quark_from_static_string("WebKitSnapshotError");
}

/**
 * webkit_user_content_filter_error_quark:
 *
 * Gets the quark for the domain of user content filter errors.
 *
 * Returns: user content filter error domain.
 */
G_DEFINE_QUARK(WebKitUserContentFilterError, webkit_user_content_filter_error)

#if ENABLE(2022_GLIB_API)
/**
 * webkit_media_error_quark:
 *
 * Gets the quark for the domain of media errors.
 *
 * Returns: media error domin.
 *
 * Since: 2.40
 */
G_DEFINE_QUARK(WebKitMediaError, webkit_media_error)
#endif
