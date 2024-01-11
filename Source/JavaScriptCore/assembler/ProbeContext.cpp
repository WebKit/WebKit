/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "ProbeContext.h"

#if ENABLE(ASSEMBLER)

#include <wtf/TZoneMallocInlines.h>

namespace JSC {
namespace Probe {

static void flushDirtyStackPages(State*);

WTF_MAKE_TZONE_ALLOCATED_IMPL(Context);

void executeJSCJITProbe(State* state)
{
    Context context(state);
#if CPU(ARM64)
    auto& cpu = context.cpu;
    void* originalLR = cpu.gpr<void*>(ARM64Registers::lr);
    void* originalPC = cpu.pc();
#endif

    state->initializeStackFunction = nullptr;
    state->initializeStackArg = nullptr;
    state->probeFunction(context);

#if CPU(ARM64)
    // The ARM64 probe trampoline does not support changing both lr and pc.
    RELEASE_ASSERT(originalPC == cpu.pc() || originalLR == cpu.gpr<void*>(ARM64Registers::lr));
#endif

    if (context.hasWritesToFlush()) {
        context.stack().setSavedStackPointer(state->cpu.sp());
        void* lowWatermark = context.stack().lowWatermarkFromVisitingDirtyPages();
        state->cpu.sp() = std::min(lowWatermark, state->cpu.sp());

        state->initializeStackFunction = flushDirtyStackPages;
        state->initializeStackArg = context.releaseStack();
    }
}

static void flushDirtyStackPages(State* state)
{
    std::unique_ptr<Stack> stack(reinterpret_cast<Probe::Stack*>(state->initializeStackArg));
    stack->flushWrites();
    state->cpu.sp() = stack->savedStackPointer();
}

// Not for general use. This should only be for writing tests.
JS_EXPORT_PRIVATE void* probeStateForContext(Context&);
void* probeStateForContext(Context& context)
{
    return context.m_state;
}

} // namespace Probe
} // namespace JSC

#endif // ENABLE(ASSEMBLER)
