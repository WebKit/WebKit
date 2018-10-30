/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#include "DFGOSRExitCompilerCommon.h"
#include "DFGOSRExitPreparation.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "DirectArguments.h"
#include "FrameTracers.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "OperandsInlines.h"
#include "ProbeContext.h"
#include "ProbeFrame.h"

namespace JSC { namespace DFG {

// Probe based OSR Exit.

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

    UCPURegister* physicalStackFrame = context.fp<UCPURegister*>();
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = calleeSaves->at(i);
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        // The callee saved values come from the original stack, not the recovered stack.
        // Hence, we read the values directly from the physical stack memory instead of
        // going through context.stack().
        ASSERT(!(entry.offset() % sizeof(UCPURegister)));
        context.gpr(entry.reg().gpr()) = physicalStackFrame[entry.offset() / sizeof(UCPURegister)];
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
        stack.set(context.fp(), entry.offset(), context.gpr<UCPURegister>(entry.reg().gpr()));
    }
}

// Based on AssemblyHelpers::restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer().
static void restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(Context& context)
{
    VM& vm = *context.arg<VM*>();

    RegisterAtOffsetList* allCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
    RegisterSet dontRestoreRegisters = RegisterSet::stackRegisters();
    unsigned registerCount = allCalleeSaves->size();

    VMEntryRecord* entryRecord = vmEntryRecord(vm.topEntryFrame);
    UCPURegister* calleeSaveBuffer = reinterpret_cast<UCPURegister*>(entryRecord->calleeSaveRegistersBuffer);

    // Restore all callee saves.
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        size_t uintptrOffset = entry.offset() / sizeof(UCPURegister);
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

    VMEntryRecord* entryRecord = vmEntryRecord(vm.topEntryFrame);
    void* calleeSaveBuffer = entryRecord->calleeSaveRegistersBuffer;

    RegisterAtOffsetList* allCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
    RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
    unsigned registerCount = allCalleeSaves->size();

    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontCopyRegisters.get(entry.reg()))
            continue;
        if (entry.reg().isGPR())
            stack.set(calleeSaveBuffer, entry.offset(), context.gpr<UCPURegister>(entry.reg().gpr()));
        else
            stack.set(calleeSaveBuffer, entry.offset(), context.fpr<UCPURegister>(entry.reg().fpr()));
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

    result->setCallee(vm, callee);

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

// The following is a list of extra initializations that need to be done in order
// of most likely needed (lower enum value) to least likely needed (higher enum value).
// Each level initialization includes the previous lower enum value (see use of the
// extraInitializationLevel value below).
enum class ExtraInitializationLevel {
    None,
    SpeculationRecovery,
    ValueProfileUpdate,
    ArrayProfileUpdate,
    Other
};

void OSRExit::executeOSRExit(Context& context)
{
    VM& vm = *context.arg<VM*>();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ExecState* exec = context.fp<ExecState*>();
    ASSERT(&exec->vm() == &vm);
    auto& cpu = context.cpu;

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
        ExtraInitializationLevel extraInitializationLevel = ExtraInitializationLevel::None;

        // We only need to execute this block once for each OSRExit record. The computed
        // results will be cached in the OSRExitState record for use of the rest of the
        // exit ramp code.

        // Ensure we have baseline codeBlocks to OSR exit to.
        prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);

        CodeBlock* baselineCodeBlock = codeBlock->baselineAlternative();
        ASSERT(baselineCodeBlock->jitType() == JITCode::BaselineJIT);

        SpeculationRecovery* recovery = nullptr;
        if (exit.m_recoveryIndex != UINT_MAX) {
            recovery = &dfgJITCode->speculationRecovery[exit.m_recoveryIndex];
            extraInitializationLevel = std::max(extraInitializationLevel, ExtraInitializationLevel::SpeculationRecovery);
        }

        if (UNLIKELY(exit.m_kind == GenericUnwind))
            extraInitializationLevel = std::max(extraInitializationLevel, ExtraInitializationLevel::Other);

        ArrayProfile* arrayProfile = nullptr;
        if (!!exit.m_jsValueSource) {
            if (exit.m_valueProfile)
                extraInitializationLevel = std::max(extraInitializationLevel, ExtraInitializationLevel::ValueProfileUpdate);
            if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType) {
                CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
                CodeBlock* profiledCodeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, baselineCodeBlock);
                arrayProfile = profiledCodeBlock->getArrayProfile(codeOrigin.bytecodeIndex);
                if (arrayProfile)
                    extraInitializationLevel = std::max(extraInitializationLevel, ExtraInitializationLevel::ArrayProfileUpdate);
            }
        }

        int32_t activeThreshold = baselineCodeBlock->adjustedCounterValue(Options::thresholdForOptimizeAfterLongWarmUp());
        double adjustedThreshold = applyMemoryUsageHeuristicsAndConvertToInt(activeThreshold, baselineCodeBlock);
        ASSERT(adjustedThreshold > 0);
        adjustedThreshold = BaselineExecutionCounter::clippedThreshold(codeBlock->globalObject(), adjustedThreshold);

        CodeBlock* codeBlockForExit = baselineCodeBlockForOriginAndBaselineCodeBlock(exit.m_codeOrigin, baselineCodeBlock);
        const JITCodeMap& codeMap = codeBlockForExit->jitCodeMap();
        CodeLocationLabel<JSEntryPtrTag> codeLocation = codeMap.find(exit.m_codeOrigin.bytecodeIndex);
        ASSERT(codeLocation);

        void* jumpTarget = codeLocation.executableAddress();

        // Compute the value recoveries.
        Operands<ValueRecovery> operands;
        Vector<UndefinedOperandSpan> undefinedOperandSpans;
        dfgJITCode->variableEventStream.reconstruct(codeBlock, exit.m_codeOrigin, dfgJITCode->minifiedDFG, exit.m_streamIndex, operands, &undefinedOperandSpans);
        ptrdiff_t stackPointerOffset = -static_cast<ptrdiff_t>(codeBlock->jitCode()->dfgCommon()->requiredRegisterCountForExit) * sizeof(Register);

        exit.exitState = adoptRef(new OSRExitState(exit, codeBlock, baselineCodeBlock, operands, WTFMove(undefinedOperandSpans), recovery, stackPointerOffset, activeThreshold, adjustedThreshold, jumpTarget, arrayProfile));

        if (UNLIKELY(vm.m_perBytecodeProfiler && codeBlock->jitCode()->dfgCommon()->compilation)) {
            Profiler::Database& database = *vm.m_perBytecodeProfiler;
            Profiler::Compilation* compilation = codeBlock->jitCode()->dfgCommon()->compilation.get();

            Profiler::OSRExit* profilerExit = compilation->addOSRExit(
                exitIndex, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
                exit.m_kind, exit.m_kind == UncountableInvalidation);
            exit.exitState->profilerExit = profilerExit;
            extraInitializationLevel = std::max(extraInitializationLevel, ExtraInitializationLevel::Other);
        }

        if (UNLIKELY(Options::printEachOSRExit()))
            extraInitializationLevel = std::max(extraInitializationLevel, ExtraInitializationLevel::Other);

        exit.exitState->extraInitializationLevel = extraInitializationLevel;

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
    Vector<UndefinedOperandSpan>& undefinedOperandSpans = exitState.undefinedOperandSpans;

    context.sp() = context.fp<uint8_t*>() + exitState.stackPointerOffset;

    // The only reason for using this do while look is so we can break out midway when appropriate.
    do {
        auto extraInitializationLevel = static_cast<ExtraInitializationLevel>(exitState.extraInitializationLevel);

        if (extraInitializationLevel == ExtraInitializationLevel::None)
            break;

        // Begin extra initilization level: SpeculationRecovery

        // We need to do speculation recovery first because array profiling and value profiling
        // may rely on a value that it recovers. However, that doesn't mean that it is likely
        // to have a recovery value. So, we'll decorate it as UNLIKELY.
        SpeculationRecovery* recovery = exitState.recovery;
        if (UNLIKELY(recovery)) {
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
        if (extraInitializationLevel <= ExtraInitializationLevel::SpeculationRecovery)
            break;

        // Begin extra initilization level: ValueProfileUpdate
        JSValue profiledValue;
        if (!!exit.m_jsValueSource) {
            profiledValue = jsValueFor(cpu, exit.m_jsValueSource);
            if (MethodOfGettingAValueProfile profile = exit.m_valueProfile)
                profile.reportValue(profiledValue);
        }
        if (extraInitializationLevel <= ExtraInitializationLevel::ValueProfileUpdate)
            break;

        // Begin extra initilization level: ArrayProfileUpdate
        ArrayProfile* arrayProfile = exitState.arrayProfile;
        if (arrayProfile) {
            ASSERT(!!exit.m_jsValueSource);
            ASSERT(exit.m_kind == BadCache || exit.m_kind == BadIndexingType);
            Structure* structure = profiledValue.asCell()->structure(vm);
            arrayProfile->observeStructure(structure);
            arrayProfile->observeArrayMode(asArrayModes(structure->indexingMode()));
        }
        if (extraInitializationLevel <= ExtraInitializationLevel::ArrayProfileUpdate)
            break;

        // Begin Extra initilization level: Other
        if (UNLIKELY(exit.m_kind == GenericUnwind)) {
            // We are acting as a defacto op_catch because we arrive here from genericUnwind().
            // So, we must restore our call frame and stack pointer.
            restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(context);
            ASSERT(context.fp() == vm.callFrameForCatch);
        }

        if (exitState.profilerExit)
            exitState.profilerExit->incCount();

        if (UNLIKELY(Options::printEachOSRExit()))
            printOSRExit(context, vm.osrExitIndex, exit);

    } while (false); // End extra initialization.

    Frame frame(cpu.fp(), context.stack());
    ASSERT(!(context.fp<uintptr_t>() & 0x7));

#if USE(JSVALUE64)
    ASSERT(cpu.gpr(GPRInfo::tagTypeNumberRegister) == TagTypeNumber);
    ASSERT(cpu.gpr(GPRInfo::tagMaskRegister) == TagMask);
#endif

    // Do all data format conversions and store the results into the stack.
    // Note: we need to recover values before restoring callee save registers below
    // because the recovery may rely on values in some of callee save registers.

    int calleeSaveSpaceAsVirtualRegisters = static_cast<int>(baselineCodeBlock->calleeSaveSpaceAsVirtualRegisters());
    size_t numberOfOperands = operands.size();
    size_t numUndefinedOperandSpans = undefinedOperandSpans.size();

    size_t nextUndefinedSpanIndex = 0;
    size_t nextUndefinedOperandIndex = numberOfOperands;
    if (numUndefinedOperandSpans)
        nextUndefinedOperandIndex = undefinedOperandSpans[nextUndefinedSpanIndex].firstIndex;

    JSValue undefined = jsUndefined();
    for (size_t spanIndex = 0; spanIndex < numUndefinedOperandSpans; ++spanIndex) {
        auto& span = undefinedOperandSpans[spanIndex];
        int firstOffset = span.minOffset;
        int lastOffset = firstOffset + span.numberOfRegisters;

        for (int offset = firstOffset; offset < lastOffset; ++offset)
            frame.setOperand(offset, undefined);
    }

    for (size_t index = 0; index < numberOfOperands; ++index) {
        const ValueRecovery& recovery = operands[index];
        VirtualRegister reg = operands.virtualRegisterForIndex(index);

        if (UNLIKELY(index == nextUndefinedOperandIndex)) {
            index += undefinedOperandSpans[nextUndefinedSpanIndex++].numberOfRegisters - 1;
            if (nextUndefinedSpanIndex < numUndefinedOperandSpans)
                nextUndefinedOperandIndex = undefinedOperandSpans[nextUndefinedSpanIndex].firstIndex;
            else
                nextUndefinedOperandIndex = numberOfOperands;
            continue;
        }

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

    // Restore the DFG callee saves and then save the ones the baseline JIT uses.
    restoreCalleeSavesFor(context, codeBlock);
    saveCalleeSavesFor(context, baselineCodeBlock);

#if USE(JSVALUE64)
    cpu.gpr(GPRInfo::tagTypeNumberRegister) = static_cast<uintptr_t>(TagTypeNumber);
    cpu.gpr(GPRInfo::tagMaskRegister) = static_cast<uintptr_t>(TagTypeNumber | TagBitTypeOther);
#endif

    if (exit.isExceptionHandler())
        copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(context);

    // Now that things on the stack are recovered, do the arguments recovery. We assume that arguments
    // recoveries don't recursively refer to each other. But, we don't try to assume that they only
    // refer to certain ranges of locals. Hence why we need to do this here, once the stack is sensible.
    // Note that we also roughly assume that the arguments might still be materialized outside of its
    // inline call frame scope - but for now the DFG wouldn't do that.

    DFG::emitRestoreArguments(context, codeBlock, dfgJITCode, operands);

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
        triggerReoptimizationNow(baselineCodeBlock, codeBlock, &exit);

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
#if USE(POINTER_PROFILING)
            void* oldEntrySP = cpu.fp<uint8_t*>() + sizeof(CallerFrameAndPC);
            void* newEntrySP = cpu.fp<uint8_t*>() + inlineCallFrame->returnPCOffset() + sizeof(void*);
            returnPC = retagCodePtr(returnPC, bitwise_cast<PtrTag>(oldEntrySP), bitwise_cast<PtrTag>(newEntrySP));
#endif
            frame.set<void*>(inlineCallFrame->returnPCOffset(), returnPC);
            callerFrame = frame.get<void*>(CallFrame::callerFrameOffset());
        } else {
            CodeBlock* baselineCodeBlockForCaller = baselineCodeBlockForOriginAndBaselineCodeBlock(*trueCaller, outermostBaselineCodeBlock);
            unsigned callBytecodeIndex = trueCaller->bytecodeIndex;
            MacroAssemblerCodePtr<JSInternalPtrTag> jumpTarget;

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

                jumpTarget = callLinkInfo->callReturnLocation();
                break;
            }

            case InlineCallFrame::GetterCall:
            case InlineCallFrame::SetterCall: {
                StructureStubInfo* stubInfo =
                    baselineCodeBlockForCaller->findStubInfo(CodeOrigin(callBytecodeIndex));
                RELEASE_ASSERT(stubInfo);

                jumpTarget = stubInfo->doneLocation();
                break;
            }

            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            if (trueCaller->inlineCallFrame)
                callerFrame = cpu.fp<uint8_t*>() + trueCaller->inlineCallFrame->stackOffset * sizeof(EncodedJSValue);

            void* targetAddress = jumpTarget.executableAddress();
#if USE(POINTER_PROFILING)
            void* newEntrySP = cpu.fp<uint8_t*>() + inlineCallFrame->returnPCOffset() + sizeof(void*);
            targetAddress = retagCodePtr(targetAddress, JSInternalPtrTag, bitwise_cast<PtrTag>(newEntrySP));
#endif
            frame.set<void*>(inlineCallFrame->returnPCOffset(), targetAddress);
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
        Instruction* instruction = &baselineCodeBlock->instructions()[codeOrigin->bytecodeIndex];
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
        Instruction* instruction = &outermostBaselineCodeBlock->instructions()[codeOrigin->bytecodeIndex];
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

    if (exit.isExceptionHandler()) {
        // Since we're jumping to op_catch, we need to set callFrameForCatch.
        vm.callFrameForCatch = context.fp<ExecState*>();
    }

    vm.topCallFrame = context.fp<ExecState*>();
    context.pc() = untagCodePtr<JSEntryPtrTag>(jumpTarget);
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

// JIT based OSR Exit.

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

CodeLocationJump<JSInternalPtrTag> OSRExit::codeLocationForRepatch() const
{
    return CodeLocationJump<JSInternalPtrTag>(m_patchableJumpLocation);
}

void OSRExit::emitRestoreArguments(CCallHelpers& jit, const Operands<ValueRecovery>& operands)
{
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
            JSValueRegs regs = JSValueRegs::withTwoAvailableRegs(GPRInfo::regT0, GPRInfo::regT1);
            jit.loadValue(CCallHelpers::addressFor(iter->value), regs);
            jit.storeValue(regs, CCallHelpers::addressFor(operand));
            continue;
        }

        InlineCallFrame* inlineCallFrame =
            jit.codeBlock()->jitCode()->dfg()->minifiedDFG.at(id)->inlineCallFrame();

        int stackOffset;
        if (inlineCallFrame)
            stackOffset = inlineCallFrame->stackOffset;
        else
            stackOffset = 0;

        if (!inlineCallFrame || inlineCallFrame->isClosureCall) {
            jit.loadPtr(
                AssemblyHelpers::addressFor(stackOffset + CallFrameSlot::callee),
                GPRInfo::regT0);
        } else {
            jit.move(
                AssemblyHelpers::TrustedImmPtr(inlineCallFrame->calleeRecovery.constant().asCell()),
                GPRInfo::regT0);
        }

        if (!inlineCallFrame || inlineCallFrame->isVarargs()) {
            jit.load32(
                AssemblyHelpers::payloadFor(stackOffset + CallFrameSlot::argumentCount),
                GPRInfo::regT1);
        } else {
            jit.move(
                AssemblyHelpers::TrustedImm32(inlineCallFrame->argumentCountIncludingThis),
                GPRInfo::regT1);
        }

        static_assert(std::is_same<decltype(operationCreateDirectArgumentsDuringExit), decltype(operationCreateClonedArgumentsDuringExit)>::value, "We assume these functions have the same signature below.");
        jit.setupArguments<decltype(operationCreateDirectArgumentsDuringExit)>(
            AssemblyHelpers::TrustedImmPtr(inlineCallFrame), GPRInfo::regT0, GPRInfo::regT1);
        switch (recovery.technique()) {
        case DirectArgumentsThatWereNotCreated:
            jit.move(AssemblyHelpers::TrustedImmPtr(tagCFunctionPtr<OperationPtrTag>(operationCreateDirectArgumentsDuringExit)), GPRInfo::nonArgGPR0);
            break;
        case ClonedArgumentsThatWereNotCreated:
            jit.move(AssemblyHelpers::TrustedImmPtr(tagCFunctionPtr<OperationPtrTag>(operationCreateClonedArgumentsDuringExit)), GPRInfo::nonArgGPR0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
        jit.storeCell(GPRInfo::returnValueGPR, AssemblyHelpers::addressFor(operand));

        alreadyAllocatedArguments.add(id, operand);
    }
}

void JIT_OPERATION OSRExit::compileOSRExit(ExecState* exec)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);

    if (vm->callFrameForCatch)
        RELEASE_ASSERT(vm->callFrameForCatch == exec);

    CodeBlock* codeBlock = exec->codeBlock();
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITCode::DFGJIT);

    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm->heap);

    uint32_t exitIndex = vm->osrExitIndex;
    OSRExit& exit = codeBlock->jitCode()->dfg()->osrExit[exitIndex];

    ASSERT(!vm->callFrameForCatch || exit.m_kind == GenericUnwind);
    EXCEPTION_ASSERT_UNUSED(scope, !!scope.exception() || !exit.isExceptionHandler());
    
    prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);

    // Compute the value recoveries.
    Operands<ValueRecovery> operands;
    codeBlock->jitCode()->dfg()->variableEventStream.reconstruct(codeBlock, exit.m_codeOrigin, codeBlock->jitCode()->dfg()->minifiedDFG, exit.m_streamIndex, operands);

    SpeculationRecovery* recovery = 0;
    if (exit.m_recoveryIndex != UINT_MAX)
        recovery = &codeBlock->jitCode()->dfg()->speculationRecovery[exit.m_recoveryIndex];

    {
        CCallHelpers jit(codeBlock);

        if (exit.m_kind == GenericUnwind) {
            // We are acting as a defacto op_catch because we arrive here from genericUnwind().
            // So, we must restore our call frame and stack pointer.
            jit.restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm->topEntryFrame);
            jit.loadPtr(vm->addressOfCallFrameForCatch(), GPRInfo::callFrameRegister);
        }
        jit.addPtr(
            CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)),
            GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

        jit.jitAssertHasValidCallFrame();

        if (UNLIKELY(vm->m_perBytecodeProfiler && codeBlock->jitCode()->dfgCommon()->compilation)) {
            Profiler::Database& database = *vm->m_perBytecodeProfiler;
            Profiler::Compilation* compilation = codeBlock->jitCode()->dfgCommon()->compilation.get();

            Profiler::OSRExit* profilerExit = compilation->addOSRExit(
                exitIndex, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
                exit.m_kind, exit.m_kind == UncountableInvalidation);
            jit.add64(CCallHelpers::TrustedImm32(1), CCallHelpers::AbsoluteAddress(profilerExit->counterAddress()));
        }

        compileExit(jit, *vm, exit, operands, recovery);

        LinkBuffer patchBuffer(jit, codeBlock);
        exit.m_code = FINALIZE_CODE_IF(
            shouldDumpDisassembly() || Options::verboseOSR() || Options::verboseDFGOSRExit(),
            patchBuffer, OSRExitPtrTag,
            "DFG OSR exit #%u (%s, %s) from %s, with operands = %s",
                exitIndex, toCString(exit.m_codeOrigin).data(),
                exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
                toCString(ignoringContext<DumpContext>(operands)).data());
    }

    MacroAssembler::repatchJump(exit.codeLocationForRepatch(), CodeLocationLabel<OSRExitPtrTag>(exit.m_code.code()));

    vm->osrExitJumpDestination = exit.m_code.code().executableAddress();
}

void OSRExit::compileExit(CCallHelpers& jit, VM& vm, const OSRExit& exit, const Operands<ValueRecovery>& operands, SpeculationRecovery* recovery)
{
    jit.jitAssertTagsInPlace();

    // Pro-forma stuff.
    if (Options::printEachOSRExit()) {
        SpeculationFailureDebugInfo* debugInfo = new SpeculationFailureDebugInfo;
        debugInfo->codeBlock = jit.codeBlock();
        debugInfo->kind = exit.m_kind;
        debugInfo->bytecodeOffset = exit.m_codeOrigin.bytecodeIndex;

        jit.debugCall(vm, debugOperationPrintSpeculationFailure, debugInfo);
    }

    // Perform speculation recovery. This only comes into play when an operation
    // starts mutating state before verifying the speculation it has already made.

    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            jit.sub32(recovery->src(), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::tagTypeNumberRegister, recovery->dest());
#endif
            break;

        case SpeculativeAddImmediate:
            jit.sub32(AssemblyHelpers::Imm32(recovery->immediate()), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::tagTypeNumberRegister, recovery->dest());
#endif
            break;

        case BooleanSpeculationCheck:
#if USE(JSVALUE64)
            jit.xor64(AssemblyHelpers::TrustedImm32(static_cast<int32_t>(ValueFalse)), recovery->dest());
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
            if (ArrayProfile* arrayProfile = jit.baselineCodeBlockFor(codeOrigin)->getArrayProfile(codeOrigin.bytecodeIndex)) {
#if USE(JSVALUE64)
                GPRReg usedRegister;
                if (exit.m_jsValueSource.isAddress())
                    usedRegister = exit.m_jsValueSource.base();
                else
                    usedRegister = exit.m_jsValueSource.gpr();
#else
                GPRReg usedRegister1;
                GPRReg usedRegister2;
                if (exit.m_jsValueSource.isAddress()) {
                    usedRegister1 = exit.m_jsValueSource.base();
                    usedRegister2 = InvalidGPRReg;
                } else {
                    usedRegister1 = exit.m_jsValueSource.payloadGPR();
                    if (exit.m_jsValueSource.hasKnownTag())
                        usedRegister2 = InvalidGPRReg;
                    else
                        usedRegister2 = exit.m_jsValueSource.tagGPR();
                }
#endif

                GPRReg scratch1;
                GPRReg scratch2;
#if USE(JSVALUE64)
                scratch1 = AssemblyHelpers::selectScratchGPR(usedRegister);
                scratch2 = AssemblyHelpers::selectScratchGPR(usedRegister, scratch1);
#else
                scratch1 = AssemblyHelpers::selectScratchGPR(usedRegister1, usedRegister2);
                scratch2 = AssemblyHelpers::selectScratchGPR(usedRegister1, usedRegister2, scratch1);
#endif

                if (isARM64()) {
                    jit.pushToSave(scratch1);
                    jit.pushToSave(scratch2);
                } else {
                    jit.push(scratch1);
                    jit.push(scratch2);
                }

                GPRReg value;
                if (exit.m_jsValueSource.isAddress()) {
                    value = scratch1;
                    jit.loadPtr(AssemblyHelpers::Address(exit.m_jsValueSource.asAddress()), value);
                } else
                    value = exit.m_jsValueSource.payloadGPR();

                jit.load32(AssemblyHelpers::Address(value, JSCell::structureIDOffset()), scratch1);
                jit.store32(scratch1, arrayProfile->addressOfLastSeenStructureID());
#if USE(JSVALUE64)
                jit.load8(AssemblyHelpers::Address(value, JSCell::indexingTypeAndMiscOffset()), scratch1);
#else
                jit.load8(AssemblyHelpers::Address(scratch1, Structure::indexingModeIncludingHistoryOffset()), scratch1);
#endif
                jit.and32(AssemblyHelpers::TrustedImm32(IndexingModeMask), scratch1);
                jit.move(AssemblyHelpers::TrustedImm32(1), scratch2);
                jit.lshift32(scratch1, scratch2);
                jit.or32(scratch2, AssemblyHelpers::AbsoluteAddress(arrayProfile->addressOfArrayModes()));

                if (isARM64()) {
                    jit.popToRestore(scratch2);
                    jit.popToRestore(scratch1);
                } else {
                    jit.pop(scratch2);
                    jit.pop(scratch1);
                }
            }
        }

        if (MethodOfGettingAValueProfile profile = exit.m_valueProfile) {
#if USE(JSVALUE64)
            if (exit.m_jsValueSource.isAddress()) {
                // We can't be sure that we have a spare register. So use the tagTypeNumberRegister,
                // since we know how to restore it.
                jit.load64(AssemblyHelpers::Address(exit.m_jsValueSource.asAddress()), GPRInfo::tagTypeNumberRegister);
                profile.emitReportValue(jit, JSValueRegs(GPRInfo::tagTypeNumberRegister));
                jit.move(AssemblyHelpers::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
            } else
                profile.emitReportValue(jit, JSValueRegs(exit.m_jsValueSource.gpr()));
#else // not USE(JSVALUE64)
            if (exit.m_jsValueSource.isAddress()) {
                // Save a register so we can use it.
                GPRReg scratchPayload = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.base());
                GPRReg scratchTag = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.base(), scratchPayload);
                jit.pushToSave(scratchPayload);
                jit.pushToSave(scratchTag);

                JSValueRegs scratch(scratchTag, scratchPayload);
                
                jit.loadValue(exit.m_jsValueSource.asAddress(), scratch);
                profile.emitReportValue(jit, scratch);
                
                jit.popToRestore(scratchTag);
                jit.popToRestore(scratchPayload);
            } else if (exit.m_jsValueSource.hasKnownTag()) {
                GPRReg scratchTag = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.payloadGPR());
                jit.pushToSave(scratchTag);
                jit.move(AssemblyHelpers::TrustedImm32(exit.m_jsValueSource.tag()), scratchTag);
                JSValueRegs value(scratchTag, exit.m_jsValueSource.payloadGPR());
                profile.emitReportValue(jit, value);
                jit.popToRestore(scratchTag);
            } else
                profile.emitReportValue(jit, exit.m_jsValueSource.regs());
#endif // USE(JSVALUE64)
        }
    }

    // What follows is an intentionally simple OSR exit implementation that generates
    // fairly poor code but is very easy to hack. In particular, it dumps all state that
    // needs conversion into a scratch buffer so that in step 6, where we actually do the
    // conversions, we know that all temp registers are free to use and the variable is
    // definitely in a well-known spot in the scratch buffer regardless of whether it had
    // originally been in a register or spilled. This allows us to decouple "where was
    // the variable" from "how was it represented". Consider that the
    // Int32DisplacedInJSStack recovery: it tells us that the value is in a
    // particular place and that that place holds an unboxed int32. We have two different
    // places that a value could be (displaced, register) and a bunch of different
    // ways of representing a value. The number of recoveries is two * a bunch. The code
    // below means that we have to have two + a bunch cases rather than two * a bunch.
    // Once we have loaded the value from wherever it was, the reboxing is the same
    // regardless of its location. Likewise, before we do the reboxing, the way we get to
    // the value (i.e. where we load it from) is the same regardless of its type. Because
    // the code below always dumps everything into a scratch buffer first, the two
    // questions become orthogonal, which simplifies adding new types and adding new
    // locations.
    //
    // This raises the question: does using such a suboptimal implementation of OSR exit,
    // where we always emit code to dump all state into a scratch buffer only to then
    // dump it right back into the stack, hurt us in any way? The asnwer is that OSR exits
    // are rare. Our tiering strategy ensures this. This is because if an OSR exit is
    // taken more than ~100 times, we jettison the DFG code block along with all of its
    // exits. It is impossible for an OSR exit - i.e. the code we compile below - to
    // execute frequently enough for the codegen to matter that much. It probably matters
    // enough that we don't want to turn this into some super-slow function call, but so
    // long as we're generating straight-line code, that code can be pretty bad. Also
    // because we tend to exit only along one OSR exit from any DFG code block - that's an
    // empirical result that we're extremely confident about - the code size of this
    // doesn't matter much. Hence any attempt to optimize the codegen here is just purely
    // harmful to the system: it probably won't reduce either net memory usage or net
    // execution time. It will only prevent us from cleanly decoupling "where was the
    // variable" from "how was it represented", which will make it more difficult to add
    // features in the future and it will make it harder to reason about bugs.

    // Save all state from GPRs into the scratch buffer.

    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(sizeof(EncodedJSValue) * operands.size());
    EncodedJSValue* scratch = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        switch (recovery.technique()) {
        case UnboxedInt32InGPR:
        case UnboxedCellInGPR:
#if USE(JSVALUE64)
        case InGPR:
        case UnboxedInt52InGPR:
        case UnboxedStrictInt52InGPR:
            jit.store64(recovery.gpr(), scratch + index);
            break;
#else
        case UnboxedBooleanInGPR:
            jit.store32(
                recovery.gpr(),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload);
            break;
            
        case InPair:
            jit.store32(
                recovery.tagGPR(),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
            jit.store32(
                recovery.payloadGPR(),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload);
            break;
#endif

        default:
            break;
        }
    }

    // And voila, all GPRs are free to reuse.

    // Save all state from FPRs into the scratch buffer.

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        switch (recovery.technique()) {
        case UnboxedDoubleInFPR:
        case InFPR:
            jit.move(AssemblyHelpers::TrustedImmPtr(scratch + index), GPRInfo::regT0);
            jit.storeDouble(recovery.fpr(), MacroAssembler::Address(GPRInfo::regT0));
            break;

        default:
            break;
        }
    }

    // Now, all FPRs are also free.

    // Save all state from the stack into the scratch buffer. For simplicity we
    // do this even for state that's already in the right place on the stack.
    // It makes things simpler later.

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        switch (recovery.technique()) {
        case DisplacedInJSStack:
        case CellDisplacedInJSStack:
        case BooleanDisplacedInJSStack:
        case Int32DisplacedInJSStack:
        case DoubleDisplacedInJSStack:
#if USE(JSVALUE64)
        case Int52DisplacedInJSStack:
        case StrictInt52DisplacedInJSStack:
            jit.load64(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, scratch + index);
            break;
#else
            jit.load32(
                AssemblyHelpers::tagFor(recovery.virtualRegister()),
                GPRInfo::regT0);
            jit.load32(
                AssemblyHelpers::payloadFor(recovery.virtualRegister()),
                GPRInfo::regT1);
            jit.store32(
                GPRInfo::regT0,
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
            jit.store32(
                GPRInfo::regT1,
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload);
            break;
#endif

        default:
            break;
        }
    }

    // Need to ensure that the stack pointer accounts for the worst-case stack usage at exit. This
    // could toast some stack that the DFG used. We need to do it before storing to stack offsets
    // used by baseline.
    jit.addPtr(
        CCallHelpers::TrustedImm32(
            -jit.codeBlock()->jitCode()->dfgCommon()->requiredRegisterCountForExit * sizeof(Register)),
        CCallHelpers::framePointerRegister, CCallHelpers::stackPointerRegister);

    // Restore the DFG callee saves and then save the ones the baseline JIT uses.
    jit.emitRestoreCalleeSaves();
    jit.emitSaveCalleeSavesFor(jit.baselineCodeBlock());

    // The tag registers are needed to materialize recoveries below.
    jit.emitMaterializeTagCheckRegisters();

    if (exit.isExceptionHandler())
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame);

    // Do all data format conversions and store the results into the stack.

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];
        VirtualRegister reg = operands.virtualRegisterForIndex(index);

        if (reg.isLocal() && reg.toLocal() < static_cast<int>(jit.baselineCodeBlock()->calleeSaveSpaceAsVirtualRegisters()))
            continue;

        int operand = reg.offset();

        switch (recovery.technique()) {
        case DisplacedInJSStack:
        case InFPR:
#if USE(JSVALUE64)
        case InGPR:
        case UnboxedCellInGPR:
        case CellDisplacedInJSStack:
        case BooleanDisplacedInJSStack:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
            break;
#else // not USE(JSVALUE64)
        case InPair:
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag,
                GPRInfo::regT0);
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT1);
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT1,
                AssemblyHelpers::payloadFor(operand));
            break;

        case UnboxedCellInGPR:
        case CellDisplacedInJSStack:
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT0);
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::CellTag),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::payloadFor(operand));
            break;

        case UnboxedBooleanInGPR:
        case BooleanDisplacedInJSStack:
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT0);
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::BooleanTag),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::payloadFor(operand));
            break;
#endif // USE(JSVALUE64)

        case UnboxedInt32InGPR:
        case Int32DisplacedInJSStack:
#if USE(JSVALUE64)
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.zeroExtend32ToPtr(GPRInfo::regT0, GPRInfo::regT0);
            jit.or64(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
#else
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT0);
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::Int32Tag),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::payloadFor(operand));
#endif
            break;

#if USE(JSVALUE64)
        case UnboxedInt52InGPR:
        case Int52DisplacedInJSStack:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.rshift64(
                AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
            break;

        case UnboxedStrictInt52InGPR:
        case StrictInt52DisplacedInJSStack:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
            break;
#endif

        case UnboxedDoubleInFPR:
        case DoubleDisplacedInJSStack:
            jit.move(AssemblyHelpers::TrustedImmPtr(scratch + index), GPRInfo::regT0);
            jit.loadDouble(MacroAssembler::Address(GPRInfo::regT0), FPRInfo::fpRegT0);
            jit.purifyNaN(FPRInfo::fpRegT0);
#if USE(JSVALUE64)
            jit.boxDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
#else
            jit.storeDouble(FPRInfo::fpRegT0, AssemblyHelpers::addressFor(operand));
#endif
            break;

        case Constant:
#if USE(JSVALUE64)
            jit.store64(
                AssemblyHelpers::TrustedImm64(JSValue::encode(recovery.constant())),
                AssemblyHelpers::addressFor(operand));
#else
            jit.store32(
                AssemblyHelpers::TrustedImm32(recovery.constant().tag()),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                AssemblyHelpers::TrustedImm32(recovery.constant().payload()),
                AssemblyHelpers::payloadFor(operand));
#endif
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

    // Now that things on the stack are recovered, do the arguments recovery. We assume that arguments
    // recoveries don't recursively refer to each other. But, we don't try to assume that they only
    // refer to certain ranges of locals. Hence why we need to do this here, once the stack is sensible.
    // Note that we also roughly assume that the arguments might still be materialized outside of its
    // inline call frame scope - but for now the DFG wouldn't do that.

    emitRestoreArguments(jit, operands);

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

    handleExitCounts(jit, exit);

    // Reify inlined call frames.

    reifyInlinedCallFrames(jit, exit);

    // And finish.
    adjustAndJumpToTarget(vm, jit, exit);
}

void JIT_OPERATION OSRExit::debugOperationPrintSpeculationFailure(ExecState* exec, void* debugInfoRaw, void* scratch)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    SpeculationFailureDebugInfo* debugInfo = static_cast<SpeculationFailureDebugInfo*>(debugInfoRaw);
    CodeBlock* codeBlock = debugInfo->codeBlock;
    CodeBlock* alternative = codeBlock->alternative();
    dataLog("Speculation failure in ", *codeBlock);
    dataLog(" @ exit #", vm->osrExitIndex, " (bc#", debugInfo->bytecodeOffset, ", ", exitKindToString(debugInfo->kind), ") with ");
    if (alternative) {
        dataLog(
            "executeCounter = ", alternative->jitExecuteCounter(),
            ", reoptimizationRetryCounter = ", alternative->reoptimizationRetryCounter(),
            ", optimizationDelayCounter = ", alternative->optimizationDelayCounter());
    } else
        dataLog("no alternative code block (i.e. we've been jettisoned)");
    dataLog(", osrExitCounter = ", codeBlock->osrExitCounter(), "\n");
    dataLog("    GPRs at time of exit:");
    char* scratchPointer = static_cast<char*>(scratch);
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg gpr = GPRInfo::toRegister(i);
        dataLog(" ", GPRInfo::debugName(gpr), ":", RawPointer(*reinterpret_cast_ptr<void**>(scratchPointer)));
        scratchPointer += sizeof(EncodedJSValue);
    }
    dataLog("\n");
    dataLog("    FPRs at time of exit:");
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg fpr = FPRInfo::toRegister(i);
        dataLog(" ", FPRInfo::debugName(fpr), ":");
        uint64_t bits = *reinterpret_cast_ptr<uint64_t*>(scratchPointer);
        double value = *reinterpret_cast_ptr<double*>(scratchPointer);
        dataLogF("%llx:%lf", static_cast<long long>(bits), value);
        scratchPointer += sizeof(EncodedJSValue);
    }
    dataLog("\n");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
