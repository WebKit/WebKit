/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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
#include "WebExtensionControllerConfiguration.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include <wtf/glib/Application.h>

namespace WebKit {

String WebExtensionControllerConfiguration::createStorageDirectoryPath(std::optional<WTF::UUID> identifier)
{
    String dataPath = WebsiteDataStore::defaultBaseDataDirectory();
    RELEASE_ASSERT(!dataPath.isEmpty());

    String identifierPath = identifier ? identifier->toString().convertToASCIIUppercase() : "Default"_s;

    String appDirectoryName = String::fromUTF8(WTF::applicationID().legacyCStringPointer());
    return FileSystem::pathByAppendingComponents(dataPath, std::initializer_list<StringView>({ "WebExtensions"_s, identifierPath }));
}

String WebExtensionControllerConfiguration::createTemporaryStorageDirectoryPath()
{
    return FileSystem::createTemporaryDirectory("WebExtensions"_s);
}

WebKitSettings* WebExtensionControllerConfiguration::webViewConfiguration()
{
    if (!m_webViewConfiguration)
        m_webViewConfiguration = adoptGRef(webkit_settings_new());
    return m_webViewConfiguration.get();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
