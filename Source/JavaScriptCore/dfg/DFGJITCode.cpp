/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "DFGJITCode.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "FTLForOSREntryJITCode.h"

namespace JSC { namespace DFG {

JITCode::JITCode()
    : DirectJITCode(JITType::DFGJIT)
#if ENABLE(FTL_JIT)
    , osrEntryRetry(0)
    , abandonOSREntry(false)
#endif // ENABLE(FTL_JIT)
{
}

JITCode::~JITCode()
{
}

CommonData* JITCode::dfgCommon()
{
    return &common;
}

JITCode* JITCode::dfg()
{
    return this;
}

void JITCode::shrinkToFit(const ConcurrentJSLocker&)
{
    common.shrinkToFit();
    minifiedDFG.prepareAndShrink();
    variableEventStream.shrinkToFit();
}

void JITCode::reconstruct(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, unsigned streamIndex,
    Operands<ValueRecovery>& result)
{
    variableEventStream.reconstruct(
        codeBlock, codeOrigin, minifiedDFG, streamIndex, result);
}

void JITCode::reconstruct(CallFrame* callFrame, CodeBlock* codeBlock, CodeOrigin codeOrigin, unsigned streamIndex, Operands<std::optional<JSValue>>& result)
{
    Operands<ValueRecovery> recoveries;
    reconstruct(codeBlock, codeOrigin, streamIndex, recoveries);
    
    result = Operands<std::optional<JSValue>>(OperandsLike, recoveries);
    for (size_t i = result.size(); i--;)
        result[i] = recoveries[i].recover(callFrame);
}

RegisterSet JITCode::liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock* codeBlock, CallSiteIndex callSiteIndex)
{
    for (OSRExit& exit : m_osrExit) {
        if (exit.isExceptionHandler() && exit.m_exceptionHandlerCallSiteIndex.bits() == callSiteIndex.bits()) {
            Operands<ValueRecovery> valueRecoveries;
            reconstruct(codeBlock, exit.m_codeOrigin, exit.m_streamIndex, valueRecoveries);
            RegisterSet liveAtOSRExit;
            for (size_t index = 0; index < valueRecoveries.size(); ++index) {
                const ValueRecovery& recovery = valueRecoveries[index];
                if (recovery.isInRegisters()) {
                    if (recovery.isInGPR())
                        liveAtOSRExit.set(recovery.gpr());
                    else if (recovery.isInFPR())
                        liveAtOSRExit.set(recovery.fpr());
#if USE(JSVALUE32_64)
                    else if (recovery.isInJSValueRegs()) {
                        liveAtOSRExit.set(recovery.payloadGPR());
                        liveAtOSRExit.set(recovery.tagGPR());
                    }
#endif
                    else
                        RELEASE_ASSERT_NOT_REACHED();
                }
            }

            return liveAtOSRExit;
        }
    }

    return { };
}

#if ENABLE(FTL_JIT)
bool JITCode::checkIfOptimizationThresholdReached(CodeBlock* codeBlock)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    return tierUpCounter.checkIfThresholdCrossedAndSet(codeBlock);
}

void JITCode::optimizeNextInvocation(CodeBlock* codeBlock)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": FTL-optimizing next invocation.");
    tierUpCounter.setNewThreshold(0, codeBlock);
}

void JITCode::dontOptimizeAnytimeSoon(CodeBlock* codeBlock)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Not FTL-optimizing anytime soon.");
    tierUpCounter.deferIndefinitely();
}

void JITCode::optimizeAfterWarmUp(CodeBlock* codeBlock)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": FTL-optimizing after warm-up.");
    CodeBlock* baseline = codeBlock->baselineVersion();
    tierUpCounter.setNewThreshold(
        baseline->adjustedCounterValue(Options::thresholdForFTLOptimizeAfterWarmUp()),
        baseline);
}

void JITCode::optimizeSoon(CodeBlock* codeBlock)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": FTL-optimizing soon.");
    CodeBlock* baseline = codeBlock->baselineVersion();
    tierUpCounter.setNewThreshold(
        baseline->adjustedCounterValue(Options::thresholdForFTLOptimizeSoon()),
        codeBlock);
}

void JITCode::forceOptimizationSlowPathConcurrently(CodeBlock* codeBlock)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Forcing slow path concurrently for FTL entry.");
    tierUpCounter.forceSlowPathConcurrently();
}

void JITCode::setOptimizationThresholdBasedOnCompilationResult(
    CodeBlock* codeBlock, CompilationResult result)
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    switch (result) {
    case CompilationSuccessful:
        optimizeNextInvocation(codeBlock);
        codeBlock->baselineVersion()->m_hasBeenCompiledWithFTL = true;
        return;
    case CompilationFailed:
        dontOptimizeAnytimeSoon(codeBlock);
        codeBlock->baselineVersion()->m_didFailFTLCompilation = true;
        return;
    case CompilationDeferred:
        optimizeAfterWarmUp(codeBlock);
        return;
    case CompilationInvalidated:
        // This is weird - it will only happen in cases when the DFG code block (i.e.
        // the code block that this JITCode belongs to) is also invalidated. So it
        // doesn't really matter what we do. But, we do the right thing anyway. Note
        // that us counting the reoptimization actually means that we might count it
        // twice. But that's generally OK. It's better to overcount reoptimizations
        // than it is to undercount them.
        codeBlock->baselineVersion()->countReoptimization();
        optimizeAfterWarmUp(codeBlock);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void JITCode::setOSREntryBlock(VM& vm, const JSCell* owner, CodeBlock* osrEntryBlock)
{
    if (Options::verboseOSR()) {
        dataLogLn(RawPointer(this), ": Setting OSR entry block to ", RawPointer(osrEntryBlock));
        dataLogLn("OSR entries will go to ", osrEntryBlock->jitCode()->ftlForOSREntry()->addressForCall(ArityCheckNotRequired));
    }
    m_osrEntryBlock.set(vm, owner, osrEntryBlock);
}

void JITCode::clearOSREntryBlockAndResetThresholds(CodeBlock *dfgCodeBlock)
{ 
    ASSERT(m_osrEntryBlock);

    BytecodeIndex osrEntryBytecode = m_osrEntryBlock->jitCode()->ftlForOSREntry()->bytecodeIndex();
    m_osrEntryBlock.clear();
    osrEntryRetry = 0;
    tierUpEntryTriggers.set(osrEntryBytecode, JITCode::TriggerReason::DontTrigger);
    setOptimizationThresholdBasedOnCompilationResult(dfgCodeBlock, CompilationDeferred);
}
#endif // ENABLE(FTL_JIT)

void JITCode::validateReferences(const TrackedReferences& trackedReferences)
{
    common.validateReferences(trackedReferences);
    
    for (OSREntryData& entry : m_osrEntry) {
        for (unsigned i = entry.m_expectedValues.size(); i--;)
            entry.m_expectedValues[i].validateReferences(trackedReferences);
    }
    
    minifiedDFG.validateReferences(trackedReferences);
}

std::optional<CodeOrigin> JITCode::findPC(CodeBlock*, void* pc)
{
    for (OSRExit& exit : m_osrExit) {
        if (ExecutableMemoryHandle* handle = exit.m_code.executableMemory()) {
            if (handle->start().untaggedPtr() <= pc && pc < handle->end().untaggedPtr())
                return std::optional<CodeOrigin>(exit.m_codeOriginForExitProfile);
        }
    }

    return std::nullopt;
}

void JITCode::finalizeOSREntrypoints(Vector<OSREntryData>&& osrEntry)
{
    auto comparator = [] (const auto& a, const auto& b) {
        return a.m_bytecodeIndex < b.m_bytecodeIndex;
    };
    std::sort(osrEntry.begin(), osrEntry.end(), comparator);

#if ASSERT_ENABLED
    auto verifyIsSorted = [&] (auto& osrVector) {
        for (unsigned i = 0; i + 1 < osrVector.size(); ++i)
            ASSERT(osrVector[i].m_bytecodeIndex <= osrVector[i + 1].m_bytecodeIndex);
    };
    verifyIsSorted(osrEntry);
#endif
    m_osrEntry = WTFMove(osrEntry);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
