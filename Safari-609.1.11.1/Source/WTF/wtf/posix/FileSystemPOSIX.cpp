/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/EnumTraits.h>
#include <wtf/FileMetadata.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WTF {

namespace FileSystemImpl {

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

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    CString fsRep = fileSystemRepresentation(path);

    if (fsRep.isNull())
        return invalidPlatformFileHandle;

    int platformFlag = 0;
    if (mode == FileOpenMode::Read)
        platformFlag |= O_RDONLY;
    else if (mode == FileOpenMode::Write)
        platformFlag |= (O_WRONLY | O_CREAT | O_TRUNC);
#if OS(DARWIN)
    else if (mode == FileOpenMode::EventsOnly)
        platformFlag |= O_EVTONLY;
#endif

    return open(fsRep.data(), platformFlag, 0666);
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        close(handle);
        handle = invalidPlatformFileHandle;
    }
}

long long seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    int whence = SEEK_SET;
    switch (origin) {
    case FileSeekOrigin::Beginning:
        whence = SEEK_SET;
        break;
    case FileSeekOrigin::Current:
        whence = SEEK_CUR;
        break;
    case FileSeekOrigin::End:
        whence = SEEK_END;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return static_cast<long long>(lseek(handle, offset, whence));
}

bool truncateFile(PlatformFileHandle handle, long long offset)
{
    // ftruncate returns 0 to indicate the success.
    return !ftruncate(handle, offset);
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    do {
        int bytesWritten = write(handle, data, static_cast<size_t>(length));
        if (bytesWritten >= 0)
            return bytesWritten;
    } while (errno == EINTR);
    return -1;
}

int readFromFile(PlatformFileHandle handle, char* data, int length)
{
    do {
        int bytesRead = read(handle, data, static_cast<size_t>(length));
        if (bytesRead >= 0)
            return bytesRead;
    } while (errno == EINTR);
    return -1;
}

#if USE(FILE_LOCK)
bool lockFile(PlatformFileHandle handle, OptionSet<FileLockMode> lockMode)
{
    COMPILE_ASSERT(LOCK_SH == WTF::enumToUnderlyingType(FileLockMode::Shared), LockSharedEncodingIsAsExpected);
    COMPILE_ASSERT(LOCK_EX == WTF::enumToUnderlyingType(FileLockMode::Exclusive), LockExclusiveEncodingIsAsExpected);
    COMPILE_ASSERT(LOCK_NB == WTF::enumToUnderlyingType(FileLockMode::Nonblocking), LockNonblockingEncodingIsAsExpected);
    int result = flock(handle, lockMode.toRaw());
    return (result != -1);
}

bool unlockFile(PlatformFileHandle handle)
{
    int result = flock(handle, LOCK_UN);
    return (result != -1);
}
#endif

#if !PLATFORM(MAC)
bool deleteEmptyDirectory(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return false;

    // rmdir(...) returns 0 on successful deletion of the path and non-zero in any other case (including invalid permissions or non-existent file)
    return !rmdir(fsRep.data());
}
#endif

bool getFileSize(const String& path, long long& result)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return false;

    struct stat fileInfo;

    if (stat(fsRep.data(), &fileInfo))
        return false;

    result = fileInfo.st_size;
    return true;
}

bool getFileSize(PlatformFileHandle handle, long long& result)
{
    struct stat fileInfo;
    if (fstat(handle, &fileInfo))
        return false;

    result = fileInfo.st_size;
    return true;
}

Optional<WallTime> getFileCreationTime(const String& path)
{
#if OS(DARWIN) || OS(OPENBSD) || OS(NETBSD) || OS(FREEBSD)
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return WTF::nullopt;

    struct stat fileInfo;

    if (stat(fsRep.data(), &fileInfo))
        return WTF::nullopt;

    return WallTime::fromRawSeconds(fileInfo.st_birthtime);
#else
    UNUSED_PARAM(path);
    return WTF::nullopt;
#endif
}

Optional<WallTime> getFileModificationTime(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return WTF::nullopt;

    struct stat fileInfo;

    if (stat(fsRep.data(), &fileInfo))
        return WTF::nullopt;

    return WallTime::fromRawSeconds(fileInfo.st_mtime);
}

static FileMetadata::Type toFileMetataType(struct stat fileInfo)
{
    if (S_ISDIR(fileInfo.st_mode))
        return FileMetadata::Type::Directory;
    if (S_ISLNK(fileInfo.st_mode))
        return FileMetadata::Type::SymbolicLink;
    return FileMetadata::Type::File;
}

static Optional<FileMetadata> fileMetadataUsingFunction(const String& path, int (*statFunc)(const char*, struct stat*))
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return WTF::nullopt;

    struct stat fileInfo;
    if (statFunc(fsRep.data(), &fileInfo))
        return WTF::nullopt;

    String filename = pathGetFileName(path);
    bool isHidden = !filename.isEmpty() && filename[0] == '.';
    return FileMetadata {
        WallTime::fromRawSeconds(fileInfo.st_mtime),
        fileInfo.st_size,
        isHidden,
        toFileMetataType(fileInfo)
    };
}

Optional<FileMetadata> fileMetadata(const String& path)
{
    return fileMetadataUsingFunction(path, &lstat);
}

Optional<FileMetadata> fileMetadataFollowingSymlinks(const String& path)
{
    return fileMetadataUsingFunction(path, &stat);
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

String pathGetFileName(const String& path)
{
    return path.substring(path.reverseFind('/') + 1);
}

String directoryName(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);

    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return String();

    return String::fromUTF8(dirname(fsRep.mutableData()));
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;
    CString cpath = fileSystemRepresentation(path);
    CString cfilter = fileSystemRepresentation(filter);
    DIR* dir = opendir(cpath.data());
    if (dir) {
        struct dirent* dp;
        while ((dp = readdir(dir))) {
            const char* name = dp->d_name;
            if (!strcmp(name, ".") || !strcmp(name, ".."))
                continue;
            if (fnmatch(cfilter.data(), name, 0))
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

#if !USE(CF)
String stringFromFileSystemRepresentation(const char* path)
{
    if (!path)
        return String();

    return String::fromUTF8(path);
}

CString fileSystemRepresentation(const String& path)
{
    return path.utf8();
}
#endif

#if !PLATFORM(COCOA)
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

bool getVolumeFreeSpace(const String& path, uint64_t& freeSpace)
{
    struct statvfs fileSystemStat;
    if (statvfs(fileSystemRepresentation(path).data(), &fileSystemStat)) {
        freeSpace = fileSystemStat.f_bavail * fileSystemStat.f_frsize;
        return true;
    }
    return false;
}

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    char buffer[PATH_MAX];
    const char* tmpDir = getenv("TMPDIR");

    if (!tmpDir)
        tmpDir = "/tmp";

    if (snprintf(buffer, PATH_MAX, "%s/%sXXXXXX", tmpDir, prefix.utf8().data()) >= PATH_MAX)
        goto end;

    handle = mkstemp(buffer);
    if (handle < 0)
        goto end;

    return String::fromUTF8(buffer);

end:
    handle = invalidPlatformFileHandle;
    return String();
}
#endif // !PLATFORM(COCOA)

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

Optional<int32_t> getFileDeviceId(const CString& fsFile)
{
    struct stat fileStat;
    if (stat(fsFile.data(), &fileStat) == -1)
        return WTF::nullopt;

    return fileStat.st_dev;
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
