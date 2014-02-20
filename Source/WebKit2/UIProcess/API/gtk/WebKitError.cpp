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

#include "WebKitPrivate.h"
#include <WebCore/ErrorsGtk.h>

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
    return g_quark_from_static_string(WebCore::errorDomainNetwork);
}

GQuark webkit_policy_error_quark()
{
    return g_quark_from_static_string(WebCore::errorDomainPolicy);
}

GQuark webkit_plugin_error_quark()
{
    return g_quark_from_static_string(WebCore::errorDomainPlugin);
}

GQuark webkit_download_error_quark()
{
    return g_quark_from_static_string(WebCore::errorDomainDownload);
}

GQuark webkit_print_error_quark()
{
    return g_quark_from_static_string(WebCore::errorDomainPrint);
}

GQuark webkit_javascript_error_quark()
{
    return g_quark_from_static_string("WebKitJavascriptError");
}

GQuark webkit_snapshot_error_quark()
{
    return g_quark_from_static_string("WebKitSnapshotError");
}
