/*
 * Copyright (C) 2021 Igalia S.L.
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

#pragma once

#if ENABLE(ASSEMBLER) && CPU(RISCV64)

#include "AssemblerBuffer.h"
#include "AssemblerCommon.h"
#include "RISCV64Registers.h"
#include <tuple>

namespace JSC {

namespace RISCV64Registers {

typedef enum : int8_t {
#define REGISTER_ID(id, name, r, cs) id,
    FOR_EACH_GP_REGISTER(REGISTER_ID)
#undef REGISTER_ID

#define REGISTER_ALIAS(id, name, alias) id = alias,
    FOR_EACH_REGISTER_ALIAS(REGISTER_ALIAS)
#undef REGISTER_ALIAS

    InvalidGPRReg = -1,
} RegisterID;

typedef enum : int8_t {
#define REGISTER_ID(id, name) id,
    FOR_EACH_SP_REGISTER(REGISTER_ID)
#undef REGISTER_ID

    InvalidSPReg = -1,
} SPRegisterID;

typedef enum : int8_t {
#define REGISTER_ID(id, name, r, cs) id,
    FOR_EACH_FP_REGISTER(REGISTER_ID)
#undef REGISTER_ID

    InvalidFPRReg = -1,
} FPRegisterID;

} // namespace RISCV64Registers

namespace RISCV64Instructions {

enum class Opcode : unsigned {
    LOAD        = 0b0000011,
    LOAD_FP     = 0b0000111,
    MISC_MEM    = 0b0001111,
    OP_IMM      = 0b0010011,
    AUIPC       = 0b0010111,
    OP_IMM_32   = 0b0011011,
    STORE       = 0b0100011,
    STORE_FP    = 0b0100111,
    AMO         = 0b0101111,
    OP          = 0b0110011,
    LUI         = 0b0110111,
    OP_32       = 0b0111011,
    MADD        = 0b1000011,
    MSUB        = 0b1000111,
    NMSUB       = 0b1001011,
    NMADD       = 0b1001111,
    OP_FP       = 0b1010011,
    BRANCH      = 0b1100011,
    JALR        = 0b1100111,
    JAL         = 0b1101111,
    SYSTEM      = 0b1110011,
};

enum class FCVTType {
    W, WU,
    L, LU,
    S, D,
};

enum class FMVType {
    X, W, D,
};

enum class FPRoundingMode : unsigned {
    RNE = 0b000,
    RTZ = 0b001,
    RDN = 0b010,
    RUP = 0b011,
    RMM = 0b100,
    DYN = 0b111,
};

enum class MemoryOperation : uint8_t {
    I = 1 << 3,
    O = 1 << 2,
    R = 1 << 1,
    W = 1 << 0,
    RW = R | W,
    IORW = I | O | R | W,
};

enum class MemoryAccess : uint8_t {
    Acquire = 1 << 1,
    Release = 1 << 0,
    AcquireRelease = Acquire | Release,
};

// Register helpers

using RegisterID = RISCV64Registers::RegisterID;
using FPRegisterID = RISCV64Registers::FPRegisterID;

template<typename T>
auto registerValue(T registerID)
    -> std::enable_if_t<(std::is_same_v<T, RegisterID> || std::is_same_v<T, FPRegisterID>), unsigned>
{
    return unsigned(registerID) & ((1 << 5) - 1);
}

// InstructionValue contains the 32-bit instruction value and also provides access into the desired field.

struct InstructionValue {
    explicit InstructionValue(uint32_t value)
        : value(value)
    { }

    template<unsigned fieldStart, unsigned fieldSize>
    uint32_t field()
    {
        static_assert(fieldStart + fieldSize <= (sizeof(uint32_t) * 8));
        return (value >> fieldStart) & ((1 << fieldSize) - 1);
    }

    uint32_t opcode() { return field<0, 7>(); }

    uint32_t value;
};

// Immediate types

// ImmediateBase acts as the base struct for the different types. The bit-size of the immediate is determined as the
// template parameter on the ImmediateBase struct. Internally, every immediate value is represented through a uint32_t
// from which the appropriate bit-sets are then copied into the target instruction.

// ImmediateBase provides three ways to construct the target immediate (the type of which is specified as a template
// parameter to these construction methods):
// ImmediateBase<N>::v<ImmediateType, int32_t>() -- for constant immediates
// ImmediateBase<N>::v<ImmediateType>(int32_t/int64_t) -- for variable immediates whose values were validated beforehand
// ImmediateBase<N>::ImmediateBase(uint32_t) -- for immediate values already packed in the uint32_t format

// There's also ImmediateType::value(InstructionValue) helpers that for a given instruction value retrieve the
// appropriate signed immediate value that was encoded in that instruction (except for the U-type immediate which is
// a 32-bit unsigned value).

template<unsigned immediateSize>
struct ImmediateBase {
    static_assert(immediateSize <= sizeof(uint32_t) * 8);

    template<typename T>
    static constexpr T immediateMask()
    {
        if constexpr(immediateSize < sizeof(uint32_t) * 8)
            return ((T(1) << immediateSize) - 1);
        return T(~0);
    }

    template<typename T>
    static auto isValid(T immValue)
        -> std::enable_if_t<(std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>), bool>
    {
        constexpr unsigned shift = sizeof(T) * 8 - immediateSize;
        return immValue == ((immValue << shift) >> shift);
    }

    template<typename ImmediateType, int32_t immValue>
    static ImmediateType v()
    {
        static_assert((-(1 << (immediateSize - 1)) <= immValue) && (immValue <= ((1 << (immediateSize - 1)) - 1)));
        int32_t value = immValue;
        return ImmediateType((*reinterpret_cast<uint32_t*>(&value)) & immediateMask<uint32_t>());
    }

    template<typename ImmediateType>
    static ImmediateType v(int32_t immValue)
    {
        ASSERT(isValid(immValue));
        uint32_t value = *reinterpret_cast<uint32_t*>(&immValue);
        return ImmediateType(value & immediateMask<uint32_t>());
    }

    template<typename ImmediateType>
    static ImmediateType v(int64_t immValue)
    {
        ASSERT(isValid(immValue));
        uint64_t value = *reinterpret_cast<uint64_t*>(&immValue);
        return ImmediateType(uint32_t(value & immediateMask<uint64_t>()));
    }

    explicit ImmediateBase(uint32_t immValue)
        : imm(immValue)
    {
        if constexpr (immediateSize < sizeof(uint32_t) * 8)
            ASSERT(imm < (1 << immediateSize));
    }

    template<unsigned fieldStart, unsigned fieldSize>
    uint32_t field()
    {
        static_assert(fieldStart + fieldSize <= immediateSize);
        return (imm >> fieldStart) & ((1 << fieldSize) - 1);
    }

    uint32_t imm;
};

struct IImmediate : ImmediateBase<12> {
    explicit IImmediate(uint32_t immValue)
        : ImmediateBase<12>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<20, 12>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 20) >> 20);
    }
};

struct SImmediate : ImmediateBase<12> {
    explicit SImmediate(uint32_t immValue)
        : ImmediateBase<12>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = 0
            | (insn.field<31, 1>() << 11)
            | (insn.field<25, 6>() <<  5)
            | (insn.field< 8, 4>() <<  1)
            | (insn.field< 7, 1>() <<  0);
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 20) >> 20);
    }
};

struct BImmediate : ImmediateBase<13> {
    explicit BImmediate(uint32_t immValue)
        : ImmediateBase<13>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = 0
            | (insn.field<31, 1>() << 12)
            | (insn.field< 7, 1>() << 11)
            | (insn.field<25, 6>() <<  5)
            | (insn.field< 8, 4>() <<  1);
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 19) >> 19);
    }
};

struct UImmediate : ImmediateBase<32> {
    explicit UImmediate(uint32_t immValue)
        : ImmediateBase((immValue >> 12) << 12)
    { }

    static uint32_t value(InstructionValue insn)
    {
        return insn.field<12, 20>() << 12;
    }
};

struct JImmediate : ImmediateBase<21> {
    explicit JImmediate(uint32_t immValue)
        : ImmediateBase<21>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = 0
            | (insn.field<31, 1>() << 20)
            | (insn.field<12, 8>() << 12)
            | (insn.field<20, 1>() << 11)
            | (insn.field<25, 6>() <<  5)
            | (insn.field<21, 4>() <<  1);
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 11) >> 11);
    }
};

struct ImmediateDecomposition {
    template<typename T, typename = std::enable_if_t<(std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>)>>
    explicit ImmediateDecomposition(T immediate)
        : upper(UImmediate(0))
        , lower(IImmediate(0))
    {
        ASSERT(ImmediateBase<32>::isValid(immediate));
        int32_t value = int32_t(immediate);
        if (value & (1 << 11))
            value += (1 << 12);

        upper = UImmediate::v<UImmediate>(value);
        lower = IImmediate::v<IImmediate>((value << 20) >> 20);
    }

    UImmediate upper;
    IImmediate lower;
};

// Instruction types

// Helper struct that provides different groupings of register types as required for different instructions.
// The tuple size and contained types are used for compile-time checks of matching register types being passed
// to those instructions.

struct RegistersBase {
    struct GType { }; // General-purpose register
    struct FType { }; // Floating-point register
    struct ZType { }; // Zero-value unused register

    template<typename... RTypes>
    using Tuple = std::tuple<RTypes...>;
    template<size_t I, typename TupleType>
    using Type = std::tuple_element_t<I, TupleType>;

    template<typename TupleType>
    static constexpr size_t Size()
    {
        return std::tuple_size_v<TupleType>;
    }

    using G = Tuple<GType>;
    using GG = Tuple<GType, GType>;
    using GF = Tuple<GType, FType>;
    using GGG = Tuple<GType, GType, GType>;
    using GGZ = Tuple<GType, GType, ZType>;
    using GFF = Tuple<GType, FType, FType>;
    using GFZ = Tuple<GType, FType, ZType>;
    using FG = Tuple<FType, GType>;
    using FF = Tuple<FType, FType>;
    using FGZ = Tuple<FType, GType, ZType>;
    using FFF = Tuple<FType, FType, FType>;
    using FFZ = Tuple<FType, FType, ZType>;
    using FFFF = Tuple<FType, FType, FType, FType>;
    using ZZ = Tuple<ZType, ZType>;
};

// These are the base instruction structs. For R-type instructions, additional variations are provided.

// Opcode, different spec-defined constant instruction fields and the required register types are specified through the
// template parameters. The construct() static methods compose and return the instruction value in the 32-bit unsigned
// format.

// The matches() methods are usable to match a given InstructionValue against the target instruction type. Baseline
// implementations test the opcode and constant fields, but different instruction specializations can provide a better
// matching technique if necessary.

// For each base instruction type there's also static getters for dynamic bit-fields like register values, rounding mode
// or different flag types. These should be used on an InstructionValue after a matching instruction type was already
// confirmed. These are mostly used for disassembly, leaving it to that implementation to handle the returned raw
// bit-field values.

template<typename RegisterTypes>
struct RTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 3);
    using RD = RegistersBase::Type<0, RegisterTypes>;
    using RS1 = RegistersBase::Type<1, RegisterTypes>;
    using RS2 = RegistersBase::Type<2, RegisterTypes>;
};

template<Opcode opcode, unsigned funct3, unsigned funct7, typename RegisterTypes>
struct RTypeBase {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct3 < (1 << 3));
    static_assert(funct7 < (1 << 7));

    using Base = RTypeBase<opcode, funct3, funct7, RegisterTypes>;
    using Registers = RTypeRegisters<RegisterTypes>;

    template<typename RDType, typename RS1Type, typename RS2Type>
    static uint32_t construct(RDType rd, RS1Type rs1, RS2Type rs2)
    {
        uint32_t instruction = 0
            | (funct7             << 25)
            | (registerValue(rs2) << 20)
            | (registerValue(rs1) << 15)
            | (funct3             << 12)
            | (registerValue(rd)  <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct3 == insn.field<12, 3>() && funct7 == insn.field<25, 7>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rs2(InstructionValue insn) { return insn.field<20, 5>(); }
};

template<Opcode opcode, unsigned funct7, typename RegisterTypes>
struct RTypeBaseWithRoundingMode {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct7 < (1 << 7));

    using Base = RTypeBaseWithRoundingMode<opcode, funct7, RegisterTypes>;
    using Registers = RTypeRegisters<RegisterTypes>;

    template<typename RDType, typename RS1Type, typename RS2Type>
    static uint32_t construct(RDType rd, RS1Type rs1, RS2Type rs2, FPRoundingMode rm)
    {
        ASSERT(unsigned(rm) < (1 << 3));

        uint32_t instruction = 0
            | (funct7             << 25)
            | (registerValue(rs2) << 20)
            | (registerValue(rs1) << 15)
            | (unsigned(rm)       << 12)
            | (registerValue(rd)  <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct7 == insn.field<25, 7>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rs2(InstructionValue insn) { return insn.field<20, 5>(); }
    static uint8_t rm(InstructionValue insn) { return insn.field<12, 3>(); }
};

template<Opcode opcode, unsigned funct3, unsigned funct7, typename RegisterTypes>
struct RTypeBaseWithAqRl {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct3 < (1 << 3));
    static_assert(funct7 < (1 << 7));

    using Base = RTypeBaseWithAqRl<opcode, funct3, funct7, RegisterTypes>;
    using Registers = RTypeRegisters<RegisterTypes>;

    template<typename RDType, typename RS1Type, typename RS2Type>
    static uint32_t construct(RDType rd, RS1Type rs1, RS2Type rs2, const std::initializer_list<MemoryAccess>& access)
    {
        unsigned aqrl = 0;
        for (auto& value : access)
            aqrl |= unsigned(value);
        ASSERT(aqrl < (1 << 2));

        uint32_t instruction = 0
            | ((funct7 | aqrl)    << 25)
            | (registerValue(rs2) << 20)
            | (registerValue(rs1) << 15)
            | (funct3             << 12)
            | (registerValue(rd)  <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct3 == insn.field<12, 3>() && (funct7 >> 2) == insn.field<27, 5>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rs2(InstructionValue insn) { return insn.field<20, 5>(); }
    static uint8_t aqrl(InstructionValue insn) { return insn.field<25, 2>(); }
};

template<typename RegisterTypes>
struct R4TypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 4);
    using RD = RegistersBase::Type<0, RegisterTypes>;
    using RS1 = RegistersBase::Type<1, RegisterTypes>;
    using RS2 = RegistersBase::Type<2, RegisterTypes>;
    using RS3 = RegistersBase::Type<3, RegisterTypes>;
};

template<Opcode opcode, unsigned funct2, typename RegisterTypes>
struct R4TypeBaseWithRoundingMode {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct2 < (1 << 2));

    using Base = R4TypeBaseWithRoundingMode<opcode, funct2, RegisterTypes>;
    using Registers = R4TypeRegisters<RegisterTypes>;

    template<typename RDType, typename RS1Type, typename RS2Type, typename RS3Type>
    static uint32_t construct(RDType rd, RS1Type rs1, RS2Type rs2, RS3Type rs3, FPRoundingMode rm)
    {
        ASSERT(unsigned(rm) < (1 << 3));

        uint32_t instruction = 0
            | (registerValue(rs3) << 27)
            | (funct2             << 25)
            | (registerValue(rs2) << 20)
            | (registerValue(rs1) << 15)
            | (unsigned(rm)       << 12)
            | (registerValue(rd)  <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct2 == insn.field<25, 2>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rs2(InstructionValue insn) { return insn.field<20, 5>(); }
    static uint8_t rs3(InstructionValue insn) { return insn.field<27, 5>(); }
    static uint8_t rm(InstructionValue insn) { return insn.field<12, 3>(); }
};

template<typename RegisterTypes>
struct ITypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RD = RegistersBase::Type<0, RegisterTypes>;
    using RS1 = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode opcode, unsigned funct3, typename RegisterTypes>
struct ITypeBase {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct3 < (1 << 3));

    using Base = ITypeBase<opcode, funct3, RegisterTypes>;
    using Registers = ITypeRegisters<RegisterTypes>;

    template<typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1, IImmediate imm)
    {
        uint32_t instruction = 0
            | (imm.field<0, 12>() << 20)
            | (registerValue(rs1) << 15)
            | (funct3             << 12)
            | (registerValue(rd)  <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct3 == insn.field<12, 3>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
};

template<typename RegisterTypes>
struct STypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RS1 = RegistersBase::Type<0, RegisterTypes>;
    using RS2 = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode opcode, unsigned funct3, typename RegisterTypes>
struct STypeBase {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct3 < (1 << 3));

    using Base = STypeBase<opcode, funct3, RegisterTypes>;
    using Registers = STypeRegisters<RegisterTypes>;

    template<typename RS1Type, typename RS2Type>
    static uint32_t construct(RS1Type rs1, RS2Type rs2, SImmediate imm)
    {
        uint32_t instruction = 0
            | (imm.field<5, 7>()  << 25)
            | (registerValue(rs2) << 20)
            | (registerValue(rs1) << 15)
            | (funct3             << 12)
            | (imm.field<0, 5>()  <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct3 == insn.field<12, 3>();
    }

    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rs2(InstructionValue insn) { return insn.field<20, 5>(); }
};

template<typename RegisterTypes>
struct BTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RS1 = RegistersBase::Type<0, RegisterTypes>;
    using RS2 = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode opcode, unsigned funct3, typename RegisterTypes>
struct BTypeBase {
    static_assert(unsigned(opcode) < (1 << 7));
    static_assert(funct3 < (1 << 3));

    using Base = BTypeBase<opcode, funct3, RegisterTypes>;
    using Registers = BTypeRegisters<RegisterTypes>;

    static constexpr unsigned funct3Value = funct3;

    template<typename RS1Type, typename RS2Type>
    static uint32_t construct(RS1Type rs1, RS2Type rs2, BImmediate imm)
    {
        uint32_t instruction = 0
            | (imm.field<12, 1>() << 31)
            | (imm.field< 5, 6>() << 25)
            | (registerValue(rs2) << 20)
            | (registerValue(rs1) << 15)
            | (funct3             << 12)
            | (imm.field< 1, 4>() <<  8)
            | (imm.field<11, 1>() <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode() && funct3 == insn.field<12, 3>();
    }

    static uint8_t rs1(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rs2(InstructionValue insn) { return insn.field<20, 5>(); }
};

template<typename RegisterTypes>
struct UTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 1);
    using RD = RegistersBase::Type<0, RegisterTypes>;
};

template<Opcode opcode, typename RegisterTypes>
struct UTypeBase {
    static_assert(unsigned(opcode) < (1 << 7));

    using Base = UTypeBase<opcode, RegisterTypes>;
    using Registers = UTypeRegisters<RegisterTypes>;

    template<typename RDType>
    static uint32_t construct(RDType rd, UImmediate imm)
    {
        uint32_t instruction = imm.imm | (registerValue(rd) << 7) | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
};

template<typename RegisterTypes>
struct JTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 1);
    using RD = RegistersBase::Type<0, RegisterTypes>;
};

template<Opcode opcode, typename RegisterTypes>
struct JTypeBase {
    static_assert(unsigned(opcode) < (1 << 7));

    using Base = JTypeBase<opcode, RegisterTypes>;
    using Registers = UTypeRegisters<RegisterTypes>;

    template<typename RDType>
    static uint32_t construct(RDType rd, JImmediate imm)
    {
        uint32_t instruction = 0
            | (imm.field<20,  1>() << 31)
            | (imm.field< 1, 10>() << 21)
            | (imm.field<11,  1>() << 20)
            | (imm.field<12,  8>() << 12)
            | (registerValue(rd)   <<  7)
            | unsigned(opcode);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.opcode();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<7, 5>(); }
};

// The following instruction definitions utilize the base instruction structs, in most cases specifying everything
// necessary in the template parameters of the base instruction struct they are inheriting from. For each instruction
// there's also a pretty-print name constant included in the definition, for use by the disassembler.

// RV32I Base Instruction Set

struct LUI : UTypeBase<Opcode::LUI, RegistersBase::G> {
    static constexpr const char* name = "lui";
};

struct AUIPC : UTypeBase<Opcode::AUIPC, RegistersBase::G> {
    static constexpr const char* name = "auipc";
};

struct JAL : JTypeBase<Opcode::JAL, RegistersBase::G> {
    static constexpr const char* name = "jal";
};

struct JALR : ITypeBase<Opcode::JALR, 0b000, RegistersBase::GG> {
    static constexpr const char* name = "jalr";
};

struct BEQ : BTypeBase<Opcode::BRANCH, 0b000, RegistersBase::GG> {
    static constexpr const char* name = "beq";
};

struct BNE : BTypeBase<Opcode::BRANCH, 0b001, RegistersBase::GG> {
    static constexpr const char* name = "bne";
};

struct BLT : BTypeBase<Opcode::BRANCH, 0b100, RegistersBase::GG> {
    static constexpr const char* name = "blt";
};

struct BGE : BTypeBase<Opcode::BRANCH, 0b101, RegistersBase::GG> {
    static constexpr const char* name = "bge";
};

struct BLTU : BTypeBase<Opcode::BRANCH, 0b110, RegistersBase::GG> {
    static constexpr const char* name = "bltu";
};

struct BGEU : BTypeBase<Opcode::BRANCH, 0b111, RegistersBase::GG> {
    static constexpr const char* name = "bgeu";
};

struct LB : ITypeBase<Opcode::LOAD, 0b000, RegistersBase::GG> {
    static constexpr const char* name = "lb";
};

struct LH : ITypeBase<Opcode::LOAD, 0b001, RegistersBase::GG> {
    static constexpr const char* name = "lh";
};

struct LW : ITypeBase<Opcode::LOAD, 0b010, RegistersBase::GG> {
    static constexpr const char* name = "lw";
};

struct LBU : ITypeBase<Opcode::LOAD, 0b100, RegistersBase::GG> {
    static constexpr const char* name = "lbu";
};

struct LHU : ITypeBase<Opcode::LOAD, 0b101, RegistersBase::GG> {
    static constexpr const char* name = "lhu";
};

struct SB : STypeBase<Opcode::STORE, 0b000, RegistersBase::GG> {
    static constexpr const char* name = "sb";
};

struct SH : STypeBase<Opcode::STORE, 0b001, RegistersBase::GG> {
    static constexpr const char* name = "sh";
};

struct SW : STypeBase<Opcode::STORE, 0b010, RegistersBase::GG> {
    static constexpr const char* name = "sw";
};

struct ADDI : ITypeBase<Opcode::OP_IMM, 0b000, RegistersBase::GG> {
    static constexpr const char* name = "addi";
};

struct SLTI : ITypeBase<Opcode::OP_IMM, 0b010, RegistersBase::GG> {
    static constexpr const char* name = "slti";
};

struct SLTIU : ITypeBase<Opcode::OP_IMM, 0b011, RegistersBase::GG> {
    static constexpr const char* name = "sltiu";
};

struct XORI : ITypeBase<Opcode::OP_IMM, 0b100, RegistersBase::GG> {
    static constexpr const char* name = "xori";
};

struct ORI : ITypeBase<Opcode::OP_IMM, 0b110, RegistersBase::GG> {
    static constexpr const char* name = "ori";
};

struct ANDI : ITypeBase<Opcode::OP_IMM, 0b111, RegistersBase::GG> {
    static constexpr const char* name = "andi";
};

struct SLLI : ITypeBase<Opcode::OP_IMM, 0b001, RegistersBase::GG> {
    static constexpr const char* name = "slli";

    using Base::construct;
    template<unsigned shiftAmount, typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        static_assert(shiftAmount < (1 << 6));
        return Base::construct(rd, rs1, IImmediate::v<IImmediate, (0b000000 << 6) | shiftAmount>());
    }
};

struct SRLI : ITypeBase<Opcode::OP_IMM, 0b101, RegistersBase::GG> {
    static constexpr const char* name = "srli";

    using Base::construct;
    template<unsigned shiftAmount, typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        static_assert(shiftAmount < (1 << 6));
        return Base::construct(rd, rs1, IImmediate::v<IImmediate, (0b000000 << 6) | shiftAmount>());
    }
};

struct SRAI : ITypeBase<Opcode::OP_IMM, 0b101, RegistersBase::GG> {
    static constexpr const char* name = "srai";

    using Base::construct;
    template<unsigned shiftAmount, typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        static_assert(shiftAmount < (1 << 6));
        return Base::construct(rd, rs1, IImmediate::v<IImmediate, (0b010000 << 6) | shiftAmount>());
    }
};

struct ADD : RTypeBase<Opcode::OP, 0b000, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "add";
};

struct SUB : RTypeBase<Opcode::OP, 0b000, 0b0100000, RegistersBase::GGG> {
    static constexpr const char* name = "sub";
};

struct SLL : RTypeBase<Opcode::OP, 0b001, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "sll";
};

struct SLT : RTypeBase<Opcode::OP, 0b010, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "slt";
};

struct SLTU : RTypeBase<Opcode::OP, 0b011, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "sltu";
};

struct XOR : RTypeBase<Opcode::OP, 0b100, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "xor";
};

struct SRL : RTypeBase<Opcode::OP, 0b101, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "srl";
};

struct SRA : RTypeBase<Opcode::OP, 0b101, 0b0100000, RegistersBase::GGG> {
    static constexpr const char* name = "sra";
};

struct OR : RTypeBase<Opcode::OP, 0b110, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "or";
};

struct AND : RTypeBase<Opcode::OP, 0b111, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "and";
};

struct FENCE : ITypeBase<Opcode::MISC_MEM, 0b000, RegistersBase::ZZ> {
    static constexpr const char* name = "fence";
};

struct ECALL : ITypeBase<Opcode::SYSTEM, 0b000, RegistersBase::ZZ> {
    static constexpr const char* name = "ecall";
};

struct EBREAK : ITypeBase<Opcode::SYSTEM, 0b000, RegistersBase::ZZ> {
    static constexpr const char* name = "ebreak";
};

// RV64I Base Instruction Set (in addition to RV32I)

struct LWU : ITypeBase<Opcode::LOAD, 0b110, RegistersBase::GG> {
    static constexpr const char* name = "lwu";
};

struct LD : ITypeBase<Opcode::LOAD, 0b011, RegistersBase::GG> {
    static constexpr const char* name = "ld";
};

struct SD : STypeBase<Opcode::STORE, 0b011, RegistersBase::GG> {
    static constexpr const char* name = "sd";
};

struct ADDIW : ITypeBase<Opcode::OP_IMM_32, 0b000, RegistersBase::GG> {
    static constexpr const char* name = "addiw";
};

struct SLLIW : ITypeBase<Opcode::OP_IMM_32, 0b001, RegistersBase::GG> {
    static constexpr const char* name = "slliw";

    using Base::construct;
    template<unsigned shiftAmount, typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        static_assert(shiftAmount < (1 << 5));
        return Base::construct(rd, rs1, IImmediate::v<IImmediate, (0b0000000 << 5) | shiftAmount>());
    }
};

struct SRLIW : ITypeBase<Opcode::OP_IMM_32, 0b101, RegistersBase::GG> {
    static constexpr const char* name = "srliw";

    using Base::construct;
    template<unsigned shiftAmount, typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        static_assert(shiftAmount < (1 << 5));
        return Base::construct(rd, rs1, IImmediate::v<IImmediate, (0b0000000 << 5) | shiftAmount>());
    }
};

struct SRAIW : ITypeBase<Opcode::OP_IMM_32, 0b101, RegistersBase::GG> {
    static constexpr const char* name = "sraiw";

    using Base::construct;
    template<unsigned shiftAmount, typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        static_assert(shiftAmount < (1 << 5));
        return Base::construct(rd, rs1, IImmediate::v<IImmediate, (0b0100000 << 5) | shiftAmount>());
    }
};

struct ADDW : RTypeBase<Opcode::OP_32, 0b000, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "addw";
};

struct SUBW : RTypeBase<Opcode::OP_32, 0b000, 0b0100000, RegistersBase::GGG> {
    static constexpr const char* name = "subw";
};

struct SLLW : RTypeBase<Opcode::OP_32, 0b001, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "sllw";
};

struct SRLW : RTypeBase<Opcode::OP_32, 0b101, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "srlw";
};

struct SRAW : RTypeBase<Opcode::OP_32, 0b101, 0b0100000, RegistersBase::GGG> {
    static constexpr const char* name = "sraw";
};

// RV32/RV64 Zifencei Standard Extension

struct FENCE_I : ITypeBase<Opcode::MISC_MEM, 0b001, RegistersBase::ZZ> {
    static constexpr const char* name = "fence.i";
};

// RV32M Standard Extension

struct MUL : RTypeBase<Opcode::OP, 0b000, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "mul";
};

struct MULH : RTypeBase<Opcode::OP, 0b001, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "mulh";
};

struct MULHSU : RTypeBase<Opcode::OP, 0b010, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "mulhsu";
};

struct MULHU : RTypeBase<Opcode::OP, 0b011, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "mulhu";
};

struct DIV : RTypeBase<Opcode::OP, 0b100, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "div";
};

struct DIVU : RTypeBase<Opcode::OP, 0b101, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "divu";
};

struct REM : RTypeBase<Opcode::OP, 0b110, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "rem";
};

struct REMU : RTypeBase<Opcode::OP, 0b111, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "remu";
};

// RV64M Standard Extension (in addition to RV32M)

struct MULW : RTypeBase<Opcode::OP_32, 0b000, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "mulw";
};

struct DIVW : RTypeBase<Opcode::OP_32, 0b100, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "divw";
};

struct DIVUW : RTypeBase<Opcode::OP_32, 0b101, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "divuw";
};

struct REMW : RTypeBase<Opcode::OP_32, 0b110, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "remw";
};

struct REMUW : RTypeBase<Opcode::OP_32, 0b111, 0b0000001, RegistersBase::GGG> {
    static constexpr const char* name = "remuw";
};

// RV32A Standard Extension

struct LR_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0001000, RegistersBase::GGZ> {
    static constexpr const char* name = "lr.w";
};

struct SC_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0001100, RegistersBase::GGG> {
    static constexpr const char* name = "sc.w";
};

struct AMOSWAP_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0000100, RegistersBase::GGG> {
    static constexpr const char* name = "amoswap.w";
};

struct AMOADD_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "amoadd.w";
};

struct AMOXOR_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0010000, RegistersBase::GGG> {
    static constexpr const char* name = "amoxor.w";
};

struct AMOAND_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0110000, RegistersBase::GGG> {
    static constexpr const char* name = "amoand.w";
};

struct AMOOR_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b0100000, RegistersBase::GGG> {
    static constexpr const char* name = "amoor.w";
};

struct AMOMIN_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b1000000, RegistersBase::GGG> {
    static constexpr const char* name = "amomin.w";
};

struct AMOMAX_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b1010000, RegistersBase::GGG> {
    static constexpr const char* name = "amomax.w";
};

struct AMOMINU_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b1100000, RegistersBase::GGG> {
    static constexpr const char* name = "amominu.w";
};

struct AMOMAXU_W : RTypeBaseWithAqRl<Opcode::AMO, 0b010, 0b1110000, RegistersBase::GGG> {
    static constexpr const char* name = "amomaxu.w";
};

// RV64A Standard Extension (in addition to RV32A)

struct LR_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0001000, RegistersBase::GGZ> {
    static constexpr const char* name = "lr.d";
};

struct SC_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0001100, RegistersBase::GGG> {
    static constexpr const char* name = "sc.d";
};

struct AMOSWAP_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0000100, RegistersBase::GGG> {
    static constexpr const char* name = "amoswap.d";
};

struct AMOADD_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0000000, RegistersBase::GGG> {
    static constexpr const char* name = "amoadd.d";
};

struct AMOXOR_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0010000, RegistersBase::GGG> {
    static constexpr const char* name = "amoxor.d";
};

struct AMOAND_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0110000, RegistersBase::GGG> {
    static constexpr const char* name = "amoand.d";
};

struct AMOOR_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b0100000, RegistersBase::GGG> {
    static constexpr const char* name = "amoor.d";
};

struct AMOMIN_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b1000000, RegistersBase::GGG> {
    static constexpr const char* name = "amomin.d";
};

struct AMOMAX_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b1010000, RegistersBase::GGG> {
    static constexpr const char* name = "amomax.d";
};

struct AMOMINU_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b1100000, RegistersBase::GGG> {
    static constexpr const char* name = "amominu.d";
};

struct AMOMAXU_D : RTypeBaseWithAqRl<Opcode::AMO, 0b011, 0b1110000, RegistersBase::GGG> {
    static constexpr const char* name = "amomaxu.d";
};

// RV32F Standard Extension

template<FCVTType ToType, FCVTType FromType>
struct FCVTBase {
    static constexpr bool valid = false;
};

template<typename RDRegisterType, typename RS1RegisterType, unsigned rs2, unsigned funct7, typename RegisterTypes>
struct FCVTImpl : RTypeBaseWithRoundingMode<Opcode::OP_FP, funct7, RegisterTypes> {
    static constexpr bool valid = true;
    using RDType = RDRegisterType;
    using RS1Type = RS1RegisterType;

    template<typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1, FPRoundingMode rm)
    {
        static_assert(rs2 < (1 << 5));
        return RTypeBaseWithRoundingMode<Opcode::OP_FP, funct7, RegisterTypes>::construct(rd, rs1, RegisterID(rs2), rm);
    }
};

template<FMVType ToType, FMVType FromType>
struct FMVBase {
    static constexpr bool valid = false;
};

template<typename RDRegisterType, typename RS1RegisterType, unsigned funct7, typename RegisterTypes>
struct FMVImpl : RTypeBase<Opcode::OP_FP, 0b000, funct7, RegisterTypes> {
    static constexpr bool valid = true;
    using RDType = RDRegisterType;
    using RS1Type = RS1RegisterType;

    template<typename RDType, typename RS1Type>
    static uint32_t construct(RDType rd, RS1Type rs1)
    {
        return RTypeBase<Opcode::OP_FP, 0b000, funct7, RegisterTypes>::construct(rd, rs1, RegisterID(0));
    }
};

struct FLW : ITypeBase<Opcode::LOAD_FP, 0b010, RegistersBase::FG> {
    static constexpr const char* name = "flw";
};

struct FSW : STypeBase<Opcode::STORE_FP, 0b010, RegistersBase::GF> {
    static constexpr const char* name = "fsw";
};

struct FMADD_S : R4TypeBaseWithRoundingMode<Opcode::MADD, 0b00, RegistersBase::FFFF> {
    static constexpr const char* name = "fmadd.s";
};

struct FMSUB_S : R4TypeBaseWithRoundingMode<Opcode::MSUB, 0b00, RegistersBase::FFFF> {
    static constexpr const char* name = "fmsub.s";
};

struct FNMSUB_S : R4TypeBaseWithRoundingMode<Opcode::NMSUB, 0b00, RegistersBase::FFFF> {
    static constexpr const char* name = "fnmsub.s";
};

struct FNMADD_S : R4TypeBaseWithRoundingMode<Opcode::NMADD, 0b00, RegistersBase::FFFF> {
    static constexpr const char* name = "fnmadd.s";
};

struct FADD_S : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0000000, RegistersBase::FFF> {
    static constexpr const char* name = "fadd.s";
};

struct FSUB_S : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0000100, RegistersBase::FFF> {
    static constexpr const char* name = "fsub.s";
};

struct FMUL_S : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0001000, RegistersBase::FFF> {
    static constexpr const char* name = "fmul.s";
};

struct FDIV_S : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0001100, RegistersBase::FFF> {
    static constexpr const char* name = "fdiv.s";
};

struct FSQRT_S : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0101100, RegistersBase::FFZ> {
    static constexpr const char* name = "fsqrt.s";
};

struct FSGNJ_S : RTypeBase<Opcode::OP_FP, 0b000, 0b0010000, RegistersBase::FFF> {
    static constexpr const char* name = "fsgnj.s";
};

struct FSGNJN_S : RTypeBase<Opcode::OP_FP, 0b001, 0b0010000, RegistersBase::FFF> {
    static constexpr const char* name = "fsgnjn.s";
};

struct FSGNJX_S : RTypeBase<Opcode::OP_FP, 0b010, 0b0010000, RegistersBase::FFF> {
    static constexpr const char* name = "fsgnjx.s";
};

struct FMIN_S : RTypeBase<Opcode::OP_FP, 0b000, 0b0010100, RegistersBase::FFF> {
    static constexpr const char* name = "fmin.s";
};

struct FMAX_S : RTypeBase<Opcode::OP_FP, 0b001, 0b0010100, RegistersBase::FFF> {
    static constexpr const char* name = "fmax.s";
};

template<>
struct FCVTBase<FCVTType::W, FCVTType::S> : FCVTImpl<RegisterID, FPRegisterID, 0b00000, 0b1100000, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.w.s";
};
using FCVT_W_S = FCVTBase<FCVTType::W, FCVTType::S>;

template<>
struct FCVTBase<FCVTType::WU, FCVTType::S> : FCVTImpl<RegisterID, FPRegisterID, 0b00001, 0b1100000, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.wu.s";
};
using FCVT_WU_S = FCVTBase<FCVTType::WU, FCVTType::S>;

template<>
struct FMVBase<FMVType::X, FMVType::W> : FMVImpl<RegisterID, FPRegisterID, 0b1110000, RegistersBase::GFZ> {
    static constexpr const char* name = "fmv.x.w";
};
using FMV_X_W = FMVBase<FMVType::X, FMVType::W>;

struct FEQ_S : RTypeBase<Opcode::OP_FP, 0b010, 0b1010000, RegistersBase::GFF> {
    static constexpr const char* name = "feq.s";
};

struct FLT_S : RTypeBase<Opcode::OP_FP, 0b001, 0b1010000, RegistersBase::GFF> {
    static constexpr const char* name = "flt.s";
};

struct FLE_S : RTypeBase<Opcode::OP_FP, 0b000, 0b1010000, RegistersBase::GFF> {
    static constexpr const char* name = "fle.s";
};

struct FCLASS_S : RTypeBase<Opcode::OP_FP, 0b001, 0b1110000, RegistersBase::GFZ> {
    static constexpr const char* name = "fclass.s";
};

template<>
struct FCVTBase<FCVTType::S, FCVTType::W> : FCVTImpl<FPRegisterID, RegisterID, 0b00000, 0b1101000, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.s.w";
};
using FCVT_S_W = FCVTBase<FCVTType::S, FCVTType::W>;

template<>
struct FCVTBase<FCVTType::S, FCVTType::WU> : FCVTImpl<FPRegisterID, RegisterID, 0b00001, 0b1101000, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.s.wu";
};
using FCVT_S_WU = FCVTBase<FCVTType::S, FCVTType::WU>;

template<>
struct FMVBase<FMVType::W, FMVType::X> : FMVImpl<FPRegisterID, RegisterID, 0b1111000, RegistersBase::FGZ> {
    static constexpr const char* name = "fmv.w.x";
};
using FMV_W_X = FMVBase<FMVType::W, FMVType::X>;

// RV64F Standard Extension (in addition to RV32F)

template<>
struct FCVTBase<FCVTType::L, FCVTType::S> : FCVTImpl<RegisterID, FPRegisterID, 0b00010, 0b1100000, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.l.s";
};
using FCVT_L_S = FCVTBase<FCVTType::L, FCVTType::S>;

template<>
struct FCVTBase<FCVTType::LU, FCVTType::S> : FCVTImpl<RegisterID, FPRegisterID, 0b00011, 0b1100000, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.lu.s";
};
using FCVT_LU_S = FCVTBase<FCVTType::LU, FCVTType::S>;

template<>
struct FCVTBase<FCVTType::S, FCVTType::L> : FCVTImpl<FPRegisterID, RegisterID, 0b00010, 0b1101000, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.s.l";
};
using FCVT_S_L = FCVTBase<FCVTType::S, FCVTType::L>;

template<>
struct FCVTBase<FCVTType::S, FCVTType::LU> : FCVTImpl<FPRegisterID, RegisterID, 0b00011, 0b1101000, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.s.lu";
};
using FCVT_S_LU = FCVTBase<FCVTType::S, FCVTType::LU>;

// RV32D Standard Extension

struct FLD : ITypeBase<Opcode::LOAD_FP, 0b011, RegistersBase::FG> {
    static constexpr const char* name = "fld";
};

struct FSD : STypeBase<Opcode::STORE_FP, 0b011, RegistersBase::GF> {
    static constexpr const char* name = "fsd";
};

struct FMADD_D : R4TypeBaseWithRoundingMode<Opcode::MADD, 0b01, RegistersBase::FFFF> {
    static constexpr const char* name = "fmadd.d";
};

struct FMSUB_D : R4TypeBaseWithRoundingMode<Opcode::MSUB, 0b01, RegistersBase::FFFF> {
    static constexpr const char* name = "fmsub.d";
};

struct FNMSUB_D : R4TypeBaseWithRoundingMode<Opcode::NMSUB, 0b01, RegistersBase::FFFF> {
    static constexpr const char* name = "fnmsub.d";
};

struct FNMADD_D : R4TypeBaseWithRoundingMode<Opcode::NMADD, 0b01, RegistersBase::FFFF> {
    static constexpr const char* name = "fnmadd.d";
};

struct FADD_D : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0000001, RegistersBase::FFF> {
    static constexpr const char* name = "fadd.d";
};

struct FSUB_D : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0000101, RegistersBase::FFF> {
    static constexpr const char* name = "fsub.d";
};

struct FMUL_D : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0001001, RegistersBase::FFF> {
    static constexpr const char* name = "fmul.d";
};

struct FDIV_D : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0001101, RegistersBase::FFF> {
    static constexpr const char* name = "fdiv.d";
};

struct FSQRT_D : RTypeBaseWithRoundingMode<Opcode::OP_FP, 0b0101101, RegistersBase::FFZ> {
    static constexpr const char* name = "fsqrt.d";
};

struct FSGNJ_D : RTypeBase<Opcode::OP_FP, 0b000, 0b0010001, RegistersBase::FFF> {
    static constexpr const char* name = "fsgnj.d";
};

struct FSGNJN_D : RTypeBase<Opcode::OP_FP, 0b001, 0b0010001, RegistersBase::FFF> {
    static constexpr const char* name = "fsgnjn.d";
};

struct FSGNJX_D : RTypeBase<Opcode::OP_FP, 0b010, 0b0010001, RegistersBase::FFF> {
    static constexpr const char* name = "fsgnjx.d";
};

struct FMIN_D : RTypeBase<Opcode::OP_FP, 0b000, 0b0010101, RegistersBase::FFF> {
    static constexpr const char* name = "fmin.d";
};

struct FMAX_D : RTypeBase<Opcode::OP_FP, 0b001, 0b0010101, RegistersBase::FFF> {
    static constexpr const char* name = "fmax.d";
};

template<>
struct FCVTBase<FCVTType::S, FCVTType::D> : FCVTImpl<FPRegisterID, FPRegisterID, 0b00001, 0b0100000, RegistersBase::FFZ> {
    static constexpr const char* name = "fcvt.s.d";
};
using FCVT_S_D = FCVTBase<FCVTType::S, FCVTType::D>;

template<>
struct FCVTBase<FCVTType::D, FCVTType::S> : FCVTImpl<FPRegisterID, FPRegisterID, 0b00000, 0b0100001, RegistersBase::FFZ> {
    static constexpr const char* name = "fcvt.d.s";
};
using FCVT_D_S = FCVTBase<FCVTType::D, FCVTType::S>;

struct FEQ_D : RTypeBase<Opcode::OP_FP, 0b010, 0b1010001, RegistersBase::GFF> {
    static constexpr const char* name = "feq.d";
};

struct FLT_D : RTypeBase<Opcode::OP_FP, 0b001, 0b1010001, RegistersBase::GFF> {
    static constexpr const char* name = "flt.d";
};

struct FLE_D : RTypeBase<Opcode::OP_FP, 0b000, 0b1010001, RegistersBase::GFF> {
    static constexpr const char* name = "fle.d";
};

struct FCLASS_D : RTypeBase<Opcode::OP_FP, 0b001, 0b1110001, RegistersBase::GFZ> {
    static constexpr const char* name = "fclass.d";
};

template<>
struct FCVTBase<FCVTType::W, FCVTType::D> : FCVTImpl<RegisterID, FPRegisterID, 0b00000, 0b1100001, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.w.d";
};
using FCVT_W_D = FCVTBase<FCVTType::W, FCVTType::D>;

template<>
struct FCVTBase<FCVTType::WU, FCVTType::D> : FCVTImpl<RegisterID, FPRegisterID, 0b00001, 0b1100001, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.wu.d";
};
using FCVT_WU_D = FCVTBase<FCVTType::WU, FCVTType::D>;

template<>
struct FCVTBase<FCVTType::D, FCVTType::W> : FCVTImpl<FPRegisterID, RegisterID, 0b00000, 0b1101001, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.d.w";
};
using FCVT_D_W = FCVTBase<FCVTType::D, FCVTType::W>;

template<>
struct FCVTBase<FCVTType::D, FCVTType::WU> : FCVTImpl<FPRegisterID, RegisterID, 0b00001, 0b1101001, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.d.wu";
};
using FCVT_D_WU = FCVTBase<FCVTType::D, FCVTType::WU>;

// RV64D Standard Extension (in addition to RV32D)

template<>
struct FCVTBase<FCVTType::L, FCVTType::D> : FCVTImpl<RegisterID, FPRegisterID, 0b00010, 0b1100001, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.l.d";
};
using FCVT_L_D = FCVTBase<FCVTType::L, FCVTType::D>;

template<>
struct FCVTBase<FCVTType::LU, FCVTType::D> : FCVTImpl<RegisterID, FPRegisterID, 0b00011, 0b1100001, RegistersBase::GFZ> {
    static constexpr const char* name = "fcvt.lu.d";
};
using FCVT_LU_D = FCVTBase<FCVTType::LU, FCVTType::D>;

template<>
struct FMVBase<FMVType::X, FMVType::D> : FMVImpl<RegisterID, FPRegisterID, 0b1110001, RegistersBase::GFZ> {
    static constexpr const char* name = "fmv.x.d";
};
using FMV_X_D = FMVBase<FMVType::X, FMVType::D>;

template<>
struct FCVTBase<FCVTType::D, FCVTType::L> : FCVTImpl<FPRegisterID, RegisterID, 0b00010, 0b1101001, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.d.l";
};
using FCVT_D_L = FCVTBase<FCVTType::D, FCVTType::L>;

template<>
struct FCVTBase<FCVTType::D, FCVTType::LU> : FCVTImpl<FPRegisterID, RegisterID, 0b00011, 0b1101001, RegistersBase::FGZ> {
    static constexpr const char* name = "fcvt.d.lu";
};
using FCVT_D_LU = FCVTBase<FCVTType::D, FCVTType::LU>;

template<>
struct FMVBase<FMVType::D, FMVType::X> : FMVImpl<FPRegisterID, RegisterID, 0b1111001, RegistersBase::FGZ> {
    static constexpr const char* name = "fmv.d.x";
};
using FMV_D_X = FMVBase<FMVType::D, FMVType::X>;

} // namespace RISCV64Instructions

class RISCV64Assembler {
public:
    using RegisterID = RISCV64Registers::RegisterID;
    using SPRegisterID = RISCV64Registers::SPRegisterID;
    using FPRegisterID = RISCV64Registers::FPRegisterID;

    static constexpr RegisterID firstRegister() { return RISCV64Registers::x0; }
    static constexpr RegisterID lastRegister() { return RISCV64Registers::x31; }
    static constexpr unsigned numberOfRegisters() { return lastRegister() - firstRegister() + 1; }

    static constexpr SPRegisterID firstSPRegister() { return RISCV64Registers::pc; }
    static constexpr SPRegisterID lastSPRegister() { return RISCV64Registers::pc; }
    static constexpr unsigned numberOfSPRegisters() { return lastSPRegister() - firstSPRegister() + 1; }

    static constexpr FPRegisterID firstFPRegister() { return RISCV64Registers::f0; }
    static constexpr FPRegisterID lastFPRegister() { return RISCV64Registers::f31; }
    static constexpr unsigned numberOfFPRegisters() { return lastFPRegister() - firstFPRegister() + 1; }

    static const char* gprName(RegisterID id)
    {
        ASSERT(id >= firstRegister() && id <= lastRegister());
        static const char* const nameForRegister[numberOfRegisters()] = {
#define REGISTER_NAME(id, name, r, cs) name,
            FOR_EACH_GP_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    static const char* sprName(SPRegisterID id)
    {
        ASSERT(id >= firstSPRegister() && id <= lastSPRegister());
        static const char* const nameForRegister[numberOfSPRegisters()] = {
#define REGISTER_NAME(id, name) name,
            FOR_EACH_SP_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    static const char* fprName(FPRegisterID id)
    {
        ASSERT(id >= firstFPRegister() && id <= lastFPRegister());
        static const char* const nameForRegister[numberOfFPRegisters()] = {
#define REGISTER_NAME(id, name, r, cs) name,
            FOR_EACH_FP_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    RISCV64Assembler() { }

    AssemblerBuffer& buffer() { return m_buffer; }

    static void* getRelocatedAddress(void* code, AssemblerLabel label)
    {
        ASSERT(label.isSet());
        return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + label.offset());
    }

    static int getDifferenceBetweenLabels(AssemblerLabel a, AssemblerLabel b)
    {
        return b.offset() - a.offset();
    }

    size_t codeSize() const { return m_buffer.codeSize(); }

    static unsigned getCallReturnOffset(AssemblerLabel call)
    {
        ASSERT(call.isSet());
        return call.offset();
    }

    AssemblerLabel labelIgnoringWatchpoints()
    {
        return m_buffer.label();
    }

    AssemblerLabel labelForWatchpoint()
    {
        AssemblerLabel result = m_buffer.label();
        if (static_cast<int>(result.offset()) != m_indexOfLastWatchpoint)
            result = label();
        m_indexOfLastWatchpoint = result.offset();
        m_indexOfTailOfLastWatchpoint = result.offset() + maxJumpReplacementSize();
        return result;
    }

    AssemblerLabel label()
    {
        AssemblerLabel result = m_buffer.label();
        while (UNLIKELY(static_cast<int>(result.offset()) < m_indexOfTailOfLastWatchpoint)) {
            nop();
            result = m_buffer.label();
        }
        return result;
    }

    static void linkJump(void* code, AssemblerLabel from, void* to)
    {
        ASSERT(from.isSet());
        if (!from.isSet())
            return;

        uint32_t* location = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(code) + from.offset());
        if (location[0] == LinkJumpImpl::placeholderInsn()) {
            LinkJumpImpl::apply(location, to);
            return;
        }

        if (location[0] == LinkBranchImpl::placeholderInsn()) {
            LinkBranchImpl::apply(location, to);
            return;
        }
    }

    static void linkCall(void* code, AssemblerLabel from, void* to)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(code) + from.offset());
        RELEASE_ASSERT(location[0] == LinkCallImpl::placeholderInsn());
        LinkCallImpl::apply(location, to);
    }

    static void linkPointer(void* code, AssemblerLabel where, void* valuePtr)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(code) + where.offset());
        PatchPointerImpl::apply(location, valuePtr);
    }

    void linkJump(AssemblerLabel from, AssemblerLabel to)
    {
        RELEASE_ASSERT(from.isSet() && to.isSet());
        if (!from.isSet() || !to.isSet())
            return;

        uint32_t* location = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(m_buffer.data()) + to.offset());
        linkJump(m_buffer.data(), from, location);
    }

    static ptrdiff_t maxJumpReplacementSize()
    {
        return sizeof(uint32_t) * 8;
    }

    static constexpr ptrdiff_t patchableJumpSize()
    {
        return sizeof(uint32_t) * 8;
    }

    static void repatchPointer(void* where, void* valuePtr)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(where);
        PatchPointerImpl::apply(location, valuePtr);
        cacheFlush(location, sizeof(uint32_t) * 8);
    }

    static void relinkJump(void* from, void* to)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(from);
        LinkJumpImpl::apply(location, to);
        cacheFlush(location, sizeof(uint32_t) * 2);
    }

    static void relinkCall(void* from, void* to)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(from);
        LinkCallImpl::apply(location, to);
        cacheFlush(location, sizeof(uint32_t) * 2);
    }

    static void relinkTailCall(void* from, void* to)
    {
        relinkJump(from, to);
    }

    static void replaceWithVMHalt(void* where)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(where);
        location[0] = RISCV64Instructions::SD::construct(RISCV64Registers::zero, RISCV64Registers::zero, SImmediate::v<SImmediate, 0>());
        cacheFlush(location, sizeof(uint32_t));
    }

    static void replaceWithJump(void* from, void* to)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(from);
        intptr_t offset = uintptr_t(to) - uintptr_t(from);

        if (JImmediate::isValid(offset)) {
            location[0] = RISCV64Instructions::JAL::construct(RISCV64Registers::zero, JImmediate::v<JImmediate>(offset));
            cacheFlush(from, sizeof(uint32_t));
            return;
        }

        RELEASE_ASSERT(ImmediateBase<32>::isValid(offset));
        RISCV64Instructions::ImmediateDecomposition immediate(offset);

        location[0] = RISCV64Instructions::AUIPC::construct(RISCV64Registers::x30, immediate.upper);
        location[1] = RISCV64Instructions::JALR::construct(RISCV64Registers::zero, RISCV64Registers::x30, immediate.lower);
        cacheFlush(from, sizeof(uint32_t) * 2);
    }

    static void replaceWithNops(void* from, size_t memoryToFillWithNopsInBytes)
    {
        fillNops<MachineCodeCopyMode::Memcpy>(from, memoryToFillWithNopsInBytes);
        cacheFlush(from, memoryToFillWithNopsInBytes);
    }

    static void revertJumpReplacementToPatch(void* from, void* valuePtr)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(from);
        PatchPointerImpl::apply(location, RISCV64Registers::x30, valuePtr);
        cacheFlush(location, sizeof(uint32_t) * 8);
    }

    static void* readCallTarget(void* from)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(from);
        return PatchPointerImpl::read(location);
    }

    unsigned debugOffset() { return m_buffer.debugOffset(); }

    static void cacheFlush(void* code, size_t size)
    {
        intptr_t end = reinterpret_cast<intptr_t>(code) + size;
        __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(end));
    }

    template<MachineCodeCopyMode copy>
    static void fillNops(void* base, size_t size)
    {
        uint32_t* ptr = reinterpret_cast<uint32_t*>(base);
        RELEASE_ASSERT(roundUpToMultipleOf<sizeof(uint32_t)>(ptr) == ptr);
        RELEASE_ASSERT(!(size % sizeof(uint32_t)));

        uint32_t nop = RISCV64Instructions::ADDI::construct(RISCV64Registers::zero, RISCV64Registers::zero, IImmediate::v<IImmediate, 0>());
        for (size_t i = 0, n = size / sizeof(uint32_t); i < n; ++i)
            machineCodeCopy<copy>(&ptr[i], &nop, sizeof(uint32_t));
    }

    typedef enum {
        ConditionEQ,
        ConditionNE,
        ConditionGTU,
        ConditionLEU,
        ConditionGEU,
        ConditionLTU,
        ConditionGT,
        ConditionLE,
        ConditionGE,
        ConditionLT,
    } Condition;

    static constexpr Condition invert(Condition cond)
    {
        return static_cast<Condition>(cond ^ 1);
    }

    template<unsigned immediateSize> using ImmediateBase = RISCV64Instructions::ImmediateBase<immediateSize>;
    using IImmediate = RISCV64Instructions::IImmediate;
    using SImmediate = RISCV64Instructions::SImmediate;
    using BImmediate = RISCV64Instructions::BImmediate;
    using UImmediate = RISCV64Instructions::UImmediate;
    using JImmediate = RISCV64Instructions::JImmediate;

    void luiInsn(RegisterID rd, UImmediate imm) { insn(RISCV64Instructions::LUI::construct(rd, imm)); }
    void auipcInsn(RegisterID rd, UImmediate imm) { insn(RISCV64Instructions::AUIPC::construct(rd, imm)); }
    void jalInsn(RegisterID rd, JImmediate imm) { insn(RISCV64Instructions::JAL::construct(rd, imm)); }
    void jalrInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::JALR::construct(rd, rs1, imm)); }
    void beqInsn(RegisterID rs1, RegisterID rs2, BImmediate imm) { insn(RISCV64Instructions::BEQ::construct(rs1, rs2, imm)); }
    void bneInsn(RegisterID rs1, RegisterID rs2, BImmediate imm) { insn(RISCV64Instructions::BNE::construct(rs1, rs2, imm)); }
    void bltInsn(RegisterID rs1, RegisterID rs2, BImmediate imm) { insn(RISCV64Instructions::BLT::construct(rs1, rs2, imm)); }
    void bgeInsn(RegisterID rs1, RegisterID rs2, BImmediate imm) { insn(RISCV64Instructions::BGE::construct(rs1, rs2, imm)); }
    void bltuInsn(RegisterID rs1, RegisterID rs2, BImmediate imm) { insn(RISCV64Instructions::BLTU::construct(rs1, rs2, imm)); }
    void bgeuInsn(RegisterID rs1, RegisterID rs2, BImmediate imm) { insn(RISCV64Instructions::BGEU::construct(rs1, rs2, imm)); }
    void lbInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LB::construct(rd, rs1, imm)); }
    void lhInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LH::construct(rd, rs1, imm)); }
    void lwInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LW::construct(rd, rs1, imm)); }
    void ldInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LD::construct(rd, rs1, imm)); }
    void lbuInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LBU::construct(rd, rs1, imm)); }
    void lhuInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LHU::construct(rd, rs1, imm)); }
    void lwuInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::LWU::construct(rd, rs1, imm)); }
    void sbInsn(RegisterID rs1, RegisterID rs2, SImmediate imm) { insn(RISCV64Instructions::SB::construct(rs1, rs2, imm)); }
    void shInsn(RegisterID rs1, RegisterID rs2, SImmediate imm) { insn(RISCV64Instructions::SH::construct(rs1, rs2, imm)); }
    void swInsn(RegisterID rs1, RegisterID rs2, SImmediate imm) { insn(RISCV64Instructions::SW::construct(rs1, rs2, imm)); }
    void sdInsn(RegisterID rs1, RegisterID rs2, SImmediate imm) { insn(RISCV64Instructions::SD::construct(rs1, rs2, imm)); }
    void addiInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::ADDI::construct(rd, rs1, imm)); }
    void sltiInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::SLTI::construct(rd, rs1, imm)); }
    void sltiuInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::SLTIU::construct(rd, rs1, imm)); }
    void xoriInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::XORI::construct(rd, rs1, imm)); }
    void oriInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::ORI::construct(rd, rs1, imm)); }
    void andiInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::ANDI::construct(rd, rs1, imm)); }
    void slliInsn(RegisterID rd, RegisterID rs1, uint32_t shamt)
    {
        ASSERT(isValidShiftAmount<6>(shamt));
        insn(RISCV64Instructions::SLLI::construct(rd, rs1, IImmediate((0b000000 << 6) | shamt)));
    }
    void srliInsn(RegisterID rd, RegisterID rs1, uint32_t shamt)
    {
        ASSERT(isValidShiftAmount<6>(shamt));
        insn(RISCV64Instructions::SRLI::construct(rd, rs1, IImmediate((0b000000 << 6) | shamt)));
    }
    void sraiInsn(RegisterID rd, RegisterID rs1, uint32_t shamt)
    {
        ASSERT(isValidShiftAmount<6>(shamt));
        insn(RISCV64Instructions::SRAI::construct(rd, rs1, IImmediate((0b010000 << 6) | shamt)));
    }
    void addInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::ADD::construct(rd, rs1, rs2)); }
    void subInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SUB::construct(rd, rs1, rs2)); }
    void sllInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SLL::construct(rd, rs1, rs2)); }
    void sltInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SLT::construct(rd, rs1, rs2)); }
    void sltuInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SLTU::construct(rd, rs1, rs2)); }
    void xorInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::XOR::construct(rd, rs1, rs2)); }
    void srlInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SRL::construct(rd, rs1, rs2)); }
    void sraInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SRA::construct(rd, rs1, rs2)); }
    void orInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::OR::construct(rd, rs1, rs2)); }
    void andInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::AND::construct(rd, rs1, rs2)); }
    void ecallInsn() { insn(RISCV64Instructions::ECALL::construct(RegisterID::zero, RegisterID::zero, IImmediate::v<IImmediate, 0>())); }
    void ebreakInsn() { insn(RISCV64Instructions::EBREAK::construct(RegisterID::zero, RegisterID::zero, IImmediate::v<IImmediate, 1>())); }
    void addiwInsn(RegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::ADDIW::construct(rd, rs1, imm)); }
    void slliwInsn(RegisterID rd, RegisterID rs1, uint32_t shamt)
    {
        ASSERT(isValidShiftAmount<5>(shamt));
        insn(RISCV64Instructions::SLLIW::construct(rd, rs1, IImmediate((0b0000000 << 5) | shamt)));
    }
    void srliwInsn(RegisterID rd, RegisterID rs1, uint32_t shamt)
    {
        ASSERT(isValidShiftAmount<5>(shamt));
        insn(RISCV64Instructions::SRLIW::construct(rd, rs1, IImmediate((0b0000000 << 5) | shamt)));
    }
    void sraiwInsn(RegisterID rd, RegisterID rs1, uint32_t shamt)
    {
        ASSERT(isValidShiftAmount<5>(shamt));
        insn(RISCV64Instructions::SRAIW::construct(rd, rs1, IImmediate((0b0100000 << 5) | shamt)));
    }
    void addwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::ADDW::construct(rd, rs1, rs2)); }
    void subwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SUBW::construct(rd, rs1, rs2)); }
    void sllwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SLLW::construct(rd, rs1, rs2)); }
    void srlwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SRLW::construct(rd, rs1, rs2)); }
    void srawInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::SRAW::construct(rd, rs1, rs2)); }

    void mulInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::MUL::construct(rd, rs1, rs2)); }
    void mulhInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::MULH::construct(rd, rs1, rs2)); }
    void mulhsuInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::MULHSU::construct(rd, rs1, rs2)); }
    void mulhuInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::MULHU::construct(rd, rs1, rs2)); }

    void divInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::DIV::construct(rd, rs1, rs2)); }
    void divuInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::DIVU::construct(rd, rs1, rs2)); }
    void remInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::REM::construct(rd, rs1, rs2)); }
    void remuInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::REMU::construct(rd, rs1, rs2)); }

    void mulwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::MULW::construct(rd, rs1, rs2)); }
    void divwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::DIVW::construct(rd, rs1, rs2)); }
    void divuwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::DIVUW::construct(rd, rs1, rs2)); }
    void remwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::REMW::construct(rd, rs1, rs2)); }
    void remuwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2) { insn(RISCV64Instructions::REMUW::construct(rd, rs1, rs2)); }

    using FCVTType = RISCV64Instructions::FCVTType;
    using FMVType = RISCV64Instructions::FMVType;

    using FPRoundingMode = RISCV64Instructions::FPRoundingMode;

    void flwInsn(FPRegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::FLW::construct(rd, rs1, imm)); }
    void fldInsn(FPRegisterID rd, RegisterID rs1, IImmediate imm) { insn(RISCV64Instructions::FLD::construct(rd, rs1, imm)); }
    void fswInsn(RegisterID rs1, FPRegisterID rs2, SImmediate imm) { insn(RISCV64Instructions::FSW::construct(rs1, rs2, imm)); }
    void fsdInsn(RegisterID rs1, FPRegisterID rs2, SImmediate imm) { insn(RISCV64Instructions::FSD::construct(rs1, rs2, imm)); }

    template<unsigned fpSize>
    void fmaddInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2, FPRegisterID rs3)
    {
        insnFP<fpSize, RISCV64Instructions::FMADD_S, RISCV64Instructions::FMADD_D>(rd, rs1, rs2, rs3, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fmsubInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2, FPRegisterID rs3)
    {
        insnFP<fpSize, RISCV64Instructions::FMSUB_S, RISCV64Instructions::FMSUB_D>(rd, rs1, rs2, rs3, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fnmsubInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2, FPRegisterID rs3)
    {
        insnFP<fpSize, RISCV64Instructions::FNMSUB_S, RISCV64Instructions::FNMSUB_D>(rd, rs1, rs2, rs3, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fnmaddInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2, FPRegisterID rs3)
    {
        insnFP<fpSize, RISCV64Instructions::FNMADD_S, RISCV64Instructions::FNMADD_D>(rd, rs1, rs2, rs3, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void faddInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FADD_S, RISCV64Instructions::FADD_D>(rd, rs1, rs2, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fsubInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FSUB_S, RISCV64Instructions::FSUB_D>(rd, rs1, rs2, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fmulInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FMUL_S, RISCV64Instructions::FMUL_D>(rd, rs1, rs2, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fdivInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FDIV_S, RISCV64Instructions::FDIV_D>(rd, rs1, rs2, FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fsqrtInsn(FPRegisterID rd, FPRegisterID rs1)
    {
        insnFP<fpSize, RISCV64Instructions::FSQRT_S, RISCV64Instructions::FSQRT_D>(rd, rs1, FPRegisterID(0), FPRoundingMode::DYN);
    }

    template<unsigned fpSize>
    void fsgnjInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FSGNJ_S, RISCV64Instructions::FSGNJ_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fsgnjnInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FSGNJN_S, RISCV64Instructions::FSGNJN_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fsgnjxInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FSGNJX_S, RISCV64Instructions::FSGNJX_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fminInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FMIN_S, RISCV64Instructions::FMIN_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fmaxInsn(FPRegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FMAX_S, RISCV64Instructions::FMAX_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void feqInsn(RegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FEQ_S, RISCV64Instructions::FEQ_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fltInsn(RegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FLT_S, RISCV64Instructions::FLT_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fleInsn(RegisterID rd, FPRegisterID rs1, FPRegisterID rs2)
    {
        insnFP<fpSize, RISCV64Instructions::FLE_S, RISCV64Instructions::FLE_D>(rd, rs1, rs2);
    }

    template<unsigned fpSize>
    void fclassInsn(RegisterID rd, FPRegisterID rs1)
    {
        insnFP<fpSize, RISCV64Instructions::FCLASS_S, RISCV64Instructions::FCLASS_D>(rd, rs1, FPRegisterID(0));
    }

    template<FPRoundingMode RM, FCVTType ToType, FCVTType FromType, typename RDType, typename RS1Type>
    void fcvtInsn(RDType rd, RS1Type rs1)
    {
        using FCVTType = RISCV64Instructions::FCVTBase<ToType, FromType>;
        static_assert(FCVTType::valid);
        static_assert(std::is_same_v<std::decay_t<RDType>, typename FCVTType::RDType>);
        static_assert(std::is_same_v<std::decay_t<RS1Type>, typename FCVTType::RS1Type>);

        insn(FCVTType::construct(rd, rs1, RM));
    }

    template<FCVTType ToType, FCVTType FromType, typename RDType, typename RS1Type>
    void fcvtInsn(RDType rd, RS1Type rs1)
    {
        fcvtInsn<FPRoundingMode::DYN, ToType, FromType, RDType, RS1Type>(rd, rs1);
    }

    template<FMVType ToType, FMVType FromType, typename RDType, typename RS1Type>
    void fmvInsn(RDType rd, RS1Type rs1)
    {
        using FMVType = RISCV64Instructions::FMVBase<ToType, FromType>;
        static_assert(FMVType::valid);
        static_assert(std::is_same_v<std::decay_t<RDType>, typename FMVType::RDType>);
        static_assert(std::is_same_v<std::decay_t<RS1Type>, typename FMVType::RS1Type>);

        insn(FMVType::construct(rd, rs1));
    }

    using MemoryOperation = RISCV64Instructions::MemoryOperation;
    using MemoryAccess = RISCV64Instructions::MemoryAccess;

    void fenceInsn(std::initializer_list<MemoryOperation> predecessor, std::initializer_list<MemoryOperation> successor)
    {
        unsigned predecessorValue = 0;
        for (auto& op : predecessor)
            predecessorValue |= unsigned(op);
        unsigned successorValue = 0;
        for (auto& op : successor)
            successorValue |= unsigned(op);

        uint32_t immediate = 0
            | (0b0000 << 8)
            | ((predecessorValue & ((1 << 4) - 1)) << 4)
            | ((successorValue & ((1 << 4) - 1)) << 0);
        insn(RISCV64Instructions::FENCE::construct(RISCV64Registers::zero, RISCV64Registers::zero, IImmediate(immediate)));
    }

    void lrwInsn(RegisterID rd, RegisterID rs1, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::LR_W::construct(rd, rs1, RISCV64Registers::zero, access));
    }

    void scwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::SC_W::construct(rd, rs1, rs2, access));
    }

    void lrdInsn(RegisterID rd, RegisterID rs1, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::LR_D::construct(rd, rs1, RISCV64Registers::zero, access));
    }

    void scdInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::SC_D::construct(rd, rs1, rs2, access));
    }

    void amoswapwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOSWAP_W::construct(rd, rs1, rs2, access));
    }

    void amoaddwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOADD_W::construct(rd, rs1, rs2, access));
    }

    void amoxorwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOXOR_W::construct(rd, rs1, rs2, access));
    }

    void amoandwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOAND_W::construct(rd, rs1, rs2, access));
    }

    void amoorwInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOOR_W::construct(rd, rs1, rs2, access));
    }

    void amoswapdInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOSWAP_D::construct(rd, rs1, rs2, access));
    }

    void amoadddInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOADD_D::construct(rd, rs1, rs2, access));
    }

    void amoxordInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOXOR_D::construct(rd, rs1, rs2, access));
    }

    void amoanddInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOAND_D::construct(rd, rs1, rs2, access));
    }

    void amoordInsn(RegisterID rd, RegisterID rs1, RegisterID rs2, std::initializer_list<MemoryAccess> access)
    {
        insn(RISCV64Instructions::AMOOR_D::construct(rd, rs1, rs2, access));
    }

    template<unsigned shiftAmount>
    void slliInsn(RegisterID rd, RegisterID rs1)
    {
        insn(RISCV64Instructions::SLLI::construct<shiftAmount>(rd, rs1));
    }

    template<unsigned shiftAmount>
    void srliInsn(RegisterID rd, RegisterID rs1)
    {
        insn(RISCV64Instructions::SRLI::construct<shiftAmount>(rd, rs1));
    }

    template<unsigned shiftAmount>
    void sraiInsn(RegisterID rd, RegisterID rs1)
    {
        insn(RISCV64Instructions::SRAI::construct<shiftAmount>(rd, rs1));
    }

    template<unsigned shiftAmount>
    void slliwInsn(RegisterID rd, RegisterID rs1)
    {
        insn(RISCV64Instructions::SLLIW::construct<shiftAmount>(rd, rs1));
    }

    template<unsigned shiftAmount>
    void srliwInsn(RegisterID rd, RegisterID rs1)
    {
        insn(RISCV64Instructions::SRLIW::construct<shiftAmount>(rd, rs1));
    }

    template<unsigned shiftAmount>
    void sraiwInsn(RegisterID rd, RegisterID rs1)
    {
        insn(RISCV64Instructions::SRAIW::construct<shiftAmount>(rd, rs1));
    }

    void nop()
    {
        addiInsn(RISCV64Registers::zero, RISCV64Registers::zero, IImmediate::v<IImmediate, 0>());
    }

    template<unsigned maskSize>
    void maskRegister(RegisterID rd, RegisterID rs)
    {
        static_assert(maskSize < 64);
        slliInsn<64 - maskSize>(rd, rs);
        srliInsn<64 - maskSize>(rd, rd);
    }

    template<unsigned maskSize>
    void maskRegister(RegisterID rd)
    {
        maskRegister<maskSize>(rd, rd);
    }

    template<unsigned bitSize>
    void signExtend(RegisterID rd)
    {
        signExtend<bitSize>(rd, rd);
    }

    template<unsigned bitSize, typename = std::enable_if_t<bitSize == 8 || bitSize == 16 || bitSize == 32 || bitSize == 64>>
    void signExtend(RegisterID rd, RegisterID rs)
    {
        if constexpr (bitSize == 64)
            return;

        if constexpr (bitSize == 32) {
            addiwInsn(rd, rs, IImmediate::v<IImmediate, 0>());
            return;
        }

        slliInsn<64 - bitSize>(rd, rs);
        sraiInsn<64 - bitSize>(rd, rd);
    }

    template<unsigned bitSize>
    void zeroExtend(RegisterID rd)
    {
        zeroExtend<bitSize>(rd, rd);
    }

    template<unsigned bitSize, typename = std::enable_if_t<bitSize == 8 || bitSize == 16 || bitSize == 32 || bitSize == 64>>
    void zeroExtend(RegisterID rd, RegisterID rs)
    {
        if constexpr (bitSize == 64)
            return;

        slliInsn<64 - bitSize>(rd, rs);
        srliInsn<64 - bitSize>(rd, rd);
    }

    template<typename F>
    void jumpPlaceholder(const F& functor)
    {
        LinkJumpImpl::generatePlaceholder(*this, functor);
    }

    template<typename F>
    void branchPlaceholder(const F& functor)
    {
        LinkBranchImpl::generatePlaceholder(*this, functor);
    }

    template<typename F>
    void pointerCallPlaceholder(const F& functor)
    {
        PatchPointerImpl::generatePlaceholder(*this, functor);
    }

    template<typename F>
    void nearCallPlaceholder(const F& functor)
    {
        LinkCallImpl::generatePlaceholder(*this, functor);
    }

    struct ImmediateLoader {
        enum PlaceholderTag { Placeholder };

        ImmediateLoader(int32_t imm)
            : ImmediateLoader(int64_t(imm))
        { }

        ImmediateLoader(PlaceholderTag, int32_t imm)
            : ImmediateLoader(Placeholder, int64_t(imm))
        { }

        ImmediateLoader(int64_t imm)
        {
            // If the immediate value fits into the IImmediate mold, we can short-cut to just generating that through a single ADDI.
            if (IImmediate::isValid(imm)) {
                m_ops[m_opCount++] = { Op::Type::IImmediate, IImmediate::v<IImmediate>(imm).imm };
                return;
            }

            // The immediate is larger than 12 bits, so it has to be loaded through the initial LUI and then additional shift-and-addi pairs.
            // This sequence is generated in reverse. moveInto() or other users traverse the sequence accordingly.
            int64_t value = imm;

            while (true) {
                uint32_t addiImm = value & ((1 << 12) - 1);
                // The addi will be sign-extending the 12-bit value and adding it to the register-contained value. If the addi-immediate
                // is negative, the remaining immediate has to be increased by 2^12 to offset the subsequent subtraction.
                if (addiImm & (1 << 11))
                    value += (1 << 12);
                m_ops[m_opCount++] = { Op::Type::ADDI, addiImm };

                // Shift out the bits incorporated into the just-added addi.
                value = value >> 12;

                // If the remainder of the immediate can fit into a 20-bit immediate, we can generate the LUI instruction that will end up
                // loading the initial higher bits of the desired immediate.
                if (ImmediateBase<20>::isValid(value)) {
                    m_ops[m_opCount++] = { Op::Type::LUI, uint32_t((value & ((1 << 20) - 1)) << 12) };
                    return;
                }

                // Otherwise, generate the lshift operation that will make room for lower parts of the immediate value.
                m_ops[m_opCount++] = { Op::Type::LSHIFT12, 0 };
            }
        }

        ImmediateLoader(PlaceholderTag, int64_t imm)
            : ImmediateLoader(imm)
        {
            // The non-placeholder constructor already generated the necessary operations to load this immediate.
            // This constructor still fills out the remaining potential operations as nops. This enables future patching
            // of these instructions with other immediate-load sequences.

            for (unsigned i = m_opCount; i < m_ops.size(); ++i)
                m_ops[i] = { Op::Type::NOP, 0 };
            m_opCount = m_ops.size();
        }

        void moveInto(RISCV64Assembler& assembler, RegisterID dest)
        {
            // This is a helper method that generates the necessary instructions through the RISCV64Assembler infrastructure.
            // Operations are traversed in reverse in order to match the generation process.

            for (unsigned i = 0; i < m_opCount; ++i) {
                auto& op = m_ops[m_opCount - (i + 1)];
                switch (op.type) {
                case Op::Type::IImmediate:
                    assembler.addiInsn(dest, RISCV64Registers::zero, IImmediate(op.value));
                    break;
                case Op::Type::LUI:
                    assembler.luiInsn(dest, UImmediate(op.value));
                    break;
                case Op::Type::ADDI:
                    assembler.addiInsn(dest, dest, IImmediate(op.value));
                    break;
                case Op::Type::LSHIFT12:
                    assembler.slliInsn<12>(dest, dest);
                    break;
                case Op::Type::NOP:
                    assembler.addiInsn(RISCV64Registers::zero, RISCV64Registers::zero, IImmediate::v<IImmediate, 0>());
                    break;
                }
            }
        }

        struct Op {
            enum class Type {
                IImmediate,
                LUI,
                ADDI,
                LSHIFT12,
                NOP,
            };

            Type type;
            uint32_t value;
        };
        std::array<Op, 8> m_ops;
        unsigned m_opCount { 0 };
    };

protected:
    void insn(uint32_t instruction)
    {
        m_buffer.putInt(instruction);
    }

    template<unsigned fpSize, typename FP32Type, typename FP64Type, typename... Args>
    void insnFP(Args&&... args)
    {
        static_assert(fpSize == 32 || fpSize == 64);
        using InstructionType = std::conditional_t<(fpSize == 32), FP32Type, FP64Type>;
        insn(InstructionType::construct(std::forward<Args>(args)...));
    }

    template<unsigned shiftBitsize>
    static constexpr bool isValidShiftAmount(unsigned amount)
    {
        return amount < (1 << shiftBitsize);
    }

    struct LinkJumpOrCallImpl {
        static void apply(uint32_t* location, void* target)
        {
            RISCV64Instructions::InstructionValue instruction(location[1]);
            RELEASE_ASSERT(instruction.opcode() == unsigned(RISCV64Instructions::Opcode::JALR) || instruction.opcode() == unsigned(RISCV64Instructions::Opcode::JAL));
            auto destination = RegisterID(instruction.field<7, 5>());

            intptr_t offset = uintptr_t(target) - uintptr_t(&location[1]);
            if (JImmediate::isValid(offset)) {
                location[0] = RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                location[1] = RISCV64Instructions::JAL::construct(destination, JImmediate::v<JImmediate>(offset));
                return;
            }

            offset += sizeof(uint32_t);
            RELEASE_ASSERT(ImmediateBase<32>::isValid(offset));
            RISCV64Instructions::ImmediateDecomposition immediate(offset);

            location[0] = RISCV64Instructions::AUIPC::construct(RISCV64Registers::x30, immediate.upper);
            location[1] = RISCV64Instructions::JALR::construct(destination, RISCV64Registers::x30, immediate.lower);
        }
    };

    struct LinkJumpImpl : LinkJumpOrCallImpl {
        static constexpr unsigned PlaceholderValue = 1;
        static uint32_t placeholderInsn()
        {
            return RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(RISCV64Assembler& assembler, const F& functor)
        {
            assembler.insn(placeholderInsn());
            functor();
        }
    };

    struct LinkCallImpl : LinkJumpOrCallImpl {
        static constexpr unsigned PlaceholderValue = 2;
        static uint32_t placeholderInsn()
        {
            return RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(RISCV64Assembler& assembler, const F& functor)
        {
            assembler.insn(placeholderInsn());
            functor();
        }
    };

    struct LinkBranchImpl {
        static constexpr unsigned PlaceholderValue = 3;
        static uint32_t placeholderInsn()
        {
            return RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(RISCV64Assembler& assembler, const F& functor)
        {
            auto insnValue = placeholderInsn();
            for (unsigned i = 0; i < 2; ++i)
                assembler.insn(insnValue);
            functor();
        }

        static void apply(uint32_t* location, void* target)
        {
            RISCV64Instructions::InstructionValue instruction(location[2]);
            RELEASE_ASSERT(instruction.opcode() == unsigned(RISCV64Instructions::Opcode::BRANCH));

            auto branchInstructionForFunct3 =
                [](uint32_t funct3, RegisterID rs1, RegisterID rs2, BImmediate imm)
                {
                    switch (funct3) {
                    case RISCV64Instructions::BEQ::funct3Value:
                        return RISCV64Instructions::BEQ::construct(rs1, rs2, imm);
                    case RISCV64Instructions::BNE::funct3Value:
                        return RISCV64Instructions::BNE::construct(rs1, rs2, imm);
                    case RISCV64Instructions::BLT::funct3Value:
                        return RISCV64Instructions::BLT::construct(rs1, rs2, imm);
                    case RISCV64Instructions::BGE::funct3Value:
                        return RISCV64Instructions::BGE::construct(rs1, rs2, imm);
                    case RISCV64Instructions::BLTU::funct3Value:
                        return RISCV64Instructions::BLTU::construct(rs1, rs2, imm);
                    case RISCV64Instructions::BGEU::funct3Value:
                        return RISCV64Instructions::BGEU::construct(rs1, rs2, imm);
                    default:
                        break;
                    }

                    RELEASE_ASSERT_NOT_REACHED();
                    return RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                };
            auto lhs = RegisterID(instruction.field<15, 5>());
            auto rhs = RegisterID(instruction.field<20, 5>());

            intptr_t offset = uintptr_t(target) - uintptr_t(&location[2]);
            if (BImmediate::isValid(offset)) {
                location[0] = RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                location[1] = RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                location[2] = branchInstructionForFunct3(instruction.field<12, 3>(), lhs, rhs, BImmediate::v<BImmediate>(offset));
                return;
            }

            if (JImmediate::isValid(offset)) {
                location[0] = RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                location[1] = branchInstructionForFunct3(instruction.field<12, 3>() ^ 0b001, lhs, rhs, BImmediate::v<BImmediate, 8>());
                location[2] = RISCV64Instructions::JAL::construct(RISCV64Registers::x0, JImmediate::v<JImmediate>(offset));
                return;
            }

            offset += sizeof(uint32_t);
            RELEASE_ASSERT(ImmediateBase<32>::isValid(offset));
            RISCV64Instructions::ImmediateDecomposition immediate(offset);

            location[0] = branchInstructionForFunct3(instruction.field<12, 3>() ^ 0b001, lhs, rhs, BImmediate::v<BImmediate, 12>());
            location[1] = RISCV64Instructions::AUIPC::construct(RISCV64Registers::x31, immediate.upper);
            location[2] = RISCV64Instructions::JALR::construct(RISCV64Registers::x0, RISCV64Registers::x31, immediate.lower);
        }
    };

    struct PatchPointerImpl {
        static constexpr unsigned PlaceholderValue = 4;
        static uint32_t placeholderInsn()
        {
            return RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(RISCV64Assembler& assembler, const F& functor)
        {
            auto insnValue = placeholderInsn();
            for (unsigned i = 0; i < 7; ++i)
                assembler.insn(insnValue);
            functor();
        }

        static void apply(uint32_t* location, void* value)
        {
            RISCV64Instructions::InstructionValue instruction(location[7]);
            // RELEASE_ASSERT, being a macro, gets confused by the comma-separated template parameters.
            bool validLocation = instruction.opcode() == unsigned(RISCV64Instructions::Opcode::OP_IMM) && instruction.field<12, 3>() == 0b000;
            RELEASE_ASSERT(validLocation);

            apply(location, RegisterID(instruction.field<7, 5>()), value);
        }

        static void apply(uint32_t* location, RegisterID destination, void* value)
        {
            using ImmediateLoader = RISCV64Assembler::ImmediateLoader;
            ImmediateLoader imml(ImmediateLoader::Placeholder, reinterpret_cast<intptr_t>(value));
            RELEASE_ASSERT(imml.m_opCount == 8);

            for (unsigned i = 0; i < imml.m_opCount; ++i) {
                auto& op = imml.m_ops[imml.m_opCount - (i + 1)];
                switch (op.type) {
                case ImmediateLoader::Op::Type::IImmediate:
                    location[i] = RISCV64Instructions::ADDI::construct(destination, RISCV64Registers::zero, IImmediate(op.value));
                    break;
                case ImmediateLoader::Op::Type::LUI:
                    location[i] = RISCV64Instructions::LUI::construct(destination, UImmediate(op.value));
                    break;
                case ImmediateLoader::Op::Type::ADDI:
                    location[i] = RISCV64Instructions::ADDI::construct(destination, destination, IImmediate(op.value));
                    break;
                case ImmediateLoader::Op::Type::LSHIFT12:
                    location[i] = RISCV64Instructions::SLLI::construct<12>(destination, destination);
                    break;
                case ImmediateLoader::Op::Type::NOP:
                    location[i] = RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                    break;
                }
            }
        }

        static void* read(uint32_t* location)
        {
            RISCV64Instructions::InstructionValue instruction(location[7]);
            // RELEASE_ASSERT, being a macro, gets confused by the comma-separated template parameters.
            bool validLocation = instruction.opcode() == unsigned(RISCV64Instructions::Opcode::OP_IMM) && instruction.field<12, 3>() == 0b000;
            RELEASE_ASSERT(validLocation);

            auto dest = RegisterID(instruction.field<7, 5>());

            unsigned i = 0;
            {
                // Iterate through all ImmediateLoader::Op::Type::NOP instructions generated for the purposes of the placeholder.
                uint32_t nopInsn = RISCV64Instructions::ADDI::construct(RISCV64Registers::x0, RISCV64Registers::x0, IImmediate::v<IImmediate, 0>());
                for (; i < 8; ++i) {
                    if (location[i] != nopInsn)
                        break;
                }
            }

            intptr_t target = 0;
            for (; i < 8; ++i) {
                RISCV64Instructions::InstructionValue insn(location[i]);

                // Counterpart to ImmediateLoader::Op::Type::LUI.
                if (insn.opcode() == unsigned(RISCV64Instructions::Opcode::LUI) && insn.field<7, 5>() == unsigned(dest)) {
                    target = int32_t(UImmediate::value(insn));
                    continue;
                }

                // Counterpart to ImmediateLoader::Op::Type::{IImmediate, ADDI}.
                if (insn.opcode() == unsigned(RISCV64Instructions::Opcode::OP_IMM) && insn.field<12, 3>() == 0b000
                    && insn.field<7, 5>() == unsigned(dest) && insn.field<15, 5>() == unsigned(dest)) {
                    target += IImmediate::value(insn);
                    continue;
                }

                // Counterpart to ImmediateLoader::Op::Type::LSHIFT12.
                if (insn.value == RISCV64Instructions::SLLI::construct<12>(dest, dest)) {
                    target <<= 12;
                    continue;
                }

                RELEASE_ASSERT_NOT_REACHED();
                return nullptr;
            }

            return reinterpret_cast<void*>(target);
        }
    };

    AssemblerBuffer m_buffer;
    int m_indexOfLastWatchpoint { INT_MIN };
    int m_indexOfTailOfLastWatchpoint { INT_MIN };
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(RISCV64)
