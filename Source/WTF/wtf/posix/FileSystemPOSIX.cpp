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

PlatformFileHandle openFile(const String& path, FileOpenMode mode, FileAccessPermission permission, bool failIfFileExists)
{
    CString fsRep = fileSystemRepresentation(path);

    if (fsRep.isNull())
        return invalidPlatformFileHandle;

    int platformFlag = 0;
    switch (mode) {
    case FileOpenMode::Read:
        platformFlag |= O_RDONLY;
        break;
    case FileOpenMode::Write:
        platformFlag |= (O_WRONLY | O_CREAT | O_TRUNC);
        break;
    case FileOpenMode::ReadWrite:
        platformFlag |= (O_RDWR | O_CREAT);
        break;
#if OS(DARWIN)
    case FileOpenMode::EventsOnly:
        platformFlag |= O_EVTONLY;
        break;
#endif
    }

    if (failIfFileExists)
        platformFlag |= (O_CREAT | O_EXCL);

    int permissionFlag = 0;
    if (permission == FileAccessPermission::User)
        permissionFlag |= (S_IRUSR | S_IWUSR);
    else if (permission == FileAccessPermission::All)
        permissionFlag |= (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    return open(fsRep.data(), platformFlag, permissionFlag);
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
String openTemporaryFile(const String& prefix, PlatformFileHandle& handle, const String& suffix)
{
    // FIXME: Suffix is not supported, but OK for now since the code using it is macOS-port-only.
    ASSERT_UNUSED(suffix, suffix.isEmpty());

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
