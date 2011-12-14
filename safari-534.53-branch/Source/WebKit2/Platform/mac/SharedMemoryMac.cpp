/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "Arguments.h"
#include "MachPort.h"
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <wtf/RefPtr.h>

namespace WebKit {

SharedMemory::Handle::Handle()
    : m_port(MACH_PORT_NULL)
    , m_size(0)
{
}

SharedMemory::Handle::~Handle()
{
    if (m_port)
        mach_port_deallocate(mach_task_self(), m_port);
}

bool SharedMemory::Handle::isNull() const
{
    return !m_port;
}

void SharedMemory::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeUInt64(m_size);
    encoder->encode(CoreIPC::MachPort(m_port, MACH_MSG_TYPE_MOVE_SEND));
    m_port = MACH_PORT_NULL;
}

bool SharedMemory::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    ASSERT(!handle.m_port);
    ASSERT(!handle.m_size);

    uint64_t size;
    if (!decoder->decodeUInt64(size))
        return false;

    CoreIPC::MachPort machPort;
    if (!decoder->decode(CoreIPC::Out(machPort)))
        return false;
    
    handle.m_size = size;
    handle.m_port = machPort.port();
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
    
PassRefPtr<SharedMemory> SharedMemory::create(size_t size)
{
    ASSERT(size);

    mach_vm_address_t address;
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &address, round_page(size), VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        LOG_ERROR("Failed to allocate mach_vm_allocate shared memory (%zu bytes) [error code: %x]", size, kr); 
        return 0;
    }

    // Create a Mach port that represents the shared memory.
    mach_port_t port;
    memory_object_size_t memoryObjectSize = round_page(size);
    kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, address, VM_PROT_DEFAULT, &port, MACH_PORT_NULL);

    if (kr != KERN_SUCCESS) {
        LOG_ERROR("Failed to create a mach port for shared memory [error code: %x]", kr);
        mach_vm_deallocate(mach_task_self(), address, round_page(size));
        return 0;
    }

    ASSERT(memoryObjectSize >= round_page(size));

    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_size = size;
    sharedMemory->m_data = toPointer(address);
    sharedMemory->m_port = port;

    return sharedMemory.release();
}

static inline vm_prot_t machProtection(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::ReadOnly:
        return VM_PROT_READ;
    case SharedMemory::ReadWrite:
        return VM_PROT_READ | VM_PROT_WRITE;
    }

    ASSERT_NOT_REACHED();
    return VM_PROT_NONE;    
}

PassRefPtr<SharedMemory> SharedMemory::create(const Handle& handle, Protection protection)
{
    if (handle.isNull())
        return 0;
    
    // Map the memory.
    vm_prot_t vmProtection = machProtection(protection);
    mach_vm_address_t mappedAddress = 0;
    kern_return_t kr = mach_vm_map(mach_task_self(), &mappedAddress, handle.m_size, 0, VM_FLAGS_ANYWHERE, handle.m_port, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS)
        return 0;

    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_size = handle.m_size;
    sharedMemory->m_data = toPointer(mappedAddress);
    sharedMemory->m_port = MACH_PORT_NULL;

    return sharedMemory.release();
}

SharedMemory::~SharedMemory()
{
    if (m_data) {
        kern_return_t kr = mach_vm_deallocate(mach_task_self(), toVMAddress(m_data), round_page(m_size));
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    }

    if (m_port) {
        kern_return_t kr = mach_port_deallocate(mach_task_self(), m_port);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    }        
}
    
bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT(!handle.m_port);
    ASSERT(!handle.m_size);

    mach_vm_address_t address = toVMAddress(m_data);
    memory_object_size_t size = round_page(m_size);

    mach_port_t port;

    if (protection == ReadWrite && m_port) {
        // Just re-use the port we have.
        port = m_port;
        if (mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS)
            return false;
    } else {
        // Create a mach port that represents the shared memory.
        kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &size, address, machProtection(protection), &port, MACH_PORT_NULL);
        if (kr != KERN_SUCCESS)
            return false;

        ASSERT(size >= round_page(m_size));
    }

    handle.m_port = port;
    handle.m_size = size;

    return true;
}

unsigned SharedMemory::systemPageSize()
{
    return vm_page_size;
}

} // namespace WebKit
