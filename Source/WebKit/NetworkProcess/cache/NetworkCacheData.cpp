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
#include "NetworkCacheData.h"

#include <fcntl.h>
#include <wtf/CryptographicallyRandomNumber.h>

#if !OS(WINDOWS)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace WebKit {
namespace NetworkCache {

Data Data::mapToFile(const String& path) const
{
    constexpr bool failIfFileExists = true;
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite, FileSystem::FileAccessPermission::User, failIfFileExists);
    if (!FileSystem::isHandleValid(handle) || !FileSystem::truncateFile(handle, m_size)) {
        FileSystem::closeFile(handle);
        return { };
    }
    
    FileSystem::makeSafeToUseMemoryMapForPath(path);
    bool success;
    FileSystem::MappedFileData mappedFile(handle, FileSystem::FileOpenMode::ReadWrite, FileSystem::MappedFileMode::Shared, success);
    if (!success) {
        FileSystem::closeFile(handle);
        return { };
    }

    void* map = const_cast<void*>(mappedFile.data());
    uint8_t* mapData = static_cast<uint8_t*>(map);
    apply([&mapData](const uint8_t* bytes, size_t bytesSize) {
        memcpy(mapData, bytes, bytesSize);
        mapData += bytesSize;
        return true;
    });

#if OS(WINDOWS)
    DWORD oldProtection;
    VirtualProtect(map, m_size, FILE_MAP_READ, &oldProtection);
    FlushViewOfFile(map, m_size);
#else
    // Drop the write permission.
    mprotect(map, m_size, PROT_READ);

    // Flush (asynchronously) to file, turning this into clean memory.
    msync(map, m_size, MS_ASYNC);
#endif

    return Data::adoptMap(WTFMove(mappedFile), handle);
}

Data mapFile(const char* path)
{
    auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(file))
        return { };
    long long size;
    if (!FileSystem::getFileSize(file, size)) {
        FileSystem::closeFile(file);
        return { };
    }
    return adoptAndMapFile(file, 0, size);
}

Data mapFile(const String& path)
{
    return mapFile(FileSystem::fileSystemRepresentation(path).data());
}

Data adoptAndMapFile(FileSystem::PlatformFileHandle handle, size_t offset, size_t size)
{
    if (!size) {
        FileSystem::closeFile(handle);
        return Data::empty();
    }
    bool success;
    FileSystem::MappedFileData mappedFile(handle, FileSystem::FileOpenMode::Read, FileSystem::MappedFileMode::Private, success);
    if (!success) {
        FileSystem::closeFile(handle);
        return { };
    }

    return Data::adoptMap(WTFMove(mappedFile), handle);
}

SHA1::Digest computeSHA1(const Data& data, const Salt& salt)
{
    SHA1 sha1;
    sha1.addBytes(salt.data(), salt.size());
    data.apply([&sha1](const uint8_t* data, size_t size) {
        sha1.addBytes(data, size);
        return true;
    });

    SHA1::Digest digest;
    sha1.computeHash(digest);
    return digest;
}

bool bytesEqual(const Data& a, const Data& b)
{
    if (a.isNull() || b.isNull())
        return false;
    if (a.size() != b.size())
        return false;
    return !memcmp(a.data(), b.data(), a.size());
}

static Salt makeSalt()
{
    Salt salt;
    static_assert(salt.size() == 8, "Salt size");
    *reinterpret_cast<uint32_t*>(&salt[0]) = cryptographicallyRandomNumber();
    *reinterpret_cast<uint32_t*>(&salt[4]) = cryptographicallyRandomNumber();
    return salt;
}

Optional<Salt> readOrMakeSalt(const String& path)
{
    if (FileSystem::fileExists(path)) {
        auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
        Salt salt;
        auto bytesRead = static_cast<std::size_t>(FileSystem::readFromFile(file, reinterpret_cast<char*>(salt.data()), salt.size()));
        FileSystem::closeFile(file);
        if (bytesRead == salt.size())
            return salt;

        FileSystem::deleteFile(path);
    }

    Salt salt = makeSalt();
    auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Write, FileSystem::FileAccessPermission::User);
    bool success = static_cast<std::size_t>(FileSystem::writeToFile(file, reinterpret_cast<char*>(salt.data()), salt.size())) == salt.size();
    FileSystem::closeFile(file);
    if (!success)
        return { };

    return salt;
}

} // namespace NetworkCache
} // namespace WebKit
