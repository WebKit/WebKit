/*
 * Copyright (C) 2025 Igalia S.L.
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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebKitWebExtension.h"
#include <WebKit/WKBase.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

// Private API required by the unit tests

typedef struct _WebKitWebExtension WebKitWebExtension;

WK_EXPORT WebKitWebExtension* webkitWebExtensionCreate(HashMap<String, GRefPtr<GBytes>>&& resources, GError**);
WK_EXPORT gboolean webkit_web_extension_get_has_service_worker_background_content(WebKitWebExtension*);
WK_EXPORT gboolean webkit_web_extension_get_has_modular_background_content(WebKitWebExtension*);

#endif // ENABLE(WK_WEB_EXTENSIONS)
