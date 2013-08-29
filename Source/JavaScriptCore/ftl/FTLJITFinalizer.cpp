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
#include "FTLJITFinalizer.h"

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "DFGPlan.h"

namespace JSC { namespace FTL {

using namespace DFG;

JITFinalizer::JITFinalizer(Plan& plan)
    : Finalizer(plan)
{
}

JITFinalizer::~JITFinalizer()
{
}

bool JITFinalizer::finalize()
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JITFinalizer::finalizeFunction()
{
    for (unsigned i = m_jitCode->handles().size(); i--;) {
        MacroAssembler::cacheFlush(
            m_jitCode->handles()[i]->start(), m_jitCode->handles()[i]->sizeInBytes());
    }
    
    if (m_exitThunksLinkBuffer) {
        m_jitCode->initializeExitThunks(
            FINALIZE_DFG_CODE(
                *m_exitThunksLinkBuffer,
                ("FTL exit thunks for %s", toCString(CodeBlockWithJITType(m_plan.codeBlock.get(), JITCode::FTLJIT)).data())));
    } // else this function had no OSR exits, so no exit thunks.
    
    MacroAssemblerCodePtr withArityCheck = m_entrypointLinkBuffer->locationOf(m_arityCheck);
    m_jitCode->initializeCode(
        FINALIZE_DFG_CODE(
            *m_entrypointLinkBuffer,
            ("FTL entrypoint thunk for %s with LLVM generated code at %p", toCString(CodeBlockWithJITType(m_plan.codeBlock.get(), JITCode::FTLJIT)).data(), m_function)));
    
    m_plan.codeBlock->setJITCode(m_jitCode, withArityCheck);
    
    return true;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

