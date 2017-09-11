/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "DFGOSRExit.h"

#if ENABLE(DFG_JIT)

#include "AssemblyHelpers.h"
#include "ClonedArguments.h"
#include "DFGGraph.h"
#include "DFGMayExit.h"
#include "DFGOSRExitPreparation.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "DirectArguments.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "OperandsInlines.h"
#include "ProbeContext.h"
#include "ProbeFrame.h"

namespace JSC { namespace DFG {

using CPUState = Probe::CPUState;
using Context = Probe::Context;
using Frame = Probe::Frame;

static void reifyInlinedCallFrames(Probe::Context&, CodeBlock* baselineCodeBlock, const OSRExitBase&);
static void adjustAndJumpToTarget(Probe::Context&, VM&, CodeBlock*, CodeBlock* baselineCodeBlock, OSRExit&);
static void printOSRExit(Context&, uint32_t osrExitIndex, const OSRExit&);

static JSValue jsValueFor(CPUState& cpu, JSValueSource source)
{
    if (source.isAddress()) {
        JSValue result;
        std::memcpy(&result, cpu.gpr<uint8_t*>(source.base()) + source.offset(), sizeof(JSValue));
        return result;
    }
#if USE(JSVALUE64)
    return JSValue::decode(cpu.gpr<EncodedJSValue>(source.gpr()));
#else
    if (source.hasKnownTag())
        return JSValue(source.tag(), cpu.gpr<int32_t>(source.payloadGPR()));
    return JSValue(cpu.gpr<int32_t>(source.tagGPR()), cpu.gpr<int32_t>(source.payloadGPR()));
#endif
}

#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0

static_assert(is64Bit(), "we only support callee save registers on 64-bit");

// Based on AssemblyHelpers::emitRestoreCalleeSavesFor().
static void restoreCalleeSavesFor(Context& context, CodeBlock* codeBlock)
{
    ASSERT(codeBlock);

    RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
    RegisterSet dontRestoreRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
    unsigned registerCount = calleeSaves->size();

    uintptr_t* physicalStackFrame = context.fp<uintptr_t*>();
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = calleeSaves->at(i);
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        // The callee saved values come from the original stack, not the recovered stack.
        // Hence, we read the values directly from the physical stack memory instead of
        // going through context.stack().
        ASSERT(!(entry.offset() % sizeof(uintptr_t)));
        context.gpr(entry.reg().gpr()) = physicalStackFrame[entry.offset() / sizeof(uintptr_t)];
    }
}

// Based on AssemblyHelpers::emitSaveCalleeSavesFor().
static void saveCalleeSavesFor(Context& context, CodeBlock* codeBlock)
{
    auto& stack = context.stack();
    ASSERT(codeBlock);

    RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
    RegisterSet dontSaveRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
    unsigned registerCount = calleeSaves->size();

    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = calleeSaves->at(i);
        if (dontSaveRegisters.get(entry.reg()))
            continue;
        stack.set(context.fp(), entry.offset(), context.gpr<uintptr_t>(entry.reg().gpr()));
    }
}

// Based on AssemblyHelpers::restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer().
static void restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(Context& context)
{
    VM& vm = *context.arg<VM*>();

    RegisterAtOffsetList* allCalleeSaves = VM::getAllCalleeSaveRegisterOffsets();
    RegisterSet dontRestoreRegisters = RegisterSet::stackRegisters();
    unsigned registerCount = allCalleeSaves->size();

    VMEntryRecord* entryRecord = vmEntryRecord(vm.topVMEntryFrame);
    uintptr_t* calleeSaveBuffer = reinterpret_cast<uintptr_t*>(entryRecord->calleeSaveRegistersBuffer);

    // Restore all callee saves.
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        size_t uintptrOffset = entry.offset() / sizeof(uintptr_t);
        if (entry.reg().isGPR())
            context.gpr(entry.reg().gpr()) = calleeSaveBuffer[uintptrOffset];
        else
            context.fpr(entry.reg().fpr()) = bitwise_cast<double>(calleeSaveBuffer[uintptrOffset]);
    }
}

// Based on AssemblyHelpers::copyCalleeSavesToVMEntryFrameCalleeSavesBuffer().
static void copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(Context& context)
{
    VM& vm = *context.arg<VM*>();
    auto& stack = context.stack();

    VMEntryRecord* entryRecord = vmEntryRecord(vm.topVMEntryFrame);
    void* calleeSaveBuffer = entryRecord->calleeSaveRegistersBuffer;

    RegisterAtOffsetList* allCalleeSaves = VM::getAllCalleeSaveRegisterOffsets();
    RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
    unsigned registerCount = allCalleeSaves->size();

    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontCopyRegisters.get(entry.reg()))
            continue;
        if (entry.reg().isGPR())
            stack.set(calleeSaveBuffer, entry.offset(), context.gpr<uintptr_t>(entry.reg().gpr()));
        else
            stack.set(calleeSaveBuffer, entry.offset(), context.fpr<uintptr_t>(entry.reg().fpr()));
    }
}

// Based on AssemblyHelpers::emitSaveOrCopyCalleeSavesFor().
static void saveOrCopyCalleeSavesFor(Context& context, CodeBlock* codeBlock, VirtualRegister offsetVirtualRegister, bool wasCalledViaTailCall)
{
    Frame frame(context.fp(), context.stack());
    ASSERT(codeBlock);

    RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
    RegisterSet dontSaveRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
    unsigned registerCount = calleeSaves->size();

    RegisterSet baselineCalleeSaves = RegisterSet::llintBaselineCalleeSaveRegisters();

    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = calleeSaves->at(i);
        if (dontSaveRegisters.get(entry.reg()))
            continue;

        uintptr_t savedRegisterValue;

        if (wasCalledViaTailCall && baselineCalleeSaves.get(entry.reg()))
            savedRegisterValue = frame.get<uintptr_t>(entry.offset());
        else
            savedRegisterValue = context.gpr(entry.reg().gpr());

        frame.set(offsetVirtualRegister.offsetInBytes() + entry.offset(), savedRegisterValue);
    }
}
#else // not NUMBER_OF_CALLEE_SAVES_REGISTERS > 0

static void restoreCalleeSavesFor(Context&, CodeBlock*) { }
static void saveCalleeSavesFor(Context&, CodeBlock*) { }
static void restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(Context&) { }
static void copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(Context&) { }
static void saveOrCopyCalleeSavesFor(Context&, CodeBlock*, VirtualRegister, bool) { }

#endif // NUMBER_OF_CALLEE_SAVES_REGISTERS > 0

static JSCell* createDirectArgumentsDuringExit(Context& context, CodeBlock* codeBlock, InlineCallFrame* inlineCallFrame, JSFunction* callee, int32_t argumentCount)
{
    VM& vm = *context.arg<VM*>();

    ASSERT(vm.heap.isDeferred());

    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);

    unsigned length = argumentCount - 1;
    unsigned capacity = std::max(length, static_cast<unsigned>(codeBlock->numParameters() - 1));
    DirectArguments* result = DirectArguments::create(
        vm, codeBlock->globalObject()->directArgumentsStructure(), length, capacity);

    result->callee().set(vm, result, callee);

    void* frameBase = context.fp<Register*>() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0);
    Frame frame(frameBase, context.stack());
    for (unsigned i = length; i--;)
        result->setIndexQuickly(vm, i, frame.argument(i));

    return result;
}

static JSCell* createClonedArgumentsDuringExit(Context& context, CodeBlock* codeBlock, InlineCallFrame* inlineCallFrame, JSFunction* callee, int32_t argumentCount)
{
    VM& vm = *context.arg<VM*>();
    ExecState* exec = context.fp<ExecState*>();

    ASSERT(vm.heap.isDeferred());

    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);

    unsigned length = argumentCount - 1;
    ClonedArguments* result = ClonedArguments::createEmpty(
        vm, codeBlock->globalObject()->clonedArgumentsStructure(), callee, length);

    void* frameBase = context.fp<Register*>() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0);
    Frame frame(frameBase, context.stack());
    for (unsigned i = length; i--;)
        result->putDirectIndex(exec, i, frame.argument(i));
    return result;
}

OSRExit::OSRExit(ExitKind kind, JSValueSource jsValueSource, MethodOfGettingAValueProfile valueProfile, SpeculativeJIT* jit, unsigned streamIndex, unsigned recoveryIndex)
    : OSRExitBase(kind, jit->m_origin.forExit, jit->m_origin.semantic, jit->m_origin.wasHoisted)
    , m_jsValueSource(jsValueSource)
    , m_valueProfile(valueProfile)
    , m_recoveryIndex(recoveryIndex)
    , m_streamIndex(streamIndex)
{
    bool canExit = jit->m_origin.exitOK;
    if (!canExit && jit->m_currentNode) {
        ExitMode exitMode = mayExit(jit->m_jit.graph(), jit->m_currentNode);
        canExit = exitMode == ExitMode::Exits || exitMode == ExitMode::ExitsForExceptions;
    }
    DFG_ASSERT(jit->m_jit.graph(), jit->m_currentNode, canExit);
}

static void emitRestoreArguments(Context& context, CodeBlock* codeBlock, DFG::JITCode* dfgJITCode, const Operands<ValueRecovery>& operands)
{
    Frame frame(context.fp(), context.stack());

    HashMap<MinifiedID, int> alreadyAllocatedArguments; // Maps phantom arguments node ID to operand.
    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];
        int operand = operands.operandForIndex(index);

        if (recovery.technique() != DirectArgumentsThatWereNotCreated
            && recovery.technique() != ClonedArgumentsThatWereNotCreated)
            continue;

        MinifiedID id = recovery.nodeID();
        auto iter = alreadyAllocatedArguments.find(id);
        if (iter != alreadyAllocatedArguments.end()) {
            frame.setOperand(operand, frame.operand(iter->value));
            continue;
        }

        InlineCallFrame* inlineCallFrame =
            dfgJITCode->minifiedDFG.at(id)->inlineCallFrame();

        int stackOffset;
        if (inlineCallFrame)
            stackOffset = inlineCallFrame->stackOffset;
        else
            stackOffset = 0;

        JSFunction* callee;
        if (!inlineCallFrame || inlineCallFrame->isClosureCall)
            callee = jsCast<JSFunction*>(frame.operand(stackOffset + CallFrameSlot::callee).asCell());
        else
            callee = jsCast<JSFunction*>(inlineCallFrame->calleeRecovery.constant().asCell());

        int32_t argumentCount;
        if (!inlineCallFrame || inlineCallFrame->isVarargs())
            argumentCount = frame.operand<int32_t>(stackOffset + CallFrameSlot::argumentCount, PayloadOffset);
        else
            argumentCount = inlineCallFrame->argumentCountIncludingThis;

        JSCell* argumentsObject;
        switch (recovery.technique()) {
        case DirectArgumentsThatWereNotCreated:
            argumentsObject = createDirectArgumentsDuringExit(context, codeBlock, inlineCallFrame, callee, argumentCount);
            break;
        case ClonedArgumentsThatWereNotCreated:
            argumentsObject = createClonedArgumentsDuringExit(context, codeBlock, inlineCallFrame, callee, argumentCount);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        frame.setOperand(operand, JSValue(argumentsObject));

        alreadyAllocatedArguments.add(id, operand);
    }
}

void OSRExit::executeOSRExit(Context& context)
{
    VM& vm = *context.arg<VM*>();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ExecState* exec = context.fp<ExecState*>();
    ASSERT(&exec->vm() == &vm);

    if (vm.callFrameForCatch) {
        exec = vm.callFrameForCatch;
        context.fp() = exec;
    }

    CodeBlock* codeBlock = exec->codeBlock();
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITCode::DFGJIT);

    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm.heap);

    uint32_t exitIndex = vm.osrExitIndex;
    DFG::JITCode* dfgJITCode = codeBlock->jitCode()->dfg();
    OSRExit& exit = dfgJITCode->osrExit[exitIndex];

    ASSERT(!vm.callFrameForCatch || exit.m_kind == GenericUnwind);
    EXCEPTION_ASSERT_UNUSED(scope, !!scope.exception() || !exit.isExceptionHandler());

    if (UNLIKELY(!exit.exitState)) {
        // We only need to execute this block once for each OSRExit record. The computed
        // results will be cached in the OSRExitState record for use of the rest of the
        // exit ramp code.

        // Ensure we have baseline codeBlocks to OSR exit to.
        prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);

        CodeBlock* baselineCodeBlock = codeBlock->baselineAlternative();
        ASSERT(baselineCodeBlock->jitType() == JITCode::BaselineJIT);

        // Compute the value recoveries.
        Operands<ValueRecovery> operands;
        dfgJITCode->variableEventStream.reconstruct(codeBlock, exit.m_codeOrigin, dfgJITCode->minifiedDFG, exit.m_streamIndex, operands);

        SpeculationRecovery* recovery = nullptr;
        if (exit.m_recoveryIndex != UINT_MAX)
            recovery = &dfgJITCode->speculationRecovery[exit.m_recoveryIndex];

        int32_t activeThreshold = baselineCodeBlock->adjustedCounterValue(Options::thresholdForOptimizeAfterLongWarmUp());
        double adjustedThreshold = applyMemoryUsageHeuristicsAndConvertToInt(activeThreshold, baselineCodeBlock);
        ASSERT(adjustedThreshold > 0);
        adjustedThreshold = BaselineExecutionCounter::clippedThreshold(codeBlock->globalObject(), adjustedThreshold);

        CodeBlock* codeBlockForExit = baselineCodeBlockForOriginAndBaselineCodeBlock(exit.m_codeOrigin, baselineCodeBlock);
        Vector<BytecodeAndMachineOffset> decodedCodeMap;
        codeBlockForExit->jitCodeMap()->decode(decodedCodeMap);

        BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned>(decodedCodeMap, decodedCodeMap.size(), exit.m_codeOrigin.bytecodeIndex, BytecodeAndMachineOffset::getBytecodeIndex);

        ASSERT(mapping);
        ASSERT(mapping->m_bytecodeIndex == exit.m_codeOrigin.bytecodeIndex);

        ptrdiff_t finalStackPointerOffset = codeBlockForExit->stackPointerOffset() * sizeof(Register);

        void* jumpTarget = codeBlockForExit->jitCode()->executableAddressAtOffset(mapping->m_machineCodeOffset);

        exit.exitState = adoptRef(new OSRExitState(exit, codeBlock, baselineCodeBlock, operands, recovery, finalStackPointerOffset, activeThreshold, adjustedThreshold, jumpTarget));

        if (UNLIKELY(vm.m_perBytecodeProfiler && codeBlock->jitCode()->dfgCommon()->compilation)) {
            Profiler::Database& database = *vm.m_perBytecodeProfiler;
            Profiler::Compilation* compilation = codeBlock->jitCode()->dfgCommon()->compilation.get();

            Profiler::OSRExit* profilerExit = compilation->addOSRExit(
                exitIndex, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
                exit.m_kind, exit.m_kind == UncountableInvalidation);
            exit.exitState->profilerExit = profilerExit;
        }

        if (UNLIKELY(Options::verboseOSR() || Options::verboseDFGOSRExit())) {
            dataLogF("DFG OSR exit #%u (%s, %s) from %s, with operands = %s\n",
                exitIndex, toCString(exit.m_codeOrigin).data(),
                exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
                toCString(ignoringContext<DumpContext>(operands)).data());
        }
    }

    OSRExitState& exitState = *exit.exitState.get();
    CodeBlock* baselineCodeBlock = exitState.baselineCodeBlock;
    ASSERT(baselineCodeBlock->jitType() == JITCode::BaselineJIT);

    Operands<ValueRecovery>& operands = exitState.operands;
    SpeculationRecovery* recovery = exitState.recovery;

    if (exit.m_kind == GenericUnwind) {
        // We are acting as a defacto op_catch because we arrive here from genericUnwind().
        // So, we must restore our call frame and stack pointer.
        restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(context);
        ASSERT(context.fp() == vm.callFrameForCatch);
    }
    context.sp() = context.fp<uint8_t*>() + (codeBlock->stackPointerOffset() * sizeof(Register));

    ASSERT(!(context.fp<uintptr_t>() & 0x7));

    if (exitState.profilerExit)
        exitState.profilerExit->incCount();

    auto& cpu = context.cpu;
    Frame frame(cpu.fp(), context.stack());

#if USE(JSVALUE64)
    ASSERT(cpu.gpr(GPRInfo::tagTypeNumberRegister) == TagTypeNumber);
    ASSERT(cpu.gpr(GPRInfo::tagMaskRegister) == TagMask);
#endif

    if (UNLIKELY(Options::printEachOSRExit()))
        printOSRExit(context, vm.osrExitIndex, exit);

    // Perform speculation recovery. This only comes into play when an operation
    // starts mutating state before verifying the speculation it has already made.

    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            cpu.gpr(recovery->dest()) = cpu.gpr<uint32_t>(recovery->dest()) - cpu.gpr<uint32_t>(recovery->src());
#if USE(JSVALUE64)
            ASSERT(!(cpu.gpr(recovery->dest()) >> 32));
            cpu.gpr(recovery->dest()) |= TagTypeNumber;
#endif
            break;

        case SpeculativeAddImmediate:
            cpu.gpr(recovery->dest()) = (cpu.gpr<uint32_t>(recovery->dest()) - recovery->immediate());
#if USE(JSVALUE64)
            ASSERT(!(cpu.gpr(recovery->dest()) >> 32));
            cpu.gpr(recovery->dest()) |= TagTypeNumber;
#endif
            break;

        case BooleanSpeculationCheck:
#if USE(JSVALUE64)
            cpu.gpr(recovery->dest()) = cpu.gpr(recovery->dest()) ^ ValueFalse;
#endif
            break;

        default:
            break;
        }
    }

    // Refine some array and/or value profile, if appropriate.

    if (!!exit.m_jsValueSource) {
        if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType) {
            // If the instruction that this originated from has an array profile, then
            // refine it. If it doesn't, then do nothing. The latter could happen for
            // hoisted checks, or checks emitted for operations that didn't have array
            // profiling - either ops that aren't array accesses at all, or weren't
            // known to be array acceses in the bytecode. The latter case is a FIXME
            // while the former case is an outcome of a CheckStructure not knowing why
            // it was emitted (could be either due to an inline cache of a property
            // property access, or due to an array profile).

            CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
            CodeBlock* profiledCodeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, baselineCodeBlock);
            if (ArrayProfile* arrayProfile = profiledCodeBlock->getArrayProfile(codeOrigin.bytecodeIndex)) {
                Structure* structure = jsValueFor(cpu, exit.m_jsValueSource).asCell()->structure(vm);
                arrayProfile->observeStructure(structure);
                // FIXME: We should be able to use arrayModeFromStructure() to determine the observed ArrayMode here.
                // However, currently, doing so would result in a pdfjs preformance regression.
                // https://bugs.webkit.org/show_bug.cgi?id=176473
                arrayProfile->observeArrayMode(asArrayModes(structure->indexingType()));
            }
        }

        if (MethodOfGettingAValueProfile profile = exit.m_valueProfile)
            profile.reportValue(jsValueFor(cpu, exit.m_jsValueSource));
    }

    // Do all data format conversions and store the results into the stack.
    // Note: we need to recover values before restoring callee save registers below
    // because the recovery may rely on values in some of callee save registers.

    int calleeSaveSpaceAsVirtualRegisters = static_cast<int>(baselineCodeBlock->calleeSaveSpaceAsVirtualRegisters());
    size_t numberOfOperands = operands.size();
    for (size_t index = 0; index < numberOfOperands; ++index) {
        const ValueRecovery& recovery = operands[index];
        VirtualRegister reg = operands.virtualRegisterForIndex(index);

        if (reg.isLocal() && reg.toLocal() < calleeSaveSpaceAsVirtualRegisters)
            continue;

        int operand = reg.offset();

        switch (recovery.technique()) {
        case DisplacedInJSStack:
            frame.setOperand(operand, exec->r(recovery.virtualRegister()).jsValue());
            break;

        case InFPR:
            frame.setOperand(operand, cpu.fpr<JSValue>(recovery.fpr()));
            break;

#if USE(JSVALUE64)
        case InGPR:
            frame.setOperand(operand, cpu.gpr<JSValue>(recovery.gpr()));
            break;
#else
        case InPair:
            frame.setOperand(operand, JSValue(cpu.gpr<int32_t>(recovery.tagGPR()), cpu.gpr<int32_t>(recovery.payloadGPR())));
            break;
#endif

        case UnboxedCellInGPR:
            frame.setOperand(operand, JSValue(cpu.gpr<JSCell*>(recovery.gpr())));
            break;

        case CellDisplacedInJSStack:
            frame.setOperand(operand, JSValue(exec->r(recovery.virtualRegister()).unboxedCell()));
            break;

#if USE(JSVALUE32_64)
        case UnboxedBooleanInGPR:
            frame.setOperand(operand, jsBoolean(cpu.gpr<bool>(recovery.gpr())));
            break;
#endif

        case BooleanDisplacedInJSStack:
#if USE(JSVALUE64)
            frame.setOperand(operand, exec->r(recovery.virtualRegister()).jsValue());
#else
            frame.setOperand(operand, jsBoolean(exec->r(recovery.virtualRegister()).jsValue().payload()));
#endif
            break;

        case UnboxedInt32InGPR:
            frame.setOperand(operand, JSValue(cpu.gpr<int32_t>(recovery.gpr())));
            break;

        case Int32DisplacedInJSStack:
            frame.setOperand(operand, JSValue(exec->r(recovery.virtualRegister()).unboxedInt32()));
            break;

#if USE(JSVALUE64)
        case UnboxedInt52InGPR:
            frame.setOperand(operand, JSValue(cpu.gpr<int64_t>(recovery.gpr()) >> JSValue::int52ShiftAmount));
            break;

        case Int52DisplacedInJSStack:
            frame.setOperand(operand, JSValue(exec->r(recovery.virtualRegister()).unboxedInt52()));
            break;

        case UnboxedStrictInt52InGPR:
            frame.setOperand(operand, JSValue(cpu.gpr<int64_t>(recovery.gpr())));
            break;

        case StrictInt52DisplacedInJSStack:
            frame.setOperand(operand, JSValue(exec->r(recovery.virtualRegister()).unboxedStrictInt52()));
            break;
#endif

        case UnboxedDoubleInFPR:
            frame.setOperand(operand, JSValue(JSValue::EncodeAsDouble, purifyNaN(cpu.fpr(recovery.fpr()))));
            break;

        case DoubleDisplacedInJSStack:
            frame.setOperand(operand, JSValue(JSValue::EncodeAsDouble, purifyNaN(exec->r(recovery.virtualRegister()).unboxedDouble())));
            break;

        case Constant:
            frame.setOperand(operand, recovery.constant());
            break;

        case DirectArgumentsThatWereNotCreated:
        case ClonedArgumentsThatWereNotCreated:
            // Don't do this, yet.
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    // Need to ensure that the stack pointer accounts for the worst-case stack usage at exit. This
    // could toast some stack that the DFG used. We need to do it before storing to stack offsets
    // used by baseline.
    cpu.sp() = cpu.fp<uint8_t*>() - (codeBlock->jitCode()->dfgCommon()->requiredRegisterCountForExit * sizeof(Register));

    // Restore the DFG callee saves and then save the ones the baseline JIT uses.
    restoreCalleeSavesFor(context, codeBlock);
    saveCalleeSavesFor(context, baselineCodeBlock);

    // The tag registers are needed to materialize recoveries below.
#if USE(JSVALUE64)
    cpu.gpr(GPRInfo::tagTypeNumberRegister) = TagTypeNumber;
    cpu.gpr(GPRInfo::tagMaskRegister) = TagTypeNumber | TagBitTypeOther;
#endif

    if (exit.isExceptionHandler())
        copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(context);

    // Now that things on the stack are recovered, do the arguments recovery. We assume that arguments
    // recoveries don't recursively refer to each other. But, we don't try to assume that they only
    // refer to certain ranges of locals. Hence why we need to do this here, once the stack is sensible.
    // Note that we also roughly assume that the arguments might still be materialized outside of its
    // inline call frame scope - but for now the DFG wouldn't do that.

    emitRestoreArguments(context, codeBlock, dfgJITCode, operands);

    // Adjust the old JIT's execute counter. Since we are exiting OSR, we know
    // that all new calls into this code will go to the new JIT, so the execute
    // counter only affects call frames that performed OSR exit and call frames
    // that were still executing the old JIT at the time of another call frame's
    // OSR exit. We want to ensure that the following is true:
    //
    // (a) Code the performs an OSR exit gets a chance to reenter optimized
    //     code eventually, since optimized code is faster. But we don't
    //     want to do such reentery too aggressively (see (c) below).
    //
    // (b) If there is code on the call stack that is still running the old
    //     JIT's code and has never OSR'd, then it should get a chance to
    //     perform OSR entry despite the fact that we've exited.
    //
    // (c) Code the performs an OSR exit should not immediately retry OSR
    //     entry, since both forms of OSR are expensive. OSR entry is
    //     particularly expensive.
    //
    // (d) Frequent OSR failures, even those that do not result in the code
    //     running in a hot loop, result in recompilation getting triggered.
    //
    // To ensure (c), we'd like to set the execute counter to
    // counterValueForOptimizeAfterWarmUp(). This seems like it would endanger
    // (a) and (b), since then every OSR exit would delay the opportunity for
    // every call frame to perform OSR entry. Essentially, if OSR exit happens
    // frequently and the function has few loops, then the counter will never
    // become non-negative and OSR entry will never be triggered. OSR entry
    // will only happen if a loop gets hot in the old JIT, which does a pretty
    // good job of ensuring (a) and (b). But that doesn't take care of (d),
    // since each speculation failure would reset the execute counter.
    // So we check here if the number of speculation failures is significantly
    // larger than the number of successes (we want 90% success rate), and if
    // there have been a large enough number of failures. If so, we set the
    // counter to 0; otherwise we set the counter to
    // counterValueForOptimizeAfterWarmUp().

    if (UNLIKELY(codeBlock->updateOSRExitCounterAndCheckIfNeedToReoptimize(exitState) == CodeBlock::OptimizeAction::ReoptimizeNow))
        triggerReoptimizationNow(baselineCodeBlock, &exit);

    reifyInlinedCallFrames(context, baselineCodeBlock, exit);
    adjustAndJumpToTarget(context, vm, codeBlock, baselineCodeBlock, exit);
}

static void reifyInlinedCallFrames(Context& context, CodeBlock* outermostBaselineCodeBlock, const OSRExitBase& exit)
{
    auto& cpu = context.cpu;
    Frame frame(cpu.fp(), context.stack());

    // FIXME: We shouldn't leave holes on the stack when performing an OSR exit
    // in presence of inlined tail calls.
    // https://bugs.webkit.org/show_bug.cgi?id=147511
    ASSERT(outermostBaselineCodeBlock->jitType() == JITCode::BaselineJIT);
    frame.setOperand<CodeBlock*>(CallFrameSlot::codeBlock, outermostBaselineCodeBlock);

    const CodeOrigin* codeOrigin;
    for (codeOrigin = &exit.m_codeOrigin; codeOrigin && codeOrigin->inlineCallFrame; codeOrigin = codeOrigin->inlineCallFrame->getCallerSkippingTailCalls()) {
        InlineCallFrame* inlineCallFrame = codeOrigin->inlineCallFrame;
        CodeBlock* baselineCodeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(*codeOrigin, outermostBaselineCodeBlock);
        InlineCallFrame::Kind trueCallerCallKind;
        CodeOrigin* trueCaller = inlineCallFrame->getCallerSkippingTailCalls(&trueCallerCallKind);
        void* callerFrame = cpu.fp();

        if (!trueCaller) {
            ASSERT(inlineCallFrame->isTail());
            void* returnPC = frame.get<void*>(CallFrame::returnPCOffset());
            frame.set<void*>(inlineCallFrame->returnPCOffset(), returnPC);
            callerFrame = frame.get<void*>(CallFrame::callerFrameOffset());
        } else {
            CodeBlock* baselineCodeBlockForCaller = baselineCodeBlockForOriginAndBaselineCodeBlock(*trueCaller, outermostBaselineCodeBlock);
            unsigned callBytecodeIndex = trueCaller->bytecodeIndex;
            void* jumpTarget = nullptr;

            switch (trueCallerCallKind) {
            case InlineCallFrame::Call:
            case InlineCallFrame::Construct:
            case InlineCallFrame::CallVarargs:
            case InlineCallFrame::ConstructVarargs:
            case InlineCallFrame::TailCall:
            case InlineCallFrame::TailCallVarargs: {
                CallLinkInfo* callLinkInfo =
                    baselineCodeBlockForCaller->getCallLinkInfoForBytecodeIndex(callBytecodeIndex);
                RELEASE_ASSERT(callLinkInfo);

                jumpTarget = callLinkInfo->callReturnLocation().executableAddress();
                break;
            }

            case InlineCallFrame::GetterCall:
            case InlineCallFrame::SetterCall: {
                StructureStubInfo* stubInfo =
                    baselineCodeBlockForCaller->findStubInfo(CodeOrigin(callBytecodeIndex));
                RELEASE_ASSERT(stubInfo);

                jumpTarget = stubInfo->doneLocation().executableAddress();
                break;
            }

            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            if (trueCaller->inlineCallFrame)
                callerFrame = cpu.fp<uint8_t*>() + trueCaller->inlineCallFrame->stackOffset * sizeof(EncodedJSValue);

            frame.set<void*>(inlineCallFrame->returnPCOffset(), jumpTarget);
        }

        frame.setOperand<void*>(inlineCallFrame->stackOffset + CallFrameSlot::codeBlock, baselineCodeBlock);

        // Restore the inline call frame's callee save registers.
        // If this inlined frame is a tail call that will return back to the original caller, we need to
        // copy the prior contents of the tag registers already saved for the outer frame to this frame.
        saveOrCopyCalleeSavesFor(context, baselineCodeBlock, VirtualRegister(inlineCallFrame->stackOffset), !trueCaller);

        if (!inlineCallFrame->isVarargs())
            frame.setOperand<uint32_t>(inlineCallFrame->stackOffset + CallFrameSlot::argumentCount, PayloadOffset, inlineCallFrame->argumentCountIncludingThis);
        ASSERT(callerFrame);
        frame.set<void*>(inlineCallFrame->callerFrameOffset(), callerFrame);
#if USE(JSVALUE64)
        uint32_t locationBits = CallSiteIndex(codeOrigin->bytecodeIndex).bits();
        frame.setOperand<uint32_t>(inlineCallFrame->stackOffset + CallFrameSlot::argumentCount, TagOffset, locationBits);
        if (!inlineCallFrame->isClosureCall)
            frame.setOperand(inlineCallFrame->stackOffset + CallFrameSlot::callee, JSValue(inlineCallFrame->calleeConstant()));
#else // USE(JSVALUE64) // so this is the 32-bit part
        Instruction* instruction = baselineCodeBlock->instructions().begin() + codeOrigin->bytecodeIndex;
        uint32_t locationBits = CallSiteIndex(instruction).bits();
        frame.setOperand<uint32_t>(inlineCallFrame->stackOffset + CallFrameSlot::argumentCount, TagOffset, locationBits);
        frame.setOperand<uint32_t>(inlineCallFrame->stackOffset + CallFrameSlot::callee, TagOffset, static_cast<uint32_t>(JSValue::CellTag));
        if (!inlineCallFrame->isClosureCall)
            frame.setOperand(inlineCallFrame->stackOffset + CallFrameSlot::callee, PayloadOffset, inlineCallFrame->calleeConstant());
#endif // USE(JSVALUE64) // ending the #else part, so directly above is the 32-bit part
    }

    // Don't need to set the toplevel code origin if we only did inline tail calls
    if (codeOrigin) {
#if USE(JSVALUE64)
        uint32_t locationBits = CallSiteIndex(codeOrigin->bytecodeIndex).bits();
#else
        Instruction* instruction = outermostBaselineCodeBlock->instructions().begin() + codeOrigin->bytecodeIndex;
        uint32_t locationBits = CallSiteIndex(instruction).bits();
#endif
        frame.setOperand<uint32_t>(CallFrameSlot::argumentCount, TagOffset, locationBits);
    }
}

static void adjustAndJumpToTarget(Context& context, VM& vm, CodeBlock* codeBlock, CodeBlock* baselineCodeBlock, OSRExit& exit)
{
    OSRExitState* exitState = exit.exitState.get();

    WTF::storeLoadFence(); // The optimizing compiler expects that the OSR exit mechanism will execute this fence.
    vm.heap.writeBarrier(baselineCodeBlock);

    // We barrier all inlined frames -- and not just the current inline stack --
    // because we don't know which inlined function owns the value profile that
    // we'll update when we exit. In the case of "f() { a(); b(); }", if both
    // a and b are inlined, we might exit inside b due to a bad value loaded
    // from a.
    // FIXME: MethodOfGettingAValueProfile should remember which CodeBlock owns
    // the value profile.
    InlineCallFrameSet* inlineCallFrames = codeBlock->jitCode()->dfgCommon()->inlineCallFrames.get();
    if (inlineCallFrames) {
        for (InlineCallFrame* inlineCallFrame : *inlineCallFrames)
            vm.heap.writeBarrier(inlineCallFrame->baselineCodeBlock.get());
    }

    if (exit.m_codeOrigin.inlineCallFrame)
        context.fp() = context.fp<uint8_t*>() + exit.m_codeOrigin.inlineCallFrame->stackOffset * sizeof(EncodedJSValue);

    void* jumpTarget = exitState->jumpTarget;
    ASSERT(jumpTarget);

    context.sp() = context.fp<uint8_t*>() + exitState->stackPointerOffset;
    if (exit.isExceptionHandler()) {
        // Since we're jumping to op_catch, we need to set callFrameForCatch.
        vm.callFrameForCatch = context.fp<ExecState*>();
    }

    vm.topCallFrame = context.fp<ExecState*>();
    context.pc() = jumpTarget;
}

static void printOSRExit(Context& context, uint32_t osrExitIndex, const OSRExit& exit)
{
    ExecState* exec = context.fp<ExecState*>();
    CodeBlock* codeBlock = exec->codeBlock();
    CodeBlock* alternative = codeBlock->alternative();
    ExitKind kind = exit.m_kind;
    unsigned bytecodeOffset = exit.m_codeOrigin.bytecodeIndex;

    dataLog("Speculation failure in ", *codeBlock);
    dataLog(" @ exit #", osrExitIndex, " (bc#", bytecodeOffset, ", ", exitKindToString(kind), ") with ");
    if (alternative) {
        dataLog(
            "executeCounter = ", alternative->jitExecuteCounter(),
            ", reoptimizationRetryCounter = ", alternative->reoptimizationRetryCounter(),
            ", optimizationDelayCounter = ", alternative->optimizationDelayCounter());
    } else
        dataLog("no alternative code block (i.e. we've been jettisoned)");
    dataLog(", osrExitCounter = ", codeBlock->osrExitCounter(), "\n");
    dataLog("    GPRs at time of exit:");
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg gpr = GPRInfo::toRegister(i);
        dataLog(" ", context.gprName(gpr), ":", RawPointer(context.gpr<void*>(gpr)));
    }
    dataLog("\n");
    dataLog("    FPRs at time of exit:");
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg fpr = FPRInfo::toRegister(i);
        dataLog(" ", context.fprName(fpr), ":");
        uint64_t bits = context.fpr<uint64_t>(fpr);
        double value = context.fpr(fpr);
        dataLogF("%llx:%lf", static_cast<long long>(bits), value);
    }
    dataLog("\n");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
