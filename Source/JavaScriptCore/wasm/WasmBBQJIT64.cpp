/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "WasmBBQJIT64.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "WasmBBQJIT.h"

#if ENABLE(WEBASSEMBLY_BBQJIT)
#if USE(JSVALUE64)

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
#include "WasmCallingConvention.h"
#include "WasmCompilationMode.h"
#include "WasmFaultSignalHandler.h"
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
#include "WebAssemblyFunctionBase.h"
#include <bit>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/SmallSet.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm { namespace BBQJITImpl {

Location Location::fromArgumentLocation(ArgumentLocation argLocation, TypeKind)
{
    switch (argLocation.location.kind()) {
    case ValueLocation::Kind::GPRRegister:
        return Location::fromGPR(argLocation.location.jsr().gpr());
    case ValueLocation::Kind::FPRRegister:
        return Location::fromFPR(argLocation.location.fpr());
    case ValueLocation::Kind::StackArgument:
        return Location::fromStackArgument(argLocation.location.offsetFromSP());
    case ValueLocation::Kind::Stack:
        return Location::fromStack(argLocation.location.offsetFromFP());
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool Location::isRegister() const
{
    return isGPR() || isFPR();
}

uint32_t BBQJIT::sizeOfType(TypeKind type)
{
    switch (type) {
    case TypeKind::I32:
    case TypeKind::F32:
        return 4;
    case TypeKind::I64:
    case TypeKind::F64:
        return 8;
    case TypeKind::V128:
        return 16;
    case TypeKind::I31ref:
    case TypeKind::Func:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Rec:
    case TypeKind::Sub:
    case TypeKind::Subfinal:
    case TypeKind::Struct:
    case TypeKind::Structref:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Array:
    case TypeKind::Arrayref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
        return sizeof(EncodedJSValue);
    case TypeKind::Void:
        return 0;
    }
    return 0;
}

// This function is intentionally not using implicitSlots since arguments and results should not include implicit slot.
Location ControlData::allocateArgumentOrResult(BBQJIT& generator, TypeKind type, unsigned i, RegisterSet& remainingGPRs, RegisterSet& remainingFPRs)
{
    switch (type) {
    case TypeKind::V128:
    case TypeKind::F32:
    case TypeKind::F64: {
        if (remainingFPRs.isEmpty())
            return generator.canonicalSlot(Value::fromTemp(type, this->enclosedHeight() + i));
        auto reg = *remainingFPRs.begin();
        remainingFPRs.remove(reg);
        return Location::fromFPR(reg.fpr());
    }
    default:
        if (remainingGPRs.isEmpty())
            return generator.canonicalSlot(Value::fromTemp(type, this->enclosedHeight() + i));
        auto reg = *remainingGPRs.begin();
        remainingGPRs.remove(reg);
        return Location::fromGPR(reg.gpr());
    }
}

Value BBQJIT::instanceValue()
{
    return Value::pinned(TypeKind::I64, Location::fromGPR(GPRInfo::wasmContextInstancePointer));
}

// Tables
PartialResult WARN_UNUSED_RETURN BBQJIT::addTableGet(unsigned tableIndex, Value index, Value& result)
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index.type() == TypeKind::I32);
    TypeKind returnType = m_info.tables[tableIndex].wasmType().kind;
    ASSERT(typeKindSizeInBytes(returnType) == 8);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(tableIndex),
        index
    };
    result = topValue(returnType);
    emitCCall(&operationGetWasmTableElement, arguments, result);
    Location resultLocation = loadIfNecessary(result);

    LOG_INSTRUCTION("TableGet", tableIndex, index, RESULT(result));

    recordJumpToThrowException(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest64(ResultCondition::Zero, resultLocation.asGPR()));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::getGlobal(uint32_t index, Value& result)
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    int32_t offset = JSWebAssemblyInstance::offsetOfGlobal(m_info, index);
    Value globalValue = Value::pinned(type.kind, Location::fromGlobal(offset));

    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        result = topValue(type.kind);
        emitLoad(globalValue, loadIfNecessary(result));
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, offset), wasmScratchGPR);
        result = topValue(type.kind);
        Location resultLocation = allocate(result);
        switch (type.kind) {
        case TypeKind::I32:
            m_jit.load32(Address(wasmScratchGPR), resultLocation.asGPR());
            break;
        case TypeKind::I64:
            m_jit.load64(Address(wasmScratchGPR), resultLocation.asGPR());
            break;
        case TypeKind::F32:
            m_jit.loadFloat(Address(wasmScratchGPR), resultLocation.asFPR());
            break;
        case TypeKind::F64:
            m_jit.loadDouble(Address(wasmScratchGPR), resultLocation.asFPR());
            break;
        case TypeKind::V128:
            m_jit.loadVector(Address(wasmScratchGPR), resultLocation.asFPR());
            break;
        case TypeKind::Func:
        case TypeKind::Funcref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Rec:
        case TypeKind::Sub:
        case TypeKind::Subfinal:
        case TypeKind::Struct:
        case TypeKind::Structref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
            m_jit.load64(Address(wasmScratchGPR), resultLocation.asGPR());
            break;
        case TypeKind::Void:
            break;
        }
        break;
    }

    LOG_INSTRUCTION("GetGlobal", index, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::setGlobal(uint32_t index, Value value)
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    int32_t offset = JSWebAssemblyInstance::offsetOfGlobal(m_info, index);
    Location valueLocation = locationOf(value);

    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
        emitMove(value, Location::fromGlobal(offset));
        consume(value);
        if (isRefType(type))
            emitWriteBarrier(GPRInfo::wasmContextInstancePointer);
        break;
    }
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, offset), wasmScratchGPR);

        Location valueLocation;
        if (value.isConst() && value.isFloat()) {
            ScratchScope<0, 1> scratches(*this);
            valueLocation = Location::fromFPR(scratches.fpr(0));
            emitMoveConst(value, valueLocation);
        } else if (value.isConst()) {
            ScratchScope<1, 0> scratches(*this);
            valueLocation = Location::fromGPR(scratches.gpr(0));
            emitMoveConst(value, valueLocation);
        } else
            valueLocation = loadIfNecessary(value);
        ASSERT(valueLocation.isRegister());
        consume(value);

        switch (type.kind) {
        case TypeKind::I32:
            m_jit.store32(valueLocation.asGPR(), Address(wasmScratchGPR));
            break;
        case TypeKind::I64:
            m_jit.store64(valueLocation.asGPR(), Address(wasmScratchGPR));
            break;
        case TypeKind::F32:
            m_jit.storeFloat(valueLocation.asFPR(), Address(wasmScratchGPR));
            break;
        case TypeKind::F64:
            m_jit.storeDouble(valueLocation.asFPR(), Address(wasmScratchGPR));
            break;
        case TypeKind::V128:
            m_jit.storeVector(valueLocation.asFPR(), Address(wasmScratchGPR));
            break;
        case TypeKind::Func:
        case TypeKind::Funcref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Rec:
        case TypeKind::Sub:
        case TypeKind::Subfinal:
        case TypeKind::Struct:
        case TypeKind::Structref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
            m_jit.store64(valueLocation.asGPR(), Address(wasmScratchGPR));
            break;
        case TypeKind::Void:
            break;
        }
        if (isRefType(type)) {
            m_jit.loadPtr(Address(wasmScratchGPR, Wasm::Global::offsetOfOwner() - Wasm::Global::offsetOfValue()), wasmScratchGPR);
            emitWriteBarrier(wasmScratchGPR);
        }
        break;
    }
    }

    LOG_INSTRUCTION("SetGlobal", index, value, valueLocation);

    return { };
}

// Memory

PartialResult WARN_UNUSED_RETURN BBQJIT::load(LoadOpType loadOp, Value pointer, Value& result, uint32_t uoffset)
{
    if (sumOverflows<uint32_t>(uoffset, sizeOfLoadOp(loadOp))) [[unlikely]] {
        // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);

        // Unreachable at runtime, so we just add a constant that makes the types work out.
        switch (loadOp) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load16S:
        case LoadOpType::I32Load:
        case LoadOpType::I32Load16U:
        case LoadOpType::I32Load8U:
            result = Value::fromI32(0);
            break;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load:
        case LoadOpType::I64Load16U:
            result = Value::fromI64(0);
            break;
        case LoadOpType::F32Load:
            result = Value::fromF32(0);
            break;
        case LoadOpType::F64Load:
            result = Value::fromF64(0);
            break;
        }
    } else {
        result = emitCheckAndPrepareAndMaterializePointerApply(pointer, uoffset, sizeOfLoadOp(loadOp), [&](auto location) -> Value {
            consume(pointer);
            Value result = topValue(typeOfLoadOp(loadOp));
            Location resultLocation = allocate(result);

            switch (loadOp) {
            case LoadOpType::I32Load8S:
                m_jit.load8SignedExtendTo32(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load8S:
                m_jit.load8SignedExtendTo64(location, resultLocation.asGPR());
                break;
            case LoadOpType::I32Load8U:
                m_jit.load8(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load8U:
                m_jit.load8(location, resultLocation.asGPR());
                break;
            case LoadOpType::I32Load16S:
                m_jit.load16SignedExtendTo32(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load16S:
                m_jit.load16SignedExtendTo64(location, resultLocation.asGPR());
                break;
            case LoadOpType::I32Load16U:
                m_jit.load16(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load16U:
                m_jit.load16(location, resultLocation.asGPR());
                break;
            case LoadOpType::I32Load:
                m_jit.load32(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load32U:
                m_jit.load32(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load32S:
                m_jit.load32SignedExtendTo64(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load:
                m_jit.load64(location, resultLocation.asGPR());
                break;
            case LoadOpType::F32Load:
                m_jit.loadFloat(location, resultLocation.asFPR());
                break;
            case LoadOpType::F64Load:
                m_jit.loadDouble(location, resultLocation.asFPR());
                break;
            }

            return result;
        });
    }

    LOG_INSTRUCTION(LOAD_OP_NAMES[(unsigned)loadOp - (unsigned)I32Load], pointer, uoffset, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::store(StoreOpType storeOp, Value pointer, Value value, uint32_t uoffset)
{
    Location valueLocation = locationOf(value);
    if (sumOverflows<uint32_t>(uoffset, sizeOfStoreOp(storeOp))) [[unlikely]] {
        // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
        emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
        consume(pointer);
        consume(value);
    } else {
        emitCheckAndPrepareAndMaterializePointerApply(pointer, uoffset, sizeOfStoreOp(storeOp), [&](auto location) -> void {
            Location valueLocation;
            if (value.isConst() && value.isFloat()) {
                ScratchScope<0, 1> scratches(*this);
                valueLocation = Location::fromFPR(scratches.fpr(0));
                emitMoveConst(value, valueLocation);
            } else if (value.isConst()) {
                ScratchScope<1, 0> scratches(*this);
                valueLocation = Location::fromGPR(scratches.gpr(0));
                emitMoveConst(value, valueLocation);
            } else
                valueLocation = loadIfNecessary(value);
            ASSERT(valueLocation.isRegister());

            consume(value);
            consume(pointer);

            switch (storeOp) {
            case StoreOpType::I64Store8:
            case StoreOpType::I32Store8:
                m_jit.store8(valueLocation.asGPR(), location);
                return;
            case StoreOpType::I64Store16:
            case StoreOpType::I32Store16:
                m_jit.store16(valueLocation.asGPR(), location);
                return;
            case StoreOpType::I64Store32:
            case StoreOpType::I32Store:
                m_jit.store32(valueLocation.asGPR(), location);
                return;
            case StoreOpType::I64Store:
                m_jit.store64(valueLocation.asGPR(), location);
                return;
            case StoreOpType::F32Store:
                m_jit.storeFloat(valueLocation.asFPR(), location);
                return;
            case StoreOpType::F64Store:
                m_jit.storeDouble(valueLocation.asFPR(), location);
                return;
            }
        });
    }

    LOG_INSTRUCTION(STORE_OP_NAMES[(unsigned)storeOp - (unsigned)I32Store], pointer, uoffset, value, valueLocation);

    return { };
}

void BBQJIT::emitSanitizeAtomicResult(ExtAtomicOpType op, TypeKind resultType, GPRReg source, GPRReg dest)
{
    switch (resultType) {
    case TypeKind::I64: {
        switch (accessWidth(op)) {
        case Width8:
            m_jit.zeroExtend8To32(source, dest);
            return;
        case Width16:
            m_jit.zeroExtend16To32(source, dest);
            return;
        case Width32:
            m_jit.zeroExtend32ToWord(source, dest);
            return;
        case Width64:
            m_jit.move(source, dest);
            return;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        return;
    }
    case TypeKind::I32:
        switch (accessWidth(op)) {
        case Width8:
            m_jit.zeroExtend8To32(source, dest);
            return;
        case Width16:
            m_jit.zeroExtend16To32(source, dest);
            return;
        case Width32:
        case Width64:
            m_jit.move(source, dest);
            return;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void BBQJIT::emitSanitizeAtomicResult(ExtAtomicOpType op, TypeKind resultType, GPRReg result)
{
    emitSanitizeAtomicResult(op, resultType, result, result);
}

template<typename Functor>
void BBQJIT::emitAtomicOpGeneric(ExtAtomicOpType op, Address address, GPRReg oldGPR, GPRReg scratchGPR, const Functor& functor)
{
    Width accessWidth = this->accessWidth(op);

    // We need a CAS loop or a LL/SC loop. Using prepare/attempt jargon, we want:
    //
    // Block #reloop:
    //     Prepare
    //     Operation
    //     Attempt
    //   Successors: Then:#done, Else:#reloop
    // Block #done:
    //     Move oldValue, result

    // Prepare
    auto reloopLabel = m_jit.label();
    switch (accessWidth) {
    case Width8:
#if CPU(ARM64)
        m_jit.loadLinkAcq8(address, oldGPR);
#else
        m_jit.load8SignedExtendTo32(address, oldGPR);
#endif
        break;
    case Width16:
#if CPU(ARM64)
        m_jit.loadLinkAcq16(address, oldGPR);
#else
        m_jit.load16SignedExtendTo32(address, oldGPR);
#endif
        break;
    case Width32:
#if CPU(ARM64)
        m_jit.loadLinkAcq32(address, oldGPR);
#else
        m_jit.load32(address, oldGPR);
#endif
        break;
    case Width64:
#if CPU(ARM64)
        m_jit.loadLinkAcq64(address, oldGPR);
#else
        m_jit.load64(address, oldGPR);
#endif
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Operation
    functor(oldGPR, scratchGPR);

#if CPU(X86_64)
    switch (accessWidth) {
    case Width8:
        m_jit.branchAtomicStrongCAS8(StatusCondition::Failure, oldGPR, scratchGPR, address).linkTo(reloopLabel, &m_jit);
        break;
    case Width16:
        m_jit.branchAtomicStrongCAS16(StatusCondition::Failure, oldGPR, scratchGPR, address).linkTo(reloopLabel, &m_jit);
        break;
    case Width32:
        m_jit.branchAtomicStrongCAS32(StatusCondition::Failure, oldGPR, scratchGPR, address).linkTo(reloopLabel, &m_jit);
        break;
    case Width64:
        m_jit.branchAtomicStrongCAS64(StatusCondition::Failure, oldGPR, scratchGPR, address).linkTo(reloopLabel, &m_jit);
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
    }
#elif CPU(ARM64)
    switch (accessWidth) {
    case Width8:
        m_jit.storeCondRel8(scratchGPR, address, scratchGPR);
        break;
    case Width16:
        m_jit.storeCondRel16(scratchGPR, address, scratchGPR);
        break;
    case Width32:
        m_jit.storeCondRel32(scratchGPR, address, scratchGPR);
        break;
    case Width64:
        m_jit.storeCondRel64(scratchGPR, address, scratchGPR);
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_jit.branchTest32(ResultCondition::NonZero, scratchGPR).linkTo(reloopLabel, &m_jit);
#endif
}

Value WARN_UNUSED_RETURN BBQJIT::emitAtomicLoadOp(ExtAtomicOpType loadOp, Type valueType, Location pointer, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(loadOp) != Width8)
        recordJumpToThrowException(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(loadOp) - 1)));

    Value result = topValue(valueType.kind);
    Location resultLocation = allocate(result);

    if (!(isARM64_LSE() || isX86_64())) {
        ScratchScope<1, 0> scratches(*this);
        emitAtomicOpGeneric(loadOp, address, resultLocation.asGPR(), scratches.gpr(0), [&](GPRReg oldGPR, GPRReg newGPR) {
            emitSanitizeAtomicResult(loadOp, canonicalWidth(accessWidth(loadOp)) == Width64 ? TypeKind::I64 : TypeKind::I32, oldGPR, newGPR);
        });
        emitSanitizeAtomicResult(loadOp, valueType.kind, resultLocation.asGPR());
        return result;
    }

    m_jit.move(TrustedImm32(0), resultLocation.asGPR());
    switch (loadOp) {
    case ExtAtomicOpType::I32AtomicLoad: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd32(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd32(resultLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicLoad: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd64(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd64(resultLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I32AtomicLoad8U: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd8(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd8(resultLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I32AtomicLoad16U: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd16(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd16(resultLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicLoad8U: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd8(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd8(resultLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicLoad16U: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd16(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd16(resultLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicLoad32U: {
#if CPU(ARM64)
        m_jit.atomicXchgAdd32(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
        m_jit.atomicXchgAdd32(resultLocation.asGPR(), address);
#endif
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    emitSanitizeAtomicResult(loadOp, valueType.kind, resultLocation.asGPR());

    return result;
}

void BBQJIT::emitAtomicStoreOp(ExtAtomicOpType storeOp, Type, Location pointer, Value value, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(storeOp) != Width8)
        recordJumpToThrowException(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(storeOp) - 1)));

    GPRReg scratch1GPR = InvalidGPRReg;
    GPRReg scratch2GPR = InvalidGPRReg;
    Location valueLocation;
    if (value.isConst()) {
        ScratchScope<3, 0> scratches(*this);
        valueLocation = Location::fromGPR(scratches.gpr(0));
        emitMoveConst(value, valueLocation);
        scratch1GPR = scratches.gpr(1);
        scratch2GPR = scratches.gpr(2);
    } else {
        ScratchScope<2, 0> scratches(*this);
        valueLocation = loadIfNecessary(value);
        scratch1GPR = scratches.gpr(0);
        scratch2GPR = scratches.gpr(1);
    }
    ASSERT(valueLocation.isRegister());

    consume(value);

    if (!(isARM64_LSE() || isX86_64())) {
        emitAtomicOpGeneric(storeOp, address, scratch1GPR, scratch2GPR, [&](GPRReg, GPRReg newGPR) {
            m_jit.move(valueLocation.asGPR(), newGPR);
        });
        return;
    }

    switch (storeOp) {
    case ExtAtomicOpType::I32AtomicStore: {
#if CPU(ARM64)
        m_jit.atomicXchg32(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store32(valueLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicStore: {
#if CPU(ARM64)
        m_jit.atomicXchg64(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store64(valueLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I32AtomicStore8U: {
#if CPU(ARM64)
        m_jit.atomicXchg8(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store8(valueLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I32AtomicStore16U: {
#if CPU(ARM64)
        m_jit.atomicXchg16(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store16(valueLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicStore8U: {
#if CPU(ARM64)
        m_jit.atomicXchg8(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store8(valueLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicStore16U: {
#if CPU(ARM64)
        m_jit.atomicXchg16(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store16(valueLocation.asGPR(), address);
#endif
        break;
    }
    case ExtAtomicOpType::I64AtomicStore32U: {
#if CPU(ARM64)
        m_jit.atomicXchg32(valueLocation.asGPR(), address, scratch1GPR);
#else
        m_jit.store32(valueLocation.asGPR(), address);
#endif
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

Value BBQJIT::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, Location pointer, Value value, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(op) != Width8)
        recordJumpToThrowException(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(op) - 1)));

    Value result = topValue(valueType.kind);
    Location resultLocation = allocate(result);

    GPRReg scratchGPR = InvalidGPRReg;
    Location valueLocation;
    if (value.isConst()) {
        ScratchScope<2, 0> scratches(*this);
        valueLocation = Location::fromGPR(scratches.gpr(0));
        emitMoveConst(value, valueLocation);
        scratchGPR = scratches.gpr(1);
    } else {
        ScratchScope<1, 0> scratches(*this);
        valueLocation = loadIfNecessary(value);
        scratchGPR = scratches.gpr(0);
    }
    ASSERT(valueLocation.isRegister());
    consume(value);

    switch (op) {
    case ExtAtomicOpType::I32AtomicRmw8AddU:
    case ExtAtomicOpType::I32AtomicRmw16AddU:
    case ExtAtomicOpType::I32AtomicRmwAdd:
    case ExtAtomicOpType::I64AtomicRmw8AddU:
    case ExtAtomicOpType::I64AtomicRmw16AddU:
    case ExtAtomicOpType::I64AtomicRmw32AddU:
    case ExtAtomicOpType::I64AtomicRmwAdd:
        if (isX86() || isARM64_LSE()) {
            switch (accessWidth(op)) {
            case Width8:
#if CPU(ARM64)
                m_jit.atomicXchgAdd8(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchgAdd8(resultLocation.asGPR(), address);
#endif
                break;
            case Width16:
#if CPU(ARM64)
                m_jit.atomicXchgAdd16(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchgAdd16(resultLocation.asGPR(), address);
#endif
                break;
            case Width32:
#if CPU(ARM64)
                m_jit.atomicXchgAdd32(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchgAdd32(resultLocation.asGPR(), address);
#endif
                break;
            case Width64:
#if CPU(ARM64)
                m_jit.atomicXchgAdd64(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchgAdd64(resultLocation.asGPR(), address);
#endif
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
            return result;
        }
        break;
    case ExtAtomicOpType::I32AtomicRmw8SubU:
    case ExtAtomicOpType::I32AtomicRmw16SubU:
    case ExtAtomicOpType::I32AtomicRmwSub:
    case ExtAtomicOpType::I64AtomicRmw8SubU:
    case ExtAtomicOpType::I64AtomicRmw16SubU:
    case ExtAtomicOpType::I64AtomicRmw32SubU:
    case ExtAtomicOpType::I64AtomicRmwSub:
        if (isX86() || isARM64_LSE()) {
            m_jit.move(valueLocation.asGPR(), scratchGPR);
            if (valueType.isI64())
                m_jit.neg64(scratchGPR);
            else
                m_jit.neg32(scratchGPR);

            switch (accessWidth(op)) {
            case Width8:
#if CPU(ARM64)
                m_jit.atomicXchgAdd8(scratchGPR, address, resultLocation.asGPR());
#else
                m_jit.move(scratchGPR, resultLocation.asGPR());
                m_jit.atomicXchgAdd8(resultLocation.asGPR(), address);
#endif
                break;
            case Width16:
#if CPU(ARM64)
                m_jit.atomicXchgAdd16(scratchGPR, address, resultLocation.asGPR());
#else
                m_jit.move(scratchGPR, resultLocation.asGPR());
                m_jit.atomicXchgAdd16(resultLocation.asGPR(), address);
#endif
                break;
            case Width32:
#if CPU(ARM64)
                m_jit.atomicXchgAdd32(scratchGPR, address, resultLocation.asGPR());
#else
                m_jit.move(scratchGPR, resultLocation.asGPR());
                m_jit.atomicXchgAdd32(resultLocation.asGPR(), address);
#endif
                break;
            case Width64:
#if CPU(ARM64)
                m_jit.atomicXchgAdd64(scratchGPR, address, resultLocation.asGPR());
#else
                m_jit.move(scratchGPR, resultLocation.asGPR());
                m_jit.atomicXchgAdd64(resultLocation.asGPR(), address);
#endif
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
            return result;
        }
        break;
    case ExtAtomicOpType::I32AtomicRmw8AndU:
    case ExtAtomicOpType::I32AtomicRmw16AndU:
    case ExtAtomicOpType::I32AtomicRmwAnd:
    case ExtAtomicOpType::I64AtomicRmw8AndU:
    case ExtAtomicOpType::I64AtomicRmw16AndU:
    case ExtAtomicOpType::I64AtomicRmw32AndU:
    case ExtAtomicOpType::I64AtomicRmwAnd:
#if CPU(ARM64)
        if (isARM64_LSE()) {
            m_jit.move(valueLocation.asGPR(), scratchGPR);
            if (valueType.isI64())
                m_jit.not64(scratchGPR);
            else
                m_jit.not32(scratchGPR);

            switch (accessWidth(op)) {
            case Width8:
                m_jit.atomicXchgClear8(scratchGPR, address, resultLocation.asGPR());
                break;
            case Width16:
                m_jit.atomicXchgClear16(scratchGPR, address, resultLocation.asGPR());
                break;
            case Width32:
                m_jit.atomicXchgClear32(scratchGPR, address, resultLocation.asGPR());
                break;
            case Width64:
                m_jit.atomicXchgClear64(scratchGPR, address, resultLocation.asGPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
            return result;
        }
#endif
        break;
    case ExtAtomicOpType::I32AtomicRmw8OrU:
    case ExtAtomicOpType::I32AtomicRmw16OrU:
    case ExtAtomicOpType::I32AtomicRmwOr:
    case ExtAtomicOpType::I64AtomicRmw8OrU:
    case ExtAtomicOpType::I64AtomicRmw16OrU:
    case ExtAtomicOpType::I64AtomicRmw32OrU:
    case ExtAtomicOpType::I64AtomicRmwOr:
#if CPU(ARM64)
        if (isARM64_LSE()) {
            switch (accessWidth(op)) {
            case Width8:
                m_jit.atomicXchgOr8(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            case Width16:
                m_jit.atomicXchgOr16(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            case Width32:
                m_jit.atomicXchgOr32(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            case Width64:
                m_jit.atomicXchgOr64(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
            return result;
        }
#endif
        break;
    case ExtAtomicOpType::I32AtomicRmw8XorU:
    case ExtAtomicOpType::I32AtomicRmw16XorU:
    case ExtAtomicOpType::I32AtomicRmwXor:
    case ExtAtomicOpType::I64AtomicRmw8XorU:
    case ExtAtomicOpType::I64AtomicRmw16XorU:
    case ExtAtomicOpType::I64AtomicRmw32XorU:
    case ExtAtomicOpType::I64AtomicRmwXor:
#if CPU(ARM64)
        if (isARM64_LSE()) {
            switch (accessWidth(op)) {
            case Width8:
                m_jit.atomicXchgXor8(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            case Width16:
                m_jit.atomicXchgXor16(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            case Width32:
                m_jit.atomicXchgXor32(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            case Width64:
                m_jit.atomicXchgXor64(valueLocation.asGPR(), address, resultLocation.asGPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
            return result;
        }
#endif
        break;
    case ExtAtomicOpType::I32AtomicRmw8XchgU:
    case ExtAtomicOpType::I32AtomicRmw16XchgU:
    case ExtAtomicOpType::I32AtomicRmwXchg:
    case ExtAtomicOpType::I64AtomicRmw8XchgU:
    case ExtAtomicOpType::I64AtomicRmw16XchgU:
    case ExtAtomicOpType::I64AtomicRmw32XchgU:
    case ExtAtomicOpType::I64AtomicRmwXchg:
        if (isX86() || isARM64_LSE()) {
            switch (accessWidth(op)) {
            case Width8:
#if CPU(ARM64)
                m_jit.atomicXchg8(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchg8(resultLocation.asGPR(), address);
#endif
                break;
            case Width16:
#if CPU(ARM64)
                m_jit.atomicXchg16(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchg16(resultLocation.asGPR(), address);
#endif
                break;
            case Width32:
#if CPU(ARM64)
                m_jit.atomicXchg32(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchg32(resultLocation.asGPR(), address);
#endif
                break;
            case Width64:
#if CPU(ARM64)
                m_jit.atomicXchg64(valueLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());
                m_jit.atomicXchg64(resultLocation.asGPR(), address);
#endif
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
            return result;
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    emitAtomicOpGeneric(op, address, resultLocation.asGPR(), scratchGPR, [&](GPRReg oldGPR, GPRReg newGPR) {
        switch (op) {
        case ExtAtomicOpType::I32AtomicRmw16AddU:
        case ExtAtomicOpType::I32AtomicRmw8AddU:
        case ExtAtomicOpType::I32AtomicRmwAdd:
            m_jit.add32(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I64AtomicRmw8AddU:
        case ExtAtomicOpType::I64AtomicRmw16AddU:
        case ExtAtomicOpType::I64AtomicRmw32AddU:
        case ExtAtomicOpType::I64AtomicRmwAdd:
            m_jit.add64(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I32AtomicRmw8SubU:
        case ExtAtomicOpType::I32AtomicRmw16SubU:
        case ExtAtomicOpType::I32AtomicRmwSub:
            m_jit.sub32(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I64AtomicRmw8SubU:
        case ExtAtomicOpType::I64AtomicRmw16SubU:
        case ExtAtomicOpType::I64AtomicRmw32SubU:
        case ExtAtomicOpType::I64AtomicRmwSub:
            m_jit.sub64(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I32AtomicRmw8AndU:
        case ExtAtomicOpType::I32AtomicRmw16AndU:
        case ExtAtomicOpType::I32AtomicRmwAnd:
            m_jit.and32(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I64AtomicRmw8AndU:
        case ExtAtomicOpType::I64AtomicRmw16AndU:
        case ExtAtomicOpType::I64AtomicRmw32AndU:
        case ExtAtomicOpType::I64AtomicRmwAnd:
            m_jit.and64(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I32AtomicRmw8OrU:
        case ExtAtomicOpType::I32AtomicRmw16OrU:
        case ExtAtomicOpType::I32AtomicRmwOr:
            m_jit.or32(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I64AtomicRmw8OrU:
        case ExtAtomicOpType::I64AtomicRmw16OrU:
        case ExtAtomicOpType::I64AtomicRmw32OrU:
        case ExtAtomicOpType::I64AtomicRmwOr:
            m_jit.or64(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I32AtomicRmw8XorU:
        case ExtAtomicOpType::I32AtomicRmw16XorU:
        case ExtAtomicOpType::I32AtomicRmwXor:
            m_jit.xor32(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I64AtomicRmw8XorU:
        case ExtAtomicOpType::I64AtomicRmw16XorU:
        case ExtAtomicOpType::I64AtomicRmw32XorU:
        case ExtAtomicOpType::I64AtomicRmwXor:
            m_jit.xor64(oldGPR, valueLocation.asGPR(), newGPR);
            break;
        case ExtAtomicOpType::I32AtomicRmw8XchgU:
        case ExtAtomicOpType::I32AtomicRmw16XchgU:
        case ExtAtomicOpType::I32AtomicRmwXchg:
        case ExtAtomicOpType::I64AtomicRmw8XchgU:
        case ExtAtomicOpType::I64AtomicRmw16XchgU:
        case ExtAtomicOpType::I64AtomicRmw32XchgU:
        case ExtAtomicOpType::I64AtomicRmwXchg:
            emitSanitizeAtomicResult(op, valueType.kind, valueLocation.asGPR(), newGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    });
    emitSanitizeAtomicResult(op, valueType.kind, resultLocation.asGPR());
    return result;
}

Value WARN_UNUSED_RETURN BBQJIT::emitAtomicCompareExchange(ExtAtomicOpType op, Type, Location pointer, Value expected, Value value, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());
    Width accessWidth = this->accessWidth(op);

    if (accessWidth != Width8)
        recordJumpToThrowException(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(op) - 1)));

    Value result = topValue(expected.type());
    Location resultLocation = allocate(result);

    ScratchScope<1, 0> scratches(*this);
    GPRReg scratchGPR = scratches.gpr(0);

    // FIXME: We should have a better way to write this.
    Location valueLocation;
    Location expectedLocation;
    if (value.isConst()) {
        if (expected.isConst()) {
            ScratchScope<2, 0> scratches(*this);
            valueLocation = Location::fromGPR(scratches.gpr(0));
            expectedLocation = Location::fromGPR(scratches.gpr(1));
            emitMoveConst(value, valueLocation);
            emitMoveConst(expected, expectedLocation);
        } else {
            ScratchScope<1, 0> scratches(*this);
            valueLocation = Location::fromGPR(scratches.gpr(0));
            emitMoveConst(value, valueLocation);
            expectedLocation = loadIfNecessary(expected);
        }
    } else {
        valueLocation = loadIfNecessary(value);
        if (expected.isConst()) {
            ScratchScope<1, 0> scratches(*this);
            expectedLocation = Location::fromGPR(scratches.gpr(0));
            emitMoveConst(expected, expectedLocation);
        } else
            expectedLocation = loadIfNecessary(expected);
    }

    ASSERT(valueLocation.isRegister());
    ASSERT(expectedLocation.isRegister());

    consume(value);
    consume(expected);

    auto emitStrongCAS = [&](GPRReg expectedGPR, GPRReg valueGPR, GPRReg resultGPR) {
        if (isX86_64() || isARM64_LSE()) {
            m_jit.move(expectedGPR, resultGPR);
            switch (accessWidth) {
            case Width8:
                m_jit.atomicStrongCAS8(resultGPR, valueGPR, address);
                break;
            case Width16:
                m_jit.atomicStrongCAS16(resultGPR, valueGPR, address);
                break;
            case Width32:
                m_jit.atomicStrongCAS32(resultGPR, valueGPR, address);
                break;
            case Width64:
                m_jit.atomicStrongCAS64(resultGPR, valueGPR, address);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            return;
        }

        m_jit.move(expectedGPR, resultGPR);
        switch (accessWidth) {
        case Width8:
            m_jit.atomicStrongCAS8(StatusCondition::Success, resultGPR, valueGPR, address, scratchGPR);
            break;
        case Width16:
            m_jit.atomicStrongCAS16(StatusCondition::Success, resultGPR, valueGPR, address, scratchGPR);
            break;
        case Width32:
            m_jit.atomicStrongCAS32(StatusCondition::Success, resultGPR, valueGPR, address, scratchGPR);
            break;
        case Width64:
            m_jit.atomicStrongCAS64(StatusCondition::Success, resultGPR, valueGPR, address, scratchGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    };

    switch (accessWidth) {
    case Width8:
        m_jit.and64(TrustedImm64(0xFF), expectedLocation.asGPR());
        break;
    case Width16:
        m_jit.and64(TrustedImm64(0xFFFF), expectedLocation.asGPR());
        break;
    case Width32:
        m_jit.and64(TrustedImm64(0xFFFFFFFF), expectedLocation.asGPR());
        break;
    default:
        break;
    }

    emitStrongCAS(expectedLocation.asGPR(), valueLocation.asGPR(), resultLocation.asGPR());
    emitSanitizeAtomicResult(op, expected.type(), resultLocation.asGPR());
    return result;
}

void BBQJIT::truncInBounds(TruncationKind truncationKind, Location operandLocation, Location resultLocation, FPRReg scratch1FPR, FPRReg scratch2FPR)
{
    switch (truncationKind) {
    case TruncationKind::I32TruncF32S:
        m_jit.truncateFloatToInt32(operandLocation.asFPR(), resultLocation.asGPR());
        break;
    case TruncationKind::I32TruncF64S:
        m_jit.truncateDoubleToInt32(operandLocation.asFPR(), resultLocation.asGPR());
        break;
    case TruncationKind::I32TruncF32U:
        m_jit.truncateFloatToUint32(operandLocation.asFPR(), resultLocation.asGPR());
        break;
    case TruncationKind::I32TruncF64U:
        m_jit.truncateDoubleToUint32(operandLocation.asFPR(), resultLocation.asGPR());
        break;
    case TruncationKind::I64TruncF32S:
        m_jit.truncateFloatToInt64(operandLocation.asFPR(), resultLocation.asGPR());
        break;
    case TruncationKind::I64TruncF64S:
        m_jit.truncateDoubleToInt64(operandLocation.asFPR(), resultLocation.asGPR());
        break;
    case TruncationKind::I64TruncF32U: {
        if constexpr (isX86())
            emitMoveConst(Value::fromF32(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())), Location::fromFPR(scratch2FPR));
        m_jit.truncateFloatToUint64(operandLocation.asFPR(), resultLocation.asGPR(), scratch1FPR, scratch2FPR);
        break;
    }
    case TruncationKind::I64TruncF64U: {
        if constexpr (isX86())
            emitMoveConst(Value::fromF64(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())), Location::fromFPR(scratch2FPR));
        m_jit.truncateDoubleToUint64(operandLocation.asFPR(), resultLocation.asGPR(), scratch1FPR, scratch2FPR);
        break;
    }
    }
}

// GC
PartialResult WARN_UNUSED_RETURN BBQJIT::addRefI31(ExpressionType value, ExpressionType& result)
{
    if (value.isConst()) {
        uint32_t lo32 = (value.asI32() << 1) >> 1;
        result = Value::fromI64(lo32 | JSValue::NumberTag);
        LOG_INSTRUCTION("RefI31", value, RESULT(result));
        return { };
    }

    Location initialValue = loadIfNecessary(value);
    consume(value);

    result = topValue(TypeKind::I64);
    Location resultLocation = allocateWithHint(result, initialValue);

    LOG_INSTRUCTION("RefI31", value, RESULT(result));

    m_jit.lshift32(TrustedImm32(1), resultLocation.asGPR());
    m_jit.rshift32(TrustedImm32(1), resultLocation.asGPR());
    m_jit.or64(TrustedImm64(JSValue::NumberTag), resultLocation.asGPR());
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI31GetS(TypedExpression typedValue, ExpressionType& result)
{
    auto value = typedValue.value();
    if (value.isConst()) {
        if (JSValue::decode(value.asI64()).isNumber())
            result = Value::fromI32((value.asI64() << 33) >> 33);
        else {
            emitThrowException(ExceptionType::NullI31Get);
            result = Value::fromI32(0);
        }

        LOG_INSTRUCTION("I31GetS", value, RESULT(result));

        return { };
    }


    Location initialValue = loadIfNecessary(value);
    if (typedValue.type().isNullable())
        emitThrowOnNullReference(ExceptionType::NullI31Get, initialValue);
    consume(value);

    result = topValue(TypeKind::I32);
    Location resultLocation = allocateWithHint(result, initialValue);

    LOG_INSTRUCTION("I31GetS", value, RESULT(result));

    m_jit.zeroExtend32ToWord(initialValue.asGPR(), resultLocation.asGPR());

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI31GetU(TypedExpression typedValue, ExpressionType& result)
{
    auto value = typedValue.value();
    if (value.isConst()) {
        if (JSValue::decode(value.asI64()).isNumber())
            result = Value::fromI32(value.asI64() & 0x7fffffffu);
        else {
            emitThrowException(ExceptionType::NullI31Get);
            result = Value::fromI32(0);
        }

        LOG_INSTRUCTION("I31GetU", value, RESULT(result));

        return { };
    }


    Location initialValue = loadIfNecessary(value);
    if (typedValue.type().isNullable())
        emitThrowOnNullReference(ExceptionType::NullI31Get, initialValue);
    consume(value);

    result = topValue(TypeKind::I32);
    Location resultLocation = allocateWithHint(result, initialValue);

    LOG_INSTRUCTION("I31GetU", value, RESULT(result));

    m_jit.and32(TrustedImm32(0x7fffffff), initialValue.asGPR(), resultLocation.asGPR());

    return { };
}

// This will replace the existing value with a new value. Note that if this is an F32 then the top bits may be garbage but that's ok for our current usage.
Value BBQJIT::marshallToI64(Value value)
{
    ASSERT(!value.isLocal());
    if (value.type() == TypeKind::F32 || value.type() == TypeKind::F64) {
        if (value.isConst())
            return Value::fromI64(value.type() == TypeKind::F32 ? std::bit_cast<uint32_t>(value.asI32()) : std::bit_cast<uint64_t>(value.asF64()));
        // This is a bit silly. We could just move initValue to the right argument GPR if we know it's in an FPR already.
        flushValue(value);
        return Value::fromTemp(TypeKind::I64, value.asTemp());
    }
    return value;
}

void BBQJIT::emitAllocateGCArrayUninitialized(GPRReg resultGPR, uint32_t typeIndex, ExpressionType size, GPRReg scratchGPR, GPRReg scratchGPR2)
{
    RELEASE_ASSERT(m_info.hasGCObjectTypes());
    JumpList slowPath;
    const ArrayType* typeDefinition = m_info.typeSignatures[typeIndex]->expand().template as<ArrayType>();
    MacroAssembler::Address allocatorBufferBase(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfAllocatorForGCObject(m_info, 0));
    MacroAssembler::Address structureIDAddress(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfGCObjectStructureID(m_info, typeIndex));
    Location sizeLocation;
    size_t elementSize = typeDefinition->elementType().type.elementSize();
    if (size.isConst()) {
        std::optional<unsigned> sizeInBytes = JSWebAssemblyArray::allocationSizeInBytes(typeDefinition->elementType(), size.asI32());

        if (sizeInBytes && sizeInBytes.value() <= MarkedSpace::largeCutoff) {
            size_t sizeClassIndex = MarkedSpace::sizeClassToIndex(sizeInBytes.value());
            m_jit.loadPtr(allocatorBufferBase.withOffset(sizeClassIndex * sizeof(Allocator)), scratchGPR2);
            JIT_COMMENT(m_jit, "Do array allocation constant sized");
            m_jit.emitAllocateWithNonNullAllocator(resultGPR, JITAllocator::variableNonNull(), scratchGPR2, scratchGPR, slowPath, AssemblyHelpers::SlowAllocationResult::UndefinedBehavior);
            m_jit.load32(structureIDAddress, scratchGPR);
            m_jit.move(TrustedImm32(JSWebAssemblyArray::typeInfoBlob().blob()), scratchGPR2);
            static_assert(JSCell::structureIDOffset() + sizeof(int32_t) == JSCell::indexingTypeAndMiscOffset());
            m_jit.storePair32(scratchGPR, scratchGPR2, MacroAssembler::Address(resultGPR, JSCell::structureIDOffset()));
            m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(resultGPR, JSObject::butterflyOffset()));
            m_jit.store32(TrustedImm32(size.asI32()), MacroAssembler::Address(resultGPR, JSWebAssemblyArray::offsetOfSize()));
        } else {
            // FIXME: emitCCall can't handle being passed a destination... which is why we just jump to the slow path here.
            slowPath.append(m_jit.jump());
        }

    } else {
        sizeLocation = loadIfNecessary(size);

        JIT_COMMENT(m_jit, "Do array allocation variable sized");

        ASSERT(hasOneBitSet(elementSize));
        m_jit.jitAssertIsInt32(sizeLocation.asGPR());
        m_jit.lshift64(sizeLocation.asGPR(), TrustedImm32(getLSBSet(elementSize)), scratchGPR);
        m_jit.add64(TrustedImm64(sizeof(JSWebAssemblyArray)), scratchGPR);

        m_jit.emitAllocateVariableSized(resultGPR, JITAllocator::variableNonNull(), allocatorBufferBase, scratchGPR, scratchGPR, scratchGPR2, slowPath, AssemblyHelpers::SlowAllocationResult::UndefinedBehavior);
        m_jit.load32(structureIDAddress, scratchGPR);
        m_jit.move(TrustedImm32(JSWebAssemblyArray::typeInfoBlob().blob()), scratchGPR2);
        static_assert(JSCell::structureIDOffset() + sizeof(int32_t) == JSCell::indexingTypeAndMiscOffset());
        m_jit.storePair32(scratchGPR, scratchGPR2, MacroAssembler::Address(resultGPR, JSCell::structureIDOffset()));
        m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(resultGPR, JSObject::butterflyOffset()));
        m_jit.store32(sizeLocation.asGPR(), MacroAssembler::Address(resultGPR, JSWebAssemblyArray::offsetOfSize()));
    }

    // FIXME: Ideally we'd have a way for our caller to set the label they want us to return to since e.g. addArrayNewDefault doesn't need to initialize
    // if we hit the slow path. But the way Labels work we need to know the exact offset we're returning to when moving to the slow path.
    JIT_COMMENT(m_jit, "Slow path return");
    MacroAssembler::Label done(m_jit);
    m_slowPaths.append({ origin(), WTFMove(slowPath), WTFMove(done), copyBindings(), [typeIndex, size, sizeLocation, resultGPR](BBQJIT& bbq, CCallHelpers& jit) {
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        if (size.isConst())
            jit.setupArguments<decltype(operationWasmArrayNewEmpty)>(GPRInfo::wasmContextInstancePointer, TrustedImm32(typeIndex), TrustedImm32(size.asI32()));
        else
            jit.setupArguments<decltype(operationWasmArrayNewEmpty)>(GPRInfo::wasmContextInstancePointer, TrustedImm32(typeIndex), sizeLocation.asGPR());
        jit.callOperation<OperationPtrTag>(operationWasmArrayNewEmpty);
        jit.move(GPRInfo::returnValueGPR, resultGPR);
        bbq.emitThrowOnNullReference(ExceptionType::BadArrayNew, Location::fromGPR(resultGPR));
    } });
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType initValue, ExpressionType& result)
{
    GPRReg resultGPR;
    {
        ScratchScope<2, 0> scratches(*this);
        resultGPR = scratches.gpr(0);
        GPRReg scratchGPR = scratches.gpr(1);
        emitAllocateGCArrayUninitialized(resultGPR, typeIndex, size, wasmScratchGPR, scratchGPR);

        JIT_COMMENT(m_jit, "Array allocation done do initialization");

        std::optional<ScratchScope<1, 0>> sizeScratch;
        Location sizeLocation = materializeToGPR(size, sizeScratch);
        StorageType elementType = getArrayElementType(typeIndex);
        emitArrayGetPayload(elementType, resultGPR, scratchGPR);

        MacroAssembler::Label loop(m_jit);
        JIT_COMMENT(m_jit, "Array initialization loop header");
        Jump done = m_jit.branchTest32(MacroAssembler::Zero, sizeLocation.asGPR());
        m_jit.sub32(TrustedImm32(1), sizeLocation.asGPR());
        constexpr bool preserveIndex = true;
        emitArrayStoreElementUnchecked(elementType, scratchGPR, sizeLocation, initValue, preserveIndex);
        m_jit.jump(loop);
        done.link(m_jit);

        if (Wasm::isRefType(elementType.unpacked()))
            emitMutatorFence();

#if ASSERT_ENABLED
        if (Wasm::isRefType(elementType.unpacked())) {
            m_jit.probeDebug([=] (Probe::Context& context) {
                auto* arrayPtr = context.gpr<JSWebAssemblyArray*>(resultGPR);
                if (!arrayPtr->isPreciseAllocation())
                    ASSERT(arrayPtr->sizeInBytes() + sizeof(JSWebAssemblyArray) <= arrayPtr->markedBlock().handle().cellSize());
                auto span = arrayPtr->refTypeSpan();
                for (uint64_t value : span)
                    validateWasmValue(value, elementType.unpacked());
            });
        }
#endif

    }

    consume(size);
    consume(initValue);
    result = topValue(TypeKind::Ref);
    bind(result, Location::fromGPR(resultGPR));

    LOG_INSTRUCTION("ArrayNew", typeIndex, size, initValue, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNewFixed(uint32_t typeIndex, ArgumentList& args, ExpressionType& result)
{
    GPRReg resultGPR;
    {
        Value size = Value::fromI32(args.size());
        ScratchScope<2, 0> scratches(*this);
        resultGPR = scratches.gpr(0);
        GPRReg scratchGPR = scratches.gpr(1);
        emitAllocateGCArrayUninitialized(resultGPR, typeIndex, size, wasmScratchGPR, scratchGPR);

        JIT_COMMENT(m_jit, "Array allocation done do initialization");
        StorageType elementType = getArrayElementType(typeIndex);
        emitArrayGetPayload(elementType, resultGPR, scratchGPR);

        for (uint32_t i = 0; i < args.size(); ++i) {
            emitArrayStoreElementUnchecked(elementType, scratchGPR, Value::fromI32(i), args[i]);
            consume(args[i]);
        }

        if (Wasm::isRefType(elementType.unpacked()))
            emitMutatorFence();

    }

    result = topValue(TypeKind::Ref);
    bind(result, Location::fromGPR(resultGPR));

    LOG_INSTRUCTION("ArrayNewFixed", typeIndex, args.size(), RESULT(result));
    return { };
}


PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result)
{
    StorageType elementType = getArrayElementType(typeIndex);
    // FIXME: We don't have a good way to fill V128s yet so just make a call.
    if (elementType.unpacked().isV128()) {
        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(typeIndex),
            size,
        };
        result = topValue(TypeKind::Arrayref);
        emitCCall(operationWasmArrayNewEmpty, arguments, result);

        Location resultLocation = loadIfNecessary(result);
        emitThrowOnNullReference(ExceptionType::BadArrayNew, resultLocation);

        LOG_INSTRUCTION("ArrayNewDefault", typeIndex, size, RESULT(result));
        return { };
    }

    GPRReg resultGPR;
    {
        ScratchScope<2, 0> scratches(*this);
        resultGPR = scratches.gpr(0);
        GPRReg scratchGPR = scratches.gpr(1);

        emitAllocateGCArrayUninitialized(resultGPR, typeIndex, size, wasmScratchGPR, scratchGPR);

        JIT_COMMENT(m_jit, "Array allocation done do initialization");
        std::optional<ScratchScope<1, 0>> sizeScratch;
        Location sizeLocation = materializeToGPR(size, sizeScratch);
        Value initValue = Value::fromI64(Wasm::isRefType(elementType.unpacked()) ? JSValue::encode(jsNull()) : 0);

        emitArrayGetPayload(elementType, resultGPR, scratchGPR);

        MacroAssembler::Label loop(m_jit);
        JIT_COMMENT(m_jit, "Array initialization loop header");
        Jump done = m_jit.branchTest32(MacroAssembler::Zero, sizeLocation.asGPR());
        m_jit.sub32(TrustedImm32(1), sizeLocation.asGPR());
        constexpr bool preserveIndex = true;
        emitArrayStoreElementUnchecked(elementType, scratchGPR, sizeLocation, initValue, preserveIndex);
        m_jit.jump(loop);
        done.link(m_jit);

        if (Wasm::isRefType(elementType.unpacked()))
            emitMutatorFence();
    }

    consume(size);
    result = topValue(TypeKind::Ref);
    bind(result, Location::fromGPR(resultGPR));

    LOG_INSTRUCTION("ArrayNewDefault", typeIndex, size, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayGet(ExtGCOpType arrayGetKind, uint32_t typeIndex, TypedExpression typedArray, ExpressionType index, ExpressionType& result)
{
    auto arrayref = typedArray.value();
    StorageType elementType = getArrayElementType(typeIndex);
    Type resultType = elementType.unpacked();

    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));
        consume(index);
        emitThrowException(ExceptionType::NullAccess);
        result = topValue(resultType.kind);
        return { };
    }

    Location arrayLocation = loadIfNecessary(arrayref);
    if (typedArray.type().isNullable())
        emitThrowOnNullReferenceBeforeAccess(arrayLocation, JSWebAssemblyArray::offsetOfSize());

    Location indexLocation;
    if (index.isConst()) {
        m_jit.load32(MacroAssembler::Address(arrayLocation.asGPR(), JSWebAssemblyArray::offsetOfSize()), wasmScratchGPR);
        recordJumpToThrowException(ExceptionType::OutOfBoundsArrayGet,
            m_jit.branch32(MacroAssembler::BelowOrEqual, wasmScratchGPR, TrustedImm32(index.asI32())));
    } else {
        indexLocation = loadIfNecessary(index);
        recordJumpToThrowException(ExceptionType::OutOfBoundsArrayGet,
            m_jit.branch32(MacroAssembler::AboveOrEqual, indexLocation.asGPR(), MacroAssembler::Address(arrayLocation.asGPR(), JSWebAssemblyArray::offsetOfSize())));
        m_jit.zeroExtend32ToWord(indexLocation.asGPR(), indexLocation.asGPR());
    }

    emitArrayGetPayload(elementType, arrayLocation.asGPR(), wasmScratchGPR);

    consume(arrayref);
    result = topValue(resultType.kind);
    Location resultLocation = allocate(result);

    if (index.isConst()) {
        auto fieldAddress = MacroAssembler::Address(wasmScratchGPR, elementType.elementSize() * index.asI32());

        if (elementType.is<PackedType>()) {
            switch (elementType.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                m_jit.load8(fieldAddress, resultLocation.asGPR());
                break;
            case Wasm::PackedType::I16:
                m_jit.load16(fieldAddress, resultLocation.asGPR());
                break;
            }
        } else {
            ASSERT(elementType.is<Type>());
            switch (result.type()) {
            case TypeKind::I32: {
                m_jit.load32(fieldAddress, resultLocation.asGPR());
                break;
            }
            case TypeKind::I64:
                m_jit.load64(fieldAddress, resultLocation.asGPR());
                break;
            case TypeKind::F32:
                m_jit.loadFloat(fieldAddress, resultLocation.asFPR());
                break;
            case TypeKind::F64:
                m_jit.loadDouble(fieldAddress, resultLocation.asFPR());
                break;
            case TypeKind::V128:
                m_jit.loadVector(fieldAddress, resultLocation.asFPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    } else {
        auto scale = static_cast<MacroAssembler::Scale>(std::bit_width(std::min(size_t { 8 }, elementType.elementSize()) - 1));
        auto fieldBaseIndex = MacroAssembler::BaseIndex(wasmScratchGPR, indexLocation.asGPR(), scale);

        if (elementType.is<PackedType>()) {
            switch (elementType.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                m_jit.load8(fieldBaseIndex, resultLocation.asGPR());
                break;
            case Wasm::PackedType::I16:
                m_jit.load16(fieldBaseIndex, resultLocation.asGPR());
                break;
            }
        } else {
            ASSERT(elementType.is<Type>());
            switch (result.type()) {
            case TypeKind::I32:
                m_jit.load32(fieldBaseIndex, resultLocation.asGPR());
                break;
            case TypeKind::I64:
                m_jit.load64(fieldBaseIndex, resultLocation.asGPR());
                break;
            case TypeKind::F32:
                m_jit.loadFloat(fieldBaseIndex, resultLocation.asFPR());
                break;
            case TypeKind::F64:
                m_jit.loadDouble(fieldBaseIndex, resultLocation.asFPR());
                break;
            case TypeKind::V128:
                // For V128, the index computation above doesn't work so we index differently.
                m_jit.mul32(Imm32(4), indexLocation.asGPR(), indexLocation.asGPR());
                m_jit.loadVector(MacroAssembler::BaseIndex(wasmScratchGPR, indexLocation.asGPR(), MacroAssembler::Scale::TimesFour), resultLocation.asFPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    consume(index);

    if (result.type() == TypeKind::I32) {
        switch (arrayGetKind) {
        case ExtGCOpType::ArrayGet:
            break;
        case ExtGCOpType::ArrayGetU:
            LOG_INSTRUCTION("ArrayGetU", typeIndex, arrayref, index, RESULT(result));
            return { };
        case ExtGCOpType::ArrayGetS: {
            ASSERT(resultType.kind == TypeKind::I32);
            uint8_t bitShift = (sizeof(uint32_t) - elementType.elementSize()) * 8;

            m_jit.lshift32(TrustedImm32(bitShift), resultLocation.asGPR());
            m_jit.rshift32(TrustedImm32(bitShift), resultLocation.asGPR());
            LOG_INSTRUCTION("ArrayGetS", typeIndex, arrayref, index, RESULT(result));
            return { };
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    LOG_INSTRUCTION("ArrayGet", typeIndex, arrayref, index, RESULT(result));

    return { };
}

void BBQJIT::emitArrayStoreElementUnchecked(StorageType elementType, GPRReg payloadGPR, Location index, Value value, bool preserveIndex)
{
    ASSERT(index.isRegister());

    auto scale = static_cast<MacroAssembler::Scale>(std::bit_width(std::min(size_t { 8 }, elementType.elementSize())) - 1);
    auto fieldBaseIndex = MacroAssembler::BaseIndex(payloadGPR, index.asGPR(), scale);

    // If we need to preserve the index then we need wasmScratchGPR to hold our temporary.
    ASSERT_IMPLIES(preserveIndex, payloadGPR != wasmScratchGPR);
    if (value.type() == TypeKind::V128) {
        GPRReg scratchGPR = preserveIndex ? wasmScratchGPR : index.asGPR();
        m_jit.mul32(TrustedImm32(4), index.asGPR(), scratchGPR);
        fieldBaseIndex = MacroAssembler::BaseIndex(payloadGPR, scratchGPR, MacroAssembler::Scale::TimesFour);
    }

    emitMove(elementType, value, fieldBaseIndex);
}

void BBQJIT::emitArrayStoreElementUnchecked(StorageType elementType, GPRReg payloadGPR, Value index, Value value)
{
    if (index.isConst()) {
        auto fieldAddress = MacroAssembler::Address(payloadGPR, elementType.elementSize() * index.asI32());
        if (!value.isConst())
            loadIfNecessary(value);
        emitMove(elementType, value, fieldAddress);
    } else {
        Location indexLocation = loadIfNecessary(index);
        emitArrayStoreElementUnchecked(elementType, payloadGPR, indexLocation, value);
    }
}

void BBQJIT::emitArraySetUnchecked(uint32_t typeIndex, Value arrayref, Value index, Value value)
{
    StorageType elementType = getArrayElementType(typeIndex);

    Location arrayLocation;
    if (arrayref.isPinned())
        arrayLocation = locationOf(arrayref);
    else
        arrayLocation = loadIfNecessary(arrayref);

    emitArrayGetPayload(elementType, arrayLocation.asGPR(), wasmScratchGPR);
    emitArrayStoreElementUnchecked(elementType, wasmScratchGPR, index, value);

    consume(index);
    consume(value);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArraySet(uint32_t typeIndex, TypedExpression typedArray, ExpressionType index, ExpressionType value)
{
    auto arrayref = typedArray.value();
    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));

        LOG_INSTRUCTION("ArraySet", typeIndex, arrayref, index, value);
        consume(value);
        emitThrowException(ExceptionType::NullAccess);
        return { };
    }

    Location arrayLocation = loadIfNecessary(arrayref);
    if (typedArray.type().isNullable())
        emitThrowOnNullReferenceBeforeAccess(arrayLocation, JSWebAssemblyArray::offsetOfSize());

    ASSERT(index.type() == TypeKind::I32);
    if (index.isConst()) {
        m_jit.load32(MacroAssembler::Address(arrayLocation.asGPR(), JSWebAssemblyArray::offsetOfSize()), wasmScratchGPR);
        recordJumpToThrowException(ExceptionType::OutOfBoundsArraySet,
            m_jit.branch32(MacroAssembler::BelowOrEqual, wasmScratchGPR, TrustedImm32(index.asI32())));
    } else {
        Location indexLocation = loadIfNecessary(index);
        recordJumpToThrowException(ExceptionType::OutOfBoundsArraySet,
            m_jit.branch32(MacroAssembler::AboveOrEqual, indexLocation.asGPR(), MacroAssembler::Address(arrayLocation.asGPR(), JSWebAssemblyArray::offsetOfSize())));
        m_jit.zeroExtend32ToWord(indexLocation.asGPR(), indexLocation.asGPR());
    }

    emitArraySetUnchecked(typeIndex, arrayref, index, value);

    if (isRefType(getArrayElementType(typeIndex).unpacked()))
        emitWriteBarrier(arrayLocation.asGPR());
    consume(arrayref);

    LOG_INSTRUCTION("ArraySet", typeIndex, arrayref, index, value);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayLen(TypedExpression typedArray, ExpressionType& result)
{
    auto arrayref = typedArray.value();
    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullAccess);
        result = Value::fromI32(0);
        LOG_INSTRUCTION("ArrayLen", arrayref, RESULT(result), "Exception");
        return { };
    }

    Location arrayLocation = loadIfNecessary(arrayref);
    consume(arrayref);
    if (typedArray.type().isNullable())
        emitThrowOnNullReferenceBeforeAccess(arrayLocation, JSWebAssemblyArray::offsetOfSize());

    result = topValue(TypeKind::I32);
    Location resultLocation = allocateWithHint(result, arrayLocation);
    m_jit.load32(MacroAssembler::Address(arrayLocation.asGPR(), JSWebAssemblyArray::offsetOfSize()), resultLocation.asGPR());

    LOG_INSTRUCTION("ArrayLen", arrayref, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayFill(uint32_t typeIndex, TypedExpression typedArray, ExpressionType offset, ExpressionType value, ExpressionType size)
{
    auto arrayref = typedArray.value();
    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));

        LOG_INSTRUCTION("ArrayFill", typeIndex, arrayref, offset, value, size);

        consume(offset);
        consume(value);
        consume(size);
        emitThrowException(ExceptionType::NullArrayFill);
        return { };
    }

    if (typedArray.type().isNullable())
        emitThrowOnNullReference(ExceptionType::NullArrayFill, loadIfNecessary(arrayref));

    Value shouldThrow = topValue(TypeKind::I32);
    if (value.type() != TypeKind::V128) {
        value = marshallToI64(value);
        Vector<Value, 8> arguments = {
            instanceValue(),
            arrayref,
            offset,
            value,
            size
        };
        emitCCall(&operationWasmArrayFill, arguments, shouldThrow);
    } else {
        ASSERT(!value.isConst());
        Location valueLocation = loadIfNecessary(value);
        consume(value);

        Value lane0, lane1;
        {
            ScratchScope<2, 0> scratches(*this);
            lane0 = Value::pinned(TypeKind::I64, Location::fromGPR(scratches.gpr(0)));
            lane1 = Value::pinned(TypeKind::I64, Location::fromGPR(scratches.gpr(1)));

            m_jit.vectorExtractLaneInt64(TrustedImm32(0), valueLocation.asFPR(), scratches.gpr(0));
            m_jit.vectorExtractLaneInt64(TrustedImm32(1), valueLocation.asFPR(), scratches.gpr(1));
        }

        Vector<Value, 8> arguments = {
            instanceValue(),
            arrayref,
            offset,
            lane0,
            lane1,
            size,
        };
        emitCCall(operationWasmArrayFillVector, arguments, shouldThrow);
    }
    Location shouldThrowLocation = loadIfNecessary(shouldThrow);

    LOG_INSTRUCTION("ArrayFill", typeIndex, arrayref, offset, value, size);

    recordJumpToThrowException(ExceptionType::OutOfBoundsArrayFill, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

bool BBQJIT::emitStructSet(GPRReg structGPR, const StructType& structType, uint32_t fieldIndex, Value value)
{
    unsigned fieldOffset = JSWebAssemblyStruct::offsetOfData() + structType.offsetOfFieldInPayload(fieldIndex);
    RELEASE_ASSERT((std::numeric_limits<int32_t>::max() & fieldOffset) == fieldOffset);

    StorageType storageType = structType.field(fieldIndex).type;

    JIT_COMMENT(m_jit, "emitStructSet for ", fieldIndex, " (", storageType, ") in ", structType);
    emitMove(storageType, value, Address(structGPR, fieldOffset));

    if (value.isConst()) {
        ASSERT_IMPLIES(isRefType(storageType.unpacked()), !JSValue::decode(value.asI64()).isCell());
        return false;
    }

    consume(value);
    return isRefType(storageType.unpacked());
}

void BBQJIT::emitAllocateGCStructUninitialized(GPRReg resultGPR, uint32_t typeIndex, GPRReg scratchGPR, GPRReg scratchGPR2)
{
    RELEASE_ASSERT(m_info.hasGCObjectTypes());
    JumpList slowPath;
    const StructType* typeDefinition = m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    MacroAssembler::Address allocatorBufferBase(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfAllocatorForGCObject(m_info, 0));
    MacroAssembler::Address structureIDAddress(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfGCObjectStructureID(m_info, typeIndex));
    Location sizeLocation;

    size_t sizeInBytes = JSWebAssemblyStruct::allocationSize(typeDefinition->instancePayloadSize());

    if (sizeInBytes <= MarkedSpace::largeCutoff) {
        size_t sizeClassIndex = MarkedSpace::sizeClassToIndex(sizeInBytes);
        m_jit.loadPtr(allocatorBufferBase.withOffset(sizeClassIndex * sizeof(Allocator)), scratchGPR2);
        JIT_COMMENT(m_jit, "Do struct allocation");
        m_jit.emitAllocateWithNonNullAllocator(resultGPR, JITAllocator::variableNonNull(), scratchGPR2, scratchGPR, slowPath, AssemblyHelpers::SlowAllocationResult::UndefinedBehavior);
        m_jit.load32(structureIDAddress, scratchGPR);
        m_jit.move(TrustedImm32(JSWebAssemblyStruct::typeInfoBlob().blob()), scratchGPR2);
        static_assert(JSCell::structureIDOffset() + sizeof(int32_t) == JSCell::indexingTypeAndMiscOffset());
        m_jit.storePair32(scratchGPR, scratchGPR2, MacroAssembler::Address(resultGPR, JSCell::structureIDOffset()));
        m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(resultGPR, JSObject::butterflyOffset()));
        m_jit.store32(TrustedImm32(typeDefinition->instancePayloadSize()), MacroAssembler::Address(resultGPR, JSWebAssemblyStruct::offsetOfSize()));
    } else {
        // FIXME: emitCCall can't handle being passed a destination... which is why we just jump to the slow path here.
        slowPath.append(m_jit.jump());
    }

    JIT_COMMENT(m_jit, "Slow path return");
    MacroAssembler::Label done(m_jit);
    m_slowPaths.append({ origin(), WTFMove(slowPath), WTFMove(done), copyBindings(), [typeIndex, resultGPR](BBQJIT& bbq, CCallHelpers& jit) {
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationWasmStructNewEmpty)>(GPRInfo::wasmContextInstancePointer, TrustedImm32(typeIndex));
        jit.callOperation<OperationPtrTag>(operationWasmStructNewEmpty);
        jit.move(GPRInfo::returnValueGPR, resultGPR);
        bbq.emitThrowOnNullReference(ExceptionType::BadStructNew, Location::fromGPR(resultGPR));
    }});
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructNewDefault(uint32_t typeIndex, ExpressionType& result)
{
    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    GPRReg resultGPR;
    {
        ScratchScope<2, 0> scratches(*this);
        resultGPR = scratches.gpr(0);
        GPRReg scratchGPR = scratches.gpr(1);
        emitAllocateGCStructUninitialized(resultGPR, typeIndex, wasmScratchGPR, scratchGPR);

        JIT_COMMENT(m_jit, "Struct allocation done, do initialization");
        bool needsMutatorFence = false;
        for (StructFieldCount i = 0; i < structType.fieldCount(); ++i) {
            if (Wasm::isRefType(structType.field(i).type))
                needsMutatorFence |= emitStructSet(resultGPR, structType, i, Value::fromRef(TypeKind::RefNull, JSValue::encode(jsNull())));
            else if (structType.field(i).type.unpacked().isV128()) {
                materializeVectorConstant(v128_t { }, Location::fromFPR(wasmScratchFPR));
                needsMutatorFence |= emitStructSet(resultGPR, structType, i, Value::pinned(TypeKind::V128, Location::fromFPR(wasmScratchFPR)));
            } else
                needsMutatorFence |= emitStructSet(resultGPR, structType, i, Value::fromI64(0));
        }

        // No write barrier needed here as all fields are set to constants.
        ASSERT_UNUSED(needsMutatorFence, !needsMutatorFence);
    }

    result = topValue(TypeKind::Ref);
    bind(result, Location::fromGPR(resultGPR));

#if ASSERT_ENABLED
    auto debugStructType = &structType;
    m_jit.probeDebug([=] (Probe::Context& context) {
        auto* structPtr = context.gpr<JSWebAssemblyStruct*>(resultGPR);
        for (unsigned i = 0; i < debugStructType->fieldCount(); ++i) {
            auto type = debugStructType->field(i).type.unpacked();
            if (type.kind != TypeKind::V128)
                validateWasmValue(structPtr->get(i), type);
        }
    });
#endif

    LOG_INSTRUCTION("StructNewDefault", typeIndex, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructNew(uint32_t typeIndex, ArgumentList& args, Value& result)
{
    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    GPRReg resultGPR;
    {
        ScratchScope<2, 0> scratches(*this);
        resultGPR = scratches.gpr(0);
        GPRReg scratchGPR = scratches.gpr(1);
        emitAllocateGCStructUninitialized(resultGPR, typeIndex, wasmScratchGPR, scratchGPR);

        JIT_COMMENT(m_jit, "Struct allocation done, do initialization");
        bool needsMutatorFence = false;
        for (uint32_t i = 0; i < args.size(); ++i)
            needsMutatorFence |= emitStructSet(resultGPR, structType, i, args[i]);

        if (needsMutatorFence)
            emitMutatorFence();

    }

    result = topValue(TypeKind::Ref);
    bind(result, Location::fromGPR(resultGPR));

#if ASSERT_ENABLED
    auto debugStructType = &structType;
    m_jit.probeDebug([=] (Probe::Context& context) {
        auto* structPtr = context.gpr<JSWebAssemblyStruct*>(resultGPR);
        for (unsigned i = 0; i < debugStructType->fieldCount(); ++i) {
            auto type = debugStructType->field(i).type.unpacked();
            if (type.kind != TypeKind::V128)
                validateWasmValue(structPtr->get(i), type);
        }
    });
#endif

    LOG_INSTRUCTION("StructNew", typeIndex, args, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructGet(ExtGCOpType structGetKind, TypedExpression typedStruct, const StructType& structType, uint32_t fieldIndex, Value& result)
{
    auto structValue = typedStruct.value();
    TypeKind resultKind = structType.field(fieldIndex).type.unpacked().kind;
    if (structValue.isConst()) {
        // This is the only constant struct currently possible.
        ASSERT(JSValue::decode(structValue.asRef()).isNull());
        emitThrowException(ExceptionType::NullAccess);
        result = topValue(resultKind);
        LOG_INSTRUCTION("StructGet", structValue, fieldIndex, "Exception");
        return { };
    }

    unsigned fieldOffset = JSWebAssemblyStruct::offsetOfData() + structType.offsetOfFieldInPayload(fieldIndex);
    RELEASE_ASSERT((std::numeric_limits<int32_t>::max() & fieldOffset) == fieldOffset);

    Location structLocation = loadIfNecessary(structValue);
    if (typedStruct.type().isNullable())
        emitThrowOnNullReferenceBeforeAccess(structLocation, fieldOffset);

    // We're ok with reusing the struct value for our result since their live ranges don't overlap within a struct.get.
    consume(structValue);
    result = topValue(resultKind);
    Location resultLocation = allocate(result);

    JIT_COMMENT(m_jit, "emitStructGet for ", fieldIndex, " in ", structType);
    switch (result.type()) {
    case TypeKind::I32:
        if (structType.field(fieldIndex).type.is<PackedType>()) {
            switch (structType.field(fieldIndex).type.as<PackedType>()) {
            case PackedType::I8:
                m_jit.load8(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asGPR());
                break;
            case PackedType::I16:
                m_jit.load16(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asGPR());
                break;
            }
            switch (structGetKind) {
            case ExtGCOpType::StructGetU:
                LOG_INSTRUCTION("StructGetU", structValue, fieldIndex, RESULT(result));
                return { };
            case ExtGCOpType::StructGetS: {
                uint8_t bitShift = (sizeof(uint32_t) - structType.field(fieldIndex).type.elementSize()) * 8;
                m_jit.lshift32(TrustedImm32(bitShift), resultLocation.asGPR());
                m_jit.rshift32(TrustedImm32(bitShift), resultLocation.asGPR());
                LOG_INSTRUCTION("StructGetS", structValue, fieldIndex, RESULT(result));
                return { };
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
                return { };
            }
        }
        m_jit.load32(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asGPR());
        break;
    case TypeKind::I64:
        m_jit.load64(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asGPR());
        break;
    case TypeKind::F32:
        m_jit.loadFloat(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asFPR());
        break;
    case TypeKind::F64:
        m_jit.loadDouble(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asFPR());
        break;
    case TypeKind::V128:
        m_jit.loadVector(MacroAssembler::Address(structLocation.asGPR(), fieldOffset), resultLocation.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

#if ASSERT_ENABLED
    if (isRefType(structType.field(fieldIndex).type.unpacked())) {
        auto resultGPR = resultLocation.asGPR();
        auto debugStructType = &structType;
        m_jit.probeDebug([=] (Probe::Context& context) {
            auto type = debugStructType->field(fieldIndex).type.unpacked();
            validateWasmValue(context.gpr<uint64_t>(resultGPR), type);
        });
    }
#endif

    LOG_INSTRUCTION("StructGet", structValue, fieldIndex, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructSet(TypedExpression typedStruct, const StructType& structType, uint32_t fieldIndex, Value value)
{
    auto structValue = typedStruct.value();
    if (structValue.isConst()) {
        // This is the only constant struct currently possible.
        ASSERT(JSValue::decode(structValue.asRef()).isNull());

        LOG_INSTRUCTION("StructSet", structValue, fieldIndex, value, "Exception");
        consume(value);
        emitThrowException(ExceptionType::NullAccess);
        return { };
    }

    unsigned fieldOffset = JSWebAssemblyStruct::offsetOfData() + structType.offsetOfFieldInPayload(fieldIndex);
    RELEASE_ASSERT((std::numeric_limits<int32_t>::max() & fieldOffset) == fieldOffset);

    Location structLocation = loadIfNecessary(structValue);
    if (typedStruct.type().isNullable())
        emitThrowOnNullReferenceBeforeAccess(structLocation, fieldOffset);

    bool needsWriteBarrier = emitStructSet(structLocation.asGPR(), structType, fieldIndex, value);
    if (needsWriteBarrier)
        emitWriteBarrier(structLocation.asGPR());

    LOG_INSTRUCTION("StructSet", structValue, fieldIndex, value);

    consume(structValue);

    return { };
}

void BBQJIT::emitRefTestOrCast(CastKind castKind, const TypedExpression& typedValue, GPRReg valueGPR, bool allowNull, int32_t toHeapType, JumpList& failureCases)
{
    auto castAccessOffset = [&] -> std::optional<ptrdiff_t> {
        if (castKind == CastKind::Test)
            return std::nullopt;

        if (allowNull)
            return std::nullopt;

        if (typeIndexIsType(static_cast<Wasm::TypeIndex>(toHeapType)))
            return std::nullopt;

        Wasm::TypeDefinition& signature = m_info.typeSignatures[toHeapType];
        if (signature.expand().is<Wasm::FunctionSignature>())
            return WebAssemblyFunctionBase::offsetOfRTT();

        if (!typedValue.type().definitelyIsCellOrNull())
            return std::nullopt;

        if (!typedValue.type().definitelyIsWasmGCObjectOrNull())
            return JSCell::typeInfoTypeOffset();
        return JSCell::structureIDOffset();
    };

    JumpList doneCases;
    if (typedValue.type().isNullable()) {
        if (auto offset = castAccessOffset(); offset && offset.value() <= maxAcceptableOffsetForNullReference()) {
            // We will have access which will be trapped.
        } else {
            if (allowNull)
                doneCases.append(m_jit.branchIfNull(valueGPR));
            else
                failureCases.append(m_jit.branchIfNull(valueGPR));
        }
    }

    if (typeIndexIsType(static_cast<Wasm::TypeIndex>(toHeapType))) {
        switch (static_cast<TypeKind>(toHeapType)) {
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::Externref:
        case Wasm::TypeKind::Anyref:
        case Wasm::TypeKind::Exnref:
            // Casts to these types cannot fail as they are the top types of their respective hierarchies, and static type-checking does not allow cross-hierarchy casts.
            break;
        case Wasm::TypeKind::Noneref:
        case Wasm::TypeKind::Nofuncref:
        case Wasm::TypeKind::Noexternref:
        case Wasm::TypeKind::Noexnref:
            // Casts to any bottom type should always fail.
            failureCases.append(m_jit.jump());
            break;
        case Wasm::TypeKind::Eqref: {
            auto checkObjectCase = m_jit.branchIfNotInt32(valueGPR, DoNotHaveTagRegisters);
            failureCases.append(m_jit.branch32(CCallHelpers::GreaterThan, valueGPR, TrustedImm32(Wasm::maxI31ref)));
            failureCases.append(m_jit.branch32(CCallHelpers::LessThan, valueGPR, TrustedImm32(Wasm::minI31ref)));
            doneCases.append(m_jit.jump());

            checkObjectCase.link(m_jit);
            if (!typedValue.type().definitelyIsCellOrNull())
                failureCases.append(m_jit.branchIfNotCell(valueGPR, DoNotHaveTagRegisters));
            if (!typedValue.type().definitelyIsWasmGCObjectOrNull())
                failureCases.append(m_jit.branchIfNotType(valueGPR, JSType::WebAssemblyGCObjectType));
            break;
        }
        case Wasm::TypeKind::I31ref: {
            failureCases.append(m_jit.branchIfNotInt32(valueGPR, DoNotHaveTagRegisters));
            failureCases.append(m_jit.branch32(CCallHelpers::GreaterThan, valueGPR, TrustedImm32(Wasm::maxI31ref)));
            failureCases.append(m_jit.branch32(CCallHelpers::LessThan, valueGPR, TrustedImm32(Wasm::minI31ref)));
            break;
        }
        case Wasm::TypeKind::Arrayref:
        case Wasm::TypeKind::Structref: {
            if (!typedValue.type().definitelyIsCellOrNull())
                failureCases.append(m_jit.branchIfNotCell(valueGPR, DoNotHaveTagRegisters));
            if (!typedValue.type().definitelyIsWasmGCObjectOrNull())
                failureCases.append(m_jit.branchIfNotType(valueGPR, JSType::WebAssemblyGCObjectType));
            m_jit.emitLoadStructure(valueGPR, wasmScratchGPR);
            m_jit.loadPtr(Address(wasmScratchGPR, WebAssemblyGCStructure::offsetOfRTT()), wasmScratchGPR);
            failureCases.append(m_jit.branch8(CCallHelpers::NotEqual, Address(wasmScratchGPR, RTT::offsetOfKind()), TrustedImm32(static_cast<int32_t>(static_cast<TypeKind>(toHeapType) == Wasm::TypeKind::Arrayref ? RTTKind::Array : RTTKind::Struct))));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else {
        ([&] {
            Wasm::TypeDefinition& signature = m_info.typeSignatures[toHeapType];
            Ref targetRTT = m_info.rtts[toHeapType];
            if (signature.expand().is<Wasm::FunctionSignature>())
                m_jit.loadPtr(Address(valueGPR, WebAssemblyFunctionBase::offsetOfRTT()), wasmScratchGPR);
            else {
                // The cell check is only needed for non-functions, as the typechecker does not allow non-Cell values for funcref casts.
                if (!typedValue.type().definitelyIsCellOrNull())
                    failureCases.append(m_jit.branchIfNotCell(valueGPR, DoNotHaveTagRegisters));
                if (!typedValue.type().definitelyIsWasmGCObjectOrNull())
                    failureCases.append(m_jit.branchIfNotType(valueGPR, JSType::WebAssemblyGCObjectType));
                m_jit.emitLoadStructure(valueGPR, wasmScratchGPR);

                if (targetRTT->displaySizeExcludingThis() < WebAssemblyGCStructure::inlinedTypeDisplaySize) {
                    m_jit.loadPtr(Address(wasmScratchGPR, WebAssemblyGCStructure::offsetOfInlinedTypeDisplay() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const RTT>)), wasmScratchGPR);
                    failureCases.append(m_jit.branchPtr(CCallHelpers::NotEqual, wasmScratchGPR, TrustedImmPtr(targetRTT.ptr())));
                    return;
                }
                m_jit.loadPtr(Address(wasmScratchGPR, WebAssemblyGCStructure::offsetOfRTT()), wasmScratchGPR);
            }

            if (signature.isFinalType()) {
                // If signature is final type and pointer equality failed, this value must not be a subtype.
                failureCases.append(m_jit.branchPtr(CCallHelpers::NotEqual, wasmScratchGPR, TrustedImmPtr(targetRTT.ptr())));
            } else {
                doneCases.append(m_jit.branchPtr(CCallHelpers::Equal, wasmScratchGPR, TrustedImmPtr(targetRTT.ptr())));
                failureCases.append(m_jit.branch32(CCallHelpers::BelowOrEqual, Address(wasmScratchGPR, RTT::offsetOfDisplaySizeExcludingThis()), TrustedImm32(targetRTT->displaySizeExcludingThis())));
                failureCases.append(m_jit.branchPtr(CCallHelpers::NotEqual, Address(wasmScratchGPR, (RTT::offsetOfData() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const RTT>))), TrustedImmPtr(targetRTT.ptr())));
            }
        }());
    }

    doneCases.link(m_jit);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefCast(TypedExpression typedValue, bool allowNull, int32_t toHeapType, ExpressionType& result)
{
    auto value = typedValue.value();
    Location valueLocation;
    if (value.isConst()) {
        valueLocation = Location::fromGPR(wasmScratchGPR);
        emitMoveConst(value, valueLocation);
    } else
        valueLocation = loadIfNecessary(value);
    ASSERT(valueLocation.isGPR());
    consume(value);

    result = topValue(TypeKind::Ref);
    Location resultLocation = allocate(result);

    JumpList failureCases;
    emitRefTestOrCast(CastKind::Cast, typedValue, valueLocation.asGPR(), allowNull, toHeapType, failureCases);
    recordJumpToThrowException(ExceptionType::CastFailure, failureCases);
    emitMove(TypeKind::Ref, valueLocation, resultLocation);

    LOG_INSTRUCTION("RefCast", value, allowNull, toHeapType, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefTest(TypedExpression typedValue, bool allowNull, int32_t toHeapType, bool shouldNegate, ExpressionType& result)
{
    auto value = typedValue.value();
    Location valueLocation;
    if (value.isConst()) {
        valueLocation = Location::fromGPR(wasmScratchGPR);
        emitMoveConst(value, valueLocation);
    } else
        valueLocation = loadIfNecessary(value);
    ASSERT(valueLocation.isGPR());
    consume(value);

    result = topValue(TypeKind::I32);
    Location resultLocation = allocate(result);

    JumpList doneCases;
    JumpList failureCases;

    emitRefTestOrCast(CastKind::Test, typedValue, valueLocation.asGPR(), allowNull, toHeapType, failureCases);
    emitMoveConst(Value::fromI32(shouldNegate ? 0 : 1), resultLocation);
    doneCases.append(m_jit.jump());

    failureCases.link(m_jit);
    emitMoveConst(Value::fromI32(shouldNegate ? 1 : 0), resultLocation);

    doneCases.link(m_jit);

    LOG_INSTRUCTION("RefTest", value, allowNull, toHeapType, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Add(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Add", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() + rhs.asI64())),
        BLOCK(
            m_jit.add64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (isRepresentableAs<int32_t>(ImmHelpers::imm(lhs, rhs).asI64())) [[likely]]
                m_jit.add64(TrustedImm32(ImmHelpers::imm(lhs, rhs).asI64()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
            else {
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.add64(Imm64(ImmHelpers::imm(lhs, rhs).asI64()), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Sub(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Sub", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() - rhs.asI64())),
        BLOCK(
            m_jit.sub64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst()) {
                m_jit.sub64(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), Imm64(ImmHelpers::imm(lhs, rhs).asI64()), resultLocation.asGPR());
            } else {
                emitMoveConst(lhs, Location::fromGPR(wasmScratchGPR));
                m_jit.sub64(wasmScratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Mul(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Mul", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() * rhs.asI64())),
        BLOCK(
            m_jit.mul64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR(wasmScratchGPR);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR(wasmScratchGPR));
            m_jit.mul64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        )
    );
}

void BBQJIT::emitThrowOnNullReference(ExceptionType type, Location ref)
{
    recordJumpToThrowException(type, m_jit.branchIfNull(ref.asGPR()));
}

void BBQJIT::emitThrowOnNullReferenceBeforeAccess(Location ref, ptrdiff_t offset)
{
    if (Options::useWasmFaultSignalHandler()) {
        if (offset <= maxAcceptableOffsetForNullReference())
            return;
    }
    emitThrowOnNullReference(ExceptionType::NullAccess, ref);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64And(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64And", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() & rhs.asI64())),
        BLOCK(
            m_jit.and64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.and64(Imm64(ImmHelpers::imm(lhs, rhs).asI64()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Xor(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Xor", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() ^ rhs.asI64())),
        BLOCK(
            m_jit.xor64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.xor64(Imm64(ImmHelpers::imm(lhs, rhs).asI64()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Or(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Or", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() | rhs.asI64())),
        BLOCK(
            m_jit.or64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            m_jit.or64(Imm64(ImmHelpers::imm(lhs, rhs).asI64()), ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Shl(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I64Shl", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() << rhs.asI64())),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.lshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.lshift64(lhsLocation.asGPR(), TrustedImm32(rhs.asI64()), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
                m_jit.lshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ShrS(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I64ShrS", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() >> rhs.asI64())),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.rshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rshift64(lhsLocation.asGPR(), TrustedImm32(rhs.asI64()), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
                m_jit.rshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ShrU(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I64ShrU", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<uint64_t>(lhs.asI64()) >> static_cast<uint64_t>(rhs.asI64()))),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.urshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.urshift64(lhsLocation.asGPR(), TrustedImm32(rhs.asI64()), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
                m_jit.urshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Rotl(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I64Rotl", TypeKind::I64,
        BLOCK(Value::fromI64(B3::rotateLeft(lhs.asI64(), rhs.asI64()))),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.rotateLeft64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rotateLeft64(lhsLocation.asGPR(), TrustedImm32(rhs.asI32()), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, resultLocation);
                m_jit.rotateLeft64(resultLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Rotr(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I64Rotr", TypeKind::I64,
        BLOCK(Value::fromI64(B3::rotateRight(lhs.asI64(), rhs.asI64()))),
        BLOCK(
            moveShiftAmountIfNecessary(rhsLocation);
            m_jit.rotateRight64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (rhs.isConst())
                m_jit.rotateRight64(lhsLocation.asGPR(), TrustedImm32(rhs.asI64()), resultLocation.asGPR());
            else {
                moveShiftAmountIfNecessary(rhsLocation);
                emitMoveConst(lhs, Location::fromGPR(wasmScratchGPR));
                m_jit.rotateRight64(wasmScratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
            }
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Clz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Clz", TypeKind::I64,
        BLOCK(Value::fromI64(WTF::clz(operand.asI64()))),
        BLOCK(
            m_jit.countLeadingZeros64(operandLocation.asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Ctz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Ctz", TypeKind::I64,
        BLOCK(Value::fromI64(WTF::ctz(operand.asI64()))),
        BLOCK(
            m_jit.countTrailingZeros64(operandLocation.asGPR(), resultLocation.asGPR());
        )
    );
}

PartialResult BBQJIT::emitCompareI64(const char* opcode, Value& lhs, Value& rhs, Value& result, RelationalCondition condition, bool (*comparator)(int64_t lhs, int64_t rhs))
{
    EMIT_BINARY(
        opcode, TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asI64(), rhs.asI64())))),
        BLOCK(
            m_jit.compare64(condition, lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
        ),
        BLOCK(
            if (lhs.isConst())
                m_jit.compare64(condition, Imm64(lhs.asI64()), rhsLocation.asGPR(), resultLocation.asGPR());
            else
                m_jit.compare64(condition, lhsLocation.asGPR(), Imm64(rhs.asI64()), resultLocation.asGPR());
        )
    )
}

PartialResult BBQJIT::addI32WrapI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32WrapI64", TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(operand.asI64()))),
        BLOCK(
            m_jit.zeroExtend32ToWord(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Extend8S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Extend8S", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int8_t>(operand.asI64())))),
        BLOCK(
            m_jit.signExtend8To64(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Extend16S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Extend16S", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int16_t>(operand.asI64())))),
        BLOCK(
            m_jit.signExtend16To64(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Extend32S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Extend32S", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int32_t>(operand.asI64())))),
        BLOCK(
            m_jit.signExtend32To64(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ExtendSI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64ExtendSI32", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(operand.asI32()))),
        BLOCK(
            m_jit.signExtend32To64(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ExtendUI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64ExtendUI32", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<uint64_t>(static_cast<uint32_t>(operand.asI32())))),
        BLOCK(
            m_jit.zeroExtend32ToWord(operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Eqz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Eqz", TypeKind::I32,
        BLOCK(Value::fromI32(!operand.asI64())),
        BLOCK(
            m_jit.test64(ResultCondition::Zero, operandLocation.asGPR(), operandLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ReinterpretF64(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64ReinterpretF64", TypeKind::I64,
        BLOCK(Value::fromI64(std::bit_cast<int64_t>(operand.asF64()))),
        BLOCK(
            m_jit.moveDoubleTo64(operandLocation.asFPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ReinterpretI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ReinterpretI64", TypeKind::F64,
        BLOCK(Value::fromF64(std::bit_cast<double>(operand.asI64()))),
        BLOCK(
            m_jit.move64ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32ConvertUI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ConvertUI32", TypeKind::F32,
        BLOCK(Value::fromF32(static_cast<uint32_t>(operand.asI32()))),
        BLOCK(
            m_jit.convertUInt32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32ConvertSI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ConvertSI64", TypeKind::F32,
        BLOCK(Value::fromF32(operand.asI64())),
        BLOCK(
            m_jit.convertInt64ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32ConvertUI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ConvertUI64", TypeKind::F32,
        BLOCK(Value::fromF32(static_cast<uint64_t>(operand.asI64()))),
        BLOCK(
#if CPU(X86_64)
            m_jit.convertUInt64ToFloat(operandLocation.asGPR(), resultLocation.asFPR(), wasmScratchGPR);
#else
            m_jit.convertUInt64ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
#endif
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ConvertUI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ConvertUI32", TypeKind::F64,
        BLOCK(Value::fromF64(static_cast<uint32_t>(operand.asI32()))),
        BLOCK(
            m_jit.convertUInt32ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ConvertSI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ConvertSI64", TypeKind::F64,
        BLOCK(Value::fromF64(operand.asI64())),
        BLOCK(
            m_jit.convertInt64ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ConvertUI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ConvertUI64", TypeKind::F64,
        BLOCK(Value::fromF64(static_cast<uint64_t>(operand.asI64()))),
        BLOCK(
#if CPU(X86_64)
            m_jit.convertUInt64ToDouble(operandLocation.asGPR(), resultLocation.asFPR(), wasmScratchGPR);
#else
            m_jit.convertUInt64ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
#endif
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Floor(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Floor", TypeKind::F32,
        BLOCK(Value::fromF32(Math::floorFloat(operand.asF32()))),
        BLOCK(
            m_jit.floorFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Floor(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Floor", TypeKind::F64,
        BLOCK(Value::fromF64(Math::floorDouble(operand.asF64()))),
        BLOCK(
            m_jit.floorDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Ceil(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Ceil", TypeKind::F32,
        BLOCK(Value::fromF32(Math::ceilFloat(operand.asF32()))),
        BLOCK(
            m_jit.ceilFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Ceil(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Ceil", TypeKind::F64,
        BLOCK(Value::fromF64(Math::ceilDouble(operand.asF64()))),
        BLOCK(
            m_jit.ceilDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Nearest(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Nearest", TypeKind::F32,
        BLOCK(Value::fromF32(std::nearbyintf(operand.asF32()))),
        BLOCK(
            m_jit.roundTowardNearestIntFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Nearest(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Nearest", TypeKind::F64,
        BLOCK(Value::fromF64(std::nearbyint(operand.asF64()))),
        BLOCK(
            m_jit.roundTowardNearestIntDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Trunc(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Trunc", TypeKind::F32,
        BLOCK(Value::fromF32(Math::truncFloat(operand.asF32()))),
        BLOCK(
            m_jit.truncFloat(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Trunc(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Trunc", TypeKind::F64,
        BLOCK(Value::fromF64(Math::truncDouble(operand.asF64()))),
        BLOCK(
            m_jit.truncDouble(operandLocation.asFPR(), resultLocation.asFPR());
        )
    )
}

// References

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefIsNull(Value operand, Value& result)
{
    EMIT_UNARY(
        "RefIsNull", TypeKind::I32,
        BLOCK(Value::fromI32(operand.asRef() == JSValue::encode(jsNull()))),
        BLOCK(
            ASSERT(JSValue::encode(jsNull()) >= 0 && JSValue::encode(jsNull()) <= INT32_MAX);
            m_jit.compare64(RelationalCondition::Equal, operandLocation.asGPR(), TrustedImm32(static_cast<int32_t>(JSValue::encode(jsNull()))), resultLocation.asGPR());
        )
    );
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefAsNonNull(Value value, Value& result)
{
    Location valueLocation;
    if (value.isConst()) {
        valueLocation = Location::fromGPR(wasmScratchGPR);
        emitMoveConst(value, valueLocation);
    } else
        valueLocation = loadIfNecessary(value);
    ASSERT(valueLocation.isGPR());
    consume(value);

    result = topValue(TypeKind::Ref);
    Location resultLocation = allocate(result);
    ASSERT(JSValue::encode(jsNull()) >= 0 && JSValue::encode(jsNull()) <= INT32_MAX);
    emitThrowOnNullReference(ExceptionType::NullRefAsNonNull, valueLocation);
    emitMove(TypeKind::Ref, valueLocation, resultLocation);

    return { };
}

void BBQJIT::emitCatchPrologue()
{
    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), GPRInfo::nonPreservedNonArgumentGPR0));
#if CPU(ARM64)
    m_jit.subPtr(GPRInfo::callFrameRegister, GPRInfo::nonPreservedNonArgumentGPR0, MacroAssembler::stackPointerRegister);
#else
    m_jit.subPtr(GPRInfo::callFrameRegister, GPRInfo::nonPreservedNonArgumentGPR0, GPRInfo::nonPreservedNonArgumentGPR0);
    m_jit.move(GPRInfo::nonPreservedNonArgumentGPR0, CCallHelpers::stackPointerRegister);
#endif
    if (!!m_info.memory)
        loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
    static_assert(noOverlap(GPRInfo::nonPreservedNonArgumentGPR0, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2));
}

void BBQJIT::emitCatchAllImpl(ControlData& dataCatch)
{
    m_catchEntrypoints.append(m_jit.label());
    emitCatchPrologue();
    bind(this->exception(dataCatch), Location::fromGPR(GPRInfo::returnValueGPR));
    Stack emptyStack { };
    dataCatch.startBlock(*this, emptyStack);
}

void BBQJIT::emitCatchImpl(ControlData& dataCatch, const TypeDefinition& exceptionSignature, ResultList& results)
{
    m_catchEntrypoints.append(m_jit.label());
    emitCatchPrologue();
    bind(this->exception(dataCatch), Location::fromGPR(GPRInfo::returnValueGPR));
    Stack emptyStack { };
    dataCatch.startBlock(*this, emptyStack);

    if (exceptionSignature.as<FunctionSignature>()->argumentCount()) {
        m_jit.loadPtr(Address(GPRInfo::returnValueGPR, JSWebAssemblyException::offsetOfPayload() + JSWebAssemblyException::Payload::offsetOfStorage()), wasmScratchGPR);
        unsigned offset = 0;
        for (unsigned i = 0; i < exceptionSignature.as<FunctionSignature>()->argumentCount(); ++i) {
            Type type = exceptionSignature.as<FunctionSignature>()->argumentType(i);
            Value result = Value::fromTemp(type.kind, dataCatch.enclosedHeight() + dataCatch.implicitSlots() + i);
            Location slot = canonicalSlot(result);
            switch (type.kind) {
            case TypeKind::I32:
                m_jit.transfer32(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                break;
            case TypeKind::I31ref:
            case TypeKind::I64:
            case TypeKind::Ref:
            case TypeKind::RefNull:
            case TypeKind::Arrayref:
            case TypeKind::Structref:
            case TypeKind::Funcref:
            case TypeKind::Exnref:
            case TypeKind::Externref:
            case TypeKind::Eqref:
            case TypeKind::Anyref:
            case TypeKind::Noexnref:
            case TypeKind::Noneref:
            case TypeKind::Nofuncref:
            case TypeKind::Noexternref:
            case TypeKind::Rec:
            case TypeKind::Sub:
            case TypeKind::Subfinal:
            case TypeKind::Array:
            case TypeKind::Struct:
            case TypeKind::Func:
                m_jit.transfer64(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                break;
            case TypeKind::F32:
                m_jit.transfer32(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                break;
            case TypeKind::F64:
                m_jit.transfer64(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                break;
            case TypeKind::V128:
                m_jit.transferVector(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                break;
            case TypeKind::Void:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            bind(result, slot);
            results.append(result);
            offset += type.kind == TypeKind::V128 ? 2 : 1;
        }
    }
}

void BBQJIT::emitCatchTableImpl(ControlData& entryData, ControlType::TryTableTarget& target)
{
    HandlerType handlerType;
    switch (target.type) {
    case CatchKind::Catch:
        handlerType = HandlerType::TryTableCatch;
        break;
    case CatchKind::CatchRef:
        handlerType = HandlerType::TryTableCatchRef;
        break;
    case CatchKind::CatchAll:
        handlerType = HandlerType::TryTableCatchAll;
        break;
    case CatchKind::CatchAllRef:
        handlerType = HandlerType::TryTableCatchAllRef;
        break;
    }

    JIT_COMMENT(m_jit, "catch handler");
    m_catchEntrypoints.append(m_jit.label());
    m_exceptionHandlers.append({ handlerType, entryData.tryStart(), m_callSiteIndex, 0, m_tryCatchDepth, target.tag });
    emitCatchPrologue();

    auto& targetControl = m_parser->resolveControlRef(target.target).controlData;
    if (target.type == CatchKind::CatchRef || target.type == CatchKind::CatchAllRef) {
        if (targetControl.targetLocations().last().isGPR())
            m_jit.move(GPRInfo::returnValueGPR, targetControl.targetLocations().last().asGPR());
        else
            m_jit.storePtr(GPRInfo::returnValueGPR, targetControl.targetLocations().last().asAddress());
    }

    if (target.type == CatchKind::Catch || target.type == CatchKind::CatchRef) {
        auto signature = target.exceptionSignature->template as<FunctionSignature>();
        if (signature->argumentCount()) {
            m_jit.loadPtr(Address(GPRInfo::returnValueGPR, JSWebAssemblyException::offsetOfPayload() + JSWebAssemblyException::Payload::offsetOfStorage()), wasmScratchGPR);
            unsigned offset = 0;
            for (unsigned i = 0; i < signature->argumentCount(); ++i) {
                Type type = signature->argumentType(i);
                Location slot = targetControl.targetLocations()[i];
                switch (type.kind) {
                case TypeKind::I32:
                    if (slot.isGPR())
                        m_jit.load32(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asGPR());
                    else
                        m_jit.transfer32(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                    break;
                case TypeKind::I31ref:
                case TypeKind::I64:
                case TypeKind::Ref:
                case TypeKind::RefNull:
                case TypeKind::Arrayref:
                case TypeKind::Structref:
                case TypeKind::Funcref:
                case TypeKind::Exnref:
                case TypeKind::Externref:
                case TypeKind::Eqref:
                case TypeKind::Anyref:
                case TypeKind::Noexnref:
                case TypeKind::Noneref:
                case TypeKind::Nofuncref:
                case TypeKind::Noexternref:
                case TypeKind::Rec:
                case TypeKind::Sub:
                case TypeKind::Subfinal:
                case TypeKind::Array:
                case TypeKind::Struct:
                case TypeKind::Func:
                    if (slot.isGPR())
                        m_jit.load64(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asGPR());
                    else
                        m_jit.transfer64(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                    break;
                case TypeKind::F32:
                    if (slot.isFPR())
                        m_jit.loadFloat(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asFPR());
                    else
                        m_jit.transfer32(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                    break;
                case TypeKind::F64:
                    if (slot.isFPR())
                        m_jit.loadDouble(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asFPR());
                    else
                        m_jit.transfer64(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                    break;
                case TypeKind::V128:
                    if (slot.isFPR())
                        m_jit.loadVector(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asFPR());
                    else
                        m_jit.transferVector(Address(wasmScratchGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                    break;
                case TypeKind::Void:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                offset += type.kind == TypeKind::V128 ? 2 : 1;
            }
        }
    }

    // jump to target
    targetControl.addBranch(m_jit.jump());
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addThrowRef(Value exception, Stack&)
{
    LOG_INSTRUCTION("ThrowRef", exception);

    emitMove(exception, Location::fromGPR(GPRInfo::argumentGPR1));
    consume(exception);

    ++m_callSiteIndex;
    if (m_profiledCallee.hasExceptionHandlers()) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegisters();
    }

    // Check for a null exception
    m_jit.move(CCallHelpers::TrustedImmPtr(JSValue::encode(jsNull())), wasmScratchGPR);
    auto noexnref = m_jit.branchPtr(CCallHelpers::Equal, GPRInfo::argumentGPR1, wasmScratchGPR);

    m_jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    emitThrowRefImpl(m_jit);

    noexnref.linkTo(m_jit.label(), &m_jit);

    emitThrowException(ExceptionType::NullExnrefReference);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRethrow(unsigned, ControlType& data)
{
    LOG_INSTRUCTION("Rethrow", exception(data));

    ++m_callSiteIndex;
    if (m_profiledCallee.hasExceptionHandlers()) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegisters();
    }
    emitMove(this->exception(data), Location::fromGPR(GPRInfo::argumentGPR1));
    m_jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    emitThrowRefImpl(m_jit);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addBranchNull(ControlData& data, ExpressionType reference, Stack& returnValues, bool shouldNegate, ExpressionType& result)
{
    if (reference.isConst() && (reference.asRef() == JSValue::encode(jsNull())) == shouldNegate) {
        // If branch is known to be not-taken, exit early.
        if (!shouldNegate)
            result = reference;
        return { };
    }

    // The way we use referenceLocation is a little tricky, here's the breakdown:
    //
    //  - For a br_on_null, we discard the reference when the branch is taken. In
    //    this case, we consume the reference as if it was popped (since it was),
    //    but use its referenceLocation after the branch. This is safe, because
    //    in the case we don't take the branch, the only operations between
    //    materializing the ref and writing the result are (1) flushing at the
    //    block boundary, which can't overwrite non-scratch registers, and (2)
    //    emitting the branch, which uses the ref but doesn't clobber it. So the
    //    ref will be live in the same register if we didn't take the branch.
    //
    //  - For a br_on_non_null, we discard the reference when we don't take the
    //    branch. Because the ref is on the expression stack in this case when we
    //    emit the branch, we don't want to eagerly consume() it - it's not used
    //    until it's passed as a parameter to the branch target. So, we don't
    //    consume the value, and rely on block parameter passing logic to ensure
    //    it's left in the right place.
    //
    // Between these cases, we ensure that the reference value is live in
    // referenceLocation by the time we reach its use.

    Location referenceLocation;
    if (!reference.isConst())
        referenceLocation = loadIfNecessary(reference);
    if (!shouldNegate)
        consume(reference);

    LOG_INSTRUCTION(shouldNegate ? "BrOnNonNull" : "BrOnNull", reference);

    if (reference.isConst()) {
        // If we didn't exit early, the branch must be always-taken.
        currentControlData().flushAndSingleExit(*this, data, returnValues, false, false);
        data.addBranch(m_jit.jump());
    } else {
        ASSERT(referenceLocation.isGPR());
        ASSERT(JSValue::encode(jsNull()) >= 0 && JSValue::encode(jsNull()) <= INT32_MAX);
        currentControlData().flushAtBlockBoundary(*this, 0, returnValues, false);
        Jump ifNotTaken = m_jit.branch64(shouldNegate ? CCallHelpers::Equal : CCallHelpers::NotEqual, referenceLocation.asGPR(), TrustedImm32(static_cast<int32_t>(JSValue::encode(jsNull()))));
        currentControlData().addExit(*this, data.targetLocations(), returnValues);
        data.addBranch(m_jit.jump());
        ifNotTaken.link(&m_jit);
        currentControlData().finalizeBlock(*this, data.targetLocations().size(), returnValues, true);
    }

    if (!shouldNegate) {
        result = topValue(reference.type());
        Location resultLocation = allocate(result);
        if (reference.isConst())
            emitMoveConst(reference, resultLocation);
        else
            emitMove(reference.type(), referenceLocation, resultLocation);
    }

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addBranchCast(ControlData& data, ExpressionType reference, Stack& returnValues, bool allowNull, int32_t heapType, bool shouldNegate)
{
    Value condition;
    if (reference.isConst()) {
        JSValue refValue = JSValue::decode(reference.asRef());
        ASSERT(refValue.isNull() || refValue.isNumber());
        if (refValue.isNull())
            condition = Value::fromI32(static_cast<uint32_t>(shouldNegate ? !allowNull : allowNull));
        else {
            bool matches = isSubtype(Type { TypeKind::Ref, static_cast<TypeIndex>(TypeKind::I31ref) }, Type { TypeKind::Ref, static_cast<TypeIndex>(heapType) });
            condition = Value::fromI32(shouldNegate ? !matches : matches);
        }
    } else {
        // Use an indirection for the reference to avoid it getting consumed here.
        Value tempReference = Value::pinned(TypeKind::Ref, Location::fromGPR(wasmScratchGPR));
        emitMove(reference, locationOf(tempReference));

        Vector<Value, 8> arguments = {
            instanceValue(),
            tempReference,
            Value::fromI32(allowNull),
            Value::fromI32(heapType),
            Value::fromI32(shouldNegate),
        };
        condition = topValue(TypeKind::I32);
        emitCCall(operationWasmRefTest, arguments, condition);
    }

    WASM_FAIL_IF_HELPER_FAILS(addBranch(data, condition, returnValues));

    LOG_INSTRUCTION("BrOnCast/CastFail", reference);

    return { };
}

int BBQJIT::alignedFrameSize(int frameSize) const
{
    return WTF::roundUpToMultipleOf<stackAlignmentBytes()>(frameSize);
}

void BBQJIT::restoreWebAssemblyGlobalState()
{
    restoreWebAssemblyContextInstance();
    // FIXME: We should just store these registers on stack and load them.
    if (!!m_info.memory)
        loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
}

void BBQJIT::restoreWebAssemblyGlobalStateAfterWasmCall()
{
    if (!!m_info.memory && (m_mode == MemoryMode::Signaling || m_info.memory.isShared())) {
        // If memory is signaling or shared, then memoryBase and memorySize will not change. This means that only thing we should check here is GPRInfo::wasmContextInstancePointer is the same or not.
        // Let's consider the case, this was calling a JS function. So it can grow / modify memory whatever. But memoryBase and memorySize are kept the same in this case.
        m_jit.loadPtr(Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * sizeof(Register)), wasmScratchGPR);
        Jump isSameInstanceAfter = m_jit.branchPtr(RelationalCondition::Equal, wasmScratchGPR, GPRInfo::wasmContextInstancePointer);
        m_jit.move(wasmScratchGPR, GPRInfo::wasmContextInstancePointer);
        m_jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);
        m_jit.cageConditionally(Gigacage::Primitive, wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister, wasmScratchGPR);
        isSameInstanceAfter.link(&m_jit);
    } else
        restoreWebAssemblyGlobalState();
}

void BBQJIT::emitStoreConst(Value constant, Location loc)
{
    LOG_INSTRUCTION("Store", constant, RESULT(loc));
    // Doesn't have to be a real Type we just need the TypeKind for the StorageType.
    emitStoreConst(StorageType(Type { constant.type(), 0 }), constant, loc.asAddress());
}

void BBQJIT::emitStoreConst(StorageType type, Value constant, BaseIndex loc)
{
    ASSERT(constant.isConst());

    switch (type.elementSize()) {
    case 1:
        m_jit.store8(TrustedImm32(constant.asI32()), loc);
        break;
    case 2:
        m_jit.store16(TrustedImm32(constant.asI32()), loc);
        break;
    case 4:
        m_jit.store32(TrustedImm32(constant.asI32()), loc);
        break;
    case 8:
        m_jit.store64(TrustedImm64(constant.asI64()), loc);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
        break;
    }
}

void BBQJIT::emitStoreConst(StorageType type, Value constant, Address loc)
{
    ASSERT(constant.isConst());

    switch (type.elementSize()) {
    case 1:
        m_jit.store8(TrustedImm32(constant.asI32()), loc);
        break;
    case 2:
        m_jit.store16(TrustedImm32(constant.asI32()), loc);
        break;
    case 4:
        m_jit.store32(TrustedImm32(constant.asI32()), loc);
        break;
    case 8:
        m_jit.store64(TrustedImm64(constant.asI64()), loc);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
        break;
    }
}


void BBQJIT::emitStore(StorageType type, Location src, BaseIndex dst)
{
    ASSERT_WITH_MESSAGE(src.isRegister(), "Memory source locations not supported. Use emitMove instead");

    switch (type.elementSize()) {
    case 1:
        m_jit.store8(src.asGPR(), dst);
        break;
    case 2:
        m_jit.store16(src.asGPR(), dst);
        break;
    case 4:
        m_jit.store32FromReg(src.asReg(), dst);
        break;
    case 8:
        m_jit.store64FromReg(src.asReg(), dst);
        break;
    case 16:
        m_jit.storeVector(src.asFPR(), dst);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant width.");
        break;
    }
}

void BBQJIT::emitStore(StorageType type, Location src, Address dst)
{
    ASSERT_WITH_MESSAGE(src.isRegister(), "Memory source locations not supported. Use emitMove instead");

    switch (type.elementSize()) {
    case 1:
        m_jit.store8(src.asGPR(), dst);
        break;
    case 2:
        m_jit.store16(src.asGPR(), dst);
        break;
    case 4:
        m_jit.store32FromReg(src.asReg(), dst);
        break;
    case 8:
        m_jit.store64FromReg(src.asReg(), dst);
        break;
    case 16:
        m_jit.storeVector(src.asFPR(), dst);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant width.");
        break;
    }
}

void BBQJIT::emitStore(TypeKind type, Location src, Location dst)
{
    ASSERT(dst.isMemory());
    ASSERT(src.isRegister());

    // Doesn't have to be a real Type we just need the TypeKind for the StorageType.
    emitStore(StorageType(Type { type, 0 }), src, dst.asAddress());
}

void BBQJIT::emitMoveConst(Value constant, Location loc)
{
    ASSERT(constant.isConst());

    if (loc.isMemory())
        return emitStoreConst(constant, loc);

    ASSERT(loc.isRegister());
    ASSERT(loc.isFPR() == constant.isFloat());

    if (!isScratch(loc))
        LOG_INSTRUCTION("Move", constant, RESULT(loc));

    switch (constant.type()) {
    case TypeKind::I32:
        m_jit.move(Imm32(constant.asI32()), loc.asGPR());
        break;
    case TypeKind::I64:
        m_jit.move(Imm64(constant.asI64()), loc.asGPR());
        break;
    case TypeKind::Ref:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::RefNull:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
        m_jit.move(TrustedImm64(constant.asRef()), loc.asGPR());
        break;
    case TypeKind::F32:
        m_jit.move32ToFloat(Imm32(constant.asI32()), loc.asFPR());
        break;
    case TypeKind::F64:
        m_jit.move64ToDouble(Imm64(constant.asI64()), loc.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
        break;
    }
}

void BBQJIT::emitMoveMemory(StorageType type, Location src, Address dst)
{
    ASSERT_WITH_MESSAGE(src.isMemory(), "Register source locations not supported. Use emitMove instead");

    switch (type.elementSize()) {
    case 1:
        m_jit.transfer8(src.asAddress(), dst);
        break;
    case 2:
        m_jit.transfer16(src.asAddress(), dst);
        break;
    case 4:
        m_jit.transfer32(src.asAddress(), dst);
        break;
    case 8:
        m_jit.transfer64(src.asAddress(), dst);
        break;
    case 16:
        m_jit.transferVector(src.asAddress(), dst);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Invalid StorageType width.");
        break;
    }
}

void BBQJIT::emitMoveMemory(StorageType type, Location src, BaseIndex dst)
{
    ASSERT_WITH_MESSAGE(src.isMemory(), "Register source locations not supported. Use emitMove instead");

    switch (type.elementSize()) {
    case 1:
        m_jit.transfer8(src.asAddress(), dst);
        break;
    case 2:
        m_jit.transfer16(src.asAddress(), dst);
        break;
    case 4:
        m_jit.transfer32(src.asAddress(), dst);
        break;
    case 8:
        m_jit.transfer64(src.asAddress(), dst);
        break;
    case 16:
        m_jit.transferVector(src.asAddress(), dst);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Invalid StorageType width.");
        break;
    }
}

void BBQJIT::emitMoveMemory(TypeKind type, Location src, Location dst)
{
    ASSERT(dst.isMemory());
    ASSERT(src.isMemory());

    if (src == dst)
        return;

    switch (type) {
    case TypeKind::I32:
    case TypeKind::F32:
        m_jit.transfer32(src.asAddress(), dst.asAddress());
        break;
    case TypeKind::I64:
    case TypeKind::F64:
        m_jit.transfer64(src.asAddress(), dst.asAddress());
        break;
    case TypeKind::I31ref:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Funcref:
    case TypeKind::Structref:
    case TypeKind::Arrayref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
        m_jit.transfer64(src.asAddress(), dst.asAddress());
        break;
    case TypeKind::V128:
        m_jit.transferVector(src.asAddress(), dst.asAddress());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind move.");
    }
}

void BBQJIT::emitMoveRegister(TypeKind type, Location src, Location dst)
{
    ASSERT(dst.isRegister());
    ASSERT(src.isRegister());

    if (src == dst)
        return;

    switch (type) {
    case TypeKind::I32:
    case TypeKind::I31ref:
    case TypeKind::I64:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
        m_jit.move(src.asGPR(), dst.asGPR());
        break;
    case TypeKind::F32:
    case TypeKind::F64:
        m_jit.moveDouble(src.asFPR(), dst.asFPR());
        break;
    case TypeKind::V128:
        m_jit.moveVector(src.asFPR(), dst.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind move.");
    }
}

void BBQJIT::emitLoad(TypeKind type, Location src, Location dst)
{
    ASSERT(dst.isRegister());
    ASSERT(src.isMemory());

    switch (type) {
    case TypeKind::I32:
        m_jit.load32(src.asAddress(), dst.asGPR());
        break;
    case TypeKind::I64:
        m_jit.load64(src.asAddress(), dst.asGPR());
        break;
    case TypeKind::F32:
        m_jit.loadFloat(src.asAddress(), dst.asFPR());
        break;
    case TypeKind::F64:
        m_jit.loadDouble(src.asAddress(), dst.asFPR());
        break;
    case TypeKind::I31ref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
        m_jit.load64(src.asAddress(), dst.asGPR());
        break;
    case TypeKind::V128:
        m_jit.loadVector(src.asAddress(), dst.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind load.");
    }
}

Location BBQJIT::materializeToGPR(Value value, std::optional<ScratchScope<1, 0>>& sizeScratch)
{
    if (value.isPinned())
        return value.asPinned();
    if (value.isConst()) {
        sizeScratch.emplace(*this);
        Location result = Location::fromGPR(sizeScratch->gpr(0));

        switch (value.type()) {
        case TypeKind::I32:
            m_jit.move(TrustedImm32(value.asI32()), result.asGPR());
            return result;
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
        case TypeKind::I64:
            m_jit.move(TrustedImm64(value.asI64()), result.asGPR());
            return result;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        return result;
    }

    return loadIfNecessary(value);
}

void BBQJIT::emitMove(StorageType type, Value src, BaseIndex dst)
{
    if (src.isConst()) {
        emitStoreConst(type, src, dst);
        return;
    }

    Location srcLocation = locationOf(src);
    if (srcLocation.isMemory())
        emitMoveMemory(type, srcLocation, dst);
    else
        emitStore(type, srcLocation, dst);
}

void BBQJIT::emitMove(StorageType type, Value src, Address dst)
{
    if (src.isConst()) {
        emitStoreConst(type, src, dst);
        return;
    }

    Location srcLocation = locationOf(src);
    if (srcLocation.isMemory())
        emitMoveMemory(type, srcLocation, dst);
    else
        emitStore(type, srcLocation, dst);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCallRef(unsigned callProfileIndex, const TypeDefinition& originalSignature, ArgumentList& args, ResultList& results, CallType callType)
{
    emitIncrementCallProfileCount(callProfileIndex);
    Value callee = args.takeLast();
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    CallInformation callInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    GPRReg importableFunction = GPRInfo::nonPreservedNonArgumentGPR1;
    {
        clobber(importableFunction);
        ScratchScope<0, 0> importableFunctionScope(*this, importableFunction);
        static_assert(GPRInfo::nonPreservedNonArgumentGPR0 == wasmScratchGPR);
        {
            ScratchScope<1, 0> otherScratch(*this);

            Location calleeLocation;
            if (callee.isConst()) {
                ASSERT(callee.asI64() == JSValue::encode(jsNull()));
                // This is going to throw anyway. It's suboptimial but probably won't happen in practice anyway.
                emitMoveConst(callee, calleeLocation = Location::fromGPR(otherScratch.gpr(0)));
            } else
                calleeLocation = loadIfNecessary(callee);
            consume(callee);
            emitThrowOnNullReferenceBeforeAccess(calleeLocation, WebAssemblyFunctionBase::offsetOfTargetInstance());

            GPRReg calleePtr = calleeLocation.asGPR();
            m_jit.addPtr(TrustedImm32(WebAssemblyFunctionBase::offsetOfImportableFunction()), calleePtr, importableFunction);
        }
    }

    if (callType == CallType::Call)
        emitIndirectCall("CallRef", callProfileIndex, callee, importableFunction, signature, args, results);
    else
        emitIndirectTailCall("ReturnCallRef", callee, importableFunction, signature, args);
    return { };
}

} } } // namespace JSC::Wasm::BBQJITImpl

#endif // USE(JSVALUE64)
#endif // ENABLE(WEBASSEMBLY_BBQJIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
