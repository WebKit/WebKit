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
#include "WebKitWebExtensionPrivate.h"
#include <WebCore/FileSystem.h>
#include <wtf/OwnPtr.h>

namespace WebKit {

WebGtkExtensionManager& WebGtkExtensionManager::shared()
{
    DEFINE_STATIC_LOCAL(WebGtkExtensionManager, extensionManager, ());
    return extensionManager;
}

WebGtkExtensionManager::WebGtkExtensionManager()
{
}

void WebGtkExtensionManager::appendModuleDirectories(Vector<String>& directories)
{
    String extensionPaths(getenv("WEBKIT_WEB_EXTENSIONS_PATH"));
    if (!extensionPaths.isEmpty()) {
        Vector<String> paths;
        extensionPaths.split(UChar(':'), /* allowEmptyEntries */ false, paths);
        directories.append(paths);
    }

    static const char* extensionDefaultDirectory = LIBDIR""G_DIR_SEPARATOR_S"webkitgtk-"WEBKITGTK_API_VERSION_STRING""G_DIR_SEPARATOR_S"web-extensions"G_DIR_SEPARATOR_S;
    directories.append(WebCore::filenameToString(extensionDefaultDirectory));
}

void WebGtkExtensionManager::scanModules(Vector<String>& modules)
{
    Vector<String> moduleDirectories;
    appendModuleDirectories(moduleDirectories);
    for (size_t i = 0; i < moduleDirectories.size(); ++i) {
        Vector<String> modulePaths = WebCore::listDirectory(moduleDirectories[i], String("*.so"));
        for (size_t j = 0; j < modulePaths.size(); ++j) {
            if (WebCore::fileExists(modulePaths[j]))
                modules.append(modulePaths[j]);
        }
    }
}

void WebGtkExtensionManager::initialize(WKBundleRef bundle)
{
    m_extension = adoptGRef(webkitWebExtensionCreate(toImpl(bundle)));

    Vector<String> modulePaths;
    scanModules(modulePaths);
    for (size_t i = 0; i < modulePaths.size(); ++i) {
        OwnPtr<Module> module = adoptPtr(new Module(modulePaths[i]));
        if (!module->load())
            continue;

        WebKitWebExtensionInitializeFunction initializeFunction =
            module->functionPointer<WebKitWebExtensionInitializeFunction>("webkit_web_extension_initialize");
        if (!initializeFunction)
            continue;

        // FIXME: check we don't load the same module twice from different paths.
        m_extensionModules.append(module.leakPtr());
        initializeFunction(m_extension.get());
    }
}

} // namespace WebKit
