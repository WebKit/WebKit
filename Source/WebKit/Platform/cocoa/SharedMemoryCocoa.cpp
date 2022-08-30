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

#include "ArgumentCoders.h"
#include "Logging.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/ProcessIdentity.h>
#include <WebCore/SharedBuffer.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <wtf/MachSendRight.h>
#include <wtf/RefPtr.h>
#include <wtf/spi/cocoa/MachVMSPI.h>

#if HAVE(MACH_MEMORY_ENTRY)
#include <mach/memory_entry.h>
#endif

namespace WebKit {

#if HAVE(MACH_MEMORY_ENTRY) && !ENABLE(XPC_IPC)
static int toVMMemoryLedger(MemoryLedger memoryLedger)
{
    switch (memoryLedger) {
    case MemoryLedger::None:
        return VM_LEDGER_TAG_NONE;
    case MemoryLedger::Default:
        return VM_LEDGER_TAG_DEFAULT;
    case MemoryLedger::Network:
        return VM_LEDGER_TAG_NETWORK;
    case MemoryLedger::Media:
        return VM_LEDGER_TAG_MEDIA;
    case MemoryLedger::Graphics:
        return VM_LEDGER_TAG_GRAPHICS;
    case MemoryLedger::Neural:
        return VM_LEDGER_TAG_NEURAL;
    }
}
#endif

static inline std::optional<size_t> safeRoundPage(size_t size)
{
    size_t roundedSize;
    if (__builtin_add_overflow(size, static_cast<size_t>(PAGE_MASK), &roundedSize))
        return std::nullopt;
    roundedSize &= ~static_cast<size_t>(PAGE_MASK);
    return roundedSize;
}

SharedMemory::Handle::Handle()
    : m_size(0)
{
}

SharedMemory::Handle::Handle(Handle&& other)
{
#if ENABLE(XPC_IPC)
    m_xpcObject = WTFMove(other.m_xpcObject);
#else
    m_port = std::exchange(other.m_port, MACH_PORT_NULL);
#endif
    m_size = std::exchange(other.m_size, 0);
}

auto SharedMemory::Handle::operator=(Handle&& other) -> Handle&
{
#if ENABLE(XPC_IPC)
    m_xpcObject = WTFMove(other.m_xpcObject);
#else
    m_port = std::exchange(other.m_port, MACH_PORT_NULL);
#endif
    m_size = std::exchange(other.m_size, 0);
    return *this;
}

SharedMemory::Handle::~Handle()
{
    clear();
}

void SharedMemory::Handle::takeOwnershipOfMemory(MemoryLedger memoryLedger) const
{
#if HAVE(MACH_MEMORY_ENTRY) && !ENABLE(XPC_IPC)
    if (!m_port)
        return;

    kern_return_t kr = mach_memory_entry_ownership(m_port, mach_task_self(), toVMMemoryLedger(memoryLedger), 0);

    if (kr != KERN_SUCCESS)
        RELEASE_LOG_ERROR(VirtualMemory, "SharedMemory::Handle::takeOwnershipOfMemory: Failed ownership of shared memory. Error: %{public}s (%x)", mach_error_string(kr), kr);
#else
    UNUSED_PARAM(memoryLedger);
#endif
}

void SharedMemory::Handle::setOwnershipOfMemory(const WebCore::ProcessIdentity& processIdentity, MemoryLedger memoryLedger) const
{
#if HAVE(TASK_IDENTITY_TOKEN) && HAVE(MACH_MEMORY_ENTRY_OWNERSHIP_IDENTITY_TOKEN_SUPPORT) && !ENABLE(XPC_IPC)
    if (!m_port)
        return;

    kern_return_t kr = mach_memory_entry_ownership(m_port, processIdentity.taskIdToken(), toVMMemoryLedger(memoryLedger), 0);

    if (kr != KERN_SUCCESS)
        RELEASE_LOG_ERROR(VirtualMemory, "SharedMemory::Handle::setOwnershipOfMemory: Failed ownership of shared memory. Error: %{public}s (%x)", mach_error_string(kr), kr);
#else
    UNUSED_PARAM(memoryLedger);
    UNUSED_PARAM(processIdentity);
#endif
}

bool SharedMemory::Handle::isNull() const
{
#if ENABLE(XPC_IPC)
    return !m_xpcObject;
#else
    return !m_port;
#endif
}

void SharedMemory::Handle::clear()
{
#if ENABLE(XPC_IPC)
    // FIXME: Release memory?
    m_xpcObject = nullptr;
#else
    if (m_port)
        deallocateSendRightSafely(m_port);

    m_port = MACH_PORT_NULL;
#endif
    m_size = 0;
}

void SharedMemory::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_size);
#if ENABLE(XPC_IPC)
    encoder << IPC::Attachment(XPCObject { m_xpcObject.get() });
#else
    encoder << MachSendRight::adopt(m_port);
    m_port = MACH_PORT_NULL;
#endif
}

bool SharedMemory::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
#if ENABLE(XPC_IPC)
    ASSERT(!handle.m_xpcObject);
#else
    ASSERT(!handle.m_port);
#endif
    ASSERT(!handle.m_size);
    uint64_t bufferSize;
    if (!decoder.decode(bufferSize))
        return false;

    auto roundedSize = safeRoundPage(bufferSize);
    if (UNLIKELY(!roundedSize))
        return false;

#if ENABLE(XPC_IPC)
    IPC::Attachment attachment;
    if (!decoder.decode(attachment))
        return false;
    handle.m_xpcObject = attachment.xpcObject();
#else
    auto sendRight = decoder.decode<MachSendRight>();
    handle.m_port = sendRight->leakSendRight();
#endif
    if (UNLIKELY(!decoder.isValid()))
        return false;
    
    handle.m_size = bufferSize;
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

#if ENABLE(XPC_IPC)
    void* result = mmap(0, *roundedSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, 0, 0);
    if (result == MAP_FAILED) {
#if RELEASE_LOG_DISABLED
        LOG_ERROR("Failed to allocate shared memory (%zu bytes), errno = %x", size, errno);
#else
        RELEASE_LOG_ERROR(VirtualMemory, "SharedMemory::allocate: Failed to allocate shared memory (%zu bytes), errno = %x", size, errno);
#endif
        return nullptr;
    }
#else
    mach_vm_address_t address = 0;
    // Using VM_FLAGS_PURGABLE so that we can later transfer ownership of the memory via mach_memory_entry_ownership().
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &address, *roundedSize, VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE);
    if (kr != KERN_SUCCESS) {
#if RELEASE_LOG_DISABLED
        LOG_ERROR("Failed to allocate mach_vm_allocate shared memory (%zu bytes). %s (%x)", size, mach_error_string(kr), kr);
#else
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::allocate: Failed to allocate mach_vm_allocate shared memory (%zu bytes). %{public}s (%x)", nullptr, size, mach_error_string(kr), kr);
#endif
        return nullptr;
    }
#endif

    auto sharedMemory = adoptRef(*new SharedMemory);
    sharedMemory->m_size = size;
#if ENABLE(XPC_IPC)
    sharedMemory->m_data = result;
#else
    sharedMemory->m_data = toPointer(address);
    sharedMemory->m_port = MACH_PORT_NULL;
#endif
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

#if !ENABLE(XPC_IPC)
static WTF::MachSendRight makeMemoryEntry(size_t size, vm_offset_t offset, SharedMemory::Protection protection, mach_port_t parentEntry)
{
    auto roundedSize = safeRoundPage(size);
    if (!roundedSize) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::makeMemoryEntry: Failed to create a mach port for shared memory (%zu bytes) due to overflow", nullptr, size);
        return { };
    }

    memory_object_size_t memoryObjectSize = *roundedSize;
    mach_port_t port = MACH_PORT_NULL;

#if HAVE(MEMORY_ATTRIBUTION_VM_SHARE_SUPPORT)
    kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_VM_SHARE, &port, parentEntry);
    if (kr != KERN_SUCCESS) {
#if RELEASE_LOG_DISABLED
        LOG_ERROR("Failed to create a mach port for shared memory. %s (%x)", mach_error_string(kr), kr);
#else
        RELEASE_LOG_ERROR(VirtualMemory, "SharedMemory::makeMemoryEntry: Failed to create a mach port for shared memory. Error: %{public}s (%x)", mach_error_string(kr), kr);
#endif
        return { };
    }
#else
    // First try without MAP_MEM_VM_SHARE because it prevents memory ownership transfer. We only pass the MAP_MEM_VM_SHARE flag as a fallback.
    kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK, &port, parentEntry);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG(VirtualMemory, "SharedMemory::makeMemoryEntry: Failed to create a mach port for shared memory, will try again with MAP_MEM_VM_SHARE flag. Error: %{public}s (%x)", mach_error_string(kr), kr);
        kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_VM_SHARE, &port, parentEntry);
        if (kr != KERN_SUCCESS) {
#if RELEASE_LOG_DISABLED
            LOG_ERROR("Failed to create a mach port for shared memory. %s (%x)", mach_error_string(kr), kr);
#else
            RELEASE_LOG_ERROR(VirtualMemory, "SharedMemory::makeMemoryEntry: Failed to create a mach port for shared memory with MAP_MEM_VM_SHARE flag. Error: %{public}s (%x)", mach_error_string(kr), kr);
#endif
            return { };
        }
    }
#endif // HAVE(MEMORY_ATTRIBUTION_VM_SHARE_SUPPORT)

    RELEASE_ASSERT(memoryObjectSize >= size);

    return WTF::MachSendRight::adopt(port);
}
#endif

RefPtr<SharedMemory> SharedMemory::wrapMap(void* data, size_t size, Protection protection)
{
    ASSERT(size);

#if ENABLE(XPC_IPC)
    auto xpcObject = xpc_shmem_create(data, size);
    RELEASE_ASSERT(xpcObject);
    if (!xpcObject)
        return nullptr;
#else
    auto sendRight = makeMemoryEntry(size, toVMAddress(data), protection, MACH_PORT_NULL);
    if (!sendRight)
        return nullptr;
#endif

    auto sharedMemory(adoptRef(*new SharedMemory));
    sharedMemory->m_size = size;
    sharedMemory->m_data = nullptr;
#if ENABLE(XPC_IPC)
    sharedMemory->m_xpcObject = xpcObject;
#else
    sharedMemory->m_port = sendRight.leakSendRight();
#endif
    sharedMemory->m_protection = protection;

    return WTFMove(sharedMemory);
}

RefPtr<SharedMemory> SharedMemory::map(const Handle& handle, Protection protection)
{
    if (handle.isNull())
        return nullptr;
    auto roundedSize = safeRoundPage(handle.m_size);
    if (UNLIKELY(!roundedSize)) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::map: Failed to map %zu bytes due to overflow", nullptr, handle.m_size);
        return nullptr;
    }

#if ENABLE(XPC_IPC)
    void* address = nullptr;
    auto size = xpc_shmem_map(handle.m_xpcObject.get(), &address);
    RELEASE_ASSERT(address);
    RELEASE_ASSERT(size >= handle.m_size);
    if (!size) {
#if RELEASE_LOG_DISABLED
        LOG_ERROR("SharedMemory::map: Failed to map shared memory.");
#else
        RELEASE_LOG_ERROR(VirtualMemory, "SharedMemory::map: Failed to map shared memory.");
#endif
        return nullptr;
    }
#else
    vm_prot_t vmProtection = machProtection(protection);
    mach_vm_address_t mappedAddress = 0;
    kern_return_t kr = mach_vm_map(mach_task_self(), &mappedAddress, *roundedSize, 0, VM_FLAGS_ANYWHERE, handle.m_port, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
#if RELEASE_LOG_DISABLED
    if (kr != KERN_SUCCESS)
        return nullptr;
#else
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::map: Failed to map shared memory. %{public}s (%x)", nullptr, mach_error_string(kr), kr);
        return nullptr;
    }
#endif
#endif

    auto sharedMemory(adoptRef(*new SharedMemory));
    sharedMemory->m_size = handle.m_size;
#if ENABLE(XPC_IPC)
    sharedMemory->m_data = address;
#else
    sharedMemory->m_data = toPointer(mappedAddress);
    sharedMemory->m_port = MACH_PORT_NULL;
#endif
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
    
#if ENABLE(XPC_IPC)
    // FIXME:
#else
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
#endif
}
    
bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
#if ENABLE(XPC_IPC)
    ASSERT(!handle.m_xpcObject);
#else
    ASSERT(!handle.m_port);
#endif
    ASSERT(!handle.m_size);

    auto roundedSize = safeRoundPage(m_size);
    if (!roundedSize) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::createHandle: Failed to create handle (%zu bytes) due to overflow", this, m_size);
        return false;
    }

#if ENABLE(XPC_IPC)
    RELEASE_ASSERT(!!m_data ^ !!m_xpcObject);
    if (m_data) {
        auto xpcObject = xpc_shmem_create(m_data, *roundedSize);
        RELEASE_ASSERT(xpcObject);
        if (!xpcObject)
            return false;
        handle.m_xpcObject = xpcObject;
    } else {
        if (!m_xpcObject)
            return false;
        handle.m_xpcObject = m_xpcObject;
    }
#else
    auto sendRight = createSendRight(protection);
    if (!sendRight)
        return false;

    handle.m_port = sendRight.leakSendRight();
#endif
    handle.m_size = m_size;

    return true;
}

#if !ENABLE(XPC_IPC)
WTF::MachSendRight SharedMemory::createSendRight(Protection protection) const
{
    ASSERT(m_protection == protection || m_protection == Protection::ReadWrite && protection == Protection::ReadOnly);
    ASSERT(!!m_data ^ !!m_port);

    if (m_port && m_protection == protection)
        return WTF::MachSendRight::create(m_port);

    ASSERT(m_data);
    return makeMemoryEntry(m_size, toVMAddress(m_data), protection, MACH_PORT_NULL);
}
#endif

} // namespace WebKit
