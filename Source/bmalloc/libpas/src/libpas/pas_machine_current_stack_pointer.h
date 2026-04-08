/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

#ifndef PAS_MACHINE_CURRENT_STACK_POINTER_H
#define PAS_MACHINE_CURRENT_STACK_POINTER_H

#include "pas_config.h"

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#if defined(NDEBUG) && (PAS_X86_64 || PAS_ARM64 || PAS_ARM)

// We should only use the inline asm implementation on release builds because it
// needs to be inlinable in order to be correct.
static PAS_ALWAYS_INLINE_FOR_CORRECTNESS void* pas_machine_current_stack_pointer(void)
{
    void* sp = 0;
#if PAS_X86_64
    __asm__ volatile("movq %%rsp, %0" : "=r"(sp) ::);
#elif PAS_ARM64 && defined(__ILP32__)
    uint64_t sp64 = 0;
    __asm__ volatile("mov %0, sp" : "=r"(sp64) ::);
    sp = (void*)(uintptr_t)sp64;
#elif PAS_ARM64 || PAS_ARM
    __asm__ volatile("mov %0, sp" : "=r"(sp) ::);
#endif
    return sp;
}

#else

PAS_API_NEVER_HIDDEN void* PAS_CDECL pas_machine_current_stack_pointer(void);

#endif

PAS_END_EXTERN_C;

#endif /* PAS_MACHINE_CURRENT_STACK_POINTER_H */
