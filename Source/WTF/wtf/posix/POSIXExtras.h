/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <stdio.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/UTF8CStringView.h>

#if OS(UNIX)
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if OS(UNIX) && !PLATFORM(PLAYSTATION)
#include <dlfcn.h>
#include <sys/mman.h>
#endif

// Wrappers for POSIX and C library functions that take UTF-8 paths, so that callers can pass typed strings
// instead of unwrapping them with legacyCStringPointer().

namespace WTF {

// fopen() is ISO C, so this one is offered on every platform.
[[nodiscard]] inline FILE* posixFopen(UTF8CStringView path, ASCIILiteral mode)
{
    return fopen(path.utf8(), mode.characters());
}

#if OS(UNIX)

[[nodiscard]] inline int posixOpen(UTF8CStringView path, int flags, mode_t mode = 0)
{
    return open(path.utf8(), flags, mode);
}

inline int posixAccess(UTF8CStringView path, int mode)
{
    return access(path.utf8(), mode);
}

inline int posixMkdir(UTF8CStringView path, mode_t mode)
{
    return mkdir(path.utf8(), mode);
}

inline int posixUnlink(UTF8CStringView path)
{
    return unlink(path.utf8());
}

inline int posixRename(UTF8CStringView oldPath, UTF8CStringView newPath)
{
    return rename(oldPath.utf8(), newPath.utf8());
}

inline int posixChmod(UTF8CStringView path, mode_t mode)
{
    return chmod(path.utf8(), mode);
}

inline int posixSymlink(UTF8CStringView targetPath, UTF8CStringView linkPath)
{
    return symlink(targetPath.utf8(), linkPath.utf8());
}

inline int posixStat(UTF8CStringView path, struct stat* result)
{
    return stat(path.utf8(), result);
}

inline int posixLstat(UTF8CStringView path, struct stat* result)
{
    return lstat(path.utf8(), result);
}

inline int posixStatvfs(UTF8CStringView path, struct statvfs* result)
{
    return statvfs(path.utf8(), result);
}

[[nodiscard]] inline DIR* posixOpendir(UTF8CStringView path)
{
    return opendir(path.utf8());
}

inline char* posixRealpath(UTF8CStringView path, char* resolvedPath)
{
    return realpath(path.utf8(), resolvedPath);
}

#if OS(DARWIN)
inline int posixChflags(UTF8CStringView path, unsigned flags)
{
    return chflags(path.utf8(), flags);
}
#endif

#if OS(LINUX) && HAVE(STATX)
inline int posixStatx(int directoryFileDescriptor, UTF8CStringView path, int flags, unsigned mask, struct statx* result)
{
    return statx(directoryFileDescriptor, path.utf8(), flags, mask, result);
}
#endif

#if !PLATFORM(PLAYSTATION)
[[nodiscard]] inline void* posixDlopen(UTF8CStringView path, int mode)
{
    return dlopen(path.utf8(), mode);
}

[[nodiscard]] inline int posixShmOpen(UTF8CStringView name, int flags, mode_t mode)
{
    return shm_open(name.utf8(), flags, mode);
}

inline int posixShmUnlink(UTF8CStringView name)
{
    return shm_unlink(name.utf8());
}
#endif

#endif // OS(UNIX)

} // namespace WTF

using WTF::posixFopen;

#if OS(UNIX)
using WTF::posixAccess;
using WTF::posixChmod;
using WTF::posixLstat;
using WTF::posixMkdir;
using WTF::posixOpen;
using WTF::posixOpendir;
using WTF::posixRealpath;
using WTF::posixRename;
using WTF::posixStat;
using WTF::posixStatvfs;
using WTF::posixSymlink;
using WTF::posixUnlink;
#if OS(DARWIN)
using WTF::posixChflags;
#endif
#if OS(LINUX) && HAVE(STATX)
using WTF::posixStatx;
#endif
#if !PLATFORM(PLAYSTATION)
using WTF::posixDlopen;
using WTF::posixShmOpen;
using WTF::posixShmUnlink;
#endif
#endif
