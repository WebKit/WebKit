/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "FTLJITFinalizer.h"

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "DFGPlan.h"
#include "FTLState.h"
#include "ProfilerDatabase.h"
#include "ThunkGenerators.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace FTL {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JITFinalizer);

JITFinalizer::JITFinalizer(DFG::Plan& plan)
    : Finalizer(plan)
{
}

JITFinalizer::~JITFinalizer() = default;

size_t JITFinalizer::codeSize()
{
    return m_codeSize;
}

bool JITFinalizer::finalize()
{
    VM& vm = *m_plan.vm();
    WTF::crossModifyingCodeFence();

    m_plan.runMainThreadFinalizationTasks();

    CodeBlock* codeBlock = m_plan.codeBlock();

    codeBlock->setJITCode(*m_jitCode);

    if (UNLIKELY(m_plan.compilation()))
        vm.m_perBytecodeProfiler->addCompilation(codeBlock, *m_plan.compilation());

    // The codeBlock is now responsible for keeping many things alive (e.g. frozen values)
    // that were previously kept alive by the plan.
    vm.writeBarrier(codeBlock);

    return true;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)
