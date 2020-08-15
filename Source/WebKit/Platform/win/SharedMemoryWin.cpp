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

#include "Decoder.h"
#include "Encoder.h"
#include <wtf/RefPtr.h>

namespace WebKit {

SharedMemory::Handle::Handle()
    : m_handle(0)
    , m_size(0)
{
}

SharedMemory::Handle::Handle(Handle&& other)
{
    m_handle = std::exchange(other.m_handle, nullptr);
    m_size = std::exchange(other.m_size, 0);
}

auto SharedMemory::Handle::operator=(Handle&& other) -> Handle&
{
    m_handle = std::exchange(other.m_handle, nullptr);
    m_size = std::exchange(other.m_size, 0);
    return *this;
}

SharedMemory::Handle::~Handle()
{
    clear();
}

bool SharedMemory::Handle::isNull() const
{
    return !m_handle;
}

void SharedMemory::IPCHandle::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(handle.m_size);
    encoder << dataSize;
    handle.encodeHandle(encoder, handle.m_handle);

    // Hand off ownership of our HANDLE to the receiving process. It will close it for us.
    // FIXME: If the receiving process crashes before it receives the memory, the memory will be
    // leaked. See <http://webkit.org/b/47502>.
    handle.m_handle = 0;
}

bool SharedMemory::IPCHandle::decode(IPC::Decoder& decoder, IPCHandle& ipcHandle)
{
    ASSERT_ARG(ipcHandle, !ipcHandle.handle.m_handle);
    ASSERT_ARG(ipcHandle, !ipcHandle.handle.m_size);

    SharedMemory::Handle handle;

    uint64_t bufferSize;
    if (!decoder.decode(bufferSize))
        return false;

    uint64_t dataLength;
    if (!decoder.decode(dataLength))
        return false;
    
    if (dataLength != bufferSize)
        return false;
    
    auto processSpecificHandle = handle.decodeHandle(decoder);
    if (!processSpecificHandle)
        return false;

    handle.m_handle = processSpecificHandle.value();
    handle.m_size = bufferSize;
    ipcHandle.handle = WTFMove(handle);
    ipcHandle.dataSize = dataLength;
    return true;
}

void SharedMemory::Handle::encodeHandle(IPC::Encoder& encoder, HANDLE handle)
{
    encoder << reinterpret_cast<uint64_t>(handle);

    // Send along our PID so that the receiving process can duplicate the HANDLE for its own use.
    encoder << static_cast<uint32_t>(::GetCurrentProcessId());
}

void SharedMemory::Handle::clear()
{
    if (!m_handle)
        return;

    ::CloseHandle(m_handle);
    m_handle = 0;
}

static bool getDuplicatedHandle(HANDLE sourceHandle, DWORD sourcePID, HANDLE& duplicatedHandle)
{
    duplicatedHandle = 0;
    if (!sourceHandle)
        return true;

    HANDLE sourceProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, sourcePID);
    if (!sourceProcess)
        return false;

    // Copy the handle into our process and close the handle that the sending process created for us.
    BOOL success = ::DuplicateHandle(sourceProcess, sourceHandle, ::GetCurrentProcess(), &duplicatedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    ASSERT_WITH_MESSAGE(success, "::DuplicateHandle failed with error %lu", ::GetLastError());

    ::CloseHandle(sourceProcess);

    return success;
}

Optional<HANDLE> SharedMemory::Handle::decodeHandle(IPC::Decoder& decoder)
{
    uint64_t sourceHandle;
    if (!decoder.decode(sourceHandle))
        return WTF::nullopt;

    uint32_t sourcePID;
    if (!decoder.decode(sourcePID))
        return WTF::nullopt;

    HANDLE duplicatedHandle;
    if (!getDuplicatedHandle(reinterpret_cast<HANDLE>(sourceHandle), sourcePID, duplicatedHandle))
        return WTF::nullopt;

    return duplicatedHandle;
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    HANDLE handle = ::CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, size, 0);
    if (!handle)
        return nullptr;

    void* baseAddress = ::MapViewOfFileEx(handle, FILE_MAP_ALL_ACCESS, 0, 0, size, nullptr);
    if (!baseAddress) {
        ::CloseHandle(handle);
        return nullptr;
    }

    RefPtr<SharedMemory> memory = adoptRef(new SharedMemory);
    memory->m_size = size;
    memory->m_data = baseAddress;
    memory->m_handle = handle;

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
    RefPtr<SharedMemory> memory = adopt(handle.m_handle, handle.m_size, protection);
    if (!memory)
        return nullptr;

    // The SharedMemory object now owns the HANDLE.
    handle.m_handle = 0;

    return memory;
}

RefPtr<SharedMemory> SharedMemory::adopt(HANDLE handle, size_t size, Protection protection)
{
    if (!handle)
        return nullptr;

    DWORD desiredAccess = accessRights(protection);

    void* baseAddress = ::MapViewOfFile(handle, desiredAccess, 0, 0, size);
    ASSERT_WITH_MESSAGE(baseAddress, "::MapViewOfFile failed with error %lu", ::GetLastError());
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
    ::CloseHandle(m_handle);
}

bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT_ARG(handle, !handle.m_handle);
    ASSERT_ARG(handle, !handle.m_size);

    HANDLE processHandle = ::GetCurrentProcess();

    HANDLE duplicatedHandle;
    if (!::DuplicateHandle(processHandle, m_handle, processHandle, &duplicatedHandle, accessRights(protection), FALSE, 0))
        return false;

    handle.m_handle = duplicatedHandle;
    handle.m_size = m_size;
    return true;
}

unsigned SharedMemory::systemPageSize()
{
    static unsigned pageSize = 0;

    if (!pageSize) {
        SYSTEM_INFO systemInfo;
        ::GetSystemInfo(&systemInfo);
        pageSize = systemInfo.dwPageSize;
    }

    return pageSize;
}


} // namespace WebKit
