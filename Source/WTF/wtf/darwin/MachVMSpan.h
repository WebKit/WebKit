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

#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <wtf/AllocSpanMixin.h>
#include <wtf/Expected.h>
#include <wtf/spi/cocoa/MachVMSPI.h>

namespace WTF {

// RAII class for allocating and holding mach virtual memory with std::span access.
template<typename T> class MachVMSpan : public AllocSpanMixin<T> {
public:
    static Expected<MachVMSpan, kern_return_t> allocate(mach_vm_size_t size, int flags)
    {
        mach_vm_address_t address = 0;
        kern_return_t kr = mach_vm_allocate(mach_task_self(), &address, size, flags);
        if (kr != KERN_SUCCESS)
            return makeUnexpected(kr);
        return MachVMSpan { toPointer(address), size };
    }

    static Expected<MachVMSpan, kern_return_t> map(mach_vm_size_t size, mach_vm_offset_t mask, int flags, mem_entry_name_port_t object, memory_object_offset_t offset, boolean_t copy, vm_prot_t curProtection, vm_prot_t maxProtection, vm_inherit_t inheritance)
    {
        mach_vm_address_t address = 0;
        kern_return_t kr = mach_vm_map(mach_task_self(), &address, size, mask, flags, object, offset, copy, curProtection, maxProtection, inheritance);
        if (kr != KERN_SUCCESS)
            return makeUnexpected(kr);
        return MachVMSpan { toPointer(address), size };
    }

    MachVMSpan() = default;
    MachVMSpan(MachVMSpan&& other)
        : AllocSpanMixin<T>(WTFMove(other))
    {
    }

    template<typename U>
    MachVMSpan(MachVMSpan<U>&& other) requires (std::is_same_v<T, uint8_t>)
        : AllocSpanMixin<T>(asWritableBytes(other.leakSpan()))
    {
    }

    ~MachVMSpan()
    {
        auto data = this->mutableSpan();
        if (!data.data())
            return;
        auto kr = mach_vm_deallocate(mach_task_self(), toVMAddress(data.data()), data.size());
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    }

    MachVMSpan& operator=(MachVMSpan&& other)
    {
        MachVMSpan ptr { WTFMove(other) };
        this->swap(ptr);
        return *this;
    }

    mach_vm_address_t toVMAddress() const { return toVMAddress(this->span().data()); }
private:
    using AllocSpanMixin<T>::AllocSpanMixin;
    static mach_vm_address_t toVMAddress(void* pointer) { return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(pointer)); }
    static T* toPointer(mach_vm_address_t address) { return reinterpret_cast<T*>(static_cast<uintptr_t>(address)); }
};

} // namespace WTF

using WTF::MachVMSpan;
