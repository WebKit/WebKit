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
        return ImmediateType((*reinterpret_cast<uint32_t*>(&value)) & ((1 << immediateSize) - 1));
    }

    template<typename ImmediateType>
    static ImmediateType v(int32_t immValue)
    {
        ASSERT(isValid(immValue));
        uint32_t value = *reinterpret_cast<uint32_t*>(&immValue);
        return ImmediateType(value & ((1 << immediateSize) - 1));
    }

    template<typename ImmediateType>
    static ImmediateType v(int64_t immValue)
    {
        ASSERT(isValid(immValue));
        uint64_t value = *reinterpret_cast<uint64_t*>(&immValue);
        return ImmediateType(uint32_t(value & ((uint64_t(1) << immediateSize) - 1)));
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

    using Base = STypeBase<opcode, funct3, RegisterTypes>;
    using Registers = STypeRegisters<RegisterTypes>;

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

    typedef enum {
        ConditionEQ,
        ConditionNE,
        ConditionHS, ConditionCS = ConditionHS,
        ConditionLO, ConditionCC = ConditionLO,
        ConditionMI,
        ConditionPL,
        ConditionVS,
        ConditionVC,
        ConditionHI,
        ConditionLS,
        ConditionGE,
        ConditionLT,
        ConditionGT,
        ConditionLE,
        ConditionAL,
        ConditionInvalid
    } Condition;

    static Condition invert(Condition cond)
    {
        return static_cast<Condition>(cond ^ 1);
    }

    static void* getRelocatedAddress(void* code, AssemblerLabel label)
    {
        ASSERT(label.isSet());
        return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + label.offset());
    }

    size_t codeSize() const { return m_buffer.codeSize(); }

    static unsigned getCallReturnOffset(AssemblerLabel) { return 0; }

    AssemblerLabel labelIgnoringWatchpoints() { return { }; }
    AssemblerLabel labelForWatchpoint() { return { }; }
    AssemblerLabel label() { return { }; }

    static void linkJump(void*, AssemblerLabel, void*) { }

    static void linkJump(AssemblerLabel, AssemblerLabel) { }

    static void linkCall(void*, AssemblerLabel, void*) { }

    static void linkPointer(void*, AssemblerLabel, void*) { }

    static ptrdiff_t maxJumpReplacementSize()
    {
        return 4;
    }

    static constexpr ptrdiff_t patchableJumpSize()
    {
        return 4;
    }

    static void repatchPointer(void*, void*) { }

    static void relinkJump(void*, void*) { }
    static void relinkJumpToNop(void*) { }
    static void relinkCall(void*, void*) { }

    unsigned debugOffset() { return m_buffer.debugOffset(); }

    static void cacheFlush(void*, size_t) { }

    using CopyFunction = void*(&)(void*, const void*, size_t);
    template <CopyFunction copy>
    static void fillNops(void*, size_t) { }

    AssemblerBuffer m_buffer;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(RISCV64)
