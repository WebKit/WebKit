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
#include "PluginDatabase.h"

#include "PluginPackage.h"
#include "PluginView.h"
#include "Frame.h"
#include <windows.h>
#include <shlwapi.h>

namespace WebCore {

PluginDatabase* PluginDatabase::installedPlugins()
{
    static PluginDatabase* plugins = 0;
    
    if (!plugins) {
        plugins = new PluginDatabase;
        plugins->setPluginPaths(PluginDatabase::defaultPluginPaths());
        plugins->refresh();
    }

    return plugins;
}

void PluginDatabase::addExtraPluginPath(const String& path)
{
    m_pluginPaths.append(path);
    refresh();
}

bool PluginDatabase::refresh()
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

Vector<PluginPackage*> PluginDatabase::plugins() const
{
    Vector<PluginPackage*> result;

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

        PluginPackage* package = PluginPackage::createPackage(String(pathStr, pathStrSize / sizeof(WCHAR) - 1), attributes.ftLastWriteTime);

        if (package)
            plugins.add(package);
    }

    RegCloseKey(key);
}

PluginSet PluginDatabase::getPluginsInPaths() const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;
    PluginSet plugins;

    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findFileData;

    PluginPackage* oldWMPPlugin = 0;
    PluginPackage* newWMPPlugin = 0;

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
            if ((!filename.startsWith("np", false) || !filename.endsWith("dll", false)) &&
                (!equalIgnoringCase(filename, "Plugin.dll") || !it->endsWith("Shockwave 10", false)))
                continue;

            String fullPath = *it + "\\" + filename;
            if (!uniqueFilenames.add(fullPath).second)
                continue;
        
            PluginPackage* pluginPackage = PluginPackage::createPackage(fullPath, findFileData.ftLastWriteTime);

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

static inline String safariPluginsPath()
{
    WCHAR moduleFileNameStr[_MAX_PATH];
    static String pluginsPath;
    static bool cachedPluginPath = false;

    if (!cachedPluginPath) {
        cachedPluginPath = true;

        int moduleFileNameLen = GetModuleFileName(0, moduleFileNameStr, _MAX_PATH);

        if (!moduleFileNameLen || moduleFileNameLen == _MAX_PATH)
            goto exit;

        if (!PathRemoveFileSpec(moduleFileNameStr))
            goto exit;

        pluginsPath = String(moduleFileNameStr) + "\\Plugins";
    }
exit:
    return pluginsPath;
}

static inline void addMacromediaPluginPaths(Vector<String>& paths)
{
    WCHAR systemDirectoryStr[MAX_PATH];

    if (GetSystemDirectory(systemDirectoryStr, _countof(systemDirectoryStr)) == 0)
        return;

    WCHAR macromediaDirectoryStr[MAX_PATH];

    PathCombine(macromediaDirectoryStr, systemDirectoryStr, TEXT("macromed\\Flash"));
    paths.append(macromediaDirectoryStr);

    PathCombine(macromediaDirectoryStr, systemDirectoryStr, TEXT("macromed\\Shockwave 10"));
    paths.append(macromediaDirectoryStr);
}

Vector<String> PluginDatabase::defaultPluginPaths()
{
    Vector<String> paths;
    String ourPath = safariPluginsPath();

    if (!ourPath.isNull())
        paths.append(ourPath);
    addQuickTimePluginPath(paths);
    addAdobeAcrobatPluginPath(paths);
    addMozillaPluginPaths(paths);
    addWindowsMediaPlayerPluginPath(paths);
    addMacromediaPluginPaths(paths);

    return paths;
}

bool PluginDatabase::isMIMETypeRegistered(const String& mimeType)
{
    if (mimeType.isNull())
        return false;
    if (m_registeredMIMETypes.contains(mimeType))
        return true;
    // No plugin was found, try refreshing the database and searching again
    return (refresh() && m_registeredMIMETypes.contains(mimeType));
}

PluginPackage* PluginDatabase::pluginForMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return 0;

    String key = mimeType.lower();
    String ourPath = safariPluginsPath();
    PluginPackage* plugin = 0;
    PluginSet::const_iterator end = m_plugins.end();

    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        if ((*it)->mimeToDescriptions().contains(key)) {
            plugin = (*it).get();
            // prefer plugins in our own plugins directory
            if (plugin->parentDirectory() == ourPath)
                break;
        }
    }

    return plugin;
}

String PluginDatabase::MIMETypeForExtension(const String& extension) const
{
    if (extension.isEmpty())
        return String();

    PluginSet::const_iterator end = m_plugins.end();
    String ourPath = safariPluginsPath();
    String mimeType;
    PluginPackage* plugin = 0;

    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        MIMEToExtensionsMap::const_iterator mime_end = (*it)->mimeToExtensions().end();

        for (MIMEToExtensionsMap::const_iterator mime_it = (*it)->mimeToExtensions().begin(); mime_it != mime_end; ++mime_it) {
            const Vector<String>& extensions = mime_it->second;
            for (unsigned i = 0; i < extensions.size(); i++) {
                if (equalIgnoringCase(extensions[i], extension)) {
                    mimeType = mime_it->first;
                    plugin = (*it).get();
                    // prefer plugins in our own plugins directory
                    if (plugin->parentDirectory() == ourPath)
                        break;
                }
            }
        }
    }

    return mimeType;
}

PluginPackage* PluginDatabase::findPlugin(const KURL& url, String& mimeType)
{   
    PluginPackage* plugin = pluginForMIMEType(mimeType);
    String filename = url.string();
    
    if (!plugin) {
        String filename = url.lastPathComponent();
        if (!filename.endsWith("/")) {
            int extensionPos = filename.reverseFind('.');
            if (extensionPos != -1) {
                String extension = filename.substring(extensionPos + 1);

                mimeType = MIMETypeForExtension(extension);
                plugin = pluginForMIMEType(mimeType);
            }
        }
    }

    // FIXME: if no plugin could be found, query Windows for the mime type 
    // corresponding to the extension.

    return plugin;
}

PluginView* PluginDatabase::createPluginView(Frame* parentFrame, const IntSize& size, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    // if we fail to find a plugin for this MIME type, findPlugin will search for
    // a plugin by the file extension and update the MIME type, so pass a mutable String
    String mimeTypeCopy = mimeType;
    PluginPackage* plugin = findPlugin(url, mimeTypeCopy);
    
    // No plugin was found, try refreshing the database and searching again
    if (!plugin && refresh()) {
        mimeTypeCopy = mimeType;
        plugin = findPlugin(url, mimeTypeCopy);
    }
        
    return new PluginView(parentFrame, size, plugin, element, url, paramNames, paramValues, mimeTypeCopy, loadManually);
}

}
