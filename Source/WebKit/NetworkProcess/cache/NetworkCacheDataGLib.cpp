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

#include "SharedMemory.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !PLATFORM(WIN)
#include <gio/gfiledescriptorbased.h>
#endif

namespace WebKit {
namespace NetworkCache {

Data::Data(const uint8_t* data, size_t size)
    : m_size(size)
{
    uint8_t* copiedData = static_cast<uint8_t*>(fastMalloc(size));
    memcpy(copiedData, data, size);
    m_buffer = adoptGRef(g_bytes_new_with_free_func(copiedData, size, fastFree, copiedData));
}

Data::Data(GRefPtr<GBytes>&& buffer, FileSystem::PlatformFileHandle fd)
    : m_buffer(WTFMove(buffer))
    , m_fileDescriptor(fd)
    , m_size(m_buffer ? g_bytes_get_size(m_buffer.get()) : 0)
    , m_isMap(m_size && FileSystem::isHandleValid(fd))
{
}

Data Data::empty()
{
    return { adoptGRef(g_bytes_new(nullptr, 0)) };
}

const uint8_t* Data::data() const
{
    return m_buffer ? reinterpret_cast<const uint8_t*>(g_bytes_get_data(m_buffer.get(), nullptr)) : nullptr;
}

bool Data::isNull() const
{
    return !m_buffer;
}

bool Data::apply(const Function<bool(Span<const uint8_t>)>& applier) const
{
    if (!m_size)
        return false;

    gsize length;
    const auto* data = g_bytes_get_data(m_buffer.get(), &length);
    return applier({ reinterpret_cast<const uint8_t*>(data), length });
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
    uint8_t* data = static_cast<uint8_t*>(fastMalloc(size));
    gsize aLength;
    const auto* aData = g_bytes_get_data(a.bytes(), &aLength);
    memcpy(data, aData, aLength);
    gsize bLength;
    const auto* bData = g_bytes_get_data(b.bytes(), &bLength);
    memcpy(data + aLength, bData, bLength);

    return { adoptGRef(g_bytes_new_with_free_func(data, size, fastFree, data)) };
}

struct MapWrapper {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    ~MapWrapper()
    {
        FileSystem::closeFile(fileDescriptor);
    }

    FileSystem::MappedFileData mappedFile;
    FileSystem::PlatformFileHandle fileDescriptor;
};

static void deleteMapWrapper(MapWrapper* wrapper)
{
    delete wrapper;
}

Data Data::adoptMap(FileSystem::MappedFileData&& mappedFile, FileSystem::PlatformFileHandle fd)
{
    size_t size = mappedFile.size();
    const void* map = mappedFile.data();
    ASSERT(map);
    ASSERT(map != MAP_FAILED);
    MapWrapper* wrapper = new MapWrapper { WTFMove(mappedFile), fd };
    return { adoptGRef(g_bytes_new_with_free_func(map, size, reinterpret_cast<GDestroyNotify>(deleteMapWrapper), wrapper)), fd };
}

RefPtr<SharedMemory> Data::tryCreateSharedMemory() const
{
    if (isNull() || !isMap())
        return nullptr;

    int fd = FileSystem::posixFileDescriptor(m_fileDescriptor);
    gsize length;
    const auto* data = g_bytes_get_data(m_buffer.get(), &length);
    return SharedMemory::wrapMap(const_cast<void*>(data), length, fd);
}

} // namespace NetworkCache
} // namespace WebKit

#endif // USE(GLIB)
