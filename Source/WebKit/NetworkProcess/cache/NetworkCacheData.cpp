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
#include <wtf/FileSystem.h>

#if !OS(WINDOWS)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace WebKit {
namespace NetworkCache {

Data Data::mapToFile(const String& path) const
{
    FileSystem::PlatformFileHandle handle;
    auto applyData = [&](const Function<bool(Span<const uint8_t>)>& applier) {
        apply(applier);
    };
    auto mappedFile = FileSystem::mapToFile(path, size(), WTFMove(applyData), &handle);
    if (!mappedFile)
        return { };
    return Data::adoptMap(WTFMove(mappedFile), handle);
}

Data mapFile(const String& path)
{
    auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(file))
        return { };
    auto size = FileSystem::fileSize(file);
    if (!size) {
        FileSystem::closeFile(file);
        return { };
    }
    return adoptAndMapFile(file, 0, *size);
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
    data.apply([&sha1](Span<const uint8_t> span) {
        sha1.addBytes(span.data(), span.size());
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

} // namespace NetworkCache
} // namespace WebKit
