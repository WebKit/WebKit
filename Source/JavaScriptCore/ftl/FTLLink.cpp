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
#include "FTLLink.h"

#if ENABLE(FTL_JIT)

#include "CCallHelpers.h"
#include "CodeBlockWithJITType.h"
#include "DFGCommon.h"
#include "FTLJITCode.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "ProfilerCompilation.h"
#include "VirtualRegister.h"

namespace JSC { namespace FTL {

using namespace DFG;

void link(State& state)
{
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;
    VM& vm = graph.m_vm;
    
    // B3 will create its own jump tables as needed.
    codeBlock->clearSwitchJumpTables();

    state.jitCode->common.requiredRegisterCountForExit = graph.requiredRegisterCountForExit();
    
    if (!graph.m_plan.inlineCallFrames->isEmpty())
        state.jitCode->common.inlineCallFrames = graph.m_plan.inlineCallFrames;
    
    graph.registerFrozenValues();

    // Create the entrypoint. Note that we use this entrypoint totally differently
    // depending on whether we're doing OSR entry or not.
    CCallHelpers jit(&vm, codeBlock);
    
    std::unique_ptr<LinkBuffer> linkBuffer;

    CCallHelpers::Address frame = CCallHelpers::Address(
        CCallHelpers::stackPointerRegister, -static_cast<int32_t>(AssemblyHelpers::prologueStackPointerDelta()));
    
    if (Profiler::Compilation* compilation = graph.compilation()) {
        compilation->addDescription(
            Profiler::OriginStack(),
            toCString("Generated FTL JIT code for ", CodeBlockWithJITType(codeBlock, JITCode::FTLJIT), ", instruction count = ", graph.m_codeBlock->instructionCount(), ":\n"));
        
        graph.ensureDominators();
        graph.ensureNaturalLoops();
        
        const char* prefix = "    ";
        
        DumpContext dumpContext;
        StringPrintStream out;
        Node* lastNode = 0;
        for (size_t blockIndex = 0; blockIndex < graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = graph.block(blockIndex);
            if (!block)
                continue;
            
            graph.dumpBlockHeader(out, prefix, block, Graph::DumpLivePhisOnly, &dumpContext);
            compilation->addDescription(Profiler::OriginStack(), out.toCString());
            out.reset();
            
            for (size_t nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                Profiler::OriginStack stack;
                
                if (node->origin.semantic.isSet()) {
                    stack = Profiler::OriginStack(
                        *vm.m_perBytecodeProfiler, codeBlock, node->origin.semantic);
                }
                
                if (graph.dumpCodeOrigin(out, prefix, lastNode, node, &dumpContext)) {
                    compilation->addDescription(stack, out.toCString());
                    out.reset();
                }
                
                graph.dump(out, prefix, node, &dumpContext);
                compilation->addDescription(stack, out.toCString());
                out.reset();
                
                if (node->origin.semantic.isSet())
                    lastNode = node;
            }
        }
        
        dumpContext.dump(out, prefix);
        compilation->addDescription(Profiler::OriginStack(), out.toCString());
        out.reset();

        out.print("    Disassembly:\n");
        out.print("        <not implemented yet>\n");
        compilation->addDescription(Profiler::OriginStack(), out.toCString());
        out.reset();
        
        state.jitCode->common.compilation = compilation;
    }
    
    switch (graph.m_plan.mode) {
    case FTLMode: {
        CCallHelpers::JumpList fillRegistersAndContinueMainPath;
        CCallHelpers::JumpList toMainPath;

        unsigned numParameters = static_cast<unsigned>(codeBlock->numParameters());
        unsigned maxRegisterArgumentCount = std::min(numParameters, NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS);

        GPRReg argCountReg = argumentRegisterForArgumentCount();

        CCallHelpers::Label registerArgumentsEntrypoints[NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS + 1];

        if (numParameters < NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS) {
            // Spill any extra register arguments passed to function onto the stack.
            for (unsigned argIndex = NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS - 1; argIndex >= numParameters; argIndex--) {
                registerArgumentsEntrypoints[argIndex + 1] = jit.label();
                jit.emitPutArgumentToCallFrameBeforePrologue(argumentRegisterForFunctionArgument(argIndex), argIndex);
            }
            incrementCounter(&jit, VM::RegArgsExtra);
            toMainPath.append(jit.jump());
        }

        CCallHelpers::JumpList continueToArityFixup;

        CCallHelpers::Label stackArgsCheckArityEntry = jit.label();
        incrementCounter(&jit, VM::StackArgsArity);
        jit.load32(frame.withOffset(sizeof(Register) * CallFrameSlot::argumentCount), GPRInfo::regT1);
        continueToArityFixup.append(jit.branch32(
            CCallHelpers::Below, GPRInfo::regT1,
            CCallHelpers::TrustedImm32(numParameters)));

#if ENABLE(VM_COUNTERS)
        CCallHelpers::Jump continueToStackArityOk = jit.jump();
#endif

        CCallHelpers::Label stackArgsArityOKEntry = jit.label();

        incrementCounter(&jit, VM::StackArgsArity);

#if ENABLE(VM_COUNTERS)
        continueToStackArityOk.link(&jit);
#endif

        // Load argument values into argument registers

        // FIXME: Would like to eliminate these to load, but we currently can't jump into
        // the B3 compiled code at an arbitrary point from the slow entry where the
        // registers are stored to the stack.
        jit.emitGetFromCallFrameHeaderBeforePrologue(CallFrameSlot::callee, argumentRegisterForCallee());
        jit.emitGetPayloadFromCallFrameHeaderBeforePrologue(CallFrameSlot::argumentCount, argumentRegisterForArgumentCount());

        for (unsigned argIndex = 0; argIndex < maxRegisterArgumentCount; argIndex++)
            jit.emitGetFromCallFrameArgumentBeforePrologue(argIndex, argumentRegisterForFunctionArgument(argIndex));

        toMainPath.append(jit.jump());

        CCallHelpers::Label registerArgsCheckArityEntry = jit.label();
        incrementCounter(&jit, VM::RegArgsArity);

        CCallHelpers::JumpList continueToRegisterArityFixup;
        CCallHelpers::Label checkForExtraRegisterArguments;

        if (numParameters < NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS) {
            toMainPath.append(jit.branch32(
                CCallHelpers::Equal, argCountReg, CCallHelpers::TrustedImm32(numParameters)));
            continueToRegisterArityFixup.append(jit.branch32(
                CCallHelpers::Below, argCountReg, CCallHelpers::TrustedImm32(numParameters)));
            //  Fall through to the "extra register arity" case.

            checkForExtraRegisterArguments = jit.label();
            // Spill any extra register arguments passed to function onto the stack.
            for (unsigned argIndex = numParameters; argIndex < NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS; argIndex++) {
                toMainPath.append(jit.branch32(CCallHelpers::BelowOrEqual, argCountReg, CCallHelpers::TrustedImm32(argIndex)));
                jit.emitPutArgumentToCallFrameBeforePrologue(argumentRegisterForFunctionArgument(argIndex), argIndex);
            }

            incrementCounter(&jit, VM::RegArgsExtra);
            toMainPath.append(jit.jump());
        } else
            toMainPath.append(jit.branch32(
                CCallHelpers::AboveOrEqual, argCountReg, CCallHelpers::TrustedImm32(numParameters)));

#if ENABLE(VM_COUNTERS)
        continueToRegisterArityFixup.append(jit.jump());
#endif

        if (numParameters > 0) {
            //  There should always be a "this" parameter.
            CCallHelpers::Label registerArgumentsNeedArityFixup = jit.label();

            for (unsigned argIndex = 1; argIndex < numParameters && argIndex <= maxRegisterArgumentCount; argIndex++)
                registerArgumentsEntrypoints[argIndex] = registerArgumentsNeedArityFixup;
        }

#if ENABLE(VM_COUNTERS)
        incrementCounter(&jit, VM::RegArgsArity);
#endif

        continueToRegisterArityFixup.link(&jit);

        jit.spillArgumentRegistersToFrameBeforePrologue(maxRegisterArgumentCount);

        continueToArityFixup.link(&jit);

        incrementCounter(&jit, VM::ArityFixupRequired);

        jit.emitFunctionPrologue();
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);
        CCallHelpers::Call callArityCheck = jit.call();

        auto noException = jit.branch32(CCallHelpers::GreaterThanOrEqual, GPRInfo::returnValueGPR, CCallHelpers::TrustedImm32(0));
        jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer();
        jit.move(CCallHelpers::TrustedImmPtr(jit.vm()), GPRInfo::argumentGPR0);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
        CCallHelpers::Call callLookupExceptionHandlerFromCallerFrame = jit.call();
        jit.jumpToExceptionHandler();
        noException.link(&jit);

        if (!ASSERT_DISABLED) {
            jit.load64(vm.addressOfException(), GPRInfo::regT1);
            jit.jitAssertIsNull(GPRInfo::regT1);
        }

        jit.move(GPRInfo::returnValueGPR, GPRInfo::argumentGPR0);
        jit.emitFunctionEpilogue();
        fillRegistersAndContinueMainPath.append(jit.branchTest32(CCallHelpers::Zero, GPRInfo::argumentGPR0));
        jit.emitFunctionPrologue();
        CCallHelpers::Call callArityFixup = jit.call();
        jit.emitFunctionEpilogue();

        fillRegistersAndContinueMainPath.append(jit.jump());

        fillRegistersAndContinueMainPath.linkTo(stackArgsArityOKEntry, &jit);

#if ENABLE(VM_COUNTERS)
        CCallHelpers::Label registerEntryNoArity = jit.label();
        incrementCounter(&jit, VM::RegArgsNoArity);
        toMainPath.append(jit.jump());
#endif

        linkBuffer = std::make_unique<LinkBuffer>(vm, jit, codeBlock, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        linkBuffer->link(callArityCheck, codeBlock->m_isConstructor ? operationConstructArityCheck : operationCallArityCheck);
        linkBuffer->link(callLookupExceptionHandlerFromCallerFrame, lookupExceptionHandlerFromCallerFrame);
        linkBuffer->link(callArityFixup, FunctionPtr((vm.getCTIStub(arityFixupGenerator)).code().executableAddress()));
        linkBuffer->link(toMainPath, CodeLocationLabel(bitwise_cast<void*>(state.generatedFunction)));

        state.jitCode->setEntryFor(StackArgsMustCheckArity, linkBuffer->locationOf(stackArgsCheckArityEntry));
        state.jitCode->setEntryFor(StackArgsArityCheckNotRequired, linkBuffer->locationOf(stackArgsArityOKEntry));

#if ENABLE(VM_COUNTERS)
        MacroAssemblerCodePtr mainEntry = linkBuffer->locationOf(registerEntryNoArity);
#else
        MacroAssemblerCodePtr mainEntry = MacroAssemblerCodePtr(bitwise_cast<void*>(state.generatedFunction));
#endif
        state.jitCode->setEntryFor(RegisterArgsArityCheckNotRequired, mainEntry);

        if (checkForExtraRegisterArguments.isSet())
            state.jitCode->setEntryFor(RegisterArgsPossibleExtraArgs, linkBuffer->locationOf(checkForExtraRegisterArguments));
        else
            state.jitCode->setEntryFor(RegisterArgsPossibleExtraArgs, mainEntry);
                                                                             
        state.jitCode->setEntryFor(RegisterArgsMustCheckArity, linkBuffer->locationOf(registerArgsCheckArityEntry));

        for (unsigned argCount = 1; argCount <= NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS; argCount++) {
            MacroAssemblerCodePtr entry;
            if (argCount == numParameters)
                entry = mainEntry;
            else if (registerArgumentsEntrypoints[argCount].isSet())
                entry = linkBuffer->locationOf(registerArgumentsEntrypoints[argCount]);
            else
                entry = linkBuffer->locationOf(registerArgsCheckArityEntry);
            state.jitCode->setEntryFor(JITEntryPoints::registerEntryTypeForArgumentCount(argCount), entry);
        }
        break;
    }
        
    case FTLForOSREntryMode: {
        // We jump to here straight from DFG code, after having boxed up all of the
        // values into the scratch buffer. Everything should be good to go - at this
        // point we've even done the stack check. Basically we just have to make the
        // call to the B3-generated code.
        CCallHelpers::Label start = jit.label();

        jit.emitFunctionEpilogue();

        // Load argument values into argument registers

        // FIXME: Would like to eliminate these to load, but we currently can't jump into
        // the B3 compiled code at an arbitrary point from the slow entry where the
        // registers are stored to the stack.
        jit.emitGetFromCallFrameHeaderBeforePrologue(CallFrameSlot::callee, argumentRegisterForCallee());
        jit.emitGetPayloadFromCallFrameHeaderBeforePrologue(CallFrameSlot::argumentCount, argumentRegisterForArgumentCount());

        for (unsigned argIndex = 0; argIndex < static_cast<unsigned>(codeBlock->numParameters()) && argIndex < NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS; argIndex++)
            jit.emitGetFromCallFrameArgumentBeforePrologue(argIndex, argumentRegisterForFunctionArgument(argIndex));

        CCallHelpers::Jump mainPathJump = jit.jump();
        
        linkBuffer = std::make_unique<LinkBuffer>(vm, jit, codeBlock, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        linkBuffer->link(mainPathJump, CodeLocationLabel(bitwise_cast<void*>(state.generatedFunction)));

        state.jitCode->setEntryFor(RegisterArgsArityCheckNotRequired, linkBuffer->locationOf(start));
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    state.finalizer->entrypointLinkBuffer = WTFMove(linkBuffer);
    state.finalizer->function = state.generatedFunction;
    state.finalizer->jitCode = state.jitCode;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

