/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "WasmBBQJIT.h"
#include "WasmBBQJIT32_64.h"
#include "WasmBBQJIT64.h"

#if ENABLE(WEBASSEMBLY_BBQJIT)

#include "B3Common.h"
#include "B3ValueRep.h"
#include "BinarySwitch.h"
#include "BytecodeStructs.h"
#include "CCallHelpers.h"
#include "CPU.h"
#include "CompilerTimingScope.h"
#include "GPRInfo.h"
#include "JSCast.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyStruct.h"
#include "MacroAssembler.h"
#include "RegisterSet.h"
#include "WasmBBQDisassembler.h"
#include "WasmCallingConvention.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionParser.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmMemoryInformation.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmOMGIRGenerator.h"
#include "WasmOperations.h"
#include "WasmOps.h"
#include "WasmThunks.h"
#include "WasmTypeDefinition.h"
#include <bit>
#include <cmath>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/SmallSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

namespace BBQJITImpl {

Location Location::none()
{
    Location loc;
    loc.m_kind = None;
    return loc;
}

Location Location::fromStack(int32_t stackOffset)
{
    Location loc;
    loc.m_kind = Stack;
    loc.m_offset = stackOffset;
    return loc;
}

Location Location::fromStackArgument(int32_t stackOffset)
{
    Location loc;
    loc.m_kind = StackArgument;
    loc.m_offset = stackOffset;
    return loc;
}

Location Location::fromGPR(GPRReg gpr)
{
    Location loc;
    loc.m_kind = Gpr;
    loc.m_gpr = gpr;
    return loc;
}

Location Location::fromFPR(FPRReg fpr)
{
    Location loc;
    loc.m_kind = Fpr;
    loc.m_fpr = fpr;
    return loc;
}

Location Location::fromGlobal(int32_t globalOffset)
{
    Location loc;
    loc.m_kind = Global;
    loc.m_offset = globalOffset;
    return loc;
}

bool Location::isNone() const
{
    return m_kind == None;
}

bool Location::isGPR() const
{
    return m_kind == Gpr;
}

bool Location::isGPR2() const
{
    return m_kind == Gpr2;
}

bool Location::isFPR() const
{
    return m_kind == Fpr;
}

bool Location::isStack() const
{
    return m_kind == Stack;
}

bool Location::isStackArgument() const
{
    return m_kind == StackArgument;
}

bool Location::isGlobal() const
{
    return m_kind == Global;
}

bool Location::isMemory() const
{
    return isStack() || isStackArgument() || isGlobal();
}

int32_t Location::asStackOffset() const
{
    ASSERT(isStack());
    return m_offset;
}

Address Location::asStackAddress() const
{
    ASSERT(isStack());
    return Address(GPRInfo::callFrameRegister, asStackOffset());
}

int32_t Location::asGlobalOffset() const
{
    ASSERT(isGlobal());
    return m_offset;
}

Address Location::asGlobalAddress() const
{
    ASSERT(isGlobal());
    return Address(GPRInfo::wasmContextInstancePointer, asGlobalOffset());
}

int32_t Location::asStackArgumentOffset() const
{
    ASSERT(isStackArgument());
    return m_offset;
}

Address Location::asStackArgumentAddress() const
{
    ASSERT(isStackArgument());
    return Address(MacroAssembler::stackPointerRegister, asStackArgumentOffset());
}

Address Location::asAddress() const
{
    switch (m_kind) {
    case Stack:
        return asStackAddress();
    case Global:
        return asGlobalAddress();
    case StackArgument:
        return asStackArgumentAddress();
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

GPRReg Location::asGPR() const
{
    ASSERT(isGPR());
    return m_gpr;
}

FPRReg Location::asFPR() const
{
    ASSERT(isFPR());
    return m_fpr;
}

GPRReg Location::asGPRlo() const
{
    ASSERT(isGPR2());
    return m_gprlo;
}

GPRReg Location::asGPRhi() const
{
    ASSERT(isGPR2());
    return m_gprhi;
}

void Location::dump(PrintStream& out) const
{
    switch (m_kind) {
    case None:
        out.print("None");
        break;
    case Gpr:
        out.print("GPR(", MacroAssembler::gprName(m_gpr), ")");
        break;
    case Fpr:
        out.print("FPR(", MacroAssembler::fprName(m_fpr), ")");
        break;
    case Stack:
        out.print("Stack(", m_offset, ")");
        break;
    case Global:
        out.print("Global(", m_offset, ")");
        break;
    case StackArgument:
        out.print("StackArgument(", m_offset, ")");
        break;
    case Gpr2:
        out.print("GPR2(", m_gprhi, ",", m_gprlo, ")");
        break;
    }
}

bool Location::operator==(Location other) const
{
    if (m_kind != other.m_kind)
        return false;
    switch (m_kind) {
    case Gpr:
        return m_gpr == other.m_gpr;
    case Gpr2:
        return m_gprlo == other.m_gprlo && m_gprhi == other.m_gprhi;
    case Fpr:
        return m_fpr == other.m_fpr;
    case Stack:
    case StackArgument:
    case Global:
        return m_offset == other.m_offset;
    case None:
        return true;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

Location::Kind Location::kind() const
{
    return Kind(m_kind);
}

bool BBQJIT::isValidValueTypeKind(TypeKind kind)
{
    switch (kind) {
    case TypeKind::I64:
    case TypeKind::I32:
    case TypeKind::F64:
    case TypeKind::F32:
    case TypeKind::V128:
        return true;
    default:
        return false;
    }
}

TypeKind BBQJIT::pointerType() { return is64Bit() ? TypeKind::I64 : TypeKind::I32; }

bool BBQJIT::isFloatingPointType(TypeKind type)
{
    return type == TypeKind::F32 || type == TypeKind::F64 || type == TypeKind::V128;
}

TypeKind BBQJIT::toValueKind(TypeKind kind)
{
    switch (kind) {
    case TypeKind::I32:
    case TypeKind::F32:
    case TypeKind::I64:
    case TypeKind::F64:
    case TypeKind::V128:
        return kind;
    case TypeKind::Func:
    case TypeKind::I31ref:
    case TypeKind::Funcref:
    case TypeKind::Exn:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Rec:
    case TypeKind::Sub:
    case TypeKind::Subfinal:
    case TypeKind::Struct:
    case TypeKind::Structref:
    case TypeKind::Externref:
    case TypeKind::Array:
    case TypeKind::Arrayref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        return TypeKind::I64;
    case TypeKind::Void:
        RELEASE_ASSERT_NOT_REACHED();
        return kind;
    }
    return kind;
}

void Value::dump(PrintStream& out) const
{
    switch (m_kind) {
    case Const:
        out.print("Const(");
        if (m_type == TypeKind::I32)
            out.print(m_i32);
        else if (m_type == TypeKind::I64)
            out.print(m_i64);
        else if (m_type == TypeKind::F32)
            out.print(m_f32);
        else if (m_type == TypeKind::F64)
            out.print(m_f64);
        out.print(")");
        break;
    case Local:
        out.print("Local(", m_index, ")");
        break;
    case Temp:
        out.print("Temp(", m_index, ")");
        break;
    case None:
        out.print("None");
        break;
    case Pinned:
        out.print(m_pinned);
    }
}

RegisterBinding RegisterBinding::fromValue(Value value)
{
    ASSERT(value.isLocal() || value.isTemp());
    RegisterBinding binding;
    binding.m_type = value.type();
    binding.m_kind = value.isLocal() ? Local : Temp;
    binding.m_index = value.isLocal() ? value.asLocal() : value.asTemp();
    return binding;
}

RegisterBinding RegisterBinding::none()
{
    RegisterBinding binding;
    binding.m_kind = None;
    return binding;
}

RegisterBinding RegisterBinding::scratch()
{
    RegisterBinding binding;
    binding.m_kind = Scratch;
    return binding;
}

Value RegisterBinding::toValue() const
{
    switch (m_kind) {
    case None:
    case Scratch:
        return Value::none();
    case Local:
        return Value::fromLocal(m_type, m_index);
    case Temp:
        return Value::fromTemp(m_type, m_index);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool RegisterBinding::isNone() const
{
    return m_kind == None;
}

bool RegisterBinding::isValid() const
{
    return m_kind != None;
}

bool RegisterBinding::isScratch() const
{
    return m_kind == Scratch;
}

bool RegisterBinding::operator==(RegisterBinding other) const
{
    if (m_kind != other.m_kind)
        return false;

    return m_kind == None || (m_index == other.m_index && m_type == other.m_type);
}

void RegisterBinding::dump(PrintStream& out) const
{
    switch (m_kind) {
    case None:
        out.print("None");
        break;
    case Scratch:
        out.print("Scratch");
        break;
    case Local:
        out.print("Local(", m_index, ")");
        break;
    case Temp:
        out.print("Temp(", m_index, ")");
        break;
    }
}

unsigned RegisterBinding::hash() const
{
    return pairIntHash(static_cast<unsigned>(m_kind), m_index);
}

uint32_t RegisterBinding::encode() const
{
    return m_uintValue;
}

ControlData::ControlData(BBQJIT& generator, BlockType blockType, BlockSignature signature, LocalOrTempIndex enclosedHeight, RegisterSet liveScratchGPRs = { }, RegisterSet liveScratchFPRs = { })
    : m_signature(signature)
    , m_blockType(blockType)
    , m_enclosedHeight(enclosedHeight)
{
    if (blockType == BlockType::TopLevel) {
        // Abide by calling convention instead.
        CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*signature, CallRole::Callee);
        for (unsigned i = 0; i < signature->argumentCount(); ++i)
            m_argumentLocations.append(Location::fromArgumentLocation(wasmCallInfo.params[i], signature->argumentType(i).kind));
        for (unsigned i = 0; i < signature->returnCount(); ++i)
            m_resultLocations.append(Location::fromArgumentLocation(wasmCallInfo.results[i], signature->returnType(i).kind));
        return;
    }

    if (!isAnyCatch(*this)) {
        auto gprSetCopy = generator.m_validGPRs;
        auto fprSetCopy = generator.m_validFPRs;
        liveScratchGPRs.forEach([&] (auto r) { gprSetCopy.remove(r); });
        liveScratchFPRs.forEach([&] (auto r) { fprSetCopy.remove(r); });

        for (unsigned i = 0; i < signature->argumentCount(); ++i)
            m_argumentLocations.append(allocateArgumentOrResult(generator, signature->argumentType(i).kind, i, gprSetCopy, fprSetCopy));
    }

    auto gprSetCopy = generator.m_validGPRs;
    auto fprSetCopy = generator.m_validFPRs;
    for (unsigned i = 0; i < signature->returnCount(); ++i)
        m_resultLocations.append(allocateArgumentOrResult(generator, signature->returnType(i).kind, i, gprSetCopy, fprSetCopy));
}

// This function is intentionally not using implicitSlots since arguments and results should not include implicit slot.

void ControlData::convertIfToBlock()
{
    ASSERT(m_blockType == BlockType::If);
    m_blockType = BlockType::Block;
}

void ControlData::convertLoopToBlock()
{
    ASSERT(m_blockType == BlockType::Loop);
    m_blockType = BlockType::Block;
}

void ControlData::addBranch(Jump jump)
{
    m_branchList.append(jump);
}

void ControlData::addLabel(Box<CCallHelpers::Label>&& label)
{
    m_labels.append(WTFMove(label));
}

void ControlData::delegateJumpsTo(ControlData& delegateTarget)
{
    delegateTarget.m_branchList.append(std::exchange(m_branchList, { }));
    delegateTarget.m_labels.appendVector(std::exchange(m_labels, { }));
}

void ControlData::linkJumps(MacroAssembler::AbstractMacroAssemblerType* masm)
{
    m_branchList.link(masm);
    fillLabels(masm->label());
}

void ControlData::linkJumpsTo(MacroAssembler::Label label, MacroAssembler::AbstractMacroAssemblerType* masm)
{
    m_branchList.linkTo(label, masm);
    fillLabels(label);
}

void ControlData::linkIfBranch(MacroAssembler::AbstractMacroAssemblerType* masm)
{
    ASSERT(m_blockType == BlockType::If);
    if (m_ifBranch.isSet())
        m_ifBranch.link(masm);
}

void ControlData::dump(PrintStream& out) const
{
    UNUSED_PARAM(out);
}

LocalOrTempIndex ControlData::enclosedHeight() const
{
    return m_enclosedHeight;
}

unsigned ControlData::implicitSlots() const { return isAnyCatch(*this) ? 1 : 0; }

const Vector<Location, 2>& ControlData::targetLocations() const
{
    return blockType() == BlockType::Loop
        ? argumentLocations()
        : resultLocations();
}

const Vector<Location, 2>& ControlData::argumentLocations() const
{
    return m_argumentLocations;
}

const Vector<Location, 2>& ControlData::resultLocations() const
{
    return m_resultLocations;
}

BlockType ControlData::blockType() const { return m_blockType; }
BlockSignature ControlData::signature() const { return m_signature; }

FunctionArgCount ControlData::branchTargetArity() const
{
    if (blockType() == BlockType::Loop)
        return m_signature->argumentCount();
    return m_signature->returnCount();
}

Type ControlData::branchTargetType(unsigned i) const
{
    ASSERT(i < branchTargetArity());
    if (m_blockType == BlockType::Loop)
        return m_signature->argumentType(i);
    return m_signature->returnType(i);
}

Type ControlData::argumentType(unsigned i) const
{
    ASSERT(i < m_signature->argumentCount());
    return m_signature->argumentType(i);
}

CatchKind ControlData::catchKind() const
{
    ASSERT(m_blockType == BlockType::Catch);
    return m_catchKind;
}

void ControlData::setCatchKind(CatchKind catchKind)
{
    ASSERT(m_blockType == BlockType::Catch);
    m_catchKind = catchKind;
}

unsigned ControlData::tryStart() const
{
    return m_tryStart;
}

unsigned ControlData::tryEnd() const
{
    return m_tryEnd;
}

unsigned ControlData::tryCatchDepth() const
{
    return m_tryCatchDepth;
}

void ControlData::setTryInfo(unsigned tryStart, unsigned tryEnd, unsigned tryCatchDepth)
{
    m_tryStart = tryStart;
    m_tryEnd = tryEnd;
    m_tryCatchDepth = tryCatchDepth;
}

void ControlData::setTryTableTargets(TargetList &&targets)
{
    m_tryTableTargets = WTFMove(targets);
}

void ControlData::setIfBranch(MacroAssembler::Jump branch)
{
    ASSERT(m_blockType == BlockType::If);
    m_ifBranch = branch;
}

void ControlData::setLoopLabel(MacroAssembler::Label label)
{
    ASSERT(m_blockType == BlockType::Loop);
    m_loopLabel = label;
}

const MacroAssembler::Label& ControlData::loopLabel() const
{
    return m_loopLabel;
}

void ControlData::touch(LocalOrTempIndex local)
{
    m_touchedLocals.add(local);
}

void ControlData::fillLabels(CCallHelpers::Label label)
{
    for (auto& box : m_labels)
        *box = label;
}

BBQJIT::BBQJIT(CCallHelpers& jit, const TypeDefinition& signature, BBQCallee& callee, const FunctionData& function, FunctionCodeIndex functionIndex, const ModuleInformation& info, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, InternalFunction* compilation, std::optional<bool> hasExceptionHandlers, unsigned loopIndexForOSREntry)
    : m_jit(jit)
    , m_callee(callee)
    , m_function(function)
    , m_functionSignature(signature.expand().as<FunctionSignature>())
    , m_functionIndex(functionIndex)
    , m_info(info)
    , m_mode(mode)
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_directCallees(m_info.internalFunctionCount())
    , m_hasExceptionHandlers(hasExceptionHandlers)
    , m_loopIndexForOSREntry(loopIndexForOSREntry)
    , m_gprBindings(jit.numberOfRegisters(), RegisterBinding::none())
    , m_fprBindings(jit.numberOfFPRegisters(), RegisterBinding::none())
    , m_gprLRU(jit.numberOfRegisters())
    , m_fprLRU(jit.numberOfFPRegisters())
    , m_lastUseTimestamp(0)
    , m_compilation(compilation)
    , m_pcToCodeOriginMapBuilder(Options::useSamplingProfiler())
{
    RegisterSetBuilder gprSetBuilder = RegisterSetBuilder::allGPRs();
    gprSetBuilder.exclude(RegisterSetBuilder::specialRegisters());
    gprSetBuilder.exclude(RegisterSetBuilder::macroClobberedGPRs());
    gprSetBuilder.exclude(RegisterSetBuilder::wasmPinnedRegisters());
    // TODO: handle callee-saved registers better.
    gprSetBuilder.exclude(RegisterSetBuilder::vmCalleeSaveRegisters());

    RegisterSetBuilder fprSetBuilder = RegisterSetBuilder::allFPRs();
    RegisterSetBuilder::macroClobberedFPRs().forEach([&](Reg reg) {
        fprSetBuilder.remove(reg);
    });
#if USE(JSVALUE32_64) && CPU(ARM_NEON)
    // Remove NEON fprs
    for (auto reg = ARMRegisters::d16; reg <= ARMRegisters::d31; reg = MacroAssembler::nextFPRegister(reg))
        fprSetBuilder.remove(reg);
#endif
    // TODO: handle callee-saved registers better.
    RegisterSetBuilder::vmCalleeSaveRegisters().forEach([&](Reg reg) {
        fprSetBuilder.remove(reg);
    });

    RegisterSetBuilder callerSaveGprs = gprSetBuilder;
    RegisterSetBuilder callerSaveFprs = fprSetBuilder;

    gprSetBuilder.remove(wasmScratchGPR);
#if USE(JSVALUE32_64)
    gprSetBuilder.remove(wasmScratchGPR2);
#endif
    fprSetBuilder.remove(wasmScratchFPR);

    m_gprSet = m_validGPRs = gprSetBuilder.buildAndValidate();
    m_fprSet = m_validFPRs = fprSetBuilder.buildAndValidate();
    m_callerSaveGPRs = callerSaveGprs.buildAndValidate();
    m_callerSaveFPRs = callerSaveFprs.buildAndValidate();
    m_callerSaves = callerSaveGprs.merge(callerSaveFprs).buildAndValidate();

    m_gprLRU.add(m_gprSet);
    m_fprLRU.add(m_fprSet);

    if ((Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tUsing GPR set: ", m_gprSet, "\n   \tFPR set: ", m_fprSet);

    if (UNLIKELY(shouldDumpDisassemblyFor(CompilationMode::BBQMode))) {
        m_disassembler = makeUnique<BBQDisassembler>();
        m_disassembler->setStartOfCode(m_jit.label());
    }

    CallInformation callInfo = wasmCallingConvention().callInformationFor(signature.expand(), CallRole::Callee);
    ASSERT(callInfo.params.size() == m_functionSignature->argumentCount());
    for (unsigned i = 0; i < m_functionSignature->argumentCount(); i ++) {
        const Type& type = m_functionSignature->argumentType(i);
        m_localSlots.append(allocateStack(Value::fromLocal(type.kind, i)));
        m_locals.append(Location::none());
        m_localTypes.append(type.kind);

        Value parameter = Value::fromLocal(type.kind, i);
        bind(parameter, Location::fromArgumentLocation(callInfo.params[i], type.kind));
        m_arguments.append(i);
    }
    m_localStorage = m_frameSize; // All stack slots allocated so far are locals.
}

bool BBQJIT::canTierUpToOMG() const
{
    if (!Options::useOMGJIT())
        return false;

    if (!Options::useBBQTierUpChecks())
        return false;

    if (m_function.data.size() > Options::maximumOMGCandidateCost())
        return false;

    return true;
}

void BBQJIT::setParser(FunctionParser<BBQJIT>* parser)
{
    m_parser = parser;
}

bool BBQJIT::addArguments(const TypeDefinition& signature)
{
    RELEASE_ASSERT(m_arguments.size() == signature.as<FunctionSignature>()->argumentCount()); // We handle arguments in the prologue
    return true;
}

Value BBQJIT::addConstant(Type type, uint64_t value)
{
    Value result;
    switch (type.kind) {
    case TypeKind::I32:
        result = Value::fromI32(value);
        LOG_INSTRUCTION("I32Const", RESULT(result));
        break;
    case TypeKind::I64:
        result = Value::fromI64(value);
        LOG_INSTRUCTION("I64Const", RESULT(result));
        break;
    case TypeKind::F32: {
        int32_t tmp = static_cast<int32_t>(value);
        result = Value::fromF32(*reinterpret_cast<float*>(&tmp));
        LOG_INSTRUCTION("F32Const", RESULT(result));
        break;
    }
    case TypeKind::F64:
        result = Value::fromF64(*reinterpret_cast<double*>(&value));
        LOG_INSTRUCTION("F64Const", RESULT(result));
        break;
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Structref:
    case TypeKind::Arrayref:
    case TypeKind::Funcref:
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        result = Value::fromRef(type.kind, static_cast<EncodedJSValue>(value));
        LOG_INSTRUCTION("RefConst", makeString(type.kind), RESULT(result));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant type.\n");
        return Value::none();
    }
    return result;
}

PartialResult BBQJIT::addDrop(Value value)
{
    LOG_INSTRUCTION("Drop", value);
    consume(value);
    return { };
}

PartialResult BBQJIT::addLocal(Type type, uint32_t numberOfLocals)
{
    for (uint32_t i = 0; i < numberOfLocals; i ++) {
        uint32_t localIndex = m_locals.size();
        m_localSlots.append(allocateStack(Value::fromLocal(type.kind, localIndex)));
        m_localStorage = m_frameSize;
        m_locals.append(m_localSlots.last());
        m_localTypes.append(type.kind);
    }
    return { };
}

// Tables

PartialResult WARN_UNUSED_RETURN BBQJIT::addTableSet(unsigned tableIndex, Value index, Value value)
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(tableIndex),
        index,
        value
    };

    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationSetWasmTableElement, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("TableSet", tableIndex, index, value);

    throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length)
{
    ASSERT(dstOffset.type() == TypeKind::I32);
    ASSERT(srcOffset.type() == TypeKind::I32);
    ASSERT(length.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(elementIndex),
        Value::fromI32(tableIndex),
        dstOffset,
        srcOffset,
        length
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmTableInit, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("TableInit", tableIndex, dstOffset, srcOffset, length);

    throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addElemDrop(unsigned elementIndex)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(elementIndex)
    };
    emitCCall(&operationWasmElemDrop, arguments);

    LOG_INSTRUCTION("ElemDrop", elementIndex);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTableSize(unsigned tableIndex, Value& result)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(tableIndex)
    };
    result = topValue(TypeKind::I32);
    emitCCall(&operationGetWasmTableSize, arguments, result);

    LOG_INSTRUCTION("TableSize", tableIndex, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTableGrow(unsigned tableIndex, Value fill, Value delta, Value& result)
{
    ASSERT(delta.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(tableIndex),
        fill, delta
    };
    result = topValue(TypeKind::I32);
    emitCCall(&operationWasmTableGrow, arguments, result);

    LOG_INSTRUCTION("TableGrow", tableIndex, fill, delta, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTableFill(unsigned tableIndex, Value offset, Value fill, Value count)
{
    ASSERT(offset.type() == TypeKind::I32);
    ASSERT(count.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(tableIndex),
        offset, fill, count
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmTableFill, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("TableFill", tableIndex, fill, offset, count);

    throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, Value dstOffset, Value srcOffset, Value length)
{
    ASSERT(dstOffset.type() == TypeKind::I32);
    ASSERT(srcOffset.type() == TypeKind::I32);
    ASSERT(length.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(dstTableIndex),
        Value::fromI32(srcTableIndex),
        dstOffset, srcOffset, length
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmTableCopy, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("TableCopy", dstTableIndex, srcTableIndex, dstOffset, srcOffset, length);

    throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

// Locals

PartialResult WARN_UNUSED_RETURN BBQJIT::getLocal(uint32_t localIndex, Value& result)
{
    // Currently, we load locals as temps, which basically prevents register allocation of locals.
    // This is probably not ideal, we have infrastructure to support binding locals to registers, but
    // we currently can't track local versioning (meaning we can get SSA-style issues where assigning
    // to a local also updates all previous uses).
    result = topValue(m_parser->typeOfLocal(localIndex).kind);
    Location resultLocation = allocate(result);
    emitLoad(Value::fromLocal(m_parser->typeOfLocal(localIndex).kind, localIndex), resultLocation);
    LOG_INSTRUCTION("GetLocal", localIndex, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::setLocal(uint32_t localIndex, Value value)
{
    if (!value.isConst())
        loadIfNecessary(value);
    Value local = Value::fromLocal(m_parser->typeOfLocal(localIndex).kind, localIndex);
    Location localLocation = locationOf(local);
    emitStore(value, localLocation);
    consume(value);
    LOG_INSTRUCTION("SetLocal", localIndex, value);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::teeLocal(uint32_t localIndex, Value value, Value& result)
{
    auto type = m_parser->typeOfLocal(localIndex);
    Value local = Value::fromLocal(type.kind, localIndex);
    if (value.isConst()) {
        Location localLocation = locationOf(local);
        emitStore(value, localLocation);
        consume(value);
        result = topValue(type.kind);
        Location resultLocation = allocate(result);
        emitMoveConst(value, resultLocation);
    } else {
        Location srcLocation = loadIfNecessary(value);
        Location localLocation = locationOf(local);
        emitStore(value, localLocation);
        consume(value);
        result = topValue(type.kind);
        Location resultLocation = allocate(result);
        emitMove(type.kind, srcLocation, resultLocation);
    }
    LOG_INSTRUCTION("TeeLocal", localIndex, value, RESULT(result));
    return { };
}

// Globals

Value BBQJIT::topValue(TypeKind type)
{
    return Value::fromTemp(type, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + m_parser->expressionStack().size());
}

Value BBQJIT::exception(const ControlData& control)
{
    ASSERT(ControlData::isAnyCatch(control));
    return Value::fromTemp(TypeKind::Externref, control.enclosedHeight());
}

void BBQJIT::emitWriteBarrier(GPRReg cellGPR)
{
    GPRReg vmGPR;
    GPRReg cellStateGPR;
    {
        ScratchScope<2, 0> scratches(*this);
        vmGPR = scratches.gpr(0);
        cellStateGPR = scratches.gpr(1);
    }

    // We must flush everything first. Jumping over flush (emitCCall) is wrong since paths need to get merged.
    flushRegisters();

    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), vmGPR);
    m_jit.load8(Address(cellGPR, JSCell::cellStateOffset()), cellStateGPR);
    auto noFenceCheck = m_jit.branch32(RelationalCondition::Above, cellStateGPR, Address(vmGPR, VM::offsetOfHeapBarrierThreshold()));

    // Fence check path
    auto toSlowPath = m_jit.branchTest8(ResultCondition::Zero, Address(vmGPR, VM::offsetOfHeapMutatorShouldBeFenced()));

    // Fence path
    m_jit.memoryFence();
    Jump belowBlackThreshold = m_jit.branch8(RelationalCondition::Above, Address(cellGPR, JSCell::cellStateOffset()), TrustedImm32(blackThreshold));

    // Slow path
    toSlowPath.link(&m_jit);
    m_jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
    m_jit.setupArguments<decltype(operationWasmWriteBarrierSlowPath)>(cellGPR, vmGPR);
    m_jit.callOperation<OperationPtrTag>(operationWasmWriteBarrierSlowPath);

    // Continuation
    noFenceCheck.link(&m_jit);
    belowBlackThreshold.link(&m_jit);
}

// Memory

Address BBQJIT::materializePointer(Location pointerLocation, uint32_t uoffset)
{
    if (static_cast<uint64_t>(uoffset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, uoffset, Width::Width128)) {
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), pointerLocation.asGPR());
        return Address(pointerLocation.asGPR());
    }
    return Address(pointerLocation.asGPR(), static_cast<int32_t>(uoffset));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addGrowMemory(Value delta, Value& result)
{
    Vector<Value, 8> arguments = { instanceValue(), delta };
    result = topValue(TypeKind::I32);
    emitCCall(&operationGrowMemory, arguments, result);
    restoreWebAssemblyGlobalState();

    LOG_INSTRUCTION("GrowMemory", delta, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCurrentMemory(Value& result)
{
    result = topValue(TypeKind::I32);
    Location resultLocation = allocate(result);
    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfJSMemory()), wasmScratchGPR);
    m_jit.loadPtr(Address(wasmScratchGPR, JSWebAssemblyMemory::offsetOfMemory()), wasmScratchGPR);
    m_jit.loadPtr(Address(wasmScratchGPR, Memory::offsetOfHandle()), wasmScratchGPR);
    m_jit.loadPtr(Address(wasmScratchGPR, BufferMemoryHandle::offsetOfSize()), wasmScratchGPR);

    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    m_jit.urshiftPtr(Imm32(shiftValue), wasmScratchGPR);
    m_jit.zeroExtend32ToWord(wasmScratchGPR, resultLocation.asGPR());

    LOG_INSTRUCTION("CurrentMemory", RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addMemoryFill(Value dstAddress, Value targetValue, Value count)
{
    ASSERT(dstAddress.type() == TypeKind::I32);
    ASSERT(targetValue.type() == TypeKind::I32);
    ASSERT(count.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        dstAddress, targetValue, count
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmMemoryFill, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    LOG_INSTRUCTION("MemoryFill", dstAddress, targetValue, count);

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addMemoryCopy(Value dstAddress, Value srcAddress, Value count)
{
    ASSERT(dstAddress.type() == TypeKind::I32);
    ASSERT(srcAddress.type() == TypeKind::I32);
    ASSERT(count.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        dstAddress, srcAddress, count
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmMemoryCopy, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    LOG_INSTRUCTION("MemoryCopy", dstAddress, srcAddress, count);

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addMemoryInit(unsigned dataSegmentIndex, Value dstAddress, Value srcAddress, Value length)
{
    ASSERT(dstAddress.type() == TypeKind::I32);
    ASSERT(srcAddress.type() == TypeKind::I32);
    ASSERT(length.type() == TypeKind::I32);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(dataSegmentIndex),
        dstAddress, srcAddress, length
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmMemoryInit, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    LOG_INSTRUCTION("MemoryInit", dataSegmentIndex, dstAddress, srcAddress, length);

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addDataDrop(unsigned dataSegmentIndex)
{
    Vector<Value, 8> arguments = { instanceValue(), Value::fromI32(dataSegmentIndex) };
    emitCCall(&operationWasmDataDrop, arguments);

    LOG_INSTRUCTION("DataDrop", dataSegmentIndex);
    return { };
}

// Atomics

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicLoad(ExtAtomicOpType loadOp, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t uoffset)
{
    if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(loadOp)))) {
        // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        result = valueType.isI64() ? Value::fromI64(0) : Value::fromI32(0);
    } else
        result = emitAtomicLoadOp(loadOp, valueType, emitCheckAndPreparePointer(pointer, uoffset, sizeOfAtomicOpMemoryAccess(loadOp)), uoffset);

    LOG_INSTRUCTION(makeString(loadOp), pointer, uoffset, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicStore(ExtAtomicOpType storeOp, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(storeOp)))) {
        // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        consume(value);
    } else
        emitAtomicStoreOp(storeOp, valueType, emitCheckAndPreparePointer(pointer, uoffset, sizeOfAtomicOpMemoryAccess(storeOp)), value, uoffset);

    LOG_INSTRUCTION(makeString(storeOp), pointer, uoffset, value, valueLocation);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        consume(value);
        result = valueType.isI64() ? Value::fromI64(0) : Value::fromI32(0);
    } else
        result = emitAtomicBinaryRMWOp(op, valueType, emitCheckAndPreparePointer(pointer, uoffset, sizeOfAtomicOpMemoryAccess(op)), value, uoffset);

    LOG_INSTRUCTION(makeString(op), pointer, uoffset, value, valueLocation, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        consume(expected);
        consume(value);
        result = valueType.isI64() ? Value::fromI64(0) : Value::fromI32(0);
    } else
        result = emitAtomicCompareExchange(op, valueType, emitCheckAndPreparePointer(pointer, uoffset, sizeOfAtomicOpMemoryAccess(op)), expected, value, uoffset);

    LOG_INSTRUCTION(makeString(op), pointer, expected, value, valueLocation, uoffset, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t uoffset)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        pointer,
        Value::fromI32(uoffset),
        value,
        timeout
    };

    result = topValue(TypeKind::I32);
    if (op == ExtAtomicOpType::MemoryAtomicWait32)
        emitCCall(&operationMemoryAtomicWait32, arguments, result);
    else
        emitCCall(&operationMemoryAtomicWait64, arguments, result);
    Location resultLocation = allocate(result);

    LOG_INSTRUCTION(makeString(op), pointer, value, timeout, uoffset, RESULT(result));

    throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPR(), TrustedImm32(0)));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicNotify(ExtAtomicOpType op, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t uoffset)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        pointer,
        Value::fromI32(uoffset),
        count
    };
    result = topValue(TypeKind::I32);
    emitCCall(&operationMemoryAtomicNotify, arguments, result);
    Location resultLocation = allocate(result);

    LOG_INSTRUCTION(makeString(op), pointer, count, uoffset, RESULT(result));

    throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPR(), TrustedImm32(0)));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::atomicFence(ExtAtomicOpType, uint8_t)
{
    m_jit.memoryFence();
    return { };
}

// Saturated truncation.

TruncationKind BBQJIT::truncationKind(OpType truncationOp)
{
    switch (truncationOp) {
    case OpType::I32TruncSF32:
        return TruncationKind::I32TruncF32S;
    case OpType::I32TruncUF32:
        return TruncationKind::I32TruncF32U;
    case OpType::I64TruncSF32:
        return TruncationKind::I64TruncF32S;
    case OpType::I64TruncUF32:
        return TruncationKind::I64TruncF32U;
    case OpType::I32TruncSF64:
        return TruncationKind::I32TruncF64S;
    case OpType::I32TruncUF64:
        return TruncationKind::I32TruncF64U;
    case OpType::I64TruncSF64:
        return TruncationKind::I64TruncF64S;
    case OpType::I64TruncUF64:
        return TruncationKind::I64TruncF64U;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Not a truncation op");
    }
}

TruncationKind BBQJIT::truncationKind(Ext1OpType truncationOp)
{
    switch (truncationOp) {
    case Ext1OpType::I32TruncSatF32S:
        return TruncationKind::I32TruncF32S;
    case Ext1OpType::I32TruncSatF32U:
        return TruncationKind::I32TruncF32U;
    case Ext1OpType::I64TruncSatF32S:
        return TruncationKind::I64TruncF32S;
    case Ext1OpType::I64TruncSatF32U:
        return TruncationKind::I64TruncF32U;
    case Ext1OpType::I32TruncSatF64S:
        return TruncationKind::I32TruncF64S;
    case Ext1OpType::I32TruncSatF64U:
        return TruncationKind::I32TruncF64U;
    case Ext1OpType::I64TruncSatF64S:
        return TruncationKind::I64TruncF64S;
    case Ext1OpType::I64TruncSatF64U:
        return TruncationKind::I64TruncF64U;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Not a truncation op");
    }
}

FloatingPointRange BBQJIT::lookupTruncationRange(TruncationKind truncationKind)
{
    Value min;
    Value max;
    bool closedLowerEndpoint = false;

    switch (truncationKind) {
    case TruncationKind::I32TruncF32S:
        closedLowerEndpoint = true;
        max = Value::fromF32(-static_cast<float>(std::numeric_limits<int32_t>::min()));
        min = Value::fromF32(static_cast<float>(std::numeric_limits<int32_t>::min()));
        break;
    case TruncationKind::I32TruncF32U:
        max = Value::fromF32(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0));
        min = Value::fromF32(static_cast<float>(-1.0));
        break;
    case TruncationKind::I32TruncF64S:
        max = Value::fromF64(-static_cast<double>(std::numeric_limits<int32_t>::min()));
        min = Value::fromF64(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0);
        break;
    case TruncationKind::I32TruncF64U:
        max = Value::fromF64(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0);
        min = Value::fromF64(-1.0);
        break;
    case TruncationKind::I64TruncF32S:
        closedLowerEndpoint = true;
        max = Value::fromF32(-static_cast<float>(std::numeric_limits<int64_t>::min()));
        min = Value::fromF32(static_cast<float>(std::numeric_limits<int64_t>::min()));
        break;
    case TruncationKind::I64TruncF32U:
        max = Value::fromF32(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0));
        min = Value::fromF32(static_cast<float>(-1.0));
        break;
    case TruncationKind::I64TruncF64S:
        closedLowerEndpoint = true;
        max = Value::fromF64(-static_cast<double>(std::numeric_limits<int64_t>::min()));
        min = Value::fromF64(static_cast<double>(std::numeric_limits<int64_t>::min()));
        break;
    case TruncationKind::I64TruncF64U:
        max = Value::fromF64(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0);
        min = Value::fromF64(-1.0);
        break;
    }

    return FloatingPointRange { min, max, closedLowerEndpoint };
}

// GC

const Ref<TypeDefinition> BBQJIT::getTypeDefinition(uint32_t typeIndex) { return m_info.typeSignatures[typeIndex]; }

// Given a type index, verify that it's an array type and return its expansion
const ArrayType* BBQJIT::getArrayTypeDefinition(uint32_t typeIndex)
{
    Ref<Wasm::TypeDefinition> typeDef = getTypeDefinition(typeIndex);
    const Wasm::TypeDefinition& arraySignature = typeDef->expand();
    return arraySignature.as<ArrayType>();
}

// Given a type index for an array signature, look it up, expand it and
// return the element type
StorageType BBQJIT::getArrayElementType(uint32_t typeIndex)
{
    const ArrayType* arrayType = getArrayTypeDefinition(typeIndex);
    return arrayType->elementType().type;
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(typeIndex),
        size,
    };
    result = topValue(TypeKind::Arrayref);
    emitCCall(&operationWasmArrayNewEmpty, arguments, result);

    Location resultLocation = loadIfNecessary(result);
    emitThrowOnNullReference(ExceptionType::BadArrayNew, resultLocation);

    LOG_INSTRUCTION("ArrayNewDefault", typeIndex, size, RESULT(result));
    return { };
}

void BBQJIT::pushArrayNewFromSegment(ArraySegmentOperation operation, uint32_t typeIndex, uint32_t segmentIndex, ExpressionType arraySize, ExpressionType offset, ExceptionType exceptionType, ExpressionType& result)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(typeIndex),
        Value::fromI32(segmentIndex),
        arraySize,
        offset,
    };
    result = topValue(TypeKind::I64);
    emitCCall(operation, arguments, result);
    Location resultLocation = allocate(result);

    emitThrowOnNullReference(exceptionType, resultLocation);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNewData(uint32_t typeIndex, uint32_t dataIndex, ExpressionType arraySize, ExpressionType offset, ExpressionType& result)
{
    pushArrayNewFromSegment(operationWasmArrayNewData, typeIndex, dataIndex, arraySize, offset, ExceptionType::BadArrayNewInitData, result);
    LOG_INSTRUCTION("ArrayNewData", typeIndex, dataIndex, arraySize, offset, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNewElem(uint32_t typeIndex, uint32_t elemSegmentIndex, ExpressionType arraySize, ExpressionType offset, ExpressionType& result)
{
    pushArrayNewFromSegment(operationWasmArrayNewElem, typeIndex, elemSegmentIndex, arraySize, offset, ExceptionType::BadArrayNewInitElem, result);
    LOG_INSTRUCTION("ArrayNewElem", typeIndex, elemSegmentIndex, arraySize, offset, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayCopy(uint32_t dstTypeIndex, ExpressionType dst, ExpressionType dstOffset, uint32_t srcTypeIndex, ExpressionType src, ExpressionType srcOffset, ExpressionType size)
{
    if (dst.isConst() || src.isConst()) {
        ASSERT_IMPLIES(dst.isConst(), dst.asI64() == JSValue::encode(jsNull()));
        ASSERT_IMPLIES(src.isConst(), src.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArrayCopy);
        return { };
    }

    emitThrowOnNullReference(ExceptionType::NullArrayCopy, loadIfNecessary(dst));
    emitThrowOnNullReference(ExceptionType::NullArrayCopy, loadIfNecessary(src));

    Vector<Value, 8> arguments = {
        instanceValue(),
        dst,
        dstOffset,
        src,
        srcOffset,
        size
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmArrayCopy, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("ArrayCopy", dstTypeIndex, dst, dstOffset, srcTypeIndex, src, srcOffset, size);

    throwExceptionIf(ExceptionType::OutOfBoundsArrayCopy, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayInitElem(uint32_t dstTypeIndex, ExpressionType dst, ExpressionType dstOffset, uint32_t srcElementIndex, ExpressionType srcOffset, ExpressionType size)
{
    if (dst.isConst()) {
        ASSERT(dst.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArrayInitElem);
        return { };
    }

    emitThrowOnNullReference(ExceptionType::NullArrayInitElem, loadIfNecessary(dst));

    Vector<Value, 8> arguments = {
        instanceValue(),
        dst,
        dstOffset,
        Value::fromI32(srcElementIndex),
        srcOffset,
        size
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmArrayInitElem, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("ArrayInitElem", dstTypeIndex, dst, dstOffset, srcElementIndex, srcOffset, size);

    throwExceptionIf(ExceptionType::OutOfBoundsArrayInitElem, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayInitData(uint32_t dstTypeIndex, ExpressionType dst, ExpressionType dstOffset, uint32_t srcDataIndex, ExpressionType srcOffset, ExpressionType size)
{
    if (dst.isConst()) {
        ASSERT(dst.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArrayInitData);
        return { };
    }

    emitThrowOnNullReference(ExceptionType::NullArrayInitData, loadIfNecessary(dst));

    Vector<Value, 8> arguments = {
        instanceValue(),
        dst,
        dstOffset,
        Value::fromI32(srcDataIndex),
        srcOffset,
        size
    };
    Value shouldThrow = topValue(TypeKind::I32);
    emitCCall(&operationWasmArrayInitData, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("ArrayInitData", dstTypeIndex, dst, dstOffset, srcDataIndex, srcOffset, size);

    throwExceptionIf(ExceptionType::OutOfBoundsArrayInitData, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

void BBQJIT::emitStructSet(GPRReg structGPR, const StructType& structType, uint32_t fieldIndex, Value value)
{
    m_jit.loadPtr(MacroAssembler::Address(structGPR, JSWebAssemblyStruct::offsetOfPayload()), wasmScratchGPR);
    emitStructPayloadSet(wasmScratchGPR, structType, fieldIndex, value);
    if (isRefType(structType.field(fieldIndex).type))
        emitWriteBarrier(structGPR);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefTest(ExpressionType reference, bool allowNull, int32_t heapType, bool shouldNegate, ExpressionType& result)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        reference,
        Value::fromI32(allowNull),
        Value::fromI32(heapType),
        Value::fromI32(shouldNegate),
    };
    result = topValue(TypeKind::I32);
    emitCCall(operationWasmRefTest, arguments, result);

    LOG_INSTRUCTION("RefTest", reference, allowNull, heapType, shouldNegate, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addAnyConvertExtern(ExpressionType reference, ExpressionType& result)
{
    Vector<Value, 8> arguments = {
        reference
    };
    result = topValue(TypeKind::Anyref);
    emitCCall(&operationWasmAnyConvertExtern, arguments, result);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addExternConvertAny(ExpressionType reference, ExpressionType& result)
{
    result = reference;
    return { };
}

// Basic operators
PartialResult WARN_UNUSED_RETURN BBQJIT::addSelect(Value condition, Value lhs, Value rhs, Value& result)
{
    if (condition.isConst()) {
        Value src = condition.asI32() ? lhs : rhs;
        Location srcLocation;
        if (src.isConst())
            result = src;
        else {
            result = topValue(lhs.type());
            srcLocation = loadIfNecessary(src);
        }

        LOG_INSTRUCTION("Select", condition, lhs, rhs, RESULT(result));
        consume(condition);
        consume(lhs);
        consume(rhs);
        if (!result.isConst()) {
            Location resultLocation = allocate(result);
            emitMove(lhs.type(), srcLocation, resultLocation);
        }
    } else {
        Location conditionLocation = loadIfNecessary(condition);
        Location lhsLocation = Location::none(), rhsLocation = Location::none();

        // Ensure all non-constant parameters are loaded into registers.
        if (!lhs.isConst())
            lhsLocation = loadIfNecessary(lhs);
        if (!rhs.isConst())
            rhsLocation = loadIfNecessary(rhs);

        ASSERT(lhs.isConst() || lhsLocation.isRegister());
        ASSERT(rhs.isConst() || rhsLocation.isRegister());
        consume(lhs);
        consume(rhs);

        result = topValue(lhs.type());
        Location resultLocation = allocate(result);
        LOG_INSTRUCTION("Select", condition, lhs, lhsLocation, rhs, rhsLocation, RESULT(result));
        LOG_INDENT();

        bool inverted = false;

        // If the operands or the result alias, we want the matching one to be on top.
        if (rhsLocation == resultLocation) {
            std::swap(lhs, rhs);
            std::swap(lhsLocation, rhsLocation);
            inverted = true;
        }

        // If the condition location and the result alias, we want to make sure the condition is
        // preserved no matter what.
        if (conditionLocation == resultLocation) {
            m_jit.move(conditionLocation.asGPR(), wasmScratchGPR);
            conditionLocation = Location::fromGPR(wasmScratchGPR);
        }

        // Kind of gross isel, but it should handle all use/def aliasing cases correctly.
        if (lhs.isConst())
            emitMoveConst(lhs, resultLocation);
        else
            emitMove(lhs.type(), lhsLocation, resultLocation);
        Jump ifZero = m_jit.branchTest32(inverted ? ResultCondition::Zero : ResultCondition::NonZero, conditionLocation.asGPR(), conditionLocation.asGPR());
        consume(condition);
        if (rhs.isConst())
            emitMoveConst(rhs, resultLocation);
        else
            emitMove(rhs.type(), rhsLocation, resultLocation);
        ifZero.link(&m_jit);

        LOG_DEDENT();
    }
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Add(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I32Add", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() + rhs.asI32())),
        BLOCK(
            m_jit.add32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.add32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Add(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Add", TypeKind::F32,
        BLOCK(Value::fromF32(lhs.asF32() + rhs.asF32())),
        BLOCK(
            m_jit.addFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.addFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Add(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Add", TypeKind::F64,
        BLOCK(Value::fromF64(lhs.asF64() + rhs.asF64())),
        BLOCK(
            m_jit.addDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.addDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Sub(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I32Sub", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() - rhs.asI32())),
        BLOCK(
            m_jit.sub32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.sub32(lhsLocation.asGPR(), TrustedImm32(rhs.asI32()), resultLocation.asGPR());
            else {
                emitMoveConst(lhs, Location::fromGPR(wasmScratchGPR));
                m_jit.sub32(wasmScratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Sub(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Sub", TypeKind::F32,
        BLOCK(Value::fromF32(lhs.asF32() - rhs.asF32())),
        BLOCK(
            m_jit.subFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            if (rhs.isConst()) {
                // If rhs is a constant, it will be expressed as a positive
                // value and so needs to be negated unless it is NaN
                auto floatVal = std::isnan(rhs.asF32()) ? rhs.asF32() : -rhs.asF32();
                emitMoveConst(Value::fromF32(floatVal), Location::fromFPR(wasmScratchFPR));
                m_jit.addFloat(lhsLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
            } else {
                emitMoveConst(lhs, Location::fromFPR(wasmScratchFPR));
                m_jit.subFloat(wasmScratchFPR, rhsLocation.asFPR(), resultLocation.asFPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Sub(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Sub", TypeKind::F64,
        BLOCK(Value::fromF64(lhs.asF64() - rhs.asF64())),
        BLOCK(
            m_jit.subDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            if (rhs.isConst()) {
                // If rhs is a constant, it will be expressed as a positive
                // value and so needs to be negated unless it is NaN
                auto floatVal = std::isnan(rhs.asF64()) ? rhs.asF64() : -rhs.asF64();
                emitMoveConst(Value::fromF64(floatVal), Location::fromFPR(wasmScratchFPR));
                m_jit.addDouble(lhsLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
            } else {
                emitMoveConst(lhs, Location::fromFPR(wasmScratchFPR));
                m_jit.subDouble(wasmScratchFPR, rhsLocation.asFPR(), resultLocation.asFPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Mul(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I32Mul", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() * rhs.asI32())),
        BLOCK(
            m_jit.mul32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.mul32(
                Imm32(ImmHelpers::imm(lhs, rhs).asI32()),
                ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(),
                resultLocation.asGPR()
            );
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Mul(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Mul", TypeKind::F32,
        BLOCK(Value::fromF32(lhs.asF32() * rhs.asF32())),
        BLOCK(
            m_jit.mulFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.mulFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Mul(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Mul", TypeKind::F64,
        BLOCK(Value::fromF64(lhs.asF64() * rhs.asF64())),
        BLOCK(
            m_jit.mulDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.mulDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

template<typename Func>
void BBQJIT::addLatePath(Func func)
{
    m_latePaths.append(createSharedTask<void(BBQJIT&, CCallHelpers&)>(WTFMove(func)));
}

void BBQJIT::emitThrowException(ExceptionType type)
{
    m_jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    m_jit.jumpThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
}

void BBQJIT::throwExceptionIf(ExceptionType type, Jump jump)
{
    m_exceptions[static_cast<unsigned>(type)].append(jump);
}

template<typename IntType>
Value BBQJIT::checkConstantDivision(const Value& lhs, const Value& rhs)
{
    constexpr bool is32 = sizeof(IntType) == 4;
    if (!(is32 ? int64_t(rhs.asI32()) : rhs.asI64())) {
        emitThrowException(ExceptionType::DivisionByZero);
        return is32 ? Value::fromI32(1) : Value::fromI64(1);
    }
    if ((is32 ? int64_t(rhs.asI32()) : rhs.asI64()) == -1
        && (is32 ? int64_t(lhs.asI32()) : lhs.asI64()) == std::numeric_limits<IntType>::min()
        && std::is_signed<IntType>()) {
        emitThrowException(ExceptionType::IntegerOverflow);
        return is32 ? Value::fromI32(1) : Value::fromI64(1);
    }
    return rhs;
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32DivS(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I32DivS", TypeKind::I32,
        BLOCK(
            Value::fromI32(lhs.asI32() / checkConstantDivision<int32_t>(lhs, rhs).asI32())
        ),
        BLOCK(
            emitModOrDiv<int32_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<int32_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64DivS(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I64DivS", TypeKind::I64,
        BLOCK(
            Value::fromI64(lhs.asI64() / checkConstantDivision<int64_t>(lhs, rhs).asI64())
        ),
        BLOCK(
            emitModOrDiv<int64_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<int64_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32DivU(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I32DivU", TypeKind::I32,
        BLOCK(
            Value::fromI32(static_cast<uint32_t>(lhs.asI32()) / static_cast<uint32_t>(checkConstantDivision<int32_t>(lhs, rhs).asI32()))
        ),
        BLOCK(
            emitModOrDiv<uint32_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<uint32_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64DivU(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I64DivU", TypeKind::I64,
        BLOCK(
            Value::fromI64(static_cast<uint64_t>(lhs.asI64()) / static_cast<uint64_t>(checkConstantDivision<int64_t>(lhs, rhs).asI64()))
        ),
        BLOCK(
            emitModOrDiv<uint64_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<uint64_t, false>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32RemS(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I32RemS", TypeKind::I32,
        BLOCK(
            Value::fromI32(lhs.asI32() % checkConstantDivision<int32_t>(lhs, rhs).asI32())
        ),
        BLOCK(
            emitModOrDiv<int32_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<int32_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64RemS(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I64RemS", TypeKind::I64,
        BLOCK(
            Value::fromI64(lhs.asI64() % checkConstantDivision<int64_t>(lhs, rhs).asI64())
        ),
        BLOCK(
            emitModOrDiv<int64_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<int64_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32RemU(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I32RemU", TypeKind::I32,
        BLOCK(
            Value::fromI32(static_cast<uint32_t>(lhs.asI32()) % static_cast<uint32_t>(checkConstantDivision<int32_t>(lhs, rhs).asI32()))
        ),
        BLOCK(
            emitModOrDiv<uint32_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<uint32_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64RemU(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_MOD_OR_DIV;
    EMIT_BINARY(
        "I64RemU", TypeKind::I64,
        BLOCK(
            Value::fromI64(static_cast<uint64_t>(lhs.asI64()) % static_cast<uint64_t>(checkConstantDivision<int64_t>(lhs, rhs).asI64()))
        ),
        BLOCK(
            emitModOrDiv<uint64_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        ),
        BLOCK(
            emitModOrDiv<uint64_t, true>(lhs, lhsLocation, rhs, rhsLocation, result, resultLocation);
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Div(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Div", TypeKind::F32,
        BLOCK(Value::fromF32(lhs.asF32() / rhs.asF32())),
        BLOCK(
            m_jit.divFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.divFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Div(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Div", TypeKind::F64,
        BLOCK(Value::fromF64(lhs.asF64() / rhs.asF64())),
        BLOCK(
            m_jit.divDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.divDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

template<MinOrMax IsMinOrMax, typename FloatType>
void BBQJIT::emitFloatingPointMinOrMax(FPRReg left, FPRReg right, FPRReg result)
{
    constexpr bool is32 = sizeof(FloatType) == 4;

#if CPU(ARM64)
        if constexpr (is32 && IsMinOrMax == MinOrMax::Min)
            m_jit.floatMin(left, right, result);
        else if constexpr (is32)
            m_jit.floatMax(left, right, result);
        else if constexpr (IsMinOrMax == MinOrMax::Min)
            m_jit.doubleMin(left, right, result);
        else
            m_jit.doubleMax(left, right, result);
        return;
#else
    // Entry
    Jump isEqual = is32
        ? m_jit.branchFloat(MacroAssembler::DoubleEqualAndOrdered, left, right)
        : m_jit.branchDouble(MacroAssembler::DoubleEqualAndOrdered, left, right);

    // Left is not equal to right
    Jump isLessThan = is32
        ? m_jit.branchFloat(MacroAssembler::DoubleLessThanAndOrdered, left, right)
        : m_jit.branchDouble(MacroAssembler::DoubleLessThanAndOrdered, left, right);

    // Left is not less than right
    Jump isGreaterThan = is32
        ? m_jit.branchFloat(MacroAssembler::DoubleGreaterThanAndOrdered, left, right)
        : m_jit.branchDouble(MacroAssembler::DoubleGreaterThanAndOrdered, left, right);

    // NaN
    if constexpr (is32)
        m_jit.addFloat(left, right, result);
    else
        m_jit.addDouble(left, right, result);
    Jump afterNaN = m_jit.jump();

    // Left is strictly greater than right
    isGreaterThan.link(&m_jit);
    auto isGreaterThanResult = IsMinOrMax == MinOrMax::Max ? left : right;
    m_jit.moveDouble(isGreaterThanResult, result);
    Jump afterGreaterThan = m_jit.jump();

    // Left is strictly less than right
    isLessThan.link(&m_jit);
    auto isLessThanResult = IsMinOrMax == MinOrMax::Max ? right : left;
    m_jit.moveDouble(isLessThanResult, result);
    Jump afterLessThan = m_jit.jump();

    // Left is equal to right
    isEqual.link(&m_jit);
    if constexpr (is32 && IsMinOrMax == MinOrMax::Max)
        m_jit.andFloat(left, right, result);
    else if constexpr (is32)
        m_jit.orFloat(left, right, result);
    else if constexpr (IsMinOrMax == MinOrMax::Max)
        m_jit.andDouble(left, right, result);
    else
        m_jit.orDouble(left, right, result);

    // Continuation
    afterNaN.link(&m_jit);
    afterGreaterThan.link(&m_jit);
    afterLessThan.link(&m_jit);
#endif
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Min(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Min", TypeKind::F32,
        BLOCK(Value::fromF32(computeFloatingPointMinOrMax<MinOrMax::Min>(lhs.asF32(), rhs.asF32()))),
        BLOCK(
            emitFloatingPointMinOrMax<MinOrMax::Min, float>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            emitFloatingPointMinOrMax<MinOrMax::Min, float>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Min(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Min", TypeKind::F64,
        BLOCK(Value::fromF64(computeFloatingPointMinOrMax<MinOrMax::Min>(lhs.asF64(), rhs.asF64()))),
        BLOCK(
            emitFloatingPointMinOrMax<MinOrMax::Min, double>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            emitFloatingPointMinOrMax<MinOrMax::Min, double>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Max(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Max", TypeKind::F32,
        BLOCK(Value::fromF32(computeFloatingPointMinOrMax<MinOrMax::Max>(lhs.asF32(), rhs.asF32()))),
        BLOCK(
            emitFloatingPointMinOrMax<MinOrMax::Max, float>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            emitFloatingPointMinOrMax<MinOrMax::Max, float>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Max(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Max", TypeKind::F64,
        BLOCK(Value::fromF64(computeFloatingPointMinOrMax<MinOrMax::Max>(lhs.asF64(), rhs.asF64()))),
        BLOCK(
            emitFloatingPointMinOrMax<MinOrMax::Max, double>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            emitFloatingPointMinOrMax<MinOrMax::Max, double>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32And(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I32And", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() & rhs.asI32())),
        BLOCK(
            m_jit.and32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.and32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Xor(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I32Xor", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() ^ rhs.asI32())),
        BLOCK(
            m_jit.xor32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.xor32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Or(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I32Or", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() | rhs.asI32())),
        BLOCK(
            m_jit.or32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.or32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

void BBQJIT::moveShiftAmountIfNecessary(Location& rhsLocation)
{
    if constexpr (isX86()) {
        m_jit.move(rhsLocation.asGPR(), shiftRCX);
        rhsLocation = Location::fromGPR(shiftRCX);
    }
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Shl(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I32Shl", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() << rhs.asI32())),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.lshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.lshift32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(rhs.asI32())), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
                m_jit.lshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32ShrS(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I32ShrS", TypeKind::I32,
        BLOCK(Value::fromI32(lhs.asI32() >> rhs.asI32())),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.rshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rshift32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(rhs.asI32())), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
                m_jit.rshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32ShrU(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I32ShrU", TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<uint32_t>(lhs.asI32()) >> static_cast<uint32_t>(rhs.asI32()))),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.urshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.urshift32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(rhs.asI32())), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
                m_jit.urshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Rotl(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I32Rotl", TypeKind::I32,
        BLOCK(Value::fromI32(B3::rotateLeft(lhs.asI32(), rhs.asI32()))),
#if CPU(X86_64)
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.rotateLeft32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rotateLeft32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(rhs.asI32())), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, resultLocation);
                m_jit.rotateLeft32(resultLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
#else
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.neg32(rhsLocation.asGPR(), wasmScratchGPR);
            m_jit.rotateRight32(lhsLocation.asGPR(), wasmScratchGPR, resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rotateRight32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(-rhs.asI32())), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                m_jit.neg32(rhsLocation.asGPR(), wasmScratchGPR);
                emitMoveConst(lhs, resultLocation);
                m_jit.rotateRight32(resultLocation.asGPR(), wasmScratchGPR, resultLocation.asGPR());
            }
        )
#endif
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Rotr(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I32Rotr", TypeKind::I32,
        BLOCK(Value::fromI32(B3::rotateRight(lhs.asI32(), rhs.asI32()))),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.rotateRight32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rotateRight32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(rhs.asI32())), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, Location::fromGPR(wasmScratchGPR));
                m_jit.rotateRight32(wasmScratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Clz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Clz", TypeKind::I32,
        BLOCK(Value::fromI32(WTF::clz(operand.asI32()))),
        BLOCK(
            m_jit.countLeadingZeros32(operandLocation.asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Ctz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Ctz", TypeKind::I32,
        BLOCK(Value::fromI32(WTF::ctz(operand.asI32()))),
        BLOCK(
            m_jit.countTrailingZeros32(operandLocation.asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult BBQJIT::emitCompareI32(const char* opcode, Value& lhs, Value& rhs, Value& result, RelationalCondition condition, bool (*comparator)(int32_t lhs, int32_t rhs))
{
    EMIT_BINARY(
        opcode, TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asI32(), rhs.asI32())))),
        BLOCK(
            m_jit.compare32(condition, lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.compare32(condition, lhsLocation.asGPR(), Imm32(rhs.asI32()), resultLocation.asGPR());
            else
                m_jit.compare32(condition, Imm32(lhs.asI32()), rhsLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

#define RELOP_AS_LAMBDA(op) [](auto lhs, auto rhs) -> auto { return lhs op rhs; }
#define TYPED_RELOP_AS_LAMBDA(type, op) [](auto lhs, auto rhs) -> auto { return static_cast<type>(lhs) op static_cast<type>(rhs); }

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32Eq", lhs, rhs, result, RelationalCondition::Equal, RELOP_AS_LAMBDA( == ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64Eq", lhs, rhs, result, RelationalCondition::Equal, RELOP_AS_LAMBDA( == ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32Ne", lhs, rhs, result, RelationalCondition::NotEqual, RELOP_AS_LAMBDA( != ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64Ne", lhs, rhs, result, RelationalCondition::NotEqual, RELOP_AS_LAMBDA( != ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32LtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LtS", lhs, rhs, result, RelationalCondition::LessThan, RELOP_AS_LAMBDA( < ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64LtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LtS", lhs, rhs, result, RelationalCondition::LessThan, RELOP_AS_LAMBDA( < ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32LeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LeS", lhs, rhs, result, RelationalCondition::LessThanOrEqual, RELOP_AS_LAMBDA( <= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64LeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LeS", lhs, rhs, result, RelationalCondition::LessThanOrEqual, RELOP_AS_LAMBDA( <= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32GtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GtS", lhs, rhs, result, RelationalCondition::GreaterThan, RELOP_AS_LAMBDA( > ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64GtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GtS", lhs, rhs, result, RelationalCondition::GreaterThan, RELOP_AS_LAMBDA( > ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32GeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GeS", lhs, rhs, result, RelationalCondition::GreaterThanOrEqual, RELOP_AS_LAMBDA( >= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64GeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GeS", lhs, rhs, result, RelationalCondition::GreaterThanOrEqual, RELOP_AS_LAMBDA( >= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32LtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LtU", lhs, rhs, result, RelationalCondition::Below, TYPED_RELOP_AS_LAMBDA(uint32_t, <));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64LtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LtU", lhs, rhs, result, RelationalCondition::Below, TYPED_RELOP_AS_LAMBDA(uint64_t, <));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32LeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LeU", lhs, rhs, result, RelationalCondition::BelowOrEqual, TYPED_RELOP_AS_LAMBDA(uint32_t, <=));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64LeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LeU", lhs, rhs, result, RelationalCondition::BelowOrEqual, TYPED_RELOP_AS_LAMBDA(uint64_t, <=));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32GtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GtU", lhs, rhs, result, RelationalCondition::Above, TYPED_RELOP_AS_LAMBDA(uint32_t, >));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64GtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GtU", lhs, rhs, result, RelationalCondition::Above, TYPED_RELOP_AS_LAMBDA(uint64_t, >));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32GeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GeU", lhs, rhs, result, RelationalCondition::AboveOrEqual, TYPED_RELOP_AS_LAMBDA(uint32_t, >=));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64GeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GeU", lhs, rhs, result, RelationalCondition::AboveOrEqual, TYPED_RELOP_AS_LAMBDA(uint64_t, >=));
}

PartialResult BBQJIT::emitCompareF32(const char* opcode, Value& lhs, Value& rhs, Value& result, DoubleCondition condition, bool (*comparator)(float lhs, float rhs))
{
    EMIT_BINARY(
        opcode, TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asF32(), rhs.asF32())))),
        BLOCK(
            m_jit.compareFloat(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.compareFloat(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
        )
    )
}

PartialResult BBQJIT::emitCompareF64(const char* opcode, Value& lhs, Value& rhs, Value& result, DoubleCondition condition, bool (*comparator)(double lhs, double rhs))
{
    EMIT_BINARY(
        opcode, TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asF64(), rhs.asF64())))),
        BLOCK(
            m_jit.compareDouble(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(wasmScratchFPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(wasmScratchFPR));
            m_jit.compareDouble(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Eq", lhs, rhs, result, DoubleCondition::DoubleEqualAndOrdered, RELOP_AS_LAMBDA( == ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Eq", lhs, rhs, result, DoubleCondition::DoubleEqualAndOrdered, RELOP_AS_LAMBDA( == ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Ne", lhs, rhs, result, DoubleCondition::DoubleNotEqualOrUnordered, RELOP_AS_LAMBDA( != ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Ne", lhs, rhs, result, DoubleCondition::DoubleNotEqualOrUnordered, RELOP_AS_LAMBDA( != ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Lt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Lt", lhs, rhs, result, DoubleCondition::DoubleLessThanAndOrdered, RELOP_AS_LAMBDA( < ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Lt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Lt", lhs, rhs, result, DoubleCondition::DoubleLessThanAndOrdered, RELOP_AS_LAMBDA( < ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Le(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Le", lhs, rhs, result, DoubleCondition::DoubleLessThanOrEqualAndOrdered, RELOP_AS_LAMBDA( <= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Le(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Le", lhs, rhs, result, DoubleCondition::DoubleLessThanOrEqualAndOrdered, RELOP_AS_LAMBDA( <= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Gt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Gt", lhs, rhs, result, DoubleCondition::DoubleGreaterThanAndOrdered, RELOP_AS_LAMBDA( > ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Gt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Gt", lhs, rhs, result, DoubleCondition::DoubleGreaterThanAndOrdered, RELOP_AS_LAMBDA( > ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Ge(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Ge", lhs, rhs, result, DoubleCondition::DoubleGreaterThanOrEqualAndOrdered, RELOP_AS_LAMBDA( >= ));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Ge(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Ge", lhs, rhs, result, DoubleCondition::DoubleGreaterThanOrEqualAndOrdered, RELOP_AS_LAMBDA( >= ));
}

#undef RELOP_AS_LAMBDA
#undef TYPED_RELOP_AS_LAMBDA

PartialResult BBQJIT::addI32Extend8S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Extend8S", TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(static_cast<int8_t>(operand.asI32())))),
        BLOCK(
            m_jit.signExtend8To32(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Extend16S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Extend16S", TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(static_cast<int16_t>(operand.asI32())))),
        BLOCK(
            m_jit.signExtend16To32(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Eqz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Eqz", TypeKind::I32,
        BLOCK(Value::fromI32(!operand.asI32())),
        BLOCK(
            m_jit.test32(ResultCondition::Zero, operandLocation.asGPR(), operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32Popcnt(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Popcnt", TypeKind::I32,
        BLOCK(Value::fromI32(std::popcount(static_cast<uint32_t>(operand.asI32())))),
        BLOCK(
            if (m_jit.supportsCountPopulation())
                m_jit.countPopulation32(operandLocation.asGPR(), resultLocation.asGPR(), wasmScratchFPR);
            else {
                // The EMIT_UNARY(...) macro will already assign result to the top value on the stack and give it a register,
                // so it should be able to be passed in. However, this creates a somewhat nasty tacit dependency - emitCCall
                // will bind result to the returnValueGPR, which will error if result is already bound to a different register
                // (to avoid mucking with the register allocator state). It shouldn't currently error specifically because we
                // only allocate caller-saved registers, which get flushed across the call regardless; however, if we add
                // callee-saved register allocation to BBQJIT in the future, we could get very niche errors.
                //
                // We avoid this by consuming the result before passing it to emitCCall, which also saves us the mov for spilling.
                consume(result);
                auto arg = Value::pinned(TypeKind::I32, operandLocation);
                emitCCall(&operationPopcount32, ArgumentList { arg }, result);
            }
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Popcnt(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Popcnt", TypeKind::I64,
        BLOCK(Value::fromI64(std::popcount(static_cast<uint64_t>(operand.asI64())))),
        BLOCK(
            if (m_jit.supportsCountPopulation())
                m_jit.countPopulation64(operandLocation.asGPR(), resultLocation.asGPR(), wasmScratchFPR);
            else {
                // The EMIT_UNARY(...) macro will already assign result to the top value on the stack and give it a register,
                // so it should be able to be passed in. However, this creates a somewhat nasty tacit dependency - emitCCall
                // will bind result to the returnValueGPR, which will error if result is already bound to a different register
                // (to avoid mucking with the register allocator state). It shouldn't currently error specifically because we
                // only allocate caller-saved registers, which get flushed across the call regardless; however, if we add
                // callee-saved register allocation to BBQJIT in the future, we could get very niche errors.
                //
                // We avoid this by consuming the result before passing it to emitCCall, which also saves us the mov for spilling.
                consume(result);
                auto arg = Value::pinned(TypeKind::I64, operandLocation);
                emitCCall(&operationPopcount64, ArgumentList { arg }, result);
            }
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32ReinterpretF32(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32ReinterpretF32", TypeKind::I32,
        BLOCK(Value::fromI32(bitwise_cast<int32_t>(operand.asF32()))),
        BLOCK(
            m_jit.moveFloatTo32(operandLocation.asFPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32ReinterpretI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ReinterpretI32", TypeKind::F32,
        BLOCK(Value::fromF32(bitwise_cast<float>(operand.asI32()))),
        BLOCK(
            m_jit.move32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32DemoteF64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32DemoteF64", TypeKind::F32,
        BLOCK(Value::fromF32(operand.asF64())),
        BLOCK(
            m_jit.convertDoubleToFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64PromoteF32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64PromoteF32", TypeKind::F64,
        BLOCK(Value::fromF64(operand.asF32())),
        BLOCK(
            m_jit.convertFloatToDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32ConvertSI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ConvertSI32", TypeKind::F32,
        BLOCK(Value::fromF32(operand.asI32())),
        BLOCK(
            m_jit.convertInt32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ConvertSI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ConvertSI32", TypeKind::F64,
        BLOCK(Value::fromF64(operand.asI32())),
        BLOCK(
            m_jit.convertInt32ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Copysign(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Copysign", TypeKind::F32,
        BLOCK(Value::fromF32(floatCopySign(lhs.asF32(), rhs.asF32()))),
        BLOCK(
            // FIXME: Better than what we have in the Air backend, but still not great. I think
            // there's some vector instruction we can use to do this much quicker.

#if CPU(X86_64)
            m_jit.moveFloatTo32(lhsLocation.asFPR(), wasmScratchGPR);
            m_jit.and32(TrustedImm32(0x7fffffff), wasmScratchGPR);
            m_jit.move32ToFloat(wasmScratchGPR, wasmScratchFPR);
            m_jit.moveFloatTo32(rhsLocation.asFPR(), wasmScratchGPR);
            m_jit.and32(TrustedImm32(static_cast<int32_t>(0x80000000u)), wasmScratchGPR, wasmScratchGPR);
            m_jit.move32ToFloat(wasmScratchGPR, resultLocation.asFPR());
            m_jit.orFloat(resultLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
#else
            m_jit.moveFloatTo32(rhsLocation.asFPR(), wasmScratchGPR);
            m_jit.and32(TrustedImm32(static_cast<int32_t>(0x80000000u)), wasmScratchGPR, wasmScratchGPR);
            m_jit.move32ToFloat(wasmScratchGPR, wasmScratchFPR);
            m_jit.absFloat(lhsLocation.asFPR(), lhsLocation.asFPR());
            m_jit.orFloat(lhsLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
#endif
        ),
        BLOCK(
            if (lhs.isConst()) {
                m_jit.moveFloatTo32(rhsLocation.asFPR(), wasmScratchGPR);
                m_jit.and32(TrustedImm32(static_cast<int32_t>(0x80000000u)), wasmScratchGPR, wasmScratchGPR);
                m_jit.move32ToFloat(wasmScratchGPR, wasmScratchFPR);

                emitMoveConst(Value::fromF32(std::abs(lhs.asF32())), resultLocation);
                m_jit.orFloat(resultLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
            } else {
                bool signBit = bitwise_cast<uint32_t>(rhs.asF32()) & 0x80000000u;
#if CPU(X86_64)
                m_jit.moveDouble(lhsLocation.asFPR(), resultLocation.asFPR());
                m_jit.move32ToFloat(TrustedImm32(0x7fffffff), wasmScratchFPR);
                m_jit.andFloat(wasmScratchFPR, resultLocation.asFPR());
                if (signBit) {
                    m_jit.xorFloat(wasmScratchFPR, wasmScratchFPR);
                    m_jit.subFloat(wasmScratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
                }
#else
                m_jit.absFloat(lhsLocation.asFPR(), resultLocation.asFPR());
                if (signBit)
                    m_jit.negateFloat(resultLocation.asFPR(), resultLocation.asFPR());
#endif
            }
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Abs(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Abs", TypeKind::F32,
        BLOCK(Value::fromF32(std::abs(operand.asF32()))),
        BLOCK(
#if CPU(X86_64)
            m_jit.move32ToFloat(TrustedImm32(0x7fffffffll), wasmScratchFPR);
            m_jit.andFloat(operandLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
#else
            m_jit.absFloat(operandLocation.asFPR(), resultLocation.asFPR());
#endif
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Abs(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Abs", TypeKind::F64,
        BLOCK(Value::fromF64(std::abs(operand.asF64()))),
        BLOCK(
#if CPU(X86_64)
            m_jit.move64ToDouble(TrustedImm64(0x7fffffffffffffffll), wasmScratchFPR);
            m_jit.andDouble(operandLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
#else
            m_jit.absDouble(operandLocation.asFPR(), resultLocation.asFPR());
#endif
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Sqrt(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Sqrt", TypeKind::F32,
        BLOCK(Value::fromF32(Math::sqrtFloat(operand.asF32()))),
        BLOCK(
            m_jit.sqrtFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Sqrt(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Sqrt", TypeKind::F64,
        BLOCK(Value::fromF64(Math::sqrtDouble(operand.asF64()))),
        BLOCK(
            m_jit.sqrtDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Neg(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Neg", TypeKind::F32,
        BLOCK(Value::fromF32(-operand.asF32())),
        BLOCK(
#if CPU(X86_64)
            m_jit.moveFloatTo32(operandLocation.asFPR(), wasmScratchGPR);
            m_jit.xor32(TrustedImm32(bitwise_cast<uint32_t>(static_cast<float>(-0.0))), wasmScratchGPR);
            m_jit.move32ToFloat(wasmScratchGPR, resultLocation.asFPR());
#else
            m_jit.negateFloat(operandLocation.asFPR(), resultLocation.asFPR());
#endif
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Neg(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Neg", TypeKind::F64,
        BLOCK(Value::fromF64(-operand.asF64())),
        BLOCK(
#if CPU(X86_64)
            m_jit.moveDoubleTo64(operandLocation.asFPR(), wasmScratchGPR);
            m_jit.xor64(TrustedImm64(bitwise_cast<uint64_t>(static_cast<double>(-0.0))), wasmScratchGPR);
            m_jit.move64ToDouble(wasmScratchGPR, resultLocation.asFPR());
#else
            m_jit.negateDouble(operandLocation.asFPR(), resultLocation.asFPR());
#endif
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32TruncSF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncSF32, operand, result, Types::I32, Types::F32);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32TruncSF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncSF64, operand, result, Types::I32, Types::F64);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32TruncUF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncUF32, operand, result, Types::I32, Types::F32);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI32TruncUF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncUF64, operand, result, Types::I32, Types::F64);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64TruncSF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncSF32, operand, result, Types::I64, Types::F32);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64TruncSF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncSF64, operand, result, Types::I64, Types::F64);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64TruncUF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncUF32, operand, result, Types::I64, Types::F32);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64TruncUF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncUF64, operand, result, Types::I64, Types::F64);
}

// References

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefEq(Value ref0, Value ref1, Value& result)
{
    return addI64Eq(ref0, ref1, result);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefFunc(FunctionSpaceIndex index, Value& result)
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    TypeKind returnType = TypeKind::Ref;

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(index)
    };
    result = topValue(returnType);
    emitCCall(&operationWasmRefFunc, arguments, result);

    return { };
}

void BBQJIT::emitEntryTierUpCheck()
{
    if (!canTierUpToOMG())
        return;

#if ENABLE(WEBASSEMBLY_OMGJIT)
    static_assert(GPRInfo::nonPreservedNonArgumentGPR0 == wasmScratchGPR);
    m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(&m_callee.tierUpCounter().m_counter)), wasmScratchGPR);
    Jump tierUp = m_jit.branchAdd32(CCallHelpers::PositiveOrZero, TrustedImm32(TierUpCount::functionEntryIncrement()), Address(wasmScratchGPR));
    MacroAssembler::Label tierUpResume = m_jit.label();
    addLatePath([tierUp, tierUpResume](BBQJIT& generator, CCallHelpers& jit) {
        tierUp.link(&jit);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::nonPreservedNonArgumentGPR0);
        jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(triggerOMGEntryTierUpThunkGenerator(generator.m_usesSIMD)).code()));
        jit.jump(tierUpResume);
    });
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

// Control flow
ControlData WARN_UNUSED_RETURN BBQJIT::addTopLevel(BlockSignature signature)
{
    if (UNLIKELY(Options::verboseBBQJITInstructions())) {
        auto nameSection = m_info.nameSection;
        std::pair<const Name*, RefPtr<NameSection>> name = nameSection->get(m_functionIndex);
        dataLog("BBQ\tFunction ");
        if (name.first)
            dataLog(makeString(*name.first));
        else
            dataLog(m_functionIndex);
        dataLogLn(" ", *m_functionSignature);
        LOG_INDENT();
    }

    m_pcToCodeOriginMapBuilder.appendItem(m_jit.label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());
    m_jit.emitFunctionPrologue();
    m_topLevel = ControlData(*this, BlockType::TopLevel, signature, 0);

    JIT_COMMENT(m_jit, "Store boxed JIT callee");
    m_jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(&m_callee)), wasmScratchGPR);
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    if constexpr (is32Bit()) {
        CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
        m_jit.storePtr(wasmScratchGPR, calleeSlot.withOffset(PayloadOffset));
        m_jit.store32(CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag), calleeSlot.withOffset(TagOffset));
        m_jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    } else
        m_jit.storePairPtr(GPRInfo::wasmContextInstancePointer, wasmScratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), wasmScratchGPR));

    bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();
    if (mayHaveExceptionHandlers)
        m_jit.store32(CCallHelpers::TrustedImm32(PatchpointExceptionHandle::s_invalidCallSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Because we compile in a single pass, we always need to pessimistically check for stack underflow/overflow.
    static_assert(wasmScratchGPR == GPRInfo::nonPreservedNonArgumentGPR0);
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, wasmScratchGPR);

    MacroAssembler::JumpList overflow;
    JIT_COMMENT(m_jit, "Stack overflow check");
    overflow.append(m_jit.branchPtr(CCallHelpers::Above, wasmScratchGPR, GPRInfo::callFrameRegister));
    overflow.append(m_jit.branchPtr(CCallHelpers::Below, wasmScratchGPR, CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit())));
    overflow.linkThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()), &m_jit);

    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);

    LocalOrTempIndex i = 0;
    for (; i < m_arguments.size(); ++i)
        flushValue(Value::fromLocal(m_parser->typeOfLocal(i).kind, i));

    // Zero all locals that aren't initialized by arguments.
    enum class ClearMode { Zero, JSNull };
    std::optional<int32_t> highest;
    std::optional<int32_t> lowest;
    auto flushZeroClear = [&]() {
        if (!lowest)
            return;
        size_t size = highest.value() - lowest.value();
        int32_t pointer = lowest.value();

        // Adjust pointer offset to be efficient for paired-stores.
        if (pointer & 4 && size >= 4) {
            m_jit.store32(TrustedImm32(0), Address(GPRInfo::callFrameRegister, pointer));
            pointer += 4;
            size -= 4;
        }

#if CPU(ARM64)
        if (pointer & 8 && size >= 8) {
            m_jit.store64(TrustedImm64(0), Address(GPRInfo::callFrameRegister, pointer));
            pointer += 8;
            size -= 8;
        }

        unsigned count = size / 16;
        for (unsigned i = 0; i < count; ++i) {
            m_jit.storePair64(ARM64Registers::zr, ARM64Registers::zr, GPRInfo::callFrameRegister, TrustedImm32(pointer));
            pointer += 16;
            size -= 16;
        }
        if (size & 8) {
            m_jit.store64(TrustedImm64(0), Address(GPRInfo::callFrameRegister, pointer));
            pointer += 8;
            size -= 8;
        }
#else
        unsigned count = size / 8;
        for (unsigned i = 0; i < count; ++i) {
            m_jit.store32(TrustedImm32(0), Address(GPRInfo::callFrameRegister, pointer));
            m_jit.store32(TrustedImm32(0), Address(GPRInfo::callFrameRegister, pointer+4));
            pointer += 8;
            size -= 8;
        }
#endif

        if (size & 4) {
            m_jit.store32(TrustedImm32(0), Address(GPRInfo::callFrameRegister, pointer));
            pointer += 4;
            size -= 4;
        }
        ASSERT(size == 0);

        highest = std::nullopt;
        lowest = std::nullopt;
    };

    auto clear = [&](ClearMode mode, TypeKind type, Location location) {
        if (mode == ClearMode::JSNull) {
            flushZeroClear();
            emitStoreConst(Value::fromI64(bitwise_cast<uint64_t>(JSValue::encode(jsNull()))), location);
            return;
        }
        if (!highest)
            highest = location.asStackOffset() + sizeOfType(type);
        lowest = location.asStackOffset();
    };

    JIT_COMMENT(m_jit, "initialize locals");
    for (; i < m_locals.size(); ++i) {
        TypeKind type = m_parser->typeOfLocal(i).kind;
        switch (type) {
        case TypeKind::I32:
        case TypeKind::I31ref:
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::I64:
        case TypeKind::Struct:
        case TypeKind::Rec:
        case TypeKind::Func:
        case TypeKind::Array:
        case TypeKind::Sub:
        case TypeKind::Subfinal:
        case TypeKind::V128:
            clear(ClearMode::Zero, type, m_locals[i]);
            break;
        case TypeKind::Exn:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Structref:
        case TypeKind::Arrayref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Nullref:
        case TypeKind::Nullfuncref:
        case TypeKind::Nullexternref:
            clear(ClearMode::JSNull, type, m_locals[i]);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    flushZeroClear();
    JIT_COMMENT(m_jit, "initialize locals done");

    for (size_t i = 0; i < m_functionSignature->argumentCount(); i ++)
        m_topLevel.touch(i); // Ensure arguments are flushed to persistent locations when this block ends.

    emitEntryTierUpCheck();

    return m_topLevel;
}

bool BBQJIT::hasLoops() const
{
    return m_compilation->bbqLoopEntrypoints.size();
}

MacroAssembler::Label BBQJIT::addLoopOSREntrypoint()
{
    // To handle OSR entry into loops, we emit a second entrypoint, which sets up our call frame then calls an
    // operation to get the address of the loop we're trying to enter. Unlike the normal prologue, by the time
    // we emit this entry point, we:
    //  - Know the frame size, so we don't need to patch the constant.
    //  - Can omit the entry tier-up check, since this entry point is only reached when we initially tier up into a loop.
    //  - Don't need to zero our locals, since they are restored from the OSR entry scratch buffer anyway.
    auto label = m_jit.label();
    m_jit.emitFunctionPrologue();

    m_jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(&m_callee)), wasmScratchGPR);
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    if constexpr (is32Bit()) {
        CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
        m_jit.storePtr(wasmScratchGPR, calleeSlot.withOffset(PayloadOffset));
        m_jit.store32(CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag), calleeSlot.withOffset(TagOffset));
        m_jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    } else
        m_jit.storePairPtr(GPRInfo::wasmContextInstancePointer, wasmScratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    int roundedFrameSize = stackCheckSize();
#if CPU(X86_64) || CPU(ARM64)
    m_jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32(roundedFrameSize), MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32(roundedFrameSize), wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif

    // The loop_osr slow path should have already checked that we have enough space. We have already destroyed the llint stack, and unwind will see the BBQ catch
        // since we already replaced callee. So, we just assert that this case doesn't happen to avoid reading a corrupted frame from the bbq catch handler.
    MacroAssembler::JumpList overflow;
    overflow.append(m_jit.branchPtr(CCallHelpers::Above, MacroAssembler::stackPointerRegister, GPRInfo::callFrameRegister));
    overflow.append(m_jit.branchPtr(CCallHelpers::Below, MacroAssembler::stackPointerRegister, CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit())));
    overflow.linkThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(crashDueToBBQStackOverflowGenerator).code()), &m_jit);

    // This operation shuffles around values on the stack, until everything is in the right place. Then,
    // it returns the address of the loop we're jumping to in wasmScratchGPR (so we don't interfere with
    // anything we just loaded from the scratch buffer into a register)
    m_jit.probe(tagCFunction<JITProbePtrTag>(operationWasmLoopOSREnterBBQJIT), nullptr, m_usesSIMD ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors);

    // We expect the loop address to be populated by the probe operation.
    static_assert(wasmScratchGPR == GPRInfo::nonPreservedNonArgumentGPR0);
    m_jit.farJump(wasmScratchGPR, WasmEntryPtrTag);
    return label;
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack)
{
    result = ControlData(*this, BlockType::Block, signature, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature->argumentCount());
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("Block", *signature);
    LOG_INDENT();
    splitStack(signature, enclosingStack, newStack);
    result.startBlock(*this, newStack);
    return { };
}

B3::Type BBQJIT::toB3Type(Type type)
{
    return Wasm::toB3Type(type);
}

B3::Type BBQJIT::toB3Type(TypeKind kind)
{
    switch (kind) {
    case TypeKind::I32:
        return B3::Type(B3::Int32);
    case TypeKind::I64:
        return B3::Type(B3::Int64);
    case TypeKind::I31ref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Structref:
    case TypeKind::Arrayref:
    case TypeKind::Funcref:
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        return B3::Type(B3::Int64);
    case TypeKind::F32:
        return B3::Type(B3::Float);
    case TypeKind::F64:
        return B3::Type(B3::Double);
    case TypeKind::V128:
        return B3::Type(B3::V128);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return B3::Void;
    }
}

B3::ValueRep BBQJIT::toB3Rep(Location location)
{
    if (location.isRegister())
        return B3::ValueRep(location.isGPR() ? Reg(location.asGPR()) : Reg(location.asFPR()));
    if (location.isStack())
        return B3::ValueRep(ValueLocation::stack(location.asStackOffset()));

    RELEASE_ASSERT_NOT_REACHED();
    return B3::ValueRep();
}

StackMap BBQJIT::makeStackMap(const ControlData& data, Stack& enclosingStack)
{
    unsigned numElements = m_locals.size() + data.enclosedHeight() + data.argumentLocations().size();

    StackMap stackMap(numElements);
    unsigned stackMapIndex = 0;
    for (unsigned i = 0; i < m_locals.size(); i ++)
        stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(m_locals[i]), toB3Type(m_localTypes[i]));

    if (Options::useWasmIPInt()) {
        // Do rethrow slots first because IPInt has them in a shadow stack.
        for (const ControlEntry& entry : m_parser->controlStack()) {
            for (unsigned i = 0; i < entry.controlData.implicitSlots(); i ++) {
                Value exception = this->exception(entry.controlData);
                stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(locationOf(exception)), B3::Int64); // Exceptions are EncodedJSValues, so they are always Int64
            }
        }

        for (const ControlEntry& entry : m_parser->controlStack()) {
            for (const TypedExpression& expr : entry.enclosedExpressionStack)
                stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(locationOf(expr.value())), toB3Type(expr.type().kind));
        }

        for (const TypedExpression& expr : enclosingStack)
            stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(locationOf(expr.value())), toB3Type(expr.type().kind));
        for (unsigned i = 0; i < data.argumentLocations().size(); i ++)
            stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(data.argumentLocations()[i]), toB3Type(data.argumentType(i).kind));


    } else {
        for (const ControlEntry& entry : m_parser->controlStack()) {
            for (const TypedExpression& expr : entry.enclosedExpressionStack)
                stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(locationOf(expr.value())), toB3Type(expr.type().kind));
            if (ControlData::isAnyCatch(entry.controlData)) {
                for (unsigned i = 0; i < entry.controlData.implicitSlots(); i ++) {
                    Value exception = this->exception(entry.controlData);
                    stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(locationOf(exception)), B3::Int64); // Exceptions are EncodedJSValues, so they are always Int64
                }
            }
        }
        for (const TypedExpression& expr : enclosingStack)
            stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(locationOf(expr.value())), toB3Type(expr.type().kind));
        for (unsigned i = 0; i < data.argumentLocations().size(); i ++)
            stackMap[stackMapIndex ++] = OSREntryValue(toB3Rep(data.argumentLocations()[i]), toB3Type(data.argumentType(i).kind));
    }

    RELEASE_ASSERT(stackMapIndex == numElements);
    m_osrEntryScratchBufferSize = std::max(m_osrEntryScratchBufferSize, numElements + BBQCallee::extraOSRValuesForLoopIndex);
    return stackMap;
}

void BBQJIT::emitLoopTierUpCheckAndOSREntryData(const ControlData& data, Stack& enclosingStack, unsigned loopIndex)
{
    auto& tierUpCounter = m_callee.tierUpCounter();
    ASSERT(tierUpCounter.osrEntryTriggers().size() == loopIndex);
    tierUpCounter.osrEntryTriggers().append(TierUpCount::TriggerReason::DontTrigger);

    unsigned outerLoops = m_outerLoops.isEmpty() ? UINT32_MAX : m_outerLoops.last();
    tierUpCounter.outerLoops().append(outerLoops);
    m_outerLoops.append(loopIndex);

    OSREntryData& osrEntryData = tierUpCounter.addOSREntryData(m_functionIndex, loopIndex, makeStackMap(data, enclosingStack));

    if (!canTierUpToOMG())
        return;

#if ENABLE(WEBASSEMBLY_OMGJIT)
    static_assert(GPRInfo::nonPreservedNonArgumentGPR0 == wasmScratchGPR);
    m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(&tierUpCounter.m_counter)), wasmScratchGPR);

    TierUpCount::TriggerReason* forceEntryTrigger = &(tierUpCounter.osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");

    Jump forceOSREntry = m_jit.branchTest8(ResultCondition::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
    Jump tierUp = m_jit.branchAdd32(ResultCondition::PositiveOrZero, TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(wasmScratchGPR));
    MacroAssembler::Label tierUpResume = m_jit.label();

    OSREntryData* osrEntryDataPtr = &osrEntryData;

    addLatePath([forceOSREntry, tierUp, tierUpResume, osrEntryDataPtr](BBQJIT& generator, CCallHelpers& jit) {
        forceOSREntry.link(&jit);
        tierUp.link(&jit);

        Probe::SavedFPWidth savedFPWidth = generator.m_usesSIMD ? Probe::SavedFPWidth::SaveVectors : Probe::SavedFPWidth::DontSaveVectors; // By the time we reach the late path, we should know whether or not the function uses SIMD.
        jit.probe(tagCFunction<JITProbePtrTag>(operationWasmTriggerOSREntryNow), osrEntryDataPtr, savedFPWidth);
        jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::nonPreservedNonArgumentGPR0).linkTo(tierUpResume, &jit);
        jit.farJump(GPRInfo::nonPreservedNonArgumentGPR0, WasmEntryPtrTag);
    });
#else
    UNUSED_PARAM(osrEntryData);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack, uint32_t loopIndex)
{
    result = ControlData(*this, BlockType::Loop, signature, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature->argumentCount());
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("Loop", *signature);
    LOG_INDENT();
    splitStack(signature, enclosingStack, newStack);
    result.startBlock(*this, newStack);
    result.setLoopLabel(m_jit.label());

    RELEASE_ASSERT(m_compilation->bbqLoopEntrypoints.size() == loopIndex);
    m_compilation->bbqLoopEntrypoints.append(result.loopLabel());

    emitLoopTierUpCheckAndOSREntryData(result, enclosingStack, loopIndex);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addIf(Value condition, BlockSignature signature, Stack& enclosingStack, ControlData& result, Stack& newStack)
{
    RegisterSet liveScratchGPRs;
    Location conditionLocation;
    if (!condition.isConst()) {
        conditionLocation = loadIfNecessary(condition);
        liveScratchGPRs.add(conditionLocation.asGPR(), IgnoreVectors);
    }
    consume(condition);

    result = ControlData(*this, BlockType::If, signature, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature->argumentCount(), liveScratchGPRs);

    // Despite being conditional, if doesn't need to worry about diverging expression stacks at block boundaries, so it doesn't need multiple exits.
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("If", *signature, condition, conditionLocation);
    LOG_INDENT();
    splitStack(signature, enclosingStack, newStack);

    result.startBlock(*this, newStack);
    if (condition.isConst() && !condition.asI32())
        result.setIfBranch(m_jit.jump()); // Emit direct branch if we know the condition is false.
    else if (!condition.isConst()) // Otherwise, we only emit a branch at all if we don't know the condition statically.
        result.setIfBranch(m_jit.branchTest32(ResultCondition::Zero, conditionLocation.asGPR()));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addElse(ControlData& data, Stack& expressionStack)
{
    data.flushAndSingleExit(*this, data, expressionStack, false, true);
    ControlData dataElse(ControlData::UseBlockCallingConventionOfOtherBranch, BlockType::Block, data);
    data.linkJumps(&m_jit);
    dataElse.addBranch(m_jit.jump());
    data.linkIfBranch(&m_jit); // Link specifically the conditional branch of the preceding If
    LOG_DEDENT();
    LOG_INSTRUCTION("Else");
    LOG_INDENT();

    // We don't care at this point about the values live at the end of the previous control block,
    // we just need the right number of temps for our arguments on the top of the stack.
    expressionStack.clear();
    while (expressionStack.size() < data.signature()->argumentCount()) {
        Type type = data.signature()->argumentType(expressionStack.size());
        expressionStack.constructAndAppend(type, Value::fromTemp(type.kind, dataElse.enclosedHeight() + dataElse.implicitSlots() + expressionStack.size()));
    }

    dataElse.startBlock(*this, expressionStack);
    data = dataElse;
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addElseToUnreachable(ControlData& data)
{
    // We want to flush or consume all values on the stack to reset the allocator
    // state entering the else block.
    data.flushAtBlockBoundary(*this, 0, m_parser->expressionStack(), true);

    ControlData dataElse(ControlData::UseBlockCallingConventionOfOtherBranch, BlockType::Block, data);
    data.linkJumps(&m_jit);
    dataElse.addBranch(m_jit.jump()); // Still needed even when the parent was unreachable to avoid running code within the else block.
    data.linkIfBranch(&m_jit); // Link specifically the conditional branch of the preceding If
    LOG_DEDENT();
    LOG_INSTRUCTION("Else");
    LOG_INDENT();

    // We don't have easy access to the original expression stack we had entering the if block,
    // so we construct a local stack just to set up temp bindings as we enter the else.
    Stack expressionStack;
    auto functionSignature = dataElse.signature();
    for (unsigned i = 0; i < functionSignature->argumentCount(); i ++)
        expressionStack.constructAndAppend(functionSignature->argumentType(i), Value::fromTemp(functionSignature->argumentType(i).kind, dataElse.enclosedHeight() + dataElse.implicitSlots() + i));
    dataElse.startBlock(*this, expressionStack);
    data = dataElse;
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTry(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack)
{
    m_usesExceptions = true;
    ++m_tryCatchDepth;
    ++m_callSiteIndex;
    result = ControlData(*this, BlockType::Try, signature, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature->argumentCount());
    result.setTryInfo(m_callSiteIndex, m_callSiteIndex, m_tryCatchDepth);
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("Try", *signature);
    LOG_INDENT();
    splitStack(signature, enclosingStack, newStack);
    result.startBlock(*this, newStack);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addTryTable(BlockSignature signature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack)
{
    m_usesExceptions = true;
    ++m_tryCatchDepth;
    ++m_callSiteIndex;

    auto targetList = targets.map(
        [&](const auto& target) -> ControlData::TryTableTarget {
            return {
                target.type,
                target.tag,
                target.exceptionSignature,
                target.target
            };
        }
    );

    result = ControlData(*this, BlockType::TryTable, signature, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature->argumentCount());
    result.setTryInfo(m_callSiteIndex, m_callSiteIndex, m_tryCatchDepth);
    result.setTryTableTargets(WTFMove(targetList));
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("TryTable", *signature);
    LOG_INDENT();
    splitStack(signature, enclosingStack, newStack);
    result.startBlock(*this, newStack);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCatch(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, Stack& expressionStack, ControlType& data, ResultList& results)
{
    m_usesExceptions = true;
    data.flushAndSingleExit(*this, data, expressionStack, false, true);
    ControlData dataCatch(*this, BlockType::Catch, data.signature(), data.enclosedHeight());
    dataCatch.setCatchKind(CatchKind::Catch);
    if (ControlData::isTry(data)) {
        ++m_callSiteIndex;
        data.setTryInfo(data.tryStart(), m_callSiteIndex, data.tryCatchDepth());
    }
    dataCatch.setTryInfo(data.tryStart(), data.tryEnd(), data.tryCatchDepth());

    data.delegateJumpsTo(dataCatch);
    dataCatch.addBranch(m_jit.jump());
    LOG_DEDENT();
    LOG_INSTRUCTION("Catch");
    LOG_INDENT();
    emitCatchImpl(dataCatch, exceptionSignature, results);
    data = WTFMove(dataCatch);
    m_exceptionHandlers.append({ HandlerType::Catch, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, ControlType& data, ResultList& results)
{
    m_usesExceptions = true;
    ControlData dataCatch(*this, BlockType::Catch, data.signature(), data.enclosedHeight());
    dataCatch.setCatchKind(CatchKind::Catch);
    if (ControlData::isTry(data)) {
        ++m_callSiteIndex;
        data.setTryInfo(data.tryStart(), m_callSiteIndex, data.tryCatchDepth());
    }
    dataCatch.setTryInfo(data.tryStart(), data.tryEnd(), data.tryCatchDepth());

    data.delegateJumpsTo(dataCatch);
    LOG_DEDENT();
    LOG_INSTRUCTION("Catch");
    LOG_INDENT();
    emitCatchImpl(dataCatch, exceptionSignature, results);
    data = WTFMove(dataCatch);
    m_exceptionHandlers.append({ HandlerType::Catch, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCatchAll(Stack& expressionStack, ControlType& data)
{
    m_usesExceptions = true;
    data.flushAndSingleExit(*this, data, expressionStack, false, true);
    ControlData dataCatch(*this, BlockType::Catch, data.signature(), data.enclosedHeight());
    dataCatch.setCatchKind(CatchKind::CatchAll);
    if (ControlData::isTry(data)) {
        ++m_callSiteIndex;
        data.setTryInfo(data.tryStart(), m_callSiteIndex, data.tryCatchDepth());
    }
    dataCatch.setTryInfo(data.tryStart(), data.tryEnd(), data.tryCatchDepth());

    data.delegateJumpsTo(dataCatch);
    dataCatch.addBranch(m_jit.jump());
    LOG_DEDENT();
    LOG_INSTRUCTION("CatchAll");
    LOG_INDENT();
    emitCatchAllImpl(dataCatch);
    data = WTFMove(dataCatch);
    m_exceptionHandlers.append({ HandlerType::CatchAll, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, 0 });
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCatchAllToUnreachable(ControlType& data)
{
    m_usesExceptions = true;
    ControlData dataCatch(*this, BlockType::Catch, data.signature(), data.enclosedHeight());
    dataCatch.setCatchKind(CatchKind::CatchAll);
    if (ControlData::isTry(data)) {
        ++m_callSiteIndex;
        data.setTryInfo(data.tryStart(), m_callSiteIndex, data.tryCatchDepth());
    }
    dataCatch.setTryInfo(data.tryStart(), data.tryEnd(), data.tryCatchDepth());

    data.delegateJumpsTo(dataCatch);
    LOG_DEDENT();
    LOG_INSTRUCTION("CatchAll");
    LOG_INDENT();
    emitCatchAllImpl(dataCatch);
    data = WTFMove(dataCatch);
    m_exceptionHandlers.append({ HandlerType::CatchAll, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, 0 });
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addDelegate(ControlType& target, ControlType& data)
{
    return addDelegateToUnreachable(target, data);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addDelegateToUnreachable(ControlType& target, ControlType& data)
{
    unsigned depth = 0;
    if (ControlType::isTry(target))
        depth = target.tryCatchDepth();

    if (ControlData::isTry(data)) {
        ++m_callSiteIndex;
        data.setTryInfo(data.tryStart(), m_callSiteIndex, data.tryCatchDepth());
    }
    m_exceptionHandlers.append({ HandlerType::Delegate, data.tryStart(), m_callSiteIndex, 0, m_tryCatchDepth, depth });
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addThrow(unsigned exceptionIndex, ArgumentList& arguments, Stack&)
{

    LOG_INSTRUCTION("Throw", arguments);

    unsigned offset = 0;
    for (auto arg : arguments) {
        Location stackLocation = Location::fromStackArgument(offset * sizeof(uint64_t));
        emitMove(arg, stackLocation);
        consume(arg);
        offset += arg.type() == TypeKind::V128 ? 2 : 1;
    }
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(offset * sizeof(uint64_t));
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    ++m_callSiteIndex;
    bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();
    if (mayHaveExceptionHandlers) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegisters();
    }
    m_jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    emitThrowImpl(m_jit, exceptionIndex);

    return { };
}

void BBQJIT::prepareForExceptions()
{
    ++m_callSiteIndex;
    bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();
    if (mayHaveExceptionHandlers) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegistersForException();
    }
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addReturn(const ControlData& data, const Stack& returnValues)
{
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*data.signature(), CallRole::Callee);

    if (!wasmCallInfo.results.isEmpty()) {
        ASSERT(returnValues.size() >= wasmCallInfo.results.size());
        unsigned offset = returnValues.size() - wasmCallInfo.results.size();
        Vector<Value, 8> returnValuesForShuffle;
        Vector<Location, 8> returnLocationsForShuffle;
        for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i) {
            returnValuesForShuffle.append(returnValues[offset + i]);
            returnLocationsForShuffle.append(Location::fromArgumentLocation(wasmCallInfo.results[i], returnValues[offset + i].type().kind));
        }
        emitShuffle(returnValuesForShuffle, returnLocationsForShuffle);
        LOG_INSTRUCTION("Return", returnLocationsForShuffle);
    } else
        LOG_INSTRUCTION("Return");

    for (const auto& value : returnValues)
        consume(value);

    const ControlData& enclosingBlock = !m_parser->controlStack().size() ? data : currentControlData();
    for (LocalOrTempIndex localIndex : enclosingBlock.m_touchedLocals) {
        Value local = Value::fromLocal(m_localTypes[localIndex], localIndex);
        if (locationOf(local).isRegister()) {
            // Flush all locals without emitting stores (since we're leaving anyway)
            unbind(local, locationOf(local));
            bind(local, canonicalSlot(local));
        }
    }

    m_jit.emitFunctionEpilogue();
    m_jit.ret();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addBranch(ControlData& target, Value condition, Stack& results)
{
    if (condition.isConst() && !condition.asI32()) // If condition is known to be false, this is a no-op.
        return { };

    // It should be safe to directly use the condition location here. Between
    // this point and when we use the condition register, we can only flush,
    // we don't do any shuffling. Flushing will involve only the registers held
    // by live values, and since we are about to consume the condition, its
    // register is not one of them. The scratch register would be vulnerable to
    // clobbering, but we don't need it - if our condition is a constant, we
    // just fold away the branch instead of materializing it.
    Location conditionLocation;
    if (!condition.isNone() && !condition.isConst())
        conditionLocation = loadIfNecessary(condition);
    consume(condition);

    if (condition.isNone())
        LOG_INSTRUCTION("Branch");
    else
        LOG_INSTRUCTION("Branch", condition, conditionLocation);

    if (condition.isConst() || condition.isNone()) {
        currentControlData().flushAndSingleExit(*this, target, results, false, condition.isNone());
        target.addBranch(m_jit.jump()); // We know condition is true, since if it was false we would have returned early.
    } else {
        currentControlData().flushAtBlockBoundary(*this, 0, results, condition.isNone());
        Jump ifNotTaken = m_jit.branchTest32(ResultCondition::Zero, conditionLocation.asGPR());
        currentControlData().addExit(*this, target.targetLocations(), results);
        target.addBranch(m_jit.jump());
        ifNotTaken.link(&m_jit);
        currentControlData().finalizeBlock(*this, target.targetLocations().size(), results, true);
    }

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSwitch(Value condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, Stack& results)
{
    ASSERT(condition.type() == TypeKind::I32);

    LOG_INSTRUCTION("BrTable", condition);

    if (!condition.isConst())
        emitMove(condition, Location::fromGPR(wasmScratchGPR));
    consume(condition);

    if (condition.isConst()) {
        // If we know the condition statically, we emit one direct branch to the known target.
        int targetIndex = condition.asI32();
        if (targetIndex >= 0 && targetIndex < static_cast<int>(targets.size())) {
            currentControlData().flushAndSingleExit(*this, *targets[targetIndex], results, false, true);
            targets[targetIndex]->addBranch(m_jit.jump());
        } else {
            currentControlData().flushAndSingleExit(*this, defaultTarget, results, false, true);
            defaultTarget.addBranch(m_jit.jump());
        }
        return { };
    }

    // Flush everything below the top N values.
    currentControlData().flushAtBlockBoundary(*this, defaultTarget.targetLocations().size(), results, true);

    constexpr unsigned minCasesForTable = 7;
    if (minCasesForTable <= targets.size()) {
        auto* jumpTable = m_callee.addJumpTable(targets.size());
        auto fallThrough = m_jit.branch32(RelationalCondition::AboveOrEqual, wasmScratchGPR, TrustedImm32(targets.size()));
        m_jit.zeroExtend32ToWord(wasmScratchGPR, wasmScratchGPR);
        if constexpr (is64Bit())
            m_jit.lshiftPtr(TrustedImm32(3), wasmScratchGPR);
        else
            m_jit.lshiftPtr(TrustedImm32(2), wasmScratchGPR);
        m_jit.addPtr(TrustedImmPtr(jumpTable->data()), wasmScratchGPR);
        m_jit.farJump(Address(wasmScratchGPR), JSSwitchPtrTag);

        auto labels = WTF::map(targets, [&](auto& target) {
            auto label = Box<CCallHelpers::Label>::create(m_jit.label());
            bool isCodeEmitted = currentControlData().addExit(*this, target->targetLocations(), results);
            if (isCodeEmitted)
                target->addBranch(m_jit.jump());
            else {
                // It is common that we do not need to emit anything before jumping to the target block.
                // In that case, we put Box<Label> which will be filled later when the end of the block is linked.
                // We put direct jump to that block in the link task.
                target->addLabel(Box { label });
            }
            return label;
        });

        m_jit.addLinkTask([labels = WTFMove(labels), jumpTable](LinkBuffer& linkBuffer) {
            for (unsigned index = 0; index < labels.size(); ++index)
                jumpTable->at(index) = linkBuffer.locationOf<JSSwitchPtrTag>(*labels[index]);
        });

        fallThrough.link(&m_jit);
    } else {
        Vector<int64_t, 16> cases(targets.size(), [](size_t i) { return i; });

        BinarySwitch binarySwitch(wasmScratchGPR, cases.span(), BinarySwitch::Int32);
        while (binarySwitch.advance(m_jit)) {
            unsigned value = binarySwitch.caseValue();
            unsigned index = binarySwitch.caseIndex();
            ASSERT_UNUSED(value, value == index);
            ASSERT(index < targets.size());
            currentControlData().addExit(*this, targets[index]->targetLocations(), results);
            targets[index]->addBranch(m_jit.jump());
        }

        binarySwitch.fallThrough().link(&m_jit);
    }
    currentControlData().addExit(*this, defaultTarget.targetLocations(), results);
    defaultTarget.addBranch(m_jit.jump());

    currentControlData().finalizeBlock(*this, defaultTarget.targetLocations().size(), results, false);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::endBlock(ControlEntry& entry, Stack& stack)
{
    return addEndToUnreachable(entry, stack, false);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addEndToUnreachable(ControlEntry& entry, Stack& stack, bool unreachable)
{
    ControlData& entryData = entry.controlData;

    unsigned returnCount = entryData.signature()->returnCount();
    if (unreachable) {
        for (unsigned i = 0; i < returnCount; ++i) {
            Type type = entryData.signature()->returnType(i);
            entry.enclosedExpressionStack.constructAndAppend(type, Value::fromTemp(type.kind, entryData.enclosedHeight() + entryData.implicitSlots() + i));
        }
        for (const auto& binding : m_gprBindings) {
            if (!binding.isNone())
                consume(binding.toValue());
        }
        for (const auto& binding : m_fprBindings) {
            if (!binding.isNone())
                consume(binding.toValue());
        }
    } else {
        unsigned offset = stack.size() - returnCount;
        for (unsigned i = 0; i < returnCount; ++i)
            entry.enclosedExpressionStack.append(stack[i + offset]);
    }

    switch (entryData.blockType()) {
    case BlockType::TopLevel:
        entryData.flushAndSingleExit(*this, entryData, entry.enclosedExpressionStack, false, true, unreachable);
        entryData.linkJumps(&m_jit);
        for (unsigned i = 0; i < returnCount; ++i) {
            // Make sure we expect the stack values in the correct locations.
            if (!entry.enclosedExpressionStack[i].value().isConst()) {
                Value& value = entry.enclosedExpressionStack[i].value();
                value = Value::fromTemp(value.type(), i);
                Location valueLocation = locationOf(value);
                if (valueLocation.isRegister())
                    RELEASE_ASSERT(valueLocation == entryData.resultLocations()[i]);
                else
                    bind(value, entryData.resultLocations()[i]);
            }
        }
        return addReturn(entryData, entry.enclosedExpressionStack);
    case BlockType::Loop:
        entryData.convertLoopToBlock();
        entryData.flushAndSingleExit(*this, entryData, entry.enclosedExpressionStack, false, true, unreachable);
        entryData.linkJumpsTo(entryData.loopLabel(), &m_jit);
        m_outerLoops.takeLast();
        break;
    case BlockType::Try:
    case BlockType::Catch:
        --m_tryCatchDepth;
        entryData.flushAndSingleExit(*this, entryData, entry.enclosedExpressionStack, false, true, unreachable);
        entryData.linkJumps(&m_jit);
        break;
    case BlockType::TryTable: {
        // normal execution: jump past the handlers
        entryData.flushAndSingleExit(*this, entryData, entry.enclosedExpressionStack, false, true, unreachable);
        entryData.addBranch(m_jit.jump());
        // similar to LLInt, we make a handler section to avoid jumping into random parts of code and not having
        // a real landing pad
        // FIXME: should we generate this all at the end of the code? this might help icache performance since
        // exceptions are rare
        ++m_callSiteIndex;

        for (auto& target : entryData.m_tryTableTargets)
            emitCatchTableImpl(entryData, target);

        // we're done!
        --m_tryCatchDepth;
        entryData.linkJumps(&m_jit);
        break;
    }
    default:
        entryData.flushAndSingleExit(*this, entryData, entry.enclosedExpressionStack, false, true, unreachable);
        entryData.linkJumps(&m_jit);
        break;
    }

    LOG_DEDENT();
    LOG_INSTRUCTION("End");

    currentControlData().resumeBlock(*this, entryData, entry.enclosedExpressionStack);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::endTopLevel(BlockSignature, const Stack&)
{
    int frameSize = stackCheckSize();
    CCallHelpers& jit = m_jit;
    m_jit.addLinkTask([frameSize, labels = WTFMove(m_frameSizeLabels), &jit](LinkBuffer& linkBuffer) {
        for (auto label : labels)
            jit.repatchPointer(linkBuffer.locationOf<NoPtrTag>(label), bitwise_cast<void*>(static_cast<uintptr_t>(frameSize)));
    });

    LOG_DEDENT();
    LOG_INSTRUCTION("End");

    if (UNLIKELY(m_disassembler))
        m_disassembler->setEndOfOpcode(m_jit.label());

    for (const auto& latePath : m_latePaths)
        latePath->run(*this, m_jit);

    for (unsigned i = 0; i < numberOfExceptionTypes; ++i) {
        auto& jumps = m_exceptions[i];
        if (!jumps.empty()) {
            jumps.link(&jit);
            emitThrowException(static_cast<ExceptionType>(i));
        }
    }

    for (const auto& [jump, returnLabel, typeIndex, rttReg] : m_rttSlowPathJumps) {
        jump.link(&jit);
        emitSlowPathRTTCheck(returnLabel, typeIndex, rttReg);
    }

    m_compilation->osrEntryScratchBufferSize = m_osrEntryScratchBufferSize;
    return { };
}

// Flush a value to its canonical slot.
void BBQJIT::flushValue(Value value)
{
    if (value.isConst() || value.isPinned())
        return;
    Location currentLocation = locationOf(value);
    Location slot = canonicalSlot(value);

    emitMove(value, slot);
    unbind(value, currentLocation);
    bind(value, slot);
}

void BBQJIT::restoreWebAssemblyContextInstance()
{
    m_jit.loadPtr(Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * sizeof(Register)), GPRInfo::wasmContextInstancePointer);
}

void BBQJIT::loadWebAssemblyGlobalState(GPRReg wasmBaseMemoryPointer, GPRReg wasmBoundsCheckingSizeRegister)
{
    m_jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
    m_jit.cageConditionally(Gigacage::Primitive, wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister, wasmScratchGPR);
}

void BBQJIT::flushRegistersForException()
{
    // Flush all locals.
    for (RegisterBinding& binding : m_gprBindings) {
        if (binding.toValue().isLocal())
            flushValue(binding.toValue());
    }

    for (RegisterBinding& binding : m_fprBindings) {
        if (binding.toValue().isLocal())
            flushValue(binding.toValue());
    }
}

void BBQJIT::flushRegisters()
{
    // Just flush everything.
    for (RegisterBinding& binding : m_gprBindings) {
        if (!binding.toValue().isNone())
            flushValue(binding.toValue());
    }

    for (RegisterBinding& binding : m_fprBindings) {
        if (!binding.toValue().isNone())
            flushValue(binding.toValue());
    }
}

template<size_t N>
void BBQJIT::saveValuesAcrossCallAndPassArguments(const Vector<Value, N>& arguments, const CallInformation& callInfo, const TypeDefinition& signature)
{
    // First, we resolve all the locations of the passed arguments, before any spillage occurs. For constants,
    // we store their normal values; for all other values, we store pinned values with their current location.
    // We'll directly use these when passing parameters, since no other instructions we emit here should
    // overwrite registers currently occupied by values.

    auto resolvedArguments = WTF::map<N>(arguments, [&](auto& argument) {
        auto value = argument.isConst() ? argument : Value::pinned(argument.type(), locationOf(argument));
        // Like other value uses, we count this as a use here, and end the lifetimes of any temps we passed.
        // This saves us the work of having to spill them to their canonical slots.
        consume(argument);
        return value;
    });

    // At this point in the program, argumentLocations doesn't represent the state of the register allocator.
    // We need to be careful not to allocate any new registers before passing them to the function, since that
    // could clobber the registers we assume still contain the argument values!

    // Next, for all values currently still occupying a caller-saved register, we flush them to their canonical slot.
    for (Reg reg : m_callerSaves) {
        RegisterBinding binding = reg.isGPR() ? m_gprBindings[reg.gpr()] : m_fprBindings[reg.fpr()];
        if (!binding.toValue().isNone())
            flushValue(binding.toValue());
    }

    // Additionally, we flush anything currently bound to a register we're going to use for parameter passing. I
    // think these will be handled by the caller-save logic without additional effort, but it doesn't hurt to be
    // careful.
    for (size_t i = 0; i < callInfo.params.size(); ++i) {
        auto type = signature.as<FunctionSignature>()->argumentType(i);
        Location paramLocation = Location::fromArgumentLocation(callInfo.params[i], type.kind);
        if (paramLocation.isRegister()) {
            RegisterBinding binding;
            if (paramLocation.isGPR())
                binding = m_gprBindings[paramLocation.asGPR()];
            else if (paramLocation.isFPR())
                binding = m_fprBindings[paramLocation.asFPR()];
            else if (paramLocation.isGPR2())
                binding = m_gprBindings[paramLocation.asGPRhi()];
            if (!binding.toValue().isNone())
                flushValue(binding.toValue());
        }
    }

    // Finally, we parallel-move arguments to the parameter locations.
    WTF::Vector<Location, N> parameterLocations;
    parameterLocations.reserveInitialCapacity(callInfo.params.size());
    for (unsigned i = 0; i < callInfo.params.size(); i++) {
        auto type = signature.as<FunctionSignature>()->argumentType(i);
        auto parameterLocation = Location::fromArgumentLocation(callInfo.params[i], type.kind);
        parameterLocations.append(parameterLocation);
    }

    emitShuffle(resolvedArguments, parameterLocations);
}

void BBQJIT::restoreValuesAfterCall(const CallInformation& callInfo)
{
    UNUSED_PARAM(callInfo);
    // Caller-saved values shouldn't actually need to be restored here, the register allocator will restore them lazily
    // whenever they are next used.
}

template<size_t N>
void BBQJIT::returnValuesFromCall(Vector<Value, N>& results, const FunctionSignature& functionType, const CallInformation& callInfo)
{
    for (size_t i = 0; i < callInfo.results.size(); i ++) {
        Value result = Value::fromTemp(functionType.returnType(i).kind, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + m_parser->expressionStack().size() + i);
        Location returnLocation = Location::fromArgumentLocation(callInfo.results[i], result.type());
        if (returnLocation.isRegister()) {
            RegisterBinding currentBinding;
            if (returnLocation.isGPR())
                currentBinding = m_gprBindings[returnLocation.asGPR()];
            else if (returnLocation.isFPR())
                currentBinding = m_fprBindings[returnLocation.asFPR()];
            else if (returnLocation.isGPR2())
                currentBinding = m_gprBindings[returnLocation.asGPRhi()];
            if (currentBinding.isScratch()) {
                // FIXME: This is a total hack and could cause problems. We assume scratch registers (allocated by a ScratchScope)
                // will never be live across a call. So far, this is probably true, but it's fragile. Probably the fix here is to
                // exclude all possible return value registers from ScratchScope so we can guarantee there's never any interference.
                currentBinding = RegisterBinding::none();
                if (returnLocation.isGPR()) {
                    ASSERT(m_validGPRs.contains(returnLocation.asGPR(), IgnoreVectors));
                    m_gprSet.add(returnLocation.asGPR(), IgnoreVectors);
                } else if (returnLocation.isGPR2()) {
                    ASSERT(m_validGPRs.contains(returnLocation.asGPRlo(), IgnoreVectors));
                    ASSERT(m_validGPRs.contains(returnLocation.asGPRhi(), IgnoreVectors));
                    m_gprSet.add(returnLocation.asGPRlo(), IgnoreVectors);
                    m_gprSet.add(returnLocation.asGPRhi(), IgnoreVectors);
                } else {
                    ASSERT(m_validFPRs.contains(returnLocation.asFPR(), Width::Width128));
                    m_fprSet.add(returnLocation.asFPR(), Width::Width128);
                }
            }
        } else {
            ASSERT(returnLocation.isStackArgument());
            // FIXME: Ideally, we would leave these values where they are but a subsequent call could clobber them before they are used.
            // That said, stack results are very rare so this isn't too painful.
            // Even if we did leave them where they are, we'd need to flush them to their canonical location at the next branch otherwise
            // we could have something like (assume no result regs for simplicity):
            // call (result i32 i32) $foo
            // if (result i32) // Stack: i32(StackArgument:8) i32(StackArgument:0)
            //   // Stack: i32(StackArgument:8)
            // else
            //   call (result i32 i32) $bar // Stack: i32(StackArgument:8) we have to flush the stack argument to make room for the result of bar
            //   drop // Stack: i32(Stack:X) i32(StackArgument:8) i32(StackArgument:0)
            //   drop // Stack: i32(Stack:X) i32(StackArgument:8)
            // end
            // return // Stack i32(*Conflicting locations*)

            Location canonicalLocation = canonicalSlot(result);
            emitMoveMemory(result.type(), returnLocation, canonicalLocation);
            returnLocation = canonicalLocation;
        }
        bind(result, returnLocation);
        results.append(result);
    }
}

void BBQJIT::emitTailCall(FunctionSpaceIndex functionIndex, const TypeDefinition& signature, ArgumentList& arguments)
{
    const auto& callingConvention = wasmCallingConvention();
    CallInformation callInfo = callingConvention.callInformationFor(signature, CallRole::Callee);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
    // Do this to ensure we don't write past SP.
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex).expand();
    CallInformation wasmCallerInfo = callingConvention.callInformationFor(callerTypeDefinition, CallRole::Callee);
    Checked<int32_t> callerStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCallerInfo.headerAndArgumentStackSizeInBytes);
    Checked<int32_t> tailCallStackOffsetFromFP = callerStackSize - calleeStackSize;
    ASSERT(callInfo.results.size() == wasmCallerInfo.results.size());
    ASSERT(arguments.size() == callInfo.params.size());

    ArgumentList resolvedArguments;
    resolvedArguments.reserveInitialCapacity(arguments.size() + isX86());
    Vector<Location, 8> parameterLocations;
    parameterLocations.reserveInitialCapacity(arguments.size() + isX86());

    // Save the old Frame Pointer for later and make sure the return address gets saved to its canonical location.
    auto preserved = callingConvention.argumentGPRs();
    if constexpr (isARM64E())
        preserved.add(callingConvention.prologueScratchGPRs[0], IgnoreVectors);
    ScratchScope<1, 0> scratches(*this, WTFMove(preserved));
    GPRReg callerFramePointer = scratches.gpr(0);
#if CPU(X86_64)
    m_jit.loadPtr(Address(MacroAssembler::framePointerRegister), callerFramePointer);
    resolvedArguments.append(Value::pinned(pointerType(), Location::fromStack(sizeof(Register))));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP + Checked<int>(sizeof(Register))));
#elif CPU(ARM64) || CPU(ARM_THUMB2)
    m_jit.loadPairPtr(MacroAssembler::framePointerRegister, callerFramePointer, MacroAssembler::linkRegister);
#else
    UNUSED_PARAM(callerFramePointer);
    UNREACHABLE_FOR_PLATFORM();
#endif

    // We don't need to restore any callee saves because we don't use them with the current register allocator.
    // If we did we'd want to do that here because we could clobber their stack slots when shuffling the parameters into place below.
    for (unsigned i = 0; i < arguments.size(); i ++) {
        if (arguments[i].isConst())
            resolvedArguments.append(arguments[i]);
        else
            resolvedArguments.append(Value::pinned(arguments[i].type(), locationOf(arguments[i])));

        // This isn't really needed but it's nice to have good book keeping.
        consume(arguments[i]);
    }

    for (size_t i = 0; i < callInfo.params.size(); ++i) {
        auto param = callInfo.params[i];
        switch (param.location.kind()) {
        case ValueLocation::Kind::GPRRegister:
        case ValueLocation::Kind::FPRRegister: {
            auto type = signature.as<FunctionSignature>()->argumentType(i);
            parameterLocations.append(Location::fromArgumentLocation(param, type.kind));
            break;
        }
        case ValueLocation::Kind::StackArgument:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case ValueLocation::Kind::Stack:
            parameterLocations.append(Location::fromStack(param.location.offsetFromFP() + tailCallStackOffsetFromFP));
            break;
        }
    }

    emitShuffle(resolvedArguments, parameterLocations);

#if CPU(ARM64E)
    JIT_COMMENT(m_jit, "Untag our return PC");
    // prologue scratch registers should be free as we already moved the arguments into place.
    GPRReg scratch = callingConvention.prologueScratchGPRs[0];
    m_jit.addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), MacroAssembler::framePointerRegister, scratch);
    m_jit.untagPtr(scratch, ARM64Registers::lr);
    m_jit.validateUntaggedPtr(ARM64Registers::lr, scratch);
#endif

    // Fix SP and FP
    m_jit.addPtr(TrustedImm32(tailCallStackOffsetFromFP + Checked<int32_t>(prologueStackPointerDelta())), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    m_jit.move(callerFramePointer, MacroAssembler::framePointerRegister);

    // Nothing should refer to FP after this point.

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        static_assert(sizeof(JSWebAssemblyInstance::ImportFunctionInfo) * maxImports < std::numeric_limits<int32_t>::max());
        RELEASE_ASSERT(JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndex) < std::numeric_limits<int32_t>::max());
        m_jit.farJump(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndex)), WasmEntryPtrTag);
    } else {
        // Record the callee so the callee knows to look for it in updateCallsitesToCallUs.
        m_directCallees.testAndSet(m_info.toCodeIndex(functionIndex));
        // Emit the call.
        Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;
        auto calleeMove = m_jit.storeWasmCalleeCalleePatchable(isX86() ? sizeof(Register) : 0);
        CCallHelpers::Call call = m_jit.threadSafePatchableNearTailCall();

        m_jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex, calleeMove] (LinkBuffer& linkBuffer) {
            unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex, linkBuffer.locationOf<WasmEntryPtrTag>(calleeMove) });
        });
    }

    LOG_INSTRUCTION("ReturnCall", functionIndex, arguments);
}


PartialResult WARN_UNUSED_RETURN BBQJIT::addCall(FunctionSpaceIndex functionIndex, const TypeDefinition& signature, ArgumentList& arguments, ResultList& results, CallType callType)
{
    JIT_COMMENT(m_jit, "calling functionIndexSpace: ", functionIndex, ConditionalDump(!m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex), " functionIndex: ", functionIndex - m_info.importFunctionCount()));

    if (callType == CallType::TailCall) {
        emitTailCall(functionIndex, signature, arguments);
        return { };
    }

    const FunctionSignature& functionType = *signature.as<FunctionSignature>();
    CallInformation callInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    // Preserve caller-saved registers and other info
    prepareForExceptions();
    saveValuesAcrossCallAndPassArguments(arguments, callInfo, signature);

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        static_assert(sizeof(JSWebAssemblyInstance::ImportFunctionInfo) * maxImports < std::numeric_limits<int32_t>::max());
        RELEASE_ASSERT(JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndex) < std::numeric_limits<int32_t>::max());
        m_jit.call(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndex)), WasmEntryPtrTag);
    } else {
        // Record the callee so the callee knows to look for it in updateCallsitesToCallUs.
        ASSERT(m_info.toCodeIndex(functionIndex) < m_info.internalFunctionCount());
        m_directCallees.testAndSet(m_info.toCodeIndex(functionIndex));
        // Emit the call.
        Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;
        auto calleeMove = m_jit.storeWasmCalleeCalleePatchable();
        CCallHelpers::Call call = m_jit.threadSafePatchableNearCall();

        m_jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex, calleeMove] (LinkBuffer& linkBuffer) {
            unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex, linkBuffer.locationOf<WasmEntryPtrTag>(calleeMove) });
        });
    }

    // Push return value(s) onto the expression stack
    returnValuesFromCall(results, functionType, callInfo);

    // Our callee could have tail called someone else and changed SP so we need to restore it. Do this after restoring our results so we don't lose them.
    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), wasmScratchGPR));
#if CPU(ARM_THUMB2)
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif

    if (m_info.callCanClobberInstance(functionIndex) || m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex))
        restoreWebAssemblyGlobalStateAfterWasmCall();

    LOG_INSTRUCTION("Call", functionIndex, arguments, "=> ", results);

    return { };
}

void BBQJIT::emitIndirectCall(const char* opcode, const Value& callee, GPRReg calleeInstance, GPRReg calleeCode, const TypeDefinition& signature, ArgumentList& arguments, ResultList& results)
{
    ASSERT(!RegisterSetBuilder::argumentGPRs().contains(calleeCode, IgnoreVectors));

    const auto& callingConvention = wasmCallingConvention();
    CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    // Do a context switch if needed.
    Jump isSameInstanceBefore = m_jit.branchPtr(RelationalCondition::Equal, calleeInstance, GPRInfo::wasmContextInstancePointer);
    m_jit.move(calleeInstance, GPRInfo::wasmContextInstancePointer);
#if USE(JSVALUE64)
    loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
#endif
    isSameInstanceBefore.link(&m_jit);

    m_jit.loadPtr(Address(calleeCode), calleeCode);
    prepareForExceptions();
    saveValuesAcrossCallAndPassArguments(arguments, wasmCalleeInfo, signature); // Keep in mind that this clobbers wasmScratchGPR and wasmScratchFPR.

    // Why can we still call calleeCode after saveValuesAcrossCallAndPassArguments? CalleeCode is a scratch and not any argument GPR.
    m_jit.call(calleeCode, WasmEntryPtrTag);
    returnValuesFromCall(results, *signature.as<FunctionSignature>(), wasmCalleeInfo);

    // Our callee could have tail called someone else and changed SP so we need to restore it. Do this after restoring our results so we don't lose them.
    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), wasmScratchGPR));
#if CPU(ARM_THUMB2)
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif

    restoreWebAssemblyGlobalStateAfterWasmCall();

    LOG_INSTRUCTION(opcode, callee, arguments, "=> ", results);
}

void BBQJIT::emitIndirectTailCall(const char* opcode, const Value& callee, GPRReg calleeInstance, GPRReg calleeCode, const TypeDefinition& signature, ArgumentList& arguments)
{
    ASSERT(!RegisterSetBuilder::argumentGPRs().contains(calleeCode, IgnoreVectors));
    m_jit.loadPtr(Address(calleeCode), calleeCode);

    // Do a context switch if needed.
    Jump isSameInstanceBefore = m_jit.branchPtr(RelationalCondition::Equal, calleeInstance, GPRInfo::wasmContextInstancePointer);
    m_jit.move(calleeInstance, GPRInfo::wasmContextInstancePointer);
#if USE(JSVALUE64)
    loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
#endif
    isSameInstanceBefore.link(&m_jit);
    // calleeInstance is dead now.

    const auto& callingConvention = wasmCallingConvention();
    CallInformation callInfo = callingConvention.callInformationFor(signature, CallRole::Callee);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
    // Do this to ensure we don't write past SP.
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex).expand();
    CallInformation wasmCallerInfo = callingConvention.callInformationFor(callerTypeDefinition, CallRole::Callee);
    Checked<int32_t> callerStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCallerInfo.headerAndArgumentStackSizeInBytes);
    Checked<int32_t> tailCallStackOffsetFromFP = callerStackSize - calleeStackSize;
    ASSERT(callInfo.results.size() == wasmCallerInfo.results.size());
    ASSERT(arguments.size() == callInfo.params.size());

    ArgumentList resolvedArguments;
    const unsigned calleeArgument = 1;
    resolvedArguments.reserveInitialCapacity(arguments.size() + calleeArgument + isX86() * 2);
    Vector<Location, 8> parameterLocations;
    parameterLocations.reserveInitialCapacity(arguments.size() + calleeArgument + isX86() * 2);

    // It's ok if we clobber our Wasm::Callee at this point since we can't hit a GC safepoint / throw an exception until we've tail called into the callee.
    // FIXME: We should just have addCallIndirect put this in the right place to begin with.
    resolvedArguments.append(Value::pinned(TypeKind::I64, Location::fromStackArgument(CCallHelpers::addressOfCalleeCalleeFromCallerPerspective(0).offset)));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP + Checked<int>(CallFrameSlot::callee * sizeof(Register))));

    // Save the old Frame Pointer for later and make sure the return address gets saved to its canonical location.
#if CPU(X86_64)
    // There are no remaining non-argument non-preserved gprs left on X86_64 so we have to shuffle FP to a temp slot.
    resolvedArguments.append(Value::pinned(pointerType(), Location::fromStack(0)));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP));

    resolvedArguments.append(Value::pinned(pointerType(), Location::fromStack(sizeof(Register))));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP + Checked<int>(sizeof(Register))));
#elif CPU(ARM64) || CPU(ARM_THUMB2)
    auto preserved = callingConvention.argumentGPRs();
    preserved.add(calleeCode, IgnoreVectors);
    if constexpr (isARM64E())
        preserved.add(callingConvention.prologueScratchGPRs[0], IgnoreVectors);
    ScratchScope<1, 0> scratches(*this, WTFMove(preserved));
    GPRReg callerFramePointer = scratches.gpr(0);
    m_jit.loadPairPtr(MacroAssembler::framePointerRegister, callerFramePointer, MacroAssembler::linkRegister);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif

    for (unsigned i = 0; i < arguments.size(); i ++) {
        if (arguments[i].isConst())
            resolvedArguments.append(arguments[i]);
        else
            resolvedArguments.append(Value::pinned(arguments[i].type(), locationOf(arguments[i])));

        // This isn't really needed but it's nice to have good book keeping.
        consume(arguments[i]);
    }

    for (size_t i = 0; i < callInfo.params.size(); ++i) {
        auto param = callInfo.params[i];
        switch (param.location.kind()) {
        case ValueLocation::Kind::GPRRegister:
        case ValueLocation::Kind::FPRRegister: {
            auto type = signature.as<FunctionSignature>()->argumentType(i);
            parameterLocations.append(Location::fromArgumentLocation(param, type.kind));
            break;
        }
        case ValueLocation::Kind::StackArgument:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case ValueLocation::Kind::Stack:
            parameterLocations.append(Location::fromStack(param.location.offsetFromFP() + tailCallStackOffsetFromFP));
            break;
        }
    }

    emitShuffle(resolvedArguments, parameterLocations);

#if CPU(ARM64E)
    JIT_COMMENT(m_jit, "Untag our return PC");
    // prologue scratch registers should be free as we already moved the arguments into place.
    GPRReg scratch = callingConvention.prologueScratchGPRs[0];
    m_jit.addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), MacroAssembler::framePointerRegister, scratch);
    m_jit.untagPtr(scratch, ARM64Registers::lr);
    m_jit.validateUntaggedPtr(ARM64Registers::lr, scratch);
#endif

    // Fix SP and FP
#if CPU(X86_64)
    m_jit.loadPtr(Address(MacroAssembler::framePointerRegister, tailCallStackOffsetFromFP), wasmScratchGPR);
    m_jit.addPtr(TrustedImm32(tailCallStackOffsetFromFP + Checked<int>(sizeof(Register))), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    m_jit.move(wasmScratchGPR, MacroAssembler::framePointerRegister);
#elif CPU(ARM64) || CPU(ARM_THUMB2)
    m_jit.addPtr(TrustedImm32(tailCallStackOffsetFromFP + Checked<int>(sizeof(CallerFrameAndPC))), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    m_jit.move(callerFramePointer, MacroAssembler::framePointerRegister);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif

    m_jit.farJump(calleeCode, WasmEntryPtrTag);
    LOG_INSTRUCTION(opcode, callee, arguments);
}

void BBQJIT::addRTTSlowPathJump(TypeIndex signature, GPRReg calleeRTT)
{
    auto jump = m_jit.jump();
    auto returnLabel = m_jit.label();
    m_rttSlowPathJumps.append({ jump, returnLabel, signature, calleeRTT });
}

void BBQJIT::emitSlowPathRTTCheck(MacroAssembler::Label returnLabel, TypeIndex typeIndex, GPRReg calleeRTT)
{
    ASSERT(Options::useWasmGC());

    auto signatureRTT = TypeInformation::getCanonicalRTT(typeIndex);
    GPRReg rttSize = wasmScratchGPR;
    m_jit.loadPtr(Address(calleeRTT, FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfRTT()), calleeRTT);
    m_jit.load32(Address(calleeRTT, RTT::offsetOfDisplaySize()), rttSize);

    auto notGreaterThanZero = m_jit.branch32(CCallHelpers::BelowOrEqual, rttSize, TrustedImm32(0));

    // Check the parent pointer in the RTT display against the signature pointer we have.
    bool parentRTTHasEntries = signatureRTT->displaySize() > 0;
    GPRReg index = rttSize;
    auto scale = static_cast<CCallHelpers::Scale>(std::bit_width(sizeof(uintptr_t) - 1));
    auto rttBaseIndex = CCallHelpers::BaseIndex(calleeRTT, index, scale, RTT::offsetOfPayload());
    MacroAssembler::Jump displaySmallerThanParent;
    if (parentRTTHasEntries)
        displaySmallerThanParent = m_jit.branch32(CCallHelpers::BelowOrEqual, rttSize, TrustedImm32(signatureRTT->displaySize()));
    m_jit.sub32(TrustedImm32(1 + (parentRTTHasEntries ? signatureRTT->displaySize() : 0)), index);
    m_jit.loadPtr(rttBaseIndex, calleeRTT);
    auto rttEqual = m_jit.branchPtr(CCallHelpers::Equal, calleeRTT, TrustedImmPtr(signatureRTT.get()));
    rttEqual.linkTo(returnLabel, &m_jit);

    notGreaterThanZero.link(&m_jit);
    if (displaySmallerThanParent.isSet())
        displaySmallerThanParent.link(&m_jit);

    emitThrowException(ExceptionType::BadSignature);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCallIndirect(unsigned tableIndex, const TypeDefinition& originalSignature, ArgumentList& args, ResultList& results, CallType callType)
{
    Value calleeIndex = args.takeLast();
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    Location calleeIndexLocation;
    GPRReg calleeInstance;
    GPRReg calleeCode;
    GPRReg calleeRTT;

    {
        ScratchScope<1, 0> calleeCodeScratch(*this, RegisterSetBuilder::argumentGPRs());
        calleeCode = calleeCodeScratch.gpr(0);
        calleeCodeScratch.unbindPreserved();

        {
            ScratchScope<2, 0> scratches(*this);

            if (calleeIndex.isConst())
                emitMoveConst(calleeIndex, calleeIndexLocation = Location::fromGPR(scratches.gpr(1)));
            else
                calleeIndexLocation = loadIfNecessary(calleeIndex);

            GPRReg callableFunctionBufferLength = calleeCode;
            GPRReg callableFunctionBuffer = scratches.gpr(0);

            ASSERT(tableIndex < m_info.tableCount());

            int numImportFunctions = m_info.importFunctionCount();
            m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfTablePtr(numImportFunctions, tableIndex)), callableFunctionBufferLength);
            auto& tableInformation = m_info.table(tableIndex);
            if (tableInformation.maximum() && tableInformation.maximum().value() == tableInformation.initial()) {
                if (!tableInformation.isImport())
                    m_jit.addPtr(TrustedImm32(FuncRefTable::offsetOfFunctionsForFixedSizedTable()), callableFunctionBufferLength, callableFunctionBuffer);
                else
                    m_jit.loadPtr(Address(callableFunctionBufferLength, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
                m_jit.move(TrustedImm32(tableInformation.initial()), callableFunctionBufferLength);
            } else {
                m_jit.loadPtr(Address(callableFunctionBufferLength, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
                m_jit.load32(Address(callableFunctionBufferLength, FuncRefTable::offsetOfLength()), callableFunctionBufferLength);
            }
            ASSERT(calleeIndexLocation.isGPR());

            JIT_COMMENT(m_jit, "Check the index we are looking for is valid");
            throwExceptionIf(ExceptionType::OutOfBoundsCallIndirect, m_jit.branch32(RelationalCondition::AboveOrEqual, calleeIndexLocation.asGPR(), callableFunctionBufferLength));

            // Neither callableFunctionBuffer nor callableFunctionBufferLength are used before any of these
            // are def'd below, so we can reuse the registers and save some pressure.
            calleeInstance = scratches.gpr(0);
            calleeRTT = scratches.gpr(1);

            static_assert(sizeof(TypeIndex) == sizeof(void*));
            GPRReg calleeSignatureIndex = wasmScratchGPR;

            JIT_COMMENT(m_jit, "Compute the offset in the table index space we are looking for");
            if constexpr (hasOneBitSet(sizeof(FuncRefTable::Function))) {
                m_jit.zeroExtend32ToWord(calleeIndexLocation.asGPR(), calleeIndexLocation.asGPR());
#if CPU(ARM64)
                m_jit.addLeftShift64(callableFunctionBuffer, calleeIndexLocation.asGPR(), TrustedImm32(getLSBSet(sizeof(FuncRefTable::Function))), calleeSignatureIndex);
#elif CPU(ARM)
                m_jit.lshift32(calleeIndexLocation.asGPR(), TrustedImm32(getLSBSet(sizeof(FuncRefTable::Function))), calleeSignatureIndex);
                m_jit.addPtr(callableFunctionBuffer, calleeSignatureIndex);
#else
                m_jit.lshift64(calleeIndexLocation.asGPR(), TrustedImm32(getLSBSet(sizeof(FuncRefTable::Function))), calleeSignatureIndex);
                m_jit.addPtr(callableFunctionBuffer, calleeSignatureIndex);
#endif
            } else {
                m_jit.move(TrustedImmPtr(sizeof(FuncRefTable::Function)), calleeSignatureIndex);
#if CPU(ARM64)
                m_jit.multiplyAddZeroExtend32(calleeIndexLocation.asGPR(), calleeSignatureIndex, callableFunctionBuffer, calleeSignatureIndex);
#elif CPU(ARM)
                m_jit.mul32(calleeIndexLocation.asGPR(), calleeSignatureIndex);
                m_jit.addPtr(callableFunctionBuffer, calleeSignatureIndex);
#else
                m_jit.zeroExtend32ToWord(calleeIndexLocation.asGPR(), calleeIndexLocation.asGPR());
                m_jit.mul64(calleeIndexLocation.asGPR(), calleeSignatureIndex);
                m_jit.addPtr(callableFunctionBuffer, calleeSignatureIndex);
#endif
            }
            consume(calleeIndex);

            // FIXME: This seems wasteful to do two checks just for a nicer error message.
            // We should move just to use a single branch and then figure out what
            // error to use in the exception handler.

            {
                auto calleeTmp = calleeInstance;
                m_jit.loadPtr(Address(calleeSignatureIndex, FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfBoxedWasmCalleeLoadLocation()), calleeTmp);
                m_jit.loadPtr(Address(calleeTmp), calleeTmp);
                m_jit.storeWasmCalleeCallee(calleeTmp);
            }

            m_jit.loadPtr(Address(calleeSignatureIndex, FuncRefTable::Function::offsetOfInstance()), calleeInstance);
            static_assert(static_cast<ptrdiff_t>(WasmToWasmImportableFunction::offsetOfSignatureIndex() + sizeof(void*)) == WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation());

            // Save the table entry in calleeRTT if needed for the subtype check.
            bool needsSubtypeCheck = Options::useWasmGC() && !originalSignature.isFinalType();
            if (needsSubtypeCheck)
                m_jit.move(calleeSignatureIndex, calleeRTT);

            m_jit.loadPairPtr(calleeSignatureIndex, TrustedImm32(FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfSignatureIndex()), calleeSignatureIndex, calleeCode);

            throwExceptionIf(ExceptionType::NullTableEntry, m_jit.branchTestPtr(ResultCondition::Zero, calleeSignatureIndex, calleeSignatureIndex));
            auto indexEqual = m_jit.branchPtr(CCallHelpers::Equal, calleeSignatureIndex, TrustedImmPtr(TypeInformation::get(originalSignature)));

            if (needsSubtypeCheck)
                addRTTSlowPathJump(originalSignature.index(), calleeRTT);
            else
                emitThrowException(ExceptionType::BadSignature);

            indexEqual.link(&m_jit);
        }
    }

    JIT_COMMENT(m_jit, "Finished loading callee code");
    if (callType == CallType::Call)
        emitIndirectCall("CallIndirect", calleeIndex, calleeInstance, calleeCode, signature, args, results);
    else
        emitIndirectTailCall("ReturnCallIndirect", calleeIndex, calleeInstance, calleeCode, signature, args);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addUnreachable()
{
    LOG_INSTRUCTION("Unreachable");
    emitThrowException(ExceptionType::Unreachable);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCrash()
{
    m_jit.breakpoint();
    return { };
}

ALWAYS_INLINE void BBQJIT::willParseOpcode()
{
    m_pcToCodeOriginMapBuilder.appendItem(m_jit.label(), CodeOrigin(BytecodeIndex(m_parser->currentOpcodeStartingOffset())));
    if (UNLIKELY(m_disassembler)) {
        OpType currentOpcode = m_parser->currentOpcode();
        switch (currentOpcode) {
        case OpType::Ext1:
        case OpType::ExtGC:
        case OpType::ExtAtomic:
        case OpType::ExtSIMD:
            return; // We'll handle these once we know the extended opcode too.
        default:
            break;
        }
        m_disassembler->setOpcode(m_jit.label(), PrefixedOpcode(m_parser->currentOpcode()), m_parser->currentOpcodeStartingOffset());
    }
}

ALWAYS_INLINE void BBQJIT::willParseExtendedOpcode()
{
    if (UNLIKELY(m_disassembler)) {
        OpType prefix = m_parser->currentOpcode();
        uint32_t opcode = m_parser->currentExtendedOpcode();
        m_disassembler->setOpcode(m_jit.label(), PrefixedOpcode(prefix, opcode), m_parser->currentOpcodeStartingOffset());
    }
}

ALWAYS_INLINE void BBQJIT::didParseOpcode()
{
}

// SIMD

bool BBQJIT::usesSIMD()
{
    return m_usesSIMD;
}

void BBQJIT::dump(const ControlStack&, const Stack*) { }
void BBQJIT::didFinishParsingLocals() { }
void BBQJIT::didPopValueFromStack(ExpressionType, ASCIILiteral) { }

void BBQJIT::finalize()
{
    if (UNLIKELY(m_disassembler))
        m_disassembler->setEndOfCode(m_jit.label());
}

Vector<UnlinkedHandlerInfo>&& BBQJIT::takeExceptionHandlers()
{
    return WTFMove(m_exceptionHandlers);
}

FixedBitVector&& BBQJIT::takeDirectCallees()
{
    return WTFMove(m_directCallees);
}

Vector<CCallHelpers::Label>&& BBQJIT::takeCatchEntrypoints()
{
    return WTFMove(m_catchEntrypoints);
}

Box<PCToCodeOriginMapBuilder> BBQJIT::takePCToCodeOriginMapBuilder()
{
    if (m_pcToCodeOriginMapBuilder.didBuildMapping())
        return Box<PCToCodeOriginMapBuilder>::create(WTFMove(m_pcToCodeOriginMapBuilder));
    return nullptr;
}

std::unique_ptr<BBQDisassembler> BBQJIT::takeDisassembler()
{
    return WTFMove(m_disassembler);
}

bool BBQJIT::isScratch(Location loc)
{
    return (loc.isGPR() && loc.asGPR() == wasmScratchGPR) || (loc.isFPR() && loc.asFPR() == wasmScratchFPR);
}

void BBQJIT::emitStore(Value src, Location dst)
{
    if (src.isConst())
        return emitStoreConst(src, dst);

    LOG_INSTRUCTION("Store", src, RESULT(dst));
    emitStore(src.type(), locationOf(src), dst);
}

void BBQJIT::emitMoveMemory(Value src, Location dst)
{
    LOG_INSTRUCTION("Move", src, RESULT(dst));
    emitMoveMemory(src.type(), locationOf(src), dst);
}

void BBQJIT::emitMoveRegister(Value src, Location dst)
{
    if (!isScratch(locationOf(src)) && !isScratch(dst))
        LOG_INSTRUCTION("Move", src, RESULT(dst));

    emitMoveRegister(src.type(), locationOf(src), dst);
}

void BBQJIT::emitLoad(Value src, Location dst)
{
    if (!isScratch(dst))
        LOG_INSTRUCTION("Load", src, RESULT(dst));

    emitLoad(src.type(), locationOf(src), dst);
}

void BBQJIT::emitMove(TypeKind type, Location src, Location dst)
{
    if (src.isRegister()) {
        if (dst.isMemory())
            emitStore(type, src, dst);
        else
            emitMoveRegister(type, src, dst);
    } else {
        if (dst.isMemory())
            emitMoveMemory(type, src, dst);
        else
            emitLoad(type, src, dst);
    }
}

void BBQJIT::emitMove(Value src, Location dst)
{
    if (src.isConst()) {
        if (dst.isMemory())
            emitStoreConst(src, dst);
        else
            emitMoveConst(src, dst);
    } else {
        Location srcLocation = locationOf(src);
        emitMove(src.type(), srcLocation, dst);
    }
}

template<size_t N, typename OverflowHandler>
void BBQJIT::emitShuffle(Vector<Value, N, OverflowHandler>& srcVector, Vector<Location, N, OverflowHandler>& dstVector)
{
    ASSERT(srcVector.size() == dstVector.size());

#if ASSERT_ENABLED
    for (size_t i = 0; i < dstVector.size(); ++i) {
        for (size_t j = i + 1; j < dstVector.size(); ++j)
            ASSERT(dstVector[i] != dstVector[j]);
    }

    // This algorithm assumes at most one cycle: https://xavierleroy.org/publi/parallel-move.pdf
    for (size_t i = 0; i < srcVector.size(); ++i) {
        for (size_t j = i + 1; j < srcVector.size(); ++j) {
            ASSERT(srcVector[i].isConst() || srcVector[j].isConst()
                || locationOf(srcVector[i]) != locationOf(srcVector[j]));
        }
    }
#endif

    if (srcVector.size() == 1) {
        emitMove(srcVector[0], dstVector[0]);
        return;
    }

    // For multi-value return, a parallel move might be necessary. This is comparatively complex
    // and slow, so we limit it to this slow path.
    Vector<ShuffleStatus, N, OverflowHandler> statusVector(srcVector.size(), ShuffleStatus::ToMove);
    for (unsigned i = 0; i < srcVector.size(); i ++) {
        if (statusVector[i] == ShuffleStatus::ToMove)
            emitShuffleMove(srcVector, dstVector, statusVector, i);
    }
}

ControlData& BBQJIT::currentControlData()
{
    return m_parser->controlStack().last().controlData;
}

void BBQJIT::setLRUKey(Location location, LocalOrTempIndex key)
{
    if (location.isGPR()) {
        m_gprLRU.increaseKey(location.asGPR(), key);
    } else if (location.isFPR()) {
        m_fprLRU.increaseKey(location.asFPR(), key);
    } else if (location.isGPR2()) {
        m_gprLRU.increaseKey(location.asGPRhi(), key);
        m_gprLRU.increaseKey(location.asGPRlo(), key);
    }
}

void BBQJIT::increaseKey(Location location)
{
    setLRUKey(location, m_lastUseTimestamp ++);
}

Location BBQJIT::bind(Value value)
{
    if (value.isPinned())
        return value.asPinned();

    Location reg = allocateRegister(value);
    increaseKey(reg);
    return bind(value, reg);
}

Location BBQJIT::allocate(Value value)
{
    return BBQJIT::allocateWithHint(value, Location::none());
}

Location BBQJIT::allocateWithHint(Value value, Location hint)
{
    if (value.isPinned())
        return value.asPinned();

    // It's possible, in some circumstances, that the value in question was already assigned to a register.
    // The most common example of this is returned values (that use registers from the calling convention) or
    // values passed between control blocks. In these cases, we just leave it in its register unmoved.
    Location existingLocation = locationOf(value);
    if (existingLocation.isRegister()) {
        ASSERT(value.isFloat() == existingLocation.isFPR());
        return existingLocation;
    }

    Location reg = hint;
    if (reg.kind() == Location::None
        || value.isFloat() != reg.isFPR()
        || (reg.isGPR() && !m_gprSet.contains(reg.asGPR(), IgnoreVectors))
        || (reg.isGPR2() && !typeNeedsGPR2(value.type()))
        || (reg.isGPR() && typeNeedsGPR2(value.type()))
        || (reg.isFPR() && !m_fprSet.contains(reg.asFPR(), Width::Width128)))
        reg = allocateRegister(value);
    increaseKey(reg);
    if (value.isLocal())
        currentControlData().touch(value.asLocal());
    if (UNLIKELY(Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tAllocated ", value, " with type ", makeString(value.type()), " to ", reg);
    return bind(value, reg);
}

Location BBQJIT::locationOfWithoutBinding(Value value)
{
    // Used internally in bind() to avoid infinite recursion.
    if (value.isPinned())
        return value.asPinned();
    if (value.isLocal()) {
        ASSERT(value.asLocal() < m_locals.size());
        return m_locals[value.asLocal()];
    }
    if (value.isTemp()) {
        if (value.asTemp() >= m_temps.size())
            return Location::none();
        return m_temps[value.asTemp()];
    }
    return Location::none();
}

Location BBQJIT::locationOf(Value value)
{
    if (value.isTemp()) {
        if (value.asTemp() >= m_temps.size() || m_temps[value.asTemp()].isNone())
            bind(value, canonicalSlot(value));
        return m_temps[value.asTemp()];
    }
    return locationOfWithoutBinding(value);
}

Location BBQJIT::loadIfNecessary(Value value)
{
    ASSERT(!value.isPinned()); // We should not load or move pinned values.
    ASSERT(!value.isConst()); // We should not be materializing things we know are constants.
    if (UNLIKELY(Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tLoading value ", value, " if necessary");
    Location loc = locationOf(value);
    if (loc.isMemory()) {
        if (UNLIKELY(Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tLoading local ", value, " to ", loc);
        loc = allocateRegister(value); // Find a register to store this value. Might spill older values if we run out.
        emitLoad(value, loc); // Generate the load instructions to move the value into the register.
        increaseKey(loc);
        if (value.isLocal())
            currentControlData().touch(value.asLocal());
        bind(value, loc); // Bind the value to the register in the register/local/temp bindings.
    } else
        increaseKey(loc);
    ASSERT(loc.isRegister());
    return loc;
}

void BBQJIT::consume(Value value)
{
    // Called whenever a value is popped from the expression stack. Mostly, we
    // use this to release the registers temporaries are bound to.
    Location location = locationOf(value);
    if (value.isTemp() && location != canonicalSlot(value))
        unbind(value, location);
}

Location BBQJIT::allocateRegister(TypeKind type)
{
    if (isFloatingPointType(type))
        return Location::fromFPR(m_fprSet.isEmpty() ? evictFPR() : nextFPR());
    if (typeNeedsGPR2(type))
        return allocateRegisterPair();
    return Location::fromGPR(m_gprSet.isEmpty() ? evictGPR() : nextGPR());
}

Location BBQJIT::allocateRegister(Value value)
{
    return allocateRegister(value.type());
}

Location BBQJIT::bind(Value value, Location loc)
{
    // Bind a value to a location. Doesn't touch the LRU, but updates the register set
    // and local/temp tables accordingly.

    // Return early if we are already bound to the chosen location. Mostly this avoids
    // spurious assertion failures.
    Location currentLocation = locationOfWithoutBinding(value);
    if (currentLocation == loc)
        return currentLocation;

    // Technically, it could be possible to bind from a register to another register. But
    // this risks (and is usually caused by) messing up the allocator state. So we check
    // for it here.
    ASSERT(!currentLocation.isRegister());

    if (loc.isRegister()) {
        if (value.isFloat()) {
            ASSERT(m_fprSet.contains(loc.asFPR(), Width::Width128));
            m_fprSet.remove(loc.asFPR());
            m_fprBindings[loc.asFPR()] = RegisterBinding::fromValue(value);
        } else if (loc.isGPR2()) {
            ASSERT(m_gprBindings[loc.asGPRlo()].isNone());
            ASSERT(m_gprBindings[loc.asGPRhi()].isNone());
            ASSERT(m_gprSet.contains(loc.asGPRlo(), IgnoreVectors));
            ASSERT(m_gprSet.contains(loc.asGPRhi(), IgnoreVectors));
            m_gprSet.remove(loc.asGPRlo());
            m_gprSet.remove(loc.asGPRhi());
            auto binding = RegisterBinding::fromValue(value);
            m_gprBindings[loc.asGPRlo()] = binding;
            m_gprBindings[loc.asGPRhi()] = binding;
        } else {
            ASSERT(m_gprSet.contains(loc.asGPR(), IgnoreVectors));
            m_gprSet.remove(loc.asGPR());
            m_gprBindings[loc.asGPR()] = RegisterBinding::fromValue(value);
        }
    }
    if (value.isLocal())
        m_locals[value.asLocal()] = loc;
    else if (value.isTemp()) {
        if (m_temps.size() <= value.asTemp())
            m_temps.grow(value.asTemp() + 1);
        m_temps[value.asTemp()] = loc;
    }

    if (UNLIKELY(Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tBound value ", value, " to ", loc);

    return loc;
}

void BBQJIT::unbind(Value value, Location loc)
{
    // Unbind a value from a location. Doesn't touch the LRU, but updates the register set
    // and local/temp tables accordingly.
    if (loc.isFPR()) {
        ASSERT(m_validFPRs.contains(loc.asFPR(), Width::Width128));
        m_fprSet.add(loc.asFPR(), Width::Width128);
        m_fprBindings[loc.asFPR()] = RegisterBinding::none();
    } else if (loc.isGPR()) {
        ASSERT(m_validGPRs.contains(loc.asGPR(), IgnoreVectors));
        m_gprSet.add(loc.asGPR(), IgnoreVectors);
        m_gprBindings[loc.asGPR()] = RegisterBinding::none();
    } else if (loc.isGPR2()) {
        m_gprSet.add(loc.asGPRlo(), IgnoreVectors);
        m_gprSet.add(loc.asGPRhi(), IgnoreVectors);
        m_gprBindings[loc.asGPRlo()] = RegisterBinding::none();
        m_gprBindings[loc.asGPRhi()] = RegisterBinding::none();
    }
    if (value.isLocal())
        m_locals[value.asLocal()] = m_localSlots[value.asLocal()];
    else if (value.isTemp())
        m_temps[value.asTemp()] = Location::none();

    if (UNLIKELY(Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tUnbound value ", value, " from ", loc);
}

GPRReg BBQJIT::nextGPR()
{
    auto next = m_gprSet.begin();
    ASSERT(next != m_gprSet.end());
    GPRReg reg = (*next).gpr();
    ASSERT(m_gprBindings[reg].m_kind == RegisterBinding::None);
    return reg;
}

FPRReg BBQJIT::nextFPR()
{
    auto next = m_fprSet.begin();
    ASSERT(next != m_fprSet.end());
    FPRReg reg = (*next).fpr();
    ASSERT(m_fprBindings[reg].m_kind == RegisterBinding::None);
    return reg;
}

GPRReg BBQJIT::evictGPR()
{
    auto lruGPR = m_gprLRU.findMin();
    auto lruBinding = m_gprBindings[lruGPR];

    if (UNLIKELY(Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tEvicting GPR ", MacroAssembler::gprName(lruGPR), " currently bound to ", lruBinding);
    flushValue(lruBinding.toValue());

    ASSERT(m_gprSet.contains(lruGPR, IgnoreVectors));
    ASSERT(m_gprBindings[lruGPR].m_kind == RegisterBinding::None);
    return lruGPR;
}

FPRReg BBQJIT::evictFPR()
{
    auto lruFPR = m_fprLRU.findMin();
    auto lruBinding = m_fprBindings[lruFPR];

    if (UNLIKELY(Options::verboseBBQJITAllocation()))
        dataLogLn("BBQ\tEvicting FPR ", MacroAssembler::fprName(lruFPR), " currently bound to ", lruBinding);
    flushValue(lruBinding.toValue());

    ASSERT(m_fprSet.contains(lruFPR, Width::Width128));
    ASSERT(m_fprBindings[lruFPR].m_kind == RegisterBinding::None);
    return lruFPR;
}

// We use this to free up specific registers that might get clobbered by an instruction.

void BBQJIT::clobber(GPRReg gpr)
{
    if (m_validGPRs.contains(gpr, IgnoreVectors) && !m_gprSet.contains(gpr, IgnoreVectors)) {
        RegisterBinding& binding = m_gprBindings[gpr];
        if (UNLIKELY(Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tClobbering GPR ", MacroAssembler::gprName(gpr), " currently bound to ", binding);
        RELEASE_ASSERT(!binding.isNone() && !binding.isScratch()); // We could probably figure out how to handle this, but let's just crash if it happens for now.
        flushValue(binding.toValue());
    }
}

void BBQJIT::clobber(FPRReg fpr)
{
    if (m_validFPRs.contains(fpr, Width::Width128) && !m_fprSet.contains(fpr, Width::Width128)) {
        RegisterBinding& binding = m_fprBindings[fpr];
        if (UNLIKELY(Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tClobbering FPR ", MacroAssembler::fprName(fpr), " currently bound to ", binding);
        RELEASE_ASSERT(!binding.isNone() && !binding.isScratch()); // We could probably figure out how to handle this, but let's just crash if it happens for now.
        flushValue(binding.toValue());
    }
}

void BBQJIT::clobber(JSC::Reg reg)
{
    reg.isGPR() ? clobber(reg.gpr()) : clobber(reg.fpr());
}

Location BBQJIT::canonicalSlot(Value value)
{
    ASSERT(value.isLocal() || value.isTemp());
    if (value.isLocal())
        return m_localSlots[value.asLocal()];

    LocalOrTempIndex tempIndex = value.asTemp();
    int slotOffset = WTF::roundUpToMultipleOf<tempSlotSize>(m_localStorage) + (tempIndex + 1) * tempSlotSize;
    if (m_frameSize < slotOffset)
        m_frameSize = slotOffset;
    return Location::fromStack(-slotOffset);
}

Location BBQJIT::allocateStack(Value value)
{
    // Align stack for value size.
    m_frameSize = WTF::roundUpToMultipleOf(value.size(), m_frameSize);
    m_frameSize += value.size();
    return Location::fromStack(-m_frameSize);
}

} // namespace JSC::Wasm::BBQJITImpl

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileBBQ(CompilationContext& compilationContext, BBQCallee& callee, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, FunctionCodeIndex functionIndex, std::optional<bool> hasExceptionHandlers, unsigned loopIndexForOSREntry)
{
    CompilerTimingScope totalTime("BBQ"_s, "Total BBQ"_s);

    Thunks::singleton().stub(catchInWasmThunkGenerator);

    auto result = makeUnique<InternalFunction>();

    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

    BBQJIT irGenerator(*compilationContext.wasmEntrypointJIT, signature, callee, function, functionIndex, info, unlinkedWasmToWasmCalls, mode, result.get(), hasExceptionHandlers, loopIndexForOSREntry);
    FunctionParser<BBQJIT> parser(irGenerator, function.data, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    if (irGenerator.hasLoops())
        result->bbqSharedLoopEntrypoint = irGenerator.addLoopOSREntrypoint();

    irGenerator.finalize();
    auto checkSize = irGenerator.stackCheckSize();
    if (!checkSize)
        checkSize = stackCheckNotNeeded;
    callee.setStackCheckSize(checkSize);

    result->exceptionHandlers = irGenerator.takeExceptionHandlers();
    result->outgoingJITDirectCallees = irGenerator.takeDirectCallees();
    compilationContext.catchEntrypoints = irGenerator.takeCatchEntrypoints();
    compilationContext.pcToCodeOriginMapBuilder = irGenerator.takePCToCodeOriginMapBuilder();
    compilationContext.bbqDisassembler = irGenerator.takeDisassembler();

    return result;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
