/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebGtkExtensionManager.h"

#include "InjectedBundle.h"
#include "WKBundleAPICast.h"
#include "WKDictionary.h"
#include "WKString.h"
#include "WKType.h"
#include "WebKitWebExtensionPrivate.h"
#include <WebCore/FileSystem.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

namespace WebKit {

WebGtkExtensionManager& WebGtkExtensionManager::shared()
{
    DEFINE_STATIC_LOCAL(WebGtkExtensionManager, extensionManager, ());
    return extensionManager;
}

WebGtkExtensionManager::WebGtkExtensionManager()
{
}

void WebGtkExtensionManager::scanModules(const String& webExtensionsDirectory, Vector<String>& modules)
{
    Vector<String> modulePaths = WebCore::listDirectory(webExtensionsDirectory, String("*.so"));
    for (size_t i = 0; i < modulePaths.size(); ++i) {
        if (WebCore::fileExists(modulePaths[i]))
            modules.append(modulePaths[i]);
    }
}

static void parseUserData(WKTypeRef userData, String& webExtensionsDirectory, GRefPtr<GVariant>& initializationUserData)
{
    ASSERT(userData);
    ASSERT(WKGetTypeID(userData) == WKStringGetTypeID());

    CString userDataString = toImpl(static_cast<WKStringRef>(userData))->string().utf8();
    GRefPtr<GVariant> variant = g_variant_parse(nullptr, userDataString.data(),
        userDataString.data() + userDataString.length(), nullptr, nullptr);

    ASSERT(variant);
    ASSERT(g_variant_check_format_string(variant.get(), "(m&smv)", FALSE));

    const char* directory = nullptr;
    GVariant* data = nullptr;
    g_variant_get(variant.get(), "(m&smv)", &directory, &data);

    webExtensionsDirectory = WebCore::filenameToString(directory);
    initializationUserData = adoptGRef(data);
}

bool WebGtkExtensionManager::initializeWebExtension(Module* extensionModule, GVariant* userData)
{
    WebKitWebExtensionInitializeWithUserDataFunction initializeWithUserDataFunction =
        extensionModule->functionPointer<WebKitWebExtensionInitializeWithUserDataFunction>("webkit_web_extension_initialize_with_user_data");
    if (initializeWithUserDataFunction) {
        initializeWithUserDataFunction(m_extension.get(), userData);
        return true;
    }

    WebKitWebExtensionInitializeFunction initializeFunction =
        extensionModule->functionPointer<WebKitWebExtensionInitializeFunction>("webkit_web_extension_initialize");
    if (initializeFunction) {
        initializeFunction(m_extension.get());
        return true;
    }

    return false;
}

void WebGtkExtensionManager::initialize(WKBundleRef bundle, WKTypeRef userDataString)
{
    m_extension = adoptGRef(webkitWebExtensionCreate(toImpl(bundle)));

    String webExtensionsDirectory;
    GRefPtr<GVariant> userData;
    parseUserData(userDataString, webExtensionsDirectory, userData);

    if (webExtensionsDirectory.isNull())
        return;

    Vector<String> modulePaths;
    scanModules(webExtensionsDirectory, modulePaths);

    for (size_t i = 0; i < modulePaths.size(); ++i) {
        OwnPtr<Module> module = adoptPtr(new Module(modulePaths[i]));
        if (!module->load())
            continue;
        if (initializeWebExtension(module.get(), userData.get()))
            m_extensionModules.append(module.leakPtr());
    }
}

} // namespace WebKit
