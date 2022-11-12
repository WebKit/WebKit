/*
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
#if USE(UNIX_DOMAIN_SOCKETS)
#include "SharedMemory.h"

#include "WebCoreArgumentCoders.h"
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
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

#if HAVE(LINUX_MEMFD_H)
#include <linux/memfd.h>
#include <sys/syscall.h>
#endif

namespace WebKit {

void SharedMemory::Handle::clear()
{
    *this = { };
}

bool SharedMemory::Handle::isNull() const
{
    return !m_handle;
}

void SharedMemory::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << m_size << WTFMove(m_handle);
}

bool SharedMemory::Handle::decode(IPC::Decoder& decoder, SharedMemory::Handle& handle)
{
    ASSERT_ARG(handle, handle.isNull());
    size_t size;
    if (!decoder.decode(size))
        return false;

    auto fd = decoder.decode<UnixFileDescriptor>();
    if (UNLIKELY(!decoder.isValid()))
        return false;

    handle.m_size = size;
    handle.m_handle = WTFMove(*fd);
    return true;
}

UnixFileDescriptor SharedMemory::Handle::releaseHandle()
{
    return WTFMove(m_handle);
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

static UnixFileDescriptor createSharedMemory()
{
    int fileDescriptor = -1;

#if HAVE(LINUX_MEMFD_H)
    static bool isMemFdAvailable = true;
    if (isMemFdAvailable) {
        do {
            fileDescriptor = syscall(__NR_memfd_create, "WebKitSharedMemory", MFD_CLOEXEC);
        } while (fileDescriptor == -1 && errno == EINTR);

        if (fileDescriptor != -1)
            return UnixFileDescriptor { fileDescriptor, UnixFileDescriptor::Adopt };

        if (errno != ENOSYS)
            return { };

        isMemFdAvailable = false;
    }
#endif

#if HAVE(SHM_ANON)
    do {
        fileDescriptor = shm_open(SHM_ANON, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    } while (fileDescriptor == -1 && errno == EINTR);
#else
    CString tempName;
    for (int tries = 0; fileDescriptor == -1 && tries < 10; ++tries) {
        auto name = makeString("/WK2SharedMemory.", cryptographicallyRandomNumber<unsigned>());
        tempName = name.utf8();

        do {
            fileDescriptor = shm_open(tempName.data(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        } while (fileDescriptor == -1 && errno == EINTR);
    }

    if (fileDescriptor != -1)
        shm_unlink(tempName.data());
#endif

    return UnixFileDescriptor { fileDescriptor, UnixFileDescriptor::Adopt };
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    auto fileDescriptor = createSharedMemory();
    if (!fileDescriptor) {
        WTFLogAlways("Failed to create shared memory: %s", safeStrerror(errno).data());
        return nullptr;
    }

    while (ftruncate(fileDescriptor.value(), size) == -1) {
        if (errno != EINTR)
            return nullptr;
    }

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor.value(), 0);
    if (data == MAP_FAILED)
        return nullptr;

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_fileDescriptor = WTFMove(fileDescriptor);
    instance->m_size = size;
    return instance;
}

RefPtr<SharedMemory> SharedMemory::map(const Handle& handle, Protection protection)
{
    ASSERT(!handle.isNull());
    void* data = mmap(0, handle.size(), accessModeMMap(protection), MAP_SHARED, handle.m_handle.value(), 0);
    handle.m_handle = { };
    if (data == MAP_FAILED)
        return nullptr;

    RefPtr<SharedMemory> instance = wrapMap(data, handle.size(), -1);
    instance->m_isWrappingMap = false;
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

auto SharedMemory::createHandle(Protection) -> std::optional<Handle>
{
    Handle handle;
    ASSERT_ARG(handle, handle.isNull());
    ASSERT(m_fileDescriptor);

    // FIXME: Handle the case where the passed Protection is ReadOnly.
    // See https://bugs.webkit.org/show_bug.cgi?id=131542.

    UnixFileDescriptor duplicate { m_fileDescriptor.value(), UnixFileDescriptor::Duplicate };
    if (!duplicate) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    handle.m_handle = WTFMove(duplicate);
    handle.m_size = m_size;
    return { WTFMove(handle) };
}

} // namespace WebKit

#endif

