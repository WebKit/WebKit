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

#if !HAVE(STD_FILESYSTEM) && !HAVE(STD_EXPERIMENTAL_FILESYSTEM)

#include <dirent.h>
#include <libgen.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/text/StringBuilder.h>

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

bool fileExists(const String& path)
{
    if (path.isNull())
        return false;

    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return false;

    return access(fsRep.data(), F_OK) != -1;
}

bool deleteFile(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0') {
        LOG_ERROR("File failed to delete. Failed to get filesystem representation to create CString from cfString or filesystem representation is a null value");
        return false;
    }

    // unlink(...) returns 0 on successful deletion of the path and non-zero in any other case (including invalid permissions or non-existent file)
    bool unlinked = !unlink(fsRep.data());
    if (!unlinked && errno != ENOENT)
        LOG_ERROR("File failed to delete. Error message: %s", strerror(errno));

    return unlinked;
}

bool deleteEmptyDirectory(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return false;

    // rmdir(...) returns 0 on successful deletion of the path and non-zero in any other case (including invalid permissions or non-existent file)
    return !rmdir(fsRep.data());
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

std::optional<uint64_t> fileSize(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return std::nullopt;

    struct stat fileInfo;

    if (stat(fsRep.data(), &fileInfo))
        return std::nullopt;

    return fileInfo.st_size;
}

bool makeAllDirectories(const String& path)
{
    CString fullPath = fileSystemRepresentation(path);
    if (!access(fullPath.data(), F_OK))
        return true;

    char* p = fullPath.mutableData() + 1;
    int length = fullPath.length();

    if (p[length - 1] == '/')
        p[length - 1] = '\0';
    for (; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (access(fullPath.data(), F_OK)) {
                if (mkdir(fullPath.data(), S_IRWXU))
                    return false;
            }
            *p = '/';
        }
    }
    if (access(fullPath.data(), F_OK)) {
        if (mkdir(fullPath.data(), S_IRWXU))
            return false;
    }

    return true;
}

std::optional<uint64_t> volumeFreeSpace(const String& path)
{
    struct statvfs fileSystemStat;
    if (!statvfs(fileSystemRepresentation(path).data(), &fileSystemStat))
        return fileSystemStat.f_bavail * fileSystemStat.f_frsize;
    return std::nullopt;
}

bool createSymbolicLink(const String& targetPath, const String& symbolicLinkPath)
{
    CString targetPathFSRep = fileSystemRepresentation(targetPath);
    if (!targetPathFSRep.data() || targetPathFSRep.data()[0] == '\0')
        return false;

    CString symbolicLinkPathFSRep = fileSystemRepresentation(symbolicLinkPath);
    if (!symbolicLinkPathFSRep.data() || symbolicLinkPathFSRep.data()[0] == '\0')
        return false;

    return !symlink(targetPathFSRep.data(), symbolicLinkPathFSRep.data());
}

bool hardLink(const String& source, const String& destination)
{
    if (source.isEmpty() || destination.isEmpty())
        return false;

    auto fsSource = fileSystemRepresentation(source);
    if (!fsSource.data())
        return false;

    auto fsDestination = fileSystemRepresentation(destination);
    if (!fsDestination.data())
        return false;

    return !link(fsSource.data(), fsDestination.data());
}

bool hardLinkOrCopyFile(const String& source, const String& destination)
{
    if (hardLink(source, destination))
        return true;

    // Hard link failed. Perform a copy instead.
    if (source.isEmpty() || destination.isEmpty())
        return false;

    auto fsSource = fileSystemRepresentation(source);
    if (!fsSource.data())
        return false;

    auto fsDestination = fileSystemRepresentation(destination);
    if (!fsDestination.data())
        return false;

    auto handle = open(fsDestination.data(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (handle == -1)
        return false;

    bool appendResult = appendFileContentsToFileHandle(source, handle);
    close(handle);

    // If the copy failed, delete the unusable file.
    if (!appendResult)
        unlink(fsDestination.data());

    return appendResult;
}

std::optional<uint64_t> hardLinkCount(const String& path)
{
    auto linkPath = fileSystemRepresentation(path);
    struct stat stat;
    if (::stat(linkPath.data(), &stat) < 0)
        return std::nullopt;

    // Link count is 2 in the single client case (the blob file and a link).
    return stat.st_nlink - 1;
}

bool deleteNonEmptyDirectory(const String& path)
{
    auto entries = listDirectory(path);
    for (auto& entry : entries) {
        if (fileTypePotentiallyFollowingSymLinks(entry, ShouldFollowSymbolicLinks::No) == FileType::Directory)
            deleteNonEmptyDirectory(entry);
        else
            deleteFile(entry);
    }
    return deleteEmptyDirectory(path);
}

std::optional<WallTime> fileModificationTime(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return std::nullopt;

    struct stat fileInfo;

    if (stat(fsRep.data(), &fileInfo))
        return std::nullopt;

    return WallTime::fromRawSeconds(fileInfo.st_mtime);
}

bool updateFileModificationTime(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return false;

    // Passing in null sets the modification time to now
    return !utimes(fsRep.data(), nullptr);
}

bool isHiddenFile(const String& path)
{
    auto filename = pathFileName(path);

    return !filename.isEmpty() && filename[0] == '.';
}

std::optional<FileType> fileType(const String& path)
{
    return fileTypePotentiallyFollowingSymLinks(path, ShouldFollowSymbolicLinks::No);
}

std::optional<FileType> fileTypeFollowingSymlinks(const String& path)
{
    return fileTypePotentiallyFollowingSymLinks(path, ShouldFollowSymbolicLinks::Yes);
}

String pathFileName(const String& path)
{
    return path.substring(path.reverseFind('/') + 1);
}

String parentPath(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return String();

    return String::fromUTF8(dirname(fsRep.mutableData()));
}

String realPath(const String& filePath)
{
    CString fsRep = fileSystemRepresentation(filePath);
    char resolvedName[PATH_MAX];
    const char* result = realpath(fsRep.data(), resolvedName);
    return result ? String::fromUTF8(result) : filePath;
}

String pathByAppendingComponent(const String& path, const String& component)
{
    if (path.endsWith('/'))
        return path + component;
    return path + "/" + component;
}

String pathByAppendingComponents(StringView path, const Vector<StringView>& components)
{
    StringBuilder builder;
    builder.append(path);
    for (auto& component : components)
        builder.append('/', component);
    return builder.toString();
}

Vector<String> listDirectory(const String& path)
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
            char filePath[PATH_MAX];
            if (static_cast<int>(sizeof(filePath) - 1) < snprintf(filePath, sizeof(filePath), "%s/%s", cpath.data(), name))
                continue; // buffer overflow

            auto string = stringFromFileSystemRepresentation(filePath);

            // Some file system representations cannot be represented as a UTF-16 string,
            // so this string might be null.
            if (!string.isNull())
                entries.append(WTFMove(string));
        }
        closedir(dir);
    }
    return entries;
}

} // namespace FileSystemImpl
} // namespace WTF

#endif // !HAVE(STD_FILESYSTEM) && !HAVE(STD_EXPERIMENTAL_FILESYSTEM)
