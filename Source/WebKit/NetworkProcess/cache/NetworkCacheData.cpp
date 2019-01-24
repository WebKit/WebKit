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
#include <wtf/FileSystem.h>

#if !OS(WINDOWS)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace WebKit {
namespace NetworkCache {

Data Data::mapToFile(const char* path) const
{
#if !OS(WINDOWS)
    int fd = open(path, O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR);
    if (fd < 0)
        return { };

    if (ftruncate(fd, m_size) < 0) {
        close(fd);
        return { };
    }

    void* map = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return { };
    }

    uint8_t* mapData = static_cast<uint8_t*>(map);
    apply([&mapData](const uint8_t* bytes, size_t bytesSize) {
        memcpy(mapData, bytes, bytesSize);
        mapData += bytesSize;
        return true;
    });

    // Drop the write permission.
    mprotect(map, m_size, PROT_READ);

    // Flush (asynchronously) to file, turning this into clean memory.
    msync(map, m_size, MS_ASYNC);

    return Data::adoptMap(map, m_size, fd);
#else
    return Data();
#endif
}

Data mapFile(const char* path)
{
#if !OS(WINDOWS)
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0)
        return { };
    struct stat stat;
    if (fstat(fd, &stat) < 0) {
        close(fd);
        return { };
    }
    size_t size = stat.st_size;
    if (!size) {
        close(fd);
        return Data::empty();
    }

    return adoptAndMapFile(fd, 0, size);
#else
    return Data();
#endif
}

Data adoptAndMapFile(int fd, size_t offset, size_t size)
{
#if !OS(WINDOWS)
    if (!size) {
        close(fd);
        return Data::empty();
    }

    void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, offset);
    if (map == MAP_FAILED) {
        close(fd);
        return { };
    }

    return Data::adoptMap(map, size, fd);
#else
    return Data();
#endif
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

#if !OS(WINDOWS)
static Salt makeSalt()
{
    Salt salt;
    static_assert(salt.size() == 8, "Salt size");
    *reinterpret_cast<uint32_t*>(&salt[0]) = cryptographicallyRandomNumber();
    *reinterpret_cast<uint32_t*>(&salt[4]) = cryptographicallyRandomNumber();
    return salt;
}
#endif

Optional<Salt> readOrMakeSalt(const String& path)
{
#if !OS(WINDOWS)
    auto cpath = FileSystem::fileSystemRepresentation(path);
    auto fd = open(cpath.data(), O_RDONLY, 0);
    Salt salt;
    auto bytesRead = read(fd, salt.data(), salt.size());
    close(fd);
    if (bytesRead != salt.size()) {
        salt = makeSalt();

        unlink(cpath.data());
        fd = open(cpath.data(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        bool success = write(fd, salt.data(), salt.size()) == salt.size();
        close(fd);
        if (!success)
            return { };
    }
    return salt;
#else
    return Salt();
#endif
}

} // namespace NetworkCache
} // namespace WebKit
