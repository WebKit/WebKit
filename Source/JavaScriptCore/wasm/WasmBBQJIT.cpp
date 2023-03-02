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
#include "WasmBBQJIT.h"

#include "BytecodeStructs.h"
#include "WasmCallingConvention.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionParser.h"
#include "WasmMemoryInformation.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmOps.h"
#include "WasmTypeDefinition.h"
#include "assembler/AbstractMacroAssembler.h"
#include "assembler/AssemblerBuffer.h"
#include "assembler/CPU.h"
#include "assembler/MacroAssembler.h"
#include "b3/B3Common.h"
#include "b3/B3ValueRep.h"
#include "bytecompiler/RegisterID.h"
#include "jit/CCallHelpers.h"
#include "jit/GPRInfo.h"
#include "jit/RegisterSet.h"
#include "runtime/JSCast.h"
#include "tools/CompilerTimingScope.h"
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/SmallSet.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(WEBASSEMBLY_BBQJIT)

namespace JSC { namespace Wasm {

class BBQJIT {
public:
    using RegisterID = MacroAssembler::RegisterID;
    using FPRegisterID = MacroAssembler::FPRegisterID;
    using ErrorType = String;
    using PartialResult = Expected<void, ErrorType>;
    using Address = MacroAssembler::Address;
    using Imm32 = MacroAssembler::Imm32;
    using Imm64 = MacroAssembler::Imm64;
    using TrustedImm32 = MacroAssembler::TrustedImm32;
    using TrustedImm64 = MacroAssembler::TrustedImm64;
    using TrustedImmPtr = MacroAssembler::TrustedImmPtr;
    using RelationalCondition = MacroAssembler::RelationalCondition;
    using ResultCondition = MacroAssembler::ResultCondition;
    using DoubleCondition = MacroAssembler::DoubleCondition;
    using StatusCondition = MacroAssembler::StatusCondition;
    using Jump = MacroAssembler::Jump;
    using JumpList = MacroAssembler::JumpList;
    using DataLabelPtr = MacroAssembler::DataLabelPtr;

    // Functions can have up to 1000000 instructions, so 32 bits is a sensible maximum number of stack items or locals.
    using LocalOrTempIndex = uint32_t;

    static constexpr unsigned LocalIndexBits = 21;
    static_assert(maxFunctionLocals < 1 << LocalIndexBits);

private:
    struct Location {
        enum Kind : uint8_t {
            None = 0,
            Stack = 1,
            Gpr = 2,
            Fpr = 3,
            Global = 4,
            StackArgument = 5
        };

        Location()
            : m_kind(None)
        { }

        static Location none()
        {
            return Location();
        }

        static Location fromStack(int32_t stackOffset)
        {
            Location loc;
            loc.m_kind = Stack;
            loc.m_offset = stackOffset;
            return loc;
        }

        static Location fromStackArgument(int32_t stackOffset)
        {
            Location loc;
            loc.m_kind = StackArgument;
            loc.m_offset = stackOffset;
            return loc;
        }

        static Location fromGPR(RegisterID gpr)
        {
            Location loc;
            loc.m_kind = Gpr;
            loc.m_gpr = gpr;
            return loc;
        }

        static Location fromFPR(FPRegisterID fpr)
        {
            Location loc;
            loc.m_kind = Fpr;
            loc.m_fpr = fpr;
            return loc;
        }

        static Location fromGlobal(int32_t globalOffset)
        {
            Location loc;
            loc.m_kind = Global;
            loc.m_offset = globalOffset;
            return loc;
        }

        static Location fromArgumentLocation(ArgumentLocation argLocation)
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
        }

        bool isNone() const
        {
            return m_kind == None;
        }

        bool isGPR() const
        {
            return m_kind == Gpr;
        }

        bool isFPR() const
        {
            return m_kind == Fpr;
        }

        bool isRegister() const
        {
            return isGPR() || isFPR();
        }

        bool isStack() const
        {
            return m_kind == Stack;
        }

        bool isStackArgument() const
        {
            return m_kind == StackArgument;
        }

        bool isGlobal() const
        {
            return m_kind == Global;
        }

        bool isMemory() const
        {
            return isStack() || isStackArgument() || isGlobal();
        }

        int32_t asStackOffset() const
        {
            ASSERT(isStack());
            return m_offset;
        }

        Address asStackAddress() const
        {
            ASSERT(isStack());
            return Address(GPRInfo::callFrameRegister, asStackOffset());
        }

        int32_t asGlobalOffset() const
        {
            ASSERT(isGlobal());
            return m_offset;
        }

        Address asGlobalAddress() const
        {
            ASSERT(isGlobal());
            return Address(GPRInfo::wasmContextInstancePointer, asGlobalOffset());
        }

        int32_t asStackArgumentOffset() const
        {
            ASSERT(isStackArgument());
            return m_offset;
        }

        Address asStackArgumentAddress() const
        {
            ASSERT(isStackArgument());
            return Address(MacroAssembler::stackPointerRegister, asStackArgumentOffset());
        }

        Address asAddress() const
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

        RegisterID asGPR() const
        {
            ASSERT(isGPR());
            return m_gpr;
        }

        FPRegisterID asFPR() const
        {
            ASSERT(isFPR());
            return m_fpr;
        }

        void dump(PrintStream& out) const
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
            }
        }

        bool operator==(Location other) const
        {
            if (m_kind != other.m_kind)
                return false;
            switch (m_kind) {
            case Gpr:
                return m_gpr == other.m_gpr;
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

        bool operator!=(Location other) const
        {
            return !(*this == other);
        }

        Kind kind() const
        {
            return Kind(m_kind);
        }
    private:
        union {
            // It's useful to we be able to cram a location into a 4-byte space, so that
            // we can store them efficiently in ControlData.
            struct {
                unsigned m_kind : 3;
                int32_t m_offset : 29;
            };
            struct {
                Kind m_padGpr;
                RegisterID m_gpr;
            };
            struct {
                Kind m_padFpr;
                FPRegisterID m_fpr;
            };
        };
    };

    static_assert(sizeof(Location) == 4);

    static bool isFloatingPointType(TypeKind type)
    {
        return type == TypeKind::F32 || type == TypeKind::F64 || type == TypeKind::V128;
    }

public:
    class Value {
    public:
        // Represents the location in which this value is stored.
        enum Kind : uint8_t {
            None = 0,
            Const = 1,
            Temp = 2,
            Local = 3,
            Pinned = 4 // Used if we need to represent a Location as a Value, mostly in operation calls
        };

        ALWAYS_INLINE bool isNone() const
        {
            return m_kind == None;
        }

        ALWAYS_INLINE bool isTemp() const
        {
            return m_kind == Temp;
        }

        ALWAYS_INLINE bool isLocal() const
        {
            return m_kind == Local;
        }

        ALWAYS_INLINE bool isConst() const
        {
            return m_kind == Const;
        }

        ALWAYS_INLINE bool isPinned() const
        {
            return m_kind == Pinned;
        }

        ALWAYS_INLINE Kind kind() const
        {
            return m_kind;
        }

        ALWAYS_INLINE int32_t asI32() const
        {
            ASSERT(m_kind == Const);
            return m_i32;
        }

        ALWAYS_INLINE int64_t asI64() const
        {
            ASSERT(m_kind == Const);
            return m_i64;
        }

        ALWAYS_INLINE float asF32() const
        {
            ASSERT(m_kind == Const);
            return m_f32;
        }

        ALWAYS_INLINE double asF64() const
        {
            ASSERT(m_kind == Const);
            return m_f64;
        }

        ALWAYS_INLINE EncodedJSValue asRef() const
        {
            ASSERT(m_kind == Const);
            return m_ref;
        }

        ALWAYS_INLINE LocalOrTempIndex asTemp() const
        {
            ASSERT(m_kind == Temp);
            return m_index;
        }

        ALWAYS_INLINE LocalOrTempIndex asLocal() const
        {
            ASSERT(m_kind == Local);
            return m_index;
        }

        ALWAYS_INLINE Location asPinned() const
        {
            ASSERT(m_kind == Pinned);
            return m_pinned;
        }

        ALWAYS_INLINE static Value none()
        {
            Value val;
            val.m_kind = None;
            return val;
        }

        ALWAYS_INLINE static Value fromI32(int32_t immediate)
        {
            Value val;
            val.m_kind = Const;
            val.m_type = TypeKind::I32;
            val.m_i32 = immediate;
            return val;
        }

        ALWAYS_INLINE static Value fromI64(int64_t immediate)
        {
            Value val;
            val.m_kind = Const;
            val.m_type = TypeKind::I64;
            val.m_i64 = immediate;
            return val;
        }

        ALWAYS_INLINE static Value fromF32(float immediate)
        {
            Value val;
            val.m_kind = Const;
            val.m_type = TypeKind::F32;
            val.m_f32 = immediate;
            return val;
        }

        ALWAYS_INLINE static Value fromF64(double immediate)
        {
            Value val;
            val.m_kind = Const;
            val.m_type = TypeKind::F64;
            val.m_f64 = immediate;
            return val;
        }

        ALWAYS_INLINE static Value fromRef(TypeKind refType, EncodedJSValue ref)
        {
            Value val;
            val.m_kind = Const;
            val.m_type = refType;
            val.m_ref = ref;
            return val;
        }

        ALWAYS_INLINE static Value fromTemp(TypeKind type, LocalOrTempIndex temp)
        {
            Value val;
            val.m_kind = Temp;
            val.m_type = type;
            val.m_index = temp;
            return val;
        }

        ALWAYS_INLINE static Value fromLocal(TypeKind type, LocalOrTempIndex local)
        {
            Value val;
            val.m_kind = Local;
            val.m_type = type;
            val.m_index = local;
            return val;
        }

        ALWAYS_INLINE static Value pinned(TypeKind type, Location location)
        {
            Value val;
            val.m_kind = Pinned;
            val.m_type = type;
            val.m_pinned = location;
            return val;
        }

        ALWAYS_INLINE Value()
            : m_kind(None)
        { }

        ALWAYS_INLINE uint32_t size() const
        {
            switch (m_type) {
            case TypeKind::I32:
            case TypeKind::F32:
            case TypeKind::I31ref:
                return 4;
            case TypeKind::I64:
            case TypeKind::F64:
                return 8;
            case TypeKind::V128:
                return 16;
            case TypeKind::Func:
            case TypeKind::Funcref:
            case TypeKind::Ref:
            case TypeKind::RefNull:
            case TypeKind::Rec:
            case TypeKind::Sub:
            case TypeKind::Struct:
            case TypeKind::Structref:
            case TypeKind::Externref:
            case TypeKind::Array:
            case TypeKind::Arrayref:
                return sizeof(EncodedJSValue);
            case TypeKind::Void:
                return 0;
            }
        }

        ALWAYS_INLINE bool isFloat() const
        {
            return isFloatingPointType(m_type);
        }

        ALWAYS_INLINE TypeKind type() const
        {
            return m_type;
        }

        void dump(PrintStream& out) const
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
    private:
        union {
            int32_t m_i32;
            int64_t m_i64;
            float m_f32;
            double m_f64;
            LocalOrTempIndex m_index;
            Location m_pinned;
            EncodedJSValue m_ref;
        };

        Kind m_kind;
        TypeKind m_type;
    };

private:
    struct RegisterBinding {
        enum Kind : uint8_t {
            None = 0,
            Local = 1,
            Temp = 2,
            Scratch = 3, // Denotes a register bound for use as a scratch, not as a local or temp's location.
        };
        union {
            uint32_t m_uintValue;
            struct {
                TypeKind m_type;
                unsigned m_kind : 3;
                unsigned m_index : LocalIndexBits;
            };
        };

        RegisterBinding()
            : m_uintValue(0)
        { }

        RegisterBinding(uint32_t uintValue)
            : m_uintValue(uintValue)
        { }

        static RegisterBinding fromValue(Value value)
        {
            ASSERT(value.isLocal() || value.isTemp());
            RegisterBinding binding;
            binding.m_type = value.type();
            binding.m_kind = value.isLocal() ? Local : Temp;
            binding.m_index = value.isLocal() ? value.asLocal() : value.asTemp();
            return binding;
        }

        static RegisterBinding none()
        {
            return RegisterBinding();
        }

        static RegisterBinding scratch()
        {
            RegisterBinding binding;
            binding.m_kind = Scratch;
            return binding;
        }

        Value toValue() const
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

        bool isNone() const
        {
            return m_kind == None;
        }

        bool isValid() const
        {
            return m_kind != None;
        }

        bool isScratch() const
        {
            return m_kind == Scratch;
        }

        bool operator==(RegisterBinding other) const
        {
            if (m_kind != other.m_kind)
                return false;

            return m_kind == None || (m_index == other.m_index && m_type == other.m_type);
        }

        void dump(PrintStream& out) const
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

        unsigned hash() const
        {
            return pairIntHash(static_cast<unsigned>(m_kind), m_index);
        }

        uint32_t encode() const
        {
            return m_uintValue;
        }
    };

public:
    struct ControlData {
        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isTry(const ControlData& control) { return control.blockType() == BlockType::Try; }
        static bool isAnyCatch(const ControlData& control) { return control.blockType() == BlockType::Catch; }
        static bool isCatch(const ControlData& control) { return isAnyCatch(control) && control.catchKind() == CatchKind::Catch; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }
        static bool isLoop(const ControlData& control) { return control.blockType() == BlockType::Loop; }
        static bool isBlock(const ControlData& control) { return control.blockType() == BlockType::Block; }

        ControlData()
            : m_enclosedHeight(0)
        { }

        ControlData(BBQJIT& generator, BlockType blockType, BlockSignature signature, LocalOrTempIndex enclosedHeight)
            : m_signature(signature)
            , m_blockType(blockType)
            , m_enclosedHeight(enclosedHeight)
        {
            if (blockType == BlockType::TopLevel) {
                // Abide by calling convention instead.
                CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*signature, CallRole::Callee);
                for (unsigned i = 0; i < signature->as<FunctionSignature>()->argumentCount(); ++i)
                    m_arguments.append(Location::fromArgumentLocation(wasmCallInfo.params[i]));
                for (unsigned i = 0; i < signature->as<FunctionSignature>()->returnCount(); ++i)
                    m_results.append(Location::fromArgumentLocation(wasmCallInfo.results[i]));
                return;
            }
            const auto& functionSignature = signature->as<FunctionSignature>();
            auto gprSetCopy = generator.m_validGPRs;
            auto fprSetCopy = generator.m_validFPRs;
            for (unsigned i = 0; i < functionSignature->argumentCount(); ++i)
                m_arguments.append(allocateArgumentOrResult(generator, functionSignature->argumentType(i).kind, i, gprSetCopy, fprSetCopy));
            gprSetCopy = generator.m_validGPRs;
            fprSetCopy = generator.m_validFPRs;
            for (unsigned i = 0; i < functionSignature->returnCount(); ++i)
                m_results.append(allocateArgumentOrResult(generator, functionSignature->returnType(i).kind, i, gprSetCopy, fprSetCopy));
        }

        Location allocateArgumentOrResult(BBQJIT& generator, TypeKind type, unsigned i, RegisterSet& remainingGPRs, RegisterSet& remainingFPRs)
        {
            switch (type) {
            case TypeKind::V128:
            case TypeKind::F32:
            case TypeKind::F64: {
                if (remainingFPRs.isEmpty())
                    return generator.canonicalSlot(Value::fromTemp(type, m_enclosedHeight + i));
                auto reg = *remainingFPRs.begin();
                remainingFPRs.remove(reg);
                return Location::fromFPR(reg.fpr());
            }
            default:
                if (remainingGPRs.isEmpty())
                    return generator.canonicalSlot(Value::fromTemp(type, m_enclosedHeight + i));
                auto reg = *remainingGPRs.begin();
                remainingGPRs.remove(reg);
                return Location::fromGPR(reg.gpr());
            }
        }

        template<typename Stack>
        void endBlock(BBQJIT& generator, ControlData& target, Stack& expressionStack, bool isChildBlock, bool endOfWasmBlock)
        {
            unsigned targetArity = isChildBlock ? target.argumentLocations().size() : target.branchTargetArity();

            // First, we flush all locals that were allocated outside of their designated slots in this block.
            for (unsigned i = 0; i < expressionStack.size(); ++i) {
                if (expressionStack[i].value().isLocal())
                    m_touchedLocals.add(expressionStack[i].value().asLocal());
            }
            for (LocalOrTempIndex touchedLocal : m_touchedLocals) {
                Value value = Value::fromLocal(generator.m_localTypes[touchedLocal], touchedLocal);
                if (generator.locationOf(value).isRegister())
                    generator.flushValue(value);
            }

            for (unsigned i = 0; i < expressionStack.size(); ++i) {
                Value& value = expressionStack[i].value();
                int resultIndex = static_cast<int>(i) - static_cast<int>(expressionStack.size() - targetArity);

                // Next, we turn all constants into temporaries, so they can be given persistent slots on the stack.
                // If this is the end of the enclosing wasm block, we know we won't need them again, so this can be skipped.
                if (value.isConst() && (resultIndex < 0 || !endOfWasmBlock)) {
                    Value constant = value;
                    value = Value::fromTemp(value.type(), static_cast<LocalOrTempIndex>(m_enclosedHeight + i));
                    Location slot = generator.locationOf(value);
                    generator.emitMoveConst(constant, slot);
                }

                // Next, we flush or consume all the temp values on the stack.
                if (value.isTemp()) {
                    if (!endOfWasmBlock)
                        generator.flushValue(value);
                    else if (resultIndex < 0)
                        generator.consume(value);
                }
            }

            unsigned offset = expressionStack.size() - targetArity;

            // Finally, we move all passed temporaries to the successor, in its argument slots.
            auto& targetLocations = target.blockType() == BlockType::Loop || isChildBlock
                ? target.argumentLocations()
                : target.resultLocations();
            if (targetArity == 1) {
                Value value = expressionStack[offset].value();
                generator.emitMove(value, targetLocations[0]);
                generator.consume(value); // We consider a value consumed when we pass it to a successor.
            } else {
                Vector<Value, 8> resultValues;
                Vector<Location, 8> resultLocations;
                for (unsigned i = 0; i < targetArity; ++i) {
                    resultValues.append(expressionStack[i + offset].value());
                    resultLocations.append(targetLocations[i]);
                }
                generator.emitShuffle(resultValues, resultLocations);
                for (const Value& value : resultValues)
                    generator.consume(value);
            }

            // As one final step, we convert any constants into temporaries on the stack, so we don't blindly assume they have
            // the same constant values in the successor.
            for (unsigned i = 0; i < targetArity; ++i) {
                Value& value = expressionStack[i + offset].value();
                if (value.isConst())
                    value = Value::fromTemp(value.type(), static_cast<LocalOrTempIndex>(m_enclosedHeight + i));
            }
        }

        template<typename Stack>
        void startBlock(BBQJIT& generator, Stack& expressionStack)
        {
            ASSERT(expressionStack.size() >= m_arguments.size());

            for (unsigned i = 0; i < m_arguments.size(); ++i) {
                ASSERT(!expressionStack[i + expressionStack.size() - m_arguments.size()].value().isConst());
                generator.bind(expressionStack[i].value(), m_arguments[i]);
            }
        }

        template<typename Stack>
        void resumeBlock(BBQJIT& generator, const ControlData& predecessor, Stack& expressionStack)
        {
            ASSERT(expressionStack.size() >= predecessor.resultLocations().size());

            for (unsigned i = 0; i < predecessor.resultLocations().size(); ++i) {
                unsigned offset = expressionStack.size() - predecessor.resultLocations().size();
                expressionStack[i + offset].value() = Value::fromTemp(expressionStack[i].type().kind, predecessor.enclosedHeight() + i);
                generator.bind(expressionStack[i + offset].value(), predecessor.resultLocations()[i]);
            }
        }

        void convertIfToBlock()
        {
            ASSERT(m_blockType == BlockType::If);
            m_blockType = BlockType::Block;
        }

        void convertLoopToBlock()
        {
            ASSERT(m_blockType == BlockType::Loop);
            m_blockType = BlockType::Block;
        }

        void addBranch(Jump jump)
        {
            m_branchList.append(jump);
        }

        void linkJumps(MacroAssembler::AbstractMacroAssemblerType* masm)
        {
            m_branchList.link(masm);
        }

        void linkJumpsTo(MacroAssembler::Label label, MacroAssembler::AbstractMacroAssemblerType* masm)
        {
            m_branchList.linkTo(label, masm);
        }

        void linkIfBranch(MacroAssembler::AbstractMacroAssemblerType* masm)
        {
            ASSERT(m_blockType == BlockType::If);
            if (m_ifBranch.isSet())
                m_ifBranch.link(masm);
        }

        void dump(PrintStream& out) const
        {
            UNUSED_PARAM(out);
        }

        LocalOrTempIndex enclosedHeight() const
        {
            return m_enclosedHeight;
        }

        const Vector<Location, 2>& argumentLocations() const
        {
            return m_arguments;
        }

        const Vector<Location, 2>& resultLocations() const
        {
            return m_results;
        }

        BlockType blockType() const { return m_blockType; }
        BlockSignature signature() const { return m_signature; }

        FunctionArgCount branchTargetArity() const
        {
            if (blockType() == BlockType::Loop)
                return m_signature->as<FunctionSignature>()->argumentCount();
            return m_signature->as<FunctionSignature>()->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (m_blockType == BlockType::Loop)
                return m_signature->as<FunctionSignature>()->argumentType(i);
            return m_signature->as<FunctionSignature>()->returnType(i);
        }

        CatchKind catchKind() const
        {
            ASSERT(m_blockType == BlockType::Catch);
            return m_catchKind;
        }

        void setIfBranch(MacroAssembler::Jump branch)
        {
            ASSERT(m_blockType == BlockType::If);
            m_ifBranch = branch;
        }

        void setLoopLabel(MacroAssembler::Label label)
        {
            ASSERT(m_blockType == BlockType::Loop);
            m_loopLabel = label;
        }

        const MacroAssembler::Label& loopLabel() const
        {
            return m_loopLabel;
        }

        void touch(LocalOrTempIndex local)
        {
            m_touchedLocals.add(local);
        }
    private:
        friend class BBQJIT;

        BlockSignature m_signature;
        BlockType m_blockType;
        CatchKind m_catchKind;
        Vector<Location, 2> m_arguments; // List of input locations to write values into when entering this block.
        Vector<Location, 2> m_results; // List of result locations to write values into when exiting this block.
        JumpList m_branchList; // List of branch control info for branches targeting the end of this block.
        MacroAssembler::Label m_loopLabel;
        MacroAssembler::Jump m_ifBranch;
        LocalOrTempIndex m_enclosedHeight; // Height of enclosed expression stack, used as the base for all temporary locations.
        BitVector m_touchedLocals; // Number of locals allocated to registers in this block.
    };

    friend struct ControlData;

    using ExpressionType = Value;
    using ControlType = ControlData;
    using CallType = CallLinkInfo::CallType;
    using ResultList = Vector<ExpressionType, 8>;
    using ControlEntry = typename FunctionParserTypes<ControlType, ExpressionType, CallType>::ControlEntry;
    using TypedExpression = typename FunctionParserTypes<ControlType, ExpressionType, CallType>::TypedExpression;
    using Stack = FunctionParser<BBQJIT>::Stack;
    using ControlStack = FunctionParser<BBQJIT>::ControlStack;

private:
    unsigned m_loggingIndent = 0;

    template<typename... Args>
    void logInstructionData(bool first, const Value& value, const Location& location, const Args&... args)
    {
        if (!first)
            dataLog(", ");

        dataLog(value);
        if (location.kind() != Location::None)
            dataLog(":", location);
        logInstructionData(false, args...);
    }

    template<typename... Args>
    void logInstructionData(bool first, const Value& value, const Args&... args)
    {
        if (!first)
            dataLog(", ");

        dataLog(value);
        if (!value.isConst() && !value.isPinned())
            dataLog(":", locationOf(value));
        logInstructionData(false, args...);
    }

    template<size_t N, typename OverflowHandler, typename... Args>
    void logInstructionData(bool first, const Vector<TypedExpression, N, OverflowHandler>& expressions, const Args&... args)
    {
        for (const TypedExpression& expression : expressions) {
            if (!first)
                dataLog(", ");

            const Value& value = expression.value();
            dataLog(value);
            if (!value.isConst() && !value.isPinned())
                dataLog(":", locationOf(value));
            first = false;
        }
        logInstructionData(false, args...);
    }

    template<size_t N, typename OverflowHandler, typename... Args>
    void logInstructionData(bool first, const Vector<Value, N, OverflowHandler>& values, const Args&... args)
    {
        for (const Value& value : values) {
            if (!first)
                dataLog(", ");

            dataLog(value);
            if (!value.isConst() && !value.isPinned())
                dataLog(":", locationOf(value));
            first = false;
        }
        logInstructionData(false, args...);
    }

    template<size_t N, typename OverflowHandler, typename... Args>
    void logInstructionData(bool first, const Vector<Location, N, OverflowHandler>& values, const Args&... args)
    {
        for (const Location& value : values) {
            if (!first)
                dataLog(", ");
            dataLog(value);
            first = false;
        }
        logInstructionData(false, args...);
    }

    inline void logInstructionData(bool)
    {
        dataLogLn();
    }

    template<typename... Data>
    ALWAYS_INLINE void logInstruction(const char* opcode, SIMDLaneOperation op, const Data&... data)
    {
        dataLog("BBQ\t");
        for (unsigned i = 0; i < m_loggingIndent; i ++)
            dataLog(" ");
        dataLog(opcode, SIMDLaneOperationDump(op), " ");
        logInstructionData(true, data...);
    }

    template<typename... Data>
    ALWAYS_INLINE void logInstruction(const char* opcode, const Data&... data)
    {
        dataLog("BBQ\t");
        for (unsigned i = 0; i < m_loggingIndent; i ++)
            dataLog(" ");
        dataLog(opcode, " ");
        logInstructionData(true, data...);
    }

    template<typename T>
    struct Result {
        T value;

        Result(const T& value_in)
            : value(value_in)
        { }
    };

    template<typename... Args>
    void logInstructionData(bool first, const char* const& literal, const Args&... args)
    {
        if (!first)
            dataLog(" ");

        dataLog(literal);
        if (!std::strcmp(literal, "=> "))
            logInstructionData(true, args...);
        else
            logInstructionData(false, args...);
    }

    template<typename T, typename... Args>
    void logInstructionData(bool first, const Result<T>& result, const Args&... args)
    {
        if (!first)
            dataLog(" ");

        dataLog("=> ");
        logInstructionData(true, result.value, args...);
    }

    template<typename T, typename... Args>
    void logInstructionData(bool first, const T& t, const Args&... args)
    {
        if (!first)
            dataLog(", ");
        dataLog(t);
        logInstructionData(false, args...);
    }

#define RESULT(...) Result { __VA_ARGS__ }
#define LOG_INSTRUCTION(...) do { if (UNLIKELY(Options::verboseBBQJITInstructions())) { logInstruction(__VA_ARGS__); } } while (false)
#define LOG_INDENT() do { if (UNLIKELY(Options::verboseBBQJITInstructions())) { m_loggingIndent += 2; } } while (false);
#define LOG_DEDENT() do { if (UNLIKELY(Options::verboseBBQJITInstructions())) { m_loggingIndent -= 2; } } while (false);

public:
    static constexpr bool tierSupportsSIMD = true;

    BBQJIT(CCallHelpers& jit, const TypeDefinition& signature, Callee& callee, const FunctionData& function, uint32_t functionIndex, const ModuleInformation& info, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, InternalFunction* compilation, TierUpCount* tierUp)
        : m_jit(jit)
        , m_callee(callee)
        , m_function(function)
        , m_functionSignature(signature.as<FunctionSignature>())
        , m_functionIndex(functionIndex)
        , m_info(info)
        , m_mode(mode)
        , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
        , m_tierUp(tierUp)
        , m_gprBindings(jit.numberOfRegisters(), RegisterBinding::none())
        , m_fprBindings(jit.numberOfFPRegisters(), RegisterBinding::none())
        , m_gprLRU(jit.numberOfRegisters())
        , m_fprLRU(jit.numberOfFPRegisters())
        , m_lastUseTimestamp(0)
        , m_compilation(compilation)
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
        // TODO: handle callee-saved registers better.
        RegisterSetBuilder::vmCalleeSaveRegisters().forEach([&](Reg reg) {
            fprSetBuilder.remove(reg);
        });

        RegisterSetBuilder callerSaveGprs = gprSetBuilder;
        RegisterSetBuilder callerSaveFprs = fprSetBuilder;

        gprSetBuilder.remove(m_scratchGPR);
        fprSetBuilder.remove(m_scratchFPR);

        m_gprSet = m_validGPRs = gprSetBuilder.buildAndValidate();
        m_fprSet = m_validFPRs = fprSetBuilder.buildAndValidate();
        m_callerSaveGPRs = callerSaveGprs.buildAndValidate();
        m_callerSaveFPRs = callerSaveFprs.buildAndValidate();
        m_callerSaves = callerSaveGprs.merge(callerSaveFprs).buildAndValidate();

        m_gprLRU.add(m_gprSet);
        m_fprLRU.add(m_fprSet);

        if ((Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tUsing GPR set: ", m_gprSet, "\n   \tFPR set: ", m_fprSet);

        CallInformation callInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);
        ASSERT(callInfo.params.size() == m_functionSignature->argumentCount());
        for (unsigned i = 0; i < m_functionSignature->argumentCount(); i ++) {
            const Type& type = m_functionSignature->argumentType(i);
            m_localSlots.append(allocateStack(Value::fromLocal(type.kind, i)));
            m_locals.append(Location::none());
            m_localTypes.append(type.kind);

            Value parameter = Value::fromLocal(type.kind, i);
            bind(parameter, Location::fromArgumentLocation(callInfo.params[i]));
            m_arguments.append(i);
        }
        m_localStorage = m_frameSize; // All stack slots allocated so far are locals.
    }

    ALWAYS_INLINE static Value emptyExpression()
    {
        return Value::none();
    }

    void setParser(FunctionParser<BBQJIT>* parser)
    {
        m_parser = parser;
    }

    bool addArguments(const TypeDefinition& signature)
    {
        RELEASE_ASSERT(m_arguments.size() == signature.as<FunctionSignature>()->argumentCount()); // We handle arguments in the prologue
        return true;
    }

    Value addConstant(Type type, uint64_t value)
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
        case TypeKind::Externref:
            result = Value::fromRef(type.kind, static_cast<EncodedJSValue>(value));
            LOG_INSTRUCTION("RefConst", makeString(type.kind), RESULT(result));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant type.\n");
            return Value::none();
        }
        return result;
    }

    PartialResult addDrop(Value value)
    {
        LOG_INSTRUCTION("Drop", value);
        consume(value);
        return { };
    }

    PartialResult addLocal(Type type, uint32_t numberOfLocals)
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

#define BBQ_STUB { RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented instruction!"); return PartialResult(); }
#define BBQ_CONTROL_STUB { RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented instruction!"); return ControlData(); }

    Value instanceValue()
    {
        return Value::pinned(TypeKind::I64, Location::fromGPR(GPRInfo::wasmContextInstancePointer));
    }

    // Tables
    PartialResult WARN_UNUSED_RETURN addTableGet(unsigned tableIndex, Value index, Value& result)
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
        emitCCall(&operationGetWasmTableElement, arguments, returnType, result);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("TableGet", tableIndex, index, RESULT(result));

        throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest64(ResultCondition::Zero, resultLocation.asGPR()));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTableSet(unsigned tableIndex, Value index, Value value)
    {
        // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
        ASSERT(index.type() == TypeKind::I32);

        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(tableIndex),
            index,
            value
        };

        Value shouldThrow;
        emitCCall(&operationSetWasmTableElement, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        LOG_INSTRUCTION("TableSet", tableIndex, index, value);

        throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length)
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
        Value shouldThrow;
        emitCCall(&operationWasmTableInit, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        LOG_INSTRUCTION("TableInit", tableIndex, dstOffset, srcOffset, length);

        throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addElemDrop(unsigned elementIndex)
    {
        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(elementIndex)
        };
        emitCCall(&operationWasmElemDrop, arguments);

        LOG_INSTRUCTION("ElemDrop", elementIndex);
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTableSize(unsigned tableIndex, Value& result)
    {
        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(tableIndex)
        };
        emitCCall(&operationGetWasmTableSize, arguments, TypeKind::I32, result);

        LOG_INSTRUCTION("TableSize", tableIndex, RESULT(result));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTableGrow(unsigned tableIndex, Value fill, Value delta, Value& result)
    {
        ASSERT(delta.type() == TypeKind::I32);

        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(tableIndex),
            fill, delta
        };
        emitCCall(&operationWasmTableGrow, arguments, TypeKind::I32, result);

        LOG_INSTRUCTION("TableGrow", tableIndex, fill, delta, RESULT(result));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTableFill(unsigned tableIndex, Value offset, Value fill, Value count)
    {
        ASSERT(offset.type() == TypeKind::I32);
        ASSERT(count.type() == TypeKind::I32);

        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(tableIndex),
            offset, fill, count
        };
        Value shouldThrow;
        emitCCall(&operationWasmTableFill, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        LOG_INSTRUCTION("TableFill", tableIndex, fill, offset, count);

        throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, Value dstOffset, Value srcOffset, Value length)
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
        Value shouldThrow;
        emitCCall(&operationWasmTableCopy, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        LOG_INSTRUCTION("TableCopy", dstTableIndex, srcTableIndex, dstOffset, srcOffset, length);

        throwExceptionIf(ExceptionType::OutOfBoundsTableAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));
        return { };
    }

    // Locals

    PartialResult WARN_UNUSED_RETURN getLocal(uint32_t localIndex, Value& result)
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

    PartialResult WARN_UNUSED_RETURN setLocal(uint32_t localIndex, Value value)
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

    // Globals

    LoadOpType loadOpForTypeKind(TypeKind type)
    {
        switch (type) {
        case TypeKind::I32:
        case TypeKind::I31ref:
            return LoadOpType::I32Load;
        case TypeKind::I64:
            return LoadOpType::I64Load;
        case TypeKind::F32:
            return LoadOpType::F32Load;
        case TypeKind::F64:
            return LoadOpType::F64Load;
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
        case TypeKind::Funcref:
        case TypeKind::Externref:
            return loadOpForTypeKind(Types::IPtr.kind);
        case TypeKind::V128:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Should be handling vectors separately (use V128Load, not a LoadOpType)");
            return LoadOpType::I64Load;
        case TypeKind::Void:
        case TypeKind::Rec:
        case TypeKind::Sub:
        case TypeKind::Array:
        case TypeKind::Struct:
        case TypeKind::Func:
            RELEASE_ASSERT_NOT_REACHED();
            return LoadOpType::I64Load;
        }
    }

    Value topValue(TypeKind type)
    {
        return Value::fromTemp(type, currentControlData().enclosedHeight() + m_parser->expressionStack().size());
    }

    PartialResult WARN_UNUSED_RETURN getGlobal(uint32_t index, Value& result)
    {
        const Wasm::GlobalInformation& global = m_info.globals[index];
        Type type = global.type;

        int32_t offset = Instance::offsetOfGlobalPtr(m_info.importFunctionCount(), m_info.tableCount(), index);
        Value globalValue = Value::pinned(type.kind, Location::fromGlobal(offset));

        switch (global.bindingMode) {
        case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
            result = topValue(type.kind);
            emitLoad(globalValue, loadIfNecessary(result));
            break;
        case Wasm::GlobalInformation::BindingMode::Portable:
            ASSERT(global.mutability == Wasm::Mutability::Mutable);
            m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, offset), m_scratchGPR);
            result = emitLoadOp(loadOpForTypeKind(type.kind), Location::fromGPR(m_scratchGPR), 0);
            break;
        }

        LOG_INSTRUCTION("GetGlobal", index, RESULT(result));

        return { };
    }

    StoreOpType storeOpForTypeKind(TypeKind type)
    {
        switch (type) {
        case TypeKind::I32:
        case TypeKind::I31ref:
            return StoreOpType::I32Store;
        case TypeKind::I64:
            return StoreOpType::I64Store;
        case TypeKind::F32:
            return StoreOpType::F32Store;
        case TypeKind::F64:
            return StoreOpType::F64Store;
        case TypeKind::Rec:
        case TypeKind::Ref:
        case TypeKind::Sub:
        case TypeKind::RefNull:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::Struct:
        case TypeKind::Structref:
        case TypeKind::Func:
        case TypeKind::Funcref:
        case TypeKind::Externref:
            return storeOpForTypeKind(Types::IPtr.kind);
        case TypeKind::V128:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Should be handling vectors separately (use V128Store, not a StoreOpType)");
            return StoreOpType::I64Store;
        case TypeKind::Void:
            RELEASE_ASSERT_NOT_REACHED();
            return StoreOpType::I64Store;
        }
    }

    void emitWriteBarrier(GPRReg cellGPR)
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

        m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfVM()), vmGPR);
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
        m_jit.callOperation(operationWasmWriteBarrierSlowPath);

        // Continuation
        noFenceCheck.link(&m_jit);
        belowBlackThreshold.link(&m_jit);
    }

    PartialResult WARN_UNUSED_RETURN setGlobal(uint32_t index, Value value)
    {
        const Wasm::GlobalInformation& global = m_info.globals[index];
        Type type = global.type;

        int32_t offset = Instance::offsetOfGlobalPtr(m_info.importFunctionCount(), m_info.tableCount(), index);

        switch (global.bindingMode) {
        case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
            emitMove(value, Location::fromGlobal(offset));
            if (isRefType(type)) {
                m_jit.load64(Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfOwner()), m_scratchGPR);
                emitWriteBarrier(m_scratchGPR);
            }
            break;
        }
        case Wasm::GlobalInformation::BindingMode::Portable: {
            ASSERT(global.mutability == Wasm::Mutability::Mutable);
            m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, offset), m_scratchGPR);
            emitStoreOp(storeOpForTypeKind(type.kind), Location::fromGPR(m_scratchGPR), value, 0);
            if (isRefType(type)) {
                m_jit.loadPtr(Address(m_scratchGPR, Wasm::Global::offsetOfOwner() - Wasm::Global::offsetOfValue()), m_scratchGPR);
                emitWriteBarrier(m_scratchGPR);
            }
            break;
        }
        }

        LOG_INSTRUCTION("SetGlobal", index, value);

        return { };
    }

    // Memory
    inline Location emitCheckAndPreparePointer(Value pointer, uint32_t uoffset, uint32_t sizeOfOperation)
    {
        ScratchScope<1, 0> scratches(*this);
        Location pointerLocation;
        if (pointer.isConst()) {
            pointerLocation = Location::fromGPR(scratches.gpr(0));
            emitMoveConst(pointer, pointerLocation);
        } else
            pointerLocation = loadIfNecessary(pointer);
        ASSERT(pointerLocation.isGPR());

        uint64_t boundary = static_cast<uint64_t>(sizeOfOperation) + uoffset - 1;
        switch (m_mode) {
        case MemoryMode::BoundsChecking: {
            // We're not using signal handling only when the memory is not shared.
            // Regardless of signaling, we must check that no memory access exceeds the current memory size.
            m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), m_scratchGPR);
            if (boundary)
                m_jit.add64(TrustedImm64(boundary), m_scratchGPR);
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch64(RelationalCondition::AboveOrEqual, m_scratchGPR, GPRInfo::wasmBoundsCheckingSizeRegister));
            break;
        }

        case MemoryMode::Signaling: {
            // We've virtually mapped 4GiB+redzone for this memory. Only the user-allocated pages are addressable, contiguously in range [0, current],
            // and everything above is mapped PROT_NONE. We don't need to perform any explicit bounds check in the 4GiB range because WebAssembly register
            // memory accesses are 32-bit. However WebAssembly register + offset accesses perform the addition in 64-bit which can push an access above
            // the 32-bit limit (the offset is unsigned 32-bit). The redzone will catch most small offsets, and we'll explicitly bounds check any
            // register + large offset access. We don't think this will be generated frequently.
            //
            // We could check that register + large offset doesn't exceed 4GiB+redzone since that's technically the limit we need to avoid overflowing the
            // PROT_NONE region, but it's better if we use a smaller immediate because it can codegens better. We know that anything equal to or greater
            // than the declared 'maximum' will trap, so we can compare against that number. If there was no declared 'maximum' then we still know that
            // any access equal to or greater than 4GiB will trap, no need to add the redzone.
            if (uoffset >= Memory::fastMappedRedzoneBytes()) {
                uint64_t maximum = m_info.memory.maximum() ? m_info.memory.maximum().bytes() : std::numeric_limits<uint32_t>::max();
                m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), m_scratchGPR);
                if (boundary)
                    m_jit.add64(TrustedImm64(boundary), m_scratchGPR);
                throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch64(RelationalCondition::AboveOrEqual, m_scratchGPR, TrustedImm64(static_cast<int64_t>(maximum))));
            }
            break;
        }
        }

#if CPU(ARM64)
        m_jit.addZeroExtend64(GPRInfo::wasmBaseMemoryPointer, pointerLocation.asGPR(), m_scratchGPR);
#else
        m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), m_scratchGPR);
        m_jit.addPtr(GPRInfo::wasmBaseMemoryPointer, m_scratchGPR);
#endif

        consume(pointer);
        return Location::fromGPR(m_scratchGPR);
    }

    static inline uint32_t sizeOfLoadOp(LoadOpType op)
    {
        switch (op) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load8U:
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
            return 1;
        case LoadOpType::I32Load16S:
        case LoadOpType::I64Load16S:
        case LoadOpType::I32Load16U:
        case LoadOpType::I64Load16U:
            return 2;
        case LoadOpType::I32Load:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load32U:
        case LoadOpType::F32Load:
            return 4;
        case LoadOpType::I64Load:
        case LoadOpType::F64Load:
            return 8;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    static inline TypeKind typeOfLoadOp(LoadOpType op)
    {
        switch (op) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load8U:
        case LoadOpType::I32Load16S:
        case LoadOpType::I32Load16U:
        case LoadOpType::I32Load:
            return TypeKind::I32;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load16U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load:
            return TypeKind::I64;
        case LoadOpType::F32Load:
            return TypeKind::F32;
        case LoadOpType::F64Load:
            return TypeKind::F64;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    Address materializePointer(Location pointerLocation, uint32_t uoffset)
    {
        if (static_cast<uint64_t>(uoffset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, uoffset, Width::Width128)) {
            m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointerLocation.asGPR());
            return Address(pointerLocation.asGPR());
        }
        return Address(pointerLocation.asGPR(), static_cast<int32_t>(uoffset));
    }

    Value WARN_UNUSED_RETURN emitLoadOp(LoadOpType loadOp, Location pointer, uint32_t uoffset)
    {
        ASSERT(pointer.isGPR());

        Address address = materializePointer(pointer, uoffset);
        Value result = topValue(typeOfLoadOp(loadOp));
        Location resultLocation = allocate(result);

        switch (loadOp) {
        case LoadOpType::I32Load8S:
            m_jit.load8SignedExtendTo32(address, resultLocation.asGPR());
            break;
        case LoadOpType::I64Load8S:
            m_jit.load8SignedExtendTo32(address, resultLocation.asGPR());
            m_jit.signExtend32ToPtr(resultLocation.asGPR(), resultLocation.asGPR());
            break;
        case LoadOpType::I32Load8U:
            m_jit.load8(address, resultLocation.asGPR());
            break;
        case LoadOpType::I64Load8U:
            m_jit.load8(address, resultLocation.asGPR());
            break;
        case LoadOpType::I32Load16S:
            m_jit.load16SignedExtendTo32(address, resultLocation.asGPR());
            break;
        case LoadOpType::I64Load16S:
            m_jit.load16SignedExtendTo32(address, resultLocation.asGPR());
            m_jit.signExtend32ToPtr(resultLocation.asGPR(), resultLocation.asGPR());
            break;
        case LoadOpType::I32Load16U:
            m_jit.load16(address, resultLocation.asGPR());
            break;
        case LoadOpType::I64Load16U:
            m_jit.load16(address, resultLocation.asGPR());
            break;
        case LoadOpType::I32Load:
            m_jit.load32(address, resultLocation.asGPR());
            break;
        case LoadOpType::I64Load32U:
            m_jit.load32(address, resultLocation.asGPR());
            break;
        case LoadOpType::I64Load32S:
            m_jit.load32(address, resultLocation.asGPR());
            m_jit.signExtend32ToPtr(resultLocation.asGPR(), resultLocation.asGPR());
            break;
        case LoadOpType::I64Load:
            m_jit.load64(address, resultLocation.asGPR());
            break;
        case LoadOpType::F32Load:
            m_jit.loadFloat(address, resultLocation.asFPR());
            break;
        case LoadOpType::F64Load:
            m_jit.loadDouble(address, resultLocation.asFPR());
            break;
        }

        return result;
    }

#define CREATE_OP_STRING(name, ...) #name,

    constexpr static const char* LOAD_OP_NAMES[14] = {
        "I32Load", "I64Load", "F32Load", "F64Load",
        "I32Load8S", "I32Load8U", "I32Load16S", "I32Load16U",
        "I64Load8S", "I64Load8U", "I64Load16S", "I64Load16U", "I64Load32S", "I64Load32U"
    };

    PartialResult WARN_UNUSED_RETURN load(LoadOpType loadOp, Value pointer, Value& result, uint32_t uoffset)
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
        } else
            result = emitLoadOp(loadOp, emitCheckAndPreparePointer(pointer, uoffset, sizeOfLoadOp(loadOp)), uoffset);

        LOG_INSTRUCTION(LOAD_OP_NAMES[(unsigned)loadOp - (unsigned)I32Load], pointer, uoffset, RESULT(result));

        return { };
    }

    inline uint32_t sizeOfStoreOp(StoreOpType op)
    {
        switch (op) {
        case StoreOpType::I32Store8:
        case StoreOpType::I64Store8:
            return 1;
        case StoreOpType::I32Store16:
        case StoreOpType::I64Store16:
            return 2;
        case StoreOpType::I32Store:
        case StoreOpType::I64Store32:
        case StoreOpType::F32Store:
            return 4;
        case StoreOpType::I64Store:
        case StoreOpType::F64Store:
            return 8;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    void emitStoreOp(StoreOpType storeOp, Location pointer, Value value, uint32_t uoffset)
    {
        ASSERT(pointer.isGPR());

        Address address = materializePointer(pointer, uoffset);
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

        switch (storeOp) {
        case StoreOpType::I64Store8:
        case StoreOpType::I32Store8:
            m_jit.store8(valueLocation.asGPR(), address);
            return;
        case StoreOpType::I64Store16:
        case StoreOpType::I32Store16:
            m_jit.store16(valueLocation.asGPR(), address);
            return;
        case StoreOpType::I64Store32:
        case StoreOpType::I32Store:
            m_jit.store32(valueLocation.asGPR(), address);
            return;
        case StoreOpType::I64Store:
            m_jit.store64(valueLocation.asGPR(), address);
            return;
        case StoreOpType::F32Store:
            m_jit.storeFloat(valueLocation.asFPR(), address);
            return;
        case StoreOpType::F64Store:
            m_jit.storeDouble(valueLocation.asFPR(), address);
            return;
        }
    }

    constexpr static const char* STORE_OP_NAMES[9] = {
        "I32Store", "I64Store", "F32Store", "F64Store",
        "I32Store8", "I32Store16",
        "I64Store8", "I64Store16", "I64Store32",
    };

    PartialResult WARN_UNUSED_RETURN store(StoreOpType storeOp, Value pointer, Value value, uint32_t uoffset)
    {
        Location valueLocation = locationOf(value);
        if (UNLIKELY(sumOverflows<uint32_t>(uoffset, sizeOfStoreOp(storeOp)))) {
            // FIXME: Same issue as in AirIRGenerator::load(): https://bugs.webkit.org/show_bug.cgi?id=166435
            emitThrowException(ExceptionType::OutOfBoundsMemoryAccess);
            consume(pointer);
            consume(value);
        } else
            emitStoreOp(storeOp, emitCheckAndPreparePointer(pointer, uoffset, sizeOfStoreOp(storeOp)), value, uoffset);

        LOG_INSTRUCTION(STORE_OP_NAMES[(unsigned)storeOp - (unsigned)I32Store], pointer, uoffset, value, valueLocation);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addGrowMemory(Value delta, Value& result)
    {
        Vector<Value, 8> arguments = { instanceValue(), delta };
        emitCCall(&operationGrowMemory, arguments, TypeKind::I32, result);
        restoreWebAssemblyGlobalState();

        LOG_INSTRUCTION("GrowMemory", delta, RESULT(result));

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addCurrentMemory(Value& result)
    {
        result = topValue(TypeKind::I32);
        Location resultLocation = allocate(result);
        m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfMemory()), m_scratchGPR);
        m_jit.loadPtr(Address(m_scratchGPR, Memory::offsetOfHandle()), m_scratchGPR);
        m_jit.loadPtr(Address(m_scratchGPR, BufferMemoryHandle::offsetOfSize()), m_scratchGPR);

        constexpr uint32_t shiftValue = 16;
        static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
        m_jit.urshiftPtr(Imm32(shiftValue), m_scratchGPR);
        m_jit.zeroExtend32ToWord(m_scratchGPR, resultLocation.asGPR());

        LOG_INSTRUCTION("CurrentMemory", RESULT(result));

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addMemoryFill(Value dstAddress, Value targetValue, Value count)
    {
        ASSERT(dstAddress.type() == TypeKind::I32);
        ASSERT(targetValue.type() == TypeKind::I32);
        ASSERT(count.type() == TypeKind::I32);

        Vector<Value, 8> arguments = {
            instanceValue(),
            dstAddress, targetValue, count
        };
        Value shouldThrow;
        emitCCall(&operationWasmMemoryFill, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

        LOG_INSTRUCTION("MemoryFill", dstAddress, targetValue, count);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addMemoryCopy(Value dstAddress, Value srcAddress, Value count)
    {
        ASSERT(dstAddress.type() == TypeKind::I32);
        ASSERT(srcAddress.type() == TypeKind::I32);
        ASSERT(count.type() == TypeKind::I32);

        Vector<Value, 8> arguments = {
            instanceValue(),
            dstAddress, srcAddress, count
        };
        Value shouldThrow;
        emitCCall(&operationWasmMemoryCopy, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

        LOG_INSTRUCTION("MemoryCopy", dstAddress, srcAddress, count);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addMemoryInit(unsigned dataSegmentIndex, Value dstAddress, Value srcAddress, Value length)
    {
        ASSERT(dstAddress.type() == TypeKind::I32);
        ASSERT(srcAddress.type() == TypeKind::I32);
        ASSERT(length.type() == TypeKind::I32);

        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(dataSegmentIndex),
            dstAddress, srcAddress, length
        };
        Value shouldThrow;
        emitCCall(&operationWasmMemoryInit, arguments, TypeKind::I32, shouldThrow);
        Location shouldThrowLocation = allocate(shouldThrow);

        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest32(ResultCondition::Zero, shouldThrowLocation.asGPR()));

        LOG_INSTRUCTION("MemoryInit", dataSegmentIndex, dstAddress, srcAddress, length);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addDataDrop(unsigned dataSegmentIndex)
    {
        Vector<Value, 8> arguments = { instanceValue(), Value::fromI32(dataSegmentIndex) };
        emitCCall(&operationWasmDataDrop, arguments);

        LOG_INSTRUCTION("DataDrop", dataSegmentIndex);
        return { };
    }

    // Atomics

    static inline Width accessWidth(ExtAtomicOpType op)
    {
        return static_cast<Width>(memoryLog2Alignment(op));
    }

    static inline uint32_t sizeOfAtomicOpMemoryAccess(ExtAtomicOpType op)
    {
        return bytesForWidth(accessWidth(op));
    }

    void emitSanitizeAtomicResult(ExtAtomicOpType op, TypeKind resultType, GPRReg source, GPRReg dest)
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

    void emitSanitizeAtomicResult(ExtAtomicOpType op, TypeKind resultType, GPRReg result)
    {
        emitSanitizeAtomicResult(op, resultType, result, result);
    }

    template<typename Functor>
    void emitAtomicOpGeneric(ExtAtomicOpType op, Address address, GPRReg oldGPR, GPRReg scratchGPR, const Functor& functor)
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

    Value WARN_UNUSED_RETURN emitAtomicLoadOp(ExtAtomicOpType loadOp, Type valueType, Location pointer, uint32_t uoffset)
    {
        ASSERT(pointer.isGPR());

        // For Atomic access, we need SimpleAddress (uoffset = 0).
        if (uoffset)
            m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
        Address address = Address(pointer.asGPR());

        if (accessWidth(loadOp) != Width8)
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(loadOp) - 1)));

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

    PartialResult WARN_UNUSED_RETURN atomicLoad(ExtAtomicOpType loadOp, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t uoffset)
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

    void emitAtomicStoreOp(ExtAtomicOpType storeOp, Type, Location pointer, Value value, uint32_t uoffset)
    {
        ASSERT(pointer.isGPR());

        // For Atomic access, we need SimpleAddress (uoffset = 0).
        if (uoffset)
            m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
        Address address = Address(pointer.asGPR());

        if (accessWidth(storeOp) != Width8)
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(storeOp) - 1)));

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

    PartialResult WARN_UNUSED_RETURN atomicStore(ExtAtomicOpType storeOp, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
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

    Value emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, Location pointer, Value value, uint32_t uoffset)
    {
        ASSERT(pointer.isGPR());

        // For Atomic access, we need SimpleAddress (uoffset = 0).
        if (uoffset)
            m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
        Address address = Address(pointer.asGPR());

        if (accessWidth(op) != Width8)
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(op) - 1)));

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

    PartialResult WARN_UNUSED_RETURN atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t uoffset)
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

    Value WARN_UNUSED_RETURN emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, Location pointer, Value expected, Value value, uint32_t uoffset)
    {
        ASSERT(pointer.isGPR());

        // For Atomic access, we need SimpleAddress (uoffset = 0).
        if (uoffset)
            m_jit.add64(TrustedImm64(static_cast<int64_t>(uoffset)), pointer.asGPR());
        Address address = Address(pointer.asGPR());
        Width valueWidth = widthForType(toB3Type(valueType));
        Width accessWidth = Wasm::accessWidth(op);

        if (accessWidth != Width8)
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchTest64(ResultCondition::NonZero, pointer.asGPR(), TrustedImm64(sizeOfAtomicOpMemoryAccess(op) - 1)));

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

        if (valueWidth == accessWidth) {
            emitStrongCAS(expectedLocation.asGPR(), valueLocation.asGPR(), resultLocation.asGPR());
            emitSanitizeAtomicResult(op, expected.type(), resultLocation.asGPR());
            return result;
        }

        emitSanitizeAtomicResult(op, expected.type(), expectedLocation.asGPR(), scratchGPR);

        Jump failure;
        switch (valueWidth) {
        case Width8:
        case Width16:
        case Width32:
            failure = m_jit.branch32(RelationalCondition::NotEqual, expectedLocation.asGPR(), scratchGPR);
            break;
        case Width64:
            failure = m_jit.branch64(RelationalCondition::NotEqual, expectedLocation.asGPR(), scratchGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        emitStrongCAS(expectedLocation.asGPR(), valueLocation.asGPR(), resultLocation.asGPR());
        auto done = m_jit.jump();

        failure.link(&m_jit);
        if (isARM64_LSE() || isX86_64()) {
            m_jit.move(TrustedImm32(0), resultLocation.asGPR());
            switch (accessWidth) {
            case Width8:
#if CPU(ARM64)
                m_jit.atomicXchgAdd8(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.atomicXchgAdd8(resultLocation.asGPR(), address);
#endif
                break;
            case Width16:
#if CPU(ARM64)
                m_jit.atomicXchgAdd32(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.atomicXchgAdd32(resultLocation.asGPR(), address);
#endif
                break;
            case Width32:
#if CPU(ARM64)
                m_jit.atomicXchgAdd32(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.atomicXchgAdd32(resultLocation.asGPR(), address);
#endif
                break;
            case Width64:
#if CPU(ARM64)
                m_jit.atomicXchgAdd64(resultLocation.asGPR(), address, resultLocation.asGPR());
#else
                m_jit.atomicXchgAdd64(resultLocation.asGPR(), address);
#endif
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        } else {
            emitAtomicOpGeneric(op, address, resultLocation.asGPR(), scratchGPR, [&](GPRReg oldGPR, GPRReg newGPR) {
                emitSanitizeAtomicResult(op, canonicalWidth(accessWidth) == Width64 ? TypeKind::I64 : TypeKind::I32, oldGPR, newGPR);
            });
        }

        done.link(&m_jit);
        emitSanitizeAtomicResult(op, expected.type(), resultLocation.asGPR());

        return result;
    }

    PartialResult WARN_UNUSED_RETURN atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t uoffset)
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

    PartialResult WARN_UNUSED_RETURN atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t uoffset)
    {
        Vector<Value, 8> arguments = {
            instanceValue(),
            pointer,
            Value::fromI32(uoffset),
            value,
            timeout
        };

        if (op == ExtAtomicOpType::MemoryAtomicWait32)
            emitCCall(&operationMemoryAtomicWait32, arguments, TypeKind::I32, result);
        else
            emitCCall(&operationMemoryAtomicWait64, arguments, TypeKind::I32, result);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION(makeString(op), pointer, value, timeout, uoffset, RESULT(result));

        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPR(), TrustedImm32(0)));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN atomicNotify(ExtAtomicOpType op, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t uoffset)
    {
        Vector<Value, 8> arguments = {
            instanceValue(),
            pointer,
            Value::fromI32(uoffset),
            count
        };
        emitCCall(&operationMemoryAtomicNotify, arguments, TypeKind::I32, result);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION(makeString(op), pointer, count, uoffset, RESULT(result));

        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branch32(RelationalCondition::LessThan, resultLocation.asGPR(), TrustedImm32(0)));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN atomicFence(ExtAtomicOpType, uint8_t)
    {
        m_jit.memoryFence();
        return { };
    }

    // Saturated truncation.

    struct FloatingPointRange {
        Value min, max;
        bool closedLowerEndpoint;
    };

    enum class TruncationKind {
        I32TruncF32S,
        I32TruncF32U,
        I64TruncF32S,
        I64TruncF32U,
        I32TruncF64S,
        I32TruncF64U,
        I64TruncF64S,
        I64TruncF64U
    };

    TruncationKind truncationKind(OpType truncationOp)
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

    TruncationKind truncationKind(Ext1OpType truncationOp)
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

    FloatingPointRange lookupTruncationRange(TruncationKind truncationKind)
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

    void truncInBounds(TruncationKind truncationKind, Location operandLocation, Location resultLocation)
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
#if CPU(X86_64)
        // x86_64 truncation to Uint64 requires extra parameters.
        case TruncationKind::I64TruncF32U: {
            ScratchScope<0, 2> scratches(*this);
            emitMoveConst(Value::fromF32(static_cast<float>(std::numeric_limits<int64_t>::min())), Location::fromFPR(scratches.fpr(1)));
            m_jit.truncateFloatToUint64(operandLocation.asFPR(), resultLocation.asGPR(), scratches.fpr(0), scratches.fpr(1));
            break;
        }
        case TruncationKind::I64TruncF64U: {
            ScratchScope<0, 2> scratches(*this);
            emitMoveConst(Value::fromF64(static_cast<double>(std::numeric_limits<int64_t>::min())), Location::fromFPR(scratches.fpr(1)));
            m_jit.truncateDoubleToUint64(operandLocation.asFPR(), resultLocation.asGPR(), scratches.fpr(0), scratches.fpr(1));
            break;
        }
#else
        case TruncationKind::I64TruncF32U:
            m_jit.truncateFloatToUint64(operandLocation.asFPR(), resultLocation.asGPR());
            break;
        case TruncationKind::I64TruncF64U:
            m_jit.truncateDoubleToUint64(operandLocation.asFPR(), resultLocation.asGPR());
            break;
#endif
        }
    }

    PartialResult WARN_UNUSED_RETURN truncTrapping(OpType truncationOp, Value operand, Value& result, Type returnType, Type operandType)
    {
        ScratchScope<0, 2> scratches(*this);

        Location operandLocation;
        if (operand.isConst()) {
            operandLocation = Location::fromFPR(m_scratchFPR);
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

        truncInBounds(kind, operandLocation, resultLocation);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType truncationOp, Value operand, Value& result, Type returnType, Type operandType)
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
            operandLocation = Location::fromFPR(m_scratchFPR);
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
        truncInBounds(kind, operandLocation, resultLocation);

        Jump afterInBounds = m_jit.jump();

        // Below-minimum case.
        lowerThanMin.link(&m_jit);

        // As an optimization, if the min result is 0; we can unconditionally return
        // that if the above-minimum-range check fails; otherwise, we need to check
        // for NaN since it also will fail the above-minimum-range-check
        if (!minResult) {
            returnType == Types::I32
                ? m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR(), resultLocation.asGPR())
                : m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR(), resultLocation.asGPR());
        } else {
            Jump isNotNaN = operandType == Types::F32
                ? m_jit.branchFloat(DoubleCondition::DoubleEqualAndOrdered, operandLocation.asFPR(), operandLocation.asFPR())
                : m_jit.branchDouble(DoubleCondition::DoubleEqualAndOrdered, operandLocation.asFPR(), operandLocation.asFPR());

            // NaN case. Set result to zero.
            returnType == Types::I32
                ? m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR(), resultLocation.asGPR())
                : m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR(), resultLocation.asGPR());
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
    PartialResult WARN_UNUSED_RETURN addI31New(ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addI31GetS(ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addI31GetU(ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t, ExpressionType, ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t, ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType, ExpressionType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addStructNewDefault(uint32_t, ExpressionType&) BBQ_STUB

    // Basic operators
    PartialResult WARN_UNUSED_RETURN addSelect(Value condition, Value lhs, Value rhs, Value& result)
    {
        if (condition.isConst()) {
            result = condition.asI32() ? lhs : rhs;
            LOG_INSTRUCTION("Select", condition, lhs, rhs, RESULT(result));
            consume(condition);
            consume(condition.asI32() ? rhs : lhs); // Kill whichever one we didn't choose.
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
                m_jit.move(conditionLocation.asGPR(), m_scratchGPR);
                conditionLocation = Location::fromGPR(m_scratchGPR);
            }

            if (lhs.isTemp())
                lhs = Value::pinned(lhs.type(), lhsLocation);
            if (rhs.isTemp())
                rhs = Value::pinned(rhs.type(), rhsLocation);

            // Kind of gross isel, but it should handle all use/def aliasing cases correctly.
            emitMove(lhs, resultLocation);
            Jump ifZero = m_jit.branchTest32(inverted ? ResultCondition::Zero : ResultCondition::NonZero, conditionLocation.asGPR(), conditionLocation.asGPR());
            consume(condition);
            emitMove(rhs, resultLocation);
            ifZero.link(&m_jit);

            LOG_DEDENT();
        }
        return { };
    }

    template<typename Fold, typename RegReg, typename RegImm>
    inline PartialResult binary(const char* opcode, TypeKind resultType, Value& lhs, Value& rhs, Value& result, Fold fold, RegReg regReg, RegImm regImm)
    {
        if (lhs.isConst() && rhs.isConst()) {
            result = fold(lhs, rhs);
            LOG_INSTRUCTION(opcode, lhs, rhs, RESULT(result));
            return { };
        }

        Location lhsLocation = Location::none(), rhsLocation = Location::none();

        // Ensure all non-constant parameters are loaded into registers.
        if (!lhs.isConst())
            lhsLocation = loadIfNecessary(lhs);
        if (!rhs.isConst())
            rhsLocation = loadIfNecessary(rhs);

        ASSERT(lhs.isConst() || lhsLocation.isRegister());
        ASSERT(rhs.isConst() || rhsLocation.isRegister());

        consume(lhs); // If either of our operands are temps, consume them and liberate any bound
        consume(rhs); // registers. This lets us reuse one of the registers for the output.

        Location toReuse = lhs.isConst() ? rhsLocation : lhsLocation; // Select the location to reuse, preferring lhs.

        result = topValue(resultType); // Result will be the new top of the stack.
        Location resultLocation = allocateWithHint(result, toReuse);
        ASSERT(resultLocation.isRegister());

        LOG_INSTRUCTION(opcode, lhs, lhsLocation, rhs, rhsLocation, RESULT(result));

        if (lhs.isConst() || rhs.isConst())
            regImm(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
        else
            regReg(lhs, lhsLocation, rhs, rhsLocation, resultLocation);

        return { };
    }

    template<typename Fold, typename Reg>
    inline PartialResult unary(const char* opcode, TypeKind resultType, Value& operand, Value& result, Fold fold, Reg reg)
    {
        if (operand.isConst()) {
            result = fold(operand);
            LOG_INSTRUCTION(opcode, operand, RESULT(result));
            return { };
        }

        Location operandLocation = loadIfNecessary(operand);
        ASSERT(operandLocation.isRegister());

        consume(operand); // If our operand is a temp, consume it and liberate its register if it has one.

        result = topValue(resultType); // Result will be the new top of the stack.
        Location resultLocation = allocateWithHint(result, operandLocation); // Try to reuse the operand location.
        ASSERT(resultLocation.isRegister());

        LOG_INSTRUCTION(opcode, operand, operandLocation, RESULT(result));

        reg(operand, operandLocation, resultLocation);
        return { };
    }

    struct ImmHelpers {
        ALWAYS_INLINE static Value& imm(Value& lhs, Value& rhs)
        {
            return lhs.isConst() ? lhs : rhs;
        }

        ALWAYS_INLINE static Location& immLocation(Location& lhsLocation, Location& rhsLocation)
        {
            return lhsLocation.isRegister() ? rhsLocation : lhsLocation;
        }

        ALWAYS_INLINE static Value& reg(Value& lhs, Value& rhs)
        {
            return lhs.isConst() ? rhs : lhs;
        }

        ALWAYS_INLINE static Location& regLocation(Location& lhsLocation, Location& rhsLocation)
        {
            return lhsLocation.isRegister() ? lhsLocation : rhsLocation;
        }
    };

#define BLOCK(...) __VA_ARGS__

#define EMIT_BINARY(opcode, resultType, foldExpr, regRegStatement, regImmStatement) \
        return binary(opcode, resultType, lhs, rhs, result, \
            [&](Value& lhs, Value& rhs) -> Value { \
                UNUSED_PARAM(lhs); \
                UNUSED_PARAM(rhs); \
                return foldExpr; /* Lambda to be called for constant folding, i.e. when both operands are constants. */ \
            }, \
            [&](Value& lhs, Location lhsLocation, Value& rhs, Location rhsLocation, Location resultLocation) -> void { \
                UNUSED_PARAM(lhs); \
                UNUSED_PARAM(rhs); \
                UNUSED_PARAM(lhsLocation); \
                UNUSED_PARAM(rhsLocation); \
                UNUSED_PARAM(resultLocation); \
                regRegStatement /* Lambda to be called when both operands are registers. */ \
            }, \
            [&](Value& lhs, Location lhsLocation, Value& rhs, Location rhsLocation, Location resultLocation) -> void { \
                UNUSED_PARAM(lhs); \
                UNUSED_PARAM(rhs); \
                UNUSED_PARAM(lhsLocation); \
                UNUSED_PARAM(rhsLocation); \
                UNUSED_PARAM(resultLocation); \
                regImmStatement /* Lambda to be when one operand is a register and the other is a constant. */ \
            });

#define EMIT_UNARY(opcode, resultType, foldExpr, regStatement) \
        return unary(opcode, resultType, operand, result, \
            [&](Value& operand) -> Value { \
                UNUSED_PARAM(operand); \
                return foldExpr; /* Lambda to be called for constant folding, i.e. when both operands are constants. */ \
            }, \
            [&](Value& operand, Location operandLocation, Location resultLocation) -> void { \
                UNUSED_PARAM(operand); \
                UNUSED_PARAM(operandLocation); \
                UNUSED_PARAM(resultLocation); \
                regStatement /* Lambda to be called when both operands are registers. */ \
            });

    PartialResult WARN_UNUSED_RETURN addI32Add(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I32Add", TypeKind::I32,
            BLOCK(Value::fromI32(lhs.asI32() + rhs.asI32())),
            BLOCK(
                m_jit.add32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.add32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Add(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I64Add", TypeKind::I64,
            BLOCK(Value::fromI64(lhs.asI64() + rhs.asI64())),
            BLOCK(
                m_jit.add64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.add64(TrustedImm64(ImmHelpers::imm(lhs, rhs).asI64()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF32Add(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Add", TypeKind::F32,
            BLOCK(Value::fromF32(lhs.asF32() + rhs.asF32())),
            BLOCK(
                m_jit.addFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.addFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF64Add(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F64Add", TypeKind::F64,
            BLOCK(Value::fromF64(lhs.asF64() + rhs.asF64())),
            BLOCK(
                m_jit.addDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.addDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Sub(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I32Sub", TypeKind::I32,
            BLOCK(Value::fromI32(lhs.asI32() - rhs.asI32())),
            BLOCK(
                m_jit.sub32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                if (rhs.isConst()) {
                    // Add a negative if rhs is a constant.
                    m_jit.move(lhsLocation.asGPR(), resultLocation.asGPR());
                    m_jit.add32(Imm32(-rhs.asI32()), resultLocation.asGPR());
                } else {
                    emitMoveConst(lhs, Location::fromGPR(m_scratchGPR));
                    m_jit.sub32(m_scratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Sub(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I64Sub", TypeKind::I64,
            BLOCK(Value::fromI64(lhs.asI64() - rhs.asI64())),
            BLOCK(
                m_jit.sub64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                if (rhs.isConst()) {
                    // Add a negative if rhs is a constant.
                    m_jit.move(lhsLocation.asGPR(), resultLocation.asGPR());
                    m_jit.add64(TrustedImm64(-rhs.asI64()), resultLocation.asGPR());
                } else {
                    emitMoveConst(lhs, Location::fromGPR(m_scratchGPR));
                    m_jit.sub64(m_scratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF32Sub(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Sub", TypeKind::F32,
            BLOCK(Value::fromF32(lhs.asF32() - rhs.asF32())),
            BLOCK(
                m_jit.subFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                if (rhs.isConst()) {
                    // Add a negative if rhs is a constant.
                    emitMoveConst(Value::fromF32(-rhs.asF32()), Location::fromFPR(m_scratchFPR));
                    m_jit.addFloat(lhsLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                } else {
                    emitMoveConst(lhs, Location::fromFPR(m_scratchFPR));
                    m_jit.subFloat(m_scratchFPR, rhsLocation.asFPR(), resultLocation.asFPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF64Sub(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F64Sub", TypeKind::F64,
            BLOCK(Value::fromF64(lhs.asF64() - rhs.asF64())),
            BLOCK(
                m_jit.subDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                if (rhs.isConst()) {
                    // Add a negative if rhs is a constant.
                    emitMoveConst(Value::fromF64(-rhs.asF64()), Location::fromFPR(m_scratchFPR));
                    m_jit.addDouble(lhsLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                } else {
                    emitMoveConst(lhs, Location::fromFPR(m_scratchFPR));
                    m_jit.subDouble(m_scratchFPR, rhsLocation.asFPR(), resultLocation.asFPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Mul(Value lhs, Value rhs, Value& result)
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

    PartialResult WARN_UNUSED_RETURN addI64Mul(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I64Mul", TypeKind::I64,
            BLOCK(Value::fromI64(lhs.asI64() * rhs.asI64())),
            BLOCK(
                m_jit.mul64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR(m_scratchGPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromGPR(m_scratchGPR));
                m_jit.mul64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF32Mul(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Mul", TypeKind::F32,
            BLOCK(Value::fromF32(lhs.asF32() * rhs.asF32())),
            BLOCK(
                m_jit.mulFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.mulFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF64Mul(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F64Mul", TypeKind::F64,
            BLOCK(Value::fromF64(lhs.asF64() * rhs.asF64())),
            BLOCK(
                m_jit.mulDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.mulDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    template<typename Func>
    void addLatePath(Func func)
    {
        m_latePaths.append(createSharedTask<void(BBQJIT&, CCallHelpers&)>(WTFMove(func)));
    }

    void emitThrowException(ExceptionType type)
    {
        m_jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
        auto jumpToExceptionStub = m_jit.jump();

        m_jit.addLinkTask([jumpToExceptionStub] (LinkBuffer& linkBuffer) {
            linkBuffer.link(jumpToExceptionStub, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
        });
    }

    void throwExceptionIf(ExceptionType type, Jump jump)
    {
        m_exceptions[static_cast<unsigned>(type)].append(jump);
    }

#if CPU(X86_64)
    RegisterSet clobbersForDivX86()
    {
        static RegisterSet x86DivClobbers;
        static std::once_flag flag;
        std::call_once(
            flag,
            []() {
                RegisterSetBuilder builder;
                builder.add(X86Registers::eax, Width::Width64);
                builder.add(X86Registers::edx, Width::Width64);
                x86DivClobbers = builder.buildAndValidate();
            });
        return x86DivClobbers;
    }

#define PREPARE_FOR_MOD_OR_DIV \
    do { \
        for (JSC::Reg reg : clobbersForDivX86()) \
            clobber(reg); \
    } while (false); \
    ScratchScope<0, 0> scratches(*this, clobbersForDivX86())

    template<typename IntType, bool IsMod>
    void emitModOrDiv(const Value& lhs, Location lhsLocation, const Value& rhs, Location rhsLocation, Location resultLocation)
    {
        // FIXME: We currently don't do nearly as sophisticated instruction selection on Intel as we do on other platforms,
        // but there's no good reason we can't. We should probably port over the isel in the future if it seems to yield
        // dividends.

        constexpr bool isSigned = std::is_signed<IntType>();
        constexpr bool is32 = sizeof(IntType) == 4;

        ASSERT(lhsLocation.isRegister() || rhsLocation.isRegister());
        if (lhs.isConst())
            emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
        else if (rhs.isConst())
            emitMoveConst(rhs, rhsLocation = Location::fromGPR(m_scratchGPR));
        ASSERT(lhsLocation.isRegister() && rhsLocation.isRegister());

        ASSERT(resultLocation.isRegister());
        ASSERT(lhsLocation.asGPR() != X86Registers::eax && lhsLocation.asGPR() != X86Registers::edx);
        ASSERT(rhsLocation.asGPR() != X86Registers::eax && lhsLocation.asGPR() != X86Registers::edx);

        ScratchScope<2, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);

        Jump toDiv, toEnd;

        Jump isZero = is32
            ? m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR())
            : m_jit.branchTest64(ResultCondition::Zero, rhsLocation.asGPR());
        throwExceptionIf(ExceptionType::DivisionByZero, isZero);
        if constexpr (isSigned) {
            if constexpr (is32)
                m_jit.compare32(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), scratches.gpr(0));
            else
                m_jit.compare64(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), scratches.gpr(0));
            if constexpr (is32)
                m_jit.compare32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min()), scratches.gpr(1));
            else {
                m_jit.move(TrustedImm64(std::numeric_limits<int64_t>::min()), scratches.gpr(1));
                m_jit.compare64(RelationalCondition::Equal, lhsLocation.asGPR(), scratches.gpr(1), scratches.gpr(1));
            }
            m_jit.and64(scratches.gpr(0), scratches.gpr(1));
            if constexpr (IsMod) {
                toDiv = m_jit.branchTest64(ResultCondition::Zero, scratches.gpr(1));
                // In this case, WASM doesn't want us to fault, but x86 will. So we set the result ourselves.
                if constexpr (is32)
                    m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
                else
                    m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
                toEnd = m_jit.jump();
            } else {
                Jump isNegativeOne = m_jit.branchTest64(ResultCondition::NonZero, scratches.gpr(1));
                throwExceptionIf(ExceptionType::IntegerOverflow, isNegativeOne);
            }
        }

        if (toDiv.isSet())
            toDiv.link(&m_jit);

        m_jit.move(lhsLocation.asGPR(), X86Registers::eax);

        if constexpr (is32 && isSigned) {
            m_jit.x86ConvertToDoubleWord32();
            m_jit.x86Div32(rhsLocation.asGPR());
        } else if constexpr (is32) {
            m_jit.xor32(X86Registers::edx, X86Registers::edx);
            m_jit.x86UDiv32(rhsLocation.asGPR());
        } else if constexpr (isSigned) {
            m_jit.x86ConvertToQuadWord64();
            m_jit.x86Div64(rhsLocation.asGPR());
        } else {
            m_jit.xor64(X86Registers::edx, X86Registers::edx);
            m_jit.x86UDiv64(rhsLocation.asGPR());
        }

        if constexpr (IsMod)
            m_jit.move(X86Registers::edx, resultLocation.asGPR());
        else
            m_jit.move(X86Registers::eax, resultLocation.asGPR());

        if (toEnd.isSet())
            toEnd.link(&m_jit);
    }
#else
#define PREPARE_FOR_MOD_OR_DIV

    template<typename IntType, bool IsMod>
    void emitModOrDiv(const Value& lhs, Location lhsLocation, const Value& rhs, Location rhsLocation, Location resultLocation)
    {
        constexpr bool isSigned = std::is_signed<IntType>();
        constexpr bool is32 = sizeof(IntType) == 4;

        ASSERT(lhsLocation.isRegister() || rhsLocation.isRegister());
        ASSERT(resultLocation.isRegister());

        bool checkedForZero = false, checkedForNegativeOne = false;
        if (rhs.isConst()) {
            int64_t divisor = is32 ? rhs.asI32() : rhs.asI64();
            if (!divisor) {
                emitThrowException(ExceptionType::DivisionByZero);
                return;
            }
            if (divisor == 1) {
                if constexpr (IsMod) {
                    // N % 1 == 0
                    if constexpr (is32)
                        m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
                    else
                        m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
                } else
                    m_jit.move(lhsLocation.asGPR(), resultLocation.asGPR());
                return;
            }
            if (divisor == -1) {
                // Check for INT_MIN / -1 case, and throw an IntegerOverflow exception if it occurs
                if (!IsMod && isSigned) {
                    Jump jump = is32
                        ? m_jit.branch32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min()))
                        : m_jit.branch64(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm64(std::numeric_limits<int64_t>::min()));
                    throwExceptionIf(ExceptionType::IntegerOverflow, jump);
                }

                if constexpr (IsMod) {
                    // N % 1 == 0
                    if constexpr (is32)
                        m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
                    else
                        m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
                    return;
                }
                if constexpr (isSigned) {
                    if constexpr (is32)
                        m_jit.neg32(lhsLocation.asGPR(), resultLocation.asGPR());
                    else
                        m_jit.neg64(lhsLocation.asGPR(), resultLocation.asGPR());
                    return;
                }

                // Fall through to general case.
            } else if (isPowerOfTwo(divisor)) {
                if constexpr (IsMod) {
                    Location originalResult = resultLocation;
                    if constexpr (isSigned)
                        resultLocation = Location::fromGPR(m_scratchGPR);

                    if constexpr (is32)
                        m_jit.and32(Imm32(static_cast<uint32_t>(divisor) - 1), lhsLocation.asGPR(), resultLocation.asGPR());
                    else
                        m_jit.and64(TrustedImm64(static_cast<uint64_t>(divisor) - 1), lhsLocation.asGPR(), resultLocation.asGPR());

                    if constexpr (isSigned) {
                        Jump isNonNegative = is32
                            ? m_jit.branch32(RelationalCondition::GreaterThanOrEqual, lhsLocation.asGPR(), TrustedImm32(0))
                            : m_jit.branch64(RelationalCondition::GreaterThanOrEqual, lhsLocation.asGPR(), TrustedImm64(0));
                        if constexpr (is32)
                            m_jit.neg32(m_scratchGPR, m_scratchGPR);
                        else
                            m_jit.neg64(m_scratchGPR, m_scratchGPR);
                        isNonNegative.link(&m_jit);
                        m_jit.move(m_scratchGPR, originalResult.asGPR());
                    }
                    return;
                }

                if constexpr (isSigned) {
                    Jump isNonNegative = is32
                        ? m_jit.branch32(RelationalCondition::GreaterThanOrEqual, lhsLocation.asGPR(), TrustedImm32(0))
                        : m_jit.branch64(RelationalCondition::GreaterThanOrEqual, lhsLocation.asGPR(), TrustedImm64(0));
                    if constexpr (is32)
                        m_jit.add32(Imm32(1), lhsLocation.asGPR(), lhsLocation.asGPR());
                    else
                        m_jit.add64(TrustedImm32(1), lhsLocation.asGPR(), lhsLocation.asGPR());
                    isNonNegative.link(&m_jit);
                }
                if constexpr (is32)
                    m_jit.rshift32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(WTF::fastLog2(static_cast<unsigned>(divisor)))), resultLocation.asGPR());
                else
                    m_jit.rshift64(lhsLocation.asGPR(),  TrustedImm32(WTF::fastLog2(static_cast<unsigned>(divisor))), resultLocation.asGPR());
                return;
            }
            // TODO: try generating integer reciprocal instead.
            checkedForNegativeOne = true;
            checkedForZero = true;
            rhsLocation = Location::fromGPR(m_scratchGPR);
            emitMoveConst(rhs, rhsLocation);
            // Fall through to register/register div.
        } else if (lhs.isConst()) {
            int64_t dividend = is32 ? lhs.asI32() : lhs.asI64();

            Jump isZero = is32
                ? m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR())
                : m_jit.branchTest64(ResultCondition::Zero, rhsLocation.asGPR());
            throwExceptionIf(ExceptionType::DivisionByZero, isZero);
            checkedForZero = true;

            if (!dividend) {
                if constexpr (is32)
                    m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
                else
                    m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
                return;
            }
            if (isSigned && !IsMod && dividend == std::numeric_limits<IntType>::min()) {
                Jump isNegativeOne = is32
                    ? m_jit.branch32(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1))
                    : m_jit.branch64(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm64(-1));
                throwExceptionIf(ExceptionType::IntegerOverflow, isNegativeOne);
                checkedForNegativeOne = true;
            }

            lhsLocation = Location::fromGPR(m_scratchGPR);
            emitMoveConst(lhs, lhsLocation);
            // Fall through to register/register div.
        }

        if (!checkedForZero) {
            Jump isZero = is32
                ? m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR())
                : m_jit.branchTest64(ResultCondition::Zero, rhsLocation.asGPR());
            throwExceptionIf(ExceptionType::DivisionByZero, isZero);
        }

        ScratchScope<1, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
        if (isSigned && !IsMod && !checkedForNegativeOne) {
            if constexpr (is32)
                m_jit.compare32(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), m_scratchGPR);
            else
                m_jit.compare64(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), m_scratchGPR);
            if constexpr (is32)
                m_jit.compare32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min()), scratches.gpr(0));
            else {
                m_jit.move(TrustedImm64(std::numeric_limits<int64_t>::min()), scratches.gpr(0));
                m_jit.compare64(RelationalCondition::Equal, lhsLocation.asGPR(), scratches.gpr(0), scratches.gpr(0));
            }
            m_jit.and64(m_scratchGPR, scratches.gpr(0), m_scratchGPR);
            Jump isNegativeOne = m_jit.branchTest64(ResultCondition::NonZero, m_scratchGPR);
            throwExceptionIf(ExceptionType::IntegerOverflow, isNegativeOne);
        }

        RegisterID divResult = IsMod ? scratches.gpr(0) : resultLocation.asGPR();
        if (is32 && isSigned)
            m_jit.div32(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);
        else if (is32)
            m_jit.uDiv32(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);
        else if (isSigned)
            m_jit.div64(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);
        else
            m_jit.uDiv64(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);

        if (IsMod) {
            if (is32)
                m_jit.multiplySub32(divResult, rhsLocation.asGPR(), lhsLocation.asGPR(), resultLocation.asGPR());
            else
                m_jit.multiplySub64(divResult, rhsLocation.asGPR(), lhsLocation.asGPR(), resultLocation.asGPR());
        }
    }
#endif

    template<typename IntType>
    Value checkConstantDivision(const Value& lhs, const Value& rhs)
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

    PartialResult WARN_UNUSED_RETURN addI32DivS(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I32DivS", TypeKind::I32,
            BLOCK(
                Value::fromI32(lhs.asI32() / checkConstantDivision<int32_t>(lhs, rhs).asI32())
            ),
            BLOCK(
                emitModOrDiv<int32_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<int32_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64DivS(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I64DivS", TypeKind::I64,
            BLOCK(
                Value::fromI64(lhs.asI64() / checkConstantDivision<int64_t>(lhs, rhs).asI64())
            ),
            BLOCK(
                emitModOrDiv<int64_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<int64_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32DivU(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I32DivU", TypeKind::I32,
            BLOCK(
                Value::fromI32(static_cast<uint32_t>(lhs.asI32()) / static_cast<uint32_t>(checkConstantDivision<int32_t>(lhs, rhs).asI32()))
            ),
            BLOCK(
                emitModOrDiv<uint32_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<uint32_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64DivU(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I64DivU", TypeKind::I64,
            BLOCK(
                Value::fromI64(static_cast<uint64_t>(lhs.asI64()) / static_cast<uint64_t>(checkConstantDivision<int64_t>(lhs, rhs).asI64()))
            ),
            BLOCK(
                emitModOrDiv<uint64_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<uint64_t, false>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32RemS(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I32RemS", TypeKind::I32,
            BLOCK(
                Value::fromI32(lhs.asI32() % checkConstantDivision<int32_t>(lhs, rhs).asI32())
            ),
            BLOCK(
                emitModOrDiv<int32_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<int32_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64RemS(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I64RemS", TypeKind::I64,
            BLOCK(
                Value::fromI64(lhs.asI64() % checkConstantDivision<int64_t>(lhs, rhs).asI64())
            ),
            BLOCK(
                emitModOrDiv<int64_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<int64_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32RemU(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I32RemU", TypeKind::I32,
            BLOCK(
                Value::fromI32(static_cast<uint32_t>(lhs.asI32()) % static_cast<uint32_t>(checkConstantDivision<int32_t>(lhs, rhs).asI32()))
            ),
            BLOCK(
                emitModOrDiv<uint32_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<uint32_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64RemU(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_MOD_OR_DIV;
        EMIT_BINARY(
            "I64RemU", TypeKind::I64,
            BLOCK(
                Value::fromI64(static_cast<uint64_t>(lhs.asI64()) % static_cast<uint64_t>(checkConstantDivision<int64_t>(lhs, rhs).asI64()))
            ),
            BLOCK(
                emitModOrDiv<uint64_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            ),
            BLOCK(
                emitModOrDiv<uint64_t, true>(lhs, lhsLocation, rhs, rhsLocation, resultLocation);
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF32Div(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Div", TypeKind::F32,
            BLOCK(Value::fromF32(lhs.asF32() / rhs.asF32())),
            BLOCK(
                m_jit.divFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.divFloat(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF64Div(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F64Div", TypeKind::F64,
            BLOCK(Value::fromF64(lhs.asF64() / rhs.asF64())),
            BLOCK(
                m_jit.divDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.divDouble(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    enum class MinOrMax { Min, Max };

    template<typename FloatType, MinOrMax IsMinOrMax>
    void emitFloatingPointMinOrMax(FPRegisterID left, FPRegisterID right, FPRegisterID result)
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
        m_jit.move(isGreaterThanResult, result);
        Jump afterGreaterThan = m_jit.jump();

        // Left is strictly less than right
        isLessThan.link(&m_jit);
        auto isLessThanResult = IsMinOrMax == MinOrMax::Max ? right : left;
        m_jit.move(isLessThanResult, result);
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

    PartialResult WARN_UNUSED_RETURN addF32Min(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Min", TypeKind::F32,
            BLOCK(Value::fromF32(std::min(lhs.asF32(), rhs.asF32()))),
            BLOCK(
                emitFloatingPointMinOrMax<float, MinOrMax::Min>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                emitFloatingPointMinOrMax<float, MinOrMax::Min>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF64Min(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F64Min", TypeKind::F64,
            BLOCK(Value::fromF64(std::min(lhs.asF64(), rhs.asF64()))),
            BLOCK(
                emitFloatingPointMinOrMax<double, MinOrMax::Min>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                emitFloatingPointMinOrMax<double, MinOrMax::Min>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF32Max(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Max", TypeKind::F32,
            BLOCK(Value::fromF32(std::max(lhs.asF32(), rhs.asF32()))),
            BLOCK(
                emitFloatingPointMinOrMax<float, MinOrMax::Max>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                emitFloatingPointMinOrMax<float, MinOrMax::Max>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addF64Max(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F64Max", TypeKind::F64,
            BLOCK(Value::fromF64(std::max(lhs.asF64(), rhs.asF64()))),
            BLOCK(
                emitFloatingPointMinOrMax<double, MinOrMax::Max>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                emitFloatingPointMinOrMax<double, MinOrMax::Max>(lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asFPR());
            )
        );
    }

    inline float floatCopySign(float lhs, float rhs)
    {
        uint32_t lhsAsInt32 = bitwise_cast<uint32_t>(lhs);
        uint32_t rhsAsInt32 = bitwise_cast<uint32_t>(rhs);
        lhsAsInt32 &= 0x7fffffffu;
        rhsAsInt32 &= 0x80000000u;
        lhsAsInt32 |= rhsAsInt32;
        return bitwise_cast<float>(lhsAsInt32);
    }

    inline double doubleCopySign(double lhs, double rhs)
    {
        uint64_t lhsAsInt64 = bitwise_cast<uint64_t>(lhs);
        uint64_t rhsAsInt64 = bitwise_cast<uint64_t>(rhs);
        lhsAsInt64 &= 0x7fffffffffffffffu;
        rhsAsInt64 &= 0x8000000000000000u;
        lhsAsInt64 |= rhsAsInt64;
        return bitwise_cast<double>(lhsAsInt64);
    }

    PartialResult WARN_UNUSED_RETURN addI32And(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I32And", TypeKind::I32,
            BLOCK(Value::fromI32(lhs.asI32() & rhs.asI32())),
            BLOCK(
                m_jit.and32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.and32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64And(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I64And", TypeKind::I64,
            BLOCK(Value::fromI64(lhs.asI64() & rhs.asI64())),
            BLOCK(
                m_jit.and64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.and64(TrustedImm64(ImmHelpers::imm(lhs, rhs).asI64()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Xor(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I32Xor", TypeKind::I32,
            BLOCK(Value::fromI32(lhs.asI32() ^ rhs.asI32())),
            BLOCK(
                m_jit.xor32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.xor32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Xor(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I64Xor", TypeKind::I64,
            BLOCK(Value::fromI64(lhs.asI64() ^ rhs.asI64())),
            BLOCK(
                m_jit.xor64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.xor64(TrustedImm64(ImmHelpers::imm(lhs, rhs).asI64()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Or(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I32Or", TypeKind::I32,
            BLOCK(Value::fromI32(lhs.asI32() | rhs.asI32())),
            BLOCK(
                m_jit.or32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.or32(Imm32(ImmHelpers::imm(lhs, rhs).asI32()), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Or(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "I64Or", TypeKind::I64,
            BLOCK(Value::fromI64(lhs.asI64() | rhs.asI64())),
            BLOCK(
                m_jit.or64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                m_jit.move(ImmHelpers::regLocation(lhsLocation, rhsLocation).asGPR(), resultLocation.asGPR());
                m_jit.or64(TrustedImm64(ImmHelpers::imm(lhs, rhs).asI64()), resultLocation.asGPR());
            )
        );
    }

#if CPU(X86_64)
#define PREPARE_FOR_SHIFT \
    do { \
        clobber(m_shiftRCX); \
    } while (false); \
    ScratchScope<0, 0> scratches(*this, Location::fromGPR(m_shiftRCX))
#else
#define PREPARE_FOR_SHIFT
#endif

    void moveShiftAmountIfNecessary(Location& rhsLocation)
    {
        if constexpr (isX86()) {
            m_jit.move(rhsLocation.asGPR(), m_shiftRCX);
            rhsLocation = Location::fromGPR(m_shiftRCX);
        }
    }

    PartialResult WARN_UNUSED_RETURN addI32Shl(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
                    m_jit.lshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Shl(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
                    m_jit.lshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32ShrS(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
                    m_jit.rshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64ShrS(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
                    m_jit.rshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32ShrU(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
                    m_jit.urshift32(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64ShrU(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, lhsLocation = Location::fromGPR(m_scratchGPR));
                    m_jit.urshift64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Rotl(Value lhs, Value rhs, Value& result)
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
                m_jit.neg32(rhsLocation.asGPR(), m_scratchGPR);
                m_jit.rotateRight32(lhsLocation.asGPR(), m_scratchGPR, resultLocation.asGPR());
            ),
            BLOCK(
                if (rhs.isConst())
                    m_jit.rotateRight32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(-rhs.asI32())), resultLocation.asGPR());
                else {
                    moveShiftAmountIfNecessary(rhsLocation);
                    m_jit.neg32(rhsLocation.asGPR(), m_scratchGPR);
                    emitMoveConst(lhs, resultLocation);
                    m_jit.rotateRight32(resultLocation.asGPR(), m_scratchGPR, resultLocation.asGPR());
                }
            )
#endif
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Rotl(Value lhs, Value rhs, Value& result)
    {
        PREPARE_FOR_SHIFT;
        EMIT_BINARY(
            "I64Rotl", TypeKind::I64,
            BLOCK(Value::fromI64(B3::rotateLeft(lhs.asI64(), rhs.asI64()))),
#if CPU(X86_64)
            BLOCK(
                moveShiftAmountIfNecessary(rhsLocation);
                m_jit.rotateLeft64(lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                if (rhs.isConst())
                    m_jit.rotateLeft64(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(rhs.asI32())), resultLocation.asGPR());
                else {
                    moveShiftAmountIfNecessary(rhsLocation);
                    emitMoveConst(lhs, resultLocation);
                    m_jit.rotateLeft64(resultLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
#else
            BLOCK(
                moveShiftAmountIfNecessary(rhsLocation);
                m_jit.neg64(rhsLocation.asGPR(), m_scratchGPR);
                m_jit.rotateRight64(lhsLocation.asGPR(), m_scratchGPR, resultLocation.asGPR());
            ),
            BLOCK(
                if (rhs.isConst())
                    m_jit.rotateRight64(lhsLocation.asGPR(), TrustedImm32(-rhs.asI64()), resultLocation.asGPR());
                else {
                    moveShiftAmountIfNecessary(rhsLocation);
                    m_jit.neg64(rhsLocation.asGPR(), m_scratchGPR);
                    emitMoveConst(lhs, resultLocation);
                    m_jit.rotateRight64(resultLocation.asGPR(), m_scratchGPR, resultLocation.asGPR());
                }
            )
#endif
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Rotr(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, Location::fromGPR(m_scratchGPR));
                    m_jit.rotateRight32(m_scratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Rotr(Value lhs, Value rhs, Value& result)
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
                    emitMoveConst(lhs, Location::fromGPR(m_scratchGPR));
                    m_jit.rotateRight64(m_scratchGPR, rhsLocation.asGPR(), resultLocation.asGPR());
                }
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Clz(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32Clz", TypeKind::I32,
            BLOCK(Value::fromI32(WTF::clz(operand.asI32()))),
            BLOCK(
                m_jit.countLeadingZeros32(operandLocation.asGPR(), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Clz(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Clz", TypeKind::I64,
            BLOCK(Value::fromI64(WTF::clz(operand.asI64()))),
            BLOCK(
                m_jit.countLeadingZeros64(operandLocation.asGPR(), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI32Ctz(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32Ctz", TypeKind::I32,
            BLOCK(Value::fromI32(WTF::ctz(operand.asI32()))),
            BLOCK(
                m_jit.countTrailingZeros32(operandLocation.asGPR(), resultLocation.asGPR());
            )
        );
    }

    PartialResult WARN_UNUSED_RETURN addI64Ctz(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Ctz", TypeKind::I64,
            BLOCK(Value::fromI64(WTF::ctz(operand.asI64()))),
            BLOCK(
                m_jit.countTrailingZeros64(operandLocation.asGPR(), resultLocation.asGPR());
            )
        );
    }

    PartialResult emitCompareI32(const char* opcode, Value& lhs, Value& rhs, Value& result, RelationalCondition condition, bool (*comparator)(int32_t lhs, int32_t rhs))
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

    PartialResult emitCompareI64(const char* opcode, Value& lhs, Value& rhs, Value& result, RelationalCondition condition, bool (*comparator)(int64_t lhs, int64_t rhs))
    {
        EMIT_BINARY(
            opcode, TypeKind::I32,
            BLOCK(Value::fromI64(static_cast<int64_t>(comparator(lhs.asI64(), rhs.asI64())))),
            BLOCK(
                m_jit.compare64(condition, lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromGPR(m_scratchGPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), ImmHelpers::immLocation(lhsLocation, rhsLocation));
                m_jit.compare64(condition, lhsLocation.asGPR(), rhsLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

#define RELOP_AS_LAMBDA(op) [](auto lhs, auto rhs) -> auto { return lhs op rhs; }
#define TYPED_RELOP_AS_LAMBDA(type, op) [](auto lhs, auto rhs) -> auto { return static_cast<type>(lhs) op static_cast<type>(rhs); }

    PartialResult WARN_UNUSED_RETURN addI32Eq(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32Eq", lhs, rhs, result, RelationalCondition::Equal, RELOP_AS_LAMBDA( == ));
    }

    PartialResult WARN_UNUSED_RETURN addI64Eq(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64Eq", lhs, rhs, result, RelationalCondition::Equal, RELOP_AS_LAMBDA( == ));
    }

    PartialResult WARN_UNUSED_RETURN addI32Ne(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32Ne", lhs, rhs, result, RelationalCondition::NotEqual, RELOP_AS_LAMBDA( != ));
    }

    PartialResult WARN_UNUSED_RETURN addI64Ne(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64Ne", lhs, rhs, result, RelationalCondition::NotEqual, RELOP_AS_LAMBDA( != ));
    }

    PartialResult WARN_UNUSED_RETURN addI32LtS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32LtS", lhs, rhs, result, RelationalCondition::LessThan, RELOP_AS_LAMBDA( < ));
    }

    PartialResult WARN_UNUSED_RETURN addI64LtS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64LtS", lhs, rhs, result, RelationalCondition::LessThan, RELOP_AS_LAMBDA( < ));
    }

    PartialResult WARN_UNUSED_RETURN addI32LeS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32LeS", lhs, rhs, result, RelationalCondition::LessThanOrEqual, RELOP_AS_LAMBDA( <= ));
    }

    PartialResult WARN_UNUSED_RETURN addI64LeS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64LeS", lhs, rhs, result, RelationalCondition::LessThanOrEqual, RELOP_AS_LAMBDA( <= ));
    }

    PartialResult WARN_UNUSED_RETURN addI32GtS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32GtS", lhs, rhs, result, RelationalCondition::GreaterThan, RELOP_AS_LAMBDA( > ));
    }

    PartialResult WARN_UNUSED_RETURN addI64GtS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64GtS", lhs, rhs, result, RelationalCondition::GreaterThan, RELOP_AS_LAMBDA( > ));
    }

    PartialResult WARN_UNUSED_RETURN addI32GeS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32GeS", lhs, rhs, result, RelationalCondition::GreaterThanOrEqual, RELOP_AS_LAMBDA( >= ));
    }

    PartialResult WARN_UNUSED_RETURN addI64GeS(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64GeS", lhs, rhs, result, RelationalCondition::GreaterThanOrEqual, RELOP_AS_LAMBDA( >= ));
    }

    PartialResult WARN_UNUSED_RETURN addI32LtU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32LtU", lhs, rhs, result, RelationalCondition::Below, TYPED_RELOP_AS_LAMBDA(uint32_t, <));
    }

    PartialResult WARN_UNUSED_RETURN addI64LtU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64LtU", lhs, rhs, result, RelationalCondition::Below, TYPED_RELOP_AS_LAMBDA(uint64_t, <));
    }

    PartialResult WARN_UNUSED_RETURN addI32LeU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32LeU", lhs, rhs, result, RelationalCondition::BelowOrEqual, TYPED_RELOP_AS_LAMBDA(uint32_t, <=));
    }

    PartialResult WARN_UNUSED_RETURN addI64LeU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64LeU", lhs, rhs, result, RelationalCondition::BelowOrEqual, TYPED_RELOP_AS_LAMBDA(uint64_t, <=));
    }

    PartialResult WARN_UNUSED_RETURN addI32GtU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32GtU", lhs, rhs, result, RelationalCondition::Above, TYPED_RELOP_AS_LAMBDA(uint32_t, >));
    }

    PartialResult WARN_UNUSED_RETURN addI64GtU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64GtU", lhs, rhs, result, RelationalCondition::Above, TYPED_RELOP_AS_LAMBDA(uint64_t, >));
    }

    PartialResult WARN_UNUSED_RETURN addI32GeU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI32("I32GeU", lhs, rhs, result, RelationalCondition::AboveOrEqual, TYPED_RELOP_AS_LAMBDA(uint32_t, >=));
    }

    PartialResult WARN_UNUSED_RETURN addI64GeU(Value lhs, Value rhs, Value& result)
    {
        return emitCompareI64("I64GeU", lhs, rhs, result, RelationalCondition::AboveOrEqual, TYPED_RELOP_AS_LAMBDA(uint64_t, >=));
    }

    PartialResult emitCompareF32(const char* opcode, Value& lhs, Value& rhs, Value& result, DoubleCondition condition, bool (*comparator)(float lhs, float rhs))
    {
        EMIT_BINARY(
            opcode, TypeKind::I32,
            BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asF32(), rhs.asF32())))),
            BLOCK(
                m_jit.compareFloat(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.compareFloat(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult emitCompareF64(const char* opcode, Value& lhs, Value& rhs, Value& result, DoubleCondition condition, bool (*comparator)(double lhs, double rhs))
    {
        EMIT_BINARY(
            opcode, TypeKind::I32,
            BLOCK(Value::fromI32(static_cast<int32_t>(comparator(lhs.asF64(), rhs.asF64())))),
            BLOCK(
                m_jit.compareDouble(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
            ),
            BLOCK(
                ImmHelpers::immLocation(lhsLocation, rhsLocation) = Location::fromFPR(m_scratchFPR);
                emitMoveConst(ImmHelpers::imm(lhs, rhs), Location::fromFPR(m_scratchFPR));
                m_jit.compareDouble(condition, lhsLocation.asFPR(), rhsLocation.asFPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Eq(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF32("F32Eq", lhs, rhs, result, DoubleCondition::DoubleEqualAndOrdered, RELOP_AS_LAMBDA( == ));
    }

    PartialResult WARN_UNUSED_RETURN addF64Eq(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF64("F64Eq", lhs, rhs, result, DoubleCondition::DoubleEqualAndOrdered, RELOP_AS_LAMBDA( == ));
    }

    PartialResult WARN_UNUSED_RETURN addF32Ne(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF32("F32Ne", lhs, rhs, result, DoubleCondition::DoubleNotEqualOrUnordered, RELOP_AS_LAMBDA( != ));
    }

    PartialResult WARN_UNUSED_RETURN addF64Ne(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF64("F64Ne", lhs, rhs, result, DoubleCondition::DoubleNotEqualOrUnordered, RELOP_AS_LAMBDA( != ));
    }

    PartialResult WARN_UNUSED_RETURN addF32Lt(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF32("F32Lt", lhs, rhs, result, DoubleCondition::DoubleLessThanAndOrdered, RELOP_AS_LAMBDA( < ));
    }

    PartialResult WARN_UNUSED_RETURN addF64Lt(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF64("F64Lt", lhs, rhs, result, DoubleCondition::DoubleLessThanAndOrdered, RELOP_AS_LAMBDA( < ));
    }

    PartialResult WARN_UNUSED_RETURN addF32Le(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF32("F32Le", lhs, rhs, result, DoubleCondition::DoubleLessThanOrEqualAndOrdered, RELOP_AS_LAMBDA( <= ));
    }

    PartialResult WARN_UNUSED_RETURN addF64Le(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF64("F64Le", lhs, rhs, result, DoubleCondition::DoubleLessThanOrEqualAndOrdered, RELOP_AS_LAMBDA( <= ));
    }

    PartialResult WARN_UNUSED_RETURN addF32Gt(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF32("F32Gt", lhs, rhs, result, DoubleCondition::DoubleGreaterThanAndOrdered, RELOP_AS_LAMBDA( > ));
    }

    PartialResult WARN_UNUSED_RETURN addF64Gt(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF64("F64Gt", lhs, rhs, result, DoubleCondition::DoubleGreaterThanAndOrdered, RELOP_AS_LAMBDA( > ));
    }

    PartialResult WARN_UNUSED_RETURN addF32Ge(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF32("F32Ge", lhs, rhs, result, DoubleCondition::DoubleGreaterThanOrEqualAndOrdered, RELOP_AS_LAMBDA( >= ));
    }

    PartialResult WARN_UNUSED_RETURN addF64Ge(Value lhs, Value rhs, Value& result)
    {
        return emitCompareF64("F64Ge", lhs, rhs, result, DoubleCondition::DoubleGreaterThanOrEqualAndOrdered, RELOP_AS_LAMBDA( >= ));
    }

#undef RELOP_AS_LAMBDA
#undef TYPED_RELOP_AS_LAMBDA

    PartialResult addI32WrapI64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32WrapI64", TypeKind::I32,
            BLOCK(Value::fromI32(static_cast<int32_t>(operand.asI64()))),
            BLOCK(
                m_jit.move(operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult addI32Extend8S(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32Extend8S", TypeKind::I32,
            BLOCK(Value::fromI32(static_cast<int32_t>(static_cast<int8_t>(operand.asI32())))),
            BLOCK(
                m_jit.signExtend8To32(operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI32Extend16S(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32Extend16S", TypeKind::I32,
            BLOCK(Value::fromI32(static_cast<int32_t>(static_cast<int16_t>(operand.asI32())))),
            BLOCK(
                m_jit.signExtend16To32(operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64Extend8S(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Extend8S", TypeKind::I64,
            BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int8_t>(operand.asI64())))),
            BLOCK(
                m_jit.signExtend8To32(operandLocation.asGPR(), resultLocation.asGPR());
                m_jit.signExtend32ToPtr(resultLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64Extend16S(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Extend16S", TypeKind::I64,
            BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int16_t>(operand.asI64())))),
            BLOCK(
                m_jit.signExtend16To32(operandLocation.asGPR(), resultLocation.asGPR());
                m_jit.signExtend32ToPtr(resultLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64Extend32S(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Extend32S", TypeKind::I64,
            BLOCK(Value::fromI64(static_cast<int64_t>(static_cast<int32_t>(operand.asI64())))),
            BLOCK(
                m_jit.signExtend32ToPtr(operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64ExtendSI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64ExtendSI32", TypeKind::I64,
            BLOCK(Value::fromI64(static_cast<int64_t>(operand.asI32()))),
            BLOCK(
                m_jit.signExtend32ToPtr(operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64ExtendUI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64ExtendUI32", TypeKind::I64,
            BLOCK(Value::fromI64(static_cast<uint64_t>(static_cast<uint32_t>(operand.asI32())))),
            BLOCK(
                m_jit.zeroExtend32ToWord(operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI32Eqz(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32Eqz", TypeKind::I32,
            BLOCK(Value::fromI32(!operand.asI32())),
            BLOCK(
                m_jit.test32(ResultCondition::Zero, operandLocation.asGPR(), operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64Eqz(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Eqz", TypeKind::I64,
            BLOCK(Value::fromI64(!operand.asI64())),
            BLOCK(
                m_jit.test64(ResultCondition::Zero, operandLocation.asGPR(), operandLocation.asGPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI32Popcnt(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32Popcnt", TypeKind::I32,
            BLOCK(Value::fromI32(__builtin_popcount(operand.asI32()))),
            BLOCK(
#if CPU(X86_64)
                if (m_jit.supportsCountPopulation())
                    m_jit.countPopulation32(operandLocation.asGPR(), resultLocation.asGPR());
                else
#endif
                    emitCCall(&operationPopcount32, Vector<Value> { operand }, TypeKind::I32, result);
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64Popcnt(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64Popcnt", TypeKind::I64,
            BLOCK(Value::fromI64(__builtin_popcountll(operand.asI64()))),
            BLOCK(
#if CPU(X86_64)
                if (m_jit.supportsCountPopulation())
                    m_jit.countPopulation64(operandLocation.asGPR(), resultLocation.asGPR());
                else
#endif
                    emitCCall(&operationPopcount64, Vector<Value> { operand }, TypeKind::I32, result);
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI32ReinterpretF32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I32ReinterpretF32", TypeKind::I32,
            BLOCK(Value::fromI32(bitwise_cast<int32_t>(operand.asF32()))),
            BLOCK(
                m_jit.moveFloatTo32(operandLocation.asFPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI64ReinterpretF64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "I64ReinterpretF64", TypeKind::I64,
            BLOCK(Value::fromI64(bitwise_cast<int64_t>(operand.asF64()))),
            BLOCK(
                m_jit.moveDoubleTo64(operandLocation.asFPR(), resultLocation.asGPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32ReinterpretI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32ReinterpretI32", TypeKind::F32,
            BLOCK(Value::fromF32(bitwise_cast<float>(operand.asI32()))),
            BLOCK(
                m_jit.move32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64ReinterpretI64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64ReinterpretI64", TypeKind::F64,
            BLOCK(Value::fromF64(bitwise_cast<double>(operand.asI64()))),
            BLOCK(
                m_jit.move64ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32DemoteF64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32DemoteF64", TypeKind::F32,
            BLOCK(Value::fromF32(operand.asF64())),
            BLOCK(
                m_jit.convertDoubleToFloat(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64PromoteF32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64PromoteF32", TypeKind::F64,
            BLOCK(Value::fromF64(operand.asF32())),
            BLOCK(
                m_jit.convertFloatToDouble(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32ConvertSI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32ConvertSI32", TypeKind::F32,
            BLOCK(Value::fromF32(operand.asI32())),
            BLOCK(
                m_jit.convertInt32ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32ConvertUI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32ConvertUI32", TypeKind::F32,
            BLOCK(Value::fromF32(static_cast<uint32_t>(operand.asI32()))),
            BLOCK(
#if CPU(X86_64)
                ScratchScope<1, 0> scratches(*this);
                m_jit.zeroExtend32ToWord(operandLocation.asGPR(), m_scratchGPR);
                m_jit.convertUInt64ToFloat(m_scratchGPR, resultLocation.asFPR(), scratches.gpr(0));
#else
                m_jit.zeroExtend32ToWord(operandLocation.asGPR(), m_scratchGPR);
                m_jit.convertUInt64ToFloat(m_scratchGPR, resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32ConvertSI64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32ConvertSI64", TypeKind::F32,
            BLOCK(Value::fromF32(operand.asI64())),
            BLOCK(
                m_jit.convertInt64ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32ConvertUI64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32ConvertUI64", TypeKind::F32,
            BLOCK(Value::fromF32(static_cast<uint64_t>(operand.asI64()))),
            BLOCK(
#if CPU(X86_64)
                m_jit.convertUInt64ToFloat(operandLocation.asGPR(), resultLocation.asFPR(), m_scratchGPR);
#else
                m_jit.convertUInt64ToFloat(operandLocation.asGPR(), resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64ConvertSI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64ConvertSI32", TypeKind::F64,
            BLOCK(Value::fromF64(operand.asI32())),
            BLOCK(
                m_jit.convertInt32ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64ConvertUI32(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64ConvertUI32", TypeKind::F64,
            BLOCK(Value::fromF64(static_cast<uint32_t>(operand.asI32()))),
            BLOCK(
#if CPU(X86_64)
                ScratchScope<1, 0> scratches(*this);
                m_jit.zeroExtend32ToWord(operandLocation.asGPR(), m_scratchGPR);
                m_jit.convertUInt64ToDouble(m_scratchGPR, resultLocation.asFPR(), scratches.gpr(0));
#else
                m_jit.zeroExtend32ToWord(operandLocation.asGPR(), m_scratchGPR);
                m_jit.convertUInt64ToDouble(m_scratchGPR, resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64ConvertSI64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64ConvertSI64", TypeKind::F64,
            BLOCK(Value::fromF64(operand.asI64())),
            BLOCK(
                m_jit.convertInt64ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64ConvertUI64(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64ConvertUI64", TypeKind::F64,
            BLOCK(Value::fromF64(static_cast<uint64_t>(operand.asI64()))),
            BLOCK(
#if CPU(X86_64)
                m_jit.convertUInt64ToDouble(operandLocation.asGPR(), resultLocation.asFPR(), m_scratchGPR);
#else
                m_jit.convertUInt64ToDouble(operandLocation.asGPR(), resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Copysign(Value lhs, Value rhs, Value& result)
    {
        EMIT_BINARY(
            "F32Copysign", TypeKind::F32,
            BLOCK(Value::fromF32(floatCopySign(lhs.asF32(), rhs.asF32()))),
            BLOCK(
                // FIXME: Better than what we have in the Air backend, but still not great. I think
                // there's some vector instruction we can use to do this much quicker.

                m_jit.moveFloatTo32(rhsLocation.asFPR(), m_scratchGPR);
                m_jit.and32(Imm32(static_cast<int32_t>(0x80000000u)), m_scratchGPR, m_scratchGPR);
                m_jit.move32ToFloat(m_scratchGPR, m_scratchFPR);
#if CPU(X86_64)
                m_jit.move32ToFloat(TrustedImm32(0x7fffffff), resultLocation.asFPR());
                m_jit.andFloat(lhsLocation.asFPR(), resultLocation.asFPR());
                m_jit.orFloat(resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
                m_jit.absFloat(lhsLocation.asFPR(), lhsLocation.asFPR());
                m_jit.orFloat(lhsLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#endif
            ),
            BLOCK(
                if (lhs.isConst()) {
                    m_jit.moveFloatTo32(rhsLocation.asFPR(), m_scratchGPR);
                    m_jit.and32(Imm32(static_cast<int32_t>(0x80000000u)), m_scratchGPR, m_scratchGPR);
                    m_jit.move32ToFloat(m_scratchGPR, m_scratchFPR);

                    emitMoveConst(Value::fromF32(std::fabsf(lhs.asF32())), resultLocation);
                    m_jit.orFloat(resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                } else {
                    bool signBit = bitwise_cast<uint32_t>(rhs.asF32()) & 0x80000000u;
#if CPU(X86_64)
                    m_jit.move32ToFloat(TrustedImm32(0x7fffffff), resultLocation.asFPR());
                    m_jit.andFloat(lhsLocation.asFPR(), resultLocation.asFPR());
                    if (signBit) {
                        m_jit.xorFloat(m_scratchFPR, m_scratchFPR);
                        m_jit.subFloat(m_scratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
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

    PartialResult WARN_UNUSED_RETURN addF64Copysign(Value lhs, Value rhs, Value& result)
    {
        if constexpr (isX86())
            clobber(m_shiftRCX);

        EMIT_BINARY(
            "F64Copysign", TypeKind::F64,
            BLOCK(Value::fromF64(doubleCopySign(lhs.asF64(), rhs.asF64()))),
            BLOCK(
                // FIXME: Better than what we have in the Air backend, but still not great. I think
                // there's some vector instruction we can use to do this much quicker.

                m_jit.moveDoubleTo64(rhsLocation.asFPR(), m_scratchGPR);

                // Probably saves us a bit of space compared to reserving another register and
                // materializing a 64-bit constant.
                m_jit.urshift64(m_scratchGPR, TrustedImm32(63), m_scratchGPR);
                m_jit.lshift64(m_scratchGPR, TrustedImm32(63), m_scratchGPR);
                m_jit.move64ToDouble(m_scratchGPR, m_scratchFPR);

#if CPU(X86_64)
                m_jit.move64ToDouble(TrustedImm64(0x7fffffffffffffffll), resultLocation.asFPR());
                m_jit.andDouble(lhsLocation.asFPR(), resultLocation.asFPR());
                m_jit.orDouble(resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
                m_jit.absDouble(lhsLocation.asFPR(), lhsLocation.asFPR());
                m_jit.orDouble(lhsLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#endif
            ),
            BLOCK(
                if (lhs.isConst()) {
                    m_jit.moveDoubleTo64(rhsLocation.asFPR(), m_scratchGPR);
                    m_jit.urshift64(m_scratchGPR, TrustedImm32(63), m_scratchGPR);
                    m_jit.lshift64(m_scratchGPR, TrustedImm32(63), m_scratchGPR);
                    m_jit.move64ToDouble(m_scratchGPR, m_scratchFPR);

                    // Moving this constant clobbers m_scratchGPR, but not m_scratchFPR
                    emitMoveConst(Value::fromF64(std::fabs(lhs.asF64())), resultLocation);
                    m_jit.orDouble(resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                } else {
                    bool signBit = bitwise_cast<uint64_t>(rhs.asF64()) & 0x8000000000000000ull;
#if CPU(X86_64)
                    m_jit.move64ToDouble(TrustedImm64(0x7fffffffffffffffll), resultLocation.asFPR());
                    m_jit.andDouble(lhsLocation.asFPR(), resultLocation.asFPR());
                    if (signBit) {
                        m_jit.xorDouble(m_scratchFPR, m_scratchFPR);
                        m_jit.subDouble(m_scratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
                    }
#else
                    m_jit.absDouble(lhsLocation.asFPR(), resultLocation.asFPR());
                    if (signBit)
                        m_jit.negateDouble(resultLocation.asFPR(), resultLocation.asFPR());
#endif
                }
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Floor(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Floor", TypeKind::F32,
            BLOCK(Value::fromF32(Math::floorFloat(operand.asF32()))),
            BLOCK(
                m_jit.floorFloat(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Floor(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Floor", TypeKind::F64,
            BLOCK(Value::fromF64(Math::floorDouble(operand.asF64()))),
            BLOCK(
                m_jit.floorDouble(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Ceil(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Ceil", TypeKind::F32,
            BLOCK(Value::fromF32(Math::ceilFloat(operand.asF32()))),
            BLOCK(
                m_jit.ceilFloat(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Ceil(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Ceil", TypeKind::F64,
            BLOCK(Value::fromF64(Math::ceilDouble(operand.asF64()))),
            BLOCK(
                m_jit.ceilDouble(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Abs(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Abs", TypeKind::F32,
            BLOCK(Value::fromF32(std::abs(operand.asF32()))),
            BLOCK(
#if CPU(X86_64)
                m_jit.move32ToFloat(TrustedImm32(0x7fffffffll), m_scratchFPR);
                m_jit.andFloat(operandLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
                m_jit.absFloat(operandLocation.asFPR(), resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Abs(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Abs", TypeKind::F64,
            BLOCK(Value::fromF64(std::abs(operand.asF64()))),
            BLOCK(
#if CPU(X86_64)
                m_jit.move64ToDouble(TrustedImm64(0x7fffffffffffffffll), m_scratchFPR);
                m_jit.andDouble(operandLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
                m_jit.absDouble(operandLocation.asFPR(), resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Sqrt(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Sqrt", TypeKind::F32,
            BLOCK(Value::fromF32(Math::sqrtFloat(operand.asF32()))),
            BLOCK(
                m_jit.sqrtFloat(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Sqrt(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Sqrt", TypeKind::F64,
            BLOCK(Value::fromF64(Math::sqrtDouble(operand.asF64()))),
            BLOCK(
                m_jit.sqrtDouble(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Neg(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Neg", TypeKind::F32,
            BLOCK(Value::fromF32(-operand.asF32())),
            BLOCK(
#if CPU(X86_64)
                m_jit.xorFloat(m_scratchFPR, m_scratchFPR);
                m_jit.subFloat(operandLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
                m_jit.negateFloat(operandLocation.asFPR(), resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Neg(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Neg", TypeKind::F64,
            BLOCK(Value::fromF64(-operand.asF64())),
            BLOCK(
#if CPU(X86_64)
                m_jit.xorDouble(m_scratchFPR, m_scratchFPR);
                m_jit.subDouble(operandLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
                m_jit.negateDouble(operandLocation.asFPR(), resultLocation.asFPR());
#endif
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Nearest(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Nearest", TypeKind::F32,
            BLOCK(Value::fromF32(std::nearbyintf(operand.asF32()))),
            BLOCK(
                m_jit.roundTowardNearestIntFloat(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Nearest(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Nearest", TypeKind::F64,
            BLOCK(Value::fromF64(std::nearbyint(operand.asF64()))),
            BLOCK(
                m_jit.roundTowardNearestIntDouble(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF32Trunc(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F32Trunc", TypeKind::F32,
            BLOCK(Value::fromF32(Math::truncFloat(operand.asF32()))),
            BLOCK(
                m_jit.roundTowardZeroFloat(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addF64Trunc(Value operand, Value& result)
    {
        EMIT_UNARY(
            "F64Trunc", TypeKind::F64,
            BLOCK(Value::fromF64(Math::truncDouble(operand.asF64()))),
            BLOCK(
                m_jit.roundTowardZeroDouble(operandLocation.asFPR(), resultLocation.asFPR());
            )
        )
    }

    PartialResult WARN_UNUSED_RETURN addI32TruncSF32(Value operand, Value& result)
    {
        return truncTrapping(OpType::I32TruncSF32, operand, result, Types::I32, Types::F32);
    }

    PartialResult WARN_UNUSED_RETURN addI32TruncSF64(Value operand, Value& result)
    {
        return truncTrapping(OpType::I32TruncSF64, operand, result, Types::I32, Types::F64);
    }

    PartialResult WARN_UNUSED_RETURN addI32TruncUF32(Value operand, Value& result)
    {
        return truncTrapping(OpType::I32TruncUF32, operand, result, Types::I32, Types::F32);
    }

    PartialResult WARN_UNUSED_RETURN addI32TruncUF64(Value operand, Value& result)
    {
        return truncTrapping(OpType::I32TruncUF64, operand, result, Types::I32, Types::F64);
    }

    PartialResult WARN_UNUSED_RETURN addI64TruncSF32(Value operand, Value& result)
    {
        return truncTrapping(OpType::I64TruncSF32, operand, result, Types::I64, Types::F32);
    }

    PartialResult WARN_UNUSED_RETURN addI64TruncSF64(Value operand, Value& result)
    {
        return truncTrapping(OpType::I64TruncSF64, operand, result, Types::I64, Types::F64);
    }

    PartialResult WARN_UNUSED_RETURN addI64TruncUF32(Value operand, Value& result)
    {
        return truncTrapping(OpType::I64TruncUF32, operand, result, Types::I64, Types::F32);
    }

    PartialResult WARN_UNUSED_RETURN addI64TruncUF64(Value operand, Value& result)
    {
        return truncTrapping(OpType::I64TruncUF64, operand, result, Types::I64, Types::F64);
    }

    // References

    PartialResult WARN_UNUSED_RETURN addRefIsNull(Value operand, Value& result)
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

    PartialResult WARN_UNUSED_RETURN addRefAsNonNull(Value value, Value& result)
    {
        Location valueLocation;
        if (value.isConst()) {
            valueLocation = Location::fromGPR(m_scratchGPR);
            emitMoveConst(value, valueLocation);
        } else
            valueLocation = loadIfNecessary(value);
        ASSERT(valueLocation.isGPR());
        consume(value);

        result = topValue(TypeKind::Ref);
        Location resultLocation = allocate(result);
        ASSERT(JSValue::encode(jsNull()) >= 0 && JSValue::encode(jsNull()) <= INT32_MAX);
        throwExceptionIf(ExceptionType::NullRefAsNonNull, m_jit.branch64(RelationalCondition::Equal, valueLocation.asGPR(), TrustedImm32(static_cast<int32_t>(JSValue::encode(jsNull())))));
        m_jit.move(valueLocation.asGPR(), resultLocation.asGPR());

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t index, Value& result)
    {
        // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
        TypeKind returnType = Options::useWebAssemblyTypedFunctionReferences() ? TypeKind::Ref : TypeKind::Funcref;

        Vector<Value, 8> arguments = {
            instanceValue(),
            Value::fromI32(index)
        };
        emitCCall(&operationWasmRefFunc, arguments, returnType, result);

        return { };
    }

#undef BLOCK
#undef EMIT_BINARY
#undef EMIT_UNARY

    void emitEntryTierUpCheck()
    {
        if (!m_tierUp)
            return;

        m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(&m_tierUp->m_counter)), m_scratchGPR);
        Jump tierUp = m_jit.branchAdd32(CCallHelpers::PositiveOrZero, TrustedImm32(TierUpCount::functionEntryIncrement()), Address(m_scratchGPR));
        MacroAssembler::Label tierUpResume = m_jit.label();
        auto functionIndex = m_functionIndex;
        addLatePath([tierUp, tierUpResume, functionIndex](BBQJIT&, CCallHelpers& jit) {
            tierUp.link(&jit);
            jit.move(TrustedImm32(functionIndex), GPRInfo::argumentGPR1);
            MacroAssembler::Call call = jit.nearCall();
            jit.jump(tierUpResume);

            bool isSIMD = false; // TODO: Support SIMD
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                MacroAssembler::repatchNearCall(linkBuffer.locationOfNearCall<NoPtrTag>(call), CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(triggerOMGEntryTierUpThunkGenerator(isSIMD)).code()));
            });
        });
    }

    // Control flow
    ControlData WARN_UNUSED_RETURN addTopLevel(BlockSignature signature)
    {
        if (UNLIKELY(Options::verboseBBQJITInstructions())) {
            auto nameSection = m_info.nameSection;
            auto functionIndexSpace = m_info.isImportedFunctionFromFunctionIndexSpace(m_functionIndex) ? m_functionIndex : m_functionIndex + m_info.importFunctionCount();
            std::pair<const Name*, RefPtr<NameSection>> name = nameSection->get(functionIndexSpace);
            dataLog("BBQ\tFunction ");
            if (name.first)
                dataLog(makeString(*name.first));
            else
                dataLog(m_functionIndex);
            dataLogLn(" ", *m_functionSignature);
            LOG_INDENT();
        }

        m_jit.emitFunctionPrologue();
        m_topLevel = ControlData(*this, BlockType::TopLevel, signature, 0);

        m_jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(&m_callee)), m_scratchGPR);
        static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
        if constexpr (is32Bit()) {
            CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
            m_jit.storePtr(m_scratchGPR, calleeSlot.withOffset(PayloadOffset));
            m_jit.store32(CCallHelpers::TrustedImm32(JSValue::WasmTag), calleeSlot.withOffset(TagOffset));
            m_jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
        } else
            m_jit.storePairPtr(GPRInfo::wasmContextInstancePointer, m_scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

        m_frameSizeLabel = m_jit.moveWithPatch(TrustedImmPtr(nullptr), m_scratchGPR);

        // Because we compile in a single pass, we always need to pessimistically check for stack underflow/overflow.
        ScratchScope<1, 0> scratches(*this);
        m_jit.subPtr(GPRInfo::callFrameRegister, m_scratchGPR, MacroAssembler::stackPointerRegister);
        m_jit.move(MacroAssembler::stackPointerRegister, m_scratchGPR);
        MacroAssembler::JumpList underflow;
        underflow.append(m_jit.branchPtr(CCallHelpers::Above, m_scratchGPR, GPRInfo::callFrameRegister));
        m_jit.addLinkTask([underflow] (LinkBuffer& linkBuffer) {
            linkBuffer.link(underflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
        });

        MacroAssembler::JumpList overflow;
        m_jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfVM()), scratches.gpr(0));
        overflow.append(m_jit.branchPtr(CCallHelpers::Below, m_scratchGPR, CCallHelpers::Address(scratches.gpr(0), VM::offsetOfSoftStackLimit())));
        m_jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
            linkBuffer.link(overflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
        });

        // Zero all locals that aren't initialized by arguments.
        // This is kind of icky...we can evaluate replacing this a memset() or a tracker for which
        // locals have been initialized.
        for (LocalOrTempIndex i = 0; i < m_locals.size(); ++i) {
            if (i < m_arguments.size()) {
                flushValue(Value::fromLocal(m_parser->typeOfLocal(i).kind, i));
                continue;
            }

            Value zero;
            switch (m_parser->typeOfLocal(i).kind) {
            case TypeKind::I32:
            case TypeKind::I31ref:
                emitStoreConst(Value::fromI32(0), m_locals[i]);
                break;
            case TypeKind::F32:
                emitStoreConst(Value::fromF32(0), m_locals[i]);
                break;
            case TypeKind::F64:
                emitStoreConst(Value::fromF64(0), m_locals[i]);
                break;
            case TypeKind::I64:
            case TypeKind::Struct:
            case TypeKind::Rec:
            case TypeKind::Func:
            case TypeKind::Array:
            case TypeKind::Sub:
                emitStoreConst(Value::fromI64(0), m_locals[i]);
                break;
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::Ref:
            case TypeKind::RefNull:
            case TypeKind::Structref:
            case TypeKind::Arrayref:
                emitStoreConst(Value::fromI64(bitwise_cast<uint64_t>(JSValue::encode(jsNull()))), m_locals[i]);
                break;
            case TypeKind::V128: {
                m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR);
                m_jit.storeVector(m_scratchFPR, m_locals[i].asAddress());
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        for (size_t i = 0; i < m_functionSignature->argumentCount(); i ++)
            m_topLevel.touch(i); // Ensure arguments are flushed to persistent locations when this block ends.

        // This clobbers argumentGPR0 and argumentGPR1. So call it after flushing arguments.
        emitEntryTierUpCheck();

        return m_topLevel;
    }

    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack)
    {
        result = ControlData(*this, BlockType::Block, signature, currentControlData().enclosedHeight() + enclosingStack.size() - signature->as<FunctionSignature>()->argumentCount());
        currentControlData().endBlock(*this, result, enclosingStack, true, false);

        LOG_INSTRUCTION("Block", *signature);
        LOG_INDENT();
        splitStack(signature, enclosingStack, newStack);
        result.startBlock(*this, newStack);
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack, uint32_t)
    {
        result = ControlData(*this, BlockType::Loop, signature, currentControlData().enclosedHeight() + enclosingStack.size() - signature->as<FunctionSignature>()->argumentCount());
        currentControlData().endBlock(*this, result, enclosingStack, true, false);

        LOG_INSTRUCTION("Loop", *signature);
        LOG_INDENT();
        splitStack(signature, enclosingStack, newStack);
        result.startBlock(*this, newStack);
        result.setLoopLabel(m_jit.label());
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addIf(Value condition, BlockSignature signature, Stack& enclosingStack, ControlData& result, Stack& newStack)
    {
        Location conditionLocation = Location::fromGPR(m_scratchGPR);
        if (!condition.isConst())
            emitMove(condition, conditionLocation);
        consume(condition);

        result = ControlData(*this, BlockType::If, signature, currentControlData().enclosedHeight() + enclosingStack.size() - signature->as<FunctionSignature>()->argumentCount());
        currentControlData().endBlock(*this, result, enclosingStack, true, false);

        LOG_INSTRUCTION("If", *signature, condition, conditionLocation);
        LOG_INDENT();
        splitStack(signature, enclosingStack, newStack);

        result.startBlock(*this, newStack);
        if (condition.isConst() && !condition.asI32())
            result.setIfBranch(m_jit.jump()); // Emit direct branch if we know the condition is false.
        else if (!condition.isConst()) // Otherwise, we only emit a branch at all if we don't know the condition statically.
            result.setIfBranch(m_jit.branchTest32(ResultCondition::Zero, m_scratchGPR, m_scratchGPR));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addElse(ControlData& data, Stack& expressionStack)
    {
        data.endBlock(*this, data, expressionStack, false, true);
        ControlData dataElse(*this, BlockType::Block, data.signature(), data.enclosedHeight());
        data.linkJumps(&m_jit);
        dataElse.addBranch(m_jit.jump());
        data.linkIfBranch(&m_jit); // Link specifically the conditional branch of the preceding If
        LOG_DEDENT();
        LOG_INSTRUCTION("Else");
        LOG_INDENT();

        // We don't care at this point about the values live at the end of the previous control block,
        // we just need the right number of temps for our arguments on the top of the stack.
        expressionStack.clear();
        while (expressionStack.size() < data.signature()->as<FunctionSignature>()->argumentCount()) {
            Type type = data.signature()->as<FunctionSignature>()->argumentType(expressionStack.size());
            expressionStack.constructAndAppend(type, Value::fromTemp(type.kind, dataElse.enclosedHeight() + expressionStack.size()));
        }

        dataElse.startBlock(*this, expressionStack);
        data = dataElse;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData& data)
    {
        // Since the end of the block was unreachable, we don't need to call endBlock() here, since the
        // block will never actually end.
        ControlData elseData(*this, BlockType::Block, data.signature(), data.enclosedHeight());
        data.linkJumps(&m_jit);
        elseData.addBranch(m_jit.jump()); // Still needed even when the parent was unreachable to avoid running code within the else block.
        data.linkIfBranch(&m_jit); // Link specifically the conditional branch of the preceding If
        LOG_DEDENT();
        LOG_INSTRUCTION("Else");
        LOG_INDENT();

        // We don't have anything left on the expression stack after an unreachable block. So let's make a fake
        // one, containing one temp for each of the parameters we are expecting.
        Stack expressionStack;
        auto functionSignature = elseData.signature()->as<FunctionSignature>();
        for (unsigned i = 0; i < functionSignature->argumentCount(); i ++)
            expressionStack.constructAndAppend(functionSignature->argumentType(i), Value::fromTemp(functionSignature->argumentType(i).kind, data.enclosedHeight() + i));
        elseData.startBlock(*this, expressionStack);
        data = elseData;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack&, ControlType&, Stack&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned, const TypeDefinition&, Stack&, ControlType&, ResultList&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned, const TypeDefinition&, ControlType&, ResultList&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned, Vector<ExpressionType>&, Stack&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&) BBQ_STUB

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData& data, const Stack& returnValues)
    {
        CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*data.signature(), CallRole::Callee);

        if (LIKELY(wasmCallInfo.results.size() == 1)) {
            ASSERT(returnValues.size() >= 1);
            emitMove(returnValues.last(), Location::fromArgumentLocation(wasmCallInfo.results[0]));
            LOG_INSTRUCTION("Return", Location::fromArgumentLocation(wasmCallInfo.results[0]));
        } else if (wasmCallInfo.results.size()) {
            // For multi-value return, a parallel move might be necessary. This is comparatively complex
            // and slow, so we limit it to this slow path.
            Vector<Value, 8> returnValuesForShuffle;
            Vector<Location, 8> returnLocationsForShuffle;
            for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i) {
                returnValuesForShuffle.append(returnValues[i]);
                returnLocationsForShuffle.append(Location::fromArgumentLocation(wasmCallInfo.results[i]));
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

    PartialResult WARN_UNUSED_RETURN addBranch(ControlData& target, Value condition, Stack& results)
    {
        UNUSED_PARAM(results);
        if (condition.isConst() && !condition.asI32()) // If condition is known to be false, this is a no-op.
            return { };

        Location conditionLocation = Location::fromGPR(m_scratchGPR);
        if (!condition.isNone() && !condition.isConst())
            emitMove(condition, conditionLocation);
        consume(condition);
        currentControlData().endBlock(*this, target, results, false, condition.isNone());

        if (condition.isNone())
            LOG_INSTRUCTION("Branch");
        else
            LOG_INSTRUCTION("Branch", condition, conditionLocation);

        if (condition.isConst() || condition.isNone())
            target.addBranch(m_jit.jump()); // We know condition is true, since if it was false we would have returned early.
        else
            target.addBranch(m_jit.branchTest32(ResultCondition::NonZero, m_scratchGPR));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSwitch(Value condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, Stack& results)
    {
        ASSERT(condition.type() == TypeKind::I32);

        LOG_INSTRUCTION("BrTable", condition);

        if (!condition.isConst())
            emitMove(condition, Location::fromGPR(m_scratchGPR));
        consume(condition);
        currentControlData().endBlock(*this, defaultTarget, results, false, true);

        if (condition.isConst()) {
            // If we know the condition statically, we emit one direct branch to the known target.
            int targetIndex = condition.asI32();
            if (targetIndex >= 0 && targetIndex < static_cast<int>(targets.size()))
                targets[targetIndex]->addBranch(m_jit.jump());
            else
                defaultTarget.addBranch(m_jit.jump());
            return { };
        }

        for (size_t i = 0; i < targets.size(); i ++) {
            Jump branch = m_jit.branch32(RelationalCondition::Equal, Imm32(i), m_scratchGPR);
            targets[i]->addBranch(branch);
        }
        defaultTarget.addBranch(m_jit.jump()); // Fall through to default case with a direct jump.

        return { };
    }

    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry& entry, Stack& stack)
    {
        return addEndToUnreachable(entry, stack);
    }

    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry& entry, Stack& stack)
    {
        ControlData& entryData = entry.controlData;

        unsigned returnCount = entryData.signature()->as<FunctionSignature>()->returnCount();
        unsigned offset = stack.size() - returnCount;
        for (unsigned i = 0; i < returnCount; ++i) {
            if (i < stack.size())
                entry.enclosedExpressionStack.append(stack[i + offset]);
            else {
                Type type = entryData.signature()->as<FunctionSignature>()->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(type, Value::fromTemp(type.kind, i + entryData.enclosedHeight()));
            }
        }
        switch (entryData.blockType()) {
        case BlockType::TopLevel:
            entryData.endBlock(*this, entryData, entry.enclosedExpressionStack, false, true);
            entryData.linkJumps(&m_jit);
            for (unsigned i = 0; i < returnCount; ++i) {
                // Make sure we expect the stack values in the correct locations.
                if (!entry.enclosedExpressionStack[i].value().isConst()) {
                    Value& value = entry.enclosedExpressionStack[i].value();
                    value = Value::fromTemp(value.type(), i);
                    bind(value, entryData.resultLocations()[i]);
                }
            }
            return addReturn(entryData, entry.enclosedExpressionStack);
        case BlockType::Loop:
            entryData.convertLoopToBlock();
            entryData.endBlock(*this, entryData, entry.enclosedExpressionStack, false, true);
            entryData.linkJumpsTo(entryData.loopLabel(), &m_jit);
            break;
        default:
            entryData.endBlock(*this, entryData, entry.enclosedExpressionStack, false, true);
            entryData.linkJumps(&m_jit);
            break;
        }

        LOG_DEDENT();
        LOG_INSTRUCTION("End");

        currentControlData().resumeBlock(*this, entryData, entry.enclosedExpressionStack);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&)
    {
        int frameSize = m_frameSize + m_maxCalleeStackSize;
        DataLabelPtr frameSizeLabel = m_frameSizeLabel;
        CCallHelpers& jit = m_jit;
        m_jit.addLinkTask([frameSize, frameSizeLabel, &jit](LinkBuffer& linkBuffer) {
            int roundedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), frameSize);
            jit.repatchPointer(linkBuffer.locationOf<NoPtrTag>(frameSizeLabel), bitwise_cast<void*>(static_cast<uintptr_t>(roundedFrameSize)));
        });

        LOG_DEDENT();
        LOG_INSTRUCTION("End");

        for (const auto& latePath : m_latePaths)
            latePath->run(*this, m_jit);

        for (unsigned i = 0; i < numberOfExceptionTypes; ++i) {
            auto& jumps = m_exceptions[i];
            if (!jumps.empty()) {
                jumps.link(&jit);
                emitThrowException(static_cast<ExceptionType>(i));
            }
        }

        return { };
    }

    // Flush a value to its canonical slot.
    void flushValue(Value value)
    {
        if (value.isConst() || value.isPinned())
            return;
        Location currentLocation = locationOf(value);
        Location slot = canonicalSlot(value);

        emitMove(value, slot);
        unbind(value, currentLocation);
        bind(value, slot);
    }

    void restoreWebAssemblyContextInstance()
    {
        m_jit.loadPtr(Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * sizeof(Register)), GPRInfo::wasmContextInstancePointer);
    }

    void restoreWebAssemblyGlobalState()
    {
        restoreWebAssemblyContextInstance();
        // FIXME: We should just store these registers on stack and load them.
        if (!!m_info.memory) {
            m_jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, TrustedImm32(Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
            m_jit.cageConditionallyAndUntag(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, m_scratchGPR, /* validateAuth */ true, /* mayBeNull */ false);
        }
    }

    void flushRegisters()
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

    void saveValuesAcrossCall(const CallInformation& callInfo)
    {
        // Flush any values in caller-saved registers.
        for (Reg reg : m_callerSaves) {
            RegisterBinding binding = reg.isGPR() ? m_gprBindings[reg.gpr()] : m_fprBindings[reg.fpr()];
            if (!binding.toValue().isNone())
                flushValue(binding.toValue());
        }

        // Flush any values that overlap with parameter locations.
        // FIXME: This is kind of a quick and dirty approach to what really should be a parallel move
        // from argument to parameter locations, we may be able to avoid some stores.
        for (size_t i = 0; i < callInfo.params.size(); ++i) {
            Location paramLocation = Location::fromArgumentLocation(callInfo.params[i]);
            if (paramLocation.isRegister()) {
                RegisterBinding binding = paramLocation.isGPR() ? m_gprBindings[paramLocation.asGPR()] : m_fprBindings[paramLocation.asFPR()];
                if (!binding.toValue().isNone())
                    flushValue(binding.toValue());
            }
        }
    }

    void restoreValuesAfterCall(const CallInformation& callInfo)
    {
        UNUSED_PARAM(callInfo);
        // Caller-saved values shouldn't actually need to be restored here, the register allocator will restore them lazily
        // whenever they are next used.
    }

    template<size_t N>
    void passParametersToCall(const Vector<Value, N>& arguments, const CallInformation& callInfo)
    {
        // Move arguments to parameter locations.
        for (size_t i = 0; i < callInfo.params.size(); i ++) {
            Location paramLocation = Location::fromArgumentLocation(callInfo.params[i]);
            Value argument = arguments[i];
            emitMove(argument, paramLocation);
            consume(argument);
        }
    }

    template<size_t N>
    void returnValuesFromCall(Vector<Value, N>& results, const FunctionSignature& functionType, const CallInformation& callInfo)
    {
        for (size_t i = 0; i < callInfo.results.size(); i ++) {
            Value result = Value::fromTemp(functionType.returnType(i).kind, m_parser->expressionStack().size() + i);
            Location returnLocation = Location::fromArgumentLocation(callInfo.results[i]);
            if (returnLocation.isRegister()) {
                RegisterBinding& currentBinding = returnLocation.isGPR() ? m_gprBindings[returnLocation.asGPR()] : m_fprBindings[returnLocation.asFPR()];
                if (currentBinding.isScratch()) {
                    // FIXME: This is a total hack and could cause problems. We assume scratch registers (allocated by a ScratchScope)
                    // will never be live across a call. So far, this is probably true, but it's fragile. Probably the fix here is to
                    // exclude all possible return value registers from ScratchScope so we can guarantee there's never any interference.
                    currentBinding = RegisterBinding::none();
                    if (returnLocation.isGPR())
                        m_gprSet.add(returnLocation.asGPR(), Width::Width64);
                    else
                        m_fprSet.add(returnLocation.asFPR(), Width::Width128);
                }
            }
            bind(result, returnLocation);
            results.append(result);
        }
    }

    template<typename Func, size_t N>
    void emitCCall(Func function, const Vector<Value, N>& arguments)
    {
        // Currently, we assume the Wasm calling convention is the same as the C calling convention
        Vector<Type, 1> resultTypes;
        Vector<Type> argumentTypes;
        for (const Value& value : arguments)
            argumentTypes.append(Type { value.type(), 0u });
        RefPtr<TypeDefinition> functionType = TypeInformation::typeDefinitionForFunction(resultTypes, argumentTypes);
        CallInformation callInfo = wasmCallingConvention().callInformationFor(*functionType, CallRole::Caller);
        Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
        m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

        // Prepare wasm operation calls.
        m_jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);

        // Preserve caller-saved registers and other info
        saveValuesAcrossCall(callInfo);

        // Move argument values to parameter locations
        passParametersToCall(arguments, callInfo);

        // Materialize address of native function and call register
        void* taggedFunctionPtr = tagCFunctionPtr<void*, OperationPtrTag>(function);
        m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(taggedFunctionPtr)), m_scratchGPR);
        m_jit.call(m_scratchGPR, OperationPtrTag);
    }

    template<typename Func, size_t N>
    void emitCCall(Func function, const Vector<Value, N>& arguments, TypeKind returnType, Value& result)
    {
        // Currently, we assume the Wasm calling convention is the same as the C calling convention
        Vector<Type, 1> resultTypes = { Type { returnType, 0u } };
        Vector<Type> argumentTypes;
        for (const Value& value : arguments)
            argumentTypes.append(Type { value.type(), 0u });
        RefPtr<TypeDefinition> functionType = TypeInformation::typeDefinitionForFunction(resultTypes, argumentTypes);
        CallInformation callInfo = wasmCallingConvention().callInformationFor(*functionType, CallRole::Caller);
        Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
        m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

        // Prepare wasm operation calls.
        m_jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);

        // Preserve caller-saved registers and other info
        saveValuesAcrossCall(callInfo);

        // Move argument values to parameter locations
        passParametersToCall(arguments, callInfo);

        // Materialize address of native function and call register
        void* taggedFunctionPtr = tagCFunctionPtr<void*, OperationPtrTag>(function);
        m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(taggedFunctionPtr)), m_scratchGPR);
        m_jit.call(m_scratchGPR, OperationPtrTag);

        // FIXME: Probably we should make CCall more lower level, and we should bind the result to Value separately.
        result = Value::fromTemp(returnType, m_parser->expressionStack().size());
        Location resultLocation;
        switch (returnType) {
        case TypeKind::I32:
        case TypeKind::I31ref:
        case TypeKind::I64:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
        case TypeKind::Funcref:
        case TypeKind::Externref:
        case TypeKind::Rec:
        case TypeKind::Sub:
        case TypeKind::Array:
        case TypeKind::Struct:
        case TypeKind::Func: {
            resultLocation = Location::fromGPR(GPRInfo::returnValueGPR);
            ASSERT(m_validGPRs.contains(GPRInfo::returnValueGPR, IgnoreVectors));
            break;
        }
        case TypeKind::F32:
        case TypeKind::F64: {
            resultLocation = Location::fromFPR(FPRInfo::returnValueFPR);
            ASSERT(m_validFPRs.contains(FPRInfo::returnValueFPR, Width::Width128));
            break;
        }
        case TypeKind::V128: {
            resultLocation = Location::fromFPR(FPRInfo::returnValueFPR);
            ASSERT(m_validFPRs.contains(FPRInfo::returnValueFPR, Width::Width128));
            break;
        }
        case TypeKind::Void:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        RegisterBinding& currentBinding = resultLocation.isGPR() ? m_gprBindings[resultLocation.asGPR()] : m_fprBindings[resultLocation.asFPR()];
        RELEASE_ASSERT(!currentBinding.isScratch());

        bind(result, resultLocation);
    }

    PartialResult WARN_UNUSED_RETURN addCall(unsigned functionIndex, const TypeDefinition& signature, Vector<Value>& arguments, ResultList& results, CallType callType = CallType::Call)
    {
        UNUSED_PARAM(callType); // TODO: handle tail calls
        const FunctionSignature& functionType = *signature.as<FunctionSignature>();
        CallInformation callInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
        Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
        m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

        // Preserve caller-saved registers and other info
        saveValuesAcrossCall(callInfo);

        // Move argument values to parameter locations
        passParametersToCall(arguments, callInfo);

        if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
            m_jit.move(TrustedImmPtr(Instance::offsetOfImportFunctionStub(functionIndex)), m_scratchGPR);
            m_jit.addPtr(GPRInfo::wasmContextInstancePointer, m_scratchGPR);
            m_jit.loadPtr(Address(m_scratchGPR), m_scratchGPR);

            m_jit.call(m_scratchGPR, WasmEntryPtrTag);
        } else {
            // Emit the call.
            Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;
            CCallHelpers::Call call = m_jit.threadSafePatchableNearCall();
            m_jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
            });
        }

        // Push return value(s) onto the expression stack
        returnValuesFromCall(results, functionType, callInfo);

        if (m_info.callCanClobberInstance(functionIndex) || m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex))
            restoreWebAssemblyGlobalState();

        LOG_INSTRUCTION("Call", functionIndex, arguments, "=> ", results);

        return { };
    }

    void emitIndirectCall(const char* opcode, const Value& calleeIndex, RegisterID calleeInstance, RegisterID calleeCode, RegisterID jsCalleeAnchor, const TypeDefinition& signature, Vector<Value>& arguments, ResultList& results, CallType callType = CallType::Call)
    {
        // TODO: Support tail calls
        UNUSED_PARAM(jsCalleeAnchor);
        RELEASE_ASSERT(callType == CallType::Call);

        const auto& callingConvention = wasmCallingConvention();
        CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
        Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCalleeInfo.headerAndArgumentStackSizeInBytes);
        m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

        // Do a context switch if needed.
        Jump isSameInstance = m_jit.branchPtr(RelationalCondition::Equal, calleeInstance, GPRInfo::wasmContextInstancePointer);
        m_jit.move(calleeInstance, GPRInfo::wasmContextInstancePointer);
        m_jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, TrustedImm32(Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
        m_jit.cageConditionallyAndUntag(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, m_scratchGPR, /* validateAuth */ true, /* mayBeNull */ false);
        isSameInstance.link(&m_jit);

        // Since this can switch instance, we need to keep JSWebAssemblyInstance anchored in the stack.
        m_jit.storePtr(jsCalleeAnchor, Location::fromArgumentLocation(wasmCalleeInfo.thisArgument).asAddress());

        // Safe to use across saveValues/passParameters since neither clobber the scratch GPR.
        m_jit.loadPtr(Address(calleeCode), m_scratchGPR);
        saveValuesAcrossCall(wasmCalleeInfo);
        passParametersToCall(arguments, wasmCalleeInfo);
        m_jit.call(m_scratchGPR, WasmEntryPtrTag);
        returnValuesFromCall(results, *signature.as<FunctionSignature>(), wasmCalleeInfo);

        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState();

        LOG_INSTRUCTION(opcode, calleeIndex, arguments, "=> ", results);
    }

    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const TypeDefinition& originalSignature, Vector<Value>& args, ResultList& results, CallType callType = CallType::Call)
    {
        Value calleeIndex = args.takeLast();
        const TypeDefinition& signature = originalSignature.expand();
        ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
        ASSERT(m_info.tableCount() > tableIndex);
        ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

        Location calleeIndexLocation;
        RegisterID calleeInstance, calleeCode, jsCalleeAnchor;

        {
            ScratchScope<3, 0> scratches(*this);

            if (calleeIndex.isConst())
                emitMoveConst(calleeIndex, calleeIndexLocation = Location::fromGPR(scratches.gpr(2)));
            else
                calleeIndexLocation = loadIfNecessary(calleeIndex);

            RegisterID callableFunctionBufferLength = scratches.gpr(0);
            RegisterID callableFunctionBuffer = scratches.gpr(1);

            ASSERT(tableIndex < m_info.tableCount());

            int numImportFunctions = m_info.importFunctionCount();
            m_jit.loadPtr(Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfTablePtr(numImportFunctions, tableIndex)), callableFunctionBufferLength);
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
            m_jit.zeroExtend32ToWord(calleeIndexLocation.asGPR(), calleeIndexLocation.asGPR());

            consume(calleeIndex);

            // Check the index we are looking for is valid.
            throwExceptionIf(ExceptionType::OutOfBoundsCallIndirect, m_jit.branch32(RelationalCondition::AboveOrEqual, calleeIndexLocation.asGPR(), callableFunctionBufferLength));

            // Neither callableFunctionBuffer nor callableFunctionBufferLength are used before any of these
            // are def'd below, so we can reuse the registers and save some pressure.
            calleeCode = scratches.gpr(0);
            calleeInstance = scratches.gpr(1);
            jsCalleeAnchor = scratches.gpr(2);

            static_assert(sizeof(TypeIndex) == sizeof(void*));
            RegisterID calleeSignatureIndex = m_scratchGPR;

            // Compute the offset in the table index space we are looking for.
            m_jit.move(TrustedImmPtr(sizeof(FuncRefTable::Function)), calleeSignatureIndex);
            m_jit.mul64(calleeIndexLocation.asGPR(), calleeSignatureIndex);
            m_jit.addPtr(callableFunctionBuffer, calleeSignatureIndex);

            // FIXME: This seems wasteful to do two checks just for a nicer error message.
            // We should move just to use a single branch and then figure out what
            // error to use in the exception handler.

            m_jit.loadPtr(Address(calleeSignatureIndex, FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.
            m_jit.loadPtr(Address(calleeSignatureIndex, FuncRefTable::Function::offsetOfInstance()), calleeInstance);
            m_jit.loadPtr(Address(calleeSignatureIndex, FuncRefTable::Function::offsetOfValue()), jsCalleeAnchor);
            m_jit.loadPtr(Address(calleeSignatureIndex, FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfSignatureIndex()), calleeSignatureIndex);

            throwExceptionIf(ExceptionType::NullTableEntry, m_jit.branchTestPtr(ResultCondition::Zero, calleeSignatureIndex, calleeSignatureIndex));
            throwExceptionIf(ExceptionType::BadSignature, m_jit.branchPtr(RelationalCondition::NotEqual, calleeSignatureIndex, TrustedImmPtr(TypeInformation::get(originalSignature))));
        }

        emitIndirectCall("CallIndirect", calleeIndex, calleeInstance, calleeCode, jsCalleeAnchor, signature, args, results, callType);
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<Value>&, ResultList&) BBQ_STUB

    PartialResult WARN_UNUSED_RETURN addUnreachable()
    {
        LOG_INSTRUCTION("Unreachable");
        emitThrowException(ExceptionType::Unreachable);
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t, Vector<Value>&, Value&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addStructGet(Value, const StructType&, uint32_t, Value&) BBQ_STUB
    PartialResult WARN_UNUSED_RETURN addStructSet(Value, const StructType&, uint32_t, Value) BBQ_STUB

    PartialResult WARN_UNUSED_RETURN addCrash()
    {
        m_jit.breakpoint();
        return { };
    }

    // SIMD

    void notifyFunctionUsesSIMD()
    { }

    PartialResult WARN_UNUSED_RETURN addSIMDLoad(ExpressionType pointer, uint32_t uoffset, ExpressionType& result)
    {
        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        Address address = materializePointer(pointerLocation, uoffset);
        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);
        m_jit.loadVector(address, resultLocation.asFPR());
        LOG_INSTRUCTION("V128Load", pointer, pointerLocation, uoffset, RESULT(result));

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDStore(ExpressionType value, ExpressionType pointer, uint32_t uoffset)
    {
        Location valueLocation = loadIfNecessary(value);
        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        consume(value);
        Address address = materializePointer(pointerLocation, uoffset);
        m_jit.storeVector(valueLocation.asFPR(), address);
        LOG_INSTRUCTION("V128Store", pointer, pointerLocation, uoffset, value, valueLocation);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDSplat(SIMDLane lane, ExpressionType value, ExpressionType& result)
    {
        Location valueLocation;
        if (value.isConst()) {
            if (value.isFloat()) {
                ScratchScope<0, 1> scratches(*this);
                valueLocation = Location::fromFPR(scratches.fpr(0));
            } else {
                ScratchScope<1, 0> scratches(*this);
                valueLocation = Location::fromGPR(scratches.gpr(0));
            }
            emitMoveConst(value, valueLocation);
        } else
            valueLocation = loadIfNecessary(value);
        consume(value);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);
        if (valueLocation.isGPR())
            m_jit.vectorSplat(lane, valueLocation.asGPR(), resultLocation.asFPR());
        else
            m_jit.vectorSplat(lane, valueLocation.asFPR(), resultLocation.asFPR());

        LOG_INSTRUCTION("VectorSplat", lane, value, valueLocation, RESULT(result));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDShuffle(v128_t imm, ExpressionType a, ExpressionType b, ExpressionType& result)
    {
#if CPU(X86_64)
        ScratchScope<0, 1> scratches(*this);
#elif CPU(ARM64)
        // We need these adjacent registers for the tbl instruction, so we clobber and preserve them in this scope here.
        clobber(ARM64Registers::q28);
        clobber(ARM64Registers::q29);
        ScratchScope<0, 0> scratches(*this, Location::fromFPR(ARM64Registers::q28), Location::fromFPR(ARM64Registers::q29));
#endif
        Location aLocation = loadIfNecessary(a);
        Location bLocation = loadIfNecessary(b);
        consume(a);
        consume(b);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("VectorShuffle", a, aLocation, b, bLocation, RESULT(result));

        if constexpr (isX86()) {
            v128_t leftImm = imm;
            v128_t rightImm = imm;
            for (unsigned i = 0; i < 16; ++i) {
                if (leftImm.u8x16[i] > 15)
                    leftImm.u8x16[i] = 0xFF; // Force OOB
                if (rightImm.u8x16[i] < 16 || rightImm.u8x16[i] > 31)
                    rightImm.u8x16[i] = 0xFF; // Force OOB
            }
            // Store each byte (w/ index < 16) of `a` to result
            // and zero clear each byte (w/ index > 15) in result.
            materializeVectorConstant(leftImm, Location::fromFPR(scratches.fpr(0)));
            m_jit.vectorSwizzle(aLocation.asFPR(), scratches.fpr(0), scratches.fpr(0));

            // Store each byte (w/ index - 16 >= 0) of `b` to result2
            // and zero clear each byte (w/ index - 16 < 0) in result2.
            materializeVectorConstant(rightImm, Location::fromFPR(m_scratchFPR));
            m_jit.vectorSwizzle(bLocation.asFPR(), m_scratchFPR, m_scratchFPR);
            m_jit.vectorOr(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, scratches.fpr(0), m_scratchFPR, resultLocation.asFPR());
            return { };
        }

#if CPU(ARM64)
        materializeVectorConstant(imm, Location::fromFPR(m_scratchFPR));
        if (unsigned(aLocation.asFPR()) + 1 != unsigned(bLocation.asFPR())) {
            m_jit.moveVector(aLocation.asFPR(), ARM64Registers::q28);
            m_jit.moveVector(bLocation.asFPR(), ARM64Registers::q29);
            aLocation = Location::fromFPR(ARM64Registers::q28);
            bLocation = Location::fromFPR(ARM64Registers::q29);
        }
        m_jit.vectorSwizzle2(aLocation.asFPR(), bLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
        UNREACHABLE_FOR_PLATFORM();
#endif

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDShift(SIMDLaneOperation op, SIMDInfo info, ExpressionType src, ExpressionType shift, ExpressionType& result)
    {
#if CPU(X86_64)
        // Clobber and preserve RCX on x86, since we need it to do shifts.
        clobber(m_shiftRCX);
        ScratchScope<2, 2> scratches(*this, Location::fromGPR(m_shiftRCX));
#endif
        Location srcLocation = loadIfNecessary(src);
        Location shiftLocation;
        if (shift.isConst()) {
            shiftLocation = Location::fromGPR(m_scratchGPR);
            emitMoveConst(shift, shiftLocation);
        } else
            shiftLocation = loadIfNecessary(shift);
        consume(src);
        consume(shift);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        int32_t mask = (elementByteSize(info.lane) * CHAR_BIT) - 1;

        LOG_INSTRUCTION("Vector", op, src, srcLocation, shift, shiftLocation, RESULT(result));

#if CPU(ARM64)
        m_jit.and32(Imm32(mask), shiftLocation.asGPR(), m_scratchGPR);
        if (op == SIMDLaneOperation::Shr) {
            // ARM64 doesn't have a version of this instruction for right shift. Instead, if the input to
            // left shift is negative, it's a right shift by the absolute value of that amount.
            m_jit.neg32(m_scratchGPR);
        }
        m_jit.vectorSplatInt8(m_scratchGPR, m_scratchFPR);
        if (info.signMode == SIMDSignMode::Signed)
            m_jit.vectorSshl(info, srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
        else
            m_jit.vectorUshl(info, srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
        ASSERT(isX86());
        m_jit.move(shiftLocation.asGPR(), m_scratchGPR);
        m_jit.and32(Imm32(mask), m_scratchGPR);

        if (op == SIMDLaneOperation::Shr && info.signMode == SIMDSignMode::Signed && info.lane == SIMDLane::i64x2) {
            // x86 has no SIMD 64-bit signed right shift instruction, so we scalarize it here.
            m_jit.move(m_scratchGPR, m_shiftRCX);
            m_jit.vectorExtractLaneInt64(TrustedImm32(0), srcLocation.asFPR(), scratches.gpr(0));
            m_jit.vectorExtractLaneInt64(TrustedImm32(1), srcLocation.asFPR(), scratches.gpr(1));
            m_jit.rshift64(m_shiftRCX, scratches.gpr(0));
            m_jit.rshift64(m_shiftRCX, scratches.gpr(1));
            m_jit.vectorReplaceLaneInt64(TrustedImm32(0), scratches.gpr(0), resultLocation.asFPR());
            m_jit.vectorReplaceLaneInt64(TrustedImm32(1), scratches.gpr(1), resultLocation.asFPR());
            return { };
        }

        // Unlike ARM, x86 expects the shift provided as a *scalar*, stored in the lower 64 bits of a vector register.
        // So, we don't need to splat the shift amount like we do on ARM.
        m_jit.move64ToDouble(m_scratchGPR, m_scratchFPR);

        // 8-bit shifts are pretty involved to implement on Intel, so they get their own instruction type with extra temps.
        if (op == SIMDLaneOperation::Shl && info.lane == SIMDLane::i8x16) {
            m_jit.vectorUshl8(srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR(), scratches.fpr(0), scratches.fpr(1));
            return { };
        }
        if (op == SIMDLaneOperation::Shr && info.lane == SIMDLane::i8x16) {
            if (info.signMode == SIMDSignMode::Signed)
                m_jit.vectorSshr8(srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR(), scratches.fpr(0), scratches.fpr(1));
            else
                m_jit.vectorUshr8(srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR(), scratches.fpr(0), scratches.fpr(1));
            return { };
        }

        if (op == SIMDLaneOperation::Shl)
            m_jit.vectorUshl(info, srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
        else if (info.signMode == SIMDSignMode::Signed)
            m_jit.vectorSshr(info, srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
        else
            m_jit.vectorUshr(info, srcLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#endif
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDExtmul(SIMDLaneOperation op, SIMDInfo info, ExpressionType left, ExpressionType right, ExpressionType& result)
    {
        ASSERT(info.signMode != SIMDSignMode::None);

        Location leftLocation = loadIfNecessary(left);
        Location rightLocation = loadIfNecessary(right);
        consume(left);
        consume(right);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, left, leftLocation, right, rightLocation, RESULT(result));

        if (op == SIMDLaneOperation::ExtmulLow) {
            m_jit.vectorExtendLow(info, leftLocation.asFPR(), m_scratchFPR);
            m_jit.vectorExtendLow(info, rightLocation.asFPR(), resultLocation.asFPR());
        } else {
            ASSERT(op == SIMDLaneOperation::ExtmulHigh);
            m_jit.vectorExtendHigh(info, leftLocation.asFPR(), m_scratchFPR);
            m_jit.vectorExtendHigh(info, rightLocation.asFPR(), resultLocation.asFPR());
        }
        emitVectorMul(info, Location::fromFPR(m_scratchFPR), resultLocation, resultLocation);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDLoadSplat(SIMDLaneOperation op, ExpressionType pointer, uint32_t uoffset, ExpressionType& result)
    {
        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        Address address = materializePointer(pointerLocation, uoffset);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, pointer, pointerLocation, uoffset, RESULT(result));

        switch (op) {
#if CPU(X86_64)
        case SIMDLaneOperation::LoadSplat8:
            m_jit.vectorLoad8Splat(address, resultLocation.asFPR(), m_scratchFPR);
            break;
#else
        case SIMDLaneOperation::LoadSplat8:
            m_jit.vectorLoad8Splat(address, resultLocation.asFPR());
            break;
#endif
        case SIMDLaneOperation::LoadSplat16:
            m_jit.vectorLoad16Splat(address, resultLocation.asFPR());
            break;
        case SIMDLaneOperation::LoadSplat32:
            m_jit.vectorLoad32Splat(address, resultLocation.asFPR());
            break;
        case SIMDLaneOperation::LoadSplat64:
            m_jit.vectorLoad64Splat(address, resultLocation.asFPR());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDLoadLane(SIMDLaneOperation op, ExpressionType pointer, ExpressionType vector, uint32_t uoffset, uint8_t lane, ExpressionType& result)
    {
        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        Address address = materializePointer(pointerLocation, uoffset);

        Location vectorLocation = loadIfNecessary(vector);
        consume(vector);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, pointer, pointerLocation, uoffset, RESULT(result));

        m_jit.move(vectorLocation.asFPR(), resultLocation.asFPR());
        switch (op) {
        case SIMDLaneOperation::LoadLane8:
            m_jit.vectorLoad8Lane(address, TrustedImm32(lane), resultLocation.asFPR());
            break;
        case SIMDLaneOperation::LoadLane16:
            m_jit.vectorLoad16Lane(address, TrustedImm32(lane), resultLocation.asFPR());
            break;
        case SIMDLaneOperation::LoadLane32:
            m_jit.vectorLoad32Lane(address, TrustedImm32(lane), resultLocation.asFPR());
            break;
        case SIMDLaneOperation::LoadLane64:
            m_jit.vectorLoad64Lane(address, TrustedImm32(lane), resultLocation.asFPR());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDStoreLane(SIMDLaneOperation op, ExpressionType pointer, ExpressionType vector, uint32_t uoffset, uint8_t lane)
    {
        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        Address address = materializePointer(pointerLocation, uoffset);

        Location vectorLocation = loadIfNecessary(vector);
        consume(vector);

        LOG_INSTRUCTION("Vector", op, vector, vectorLocation, pointer, pointerLocation, uoffset);

        switch (op) {
        case SIMDLaneOperation::StoreLane8:
            m_jit.vectorStore8Lane(vectorLocation.asFPR(), address, TrustedImm32(lane));
            break;
        case SIMDLaneOperation::StoreLane16:
            m_jit.vectorStore16Lane(vectorLocation.asFPR(), address, TrustedImm32(lane));
            break;
        case SIMDLaneOperation::StoreLane32:
            m_jit.vectorStore32Lane(vectorLocation.asFPR(), address, TrustedImm32(lane));
            break;
        case SIMDLaneOperation::StoreLane64:
            m_jit.vectorStore64Lane(vectorLocation.asFPR(), address, TrustedImm32(lane));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDLoadExtend(SIMDLaneOperation op, ExpressionType pointer, uint32_t uoffset, ExpressionType& result)
    {
        SIMDLane lane;
        SIMDSignMode signMode;

        switch (op) {
        case SIMDLaneOperation::LoadExtend8U:
            lane = SIMDLane::i16x8;
            signMode = SIMDSignMode::Unsigned;
            break;
        case SIMDLaneOperation::LoadExtend8S:
            lane = SIMDLane::i16x8;
            signMode = SIMDSignMode::Signed;
            break;
        case SIMDLaneOperation::LoadExtend16U:
            lane = SIMDLane::i32x4;
            signMode = SIMDSignMode::Unsigned;
            break;
        case SIMDLaneOperation::LoadExtend16S:
            lane = SIMDLane::i32x4;
            signMode = SIMDSignMode::Signed;
            break;
        case SIMDLaneOperation::LoadExtend32U:
            lane = SIMDLane::i64x2;
            signMode = SIMDSignMode::Unsigned;
            break;
        case SIMDLaneOperation::LoadExtend32S:
            lane = SIMDLane::i64x2;
            signMode = SIMDSignMode::Signed;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        Address address = materializePointer(pointerLocation, uoffset);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, pointer, pointerLocation, uoffset, RESULT(result));

        m_jit.loadDouble(address, resultLocation.asFPR());
        m_jit.vectorExtendLow(SIMDInfo { lane, signMode }, resultLocation.asFPR(), resultLocation.asFPR());

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDLoadPad(SIMDLaneOperation op, ExpressionType pointer, uint32_t uoffset, ExpressionType& result)
    {
        Location pointerLocation = emitCheckAndPreparePointer(pointer, uoffset, bytesForWidth(Width::Width128));
        Address address = materializePointer(pointerLocation, uoffset);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, pointer, pointerLocation, uoffset, RESULT(result));

        if (op == SIMDLaneOperation::LoadPad32)
            m_jit.loadFloat(address, resultLocation.asFPR());
        else {
            ASSERT(op == SIMDLaneOperation::LoadPad64);
            m_jit.loadDouble(address, resultLocation.asFPR());
        }

        return { };
    }

    void materializeVectorConstant(v128_t value, Location result)
    {
        if (!value.u64x2[0] && !value.u64x2[1])
            m_jit.moveZeroToVector(result.asFPR());
        else if (value.u64x2[0] == 0xffffffffffffffffull && value.u64x2[1] == 0xffffffffffffffffull)
#if CPU(X86_64)
            m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::Unsigned }, result.asFPR(), result.asFPR(), result.asFPR(), m_scratchFPR);
#else
            m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::Unsigned }, result.asFPR(), result.asFPR(), result.asFPR());
#endif
        else
            m_jit.materializeVector(value, result.asFPR());
    }

    ExpressionType WARN_UNUSED_RETURN addConstant(v128_t value)
    {
        // We currently don't track constant Values for V128s, since folding them seems like a lot of work that might not be worth it.
        // Maybe we can look into this eventually?
        Value temp = topValue(TypeKind::V128);
        Location tempLocation = allocate(temp);
        materializeVectorConstant(value, tempLocation);
        LOG_INSTRUCTION("V128Const", value, RESULT(temp));
        return temp;
    }

    // SIMD generated

    PartialResult WARN_UNUSED_RETURN addExtractLane(SIMDInfo info, uint8_t lane, Value value, Value& result)
    {
        Location valueLocation = loadIfNecessary(value);
        consume(value);

        result = topValue(simdScalarType(info.lane).kind);
        Location resultLocation = allocate(result);
        LOG_INSTRUCTION("VectorExtractLane", info.lane, lane, value, valueLocation, RESULT(result));

        if (scalarTypeIsFloatingPoint(info.lane))
            m_jit.vectorExtractLane(info.lane, TrustedImm32(lane), valueLocation.asFPR(), resultLocation.asFPR());
        else
            m_jit.vectorExtractLane(info.lane, info.signMode, TrustedImm32(lane), valueLocation.asFPR(), resultLocation.asGPR());
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addReplaceLane(SIMDInfo info, uint8_t lane, ExpressionType vector, ExpressionType scalar, ExpressionType& result)
    {
        Location vectorLocation = loadIfNecessary(vector);
        Location scalarLocation;
        if (scalar.isConst()) {
            scalarLocation = scalar.isFloat() ? Location::fromFPR(m_scratchFPR) : Location::fromGPR(m_scratchGPR);
            emitMoveConst(scalar, scalarLocation);
        } else
            scalarLocation = loadIfNecessary(scalar);
        consume(vector);
        consume(scalar);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        if (scalarLocation == resultLocation) {
            m_jit.moveVector(scalarLocation.asFPR(), m_scratchFPR);
            scalarLocation = Location::fromFPR(m_scratchFPR);
        }

        LOG_INSTRUCTION("VectorReplaceLane", info.lane, lane, vector, vectorLocation, scalar, scalarLocation, RESULT(result));

        m_jit.moveVector(vectorLocation.asFPR(), resultLocation.asFPR());
        if (scalarLocation.isFPR())
            m_jit.vectorReplaceLane(info.lane, TrustedImm32(lane), scalarLocation.asFPR(), resultLocation.asFPR());
        else
            m_jit.vectorReplaceLane(info.lane, TrustedImm32(lane), scalarLocation.asGPR(), resultLocation.asFPR());
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDI_V(SIMDLaneOperation op, SIMDInfo info, ExpressionType value, ExpressionType& result)
    {
        Location valueLocation = loadIfNecessary(value);
        consume(value);

        result = topValue(TypeKind::I32);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, value, valueLocation, RESULT(result));

        switch (op) {
        case SIMDLaneOperation::Bitmask:
#if CPU(ARM64)
            if (info.lane == SIMDLane::i64x2) {
                // This might look bad, but remember: every bit of information we destroy contributes to the heat death of the universe.
                m_jit.vectorSshr8(SIMDInfo { SIMDLane::i64x2, SIMDSignMode::None }, valueLocation.asFPR(), TrustedImm32(63), m_scratchFPR);
                m_jit.vectorUnzipEven(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR);
                m_jit.moveDoubleTo64(m_scratchFPR, m_scratchGPR);
                m_jit.rshift64(m_scratchGPR, TrustedImm32(31), m_scratchGPR);
                m_jit.and32(Imm32(0b11), m_scratchGPR, resultLocation.asGPR());
                return { };
            }

            {
                v128_t towerOfPower { };
                switch (info.lane) {
                case SIMDLane::i32x4:
                    for (unsigned i = 0; i < 4; ++i)
                        towerOfPower.u32x4[i] = 1 << i;
                    break;
                case SIMDLane::i16x8:
                    for (unsigned i = 0; i < 8; ++i)
                        towerOfPower.u16x8[i] = 1 << i;
                    break;
                case SIMDLane::i8x16:
                    for (unsigned i = 0; i < 8; ++i)
                        towerOfPower.u8x16[i] = 1 << i;
                    for (unsigned i = 0; i < 8; ++i)
                        towerOfPower.u8x16[i + 8] = 1 << i;
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }

                // FIXME: this is bad, we should load
                materializeVectorConstant(towerOfPower, Location::fromFPR(m_scratchFPR));
            }

            {
                ScratchScope<0, 1> scratches(*this, valueLocation, resultLocation);

                m_jit.vectorSshr8(info, valueLocation.asFPR(), TrustedImm32(elementByteSize(info.lane) * 8 - 1), scratches.fpr(0));
                m_jit.vectorAnd(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, scratches.fpr(0), m_scratchFPR, scratches.fpr(0));

                if (info.lane == SIMDLane::i8x16) {
                    m_jit.vectorExtractPair(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::None }, TrustedImm32(8), scratches.fpr(0), scratches.fpr(0), m_scratchFPR);
                    m_jit.vectorZipUpper(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::None }, scratches.fpr(0), m_scratchFPR, scratches.fpr(0));
                    info.lane = SIMDLane::i16x8;
                }

                m_jit.vectorHorizontalAdd(info, scratches.fpr(0), scratches.fpr(0));
                m_jit.moveFloatTo32(scratches.fpr(0), resultLocation.asGPR());
            }
#else
            ASSERT(isX86());
            m_jit.vectorBitmask(info, valueLocation.asFPR(), resultLocation.asGPR(), m_scratchFPR);
#endif
            return { };
        case JSC::SIMDLaneOperation::AnyTrue:
#if CPU(ARM64)
            m_jit.vectorUnsignedMax(SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, valueLocation.asFPR(), m_scratchFPR);
            m_jit.moveFloatTo32(m_scratchFPR, resultLocation.asGPR());
            m_jit.test32(ResultCondition::NonZero, resultLocation.asGPR(), resultLocation.asGPR(), resultLocation.asGPR());
#else
            m_jit.vectorAnyTrue(valueLocation.asFPR(), resultLocation.asGPR());
#endif
            return { };
        case JSC::SIMDLaneOperation::AllTrue:
#if CPU(ARM64)
            ASSERT(scalarTypeIsIntegral(info.lane));
            switch (info.lane) {
            case SIMDLane::i64x2:
                m_jit.compareIntegerVectorWithZero(RelationalCondition::NotEqual, info, valueLocation.asFPR(), m_scratchFPR);
                m_jit.vectorUnsignedMin(SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR);
                break;
            case SIMDLane::i32x4:
            case SIMDLane::i16x8:
            case SIMDLane::i8x16:
                m_jit.vectorUnsignedMin(info, valueLocation.asFPR(), m_scratchFPR);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            m_jit.moveFloatTo32(m_scratchFPR, m_scratchGPR);
            m_jit.test32(ResultCondition::NonZero, m_scratchGPR, m_scratchGPR, resultLocation.asGPR());
#else
            ASSERT(isX86());
            m_jit.vectorAllTrue(info, valueLocation.asFPR(), resultLocation.asGPR(), m_scratchFPR);
#endif
            return { };
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    PartialResult WARN_UNUSED_RETURN addSIMDV_V(SIMDLaneOperation op, SIMDInfo info, ExpressionType value, ExpressionType& result)
    {
        Location valueLocation = loadIfNecessary(value);
        consume(value);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, value, valueLocation, RESULT(result));

        switch (op) {
        case JSC::SIMDLaneOperation::Demote:
            m_jit.vectorDemote(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Promote:
            m_jit.vectorPromote(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Abs:
#if CPU(X86_64)
            if (info.lane == SIMDLane::i64x2) {
                m_jit.vectorAbsInt64(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
                return { };
            }
            if (scalarTypeIsFloatingPoint(info.lane)) {
                if (info.lane == SIMDLane::f32x4) {
                    m_jit.move32ToFloat(TrustedImm32(0x7fffffff), m_scratchFPR);
                    m_jit.vectorSplatFloat32(m_scratchFPR, m_scratchFPR);
                } else {
                    m_jit.move64ToDouble(TrustedImm64(0x7fffffffffffffffll), m_scratchFPR);
                    m_jit.vectorSplatFloat64(m_scratchFPR, m_scratchFPR);
                }
                m_jit.vectorAnd(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, valueLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                return { };
            }
#endif
            m_jit.vectorAbs(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Popcnt:
#if CPU(X86_64)
            {
                ScratchScope<0, 1> scratches(*this, valueLocation, resultLocation);
                ASSERT(info.lane == SIMDLane::i8x16);

                // x86_64 does not natively support vector lanewise popcount, so we emulate it using multiple
                // masks.

                v128_t bottomNibbleConst;
                v128_t popcntConst;
                bottomNibbleConst.u64x2[0] = 0x0f0f0f0f0f0f0f0f;
                bottomNibbleConst.u64x2[1] = 0x0f0f0f0f0f0f0f0f;
                popcntConst.u64x2[0] = 0x0302020102010100;
                popcntConst.u64x2[1] = 0x0403030203020201;

                materializeVectorConstant(bottomNibbleConst, Location::fromFPR(scratches.fpr(0)));
                m_jit.vectorAndnot(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, valueLocation.asFPR(), scratches.fpr(0), m_scratchFPR);
                m_jit.vectorAnd(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, valueLocation.asFPR(), scratches.fpr(0), resultLocation.asFPR());
                m_jit.vectorUshr8(SIMDInfo { SIMDLane::i16x8, SIMDSignMode::None }, m_scratchFPR, TrustedImm32(4), m_scratchFPR);

                materializeVectorConstant(popcntConst, Location::fromFPR(scratches.fpr(0)));
                m_jit.vectorSwizzle(scratches.fpr(0), resultLocation.asFPR(), resultLocation.asFPR());
                m_jit.vectorSwizzle(scratches.fpr(0), m_scratchFPR, m_scratchFPR);
                m_jit.vectorAdd(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
            }
#else
            m_jit.vectorPopcnt(info, valueLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case JSC::SIMDLaneOperation::Ceil:
            m_jit.vectorCeil(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Floor:
            m_jit.vectorFloor(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Trunc:
            m_jit.vectorTrunc(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Nearest:
            m_jit.vectorNearest(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::Sqrt:
            m_jit.vectorSqrt(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::ExtaddPairwise:
#if CPU(X86_64)
            if (info.lane == SIMDLane::i16x8 && info.signMode == SIMDSignMode::Unsigned) {
                m_jit.vectorExtaddPairwiseUnsignedInt16(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
                return { };
            }
            m_jit.vectorExtaddPairwise(info, valueLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR);
#else
            m_jit.vectorExtaddPairwise(info, valueLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case JSC::SIMDLaneOperation::Convert:
#if CPU(X86_64)
            if (info.signMode == SIMDSignMode::Unsigned) {
                m_jit.vectorConvertUnsigned(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
                return { };
            }
#endif
            m_jit.vectorConvert(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::ConvertLow:
#if CPU(X86_64)
            if (info.signMode == SIMDSignMode::Signed)
                m_jit.vectorConvertLowSignedInt32(valueLocation.asFPR(), resultLocation.asFPR());
            else
                m_jit.vectorConvertLowUnsignedInt32(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR);
#else
            m_jit.vectorConvertLow(info, valueLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case JSC::SIMDLaneOperation::ExtendHigh:
            m_jit.vectorExtendHigh(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::ExtendLow:
            m_jit.vectorExtendLow(info, valueLocation.asFPR(), resultLocation.asFPR());
            return { };
        case JSC::SIMDLaneOperation::TruncSat:
#if CPU(X86_64)
            switch (info.lane) {
            case SIMDLane::f64x2:
                if (info.signMode == SIMDSignMode::Signed)
                    m_jit.vectorTruncSatSignedFloat64(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR);
                else
                    m_jit.vectorTruncSatUnsignedFloat64(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR);
                break;
            case SIMDLane::f32x4: {
                ScratchScope<0, 1> scratches(*this, valueLocation, resultLocation);
                if (info.signMode == SIMDSignMode::Signed)
                    m_jit.vectorTruncSat(info, valueLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR, scratches.fpr(0));
                else
                    m_jit.vectorTruncSatUnsignedFloat32(valueLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR, scratches.fpr(0));
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
#else
            m_jit.vectorTruncSat(info, valueLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case JSC::SIMDLaneOperation::Not: {
#if CPU(X86_64)
            ScratchScope<0, 1> scratches(*this, valueLocation, resultLocation);
            m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR, scratches.fpr(0));
            m_jit.vectorXor(info, valueLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
            m_jit.vectorNot(info, valueLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        }
        case JSC::SIMDLaneOperation::Neg:
#if CPU(X86_64)
            switch (info.lane) {
            case SIMDLane::i8x16:
            case SIMDLane::i16x8:
            case SIMDLane::i32x4:
            case SIMDLane::i64x2:
                // For integers, we can negate by subtracting our input from zero.
                m_jit.moveZeroToVector(m_scratchFPR);
                m_jit.vectorSub(info, m_scratchFPR, valueLocation.asFPR(), resultLocation.asFPR());
                break;
            case SIMDLane::f32x4:
                // For floats, we unfortunately have to flip the sign bit using XOR.
                m_jit.move32ToFloat(TrustedImm32(-0x80000000), m_scratchFPR);
                m_jit.vectorSplatFloat32(m_scratchFPR, m_scratchFPR);
                m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, valueLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                break;
            case SIMDLane::f64x2:
                m_jit.move64ToDouble(TrustedImm64(-0x8000000000000000ll), m_scratchFPR);
                m_jit.vectorSplatFloat64(m_scratchFPR, m_scratchFPR);
                m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, valueLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
#else
            m_jit.vectorNeg(info, valueLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    PartialResult WARN_UNUSED_RETURN addSIMDBitwiseSelect(ExpressionType left, ExpressionType right, ExpressionType selector, ExpressionType& result)
    {
        Location leftLocation = loadIfNecessary(left);
        Location rightLocation = loadIfNecessary(right);
        Location selectorLocation = loadIfNecessary(selector);
        consume(left);
        consume(right);
        consume(selector);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("VectorBitwiseSelect", left, leftLocation, right, rightLocation, selector, selectorLocation, RESULT(result));

#if CPU(X86_64)
        m_jit.vectorAnd(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, leftLocation.asFPR(), selectorLocation.asFPR(), m_scratchFPR);
        m_jit.vectorAndnot(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, rightLocation.asFPR(), selectorLocation.asFPR(), resultLocation.asFPR());
        m_jit.vectorOr(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
#else
        m_jit.moveVector(selectorLocation.asFPR(), m_scratchFPR);
        m_jit.vectorBitwiseSelect(leftLocation.asFPR(), rightLocation.asFPR(), m_scratchFPR);
        m_jit.moveVector(m_scratchFPR, resultLocation.asFPR());
#endif
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDRelOp(SIMDLaneOperation op, SIMDInfo info, ExpressionType left, ExpressionType right, Air::Arg relOp, ExpressionType& result)
    {
        Location leftLocation = loadIfNecessary(left);
        Location rightLocation = loadIfNecessary(right);
        consume(left);
        consume(right);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, left, leftLocation, right, rightLocation, RESULT(result));

        if (scalarTypeIsFloatingPoint(info.lane)) {
            m_jit.compareFloatingPointVector(relOp.asDoubleCondition(), info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        }

#if CPU(X86_64)
        // On Intel, the best codegen for a bitwise-complement of an integer vector is to
        // XOR with a vector of all ones. This is necessary here since Intel also doesn't
        // directly implement most relational conditions between vectors: the cases below
        // are best emitted as inversions of conditions that are supported.
        switch (relOp.asRelationalCondition()) {
        case MacroAssembler::NotEqual: {
            ScratchScope<0, 1> scratches(*this, leftLocation, rightLocation, resultLocation);
            m_jit.compareIntegerVector(RelationalCondition::Equal, info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
            m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR, scratches.fpr(0));
            m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
            break;
        }
        case MacroAssembler::Above: {
            ScratchScope<0, 1> scratches(*this, leftLocation, rightLocation, resultLocation);
            m_jit.compareIntegerVector(RelationalCondition::BelowOrEqual, info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
            m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR, scratches.fpr(0));
            m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
            break;
        }
        case MacroAssembler::Below: {
            ScratchScope<0, 1> scratches(*this, leftLocation, rightLocation, resultLocation);
            m_jit.compareIntegerVector(RelationalCondition::AboveOrEqual, info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
            m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR, scratches.fpr(0));
            m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
            break;
        }
        case MacroAssembler::GreaterThanOrEqual:
            if (info.lane == SIMDLane::i64x2) {
                // Note: rhs and lhs are reversed here, we are semantically negating LessThan. GreaterThan is
                // just better supported on AVX.
                ScratchScope<0, 1> scratches(*this, leftLocation, rightLocation, resultLocation);
                m_jit.compareIntegerVector(RelationalCondition::GreaterThan, info, rightLocation.asFPR(), leftLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
                m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR, scratches.fpr(0));
                m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
            } else
                m_jit.compareIntegerVector(relOp.asRelationalCondition(), info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
            break;
        case MacroAssembler::LessThanOrEqual:
            if (info.lane == SIMDLane::i64x2) {
                ScratchScope<0, 1> scratches(*this, leftLocation, rightLocation, resultLocation);
                m_jit.compareIntegerVector(RelationalCondition::GreaterThan, info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
                m_jit.compareIntegerVector(RelationalCondition::Equal, SIMDInfo { SIMDLane::i32x4, SIMDSignMode::None }, m_scratchFPR, m_scratchFPR, m_scratchFPR, scratches.fpr(0));
                m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
            } else
                m_jit.compareIntegerVector(relOp.asRelationalCondition(), info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
            break;
        default:
            m_jit.compareIntegerVector(relOp.asRelationalCondition(), info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
        }
#else
        m_jit.compareIntegerVector(relOp.asRelationalCondition(), info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
        return { };
    }

    void emitVectorMul(SIMDInfo info, Location left, Location right, Location result)
    {
        if (info.lane == SIMDLane::i64x2) {
            // Multiplication of 64-bit ints isn't natively supported on ARM or Intel (at least the ones we're targeting)
            // so we scalarize it instead.
            ScratchScope<1, 0> scratches(*this);
            GPRReg dataScratchGPR = scratches.gpr(0);
            m_jit.vectorExtractLaneInt64(TrustedImm32(0), left.asFPR(), m_scratchGPR);
            m_jit.vectorExtractLaneInt64(TrustedImm32(0), right.asFPR(), dataScratchGPR);
            m_jit.mul64(m_scratchGPR, dataScratchGPR, m_scratchGPR);
            m_jit.vectorReplaceLane(SIMDLane::i64x2, TrustedImm32(0), m_scratchGPR, m_scratchFPR);
            m_jit.vectorExtractLaneInt64(TrustedImm32(1), left.asFPR(), m_scratchGPR);
            m_jit.vectorExtractLaneInt64(TrustedImm32(1), right.asFPR(), dataScratchGPR);
            m_jit.mul64(m_scratchGPR, dataScratchGPR, m_scratchGPR);
            m_jit.vectorReplaceLane(SIMDLane::i64x2, TrustedImm32(1), m_scratchGPR, m_scratchFPR);
            m_jit.moveVector(m_scratchFPR, result.asFPR());
        } else
            m_jit.vectorMul(info, left.asFPR(), right.asFPR(), result.asFPR());
    }

    PartialResult WARN_UNUSED_RETURN fixupOutOfBoundsIndicesForSwizzle(Location a, Location b, Location result)
    {
        ASSERT(isX86());
        // Let each byte mask be 112 (0x70) then after VectorAddSat
        // each index > 15 would set the saturated index's bit 7 to 1,
        // whose corresponding byte will be zero cleared in VectorSwizzle.
        // https://github.com/WebAssembly/simd/issues/93
        v128_t mask;
        mask.u64x2[0] = 0x7070707070707070;
        mask.u64x2[1] = 0x7070707070707070;
        materializeVectorConstant(mask, Location::fromFPR(m_scratchFPR));
        m_jit.vectorAddSat(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::Unsigned }, m_scratchFPR, b.asFPR(), m_scratchFPR);
        m_jit.vectorSwizzle(a.asFPR(), m_scratchFPR, result.asFPR());
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSIMDV_VV(SIMDLaneOperation op, SIMDInfo info, ExpressionType left, ExpressionType right, ExpressionType& result)
    {
        Location leftLocation = loadIfNecessary(left);
        Location rightLocation = loadIfNecessary(right);
        consume(left);
        consume(right);

        result = topValue(TypeKind::V128);
        Location resultLocation = allocate(result);

        LOG_INSTRUCTION("Vector", op, left, leftLocation, right, rightLocation, RESULT(result));

        switch (op) {
        case SIMDLaneOperation::And:
            m_jit.vectorAnd(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Andnot:
            m_jit.vectorAndnot(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::AvgRound:
            m_jit.vectorAvgRound(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::DotProduct:
#if CPU(ARM64)
            m_jit.vectorDotProduct(leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
#else
            m_jit.vectorDotProduct(leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case SIMDLaneOperation::Add:
            m_jit.vectorAdd(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Mul:
            emitVectorMul(info, leftLocation, rightLocation, resultLocation);
            return { };
        case SIMDLaneOperation::MulSat:
#if CPU(X86_64)
            m_jit.vectorMulSat(leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchGPR, m_scratchFPR);
#else
            m_jit.vectorMulSat(leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case SIMDLaneOperation::Sub:
            m_jit.vectorSub(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Div:
            m_jit.vectorDiv(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Pmax:
#if CPU(ARM64)
            m_jit.vectorPmax(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
#else
            m_jit.vectorPmax(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case SIMDLaneOperation::Pmin:
#if CPU(ARM64)
            m_jit.vectorPmin(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
#else
            m_jit.vectorPmin(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case SIMDLaneOperation::Or:
            m_jit.vectorOr(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Swizzle:
            if constexpr (isX86())
                return fixupOutOfBoundsIndicesForSwizzle(leftLocation, rightLocation, resultLocation);
            m_jit.vectorSwizzle(leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Xor:
            m_jit.vectorXor(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Narrow:
            m_jit.vectorNarrow(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR(), m_scratchFPR);
            return { };
        case SIMDLaneOperation::AddSat:
            m_jit.vectorAddSat(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::SubSat:
            m_jit.vectorSubSat(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
            return { };
        case SIMDLaneOperation::Max:
#if CPU(X86_64)
            if (scalarTypeIsFloatingPoint(info.lane)) {
                // Intel's vectorized maximum instruction has slightly different semantics to the WebAssembly vectorized
                // minimum instruction, namely in terms of signed zero values and propagating NaNs. VectorPmax implements
                // a fast version of this instruction that compiles down to a single op, without conforming to the exact
                // semantics. In order to precisely implement VectorMax, we need to do extra work on Intel to check for
                // the necessary edge cases.

                // Compute result in both directions.
                m_jit.vectorPmax(info, rightLocation.asFPR(), leftLocation.asFPR(), m_scratchFPR);
                m_jit.vectorPmax(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());

                // Check for discrepancies by XORing the two results together.
                m_jit.vectorXor(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, resultLocation.asFPR(), resultLocation.asFPR());

                // OR results, propagating the sign bit for negative zeroes, and NaNs.
                m_jit.vectorOr(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, resultLocation.asFPR(), m_scratchFPR);

                // Propagate discrepancies in the sign bit.
                m_jit.vectorSub(info, m_scratchFPR, resultLocation.asFPR(), m_scratchFPR);

                // Canonicalize NaNs by checking for unordered values and clearing payload if necessary.
                m_jit.compareFloatingPointVectorUnordered(info, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                m_jit.vectorUshr8(SIMDInfo { info.lane == SIMDLane::f32x4 ? SIMDLane::i32x4 : SIMDLane::i64x2, SIMDSignMode::None }, resultLocation.asFPR(), TrustedImm32(info.lane == SIMDLane::f32x4 ? 10 : 13), resultLocation.asFPR());
                m_jit.vectorAndnot(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
            } else
                m_jit.vectorMax(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#else
            m_jit.vectorMax(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        case SIMDLaneOperation::Min:
#if CPU(X86_64)
            if (scalarTypeIsFloatingPoint(info.lane)) {
                // Intel's vectorized minimum instruction has slightly different semantics to the WebAssembly vectorized
                // minimum instruction, namely in terms of signed zero values and propagating NaNs. VectorPmin implements
                // a fast version of this instruction that compiles down to a single op, without conforming to the exact
                // semantics. In order to precisely implement VectorMin, we need to do extra work on Intel to check for
                // the necessary edge cases.

                // Compute result in both directions.
                m_jit.vectorPmin(info, rightLocation.asFPR(), leftLocation.asFPR(), m_scratchFPR);
                m_jit.vectorPmin(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());

                // OR results, propagating the sign bit for negative zeroes, and NaNs.
                m_jit.vectorOr(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, resultLocation.asFPR(), m_scratchFPR);

                // Canonicalize NaNs by checking for unordered values and clearing payload if necessary.
                m_jit.compareFloatingPointVectorUnordered(info, resultLocation.asFPR(), m_scratchFPR, resultLocation.asFPR());
                m_jit.vectorOr(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, resultLocation.asFPR(), m_scratchFPR);
                m_jit.vectorUshr8(SIMDInfo { info.lane == SIMDLane::f32x4 ? SIMDLane::i32x4 : SIMDLane::i64x2, SIMDSignMode::None }, resultLocation.asFPR(), TrustedImm32(info.lane == SIMDLane::f32x4 ? 10 : 13), resultLocation.asFPR());
                m_jit.vectorAndnot(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }, m_scratchFPR, resultLocation.asFPR(), resultLocation.asFPR());
            } else
                m_jit.vectorMin(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#else
            m_jit.vectorMin(info, leftLocation.asFPR(), rightLocation.asFPR(), resultLocation.asFPR());
#endif
            return { };
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    void dump(const ControlStack&, const Stack*) { }
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { }
#undef BBQ_STUB
#undef BBQ_CONTROL_STUB

private:
    bool isScratch(Location loc)
    {
        return (loc.isGPR() && loc.asGPR() == m_scratchGPR) || (loc.isFPR() && loc.asFPR() == m_scratchFPR);
    }

    void emitStoreConst(Value constant, Location loc)
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
        case TypeKind::Externref:
            m_jit.store64(TrustedImm64(constant.asRef()), loc.asAddress());
            break;
        case TypeKind::I64:
        case TypeKind::F64:
            m_jit.store64(Imm64(constant.asI64()), loc.asAddress());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
            break;
        }
    }

    void emitMoveConst(Value constant, Location loc)
    {
        ASSERT(constant.isConst());

        if (loc.isStack() || loc.isGlobal())
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
        case TypeKind::Externref:
            m_jit.move(TrustedImm64(constant.asRef()), loc.asGPR());
            break;
        case TypeKind::F32:
            m_jit.moveFloat(Imm32(constant.asI32()), loc.asFPR());
            break;
        case TypeKind::F64:
            m_jit.moveDouble(Imm64(constant.asI64()), loc.asFPR());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant typekind.");
            break;
        }
    }

    void emitStore(Value src, Location dst)
    {
        if (src.isConst())
            return emitStoreConst(src, dst);

        LOG_INSTRUCTION("Store", src, RESULT(dst));

        ASSERT(dst.isMemory());
        Location srcLocation = locationOf(src);
        ASSERT(srcLocation.isRegister());
        switch (src.type()) {
        case TypeKind::I32:
        case TypeKind::I31ref:
            m_jit.store32(srcLocation.asGPR(), dst.asAddress());
            break;
        case TypeKind::I64:
            m_jit.store64(srcLocation.asGPR(), dst.asAddress());
            break;
        case TypeKind::F32:
            m_jit.storeFloat(srcLocation.asFPR(), dst.asAddress());
            break;
        case TypeKind::F64:
            m_jit.storeDouble(srcLocation.asFPR(), dst.asAddress());
            break;
        case TypeKind::Externref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Funcref:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
            m_jit.storePtr(srcLocation.asGPR(), dst.asAddress());
            break;
        case TypeKind::V128:
            m_jit.storeVector(srcLocation.asFPR(), dst.asAddress());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind store.");
        }
    }

    void emitMoveMemory(Value src, Location dst)
    {
        ASSERT(dst.isMemory());
        Location srcLocation = locationOf(src);
        ASSERT(srcLocation.isMemory());

        if (srcLocation == dst)
            return;
        LOG_INSTRUCTION("Move", src, RESULT(dst));

        switch (src.type()) {
        case TypeKind::I32:
        case TypeKind::I31ref:
        case TypeKind::F32:
            m_jit.transfer32(srcLocation.asAddress(), dst.asAddress());
            break;
        case TypeKind::I64:
            m_jit.transfer64(srcLocation.asAddress(), dst.asAddress());
            break;
        case TypeKind::F64:
            m_jit.loadDouble(srcLocation.asAddress(), m_scratchFPR);
            m_jit.storeDouble(m_scratchFPR, dst.asAddress());
            break;
        case TypeKind::Externref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Funcref:
        case TypeKind::Structref:
        case TypeKind::Arrayref:
            m_jit.transfer64(srcLocation.asAddress(), dst.asAddress());
            break;
        case TypeKind::V128: {
            // We do two scalar load/store pairs instead of one vector load/store in order to avoid
            // clobbering m_scratchFPR or reserving an FPR for data scratch. This should probably be
            // a very uncommon operation and if it's ever the bottleneck let's just assume OMG will
            // handle it.
            Address srcAddress = srcLocation.asAddress();
            Address dstAddress = dst.asAddress();
            m_jit.loadVector(srcAddress, m_scratchFPR);
            m_jit.storeVector(m_scratchFPR, dstAddress);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind move.");
        }
    }

    void emitMoveRegister(Value src, Location dst)
    {
        ASSERT(dst.isRegister());
        Location srcLocation = locationOf(src);

        if (srcLocation == dst)
            return;

        if (!isScratch(srcLocation) && !isScratch(dst))
            LOG_INSTRUCTION("Move", src, RESULT(dst));

        ASSERT(srcLocation.isRegister());
        switch (src.type()) {
        case TypeKind::I32:
        case TypeKind::I31ref:
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Funcref:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
            m_jit.move(srcLocation.asGPR(), dst.asGPR());
            break;
        case TypeKind::F32:
        case TypeKind::F64:
            m_jit.move(srcLocation.asFPR(), dst.asFPR());
            break;
        case TypeKind::V128:
            m_jit.moveVector(srcLocation.asFPR(), dst.asFPR());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind move.");
        }
    }

    void emitLoad(Value src, Location dst)
    {

        ASSERT(dst.isRegister());
        Location srcLocation = locationOf(src);
        ASSERT(srcLocation.isMemory());

        if (!isScratch(dst))
            LOG_INSTRUCTION("Load", src, RESULT(dst));

        switch (src.type()) {
        case TypeKind::I32:
        case TypeKind::I31ref:
            m_jit.load32(srcLocation.asAddress(), dst.asGPR());
            break;
        case TypeKind::I64:
            m_jit.load64(srcLocation.asAddress(), dst.asGPR());
            break;
        case TypeKind::F32:
            m_jit.loadFloat(srcLocation.asAddress(), dst.asFPR());
            break;
        case TypeKind::F64:
            m_jit.loadDouble(srcLocation.asAddress(), dst.asFPR());
            break;
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
            m_jit.loadPtr(srcLocation.asAddress(), dst.asGPR());
            break;
        case TypeKind::V128:
            m_jit.loadVector(srcLocation.asAddress(), dst.asFPR());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented type kind load.");
        }
    }

    void emitMove(Value src, Location dst)
    {
        if (src.isConst()) {
            if (dst.isMemory())
                emitStoreConst(src, dst);
            else
                emitMoveConst(src, dst);
        } else {
            Location srcLocation = locationOf(src);
            if (srcLocation.isRegister()) {
                if (dst.isMemory())
                    emitStore(src, dst);
                else
                    emitMoveRegister(src, dst);
            } else {
                if (dst.isMemory())
                    emitMoveMemory(src, dst);
                else
                    emitLoad(src, dst);
            }
        }
    }

    enum class ShuffleStatus {
        ToMove,
        BeingMoved,
        Moved
    };

    template<size_t N, typename OverflowHandler>
    void emitShuffleMove(Vector<Value, N, OverflowHandler>& srcVector, Vector<Location, N, OverflowHandler>& dstVector, Vector<ShuffleStatus, N, OverflowHandler>& statusVector, unsigned index, Location gpTemp, Location fpTemp)
    {
        Location srcLocation = locationOf(srcVector[index]);
        Location dst = dstVector[index];
        if (srcLocation == dst)
            return; // Easily eliminate redundant moves here.

        statusVector[index] = ShuffleStatus::BeingMoved;
        for (unsigned i = 0; i < srcVector.size(); i ++) {
            // This check should handle constants too - constants always have location None, and no
            // dst should ever be a constant. But we assume that's asserted in the caller.
            if (locationOf(srcVector[i]) == dst) {
                switch (statusVector[i]) {
                case ShuffleStatus::ToMove:
                    emitShuffleMove(srcVector, dstVector, statusVector, i, gpTemp, fpTemp);
                    break;
                case ShuffleStatus::BeingMoved: {
                    Location temp = srcVector[i].isFloat() ? fpTemp : gpTemp;
                    emitMove(srcVector[i], temp);
                    srcVector[i] = Value::pinned(srcVector[i].type(), temp);
                    break;
                }
                case ShuffleStatus::Moved:
                    break;
                }
            }
        }
        emitMove(srcVector[index], dst);
        statusVector[index] = ShuffleStatus::Moved;
    }

    template<size_t N, typename OverflowHandler>
    void emitShuffle(Vector<Value, N, OverflowHandler>& srcVector, Vector<Location, N, OverflowHandler>& dstVector)
    {
        ASSERT(srcVector.size() == dstVector.size());

        ScratchScope<1, 1> scratches(*this);
        Location gpTemp = Location::fromGPR(scratches.gpr(0));
        Location fpTemp = Location::fromFPR(scratches.fpr(0));
        Vector<ShuffleStatus, N, OverflowHandler> statusVector(srcVector.size(), ShuffleStatus::ToMove);

        for (unsigned i = 0; i < srcVector.size(); i ++) {
            if (statusVector[i] == ShuffleStatus::ToMove)
                emitShuffleMove(srcVector, dstVector, statusVector, i, gpTemp, fpTemp);
        }
    }

    ControlData& currentControlData()
    {
        return m_parser->controlStack().last().controlData;
    }

    void setLRUKey(Location location, LocalOrTempIndex key)
    {
        if (location.isGPR())
            m_gprLRU.increaseKey(location.asGPR(), key);
        else if (location.isFPR())
            m_fprLRU.increaseKey(location.asFPR(), key);
    }

    void increaseKey(Location location)
    {
        setLRUKey(location, m_lastUseTimestamp ++);
    }

    Location bind(Value value)
    {
        if (value.isPinned())
            return value.asPinned();

        Location reg = allocateRegister(value);
        increaseKey(reg);
        return bind(value, reg);
    }

    Location allocate(Value value)
    {
        return allocateWithHint(value, Location::none());
    }

    Location allocateWithHint(Value value, Location hint)
    {
        if (value.isPinned())
            return value.asPinned();

        // It's possible, in some circumstances, that the value in question was already assigned to a register.
        // The most common example of this is returned values (that use registers from the calling convention) or
        // values passed between control blocks. In these cases, we just leave it in its register unmoved.
        Location existingLocation = locationOf(value);
        if (existingLocation.isRegister())
            return existingLocation;

        Location reg = hint;
        if (reg.kind() == Location::None
            || value.isFloat() != reg.isFPR()
            || (reg.isGPR() && !m_gprSet.contains(reg.asGPR(), Width::Width64))
            || (reg.isFPR() && !m_fprSet.contains(reg.asFPR(), Width::Width128)))
            reg = allocateRegister(value);
        increaseKey(reg);
        if (value.isLocal())
            currentControlData().touch(value.asLocal());
        return bind(value, reg);
    }

    Location locationOfWithoutBinding(Value value)
    {
        // Used internally in bind() to avoid infinite recursion.
        if (value.isPinned())
            return value.asPinned();
        if (value.isLocal()) {
            ASSERT(value.asLocal() >= 0 && value.asLocal() < m_locals.size());
            return m_locals[value.asLocal()];
        }
        if (value.isTemp()) {
            if (value.asTemp() >= m_temps.size())
                return Location::none();
            return m_temps[value.asTemp()];
        }
        return Location::none();
    }

    Location locationOf(Value value)
    {
        if (value.isTemp()) {
            if (value.asTemp() >= m_temps.size() || m_temps[value.asTemp()].isNone())
                bind(value, canonicalSlot(value));
            return m_temps[value.asTemp()];
        }
        return locationOfWithoutBinding(value);
    }

    Location loadIfNecessary(Value value)
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

    void consume(Value value)
    {
        // Called whenever a value is popped from the expression stack. Mostly, we
        // use this to release the registers temporaries are bound to.
        Location result = locationOf(value);
        if (value.isTemp() && result.isRegister())
            unbind(value, result);
    }

    Location allocateRegister(TypeKind type)
    {
        if (isFloatingPointType(type))
            return Location::fromFPR(m_fprSet.isEmpty() ? evictFPR() : nextFPR());
        return Location::fromGPR(m_gprSet.isEmpty() ? evictGPR() : nextGPR());
    }

    Location allocateRegister(Value value)
    {
        return allocateRegister(value.type());
    }

    Location bind(Value value, Location loc)
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
            } else {
                ASSERT(m_gprSet.contains(loc.asGPR(), Width::Width64));
                m_gprSet.remove(loc.asGPR());
                m_gprBindings[loc.asGPR()] = RegisterBinding::fromValue(value);
            }
        }
        if (value.isLocal())
            m_locals[value.asLocal()] = loc;
        else if (value.isTemp()) {
            if (m_temps.size() <= value.asTemp())
                m_temps.resize(value.asTemp() + 1);
            m_temps[value.asTemp()] = loc;
        }

        if (UNLIKELY(Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tBound value ", value, " to ", loc);

        return loc;
    }

    void unbind(Value value, Location loc)
    {
        // Unbind a value from a location. Doesn't touch the LRU, but updates the register set
        // and local/temp tables accordingly.
        if (loc.isFPR()) {
            m_fprSet.add(loc.asFPR(), Width::Width128);
            m_fprBindings[loc.asFPR()] = RegisterBinding::none();
        } else if (loc.isGPR()) {
            m_gprSet.add(loc.asGPR(), Width::Width64);
            m_gprBindings[loc.asGPR()] = RegisterBinding::none();
        }
        if (value.isLocal())
            m_locals[value.asLocal()] = m_localSlots[value.asLocal()];
        else if (value.isTemp())
            m_temps[value.asTemp()] = Location::none();

        if (UNLIKELY(Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tUnbound value ", value, " from ", loc);
    }

    template<typename Register>
    static Register fromJSCReg(Reg);

    template<>
    RegisterID fromJSCReg<RegisterID>(Reg gpr)
    {
        ASSERT(gpr.isGPR());
        return gpr.gpr();
    }

    template<>
    FPRegisterID fromJSCReg<FPRegisterID>(Reg fpr)
    {
        ASSERT(fpr.isFPR());
        return fpr.fpr();
    }

    template<typename Register>
    struct LRU {
        ALWAYS_INLINE LRU(uint32_t numRegisters)
            : m_keys(numRegisters, -1) // We use -1 to signify registers that can never be allocated or used.
        { }

        void add(RegisterSet registers)
        {
            registers.forEach([&] (JSC::Reg r) {
                m_keys[fromJSCReg<Register>(r)] = 0;
            });
        }

        Register findMin()
        {
            int32_t minIndex = -1;
            int32_t minKey = -1;
            for (unsigned i = 0; i < m_keys.size(); i ++) {
                if (m_keys[i] >= 0 && (minKey < 0 || m_keys[i] < minKey)) {
                    minKey = m_keys[i];
                    minIndex = i;
                }
            }
            ASSERT(minIndex >= 0, "No allocatable registers in LRU");
            return Register(minIndex);
        }

        void increaseKey(Register reg, uint32_t newKey)
        {
            if (m_keys[reg] >= 0) // Leave untracked registers alone.
                m_keys[reg] = newKey;
        }
    private:
        Vector<int32_t, 32> m_keys;
    };

    RegisterID nextGPR()
    {
        auto next = m_gprSet.begin();
        ASSERT(next != m_gprSet.end());
        RegisterID reg = (*next).gpr();
        ASSERT(m_gprBindings[reg].m_kind == RegisterBinding::None);
        return reg;
    }

    FPRegisterID nextFPR()
    {
        auto next = m_fprSet.begin();
        ASSERT(next != m_fprSet.end());
        FPRegisterID reg = (*next).fpr();
        ASSERT(m_fprBindings[reg].m_kind == RegisterBinding::None);
        return reg;
    }

    RegisterID evictGPR()
    {
        auto lruGPR = m_gprLRU.findMin();
        auto lruBinding = m_gprBindings[lruGPR];

        if (UNLIKELY(Options::verboseBBQJITAllocation()))
            dataLogLn("BBQ\tEvicting GPR ", MacroAssembler::gprName(lruGPR), " currently bound to ", lruBinding);
        flushValue(lruBinding.toValue());

        ASSERT(m_gprSet.contains(lruGPR, Width::Width64));
        ASSERT(m_gprBindings[lruGPR].m_kind == RegisterBinding::None);
        return lruGPR;
    }

    FPRegisterID evictFPR()
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

    void clobber(RegisterID gpr)
    {
        if (m_validGPRs.contains(gpr, Width::Width64) && !m_gprSet.contains(gpr, Width::Width64)) {
            RegisterBinding& binding = m_gprBindings[gpr];
            if (UNLIKELY(Options::verboseBBQJITAllocation()))
                dataLogLn("BBQ\tClobbering GPR ", MacroAssembler::gprName(gpr), " currently bound to ", binding);
            RELEASE_ASSERT(!binding.isNone() && !binding.isScratch()); // We could probably figure out how to handle this, but let's just crash if it happens for now.
            flushValue(binding.toValue());
        }
    }

    void clobber(FPRegisterID fpr)
    {
        if (m_validFPRs.contains(fpr, Width::Width128) && !m_fprSet.contains(fpr, Width::Width128)) {
            RegisterBinding& binding = m_fprBindings[fpr];
            if (UNLIKELY(Options::verboseBBQJITAllocation()))
                dataLogLn("BBQ\tClobbering FPR ", MacroAssembler::fprName(fpr), " currently bound to ", binding);
            RELEASE_ASSERT(!binding.isNone() && !binding.isScratch()); // We could probably figure out how to handle this, but let's just crash if it happens for now.
            flushValue(binding.toValue());
        }
    }

    void clobber(JSC::Reg reg)
    {
        reg.isGPR() ? clobber(reg.gpr()) : clobber(reg.fpr());
    }

    template<int GPRs, int FPRs>
    struct ScratchScope {
        WTF_MAKE_NONCOPYABLE(ScratchScope);

        template<typename... Args>
        ScratchScope(BBQJIT& generator, Args... locationsToPreserve)
            : m_generator(generator)
            , m_endedEarly(false)
        {
            initializedPreservedSet(locationsToPreserve...);
            for (JSC::Reg reg : m_preserved) {
                if (reg.isGPR())
                    bindGPRToScratch(reg.gpr());
                else
                    bindFPRToScratch(reg.fpr());
            }
            for (int i = 0; i < GPRs; i ++)
                m_tempGPRs[i] = bindGPRToScratch(m_generator.allocateRegister(TypeKind::I64).asGPR());
            for (int i = 0; i < FPRs; i ++)
                m_tempFPRs[i] = bindFPRToScratch(m_generator.allocateRegister(TypeKind::F64).asFPR());
        }

        ~ScratchScope()
        {
            if (!m_endedEarly)
                unbind();
        }

        void unbindEarly()
        {
            m_endedEarly = true;
            unbind();
        }

        RegisterID bindGPRToScratch(RegisterID reg)
        {
            RegisterBinding& binding = m_generator.m_gprBindings[reg];
            if (m_preserved.contains(reg, Width::Width64) && !binding.isNone()) {
                if (UNLIKELY(Options::verboseBBQJITAllocation()))
                    dataLogLn("BBQ\tPreserving GPR ", MacroAssembler::gprName(reg), " currently bound to ", binding);
                return reg; // If the register is already bound, we don't need to preserve it ourselves.
            }
            ASSERT(binding.isNone());
            binding = RegisterBinding::scratch();
            m_generator.m_gprSet.remove(reg);
            if (UNLIKELY(Options::verboseBBQJITAllocation()))
                dataLogLn("BBQ\tReserving scratch GPR ", MacroAssembler::gprName(reg));
            return reg;
        }

        FPRegisterID bindFPRToScratch(FPRegisterID reg)
        {
            RegisterBinding& binding = m_generator.m_fprBindings[reg];
            if (m_preserved.contains(reg, Width::Width128) && !binding.isNone()) {
                if (UNLIKELY(Options::verboseBBQJITAllocation()))
                    dataLogLn("BBQ\tPreserving FPR ", MacroAssembler::fprName(reg), " currently bound to ", binding);
                return reg; // If the register is already bound, we don't need to preserve it ourselves.
            }
            ASSERT(binding.isNone());
            binding = RegisterBinding::scratch();
            m_generator.m_fprSet.remove(reg);
            if (UNLIKELY(Options::verboseBBQJITAllocation()))
                dataLogLn("BBQ\tReserving scratch FPR ", MacroAssembler::fprName(reg));
            return reg;
        }

        void unbindGPRFromScratch(RegisterID reg)
        {
            RegisterBinding& binding = m_generator.m_gprBindings[reg];
            if (UNLIKELY(Options::verboseBBQJITAllocation()))
                dataLogLn("BBQ\tReleasing GPR ", MacroAssembler::gprName(reg));
            if (m_preserved.contains(reg, Width::Width64) && !binding.isScratch())
                return; // It's okay if the register isn't bound to a scratch if we meant to preserve it - maybe it was just already bound to something.
            ASSERT(binding.isScratch());
            binding = RegisterBinding::none();
            m_generator.m_gprSet.add(reg, Width::Width64);
        }

        void unbindFPRFromScratch(FPRegisterID reg)
        {
            RegisterBinding& binding = m_generator.m_fprBindings[reg];
            if (UNLIKELY(Options::verboseBBQJITAllocation()))
                dataLogLn("BBQ\tReleasing FPR ", MacroAssembler::fprName(reg));
            if (m_preserved.contains(reg, Width::Width128) && !binding.isScratch())
                return; // It's okay if the register isn't bound to a scratch if we meant to preserve it - maybe it was just already bound to something.
            ASSERT(binding.isScratch());
            binding = RegisterBinding::none();
            m_generator.m_fprSet.add(reg, Width::Width128);
        }

        template<typename... Args>
        void initializedPreservedSet(Location location, Args... args)
        {
            if (location.isGPR())
                m_preserved.add(location.asGPR(), Width::Width64);
            else if (location.isFPR())
                m_preserved.add(location.asFPR(), Width::Width128);
            initializedPreservedSet(args...);
        }

        template<typename... Args>
        void initializedPreservedSet(RegisterSet registers, Args... args)
        {
            for (JSC::Reg reg : registers) {
                if (reg.isGPR())
                    m_preserved.add(reg.gpr(), Width::Width64);
                else
                    m_preserved.add(reg.fpr(), Width::Width128);
            }
            initializedPreservedSet(args...);
        }

        inline void initializedPreservedSet()
        { }

        inline RegisterID gpr(unsigned i) const
        {
            ASSERT(i < GPRs);
            ASSERT(!m_endedEarly);
            return m_tempGPRs[i];
        }

        inline FPRegisterID fpr(unsigned i) const
        {
            ASSERT(i < FPRs);
            ASSERT(!m_endedEarly);
            return m_tempFPRs[i];
        }
    private:
        void unbind()
        {
            for (int i = 0; i < GPRs; i ++)
                unbindGPRFromScratch(m_tempGPRs[i]);
            for (int i = 0; i < FPRs; i ++)
                unbindFPRFromScratch(m_tempFPRs[i]);

            for (JSC::Reg reg : m_preserved) {
                if (reg.isGPR())
                    unbindGPRFromScratch(reg.gpr());
                else
                    unbindFPRFromScratch(reg.fpr());
            }
        }

        BBQJIT& m_generator;
        RegisterID m_tempGPRs[GPRs];
        FPRegisterID m_tempFPRs[FPRs];
        RegisterSet m_preserved;
        bool m_endedEarly;
    };

    Location canonicalSlot(Value value)
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

    Location allocateStack(Value value)
    {
        // Align stack for value size.
        m_frameSize = WTF::roundUpToMultipleOf(value.size(), m_frameSize);
        m_frameSize += value.size();
        return Location::fromStack(-m_frameSize);
    }

#undef LOG_INSTRUCTION
#undef RESULT

    CCallHelpers& m_jit;
    Callee& m_callee;
    const FunctionData& m_function;
    const FunctionSignature* m_functionSignature;
    uint32_t m_functionIndex;
    const ModuleInformation& m_info;
    MemoryMode m_mode;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls;
    TierUpCount* m_tierUp;
    FunctionParser<BBQJIT>* m_parser;
    Vector<uint32_t, 4> m_arguments;
    ControlData m_topLevel;

    Vector<RegisterBinding, 32> m_gprBindings; // Tables mapping from each register to the current value bound to it.
    Vector<RegisterBinding, 32> m_fprBindings;
    RegisterSet m_gprSet, m_fprSet; // Sets tracking whether registers are bound or free.
    RegisterSet m_validGPRs, m_validFPRs; // These contain the original register sets used in m_gprSet and m_fprSet.
    Vector<Location, 8> m_locals; // Vectors mapping local and temp indices to binding indices.
    Vector<Location, 8> m_temps;
    Vector<Location, 8> m_localSlots; // Persistent stack slots for local variables.
    Vector<TypeKind, 8> m_localTypes; // Types of all non-argument locals in this function.
    LRU<RegisterID> m_gprLRU; // LRU cache tracking when general-purpose registers were last used.
    LRU<FPRegisterID> m_fprLRU; // LRU cache tracking when floating-point registers were last used.
    uint32_t m_lastUseTimestamp; // Monotonically increasing integer incrementing with each register use.
    Vector<RefPtr<SharedTask<void(BBQJIT&, CCallHelpers&)>>, 8> m_latePaths; // Late paths to emit after the rest of the function body.

    DataLabelPtr m_frameSizeLabel;
    int m_frameSize { 0 };
    int m_maxCalleeStackSize { 0 };
    int m_localStorage { 0 }; // Stack offset pointing to the local with the lowest address.
    constexpr static int tempSlotSize { 16 }; // Size of the stack slot for a stack temporary. Currently the size of the largest possible temporary (a v128).
    int m_blockCount;

    RegisterID m_scratchGPR { GPRInfo::nonPreservedNonArgumentGPR0 }; // Scratch registers to hold temporaries in operations.
    FPRegisterID m_scratchFPR { FPRInfo::nonPreservedNonArgumentFPR0 };

#if CPU(X86) || CPU(X86_64)
    RegisterID m_shiftRCX { X86Registers::ecx };
#else
    RegisterID m_shiftRCX { RegisterID::InvalidGPRReg };
#endif

    RegisterSet m_callerSaveGPRs;
    RegisterSet m_callerSaveFPRs;
    RegisterSet m_callerSaves;

    InternalFunction* m_compilation;

    std::array<JumpList, numberOfExceptionTypes> m_exceptions { };
};

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileBBQ(CompilationContext& compilationContext, Callee& callee, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp)
{
    CompilerTimingScope totalTime("BBQ", "Total BBQ");

    UNUSED_PARAM(hasExceptionHandlers);
    UNUSED_PARAM(tierUp);
    auto result = makeUnique<InternalFunction>();

    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

    BBQJIT irGenerator(*compilationContext.wasmEntrypointJIT, signature, callee, function, functionIndex, info, unlinkedWasmToWasmCalls, mode, result.get(), tierUp);
    FunctionParser<BBQJIT> parser(irGenerator, function.data.data(), function.data.size(), signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    return result;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
