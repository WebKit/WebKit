/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "WebExtensionContext.h"

#include "WebKitWebExtensionPrivate.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#if ENABLE(2022_GLIB_API)

namespace WebKit {

WebExtensionContext::WebExtensionContext(WebKitWebExtensionContext* contextObject)
    : WebExtensionContext()
{
    m_extension = webkitWebExtensionToImpl(webkit_web_extension_context_get_web_extension(contextObject)).get();
    m_baseURL = URL { makeString("webkit-extension://"_s, uniqueIdentifier(), '/') };
    m_delegate.reset(contextObject);
}

} // namespace WebKit

#endif // ENABLE(2022_GLIB_API)

#endif // ENABLE(WK_WEB_EXTENSIONS)
