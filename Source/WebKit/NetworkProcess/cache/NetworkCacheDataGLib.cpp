/*
 * Copyright (C) 2015 Igalia S.L
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

#if USE(GLIB)

#include <WebCore/SharedMemory.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !PLATFORM(WIN)
#include <gio/gfiledescriptorbased.h>
#endif

#include <wtf/MallocSpan.h>
#include <wtf/glib/GSpanExtras.h>

namespace WebKit {
namespace NetworkCache {

Data::Data(std::span<const uint8_t> data)
{
    auto span = MallocSpan<uint8_t>::malloc(data.size());
    memcpySpan(span.mutableSpan(), data);
    auto* copiedData = span.leakSpan().data();
    m_buffer = adoptGRef(g_bytes_new_with_free_func(copiedData, data.size(), fastFree, copiedData));
}

Data::Data(GRefPtr<GBytes>&& buffer, FileSystem::FileHandle&& fileHandle)
    : m_buffer(WTF::move(buffer))
    , m_fileHandle(Box<FileSystem::FileHandle>::create(WTF::move(fileHandle)))
    , m_isMap(m_buffer && g_bytes_get_size(m_buffer.get()) && m_fileHandle->isValid())
{
}

Data Data::empty()
{
    return { adoptGRef(g_bytes_new(nullptr, 0)) };
}

std::span<const uint8_t> Data::span() const
{
    if (!m_buffer)
        return { };
    return WTF::span(m_buffer);
}

size_t Data::size() const
{
    return m_buffer ? g_bytes_get_size(m_buffer.get()) : 0;
}

bool Data::isNull() const
{
    return !m_buffer;
}

bool Data::apply(NOESCAPE const Function<bool(std::span<const uint8_t>)>& applier) const
{
    if (!size())
        return false;

    return applier(span());
}

Data Data::subrange(size_t offset, size_t size) const
{
    if (!m_buffer)
        return { };

    return { adoptGRef(g_bytes_new_from_bytes(m_buffer.get(), offset, size)) };
}

Data concatenate(const Data& a, const Data& b)
{
    if (a.isNull())
        return b;
    if (b.isNull())
        return a;

    size_t size = a.size() + b.size();
    auto span = MallocSpan<uint8_t>::malloc(size);
    memcpySpan(span.mutableSpan(), a.span());
    memcpySpan(span.mutableSpan().last(b.size()), b.span());
    auto* data = span.leakSpan().data();

    return { adoptGRef(g_bytes_new_with_free_func(data, size, fastFree, data)) };
}

struct MapWrapper {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(MapWrapper);

    FileSystem::MappedFileData mappedFile;
};

static void deleteMapWrapper(MapWrapper* wrapper)
{
    delete wrapper;
}

Data Data::adoptMap(FileSystem::MappedFileData&& mappedFile, FileSystem::FileHandle&& fileHandle)
{
    size_t size = mappedFile.size();
    auto* map = mappedFile.span().data();
    ASSERT(map);
    ASSERT(map != MAP_FAILED);
    MapWrapper* wrapper = new MapWrapper { WTF::move(mappedFile) };
    return { adoptGRef(g_bytes_new_with_free_func(map, size, reinterpret_cast<GDestroyNotify>(deleteMapWrapper), wrapper)), WTF::move(fileHandle) };
}

RefPtr<WebCore::SharedMemory> Data::tryCreateSharedMemory() const
{
    if (isNull() || !isMap())
        return nullptr;

    gsize length;
    const auto* data = g_bytes_get_data(m_buffer.get(), &length);
    return WebCore::SharedMemory::wrapMap(const_cast<void*>(data), length, m_fileHandle->platformHandle());
}

} // namespace NetworkCache
} // namespace WebKit

#endif // USE(GLIB)
