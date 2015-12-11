/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "FTLThunks.h"
#include "ProfilerDatabase.h"

namespace JSC { namespace FTL {

using namespace DFG;

JITFinalizer::JITFinalizer(Plan& plan)
    : Finalizer(plan)
{
}

JITFinalizer::~JITFinalizer()
{
}

size_t JITFinalizer::codeSize()
{
    size_t result = 0;

#if FTL_USES_B3
    if (b3CodeLinkBuffer)
        result += b3CodeLinkBuffer->size();
#else // FTL_USES_B3
    if (exitThunksLinkBuffer)
        result += exitThunksLinkBuffer->size();
    if (sideCodeLinkBuffer)
        result += sideCodeLinkBuffer->size();
    if (handleExceptionsLinkBuffer)
        result += handleExceptionsLinkBuffer->size();
    for (unsigned i = jitCode->handles().size(); i--;)
        result += jitCode->handles()[i]->sizeInBytes();
#endif // FTL_USES_B3
    
    if (entrypointLinkBuffer)
        result += entrypointLinkBuffer->size();
    
    return result;
}

bool JITFinalizer::finalize()
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JITFinalizer::finalizeFunction()
{
    bool dumpDisassembly = shouldDumpDisassembly() || Options::asyncDisassembly();
    
#if FTL_USES_B3
    jitCode->initializeB3Code(
        FINALIZE_CODE_IF(
            dumpDisassembly, *b3CodeLinkBuffer,
            ("FTL B3 code for %s", toCString(CodeBlockWithJITType(m_plan.codeBlock, JITCode::FTLJIT)).data())));

#else // FTL_USES_B3
    for (unsigned i = jitCode->handles().size(); i--;) {
        MacroAssembler::cacheFlush(
            jitCode->handles()[i]->start(), jitCode->handles()[i]->sizeInBytes());
    }

    if (exitThunksLinkBuffer) {
        for (unsigned i = 0; i < osrExit.size(); ++i) {
            OSRExitCompilationInfo& info = osrExit[i];
            exitThunksLinkBuffer->link(
                info.m_thunkJump,
                CodeLocationLabel(
                    m_plan.vm.getCTIStub(osrExitGenerationThunkGenerator).code()));
        }
        
        jitCode->initializeExitThunks(
            FINALIZE_CODE_IF(
                dumpDisassembly, *exitThunksLinkBuffer,
                ("FTL exit thunks for %s", toCString(CodeBlockWithJITType(m_plan.codeBlock, JITCode::FTLJIT)).data())));
    } // else this function had no OSR exits, so no exit thunks.
    
    if (sideCodeLinkBuffer) {
        // Side code is for special slow paths that we generate ourselves, like for inline
        // caches.
        
        for (CCallHelpers::Jump jump : lazySlowPathGeneratorJumps) {
            sideCodeLinkBuffer->link(
                jump,
                CodeLocationLabel(
                    m_plan.vm.getCTIStub(lazySlowPathGenerationThunkGenerator).code()));
        }
        
        jitCode->addHandle(FINALIZE_CODE_IF(
            dumpDisassembly, *sideCodeLinkBuffer,
            ("FTL side code for %s",
                toCString(CodeBlockWithJITType(m_plan.codeBlock, JITCode::FTLJIT)).data()))
            .executableMemory());
    }
    
    if (handleExceptionsLinkBuffer) {
        jitCode->addHandle(FINALIZE_CODE_IF(
            dumpDisassembly, *handleExceptionsLinkBuffer,
            ("FTL exception handler for %s",
                toCString(CodeBlockWithJITType(m_plan.codeBlock, JITCode::FTLJIT)).data()))
            .executableMemory());
    }

    for (unsigned i = 0; i < outOfLineCodeInfos.size(); ++i) {
        jitCode->addHandle(FINALIZE_CODE_IF(
            dumpDisassembly, *outOfLineCodeInfos[i].m_linkBuffer,
            ("FTL out of line code for %s inline cache", outOfLineCodeInfos[i].m_codeDescription)).executableMemory());
    }
#endif // FTL_USES_B3

    jitCode->initializeArityCheckEntrypoint(
        FINALIZE_CODE_IF(
            dumpDisassembly, *entrypointLinkBuffer,
            ("FTL entrypoint thunk for %s with LLVM generated code at %p", toCString(CodeBlockWithJITType(m_plan.codeBlock, JITCode::FTLJIT)).data(), function)));
    
    m_plan.codeBlock->setJITCode(jitCode);
#if !FTL_USES_B3
    m_plan.vm.updateFTLLargestStackSize(jitCode->stackmaps.stackSize());
#endif

    if (m_plan.compilation)
        m_plan.vm.m_perBytecodeProfiler->addCompilation(m_plan.compilation);
    
    return true;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

