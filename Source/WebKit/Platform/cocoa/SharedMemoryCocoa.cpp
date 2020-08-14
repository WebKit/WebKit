/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "MachPort.h"
#include <WebCore/SharedBuffer.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <wtf/MachSendRight.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/spi/cocoa/MachVMSPI.h>

namespace WebKit {

static inline Optional<size_t> safeRoundPage(size_t size)
{
    size_t roundedSize;
    if (__builtin_add_overflow(size, static_cast<size_t>(PAGE_MASK), &roundedSize))
        return WTF::nullopt;
    roundedSize &= ~static_cast<size_t>(PAGE_MASK);
    return roundedSize;
}

SharedMemory::Handle::Handle()
    : m_port(MACH_PORT_NULL)
    , m_size(0)
{
}

SharedMemory::Handle::Handle(Handle&& other)
{
    m_port = std::exchange(other.m_port, MACH_PORT_NULL);
    m_size = std::exchange(other.m_size, 0);
}

auto SharedMemory::Handle::operator=(Handle&& other) -> Handle&
{
    m_port = std::exchange(other.m_port, MACH_PORT_NULL);
    m_size = std::exchange(other.m_size, 0);
    return *this;
}

SharedMemory::Handle::~Handle()
{
    clear();
}

bool SharedMemory::Handle::isNull() const
{
    return !m_port;
}

void SharedMemory::Handle::clear()
{
    if (m_port)
        deallocateSendRightSafely(m_port);

    m_port = MACH_PORT_NULL;
    m_size = 0;
}

void SharedMemory::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_size);
    encoder << IPC::MachPort(m_port, MACH_MSG_TYPE_MOVE_SEND);
    m_port = MACH_PORT_NULL;
}

bool SharedMemory::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    ASSERT(!handle.m_port);
    ASSERT(!handle.m_size);

    uint64_t size;
    if (!decoder.decode(size))
        return false;

    IPC::MachPort machPort;
    if (!decoder.decode(machPort))
        return false;
    
    handle.m_size = size;
    handle.m_port = machPort.port();
    return true;
}

void SharedMemory::IPCHandle::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(handle.m_size);
    encoder << dataSize;
    encoder << IPC::MachPort(handle.m_port, MACH_MSG_TYPE_MOVE_SEND);
    handle.m_port = MACH_PORT_NULL;
}

bool SharedMemory::IPCHandle::decode(IPC::Decoder& decoder, IPCHandle& ipcHandle)
{
    ASSERT(!ipcHandle.handle.m_port);
    ASSERT(!ipcHandle.handle.m_size);

    SharedMemory::Handle handle;

    uint64_t bufferSize;
    if (!decoder.decode(bufferSize))
        return false;

    uint64_t dataLength;
    if (!decoder.decode(dataLength))
        return false;

    // SharedMemory::Handle::size() is rounded up to the nearest page.
    if (dataLength > bufferSize)
        return false;

    IPC::MachPort machPort;
    if (!decoder.decode(machPort))
        return false;
    
    handle.m_size = bufferSize;
    handle.m_port = machPort.port();
    ipcHandle.handle = WTFMove(handle);
    ipcHandle.dataSize = dataLength;
    return true;
}

static inline void* toPointer(mach_vm_address_t address)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

static inline mach_vm_address_t toVMAddress(void* pointer)
{
    return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(pointer));
}
    
RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    ASSERT(size);

    auto roundedSize = safeRoundPage(size);
    if (!roundedSize) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::allocate: Failed to allocate shared memory (%zu bytes) due to overflow", nullptr, size);
        return nullptr;
    }

    mach_vm_address_t address = 0;
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &address, *roundedSize, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
#if RELEASE_LOG_DISABLED
        LOG_ERROR("Failed to allocate mach_vm_allocate shared memory (%zu bytes). %s (%x)", size, mach_error_string(kr), kr);
#else
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::allocate: Failed to allocate mach_vm_allocate shared memory (%zu bytes). %{public}s (%x)", nullptr, size, mach_error_string(kr), kr);
#endif
        return nullptr;
    }

    auto sharedMemory = adoptRef(*new SharedMemory);
    sharedMemory->m_size = size;
    sharedMemory->m_data = toPointer(address);
    sharedMemory->m_port = MACH_PORT_NULL;
    sharedMemory->m_protection = Protection::ReadWrite;

    return WTFMove(sharedMemory);
}

static inline vm_prot_t machProtection(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        return VM_PROT_READ;
    case SharedMemory::Protection::ReadWrite:
        return VM_PROT_READ | VM_PROT_WRITE;
    }

    ASSERT_NOT_REACHED();
    return VM_PROT_NONE;
}

static WTF::MachSendRight makeMemoryEntry(size_t size, vm_offset_t offset, SharedMemory::Protection protection, mach_port_t parentEntry)
{
    auto roundedSize = safeRoundPage(size);
    if (!roundedSize) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::makeMemoryEntry: Failed to create a mach port for shared memory (%zu bytes) due to overflow", nullptr, size);
        return { };
    }

    memory_object_size_t memoryObjectSize = *roundedSize;

    mach_port_t port = MACH_PORT_NULL;
    kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_VM_SHARE, &port, parentEntry);
    if (kr != KERN_SUCCESS) {
#if RELEASE_LOG_DISABLED
        LOG_ERROR("Failed to create a mach port for shared memory. %s (%x)", mach_error_string(kr), kr);
#else
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::makeMemoryEntry: Failed to create a mach port for shared memory. %{public}s (%x)", nullptr, mach_error_string(kr), kr);
#endif
        return { };
    }

    RELEASE_ASSERT(memoryObjectSize >= size);

    return WTF::MachSendRight::adopt(port);
}

RefPtr<SharedMemory> SharedMemory::wrapMap(void* data, size_t size, Protection protection)
{
    ASSERT(size);

    auto sendRight = makeMemoryEntry(size, toVMAddress(data), protection, MACH_PORT_NULL);
    if (!sendRight)
        return nullptr;

    auto sharedMemory(adoptRef(*new SharedMemory));
    sharedMemory->m_size = size;
    sharedMemory->m_data = nullptr;
    sharedMemory->m_port = sendRight.leakSendRight();
    sharedMemory->m_protection = protection;

    return WTFMove(sharedMemory);
}

RefPtr<SharedMemory> SharedMemory::map(const Handle& handle, Protection protection)
{
    if (handle.isNull())
        return nullptr;

    vm_prot_t vmProtection = machProtection(protection);
    mach_vm_address_t mappedAddress = 0;
    kern_return_t kr = mach_vm_map(mach_task_self(), &mappedAddress, handle.m_size, 0, VM_FLAGS_ANYWHERE, handle.m_port, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
#if RELEASE_LOG_DISABLED
    if (kr != KERN_SUCCESS)
        return nullptr;
#else
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::map: Failed to map shared memory. %{public}s (%x)", nullptr, mach_error_string(kr), kr);
        return nullptr;
    }
#endif

    auto sharedMemory(adoptRef(*new SharedMemory));
    sharedMemory->m_size = handle.m_size;
    sharedMemory->m_data = toPointer(mappedAddress);
    sharedMemory->m_port = MACH_PORT_NULL;
    sharedMemory->m_protection = protection;

    return WTFMove(sharedMemory);
}

SharedMemory::~SharedMemory()
{
    if (m_data) {
        auto roundedSize = safeRoundPage(m_size);
        if (!roundedSize) {
            RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::~SharedMemory: Failed to deallocate memory (%zu bytes) due to overflow", this, m_size);
            RELEASE_ASSERT_NOT_REACHED();
        }

        kern_return_t kr = mach_vm_deallocate(mach_task_self(), toVMAddress(m_data), *roundedSize);
#if RELEASE_LOG_DISABLED
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
#else
        if (kr != KERN_SUCCESS) {
            RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::~SharedMemory: Failed to deallocate shared memory. %{public}s (%x)", this, mach_error_string(kr), kr);
            ASSERT_NOT_REACHED();
        }
#endif
    }

    if (m_port) {
        kern_return_t kr = mach_port_deallocate(mach_task_self(), m_port);
#if RELEASE_LOG_DISABLED
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
#else
        if (kr != KERN_SUCCESS) {
            RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::~SharedMemory: Failed to deallocate port. %{public}s (%x)", this, mach_error_string(kr), kr);
            ASSERT_NOT_REACHED();
        }
#endif
    }        
}
    
bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT(!handle.m_port);
    ASSERT(!handle.m_size);

    auto roundedSize = safeRoundPage(m_size);
    if (!roundedSize) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::createHandle: Failed to create handle (%zu bytes) due to overflow", this, m_size);
        return false;
    }

    auto sendRight = createSendRight(protection);
    if (!sendRight)
        return false;

    handle.m_port = sendRight.leakSendRight();
    handle.m_size = *roundedSize;

    return true;
}

unsigned SharedMemory::systemPageSize()
{
    return vm_page_size;
}

WTF::MachSendRight SharedMemory::createSendRight(Protection protection) const
{
    ASSERT(m_protection == protection || m_protection == Protection::ReadWrite && protection == Protection::ReadOnly);
    ASSERT(!!m_data ^ !!m_port);

    if (m_port && m_protection == protection)
        return WTF::MachSendRight::create(m_port);

    ASSERT(m_data);
    return makeMemoryEntry(m_size, toVMAddress(m_data), protection, MACH_PORT_NULL);
}

} // namespace WebKit
