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
#include "DFGJITFinalizer.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGPlan.h"
#include "HeapInlines.h"
#include "ProfilerDatabase.h"

namespace JSC { namespace DFG {

JITFinalizer::JITFinalizer(Plan& plan, Ref<JITCode>&& jitCode, std::unique_ptr<LinkBuffer> linkBuffer, CodePtr<JSEntryPtrTag> withArityCheck)
    : Finalizer(plan)
    , m_jitCode(WTFMove(jitCode))
    , m_linkBuffer(WTFMove(linkBuffer))
    , m_withArityCheck(withArityCheck)
{
}

JITFinalizer::~JITFinalizer()
{
}

size_t JITFinalizer::codeSize()
{
    return m_linkBuffer->size();
}

bool JITFinalizer::finalize()
{
    VM& vm = *m_plan.vm();

    WTF::crossModifyingCodeFence();

    m_linkBuffer->runMainThreadFinalizationTasks();

    CodeBlock* codeBlock = m_plan.codeBlock();

    codeBlock->setJITCode(m_jitCode.copyRef());

    auto data = m_plan.tryFinalizeJITData(m_jitCode.get());
    if (UNLIKELY(!data))
        return false;
    codeBlock->setDFGJITData(WTFMove(data));

#if ENABLE(FTL_JIT)
    m_jitCode->optimizeAfterWarmUp(codeBlock);
#endif // ENABLE(FTL_JIT)

    if (UNLIKELY(m_plan.compilation()))
        vm.m_perBytecodeProfiler->addCompilation(codeBlock, *m_plan.compilation());

    if (!m_plan.willTryToTierUp())
        codeBlock->baselineVersion()->m_didFailFTLCompilation = true;

    // The codeBlock is now responsible for keeping many things alive (e.g. frozen values)
    // that were previously kept alive by the plan.
    vm.writeBarrier(codeBlock);

    return true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
