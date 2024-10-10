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
#include "WasmBBQJIT32_64.h"

#include "WasmBBQJIT.h"

#if ENABLE(WEBASSEMBLY_BBQJIT)
#if USE(JSVALUE32_64)

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
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/SmallSet.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm { namespace BBQJITImpl {

Location Location::fromArgumentLocation(ArgumentLocation argLocation, TypeKind type)
{
    switch (argLocation.location.kind()) {
    case ValueLocation::Kind::GPRRegister:
        if (typeNeedsGPR2(type))
            return Location::fromGPR2(argLocation.location.jsr().tagGPR(), argLocation.location.jsr().payloadGPR());
        return Location::fromGPR(argLocation.location.jsr().payloadGPR());
    case ValueLocation::Kind::FPRRegister:
        return Location::fromFPR(argLocation.location.fpr());
    case ValueLocation::Kind::StackArgument:
        return Location::fromStackArgument(argLocation.location.offsetFromSP());
    case ValueLocation::Kind::Stack:
        return Location::fromStack(argLocation.location.offsetFromFP());
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Location Location::fromGPR2(GPRReg hi, GPRReg lo)
{
    ASSERT(hi != lo);

    // we canonicalize hi and lo to avoid a dreadful situation of having two
    // GPR2 locations (x, y) and (y, x) which we can't easliy generate a move between

    Location loc;
    loc.m_kind = Gpr2;
    if (hi > lo) {
        loc.m_gprhi = hi;
        loc.m_gprlo = lo;
    } else {
        loc.m_gprhi = lo;
        loc.m_gprlo = hi;
    }
    return loc;
}

bool Location::isRegister() const
{
    return isGPR() || isGPR2() || isFPR();
}

bool BBQJIT::typeNeedsGPR2(TypeKind type)
{
    return toValueKind(type) == TypeKind::I64;
}

uint32_t BBQJIT::sizeOfType(TypeKind type)
{
    switch (type) {
    case TypeKind::I32:
    case TypeKind::F32:
        // NB: size in memory (on JSVALUE32_64 we represent even four-byte values as EncodedJSValue)
        return sizeof(EncodedJSValue);
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
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Array:
    case TypeKind::Arrayref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        return sizeof(EncodedJSValue);
    case TypeKind::Void:
        return 0;
    }
    return 0;
}

int32_t Value::asI64hi() const
{
    ASSERT(m_kind == Const);
    return m_i32_pair.hi;
}

int32_t Value::asI64lo() const
{
    ASSERT(m_kind == Const);
    return m_i32_pair.lo;
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
        if (typeNeedsGPR2(type)) {
            auto next = remainingGPRs.begin();
            if (next != remainingGPRs.end()) {
                auto lo = (*next);
                if (++next != remainingGPRs.end()) {
                    auto hi = (*next);
                    remainingGPRs.remove(lo);
                    remainingGPRs.remove(hi);
                    return Location::fromGPR2(hi.gpr(), lo.gpr());
                }
            }
            return generator.canonicalSlot(Value::fromTemp(type, this->enclosedHeight() + i));
        }
        if (remainingGPRs.isEmpty())
            return generator.canonicalSlot(Value::fromTemp(type, this->enclosedHeight() + i));
        auto reg = *remainingGPRs.begin();
        remainingGPRs.remove(reg);
        return Location::fromGPR(reg.gpr());
    }
}

Value BBQJIT::instanceValue()
{
    return Value::pinned(TypeKind::I32, Location::fromGPR(GPRInfo::wasmContextInstancePointer));
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
    Location resultLocation = allocate(result);

    LOG_INSTRUCTION("TableGet", tableIndex, index, RESULT(result));

    m_jit.or32(resultLocation.asGPRhi(), resultLocation.asGPRlo(), wasmScratchGPR);
    throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, wasmScratchGPR));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::getGlobal(uint32_t index, Value& result)
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    int32_t offset = JSWebAssemblyInstance::offsetOfGlobalPtr(m_info.importFunctionCount(), m_info.tableCount(), index);
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
        case TypeKind::I31ref:
            m_jit.load32(Address(wasmScratchGPR), resultLocation.asGPRlo());
            m_jit.move(TrustedImm32(JSValue::Int32Tag), resultLocation.asGPRhi());
            break;
        case TypeKind::I64:
            m_jit.loadPair32(Address(wasmScratchGPR), resultLocation.asGPRlo(), resultLocation.asGPRhi());
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
        case TypeKind::Exn:
        case TypeKind::Externref:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Nullref:
        case TypeKind::Nullfuncref:
        case TypeKind::Nullexternref:
            m_jit.loadPair32(Address(wasmScratchGPR), resultLocation.asGPRlo(), resultLocation.asGPRhi());
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

    int32_t offset = JSWebAssemblyInstance::offsetOfGlobalPtr(m_info.importFunctionCount(), m_info.tableCount(), index);
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
            if (typeNeedsGPR2(value.type())) {
                ScratchScope<2, 0> scratches(*this);
                valueLocation = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
            } else {
                ScratchScope<1, 0> scratches(*this);
                valueLocation = Location::fromGPR(scratches.gpr(0));
            }
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
            m_jit.storePair32(valueLocation.asGPRlo(), valueLocation.asGPRhi(), Address(wasmScratchGPR));
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
        case TypeKind::Exn:
        case TypeKind::Externref:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Nullref:
        case TypeKind::Nullfuncref:
        case TypeKind::Nullexternref:
            m_jit.storePair32(valueLocation.asGPRlo(), valueLocation.asGPRhi(), Address(wasmScratchGPR));
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
    if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfLoadOp(loadOp)))) {
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
                m_jit.load8SignedExtendTo32(location, resultLocation.asGPRlo());
                m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
                break;
            case LoadOpType::I32Load8U:
                m_jit.load8(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load8U:
                m_jit.load8(location, resultLocation.asGPRlo());
                m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
                break;
            case LoadOpType::I32Load16S:
                m_jit.load16SignedExtendTo32(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load16S:
                m_jit.load16SignedExtendTo32(location, resultLocation.asGPRlo());
                m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
                break;
            case LoadOpType::I32Load16U:
                m_jit.load16(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load16U:
                m_jit.load16(location, resultLocation.asGPRlo());
                m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
                break;
            case LoadOpType::I32Load:
                m_jit.load32(location, resultLocation.asGPR());
                break;
            case LoadOpType::I64Load32U:
                m_jit.load32(location, resultLocation.asGPRlo());
                m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
                break;
            case LoadOpType::I64Load32S:
                m_jit.load32(location, resultLocation.asGPRlo());
                m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
                break;
            case LoadOpType::I64Load:
                m_jit.loadPair32(location, resultLocation.asGPRlo(), resultLocation.asGPRhi());
                break;
            case LoadOpType::F32Load:
                m_jit.load32(location, wasmScratchGPR);
                m_jit.move32ToFloat(wasmScratchGPR, resultLocation.asFPR());
                break;
            case LoadOpType::F64Load:
                m_jit.loadPair32(location, wasmScratchGPR, wasmScratchGPR2);
                m_jit.move64ToDouble(wasmScratchGPR2, wasmScratchGPR, resultLocation.asFPR());
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
    if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfStoreOp(storeOp)))) {
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
            } else if (value.isConst() && typeNeedsGPR2(value.type())) {
                ScratchScope<2, 0> scratches(*this);
                valueLocation = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
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
                m_jit.store8(valueLocation.asGPRlo(), location);
                return;
            case StoreOpType::I32Store8:
                m_jit.store8(valueLocation.asGPR(), location);
                return;
            case StoreOpType::I64Store16:
                m_jit.store16(valueLocation.asGPRlo(), location);
                return;
            case StoreOpType::I32Store16:
                m_jit.store16(valueLocation.asGPR(), location);
                return;
            case StoreOpType::I64Store32:
                m_jit.store32(valueLocation.asGPRlo(), location);
                return;
            case StoreOpType::I32Store:
                m_jit.store32(valueLocation.asGPR(), location);
                return;
            case StoreOpType::I64Store:
                m_jit.storePair32(valueLocation.asGPRlo(), valueLocation.asGPRhi(), location);
                return;
            case StoreOpType::F32Store: {
                ScratchScope<1, 0> scratches(*this);
                m_jit.moveFloatTo32(valueLocation.asFPR(), scratches.gpr(0));
                m_jit.store32(scratches.gpr(0), location);
                return;
            }
            case StoreOpType::F64Store: {
                ScratchScope<1, 0> scratches(*this);
                m_jit.moveDoubleTo64(valueLocation.asFPR(), wasmScratchGPR2, scratches.gpr(0));
                m_jit.storePair32(scratches.gpr(0), wasmScratchGPR2, location);
                return;
            }
            }
        });
    }

    LOG_INSTRUCTION(STORE_OP_NAMES[(unsigned)storeOp - (unsigned)I32Store], pointer, uoffset, value, valueLocation);

    return { };
}

void BBQJIT::emitSanitizeAtomicResult(ExtAtomicOpType op, TypeKind resultType, Location source, Location dest)
{
    switch (resultType) {
    case TypeKind::I64: {
        switch (accessWidth(op)) {
        case Width8:
            m_jit.zeroExtend8To32(source.asGPR(), dest.asGPRlo());
            m_jit.xor32(dest.asGPRhi(), dest.asGPRhi());
            return;
        case Width16:
            m_jit.zeroExtend16To32(source.asGPR(), dest.asGPRlo());
            m_jit.xor32(dest.asGPRhi(), dest.asGPRhi());
            return;
        case Width32:
            m_jit.zeroExtend32ToWord(source.asGPR(), dest.asGPRlo());
            m_jit.xor32(dest.asGPRhi(), dest.asGPRhi());
            return;
        case Width64:
            ASSERT(dest.asGPRlo() != source.asGPRhi());
            m_jit.move(source.asGPRlo(), dest.asGPRlo());
            m_jit.move(source.asGPRhi(), dest.asGPRhi());
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
            m_jit.zeroExtend8To32(source.asGPR(), dest.asGPR());
            return;
        case Width16:
            m_jit.zeroExtend16To32(source.asGPR(), dest.asGPR());
            return;
        case Width32:
            m_jit.move(source.asGPR(), dest.asGPR());
            return;
        case Width64:
            m_jit.move(source.asGPRlo(), dest.asGPR());
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

void BBQJIT::emitSanitizeAtomicOperand(ExtAtomicOpType op, TypeKind operandType, Location source, Location dest)
{
    switch (operandType) {
    case TypeKind::I64:
        switch (accessWidth(op)) {
        case Width8:
            m_jit.zeroExtend8To32(source.asGPRlo(), dest.asGPR());
            return;
        case Width16:
            m_jit.zeroExtend16To32(source.asGPRlo(), dest.asGPR());
            return;
        case Width32:
            m_jit.move(source.asGPRlo(), dest.asGPR());
            return;
        case Width64:
            ASSERT(dest.asGPRlo() != source.asGPRhi());
            m_jit.move(source.asGPRlo(), dest.asGPRlo());
            m_jit.move(source.asGPRhi(), dest.asGPRhi());
            return;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        return;
    case TypeKind::I32:
        switch (accessWidth(op)) {
        case Width8:
            m_jit.zeroExtend8To32(source.asGPR(), dest.asGPR());
            return;
        case Width16:
            m_jit.zeroExtend16To32(source.asGPR(), dest.asGPR());
            return;
        case Width32:
            m_jit.move(source.asGPR(), dest.asGPR());
            return;
        case Width64:
            m_jit.move(source.asGPR(), dest.asGPRlo());
            m_jit.xor32(dest.asGPRhi(), dest.asGPRhi());
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

Location BBQJIT::emitMaterializeAtomicOperand(Value value)
{
    Location valueLocation;

    if (value.isConst()) {
        switch (value.type()) {
        case TypeKind::I64: {
            ScratchScope<2, 0> scratches(*this);
            valueLocation = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
            break;
        }
        case TypeKind::I32: {
            ScratchScope<1, 0> scratches(*this);
            valueLocation = Location::fromGPR(scratches.gpr(0));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        emitMoveConst(value, valueLocation);
    } else
        valueLocation = loadIfNecessary(value);
    ASSERT(valueLocation.isRegister());

    consume(value);

    return valueLocation;
}

template<typename Functor>
void BBQJIT::emitAtomicOpGeneric(ExtAtomicOpType op, Address address, Location old, Location cur, const Functor& functor)
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
        m_jit.loadLink8(address, old.asGPR());
        break;
    case Width16:
        m_jit.loadLink16(address, old.asGPR());
        break;
    case Width32:
        m_jit.loadLink32(address, old.asGPR());
        break;
    case Width64:
        m_jit.loadLinkPair32(address, old.asGPRlo(), old.asGPRhi());
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    // Operation
    functor(old, cur);

    switch (accessWidth) {
    case Width8:
        m_jit.storeCond8(cur.asGPR(), address, wasmScratchGPR2);
        break;
    case Width16:
        m_jit.storeCond16(cur.asGPR(), address, wasmScratchGPR2);
        break;
    case Width32:
        m_jit.storeCond32(cur.asGPR(), address, wasmScratchGPR2);
        break;
    case Width64:
        m_jit.storeCondPair32(cur.asGPRlo(), cur.asGPRhi(), address, wasmScratchGPR2);
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_jit.branchTest32(ResultCondition::NonZero, wasmScratchGPR2).linkTo(reloopLabel, &m_jit);
}

Value WARN_UNUSED_RETURN BBQJIT::emitAtomicLoadOp(ExtAtomicOpType loadOp, Type valueType, Location pointer, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(loadOp) != Width8)
        throwExceptionIf(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest32(ResultCondition::NonZero, pointer.asGPR(), TrustedImm32(sizeOfAtomicOpMemoryAccess(loadOp) - 1)));

    Value result = topValue(valueType.kind);
    Location resultLocation;
    Location old, cur;

    switch (valueType.kind) {
    case TypeKind::I64:
        switch (canonicalWidth(accessWidth(loadOp))) {
        case Width32: {
            resultLocation = allocate(result);
            cur = Location::fromGPR(resultLocation.asGPRlo());
            ScratchScope<1, 0> scratches(*this);
            old = Location::fromGPR(scratches.gpr(0));
            break;
        }
        case Width64: {
            resultLocation = allocate(result);
            cur = resultLocation;
            ScratchScope<2, 0> scratches(*this);
            old = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    case TypeKind::I32:
        switch (canonicalWidth(accessWidth(loadOp))) {
        case Width32: {
            resultLocation = allocate(result);
            cur = resultLocation;
            ScratchScope<1, 0> scratches(*this);
            old = Location::fromGPR(scratches.gpr(0));
            break;
        }
        case Width64: {
            ScratchScope<4, 0> scratches(*this);
            cur = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
            resultLocation = Location::fromGPR(scratches.gpr(0));
            ASSERT(resultLocation == allocateWithHint(result, resultLocation));
            old = Location::fromGPR2(scratches.gpr(3), scratches.gpr(2));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    emitAtomicOpGeneric(loadOp, address, old, cur, [&](Location old, Location cur) {
        if (old.isGPR2()) {
            ASSERT(cur.asGPRlo() != old.asGPRhi());
            m_jit.move(old.asGPRlo(), cur.asGPRlo());
            m_jit.move(old.asGPRhi(), cur.asGPRhi());
        } else
            m_jit.move(old.asGPR(), cur.asGPR());
    });
    emitSanitizeAtomicResult(loadOp, valueType.kind, cur, resultLocation);

    return result;
}

void BBQJIT::emitAtomicStoreOp(ExtAtomicOpType storeOp, Type, Location pointer, Value value, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(storeOp) != Width8)
        throwExceptionIf(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest32(ResultCondition::NonZero, pointer.asGPR(), TrustedImm32(sizeOfAtomicOpMemoryAccess(storeOp) - 1)));

    Location source, old, cur;
    switch (canonicalWidth(accessWidth(storeOp))) {
    case Width32: {
        Location valueLocation = emitMaterializeAtomicOperand(value);
        ScratchScope<1, 0> scratchSource(*this);
        source = Location::fromGPR(scratchSource.gpr(0));
        emitSanitizeAtomicOperand(storeOp, value.type(), valueLocation, source);
        ScratchScope<2, 0> scratchTmp(*this);
        old = Location::fromGPR(scratchTmp.gpr(0));
        cur = Location::fromGPR(scratchTmp.gpr(1));
        break;
    }
    case Width64: {
        Location valueLocation = emitMaterializeAtomicOperand(value);
        ScratchScope<2, 0> scratchSource(*this);
        source = Location::fromGPR2(scratchSource.gpr(1), scratchSource.gpr(0));
        emitSanitizeAtomicOperand(storeOp, value.type(), valueLocation, source);
        ScratchScope<4, 0> scratchTmp(*this);
        old = Location::fromGPR2(scratchTmp.gpr(1), scratchTmp.gpr(0));
        cur = Location::fromGPR2(scratchTmp.gpr(3), scratchTmp.gpr(2));
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    emitAtomicOpGeneric(storeOp, address, old, cur, [&](Location, Location cur) {
        if (source.isGPR2()) {
            ASSERT(cur.asGPRlo() != source.asGPRhi());
            m_jit.move(source.asGPRlo(), cur.asGPRlo());
            m_jit.move(source.asGPRhi(), cur.asGPRhi());
        } else
            m_jit.move(source.asGPR(), cur.asGPR());
    });
}

Value BBQJIT::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, Location pointer, Value value, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(op) != Width8)
        throwExceptionIf(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest32(ResultCondition::NonZero, pointer.asGPR(), TrustedImm32(sizeOfAtomicOpMemoryAccess(op) - 1)));

    Value result = topValue(valueType.kind);
    Location resultLocation;
    Location operand, old, cur;

    switch (valueType.kind) {
    case TypeKind::I64:
        switch (canonicalWidth(accessWidth(op))) {
        case Width32: {
            resultLocation = allocate(result);
            old = Location::fromGPR(resultLocation.asGPRlo());
            Location valueLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<1, 0> scratchOperand(*this, old);
            operand = Location::fromGPR(scratchOperand.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), valueLocation, operand);
            ScratchScope<1, 0> scratchTmp(*this, old);
            cur = Location::fromGPR(scratchTmp.gpr(0));
            break;
        }
        case Width64: {
            resultLocation = allocate(result);
            old = resultLocation;
            Location valueLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<2, 0> scratchOperand(*this, old);
            operand = Location::fromGPR2(scratchOperand.gpr(1), scratchOperand.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), valueLocation, operand);
            ScratchScope<2, 0> scratchTmp(*this, old);
            cur = Location::fromGPR2(scratchTmp.gpr(1), scratchTmp.gpr(0));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    case TypeKind::I32:
        switch (canonicalWidth(accessWidth(op))) {
        case Width32: {
            resultLocation = allocate(result);
            old = resultLocation;
            Location valueLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<1, 0> scratchOperand(*this, old);
            operand = Location::fromGPR(scratchOperand.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), valueLocation, operand);
            ScratchScope<1, 0> scratchTmp(*this, old);
            cur = Location::fromGPR(scratchTmp.gpr(0));
            break;
        }
        case Width64: {
            ScratchScope<2, 0> scratchResult(*this);
            old = Location::fromGPR2(scratchResult.gpr(1), scratchResult.gpr(0));
            resultLocation = Location::fromGPR(scratchResult.gpr(0));
            ASSERT(resultLocation == allocateWithHint(result, resultLocation));
            Location valueLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<2, 0> scratchOperand(*this);
            operand = Location::fromGPR2(scratchOperand.gpr(1), scratchOperand.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), valueLocation, operand);
            ScratchScope<2, 0> scratchTmp(*this);
            cur = Location::fromGPR2(scratchTmp.gpr(1), scratchTmp.gpr(0));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    emitAtomicOpGeneric(op, address, old, cur, [&](Location old, Location cur) {
        switch (op) {
        case ExtAtomicOpType::I32AtomicRmw16AddU:
        case ExtAtomicOpType::I32AtomicRmw8AddU:
        case ExtAtomicOpType::I32AtomicRmwAdd:
        case ExtAtomicOpType::I64AtomicRmw8AddU:
        case ExtAtomicOpType::I64AtomicRmw16AddU:
        case ExtAtomicOpType::I64AtomicRmw32AddU:
            m_jit.add32(old.asGPR(), operand.asGPR(), cur.asGPR());
            break;
        case ExtAtomicOpType::I64AtomicRmwAdd:
            m_jit.add64(old.asGPRhi(), old.asGPRlo(),
                operand.asGPRhi(), operand.asGPRlo(),
                cur.asGPRhi(), cur.asGPRlo());
            break;
        case ExtAtomicOpType::I32AtomicRmw8SubU:
        case ExtAtomicOpType::I32AtomicRmw16SubU:
        case ExtAtomicOpType::I32AtomicRmwSub:
        case ExtAtomicOpType::I64AtomicRmw8SubU:
        case ExtAtomicOpType::I64AtomicRmw16SubU:
        case ExtAtomicOpType::I64AtomicRmw32SubU:
            m_jit.sub32(old.asGPR(), operand.asGPR(), cur.asGPR());
            break;
        case ExtAtomicOpType::I64AtomicRmwSub:
            m_jit.sub64(old.asGPRhi(), old.asGPRlo(),
                operand.asGPRhi(), operand.asGPRlo(),
                cur.asGPRhi(), cur.asGPRlo());
            break;
        case ExtAtomicOpType::I32AtomicRmw8AndU:
        case ExtAtomicOpType::I32AtomicRmw16AndU:
        case ExtAtomicOpType::I32AtomicRmwAnd:
        case ExtAtomicOpType::I64AtomicRmw8AndU:
        case ExtAtomicOpType::I64AtomicRmw16AndU:
        case ExtAtomicOpType::I64AtomicRmw32AndU:
            m_jit.and32(old.asGPR(), operand.asGPR(), cur.asGPR());
            break;
        case ExtAtomicOpType::I64AtomicRmwAnd:
            m_jit.and32(old.asGPRhi(), operand.asGPRhi(), cur.asGPRhi());
            m_jit.and32(old.asGPRlo(), operand.asGPRlo(), cur.asGPRlo());
            break;
        case ExtAtomicOpType::I32AtomicRmw8OrU:
        case ExtAtomicOpType::I32AtomicRmw16OrU:
        case ExtAtomicOpType::I32AtomicRmwOr:
        case ExtAtomicOpType::I64AtomicRmw8OrU:
        case ExtAtomicOpType::I64AtomicRmw16OrU:
        case ExtAtomicOpType::I64AtomicRmw32OrU:
            m_jit.or32(old.asGPR(), operand.asGPR(), cur.asGPR());
            break;
        case ExtAtomicOpType::I64AtomicRmwOr:
            m_jit.or32(old.asGPRhi(), operand.asGPRhi(), cur.asGPRhi());
            m_jit.or32(old.asGPRlo(), operand.asGPRlo(), cur.asGPRlo());
            break;
        case ExtAtomicOpType::I32AtomicRmw8XorU:
        case ExtAtomicOpType::I32AtomicRmw16XorU:
        case ExtAtomicOpType::I32AtomicRmwXor:
        case ExtAtomicOpType::I64AtomicRmw8XorU:
        case ExtAtomicOpType::I64AtomicRmw16XorU:
        case ExtAtomicOpType::I64AtomicRmw32XorU:
            m_jit.xor32(old.asGPR(), operand.asGPR(), cur.asGPR());
            break;
        case ExtAtomicOpType::I64AtomicRmwXor:
            m_jit.xor32(old.asGPRhi(), operand.asGPRhi(), cur.asGPRhi());
            m_jit.xor32(old.asGPRlo(), operand.asGPRlo(), cur.asGPRlo());
            break;
        case ExtAtomicOpType::I32AtomicRmw8XchgU:
        case ExtAtomicOpType::I32AtomicRmw16XchgU:
        case ExtAtomicOpType::I32AtomicRmwXchg:
        case ExtAtomicOpType::I64AtomicRmw8XchgU:
        case ExtAtomicOpType::I64AtomicRmw16XchgU:
        case ExtAtomicOpType::I64AtomicRmw32XchgU:
            m_jit.move(operand.asGPR(), cur.asGPR());
            break;
        case ExtAtomicOpType::I64AtomicRmwXchg:
            ASSERT(cur.asGPRlo() != operand.asGPRhi());
            m_jit.move(operand.asGPRlo(), cur.asGPRlo());
            m_jit.move(operand.asGPRhi(), cur.asGPRhi());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    });
    emitSanitizeAtomicResult(op, valueType.kind, old, resultLocation);

    return result;
}

Value WARN_UNUSED_RETURN BBQJIT::emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, Location pointer, Value expected, Value value, uint32_t uoffset)
{
    ASSERT(pointer.isGPR());

    // For Atomic access, we need SimpleAddress (uoffset = 0).
    if (uoffset)
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), pointer.asGPR());
    Address address = Address(pointer.asGPR());

    if (accessWidth(op) != Width8)
        throwExceptionIf(ExceptionType::UnalignedMemoryAccess, m_jit.branchTest32(ResultCondition::NonZero, pointer.asGPR(), TrustedImm32(sizeOfAtomicOpMemoryAccess(op) - 1)));

    Value result = topValue(valueType.kind);
    Location resultLocation;
    Location a, b, old, cur;

    switch (valueType.kind) {
    case TypeKind::I64:
        switch (canonicalWidth(accessWidth(op))) {
        case Width32: {
            resultLocation = allocate(result);
            old = Location::fromGPR(resultLocation.asGPRlo());
            Location aLocation = emitMaterializeAtomicOperand(expected);
            ScratchScope<1, 0> scratchA(*this, old);
            a = Location::fromGPR(scratchA.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), aLocation, a);
            Location bLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<1, 0> scratchB(*this, old);
            b = Location::fromGPR(scratchB.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), bLocation, b);
            cur = old;
            break;
        }
        case Width64: {
            resultLocation = allocate(result);
            old = resultLocation;
            Location aLocation = emitMaterializeAtomicOperand(expected);
            ScratchScope<2, 0> scratchA(*this, old);
            a = Location::fromGPR2(scratchA.gpr(1), scratchA.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), aLocation, a);
            Location bLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<2, 0> scratchB(*this, old);
            b = Location::fromGPR2(scratchB.gpr(1), scratchB.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), bLocation, b);
            cur = old;
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    case TypeKind::I32:
        switch (canonicalWidth(accessWidth(op))) {
        case Width32: {
            resultLocation = allocate(result);
            old = resultLocation;
            Location aLocation = emitMaterializeAtomicOperand(expected);
            ScratchScope<1, 0> scratchA(*this, old);
            a = Location::fromGPR(scratchA.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), aLocation, a);
            Location bLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<1, 0> scratchB(*this, old);
            b = Location::fromGPR(scratchB.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), bLocation, b);
            cur = old;
            break;
        }
        case Width64: {
            ScratchScope<2, 0> scratchResult(*this);
            old = Location::fromGPR2(scratchResult.gpr(1), scratchResult.gpr(0));
            resultLocation = Location::fromGPR(scratchResult.gpr(0));
            ASSERT(resultLocation == allocateWithHint(result, resultLocation));
            Location aLocation = emitMaterializeAtomicOperand(expected);
            ScratchScope<2, 0> scratchA(*this);
            a = Location::fromGPR2(scratchA.gpr(1), scratchA.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), aLocation, a);
            Location bLocation = emitMaterializeAtomicOperand(value);
            ScratchScope<2, 0> scratchB(*this);
            b = Location::fromGPR2(scratchB.gpr(1), scratchB.gpr(0));
            emitSanitizeAtomicOperand(op, value.type(), bLocation, b);
            cur = old;
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    JumpList failure;

    emitAtomicOpGeneric(op, address, old, cur, [&](Location old, Location cur) {
        emitSanitizeAtomicResult(op, valueType.kind, old, resultLocation);
        if (old.isGPR2()) {
            failure.append(m_jit.branch32(RelationalCondition::NotEqual, old.asGPRlo(), a.asGPRlo()));
            failure.append(m_jit.branch32(RelationalCondition::NotEqual, old.asGPRhi(), a.asGPRhi()));
            m_jit.move(b.asGPRlo(), cur.asGPRlo());
            m_jit.move(b.asGPRhi(), cur.asGPRhi());
        } else {
            failure.append(m_jit.branch32(RelationalCondition::NotEqual, old.asGPR(), a.asGPR()));
            m_jit.move(b.asGPR(), cur.asGPR());
        }
    });
    emitSanitizeAtomicResult(op, valueType.kind, a, resultLocation);
    failure.link(&m_jit);

    return result;
}

void BBQJIT::truncInBounds(TruncationKind truncationKind, Location operandLocation, Value& result, Location resultLocation)
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
    case TruncationKind::I64TruncF32S: {
        auto operand = Value::pinned(TypeKind::F32, operandLocation);
        consume(result);
        emitCCall(Math::i64_trunc_s_f32, ArgumentList { operand }, result);
        break;
    }
    case TruncationKind::I64TruncF64S: {
        auto operand = Value::pinned(TypeKind::F64, operandLocation);
        consume(result);
        emitCCall(Math::i64_trunc_s_f64, ArgumentList { operand }, result);
        break;
    }
    case TruncationKind::I64TruncF32U: {
        auto operand = Value::pinned(TypeKind::F32, operandLocation);
        consume(result);
        emitCCall(Math::i64_trunc_u_f32, ArgumentList { operand }, result);
        break;
    }
    case TruncationKind::I64TruncF64U: {
        auto operand = Value::pinned(TypeKind::F64, operandLocation);
        consume(result);
        emitCCall(Math::i64_trunc_u_f64, ArgumentList { operand }, result);
        break;
    }
    }
}

PartialResult WARN_UNUSED_RETURN BBQJIT::truncTrapping(OpType truncationOp, Value operand, Value& result, Type returnType, Type operandType)
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

    LOG_INSTRUCTION("TruncSaturated", operand, operandLocation, RESULT(result));

    DoubleCondition minCondition = range.closedLowerEndpoint ? DoubleCondition::DoubleLessThanOrUnordered : DoubleCondition::DoubleLessThanOrEqualOrUnordered;
    Jump belowMin = operandType == Types::F32
        ? m_jit.branchFloat(minCondition, operandLocation.asFPR(), minFloat.asFPR())
        : m_jit.branchDouble(minCondition, operandLocation.asFPR(), minFloat.asFPR());
    throwExceptionIf(ExceptionType::OutOfBoundsTrunc, belowMin);

    Jump aboveMax = operandType == Types::F32
        ? m_jit.branchFloat(DoubleCondition::DoubleGreaterThanOrEqualOrUnordered, operandLocation.asFPR(), maxFloat.asFPR())
        : m_jit.branchDouble(DoubleCondition::DoubleGreaterThanOrEqualOrUnordered, operandLocation.asFPR(), maxFloat.asFPR());
    throwExceptionIf(ExceptionType::OutOfBoundsTrunc, aboveMax);

    truncInBounds(kind, operandLocation, result, resultLocation);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::truncSaturated(Ext1OpType truncationOp, Value operand, Value& result, Type returnType, Type operandType)
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

    // FIXME: Lots of this is duplicated from AirIRGeneratorBase. Might be nice to unify it?
    uint64_t minResult = 0;
    uint64_t maxResult = 0;
    switch (kind) {
    case TruncationKind::I32TruncF32S:
    case TruncationKind::I32TruncF64S:
        maxResult = bitwise_cast<uint32_t>(INT32_MAX);
        minResult = bitwise_cast<uint32_t>(INT32_MIN);
        break;
    case TruncationKind::I32TruncF32U:
    case TruncationKind::I32TruncF64U:
        maxResult = bitwise_cast<uint32_t>(UINT32_MAX);
        minResult = bitwise_cast<uint32_t>(0U);
        break;
    case TruncationKind::I64TruncF32S:
    case TruncationKind::I64TruncF64S:
        maxResult = bitwise_cast<uint64_t>(INT64_MAX);
        minResult = bitwise_cast<uint64_t>(INT64_MIN);
        break;
    case TruncationKind::I64TruncF32U:
    case TruncationKind::I64TruncF64U:
        maxResult = bitwise_cast<uint64_t>(UINT64_MAX);
        minResult = bitwise_cast<uint64_t>(0ULL);
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
    truncInBounds(kind, operandLocation, result, resultLocation);
    resultLocation = locationOf(result);

    Jump afterInBounds = m_jit.jump();

    // Below-minimum case.
    lowerThanMin.link(&m_jit);

    // As an optimization, if the min result is 0; we can unconditionally return
    // that if the above-minimum-range check fails; otherwise, we need to check
    // for NaN since it also will fail the above-minimum-range-check
    if (!minResult) {
        if (returnType == Types::I32)
            m_jit.move(TrustedImm32(0), resultLocation.asGPR());
        else {
            m_jit.move(TrustedImm32(0), resultLocation.asGPRlo());
            m_jit.move(TrustedImm32(0), resultLocation.asGPRhi());
        }
    } else {
        Jump isNotNaN = operandType == Types::F32
            ? m_jit.branchFloat(DoubleCondition::DoubleEqualAndOrdered, operandLocation.asFPR(), operandLocation.asFPR())
            : m_jit.branchDouble(DoubleCondition::DoubleEqualAndOrdered, operandLocation.asFPR(), operandLocation.asFPR());

        // NaN case. Set result to zero.
        if (returnType == Types::I32)
            m_jit.move(TrustedImm32(0), resultLocation.asGPR());
        else {
            m_jit.move(TrustedImm32(0), resultLocation.asGPRlo());
            m_jit.move(TrustedImm32(0), resultLocation.asGPRhi());
        }
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
PartialResult WARN_UNUSED_RETURN BBQJIT::addRefI31(ExpressionType value, ExpressionType& result)
{
    if (value.isConst()) {
        result = Value::fromI64(JSValue::encode(JSValue(JSValue::Int32Tag, value.asI32() & 0x7fffffff)));
        LOG_INSTRUCTION("RefI31", value, RESULT(result));
        return { };
    }

    Location initialValue = loadIfNecessary(value);
    consume(value);

    result = topValue(TypeKind::I64);
    Location resultLocation = allocateWithHint(result, initialValue);

    LOG_INSTRUCTION("RefI31", value, RESULT(result));
    m_jit.and32(TrustedImm32(0x7fffffff), initialValue.asGPR(), resultLocation.asGPRlo());
    m_jit.move(TrustedImm32(JSValue::Int32Tag), resultLocation.asGPRhi());
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI31GetS(ExpressionType value, ExpressionType& result)
{
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
    emitThrowOnNullReference(ExceptionType::NullI31Get, initialValue);
    consume(value);

    result = topValue(TypeKind::I32);
    Location resultLocation = allocateWithHint(result, initialValue);

    LOG_INSTRUCTION("I31GetS", value, RESULT(result));

    m_jit.move(initialValue.asGPRlo(), resultLocation.asGPR());

    m_jit.lshift32(TrustedImm32(1), resultLocation.asGPR());
    m_jit.rshift32(TrustedImm32(1), resultLocation.asGPR());
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI31GetU(ExpressionType value, ExpressionType& result)
{
    if (value.isConst()) {
        if (JSValue::decode(value.asI64()).isNumber())
            result = Value::fromI32(value.asI64());
        else {
            emitThrowException(ExceptionType::NullI31Get);
            result = Value::fromI32(0);
        }

        LOG_INSTRUCTION("I31GetU", value, RESULT(result));

        return { };
    }


    Location initialValue = loadIfNecessary(value);
    emitThrowOnNullReference(ExceptionType::NullI31Get, initialValue);
    consume(value);

    result = topValue(TypeKind::I32);
    Location resultLocation = allocateWithHint(result, initialValue);

    LOG_INSTRUCTION("I31GetU", value, RESULT(result));

    m_jit.move(initialValue.asGPRlo(), resultLocation.asGPR());

    return { };
}

// This will replace the existing value with a new value. Note that if this is an F32 then the top bits may be garbage but that's ok for our current usage.
Value BBQJIT::marshallToI64(Value value)
{
    ASSERT(!value.isLocal());
    if (value.isConst()) {
        switch (value.type()) {
        case TypeKind::F64:
            return Value::fromI64(bitwise_cast<uint64_t>(value.asF64()));
        case TypeKind::F32:
        case TypeKind::I32:
            return Value::fromI64(bitwise_cast<uint32_t>(value.asI32()));
        default:
            return value;
        }
    }
    switch (value.type()) {
    case TypeKind::F64:
        flushValue(value);
        return Value::fromTemp(TypeKind::I64, value.asTemp());
    case TypeKind::F32:
    case TypeKind::I32: {
        flushValue(value);
        Location slot = canonicalSlot(value);
        m_jit.store32(TrustedImm32(0), slot.asAddress().withOffset(4));
        return Value::fromTemp(TypeKind::I64, value.asTemp());
    }
    default:
        return value;
    }
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType initValue, ExpressionType& result)
{
    result = topValue(TypeKind::Arrayref);

    initValue = marshallToI64(initValue);

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(typeIndex),
        size,
        initValue,
    };
    emitCCall(operationWasmArrayNew, arguments, result);

    Location resultLocation = loadIfNecessary(result);
    emitThrowOnNullReference(ExceptionType::BadArrayNew, resultLocation);

    LOG_INSTRUCTION("ArrayNew", typeIndex, size, initValue, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayNewFixed(uint32_t typeIndex, ArgumentList& args, ExpressionType& result)
{
    // Allocate an uninitialized array whose length matches the argument count
    // FIXME: inline the allocation.
    // https://bugs.webkit.org/show_bug.cgi?id=244388
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(typeIndex),
        Value::fromI32(args.size()),
    };
    Value allocationResult = Value::fromTemp(TypeKind::Arrayref, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + m_parser->expressionStack().size() + args.size());
    emitCCall(operationWasmArrayNewEmpty, arguments, allocationResult);

    Location allocationResultLocation = allocate(allocationResult);
    emitThrowOnNullReference(ExceptionType::BadArrayNew, allocationResultLocation);

    for (uint32_t i = 0; i < args.size(); ++i) {
        // Emit the array set code -- note that this omits the bounds check, since
        // if operationWasmArrayNewEmpty() returned a non-null value, it's an array of the right size
        allocationResultLocation = loadIfNecessary(allocationResult);
        Value pinnedResult = Value::pinned(TypeKind::I64, allocationResultLocation);
        emitArraySetUnchecked(typeIndex, pinnedResult, Value::fromI32(i), args[i]);
        consume(pinnedResult);
    }

    result = topValue(TypeKind::Arrayref);
    Location resultLocation = allocate(result);
    emitMove(allocationResult, resultLocation);

    // If args.isEmpty() then allocationResult.asTemp() == result.asTemp() so we will consume our result.
    if (args.size()) {
        consume(allocationResult);
        if (isRefType(getArrayElementType(typeIndex).unpacked()))
            emitWriteBarrier(resultLocation.asGPRlo());
    }

    LOG_INSTRUCTION("ArrayNewFixed", typeIndex, args.size(), RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayGet(ExtGCOpType arrayGetKind, uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result)
{
    StorageType elementType = getArrayElementType(typeIndex);
    Type resultType = elementType.unpacked();

    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArrayGet);
        result = Value::fromRef(resultType.kind, 0);
        return { };
    }

    Location arrayLocation = loadIfNecessary(arrayref);
    emitThrowOnNullReference(ExceptionType::NullArrayGet, arrayLocation);

    Location indexLocation;
    if (index.isConst()) {
        m_jit.load32(MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfSize()), wasmScratchGPR);
        throwExceptionIf(ExceptionType::OutOfBoundsArrayGet,
            m_jit.branch32(MacroAssembler::BelowOrEqual, wasmScratchGPR, TrustedImm32(index.asI32())));
    } else {
        indexLocation = loadIfNecessary(index);
        throwExceptionIf(ExceptionType::OutOfBoundsArrayGet,
            m_jit.branch32(MacroAssembler::AboveOrEqual, indexLocation.asGPR(), MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfSize())));
    }

    m_jit.loadPtr(MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfPayload()), wasmScratchGPR);

    consume(arrayref);
    result = topValue(resultType.kind);
    Location resultLocation = allocate(result);

    if (index.isConst()) {
        auto fieldAddress = MacroAssembler::Address(wasmScratchGPR, JSWebAssemblyArray::offsetOfElements(elementType) + elementType.elementSize() * index.asI32());

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
                m_jit.loadPair32(fieldAddress, resultLocation.asGPRlo(), resultLocation.asGPRhi());
                break;
            case TypeKind::F32:
                m_jit.loadFloat(fieldAddress, resultLocation.asFPR());
                break;
            case TypeKind::F64:
                m_jit.loadDouble(fieldAddress, resultLocation.asFPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    } else {
        auto scale = static_cast<MacroAssembler::Scale>(std::bit_width(elementType.elementSize()) - 1);
        auto fieldAddress = MacroAssembler::Address(wasmScratchGPR, JSWebAssemblyArray::offsetOfElements(elementType));
        auto fieldBaseIndex = fieldAddress.indexedBy(indexLocation.asGPR(), scale);

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
                m_jit.loadPair32(fieldBaseIndex, resultLocation.asGPRlo(), resultLocation.asGPRhi());
                break;
            case TypeKind::F32:
                m_jit.loadFloat(fieldBaseIndex, resultLocation.asFPR());
                break;
            case TypeKind::F64:
                m_jit.loadDouble(fieldBaseIndex, resultLocation.asFPR());
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

void BBQJIT::emitArraySetUnchecked(uint32_t typeIndex, Value arrayref, Value index, Value value)
{
    StorageType elementType = getArrayElementType(typeIndex);

    Location arrayLocation;
    if (arrayref.isPinned())
        arrayLocation = locationOf(arrayref);
    else
        arrayLocation = loadIfNecessary(arrayref);
    ScratchScope<1, 0> tmp(*this);
    auto payloadGPR = tmp.gpr(0);
    m_jit.loadPtr(MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfPayload()), payloadGPR);
    if (index.isConst()) {
        ScratchScope<1, 0> scratches(*this);
        auto fieldAddress = MacroAssembler::Address(payloadGPR, JSWebAssemblyArray::offsetOfElements(elementType) + elementType.elementSize() * index.asI32());

        Location valueLocation;
        if (value.isConst() && value.isFloat()) {
            ScratchScope<0, 1> scratches(*this);
            valueLocation = Location::fromFPR(scratches.fpr(0));
            // Materialize the constant to ensure constant blinding.
            emitMoveConst(value, valueLocation);
        } else if (value.isConst() && typeNeedsGPR2(value.type())) {
            ScratchScope<2, 0> scratches(*this);
            valueLocation = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
            // Materialize the constant to ensure constant blinding.
            emitMoveConst(value, valueLocation);
        } else if (value.isConst()) {
            ScratchScope<1, 0> scratches(*this);
            valueLocation = Location::fromGPR(scratches.gpr(0));
            // Materialize the constant to ensure constant blinding.
            emitMoveConst(value, valueLocation);
        } else
            valueLocation = loadIfNecessary(value);
        ASSERT(valueLocation.isRegister());

        if (elementType.is<PackedType>()) {
            switch (elementType.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                m_jit.store8(valueLocation.asGPR(), fieldAddress);
                break;
            case Wasm::PackedType::I16:
                m_jit.store16(valueLocation.asGPR(), fieldAddress);
                break;
            }
        } else {
            ASSERT(elementType.is<Type>());
            switch (value.type()) {
            case TypeKind::I32:
                m_jit.store32(valueLocation.asGPR(), fieldAddress);
                break;
            case TypeKind::I64:
                m_jit.storePair32(valueLocation.asGPRlo(), valueLocation.asGPRhi(), fieldAddress);
                break;
            case TypeKind::F32:
                m_jit.storeFloat(valueLocation.asFPR(), fieldAddress);
                break;
            case TypeKind::F64:
                m_jit.storeDouble(valueLocation.asFPR(), fieldAddress);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    } else {
        Location indexLocation = loadIfNecessary(index);
        auto scale = static_cast<MacroAssembler::Scale>(std::bit_width(elementType.elementSize()) - 1);
        auto fieldAddress = MacroAssembler::Address(payloadGPR, JSWebAssemblyArray::offsetOfElements(elementType));
        auto fieldBaseIndex = fieldAddress.indexedBy(indexLocation.asGPR(), scale);

        Location valueLocation;
        if (value.isConst() && value.isFloat()) {
            ScratchScope<0, 1> scratches(*this);
            valueLocation = Location::fromFPR(scratches.fpr(0));
            emitMoveConst(value, valueLocation);
        } else if (value.isConst() && typeNeedsGPR2(value.type())) {
            ScratchScope<2, 0> scratches(*this);
            valueLocation = Location::fromGPR2(scratches.gpr(1), scratches.gpr(0));
            emitMoveConst(value, valueLocation);
        } else if (value.isConst()) {
            ScratchScope<1, 0> scratches(*this);
            valueLocation = Location::fromGPR(scratches.gpr(0));
            emitMoveConst(value, valueLocation);
        } else
            valueLocation = loadIfNecessary(value);
        ASSERT(valueLocation.isRegister());

        if (elementType.is<PackedType>()) {
            switch (elementType.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                m_jit.store8(valueLocation.asGPR(), fieldBaseIndex);
                break;
            case Wasm::PackedType::I16:
                m_jit.store16(valueLocation.asGPR(), fieldBaseIndex);
                break;
            }
        } else {
            ASSERT(elementType.is<Type>());
            switch (value.type()) {
            case TypeKind::I32:
                m_jit.store32(valueLocation.asGPR(), fieldBaseIndex);
                break;
            case TypeKind::I64:
                m_jit.storePair32(valueLocation.asGPRlo(), valueLocation.asGPRhi(), fieldBaseIndex);
                break;
            case TypeKind::F32:
                m_jit.storeFloat(valueLocation.asFPR(), fieldBaseIndex);
                break;
            case TypeKind::F64:
                m_jit.storeDouble(valueLocation.asFPR(), fieldBaseIndex);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    consume(index);
    consume(value);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value)
{
    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArraySet);
        return { };
    }

    Location arrayLocation = loadIfNecessary(arrayref);
    emitThrowOnNullReference(ExceptionType::NullArraySet, arrayLocation);

    if (index.isConst()) {
        m_jit.load32(MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfSize()), wasmScratchGPR);
        throwExceptionIf(ExceptionType::OutOfBoundsArraySet,
            m_jit.branch32(MacroAssembler::BelowOrEqual, wasmScratchGPR, TrustedImm32(index.asI32())));
    } else {
        Location indexLocation = loadIfNecessary(index);
        throwExceptionIf(ExceptionType::OutOfBoundsArraySet,
            m_jit.branch32(MacroAssembler::AboveOrEqual, indexLocation.asGPR(), MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfSize())));
    }

    emitArraySetUnchecked(typeIndex, arrayref, index, value);

    if (isRefType(getArrayElementType(typeIndex).unpacked()))
        emitWriteBarrier(arrayLocation.asGPRlo());
    consume(arrayref);

    LOG_INSTRUCTION("ArraySet", typeIndex, arrayref, index, value);
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayLen(ExpressionType arrayref, ExpressionType& result)
{
    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArrayLen);
        result = Value::fromI32(0);
        LOG_INSTRUCTION("ArrayLen", arrayref, RESULT(result), "Exception");
        return { };
    }

    Location arrayLocation = loadIfNecessary(arrayref);
    consume(arrayref);
    emitThrowOnNullReference(ExceptionType::NullArrayLen, arrayLocation);

    result = topValue(TypeKind::I32);
    Location resultLocation = allocateWithHint(result, arrayLocation);
    m_jit.load32(MacroAssembler::Address(arrayLocation.asGPRlo(), JSWebAssemblyArray::offsetOfSize()), resultLocation.asGPR());

    LOG_INSTRUCTION("ArrayLen", arrayref, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addArrayFill(uint32_t typeIndex, ExpressionType arrayref, ExpressionType offset, ExpressionType value, ExpressionType size)
{
    if (arrayref.isConst()) {
        ASSERT(arrayref.asI64() == JSValue::encode(jsNull()));
        emitThrowException(ExceptionType::NullArrayFill);
        return { };
    }

    emitThrowOnNullReference(ExceptionType::NullArrayFill, loadIfNecessary(arrayref));

    Value shouldThrow = topValue(TypeKind::I32);
    value = marshallToI64(value);
    Vector<Value, 8> arguments = {
        instanceValue(),
        arrayref,
        offset,
        value,
        size
    };
    emitCCall(&operationWasmArrayFill, arguments, shouldThrow);
    Location shouldThrowLocation = allocate(shouldThrow);

    LOG_INSTRUCTION("ArrayFill", typeIndex, arrayref, offset, value, size);

    throwExceptionIf(ExceptionType::OutOfBoundsArrayFill, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

    consume(shouldThrow);

    return { };
}

void BBQJIT::emitStructPayloadSet(GPRReg payloadGPR, const StructType& structType, uint32_t fieldIndex, Value value)
{
    unsigned fieldOffset = *structType.offsetOfField(fieldIndex);
    RELEASE_ASSERT((std::numeric_limits<int32_t>::max() & fieldOffset) == fieldOffset);

    TypeKind kind = toValueKind(structType.field(fieldIndex).type.unpacked().kind);
    if (value.isConst()) {
        switch (kind) {
        case TypeKind::I32:
            if (structType.field(fieldIndex).type.is<PackedType>()) {
                ScratchScope<1, 0> scratches(*this);
                // If it's a packed type, we materialize the constant to ensure constant blinding.
                emitMoveConst(value, Location::fromGPR(scratches.gpr(0)));
                switch (structType.field(fieldIndex).type.as<PackedType>()) {
                case PackedType::I8:
                    m_jit.store8(scratches.gpr(0), MacroAssembler::Address(payloadGPR, fieldOffset));
                    break;
                case PackedType::I16:
                    m_jit.store16(scratches.gpr(0), MacroAssembler::Address(payloadGPR, fieldOffset));
                    break;
                }
                break;
            }
            m_jit.store32(MacroAssembler::Imm32(value.asI32()), MacroAssembler::Address(payloadGPR, fieldOffset));
            break;
        case TypeKind::F32:
            m_jit.store32(MacroAssembler::Imm32(value.asI32()), MacroAssembler::Address(payloadGPR, fieldOffset));
            break;
        case TypeKind::I64:
        case TypeKind::F64:
            m_jit.storePair32(TrustedImm32(value.asI64lo()), TrustedImm32(value.asI64hi()), MacroAssembler::Address(payloadGPR, fieldOffset));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        return;
    }

    Location valueLocation = loadIfNecessary(value);
    switch (kind) {
    case TypeKind::I32:
        if (structType.field(fieldIndex).type.is<PackedType>()) {
            switch (structType.field(fieldIndex).type.as<PackedType>()) {
            case PackedType::I8:
                m_jit.store8(valueLocation.asGPR(), MacroAssembler::Address(payloadGPR, fieldOffset));
                break;
            case PackedType::I16:
                m_jit.store16(valueLocation.asGPR(), MacroAssembler::Address(payloadGPR, fieldOffset));
                break;
            }
            break;
        }
        m_jit.store32(valueLocation.asGPR(), MacroAssembler::Address(payloadGPR, fieldOffset));
        break;
    case TypeKind::I64:
        m_jit.storePair32(valueLocation.asGPRlo(), valueLocation.asGPRhi(), MacroAssembler::Address(payloadGPR, fieldOffset));
        break;
    case TypeKind::F32:
        m_jit.storeFloat(valueLocation.asFPR(), MacroAssembler::Address(payloadGPR, fieldOffset));
        break;
    case TypeKind::F64:
        m_jit.storeDouble(valueLocation.asFPR(), MacroAssembler::Address(payloadGPR, fieldOffset));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    consume(value);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructNewDefault(uint32_t typeIndex, ExpressionType& result)
{

    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(typeIndex),
    };
    result = topValue(TypeKind::I64);
    emitCCall(operationWasmStructNewEmpty, arguments, result);

    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    Location structLocation = allocate(result);
    emitThrowOnNullReference(ExceptionType::BadStructNew, structLocation);
    m_jit.loadPtr(MacroAssembler::Address(structLocation.asGPRlo(), JSWebAssemblyStruct::offsetOfPayload()), wasmScratchGPR);
    for (StructFieldCount i = 0; i < structType.fieldCount(); ++i) {
        if (Wasm::isRefType(structType.field(i).type))
            emitStructPayloadSet(wasmScratchGPR, structType, i, Value::fromRef(TypeKind::RefNull, JSValue::encode(jsNull())));
        else
            emitStructPayloadSet(wasmScratchGPR, structType, i, Value::fromI64(0));
    }

    // No write barrier needed here as all fields are set to constants.

    LOG_INSTRUCTION("StructNewDefault", typeIndex, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructNew(uint32_t typeIndex, ArgumentList& args, Value& result)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        Value::fromI32(typeIndex),
    };
    // Note: using topValue here would be wrong because args[0] would be clobbered by the result.
    Value allocationResult = Value::fromTemp(TypeKind::Structref, currentControlData().enclosedHeight() + currentControlData().implicitSlots() + m_parser->expressionStack().size() + args.size());
    emitCCall(operationWasmStructNewEmpty, arguments, allocationResult);

    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    Location structLocation = allocate(allocationResult);
    emitThrowOnNullReference(ExceptionType::BadStructNew, structLocation);
    m_jit.loadPtr(MacroAssembler::Address(structLocation.asGPRlo(), JSWebAssemblyStruct::offsetOfPayload()), wasmScratchGPR);
    bool hasRefTypeField = false;
    for (uint32_t i = 0; i < args.size(); ++i) {
        if (isRefType(structType.field(i).type))
            hasRefTypeField = true;
        emitStructPayloadSet(wasmScratchGPR, structType, i, args[i]);
    }

    if (hasRefTypeField)
        emitWriteBarrier(structLocation.asGPRlo());

    result = topValue(TypeKind::Structref);
    Location resultLocation = allocate(result);
    emitMove(allocationResult, resultLocation);
    // If args.isEmpty() then allocationResult.asTemp() == result.asTemp() so we will consume our result.
    if (args.size())
        consume(allocationResult);

    LOG_INSTRUCTION("StructNew", typeIndex, args, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructGet(ExtGCOpType structGetKind, Value structValue, const StructType& structType, uint32_t fieldIndex, Value& result)
{
    TypeKind resultKind = structType.field(fieldIndex).type.unpacked().kind;
    if (structValue.isConst()) {
        // This is the only constant struct currently possible.
        ASSERT(JSValue::decode(structValue.asRef()).isNull());
        emitThrowException(ExceptionType::NullStructGet);
        result = Value::fromRef(resultKind, 0);
        LOG_INSTRUCTION("StructGet", structValue, fieldIndex, "Exception");
        return { };
    }

    Location structLocation = loadIfNecessary(structValue);
    emitThrowOnNullReference(ExceptionType::NullStructGet, structLocation);

    m_jit.loadPtr(MacroAssembler::Address(structLocation.asGPRlo(), JSWebAssemblyStruct::offsetOfPayload()), wasmScratchGPR);
    unsigned fieldOffset = *structType.offsetOfField(fieldIndex);
    RELEASE_ASSERT((std::numeric_limits<int32_t>::max() & fieldOffset) == fieldOffset);

    consume(structValue);
    result = topValue(resultKind);
    Location resultLocation = allocate(result);

    switch (result.type()) {
    case TypeKind::I32:
        if (structType.field(fieldIndex).type.is<PackedType>()) {
            switch (structType.field(fieldIndex).type.as<PackedType>()) {
            case PackedType::I8:
                m_jit.load8(MacroAssembler::Address(wasmScratchGPR, fieldOffset), resultLocation.asGPR());
                break;
            case PackedType::I16:
                m_jit.load16(MacroAssembler::Address(wasmScratchGPR, fieldOffset), resultLocation.asGPR());
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
        m_jit.load32(MacroAssembler::Address(wasmScratchGPR, fieldOffset), resultLocation.asGPR());
        break;
    case TypeKind::I64:
        m_jit.loadPair32(MacroAssembler::Address(wasmScratchGPR, fieldOffset), resultLocation.asGPRlo(), resultLocation.asGPRhi());
        break;
    case TypeKind::F32:
        m_jit.loadFloat(MacroAssembler::Address(wasmScratchGPR, fieldOffset), resultLocation.asFPR());
        break;
    case TypeKind::F64:
        m_jit.loadDouble(MacroAssembler::Address(wasmScratchGPR, fieldOffset), resultLocation.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    LOG_INSTRUCTION("StructGet", structValue, fieldIndex, RESULT(result));
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addStructSet(Value structValue, const StructType& structType, uint32_t fieldIndex, Value value)
{
    if (structValue.isConst()) {
        // This is the only constant struct currently possible.
        ASSERT(JSValue::decode(structValue.asRef()).isNull());
        emitThrowException(ExceptionType::NullStructSet);
        LOG_INSTRUCTION("StructSet", structValue, fieldIndex, value, "Exception");
        return { };
    }

    Location structLocation = loadIfNecessary(structValue);
    emitThrowOnNullReference(ExceptionType::NullStructSet, structLocation);

    emitStructSet(structLocation.asGPRlo(), structType, fieldIndex, value);
    LOG_INSTRUCTION("StructSet", structValue, fieldIndex, value);

    consume(structValue);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefCast(ExpressionType reference, bool allowNull, int32_t heapType, ExpressionType& result)
{
    Vector<Value, 8> arguments = {
        instanceValue(),
        reference,
        Value::fromI32(allowNull),
        Value::fromI32(heapType),
    };
    result = topValue(TypeKind::Ref);
    emitCCall(operationWasmRefCast, arguments, result);
    Location resultLocation = allocate(result);
    throwExceptionIf(ExceptionType::CastFailure,
        m_jit.branch32(MacroAssembler::Equal, resultLocation.asGPRhi(), TrustedImm32(JSValue::EmptyValueTag)));

    LOG_INSTRUCTION("RefCast", reference, allowNull, heapType, RESULT(result));

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Add(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Add", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() + rhs.asI64())),
        BLOCK(
            m_jit.add64(lhsLocation.asGPRhi(), lhsLocation.asGPRlo(), rhsLocation.asGPRhi(), rhsLocation.asGPRlo(), resultLocation.asGPRhi(), resultLocation.asGPRlo());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            m_jit.add64(lhsLocation.asGPRhi(), lhsLocation.asGPRlo(), rhsLocation.asGPRhi(), rhsLocation.asGPRlo(), resultLocation.asGPRhi(), resultLocation.asGPRlo());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Sub(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Sub", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() - rhs.asI64())),
        BLOCK(
            m_jit.sub64(
                lhsLocation.asGPRhi(), lhsLocation.asGPRlo(),
                rhsLocation.asGPRhi(), rhsLocation.asGPRlo(),
                resultLocation.asGPRhi(), resultLocation.asGPRlo()
            );
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            m_jit.sub64(
                lhsLocation.asGPRhi(), lhsLocation.asGPRlo(),
                rhsLocation.asGPRhi(), rhsLocation.asGPRlo(),
                resultLocation.asGPRhi(), resultLocation.asGPRlo()
            );
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Mul(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Mul", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() * rhs.asI64())),
        BLOCK(
            ScratchScope<2, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
            Location tmpHiLo = Location::fromGPR(scratches.gpr(0));
            Location tmpLoHi = Location::fromGPR(scratches.gpr(1));
            m_jit.mul32(lhsLocation.asGPRhi(), rhsLocation.asGPRlo(), tmpHiLo.asGPR());
            m_jit.mul32(lhsLocation.asGPRlo(), rhsLocation.asGPRhi(), tmpLoHi.asGPR());
            m_jit.uMull32(lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), resultLocation.asGPRhi(),  resultLocation.asGPRlo());
            m_jit.add32(tmpHiLo.asGPR(), resultLocation.asGPRhi());
            m_jit.add32(tmpLoHi.asGPR(), resultLocation.asGPRhi());
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR,  wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            ScratchScope<2, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
            Location tmpHiLo = Location::fromGPR(scratches.gpr(0));
            Location tmpLoHi = Location::fromGPR(scratches.gpr(1));
            m_jit.mul32(lhsLocation.asGPRhi(), rhsLocation.asGPRlo(), tmpHiLo.asGPR());
            m_jit.mul32(lhsLocation.asGPRlo(), rhsLocation.asGPRhi(), tmpLoHi.asGPR());
            m_jit.uMull32(lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), resultLocation.asGPRhi(),  resultLocation.asGPRlo());
            m_jit.add32(tmpHiLo.asGPR(), resultLocation.asGPRhi());
            m_jit.add32(tmpLoHi.asGPR(), resultLocation.asGPRhi());
        )
    );
}

void BBQJIT::emitThrowOnNullReference(ExceptionType type, Location ref)
{
    throwExceptionIf(type, m_jit.branch32(MacroAssembler::Equal, ref.asGPRhi(), TrustedImm32(JSValue::NullTag)));
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64And(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64And", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() & rhs.asI64())),
        BLOCK(
            m_jit.and32(lhsLocation.asGPRhi(), rhsLocation.asGPRhi(), resultLocation.asGPRhi());
            m_jit.and32(lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), resultLocation.asGPRlo());
        ),
        BLOCK(
            m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPRhi(), resultLocation.asGPRhi());
            m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPRlo(), resultLocation.asGPRlo());
            m_jit.and32(TrustedImm32(static_cast<int32_t>(ImmHelpers::imm(lhs, rhs).asI64() >> 32)), resultLocation.asGPRhi());
            m_jit.and32(TrustedImm32(static_cast<int32_t>(ImmHelpers::imm(lhs, rhs).asI64() & 0xffffffff)), resultLocation.asGPRlo());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Xor(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Xor", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() ^ rhs.asI64())),
        BLOCK(
            m_jit.xor32(lhsLocation.asGPRhi(), rhsLocation.asGPRhi(), resultLocation.asGPRhi());
            m_jit.xor32(lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), resultLocation.asGPRlo());
        ),
        BLOCK(
            m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPRhi(), resultLocation.asGPRhi());
            m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPRlo(), resultLocation.asGPRlo());
            m_jit.xor32(TrustedImm32(static_cast<int32_t>(ImmHelpers::imm(lhs, rhs).asI64() >> 32)), resultLocation.asGPRhi());
            m_jit.xor32(TrustedImm32(static_cast<int32_t>(ImmHelpers::imm(lhs, rhs).asI64() & 0xffffffff)), resultLocation.asGPRlo());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Or(Value lhs, Value rhs, Value& result)
{
    EMIT_BINARY(
        "I64Or", TypeKind::I64,
        BLOCK(Value::fromI64(lhs.asI64() | rhs.asI64())),
        BLOCK(
            m_jit.or32(lhsLocation.asGPRhi(), rhsLocation.asGPRhi(), resultLocation.asGPRhi());
            m_jit.or32(lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), resultLocation.asGPRlo());
        ),
        BLOCK(
            m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPRhi(), resultLocation.asGPRhi());
            m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPRlo(), resultLocation.asGPRlo());
            m_jit.or32(TrustedImm32(static_cast<int32_t>(ImmHelpers::imm(lhs, rhs).asI64() >> 32)), resultLocation.asGPRhi());
            m_jit.or32(TrustedImm32(static_cast<int32_t>(ImmHelpers::imm(lhs, rhs).asI64() & 0xffffffff)), resultLocation.asGPRlo());
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
            shiftI64Helper(ShiftI64HelperOp::Lshift, lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            shiftI64Helper(ShiftI64HelperOp::Lshift, lhsLocation, rhsLocation, resultLocation);
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
            shiftI64Helper(ShiftI64HelperOp::Rshift, lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            shiftI64Helper(ShiftI64HelperOp::Rshift, lhsLocation, rhsLocation, resultLocation);
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
            shiftI64Helper(ShiftI64HelperOp::Urshift, lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            shiftI64Helper(ShiftI64HelperOp::Urshift, lhsLocation, rhsLocation, resultLocation);
        )
    );
}

void BBQJIT::shiftI64Helper(ShiftI64HelperOp op, Location lhsLocation, Location rhsLocation, Location resultLocation)
{
    auto shift = rhsLocation.asGPRlo();
    m_jit.and32(TrustedImm32(63), rhsLocation.asGPRlo(), shift);
    auto zero = m_jit.branch32(RelationalCondition::Equal, shift, TrustedImm32(0));
    auto aboveOrEqual32 = m_jit.branch32(RelationalCondition::AboveOrEqual, shift, TrustedImm32(32));
    // shift < 32
    ScratchScope<1, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
    auto carry = scratches.gpr(0);
    m_jit.move(TrustedImm32(32), carry);
    m_jit.sub32(carry, shift, carry);
    if (op == ShiftI64HelperOp::Lshift) {
        ASSERT(resultLocation.asGPRhi() != shift);
        ASSERT(resultLocation.asGPRhi() != lhsLocation.asGPRlo());
        m_jit.lshift32(lhsLocation.asGPRhi(), shift, resultLocation.asGPRhi());
        m_jit.urshift32(lhsLocation.asGPRlo(), carry, carry);
        m_jit.or32(carry, resultLocation.asGPRhi());
        m_jit.lshift32(lhsLocation.asGPRlo(), shift, resultLocation.asGPRlo());
    } else if (op == ShiftI64HelperOp::Urshift) {
        m_jit.lshift32(lhsLocation.asGPRhi(), carry, carry);
        ASSERT(resultLocation.asGPRhi() != shift);
        ASSERT(resultLocation.asGPRhi() != lhsLocation.asGPRlo());
        m_jit.urshift32(lhsLocation.asGPRhi(), shift, resultLocation.asGPRhi());
        m_jit.urshift32(lhsLocation.asGPRlo(), shift, resultLocation.asGPRlo());
        m_jit.or32(carry, resultLocation.asGPRlo());
    } else if (op ==ShiftI64HelperOp::Rshift) {
        m_jit.lshift32(lhsLocation.asGPRhi(), carry, carry);
        ASSERT(resultLocation.asGPRhi() != shift);
        ASSERT(resultLocation.asGPRhi() != lhsLocation.asGPRlo());
        m_jit.rshift32(lhsLocation.asGPRhi(), shift, resultLocation.asGPRhi());
        m_jit.urshift32(lhsLocation.asGPRlo(), shift, resultLocation.asGPRlo());
        m_jit.or32(carry, resultLocation.asGPRlo());
    }
    auto done = m_jit.jump();
    // shift >= 32
    aboveOrEqual32.link(&m_jit);
    m_jit.sub32(shift, TrustedImm32(32), shift);
    if (op == ShiftI64HelperOp::Lshift) {
        m_jit.lshift32(lhsLocation.asGPRlo(), shift, resultLocation.asGPRhi());
        m_jit.xor32(resultLocation.asGPRlo(), resultLocation.asGPRlo());
    } else if (op == ShiftI64HelperOp::Urshift) {
        m_jit.urshift32(lhsLocation.asGPRhi(), shift, resultLocation.asGPRlo());
        m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
    } else if (op ==ShiftI64HelperOp::Rshift) {
        ASSERT(resultLocation.asGPRlo() != lhsLocation.asGPRhi());
        m_jit.rshift32(lhsLocation.asGPRhi(), shift, resultLocation.asGPRlo());
        m_jit.rshift32(lhsLocation.asGPRhi(), TrustedImm32(31), resultLocation.asGPRhi());
    }
    // shift == 0
    zero.link(&m_jit);
    done.link(&m_jit);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Rotl(Value lhs, Value rhs, Value& result)
{
    PREPARE_FOR_SHIFT;
    EMIT_BINARY(
        "I64Rotl", TypeKind::I64,
        BLOCK(Value::fromI64(B3::rotateLeft(lhs.asI64(), rhs.asI64()))),
        BLOCK(
            rotI64Helper(RotI64HelperOp::Left, lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            rotI64Helper(RotI64HelperOp::Left, lhsLocation, rhsLocation, resultLocation);
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
            rotI64Helper(RotI64HelperOp::Right, lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            rotI64Helper(RotI64HelperOp::Right, lhsLocation, rhsLocation, resultLocation);
        )
    );
}

void BBQJIT::rotI64Helper(RotI64HelperOp op, Location lhsLocation, Location rhsLocation, Location resultLocation)
{
    // NB: this only works as long as result is allocated in lhsLocation!
    auto carry = rhsLocation.asGPRhi();
    auto shift = rhsLocation.asGPRlo();
    ScratchScope<2, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
    auto tmpHi = scratches.gpr(0);
    auto tmpLo = scratches.gpr(1);
    m_jit.move(lhsLocation.asGPRhi(), resultLocation.asGPRhi());
    m_jit.move(lhsLocation.asGPRlo(), resultLocation.asGPRlo());
    m_jit.and32(TrustedImm32(63), shift, shift);
    auto rotate = m_jit.branch32(RelationalCondition::LessThan, shift, TrustedImm32(32));
    // swap
    m_jit.swap(resultLocation.asGPRhi(), resultLocation.asGPRlo());
    rotate.link(&m_jit);
    m_jit.and32(TrustedImm32(31), shift, shift);
    auto zero = m_jit.branch32(RelationalCondition::Equal, shift, TrustedImm32(0));
    // rotate
    m_jit.move(TrustedImm32(32), carry);
    m_jit.sub32(carry, shift, carry);
    if (op == RotI64HelperOp::Left) {
        m_jit.urshift32(resultLocation.asGPRhi(), carry, tmpLo);
        m_jit.urshift32(resultLocation.asGPRlo(), carry, tmpHi);
        m_jit.lshift32(resultLocation.asGPRhi(), shift, resultLocation.asGPRhi());
        m_jit.lshift32(resultLocation.asGPRlo(), shift, resultLocation.asGPRlo());
    } else if (op == RotI64HelperOp::Right) {
        m_jit.lshift32(resultLocation.asGPRhi(), carry, tmpLo);
        m_jit.lshift32(resultLocation.asGPRlo(), carry, tmpHi);
        m_jit.urshift32(resultLocation.asGPRhi(), shift, resultLocation.asGPRhi());
        m_jit.urshift32(resultLocation.asGPRlo(), shift, resultLocation.asGPRlo());
    }
    m_jit.or32(tmpHi, resultLocation.asGPRhi());
    m_jit.or32(tmpLo, resultLocation.asGPRlo());
    zero.link(&m_jit);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Clz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Clz", TypeKind::I64,
        BLOCK(Value::fromI64(WTF::clz(operand.asI64()))),
        BLOCK(
            m_jit.countLeadingZeros32(operandLocation.asGPRhi(), resultLocation.asGPRhi());
            Jump done = m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPRhi(), TrustedImm32(32));
            m_jit.countLeadingZeros32(operandLocation.asGPRlo(), resultLocation.asGPRhi());
            m_jit.add32(TrustedImm32(32), resultLocation.asGPRhi());
            done.link(&m_jit);
            m_jit.move(resultLocation.asGPRhi(), resultLocation.asGPRlo());
            m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
        )
    );
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Ctz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Ctz", TypeKind::I64,
        BLOCK(Value::fromI64(WTF::ctz(operand.asI64()))),
        BLOCK(
            m_jit.countTrailingZeros32(operandLocation.asGPRlo(), resultLocation.asGPRlo());
            Jump done = m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPRlo(), TrustedImm32(32));
            m_jit.countTrailingZeros32(operandLocation.asGPRhi(), resultLocation.asGPRlo());
            m_jit.add32(TrustedImm32(32), resultLocation.asGPRlo());
            done.link(&m_jit);
            m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
        )
    );
}

PartialResult BBQJIT::emitCompareI64(const char* opcode, Value& lhs, Value& rhs, Value& result, RelationalCondition condition, bool (*comparator)(int64_t lhs, int64_t rhs))
{
    EMIT_BINARY(
        opcode, TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asI64(), rhs.asI64())))),
        BLOCK(
            compareI64Helper(condition, lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            compareI64Helper(condition, lhsLocation, rhsLocation, resultLocation);
        )
    )
}

void BBQJIT::compareI64Helper(RelationalCondition condition, Location lhsLocation, Location rhsLocation, Location resultLocation)
{
    if (condition == MacroAssembler::Equal || condition == MacroAssembler::NotEqual) {
        ScratchScope<1, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
        auto tmp = Location::fromGPR(scratches.gpr(0));
        m_jit.move(TrustedImm32(condition == MacroAssembler::NotEqual), tmp.asGPR());
        auto compareLo = m_jit.branch32(RelationalCondition::NotEqual, lhsLocation.asGPRhi(), rhsLocation.asGPRhi());
        m_jit.compare32(condition, lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), tmp.asGPR());
        compareLo.link(&m_jit);
        m_jit.move(tmp.asGPR(), resultLocation.asGPR());
    } else {
        auto compareLo = m_jit.branch32(RelationalCondition::Equal, lhsLocation.asGPRhi(), rhsLocation.asGPRhi());
        m_jit.compare32(condition, lhsLocation.asGPRhi(), rhsLocation.asGPRhi(), resultLocation.asGPR());
        auto done = m_jit.jump();
        compareLo.link(&m_jit);
        // Signed to unsigned, leave the rest alone
        RelationalCondition loCond = condition;
        switch (condition) {
        case MacroAssembler::GreaterThan:
            loCond = MacroAssembler::Above;
            break;
        case MacroAssembler::LessThan:
            loCond = MacroAssembler::Below;
            break;
        case MacroAssembler::GreaterThanOrEqual:
            loCond = MacroAssembler::AboveOrEqual;
            break;
        case MacroAssembler::LessThanOrEqual:
            loCond = MacroAssembler::BelowOrEqual;
            break;
        default:
            break;
        }
        m_jit.compare32(loCond, lhsLocation.asGPRlo(), rhsLocation.asGPRlo(), resultLocation.asGPR());
        done.link(&m_jit);
    }
}

PartialResult BBQJIT::addI32WrapI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "I32WrapI64", TypeKind::I32,
        BLOCK(Value::fromI32(static_cast<int32_t>(operand.asI64()))),
        BLOCK(
            m_jit.move(operandLocation.asGPRlo(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Extend8S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Extend8S", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int8_t>(operand.asI64())))),
        BLOCK(
            GPRReg operandReg = operandLocation.isGPR() ? operandLocation.asGPR() : operandLocation.asGPRlo();
            m_jit.signExtend8To32(operandReg, resultLocation.asGPRlo());
            m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Extend16S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Extend16S", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int16_t>(operand.asI64())))),
        BLOCK(
            GPRReg operandReg = operandLocation.isGPR() ? operandLocation.asGPR() : operandLocation.asGPRlo();
            m_jit.signExtend16To32(operandReg, resultLocation.asGPRlo());
            m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Extend32S(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Extend32S", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int32_t>(operand.asI64())))),
        BLOCK(
            GPRReg operandReg = operandLocation.isGPR() ? operandLocation.asGPR() : operandLocation.asGPRlo();
            m_jit.move(operandReg, resultLocation.asGPRlo());
            m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ExtendSI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64ExtendSI32", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<int64_t>(operand.asI32()))),
        BLOCK(
            GPRReg operandReg = operandLocation.isGPR() ? operandLocation.asGPR() : operandLocation.asGPRlo();
            m_jit.move(operandReg, resultLocation.asGPRlo());
            m_jit.rshift32(resultLocation.asGPRlo(), TrustedImm32(31), resultLocation.asGPRhi());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ExtendUI32(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64ExtendUI32", TypeKind::I64,
        BLOCK(Value::fromI64(static_cast<uint64_t>(static_cast<uint32_t>(operand.asI32())))),
        BLOCK(
            GPRReg operandReg = operandLocation.isGPR() ? operandLocation.asGPR() : operandLocation.asGPRlo();
            m_jit.move(operandReg, resultLocation.asGPRlo());
            m_jit.xor32(resultLocation.asGPRhi(), resultLocation.asGPRhi());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64Eqz(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64Eqz", TypeKind::I32,
        BLOCK(Value::fromI32(!operand.asI64())),
        BLOCK(
            m_jit.or32(operandLocation.asGPRhi(), operandLocation.asGPRlo(), resultLocation.asGPR());
            m_jit.test32(ResultCondition::Zero, resultLocation.asGPR(), resultLocation.asGPR(), resultLocation.asGPR());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addI64ReinterpretF64(Value operand, Value& result)
{
    EMIT_UNARY(
        "I64ReinterpretF64", TypeKind::I64,
        BLOCK(Value::fromI64(bitwise_cast<int64_t>(operand.asF64()))),
        BLOCK(
            m_jit.moveDoubleTo64(operandLocation.asFPR(), resultLocation.asGPRhi(), resultLocation.asGPRlo());
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ReinterpretI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ReinterpretI64", TypeKind::F64,
        BLOCK(Value::fromF64(bitwise_cast<double>(operand.asI64()))),
        BLOCK(
            m_jit.moveIntsToDouble(operandLocation.asGPRlo(), operandLocation.asGPRhi(), resultLocation.asFPR());
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
            auto arg = Value::pinned(TypeKind::I64, operandLocation);
            consume(result);
            emitCCall(Math::f32_convert_s_i64, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32ConvertUI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32ConvertUI64", TypeKind::F32,
        BLOCK(Value::fromF32(static_cast<uint64_t>(operand.asI64()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::I64, operandLocation);
            consume(result);
            emitCCall(Math::f32_convert_u_i64, ArgumentList { arg }, result);
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
            auto arg = Value::pinned(TypeKind::I64, operandLocation);
            consume(result);
            emitCCall(Math::f64_convert_s_i64, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64ConvertUI64(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64ConvertUI64", TypeKind::F64,
        BLOCK(Value::fromF64(static_cast<uint64_t>(operand.asI64()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::I64, operandLocation);
            consume(result);
            emitCCall(Math::f64_convert_u_i64, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Copysign(Value lhs, Value rhs, Value& result)
{
    if constexpr (isX86())
        clobber(shiftRCX);

    EMIT_BINARY(
        "F64Copysign", TypeKind::F64,
        BLOCK(Value::fromF64(doubleCopySign(lhs.asF64(), rhs.asF64()))),
        BLOCK(
            F64CopysignHelper(lhsLocation, rhsLocation, resultLocation);
        ),
        BLOCK(
            ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
            emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2));
            F64CopysignHelper(lhsLocation, rhsLocation, resultLocation);
        )
    )
}

void BBQJIT::F64CopysignHelper(Location lhsLocation, Location rhsLocation, Location resultLocation)
{
    ScratchScope<2, 0> scratches(*this);
    auto hi = scratches.gpr(1);
    auto lo = scratches.gpr(0);
    auto sign = wasmScratchGPR2;

    m_jit.moveDoubleHiTo32(rhsLocation.asFPR(), sign);
    m_jit.move(TrustedImm32(0x80000000), wasmScratchGPR);
    m_jit.and32(wasmScratchGPR, sign);

    m_jit.moveDoubleTo64(lhsLocation.asFPR(), hi, lo);
    m_jit.move(TrustedImm32(0x7fffffff), wasmScratchGPR);
    m_jit.and32(wasmScratchGPR, hi);

    m_jit.or32(sign, hi);
    m_jit.move64ToDouble(hi, lo, resultLocation.asFPR());
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Floor(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Floor", TypeKind::F32,
        BLOCK(Value::fromF32(Math::floorFloat(operand.asF32()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F32, operandLocation);
            consume(result);
            emitCCall(Math::floorFloat, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Floor(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Floor", TypeKind::F64,
        BLOCK(Value::fromF64(Math::floorDouble(operand.asF64()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F64, operandLocation);
            consume(result);
            emitCCall(Math::floorDouble, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Ceil(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Ceil", TypeKind::F32,
        BLOCK(Value::fromF32(Math::ceilFloat(operand.asF32()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F32, operandLocation);
            consume(result);
            emitCCall(Math::ceilFloat, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Ceil(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Ceil", TypeKind::F64,
        BLOCK(Value::fromF64(Math::ceilDouble(operand.asF64()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F64, operandLocation);
            consume(result);
            emitCCall(Math::ceilDouble, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Nearest(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Nearest", TypeKind::F32,
        BLOCK(Value::fromF32(std::nearbyintf(operand.asF32()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F32, operandLocation);
            consume(result);
            emitCCall(Math::f32_nearest, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Nearest(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Nearest", TypeKind::F64,
        BLOCK(Value::fromF64(std::nearbyint(operand.asF64()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F64, operandLocation);
            consume(result);
            emitCCall(Math::f64_nearest, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF32Trunc(Value operand, Value& result)
{
    EMIT_UNARY(
        "F32Trunc", TypeKind::F32,
        BLOCK(Value::fromF32(Math::truncFloat(operand.asF32()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F32, operandLocation);
            consume(result);
            emitCCall(Math::truncFloat, ArgumentList { arg }, result);
        )
    )
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addF64Trunc(Value operand, Value& result)
{
    EMIT_UNARY(
        "F64Trunc", TypeKind::F64,
        BLOCK(Value::fromF64(Math::truncDouble(operand.asF64()))),
        BLOCK(
            auto arg = Value::pinned(TypeKind::F64, operandLocation);
            consume(result);
            emitCCall(Math::truncDouble, ArgumentList { arg }, result);
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
            m_jit.compare32(RelationalCondition::Equal, operandLocation.asGPRhi(), TrustedImm32(JSValue::NullTag), resultLocation.asGPR());
        )
    );
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRefAsNonNull(Value value, Value& result)
{
    Location valueLocation;
    if (value.isConst()) {
        valueLocation = Location::fromGPR2(wasmScratchGPR, wasmScratchGPR2);
        emitMoveConst(value, valueLocation);
    } else
        valueLocation = loadIfNecessary(value);
    ASSERT(valueLocation.isGPR2());
    consume(value);

    result = topValue(TypeKind::Ref);
    Location resultLocation = allocate(result);
    throwExceptionIf(ExceptionType::NullRefAsNonNull, m_jit.branch32(RelationalCondition::Equal, valueLocation.asGPRhi(), TrustedImm32(JSValue::NullTag)));
    emitMove(TypeKind::Ref, valueLocation, resultLocation);

    return { };
}

void BBQJIT::emitCatchPrologue()
{
    m_frameSizeLabels.append(m_jit.moveWithPatch(TrustedImmPtr(nullptr), GPRInfo::nonPreservedNonArgumentGPR0));
    m_jit.subPtr(GPRInfo::callFrameRegister, GPRInfo::nonPreservedNonArgumentGPR0, wasmScratchGPR);
    m_jit.move(wasmScratchGPR, MacroAssembler::stackPointerRegister);

    static_assert(noOverlap(GPRInfo::nonPreservedNonArgumentGPR0, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2));
}

void BBQJIT::emitCatchAllImpl(ControlData& dataCatch)
{
    m_catchEntrypoints.append(m_jit.label());
    emitCatchPrologue();
    bind(this->exception(dataCatch), Location::fromGPR2(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR));
    Stack emptyStack { };
    dataCatch.startBlock(*this, emptyStack);
}

void BBQJIT::emitCatchImpl(ControlData& dataCatch, const TypeDefinition& exceptionSignature, ResultList& results)
{
    m_catchEntrypoints.append(m_jit.label());
    emitCatchPrologue();
    bind(this->exception(dataCatch), Location::fromGPR2(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR));
    Stack emptyStack { };
    dataCatch.startBlock(*this, emptyStack);

    if (exceptionSignature.as<FunctionSignature>()->argumentCount()) {
        ScratchScope<1, 0> scratches(*this);
        GPRReg bufferGPR = scratches.gpr(0);
        m_jit.loadPtr(Address(GPRInfo::returnValueGPR, JSWebAssemblyException::offsetOfPayload() + JSWebAssemblyException::Payload::offsetOfStorage()), bufferGPR);
        unsigned offset = 0;
        for (unsigned i = 0; i < exceptionSignature.as<FunctionSignature>()->argumentCount(); ++i) {
            Type type = exceptionSignature.as<FunctionSignature>()->argumentType(i);
            Value result = Value::fromTemp(type.kind, dataCatch.enclosedHeight() + dataCatch.implicitSlots() + i);
            Location slot = canonicalSlot(result);
            switch (type.kind) {
            case TypeKind::I32:
                m_jit.load32(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchGPR);
                m_jit.store32(wasmScratchGPR, slot.asAddress());
                break;
            case TypeKind::I31ref:
            case TypeKind::I64:
            case TypeKind::Ref:
            case TypeKind::RefNull:
            case TypeKind::Arrayref:
            case TypeKind::Structref:
            case TypeKind::Funcref:
            case TypeKind::Exn:
            case TypeKind::Externref:
            case TypeKind::Eqref:
            case TypeKind::Anyref:
            case TypeKind::Nullref:
            case TypeKind::Nullfuncref:
            case TypeKind::Nullexternref:
            case TypeKind::Rec:
            case TypeKind::Sub:
            case TypeKind::Subfinal:
            case TypeKind::Array:
            case TypeKind::Struct:
            case TypeKind::Func: {
                m_jit.loadPair32(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchGPR, wasmScratchGPR2);
                m_jit.storePair32(wasmScratchGPR, wasmScratchGPR2, slot.asAddress());
                break;
            }
            case TypeKind::F32:
                m_jit.loadFloat(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchFPR);
                m_jit.storeFloat(wasmScratchFPR, slot.asAddress());
                break;
            case TypeKind::F64:
                m_jit.loadDouble(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchFPR);
                m_jit.storeDouble(wasmScratchFPR, slot.asAddress());
                break;
            case TypeKind::V128:
                m_jit.loadVector(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchFPR);
                m_jit.storeVector(wasmScratchFPR, slot.asAddress());
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

    if (target.type == CatchKind::CatchRef || target.type == CatchKind::CatchAllRef) {
        if (target.target->targetLocations().last().isGPR())
            m_jit.move(GPRInfo::returnValueGPR, target.target->targetLocations().last().asGPR());
        else if (target.target->targetLocations().last().isGPR2()) {
            m_jit.move(GPRInfo::returnValueGPR, target.target->targetLocations().last().asGPRlo());
            m_jit.move(GPRInfo::returnValueGPR2, target.target->targetLocations().last().asGPRhi());
        } else
            m_jit.storePtr(GPRInfo::returnValueGPR, target.target->targetLocations().last().asAddress());
    }

    if (target.type == CatchKind::Catch || target.type == CatchKind::CatchRef) {
        auto signature = target.exceptionSignature->template as<FunctionSignature>();
        if (signature->argumentCount()) {
            ScratchScope<1, 0> scratches(*this);
            GPRReg bufferGPR = scratches.gpr(0);
            m_jit.loadPtr(Address(GPRInfo::returnValueGPR, JSWebAssemblyException::offsetOfPayload() + JSWebAssemblyException::Payload::offsetOfStorage()), bufferGPR);
            unsigned offset = 0;
            for (unsigned i = 0; i < signature->argumentCount(); ++i) {
                Type type = signature->argumentType(i);
                Location slot = target.target->targetLocations()[i];
                switch (type.kind) {
                case TypeKind::I32:
                    if (slot.isGPR())
                        m_jit.load32(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asGPR());
                    else
                        m_jit.transfer32(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asAddress());
                    break;
                case TypeKind::I31ref:
                case TypeKind::I64:
                case TypeKind::Ref:
                case TypeKind::RefNull:
                case TypeKind::Arrayref:
                case TypeKind::Structref:
                case TypeKind::Funcref:
                case TypeKind::Exn:
                case TypeKind::Externref:
                case TypeKind::Eqref:
                case TypeKind::Anyref:
                case TypeKind::Nullref:
                case TypeKind::Nullfuncref:
                case TypeKind::Nullexternref:
                case TypeKind::Rec:
                case TypeKind::Sub:
                case TypeKind::Subfinal:
                case TypeKind::Array:
                case TypeKind::Struct:
                case TypeKind::Func:
                    if (slot.isGPR2())
                        m_jit.loadPair32(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asGPRlo(), slot.asGPRhi());
                    else {
                        m_jit.loadPair32(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchGPR, wasmScratchGPR2);
                        m_jit.storePair32(wasmScratchGPR, wasmScratchGPR2, slot.asAddress());
                    }
                    break;
                case TypeKind::F32:
                    if (slot.isFPR())
                        m_jit.loadFloat(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asFPR());
                    else {
                        m_jit.loadFloat(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchFPR);
                        m_jit.storeFloat(wasmScratchFPR, slot.asAddress());
                    }
                    break;
                case TypeKind::F64:
                    if (slot.isFPR())
                        m_jit.loadDouble(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), slot.asFPR());
                    else {
                        m_jit.loadDouble(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchFPR);
                        m_jit.storeDouble(wasmScratchFPR, slot.asAddress());
                    }
                    break;
                case TypeKind::V128:
                    m_jit.loadVector(Address(bufferGPR, JSWebAssemblyException::Payload::Storage::offsetOfData() + offset * sizeof(uint64_t)), wasmScratchFPR);
                    m_jit.storeVector(wasmScratchFPR, slot.asAddress());
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
    target.target->addBranch(m_jit.jump());
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addThrowRef(Value exception, Stack&)
{
    LOG_INSTRUCTION("ThrowRef", exception);

    emitMove(exception, Location::fromGPR2(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3));
    consume(exception);

    ++m_callSiteIndex;
    bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();
    if (mayHaveExceptionHandlers) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegisters();
    }

    // Check for a null exception
    auto nullexn = m_jit.branch32(CCallHelpers::Equal, GPRInfo::argumentGPR2, TrustedImm32(JSValue::NullTag));

    m_jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    emitThrowRefImpl(m_jit);

    nullexn.linkTo(m_jit.label(), &m_jit);

    emitThrowException(ExceptionType::NullExnReference);

    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addRethrow(unsigned, ControlType& data)
{
    LOG_INSTRUCTION("Rethrow", exception(data));

    ++m_callSiteIndex;
    bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();
    if (mayHaveExceptionHandlers) {
        m_jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        flushRegisters();
    }
    emitMove(this->exception(data), Location::fromGPR2(GPRInfo::argumentGPR3, GPRInfo::argumentGPR2));
    m_jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    emitThrowRefImpl(m_jit);
    return { };
}

BBQJIT::BranchFoldResult BBQJIT::tryFoldFusedBranchCompare(OpType, ExpressionType)
{
    RELEASE_ASSERT_NOT_REACHED();
}

BBQJIT::Jump BBQJIT::emitFusedBranchCompareBranch(OpType, ExpressionType, Location)
{
    RELEASE_ASSERT_NOT_REACHED();
}

BBQJIT::BranchFoldResult BBQJIT::tryFoldFusedBranchCompare(OpType, ExpressionType, ExpressionType)
{
    RELEASE_ASSERT_NOT_REACHED();
}

BBQJIT::Jump BBQJIT::emitFusedBranchCompareBranch(OpType, ExpressionType, Location, ExpressionType, Location)
{
    RELEASE_ASSERT_NOT_REACHED();
}

PartialResult BBQJIT::addFusedBranchCompare(OpType, ControlType&, ExpressionType, Stack&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

PartialResult BBQJIT::addFusedBranchCompare(OpType, ControlType&, ExpressionType, ExpressionType, Stack&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

PartialResult BBQJIT::addFusedIfCompare(OpType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

PartialResult BBQJIT::addFusedIfCompare(OpType, ExpressionType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&)
{
    RELEASE_ASSERT_NOT_REACHED();
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

    LOG_INSTRUCTION("BrOnNull/NonNull", reference);

    if (reference.isConst()) {
        // If we didn't exit early, the branch must be always-taken.
        currentControlData().flushAndSingleExit(*this, data, returnValues, false, false);
        data.addBranch(m_jit.jump());
    } else {
        ASSERT(referenceLocation.isGPR2());
        currentControlData().flushAtBlockBoundary(*this, 0, returnValues, false);
        Jump ifNotTaken = m_jit.branch32(shouldNegate ? CCallHelpers::Equal : CCallHelpers::NotEqual, referenceLocation.asGPRhi(), TrustedImm32(JSValue::NullTag));
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
        Value tempReference = Value::pinned(TypeKind::Ref, Location::fromGPR2(wasmScratchGPR2, wasmScratchGPR));
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
    // On armv7 account for misalignment due to of saved {FP, PC}
    constexpr int misalignment = 4 + 4;
    return WTF::roundUpToMultipleOf<stackAlignmentBytes()>(frameSize + misalignment) - misalignment;
}

void BBQJIT::restoreWebAssemblyGlobalState()
{
    restoreWebAssemblyContextInstance();
}

void BBQJIT::restoreWebAssemblyGlobalStateAfterWasmCall()
{
    restoreWebAssemblyGlobalState();
}

// SIMD

void BBQJIT::notifyFunctionUsesSIMD()
{
    m_usesSIMD = false;
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDLoad(ExpressionType, uint32_t, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDStore(ExpressionType, ExpressionType, uint32_t)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

void BBQJIT::materializeVectorConstant(v128_t, Location)
{
    UNREACHABLE_FOR_PLATFORM();
}

ExpressionType BBQJIT::addConstant(v128_t)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addExtractLane(SIMDInfo, uint8_t, Value, Value&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    UNREACHABLE_FOR_PLATFORM();
    return { };

}

void BBQJIT::emitStoreConst(Value constant, Location loc)
{
    LOG_INSTRUCTION("Store", constant, RESULT(loc));

    ASSERT(constant.isConst());
    ASSERT(loc.isMemory());

    switch (constant.type()) {
    case TypeKind::I32:
    case TypeKind::F32:
        m_jit.store32(Imm32(constant.asI32()), loc.asAddress());
        break;
    case TypeKind::Ref:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::RefNull:
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        m_jit.storePair32(TrustedImm32(constant.asI64lo()), TrustedImm32(constant.asI64hi()), loc.asAddress());
        break;
    case TypeKind::I64:
    case TypeKind::F64:
        m_jit.storePair32(TrustedImm32(constant.asI64lo()), TrustedImm32(constant.asI64hi()), loc.asAddress());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
        break;
    }
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
        m_jit.move(Imm32(constant.asI64hi()), loc.asGPRhi());
        m_jit.move(Imm32(constant.asI64lo()), loc.asGPRlo());
        break;
    case TypeKind::Ref:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::RefNull:
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        m_jit.move(Imm32(constant.asI64hi()), loc.asGPRhi());
        m_jit.move(Imm32(constant.asI64lo()), loc.asGPRlo());
        break;
    case TypeKind::F32:
        m_jit.move(Imm32(constant.asI32()), wasmScratchGPR);
        m_jit.move32ToFloat(wasmScratchGPR, loc.asFPR());
        break;
    case TypeKind::F64:
        m_jit.move(Imm32(constant.asI64hi()), wasmScratchGPR2);
        m_jit.move(Imm32(constant.asI64lo()), wasmScratchGPR);
        m_jit.move64ToDouble(wasmScratchGPR2, wasmScratchGPR, loc.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
        break;
    }
}

void BBQJIT::emitStore(TypeKind type, Location src, Location dst)
{
    ASSERT(dst.isMemory());
    ASSERT(src.isRegister());

    switch (type) {
    case TypeKind::I32:
        m_jit.store32(src.asGPR(), dst.asAddress());
        break;
    case TypeKind::I31ref:
        m_jit.store32(src.asGPRlo(), dst.asAddress());
        m_jit.store32(TrustedImm32(JSValue::Int32Tag), dst.asAddress().withOffset(4));
        break;
    case TypeKind::I64:
        m_jit.storePair32(src.asGPRlo(), src.asGPRhi(), dst.asAddress());
        break;
    case TypeKind::F32:
        m_jit.storeFloat(src.asFPR(), dst.asAddress());
        break;
    case TypeKind::F64:
        m_jit.storeDouble(src.asFPR(), dst.asAddress());
        break;
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        m_jit.storePair32(src.asGPRlo(), src.asGPRhi(), dst.asAddress());
        break;
    case TypeKind::V128:
        m_jit.storeVector(src.asFPR(), dst.asAddress());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind store.");
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
        m_jit.transfer32(src.asAddress(), dst.asAddress());
        break;
    case TypeKind::I31ref:
        m_jit.transfer32(src.asAddress(), dst.asAddress());
        m_jit.store32(TrustedImm32(JSValue::Int32Tag), dst.asAddress().withOffset(4));
        break;
    case TypeKind::F32:
        m_jit.transfer32(src.asAddress(), dst.asAddress());
        break;
    case TypeKind::I64:
        m_jit.transfer32(src.asAddress().withOffset(0), dst.asAddress().withOffset(0));
        m_jit.transfer32(src.asAddress().withOffset(4), dst.asAddress().withOffset(4));
        break;
    case TypeKind::F64:
        m_jit.transferDouble(src.asAddress(), dst.asAddress());
        break;
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Funcref:
    case TypeKind::Structref:
    case TypeKind::Arrayref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        m_jit.transfer32(src.asAddress().withOffset(0), dst.asAddress().withOffset(0));
        m_jit.transfer32(src.asAddress().withOffset(4), dst.asAddress().withOffset(4));
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
        m_jit.move(src.asGPR(), dst.asGPR());
        break;
    case TypeKind::I31ref:
    case TypeKind::I64:
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        if (dst.asGPRlo() == src.asGPRhi()) {
            ASSERT(dst.asGPRhi() != src.asGPRlo());
            m_jit.move(src.asGPRhi(), dst.asGPRhi());
            m_jit.move(src.asGPRlo(), dst.asGPRlo());
            break;
        }
        m_jit.move(src.asGPRlo(), dst.asGPRlo());
        m_jit.move(src.asGPRhi(), dst.asGPRhi());
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
    case TypeKind::I31ref:
    case TypeKind::I64:
        m_jit.loadPair32(src.asAddress(), dst.asGPRlo(), dst.asGPRhi());
        break;
    case TypeKind::F32:
        m_jit.loadFloat(src.asAddress(), dst.asFPR());
        break;
    case TypeKind::F64:
        m_jit.loadDouble(src.asAddress(), dst.asFPR());
        break;
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Exn:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
        m_jit.loadPair32(src.asAddress(), dst.asGPRlo(), dst.asGPRhi());
        break;
    case TypeKind::V128:
        m_jit.loadVector(src.asAddress(), dst.asFPR());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind load.");
    }
}

Location BBQJIT::allocateRegisterPair()
{
    GPRReg hi, lo;

    do {
        // we loop here until we can get _two_ register from m_gprSet
        // this wouldn't be necessary except that evictGPR modifies m_gprSet and nextGPR _doesn't_

        if (m_gprSet.isEmpty()) {
            evictGPR();
            continue;
        }

        auto iter = m_gprSet.begin();
        ASSERT(iter != m_gprSet.end());
        hi = (*iter).gpr();
        ++iter;
        if (iter == m_gprSet.end()) {
            m_gprLRU.lock(hi);
            evictGPR();
            m_gprLRU.unlock(hi);
            continue;
        }
        lo = (*iter).gpr();

        return Location::fromGPR2(hi, lo);
    } while (1);
}

PartialResult WARN_UNUSED_RETURN BBQJIT::addCallRef(const TypeDefinition& originalSignature, ArgumentList& args, ResultList& results, CallType callType)
{
    Value callee = args.takeLast();
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    CallInformation callInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    GPRReg calleePtr;
    GPRReg calleeInstance;
    GPRReg calleeCode;
    {
        ScratchScope<1, 0> calleeCodeScratch(*this, RegisterSetBuilder::argumentGPRs());
        calleeCode = calleeCodeScratch.gpr(0);
        calleeCodeScratch.unbindPreserved();

        ScratchScope<2, 0> otherScratches(*this);

        Location calleeLocation;
        if (callee.isConst()) {
            ASSERT(callee.asI64() == JSValue::encode(jsNull()));
            // This is going to throw anyway. It's suboptimial but probably won't happen in practice anyway.
            emitMoveConst(callee, calleeLocation = Location::fromGPR2(otherScratches.gpr(1), otherScratches.gpr(0)));
        } else
            calleeLocation = loadIfNecessary(callee);
        emitThrowOnNullReference(ExceptionType::NullReference, calleeLocation);

        calleePtr = calleeLocation.asGPRlo();
        calleeInstance = otherScratches.gpr(1);

        {
            auto calleeTmp = calleeInstance;
            m_jit.loadPtr(Address(calleePtr, WebAssemblyFunctionBase::offsetOfBoxedWasmCalleeLoadLocation()), calleeTmp);
            m_jit.loadPtr(Address(calleeTmp), calleeTmp);
            m_jit.storeWasmCalleeCallee(calleeTmp);
        }

        m_jit.loadPtr(MacroAssembler::Address(calleePtr, WebAssemblyFunctionBase::offsetOfInstance()), calleeInstance);
        m_jit.loadPtr(MacroAssembler::Address(calleePtr, WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation()), calleeCode);
    }

    if (callType == CallType::Call)
        emitIndirectCall("CallRef", callee, calleeInstance, calleeCode, signature, args, results);
    else
        emitIndirectTailCall("ReturnCallRef", callee, calleeInstance, calleeCode, signature, args);
    return { };
}

} } } // namespace JSC::Wasm::BBQJITImpl

#endif // USE(JSVALUE32_64)
#endif // ENABLE(WEBASSEMBLY_BBQJIT)
