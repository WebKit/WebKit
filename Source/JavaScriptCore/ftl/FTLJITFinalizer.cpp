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
#include "FTLThunks.h"

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
    
    result += exitThunksLinkBuffer->size();
    result += entrypointLinkBuffer->size();
    if (sideCodeLinkBuffer)
        result += sideCodeLinkBuffer->size();
    result += handleExceptionsLinkBuffer->size();
    
    for (unsigned i = jitCode->handles().size(); i--;)
        result += jitCode->handles()[i]->sizeInBytes();
    
    return result;
}

bool JITFinalizer::finalize()
{
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JITFinalizer::finalizeFunction()
{
    for (unsigned i = jitCode->handles().size(); i--;) {
        MacroAssembler::cacheFlush(
            jitCode->handles()[i]->start(), jitCode->handles()[i]->sizeInBytes());
    }
    
    if (exitThunksLinkBuffer) {
        StackMaps::RecordMap recordMap = jitCode->stackmaps.getRecordMap();
        
        for (unsigned i = 0; i < osrExit.size(); ++i) {
            OSRExitCompilationInfo& info = osrExit[i];
            OSRExit& exit = jitCode->osrExit[i];
            StackMaps::RecordMap::iterator iter = recordMap.find(exit.m_stackmapID);
            if (iter == recordMap.end()) {
                // It's OK, it was optimized out.
                continue;
            }
            
            exitThunksLinkBuffer->link(
                info.m_thunkJump,
                CodeLocationLabel(
                    m_plan.vm.getCTIStub(osrExitGenerationThunkGenerator).code()));
        }
        
        jitCode->initializeExitThunks(
            FINALIZE_DFG_CODE(
                *exitThunksLinkBuffer,
                ("FTL exit thunks for %s", toCString(CodeBlockWithJITType(m_plan.codeBlock.get(), JITCode::FTLJIT)).data())));
    } // else this function had no OSR exits, so no exit thunks.
    
    if (sideCodeLinkBuffer) {
        // Side code is for special slow paths that we generate ourselves, like for inline
        // caches.
        
        for (unsigned i = slowPathCalls.size(); i--;) {
            SlowPathCall& call = slowPathCalls[i];
            sideCodeLinkBuffer->link(
                call.call(),
                CodeLocationLabel(m_plan.vm.ftlThunks->getSlowPathCallThunk(m_plan.vm, call.key()).code()));
        }
        
        jitCode->addHandle(FINALIZE_DFG_CODE(
            *sideCodeLinkBuffer,
            ("FTL side code for %s",
                toCString(CodeBlockWithJITType(m_plan.codeBlock.get(), JITCode::FTLJIT)).data()))
            .executableMemory());
    }
    
    if (handleExceptionsLinkBuffer) {
        jitCode->addHandle(FINALIZE_DFG_CODE(
            *handleExceptionsLinkBuffer,
            ("FTL exception handler for %s",
                toCString(CodeBlockWithJITType(m_plan.codeBlock.get(), JITCode::FTLJIT)).data()))
            .executableMemory());
    }
    
    MacroAssemblerCodePtr withArityCheck;
    if (arityCheck.isSet())
        withArityCheck = entrypointLinkBuffer->locationOf(arityCheck);
    jitCode->initializeArityCheckEntrypoint(
        FINALIZE_DFG_CODE(
            *entrypointLinkBuffer,
            ("FTL entrypoint thunk for %s with LLVM generated code at %p", toCString(CodeBlockWithJITType(m_plan.codeBlock.get(), JITCode::FTLJIT)).data(), function)));
    
    m_plan.codeBlock->setJITCode(jitCode);
    
    if (m_plan.compilation)
        m_plan.vm.m_perBytecodeProfiler->addCompilation(m_plan.compilation);
    
    return true;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

