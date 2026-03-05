/*
 * Copyright (C) 2025 Igalia S.L.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 University of Szeged
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "SharedMemory.h"

#if OS(ANDROID)

#include <android/sharedmem.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/Assertions.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/SafeStrerror.h>
#include <wtf/UniStdExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

SharedMemoryHandle::SharedMemoryHandle(const SharedMemoryHandle& handle)
{
    m_handle = handle.m_handle.duplicate();
    m_size = handle.m_size;
}

UnixFileDescriptor SharedMemoryHandle::releaseHandle()
{
    return WTF::move(m_handle);
}

static inline int accessModeMMap(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        return PROT_READ;
    case SharedMemory::Protection::ReadWrite:
        return PROT_READ | PROT_WRITE;
    }

    ASSERT_NOT_REACHED();
    return PROT_READ | PROT_WRITE;
}

static UnixFileDescriptor createSharedMemory(size_t size)
{
    const auto name = makeString("/WK2SharedMemory."_s, cryptographicallyRandomNumber<unsigned>());
    int fileDescriptor = ASharedMemory_create(name.utf8().data(), size);
    return UnixFileDescriptor { fileDescriptor, UnixFileDescriptor::Adopt };
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    auto fileDescriptor = createSharedMemory(size);
    if (!fileDescriptor) {
        WTFLogAlways("Failed to create shared memory: %s", safeStrerror(errno).data());
        return nullptr;
    }

    if (ASharedMemory_setProt(fileDescriptor.value(), PROT_READ | PROT_WRITE) == -1) {
        WTFLogAlways("Failed to set ASharedMemory protection: %s", safeStrerror(errno).data());
        return nullptr;
    }

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor.value(), 0);
    if (data == MAP_FAILED)
        return nullptr;

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_fileDescriptor = WTF::move(fileDescriptor);
    instance->m_size = size;
    return instance;
}

RefPtr<SharedMemory> SharedMemory::map(Handle&& handle, Protection protection, CopyOnWrite copyOnWrite)
{
    // Copy-on-write is not supported on this platform.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305633
    ASSERT_UNUSED(copyOnWrite, copyOnWrite == CopyOnWrite::No);

    void* data = mmap(0, handle.size(), accessModeMMap(protection), MAP_SHARED, handle.m_handle.value(), 0);
    if (data == MAP_FAILED)
        return nullptr;

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_size = handle.size();
    return instance;
}

RefPtr<SharedMemory> SharedMemory::wrapMap(void* data, size_t size, int fileDescriptor)
{
    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_size = size;
    instance->m_fileDescriptor = UnixFileDescriptor { fileDescriptor, UnixFileDescriptor::Adopt };
    instance->m_isWrappingMap = true;
    return instance;
}

SharedMemory::~SharedMemory()
{
    if (m_isWrappingMap) {
        auto wrapped = m_fileDescriptor.release();
        UNUSED_VARIABLE(wrapped);
        return;
    }

    munmap(m_data, m_size);
}

auto SharedMemory::createHandle([[maybe_unused]] Protection protection) -> std::optional<Handle>
{
    UnixFileDescriptor duplicate { m_fileDescriptor.value(), UnixFileDescriptor::Duplicate };
    if (!duplicate) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    int protectionMode;
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        protectionMode = PROT_READ;
        break;
    case SharedMemory::Protection::ReadWrite:
        protectionMode = PROT_WRITE;
        break;
    }

    if (ASharedMemory_setProt(duplicate.value(), protectionMode) == -1) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return { Handle(WTF::move(duplicate), m_size) };
}

} // namespace WebCore

#endif // OS(ANDROID)
