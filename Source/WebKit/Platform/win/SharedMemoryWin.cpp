/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "ArgumentCoders.h"
#include <wtf/RefPtr.h>

namespace WebKit {

bool SharedMemory::Handle::isNull() const
{
    return !m_handle;
}

void SharedMemory::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_size);
    // Hand off ownership of our HANDLE to the receiving process. It will close it for us.
    // FIXME: If the receiving process crashes before it receives the memory, the memory will be
    // leaked. See <http://webkit.org/b/47502>.
    encoder << WTFMove(m_handle);
}

bool SharedMemory::Handle::decode(IPC::Decoder& decoder, SharedMemory::Handle& handle)
{
    ASSERT_ARG(handle, !handle.m_handle);
    ASSERT_ARG(handle, !handle.m_size);

    uint64_t dataLength;
    if (!decoder.decode(dataLength))
        return false;
    
    auto processSpecificHandle = decoder.decode<Win32Handle>();
    if (!processSpecificHandle)
        return false;

    handle.m_handle = WTFMove(*processSpecificHandle);
    handle.m_size = dataLength;
    return true;
}


void SharedMemory::Handle::clear()
{
    m_handle = { };
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    Win32Handle handle { ::CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, size, 0) };
    if (!handle)
        return nullptr;

    void* baseAddress = ::MapViewOfFileEx(handle.get(), FILE_MAP_ALL_ACCESS, 0, 0, size, nullptr);
    if (!baseAddress)
        return nullptr;

    RefPtr<SharedMemory> memory = adoptRef(new SharedMemory);
    memory->m_size = size;
    memory->m_data = baseAddress;
    memory->m_handle = WTFMove(handle);

    return memory;
}

static DWORD accessRights(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        return FILE_MAP_READ;
    case SharedMemory::Protection::ReadWrite:
        return FILE_MAP_READ | FILE_MAP_WRITE;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

RefPtr<SharedMemory> SharedMemory::map(const Handle& handle, Protection protection)
{
    RefPtr<SharedMemory> memory = adopt(handle.m_handle.get(), handle.m_size, protection);
    if (!memory)
        return nullptr;

    // The SharedMemory object now owns the HANDLE.
    (void) handle.m_handle.release();

    return memory;
}

RefPtr<SharedMemory> SharedMemory::adopt(HANDLE handle, size_t size, Protection protection)
{
    if (handle == INVALID_HANDLE_VALUE)
        return nullptr;

    DWORD desiredAccess = accessRights(protection);

    void* baseAddress = ::MapViewOfFile(handle, desiredAccess, 0, 0, size);
    ASSERT_WITH_MESSAGE(baseAddress, "::MapViewOfFile failed with error %lu %p", ::GetLastError(), handle);
    if (!baseAddress)
        return nullptr;

    RefPtr<SharedMemory> memory = adoptRef(new SharedMemory);
    memory->m_size = size;
    memory->m_data = baseAddress;
    memory->m_handle = handle;

    return memory;
}

SharedMemory::~SharedMemory()
{
    ASSERT(m_data);
    ASSERT(m_handle);

    ::UnmapViewOfFile(m_data);
}

auto SharedMemory::createHandle(Protection protection) -> std::optional<Handle>
{
    Handle handle;
    ASSERT_ARG(handle, !handle.m_handle);
    ASSERT_ARG(handle, !handle.m_size);

    HANDLE processHandle = ::GetCurrentProcess();

    HANDLE duplicatedHandle;
    if (!::DuplicateHandle(processHandle, m_handle.get(), processHandle, &duplicatedHandle, accessRights(protection), FALSE, 0))
        return std::nullopt;

    handle.m_handle = Win32Handle { duplicatedHandle };
    handle.m_size = m_size;
    return WTFMove(handle);
}

} // namespace WebKit
