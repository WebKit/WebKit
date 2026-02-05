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
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/EnumTraits.h>
#include <wtf/FileHandle.h>
#include <wtf/MallocSpan.h>
#include <wtf/MappedFileData.h>
#include <wtf/SafeStrerror.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if USE(GLIB)
#include <glib.h>
#endif

#if OS(HAIKU)
#include "FindDirectory.h"
#include "Path.h"
#endif

namespace WTF {

namespace FileSystemImpl {

FileHandle openFile(const String& path, FileOpenMode mode, FileAccessPermission permission, OptionSet<FileLockMode> lockMode, bool failIfFileExists)
{
    CString fsRep = fileSystemRepresentation(path);

    if (fsRep.isNull())
        return { };

    int platformFlag = O_CLOEXEC;
    switch (mode) {
    case FileOpenMode::Read:
        platformFlag |= O_RDONLY;
        break;
    case FileOpenMode::Truncate:
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

    return FileHandle::adopt(open(fsRep.data(), platformFlag, permissionFlag), lockMode);
}

std::optional<WallTime> fileCreationTime(const String& path)
{
#if (OS(LINUX) && HAVE(STATX)) || OS(DARWIN) || OS(OPENBSD) || OS(NETBSD) || OS(FREEBSD)
    CString fsRep = fileSystemRepresentation(path);
    if (!fsRep.data() || fsRep.data()[0] == '\0')
        return std::nullopt;

#if OS(LINUX) && HAVE(STATX)
    struct statx fileInfo;

    if (statx(-1, fsRep.data(), 0, STATX_BTIME, &fileInfo) == -1)
        return std::nullopt;

    return WallTime::fromRawSeconds(fileInfo.stx_btime.tv_sec);
#elif OS(DARWIN) || OS(OPENBSD) || OS(NETBSD) || OS(FREEBSD) || OS(HAIKU)
    struct stat fileInfo;

    if (stat(fsRep.data(), &fileInfo) == -1)
        return std::nullopt;

    return WallTime::fromRawSeconds(fileInfo.st_birthtime);
#endif
#endif

    UNUSED_PARAM(path);
    return std::nullopt;
}

bool fileIDsAreEqual(std::optional<PlatformFileID> a, std::optional<PlatformFileID> b)
{
    return a == b;
}

std::optional<uint32_t> volumeFileBlockSize(const String& path)
{
    struct statvfs fileStat;
    if (!statvfs(fileSystemRepresentation(path).data(), &fileStat))
        return fileStat.f_frsize;

    return std::nullopt;
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
static const char* temporaryFileDirectory()
{
#if USE(GLIB)
    return g_get_tmp_dir();
#elif OS(HAIKU)
    static char buffer[B_PATH_NAME_LENGTH];
    static std::once_flag once;
    std::call_once(once, [] {
        BPath path;
        if (find_directory(B_SYSTEM_TEMP_DIRECTORY, &path) == B_OK)
            strlcpy(buffer, path.Path(), sizeof(buffer));
        else
            strlcpy(buffer, "/tmp", sizeof(buffer));
    });
    return buffer;
#else
    if (auto* tmpDir = getenv("TMPDIR"))
        return tmpDir;

    return "/tmp";
#endif
}

std::pair<String, FileHandle> openTemporaryFile(StringView prefix, StringView suffix)
{
    // Suffix is not supported because that's incompatible with mkostemp, mkostemps would be needed for that.
    // This is OK for now since the code using it is built on macOS only.
    ASSERT_UNUSED(suffix, suffix.isEmpty());

    IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
    const char* directory = temporaryFileDirectory();
    CString prefixUTF8 = prefix.utf8();
    size_t length = strlen(directory) + 1 + prefixUTF8.length() + 1 + 6 + 1;
    auto buffer = MallocSpan<char>::malloc(length);
    snprintf(buffer.mutableSpan().data(), length, "%s/%s-XXXXXX", directory, prefixUTF8.data());
    IGNORE_CLANG_WARNINGS_END

    auto handle = FileHandle::adopt(mkostemp(buffer.mutableSpan().data(), O_CLOEXEC));
    if (!handle)
        return { String(), FileHandle() };

    return { String::fromUTF8(buffer.span().data()), WTF::move(handle) };
}
#endif // !PLATFORM(COCOA)

std::optional<int32_t> getFileDeviceId(const String& path)
{
    auto fsFile = fileSystemRepresentation(path);
    if (fsFile.isNull())
        return std::nullopt;

    struct stat fileStat;
    if (stat(fsFile.data(), &fileStat) == -1)
        return std::nullopt;

    return fileStat.st_dev;
}

// On macOS, stat() used by std::filesystem is much slower than access() when sandboxed.
// This fast path exists to avoid calls to stat(). It's not needed on other platforms.
#if ENABLE(FILESYSTEM_POSIX_FAST_PATH)

bool fileExists(const String& path)
{
    return access(fileSystemRepresentation(path).data(), F_OK) != -1;
}

bool deleteFile(const String& path)
{
    // unlink(...) returns 0 on successful deletion of the path and non-zero in any other case (including invalid permissions or non-existent file)
    bool unlinked = !unlink(fileSystemRepresentation(path).data());
    if (!unlinked && errno != ENOENT)
        LOG_ERROR("File failed to delete. Error message: %s", safeStrerror(errno).data());

    return unlinked;
}

bool makeAllDirectories(const String& path)
{
    auto fullPath = fileSystemRepresentation(path);
    int length = fullPath.length();
    if (!length)
        return false;

    if (!access(fullPath.data(), F_OK))
        return true;

    auto p = fullPath.mutableSpanIncludingNullTerminator().subspan(1);
    if (p[length - 1] == '/')
        p[length - 1] = '\0';
    for (; p[0]; skip(p, 1)) {
        if (p[0] == '/') {
            p[0] = '\0';
            if (access(fullPath.data(), F_OK)) {
                if (mkdir(fullPath.data(), S_IRWXU))
                    return false;
            }
            p[0] = '/';
        }
    }
    if (access(fullPath.data(), F_OK)) {
        if (mkdir(fullPath.data(), S_IRWXU))
            return false;
    }

    return true;
}

String pathByAppendingComponent(StringView path, StringView component)
{
    if (path.endsWith('/'))
        return makeString(path, component);
    return makeString(path, '/', component);
}

String pathByAppendingComponents(StringView path, std::span<const StringView> components)
{
    StringBuilder builder;
    builder.append(path);
    bool isFirstComponent = true;
    for (auto& component : components) {
        if (isFirstComponent) {
            isFirstComponent = false;
            if (path.endsWith('/')) {
                builder.append(component);
                continue;
            }
        }
        builder.append('/', component);
    }
    return builder.toString();
}

#endif

} // namespace FileSystemImpl
} // namespace WTF
