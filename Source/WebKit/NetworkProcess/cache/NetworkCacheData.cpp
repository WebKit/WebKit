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
#include <wtf/StdLibExtras.h>

#if !OS(WINDOWS)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace WebKit {
namespace NetworkCache {

#if !USE(GLIB) && USE(CURL)
Data::Data(Vector<uint8_t>&& data)
    : Data(Variant<Vector<uint8_t>, FileSystem::MappedFileData> { WTFMove(data) })
{
}
#elif !PLATFORM(COCOA)
Data::Data(Vector<uint8_t>&& data)
    : Data(data.span())
{
}
#endif

Data Data::mapToFile(const String& path) const
{
    FileSystem::FileHandle handle;
    auto applyData = [&](NOESCAPE const Function<bool(std::span<const uint8_t>)>& applier) {
        apply(applier);
    };
    auto mappedFile = FileSystem::mapToFile(path, size(), WTFMove(applyData), &handle);
    if (!mappedFile)
        return { };
    return Data::adoptMap(WTFMove(mappedFile), WTFMove(handle));
}

Data mapFile(const String& path)
{
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    if (!handle)
        return { };

    auto size = handle.size();
    if (!size)
        return { };

    if (!*size)
        return Data::empty();

    auto mappedFile = handle.map(FileSystem::MappedFileMode::Private);
    if (!mappedFile)
        return { };

    return Data::adoptMap(WTFMove(*mappedFile), WTFMove(handle));
}

SHA1::Digest computeSHA1(const Data& data, const Salt& salt)
{
    SHA1 sha1;
    sha1.addBytes(salt);
    data.apply([&sha1](std::span<const uint8_t> span) {
        sha1.addBytes(span);
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
    return equalSpans(a.span(), b.span());
}

} // namespace NetworkCache
} // namespace WebKit
