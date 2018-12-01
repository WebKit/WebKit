/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "PluginDatabase.h"

#include "PluginPackage.h"
#include <WebCore/Frame.h>
#include <wtf/URL.h>
#include <wtf/WindowsExtras.h>
#include <wtf/text/win/WCharStringExtras.h>

namespace WebCore {

static inline void addPluginPathsFromRegistry(HKEY rootKey, HashSet<String>& paths)
{
    HKEY key;
    HRESULT result = RegOpenKeyExW(rootKey, L"Software\\MozillaPlugins", 0, KEY_ENUMERATE_SUB_KEYS, &key);

    if (result != ERROR_SUCCESS)
        return;

    wchar_t name[128];
    FILETIME lastModified;

    // Enumerate subkeys
    for (int i = 0;; i++) {
        DWORD nameLen = WTF_ARRAY_LENGTH(name);
        result = RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

        if (result != ERROR_SUCCESS)
            break;

        WCHAR pathStr[_MAX_PATH];
        DWORD pathStrSize = sizeof(pathStr);
        DWORD type;

        result = getRegistryValue(key, name, L"Path", &type, pathStr, &pathStrSize);
        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;

        paths.add(wcharToString(pathStr, pathStrSize / sizeof(WCHAR) - 1));
    }

    RegCloseKey(key);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void PluginDatabase::getPluginPathsInDirectories(HashSet<String>& paths) const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;

    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findFileData;

    String oldWMPPluginPath;
    String newWMPPluginPath;

    for (auto& directory : m_pluginDirectories) {
        String pattern = directory + "\\*";

        hFind = FindFirstFileW(stringToNullTerminatedWChar(pattern).data(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE)
            continue;

        do {
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            String filename = wcharToString(findFileData.cFileName, wcslen(findFileData.cFileName));
            if (!(startsWithLettersIgnoringASCIICase(filename, "np") && filename.endsWithIgnoringASCIICase("dll"))
                && !(equalLettersIgnoringASCIICase(filename, "plugin.dll") && directory.endsWithIgnoringASCIICase("shockwave 10")))
                continue;

            String fullPath = directory + "\\" + filename;
            if (!uniqueFilenames.add(fullPath).isNewEntry)
                continue;

            paths.add(fullPath);

            if (equalLettersIgnoringASCIICase(filename, "npdsplay.dll"))
                oldWMPPluginPath = fullPath;
            else if (equalLettersIgnoringASCIICase(filename, "np-mswmp.dll"))
                newWMPPluginPath = fullPath;

        } while (FindNextFileW(hFind, &findFileData) != 0);

        FindClose(hFind);
    }

    addPluginPathsFromRegistry(HKEY_LOCAL_MACHINE, paths);
    addPluginPathsFromRegistry(HKEY_CURRENT_USER, paths);

    // If both the old and new WMP plugin are present in the plugins set, 
    // we remove the old one so we don't end up choosing the old one.
    if (!oldWMPPluginPath.isEmpty() && !newWMPPluginPath.isEmpty())
        paths.remove(oldWMPPluginPath);
}
#endif

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

static inline void addMozillaPluginDirectories(Vector<String>& directories)
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

            String extensionsPath = wcharToString(name, nameLen) + "\\Extensions";
            HKEY extensionsKey;

            // Try opening the key
            result = RegOpenKeyEx(key, stringToNullTerminatedWChar(extensionsPath).data(), 0, KEY_READ, &extensionsKey);

            if (result == ERROR_SUCCESS) {
                // Now get the plugins directory
                WCHAR pluginsDirectoryStr[_MAX_PATH];
                DWORD pluginsDirectorySize = sizeof(pluginsDirectoryStr);
                DWORD type;

                result = RegQueryValueEx(extensionsKey, TEXT("Plugins"), 0, &type, (LPBYTE)&pluginsDirectoryStr, &pluginsDirectorySize);

                if (result == ERROR_SUCCESS && type == REG_SZ)
                    directories.append(wcharToString(pluginsDirectoryStr, pluginsDirectorySize / sizeof(WCHAR) - 1));

                RegCloseKey(extensionsKey);
            }
        }
        
        RegCloseKey(key);
    }
}

static inline void addWindowsMediaPlayerPluginDirectory(Vector<String>& directories)
{
    // The new WMP Firefox plugin is installed in \PFiles\Plugins if it can't find any Firefox installs
    WCHAR pluginDirectoryStr[_MAX_PATH + 1];
    DWORD pluginDirectorySize = ::ExpandEnvironmentStringsW(TEXT("%SYSTEMDRIVE%\\PFiles\\Plugins"), pluginDirectoryStr, WTF_ARRAY_LENGTH(pluginDirectoryStr));

    if (pluginDirectorySize > 0 && pluginDirectorySize <= WTF_ARRAY_LENGTH(pluginDirectoryStr))
        directories.append(wcharToString(pluginDirectoryStr, pluginDirectorySize - 1));

    DWORD type;
    WCHAR installationDirectoryStr[_MAX_PATH];
    DWORD installationDirectorySize = sizeof(installationDirectoryStr);

    HRESULT result = getRegistryValue(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\MediaPlayer", L"Installation Directory", &type, &installationDirectoryStr, &installationDirectorySize);

    if (result == ERROR_SUCCESS && type == REG_SZ)
        directories.append(wcharToString(installationDirectoryStr, installationDirectorySize / sizeof(WCHAR) - 1));
}

static inline void addAdobeAcrobatPluginDirectory(Vector<String>& directories)
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

        Vector<int> acrobatVersion = parseVersionString(wcharToString(name, nameLen));
        if (compareVersions(acrobatVersion, latestAcrobatVersion)) {
            latestAcrobatVersion = acrobatVersion;
            latestAcrobatVersionString = wcharToString(name, nameLen);
        }
    }

    if (!latestAcrobatVersionString.isNull()) {
        DWORD type;
        WCHAR acrobatInstallPathStr[_MAX_PATH];
        DWORD acrobatInstallPathSize = sizeof(acrobatInstallPathStr);

        String acrobatPluginKeyPath = "Software\\Adobe\\Acrobat Reader\\" + latestAcrobatVersionString + "\\InstallPath";
        result = getRegistryValue(HKEY_LOCAL_MACHINE, stringToNullTerminatedWChar(acrobatPluginKeyPath).data(), 0, &type, acrobatInstallPathStr, &acrobatInstallPathSize);

        if (result == ERROR_SUCCESS) {
            String acrobatPluginDirectory = wcharToString(acrobatInstallPathStr, acrobatInstallPathSize / sizeof(WCHAR) - 1) + "\\browser";
            directories.append(acrobatPluginDirectory);
        }
    }

    RegCloseKey(key);
}

static inline void addJavaPluginDirectory(Vector<String>& directories)
{
    HKEY key;
    HRESULT result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\JavaSoft\\Java Plug-in"), 0, KEY_READ, &key);
    if (result != ERROR_SUCCESS)
        return;

    WCHAR name[128];
    FILETIME lastModified;

    Vector<int> latestJavaVersion;
    String latestJavaVersionString;

    // Enumerate subkeys
    for (int i = 0;; i++) {
        DWORD nameLen = sizeof(name) / sizeof(WCHAR);
        result = RegEnumKeyExW(key, i, name, &nameLen, 0, 0, 0, &lastModified);

        if (result != ERROR_SUCCESS)
            break;

        Vector<int> javaVersion = parseVersionString(wcharToString(name, nameLen));
        if (compareVersions(javaVersion, latestJavaVersion)) {
            latestJavaVersion = javaVersion;
            latestJavaVersionString = wcharToString(name, nameLen);
        }
    }

    if (!latestJavaVersionString.isEmpty()) {
        DWORD type;
        WCHAR javaInstallPathStr[_MAX_PATH];
        DWORD javaInstallPathSize = sizeof(javaInstallPathStr);
        DWORD useNewPluginValue;
        DWORD useNewPluginSize;

        String javaPluginKeyPath = "Software\\JavaSoft\\Java Plug-in\\" + latestJavaVersionString;
        result = getRegistryValue(HKEY_LOCAL_MACHINE, stringToNullTerminatedWChar(javaPluginKeyPath).data(), L"UseNewJavaPlugin", &type, &useNewPluginValue, &useNewPluginSize);

        if (result == ERROR_SUCCESS && useNewPluginValue == 1) {
            result = getRegistryValue(HKEY_LOCAL_MACHINE, stringToNullTerminatedWChar(javaPluginKeyPath).data(), L"JavaHome", &type, javaInstallPathStr, &javaInstallPathSize);
            if (result == ERROR_SUCCESS) {
                String javaPluginDirectory = wcharToString(javaInstallPathStr, javaInstallPathSize / sizeof(WCHAR) - 1) + "\\bin\\new_plugin";
                directories.append(javaPluginDirectory);
            }
        }
    }

    RegCloseKey(key);
}

static inline String safariPluginsDirectory()
{
    WCHAR moduleFileNameStr[_MAX_PATH];
    static String pluginsDirectory;
    static bool cachedPluginDirectory = false;

    if (!cachedPluginDirectory) {
        cachedPluginDirectory = true;

        int moduleFileNameLen = GetModuleFileName(0, moduleFileNameStr, _MAX_PATH);

        if (!moduleFileNameLen || moduleFileNameLen == _MAX_PATH)
            goto exit;

        if (!PathRemoveFileSpec(moduleFileNameStr))
            goto exit;

        pluginsDirectory = nullTerminatedWCharToString(moduleFileNameStr) + "\\Plugins";
    }
exit:
    return pluginsDirectory;
}

static inline void addMacromediaPluginDirectories(Vector<String>& directories)
{
    WCHAR systemDirectoryStr[MAX_PATH];

    if (!GetSystemDirectory(systemDirectoryStr, WTF_ARRAY_LENGTH(systemDirectoryStr)))
        return;

    WCHAR macromediaDirectoryStr[MAX_PATH];

    PathCombine(macromediaDirectoryStr, systemDirectoryStr, TEXT("macromed\\Flash"));
    directories.append(nullTerminatedWCharToString(macromediaDirectoryStr));

    PathCombine(macromediaDirectoryStr, systemDirectoryStr, TEXT("macromed\\Shockwave 10"));
    directories.append(nullTerminatedWCharToString(macromediaDirectoryStr));
}

#if ENABLE(NETSCAPE_PLUGIN_API)
Vector<String> PluginDatabase::defaultPluginDirectories()
{
    Vector<String> directories;
    String ourDirectory = safariPluginsDirectory();

    if (!ourDirectory.isNull())
        directories.append(ourDirectory);
    addAdobeAcrobatPluginDirectory(directories);
    addMozillaPluginDirectories(directories);
    addWindowsMediaPlayerPluginDirectory(directories);
    addMacromediaPluginDirectories(directories);

    return directories;
}

bool PluginDatabase::isPreferredPluginDirectory(const String& directory)
{
    String ourDirectory = safariPluginsDirectory();

    if (!ourDirectory.isNull() && !directory.isNull())
        return ourDirectory == directory;

    return false;
}
#endif

}
