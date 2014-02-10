/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGJITFinalizer.h"

#include "CodeBlock.h"
#include "DFGPlan.h"
#include "Operations.h"

namespace JSC { namespace DFG {

JITFinalizer::JITFinalizer(Plan& plan, PassRefPtr<JITCode> jitCode, PassOwnPtr<LinkBuffer> linkBuffer, MacroAssemblerCodePtr withArityCheck)
    : Finalizer(plan)
    , m_jitCode(jitCode)
    , m_linkBuffer(linkBuffer)
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
    m_jitCode->initializeCodeRef(
        m_linkBuffer->finalizeCodeWithoutDisassembly(), MacroAssemblerCodePtr());
    m_plan.codeBlock->setJITCode(m_jitCode);
    
    finalizeCommon();
    
    return true;
}

bool JITFinalizer::finalizeFunction()
{
    RELEASE_ASSERT(!m_withArityCheck.isEmptyValue());
    m_jitCode->initializeCodeRef(
        m_linkBuffer->finalizeCodeWithoutDisassembly(), m_withArityCheck);
    m_plan.codeBlock->setJITCode(m_jitCode);
    
    finalizeCommon();
    
    return true;
}

void JITFinalizer::finalizeCommon()
{
#if ENABLE(FTL_JIT)
    m_jitCode->optimizeAfterWarmUp(m_plan.codeBlock.get());
#endif // ENABLE(FTL_JIT)
    
    if (m_plan.compilation)
        m_plan.vm.m_perBytecodeProfiler->addCompilation(m_plan.compilation);
    
    if (!m_plan.willTryToTierUp)
        m_plan.codeBlock->baselineVersion()->m_didFailFTLCompilation = true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

