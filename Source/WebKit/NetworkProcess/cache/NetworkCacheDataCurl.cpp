/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include <WebCore/SharedMemory.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
namespace NetworkCache {

Data::Data(std::span<const uint8_t> data)
    : m_buffer(Box<Variant<Vector<uint8_t>, FileSystem::MappedFileData>>::create(Vector<uint8_t>(data.size())))
{
    memcpySpan(std::get<Vector<uint8_t>>(*m_buffer).mutableSpan(), data);
}

Data::Data(Variant<Vector<uint8_t>, FileSystem::MappedFileData>&& data)
    : m_buffer(Box<Variant<Vector<uint8_t>, FileSystem::MappedFileData>>::create(WTFMove(data)))
    , m_isMap(std::holds_alternative<FileSystem::MappedFileData>(*m_buffer))
{
}

Data Data::empty()
{
    Vector<uint8_t> buffer;
    return { WTFMove(buffer) };
}

std::span<const uint8_t> Data::span() const
{
    if (!m_buffer)
        return { };

    return WTF::switchOn(*m_buffer,
        [](const Vector<uint8_t>& buffer) { return buffer.span(); },
        [](const FileSystem::MappedFileData& mappedFile) { return mappedFile.span(); }
    );
}

size_t Data::size() const
{
    if (!m_buffer)
        return 0;
    return WTF::switchOn(*m_buffer,
        [](const Vector<uint8_t>& buffer) -> size_t { return buffer.size(); },
        [](const FileSystem::MappedFileData& mappedFile) -> size_t { return mappedFile.size(); }
    );
}

bool Data::isNull() const
{
    return !m_buffer;
}

bool Data::apply(NOESCAPE const Function<bool(std::span<const uint8_t>)>& applier) const
{
    if (isEmpty())
        return false;

    return applier(span());
}

Data Data::subrange(size_t offset, size_t size) const
{
    if (!m_buffer)
        return { };

    return span().subspan(offset, size);
}

Data concatenate(const Data& a, const Data& b)
{
    if (a.isNull())
        return b;
    if (b.isNull())
        return a;

    Vector<uint8_t> buffer(a.size() + b.size());
    memcpySpan(buffer.mutableSpan(), a.span());
    memcpySpan(buffer.mutableSpan().subspan(a.size()), b.span());
    return Data(WTFMove(buffer));
}

Data Data::adoptMap(FileSystem::MappedFileData&& mappedFile, FileSystem::FileHandle&& fileHandle)
{
    ASSERT(mappedFile);
    fileHandle = { };

    return { WTFMove(mappedFile) };
}

#if ENABLE(SHAREABLE_RESOURCE) && OS(WINDOWS)
RefPtr<WebCore::SharedMemory> Data::tryCreateSharedMemory() const
{
    if (isNull() || !isMap())
        return nullptr;

    auto newHandle = Win32Handle { std::get<FileSystem::MappedFileData>(*m_buffer).fileMapping() };
    if (!newHandle)
        return nullptr;

    return WebCore::SharedMemory::map({ WTFMove(newHandle), size() }, WebCore::SharedMemory::Protection::ReadOnly);
}
#endif

} // namespace NetworkCache
} // namespace WebKit
