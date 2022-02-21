/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/FileSystem.h>

#include <dirent.h>
#include <sys/statvfs.h>

namespace WTF {

namespace FileSystemImpl {

enum class ShouldFollowSymbolicLinks { No, Yes };
static std::optional<FileType> fileTypePotentiallyFollowingSymLinks(const String& path, ShouldFollowSymbolicLinks shouldFollowSymbolicLinks)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return std::nullopt;

    auto statFunc = shouldFollowSymbolicLinks == ShouldFollowSymbolicLinks::Yes ? stat : lstat;
    struct stat fileInfo;
    if (statFunc(fsRep.data(), &fileInfo))
        return std::nullopt;

    if (S_ISDIR(fileInfo.st_mode))
        return FileType::Directory;
    if (S_ISLNK(fileInfo.st_mode))
        return FileType::SymbolicLink;
    return FileType::Regular;
}

bool moveFile(const String& oldPath, const String& newPath)
{
    auto oldFilename = fileSystemRepresentation(oldPath);
    if (oldFilename.isNull())
        return false;

    auto newFilename = fileSystemRepresentation(newPath);
    if (newFilename.isNull())
        return false;

    return rename(oldFilename.data(), newFilename.data()) != -1;
}

std::optional<uint64_t> volumeFreeSpace(const String& path)
{
    struct statvfs fileSystemStat;
    if (!statvfs(fileSystemRepresentation(path).data(), &fileSystemStat))
        return fileSystemStat.f_bavail * fileSystemStat.f_frsize;
    return std::nullopt;
}

Vector<String> listDirectorySub(const String& path, bool fullPath)
{
    Vector<String> entries;
    CString cpath = fileSystemRepresentation(path);
    DIR* dir = opendir(cpath.data());
    if (dir) {
        struct dirent* dp;
        while ((dp = readdir(dir))) {
            const char* name = dp->d_name;
            if (!strcmp(name, ".") || !strcmp(name, ".."))
                continue;
            String newEntry;
            if (fullPath) {
                char filePath[PATH_MAX];
                if (fullPath && static_cast<int>(sizeof(filePath) - 1) < snprintf(filePath, sizeof(filePath), "%s/%s", cpath.data(), name))
                    continue; // buffer overflow

                newEntry = stringFromFileSystemRepresentation(filePath);
            } else
                newEntry = stringFromFileSystemRepresentation(name);

            // Some file system representations cannot be represented as a UTF-16 string,
            // so this newEntry might be null.
            if (!newEntry.isNull())
                entries.append(WTFMove(newEntry));
        }
        closedir(dir);
    }
    return entries;
}

Vector<String> listDirectory(const String& path)
{
    return listDirectorySub(path, false);
}

bool deleteNonEmptyDirectory(const String& path)
{
    auto entries = listDirectorySub(path, true);
    for (auto& entry : entries) {
        if (fileTypePotentiallyFollowingSymLinks(entry, ShouldFollowSymbolicLinks::No) == FileType::Directory)
            deleteNonEmptyDirectory(entry);
        else
            deleteFile(entry);
    }
    return deleteEmptyDirectory(path);
}


String realPath(const String& filePath)
{
    CString fsRep = fileSystemRepresentation(filePath);
    char resolvedName[PATH_MAX];
    const char* result = realpath(fsRep.data(), resolvedName);
    return result ? String::fromUTF8(result) : filePath;
}



} // namespace FileSystemImpl
} // namespace WTF

