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

#ifndef PAS_MACHINE_REGISTERS_H
#define PAS_MACHINE_REGISTERS_H

#include "pas_config.h"

#include "pas_platform.h"
#include "pas_utils.h"

// Keep this in sync with WTF's HAVE(MACHINE_CONTEXT) in wtf/PlatformHave.h.
#if PAS_OS(DARWIN) || PAS_OS(FUCHSIA) || \
    ((PAS_OS(FREEBSD) || PAS_OS(HAIKU) || PAS_OS(NETBSD) || PAS_OS(OPENBSD) || \
      PAS_OS(LINUX) || PAS_OS(HURD) || PAS_OS(QNX)) && \
     (PAS_X86_64 || PAS_ARM || PAS_RISCV))
#define PAS_HAVE_MACHINE_CONTEXT 1
#endif

#if PAS_OS(DARWIN)
#include <mach/exception_types.h>
#include <mach/thread_act.h>
#include <signal.h>
#elif PAS_OS(WINDOWS)
#include <windows.h>
#elif PAS_OS(QNX)
#include <ucontext.h>
#else
#if PAS_OS(OPENBSD)
#include <sys/signal.h>
#endif
#include <sys/ucontext.h>
#endif

PAS_BEGIN_EXTERN_C;

#if PAS_OS(DARWIN)

#if PAS_X86
typedef i386_thread_state_t PlatformRegisters;
#elif PAS_X86_64
typedef x86_thread_state64_t PlatformRegisters;
#elif PAS_ARM32
typedef arm_thread_state_t PlatformRegisters;
#elif PAS_ARM64
typedef arm_thread_state64_t PlatformRegisters;
#else
#error Unknown Architecture
#endif

struct pas_machine_registers {
    PlatformRegisters platformRegisters;
};
typedef struct pas_machine_registers pas_machine_registers;

static PAS_ALWAYS_INLINE pas_machine_registers* pas_registers_from_ucontext(ucontext_t* ucontext)
{
    return (pas_machine_registers*)&ucontext->uc_mcontext->__ss;
}

#elif PAS_OS(WINDOWS)

typedef CONTEXT PlatformRegisters;

struct pas_machine_registers {
    PlatformRegisters platformRegisters;
};
typedef struct pas_machine_registers pas_machine_registers;

#elif PAS_HAVE(MACHINE_CONTEXT)

struct pas_machine_registers {
    mcontext_t machineContext;
};
typedef struct pas_machine_registers pas_machine_registers;

static PAS_ALWAYS_INLINE pas_machine_registers* pas_registers_from_ucontext(ucontext_t* ucontext)
{
#if PAS_OS(OPENBSD)
    return (pas_machine_registers*)ucontext;
#else
    return (pas_machine_registers*)&ucontext->uc_mcontext;
#endif
}

#else

struct pas_machine_registers {
    void* stackPointer;
};
typedef struct pas_machine_registers pas_machine_registers;

#endif /* PAS_OS(DARWIN) */

struct pas_machine_stack_bounds {
    void* origin;
    void* bound;
};
typedef struct pas_machine_stack_bounds pas_machine_stack_bounds;

static PAS_ALWAYS_INLINE bool pas_machine_stack_bounds_contains(pas_machine_stack_bounds bounds, void* pointer)
{
    return (uintptr_t)pointer >= (uintptr_t)bounds.bound && (uintptr_t)pointer <= (uintptr_t)bounds.origin;
}

PAS_END_EXTERN_C;

#endif /* PAS_MACHINE_REGISTERS_H */
