/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebMediaKeyStorageManager.h"

#include "WebProcessDataStoreParameters.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/FileSystem.h>
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

void WebMediaKeyStorageManager::setWebsiteDataStore(const WebProcessDataStoreParameters& parameters)
{
    m_mediaKeyStorageDirectory = parameters.mediaKeyStorageDirectory;
}

const char* WebMediaKeyStorageManager::supplementName()
{
    return "WebMediaKeyStorageManager";
}

String WebMediaKeyStorageManager::mediaKeyStorageDirectoryForOrigin(const SecurityOriginData& originData)
{
    if (!m_mediaKeyStorageDirectory)
        return emptyString();

    return FileSystem::pathByAppendingComponent(m_mediaKeyStorageDirectory, originData.databaseIdentifier());
}

Vector<SecurityOriginData> WebMediaKeyStorageManager::getMediaKeyOrigins()
{
    Vector<SecurityOriginData> results;

    if (m_mediaKeyStorageDirectory.isEmpty())
        return results;

    for (auto& identifier : FileSystem::listDirectory(m_mediaKeyStorageDirectory)) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(identifier))
            results.append(*securityOrigin);
    }

    return results;
}

static void removeAllMediaKeyStorageForOriginPath(const String& originPath, WallTime startDate, WallTime endDate)
{
    Vector<String> mediaKeyNames = FileSystem::listDirectory(originPath);

    for (const auto& mediaKeyName : mediaKeyNames) {
        auto mediaKeyPath = FileSystem::pathByAppendingComponent(originPath, mediaKeyName);
        String mediaKeyFile = FileSystem::pathByAppendingComponent(mediaKeyPath, "SecureStop.plist");

        if (!FileSystem::fileExists(mediaKeyFile))
            continue;

        auto modificationTime = FileSystem::fileModificationTime(mediaKeyFile);
        if (!modificationTime)
            continue;
        if (modificationTime.value() < startDate || modificationTime.value() > endDate)
            continue;

        FileSystem::deleteFile(mediaKeyFile);
        FileSystem::deleteEmptyDirectory(mediaKeyPath);
    }
    
    FileSystem::deleteEmptyDirectory(originPath);
}

void WebMediaKeyStorageManager::deleteMediaKeyEntriesForOrigin(const SecurityOriginData& originData)
{
    if (m_mediaKeyStorageDirectory.isEmpty())
        return;

    String originPath = mediaKeyStorageDirectoryForOrigin(originData);
    removeAllMediaKeyStorageForOriginPath(originPath, -WallTime::infinity(), WallTime::infinity());
}

void WebMediaKeyStorageManager::deleteMediaKeyEntriesModifiedBetweenDates(WallTime startDate, WallTime endDate)
{
    if (m_mediaKeyStorageDirectory.isEmpty())
        return;

    Vector<String> originNames = FileSystem::listDirectory(m_mediaKeyStorageDirectory);
    for (auto& originName : originNames)
        removeAllMediaKeyStorageForOriginPath(FileSystem::pathByAppendingComponent(m_mediaKeyStorageDirectory, originName), startDate, endDate);
}

void WebMediaKeyStorageManager::deleteAllMediaKeyEntries()
{
    if (m_mediaKeyStorageDirectory.isEmpty())
        return;

    Vector<String> originNames = FileSystem::listDirectory(m_mediaKeyStorageDirectory);
    for (auto& originName : originNames)
        removeAllMediaKeyStorageForOriginPath(FileSystem::pathByAppendingComponent(m_mediaKeyStorageDirectory, originName), -WallTime::infinity(), WallTime::infinity());
}

}
