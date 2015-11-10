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

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "CCallHelpers.h"
#include "DFGCommon.h"
#include "DFGGraphSafepoint.h"
#include "DFGOperations.h"
#include "DataView.h"
#include "Disassembler.h"
#include "FTLExitThunkGenerator.h"
#include "FTLInlineCacheSize.h"
#include "FTLJITCode.h"
#include "FTLThunks.h"
#include "FTLUnwindInfo.h"
#include "JITSubGenerator.h"
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

    state.finalizer->outOfLineCodeInfos.append(OutOfLineCodeInfo(WTF::move(codeLinkBuffer), codeDescription));
}

template<typename DescriptorType>
void generateICFastPath(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, DescriptorType& ic, size_t sizeOfIC)
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

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "inline cache fast path", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
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

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "CheckIn inline cache", postLink);
    }
}

class BinarySnippetRegisterContext {
    // The purpose of this class is to shuffle registers to get them into the state
    // that baseline code expects so that we can use the baseline snippet generators i.e.
    //    1. ensure that the inputs and outputs are not in tag or scratch registers.
    //    2. tag registers are loaded with the expected values.
    //
    // We also need to:
    //    1. restore the input and tag registers to the values that LLVM put there originally.
    //    2. that is except when one of the input registers is also the result register.
    //       In this case, we don't want to trash the result, and hence, should not restore into it.

public:
    BinarySnippetRegisterContext(ScratchRegisterAllocator& allocator, GPRReg& result, GPRReg& left, GPRReg& right)
        : m_allocator(allocator)
        , m_result(result)
        , m_left(left)
        , m_right(right)
        , m_origResult(result)
        , m_origLeft(left)
        , m_origRight(right)
    {
        m_allocator.lock(m_result);
        m_allocator.lock(m_left);
        m_allocator.lock(m_right);

        RegisterSet inputRegisters = RegisterSet(m_left, m_right);
        RegisterSet inputAndOutputRegisters = RegisterSet(inputRegisters, m_result);

        RegisterSet reservedRegisters;
        for (GPRReg reg : GPRInfo::reservedRegisters())
            reservedRegisters.set(reg);

        if (reservedRegisters.get(m_left))
            m_left = m_allocator.allocateScratchGPR();
        if (reservedRegisters.get(m_right))
            m_right = m_allocator.allocateScratchGPR();
        if (!inputRegisters.get(m_result) && reservedRegisters.get(m_result))
            m_result = m_allocator.allocateScratchGPR();
        
        if (!inputAndOutputRegisters.get(GPRInfo::tagMaskRegister))
            m_savedTagMaskRegister = m_allocator.allocateScratchGPR();
        if (!inputAndOutputRegisters.get(GPRInfo::tagTypeNumberRegister))
            m_savedTagTypeNumberRegister = m_allocator.allocateScratchGPR();
    }

    void initializeRegisters(CCallHelpers& jit)
    {
        if (m_left != m_origLeft)
            jit.move(m_origLeft, m_left);
        if (m_right != m_origRight)
            jit.move(m_origRight, m_right);

        if (m_savedTagMaskRegister != InvalidGPRReg)
            jit.move(GPRInfo::tagMaskRegister, m_savedTagMaskRegister);
        if (m_savedTagTypeNumberRegister != InvalidGPRReg)
            jit.move(GPRInfo::tagTypeNumberRegister, m_savedTagTypeNumberRegister);

        jit.emitMaterializeTagCheckRegisters();
    }

    void restoreRegisters(CCallHelpers& jit)
    {
        if (m_origLeft != m_left && m_origLeft != m_origResult)
            jit.move(m_left, m_origLeft);
        if (m_origRight != m_right && m_origRight != m_origResult)
            jit.move(m_right, m_origRight);
        
        if (m_savedTagMaskRegister != InvalidGPRReg)
            jit.move(m_savedTagMaskRegister, GPRInfo::tagMaskRegister);
        if (m_savedTagTypeNumberRegister != InvalidGPRReg)
            jit.move(m_savedTagTypeNumberRegister, GPRInfo::tagTypeNumberRegister);
    }

private:
    ScratchRegisterAllocator& m_allocator;

    GPRReg& m_result;
    GPRReg& m_left;
    GPRReg& m_right;

    GPRReg m_origResult;
    GPRReg m_origLeft;
    GPRReg m_origRight;

    GPRReg m_savedTagMaskRegister { InvalidGPRReg };
    GPRReg m_savedTagTypeNumberRegister { InvalidGPRReg };
};

static void generateArithSubICFastPath(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, ArithSubDescriptor& ic)
{
    VM& vm = state.graph.m_vm;
    size_t sizeOfIC = sizeOfArithSub();

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
        ScratchRegisterAllocator allocator(usedRegisters);

        BinarySnippetRegisterContext context(allocator, result, left, right);

        GPRReg scratchGPR = allocator.allocateScratchGPR();
        FPRReg leftFPR = allocator.allocateScratchFPR();
        FPRReg rightFPR = allocator.allocateScratchFPR();
        FPRReg scratchFPR = InvalidFPRReg;

        JITSubGenerator gen(JSValueRegs(result), JSValueRegs(left), JSValueRegs(right), ic.leftType(), ic.rightType(), leftFPR, rightFPR, scratchGPR, scratchFPR);

        auto numberOfBytesUsedToPreserveReusedRegisters =
            allocator.preserveReusedRegistersByPushing(fastPathJIT, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        context.initializeRegisters(fastPathJIT);
        gen.generateFastPath(fastPathJIT);

        gen.endJumpList().link(&fastPathJIT);
        context.restoreRegisters(fastPathJIT);
        allocator.restoreReusedRegistersByPopping(fastPathJIT, numberOfBytesUsedToPreserveReusedRegisters,
            ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);
        CCallHelpers::Jump done = fastPathJIT.jump();

        gen.slowPathJumpList().link(&fastPathJIT);
        context.restoreRegisters(fastPathJIT);
        allocator.restoreReusedRegistersByPopping(fastPathJIT, numberOfBytesUsedToPreserveReusedRegisters,
            ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);
        CCallHelpers::Jump slowPathStart = fastPathJIT.jump();

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + record.instructionOffset;
        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "ArithSub inline cache fast path", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            linkBuffer.link(done, CodeLocationLabel(startOfIC + sizeOfIC));
            state.finalizer->sideCodeLinkBuffer->link(ic.m_slowPathDone[i], CodeLocationLabel(startOfIC + sizeOfIC));
            
            linkBuffer.link(slowPathStart, state.finalizer->sideCodeLinkBuffer->locationOf(ic.m_slowPathStarts[i]));
        });
    }
}

static RegisterSet usedRegistersFor(const StackMaps::Record& record)
{
    if (Options::assumeAllRegsInFTLICAreLive())
        return RegisterSet::allRegisters();
    return RegisterSet(record.usedRegisterSet(), RegisterSet::calleeSaveRegisters());
}

template<typename CallType>
void adjustCallICsForStackmaps(Vector<CallType>& calls, StackMaps::RecordMap& recordMap, std::function<CallSiteIndex (uint32_t recordIndex, CodeOrigin origin)> generateCallSiteIndexFunction, std::function<OSRExit* (uint32_t recordIndex)> getCorrespondingOSRExit)
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
            copy.setCallSiteIndex(generateCallSiteIndexFunction(iter->value[j].index, copy.callSiteDescriptionOrigin()));
            copy.setCorrespondingGenericUnwindOSRExit(getCorrespondingOSRExit(iter->value[j].index));

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

    // We fill this when generating OSR exits that will be executed via genericUnwind()
    // or lazy slow path exception checks.
    // That way, when we assign a CallSiteIndex to the Call/GetById/PutById/LazySlowPath, we assign
    // it the proper CallSiteIndex that corresponds to the OSRExit exception handler.
    HashMap<uint32_t, size_t, WTF::IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> recordIndexToGenericUnwindOrLazySlowPathOSRExit;
    auto generateOrGetAlreadyGeneratedCallSiteIndex = [&] (uint32_t recordIndex, CodeOrigin origin) -> CallSiteIndex {
        auto findResult = recordIndexToGenericUnwindOrLazySlowPathOSRExit.find(recordIndex);
        if (findResult == recordIndexToGenericUnwindOrLazySlowPathOSRExit.end())
            return state.jitCode->common.addUniqueCallSiteIndex(origin);
        size_t osrExitIndex = findResult->value;
        return state.jitCode->osrExit[osrExitIndex].m_exceptionHandlerCallSiteIndex;
    };
    auto jsCallOSRExitForRecordIndex = [&] (uint32_t recordIndex) -> OSRExit* {
        auto findResult = recordIndexToGenericUnwindOrLazySlowPathOSRExit.find(recordIndex);
        if (findResult == recordIndexToGenericUnwindOrLazySlowPathOSRExit.end())
            return nullptr;

        size_t osrExitIndex = findResult->value;
        OSRExit& exit = state.jitCode->osrExit[osrExitIndex];
        if (!exit.m_descriptor.m_isExceptionFromJSCall)
            return nullptr;
        return &exit;
    };
    
    int localsOffset = offsetOfStackRegion(recordMap, state.capturedStackmapID) + graph.m_nextMachineLocal;
    int varargsSpillSlotsOffset = offsetOfStackRegion(recordMap, state.varargsSpillSlotsStackmapID);
    int jsCallThatMightThrowSpillOffset = offsetOfStackRegion(recordMap, state.exceptionHandlingSpillSlotStackmapID);
    
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

        state.finalizer->handleExceptionsLinkBuffer = WTF::move(linkBuffer);
    }

    RELEASE_ASSERT(state.jitCode->osrExit.size() == 0);
    for (unsigned i = 0; i < state.jitCode->osrExitDescriptors.size(); i++) {
        OSRExitDescriptor& exitDescriptor = state.jitCode->osrExitDescriptors[i];
        auto iter = recordMap.find(exitDescriptor.m_stackmapID);
        if (iter == recordMap.end()) {
            // It was optimized out.
            continue;
        }

        for (unsigned j = exitDescriptor.m_values.size(); j--;)
            exitDescriptor.m_values[j] = exitDescriptor.m_values[j].withLocalsOffset(localsOffset);
        for (ExitTimeObjectMaterialization* materialization : exitDescriptor.m_materializations)
            materialization->accountForLocalsOffset(localsOffset);

        for (unsigned j = 0; j < iter->value.size(); j++) {
            {
                uint32_t stackmapRecordIndex = iter->value[j].index;
                OSRExit exit(exitDescriptor, stackmapRecordIndex);
                state.jitCode->osrExit.append(exit);
                state.finalizer->osrExit.append(OSRExitCompilationInfo());
            }

            OSRExit& exit = state.jitCode->osrExit.last();
            if (exitDescriptor.m_willArriveAtOSRExitFromGenericUnwind || exitDescriptor.m_isExceptionFromLazySlowPath) {
                StackMaps::Record& record = iter->value[j].record;
                RELEASE_ASSERT(exit.m_descriptor.m_semanticCodeOriginForCallFrameHeader.isSet());
                CallSiteIndex callSiteIndex = state.jitCode->common.addUniqueCallSiteIndex(exit.m_descriptor.m_semanticCodeOriginForCallFrameHeader);
                exit.m_exceptionHandlerCallSiteIndex = callSiteIndex;
                recordIndexToGenericUnwindOrLazySlowPathOSRExit.add(iter->value[j].index, state.jitCode->osrExit.size() - 1);

                if (exitDescriptor.m_isExceptionFromJSCall)
                    exit.gatherRegistersToSpillForCallIfException(stackmaps, record);
                if (exitDescriptor.m_isExceptionFromGetById) {
                    GPRReg result = record.locations[0].directGPR();
                    GPRReg base = record.locations[1].directGPR();
                    // This has an interesting story, see comment below describing it.
                    if (result == base)
                        exit.registersToPreserveForCallThatMightThrow.set(base);
                }
            }
        }
    }
    ExitThunkGenerator exitThunkGenerator(state);
    exitThunkGenerator.emitThunks(jsCallThatMightThrowSpillOffset);
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
                dataLog("Handling OSR stackmap #", exit.m_descriptor.m_stackmapID, " for ", exit.m_codeOrigin, "\n");

            info.m_thunkAddress = linkBuffer->locationOf(info.m_thunkLabel);
            exit.m_patchableCodeOffset = linkBuffer->offsetOf(info.m_thunkJump);

            if (exit.m_descriptor.m_willArriveAtOSRExitFromGenericUnwind) {
                HandlerInfo newHandler = exit.m_descriptor.m_baselineExceptionHandler;
                newHandler.start = exit.m_exceptionHandlerCallSiteIndex.bits();
                newHandler.end = exit.m_exceptionHandlerCallSiteIndex.bits() + 1;
                newHandler.nativeCode = info.m_thunkAddress;
                codeBlock->appendExceptionHandler(newHandler);
            }

            if (verboseCompilationEnabled()) {
                DumpContext context;
                dataLog("    Exit values: ", inContext(exit.m_descriptor.m_values, &context), "\n");
                if (!exit.m_descriptor.m_materializations.isEmpty()) {
                    dataLog("    Materializations: \n");
                    for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor.m_materializations)
                        dataLog("        Materialize(", pointerDump(materialization), ")\n");
                }
            }
        }
        
        state.finalizer->exitThunksLinkBuffer = WTF::move(linkBuffer);
    }

    if (!state.getByIds.isEmpty()
        || !state.putByIds.isEmpty()
        || !state.checkIns.isEmpty()
        || !state.arithSubs.isEmpty()
        || !state.lazySlowPaths.isEmpty()) {
        CCallHelpers slowPathJIT(&vm, codeBlock);
        
        CCallHelpers::JumpList exceptionTarget;

        Vector<std::pair<CCallHelpers::JumpList, CodeLocationLabel>> exceptionJumpsToLink;
        auto addNewExceptionJumpIfNecessary = [&] (uint32_t recordIndex) {
            auto findResult = recordIndexToGenericUnwindOrLazySlowPathOSRExit.find(recordIndex);
            if (findResult == recordIndexToGenericUnwindOrLazySlowPathOSRExit.end())
                return false;

            size_t osrExitIndex = findResult->value;
            RELEASE_ASSERT(state.jitCode->osrExit[osrExitIndex].m_descriptor.m_willArriveAtOSRExitFromGenericUnwind);
            OSRExitCompilationInfo& info = state.finalizer->osrExit[osrExitIndex];
            RELEASE_ASSERT(info.m_getAndPutByIdCallOperationExceptionOSRExitEntrance.isSet());
            exceptionJumpsToLink.append(
                std::make_pair(CCallHelpers::JumpList(), state.finalizer->exitThunksLinkBuffer->locationOf(info.m_getAndPutByIdCallOperationExceptionOSRExitEntrance)));
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
                    codeBlock, codeOrigin, generateOrGetAlreadyGeneratedCallSiteIndex(iter->value[i].index, codeOrigin), usedRegisters, JSValueRegs(base),
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
                    auto findResult = recordIndexToGenericUnwindOrLazySlowPathOSRExit.find(iter->value[i].index);
                    if (findResult != recordIndexToGenericUnwindOrLazySlowPathOSRExit.end()) {
                        size_t osrExitIndex = findResult->value;
                        OSRExit& exit = state.jitCode->osrExit[osrExitIndex];
                        RELEASE_ASSERT(exit.m_descriptor.m_isExceptionFromGetById);
                        exit.spillRegistersToSpillSlot(slowPathJIT, jsCallThatMightThrowSpillOffset);
                    }
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
                    codeBlock, codeOrigin, generateOrGetAlreadyGeneratedCallSiteIndex(iter->value[i].index, codeOrigin), usedRegisters, JSValueRegs(base),
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

        for (size_t i = state.arithSubs.size(); i--;) {
            ArithSubDescriptor& arithSub = state.arithSubs[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling ArithSub stackmap #", arithSub.stackmapID(), "\n");
            
            auto iter = recordMap.find(arithSub.stackmapID());
            if (iter == recordMap.end())
                continue; // It was optimized out.
            
            CodeOrigin codeOrigin = arithSub.codeOrigin();
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i].record;
                RegisterSet usedRegisters = usedRegistersFor(record);

                GPRReg result = record.locations[0].directGPR();
                GPRReg left = record.locations[1].directGPR();
                GPRReg right = record.locations[2].directGPR();

                arithSub.m_slowPathStarts.append(slowPathJIT.label());

                callOperation(state, usedRegisters, slowPathJIT, codeOrigin, &exceptionTarget,
                    operationValueSub, result, left, right).call();

                arithSub.m_slowPathDone.append(slowPathJIT.jump());
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
                Vector<Location> locations;
                for (auto location : record.locations)
                    locations.append(Location::forStackmaps(&stackmaps, location));

                char* startOfIC =
                    bitwise_cast<char*>(generatedFunction) + record.instructionOffset;
                CodeLocationLabel patchpoint((MacroAssemblerCodePtr(startOfIC)));
                CodeLocationLabel exceptionTarget;
                auto findResult = recordIndexToGenericUnwindOrLazySlowPathOSRExit.find(iter->value[i].index);
                if (findResult != recordIndexToGenericUnwindOrLazySlowPathOSRExit.end()) {
                    size_t osrExitIndex = findResult->value;
                    OSRExitCompilationInfo& info = state.finalizer->osrExit[osrExitIndex];
                    RELEASE_ASSERT(state.jitCode->osrExit[osrExitIndex].m_descriptor.m_isExceptionFromLazySlowPath);
                    exceptionTarget = state.finalizer->exitThunksLinkBuffer->locationOf(info.m_thunkLabel);
                } else
                    exceptionTarget = state.finalizer->handleExceptionsLinkBuffer->entrypoint();

                std::unique_ptr<LazySlowPath> lazySlowPath = std::make_unique<LazySlowPath>(
                    patchpoint, exceptionTarget, usedRegisters, generateOrGetAlreadyGeneratedCallSiteIndex(iter->value[i].index, codeOrigin),
                    descriptor.m_linker->run(locations));

                CCallHelpers::Label begin = slowPathJIT.label();

                slowPathJIT.pushToSaveImmediateWithoutTouchingRegisters(
                    CCallHelpers::TrustedImm32(state.jitCode->lazySlowPaths.size()));
                CCallHelpers::Jump generatorJump = slowPathJIT.jump();
                
                descriptor.m_generators.append(std::make_tuple(lazySlowPath.get(), begin));

                state.jitCode->lazySlowPaths.append(WTF::move(lazySlowPath));
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
                sizeOfGetById());
        }
        for (unsigned i = state.putByIds.size(); i--;) {
            generateICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.putByIds[i],
                sizeOfPutById());
        }
        for (unsigned i = state.checkIns.size(); i--;) {
            generateCheckInICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.checkIns[i],
                sizeOfIn()); 
        }
        for (unsigned i = state.arithSubs.size(); i--;) {
            ArithSubDescriptor& arithSub = state.arithSubs[i];
            generateArithSubICFastPath(state, codeBlock, generatedFunction, recordMap, arithSub);
        }
        for (unsigned i = state.lazySlowPaths.size(); i--;) {
            LazySlowPathDescriptor& lazySlowPath = state.lazySlowPaths[i];
            for (auto& tuple : lazySlowPath.m_generators) {
                MacroAssembler::replaceWithJump(
                    std::get<0>(tuple)->patchpoint(),
                    state.finalizer->sideCodeLinkBuffer->locationOf(std::get<1>(tuple)));
            }
        }
        for (auto& pair : exceptionJumpsToLink)
            state.finalizer->sideCodeLinkBuffer->link(pair.first, pair.second);
    }
    
    adjustCallICsForStackmaps(state.jsCalls, recordMap, generateOrGetAlreadyGeneratedCallSiteIndex, jsCallOSRExitForRecordIndex);
    
    for (unsigned i = state.jsCalls.size(); i--;) {
        JSCall& call = state.jsCalls[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(fastPathJIT, state, jsCallThatMightThrowSpillOffset);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfCall(), "JSCall inline cache", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            call.link(vm, linkBuffer);
        });
    }
    
    adjustCallICsForStackmaps(state.jsCallVarargses, recordMap, generateOrGetAlreadyGeneratedCallSiteIndex, jsCallOSRExitForRecordIndex);
    
    for (unsigned i = state.jsCallVarargses.size(); i--;) {
        JSCallVarargs& call = state.jsCallVarargses[i];
        
        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(fastPathJIT, state, varargsSpillSlotsOffset, jsCallThatMightThrowSpillOffset);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;
        size_t sizeOfIC = sizeOfICFor(call.node());

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "varargs call inline cache", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
            call.link(vm, linkBuffer, state.finalizer->handleExceptionsLinkBuffer->entrypoint());
        });
    }

    // FIXME: We shouldn't generate CallSite indices for tail calls.
    // https://bugs.webkit.org/show_bug.cgi?id=151079
    adjustCallICsForStackmaps(state.jsTailCalls, recordMap, generateOrGetAlreadyGeneratedCallSiteIndex, jsCallOSRExitForRecordIndex);

    for (unsigned i = state.jsTailCalls.size(); i--;) {
        JSTailCall& call = state.jsTailCalls[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(*state.jitCode.get(), fastPathJIT);

        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;
        size_t sizeOfIC = call.estimatedSize();

        generateInlineIfPossibleOutOfLineIfNot(state, vm, codeBlock, fastPathJIT, startOfIC, sizeOfIC, "tail call inline cache", [&] (LinkBuffer& linkBuffer, CCallHelpers&, bool) {
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

        if (exit.m_descriptor.m_willArriveAtOSRExitFromGenericUnwind || exit.m_descriptor.m_isExceptionFromLazySlowPath) // This is reached by a jump from genericUnwind or a jump from a lazy slow path.
            continue;
        
        StackMaps::Record& record = jitCode->stackmaps.records[exit.m_stackmapRecordIndex];
        
        CodeLocationLabel source = CodeLocationLabel(
            bitwise_cast<char*>(generatedFunction) + record.instructionOffset);
        
        codeAddresses.append(bitwise_cast<char*>(generatedFunction) + record.instructionOffset + MacroAssembler::maxJumpReplacementSize());
        
        if (exit.m_descriptor.m_isInvalidationPoint)
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
    state.graph.m_codeBlock->setCalleeSaveRegisters(WTF::move(registerOffsets));
    
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

#endif // ENABLE(FTL_JIT)

