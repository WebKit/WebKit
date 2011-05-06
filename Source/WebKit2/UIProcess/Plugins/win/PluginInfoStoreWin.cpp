/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PluginInfoStore.h"

#include "NetscapePluginModule.h"
#include <WebCore/FileSystem.h>
#include <WebCore/PathWalker.h>
#include <shlwapi.h>

using namespace WebCore;

namespace WebKit {

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

static inline String safariPluginsDirectory()
{
    static String pluginsDirectory;
    static bool cachedPluginDirectory = false;

    if (!cachedPluginDirectory) {
        cachedPluginDirectory = true;

        WCHAR moduleFileNameStr[MAX_PATH];
        int moduleFileNameLen = ::GetModuleFileNameW(0, moduleFileNameStr, WTF_ARRAY_LENGTH(moduleFileNameStr));

        if (!moduleFileNameLen || moduleFileNameLen == WTF_ARRAY_LENGTH(moduleFileNameStr))
            return pluginsDirectory;

        if (!::PathRemoveFileSpecW(moduleFileNameStr))
            return pluginsDirectory;

        pluginsDirectory = String(moduleFileNameStr) + "\\Plugins";
    }

    return pluginsDirectory;
}

static inline void addMozillaPluginDirectories(Vector<String>& directories)
{
    // Enumerate all Mozilla plugin directories in the registry
    HKEY key;
    LONG result = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Mozilla", 0, KEY_READ, &key);
    if (result != ERROR_SUCCESS)
        return;

    WCHAR name[128];
    FILETIME lastModified;

    // Enumerate subkeys
    for (int i = 0;; i++) {
        DWORD nameLen = WTF_ARRAY_LENGTH(name);
        result = ::RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

        if (result != ERROR_SUCCESS)
            break;

        String extensionsPath = String(name, nameLen) + "\\Extensions";
        HKEY extensionsKey;

        // Try opening the key
        result = ::RegOpenKeyExW(key, extensionsPath.charactersWithNullTermination(), 0, KEY_READ, &extensionsKey);

        if (result == ERROR_SUCCESS) {
            // Now get the plugins directory
            WCHAR pluginsDirectoryStr[MAX_PATH];
            DWORD pluginsDirectorySize = sizeof(pluginsDirectoryStr);
            DWORD type;

            result = ::RegQueryValueExW(extensionsKey, L"Plugins", 0, &type, reinterpret_cast<LPBYTE>(&pluginsDirectoryStr), &pluginsDirectorySize);

            if (result == ERROR_SUCCESS && type == REG_SZ)
                directories.append(String(pluginsDirectoryStr, pluginsDirectorySize / sizeof(WCHAR) - 1));

            ::RegCloseKey(extensionsKey);
        }
    }

    ::RegCloseKey(key);
}

static inline void addWindowsMediaPlayerPluginDirectory(Vector<String>& directories)
{
    // The new WMP Firefox plugin is installed in \PFiles\Plugins if it can't find any Firefox installs
    WCHAR pluginDirectoryStr[MAX_PATH + 1];
    DWORD pluginDirectorySize = ::ExpandEnvironmentStringsW(L"%SYSTEMDRIVE%\\PFiles\\Plugins", pluginDirectoryStr, WTF_ARRAY_LENGTH(pluginDirectoryStr));

    if (pluginDirectorySize > 0 && pluginDirectorySize <= WTF_ARRAY_LENGTH(pluginDirectoryStr))
        directories.append(String(pluginDirectoryStr, pluginDirectorySize - 1));

    DWORD type;
    WCHAR installationDirectoryStr[MAX_PATH];
    DWORD installationDirectorySize = sizeof(installationDirectoryStr);

    HRESULT result = ::SHGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\MediaPlayer", L"Installation Directory", &type, reinterpret_cast<LPBYTE>(&installationDirectoryStr), &installationDirectorySize);

    if (result == ERROR_SUCCESS && type == REG_SZ)
        directories.append(String(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1));
}

static inline void addQuickTimePluginDirectory(Vector<String>& directories)
{
    DWORD type;
    WCHAR installationDirectoryStr[MAX_PATH];
    DWORD installationDirectorySize = sizeof(installationDirectoryStr);

    HRESULT result = ::SHGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Apple Computer, Inc.\\QuickTime", L"InstallDir", &type, reinterpret_cast<LPBYTE>(&installationDirectoryStr), &installationDirectorySize);

    if (result == ERROR_SUCCESS && type == REG_SZ) {
        String pluginDir = String(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1) + "\\plugins";
        directories.append(pluginDir);
    }
}

static inline void addAdobeAcrobatPluginDirectory(Vector<String>& directories)
{
    HKEY key;
    HRESULT result = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Adobe\\Acrobat Reader", 0, KEY_READ, &key);
    if (result != ERROR_SUCCESS)
        return;

    WCHAR name[128];
    FILETIME lastModified;

    Vector<int> latestAcrobatVersion;
    String latestAcrobatVersionString;

    // Enumerate subkeys
    for (int i = 0;; i++) {
        DWORD nameLen = WTF_ARRAY_LENGTH(name);
        result = ::RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

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
        WCHAR acrobatInstallPathStr[MAX_PATH];
        DWORD acrobatInstallPathSize = sizeof(acrobatInstallPathStr);

        String acrobatPluginKeyPath = "Software\\Adobe\\Acrobat Reader\\" + latestAcrobatVersionString + "\\InstallPath";
        result = ::SHGetValueW(HKEY_LOCAL_MACHINE, acrobatPluginKeyPath.charactersWithNullTermination(), 0, &type, reinterpret_cast<LPBYTE>(acrobatInstallPathStr), &acrobatInstallPathSize);

        if (result == ERROR_SUCCESS) {
            String acrobatPluginDirectory = String(acrobatInstallPathStr, acrobatInstallPathSize / sizeof(WCHAR) - 1) + "\\browser";
            directories.append(acrobatPluginDirectory);
        }
    }

    ::RegCloseKey(key);
}

static inline void addMacromediaPluginDirectories(Vector<String>& directories)
{
#if !OS(WINCE)
    WCHAR systemDirectoryStr[MAX_PATH];

    if (!::GetSystemDirectoryW(systemDirectoryStr, WTF_ARRAY_LENGTH(systemDirectoryStr)))
        return;

    WCHAR macromediaDirectoryStr[MAX_PATH];

    if (!::PathCombineW(macromediaDirectoryStr, systemDirectoryStr, L"macromed\\Flash"))
        return;

    directories.append(macromediaDirectoryStr);

    if (!::PathCombineW(macromediaDirectoryStr, systemDirectoryStr, L"macromed\\Shockwave 10"))
        return;

    directories.append(macromediaDirectoryStr);
#endif
}

Vector<String> PluginInfoStore::pluginsDirectories()
{
    Vector<String> directories;

    String ourDirectory = safariPluginsDirectory();
    if (!ourDirectory.isNull())
        directories.append(ourDirectory);

    addQuickTimePluginDirectory(directories);
    addAdobeAcrobatPluginDirectory(directories);
    addMozillaPluginDirectories(directories);
    addWindowsMediaPlayerPluginDirectory(directories);
    addMacromediaPluginDirectories(directories);

    return directories;
}

Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> paths;

    PathWalker walker(directory, "*");
    if (!walker.isValid())
        return paths;

    do {
        if (walker.data().dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        String filename = walker.data().cFileName;
        if ((!filename.startsWith("np", false) || !filename.endsWith("dll", false)) && (!equalIgnoringCase(filename, "Plugin.dll") || !directory.endsWith("Shockwave 10", false)))
            continue;

        paths.append(directory + "\\" + filename);
    } while (walker.step());

    return paths;
}

static void addPluginPathsFromRegistry(HKEY rootKey, Vector<String>& paths)
{
    HKEY key;
    if (::RegOpenKeyExW(rootKey, L"Software\\MozillaPlugins", 0, KEY_ENUMERATE_SUB_KEYS, &key) != ERROR_SUCCESS)
        return;

    for (size_t i = 0; ; ++i) {
        // MSDN says that key names have a maximum length of 255 characters.
        wchar_t name[256];
        DWORD nameLen = WTF_ARRAY_LENGTH(name);
        if (::RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, 0) != ERROR_SUCCESS)
            break;

        wchar_t path[MAX_PATH];
        DWORD pathSizeInBytes = sizeof(path);
        DWORD type;
        if (::SHGetValueW(key, name, L"Path", &type, path, &pathSizeInBytes) != ERROR_SUCCESS)
            continue;
        if (type != REG_SZ)
            continue;

        paths.append(path);
    }

    ::RegCloseKey(key);
}

Vector<String> PluginInfoStore::individualPluginPaths()
{
    Vector<String> paths;

    addPluginPathsFromRegistry(HKEY_LOCAL_MACHINE, paths);
    addPluginPathsFromRegistry(HKEY_CURRENT_USER, paths);

    return paths;
}

static uint64_t fileVersion(DWORD leastSignificant, DWORD mostSignificant)
{
    ULARGE_INTEGER version;
    version.LowPart = leastSignificant;
    version.HighPart = mostSignificant;
    return version.QuadPart;
}

bool PluginInfoStore::getPluginInfo(const String& pluginPath, Plugin& plugin)
{
    return NetscapePluginModule::getPluginInfo(pluginPath, plugin);
}

static bool isOldWindowsMediaPlayerPlugin(const PluginInfoStore::Plugin& plugin)
{
    return equalIgnoringCase(plugin.info.file, "npdsplay.dll");
}

static bool isNewWindowsMediaPlayerPlugin(const PluginInfoStore::Plugin& plugin)
{
    return equalIgnoringCase(plugin.info.file, "np-mswmp.dll");
}

bool PluginInfoStore::shouldUsePlugin(Vector<Plugin>& alreadyLoadedPlugins, const Plugin& plugin)
{
    if (plugin.info.name == "Citrix ICA Client") {
        // The Citrix ICA Client plug-in requires a Mozilla-based browser; see <rdar://6418681>.
        return false;
    }

    if (plugin.info.name == "Silverlight Plug-In") {
        // workaround for <rdar://5557379> Crash in Silverlight when opening microsoft.com.
        // the latest 1.0 version of Silverlight does not reproduce this crash, so allow it
        // and any newer versions
        static const uint64_t minimumRequiredVersion = fileVersion(0x51BE0000, 0x00010000);
        return plugin.fileVersion >= minimumRequiredVersion;
    }

    if (equalIgnoringCase(plugin.info.file, "npmozax.dll")) {
        // Bug 15217: Mozilla ActiveX control complains about missing xpcom_core.dll
        return false;
    }

    if (equalIgnoringCase(plugin.info.file, "npwpf.dll")) {
        // Bug 57119: Microsoft Windows Presentation Foundation (WPF) plug-in complains about missing xpcom.dll
        return false;
    }

    if (plugin.info.name == "Yahoo Application State Plugin") {
        // https://bugs.webkit.org/show_bug.cgi?id=26860
        // Bug in Yahoo Application State plug-in earlier than 1.0.0.6 leads to heap corruption.
        static const uint64_t minimumRequiredVersion = fileVersion(0x00000006, 0x00010000);
        return plugin.fileVersion >= minimumRequiredVersion;
    }

    if (isOldWindowsMediaPlayerPlugin(plugin)) {
        // Don't load the old Windows Media Player plugin if we've already loaded the new Windows
        // Media Player plugin.
        for (size_t i = 0; i < alreadyLoadedPlugins.size(); ++i) {
            if (!isNewWindowsMediaPlayerPlugin(alreadyLoadedPlugins[i]))
                continue;
            return false;
        }
        return true;
    }

    if (isNewWindowsMediaPlayerPlugin(plugin)) {
        // Remove the old Windows Media Player plugin if we've already added it.
        for (size_t i = 0; i < alreadyLoadedPlugins.size(); ++i) {
            if (!isOldWindowsMediaPlayerPlugin(alreadyLoadedPlugins[i]))
                continue;
            alreadyLoadedPlugins.remove(i);
        }
        return true;
    }

    // FIXME: We should prefer a newer version of a plugin to an older version, rather than loading
    // only the first. <http://webkit.org/b/58469>
    String pluginFileName = pathGetFileName(plugin.path);
    for (size_t i = 0; i < alreadyLoadedPlugins.size(); ++i) {
        const Plugin& loadedPlugin = alreadyLoadedPlugins[i];

        // If a plug-in with the same filename already exists, we don't want to load it.
        if (equalIgnoringCase(pluginFileName, pathGetFileName(loadedPlugin.path)))
            return false;
    }

    return true;
}

} // namespace WebKit
