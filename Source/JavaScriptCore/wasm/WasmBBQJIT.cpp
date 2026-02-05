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
#include "JSWebAssemblyArrayInlines.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyStruct.h"
#include "MacroAssembler.h"
#include "RegisterSet.h"
#include "WasmBBQDisassembler.h"
#include "WasmBaselineData.h"
#include "WasmCallProfile.h"
#include "WasmCallingConvention.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionParser.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmMemoryInformation.h"
#include "WasmMergedProfile.h"
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
    case TypeKind::Exnref:
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
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
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

bool RegisterBinding::operator==(RegisterBinding other) const
{
    if (m_kind != other.m_kind)
        return false;

    if (m_kind == None || m_kind == Scratch)
        return true;

    return m_index == other.m_index && m_type == other.m_type;
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
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

unsigned RegisterBinding::hash() const
{
    return pairIntHash(static_cast<unsigned>(m_kind), m_index);
}

ControlData::ControlData(BBQJIT& generator, BlockType blockType, BlockSignature&& signature, LocalOrTempIndex enclosedHeight, RegisterSet liveScratchGPRs = { }, RegisterSet liveScratchFPRs = { })
    : m_signature(WTF::move(signature))
    , m_blockType(blockType)
    , m_enclosedHeight(enclosedHeight)
{
    if (blockType == BlockType::TopLevel) {
        // For TopLevel, use the function's calling convention
        ASSERT(generator.m_functionSignature);
        CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*generator.m_functionSignature, CallRole::Callee);
        for (unsigned i = 0; i < m_signature.argumentCount(); ++i)
            m_argumentLocations.append(Location::fromArgumentLocation(wasmCallInfo.params[i], m_signature.argumentType(i).kind));
        for (unsigned i = 0; i < m_signature.returnCount(); ++i)
            m_resultLocations.append(Location::fromArgumentLocation(wasmCallInfo.results[i], m_signature.returnType(i).kind));
        return;
    }

    if (!isAnyCatch(*this)) {
        auto gprSetCopy = generator.validGPRs();
        auto fprSetCopy = generator.validFPRs();
        liveScratchGPRs.forEach([&] (auto r) { gprSetCopy.remove(r); });
        liveScratchFPRs.forEach([&] (auto r) { fprSetCopy.remove(r); });

        for (unsigned i = 0; i < m_signature.argumentCount(); ++i)
            m_argumentLocations.append(allocateArgumentOrResult(generator, m_signature.argumentType(i).kind, i, gprSetCopy, fprSetCopy));
    }

    auto gprSetCopy = generator.validGPRs();
    auto fprSetCopy = generator.validFPRs();
    for (unsigned i = 0; i < m_signature.returnCount(); ++i)
        m_resultLocations.append(allocateArgumentOrResult(generator, m_signature.returnType(i).kind, i, gprSetCopy, fprSetCopy));
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
    m_labels.append(WTF::move(label));
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

unsigned ControlData::implicitSlots() const
{
    return isAnyCatch(*this) ? 1 : 0;
}

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
const BlockSignature& ControlData::signature() const { return m_signature; }

FunctionArgCount ControlData::branchTargetArity() const
{
    if (blockType() == BlockType::Loop)
        return m_signature.argumentCount();
    return m_signature.returnCount();
}

Type ControlData::branchTargetType(unsigned i) const
{
    ASSERT(i < branchTargetArity());
    if (m_blockType == BlockType::Loop)
        return m_signature.argumentType(i);
    return m_signature.returnType(i);
}

Type ControlData::argumentType(unsigned i) const
{
    ASSERT(i < m_signature.argumentCount());
    return m_signature.argumentType(i);
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
    m_tryTableTargets = WTF::move(targets);
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

BBQJIT::BBQJIT(CompilationContext& compilationContext, const TypeDefinition& signature, Module& module, CalleeGroup& calleeGroup, IPIntCallee& profiledCallee, BBQCallee& callee, const FunctionData& function, FunctionCodeIndex functionIndex, const ModuleInformation& info, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, InternalFunction* compilation)
    : m_context(compilationContext)
    , m_jit(*compilationContext.wasmEntrypointJIT)
    , m_module(module)
    , m_calleeGroup(calleeGroup)
    , m_profiledCallee(profiledCallee)
    , m_callee(callee)
    , m_function(function)
    , m_functionSignature(signature.expand().as<FunctionSignature>())
    , m_functionIndex(functionIndex)
    , m_info(info)
    , m_mode(mode)
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_directCallees(m_info.internalFunctionCount())
    , m_lastUseTimestamp(0)
    , m_compilation(compilation)
    , m_pcToCodeOriginMapBuilder(Options::useSamplingProfiler())
    , m_profile(module.createMergedProfile(profiledCallee))
{
    RegisterSetBuilder gprSetBuilder = RegisterSetBuilder::allGPRs();
    gprSetBuilder.exclude(RegisterSetBuilder::specialRegisters());
    gprSetBuilder.exclude(RegisterSetBuilder::macroClobberedGPRs());
    gprSetBuilder.exclude(RegisterSetBuilder::wasmPinnedRegisters());
    gprSetBuilder.exclude(RegisterSetBuilder::bbqCalleeSaveRegisters());
    // FIXME: handle callee-saved registers better.
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

    ASCIILiteral logPrefix = Options::verboseBBQJITAllocation() ? "BBQ"_s : ASCIILiteral();
    m_gprAllocator.initialize(gprSetBuilder.buildAndValidate(), logPrefix);
    m_fprAllocator.initialize(fprSetBuilder.buildAndValidate(), logPrefix);
    m_callerSaveGPRs = callerSaveGprs.buildAndValidate();
    m_callerSaveFPRs = callerSaveFprs.buildAndValidate();
    m_callerSaves = callerSaveGprs.merge(callerSaveFprs).buildAndValidate();

    if (shouldDumpDisassemblyFor(CompilationMode::BBQMode)) [[unlikely]] {
        m_disassembler = makeUnique<BBQDisassembler>();
        m_disassembler->setStartOfCode(m_jit.label());
    }

    CallInformation callInfo = wasmCallingConvention().callInformationFor(signature.expand(), CallRole::Callee);

    // Allocate callee save register spaces.
    for (size_t i = 0, size = RegisterAtOffsetList::bbqCalleeSaveRegisters().registerCount(); i < size; ++i)
        allocateStack(Value::fromPointer(nullptr));

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
    m_localAndCalleeSaveStorage = m_frameSize; // All stack slots allocated so far are locals.
}

bool BBQJIT::canTierUpToOMG() const
{
    if (!Options::useOMGJIT())
        return false;

    if (!Options::useBBQTierUpChecks())
        return false;

    if (m_function.data.size() > Options::maximumOMGCandidateCost()) {
        dataLogLnIf(Options::verboseOSR(), m_callee, ": Too large to tier-up to OMG: size = ", m_function.data.size());
        return false;
    }
    return true;
}

void BBQJIT::emitIncrementCallProfileCount(unsigned callProfileIndex)
{
    m_jit.add32(TrustedImm32(1), CCallHelpers::Address(GPRInfo::jitDataRegister, safeCast<int32_t>(BaselineData::offsetOfData() + sizeof(CallProfile) * callProfileIndex + CallProfile::offsetOfCount())));
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
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
        result = Value::fromRef(type.kind, static_cast<EncodedJSValue>(value));
        LOG_INSTRUCTION("RefConst", makeString(type.kind), RESULT(result));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant type.\n");
        return Value::none();
    }

    if (Options::disableBBQConsts()) [[unlikely]] {
        Value stackResult = topValue(type.kind);
        emitStoreConst(result, canonicalSlot(stackResult));
        result = stackResult;
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
        m_localAndCalleeSaveStorage = m_frameSize;
        m_locals.append(m_localSlots.last());
        m_localTypes.append(type.kind);
    }
    return { };
}

// Tables

[[nodiscard]] PartialResult BBQJIT::addTableSet(unsigned tableIndex, Value index, Value value)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("TableSet", tableIndex, index, value);

    recordJumpToThrowException(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("TableInit", tableIndex, dstOffset, srcOffset, length);

    recordJumpToThrowException(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addElemDrop(unsigned elementIndex)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(elementIndex)
    };
    emitCCall(&operationWasmElemDrop, arguments);

    LOG_INSTRUCTION("ElemDrop", elementIndex);
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addTableSize(unsigned tableIndex, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addTableGrow(unsigned tableIndex, Value fill, Value delta, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addTableFill(unsigned tableIndex, Value offset, Value fill, Value count)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("TableFill", tableIndex, fill, offset, count);

    recordJumpToThrowException(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, Value dstOffset, Value srcOffset, Value length)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("TableCopy", dstTableIndex, srcTableIndex, dstOffset, srcOffset, length);

    recordJumpToThrowException(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

// Locals

[[nodiscard]] PartialResult BBQJIT::getLocal(uint32_t localIndex, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::setLocal(uint32_t localIndex, Value value)
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

[[nodiscard]] PartialResult BBQJIT::teeLocal(uint32_t localIndex, Value value, Value& result)
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

void BBQJIT::emitMutatorFence()
{
    if (isX86_64())
        return;

    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), wasmScratchGPR);
    Jump ok = m_jit.branchTest8(MacroAssembler::Zero, Address(wasmScratchGPR, VM::offsetOfHeapMutatorShouldBeFenced()));
    m_jit.storeFence();
    ok.link(m_jit);
}

// Memory

Address BBQJIT::materializePointer(Location pointerLocation, uint32_t uoffset)
{
    if (static_cast<uint64_t>(uoffset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, static_cast<int32_t>(uoffset), Width::Width128)) {
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), pointerLocation.asGPR());
        return Address(pointerLocation.asGPR());
    }
    return Address(pointerLocation.asGPR(), static_cast<int32_t>(uoffset));
}

[[nodiscard]] PartialResult BBQJIT::addGrowMemory(Value delta, Value& result)
{
    Vector<Value, 8> arguments = { instanceValue(), delta };
    result = topValue(TypeKind::I32);
    emitCCall(&operationGrowMemory, arguments, result);
    restoreWebAssemblyGlobalState();

    LOG_INSTRUCTION("GrowMemory", delta, RESULT(result));

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addCurrentMemory(Value& result)
{
    result = topValue(TypeKind::I32);
    Location resultLocation = allocate(result);
    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfCachedMemorySize()), wasmScratchGPR);

    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    m_jit.urshiftPtr(Imm32(shiftValue), wasmScratchGPR);
    m_jit.zeroExtend32ToWord(wasmScratchGPR, resultLocation.asGPR());

    LOG_INSTRUCTION("CurrentMemory", RESULT(result));

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addMemoryFill(Value dstAddress, Value targetValue, Value count)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    recordJumpToThrowException(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    LOG_INSTRUCTION("MemoryFill", dstAddress, targetValue, count);

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addMemoryCopy(Value dstAddress, Value srcAddress, Value count)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    recordJumpToThrowException(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    LOG_INSTRUCTION("MemoryCopy", dstAddress, srcAddress, count);

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addMemoryInit(unsigned dataSegmentIndex, Value dstAddress, Value srcAddress, Value length)
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    recordJumpToThrowException(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    LOG_INSTRUCTION("MemoryInit", dataSegmentIndex, dstAddress, srcAddress, length);

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addDataDrop(unsigned dataSegmentIndex)
{
    Vector<Value, 8> arguments = { instanceValue(), Value::fromI32(dataSegmentIndex) };
    emitCCall(&operationWasmDataDrop, arguments);

    LOG_INSTRUCTION("DataDrop", dataSegmentIndex);
    return { };
}

// Atomics

[[nodiscard]] PartialResult BBQJIT::atomicLoad(ExtAtomicOpType loadOp, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t uoffset)
{
    if (sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(loadOp))) [[unlikely]] {
        // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        result = valueType.isI64() ? Value::fromI64(0) : Value::fromI32(0);
    } else
        result = emitAtomicLoadOp(loadOp, valueType, emitCheckAndPreparePointer(pointer, uoffset, sizeOfAtomicOpMemoryAccess(loadOp)), uoffset);

    LOG_INSTRUCTION(makeString(loadOp), pointer, uoffset, RESULT(result));

    return { };
}

[[nodiscard]] PartialResult BBQJIT::atomicStore(ExtAtomicOpType storeOp, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(storeOp))) [[unlikely]] {
        // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        consume(value);
    } else
        emitAtomicStoreOp(storeOp, valueType, emitCheckAndPreparePointer(pointer, uoffset, sizeOfAtomicOpMemoryAccess(storeOp)), value, uoffset);

    LOG_INSTRUCTION(makeString(storeOp), pointer, uoffset, value, valueLocation);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(op))) [[unlikely]] {
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

[[nodiscard]] PartialResult BBQJIT::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (sumOverflows<uint32_t>(uoffset, sizeOfAtomicOpMemoryAccess(op))) [[unlikely]] {
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

[[nodiscard]] PartialResult BBQJIT::atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t uoffset)
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
    Location resultLocation = loadIfNecessary(result);

    LOG_INSTRUCTION(makeString(op), pointer, value, timeout, uoffset, RESULT(result));

    recordJumpToThrowException(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPR(), TrustedImm32(0)));
    return { };
}

[[nodiscard]] PartialResult BBQJIT::atomicNotify(ExtAtomicOpType op, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t uoffset)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        pointer,
        Value::fromI32(uoffset),
        count
    };
    result = topValue(TypeKind::I32);
    emitCCall(&operationMemoryAtomicNotify, arguments, result);
    Location resultLocation = loadIfNecessary(result);

    LOG_INSTRUCTION(makeString(op), pointer, count, uoffset, RESULT(result));

    recordJumpToThrowException(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPR(), TrustedImm32(0)));
    return { };
}

[[nodiscard]] PartialResult BBQJIT::atomicFence(ExtAtomicOpType, uint8_t)
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

[[nodiscard]] PartialResult BBQJIT::truncTrapping(OpType truncationOp, Value operand, Value& result, Type returnType, Type operandType)
{
    ScratchScope<0, 2> scratches(*this);

    Location operandLocation;
    if (operand.isConst()) {
        operandLocation = Location::fromFPR(wasmScratchFPR);
        emitMoveConst(operand, operandLocation);
    } else
        operandLocation = loadIfNecessary(operand);
    ASSERT(operandLocation.isRegister());

    consume(operand); // Allow temp operand location to be reused

    result = topValue(returnType.kind);
    Location resultLocation = allocate(result);
    TruncationKind kind = truncationKind(truncationOp);
    auto range = lookupTruncationRange(kind);
    auto minFloatConst = range.min;
    auto maxFloatConst = range.max;
    Location minFloat = Location::fromFPR(scratches.fpr(0));
    Location maxFloat = Location::fromFPR(scratches.fpr(1));

    // FIXME: Can we do better isel here? Two floating-point constant materializations for every
    // trunc seems costly.
    emitMoveConst(minFloatConst, minFloat);
    emitMoveConst(maxFloatConst, maxFloat);

    LOG_INSTRUCTION("TruncTrapping", operand, operandLocation, RESULT(result));

    DoubleCondition minCondition = range.closedLowerEndpoint ? DoubleCondition::DoubleLessThanOrUnordered : DoubleCondition::DoubleLessThanOrEqualOrUnordered;
    Jump belowMin = operandType == Types::F32
        ? m_jit.branchFloat(minCondition, operandLocation.asFPR(), minFloat.asFPR())
        : m_jit.branchDouble(minCondition, operandLocation.asFPR(), minFloat.asFPR());
    recordJumpToThrowException(ExceptionType::OutOfBoundsTrunc, belowMin);

    Jump aboveMax = operandType == Types::F32
        ? m_jit.branchFloat(DoubleCondition::DoubleGreaterThanOrEqualOrUnordered, operandLocation.asFPR(), maxFloat.asFPR())
        : m_jit.branchDouble(DoubleCondition::DoubleGreaterThanOrEqualOrUnordered, operandLocation.asFPR(), maxFloat.asFPR());
    recordJumpToThrowException(ExceptionType::OutOfBoundsTrunc, aboveMax);

    truncInBounds(kind, operandLocation, resultLocation, scratches.fpr(0), scratches.fpr(1));

    return { };
}

[[nodiscard]] PartialResult BBQJIT::truncSaturated(Ext1OpType truncationOp, Value operand, Value& result, Type returnType, Type operandType)
{
    ScratchScope<0, 2> scratches(*this);

    TruncationKind kind = truncationKind(truncationOp);
    auto range = lookupTruncationRange(kind);
    auto minFloatConst = range.min;
    auto maxFloatConst = range.max;
    Location minFloat = Location::fromFPR(scratches.fpr(0));
    Location maxFloat = Location::fromFPR(scratches.fpr(1));

    // FIXME: Can we do better isel here? Two floating-point constant materializations for every
    // trunc seems costly.
    emitMoveConst(minFloatConst, minFloat);
    emitMoveConst(maxFloatConst, maxFloat);

    // Determine min/max integer results for saturation.
    uint64_t minResult = 0;
    uint64_t maxResult = 0;
    switch (kind) {
    case TruncationKind::I32TruncF32S:
    case TruncationKind::I32TruncF64S:
        maxResult = std::bit_cast<uint32_t>(INT32_MAX);
        minResult = std::bit_cast<uint32_t>(INT32_MIN);
        break;
    case TruncationKind::I32TruncF32U:
    case TruncationKind::I32TruncF64U:
        maxResult = std::bit_cast<uint32_t>(UINT32_MAX);
        minResult = std::bit_cast<uint32_t>(0U);
        break;
    case TruncationKind::I64TruncF32S:
    case TruncationKind::I64TruncF64S:
        maxResult = std::bit_cast<uint64_t>(INT64_MAX);
        minResult = std::bit_cast<uint64_t>(INT64_MIN);
        break;
    case TruncationKind::I64TruncF32U:
    case TruncationKind::I64TruncF64U:
        maxResult = std::bit_cast<uint64_t>(UINT64_MAX);
        minResult = std::bit_cast<uint64_t>(0ULL);
        break;
    }

    Location operandLocation;
    if (operand.isConst()) {
        operandLocation = Location::fromFPR(wasmScratchFPR);
        emitMoveConst(operand, operandLocation);
    } else
        operandLocation = loadIfNecessary(operand);
    ASSERT(operandLocation.isRegister());

    consume(operand); // Allow temp operand location to be reused

    result = topValue(returnType.kind);
    Location resultLocation = allocate(result);

    LOG_INSTRUCTION("TruncSaturated", operand, operandLocation, RESULT(result));

    Jump lowerThanMin = operandType == Types::F32
        ? m_jit.branchFloat(DoubleCondition::DoubleLessThanOrEqualOrUnordered, operandLocation.asFPR(), minFloat.asFPR())
        : m_jit.branchDouble(DoubleCondition::DoubleLessThanOrEqualOrUnordered, operandLocation.asFPR(), minFloat.asFPR());
    Jump higherThanMax = operandType == Types::F32
        ? m_jit.branchFloat(DoubleCondition::DoubleGreaterThanOrEqualOrUnordered, operandLocation.asFPR(), maxFloat.asFPR())
        : m_jit.branchDouble(DoubleCondition::DoubleGreaterThanOrEqualOrUnordered, operandLocation.asFPR(), maxFloat.asFPR());

    // In-bounds case. Emit normal truncation instructions.
    truncInBounds(kind, operandLocation, resultLocation, scratches.fpr(0), scratches.fpr(1));

    Jump afterInBounds = m_jit.jump();

    // Below-minimum case.
    lowerThanMin.link(&m_jit);

    // As an optimization, if the min result is 0; we can unconditionally return
    // that if the above-minimum-range check fails; otherwise, we need to check
    // for NaN since it also will fail the above-minimum-range-check
    if (!minResult) {
        // Use emitMoveConst to handle 32-bit and 64-bit uniformly.
        emitMoveConst(returnType == Types::I32 ? Value::fromI32(0) : Value::fromI64(0), resultLocation);
    } else {
        Jump isNotNaN = operandType == Types::F32
            ? m_jit.branchFloat(DoubleCondition::DoubleEqualAndOrdered, operandLocation.asFPR(), operandLocation.asFPR())
            : m_jit.branchDouble(DoubleCondition::DoubleEqualAndOrdered, operandLocation.asFPR(), operandLocation.asFPR());

        // NaN case. Set result to zero.
        emitMoveConst(returnType == Types::I32 ? Value::fromI32(0) : Value::fromI64(0), resultLocation);
        Jump afterNaN = m_jit.jump();

        // Non-NaN case. Set result to the minimum value.
        isNotNaN.link(&m_jit);
        emitMoveConst(returnType == Types::I32 ? Value::fromI32(static_cast<int32_t>(minResult)) : Value::fromI64(static_cast<int64_t>(minResult)), resultLocation);
        afterNaN.link(&m_jit);
    }
    Jump afterMin = m_jit.jump();

    // Above maximum case.
    higherThanMax.link(&m_jit);
    emitMoveConst(returnType == Types::I32 ? Value::fromI32(static_cast<int32_t>(maxResult)) : Value::fromI64(static_cast<int64_t>(maxResult)), resultLocation);

    afterInBounds.link(&m_jit);
    afterMin.link(&m_jit);

    return { };
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
    Location resultLocation = loadIfNecessary(result);

    emitThrowOnNullReference(exceptionType, resultLocation);
}

[[nodiscard]] PartialResult BBQJIT::addArrayNewData(uint32_t typeIndex, uint32_t dataIndex, ExpressionType arraySize, ExpressionType offset, ExpressionType& result)
{
    pushArrayNewFromSegment(operationWasmArrayNewData, typeIndex, dataIndex, arraySize, offset, ExceptionType::BadArrayNewInitData, result);
    LOG_INSTRUCTION("ArrayNewData", typeIndex, dataIndex, arraySize, offset, RESULT(result));
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addArrayNewElem(uint32_t typeIndex, uint32_t elemSegmentIndex, ExpressionType arraySize, ExpressionType offset, ExpressionType& result)
{
    pushArrayNewFromSegment(operationWasmArrayNewElem, typeIndex, elemSegmentIndex, arraySize, offset, ExceptionType::BadArrayNewInitElem, result);
    LOG_INSTRUCTION("ArrayNewElem", typeIndex, elemSegmentIndex, arraySize, offset, RESULT(result));
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addArrayCopy(uint32_t dstTypeIndex, TypedExpression typedDst, ExpressionType dstOffset, uint32_t srcTypeIndex, TypedExpression typedSrc, ExpressionType srcOffset, ExpressionType size)
{
    auto dst = typedDst.value();
    auto src = typedSrc.value();
    if (dst.isConst() || src.isConst()) {
        ASSERT_IMPLIES(dst.isConst(), dst.asI64() == JSValue::encode(jsNull()));
        ASSERT_IMPLIES(src.isConst(), src.asI64() == JSValue::encode(jsNull()));

        LOG_INSTRUCTION("ArrayCopy", dstTypeIndex, dst, dstOffset, srcTypeIndex, src, srcOffset, size);

        consume(dst);
        consume(dstOffset);
        consume(src);
        consume(srcOffset);
        consume(size);
        emitThrowException(ExceptionType::NullArrayCopy);
        return { };
    }

    if (typedDst.type().isNullable())
        emitThrowOnNullReference(ExceptionType::NullArrayCopy, loadIfNecessary(dst));
    if (typedSrc.type().isNullable())
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("ArrayCopy", dstTypeIndex, dst, dstOffset, srcTypeIndex, src, srcOffset, size);

    recordJumpToThrowException(ExceptionType::OutOfBoundsArrayCopy, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addArrayInitElem(uint32_t dstTypeIndex, TypedExpression typedDst, ExpressionType dstOffset, uint32_t srcElementIndex, ExpressionType srcOffset, ExpressionType size)
{
    auto dst = typedDst.value();
    if (dst.isConst()) {
        ASSERT(dst.asI64() == JSValue::encode(jsNull()));

        LOG_INSTRUCTION("ArrayInitElem", dstTypeIndex, dst, dstOffset, srcElementIndex, srcOffset, size);

        consume(dstOffset);
        consume(srcOffset);
        consume(size);
        emitThrowException(ExceptionType::NullArrayInitElem);
        return { };
    }

    if (typedDst.type().isNullable())
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("ArrayInitElem", dstTypeIndex, dst, dstOffset, srcElementIndex, srcOffset, size);

    recordJumpToThrowException(ExceptionType::OutOfBoundsArrayInitElem, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addArrayInitData(uint32_t dstTypeIndex, TypedExpression typedDst, ExpressionType dstOffset, uint32_t srcDataIndex, ExpressionType srcOffset, ExpressionType size)
{
    auto dst = typedDst.value();
    if (dst.isConst()) {
        ASSERT(dst.asI64() == JSValue::encode(jsNull()));

        LOG_INSTRUCTION("ArrayInitData", dstTypeIndex, dst, dstOffset, srcDataIndex, srcOffset, size);

        consume(dstOffset);
        consume(srcOffset);
        consume(size);
        emitThrowException(ExceptionType::NullArrayInitData);
        return { };
    }

    if (typedDst.type().isNullable())
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
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("ArrayInitData", dstTypeIndex, dst, dstOffset, srcDataIndex, srcOffset, size);

    recordJumpToThrowException(ExceptionType::OutOfBoundsArrayInitData, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addAnyConvertExtern(ExpressionType reference, ExpressionType& result)
{
    Vector<Value, 8> arguments = {
        reference
    };
    result = topValue(TypeKind::Anyref);
    emitCCall(&operationWasmAnyConvertExtern, arguments, result);

    LOG_INSTRUCTION("AnyConvertExtern", reference, RESULT(result));
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addExternConvertAny(ExpressionType reference, ExpressionType& result)
{
    auto referenceLocation = reference.isConst() ? Location::none() : loadIfNecessary(reference);
    consume(reference);

    result = topValue(TypeKind::Externref);
    auto resultLocation = allocate(result);
    if (reference.isConst())
        emitMoveConst(reference, resultLocation);
    else
        emitMove(reference.type(), referenceLocation, resultLocation);

    LOG_INSTRUCTION("ExternConvertAny", reference, RESULT(result));
    return { };
}

// Basic operators
[[nodiscard]] PartialResult BBQJIT::addSelect(Value condition, Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Add(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Add(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Add(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Sub(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Sub(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Sub(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Mul(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Mul(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Mul(Value lhs, Value rhs, Value& result)
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
void BBQJIT::addLatePath(WasmOrigin origin, Func&& func)
{
    m_latePaths.append({ origin, WTF::move(func) });
}

void BBQJIT::emitThrowException(ExceptionType type)
{
    m_jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    m_jit.jumpThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
}

void BBQJIT::recordJumpToThrowException(ExceptionType type, Jump jump)
{
    m_exceptions[static_cast<unsigned>(type)].append(jump);
}

void BBQJIT::recordJumpToThrowException(ExceptionType type, const JumpList& jumps)
{
    m_exceptions[static_cast<unsigned>(type)].append(jumps);
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

[[nodiscard]] PartialResult BBQJIT::addI32DivS(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI64DivS(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32DivU(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI64DivU(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32RemS(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI64RemS(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32RemU(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI64RemU(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Div(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Div(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Min(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Min(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Max(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Max(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32And(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Xor(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Or(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Shl(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32ShrS(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32ShrU(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Rotl(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Rotr(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Clz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Clz", TypeKind::I32,
        BLOCK(Value::fromI32(WTF::clz(operand.asI32()))),
        BLOCK(
            m_jit.countLeadingZeros32(operandLocation.asGPR(), resultLocation.asGPR());
        )
    );
}

[[nodiscard]] PartialResult BBQJIT::addI32Ctz(Value operand, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32Eq", lhs, rhs, result, RelationalCondition::Equal, RELOP_AS_LAMBDA( == ));
}

[[nodiscard]] PartialResult BBQJIT::addI64Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64Eq", lhs, rhs, result, RelationalCondition::Equal, RELOP_AS_LAMBDA( == ));
}

[[nodiscard]] PartialResult BBQJIT::addI32Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32Ne", lhs, rhs, result, RelationalCondition::NotEqual, RELOP_AS_LAMBDA( != ));
}

[[nodiscard]] PartialResult BBQJIT::addI64Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64Ne", lhs, rhs, result, RelationalCondition::NotEqual, RELOP_AS_LAMBDA( != ));
}

[[nodiscard]] PartialResult BBQJIT::addI32LtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LtS", lhs, rhs, result, RelationalCondition::LessThan, RELOP_AS_LAMBDA( < ));
}

[[nodiscard]] PartialResult BBQJIT::addI64LtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LtS", lhs, rhs, result, RelationalCondition::LessThan, RELOP_AS_LAMBDA( < ));
}

[[nodiscard]] PartialResult BBQJIT::addI32LeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LeS", lhs, rhs, result, RelationalCondition::LessThanOrEqual, RELOP_AS_LAMBDA( <= ));
}

[[nodiscard]] PartialResult BBQJIT::addI64LeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LeS", lhs, rhs, result, RelationalCondition::LessThanOrEqual, RELOP_AS_LAMBDA( <= ));
}

[[nodiscard]] PartialResult BBQJIT::addI32GtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GtS", lhs, rhs, result, RelationalCondition::GreaterThan, RELOP_AS_LAMBDA( > ));
}

[[nodiscard]] PartialResult BBQJIT::addI64GtS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GtS", lhs, rhs, result, RelationalCondition::GreaterThan, RELOP_AS_LAMBDA( > ));
}

[[nodiscard]] PartialResult BBQJIT::addI32GeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GeS", lhs, rhs, result, RelationalCondition::GreaterThanOrEqual, RELOP_AS_LAMBDA( >= ));
}

[[nodiscard]] PartialResult BBQJIT::addI64GeS(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GeS", lhs, rhs, result, RelationalCondition::GreaterThanOrEqual, RELOP_AS_LAMBDA( >= ));
}

[[nodiscard]] PartialResult BBQJIT::addI32LtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LtU", lhs, rhs, result, RelationalCondition::Below, TYPED_RELOP_AS_LAMBDA(uint32_t, <));
}

[[nodiscard]] PartialResult BBQJIT::addI64LtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LtU", lhs, rhs, result, RelationalCondition::Below, TYPED_RELOP_AS_LAMBDA(uint64_t, <));
}

[[nodiscard]] PartialResult BBQJIT::addI32LeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32LeU", lhs, rhs, result, RelationalCondition::BelowOrEqual, TYPED_RELOP_AS_LAMBDA(uint32_t, <=));
}

[[nodiscard]] PartialResult BBQJIT::addI64LeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64LeU", lhs, rhs, result, RelationalCondition::BelowOrEqual, TYPED_RELOP_AS_LAMBDA(uint64_t, <=));
}

[[nodiscard]] PartialResult BBQJIT::addI32GtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GtU", lhs, rhs, result, RelationalCondition::Above, TYPED_RELOP_AS_LAMBDA(uint32_t, >));
}

[[nodiscard]] PartialResult BBQJIT::addI64GtU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI64("I64GtU", lhs, rhs, result, RelationalCondition::Above, TYPED_RELOP_AS_LAMBDA(uint64_t, >));
}

[[nodiscard]] PartialResult BBQJIT::addI32GeU(Value lhs, Value rhs, Value& result)
{
    return emitCompareI32("I32GeU", lhs, rhs, result, RelationalCondition::AboveOrEqual, TYPED_RELOP_AS_LAMBDA(uint32_t, >=));
}

[[nodiscard]] PartialResult BBQJIT::addI64GeU(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Eq", lhs, rhs, result, DoubleCondition::DoubleEqualAndOrdered, RELOP_AS_LAMBDA( == ));
}

[[nodiscard]] PartialResult BBQJIT::addF64Eq(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Eq", lhs, rhs, result, DoubleCondition::DoubleEqualAndOrdered, RELOP_AS_LAMBDA( == ));
}

[[nodiscard]] PartialResult BBQJIT::addF32Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Ne", lhs, rhs, result, DoubleCondition::DoubleNotEqualOrUnordered, RELOP_AS_LAMBDA( != ));
}

[[nodiscard]] PartialResult BBQJIT::addF64Ne(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Ne", lhs, rhs, result, DoubleCondition::DoubleNotEqualOrUnordered, RELOP_AS_LAMBDA( != ));
}

[[nodiscard]] PartialResult BBQJIT::addF32Lt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Lt", lhs, rhs, result, DoubleCondition::DoubleLessThanAndOrdered, RELOP_AS_LAMBDA( < ));
}

[[nodiscard]] PartialResult BBQJIT::addF64Lt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Lt", lhs, rhs, result, DoubleCondition::DoubleLessThanAndOrdered, RELOP_AS_LAMBDA( < ));
}

[[nodiscard]] PartialResult BBQJIT::addF32Le(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Le", lhs, rhs, result, DoubleCondition::DoubleLessThanOrEqualAndOrdered, RELOP_AS_LAMBDA( <= ));
}

[[nodiscard]] PartialResult BBQJIT::addF64Le(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Le", lhs, rhs, result, DoubleCondition::DoubleLessThanOrEqualAndOrdered, RELOP_AS_LAMBDA( <= ));
}

[[nodiscard]] PartialResult BBQJIT::addF32Gt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Gt", lhs, rhs, result, DoubleCondition::DoubleGreaterThanAndOrdered, RELOP_AS_LAMBDA( > ));
}

[[nodiscard]] PartialResult BBQJIT::addF64Gt(Value lhs, Value rhs, Value& result)
{
    return emitCompareF64("F64Gt", lhs, rhs, result, DoubleCondition::DoubleGreaterThanAndOrdered, RELOP_AS_LAMBDA( > ));
}

[[nodiscard]] PartialResult BBQJIT::addF32Ge(Value lhs, Value rhs, Value& result)
{
    return emitCompareF32("F32Ge", lhs, rhs, result, DoubleCondition::DoubleGreaterThanOrEqualAndOrdered, RELOP_AS_LAMBDA( >= ));
}

[[nodiscard]] PartialResult BBQJIT::addF64Ge(Value lhs, Value rhs, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addI32Extend16S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Extend16S", TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(static_cast<int16_t>(operand.asI32())))),
        BLOCK(
            m_jit.signExtend16To32(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addI32Eqz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32Eqz", TypeKind::I32,
        BLOCK(Value::fromI32(!operand.asI32())),
        BLOCK(
            m_jit.test32(ResultCondition::Zero, operandLocation.asGPR(), operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addI32Popcnt(Value operand, Value& result)
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
                emitCCall(&operationPopcount32, Vector<Value, 8> { arg }, result);
            }
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addI64Popcnt(Value operand, Value& result)
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
                emitCCall(&operationPopcount64, Vector<Value, 8> { arg }, result);
            }
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addI32ReinterpretF32(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32ReinterpretF32", TypeKind::I32,
        BLOCK(Value::fromI32(std::bit_cast<int32_t>(operand.asF32()))),
        BLOCK(
            m_jit.moveFloatTo32(operandLocation.asFPR(), resultLocation.asGPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF32ReinterpretI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ReinterpretI32", TypeKind::F32,
        BLOCK(Value::fromF32(std::bit_cast<float>(operand.asI32()))),
        BLOCK(
            m_jit.move32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF32DemoteF64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32DemoteF64", TypeKind::F32,
        BLOCK(Value::fromF32(operand.asF64())),
        BLOCK(
            m_jit.convertDoubleToFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF64PromoteF32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64PromoteF32", TypeKind::F64,
        BLOCK(Value::fromF64(operand.asF32())),
        BLOCK(
            m_jit.convertFloatToDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF64Copysign(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F64Copysign", TypeKind::F64,
        BLOCK(Value::fromF64(doubleCopySign(lhs.asF64(), rhs.asF64()))),
        BLOCK(
            m_jit.move64ToDouble(TrustedImm64(std::numeric_limits<int64_t>::min()), wasmScratchFPR);
            m_jit.andDouble(rhsLocation.asFPR(), wasmScratchFPR, wasmScratchFPR);
            m_jit.absDouble(lhsLocation.asFPR(), resultLocation.asFPR());
            m_jit.orDouble(wasmScratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            if (lhs.isConst()) {
                m_jit.move64ToDouble(TrustedImm64(std::numeric_limits<int64_t>::min()), wasmScratchFPR);
                m_jit.andDouble(rhsLocation.asFPR(), wasmScratchFPR, wasmScratchFPR);
                emitMoveConst(Value::fromF64(std::abs(lhs.asF64())), resultLocation);
                m_jit.orDouble(resultLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
            } else {
                bool signBit = std::bit_cast<uint64_t>(rhs.asF64()) & 0x8000000000000000ull;
                m_jit.absDouble(lhsLocation.asFPR(), resultLocation.asFPR());
                if (signBit)
                    m_jit.negateDouble(resultLocation.asFPR(), resultLocation.asFPR());
            }
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF32ConvertSI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ConvertSI32", TypeKind::F32,
        BLOCK(Value::fromF32(operand.asI32())),
        BLOCK(
            m_jit.convertInt32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF64ConvertSI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ConvertSI32", TypeKind::F64,
        BLOCK(Value::fromF64(operand.asI32())),
        BLOCK(
            m_jit.convertInt32ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF32Copysign(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "F32Copysign", TypeKind::F32,
        BLOCK(Value::fromF32(floatCopySign(lhs.asF32(), rhs.asF32()))),
        BLOCK(
            m_jit.move32ToFloat(TrustedImm32(std::numeric_limits<int32_t>::min()), wasmScratchFPR);
            m_jit.andFloat(rhsLocation.asFPR(), wasmScratchFPR, wasmScratchFPR);
            m_jit.absFloat(lhsLocation.asFPR(), resultLocation.asFPR());
            m_jit.orFloat(wasmScratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
        ),
        BLOCK(
            if (lhs.isConst()) {
                m_jit.move32ToFloat(TrustedImm32(std::numeric_limits<int32_t>::min()), wasmScratchFPR);
                m_jit.andFloat(rhsLocation.asFPR(), wasmScratchFPR, wasmScratchFPR);
                emitMoveConst(Value::fromF32(std::abs(lhs.asF32())), resultLocation);
                m_jit.orFloat(resultLocation.asFPR(), wasmScratchFPR, resultLocation.asFPR());
            } else {
                bool signBit = std::bit_cast<uint32_t>(rhs.asF32()) & 0x80000000u;
                m_jit.absFloat(lhsLocation.asFPR(), resultLocation.asFPR());
                if (signBit)
                    m_jit.negateFloat(resultLocation.asFPR(), resultLocation.asFPR());
            }
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF32Abs(Value operand, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF64Abs(Value operand, Value& result)
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

[[nodiscard]] PartialResult BBQJIT::addF32Sqrt(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Sqrt", TypeKind::F32,
        BLOCK(Value::fromF32(Math::sqrtFloat(operand.asF32()))),
        BLOCK(
            m_jit.sqrtFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF64Sqrt(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Sqrt", TypeKind::F64,
        BLOCK(Value::fromF64(Math::sqrtDouble(operand.asF64()))),
        BLOCK(
            m_jit.sqrtDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF32Neg(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Neg", TypeKind::F32,
        BLOCK(Value::fromF32(-operand.asF32())),
        BLOCK(
            m_jit.negateFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addF64Neg(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Neg", TypeKind::F64,
        BLOCK(Value::fromF64(-operand.asF64())),
        BLOCK(
            m_jit.negateDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

[[nodiscard]] PartialResult BBQJIT::addI32TruncSF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncSF32, operand, result, Types::I32, Types::F32);
}

[[nodiscard]] PartialResult BBQJIT::addI32TruncSF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncSF64, operand, result, Types::I32, Types::F64);
}

[[nodiscard]] PartialResult BBQJIT::addI32TruncUF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncUF32, operand, result, Types::I32, Types::F32);
}

[[nodiscard]] PartialResult BBQJIT::addI32TruncUF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I32TruncUF64, operand, result, Types::I32, Types::F64);
}

[[nodiscard]] PartialResult BBQJIT::addI64TruncSF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncSF32, operand, result, Types::I64, Types::F32);
}

[[nodiscard]] PartialResult BBQJIT::addI64TruncSF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncSF64, operand, result, Types::I64, Types::F64);
}

[[nodiscard]] PartialResult BBQJIT::addI64TruncUF32(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncUF32, operand, result, Types::I64, Types::F32);
}

[[nodiscard]] PartialResult BBQJIT::addI64TruncUF64(Value operand, Value& result)
{
    return truncTrapping(OpType::I64TruncUF64, operand, result, Types::I64, Types::F64);
}

// References

[[nodiscard]] PartialResult BBQJIT::addRefEq(Value ref0, Value ref1, Value& result)
{
    return addI64Eq(ref0, ref1, result);
}

[[nodiscard]] PartialResult BBQJIT::addRefFunc(FunctionSpaceIndex index, Value& result)
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
    m_jit.move(TrustedImmPtr(std::bit_cast<uintptr_t>(&m_callee.tierUpCounter().m_counter)), wasmScratchGPR);
    Jump tierUp = m_jit.branchAdd32(CCallHelpers::PositiveOrZero, TrustedImm32(TierUpCount::functionEntryIncrement()), Address(wasmScratchGPR));
    MacroAssembler::Label tierUpResume = m_jit.label();
    addLatePath(origin(), [tierUp, tierUpResume](BBQJIT& generator, CCallHelpers& jit) {
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
[[nodiscard]] ControlData BBQJIT::addTopLevel(BlockSignature&& signature)
{
    if (Options::verboseBBQJITInstructions()) [[unlikely]] {
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
    emitPushCalleeSaves();
    m_topLevel = ControlData(*this, BlockType::TopLevel, WTF::move(signature), 0);

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

    if (m_profiledCallee.hasExceptionHandlers())
        m_jit.store32(CCallHelpers::TrustedImm32(wasmInvalidCallSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Because we compile in a single pass, we always need to pessimistically check for stack underflow/overflow.
    static_assert(wasmScratchGPR == GPRInfo::nonPreservedNonArgumentGPR0);
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, wasmScratchGPR);

    MacroAssembler::JumpList overflow;
    JIT_COMMENT(m_jit, "Stack overflow check");
#if !CPU(ADDRESS64)
    overflow.append(m_jit.branchPtr(CCallHelpers::Above, wasmScratchGPR, GPRInfo::callFrameRegister));
#endif
    overflow.append(m_jit.branchPtr(CCallHelpers::LessThan, wasmScratchGPR, CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit())));
    overflow.linkThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()), &m_jit);

    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);

    Address baselineDataAddress(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfBaselineData(m_info, m_functionIndex));
    m_jit.loadPtr(baselineDataAddress, GPRInfo::jitDataRegister);
    Jump materialize = m_jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::jitDataRegister);
    MacroAssembler::Label done = m_jit.label();
    addLatePath(origin(), [materialize = WTF::move(materialize), done, baselineDataAddress](BBQJIT&, CCallHelpers& jit) {
        materialize.link(&jit);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::nonPreservedNonArgumentGPR0);
        jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(materializeBaselineDataGenerator).code()));
        jit.loadPtr(baselineDataAddress, GPRInfo::jitDataRegister);
        jit.jump(done);
    });
    m_jit.add32(TrustedImm32(1), CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineData::offsetOfTotalCount()));

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
            emitStoreConst(Value::fromI64(std::bit_cast<uint64_t>(JSValue::encode(jsNull()))), location);
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
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Structref:
        case TypeKind::Arrayref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
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
    emitPushCalleeSaves();

    m_jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(&m_callee)), wasmScratchGPR);
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    if constexpr (is32Bit()) {
        CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
        m_jit.storePtr(wasmScratchGPR, calleeSlot.withOffset(PayloadOffset));
        m_jit.store32(CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag), calleeSlot.withOffset(TagOffset));
        m_jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    } else
        m_jit.storePairPtr(GPRInfo::wasmContextInstancePointer, wasmScratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    // Because tiering up code materializes BaselineData, this is always non nullptr.
    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfBaselineData(m_info, m_functionIndex)), GPRInfo::jitDataRegister);

    int roundedFrameSize = stackCheckSize();
#if CPU(X86_64) || CPU(ARM64)
    m_jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32(roundedFrameSize), MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32(roundedFrameSize), wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif

    // The loop_osr slow path should have already checked that we have enough space. We have already destroyed the ipint stack, and unwind will see the BBQ catch
    // since we already replaced callee. So, we just assert that this case doesn't happen to avoid reading a corrupted frame from the bbq catch handler.
    MacroAssembler::JumpList overflow;
#if !CPU(ADDRESS64)
    overflow.append(m_jit.branchPtr(CCallHelpers::Above, wasmScratchGPR, GPRInfo::callFrameRegister));
    overflow.append(m_jit.branchPtr(CCallHelpers::LessThanOrEqual, wasmScratchGPR, CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit())));
#else
    overflow.append(m_jit.branchPtr(CCallHelpers::LessThanOrEqual, MacroAssembler::stackPointerRegister, CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit())));
#endif
    overflow.linkThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(crashDueToBBQStackOverflowGenerator).code()), &m_jit);

    // This operation shuffles around values on the stack, until everything is in the right place. Then,
    // it returns the address of the loop we're jumping to in wasmScratchGPR (so we don't interfere with
    // anything we just loaded from the scratch buffer into a register)
    m_jit.probe(tagCFunction<JITProbePtrTag>(operationWasmLoopOSREnterBBQJIT), nullptr);

    // We expect the loop address to be populated by the probe operation.
    static_assert(wasmScratchGPR == GPRInfo::nonPreservedNonArgumentGPR0);
    m_jit.farJump(wasmScratchGPR, WasmEntryPtrTag);
    return label;
}

[[nodiscard]] PartialResult BBQJIT::addBlock(BlockSignature&& signature, Stack& enclosingStack, ControlType& result, Stack& newStack)
{
    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::Block, WTF::move(signature), height);
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("Block", result.signature());
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);
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
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
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
#if USE(JSVALUE32_64)
    if (location.isGPR2())
        return B3::ValueRep(B3::ValueRep::OSRValueRep, Reg(location.asGPRlo()), Reg(location.asGPRhi()));
#endif
    if (location.isRegister())
        return B3::ValueRep(location.isGPR() ? Reg(location.asGPR()) : Reg(location.asFPR()));
    if (location.isStack())
        return B3::ValueRep(ValueLocation::stack(location.asStackOffset()));

    RELEASE_ASSERT_NOT_REACHED();
    return B3::ValueRep();
}

// This needs to be kept in sync with WasmIPIntSlowPaths.cpp buildEntryBufferForLoopOSR and OMGIRGenerator::addLoop.
StackMap BBQJIT::makeStackMap(const ControlData& data, Stack& enclosingStack)
{
    unsigned numElements = m_locals.size() + data.enclosedHeight() + data.argumentLocations().size();
    for (const ControlEntry& entry : m_parser->controlStack()) {
        if (BBQJIT::ControlData::isTry(entry.controlData))
            ++numElements;
    }

    StackMap stackMap(numElements);
    unsigned stackMapIndex = 0;
    for (unsigned i = 0; i < m_locals.size(); i ++)
        stackMap[stackMapIndex++] = OSREntryValue(toB3Rep(m_locals[i]), toB3Type(m_localTypes[i]));

    // Do rethrow slots first because IPInt has them in a shadow stack.
    for (const ControlEntry& entry : m_parser->controlStack()) {
        if (ControlData::isAnyCatch(entry.controlData)) {
            ASSERT(entry.controlData.implicitSlots() == 1);
            Value exception = this->exception(entry.controlData);
            stackMap[stackMapIndex++] = OSREntryValue(toB3Rep(locationOf(exception)), B3::Int64); // Exceptions are EncodedJSValues, so they are always Int64
        } else if (BBQJIT::ControlData::isTry(entry.controlData)) {
            // IPInt reserves rethrow slots based on Try blocks, but there is no exception to rethrow until Catch,
            // and BBQ and OMG do not represent the implicit exception slot/variable except within the Catch, so
            // use Void to signify we shouldn't load and constant 0 to zero fill this slot when storing.
            ASSERT(!entry.controlData.implicitSlots());
            stackMap[stackMapIndex++] = OSREntryValue(B3::ValueRep::constant(0), B3::Void);
        } else
            ASSERT(!entry.controlData.implicitSlots());
    }

    for (const ControlEntry& entry : m_parser->controlStack()) {
        for (const TypedExpression& expr : entry.enclosedExpressionStack)
            stackMap[stackMapIndex++] = OSREntryValue(toB3Rep(locationOf(expr.value())), toB3Type(expr.type().kind));
    }

    for (const TypedExpression& expr : enclosingStack)
        stackMap[stackMapIndex++] = OSREntryValue(toB3Rep(locationOf(expr.value())), toB3Type(expr.type().kind));
    for (unsigned i = 0; i < data.argumentLocations().size(); i++)
        stackMap[stackMapIndex++] = OSREntryValue(toB3Rep(data.argumentLocations()[i]), toB3Type(data.argumentType(i).kind));

    RELEASE_ASSERT(stackMapIndex == numElements);
    m_osrEntryScratchBufferSize = std::max(m_osrEntryScratchBufferSize, BBQCallee::extraOSRValuesForLoopIndex + numElements);
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
    m_jit.move(TrustedImmPtr(std::bit_cast<uintptr_t>(&tierUpCounter.m_counter)), wasmScratchGPR);

    TierUpCount::TriggerReason* forceEntryTrigger = &(tierUpCounter.osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");

    Jump forceOSREntry = m_jit.branchTest8(ResultCondition::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
    Jump tierUp = m_jit.branchAdd32(ResultCondition::PositiveOrZero, TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(wasmScratchGPR));
    MacroAssembler::Label tierUpResume = m_jit.label();

    OSREntryData* osrEntryDataPtr = &osrEntryData;

    addLatePath(origin(), [forceOSREntry, tierUp, tierUpResume, osrEntryDataPtr](BBQJIT&, CCallHelpers& jit) {
        forceOSREntry.link(&jit);
        tierUp.link(&jit);

        jit.probe(tagCFunction<JITProbePtrTag>(operationWasmTriggerOSREntryNow), osrEntryDataPtr);
        jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::nonPreservedNonArgumentGPR0).linkTo(tierUpResume, &jit);

        // operationWasmTriggerOSREntryNow is already restoring callee saves. Thus we do not need to restore them before jumping.
        jit.farJump(GPRInfo::nonPreservedNonArgumentGPR0, WasmEntryPtrTag);
    });
#else
    UNUSED_PARAM(osrEntryData);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

[[nodiscard]] PartialResult BBQJIT::addLoop(BlockSignature&& signature, Stack& enclosingStack, ControlType& result, Stack& newStack, uint32_t loopIndex)
{
    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::Loop, WTF::move(signature), height);
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("Loop", result.signature());
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);
    result.startBlock(*this, newStack);
    result.setLoopLabel(m_jit.label());

    RELEASE_ASSERT(m_compilation->bbqLoopEntrypoints.size() == loopIndex);
    m_compilation->bbqLoopEntrypoints.append(result.loopLabel());

    emitLoopTierUpCheckAndOSREntryData(result, enclosingStack, loopIndex);
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addIf(Value condition, BlockSignature&& signature, Stack& enclosingStack, ControlData& result, Stack& newStack)
{
    RegisterSet liveScratchGPRs;
    Location conditionLocation;
    if (!condition.isConst()) {
        conditionLocation = loadIfNecessary(condition);
        liveScratchGPRs.add(conditionLocation.asGPR(), IgnoreVectors);
    }
    consume(condition);

    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::If, WTF::move(signature), height, liveScratchGPRs);

    // Despite being conditional, if doesn't need to worry about diverging expression stacks at block boundaries, so it doesn't need multiple exits.
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("If", result.signature(), condition, conditionLocation);
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);

    result.startBlock(*this, newStack);
    if (condition.isConst() && !condition.asI32())
        result.setIfBranch(m_jit.jump()); // Emit direct branch if we know the condition is false.
    else if (!condition.isConst()) // Otherwise, we only emit a branch at all if we don't know the condition statically.
        result.setIfBranch(m_jit.branchTest32(ResultCondition::Zero, conditionLocation.asGPR()));
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addElse(ControlData& data, Stack& expressionStack)
{
    data.flushAndSingleExit(*this, data, expressionStack, false, true);
    ControlData dataElse(ControlData::UseBlockCallingConventionOfOtherBranch, BlockType::Else, data);
    data.linkJumps(&m_jit);
    dataElse.addBranch(m_jit.jump());
    data.linkIfBranch(&m_jit); // Link specifically the conditional branch of the preceding If
    LOG_DEDENT();
    LOG_INSTRUCTION("Else");
    LOG_INDENT();

    // We don't care at this point about the values live at the end of the previous control block,
    // we just need the right number of temps for our arguments on the top of the stack.
    expressionStack.clear();
    const auto& blockSignature = data.signature();
    while (expressionStack.size() < blockSignature.argumentCount()) {
        Type type = blockSignature.argumentType(expressionStack.size());
        expressionStack.constructAndAppend(type, Value::fromTemp(type.kind, dataElse.enclosedHeight() + dataElse.implicitSlots() + expressionStack.size()));
    }

    dataElse.startBlock(*this, expressionStack);
    data = dataElse;
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addElseToUnreachable(ControlData& data)
{
    // We want to flush or consume all values on the stack to reset the allocator
    // state entering the else block.
    data.flushAtBlockBoundary(*this, 0, m_parser->expressionStack(), true);

    ControlData dataElse(ControlData::UseBlockCallingConventionOfOtherBranch, BlockType::Else, data);
    data.linkJumps(&m_jit);
    dataElse.addBranch(m_jit.jump()); // Still needed even when the parent was unreachable to avoid running code within the else block.
    data.linkIfBranch(&m_jit); // Link specifically the conditional branch of the preceding If
    LOG_DEDENT();
    LOG_INSTRUCTION("Else");
    LOG_INDENT();

    // We don't have easy access to the original expression stack we had entering the if block,
    // so we construct a local stack just to set up temp bindings as we enter the else.
    Stack expressionStack;
    const auto& functionSignature = dataElse.signature();
    for (unsigned i = 0; i < functionSignature.argumentCount(); i ++)
        expressionStack.constructAndAppend(functionSignature.argumentType(i), Value::fromTemp(functionSignature.argumentType(i).kind, dataElse.enclosedHeight() + dataElse.implicitSlots() + i));
    dataElse.startBlock(*this, expressionStack);
    data = dataElse;
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addTry(BlockSignature&& signature, Stack& enclosingStack, ControlType& result, Stack& newStack)
{
    m_usesExceptions = true;
    ++m_tryCatchDepth;
    ++m_callSiteIndex;
    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::Try, WTF::move(signature), height);
    result.setTryInfo(m_callSiteIndex, m_callSiteIndex, m_tryCatchDepth);
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("Try", result.signature());
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);
    result.startBlock(*this, newStack);
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addTryTable(BlockSignature&& signature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack)
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

    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::TryTable, WTF::move(signature), height);
    result.setTryInfo(m_callSiteIndex, m_callSiteIndex, m_tryCatchDepth);
    result.setTryTableTargets(WTF::move(targetList));
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("TryTable", result.signature());
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);
    result.startBlock(*this, newStack);
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addCatch(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, Stack& expressionStack, ControlType& data, ResultList& results)
{
    m_usesExceptions = true;
    data.flushAndSingleExit(*this, data, expressionStack, false, true);
    unbindAllRegisters();
    ControlData dataCatch(*this, BlockType::Catch, BlockSignature { data.signature() }, data.enclosedHeight());
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
    data = WTF::move(dataCatch);
    m_exceptionHandlers.append({ HandlerType::Catch, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, ControlType& data, ResultList& results)
{
    m_usesExceptions = true;
    unbindAllRegisters();
    ControlData dataCatch(*this, BlockType::Catch, BlockSignature { data.signature() }, data.enclosedHeight());
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
    data = WTF::move(dataCatch);
    m_exceptionHandlers.append({ HandlerType::Catch, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addCatchAll(Stack& expressionStack, ControlType& data)
{
    m_usesExceptions = true;
    data.flushAndSingleExit(*this, data, expressionStack, false, true);
    unbindAllRegisters();
    ControlData dataCatch(*this, BlockType::Catch, BlockSignature { data.signature() }, data.enclosedHeight());
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
    data = WTF::move(dataCatch);
    m_exceptionHandlers.append({ HandlerType::CatchAll, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, 0 });
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addCatchAllToUnreachable(ControlType& data)
{
    m_usesExceptions = true;
    unbindAllRegisters();
    ControlData dataCatch(*this, BlockType::Catch, BlockSignature { data.signature() }, data.enclosedHeight());
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
    data = WTF::move(dataCatch);
    m_exceptionHandlers.append({ HandlerType::CatchAll, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, 0 });
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addDelegate(ControlType& target, ControlType& data)
{
    return addDelegateToUnreachable(target, data);
}

[[nodiscard]] PartialResult BBQJIT::addDelegateToUnreachable(ControlType& target, ControlType& data)
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

[[nodiscard]] PartialResult BBQJIT::addThrow(unsigned exceptionIndex, ArgumentList& arguments, Stack&)
{

    LOG_INSTRUCTION("Throw", arguments);

    unsigned offset = 0;
    for (auto arg : arguments) {
        Location stackLocation = Location::fromStackArgument(offset * sizeof(uint64_t));
        emitMove(arg, stackLocation);
        consume(arg);
        offset += arg.value().type() == TypeKind::V128 ? 2 : 1;
    }
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(offset * sizeof(uint64_t));
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    ++m_callSiteIndex;
    if (m_profiledCallee.hasExceptionHandlers()) {
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
    if (m_profiledCallee.hasExceptionHandlers()) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegistersForException();
    }
}

[[nodiscard]] PartialResult BBQJIT::addReturn(const ControlData& data, const Stack& returnValues)
{
    // Use the function signature from the parser
    ASSERT(m_parser);
    const FunctionSignature& functionSignature = *m_parser->signature().template as<FunctionSignature>();

    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(functionSignature, CallRole::Callee);

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

    emitRestoreCalleeSaves();
    m_jit.emitFunctionEpilogue();
    m_jit.ret();
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addBranch(ControlData& target, Value condition, Stack& results)
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

[[nodiscard]] PartialResult BBQJIT::addSwitch(Value condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, Stack& results)
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
#if USE(JSVALUE64)
        auto* jumpTable = m_callee.addJumpTable(targets.size() + 1);
        m_jit.moveConditionally32(RelationalCondition::AboveOrEqual, wasmScratchGPR, TrustedImm32(targets.size()), TrustedImm32(targets.size()), wasmScratchGPR, wasmScratchGPR);
#else
        auto* jumpTable = m_callee.addJumpTable(targets.size());
        auto fallThrough = m_jit.branch32(RelationalCondition::AboveOrEqual, wasmScratchGPR, TrustedImm32(targets.size()));
#endif
        m_jit.zeroExtend32ToWord(wasmScratchGPR, wasmScratchGPR);
        if constexpr (is64Bit())
            m_jit.lshiftPtr(TrustedImm32(3), wasmScratchGPR);
        else
            m_jit.lshiftPtr(TrustedImm32(2), wasmScratchGPR);
        m_jit.addPtr(TrustedImmPtr(jumpTable->span().data()), wasmScratchGPR);
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
#if USE(JSVALUE64)
        labels.append(Box<CCallHelpers::Label>::create(m_jit.label()));
#else
        fallThrough.link(&m_jit);
#endif

        m_jit.addLinkTask([labels = WTF::move(labels), jumpTable](LinkBuffer& linkBuffer) {
            for (unsigned index = 0; index < labels.size(); ++index)
                jumpTable->at(index) = linkBuffer.locationOf<JSSwitchPtrTag>(*labels[index]);
        });
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

[[nodiscard]] PartialResult BBQJIT::endBlock(ControlEntry& entry, Stack& stack)
{
    return addEndToUnreachable(entry, stack, false);
}

[[nodiscard]] PartialResult BBQJIT::addEndToUnreachable(ControlEntry& entry, Stack& stack, bool unreachable)
{
    ControlData& entryData = entry.controlData;

    const auto& blockSignature = entryData.signature();
    unsigned returnCount = blockSignature.returnCount();
    if (unreachable) {
        for (unsigned i = 0; i < returnCount; ++i) {
            Type type = blockSignature.returnType(i);
            entry.enclosedExpressionStack.constructAndAppend(type, Value::fromTemp(type.kind, entryData.enclosedHeight() + entryData.implicitSlots() + i));
        }
        unbindAllRegisters();
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
        // similar to IPInt, we make a handler section to avoid jumping into random parts of code and not having
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

[[nodiscard]] PartialResult BBQJIT::endTopLevel(const Stack&)
{
    int frameSize = stackCheckSize();
    CCallHelpers& jit = m_jit;
    m_jit.addLinkTask([frameSize, labels = WTF::move(m_frameSizeLabels), &jit](LinkBuffer& linkBuffer) {
        for (auto label : labels)
            jit.repatchPointer(linkBuffer.locationOf<NoPtrTag>(label), std::bit_cast<void*>(static_cast<uintptr_t>(frameSize)));
    });

    LOG_DEDENT();
    LOG_INSTRUCTION("End");

    if (m_disassembler) [[unlikely]]
        m_disassembler->setEndOfOpcode(m_jit.label());

    for (const auto& [ origin, latePath ] : m_latePaths) {
        m_pcToCodeOriginMapBuilder.appendItem(m_jit.label(), CodeOrigin(BytecodeIndex(origin.m_opcodeOrigin.location())));
        latePath(*this, m_jit);
    }

    for (auto& [ origin, jumpList, returnLabel, registerBindings, generator ] : m_slowPaths) {
        JIT_COMMENT(m_jit, "Slow path start");
        jumpList.link(m_jit);
        m_pcToCodeOriginMapBuilder.appendItem(m_jit.label(), CodeOrigin(BytecodeIndex(origin.m_opcodeOrigin.location())));
        slowPathSpillBindings(registerBindings);
        generator(*this, m_jit);
        slowPathRestoreBindings(registerBindings);
        JIT_COMMENT(m_jit, "Slow path end");
        m_jit.jump(returnLabel);
    }

    for (unsigned i = 0; i < numberOfExceptionTypes; ++i) {
        auto& jumps = m_exceptions[i];
        if (!jumps.empty()) {
            jumps.link(&jit);
            emitThrowException(static_cast<ExceptionType>(i));
        }
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

void BBQJIT::flush(GPRReg, const RegisterBinding& binding)
{
    Value value = binding.toValue();
    ASSERT(value.isLocal() || value.isTemp());
    flushValue(value);
}

void BBQJIT::flush(FPRReg, const RegisterBinding& binding)
{
    Value value = binding.toValue();
    ASSERT(value.isLocal() || value.isTemp());
    flushValue(value);
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
    m_gprAllocator.flushIf(*this, [&](GPRReg, const RegisterBinding& binding) {
        return binding.toValue().isLocal();
    });
    m_fprAllocator.flushIf(*this, [&](FPRReg, const RegisterBinding& binding) {
        return binding.toValue().isLocal();
    });
}

void BBQJIT::flushRegisters()
{
    // Just flush everything.
    // FIXME: These should be store pairs.
    m_gprAllocator.flushAllRegisters(*this);
    m_fprAllocator.flushAllRegisters(*this);
}

void BBQJIT::RegisterBindings::dump(PrintStream& out) const
{
    CommaPrinter comma(", ", "[");
    for (unsigned i = 0; i < m_fprBindings.size(); ++i) {
        if (!m_fprBindings[i].isNone())
            out.print(comma, "<", static_cast<FPRReg>(i), ": ", m_fprBindings[i], ">");
    }
    if (comma.didPrint())
        out.print("]");

    comma = CommaPrinter(", ", comma.didPrint() ? ", ["_s : "["_s);
    for (unsigned i = 0; i < m_gprBindings.size(); ++i) {
        if (!m_gprBindings[i].isNone())
            out.print(comma, "<", static_cast<GPRReg>(i), ": ", m_gprBindings[i], ">");
    }
    if (comma.didPrint())
        out.print("]");
}

void BBQJIT::slowPathSpillBindings(const RegisterBindings& bindings)
{
    for (unsigned i = 0; i < bindings.m_fprBindings.size(); ++i) {
        Value value = bindings.m_fprBindings[i].toValue();
        if (!value.isNone())
            emitStore(value.type(), Location::fromFPR(static_cast<FPRReg>(i)), canonicalSlot(value));
    }

    // FIXME: These should be store load pairs.
    for (unsigned i = 0; i < bindings.m_gprBindings.size(); ++i) {
        Value value = bindings.m_gprBindings[i].toValue();
        if (!value.isNone())
            emitStore(value.type(), Location::fromGPR(static_cast<GPRReg>(i)), canonicalSlot(value));
    }
}

void BBQJIT::slowPathRestoreBindings(const RegisterBindings& bindings)
{
    for (unsigned i = 0; i < bindings.m_fprBindings.size(); ++i) {
        Value value = bindings.m_fprBindings[i].toValue();
        if (!value.isNone())
            emitLoad(value.type(), canonicalSlot(value), Location::fromFPR(static_cast<FPRReg>(i)));
    }

    // FIXME: These should be store load pairs.
    for (unsigned i = 0; i < bindings.m_gprBindings.size(); ++i) {
        Value value = bindings.m_gprBindings[i].toValue();
        if (!value.isNone())
            emitLoad(value.type(), canonicalSlot(value), Location::fromGPR(static_cast<GPRReg>(i)));
    }
}

template<typename Args>
void BBQJIT::saveValuesAcrossCallAndPassArguments(const Args& arguments, const CallInformation& callInfo, const TypeDefinition& signature)
{
    // First, we resolve all the locations of the passed arguments, before any spillage occurs. For constants,
    // we store their normal values; for all other values, we store pinned values with their current location.
    // We'll directly use these when passing parameters, since no other instructions we emit here should
    // overwrite registers currently occupied by values.

    auto resolvedArguments = WTF::map<8>(arguments, [&](auto& input) {
        auto argument = Value(input);
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
        RegisterBinding binding = bindingFor(reg);
        ASSERT(!binding.isScratch());
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
                binding = bindingFor(paramLocation.asGPR());
            else if (paramLocation.isFPR())
                binding = bindingFor(paramLocation.asFPR());
            else if (paramLocation.isGPR2())
                binding = bindingFor(paramLocation.asGPRhi());
            if (!binding.toValue().isNone())
                flushValue(binding.toValue());
        }
    }

    // Finally, we parallel-move arguments to the parameter locations.
    WTF::Vector<Location, 8> parameterLocations;
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
                currentBinding = bindingFor(returnLocation.asGPR());
            else if (returnLocation.isFPR())
                currentBinding = bindingFor(returnLocation.asFPR());
            else if (returnLocation.isGPR2())
                currentBinding = bindingFor(returnLocation.asGPRhi());
            // There's no way to preserve an abritrary scratch over a call so we shouldn't try to do so.
            ASSERT(!currentBinding.isScratch());
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

void BBQJIT::emitTailCall(FunctionSpaceIndex functionIndexSpace, const TypeDefinition& signature, ArgumentList& arguments)
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

    Vector<Value, 8> resolvedArguments;
    resolvedArguments.reserveInitialCapacity(arguments.size() + isX86());
    Vector<Location, 8> parameterLocations;
    parameterLocations.reserveInitialCapacity(arguments.size() + isX86());

    // Save the old Frame Pointer for later and make sure the return address gets saved to its canonical location.
    emitRestoreCalleeSaves();
    auto preserved = callingConvention.argumentGPRs();
    if constexpr (isARM64E())
        preserved.add(callingConvention.prologueScratchGPRs[0], IgnoreVectors);
    ScratchScope<1, 0> scratches(*this, WTF::move(preserved));
    GPRReg callerFramePointer = scratches.gpr(0);
    scratches.unbindPreserved();

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
        if (arguments[i].value().isConst())
            resolvedArguments.append(arguments[i].value());
        else
            resolvedArguments.append(Value::pinned(arguments[i].value().type(), locationOf(arguments[i])));

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

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace)) {
        static_assert(sizeof(WasmOrJSImportableFunctionCallLinkInfo) * maxImports < std::numeric_limits<int32_t>::max());
        RELEASE_ASSERT(JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndexSpace) < std::numeric_limits<int32_t>::max());
        m_jit.farJump(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndexSpace)), WasmEntryPtrTag);
    } else {
        // Record the callee so the callee knows to look for it in updateCallsitesToCallUs.
        m_directCallees.testAndSet(m_info.toCodeIndex(functionIndexSpace));
        // Emit the call.
        Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;
        ASSERT(!m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace));
        Ref<IPIntCallee> callee = m_calleeGroup.ipintCalleeFromFunctionIndexSpace(functionIndexSpace);
        m_jit.storeWasmCalleeToCalleeCallFrame(TrustedImmPtr(CalleeBits::boxNativeCallee(callee.ptr())), sizeof(CallerFrameAndPC) - prologueStackPointerDelta());
        CCallHelpers::Call call = m_jit.threadSafePatchableNearTailCall();

        m_jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
            unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace });
        });
    }

    LOG_INSTRUCTION("ReturnCall", functionIndexSpace, arguments);

    // Because we're returning, we need to unbind all elements of the
    // expression stack here so they don't spuriously hold onto their bindings
    // in the subsequent unreachable code.
    unbindAllRegisters();
}


[[nodiscard]] PartialResult BBQJIT::addCall(unsigned callProfileIndex, FunctionSpaceIndex functionIndexSpace, const TypeDefinition& signature, ArgumentList& arguments, ResultList& results, CallType callType)
{
    emitIncrementCallProfileCount(callProfileIndex);
    JIT_COMMENT(m_jit, "calling functionIndexSpace: ", functionIndexSpace, ConditionalDump(!m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace), " functionIndex: ", functionIndexSpace - m_info.importFunctionCount()));

    if (callType == CallType::TailCall) {
        emitTailCall(functionIndexSpace, signature, arguments);
        return { };
    }

    const FunctionSignature& functionType = *signature.as<FunctionSignature>();
    CallInformation callInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    // Preserve caller-saved registers and other info
    prepareForExceptions();
    saveValuesAcrossCallAndPassArguments(arguments, callInfo, signature);

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace)) {
        static_assert(sizeof(WasmOrJSImportableFunctionCallLinkInfo) * maxImports < std::numeric_limits<int32_t>::max());
        RELEASE_ASSERT(JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndexSpace) < std::numeric_limits<int32_t>::max());
        m_jit.call(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndexSpace)), WasmEntryPtrTag);
    } else {
        // Record the callee so the callee knows to look for it in updateCallsitesToCallUs.
        ASSERT(m_info.toCodeIndex(functionIndexSpace) < m_info.internalFunctionCount());
        m_directCallees.testAndSet(m_info.toCodeIndex(functionIndexSpace));
        // Emit the call.
        Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;
        ASSERT(!m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace));
        Ref<IPIntCallee> callee = m_calleeGroup.ipintCalleeFromFunctionIndexSpace(functionIndexSpace);
        m_jit.storeWasmCalleeToCalleeCallFrame(TrustedImmPtr(CalleeBits::boxNativeCallee(callee.ptr())), 0);
        CCallHelpers::Call call = m_jit.threadSafePatchableNearCall();

        m_jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
            unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace });
        });
    }

    // Our callee could have tail called someone else and changed SP so we need to restore it. Do this before restoring our results since results are stored at the top of the reserved stack space.
    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), wasmScratchGPR));
#if CPU(ARM64)
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif

    // Push return value(s) onto the expression stack
    returnValuesFromCall(results, functionType, callInfo);

    if (m_info.callCanClobberInstance(functionIndexSpace) || m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace))
        restoreWebAssemblyGlobalStateAfterWasmCall();

    LOG_INSTRUCTION("Call", functionIndexSpace, arguments, "=> ", results);

    return { };
}

void BBQJIT::emitIndirectCall(const char* opcode, unsigned callProfileIndex, const Value& callee, GPRReg importableFunction, const TypeDefinition& signature, ArgumentList& arguments, ResultList& results)
{
    ASSERT(importableFunction == GPRInfo::nonPreservedNonArgumentGPR1);
    ASSERT(!RegisterSetBuilder::argumentGPRs().contains(importableFunction, IgnoreVectors));
    ASSERT(!RegisterSetBuilder::argumentGPRs().contains(wasmScratchGPR, IgnoreVectors));

    const auto& callingConvention = wasmCallingConvention();
    CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    prepareForExceptions();
    saveValuesAcrossCallAndPassArguments(arguments, wasmCalleeInfo, signature); // Keep in mind that this clobbers wasmScratchGPR and wasmScratchFPR.

    // Now, all argument GPRs are set up, but
    // 1. Callee save registers are kept, thus we can access GPRInfo::jitDataRegister.
    // 2. boxedCallee is placed in GPRInfo::nonPreservedNonArgumentGPR1, so not clobbered via arguments set up.
    // 3. wasmScratchGPR can be clobbered now.

    JumpList afterCall;
    m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfBoxedCallee()), wasmScratchGPR);
    m_jit.storeWasmCalleeToCalleeCallFrame(wasmScratchGPR);
    if (m_profile->isMegamorphic(callProfileIndex)) {
        m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfTargetInstance()), wasmScratchGPR);
        // Do a context switch if needed.
        Jump isSameInstanceBefore = m_jit.branchPtr(RelationalCondition::Equal, wasmScratchGPR, GPRInfo::wasmContextInstancePointer);
        m_jit.move(wasmScratchGPR, GPRInfo::wasmContextInstancePointer);
#if USE(JSVALUE64)
        loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
#endif
        isSameInstanceBefore.link(m_jit);
    } else {
        JumpList profilingDone;
        JumpList updateProfile;

        m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfTargetInstance()), m_jit.scratchRegister());
        Jump isSameInstanceBefore = m_jit.branchPtr(RelationalCondition::Equal, m_jit.scratchRegister(), GPRInfo::wasmContextInstancePointer);
        m_jit.move(m_jit.scratchRegister(), GPRInfo::wasmContextInstancePointer);
#if USE(JSVALUE64)
        loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
#endif
        m_jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, safeCast<int32_t>(BaselineData::offsetOfData() + sizeof(CallProfile) * callProfileIndex + CallProfile::offsetOfBoxedCallee())), wasmScratchGPR);
        m_jit.orPtr(TrustedImm32(CallProfile::Megamorphic), wasmScratchGPR);
        updateProfile.append(m_jit.jump());

        isSameInstanceBefore.link(m_jit);
        m_jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, safeCast<int32_t>(BaselineData::offsetOfData() + sizeof(CallProfile) * callProfileIndex + CallProfile::offsetOfBoxedCallee())), m_jit.scratchRegister());
        profilingDone.append(m_jit.branchPtr(CCallHelpers::Equal, wasmScratchGPR, m_jit.scratchRegister()));
        profilingDone.append(m_jit.branchTestPtr(CCallHelpers::NonZero, m_jit.scratchRegister(), TrustedImm32(CallProfile::Megamorphic)));

        updateProfile.append(m_jit.branchTestPtr(CCallHelpers::Zero, m_jit.scratchRegister()));
        m_jit.addPtr(TrustedImm32(safeCast<int32_t>(BaselineData::offsetOfData() + sizeof(CallProfile) * callProfileIndex)), GPRInfo::jitDataRegister, GPRInfo::wasmContextInstancePointer);
        m_jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(callPolymorphicCalleeGenerator).code()));
        afterCall.append(m_jit.jump());

        updateProfile.link(&m_jit);
        m_jit.storePtr(wasmScratchGPR, CCallHelpers::Address(GPRInfo::jitDataRegister, safeCast<int32_t>(BaselineData::offsetOfData() + sizeof(CallProfile) * callProfileIndex + CallProfile::offsetOfBoxedCallee())));
        profilingDone.link(m_jit);
    }

    m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()), wasmScratchGPR);
    m_jit.call(CCallHelpers::Address(wasmScratchGPR), WasmEntryPtrTag);

    // Our callee could have tail called someone else and changed SP so we need to restore it. Do this before restoring our results since results are stored at the top of the reserved stack space.
    afterCall.link(m_jit);
    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), wasmScratchGPR));
#if CPU(ARM64)
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, wasmScratchGPR, wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif

    returnValuesFromCall(results, *signature.as<FunctionSignature>(), wasmCalleeInfo);

    restoreWebAssemblyGlobalStateAfterWasmCall();

    LOG_INSTRUCTION(opcode, callee, arguments, "=> ", results);
}

void BBQJIT::emitIndirectTailCall(const char* opcode, const Value& callee, GPRReg importableFunction, const TypeDefinition& signature, ArgumentList& arguments)
{
    ASSERT(!RegisterSetBuilder::argumentGPRs().contains(importableFunction, IgnoreVectors));
    ASSERT(!RegisterSetBuilder::argumentGPRs().contains(wasmScratchGPR, IgnoreVectors));

    m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfBoxedCallee()), wasmScratchGPR);
    m_jit.storeWasmCalleeToCalleeCallFrame(wasmScratchGPR);

    // Do a context switch if needed.
    m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfTargetInstance()), wasmScratchGPR);
    Jump isSameInstanceBefore = m_jit.branchPtr(RelationalCondition::Equal, wasmScratchGPR, GPRInfo::wasmContextInstancePointer);
    m_jit.move(wasmScratchGPR, GPRInfo::wasmContextInstancePointer);
#if USE(JSVALUE64)
    loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
#endif
    isSameInstanceBefore.link(&m_jit);

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

    Vector<Value, 8> resolvedArguments;
    const unsigned calleeArgument = 1;
    resolvedArguments.reserveInitialCapacity(arguments.size() + calleeArgument + isX86() * 2);
    Vector<Location, 8> parameterLocations;
    parameterLocations.reserveInitialCapacity(arguments.size() + calleeArgument + isX86() * 2);

    // It's ok if we clobber our Wasm::Callee at this point since we can't hit a GC safepoint / throw an exception until we've tail called into the callee.
    // FIXME: We should just have addCallIndirect put this in the right place to begin with.
    resolvedArguments.append(Value::pinned(TypeKind::I64, Location::fromStackArgument(CCallHelpers::addressOfCalleeCalleeFromCallerPerspective(0).offset)));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP + Checked<int>(CallFrameSlot::callee * sizeof(Register))));

    // Save the old Frame Pointer for later and make sure the return address gets saved to its canonical location.
    emitRestoreCalleeSaves();
#if CPU(X86_64)
    // There are no remaining non-argument non-preserved gprs left on X86_64 so we have to shuffle FP to a temp slot.
    resolvedArguments.append(Value::pinned(pointerType(), Location::fromStack(0)));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP));

    resolvedArguments.append(Value::pinned(pointerType(), Location::fromStack(sizeof(Register))));
    parameterLocations.append(Location::fromStack(tailCallStackOffsetFromFP + Checked<int>(sizeof(Register))));
#elif CPU(ARM64) || CPU(ARM_THUMB2)
    auto preserved = callingConvention.argumentGPRs();
    preserved.add(importableFunction, IgnoreVectors);
    if constexpr (isARM64E())
        preserved.add(callingConvention.prologueScratchGPRs[0], IgnoreVectors);
    ScratchScope<1, 0> scratches(*this, WTF::move(preserved));
    GPRReg callerFramePointer = scratches.gpr(0);
    scratches.unbindPreserved();
    m_jit.loadPairPtr(MacroAssembler::framePointerRegister, callerFramePointer, MacroAssembler::linkRegister);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif

    for (unsigned i = 0; i < arguments.size(); i ++) {
        if (arguments[i].value().isConst())
            resolvedArguments.append(arguments[i].value());
        else
            resolvedArguments.append(Value::pinned(arguments[i].value().type(), locationOf(arguments[i])));

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

    m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()), importableFunction);
    m_jit.farJump(CCallHelpers::Address(importableFunction), WasmEntryPtrTag);
    LOG_INSTRUCTION(opcode, callee, arguments);

    // Because we're returning, we need to consume all elements of the
    // expression stack here so they don't spuriously hold onto their bindings
    // in the subsequent unreachable code.

    for (const auto& value : m_parser->expressionStack())
        consume(value);
}

[[nodiscard]] PartialResult BBQJIT::addCallIndirect(unsigned callProfileIndex, unsigned tableIndex, const TypeDefinition& originalSignature, ArgumentList& args, ResultList& results, CallType callType)
{
    emitIncrementCallProfileCount(callProfileIndex);
    Value calleeIndex = args.takeLast();
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    Location calleeIndexLocation;
    GPRReg importableFunction = GPRInfo::nonPreservedNonArgumentGPR1;
    {
        clobber(importableFunction);
        ScratchScope<0, 0> importableFunctionScope(*this, importableFunction);
        static_assert(GPRInfo::nonPreservedNonArgumentGPR0 == wasmScratchGPR);

        {
            ScratchScope<2, 0> scratches(*this);

            if (calleeIndex.isConst())
                emitMoveConst(calleeIndex, calleeIndexLocation = Location::fromGPR(scratches.gpr(1)));
            else
                calleeIndexLocation = loadIfNecessary(calleeIndex);

            GPRReg callableFunctionBuffer = scratches.gpr(0);

            ASSERT(tableIndex < m_info.tableCount());

            auto& tableInformation = m_info.table(tableIndex);
            if (tableInformation.maximum() && tableInformation.maximum().value() == tableInformation.initial()) {
                if (!tableIndex)
                    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfCachedTable0Buffer()), callableFunctionBuffer);
                else {
                    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfTable(m_info, tableIndex)), wasmScratchGPR);
                    if (!tableInformation.isImport())
                        m_jit.addPtr(TrustedImm32(FuncRefTable::offsetOfFunctionsForFixedSizedTable()), wasmScratchGPR, callableFunctionBuffer);
                    else
                        m_jit.loadPtr(Address(wasmScratchGPR, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
                }
                m_jit.move(TrustedImm32(tableInformation.initial()), wasmScratchGPR);
            } else {
                if (!tableIndex) {
                    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfCachedTable0Buffer()), callableFunctionBuffer);
                    m_jit.load32(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfCachedTable0Length()), wasmScratchGPR);
                } else {
                    m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfTable(m_info, tableIndex)), wasmScratchGPR);
                    m_jit.loadPtr(Address(wasmScratchGPR, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
                    m_jit.load32(Address(wasmScratchGPR, FuncRefTable::offsetOfLength()), wasmScratchGPR);
                }
            }
            ASSERT(calleeIndexLocation.isGPR());

            JIT_COMMENT(m_jit, "Check the index we are looking for is valid");
            recordJumpToThrowException(ExceptionType::OutOfBoundsCallIndirect, m_jit.branch32(RelationalCondition::AboveOrEqual, calleeIndexLocation.asGPR(), wasmScratchGPR));

            // Neither callableFunctionBuffer nor wasmScratchGPR are used before any of these
            // are def'd below, so we can reuse the registers and save some pressure.

            static_assert(sizeof(TypeIndex) == sizeof(void*));

            JIT_COMMENT(m_jit, "Compute the offset in the table index space we are looking for");
            if constexpr (hasOneBitSet(sizeof(FuncRefTable::Function))) {
                m_jit.zeroExtend32ToWord(calleeIndexLocation.asGPR(), calleeIndexLocation.asGPR());
#if CPU(ARM64)
                m_jit.addLeftShift64(callableFunctionBuffer, calleeIndexLocation.asGPR(), TrustedImm32(getLSBSet(sizeof(FuncRefTable::Function))), importableFunction);
#elif CPU(ARM)
                m_jit.lshift32(calleeIndexLocation.asGPR(), TrustedImm32(getLSBSet(sizeof(FuncRefTable::Function))), importableFunction);
                m_jit.addPtr(callableFunctionBuffer, importableFunction);
#else
                m_jit.lshift64(calleeIndexLocation.asGPR(), TrustedImm32(getLSBSet(sizeof(FuncRefTable::Function))), importableFunction);
                m_jit.addPtr(callableFunctionBuffer, importableFunction);
#endif
            } else {
                m_jit.move(TrustedImmPtr(sizeof(FuncRefTable::Function)), importableFunction);
#if CPU(ARM64)
                m_jit.multiplyAddZeroExtend32(calleeIndexLocation.asGPR(), importableFunction, callableFunctionBuffer, importableFunction);
#elif CPU(ARM)
                m_jit.mul32(calleeIndexLocation.asGPR(), importableFunction);
                m_jit.addPtr(callableFunctionBuffer, importableFunction);
#else
                m_jit.zeroExtend32ToWord(calleeIndexLocation.asGPR(), calleeIndexLocation.asGPR());
                m_jit.mul64(calleeIndexLocation.asGPR(), importableFunction);
                m_jit.addPtr(callableFunctionBuffer, importableFunction);
#endif
            }
            consume(calleeIndex);

            // FIXME: This seems wasteful to do two checks just for a nicer error message.
            // We should move just to use a single branch and then figure out what
            // error to use in the exception handler.

            auto targetRTT = TypeInformation::getCanonicalRTT(originalSignature.index());
            m_jit.loadPtr(CCallHelpers::Address(importableFunction, WasmToWasmImportableFunction::offsetOfRTT()), wasmScratchGPR);
            if (originalSignature.isFinalType())
                recordJumpToThrowException(ExceptionType::BadSignature, m_jit.branchPtr(CCallHelpers::NotEqual, wasmScratchGPR, TrustedImmPtr(targetRTT.ptr())));
            else {
                auto indexEqual = m_jit.branchPtr(CCallHelpers::Equal, wasmScratchGPR, TrustedImmPtr(targetRTT.ptr()));
                recordJumpToThrowException(ExceptionType::BadSignature, m_jit.branchTestPtr(ResultCondition::Zero, wasmScratchGPR));
                recordJumpToThrowException(ExceptionType::BadSignature, m_jit.branch32(CCallHelpers::BelowOrEqual, Address(wasmScratchGPR, RTT::offsetOfDisplaySizeExcludingThis()), TrustedImm32(targetRTT->displaySizeExcludingThis())));
                recordJumpToThrowException(ExceptionType::BadSignature, m_jit.branchPtr(CCallHelpers::NotEqual, CCallHelpers::Address(wasmScratchGPR, RTT::offsetOfData() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const RTT>)), TrustedImmPtr(targetRTT.ptr())));

                indexEqual.link(&m_jit);
            }
        }
    }

    JIT_COMMENT(m_jit, "Finished loading callee code");
    if (callType == CallType::Call)
        emitIndirectCall("CallIndirect", callProfileIndex, calleeIndex, importableFunction, signature, args, results);
    else
        emitIndirectTailCall("ReturnCallIndirect", calleeIndex, importableFunction, signature, args);
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addUnreachable()
{
    LOG_INSTRUCTION("Unreachable");
    emitThrowException(ExceptionType::Unreachable);
    return { };
}

[[nodiscard]] PartialResult BBQJIT::addCrash()
{
    m_jit.breakpoint();
    return { };
}

WasmOrigin BBQJIT::origin()
{
    if (!m_parser)
        return { { }, { } };

    OpcodeOrigin opcodeOrigin = OpcodeOrigin(m_parser->currentOpcode(), m_parser->currentOpcodeStartingOffset());
    switch (m_parser->currentOpcode()) {
    case OpType::Ext1:
    case OpType::ExtGC:
    case OpType::ExtAtomic:
    case OpType::ExtSIMD:
        opcodeOrigin = OpcodeOrigin(m_parser->currentOpcode(), m_parser->currentExtendedOpcode(), m_parser->currentOpcodeStartingOffset());
        break;
    default:
        break;
    }
    ASSERT(isValidOpType(static_cast<uint8_t>(opcodeOrigin.opcode())));
    WasmOrigin result { CallSiteIndex(m_callSiteIndex), opcodeOrigin };
    if (m_context.origins.isEmpty() || m_context.origins.last() != result)
        m_context.origins.append(result);
    return m_context.origins.last();
}

ALWAYS_INLINE void BBQJIT::willParseOpcode()
{
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

    auto origin = this->origin();
    m_pcToCodeOriginMapBuilder.appendItem(m_jit.label(), CodeOrigin(BytecodeIndex(origin.m_opcodeOrigin.location())));
    if (m_disassembler) [[unlikely]]
        m_disassembler->setOpcode(m_jit.label(), origin.m_opcodeOrigin);

    m_gprAllocator.assertAllValidRegistersAreUnlocked();
    m_fprAllocator.assertAllValidRegistersAreUnlocked();

#if ASSERT_ENABLED
    if (shouldFuseBranchCompare && isCompareOpType(m_prevOpcode)
        && (m_parser->currentOpcode() == OpType::BrIf || m_parser->currentOpcode() == OpType::If)) {
        m_prevOpcode = m_parser->currentOpcode();
        return;
    }
    for (Value value : m_justPoppedStack) {
        // Temps should have been consumed, we should have removed them from this list.
        ASSERT_WITH_MESSAGE(!value.isTemp(), "Temp(%u) was not consumed by the instruction that popped it!", value.asTemp());

        ASSERT(!value.isLocal()); // Change this if/when we start register-allocating locals.
    }
    m_prevOpcode = m_parser->currentOpcode();
    m_justPoppedStack.clear();
#endif
}

ALWAYS_INLINE void BBQJIT::willParseExtendedOpcode()
{
    auto origin = this->origin();
    m_pcToCodeOriginMapBuilder.appendItem(m_jit.label(), CodeOrigin(BytecodeIndex(origin.m_opcodeOrigin.location())));
    if (m_disassembler) [[unlikely]]
        m_disassembler->setOpcode(m_jit.label(), origin.m_opcodeOrigin);

    m_gprAllocator.assertAllValidRegistersAreUnlocked();
    m_fprAllocator.assertAllValidRegistersAreUnlocked();
}

ALWAYS_INLINE void BBQJIT::didParseOpcode()
{
}

BBQJIT::BranchFoldResult BBQJIT::tryFoldFusedBranchCompare(OpType opType, ExpressionType operand)
{
    if (!operand.isConst())
        return BranchNotFolded;
    switch (opType) {
    case OpType::I32Eqz:
        return operand.asI32() ? BranchNeverTaken : BranchAlwaysTaken;
    case OpType::I64Eqz:
        return operand.asI64() ? BranchNeverTaken : BranchAlwaysTaken;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Op type '%s' is not a unary comparison and should not have been fused.\n", makeString(opType).characters());
    }
    return BranchNotFolded;
}

BBQJIT::Jump BBQJIT::emitFusedBranchCompareBranch(OpType opType, ExpressionType, Location operandLocation)
{
    // Emit the negation of the intended branch.
    switch (opType) {
    case OpType::I32Eqz:
        return m_jit.branchTest32(ResultCondition::NonZero, operandLocation.asGPR());
    case OpType::I64Eqz:
#if USE(JSVALUE64)
        return m_jit.branchTest64(ResultCondition::NonZero, operandLocation.asGPR());
#else
        return m_jit.branchTest64(ResultCondition::NonZero, operandLocation.asGPRhi(), operandLocation.asGPRlo());
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Op type '%s' is not a unary comparison and should not have been fused.\n", makeString(opType).characters());
    }
}

PartialResult BBQJIT::addFusedBranchCompare(OpType opType, ControlType& target, ExpressionType operand, Stack& results)
{
    ASSERT(!operand.isNone());

    switch (tryFoldFusedBranchCompare(opType, operand)) {
    case BranchNeverTaken:
        return { };
    case BranchAlwaysTaken:
        currentControlData().flushAndSingleExit(*this, target, results, false, false);
        target.addBranch(m_jit.jump());
        return { };
    case BranchNotFolded:
        break;
    }

    {
        // Like in normal addBranch(), we can directly use the operand location
        // because it shouldn't interfere with flushAtBlockBoundary().
        Location operandLocation = loadIfNecessary(operand);
        consume(operand);

        LOG_INSTRUCTION("BranchCompare", makeString(opType).characters(), operand, operandLocation);

        currentControlData().flushAtBlockBoundary(*this, 0, results, false);
        Jump ifNotTaken = emitFusedBranchCompareBranch(opType, operand, operandLocation);
        currentControlData().addExit(*this, target.targetLocations(), results);
        target.addBranch(m_jit.jump());
        ifNotTaken.link(&m_jit);
        currentControlData().finalizeBlock(*this, target.targetLocations().size(), results, true);
    }

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addFusedIfCompare(OpType op, ExpressionType operand, BlockSignature&& signature, Stack& enclosingStack, ControlData& result, Stack& newStack)
{
    BranchFoldResult foldResult = tryFoldFusedBranchCompare(op, operand);

    ScratchScope<0, 1> scratches(*this);
    Location operandLocation;
    RegisterSet liveScratchGPRs, liveScratchFPRs;
    if (foldResult == BranchNotFolded) {
        if (!operand.isConst())
            operandLocation = loadIfNecessary(operand);
        else if (operand.isFloat()) {
            operandLocation = Location::fromFPR(scratches.fpr(0));
            emitMove(operand, operandLocation);
        }

        if (operandLocation.isGPR())
            liveScratchGPRs.add(operandLocation.asGPR(), IgnoreVectors);
        else if (operandLocation.isGPR2()) {
            liveScratchGPRs.add(operandLocation.asGPRlo(), IgnoreVectors);
            liveScratchGPRs.add(operandLocation.asGPRhi(), IgnoreVectors);
        } else if (operandLocation.isFPR())
            liveScratchFPRs.add(operandLocation.asFPR(), operand.type() == TypeKind::V128 ? Width128 : Width64);
    }
    if (!liveScratchFPRs.contains(scratches.fpr(0), IgnoreVectors))
        scratches.unbindEarly();

    consume(operand);

    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::If, WTF::move(signature), height, liveScratchGPRs, liveScratchFPRs);

    // Despite being conditional, if doesn't need to worry about diverging expression stacks at block boundaries, so it doesn't need multiple exits.
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("IfCompare", makeString(op).characters(), result.signature(), operand, operandLocation);
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);

    result.startBlock(*this, newStack);
    if (foldResult == BranchNeverTaken)
        result.setIfBranch(m_jit.jump()); // Emit direct branch if we know the condition is false.
    else if (foldResult == BranchNotFolded) // Otherwise, we only emit a branch at all if we don't know the condition statically.
        result.setIfBranch(emitFusedBranchCompareBranch(op, operand, operandLocation));
    return { };
}

BBQJIT::BranchFoldResult BBQJIT::tryFoldFusedBranchCompare(OpType opType, ExpressionType left, ExpressionType right)
{
    if (!left.isConst() || !right.isConst())
        return BranchNotFolded;
    switch (opType) {
    case OpType::I32LtS:
        return left.asI32() < right.asI32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32LtU:
        return static_cast<uint32_t>(left.asI32()) < static_cast<uint32_t>(right.asI32()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32GtS:
        return left.asI32() > right.asI32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32GtU:
        return static_cast<uint32_t>(left.asI32()) > static_cast<uint32_t>(right.asI32()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32LeS:
        return left.asI32() <= right.asI32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32LeU:
        return static_cast<uint32_t>(left.asI32()) <= static_cast<uint32_t>(right.asI32()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32GeS:
        return left.asI32() >= right.asI32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32GeU:
        return static_cast<uint32_t>(left.asI32()) >= static_cast<uint32_t>(right.asI32()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32Eq:
        return left.asI32() == right.asI32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I32Ne:
        return left.asI32() != right.asI32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64LtS:
        return left.asI64() < right.asI64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64LtU:
        return static_cast<uint64_t>(left.asI64()) < static_cast<uint64_t>(right.asI64()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64GtS:
        return left.asI64() > right.asI64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64GtU:
        return static_cast<uint64_t>(left.asI64()) > static_cast<uint64_t>(right.asI64()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64LeS:
        return left.asI64() <= right.asI64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64LeU:
        return static_cast<uint64_t>(left.asI64()) <= static_cast<uint64_t>(right.asI64()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64GeS:
        return left.asI64() >= right.asI64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64GeU:
        return static_cast<uint64_t>(left.asI64()) >= static_cast<uint64_t>(right.asI64()) ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64Eq:
        return left.asI64() == right.asI64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::I64Ne:
        return left.asI64() != right.asI64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F32Lt:
        return left.asF32() < right.asF32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F32Gt:
        return left.asF32() > right.asF32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F32Le:
        return left.asF32() <= right.asF32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F32Ge:
        return left.asF32() >= right.asF32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F32Eq:
        return left.asF32() == right.asF32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F32Ne:
        return left.asF32() != right.asF32() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F64Lt:
        return left.asF64() < right.asF64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F64Gt:
        return left.asF64() > right.asF64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F64Le:
        return left.asF64() <= right.asF64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F64Ge:
        return left.asF64() >= right.asF64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F64Eq:
        return left.asF64() == right.asF64() ? BranchAlwaysTaken : BranchNeverTaken;
    case OpType::F64Ne:
        return left.asF64() != right.asF64() ? BranchAlwaysTaken : BranchNeverTaken;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Op type '%s' is not a binary comparison and should not have been fused.\n", makeString(opType).characters());
    }
}

static MacroAssembler::Jump emitBranchI32(CCallHelpers& jit, MacroAssembler::RelationalCondition condition, Value left, Location leftLocation, Value right, Location rightLocation)
{
    if (right.isConst())
        return jit.branch32(condition, leftLocation.asGPR(), MacroAssembler::TrustedImm32(right.asI32()));
    if (left.isConst())
        return jit.branch32(condition, MacroAssembler::TrustedImm32(left.asI32()), rightLocation.asGPR());
    return jit.branch32(condition, leftLocation.asGPR(), rightLocation.asGPR());
}

static MacroAssembler::Jump emitBranchI64(CCallHelpers& jit, MacroAssembler::RelationalCondition condition, Value left, Location leftLocation, Value right, Location rightLocation)
{
#if USE(JSVALUE64)
    if (right.isConst())
        return jit.branch64(condition, leftLocation.asGPR(), MacroAssembler::Imm64(right.asI64()));
    if (left.isConst())
        return jit.branch64(MacroAssembler::commute(condition), rightLocation.asGPR(), MacroAssembler::Imm64(left.asI64()));
    return jit.branch64(condition, leftLocation.asGPR(), rightLocation.asGPR());
#else
    if (right.isConst())
        return jit.branch64(condition, leftLocation.asGPRhi(), leftLocation.asGPRlo(), MacroAssembler::TrustedImm64(right.asI64()));
    if (left.isConst())
        return jit.branch64(MacroAssembler::commute(condition), rightLocation.asGPRhi(), rightLocation.asGPRlo(), MacroAssembler::TrustedImm64(left.asI64()));
    return jit.branch64(condition, leftLocation.asGPRhi(), leftLocation.asGPRlo(), rightLocation.asGPRhi(), rightLocation.asGPRlo());
#endif
}

static MacroAssembler::Jump emitBranchF32(CCallHelpers& jit, MacroAssembler::DoubleCondition condition, Value, Location leftLocation, Value, Location rightLocation)
{
    return jit.branchFloat(condition, leftLocation.asFPR(), rightLocation.asFPR());
}

static MacroAssembler::Jump emitBranchF64(CCallHelpers& jit, MacroAssembler::DoubleCondition condition, Value, Location leftLocation, Value, Location rightLocation)
{
    return jit.branchDouble(condition, leftLocation.asFPR(), rightLocation.asFPR());
}

BBQJIT::Jump BBQJIT::emitFusedBranchCompareBranch(OpType opType, ExpressionType left, Location leftLocation, ExpressionType right, Location rightLocation)
{
    // Emit a branch with the inverse of the comparison. We're generating the "branch-if-false" case.
    switch (opType) {
    case OpType::I32LtS:
        return emitBranchI32(m_jit, RelationalCondition::GreaterThanOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I32LtU:
        return emitBranchI32(m_jit, RelationalCondition::AboveOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I32GtS:
        return emitBranchI32(m_jit, RelationalCondition::LessThanOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I32GtU:
        return emitBranchI32(m_jit, RelationalCondition::BelowOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I32LeS:
        return emitBranchI32(m_jit, RelationalCondition::GreaterThan, left, leftLocation, right, rightLocation);
    case OpType::I32LeU:
        return emitBranchI32(m_jit, RelationalCondition::Above, left, leftLocation, right, rightLocation);
    case OpType::I32GeS:
        return emitBranchI32(m_jit, RelationalCondition::LessThan, left, leftLocation, right, rightLocation);
    case OpType::I32GeU:
        return emitBranchI32(m_jit, RelationalCondition::Below, left, leftLocation, right, rightLocation);
    case OpType::I32Eq:
        return emitBranchI32(m_jit, RelationalCondition::NotEqual, left, leftLocation, right, rightLocation);
    case OpType::I32Ne:
        return emitBranchI32(m_jit, RelationalCondition::Equal, left, leftLocation, right, rightLocation);
    case OpType::I64LtS:
        return emitBranchI64(m_jit, RelationalCondition::GreaterThanOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I64LtU:
        return emitBranchI64(m_jit, RelationalCondition::AboveOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I64GtS:
        return emitBranchI64(m_jit, RelationalCondition::LessThanOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I64GtU:
        return emitBranchI64(m_jit, RelationalCondition::BelowOrEqual, left, leftLocation, right, rightLocation);
    case OpType::I64LeS:
        return emitBranchI64(m_jit, RelationalCondition::GreaterThan, left, leftLocation, right, rightLocation);
    case OpType::I64LeU:
        return emitBranchI64(m_jit, RelationalCondition::Above, left, leftLocation, right, rightLocation);
    case OpType::I64GeS:
        return emitBranchI64(m_jit, RelationalCondition::LessThan, left, leftLocation, right, rightLocation);
    case OpType::I64GeU:
        return emitBranchI64(m_jit, RelationalCondition::Below, left, leftLocation, right, rightLocation);
    case OpType::I64Eq:
        return emitBranchI64(m_jit, RelationalCondition::NotEqual, left, leftLocation, right, rightLocation);
    case OpType::I64Ne:
        return emitBranchI64(m_jit, RelationalCondition::Equal, left, leftLocation, right, rightLocation);
    case OpType::F32Lt:
        return emitBranchF32(m_jit, MacroAssembler::invert(DoubleCondition::DoubleLessThanAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F32Gt:
        return emitBranchF32(m_jit, MacroAssembler::invert(DoubleCondition::DoubleGreaterThanAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F32Le:
        return emitBranchF32(m_jit, MacroAssembler::invert(DoubleCondition::DoubleLessThanOrEqualAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F32Ge:
        return emitBranchF32(m_jit, MacroAssembler::invert(DoubleCondition::DoubleGreaterThanOrEqualAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F32Eq:
        return emitBranchF32(m_jit, MacroAssembler::invert(DoubleCondition::DoubleEqualAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F32Ne:
        return emitBranchF32(m_jit, MacroAssembler::invert(DoubleCondition::DoubleNotEqualOrUnordered), left, leftLocation, right, rightLocation);
    case OpType::F64Lt:
        return emitBranchF64(m_jit, MacroAssembler::invert(DoubleCondition::DoubleLessThanAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F64Gt:
        return emitBranchF64(m_jit, MacroAssembler::invert(DoubleCondition::DoubleGreaterThanAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F64Le:
        return emitBranchF64(m_jit, MacroAssembler::invert(DoubleCondition::DoubleLessThanOrEqualAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F64Ge:
        return emitBranchF64(m_jit, MacroAssembler::invert(DoubleCondition::DoubleGreaterThanOrEqualAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F64Eq:
        return emitBranchF64(m_jit, MacroAssembler::invert(DoubleCondition::DoubleEqualAndOrdered), left, leftLocation, right, rightLocation);
    case OpType::F64Ne:
        return emitBranchF64(m_jit, MacroAssembler::invert(DoubleCondition::DoubleNotEqualOrUnordered), left, leftLocation, right, rightLocation);
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Op type '%s' is not a binary comparison and should not have been fused.\n", makeString(opType).characters());
    }
}

PartialResult BBQJIT::addFusedBranchCompare(OpType opType, ControlType& target, ExpressionType left, ExpressionType right, Stack& results)
{
    switch (tryFoldFusedBranchCompare(opType, left, right)) {
    case BranchNeverTaken:
        return { };
    case BranchAlwaysTaken:
        currentControlData().flushAndSingleExit(*this, target, results, false, false);
        target.addBranch(m_jit.jump());
        return { };
    case BranchNotFolded:
        break;
    }

    {
        Location leftLocation, rightLocation;

        if (!left.isConst())
            leftLocation = loadIfNecessary(left);
        else if (left.isFloat()) // Materialize floats here too, since they don't have a good immediate lowering.
            emitMove(left, leftLocation = Location::fromFPR(wasmScratchFPR));
        if (!right.isConst())
            rightLocation = loadIfNecessary(right);
        else if (right.isFloat())
            emitMove(right, rightLocation = Location::fromFPR(wasmScratchFPR));

        consume(left);
        consume(right);

        LOG_INSTRUCTION("BranchCompare", makeString(opType).characters(), left, leftLocation, right, rightLocation);

        currentControlData().flushAtBlockBoundary(*this, 0, results, false);
        Jump ifNotTaken = emitFusedBranchCompareBranch(opType, left, leftLocation, right, rightLocation);
        currentControlData().addExit(*this, target.targetLocations(), results);
        target.addBranch(m_jit.jump());
        ifNotTaken.link(&m_jit);
        currentControlData().finalizeBlock(*this, target.targetLocations().size(), results, true);
    }

    return { };
}

[[nodiscard]] PartialResult BBQJIT::addFusedIfCompare(OpType op, ExpressionType left, ExpressionType right, BlockSignature&& signature, Stack& enclosingStack, ControlData& result, Stack& newStack)
{
    BranchFoldResult foldResult = tryFoldFusedBranchCompare(op, left, right);

    Location leftLocation, rightLocation;
    RegisterSet liveScratchGPRs, liveScratchFPRs;
    if (foldResult == BranchNotFolded) {
        ASSERT(!left.isConst() || !right.isConst()); // If they're both constants, we should have folded.

        if (!left.isConst())
            leftLocation = loadIfNecessary(left);
        else if (left.isFloat())
            emitMove(left, leftLocation = Location::fromFPR(wasmScratchFPR));
        if (leftLocation.isGPR())
            liveScratchGPRs.add(leftLocation.asGPR(), IgnoreVectors);
        else if (leftLocation.isGPR2()) {
            liveScratchGPRs.add(leftLocation.asGPRlo(), IgnoreVectors);
            liveScratchGPRs.add(leftLocation.asGPRhi(), IgnoreVectors);
        } else if (leftLocation.isFPR())
            liveScratchFPRs.add(leftLocation.asFPR(), left.type() == TypeKind::V128 ? Width128 : Width64);

        if (!right.isConst())
            rightLocation = loadIfNecessary(right);
        else if (right.isFloat())
            emitMove(right, rightLocation = Location::fromFPR(wasmScratchFPR));
        if (rightLocation.isGPR())
            liveScratchGPRs.add(rightLocation.asGPR(), IgnoreVectors);
        else if (rightLocation.isGPR2()) {
            liveScratchGPRs.add(rightLocation.asGPRlo(), IgnoreVectors);
            liveScratchGPRs.add(rightLocation.asGPRhi(), IgnoreVectors);
        } else if (rightLocation.isFPR())
            liveScratchFPRs.add(rightLocation.asFPR(), right.type() == TypeKind::V128 ? Width128 : Width64);
    }
    consume(left);
    consume(right);

    auto height = currentControlData().enclosedHeight() + currentControlData().implicitSlots() + enclosingStack.size() - signature.argumentCount();
    result = ControlData(*this, BlockType::If, WTF::move(signature), height, liveScratchGPRs, liveScratchFPRs);

    // Despite being conditional, if doesn't need to worry about diverging expression stacks at block boundaries, so it doesn't need multiple exits.
    currentControlData().flushAndSingleExit(*this, result, enclosingStack, true, false);

    LOG_INSTRUCTION("IfCompare", makeString(op).characters(), result.signature(), left, leftLocation, right, rightLocation);
    LOG_INDENT();
    splitStack(result.signature(), enclosingStack, newStack);

    result.startBlock(*this, newStack);
    if (foldResult == BranchNeverTaken)
        result.setIfBranch(m_jit.jump()); // Emit direct branch if we know the condition is false.
    else if (foldResult == BranchNotFolded) // Otherwise, we only emit a branch at all if we don't know the condition statically.
        result.setIfBranch(emitFusedBranchCompareBranch(op, left, leftLocation, right, rightLocation));
    return { };
}

// SIMD

bool BBQJIT::usesSIMD()
{
    return m_usesSIMD;
}

void BBQJIT::dump(const ControlStack&, const Stack*) { }
void BBQJIT::didFinishParsingLocals() { }

void BBQJIT::didPopValueFromStack(Value value, ASCIILiteral)
{
    UNUSED_PARAM(value);
#if ASSERT_ENABLED
    m_justPoppedStack.append(value);
#endif
}

void BBQJIT::finalize()
{
    if (m_disassembler) [[unlikely]]
        m_disassembler->setEndOfCode(m_jit.label());
}

Vector<UnlinkedHandlerInfo>&& BBQJIT::takeExceptionHandlers()
{
    return WTF::move(m_exceptionHandlers);
}

FixedBitVector&& BBQJIT::takeDirectCallees()
{
    return WTF::move(m_directCallees);
}

Vector<CCallHelpers::Label>&& BBQJIT::takeCatchEntrypoints()
{
    return WTF::move(m_catchEntrypoints);
}

Box<PCToCodeOriginMapBuilder> BBQJIT::takePCToCodeOriginMapBuilder()
{
    if (m_pcToCodeOriginMapBuilder.didBuildMapping())
        return Box<PCToCodeOriginMapBuilder>::create(WTF::move(m_pcToCodeOriginMapBuilder));
    return nullptr;
}

std::unique_ptr<BBQDisassembler> BBQJIT::takeDisassembler()
{
    return WTF::move(m_disassembler);
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
    ASSERT(location.isRegister());
    if (location.isGPR()) {
        m_gprAllocator.setSpillHint(location.asGPR(), key);
    } else if (location.isFPR()) {
        m_fprAllocator.setSpillHint(location.asFPR(), key);
    } else if (location.isGPR2()) {
        m_gprAllocator.setSpillHint(location.asGPRhi(), key);
        m_gprAllocator.setSpillHint(location.asGPRlo(), key);
    }
}

void BBQJIT::increaseLRUKey(Location location)
{
    setLRUKey(location, nextLRUKey());
}

Location BBQJIT::allocate(Value value)
{
    return BBQJIT::allocateWithHint(value, Location::none());
}

Location BBQJIT::allocateWithHint(Value value, Location hint)
{
    if (value.isPinned())
        return value.asPinned();

    Location result;
    if (value.isFloat())
        result = Location::fromFPR(m_fprAllocator.allocate(*this, RegisterBinding::fromValue(value), nextLRUKey(), hint.isFPR() ? hint.asFPR() : InvalidFPRReg));
    else if (!typeNeedsGPR2(value.type()))
        result = Location::fromGPR(m_gprAllocator.allocate(*this, RegisterBinding::fromValue(value), nextLRUKey(), hint.isGPR() ? hint.asGPR() : InvalidGPRReg));
    else if constexpr (is32Bit()) {
        // FIXME: Add hint support for GPR2s.
        auto lruKey = nextLRUKey();
        GPRReg lo = m_gprAllocator.allocate(*this, RegisterBinding::fromValue(value), lruKey);
        GPRReg hi = m_gprAllocator.allocate(*this, RegisterBinding::fromValue(value), lruKey);
        result = Location::fromGPR2(hi, lo);
    } else
        RELEASE_ASSERT_NOT_REACHED();

    if (value.isLocal())
        currentControlData().touch(value.asLocal());
    if (Options::verboseBBQJITAllocation()) [[unlikely]]
        dataLogLn("BBQ\tAllocated ", value, " with type ", makeString(value.type()), " to ", result);
    return bind(value, result);
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
    if (Options::verboseBBQJITAllocation()) [[unlikely]]
        dataLogLn("BBQ\tLoading value ", value, " if necessary");
    Location loc = locationOf(value);
    if (loc.isMemory()) {
        if (Options::verboseBBQJITAllocation()) [[unlikely]]
            dataLogLn("BBQ\tLoading local ", value, " to ", loc);
        Location dst = allocate(value); // Find a register to store this value. Might spill older values if we run out.
        emitLoad(value.type(), loc, dst); // Generate the load instructions to move the value into the register.
        if (value.isLocal())
            currentControlData().touch(value.asLocal());
        loc = dst;
    } else
        increaseLRUKey(loc);
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

#if ASSERT_ENABLED
    m_justPoppedStack.removeFirstMatching([&](Value& popped) -> bool {
        return popped.isTemp() && value.isTemp() && popped.asTemp() == value.asTemp();
    });
#endif
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

    // Some callers have already updated the allocator to bind value to loc. But if not, we should do that bookkeeping now.
    if (loc.isRegister()) {
        if (value.isFloat()) {
            if (m_fprAllocator.freeRegisters().contains(loc.asFPR(), Width::Width128))
                m_fprAllocator.bind(loc.asFPR(), RegisterBinding::fromValue(value), std::nullopt);
            ASSERT(bindingFor(loc.asFPR()) == RegisterBinding::fromValue(value));
        } else if (loc.isGPR2()) {
            if (m_gprAllocator.freeRegisters().contains(loc.asGPRlo(), IgnoreVectors))
                m_gprAllocator.bind(loc.asGPRlo(), RegisterBinding::fromValue(value), std::nullopt);
            ASSERT(bindingFor(loc.asGPRlo()) == RegisterBinding::fromValue(value));
            if (m_gprAllocator.freeRegisters().contains(loc.asGPRhi(), IgnoreVectors))
                m_gprAllocator.bind(loc.asGPRhi(), RegisterBinding::fromValue(value), std::nullopt);
            ASSERT(bindingFor(loc.asGPRhi()) == RegisterBinding::fromValue(value));
        } else {
            if (m_gprAllocator.freeRegisters().contains(loc.asGPR(), IgnoreVectors))
                m_gprAllocator.bind(loc.asGPR(), RegisterBinding::fromValue(value), std::nullopt);
            ASSERT(bindingFor(loc.asGPR()) == RegisterBinding::fromValue(value));
        }
    }
    if (value.isLocal())
        m_locals[value.asLocal()] = loc;
    else if (value.isTemp()) {
        if (m_temps.size() <= value.asTemp())
            m_temps.grow(value.asTemp() + 1);
        m_temps[value.asTemp()] = loc;
    }

    if (Options::verboseBBQJITAllocation()) [[unlikely]]
        dataLogLn("BBQ\tBound value ", value, " to ", loc);

    return loc;
}

void BBQJIT::unbind(Value value, Location loc)
{
    // Unbind a value from a location. Doesn't touch the LRU, but updates the register set
    // and local/temp tables accordingly.
    if (loc.isFPR())
        m_fprAllocator.unbind(loc.asFPR());
    else if (loc.isGPR())
        m_gprAllocator.unbind(loc.asGPR());
    else if (loc.isGPR2()) {
        m_gprAllocator.unbind(loc.asGPRlo());
        m_gprAllocator.unbind(loc.asGPRhi());
    }
    if (value.isLocal())
        m_locals[value.asLocal()] = m_localSlots[value.asLocal()];
    else if (value.isTemp())
        m_temps[value.asTemp()] = Location::none();

    if (Options::verboseBBQJITAllocation()) [[unlikely]]
        dataLogLn("BBQ\tUnbound value ", value, " from ", loc);
}

void BBQJIT::unbindAllRegisters()
{
    auto doUnbind = [&](Reg reg) {
        const auto& binding = bindingFor(reg);
        Value value = binding.toValue();
        if (value.isNone())
            return;

        ASSERT(value.isTemp() || value.isLocal());
        unbind(value, locationOfWithoutBinding(value));
    };

    for (auto reg : m_gprAllocator.validRegisters())
        doUnbind(reg);

    for (auto reg : m_fprAllocator.validRegisters())
        doUnbind(reg);
}

Location BBQJIT::canonicalSlot(Value value)
{
    ASSERT(value.isLocal() || value.isTemp());
    if (value.isLocal())
        return m_localSlots[value.asLocal()];

    LocalOrTempIndex tempIndex = value.asTemp();
    int slotOffset = WTF::roundUpToMultipleOf<tempSlotSize>(m_localAndCalleeSaveStorage) + (tempIndex + 1) * tempSlotSize;
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

void BBQJIT::emitArrayGetPayload(StorageType type, GPRReg arrayGPR, GPRReg payloadGPR)
{
    ASSERT(arrayGPR != payloadGPR);
    if (!JSWebAssemblyArray::needsAlignmentCheck(type)) {
        m_jit.addPtr(MacroAssembler::TrustedImm32(JSWebAssemblyArray::offsetOfData()), arrayGPR, payloadGPR);
        return;
    }

    // Round-up to 16x for PreciseAllocation + V128 array data handling.
    m_jit.addPtr(MacroAssembler::TrustedImm32(JSWebAssemblyArray::offsetOfData() + 15), arrayGPR, payloadGPR);
    m_jit.andPtr(MacroAssembler::TrustedImm32(-16), payloadGPR);
}

} // namespace JSC::Wasm::BBQJITImpl

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileBBQ(CompilationContext& compilationContext, IPIntCallee& profiledCallee, BBQCallee& callee, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, Module& module, CalleeGroup& calleeGroup, const ModuleInformation& info, MemoryMode mode, FunctionCodeIndex functionIndex)
{
    CompilerTimingScope totalTime("BBQ"_s, "Total BBQ"_s);

    Thunks::singleton().stub(catchInWasmThunkGenerator);

    auto result = makeUnique<InternalFunction>();

    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

    BBQJIT irGenerator(compilationContext, signature, module, calleeGroup, profiledCallee, callee, function, functionIndex, info, unlinkedWasmToWasmCalls, mode, result.get());
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

void BBQJIT::emitPushCalleeSaves()
{
    size_t stackSizeForCalleeSaves = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(RegisterAtOffsetList::bbqCalleeSaveRegisters().registerCount() * sizeof(UCPURegister));
#if CPU(X86_64) || CPU(ARM64)
    m_jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32(stackSizeForCalleeSaves), MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32(stackSizeForCalleeSaves), wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);
#endif
    m_jit.emitSaveCalleeSavesFor(&RegisterAtOffsetList::bbqCalleeSaveRegisters());
}

void BBQJIT::emitRestoreCalleeSaves()
{
    m_jit.emitRestoreCalleeSavesFor(&RegisterAtOffsetList::bbqCalleeSaveRegisters());
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
