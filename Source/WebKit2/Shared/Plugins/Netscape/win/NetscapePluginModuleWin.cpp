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
#include "NetscapePluginModule.h"

#include <WebCore/FileSystem.h>
#include <wtf/OwnArrayPtr.h>

using namespace WebCore;

namespace WebKit {

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

static uint64_t fileVersion(DWORD leastSignificant, DWORD mostSignificant)
{
    ULARGE_INTEGER version;
    version.LowPart = leastSignificant;
    version.HighPart = mostSignificant;
    return version.QuadPart;
}

bool NetscapePluginModule::getPluginInfo(const String& pluginPath, PluginInfoStore::Plugin& plugin)
{
        String pathCopy = pluginPath;
    DWORD versionInfoSize = ::GetFileVersionInfoSizeW(pathCopy.charactersWithNullTermination(), 0);
    if (!versionInfoSize)
        return false;

    OwnArrayPtr<char> versionInfoData = adoptArrayPtr(new char[versionInfoSize]);
    if (!::GetFileVersionInfoW(pathCopy.charactersWithNullTermination(), 0, versionInfoSize, versionInfoData.get()))
        return false;

    String name = getVersionInfo(versionInfoData.get(), "ProductName");
    String description = getVersionInfo(versionInfoData.get(), "FileDescription");
    if (name.isNull() || description.isNull())
        return false;

    VS_FIXEDFILEINFO* info;
    UINT infoSize;
    if (!::VerQueryValueW(versionInfoData.get(), L"\\", reinterpret_cast<void**>(&info), &infoSize) || infoSize < sizeof(VS_FIXEDFILEINFO))
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
    plugin.fileVersion = fileVersion(info->dwFileVersionLS, info->dwFileVersionMS);

    return true;
}

void NetscapePluginModule::determineQuirks()
{
}

} // namespace WebKit

