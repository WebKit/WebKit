/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/PlatformRegisters.h>

#include <wtf/PtrTag.h>

namespace WTF {

#if USE(PLATFORM_REGISTERS_WITH_PROFILE) && CPU(ARM64E)

#define USE_UNTAGGED_THREAD_STATE_PTR 1

void* threadStateLRInternal(PlatformRegisters& regs)
{
    void* candidateLR = arm_thread_state64_get_lr_fptr(regs);

#if USE(UNTAGGED_THREAD_STATE_PTR)
    if (candidateLR && isTaggedWith(candidateLR, CFunctionPtrTag))
        return retagCodePtr<CFunctionPtrTag, PlatformRegistersLRPtrTag>(candidateLR);
    candidateLR = bitwise_cast<void*>(arm_thread_state64_get_lr(regs));
    if (!candidateLR)
        return candidateLR;
    return tagCodePtr<PlatformRegistersLRPtrTag>(candidateLR);

#else
    return retagCodePtr<CFunctionPtrTag, PlatformRegistersLRPtrTag>(candidateLR);
#endif
}

void* threadStatePCInternal(PlatformRegisters& regs)
{
    void* candidatePC = arm_thread_state64_get_pc_fptr(regs);

#if USE(UNTAGGED_THREAD_STATE_PTR)
    if (candidatePC && isTaggedWith(candidatePC, CFunctionPtrTag))
        return retagCodePtr<CFunctionPtrTag, PlatformRegistersPCPtrTag>(candidatePC);
    candidatePC = bitwise_cast<void*>(arm_thread_state64_get_pc(regs));
    if (!candidatePC)
        return candidatePC;
    return tagCodePtr<PlatformRegistersPCPtrTag>(candidatePC);

#else
    return retagCodePtr<CFunctionPtrTag, PlatformRegistersPCPtrTag>(candidatePC);
#endif
}

#endif // USE(PLATFORM_REGISTERS_WITH_PROFILE) && CPU(ARM64E)

} // namespace WTF
