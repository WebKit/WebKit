/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
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

#pragma once

#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <mach/thread_act.h>
#include <signal.h>
#elif OS(WINDOWS)
#include <windows.h>
#else
#include <ucontext.h>
#endif

namespace WTF {

#if OS(DARWIN)

#if CPU(X86)
typedef i386_thread_state_t PlatformRegisters;
#elif CPU(X86_64)
typedef x86_thread_state64_t PlatformRegisters;
#elif CPU(PPC)
typedef ppc_thread_state_t PlatformRegisters;
#elif CPU(PPC64)
typedef ppc_thread_state64_t PlatformRegisters;
#elif CPU(ARM)
typedef arm_thread_state_t PlatformRegisters;
#elif CPU(ARM64)
typedef arm_thread_state64_t PlatformRegisters;
#else
#error Unknown Architecture
#endif

inline PlatformRegisters& registersFromUContext(ucontext_t* ucontext)
{
    return ucontext->uc_mcontext->__ss;
}

#elif OS(WINDOWS)

using PlatformRegisters = CONTEXT;

#elif HAVE(MACHINE_CONTEXT)

struct PlatformRegisters {
    mcontext_t machineContext;
};

inline PlatformRegisters& registersFromUContext(ucontext_t* ucontext)
{
#if CPU(PPC)
    return *bitwise_cast<PlatformRegisters*>(ucontext->uc_mcontext.uc_regs);
#else
    return *bitwise_cast<PlatformRegisters*>(&ucontext->uc_mcontext);
#endif
}

#else

struct PlatformRegisters {
    void* stackPointer;
};

#endif

} // namespace WTF

using WTF::PlatformRegisters;
