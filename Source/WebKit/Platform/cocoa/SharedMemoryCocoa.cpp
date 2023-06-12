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

#if HAVE(MACH_MEMORY_ENTRY)
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

static inline void* toPointer(mach_vm_address_t address)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

static inline mach_vm_address_t toVMAddress(void* pointer)
{
    return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(pointer));
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

static WTF::MachSendRight makeMemoryEntry(void* data, size_t size, SharedMemory::Protection protection)
{
    constexpr mach_port_t parentEntry = MACH_PORT_NULL;
#if HAVE(MEMORY_ATTRIBUTION_VM_SHARE_SUPPORT)
    constexpr bool vmShareSupportsOwnerAttribution = true;
#else
    constexpr bool vmShareSupportsOwnerAttribution = false;
#endif
    const memory_object_offset_t offset = reinterpret_cast<uintptr_t>(data);

    if constexpr(!vmShareSupportsOwnerAttribution) {
        // First try without MAP_MEM_VM_SHARE because it prevents memory ownership transfer. We only pass the MAP_MEM_VM_SHARE flag as a fallback.
        memory_object_size_t memoryObjectSize = size;
        mach_port_t port = MACH_PORT_NULL;
        kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_USE_DATA_ADDR, &port, parentEntry);
        if (kr != KERN_SUCCESS) {
            RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for shared memory. Error: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
            return { };
        }
        auto result = WTF::MachSendRight::adopt(port);
        // Not passing MAP_MEM_VM_SHARE makes the entry use the existing memory object. If the address range spans multiple memory objects, we would need to
        // use multiple handles. Current WebKit SHM implementation does not support this. Instead, use MAP_MEM_VM_SHARE and lose the memory attribution.
        if (memoryObjectSize >= size)
            return result;
    }
    memory_object_size_t memoryObjectSize = size;
    mach_port_t port = MACH_PORT_NULL;
    kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_VM_SHARE | MAP_MEM_USE_DATA_ADDR, &port, parentEntry);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for shared memory. Error: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        return { };
    }
    auto result = WTF::MachSendRight::adopt(port);
    if (memoryObjectSize < size) {
        RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for shared memory. Unexpected memory object size with MAP_MEM_VM_SHARE: %lld < %zu at %llx", memoryObjectSize, size, offset);
        ASSERT_WITH_MESSAGE(memoryObjectSize >= size, "Unexpected memory object size with MAP_MEM_VM_SHARE: %lld < %zu at %llx", memoryObjectSize, size, offset);
        return { };
    }
    return result;
}

void SharedMemoryHandle::takeOwnershipOfMemory(MemoryLedger memoryLedger) const
{
#if HAVE(MACH_MEMORY_ENTRY)
    if (!m_handle)
        return;

    kern_return_t kr = mach_memory_entry_ownership(m_handle.sendRight(), mach_task_self(), toVMMemoryLedger(memoryLedger), 0);
    RELEASE_LOG_ERROR_IF(kr != KERN_SUCCESS, VirtualMemory, "SharedMemoryHandle::takeOwnershipOfMemory: Failed ownership of shared memory. Error: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
#else
    UNUSED_PARAM(memoryLedger);
#endif
}

void SharedMemoryHandle::setOwnershipOfMemory(const WebCore::ProcessIdentity& processIdentity, MemoryLedger memoryLedger) const
{
#if HAVE(TASK_IDENTITY_TOKEN) && HAVE(MACH_MEMORY_ENTRY_OWNERSHIP_IDENTITY_TOKEN_SUPPORT)
    if (!m_handle)
        return;

    kern_return_t kr = mach_memory_entry_ownership(m_handle.sendRight(), processIdentity.taskIdToken(), toVMMemoryLedger(memoryLedger), 0);
    RELEASE_LOG_ERROR_IF(kr != KERN_SUCCESS, VirtualMemory, "SharedMemoryHandle::setOwnershipOfMemory: Failed ownership of shared memory. Error: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
#else
    UNUSED_PARAM(memoryLedger);
    UNUSED_PARAM(processIdentity);
#endif
}

std::optional<SharedMemoryHandle> SharedMemoryHandle::create(std::span<uint8_t> data, SharedMemoryProtection protection)
{
    // Creating a handle to an existing memory range implies that the ownership is never transferred.
    memory_object_size_t memoryObjectSize = data.size();
    mach_port_t port = MACH_PORT_NULL;
    const memory_object_offset_t offset = reinterpret_cast<uintptr_t>(data.data());
    kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_VM_SHARE | MAP_MEM_USE_DATA_ADDR, &port, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for shared memory. Error: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        return std::nullopt;
    }
    auto result = WTF::MachSendRight::adopt(port);
    if (memoryObjectSize < data.size()) {
        RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for shared memory. Unexpected memory object size with MAP_MEM_VM_SHARE: %lld < %zu at %llx", memoryObjectSize, data.size(), offset);
        ASSERT_WITH_MESSAGE(memoryObjectSize >= data.size(), "Unexpected memory object size with MAP_MEM_VM_SHARE: %lld < %zu at %llx", memoryObjectSize, data.size(), offset);
        return std::nullopt;
    }
    if (!result)
        return std::nullopt;
    return std::optional<SharedMemoryHandle> { std::in_place, WTFMove(result), data.size() };
}

std::optional<SharedMemoryHandle> SharedMemoryHandle::createCopy(std::span<const uint8_t> data, SharedMemoryProtection protection)
{
    memory_object_size_t memoryObjectSize = data.size();
    mach_port_t port = MACH_PORT_NULL;
    const memory_object_offset_t offset = reinterpret_cast<uintptr_t>(data.data());
    kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, offset, machProtection(protection) | VM_PROT_IS_MASK | MAP_MEM_VM_COPY | MAP_MEM_USE_DATA_ADDR, &port, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for copy. Error: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        return std::nullopt;
    }
    auto result = WTF::MachSendRight::adopt(port);
    if (memoryObjectSize < data.size()) {
        RELEASE_LOG_ERROR(VirtualMemory, "Failed to create memory entry for copy.");
        ASSERT_WITH_MESSAGE(memoryObjectSize >= data.size(), "Unexpected memory object size with copy: %lld < %zu at %llx", memoryObjectSize, data.size(), offset);
        return std::nullopt;
    }
    return std::optional<SharedMemoryHandle> { std::in_place, WTFMove(result), data.size() };
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    ASSERT(size);

    mach_vm_address_t address = 0;
    // Using VM_FLAGS_PURGABLE so that we can later transfer ownership of the memory via mach_memory_entry_ownership().
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::allocate: Failed to allocate mach_vm_allocate shared memory (%zu bytes). %" PUBLIC_LOG_STRING " (%x)", nullptr, size, mach_error_string(kr), kr);
        return nullptr;
    }

    auto sharedMemory = adoptRef(*new SharedMemory);
    sharedMemory->m_size = size;
    sharedMemory->m_data = toPointer(address);
    sharedMemory->m_protection = Protection::ReadWrite;
    return WTFMove(sharedMemory);
}

RefPtr<SharedMemory> SharedMemory::wrapMap(void* data, size_t size, Protection protection)
{
    ASSERT(size);
    auto sendRight = makeMemoryEntry(data, size, protection);
    if (!sendRight)
        return nullptr;

    auto sharedMemory(adoptRef(*new SharedMemory));
    sharedMemory->m_size = size;
    sharedMemory->m_data = nullptr;
    sharedMemory->m_sendRight = WTFMove(sendRight);
    sharedMemory->m_protection = protection;

    return WTFMove(sharedMemory);
}

RefPtr<SharedMemory> SharedMemory::map(Handle&& handle, Protection protection)
{
    if (handle.isNull())
        return nullptr;

    vm_prot_t vmProtection = machProtection(protection);
    mach_vm_address_t mappedAddress = 0;

    kern_return_t kr = mach_vm_map(mach_task_self(), &mappedAddress, handle.m_size, 0, VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR, handle.m_handle.sendRight(), 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::map: Failed to map shared memory. %" PUBLIC_LOG_STRING " (%x)", nullptr, mach_error_string(kr), kr);
        return nullptr;
    }

    auto sharedMemory(adoptRef(*new SharedMemory));
    sharedMemory->m_size = handle.m_size;
    sharedMemory->m_data = toPointer(mappedAddress);
    sharedMemory->m_protection = protection;

    return WTFMove(sharedMemory);
}

SharedMemory::~SharedMemory()
{
    if (m_data) {
        kern_return_t kr = mach_vm_deallocate(mach_task_self(), toVMAddress(m_data), m_size);
        if (kr != KERN_SUCCESS) {
            RELEASE_LOG_ERROR(VirtualMemory, "%p - SharedMemory::~SharedMemory: Failed to deallocate shared memory. %" PUBLIC_LOG_STRING " (%x)", this, mach_error_string(kr), kr);
            ASSERT_NOT_REACHED();
        }
    }
}

auto SharedMemory::createHandle(Protection protection) -> std::optional<Handle>
{
    ASSERT(m_protection == protection || m_protection == Protection::ReadWrite && protection == Protection::ReadOnly);
    ASSERT(!!m_data ^ !!m_sendRight);
    if (m_sendRight && m_protection == protection)
        return std::optional<Handle> { std::in_place, MachSendRight { m_sendRight }, m_size };
    auto sendRight = makeMemoryEntry(m_data, m_size, protection);
    if (!sendRight)
        return std::nullopt;
    return std::optional<Handle> { std::in_place, WTFMove(sendRight), m_size };
}

} // namespace WebKit
