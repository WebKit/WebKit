/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginDatabaseWin.h"

#include "PluginPackageWin.h"
#include "PluginViewWin.h"
#include "Frame.h"
#include <windows.h>
#include <shlwapi.h>

namespace WebCore {

PluginDatabaseWin* PluginDatabaseWin::installedPlugins()
{
    static PluginDatabaseWin* plugins = 0;
    
    if (!plugins) {
        plugins = new PluginDatabaseWin;
        plugins->setPluginPaths(PluginDatabaseWin::defaultPluginPaths());
        plugins->refresh();
    }

    return plugins;
}

void PluginDatabaseWin::addExtraPluginPath(const String& path)
{
    m_pluginPaths.append(path);
    refresh();
}

bool PluginDatabaseWin::refresh()
{   
    PluginSet newPlugins;

    bool pluginSetChanged = false;

    // Create a new set of plugins
    newPlugins = getPluginsInPaths();

    if (!m_plugins.isEmpty()) {
        m_registeredMIMETypes.clear();

        PluginSet pluginsToUnload = m_plugins;

        PluginSet::const_iterator end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            pluginsToUnload.remove(*it);

        end = m_plugins.end();
        for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
            newPlugins.remove(*it);

        // Unload plugins
        end = pluginsToUnload.end();
        for (PluginSet::const_iterator it = pluginsToUnload.begin(); it != end; ++it)
            m_plugins.remove(*it);

        // Add new plugins
        end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            m_plugins.add(*it);

        pluginSetChanged = !pluginsToUnload.isEmpty() || !newPlugins.isEmpty();
    } else {
        m_plugins = newPlugins;
        PluginSet::const_iterator end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            m_plugins.add(*it);

        pluginSetChanged = !newPlugins.isEmpty();
    }

    // Register plug-in MIME types
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        // Get MIME types
        MIMEToDescriptionsMap::const_iterator map_end = (*it)->mimeToDescriptions().end();
        for (MIMEToDescriptionsMap::const_iterator map_it = (*it)->mimeToDescriptions().begin(); map_it != map_end; ++map_it) {
            m_registeredMIMETypes.add(map_it->first);
        }
    }

    return pluginSetChanged;
}

Vector<PluginPackageWin*> PluginDatabaseWin::plugins() const
{
    Vector<PluginPackageWin*> result;

    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
        result.append((*it).get());

    return result;
}

static inline void addPluginsFromRegistry(HKEY rootKey, PluginSet& plugins)
{
    HKEY key;
    HRESULT result = RegOpenKeyExW(rootKey, L"Software\\MozillaPlugins", 0, KEY_ENUMERATE_SUB_KEYS, &key);

    if (result != ERROR_SUCCESS)
        return;

    wchar_t name[128];
    FILETIME lastModified;

    // Enumerate subkeys
    for (int i = 0;; i++) {
        DWORD nameLen = _countof(name);
        result = RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

        if (result != ERROR_SUCCESS)
            break;

        WCHAR pathStr[_MAX_PATH];
        DWORD pathStrSize = sizeof(pathStr);
        DWORD type;

        result = SHGetValue(key, name, TEXT("Path"), &type, (LPBYTE)pathStr, &pathStrSize);
        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;

        WIN32_FILE_ATTRIBUTE_DATA attributes;
        if (GetFileAttributesEx(pathStr, GetFileExInfoStandard, &attributes) == 0)
            continue;

        PluginPackageWin* package = PluginPackageWin::createPackage(String(pathStr, pathStrSize / sizeof(WCHAR) - 1), attributes.ftLastWriteTime);

        if (package)
            plugins.add(package);
    }

    RegCloseKey(key);
}

PluginSet PluginDatabaseWin::getPluginsInPaths() const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;
    PluginSet plugins;

    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findFileData;

    PluginPackageWin* oldWMPPlugin = 0;
    PluginPackageWin* newWMPPlugin = 0;

    Vector<String>::const_iterator end = m_pluginPaths.end();
    for (Vector<String>::const_iterator it = m_pluginPaths.begin(); it != end; ++it) {
        String pattern = *it + "\\*";

        hFind = FindFirstFileW(pattern.charactersWithNullTermination(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE)
            continue;

        do {
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            String filename = String(findFileData.cFileName, wcslen(findFileData.cFileName));
            if (!filename.startsWith("np", false) || !filename.endsWith(".dll", false))
                continue;

            String fullPath = *it + "\\" + filename;
            if (!uniqueFilenames.add(fullPath).second)
                continue;
        
            PluginPackageWin* pluginPackage = PluginPackageWin::createPackage(fullPath, findFileData.ftLastWriteTime);

            if (pluginPackage) {
                plugins.add(pluginPackage);

                if (equalIgnoringCase(filename, "npdsplay.dll"))
                    oldWMPPlugin = pluginPackage;
                else if (equalIgnoringCase(filename, "np-mswmp.dll"))
                    newWMPPlugin = pluginPackage;
            }

        } while (FindNextFileW(hFind, &findFileData) != 0);

        FindClose(hFind);
    }

    addPluginsFromRegistry(HKEY_LOCAL_MACHINE, plugins);
    addPluginsFromRegistry(HKEY_CURRENT_USER, plugins);

    // If both the old and new WMP plugin are present in the plugins set, 
    // we remove the old one so we don't end up choosing the old one.
    if (oldWMPPlugin && newWMPPlugin)
        plugins.remove(oldWMPPlugin);

    return plugins;
}

static inline Vector<int> parseVersionString(const String& versionString)
{
    Vector<int> version;

    unsigned startPos = 0;
    unsigned endPos;
    
    while (startPos < versionString.length()) {
        for (endPos = startPos; endPos < versionString.length(); ++endPos)
            if (versionString[endPos] == '.' || versionString[endPos] == '_')
                break;

        int versionComponent = versionString.substring(startPos, endPos - startPos).toInt();
        version.append(versionComponent);

        startPos = endPos + 1;
    }

    return version;
}

// This returns whether versionA is higher than versionB
static inline bool compareVersions(const Vector<int>& versionA, const Vector<int>& versionB)
{
    for (unsigned i = 0; i < versionA.size(); i++) {
        if (i >= versionB.size())
            return true;

        if (versionA[i] > versionB[i])
            return true;
        else if (versionA[i] < versionB[i])
            return false;
    }

    // If we come here, the versions are either the same or versionB has an extra component, just return false
    return false;
}

static inline void addMozillaPluginPaths(Vector<String>& paths)
{
    // Enumerate all Mozilla plugin directories in the registry
    HKEY key;
    LONG result;
    
    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Mozilla"), 0, KEY_READ, &key);
    if (result == ERROR_SUCCESS) {
        WCHAR name[128];
        FILETIME lastModified;

        // Enumerate subkeys
        for (int i = 0;; i++) {
            DWORD nameLen = sizeof(name) / sizeof(WCHAR);
            result = RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

            if (result != ERROR_SUCCESS)
                break;

            String extensionsPath = String(name, nameLen) + "\\Extensions";
            HKEY extensionsKey;

            // Try opening the key
            result = RegOpenKeyEx(key, extensionsPath.charactersWithNullTermination(), 0, KEY_READ, &extensionsKey);

            if (result == ERROR_SUCCESS) {
                // Now get the plugins path
                WCHAR pluginsPathStr[_MAX_PATH];
                DWORD pluginsPathSize = sizeof(pluginsPathStr);
                DWORD type;

                result = RegQueryValueEx(extensionsKey, TEXT("Plugins"), 0, &type, (LPBYTE)&pluginsPathStr, &pluginsPathSize);

                if (result == ERROR_SUCCESS && type == REG_SZ)
                    paths.append(String(pluginsPathStr, pluginsPathSize / sizeof(WCHAR) - 1));

                RegCloseKey(extensionsKey);
            }
        }
        
        RegCloseKey(key);
    }
}

static inline void addWindowsMediaPlayerPluginPath(Vector<String>& paths)
{
    // The new WMP Firefox plugin is installed in \PFiles\Plugins if it can't find any Firefox installs
    WCHAR pluginDirectoryStr[_MAX_PATH + 1];
    DWORD pluginDirectorySize = ::ExpandEnvironmentStringsW(TEXT("%SYSTEMDRIVE%\\PFiles\\Plugins"), pluginDirectoryStr, _countof(pluginDirectoryStr));

    if (pluginDirectorySize > 0 && pluginDirectorySize <= _countof(pluginDirectoryStr))
        paths.append(String(pluginDirectoryStr, pluginDirectorySize - 1));

    DWORD type;
    WCHAR installationDirectoryStr[_MAX_PATH];
    DWORD installationDirectorySize = sizeof(installationDirectoryStr);

    HRESULT result = SHGetValue(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\MediaPlayer"), TEXT("Installation Directory"), &type, (LPBYTE)&installationDirectoryStr, &installationDirectorySize);

    if (result == ERROR_SUCCESS && type == REG_SZ)
        paths.append(String(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1));
}

static inline void addQuickTimePluginPath(Vector<String>& paths)
{
    DWORD type;
    WCHAR installationDirectoryStr[_MAX_PATH];
    DWORD installationDirectorySize = sizeof(installationDirectoryStr);

    HRESULT result = SHGetValue(HKEY_LOCAL_MACHINE, TEXT("Software\\Apple Computer, Inc.\\QuickTime"), TEXT("InstallDir"), &type, (LPBYTE)&installationDirectoryStr, &installationDirectorySize);

    if (result == ERROR_SUCCESS && type == REG_SZ) {
        String pluginDir = String(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1) + "\\plugins";
        paths.append(pluginDir);
    }
}

static inline void addAdobeAcrobatPluginPath(Vector<String>& paths)
{
    HKEY key;
    HRESULT result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Adobe\\Acrobat Reader"), 0, KEY_READ, &key);
    if (result != ERROR_SUCCESS)
        return;

    WCHAR name[128];
    FILETIME lastModified;

    Vector<int> latestAcrobatVersion;
    String latestAcrobatVersionString;

    // Enumerate subkeys
    for (int i = 0;; i++) {
        DWORD nameLen = sizeof(name) / sizeof(WCHAR);
        result = RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

        if (result != ERROR_SUCCESS)
            break;

        Vector<int> acrobatVersion = parseVersionString(String(name, nameLen));
        if (compareVersions(acrobatVersion, latestAcrobatVersion)) {
            latestAcrobatVersion = acrobatVersion;
            latestAcrobatVersionString = String(name, nameLen);
        }
    }

    if (!latestAcrobatVersionString.isNull()) {
        DWORD type;
        WCHAR acrobatInstallPathStr[_MAX_PATH];
        DWORD acrobatInstallPathSize = sizeof(acrobatInstallPathStr);

        String acrobatPluginKeyPath = "Software\\Adobe\\Acrobat Reader\\" + latestAcrobatVersionString + "\\InstallPath";
        result = SHGetValue(HKEY_LOCAL_MACHINE, acrobatPluginKeyPath.charactersWithNullTermination(), 0, &type, (LPBYTE)acrobatInstallPathStr, &acrobatInstallPathSize);

        if (result == ERROR_SUCCESS) {
            String acrobatPluginPath = String(acrobatInstallPathStr, acrobatInstallPathSize / sizeof(WCHAR) - 1) + "\\browser";
            paths.append(acrobatPluginPath);
        }
    }

    RegCloseKey(key);
}

static inline void addPluginPath(Vector<String>& paths)
{
    WCHAR moduleFileNameStr[_MAX_PATH];

    int moduleFileNameLen = GetModuleFileName(0, moduleFileNameStr, _MAX_PATH);

    if (!moduleFileNameLen)
        return;

    String moduleFileName = String(moduleFileNameStr, moduleFileNameLen);
    int i = moduleFileName.reverseFind('\\');
    if (i == -1)
        return;

    String pluginsPath = moduleFileName.left(i);
    pluginsPath.append("\\Plugins");

    paths.append(pluginsPath);
}

static inline void addFlashPluginPath(Vector<String>& paths)
{
    WCHAR systemDirectoryStr[MAX_PATH];

    if (GetSystemDirectory(systemDirectoryStr, _countof(systemDirectoryStr)) == 0)
        return;

    WCHAR flashDirectoryStr[MAX_PATH];

    PathCombine(flashDirectoryStr, systemDirectoryStr, TEXT("macromed\\Flash"));

    paths.append(flashDirectoryStr);
}

Vector<String> PluginDatabaseWin::defaultPluginPaths()
{
    Vector<String> paths;

    addPluginPath(paths);
    addQuickTimePluginPath(paths);
    addAdobeAcrobatPluginPath(paths);
    addMozillaPluginPaths(paths);
    addWindowsMediaPlayerPluginPath(paths);
    addFlashPluginPath(paths);

    return paths;
}

bool PluginDatabaseWin::isMIMETypeRegistered(const String& mimeType) const
{
    return !mimeType.isNull() && m_registeredMIMETypes.contains(mimeType);
}

PluginPackageWin* PluginDatabaseWin::pluginForMIMEType(const String& mimeType)
{
    String key = mimeType.lower();

    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        if ((*it)->mimeToDescriptions().contains(key))
            return (*it).get();
    }

    return 0;
}

PluginPackageWin* PluginDatabaseWin::pluginForExtension(const String& extension)
{
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        MIMEToExtensionsMap::const_iterator mime_end = (*it)->mimeToExtensions().end();

        for (MIMEToExtensionsMap::const_iterator mime_it = (*it)->mimeToExtensions().begin(); mime_it != mime_end; ++mime_it) {
            const Vector<String>& extensions = mime_it->second;

            for (unsigned i = 0; i < extensions.size(); i++) {
                if (extensions[i] == extension)
                    return (*it).get();
            }
        }
    }

    return 0;
}

PluginPackageWin* PluginDatabaseWin::findPlugin(const KURL& url, const String& mimeType)
{   
    PluginPackageWin* plugin = 0;

    if (!mimeType.isNull())
        plugin = pluginForMIMEType(mimeType);
    
    if (!plugin) {
        String path = url.path();
        String extension = path.substring(path.reverseFind('.') + 1);

        plugin = pluginForExtension(extension);

        // FIXME: if no plugin could be found, query Windows for the mime type 
        // corresponding to the extension.
    }

    return plugin;
}

PluginViewWin* PluginDatabaseWin::createPluginView(Frame* parentFrame, const IntSize& size, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    PluginPackageWin* plugin = findPlugin(url, mimeType);
    
    // No plugin was found, try refreshing the database and searching again
    if (!plugin && refresh())
        plugin = findPlugin(url, mimeType);
        
    return new PluginViewWin(parentFrame, size, plugin, element, url, paramNames, paramValues, mimeType, loadManually);
}

}
