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
#include "WebKitExtensionManager.h"

#include "APIString.h"
#include "InjectedBundle.h"
#include "WebKitWebExtensionPrivate.h"
#include <memory>
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>

namespace WebKit {

WebKitExtensionManager& WebKitExtensionManager::singleton()
{
    static NeverDestroyed<WebKitExtensionManager> extensionManager;
    return extensionManager;
}

WebKitExtensionManager::WebKitExtensionManager()
{
}

void WebKitExtensionManager::scanModules(const String& webExtensionsDirectory, Vector<String>& modules)
{
    Vector<String> modulePaths = FileSystem::listDirectory(webExtensionsDirectory, String("*.so"));
    for (size_t i = 0; i < modulePaths.size(); ++i) {
        if (FileSystem::fileExists(modulePaths[i]))
            modules.append(modulePaths[i]);
    }
}

static void parseUserData(API::Object* userData, String& webExtensionsDirectory, GRefPtr<GVariant>& initializationUserData)
{
    ASSERT(userData->type() == API::Object::Type::String);

    CString userDataString = static_cast<API::String*>(userData)->string().utf8();
    GRefPtr<GVariant> variant = g_variant_parse(nullptr, userDataString.data(),
        userDataString.data() + userDataString.length(), nullptr, nullptr);

    ASSERT(variant);
    ASSERT(g_variant_check_format_string(variant.get(), "(m&smv)", FALSE));

    const char* directory = nullptr;
    GVariant* data = nullptr;
    g_variant_get(variant.get(), "(m&smv)", &directory, &data);

    webExtensionsDirectory = FileSystem::stringFromFileSystemRepresentation(directory);
    initializationUserData = adoptGRef(data);
}

bool WebKitExtensionManager::initializeWebExtension(Module* extensionModule, GVariant* userData)
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

void WebKitExtensionManager::initialize(InjectedBundle* bundle, API::Object* userDataObject)
{
    ASSERT(bundle);
    ASSERT(userDataObject);
    m_extension = adoptGRef(webkitWebExtensionCreate(bundle));

    String webExtensionsDirectory;
    GRefPtr<GVariant> userData;
    parseUserData(userDataObject, webExtensionsDirectory, userData);

    if (webExtensionsDirectory.isNull())
        return;

    Vector<String> modulePaths;
    scanModules(webExtensionsDirectory, modulePaths);

    for (size_t i = 0; i < modulePaths.size(); ++i) {
        auto module = std::make_unique<Module>(modulePaths[i]);
        if (!module->load())
            continue;
        if (initializeWebExtension(module.get(), userData.get()))
            m_extensionModules.append(module.release());
    }
}

} // namespace WebKit
