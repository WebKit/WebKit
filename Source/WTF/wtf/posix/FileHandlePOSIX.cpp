/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
#include <wtf/FileHandle.h>

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FileSystem.h>
#include <wtf/MallocSpan.h>
#include <wtf/MappedFileData.h>

namespace WTF::FileSystemImpl {

std::optional<uint64_t> FileHandle::read(std::span<uint8_t> data)
{
    if (!m_handle)
        return { };

    do {
        auto bytesRead = ::read(*m_handle, data.data(), data.size());
        if (bytesRead >= 0)
            return bytesRead;
    } while (errno == EINTR);
    return { };
}

std::optional<uint64_t> FileHandle::write(std::span<const uint8_t> data)
{
    if (!m_handle)
        return { };

    do {
        auto bytesWritten = ::write(*m_handle, data.data(), data.size());
        if (bytesWritten >= 0)
            return bytesWritten;
    } while (errno == EINTR);
    return { };
}

bool FileHandle::truncate(int64_t offset)
{
    // ftruncate returns 0 to indicate the success.
    return m_handle && !ftruncate(*m_handle, offset);
}

bool FileHandle::flush()
{
    return m_handle && !fsync(*m_handle);
}

std::optional<uint64_t> FileHandle::seek(int64_t offset, FileSeekOrigin origin)
{
    if (!m_handle)
        return { };

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
        break;
    }
    auto result = lseek(*m_handle, offset, whence);
    if (result < 0)
        return { };
    return static_cast<uint64_t>(result);
}

std::optional<PlatformFileID> FileHandle::id()
{
    if (!m_handle)
        return std::nullopt;

    struct stat fileInfo;
    if (fstat(*m_handle, &fileInfo))
        return std::nullopt;

    return fileInfo.st_ino;
}

void FileHandle::close()
{
    if (auto handle = std::exchange(m_handle, std::nullopt))
        ::close(*handle);
}

std::optional<uint64_t> FileHandle::size()
{
    if (!m_handle)
        return std::nullopt;

    struct stat fileInfo;
    if (fstat(*m_handle, &fileInfo))
        return std::nullopt;

    return fileInfo.st_size;
}

#if USE(FILE_LOCK)
bool FileHandle::lock(OptionSet<FileLockMode> lockMode)
{
    if (!m_handle)
        return false;

    static_assert(LOCK_SH == WTF::enumToUnderlyingType(FileLockMode::Shared), "LockSharedEncoding is as expected");
    static_assert(LOCK_EX == WTF::enumToUnderlyingType(FileLockMode::Exclusive), "LockExclusiveEncoding is as expected");
    static_assert(LOCK_NB == WTF::enumToUnderlyingType(FileLockMode::Nonblocking), "LockNonblockingEncoding is as expected");

    return flock(*m_handle, lockMode.toRaw()) != -1;
}
#endif // USE(FILE_LOCK)

#if HAVE(MMAP)
std::optional<MappedFileData> FileHandle::map(MappedFileMode mapMode, FileOpenMode openMode)
{
    if (!m_handle)
        return { };

    struct stat fileStat;
    if (fstat(platformHandle(), &fileStat))
        return { };

    size_t size;
    if (!WTF::convertSafely(fileStat.st_size, size))
        return { };

    if (!size)
        return MappedFileData { };

    int pageProtection = PROT_READ;
    switch (openMode) {
    case FileOpenMode::Read:
        pageProtection = PROT_READ;
        break;
    case FileOpenMode::Truncate:
        pageProtection = PROT_WRITE;
        break;
    case FileOpenMode::ReadWrite:
        pageProtection = PROT_READ | PROT_WRITE;
        break;
#if OS(DARWIN)
    case FileOpenMode::EventsOnly:
        ASSERT_NOT_REACHED();
#endif
    }

    auto fileData = MmapSpan<uint8_t>::mmap(nullptr, size, pageProtection, MAP_FILE | (mapMode == MappedFileMode::Shared ? MAP_SHARED : MAP_PRIVATE), platformHandle());
    if (!fileData)
        return { };

    return MappedFileData { WTFMove(fileData) };
}
#endif // HAVE(MMAP)

} // WTF::FileSystemImpl
