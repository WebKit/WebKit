/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "DFGOSRExitCompiler.h"

#include "CallFrame.h"
#include "DFGCommon.h"
#include "DFGJITCode.h"
#include "DFGOSRExitPreparation.h"
#include "LinkBuffer.h"
#include "OperandsInlines.h"
#include "JSCInlines.h"
#include "RepatchBuffer.h"
#include <wtf/StringPrintStream.h>

namespace JSC { namespace DFG {

extern "C" {

void compileOSRExit(ExecState* exec)
{
    SamplingRegion samplingRegion("DFG OSR Exit Compilation");
    
    CodeBlock* codeBlock = exec->codeBlock();
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITCode::DFGJIT);
    
    VM* vm = &exec->vm();
    
    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm->heap);

    uint32_t exitIndex = vm->osrExitIndex;
    OSRExit& exit = codeBlock->jitCode()->dfg()->osrExit[exitIndex];
    
    prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);
    
    // Compute the value recoveries.
    Operands<ValueRecovery> operands;
    codeBlock->jitCode()->dfg()->variableEventStream.reconstruct(codeBlock, exit.m_codeOrigin, codeBlock->jitCode()->dfg()->minifiedDFG, exit.m_streamIndex, operands);
    
    // There may be an override, for forward speculations.
    if (!!exit.m_valueRecoveryOverride) {
        operands.setOperand(
            exit.m_valueRecoveryOverride->operand, exit.m_valueRecoveryOverride->recovery);
    }
    
    SpeculationRecovery* recovery = 0;
    if (exit.m_recoveryIndex != UINT_MAX)
        recovery = &codeBlock->jitCode()->dfg()->speculationRecovery[exit.m_recoveryIndex];

    {
        CCallHelpers jit(vm, codeBlock);
        OSRExitCompiler exitCompiler(jit);

        jit.jitAssertHasValidCallFrame();
        
        if (vm->m_perBytecodeProfiler && codeBlock->jitCode()->dfgCommon()->compilation) {
            Profiler::Database& database = *vm->m_perBytecodeProfiler;
            Profiler::Compilation* compilation = codeBlock->jitCode()->dfgCommon()->compilation.get();
            
            Profiler::OSRExit* profilerExit = compilation->addOSRExit(
                exitIndex, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
                exit.m_kind, isWatchpoint(exit.m_kind));
            jit.add64(CCallHelpers::TrustedImm32(1), CCallHelpers::AbsoluteAddress(profilerExit->counterAddress()));
        }
        
        exitCompiler.compileExit(exit, operands, recovery);
        
        LinkBuffer patchBuffer(*vm, &jit, codeBlock);
        exit.m_code = FINALIZE_CODE_IF(
            shouldShowDisassembly() || Options::verboseOSR(),
            patchBuffer,
            ("DFG OSR exit #%u (%s, %s) from %s, with operands = %s",
                exitIndex, toCString(exit.m_codeOrigin).data(),
                exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
                toCString(ignoringContext<DumpContext>(operands)).data()));
    }
    
    {
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(exit.codeLocationForRepatch(codeBlock), CodeLocationLabel(exit.m_code.code()));
    }
    
    vm->osrExitJumpDestination = exit.m_code.code().executableAddress();
}

} // extern "C"

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
