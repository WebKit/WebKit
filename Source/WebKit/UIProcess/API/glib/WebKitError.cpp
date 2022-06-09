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
 * SECTION: WebKitError
 * @Short_description: Categorized WebKit errors
 * @Title: WebKitError
 *
 * Categorized WebKit errors.
 *
 */

GQuark webkit_network_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitNetworkErrorDomain().characters8()));
}

GQuark webkit_policy_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitPolicyErrorDomain().characters8()));
}

GQuark webkit_plugin_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitPluginErrorDomain().characters8()));
}

GQuark webkit_download_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitDownloadErrorDomain().characters8()));
}

#if PLATFORM(GTK)
GQuark webkit_print_error_quark()
{
    return g_quark_from_static_string(reinterpret_cast<const char*>(API::Error::webKitPrintErrorDomain().characters8()));
}
#endif

GQuark webkit_javascript_error_quark()
{
    return g_quark_from_static_string("WebKitJavascriptError");
}

GQuark webkit_snapshot_error_quark()
{
    return g_quark_from_static_string("WebKitSnapshotError");
}

G_DEFINE_QUARK(WebKitUserContentFilterError, webkit_user_content_filter_error)
