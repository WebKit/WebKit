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
#include "WebExtensionDynamicScripts.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

namespace WebExtensionDynamicScripts {

void WebExtensionRegisteredScript::addUserScript(const String& identifier, API::UserScript& userScript)
{
    auto& userScripts = m_userScriptsMap.ensure(identifier, [&] {
        return UserScriptVector { };
    }).iterator->value;
    userScripts.append(userScript);
}

void WebExtensionRegisteredScript::addUserStyleSheet(const String& identifier, API::UserStyleSheet& userStyleSheet)
{
    auto& userStyleSheets = m_userStyleSheetsMap.ensure(identifier, [&] {
        return UserStyleSheetVector { };
    }).iterator->value;
    userStyleSheets.append(userStyleSheet);
}

} // namespace WebExtensionDynamicScripts

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
