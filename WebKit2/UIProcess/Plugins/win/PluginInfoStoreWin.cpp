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

#include "PluginInfoStore.h"

#define DISABLE_NOT_IMPLEMENTED_WARNINGS 1
#include "NotImplemented.h"
#include <WebCore/FileSystem.h>
#include <shlwapi.h>
#include <wtf/OwnArrayPtr.h>

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
        int moduleFileNameLen = ::GetModuleFileNameW(0, moduleFileNameStr, _countof(moduleFileNameStr));

        if (!moduleFileNameLen || moduleFileNameLen == _countof(moduleFileNameStr))
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
        DWORD nameLen = _countof(name);
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
    DWORD pluginDirectorySize = ::ExpandEnvironmentStringsW(L"%SYSTEMDRIVE%\\PFiles\\Plugins", pluginDirectoryStr, _countof(pluginDirectoryStr));

    if (pluginDirectorySize > 0 && pluginDirectorySize <= _countof(pluginDirectoryStr))
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
        DWORD nameLen = _countof(name);
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

    if (!::GetSystemDirectoryW(systemDirectoryStr, _countof(systemDirectoryStr)))
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

class PathWalker : public Noncopyable {
public:
    PathWalker(const String& directory)
    {
        String pattern = directory + "\\*";
        m_handle = ::FindFirstFileW(pattern.charactersWithNullTermination(), &m_data);
    }

    ~PathWalker()
    {
        if (!isValid())
            return;
        ::FindClose(m_handle);
    }

    bool isValid() const { return m_handle != INVALID_HANDLE_VALUE; }
    const WIN32_FIND_DATAW& data() const { return m_data; }

    bool step() { return ::FindNextFileW(m_handle, &m_data); }

private:
    HANDLE m_handle;
    WIN32_FIND_DATAW m_data;
};

Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> paths;

    PathWalker walker(directory);
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

static String getVersionInfo(const LPVOID versionInfoData, const String& info)
{
    LPVOID buffer;
    UINT bufferLength;
    String subInfo = "\\StringfileInfo\\040904E4\\" + info;
    if (!::VerQueryValueW(versionInfoData, const_cast<UChar*>(subInfo.charactersWithNullTermination()), &buffer, &bufferLength) || !bufferLength)
        return String();

    // Subtract 1 from the length; we don't want the trailing null character.
    return String(reinterpret_cast<UChar*>(buffer), bufferLength - 1);
}

bool PluginInfoStore::getPluginInfo(const String& pluginPath, Plugin& plugin)
{
    String pathCopy = pluginPath;
    DWORD versionInfoSize = ::GetFileVersionInfoSizeW(pathCopy.charactersWithNullTermination(), 0);
    if (!versionInfoSize)
        return false;

    OwnArrayPtr<char> versionInfoData(new char[versionInfoSize]);
    if (!::GetFileVersionInfoW(pathCopy.charactersWithNullTermination(), 0, versionInfoSize, versionInfoData.get()))
        return false;

    String name = getVersionInfo(versionInfoData.get(), "ProductName");
    String description = getVersionInfo(versionInfoData.get(), "FileDescription");
    if (name.isNull() || description.isNull())
        return false;

    Vector<String> types;
    getVersionInfo(versionInfoData.get(), "MIMEType").split('|', types);
    Vector<String> extensionLists;
    getVersionInfo(versionInfoData.get(), "FileExtents").split('|', extensionLists);
    Vector<String> descriptions;
    getVersionInfo(versionInfoData.get(), "FileOpenName").split('|', descriptions);

    Vector<MimeClassInfo> mimes(types.size());
    for (size_t i = 0; i < types.size(); i++) {
        String type = types[i].lower();
        String description = i < descriptions.size() ? descriptions[i] : "";
        String extensionList = i < extensionLists.size() ? extensionLists[i] : "";

        Vector<String> extensionsVector;
        extensionList.split(',', extensionsVector);

        // Get rid of the extension list that may be at the end of the description string.
        int pos = description.find("(*");
        if (pos != -1) {
            // There might be a space that we need to get rid of.
            if (pos > 1 && description[pos - 1] == ' ')
                pos--;
            description = description.left(pos);
        }

        mimes[i].type = type;
        mimes[i].desc = description;
        mimes[i].extensions.swap(extensionsVector);
    }

    plugin.path = pluginPath;
    plugin.info.desc = description;
    plugin.info.name = name;
    plugin.info.file = pathGetFileName(pluginPath);
    plugin.info.mimes.swap(mimes);
    return true;
}

bool PluginInfoStore::shouldUsePlugin(const Plugin& plugin, const Vector<Plugin>& loadedPlugins)
{
    // FIXME: <http://webkit.org/b/43509> Migrate logic here from
    // PluginDatabase::getPluginPathsInDirectories and PluginPackage::isPluginBlacklisted.
    notImplemented();
    return true;
}

} // namespace WebKit
