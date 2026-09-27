/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// The Mach VM SPI this library needs on top of what the rest of WebKit uses, kept
// here so that what libJavaScriptCoreTools adds stays separate and distinct from
// wtf/spi/cocoa/MachVMSPI.h.

DECLARE_SYSTEM_HEADER

#include <wtf/spi/cocoa/MachVMSPI.h>

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

WTF_EXTERN_C_BEGIN

kern_return_t mach_vm_remap_new(vm_map_t targetTask, mach_vm_address_t*, mach_vm_size_t, mach_vm_offset_t mask, int flags,
    vm_map_read_t srcTask, mach_vm_address_t srcAddress, boolean_t copy, vm_prot_t* currentProtection, vm_prot_t* maximumProtection, vm_inherit_t);

WTF_EXTERN_C_END

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
