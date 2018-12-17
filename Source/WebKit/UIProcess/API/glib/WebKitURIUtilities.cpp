/*
 * Copyright (C) 2018 Igalia S.L.
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
#include "WebKitURIUtilities.h"

#include <wtf/URLHelpers.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

/**
 * SECTION: WebKitURIUtilities
 * @Short_description: Utility functions to manipulate URIs
 * @Title: WebKitURIUtilities
 */

/**
 * webkit_uri_for_display:
 * @uri: the URI to be converted
 *
 * Use this function to format a URI for display. The URIs used internally by
 * WebKit may contain percent-encoded characters or Punycode, which are not
 * generally suitable to display to users. This function provides protection
 * against IDN homograph attacks, so in some cases the host part of the returned
 * URI may be in Punycode if the safety check fails.
 *
 * Returns: (nullable) (transfer full): @uri suitable for display, or %NULL in
 *    case of error.
 *
 * Since: 2.24
 */
gchar* webkit_uri_for_display(const gchar* uri)
{
    g_return_val_if_fail(uri, nullptr);

    String result = WTF::URLHelpers::userVisibleURL(uri);
    if (!result)
        return nullptr;

    return g_strdup(result.utf8().data());
}
