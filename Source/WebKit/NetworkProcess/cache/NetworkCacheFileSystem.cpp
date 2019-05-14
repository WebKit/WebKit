/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "NetworkCacheFileSystem.h"

#include "Logging.h"
#include <wtf/Assertions.h>
#include <wtf/FileSystem.h>
#include <wtf/Function.h>
#include <wtf/text/CString.h>

#if !OS(WINDOWS)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#else
#include <windows.h>
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#include <sys/attr.h>
#include <unistd.h>
#endif

#if USE(SOUP)
#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebKit {
namespace NetworkCache {

#if !OS(WINDOWS)
static DirectoryEntryType directoryEntryType(uint8_t dtype)
{
    switch (dtype) {
    case DT_DIR:
        return DirectoryEntryType::Directory;
    case DT_REG:
        return DirectoryEntryType::File;
    default:
        ASSERT_NOT_REACHED();
        return DirectoryEntryType::File;
    }
    return DirectoryEntryType::File;
}
#endif

void traverseDirectory(const String& path, const Function<void (const String&, DirectoryEntryType)>& function)
{
#if !OS(WINDOWS)
    DIR* dir = opendir(FileSystem::fileSystemRepresentation(path).data());
    if (!dir)
        return;
    dirent* dp;
    while ((dp = readdir(dir))) {
        if (dp->d_type != DT_DIR && dp->d_type != DT_REG)
            continue;
        const char* name = dp->d_name;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        auto nameString = String::fromUTF8(name);
        if (nameString.isNull())
            continue;
        function(nameString, directoryEntryType(dp->d_type));
    }
    closedir(dir);
#else
    auto entries = FileSystem::listDirectory(path);
    for (auto& entry : entries) {
        auto type = FileSystem::fileIsDirectory(entry, FileSystem::ShouldFollowSymbolicLinks::No) ? DirectoryEntryType::Directory : DirectoryEntryType::File;
        function(entry, type);
    }
#endif
}

void deleteDirectoryRecursively(const String& path)
{
    traverseDirectory(path, [&path](const String& name, DirectoryEntryType type) {
        String entryPath = FileSystem::pathByAppendingComponent(path, name);
        switch (type) {
        case DirectoryEntryType::File:
            FileSystem::deleteFile(entryPath);
            break;
        case DirectoryEntryType::Directory:
            deleteDirectoryRecursively(entryPath);
            break;
        // This doesn't follow symlinks.
        }
    });
    FileSystem::deleteEmptyDirectory(path);
}

FileTimes fileTimes(const String& path)
{
#if HAVE(STAT_BIRTHTIME)
    struct stat fileInfo;
    if (stat(FileSystem::fileSystemRepresentation(path).data(), &fileInfo))
        return { };
    return { WallTime::fromRawSeconds(fileInfo.st_birthtime), WallTime::fromRawSeconds(fileInfo.st_mtime) };
#elif USE(SOUP)
    // There's no st_birthtime in some operating systems like Linux, so we use xattrs to set/get the creation time.
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(FileSystem::fileSystemRepresentation(path).data()));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_info(file.get(), "xattr::birthtime,time::modified", G_FILE_QUERY_INFO_NONE, nullptr, nullptr));
    if (!fileInfo)
        return { };
    const char* birthtimeString = g_file_info_get_attribute_string(fileInfo.get(), "xattr::birthtime");
    if (!birthtimeString)
        return { };
    return { WallTime::fromRawSeconds(g_ascii_strtoull(birthtimeString, nullptr, 10)),
        WallTime::fromRawSeconds(g_file_info_get_attribute_uint64(fileInfo.get(), "time::modified")) };
#elif OS(WINDOWS)
    auto createTime = FileSystem::getFileCreationTime(path);
    auto modifyTime = FileSystem::getFileModificationTime(path);
    return { createTime.valueOr(WallTime()), modifyTime.valueOr(WallTime()) };
#endif
}

void updateFileModificationTimeIfNeeded(const String& path)
{
    auto times = fileTimes(path);
    if (times.creation != times.modification) {
        // Don't update more than once per hour.
        if (WallTime::now() - times.modification < 1_h)
            return;
    }
#if !OS(WINDOWS)
    // This really updates both the access time and the modification time.
    utimes(FileSystem::fileSystemRepresentation(path).data(), nullptr);
#else
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    auto file = CreateFile(path.wideCharacters().data(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    SetFileTime(file, &time, &time, &time);
    CloseHandle(file);
#endif
}

}
}
