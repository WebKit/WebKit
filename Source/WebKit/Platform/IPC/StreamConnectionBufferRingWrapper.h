/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <cstdint>
#include <mach/arm/vm_types.h>
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#include <wtf/Noncopyable.h>

_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wunsafe-buffer-usage\"")

class RingBuffer {
    WTF_MAKE_NONCOPYABLE(RingBuffer);
    void* m_ring_buffer_ptr { nullptr };
    mach_port_t m_memory_entry { MACH_PORT_NULL };
    size_t m_size { 0 };

public:
    bool isValid() const { return m_ring_buffer_ptr; }

    RingBuffer(RingBuffer&& other)
        : m_ring_buffer_ptr(std::exchange(other.m_ring_buffer_ptr, nullptr))
        , m_memory_entry(std::exchange(other.m_memory_entry, MACH_PORT_NULL))
        , m_size(std::exchange(other.m_size, 0))
    { }

    RingBuffer& operator= (RingBuffer&& rhs)
    {
        std::swap(m_ring_buffer_ptr, rhs.m_ring_buffer_ptr);
        std::swap(m_memory_entry, rhs.m_memory_entry);
        std::swap(m_size, rhs.m_size);
        return *this;
    }

    RingBuffer() { }
    RingBuffer(std::span<uint8_t> buffer)
    {
        uintptr_t addr = (uintptr_t) buffer.data();
        RELEASE_ASSERT(!(addr % vm_page_size));
        RELEASE_ASSERT(!(buffer.size() % vm_page_size));
        m_size = buffer.size();
        kern_return_t kr;

        // Create a memory entry for the existing buffer:
        memory_object_size_t entrySize = m_size;
        kr = mach_make_memory_entry_64(
            mach_task_self(),
            &entrySize,
            (mach_vm_address_t)buffer.data(),
            VM_PROT_READ | VM_PROT_WRITE,
            &m_memory_entry,
            MACH_PORT_NULL
        );

        RELEASE_ASSERT(kr == KERN_SUCCESS);

        mach_vm_address_t target = 0;
        kr = mach_vm_allocate(
            mach_task_self(),
            &target,
            2*m_size,
            VM_FLAGS_ANYWHERE
        );

        RELEASE_ASSERT(kr == KERN_SUCCESS);

        // Commented out:
        // mach_vm_deallocate(mach_task_self(), target, 2 * m_size);

        mach_vm_address_t firstHalf = target;
        mach_vm_address_t secondHalf = target + m_size;

        kr = mach_vm_map(
            mach_task_self(),
            &firstHalf,
            m_size,
            0,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
            m_memory_entry,
            0,
            FALSE,
            VM_PROT_READ | VM_PROT_WRITE,
            VM_PROT_READ | VM_PROT_WRITE,
            VM_INHERIT_NONE
        );
        RELEASE_ASSERT(kr == KERN_SUCCESS);

        kr = mach_vm_map(
            mach_task_self(),
            &secondHalf,
            m_size,
            0,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
            m_memory_entry,
            0,
            FALSE,
            VM_PROT_READ | VM_PROT_WRITE,
            VM_PROT_READ | VM_PROT_WRITE,
            VM_INHERIT_NONE
        );
        RELEASE_ASSERT(kr == KERN_SUCCESS);

        m_ring_buffer_ptr = (void*)target;
    }

    ~RingBuffer()
    {
        if (m_ring_buffer_ptr)
            mach_vm_deallocate(mach_task_self(), (mach_vm_address_t) m_ring_buffer_ptr, 2 * m_size);

        if (m_memory_entry != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), m_memory_entry);
    }

    std::span<uint8_t> getSpan(size_t offset, size_t length)
    {
        RELEASE_ASSERT(offset < m_size);
        RELEASE_ASSERT(length <= m_size);
        return std::span<uint8_t>(static_cast<uint8_t*>(m_ring_buffer_ptr) + offset, length);
    }
};


_Pragma("clang diagnostic pop")
