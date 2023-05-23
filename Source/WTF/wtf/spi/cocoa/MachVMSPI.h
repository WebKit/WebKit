/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
#include <mach/mach_vm.h>
#endif

WTF_EXTERN_C_BEGIN

kern_return_t mach_vm_allocate(vm_map_t target, mach_vm_address_t*, mach_vm_size_t, int flags);
kern_return_t mach_vm_deallocate(vm_map_t target, mach_vm_address_t, mach_vm_size_t);
kern_return_t mach_vm_map(vm_map_t targetTask, mach_vm_address_t*, mach_vm_size_t, mach_vm_offset_t mask, int flags,
    mem_entry_name_port_t, memory_object_offset_t, boolean_t copy, vm_prot_t currentProtection, vm_prot_t maximumProtection, vm_inherit_t);
kern_return_t mach_vm_protect(vm_map_t targetTask, mach_vm_address_t, mach_vm_size_t, boolean_t setMaximum, vm_prot_t newProtection);
kern_return_t mach_vm_region(vm_map_t targetTask, mach_vm_address_t*, mach_vm_size_t*, vm_region_flavor_t, vm_region_info_t,
    mach_msg_type_number_t* infoCount, mach_port_t* objectName);
kern_return_t mach_vm_region_recurse(vm_map_t targetTask, mach_vm_address_t*, mach_vm_size_t*, uint32_t* depth, vm_region_recurse_info_t, mach_msg_type_number_t* infoCount);
kern_return_t mach_vm_purgable_control(vm_map_t target, mach_vm_address_t, vm_purgable_t control, int* state);

WTF_EXTERN_C_END
