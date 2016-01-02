/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics
 * Copyright (C) 2014 University of Szeged
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
#include "FTLCompile.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "CodeBlockWithJITType.h"
#include "CCallHelpers.h"
#include "DFGCommon.h"
#include "DFGGraphSafepoint.h"
#include "DFGOperations.h"
#include "DataView.h"
#include "Disassembler.h"
#include "FTLCompileBinaryOp.h"
#include "FTLExceptionHandlerManager.h"
#include "FTLExitThunkGenerator.h"
#include "FTLInlineCacheDescriptor.h"
#include "FTLInlineCacheSize.h"
#include "FTLJITCode.h"
#include "FTLThunks.h"
#include "FTLUnwindInfo.h"
#include "LLVMAPI.h"
#include "LinkBuffer.h"
#include "ScratchRegisterAllocator.h"

namespace JSC { namespace FTL {

using namespace DFG;

static RegisterSet usedRegistersFor(const StackMaps::Record&);

static uint8_t* mmAllocateCodeSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned, const char* sectionName)
{
    State& state = *static_cast<State*>(opaqueState);
    
    RELEASE_ASSERT(alignment <= jitAllocationGranule);
    
    RefPtr<ExecutableMemoryHandle> result =
        state.graph.m_vm.executableAllocator.allocate(
            state.graph.m_vm, size, state.graph.m_codeBlock, JITCompilationCanFail);
    
    if (!result) {
        // Signal failure. This compilation will get tossed.
        state.allocationFailed = true;
        
        // Fake an allocation, since LLVM cannot handle failures in the memory manager.
        RefPtr<DataSection> fakeSection = adoptRef(new DataSection(size, jitAllocationGranule));
        state.jitCode->addDataSection(fakeSection);
        return bitwise_cast<uint8_t*>(fakeSection->base());
    }
    
    // LLVM used to put __compact_unwind in a code section. We keep this here defensively,
    // for clients that use older LLVMs.
    if (!strcmp(sectionName, SECTION_NAME("compact_unwind"))) {
        state.unwindDataSection = result->start();
        state.unwindDataSectionSize = result->sizeInBytes();
    }
    
    state.jitCode->addHandle(result);
    state.codeSectionNames.append(sectionName);
    
    return static_cast<uint8_t*>(result->start());
}

static uint8_t* mmAllocateDataSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned sectionID,
    const char* sectionName, LLVMBool isReadOnly)
{
    UNUSED_PARAM(sectionID);
    UNUSED_PARAM(isReadOnly);

    // Allocate the GOT in the code section to make it reachable for all code.
    if (!strcmp(sectionName, SECTION_NAME("got")))
        return mmAllocateCodeSection(opaqueState, size, alignment, sectionID, sectionName);

    State& state = *static_cast<State*>(opaqueState);

    RefPtr<DataSection> section = adoptRef(new DataSection(size, alignment));

    if (!strcmp(sectionName, SECTION_NAME("llvm_stackmaps")))
        state.stackmapsSection = section;
    else {
        state.jitCode->addDataSection(section);
        state.dataSectionNames.append(sectionName);
#if OS(DARWIN)
        if (!strcmp(sectionName, SECTION_NAME("compact_unwind"))) {
#elif OS(LINUX)
        if (!strcmp(sectionName, SECTION_NAME("eh_frame"))) {
#else
#error "Unrecognized OS"
#endif
            state.unwindDataSection = section->base();
            state.unwindDataSectionSize = size;
        }
    }

    return bitwise_cast<uint8_t*>(section->base());
}

static LLVMBool mmApplyPermissions(void*, char**)
{
    return false;
}

static void mmDestroy(void*)
{
}

static void dumpDataSection(DataSection* section, const char* prefix)
{
    for (unsigned j = 0; j < section->size() / sizeof(int64_t); ++j) {
        char buf[32];
        int64_t* wordPointer = static_cast<int64_t*>(section->base()) + j;
        snprintf(buf, sizeof(buf), "0x%lx", static_cast<unsigned long>(bitwise_cast<uintptr_t>(wordPointer)));
        dataLogF("%s%16s: 0x%016llx\n", prefix, buf, static_cast<long long>(*wordPointer));
    }
}

static int offsetOfStackRegion(StackMaps::RecordMap& recordMap, uint32_t stackmapID)
{
    if (stackmapID == UINT_MAX)
        return 0;
    
    StackMaps::RecordMap::iterator iter = recordMap.find(stackmapID);
    RELEASE_ASSERT(iter != recordMap.end());
    RELEASE_ASSERT(iter->value.size() == 1);
    RELEASE_ASSERT(iter->value[0].record.locations.size() == 1);
    Location capturedLocation =
        Location::forStackmaps(nullptr, iter->value[0].record.locations[0]);
    RELEASE_ASSERT(capturedLocation.kind() == Location::Register);
    RELEASE_ASSERT(capturedLocation.gpr() == GPRInfo::callFrameRegister);
    RELEASE_ASSERT(!(capturedLocation.addend() % sizeof(Register)));
    return capturedLocation.addend() / sizeof(Register);
}

static void generateInlineIfPossibleOutOfLineIfNot(State& state, VM& vm, CodeBlock* codeBlock, CCallHelpers& code, char* startOfInlineCode, size_t sizeOfInlineCode, const char* codeDescription, const std::function<void(LinkBuffer&, CCallHelpers&, bool wasCompiledInline)>& callback)
{
    std::unique_ptr<LinkBuffer> codeLinkBuffer;
    size_t actualCodeSize = code.m_assembler.buffer().codeSize();

    if (actualCodeSize <= sizeOfInlineCode) {
        LinkBuffer codeLinkBuffer(vm, code, startOfInlineCode, sizeOfInlineCode);

        // Fill the remainder of the inline space with nops to avoid confusing the disassembler.
        MacroAssembler::AssemblerType_T::fillNops(bitwise_cast<char*>(startOfInlineCode) + actualCodeSize, sizeOfInlineCode - actualCodeSize);

        callback(codeLinkBuffer, code, true);

        return;
    }

    if (Options::assertICSizing() || Options::dumpFailedICSizing()) {
        static size_t maxSize = 0;
        if (maxSize < actualCodeSize)
            maxSize = actualCodeSize;
        dataLogF("ALERT: Under-estimated FTL Inline Cache Size for %s: estimated %zu, actual %zu, max %zu\n", codeDescription, sizeOfInlineCode, actualCodeSize, maxSize);
        if (Options::assertICSizing())
            CRASH();
    }

    // If there isn't enough space in the provided inline code area, allocate out of line
    // executable memory to link the provided code. Place a jump at the beginning of the
    // inline area and jump to the out of line code. Similarly return by appending a jump
    // to the provided code that goes to the instruction after the inline code.
    // Fill the middle with nop's.
    MacroAssembler::Jump returnToMainline = code.jump();

    // Allocate out of line executable memory and link the provided code there.
    codeLinkBuffer = std::make_unique<LinkBuffer>(vm, code, codeBlock, JITCompilationMustSucceed);

    // Plant a jmp in the inline buffer to the out of line code.
    MacroAssembler callToOutOfLineCode;
    MacroAssembler::Jump jumpToOutOfLine = callToOutOfLineCode.jump();
    LinkBuffer inlineBuffer(vm, callToOutOfLineCode, startOfInlineCode, sizeOfInlineCode);
    inlineBuffer.link(jumpToOutOfLine, codeLinkBuffer->entrypoint());

    // Fill the remainder of the inline space with nops to avoid confusing the disassembler.
    MacroAssembler::AssemblerType_T::fillNops(bitwise_cast<char*>(startOfInlineCode) + inlineBuffer.size(), sizeOfInlineCode - inlineBuffer.size());

    // Link the end of the out of line code to right after the inline area.
    codeLinkBuffer->link(returnToMainline, CodeLocationLabel(MacroAssemblerCodePtr::createFromExecutableAddress(startOfInlineCode)).labelAtOffset(sizeOfInlineCode));

    callback(*codeLinkBuffer.get(), code, false);

    state.finalizer->outOfLineCodeInfos.append(OutOfLineCodeInfo(WTFMove(codeLinkBuffer), codeDescription));
}

template<typename DescriptorType>
void generateICFastPath(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, DescriptorType& ic, size_t sizeOfIC, const char* icName)
{
    VM& vm = state.graph.m_vm;

    StackMaps::RecordMap::iterator iter = recordMap.find(ic.stackmapID());
    if (iter == recordMap.end()) {
        // It was optimized out.
        return;
    }
    
    Vector<StackMaps::RecordAndIndex>& records = iter->value;
    
    RELEASE_ASSERT(records.size() == ic.m_generators.size());
    
    for (unsigned i = records.size(); i--;) {
        StackMaps::Record& record = records[i].record;
        auto generator = ic.m_generators[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        generator.generateFastPath(fastPathJIT);
        
        char* startOfIC =
            bitwise_cast<char*>(generatedFunction) + record.instructionOffset;

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, icName, [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            state.finalizer->sideCodeLinkBuffer->link(ic.m_slowPathDone[i],
                CodeLocationLabel(startOfIC + sizeOfIC));

            linkBuffer.link(generator.slowPathJump(),
                state.finalizer->sideCodeLinkBuffer->locationOf(generator.slowPathBegin()));

            generator.finalize(linkBuffer, *state.finalizer->sideCodeLinkBuffer);
        });
    }
}

static void generateCheckInICFastPath(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, CheckInDescriptor& ic, size_t sizeOfIC)
{
    VM& vm = state.graph.m_vm;

    StackMaps::RecordMap::iterator iter = recordMap.find(ic.stackmapID());
    if (iter == recordMap.end()) {
        // It was optimized out.
        return;
    }
    
    Vector<StackMaps::RecordAndIndex>& records = iter->value;
    
    RELEASE_ASSERT(records.size() == ic.m_generators.size());

    for (unsigned i = records.size(); i--;) {
        StackMaps::Record& record = records[i].record;
        auto generator = ic.m_generators[i];

        StructureStubInfo& stubInfo = *generator.m_stub;
        auto call = generator.m_slowCall;
        auto slowPathBegin = generator.m_beginLabel;

        CCallHelpers fastPathJIT(&vm, codeBlock);
        
        auto jump = fastPathJIT.patchableJump();
        auto done = fastPathJIT.label();

        char* startOfIC =
            bitwise_cast<char*>(generatedFunction) + record.instructionOffset;

        auto postLink = [&] (LinkBuffer& fastPath, CCallHelpers&, bool) {
            LinkBuffer& slowPath = *state.finalizer->sideCodeLinkBuffer;

            state.finalizer->sideCodeLinkBuffer->link(
                ic.m_slowPathDone[i], CodeLocationLabel(startOfIC + sizeOfIC));

            CodeLocationLabel slowPathBeginLoc = slowPath.locationOf(slowPathBegin);
            fastPath.link(jump, slowPathBeginLoc);

            CodeLocationCall callReturnLocation = slowPath.locationOf(call);

            stubInfo.patch.deltaCallToDone = MacroAssembler::differenceBetweenCodePtr(
                callReturnLocation, fastPath.locationOf(done));

            stubInfo.patch.deltaCallToJump = MacroAssembler::differenceBetweenCodePtr(
                callReturnLocation, fastPath.locationOf(jump));
            stubInfo.callReturnLocation = callReturnLocation;
            stubInfo.patch.deltaCallToSlowCase = MacroAssembler::differenceBetweenCodePtr(
                callReturnLocation, slowPathBeginLoc);
        };

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "CheckIn", postLink);
    }
}

static void generateBinaryOpICFastPath(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, BinaryOpDescriptor& ic)
{
    VM& vm = state.graph.m_vm;
    size_t sizeOfIC = ic.size();

    StackMaps::RecordMap::iterator iter = recordMap.find(ic.stackmapID());
    if (iter == recordMap.end())
        return; // It was optimized out.

    Vector<StackMaps::RecordAndIndex>& records = iter->value;

    RELEASE_ASSERT(records.size() == ic.m_slowPathStarts.size());

    for (unsigned i = records.size(); i--;) {
        StackMaps::Record& record = records[i].record;

        CCallHelpers fastPathJIT(&vm, codeBlock);

        GPRReg result = record.locations[0].directGPR();
        GPRReg left = record.locations[1].directGPR();
        GPRReg right = record.locations[2].directGPR();
        RegisterSet usedRegisters = usedRegistersFor(record);

        CCallHelpers::Jump done;
        CCallHelpers::Jump slowPathStart;

        generateBinaryOpFastPath(ic, fastPathJIT, result, left, right, usedRegisters, done, slowPathStart);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + record.instructionOffset;
        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, ic.name(), [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            linkBuffer.link(done, CodeLocationLabel(startOfIC + sizeOfIC));
            state.finalizer->sideCodeLinkBuffer->link(ic.m_slowPathDone[i], CodeLocationLabel(startOfIC + sizeOfIC));
            
            linkBuffer.link(slowPathStart, state.finalizer->sideCodeLinkBuffer->locationOf(ic.m_slowPathStarts[i]));
        });
    }
}

#if ENABLE(MASM_PROBE)

static void generateProbe(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, ProbeDescriptor& ic)
{
    VM& vm = state.graph.m_vm;
    size_t sizeOfIC = sizeOfProbe();

    StackMaps::RecordMap::iterator iter = recordMap.find(ic.stackmapID());
    if (iter == recordMap.end())
        return; // It was optimized out.

    CCallHelpers fastPathJIT(&vm, codeBlock);
    Vector<StackMaps::RecordAndIndex>& records = iter->value;
    for (unsigned i = records.size(); i--;) {
        StackMaps::Record& record = records[i].record;

        fastPathJIT.probe(ic.probeFunction());
        CCallHelpers::Jump done = fastPathJIT.jump();

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + record.instructionOffset;
        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "Probe", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            linkBuffer.link(done, CodeLocationLabel(startOfIC + sizeOfIC));
        });
    }
}

#endif // ENABLE(MASM_PROBE)

static RegisterSet usedRegistersFor(const StackMaps::Record& record)
{
    if (Options::assumeAllRegsInFTLICAreLive())
        return RegisterSet::allRegisters();
    return RegisterSet(record.usedRegisterSet(), RegisterSet::calleeSaveRegisters());
}

template<typename CallType>
void adjustCallICsForStackmaps(Vector<CallType>& calls, StackMaps::RecordMap& recordMap, ExceptionHandlerManager& exceptionHandlerManager)
{
    // Handling JS calls is weird: we need to ensure that we sort them by the PC in LLVM
    // generated code. That implies first pruning the ones that LLVM didn't generate.

    Vector<CallType> oldCalls;
    oldCalls.swap(calls);
    
    for (unsigned i = 0; i < oldCalls.size(); ++i) {
        CallType& call = oldCalls[i];
        
        StackMaps::RecordMap::iterator iter = recordMap.find(call.stackmapID());
        if (iter == recordMap.end())
            continue;
        
        for (unsigned j = 0; j < iter->value.size(); ++j) {
            CallType copy = call;
            copy.m_instructionOffset = iter->value[j].record.instructionOffset;
            copy.setCallSiteIndex(exceptionHandlerManager.procureCallSiteIndex(iter->value[j].index, copy));
            copy.setCorrespondingGenericUnwindOSRExit(exceptionHandlerManager.getCallOSRExit(iter->value[j].index, copy));

            calls.append(copy);
        }
    }

    std::sort(calls.begin(), calls.end());
}

static void fixFunctionBasedOnStackMaps(
    State& state, CodeBlock* codeBlock, JITCode* jitCode, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap)
{
    Graph& graph = state.graph;
    VM& vm = graph.m_vm;
    StackMaps& stackmaps = jitCode->stackmaps;

    ExceptionHandlerManager exceptionHandlerManager(state);
    
    int localsOffset = offsetOfStackRegion(recordMap, state.capturedStackmapID) + graph.m_nextMachineLocal;
    int varargsSpillSlotsOffset = offsetOfStackRegion(recordMap, state.varargsSpillSlotsStackmapID);
    int osrExitFromGenericUnwindStackSpillSlot  = offsetOfStackRegion(recordMap, state.exceptionHandlingSpillSlotStackmapID);
    jitCode->osrExitFromGenericUnwindStackSpillSlot = osrExitFromGenericUnwindStackSpillSlot;
    
    for (unsigned i = graph.m_inlineVariableData.size(); i--;) {
        InlineCallFrame* inlineCallFrame = graph.m_inlineVariableData[i].inlineCallFrame;
        
        if (inlineCallFrame->argumentCountRegister.isValid())
            inlineCallFrame->argumentCountRegister += localsOffset;
        
        for (unsigned argument = inlineCallFrame->arguments.size(); argument-- > 1;) {
            inlineCallFrame->arguments[argument] =
                inlineCallFrame->arguments[argument].withLocalsOffset(localsOffset);
        }
        
        if (inlineCallFrame->isClosureCall) {
            inlineCallFrame->calleeRecovery =
                inlineCallFrame->calleeRecovery.withLocalsOffset(localsOffset);
        }

        if (graph.hasDebuggerEnabled())
            codeBlock->setScopeRegister(codeBlock->scopeRegister() + localsOffset);
    }
    
    MacroAssembler::Label stackOverflowException;

    {
        CCallHelpers checkJIT(&vm, codeBlock);
        
        // At this point it's perfectly fair to just blow away all state and restore the
        // JS JIT view of the universe.
        checkJIT.copyCalleeSavesToVMCalleeSavesBuffer();
        checkJIT.move(MacroAssembler::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
        checkJIT.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
        MacroAssembler::Call callLookupExceptionHandler = checkJIT.call();
        checkJIT.jumpToExceptionHandler();

        stackOverflowException = checkJIT.label();
        checkJIT.copyCalleeSavesToVMCalleeSavesBuffer();
        checkJIT.move(MacroAssembler::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
        checkJIT.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
        MacroAssembler::Call callLookupExceptionHandlerFromCallerFrame = checkJIT.call();
        checkJIT.jumpToExceptionHandler();

        auto linkBuffer = std::make_unique<LinkBuffer>(
            vm, checkJIT, codeBlock, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        linkBuffer->link(callLookupExceptionHandler, FunctionPtr(lookupExceptionHandler));
        linkBuffer->link(callLookupExceptionHandlerFromCallerFrame, FunctionPtr(lookupExceptionHandlerFromCallerFrame));

        state.finalizer->handleExceptionsLinkBuffer = WTFMove(linkBuffer);
    }

    RELEASE_ASSERT(state.jitCode->osrExit.size() == 0);
    HashMap<OSRExitDescriptor*, OSRExitDescriptorImpl*> genericUnwindOSRExitDescriptors;
    for (unsigned i = 0; i < state.jitCode->osrExitDescriptors.size(); i++) {
        OSRExitDescriptor* exitDescriptor = &state.jitCode->osrExitDescriptors[i];
        auto iter = recordMap.find(exitDescriptor->m_stackmapID);
        if (iter == recordMap.end()) {
            // It was optimized out.
            continue;
        }

        OSRExitDescriptorImpl& exitDescriptorImpl = state.osrExitDescriptorImpls[i];
        if (exceptionTypeWillArriveAtOSRExitFromGenericUnwind(exitDescriptorImpl.m_exceptionType))
            genericUnwindOSRExitDescriptors.add(exitDescriptor, &exitDescriptorImpl);

        for (unsigned j = exitDescriptor->m_values.size(); j--;)
            exitDescriptor->m_values[j] = exitDescriptor->m_values[j].withLocalsOffset(localsOffset);
        for (ExitTimeObjectMaterialization* materialization : exitDescriptor->m_materializations)
            materialization->accountForLocalsOffset(localsOffset);

        for (unsigned j = 0; j < iter->value.size(); j++) {
            {
                uint32_t stackmapRecordIndex = iter->value[j].index;
                OSRExit exit(exitDescriptor, exitDescriptorImpl, stackmapRecordIndex);
                state.jitCode->osrExit.append(exit);
                state.finalizer->osrExit.append(OSRExitCompilationInfo());
            }

            OSRExit& exit = state.jitCode->osrExit.last();
            if (exit.willArriveAtExitFromIndirectExceptionCheck()) {
                StackMaps::Record& record = iter->value[j].record;
                RELEASE_ASSERT(exitDescriptorImpl.m_semanticCodeOriginForCallFrameHeader.isSet());
                CallSiteIndex callSiteIndex = state.jitCode->common.addUniqueCallSiteIndex(exitDescriptorImpl.m_semanticCodeOriginForCallFrameHeader);
                exit.m_exceptionHandlerCallSiteIndex = callSiteIndex;

                OSRExit* callOperationExit = nullptr;
                if (exitDescriptorImpl.m_exceptionType == ExceptionType::BinaryOpGenerator) {
                    exceptionHandlerManager.addNewCallOperationExit(iter->value[j].index, state.jitCode->osrExit.size() - 1);
                    callOperationExit = &exit;
                } else
                    exceptionHandlerManager.addNewExit(iter->value[j].index, state.jitCode->osrExit.size() - 1);
                
                if (exitDescriptorImpl.m_exceptionType == ExceptionType::GetById || exitDescriptorImpl.m_exceptionType == ExceptionType::PutById) {
                    // We create two different OSRExits for GetById and PutById.
                    // One exit that will be arrived at from the genericUnwind exception handler path,
                    // and the other that will be arrived at from the callOperation exception handler path.
                    // This code here generates the second callOperation variant.
                    uint32_t stackmapRecordIndex = iter->value[j].index;
                    OSRExit exit(exitDescriptor, exitDescriptorImpl, stackmapRecordIndex);
                    if (exitDescriptorImpl.m_exceptionType == ExceptionType::GetById)
                        exit.m_exceptionType = ExceptionType::GetByIdCallOperation;
                    else
                        exit.m_exceptionType = ExceptionType::PutByIdCallOperation;
                    CallSiteIndex callSiteIndex = state.jitCode->common.addUniqueCallSiteIndex(exitDescriptorImpl.m_semanticCodeOriginForCallFrameHeader);
                    exit.m_exceptionHandlerCallSiteIndex = callSiteIndex;

                    state.jitCode->osrExit.append(exit);
                    state.finalizer->osrExit.append(OSRExitCompilationInfo());

                    exceptionHandlerManager.addNewCallOperationExit(iter->value[j].index, state.jitCode->osrExit.size() - 1);
                    callOperationExit = &state.jitCode->osrExit.last();
                }

                // Subs and GetByIds have an interesting register preservation story,
                // see comment below at GetById to read about it.
                //
                // We set the registers needing spillage here because they need to be set
                // before we generate OSR exits so the exit knows to do the proper recovery.
                if (exitDescriptorImpl.m_exceptionType == ExceptionType::JSCall) {
                    // Call patchpoints might have values we want to do value recovery
                    // on inside volatile registers. We need to collect the volatile
                    // registers we want to do value recovery on here because they must
                    // be preserved to the stack before the call, that way the OSR exit
                    // exception handler can recover them into the proper registers.
                    exit.gatherRegistersToSpillForCallIfException(stackmaps, record);
                } else if (exitDescriptorImpl.m_exceptionType == ExceptionType::GetById) {
                    GPRReg result = record.locations[0].directGPR();
                    GPRReg base = record.locations[1].directGPR();
                    if (base == result)
                        callOperationExit->registersToPreserveForCallThatMightThrow.set(base);
                } else if (exitDescriptorImpl.m_exceptionType == ExceptionType::BinaryOpGenerator) {
                    GPRReg result = record.locations[0].directGPR();
                    GPRReg left = record.locations[1].directGPR();
                    GPRReg right = record.locations[2].directGPR();
                    if (result == left || result == right)
                        callOperationExit->registersToPreserveForCallThatMightThrow.set(result);
                }
            }
        }
    }
    ExitThunkGenerator exitThunkGenerator(state);
    exitThunkGenerator.emitThunks();
    if (exitThunkGenerator.didThings()) {
        RELEASE_ASSERT(state.finalizer->osrExit.size());
        
        auto linkBuffer = std::make_unique<LinkBuffer>(
            vm, exitThunkGenerator, codeBlock, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        
        RELEASE_ASSERT(state.finalizer->osrExit.size() == state.jitCode->osrExit.size());
        
        codeBlock->clearExceptionHandlers();

        for (unsigned i = 0; i < state.jitCode->osrExit.size(); ++i) {
            OSRExitCompilationInfo& info = state.finalizer->osrExit[i];
            OSRExit& exit = state.jitCode->osrExit[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling OSR stackmap #", exit.m_descriptor->m_stackmapID, " for ", exit.m_codeOrigin, "\n");

            info.m_thunkAddress = linkBuffer->locationOf(info.m_thunkLabel);
            exit.m_patchableCodeOffset = linkBuffer->offsetOf(info.m_thunkJump);

            if (exit.willArriveAtOSRExitFromGenericUnwind()) {
                HandlerInfo newHandler = genericUnwindOSRExitDescriptors.get(exit.m_descriptor)->m_baselineExceptionHandler;
                newHandler.start = exit.m_exceptionHandlerCallSiteIndex.bits();
                newHandler.end = exit.m_exceptionHandlerCallSiteIndex.bits() + 1;
                newHandler.nativeCode = info.m_thunkAddress;
                codeBlock->appendExceptionHandler(newHandler);
            }

            if (verboseCompilationEnabled()) {
                DumpContext context;
                dataLog("    Exit values: ", inContext(exit.m_descriptor->m_values, &context), "\n");
                if (!exit.m_descriptor->m_materializations.isEmpty()) {
                    dataLog("    Materializations: \n");
                    for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor->m_materializations)
                        dataLog("        Materialize(", pointerDump(materialization), ")\n");
                }
            }
        }
        
        state.finalizer->exitThunksLinkBuffer = WTFMove(linkBuffer);
    }

    if (!state.getByIds.isEmpty()
        || !state.putByIds.isEmpty()
        || !state.checkIns.isEmpty()
        || !state.binaryOps.isEmpty()
        || !state.lazySlowPaths.isEmpty()) {
        CCallHelpers slowPathJIT(&vm, codeBlock);
        
        CCallHelpers::JumpList exceptionTarget;

        Vector<std::pair<CCallHelpers::JumpList, CodeLocationLabel>> exceptionJumpsToLink;
        auto addNewExceptionJumpIfNecessary = [&] (uint32_t recordIndex) {
            CodeLocationLabel exceptionTarget = exceptionHandlerManager.callOperationExceptionTarget(recordIndex);
            if (!exceptionTarget)
                return false;
            exceptionJumpsToLink.append(
                std::make_pair(CCallHelpers::JumpList(), exceptionTarget));
            return true;
        };
        
        for (unsigned i = state.getByIds.size(); i--;) {
            GetByIdDescriptor& getById = state.getByIds[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling GetById stackmap #", getById.stackmapID(), "\n");
            
            auto iter = recordMap.find(getById.stackmapID());
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            
            CodeOrigin codeOrigin = getById.codeOrigin();
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i].record;
            
                RegisterSet usedRegisters = usedRegistersFor(record);
                
                GPRReg result = record.locations[0].directGPR();
                GPRReg base = record.locations[1].directGPR();
                
                JITGetByIdGenerator gen(
                    codeBlock, codeOrigin, exceptionHandlerManager.procureCallSiteIndex(iter->value[i].index, codeOrigin), usedRegisters, JSValueRegs(base),
                    JSValueRegs(result));
                
                bool addedUniqueExceptionJump = addNewExceptionJumpIfNecessary(iter->value[i].index);
                MacroAssembler::Label begin = slowPathJIT.label();
                if (result == base) {
                    // This situation has a really interesting story. We may have a GetById inside
                    // a try block where LLVM assigns the result and the base to the same register.
                    // The inline cache may miss and we may end up at this slow path callOperation. 
                    // Then, suppose the base and the result are both the same register, so the return
                    // value of the C call gets stored into the original base register. If the operationGetByIdOptimize
                    // throws, it will return "undefined" and we will be stuck with "undefined" in the base
                    // register that we would like to do value recovery on. We combat this situation from ever
                    // taking place by ensuring we spill the original base value and then recover it from
                    // the spill slot as the first step in OSR exit.
                    if (OSRExit* exit = exceptionHandlerManager.callOperationOSRExit(iter->value[i].index))
                        exit->spillRegistersToSpillSlot(slowPathJIT, osrExitFromGenericUnwindStackSpillSlot);
                }
                MacroAssembler::Call call = callOperation(
                    state, usedRegisters, slowPathJIT, codeOrigin, addedUniqueExceptionJump ? &exceptionJumpsToLink.last().first : &exceptionTarget,
                    operationGetByIdOptimize, result, CCallHelpers::TrustedImmPtr(gen.stubInfo()),
                    base, CCallHelpers::TrustedImmPtr(getById.uid())).call();

                gen.reportSlowPathCall(begin, call);

                getById.m_slowPathDone.append(slowPathJIT.jump());
                getById.m_generators.append(gen);
            }
        }
        
        for (unsigned i = state.putByIds.size(); i--;) {
            PutByIdDescriptor& putById = state.putByIds[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling PutById stackmap #", putById.stackmapID(), "\n");
            
            auto iter = recordMap.find(putById.stackmapID());
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            
            CodeOrigin codeOrigin = putById.codeOrigin();
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i].record;
                
                RegisterSet usedRegisters = usedRegistersFor(record);
                
                GPRReg base = record.locations[0].directGPR();
                GPRReg value = record.locations[1].directGPR();
                
                JITPutByIdGenerator gen(
                    codeBlock, codeOrigin, exceptionHandlerManager.procureCallSiteIndex(iter->value[i].index, codeOrigin), usedRegisters, JSValueRegs(base),
                    JSValueRegs(value), GPRInfo::patchpointScratchRegister, putById.ecmaMode(), putById.putKind());
                
                bool addedUniqueExceptionJump = addNewExceptionJumpIfNecessary(iter->value[i].index);

                MacroAssembler::Label begin = slowPathJIT.label();

                MacroAssembler::Call call = callOperation(
                    state, usedRegisters, slowPathJIT, codeOrigin, addedUniqueExceptionJump ? &exceptionJumpsToLink.last().first : &exceptionTarget,
                    gen.slowPathFunction(), InvalidGPRReg,
                    CCallHelpers::TrustedImmPtr(gen.stubInfo()), value, base,
                    CCallHelpers::TrustedImmPtr(putById.uid())).call();
                
                gen.reportSlowPathCall(begin, call);
                
                putById.m_slowPathDone.append(slowPathJIT.jump());
                putById.m_generators.append(gen);
            }
        }

        for (unsigned i = state.checkIns.size(); i--;) {
            CheckInDescriptor& checkIn = state.checkIns[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling checkIn stackmap #", checkIn.stackmapID(), "\n");
            
            auto iter = recordMap.find(checkIn.stackmapID());
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            
            CodeOrigin codeOrigin = checkIn.codeOrigin();
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i].record;
                RegisterSet usedRegisters = usedRegistersFor(record);
                GPRReg result = record.locations[0].directGPR();
                GPRReg obj = record.locations[1].directGPR();
                StructureStubInfo* stubInfo = codeBlock->addStubInfo(AccessType::In); 
                stubInfo->codeOrigin = codeOrigin;
                stubInfo->callSiteIndex = state.jitCode->common.addUniqueCallSiteIndex(codeOrigin);
                stubInfo->patch.baseGPR = static_cast<int8_t>(obj);
                stubInfo->patch.valueGPR = static_cast<int8_t>(result);
                stubInfo->patch.usedRegisters = usedRegisters;

                MacroAssembler::Label begin = slowPathJIT.label();

                MacroAssembler::Call slowCall = callOperation(
                    state, usedRegisters, slowPathJIT, codeOrigin, &exceptionTarget,
                    operationInOptimize, result, CCallHelpers::TrustedImmPtr(stubInfo), obj,
                    CCallHelpers::TrustedImmPtr(checkIn.uid())).call();

                checkIn.m_slowPathDone.append(slowPathJIT.jump());
                
                checkIn.m_generators.append(CheckInGenerator(stubInfo, slowCall, begin));
            }
        }

        for (size_t i = state.binaryOps.size(); i--;) {
            BinaryOpDescriptor& binaryOp = state.binaryOps[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling ", binaryOp.name(), " stackmap #", binaryOp.stackmapID(), "\n");
            
            auto iter = recordMap.find(binaryOp.stackmapID());
            if (iter == recordMap.end())
                continue; // It was optimized out.
            
            CodeOrigin codeOrigin = binaryOp.codeOrigin();
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i].record;
                RegisterSet usedRegisters = usedRegistersFor(record);

                GPRReg result = record.locations[0].directGPR();
                GPRReg left = record.locations[1].directGPR();
                GPRReg right = record.locations[2].directGPR();

                binaryOp.m_slowPathStarts.append(slowPathJIT.label());
                bool addedUniqueExceptionJump = addNewExceptionJumpIfNecessary(iter->value[i].index);
                if (result == left || result == right) {
                    // This situation has a really interesting register preservation story.
                    // See comment above for GetByIds.
                    if (OSRExit* exit = exceptionHandlerManager.callOperationOSRExit(iter->value[i].index))
                        exit->spillRegistersToSpillSlot(slowPathJIT, osrExitFromGenericUnwindStackSpillSlot);
                }

                callOperation(state, usedRegisters, slowPathJIT, codeOrigin, addedUniqueExceptionJump ? &exceptionJumpsToLink.last().first : &exceptionTarget,
                    binaryOp.slowPathFunction(), result, left, right).call();

                binaryOp.m_slowPathDone.append(slowPathJIT.jump());
            }
        }

        for (unsigned i = state.lazySlowPaths.size(); i--;) {
            LazySlowPathDescriptor& descriptor = state.lazySlowPaths[i];

            if (verboseCompilationEnabled())
                dataLog("Handling lazySlowPath stackmap #", descriptor.stackmapID(), "\n");

            auto iter = recordMap.find(descriptor.stackmapID());
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            CodeOrigin codeOrigin = descriptor.codeOrigin();
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i].record;
                RegisterSet usedRegisters = usedRegistersFor(record);
                char* startOfIC =
                    bitwise_cast<char*>(generatedFunction) + record.instructionOffset;
                CodeLocationLabel patchpoint((MacroAssemblerCodePtr(startOfIC)));
                CodeLocationLabel exceptionTarget = exceptionHandlerManager.lazySlowPathExceptionTarget(iter->value[i].index);
                if (!exceptionTarget)
                    exceptionTarget = state.finalizer->handleExceptionsLinkBuffer->entrypoint();

                ScratchRegisterAllocator scratchAllocator(usedRegisters);
                GPRReg newZero = InvalidGPRReg;
                Vector<Location> locations;
                for (auto stackmapLocation : record.locations) {
                    FTL::Location location = Location::forStackmaps(&stackmaps, stackmapLocation);
                    if (isARM64()) {
                        // If LLVM proves that something is zero, it may pass us the zero register (aka, the stack pointer). Our assembler
                        // isn't prepared to handle this well. We need to move it into a different register if such a case arises.
                        if (location.isGPR() && location.gpr() == MacroAssembler::stackPointerRegister) {
                            if (newZero == InvalidGPRReg) {
                                newZero = scratchAllocator.allocateScratchGPR();
                                usedRegisters.set(newZero);
                            }
                            location = FTL::Location::forRegister(DWARFRegister(static_cast<uint16_t>(newZero)), 0); // DWARF GPRs for arm64 are sensibly numbered.
                        }
                    }
                    locations.append(location);
                }

                std::unique_ptr<LazySlowPath> lazySlowPath = std::make_unique<LazySlowPath>(
                    patchpoint, exceptionTarget, usedRegisters, exceptionHandlerManager.procureCallSiteIndex(iter->value[i].index, codeOrigin),
                    descriptor.m_linker->run(locations), newZero, scratchAllocator);

                CCallHelpers::Label begin = slowPathJIT.label();

                slowPathJIT.pushToSaveImmediateWithoutTouchingRegisters(
                    CCallHelpers::TrustedImm32(state.jitCode->lazySlowPaths.size()));
                CCallHelpers::Jump generatorJump = slowPathJIT.jump();
                
                descriptor.m_generators.append(std::make_tuple(lazySlowPath.get(), begin));

                state.jitCode->lazySlowPaths.append(WTFMove(lazySlowPath));
                state.finalizer->lazySlowPathGeneratorJumps.append(generatorJump);
            }
        }
        
        exceptionTarget.link(&slowPathJIT);
        MacroAssembler::Jump exceptionJump = slowPathJIT.jump();
        
        state.finalizer->sideCodeLinkBuffer = std::make_unique<LinkBuffer>(vm, slowPathJIT, codeBlock, JITCompilationCanFail);
        if (state.finalizer->sideCodeLinkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        state.finalizer->sideCodeLinkBuffer->link(
            exceptionJump, state.finalizer->handleExceptionsLinkBuffer->entrypoint());
        
        for (unsigned i = state.getByIds.size(); i--;) {
            generateICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.getByIds[i],
                sizeOfGetById(), "GetById");
        }
        for (unsigned i = state.putByIds.size(); i--;) {
            generateICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.putByIds[i],
                sizeOfPutById(), "PutById");
        }
        for (unsigned i = state.checkIns.size(); i--;) {
            generateCheckInICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.checkIns[i],
                sizeOfIn()); 
        }
        for (unsigned i = state.binaryOps.size(); i--;) {
            BinaryOpDescriptor& binaryOp = state.binaryOps[i];
            generateBinaryOpICFastPath(state, codeBlock, generatedFunction, recordMap, binaryOp);
        }
        for (unsigned i = state.lazySlowPaths.size(); i--;) {
            LazySlowPathDescriptor& lazySlowPath = state.lazySlowPaths[i];
            for (auto& tuple : lazySlowPath.m_generators) {
                MacroAssembler::replaceWithJump(
                    std::get<0>(tuple)->patchpoint(),
                    state.finalizer->sideCodeLinkBuffer->locationOf(std::get<1>(tuple)));
            }
        }
#if ENABLE(MASM_PROBE)
        for (unsigned i = state.probes.size(); i--;) {
            ProbeDescriptor& probe = state.probes[i];
            generateProbe(state, codeBlock, generatedFunction, recordMap, probe);
        }
#endif
        for (auto& pair : exceptionJumpsToLink)
            state.finalizer->sideCodeLinkBuffer->link(pair.first, pair.second);
    }
    
    adjustCallICsForStackmaps(state.jsCalls, recordMap, exceptionHandlerManager);
    
    for (unsigned i = state.jsCalls.size(); i--;) {
        JSCall& call = state.jsCalls[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(fastPathJIT, state, osrExitFromGenericUnwindStackSpillSlot);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfCall(), "JSCall", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            call.link(vm, linkBuffer);
        });
    }
    
    adjustCallICsForStackmaps(state.jsCallVarargses, recordMap, exceptionHandlerManager);
    
    for (unsigned i = state.jsCallVarargses.size(); i--;) {
        JSCallVarargs& call = state.jsCallVarargses[i];
        
        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(fastPathJIT, state, varargsSpillSlotsOffset, osrExitFromGenericUnwindStackSpillSlot);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;
        size_t sizeOfIC = sizeOfICFor(call.node());

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "varargs call", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            call.link(vm, linkBuffer, state.finalizer->handleExceptionsLinkBuffer->entrypoint());
        });
    }

    adjustCallICsForStackmaps(state.jsTailCalls, recordMap, exceptionHandlerManager);

    for (unsigned i = state.jsTailCalls.size(); i--;) {
        JSTailCall& call = state.jsTailCalls[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(*state.jitCode.get(), fastPathJIT);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;
        size_t sizeOfIC = call.estimatedSize();

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "tail call", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            call.link(vm, linkBuffer);
        });
    }
    
    auto iter = recordMap.find(state.handleStackOverflowExceptionStackmapID);
    // It's sort of remotely possible that we won't have an in-band exception handling
    // path, for some kinds of functions.
    if (iter != recordMap.end()) {
        for (unsigned i = iter->value.size(); i--;) {
            StackMaps::Record& record = iter->value[i].record;
            
            CodeLocationLabel source = CodeLocationLabel(
                bitwise_cast<char*>(generatedFunction) + record.instructionOffset);

            RELEASE_ASSERT(stackOverflowException.isSet());

            MacroAssembler::replaceWithJump(source, state.finalizer->handleExceptionsLinkBuffer->locationOf(stackOverflowException));
        }
    }
    
    iter = recordMap.find(state.handleExceptionStackmapID);
    // It's sort of remotely possible that we won't have an in-band exception handling
    // path, for some kinds of functions.
    if (iter != recordMap.end()) {
        for (unsigned i = iter->value.size(); i--;) {
            StackMaps::Record& record = iter->value[i].record;
            
            CodeLocationLabel source = CodeLocationLabel(
                bitwise_cast<char*>(generatedFunction) + record.instructionOffset);
            
            MacroAssembler::replaceWithJump(source, state.finalizer->handleExceptionsLinkBuffer->entrypoint());
        }
    }
    
    for (unsigned exitIndex = 0; exitIndex < jitCode->osrExit.size(); ++exitIndex) {
        OSRExitCompilationInfo& info = state.finalizer->osrExit[exitIndex];
        OSRExit& exit = jitCode->osrExit[exitIndex];
        Vector<const void*> codeAddresses;

        if (exit.willArriveAtExitFromIndirectExceptionCheck()) // This jump doesn't happen directly from a patchpoint/stackmap we compile. It happens indirectly through an exception check somewhere.
            continue;
        
        StackMaps::Record& record = jitCode->stackmaps.records[exit.m_stackmapRecordIndex];
        
        CodeLocationLabel source = CodeLocationLabel(
            bitwise_cast<char*>(generatedFunction) + record.instructionOffset);
        
        codeAddresses.append(bitwise_cast<char*>(generatedFunction) + record.instructionOffset + MacroAssembler::maxJumpReplacementSize());
        
        if (exit.m_descriptor->m_isInvalidationPoint)
            jitCode->common.jumpReplacements.append(JumpReplacement(source, info.m_thunkAddress));
        else
            MacroAssembler::replaceWithJump(source, info.m_thunkAddress);
        
        if (graph.compilation())
            graph.compilation()->addOSRExitSite(codeAddresses);
    }
}

void compile(State& state, Safepoint::Result& safepointResult)
{
    char* error = 0;
    
    {
        GraphSafepoint safepoint(state.graph, safepointResult);
        
        LLVMMCJITCompilerOptions options;
        llvm->InitializeMCJITCompilerOptions(&options, sizeof(options));
        options.OptLevel = Options::llvmBackendOptimizationLevel();
        options.NoFramePointerElim = true;
        if (Options::useLLVMSmallCodeModel())
            options.CodeModel = LLVMCodeModelSmall;
        options.EnableFastISel = enableLLVMFastISel;
        options.MCJMM = llvm->CreateSimpleMCJITMemoryManager(
            &state, mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);
    
        LLVMExecutionEngineRef engine;
        
        if (isARM64()) {
#if OS(DARWIN)
            llvm->SetTarget(state.module, "arm64-apple-ios");
#elif OS(LINUX)
            llvm->SetTarget(state.module, "aarch64-linux-gnu");
#else
#error "Unrecognized OS"
#endif
        }

        if (llvm->CreateMCJITCompilerForModule(&engine, state.module, &options, sizeof(options), &error)) {
            dataLog("FATAL: Could not create LLVM execution engine: ", error, "\n");
            CRASH();
        }
        
        // At this point we no longer own the module.
        LModule module = state.module;
        state.module = nullptr;

        // The data layout also has to be set in the module. Get the data layout from the MCJIT and apply
        // it to the module.
        LLVMTargetMachineRef targetMachine = llvm->GetExecutionEngineTargetMachine(engine);
        LLVMTargetDataRef targetData = llvm->GetExecutionEngineTargetData(engine);
        char* stringRepOfTargetData = llvm->CopyStringRepOfTargetData(targetData);
        llvm->SetDataLayout(module, stringRepOfTargetData);
        free(stringRepOfTargetData);

        LLVMPassManagerRef functionPasses = 0;
        LLVMPassManagerRef modulePasses;

        if (Options::llvmSimpleOpt()) {
            modulePasses = llvm->CreatePassManager();
            llvm->AddTargetData(targetData, modulePasses);
            llvm->AddAnalysisPasses(targetMachine, modulePasses);
            llvm->AddPromoteMemoryToRegisterPass(modulePasses);
            llvm->AddGlobalOptimizerPass(modulePasses);
            llvm->AddFunctionInliningPass(modulePasses);
            llvm->AddPruneEHPass(modulePasses);
            llvm->AddGlobalDCEPass(modulePasses);
            llvm->AddConstantPropagationPass(modulePasses);
            llvm->AddAggressiveDCEPass(modulePasses);
            llvm->AddInstructionCombiningPass(modulePasses);
            // BEGIN - DO NOT CHANGE THE ORDER OF THE ALIAS ANALYSIS PASSES
            llvm->AddTypeBasedAliasAnalysisPass(modulePasses);
            llvm->AddBasicAliasAnalysisPass(modulePasses);
            // END - DO NOT CHANGE THE ORDER OF THE ALIAS ANALYSIS PASSES
            llvm->AddGVNPass(modulePasses);
            llvm->AddCFGSimplificationPass(modulePasses);
            llvm->AddDeadStoreEliminationPass(modulePasses);
            
            if (enableLLVMFastISel)
                llvm->AddLowerSwitchPass(modulePasses);

            llvm->RunPassManager(modulePasses, module);
        } else {
            LLVMPassManagerBuilderRef passBuilder = llvm->PassManagerBuilderCreate();
            llvm->PassManagerBuilderSetOptLevel(passBuilder, Options::llvmOptimizationLevel());
            llvm->PassManagerBuilderUseInlinerWithThreshold(passBuilder, 275);
            llvm->PassManagerBuilderSetSizeLevel(passBuilder, Options::llvmSizeLevel());
        
            functionPasses = llvm->CreateFunctionPassManagerForModule(module);
            modulePasses = llvm->CreatePassManager();
        
            llvm->AddTargetData(llvm->GetExecutionEngineTargetData(engine), modulePasses);
        
            llvm->PassManagerBuilderPopulateFunctionPassManager(passBuilder, functionPasses);
            llvm->PassManagerBuilderPopulateModulePassManager(passBuilder, modulePasses);
        
            llvm->PassManagerBuilderDispose(passBuilder);
        
            llvm->InitializeFunctionPassManager(functionPasses);
            for (LValue function = llvm->GetFirstFunction(module); function; function = llvm->GetNextFunction(function))
                llvm->RunFunctionPassManager(functionPasses, function);
            llvm->FinalizeFunctionPassManager(functionPasses);
        
            llvm->RunPassManager(modulePasses, module);
        }

        if (shouldDumpDisassembly() || verboseCompilationEnabled())
            state.dumpState(module, "after optimization");
        
        // FIXME: Need to add support for the case where JIT memory allocation failed.
        // https://bugs.webkit.org/show_bug.cgi?id=113620
        state.generatedFunction = reinterpret_cast<GeneratedFunction>(llvm->GetPointerToGlobal(engine, state.function));
        if (functionPasses)
            llvm->DisposePassManager(functionPasses);
        llvm->DisposePassManager(modulePasses);
        llvm->DisposeExecutionEngine(engine);
    }

    if (safepointResult.didGetCancelled())
        return;
    RELEASE_ASSERT(!state.graph.m_vm.heap.isCollecting());
    
    if (state.allocationFailed)
        return;
    
    if (shouldDumpDisassembly()) {
        for (unsigned i = 0; i < state.jitCode->handles().size(); ++i) {
            ExecutableMemoryHandle* handle = state.jitCode->handles()[i].get();
            dataLog(
                "Generated LLVM code for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT),
                " #", i, ", ", state.codeSectionNames[i], ":\n");
            disassemble(
                MacroAssemblerCodePtr(handle->start()), handle->sizeInBytes(),
                "    ", WTF::dataFile(), LLVMSubset);
        }
        
        for (unsigned i = 0; i < state.jitCode->dataSections().size(); ++i) {
            DataSection* section = state.jitCode->dataSections()[i].get();
            dataLog(
                "Generated LLVM data section for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT),
                " #", i, ", ", state.dataSectionNames[i], ":\n");
            dumpDataSection(section, "    ");
        }
    }
    
    std::unique_ptr<RegisterAtOffsetList> registerOffsets = parseUnwindInfo(
        state.unwindDataSection, state.unwindDataSectionSize,
        state.generatedFunction);
    if (shouldDumpDisassembly()) {
        dataLog("Unwind info for ", CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT), ":\n");
        dataLog("    ", *registerOffsets, "\n");
    }
    state.graph.m_codeBlock->setCalleeSaveRegisters(WTFMove(registerOffsets));
    
    if (state.stackmapsSection && state.stackmapsSection->size()) {
        if (shouldDumpDisassembly()) {
            dataLog(
                "Generated LLVM stackmaps section for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT), ":\n");
            dataLog("    Raw data:\n");
            dumpDataSection(state.stackmapsSection.get(), "    ");
        }
        
        RefPtr<DataView> stackmapsData = DataView::create(
            ArrayBuffer::create(state.stackmapsSection->base(), state.stackmapsSection->size()));
        state.jitCode->stackmaps.parse(stackmapsData.get());
    
        if (shouldDumpDisassembly()) {
            dataLog("    Structured data:\n");
            state.jitCode->stackmaps.dumpMultiline(WTF::dataFile(), "        ");
        }
        
        StackMaps::RecordMap recordMap = state.jitCode->stackmaps.computeRecordMap();
        fixFunctionBasedOnStackMaps(
            state, state.graph.m_codeBlock, state.jitCode.get(), state.generatedFunction,
            recordMap);
        if (state.allocationFailed)
            return;
        
        if (shouldDumpDisassembly() || Options::asyncDisassembly()) {
            for (unsigned i = 0; i < state.jitCode->handles().size(); ++i) {
                if (state.codeSectionNames[i] != SECTION_NAME("text"))
                    continue;
                
                ExecutableMemoryHandle* handle = state.jitCode->handles()[i].get();
                
                CString header = toCString(
                    "Generated LLVM code after stackmap-based fix-up for ",
                    CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT),
                    " in ", state.graph.m_plan.mode, " #", i, ", ",
                    state.codeSectionNames[i], ":\n");
                
                if (Options::asyncDisassembly()) {
                    disassembleAsynchronously(
                        header, MacroAssemblerCodeRef(handle), handle->sizeInBytes(), "    ",
                        LLVMSubset);
                    continue;
                }
                
                dataLog(header);
                disassemble(
                    MacroAssemblerCodePtr(handle->start()), handle->sizeInBytes(),
                    "    ", WTF::dataFile(), LLVMSubset);
            }
        }
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && FTL_USES_B3

