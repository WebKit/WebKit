/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023-2024 Loongson Technology. All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(LOONGARCH64)

#include "AssemblerBuffer.h"
#include "AssemblerCommon.h"
#include "LOONGARCH64Registers.h"
#include <tuple>

namespace JSC {

namespace LOONGARCH64Registers {

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

typedef enum : int8_t {
#define REGISTER_ID(id, name, r, cs) id,
    FOR_EACH_CF_REGISTER(REGISTER_ID)
#undef REGISTER_ID

    InvalidCFRReg = -1,
} CFRegisterID;

} // namespace LOONGARCH64Registers

namespace LOONGARCH64Instructions {

enum class Opcode22 : unsigned {
    FABS_S_OP          = 0b0000000100010100000001,
    FABS_D_OP          = 0b0000000100010100000010,
    FNEG_S_OP          = 0b0000000100010100000101,
    FNEG_D_OP          = 0b0000000100010100000110,
    FCLASS_S_OP        = 0b0000000100010100001101,
    FCLASS_D_OP        = 0b0000000100010100001110,
    FSQRT_S_OP         = 0b0000000100010100010001,
    FSQRT_D_OP         = 0b0000000100010100010010,
    FMOV_S_OP          = 0b0000000100010100100101,
    FMOV_D_OP          = 0b0000000100010100100110,
    MOVGR2FR_W_OP      = 0b0000000100010100101001,
    MOVGR2FR_D_OP      = 0b0000000100010100101010,
    MOVFR2GR_S_OP      = 0b0000000100010100101101,
    MOVFR2GR_D_OP      = 0b0000000100010100101110,
    MOVCF2GR_OP        = 0b0000000100010100110111,
    FCVT_S_D_OP        = 0b0000000100011001000110,
    FCVT_D_S_OP        = 0b0000000100011001001001,
    FTINTRM_W_S_OP     = 0b0000000100011010000001,
    FTINTRM_W_D_OP     = 0b0000000100011010000010,
    FTINTRM_L_S_OP     = 0b0000000100011010001001,
    FTINTRM_L_D_OP     = 0b0000000100011010001010,
    FTINTRP_W_S_OP     = 0b0000000100011010010001,
    FTINTRP_W_D_OP     = 0b0000000100011010010010,
    FTINTRP_L_S_OP     = 0b0000000100011010011001,
    FTINTRP_L_D_OP     = 0b0000000100011010011010,
    FTINTRZ_W_S_OP     = 0b0000000100011010100001,
    FTINTRZ_W_D_OP     = 0b0000000100011010100010,
    FTINTRZ_L_S_OP     = 0b0000000100011010101001,
    FTINTRZ_L_D_OP     = 0b0000000100011010101010,
    FTINTRNE_W_S_OP    = 0b0000000100011010110001,
    FTINTRNE_W_D_OP    = 0b0000000100011010110010,
    FTINTRNE_L_S_OP    = 0b0000000100011010111001,
    FTINTRNE_L_D_OP    = 0b0000000100011010111010,
    FFINT_S_W_OP       = 0b0000000100011101000100,
    FFINT_S_L_OP       = 0b0000000100011101000110,
    FFINT_D_W_OP       = 0b0000000100011101001000,
    FFINT_D_L_OP       = 0b0000000100011101001010,
    VFRINTRM_S_OP      = 0b0111001010011101010001,
    VFRINTRM_D_OP      = 0b0111001010011101010010,
    VFRINTRP_S_OP      = 0b0111001010011101010101,
    VFRINTRP_D_OP      = 0b0111001010011101010110,
    VFRINTRZ_S_OP      = 0b0111001010011101011001,
    VFRINTRZ_D_OP      = 0b0111001010011101011010,
    VFRINTRNE_S_OP     = 0b0111001010011101011101,
    VFRINTRNE_D_OP     = 0b0111001010011101011110,
};

enum class Opcode17 : unsigned {
    ADD_W_OP           = 0b00000000000100000,
    ADD_D_OP           = 0b00000000000100001,
    SUB_W_OP           = 0b00000000000100010,
    SUB_D_OP           = 0b00000000000100011,
    SLT_OP             = 0b00000000000100100,
    SLTU_OP            = 0b00000000000100101,
    AND_OP             = 0b00000000000101001,
    OR_OP              = 0b00000000000101010,
    XOR_OP             = 0b00000000000101011,
    SLL_W_OP           = 0b00000000000101110,
    SRL_W_OP           = 0b00000000000101111,
    SRA_W_OP           = 0b00000000000110000,
    SLL_D_OP           = 0b00000000000110001,
    SRL_D_OP           = 0b00000000000110010,
    SRA_D_OP           = 0b00000000000110011,
    MUL_W_OP           = 0b00000000000111000,
    MULH_W_OP          = 0b00000000000111001,
    MULH_WU_OP         = 0b00000000000111010,
    MUL_D_OP           = 0b00000000000111011,
    MULH_D_OP          = 0b00000000000111100,
    MULH_DU_OP         = 0b00000000000111101,
    MULW_D_W_OP        = 0b00000000000111110,
    MULW_D_WU_OP       = 0b00000000000111111,
    DIV_W_OP           = 0b00000000001000000,
    MOD_W_OP           = 0b00000000001000001,
    DIV_WU_OP          = 0b00000000001000010,
    MOD_WU_OP          = 0b00000000001000011,
    DIV_D_OP           = 0b00000000001000100,
    MOD_D_OP           = 0b00000000001000101,
    DIV_DU_OP          = 0b00000000001000110,
    MOD_DU_OP          = 0b00000000001000111,
    BREAK_OP           = 0b00000000001010100,
    FADD_S_OP          = 0b00000001000000001,
    FADD_D_OP          = 0b00000001000000010,
    FSUB_S_OP          = 0b00000001000000101,
    FSUB_D_OP          = 0b00000001000000110,
    FMUL_S_OP          = 0b00000001000001001,
    FMUL_D_OP          = 0b00000001000001010,
    FDIV_S_OP          = 0b00000001000001101,
    FDIV_D_OP          = 0b00000001000001110,
    FMAX_S_OP          = 0b00000001000010001,
    FMAX_D_OP          = 0b00000001000010010,
    FMIN_S_OP          = 0b00000001000010101,
    FMIN_D_OP          = 0b00000001000010110,
    FCOPYSIGN_S_OP     = 0b00000001000100101,
    FCOPYSIGN_D_OP     = 0b00000001000100110,
    DBAR_OP            = 0b00111000011100100,
    VAND_V_OP          = 0b01110001001001100,
    VOR_V_OP           = 0b01110001001001101,
};

enum class Opcode14 : unsigned {
    SLLI_OP            = 0b00000000010000,
    SRLI_OP            = 0b00000000010001,
    SRAI_OP            = 0b00000000010010,
};

enum class Opcode12 : unsigned {
    FCMP_COND_S_OP     = 0b000011000001,
    FCMP_COND_D_OP     = 0b000011000010,
};

enum class Opcode10 : unsigned {
    SLTI_OP            = 0b0000001000,
    SLTUI_OP           = 0b0000001001,
    ADDI_W_OP          = 0b0000001010,
    ADDI_D_OP          = 0b0000001011,
    LU52I_D_OP         = 0b0000001100,
    ANDI_OP            = 0b0000001101,
    ORI_OP             = 0b0000001110,
    XORI_OP            = 0b0000001111,
    LD_B_OP            = 0b0010100000,
    LD_H_OP            = 0b0010100001,
    LD_W_OP            = 0b0010100010,
    LD_D_OP            = 0b0010100011,
    ST_B_OP            = 0b0010100100,
    ST_H_OP            = 0b0010100101,
    ST_W_OP            = 0b0010100110,
    ST_D_OP            = 0b0010100111,
    LD_BU_OP           = 0b0010101000,
    LD_HU_OP           = 0b0010101001,
    LD_WU_OP           = 0b0010101010,
    FLD_S_OP           = 0b0010101100,
    FST_S_OP           = 0b0010101101,
    FLD_D_OP           = 0b0010101110,
    FST_D_OP           = 0b0010101111,
};

enum class Opcode8 : unsigned {
    LL_W_OP            = 0b00100000,
    SC_W_OP            = 0b00100001,
    LL_D_OP            = 0b00100010,
    SC_D_OP            = 0b00100011,
};

enum class Opcode7 : unsigned {
    LU12I_W_OP         = 0b0001010,
    LU32I_D_OP         = 0b0001011,
    PCADDU18I_OP       = 0b0001111,
};

enum class Opcode6 : unsigned {
    BEQZ_OP            = 0b010000,
    BNEZ_OP            = 0b010001,
    BCCONDZ_OP         = 0b010010,
    JIRL_OP            = 0b010011,
    B_OP               = 0b010100,
    BL_OP              = 0b010101,
    BEQ_OP             = 0b010110,
    BNE_OP             = 0b010111,
    BLT_OP             = 0b011000,
    BGE_OP             = 0b011001,
    BLTU_OP            = 0b011010,
    BGEU_OP            = 0b011011,
};

enum class FCmpCond : unsigned {
    FCMP_CAF           = 0x00,
    FCMP_CUN           = 0x08,
    FCMP_CEQ           = 0x04,
    FCMP_CUEQ          = 0x0c,
    FCMP_CLT           = 0x02,
    FCMP_CULT          = 0x0a,
    FCMP_CLE           = 0x06,
    FCMP_CULE          = 0x0e,
    FCMP_CNE           = 0x10,
    FCMP_COR           = 0x14,
    FCMP_CUNE          = 0x18,
    FCMP_SAF           = 0x01,
    FCMP_SUN           = 0x09,
    FCMP_SEQ           = 0x05,
    FCMP_SUEQ          = 0x0d,
    FCMP_SLT           = 0x03,
    FCMP_SULT          = 0x0b,
    FCMP_SLE           = 0x07,
    FCMP_SULE          = 0x0f,
    FCMP_SNE           = 0x11,
    FCMP_SOR           = 0x15,
    FCMP_SUNE          = 0x19
};

enum class FPRoundingMode : unsigned {
    RM  = 0b000,
    RP  = 0b001,
    RZ  = 0b011,
    RNE = 0b100,
};

// Register helpers

using RegisterID = LOONGARCH64Registers::RegisterID;
using FPRegisterID = LOONGARCH64Registers::FPRegisterID;
using CFRegisterID = LOONGARCH64Registers::CFRegisterID;

template<typename T>
auto registerValue(T registerID)
    -> std::enable_if_t<(std::is_same_v<T, RegisterID> || std::is_same_v<T, FPRegisterID> || std::is_same_v<T, CFRegisterID>), unsigned>
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
    static_assert(immediateSize <= sizeof(uint64_t) * 8);

    template<typename T>
    static constexpr T immediateMask()
    {
        if constexpr(immediateSize < sizeof(uint32_t) * 8)
            return ((T(1) << immediateSize) - 1);
        return T(~0);
    }

    template<unsigned nBitsize>
    static bool isUImm(int32_t x)
    {
        static_assert(0 < nBitsize && nBitsize < 32);
        const int32_t maxplus1 = (((int32_t)1) << nBitsize);
        return 0 <= x && x < maxplus1;
    }

    template<unsigned nBitsize>
    static bool isUImm(int64_t x)
    {
        static_assert(0 < nBitsize && nBitsize < 64);
        const int64_t maxplus1 = (((int64_t)1) << nBitsize);
        return 0 <= x && x < maxplus1;
    }

    template<unsigned nBitsize>
    static bool isSImm(int32_t x)
    {
        static_assert(0 < nBitsize && nBitsize < 32);
        const int32_t min      = -(((int32_t)1) << (nBitsize-1));
        const int32_t maxplus1 =  (((int32_t)1) << (nBitsize-1));
        return min <= x && x < maxplus1;
    }

    template<unsigned nBitsize>
    static bool isSImm(int64_t x)
    {
        static_assert(0 < nBitsize && nBitsize < 64);
        const int64_t min      = -(((int64_t)1) << (nBitsize-1));
        const int64_t maxplus1 =  (((int64_t)1) << (nBitsize-1));
        return min <= x && x < maxplus1;
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
        ASSERT_WITH_MESSAGE(isSImm<immediateSize>(immValue), "invalid %d", immValue);
        uint32_t value = *reinterpret_cast<uint32_t*>(&immValue);
        return ImmediateType(value & immediateMask<uint32_t>());
    }

    template<typename ImmediateType>
    static ImmediateType v(int64_t immValue)
    {
        ASSERT_WITH_MESSAGE(isSImm<immediateSize>(immValue), "invalid %ld", immValue);
        uint64_t value = *reinterpret_cast<uint64_t*>(&immValue);
        return ImmediateType(uint32_t(value & immediateMask<uint64_t>()));
    }

    explicit ImmediateBase(uint32_t immValue)
        : imm(immValue)
    {
        if constexpr (immediateSize < sizeof(uint32_t) * 8)
            ASSERT_WITH_MESSAGE(imm < (1 << immediateSize), "invalid %d", immValue);
    }

    template<unsigned fieldStart, unsigned fieldSize>
    uint32_t field()
    {
        static_assert(fieldStart + fieldSize <= immediateSize);
        return (imm >> fieldStart) & ((1 << fieldSize) - 1);
    }

    uint32_t imm;
};

struct I8Immediate : ImmediateBase<8> {
    explicit I8Immediate(uint32_t immValue)
        : ImmediateBase<8>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<10, 8>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 24) >> 24);
    }
};

struct I12Immediate : ImmediateBase<12> {
    explicit I12Immediate(uint32_t immValue)
        : ImmediateBase<12>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<10, 12>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 20) >> 20);
    }
};

struct I14Immediate : ImmediateBase<14> {
    explicit I14Immediate(uint32_t immValue)
        : ImmediateBase<14>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<10, 14>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 18) >> 18);
    }
};

struct I15Immediate : ImmediateBase<15> {
    explicit I15Immediate(uint32_t immValue)
        : ImmediateBase<15>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<0, 15>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 17) >> 17);
    }
};

struct I16Immediate : ImmediateBase<16> {
    explicit I16Immediate(uint32_t immValue)
        : ImmediateBase<16>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<10, 16>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 16) >> 16);
    }
};

struct I20Immediate : ImmediateBase<20> {
    explicit I20Immediate(uint32_t immValue)
        : ImmediateBase<20>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = insn.field<5, 20>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 12) >> 12);
    }
};

struct I21Immediate : ImmediateBase<21> {
    explicit I21Immediate(uint32_t immValue)
        : ImmediateBase<21>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = (insn.field<0, 5>() << 16) | insn.field<10, 16>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 11) >> 11);
    }
};

struct I26Immediate : ImmediateBase<26> {
    explicit I26Immediate(uint32_t immValue)
        : ImmediateBase<26>(immValue)
    { }

    static int32_t value(InstructionValue insn)
    {
        uint32_t base = (insn.field<0, 10>() << 16) | insn.field<10, 16>();
        int32_t imm = *reinterpret_cast<int32_t*>(&base);
        return ((imm << 6) >> 6);
    }
};

// Instruction types

// Helper struct that provides different groupings of register types as required for different instructions.
// The tuple size and contained types are used for compile-time checks of matching register types being passed
// to those instructions.

struct RegistersBase {
    struct GType { }; // General-purpose register
    struct FType { }; // Floating-point register
    struct CType { }; // Conditional-flag register

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
    using C = Tuple<CType>;
    using GG = Tuple<GType, GType>;
    using GF = Tuple<GType, FType>;
    using FG = Tuple<FType, GType>;
    using CG = Tuple<CType, GType>;
    using FF = Tuple<FType, FType>;
    using GGG = Tuple<GType, GType, GType>;
    using FFF = Tuple<FType, FType, FType>;
    using CFFF = Tuple<CType, FType, FType, FType>;
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
struct RRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RJ = RegistersBase::Type<0, RegisterTypes>;
    using RD = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode22 opcode, typename RegisterTypes>
struct RRTypeBase {
    using Base = RRTypeBase<opcode, RegisterTypes>;
    using Registers = RRTypeRegisters<RegisterTypes>;

    template<typename RJType, typename RDType>
    static uint32_t construct(RJType rj, RDType rd)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<10, 22>();
    }

    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
};

template<typename RegisterTypes>
struct RRRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 3);
    using RK = RegistersBase::Type<0, RegisterTypes>;
    using RJ = RegistersBase::Type<1, RegisterTypes>;
    using RD = RegistersBase::Type<2, RegisterTypes>;
};

template<Opcode17 opcode, typename RegisterTypes>
struct RRRTypeBase {
    using Base = RRRTypeBase<opcode, RegisterTypes>;
    using Registers = RRRTypeRegisters<RegisterTypes>;

    template<typename RKType, typename RJType, typename RDType>
    static uint32_t construct(RKType rk, RJType rj, RDType rd)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 15)
            | (registerValue(rk) << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<15, 17>();
    }

    static uint8_t rk(InstructionValue insn) { return insn.field<10, 5>(); }
    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
};

template<typename RegisterTypes>
struct RRRRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 4);
    using RA = RegistersBase::Type<0, RegisterTypes>;
    using RK = RegistersBase::Type<1, RegisterTypes>;
    using RJ = RegistersBase::Type<2, RegisterTypes>;
    using RD = RegistersBase::Type<3, RegisterTypes>;
};

template<Opcode12 opcode, typename RegisterTypes>
struct RRRRTypeBase {
    using Base = RRRRTypeBase<opcode, RegisterTypes>;
    using Registers = RRRRTypeRegisters<RegisterTypes>;

    template<typename RAType, typename RKType, typename RJType, typename RDType>
    static uint32_t construct(RAType ra, RKType rk, RJType rj, RDType rd)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 20)
            | (registerValue(ra) << 15)
            | (registerValue(rk) << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<20, 12>();
    }

    static uint8_t ra(InstructionValue insn) { return insn.field<15, 5>(); }
    static uint8_t rk(InstructionValue insn) { return insn.field<10, 5>(); }
    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
};

template<Opcode6 opcode>
struct I26TypeBase {
    using Base = I26TypeBase<opcode>;

    static uint32_t construct(I26Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 26)
            | (imm.field<0, 16>() << 10)
            | (imm.field<16, 10>());
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<26, 6>();
    }
};

template<typename RegisterTypes>
struct I20RTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 1);
    using RD = RegistersBase::Type<0, RegisterTypes>;
};

template<Opcode7 opcode, typename RegisterTypes>
struct I20RTypeBase {
    using Base = I20RTypeBase<opcode, RegisterTypes>;
    using Registers = I20RTypeRegisters<RegisterTypes>;

    template<typename RDType>
    static uint32_t construct(RDType rd, I20Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 25)
            | (imm.field<0, 20>() << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<25, 7>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
};

template<typename RegisterTypes>
struct I16RRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RJ = RegistersBase::Type<0, RegisterTypes>;
    using RD = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode6 opcode, typename RegisterTypes>
struct I16RRTypeBase {
    using Base = I16RRTypeBase<opcode, RegisterTypes>;
    using Registers = I16RRTypeRegisters<RegisterTypes>;

    template<typename RJType, typename RDType>
    static uint32_t construct(RJType rj, RDType rd, I16Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 26)
            | (imm.field<0, 16>() << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<26, 6>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
};

template<typename RegisterTypes>
struct IRITypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 1);
    using RJ = RegistersBase::Type<0, RegisterTypes>;
};

template<Opcode6 opcode, typename RegisterTypes>
struct IRITypeBase {
    using Base = IRITypeBase<opcode, RegisterTypes>;
    using Registers = IRITypeRegisters<RegisterTypes>;

    template<typename RJType>
    static uint32_t construct(RJType rj, I21Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 26)
            | (imm.field<0, 16>() << 10)
            | (registerValue(rj) << 5)
            | imm.field<16, 5>();
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<26, 6>();
    }

    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
};

template<typename RegisterTypes>
struct I14RRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RJ = RegistersBase::Type<0, RegisterTypes>;
    using RD = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode8 opcode, typename RegisterTypes>
struct I14RRTypeBase {
    using Base = I14RRTypeBase<opcode, RegisterTypes>;
    using Registers = I14RRTypeRegisters<RegisterTypes>;

    template<typename RJType, typename RDType>
    static uint32_t construct(RJType rj, RDType rd, I14Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 24)
            | (imm.field<0, 14>() << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<24, 8>();
    }

    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
};

template<Opcode17 opcode>
struct I15TypeBase {
    using Base = I15TypeBase<opcode>;

    static uint32_t construct(I15Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 15)
            | imm.field<0, 15>();
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<15, 17>();
    }
};

template<typename RegisterTypes>
struct I12RRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RJ = RegistersBase::Type<0, RegisterTypes>;
    using RD = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode10 opcode, typename RegisterTypes>
struct I12RRTypeBase {
    using Base = I12RRTypeBase<opcode, RegisterTypes>;
    using Registers = I12RRTypeRegisters<RegisterTypes>;

    template<typename RJType, typename RDType>
    static uint32_t construct(RJType rj, RDType rd, I12Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 22)
            | (imm.field<0, 12>() << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<22, 10>();
    }

    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
};

template<typename RegisterTypes>
struct I8RRTypeRegisters {
    static_assert(RegistersBase::Size<RegisterTypes>() == 2);
    using RJ = RegistersBase::Type<0, RegisterTypes>;
    using RD = RegistersBase::Type<1, RegisterTypes>;
};

template<Opcode14 opcode, typename RegisterTypes>
struct I8RRTypeBase {
    using Base = I8RRTypeBase<opcode, RegisterTypes>;
    using Registers = I8RRTypeRegisters<RegisterTypes>;

    template<typename RJType, typename RDType>
    static uint32_t construct(RJType rj, RDType rd, I8Immediate imm)
    {
        uint32_t instruction = 0
            | (unsigned(opcode) << 18)
            | (imm.field<0, 8>() << 10)
            | (registerValue(rj) << 5)
            | registerValue(rd);
        return instruction;
    }

    static bool matches(InstructionValue insn)
    {
        return unsigned(opcode) == insn.field<18, 14>();
    }

    static uint8_t rj(InstructionValue insn) { return insn.field<5, 5>(); }
    static uint8_t rd(InstructionValue insn) { return insn.field<0, 5>(); }
};

// The following instruction definitions utilize the base instruction structs, in most cases specifying everything
// necessary in the template parameters of the base instruction struct they are inheriting from. For each instruction
// there's also a pretty-print name constant included in the definition, for use by the disassembler.

struct LL_W : I14RRTypeBase<Opcode8::LL_W_OP, RegistersBase::GG> {
    static constexpr const char* name = "ll.w";
};

struct SC_W : I14RRTypeBase<Opcode8::SC_W_OP, RegistersBase::GG> {
    static constexpr const char* name = "sc.w";
};

struct LL_D : I14RRTypeBase<Opcode8::LL_D_OP, RegistersBase::GG> {
    static constexpr const char* name = "ll.d";
};

struct SC_D : I14RRTypeBase<Opcode8::SC_D_OP, RegistersBase::GG> {
    static constexpr const char* name = "sc.d";
};

struct LU32I_D : I20RTypeBase<Opcode7::LU32I_D_OP, RegistersBase::G> {
    static constexpr const char* name = "lu32i.d";
};

struct LU12I_W : I20RTypeBase<Opcode7::LU12I_W_OP, RegistersBase::G> {
    static constexpr const char* name = "lu12i.w";
};

struct PCADDU18I : I20RTypeBase<Opcode7::PCADDU18I_OP, RegistersBase::G> {
    static constexpr const char* name = "pcaddu18i";
};

struct B : I26TypeBase<Opcode6::B_OP> {
    static constexpr const char* name = "b";
};

struct BL : I26TypeBase<Opcode6::BL_OP> {
    static constexpr const char* name = "bl";
};

struct JIRL : I16RRTypeBase<Opcode6::JIRL_OP, RegistersBase::GG> {
    static constexpr const char* name = "jirl";
};

struct BEQ : I16RRTypeBase<Opcode6::BEQ_OP, RegistersBase::GG> {
    static constexpr const char* name = "beq";
};

struct BNE : I16RRTypeBase<Opcode6::BNE_OP, RegistersBase::GG> {
    static constexpr const char* name = "bne";
};

struct BLT : I16RRTypeBase<Opcode6::BLT_OP, RegistersBase::GG> {
    static constexpr const char* name = "blt";
};

struct BGE : I16RRTypeBase<Opcode6::BGE_OP, RegistersBase::GG> {
    static constexpr const char* name = "bge";
};

struct BLTU : I16RRTypeBase<Opcode6::BLTU_OP, RegistersBase::GG> {
    static constexpr const char* name = "bltu";
};

struct BGEU : I16RRTypeBase<Opcode6::BGEU_OP, RegistersBase::GG> {
    static constexpr const char* name = "bgeu";
};

struct BCNEZ : IRITypeBase<Opcode6::BCCONDZ_OP, RegistersBase::C> {
    static constexpr const char* name = "bcnez";
};

struct LD_B : I12RRTypeBase<Opcode10::LD_B_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.b";
};

struct LD_H : I12RRTypeBase<Opcode10::LD_H_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.h";
};

struct LD_W : I12RRTypeBase<Opcode10::LD_W_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.w";
};

struct LD_D : I12RRTypeBase<Opcode10::LD_D_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.d";
};

struct LD_BU : I12RRTypeBase<Opcode10::LD_BU_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.bu";
};

struct LD_HU : I12RRTypeBase<Opcode10::LD_HU_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.hu";
};

struct LD_WU : I12RRTypeBase<Opcode10::LD_WU_OP, RegistersBase::GG> {
    static constexpr const char* name = "ld.wu";
};

struct ST_B : I12RRTypeBase<Opcode10::ST_B_OP, RegistersBase::GG> {
    static constexpr const char* name = "st.b";
};

struct ST_H : I12RRTypeBase<Opcode10::ST_H_OP, RegistersBase::GG> {
    static constexpr const char* name = "st.h";
};

struct ST_W : I12RRTypeBase<Opcode10::ST_W_OP, RegistersBase::GG> {
    static constexpr const char* name = "st.w";
};

struct ST_D : I12RRTypeBase<Opcode10::ST_D_OP, RegistersBase::GG> {
    static constexpr const char* name = "st.d";
};

struct ADDI_W : I12RRTypeBase<Opcode10::ADDI_W_OP, RegistersBase::GG> {
    static constexpr const char* name = "addi.w";
};

struct ADDI_D : I12RRTypeBase<Opcode10::ADDI_D_OP, RegistersBase::GG> {
    static constexpr const char* name = "addi.d";
};

struct SLTI : I12RRTypeBase<Opcode10::SLTI_OP, RegistersBase::GG> {
    static constexpr const char* name = "slti";
};

struct SLTUI : I12RRTypeBase<Opcode10::SLTUI_OP, RegistersBase::GG> {
    static constexpr const char* name = "sltui";
};

struct XORI : I12RRTypeBase<Opcode10::XORI_OP, RegistersBase::GG> {
    static constexpr const char* name = "xori";
};

struct ORI : I12RRTypeBase<Opcode10::ORI_OP, RegistersBase::GG> {
    static constexpr const char* name = "ori";
};

struct LU52I_D : I12RRTypeBase<Opcode10::LU52I_D_OP, RegistersBase::GG> {
    static constexpr const char* name = "lu52i.d";
};

struct ANDI : I12RRTypeBase<Opcode10::ANDI_OP, RegistersBase::GG> {
    static constexpr const char* name = "andi";
};

struct SLLI_D : I8RRTypeBase<Opcode14::SLLI_OP, RegistersBase::GG> {
    static constexpr const char* name = "slli.d";
};

struct SRAI_D : I8RRTypeBase<Opcode14::SRAI_OP, RegistersBase::GG> {
    static constexpr const char* name = "srai.d";
};

struct SRLI_D : I8RRTypeBase<Opcode14::SRLI_OP, RegistersBase::GG> {
    static constexpr const char* name = "srli.d";
};

struct ADD_D : RRRTypeBase<Opcode17::ADD_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "add.d";
};

struct SUB_D : RRRTypeBase<Opcode17::SUB_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sub.d";
};

struct SLL_D : RRRTypeBase<Opcode17::SLL_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sll.d";
};

struct SLT : RRRTypeBase<Opcode17::SLT_OP, RegistersBase::GGG> {
    static constexpr const char* name = "slt";
};

struct SLTU : RRRTypeBase<Opcode17::SLTU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sltu";
};

struct XOR : RRRTypeBase<Opcode17::XOR_OP, RegistersBase::GGG> {
    static constexpr const char* name = "xor";
};

struct SRL_D : RRRTypeBase<Opcode17::SRL_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "srl.d";
};

struct SRA_D : RRRTypeBase<Opcode17::SRA_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sra.d";
};

struct OR : RRRTypeBase<Opcode17::OR_OP, RegistersBase::GGG> {
    static constexpr const char* name = "or";
};

struct AND : RRRTypeBase<Opcode17::AND_OP, RegistersBase::GGG> {
    static constexpr const char* name = "and";
};

struct VAND_V : RRRTypeBase<Opcode17::VAND_V_OP, RegistersBase::FFF> {
    static constexpr const char* name = "vand.v";
};

struct VOR_V : RRRTypeBase<Opcode17::VOR_V_OP, RegistersBase::FFF> {
    static constexpr const char* name = "vor.v";
};

struct BREAK : I15TypeBase<Opcode17::BREAK_OP> {
    static constexpr const char* name = "break";
};

struct DBAR : I15TypeBase<Opcode17::DBAR_OP> {
    static constexpr const char* name = "dbar";
};

struct SLLI_W : I8RRTypeBase<Opcode14::SLLI_OP, RegistersBase::GG> {
    static constexpr const char* name = "slli.w";
};

struct SRAI_W : I8RRTypeBase<Opcode14::SRAI_OP, RegistersBase::GG> {
    static constexpr const char* name = "srai.w";
};

struct SRLI_W : I8RRTypeBase<Opcode14::SRLI_OP, RegistersBase::GG> {
    static constexpr const char* name = "srli.w";
};

struct ADD_W : RRRTypeBase<Opcode17::ADD_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "add.w";
};

struct SUB_W : RRRTypeBase<Opcode17::SUB_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sub.w";
};

struct SLL_W : RRRTypeBase<Opcode17::SLL_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sll.w";
};

struct SRL_W : RRRTypeBase<Opcode17::SRL_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "srl.w";
};

struct SRA_W : RRRTypeBase<Opcode17::SRA_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "sra.w";
};

struct MUL_D : RRRTypeBase<Opcode17::MUL_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mul.d";
};

struct MULH_D : RRRTypeBase<Opcode17::MULH_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mulh.d";
};

struct MULH_WU : RRRTypeBase<Opcode17::MULH_WU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mulh.wu";
};

struct MULH_DU : RRRTypeBase<Opcode17::MULH_DU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mulh.du";
};

struct DIV_D : RRRTypeBase<Opcode17::DIV_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "div.d";
};

struct DIV_DU : RRRTypeBase<Opcode17::DIV_DU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "div.du";
};

struct MOD_D : RRRTypeBase<Opcode17::MOD_D_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mod.d";
};

struct MOD_DU : RRRTypeBase<Opcode17::MOD_DU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mod.du";
};

struct MUL_W : RRRTypeBase<Opcode17::MUL_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mul.w";
};

struct DIV_W : RRRTypeBase<Opcode17::DIV_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "div.w";
};

struct DIV_WU : RRRTypeBase<Opcode17::DIV_WU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "div.wu";
};

struct MOD_W : RRRTypeBase<Opcode17::MOD_W_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mod.w";
};

struct MOD_WU : RRRTypeBase<Opcode17::MOD_WU_OP, RegistersBase::GGG> {
    static constexpr const char* name = "mod.wu";
};

struct FADD_S : RRRTypeBase<Opcode17::FADD_S_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fadd.s";
};

struct FSUB_S : RRRTypeBase<Opcode17::FSUB_S_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fsub.s";
};

struct FMUL_S : RRRTypeBase<Opcode17::FMUL_S_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fmul.s";
};

struct FDIV_S : RRRTypeBase<Opcode17::FDIV_S_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fdiv.s";
};

struct FCOPYSIGN_S : RRRTypeBase<Opcode17::FCOPYSIGN_S_OP, RegistersBase::FFF> {
    static constexpr const char* name = "fcopysign.s";
};

struct FSQRT_S : RRTypeBase<Opcode22::FSQRT_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "fsqrt.s";
};

struct FNEG_S : RRTypeBase<Opcode22::FNEG_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "fneg.s";
};

struct FMOV_S : RRTypeBase<Opcode22::FMOV_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "fmov.s";
};

struct FCVT_D_S : RRTypeBase<Opcode22::FCVT_D_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "fcvt.d.s";
};

struct FCVT_S_D : RRTypeBase<Opcode22::FCVT_S_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "fcvt.s.d";
};

struct MOVCF2GR : RRTypeBase<Opcode22::MOVCF2GR_OP, RegistersBase::CG> {
    static constexpr const char* name = "movcf2gr";
};

struct MOVGR2FR_W : RRTypeBase<Opcode22::MOVGR2FR_W_OP, RegistersBase::FG> {
    static constexpr const char* name = "movgr2fr.w";
};

struct MOVGR2FR_D : RRTypeBase<Opcode22::MOVGR2FR_D_OP, RegistersBase::FG> {
    static constexpr const char* name = "movgr2fr.d";
};

struct FABS_S : RRTypeBase<Opcode22::FABS_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "fabs.s";
};

struct FTINTRM_W_S : RRTypeBase<Opcode22::FTINTRM_W_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrm.w.s";
};

struct FTINTRM_L_S : RRTypeBase<Opcode22::FTINTRM_L_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrm.l.s";
};

struct FTINTRP_W_S : RRTypeBase<Opcode22::FTINTRP_W_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrp.w.s";
};

struct FTINTRP_L_S : RRTypeBase<Opcode22::FTINTRP_L_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrp.l.s";
};

struct FTINTRZ_W_S : RRTypeBase<Opcode22::FTINTRZ_W_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrz.w.s";
};

struct FTINTRZ_L_S : RRTypeBase<Opcode22::FTINTRZ_L_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrz.l.s";
};

struct FTINTRNE_W_S : RRTypeBase<Opcode22::FTINTRNE_W_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrne.w.s";
};

struct FTINTRNE_L_S : RRTypeBase<Opcode22::FTINTRNE_L_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrne.l.s";
};

struct MOVFR2GR_S : RRTypeBase<Opcode22::MOVFR2GR_S_OP, RegistersBase::GF> {
    static constexpr const char* name = "movfr2gr.s";
};

struct FFINT_S_W : RRTypeBase<Opcode22::FFINT_S_W_OP, RegistersBase::FF> {
    static constexpr const char* name = "ffint.s.w";
};

struct FFINT_S_L : RRTypeBase<Opcode22::FFINT_S_L_OP, RegistersBase::FF> {
    static constexpr const char* name = "ffint.s.l";
};

struct FCLASS_S : RRTypeBase<Opcode22::FCLASS_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "fclass.s";
};

struct VFRINTRM_S : RRTypeBase<Opcode22::VFRINTRM_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrm.s";
};

struct VFRINTRP_S : RRTypeBase<Opcode22::VFRINTRP_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrp.s";
};

struct VFRINTRNE_S : RRTypeBase<Opcode22::VFRINTRNE_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrne.s";
};

struct VFRINTRZ_S : RRTypeBase<Opcode22::VFRINTRZ_S_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrz.s";
};

struct FMIN_S : RRRTypeBase<Opcode17::FMIN_S_OP, RegistersBase::FFF> {
    static constexpr const char* name = "fmin.s";
};

struct FMAX_S : RRRTypeBase<Opcode17::FMAX_S_OP, RegistersBase::FFF> {
    static constexpr const char* name = "fmax.s";
};

struct FCMP_CEQ_S : RRRRTypeBase<Opcode12::FCMP_COND_S_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.ceq.s";
};

struct FCMP_CLT_S : RRRRTypeBase<Opcode12::FCMP_COND_S_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.clt.s";
};

struct FCMP_CLE_S : RRRRTypeBase<Opcode12::FCMP_COND_S_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.cle.s";
};

struct FCMP_SLE_S : RRRRTypeBase<Opcode12::FCMP_COND_S_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.sle.s";
};

struct FLD_S : I12RRTypeBase<Opcode10::FLD_S_OP, RegistersBase::FG> {
    static constexpr const char* name = "fld.s";
};

struct FLD_D : I12RRTypeBase<Opcode10::FLD_D_OP, RegistersBase::FG> {
    static constexpr const char* name = "fld.d";
};

struct FST_S : I12RRTypeBase<Opcode10::FST_S_OP, RegistersBase::FG> {
    static constexpr const char* name = "fst.s";
};

struct FST_D : I12RRTypeBase<Opcode10::FST_D_OP, RegistersBase::FG> {
    static constexpr const char* name = "fst.d";
};

struct FADD_D : RRRTypeBase<Opcode17::FADD_D_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fadd.d";
};

struct FSUB_D : RRRTypeBase<Opcode17::FSUB_D_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fsub.d";
};

struct FMUL_D : RRRTypeBase<Opcode17::FMUL_D_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fmul.d";
};

struct FDIV_D : RRRTypeBase<Opcode17::FDIV_D_OP, RegistersBase::FFF>{
    static constexpr const char* name = "fmul.d";
};

struct FCOPYSIGN_D : RRRTypeBase<Opcode17::FCOPYSIGN_D_OP, RegistersBase::FFF> {
    static constexpr const char* name = "fcopysign.d";
};

struct FSQRT_D : RRTypeBase<Opcode22::FSQRT_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "fsqrt.d";
};

struct FNEG_D : RRTypeBase<Opcode22::FNEG_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "fneg.d";
};

struct FMOV_D : RRTypeBase<Opcode22::FMOV_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "fmov.d";
};

struct FABS_D : RRTypeBase<Opcode22::FABS_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "fabs.d";
};

struct FTINTRM_W_D : RRTypeBase<Opcode22::FTINTRM_W_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrm.w.d";
};

struct FTINTRM_L_D : RRTypeBase<Opcode22::FTINTRM_L_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrm.l.d";
};

struct FTINTRP_W_D : RRTypeBase<Opcode22::FTINTRP_W_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrp.w.d";
};

struct FTINTRP_L_D : RRTypeBase<Opcode22::FTINTRP_L_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrp.l.d";
};

struct FTINTRZ_W_D : RRTypeBase<Opcode22::FTINTRZ_W_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrz.w.d";
};

struct FTINTRZ_L_D : RRTypeBase<Opcode22::FTINTRZ_L_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrz.l.d";
};

struct FTINTRNE_W_D : RRTypeBase<Opcode22::FTINTRNE_W_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrne.w.d";
};

struct FTINTRNE_L_D : RRTypeBase<Opcode22::FTINTRNE_L_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "ftintrne.l.d";
};

struct MOVFR2GR_D : RRTypeBase<Opcode22::MOVFR2GR_D_OP, RegistersBase::GF> {
    static constexpr const char* name = "movfr2gr.d";
};

struct FFINT_D_W : RRTypeBase<Opcode22::FFINT_D_W_OP, RegistersBase::FF> {
    static constexpr const char* name = "ffint.d.w";
};

struct FFINT_D_L : RRTypeBase<Opcode22::FFINT_D_L_OP, RegistersBase::FF> {
    static constexpr const char* name = "ffint.d.l";
};

struct FCLASS_D : RRTypeBase<Opcode22::FCLASS_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "fclass.d";
};

struct VFRINTRM_D : RRTypeBase<Opcode22::VFRINTRM_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrm.d";
};

struct VFRINTRP_D : RRTypeBase<Opcode22::VFRINTRP_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrp.d";
};

struct VFRINTRNE_D : RRTypeBase<Opcode22::VFRINTRNE_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrne.d";
};

struct VFRINTRZ_D : RRTypeBase<Opcode22::VFRINTRZ_D_OP, RegistersBase::FF> {
    static constexpr const char* name = "vfrintrz.d";
};

struct FMIN_D : RRRTypeBase<Opcode17::FMIN_D_OP, RegistersBase::FFF> {
    static constexpr const char* name = "fmin.d";
};

struct FMAX_D : RRRTypeBase<Opcode17::FMAX_D_OP, RegistersBase::FFF> {
    static constexpr const char* name = "fmax.d";
};

struct FCMP_CEQ_D : RRRRTypeBase<Opcode12::FCMP_COND_D_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.ceq.d";
};

struct FCMP_CLT_D : RRRRTypeBase<Opcode12::FCMP_COND_D_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.clt.d";
};

struct FCMP_CLE_D : RRRRTypeBase<Opcode12::FCMP_COND_D_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.cle.d";
};

struct FCMP_SLE_D : RRRRTypeBase<Opcode12::FCMP_COND_D_OP, RegistersBase::CFFF> {
    static constexpr const char* name = "fcmp.sle.d";
};

} // namespace LOONGARCH64Instructions

class LOONGARCH64Assembler {
public:
    using RegisterID = LOONGARCH64Registers::RegisterID;
    using SPRegisterID = LOONGARCH64Registers::SPRegisterID;
    using FPRegisterID = LOONGARCH64Registers::FPRegisterID;
    using CFRegisterID = LOONGARCH64Registers::CFRegisterID;

    static constexpr RegisterID firstRegister() { return LOONGARCH64Registers::r0; }
    static constexpr RegisterID lastRegister() { return LOONGARCH64Registers::r31; }
    static constexpr unsigned numberOfRegisters() { return lastRegister() - firstRegister() + 1; }

    static constexpr SPRegisterID firstSPRegister() { return LOONGARCH64Registers::pc; }
    static constexpr SPRegisterID lastSPRegister() { return LOONGARCH64Registers::pc; }
    static constexpr unsigned numberOfSPRegisters() { return lastSPRegister() - firstSPRegister() + 1; }

    static constexpr FPRegisterID firstFPRegister() { return LOONGARCH64Registers::f0; }
    static constexpr FPRegisterID lastFPRegister() { return LOONGARCH64Registers::f31; }
    static constexpr unsigned numberOfFPRegisters() { return lastFPRegister() - firstFPRegister() + 1; }

    static constexpr CFRegisterID firstCFRegister() { return LOONGARCH64Registers::fcc0; }
    static constexpr CFRegisterID lastCFRegister() { return LOONGARCH64Registers::fcc7; }
    static constexpr unsigned numberOfCFRegisters() { return lastCFRegister() - firstCFRegister() + 1; }

    static ASCIILiteral gprName(RegisterID id)
    {
        ASSERT(id >= firstRegister() && id <= lastRegister());
        static constexpr ASCIILiteral nameForRegister[numberOfRegisters()] = {
#define REGISTER_NAME(id, name, r, cs) name,
            FOR_EACH_GP_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    static ASCIILiteral sprName(SPRegisterID id)
    {
        ASSERT(id >= firstSPRegister() && id <= lastSPRegister());
        static constexpr ASCIILiteral nameForRegister[numberOfSPRegisters()] = {
#define REGISTER_NAME(id, name) name,
            FOR_EACH_SP_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    static ASCIILiteral fprName(FPRegisterID id)
    {
        ASSERT(id >= firstFPRegister() && id <= lastFPRegister());
        static constexpr ASCIILiteral nameForRegister[numberOfFPRegisters()] = {
#define REGISTER_NAME(id, name, r, cs) name,
            FOR_EACH_FP_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    static ASCIILiteral cfrName(CFRegisterID id)
    {
        ASSERT(id >= firstCFRegister() && id <= lastCFRegister());
        static constexpr ASCIILiteral nameForRegister[numberOfCFRegisters()] = {
#define REGISTER_NAME(id, name, r, cs) name,
            FOR_EACH_CF_REGISTER(REGISTER_NAME)
#undef REGISTER_NAME
        };
        return nameForRegister[id];
    }

    LOONGARCH64Assembler() { }

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
        location[0] = LOONGARCH64Instructions::ST_D::construct(LOONGARCH64Registers::zero,
                                                               LOONGARCH64Registers::zero,
                                                               I12Immediate::v<I12Immediate, 0>());
        cacheFlush(location, sizeof(uint32_t));
    }

    static void splitSimm38(int64_t si38, int32_t& si18, int32_t& si20) {
        ASSERT_WITH_MESSAGE(ImmediateBase<38>::isSImm<38>(si38), "not signed 38-bit offset");
        si18 = ((int)(si38 & 0x3ffff) << 14) >> 14;
        si38 += (si38 & 0x20000) << 1;
        si20 = si38 >> 18;
    }

    static void replaceWithJump(void* from, void* to)
    {
        uint32_t* location = reinterpret_cast<uint32_t*>(from);
        intptr_t offset = uintptr_t(to) - uintptr_t(from);

        if (I26Immediate::isSImm<26>(offset >> 2)) {
            location[0] = LOONGARCH64Instructions::B::construct(I26Immediate::v<I26Immediate>(offset >> 2));
            cacheFlush(from, sizeof(uint32_t));
            return;
        }

        int32_t si18, si20;
        splitSimm38(offset, si18, si20);

        location[0] = LOONGARCH64Instructions::PCADDU18I::construct(LOONGARCH64Registers::r19,
                                                                    I20Immediate::v<I20Immediate>(si20 >> 18));
        location[1] = LOONGARCH64Instructions::JIRL::construct(LOONGARCH64Registers::r19,
                                                               LOONGARCH64Registers::zero,
                                                               I16Immediate(si18 >> 2));
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
        PatchPointerImpl::apply(location, LOONGARCH64Registers::r20, valuePtr);
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

        uint32_t nop = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                LOONGARCH64Registers::zero,
                                                                I12Immediate::v<I12Immediate, 0>());
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

    template<unsigned immediateSize> using ImmediateBase = LOONGARCH64Instructions::ImmediateBase<immediateSize>;
    using I8Immediate  = LOONGARCH64Instructions::I8Immediate;
    using I12Immediate = LOONGARCH64Instructions::I12Immediate;
    using I14Immediate = LOONGARCH64Instructions::I14Immediate;
    using I15Immediate = LOONGARCH64Instructions::I15Immediate;
    using I16Immediate = LOONGARCH64Instructions::I16Immediate;
    using I20Immediate = LOONGARCH64Instructions::I20Immediate;
    using I21Immediate = LOONGARCH64Instructions::I21Immediate;
    using I26Immediate = LOONGARCH64Instructions::I26Immediate;

#define TEST_INSN(expected, mc) \
    if (expected != 0) { \
        ASSERT_WITH_MESSAGE(expected == mc, "expected: 0x%x mc: 0x%x", expected, mc); \
        return; \
    } \

    void lu32i_dInsn(RegisterID rd, I20Immediate imm, uint32_t expected = 0)
    {
        uint32_t mc = LOONGARCH64Instructions::LU32I_D::construct(rd, imm);
#ifdef NDEBUG
        UNUSED_PARAM(expected);
#else
        TEST_INSN(expected, mc);
#endif
        insn(mc);
    }
    void lu12i_wInsn(RegisterID rd, I20Immediate imm, uint32_t expected = 0)
    {
        uint32_t mc = LOONGARCH64Instructions::LU12I_W::construct(rd, imm);
#ifdef NDEBUG
        UNUSED_PARAM(expected);
#else
        TEST_INSN(expected, mc);
#endif
        insn(mc);
    }
    void pcaddu18iInsn(RegisterID rd, int32_t imm, uint32_t expected = 0)
    {
        ASSERT_WITH_MESSAGE(I20Immediate::isSImm<20>(imm >> 18), "not a signed 20-bit int");
        uint32_t mc = LOONGARCH64Instructions::PCADDU18I::construct(rd, I20Immediate::v<I20Immediate>(imm >> 18));
#ifdef NDEBUG
        UNUSED_PARAM(expected);
#else
        TEST_INSN(expected, mc);
#endif
        insn(mc);
    }
    void bInsn(int32_t imm) { insn(LOONGARCH64Instructions::B::construct(I26Immediate(imm >> 2))); }
    void blInsn(int32_t imm) { insn(LOONGARCH64Instructions::BL::construct(I26Immediate(imm >> 2))); }
    void jirlInsn(RegisterID rd, RegisterID rj, int16_t imm) { insn(LOONGARCH64Instructions::JIRL::construct(rj, rd, I16Immediate(imm >> 2))); }
    void beqInsn(RegisterID rj, RegisterID rd, int16_t imm) { insn(LOONGARCH64Instructions::BEQ::construct(rj, rd, I16Immediate(imm >> 2))); }
    void bneInsn(RegisterID rj, RegisterID rd, int16_t imm) { insn(LOONGARCH64Instructions::BNE::construct(rj, rd, I16Immediate(imm >> 2))); }
    void bltInsn(RegisterID rj, RegisterID rd, int16_t imm) { insn(LOONGARCH64Instructions::BLT::construct(rj, rd, I16Immediate(imm >> 2))); }
    void bgeInsn(RegisterID rj, RegisterID rd, int16_t imm) { insn(LOONGARCH64Instructions::BGE::construct(rj, rd, I16Immediate(imm >> 2))); }
    void bltuInsn(RegisterID rj, RegisterID rd, int16_t imm) { insn(LOONGARCH64Instructions::BLTU::construct(rj, rd, I16Immediate(imm >> 2))); }
    void bgeuInsn(RegisterID rj, RegisterID rd, int16_t imm) { insn(LOONGARCH64Instructions::BGEU::construct(rj, rd, I16Immediate(imm >> 2))); }
    void bcnezInsn(CFRegisterID cj, int32_t imm, uint32_t expected = 0)
    {
        ASSERT_WITH_MESSAGE(I21Immediate::isSImm<21>(imm >> 2), "not a signed 21-bit int");
        CFRegisterID rj = static_cast<CFRegisterID>(0b01000 | static_cast<int>(cj));
        uint32_t mc = LOONGARCH64Instructions::BCNEZ::construct(rj, I21Immediate::v<I21Immediate>(imm >> 2));
#ifdef NDEBUG
        UNUSED_PARAM(expected);
#else
        TEST_INSN(expected, mc);
#endif
        insn(mc);
    }
    void ld_bInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LD_B::construct(rj, rd, imm)); }
    void ld_hInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LD_H::construct(rj, rd, imm)); }
    void ld_wInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LD_W::construct(rj, rd, imm)); }
    void ld_dInsn(RegisterID rd, RegisterID rj, I12Immediate imm, uint32_t expected = 0)
    {
        uint32_t mc = LOONGARCH64Instructions::LD_D::construct(rj, rd, imm);
#ifdef NDEBUG
        UNUSED_PARAM(expected);
#else
        TEST_INSN(expected, mc);
#endif
        insn(mc);
    }
    void ld_buInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LD_BU::construct(rj, rd, imm)); }
    void ld_huInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LD_HU::construct(rj, rd, imm)); }
    void ld_wuInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LD_WU::construct(rj, rd, imm)); }
    void st_bInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ST_B::construct(rj, rd, imm)); }
    void st_hInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ST_H::construct(rj, rd, imm)); }
    void st_wInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ST_W::construct(rj, rd, imm)); }
    void st_dInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ST_D::construct(rj, rd, imm)); }
    void addi_dInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ADDI_D::construct(rj, rd, imm)); }
    void sltiInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::SLTI::construct(rj, rd, imm)); }
    void sltuiInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::SLTUI::construct(rj, rd, imm)); }
    void xoriInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::XORI::construct(rj, rd, imm)); }
    void oriInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ORI::construct(rj, rd, imm)); }
    void lu52i_dInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::LU52I_D::construct(rj, rd, imm)); }
    void andiInsn(RegisterID rd, RegisterID rj, I12Immediate imm, uint32_t expected = 0)
    {
        uint32_t mc = LOONGARCH64Instructions::ANDI::construct(rj, rd, imm);
#ifdef NDEBUG
        UNUSED_PARAM(expected);
#else
        TEST_INSN(expected, mc);
#endif
        insn(mc);
    }
    void slli_dInsn(RegisterID rd, RegisterID rj, int32_t shamt)
    {
        ASSERT_WITH_MESSAGE(I8Immediate::isUImm<6>(shamt), "not a unsigned 6-bit int");
        insn(LOONGARCH64Instructions::SLLI_D::construct(rj, rd, I8Immediate((0b01 << 6) | shamt)));
    }
    void srli_dInsn(RegisterID rd, RegisterID rj, int32_t shamt)
    {
        ASSERT_WITH_MESSAGE(I8Immediate::isUImm<6>(shamt), "not a unsigned 6-bit int");
        insn(LOONGARCH64Instructions::SRLI_D::construct(rj, rd, I8Immediate((0b01 << 6) | shamt)));
    }
    void srai_dInsn(RegisterID rd, RegisterID rj, int32_t shamt)
    {
        ASSERT_WITH_MESSAGE(I8Immediate::isUImm<6>(shamt), "not a unsigned 6-bit int");
        insn(LOONGARCH64Instructions::SRAI_D::construct(rj, rd, I8Immediate((0b01 << 6) | shamt)));
    }
    void add_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::ADD_D::construct(rk, rj, rd)); }
    void sub_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SUB_D::construct(rk, rj, rd)); }
    void sll_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SLL_D::construct(rk, rj, rd)); }
    void sltInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SLT::construct(rk, rj, rd)); }
    void sltuInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SLTU::construct(rk, rj, rd)); }
    void xorInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::XOR::construct(rk, rj, rd)); }
    void srl_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SRL_D::construct(rk, rj, rd)); }
    void sra_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SRA_D::construct(rk, rj, rd)); }
    void orInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::OR::construct(rk, rj, rd)); }
    void andInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::AND::construct(rk, rj, rd)); }
    void breakInsn(uint16_t imm) { insn(LOONGARCH64Instructions::BREAK::construct(I15Immediate(imm))); }
    void dbarInsn(uint16_t imm) { insn(LOONGARCH64Instructions::DBAR::construct(I15Immediate(imm))); }
    void addi_wInsn(RegisterID rd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::ADDI_W::construct(rj, rd, imm)); }
    void slli_wInsn(RegisterID rd, RegisterID rj, int32_t shamt)
    {
        ASSERT_WITH_MESSAGE(I8Immediate::isUImm<5>(shamt), "not a unsigned 5-bit int");
        insn(LOONGARCH64Instructions::SLLI_W::construct(rj, rd, I8Immediate((0b001 << 5) | shamt)));
    }
    void srli_wInsn(RegisterID rd, RegisterID rj, int32_t shamt)
    {
        ASSERT_WITH_MESSAGE(I8Immediate::isUImm<5>(shamt), "not a unsigned 5-bit int");
        insn(LOONGARCH64Instructions::SRLI_W::construct(rj, rd, I8Immediate((0b001 << 5) | shamt)));
    }
    void srai_wInsn(RegisterID rd, RegisterID rj, int32_t shamt)
    {
        ASSERT_WITH_MESSAGE(I8Immediate::isUImm<5>(shamt), "not a unsigned 5-bit int");
        insn(LOONGARCH64Instructions::SRAI_W::construct(rj, rd, I8Immediate((0b001 << 5) | shamt)));
    }
    void add_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::ADD_W::construct(rk, rj, rd)); }
    void sub_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SUB_W::construct(rk, rj, rd)); }
    void sll_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SLL_W::construct(rk, rj, rd)); }
    void srl_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SRL_W::construct(rk, rj, rd)); }
    void sra_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::SRA_W::construct(rk, rj, rd)); }

    void mul_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MUL_D::construct(rk, rj, rd)); }
    void mulh_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MULH_D::construct(rk, rj, rd)); }
    void mulh_wuInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MULH_WU::construct(rk, rj, rd)); }
    void mulh_duInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MULH_DU::construct(rk, rj, rd)); }

    void div_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::DIV_D::construct(rk, rj, rd)); }
    void div_duInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::DIV_DU::construct(rk, rj, rd)); }
    void mod_dInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MOD_D::construct(rk, rj, rd)); }
    void mod_duInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MOD_DU::construct(rk, rj, rd)); }

    void mul_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MUL_W::construct(rk, rj, rd)); }
    void div_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::DIV_W::construct(rk, rj, rd)); }
    void div_wuInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::DIV_WU::construct(rk, rj, rd)); }
    void mod_wInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MOD_W::construct(rk, rj, rd)); }
    void mod_wuInsn(RegisterID rd, RegisterID rj, RegisterID rk) { insn(LOONGARCH64Instructions::MOD_WU::construct(rk, rj, rd)); }

    void movcf2grInsn(RegisterID rd, CFRegisterID cj) { insn(LOONGARCH64Instructions::MOVCF2GR::construct(cj, rd)); }

    void fld_sInsn(FPRegisterID fd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::FLD_S::construct(rj, fd, imm)); }
    void fld_dInsn(FPRegisterID fd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::FLD_D::construct(rj, fd, imm)); }
    void fst_sInsn(FPRegisterID fd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::FST_S::construct(rj, fd, imm)); }
    void fst_dInsn(FPRegisterID fd, RegisterID rj, I12Immediate imm) { insn(LOONGARCH64Instructions::FST_D::construct(rj, fd, imm)); }

    void movgr2fr_wInsn(FPRegisterID fd, RegisterID rj) { insn(LOONGARCH64Instructions::MOVGR2FR_W::construct(rj, fd)); }
    void movgr2fr_dInsn(FPRegisterID fd, RegisterID rj) { insn(LOONGARCH64Instructions::MOVGR2FR_D::construct(rj, fd)); }

    void fcvt_d_sInsn(FPRegisterID fd, FPRegisterID fj) { insn(LOONGARCH64Instructions::FCVT_D_S::construct(fj, fd)); }
    void fcvt_s_dInsn(FPRegisterID fd, FPRegisterID fj) { insn(LOONGARCH64Instructions::FCVT_S_D::construct(fj, fd)); }

    template<unsigned fpSize>
    void faddInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FADD_S, LOONGARCH64Instructions::FADD_D>(fk, fj, fd);
    }

    template<unsigned fpSize>
    void fsubInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FSUB_S, LOONGARCH64Instructions::FSUB_D>(fk, fj, fd);
    }

    template<unsigned fpSize>
    void fmulInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FMUL_S, LOONGARCH64Instructions::FMUL_D>(fk, fj, fd);
    }

    template<unsigned fpSize>
    void fdivInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FDIV_S, LOONGARCH64Instructions::FDIV_D>(fk, fj, fd);
    }

    template<unsigned fpSize>
    void fsqrtInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FSQRT_S, LOONGARCH64Instructions::FSQRT_D>(fj, fd);
    }

    template<unsigned fpSize>
    void fnegInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FNEG_S, LOONGARCH64Instructions::FNEG_D>(fj, fd);
    }

    template<unsigned fpSize>
    void fmovInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FMOV_S, LOONGARCH64Instructions::FMOV_D>(fj, fd);
    }

    template<unsigned fpSize>
    void fabsInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FABS_S, LOONGARCH64Instructions::FABS_D>(fj, fd);
    }

    template<unsigned fpSize>
    void fclassInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FCLASS_S, LOONGARCH64Instructions::FCLASS_D>(fj, fd);
    }

    using FPRoundingMode = LOONGARCH64Instructions::FPRoundingMode;

    template<unsigned fpSize>
    void ftintrm_wInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRM_W_S, LOONGARCH64Instructions::FTINTRM_W_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrm_lInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRM_L_S, LOONGARCH64Instructions::FTINTRM_L_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrp_wInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRP_W_S, LOONGARCH64Instructions::FTINTRP_W_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrp_lInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRP_L_S, LOONGARCH64Instructions::FTINTRP_L_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrz_wInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRZ_W_S, LOONGARCH64Instructions::FTINTRZ_W_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrz_lInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRZ_L_S, LOONGARCH64Instructions::FTINTRZ_L_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrne_wInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRNE_W_S, LOONGARCH64Instructions::FTINTRNE_W_D>(fj, fd);
    }

    template<unsigned fpSize>
    void ftintrne_lInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FTINTRNE_L_S, LOONGARCH64Instructions::FTINTRNE_L_D>(fj, fd);
    }

    template<unsigned fpSize>
    void movfr2grInsn(RegisterID rd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::MOVFR2GR_S, LOONGARCH64Instructions::MOVFR2GR_D>(fj, rd);
    }

    template<unsigned fpSize>
    void ffint_wInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FFINT_S_W, LOONGARCH64Instructions::FFINT_D_W>(fj, fd);
    }

    template<unsigned fpSize>
    void ffint_lInsn(FPRegisterID fd, FPRegisterID fj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FFINT_S_L, LOONGARCH64Instructions::FFINT_D_L>(fj, fd);
    }

    template<unsigned fpSize>
    void fcopysignInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FCOPYSIGN_S, LOONGARCH64Instructions::FCOPYSIGN_D>(fk, fj, fd);
    }

    template<unsigned fpSize>
    void fminInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FMIN_S, LOONGARCH64Instructions::FMIN_D>(fk, fj, fd);
    }

    template<unsigned fpSize>
    void fmaxInsn(FPRegisterID fd, FPRegisterID fj, FPRegisterID fk)
    {
        insnFP<fpSize, LOONGARCH64Instructions::FMAX_S, LOONGARCH64Instructions::FMAX_D>(fk, fj, fd);
    }

    using FCmpCond = LOONGARCH64Instructions::FCmpCond;

    template<unsigned fpSize>
    void fcmp_ceqInsn(CFRegisterID cd, FPRegisterID fj, FPRegisterID fk)
    {
        FPRegisterID fa = static_cast<FPRegisterID>(FCmpCond::FCMP_CEQ);
        insnFP<fpSize, LOONGARCH64Instructions::FCMP_CEQ_S, LOONGARCH64Instructions::FCMP_CEQ_D>(fa, fk, fj, cd);
    }

    template<unsigned fpSize>
    void fcmp_cltInsn(CFRegisterID cd, FPRegisterID fj, FPRegisterID fk)
    {
        FPRegisterID fa = static_cast<FPRegisterID>(FCmpCond::FCMP_CLT);
        insnFP<fpSize, LOONGARCH64Instructions::FCMP_CLT_S, LOONGARCH64Instructions::FCMP_CLT_D>(fa, fk, fj, cd);
    }

    template<unsigned fpSize>
    void fcmp_cleInsn(CFRegisterID cd, FPRegisterID fj, FPRegisterID fk)
    {
        FPRegisterID fa = static_cast<FPRegisterID>(FCmpCond::FCMP_CLE);
        insnFP<fpSize, LOONGARCH64Instructions::FCMP_CLE_S, LOONGARCH64Instructions::FCMP_CLE_D>(fa, fk, fj, cd);
    }

    template<unsigned fpSize>
    void fcmp_sleInsn(CFRegisterID cd, FPRegisterID fj, FPRegisterID fk)
    {
        FPRegisterID fa = static_cast<FPRegisterID>(FCmpCond::FCMP_SLE);
        insnFP<fpSize, LOONGARCH64Instructions::FCMP_SLE_S, LOONGARCH64Instructions::FCMP_SLE_D>(fa, fk, fj, cd);
    }

    void fenceInsn()
    {
        insn(LOONGARCH64Instructions::DBAR::construct(I15Immediate(0)));
    }

    void ll_wInsn(RegisterID rd, RegisterID rj, int32_t imm)
    {
        insn(LOONGARCH64Instructions::LL_W::construct(rj, rd, I14Immediate(imm >> 2)));
    }

    void sc_wInsn(RegisterID rd, RegisterID rj, int32_t imm)
    {
        insn(LOONGARCH64Instructions::SC_W::construct(rj, rd, I14Immediate(imm >> 2)));
    }

    void ll_dInsn(RegisterID rd, RegisterID rj, int32_t imm)
    {
        insn(LOONGARCH64Instructions::LL_D::construct(rj, rd, I14Immediate(imm >> 2)));
    }

    void sc_dInsn(RegisterID rd, RegisterID rj, int32_t imm)
    {
        insn(LOONGARCH64Instructions::SC_D::construct(rj, rd, I14Immediate(imm >> 2)));
    }

    void vand_vInsn(FPRegisterID vd, FPRegisterID vj, FPRegisterID vk) { insn(LOONGARCH64Instructions::VAND_V::construct(vk, vj, vd)); }

    void vor_vInsn(FPRegisterID vd, FPRegisterID vj, FPRegisterID vk) { insn(LOONGARCH64Instructions::VOR_V::construct(vk, vj, vd)); }

    template<unsigned fpSize>
    void vfrintrmInsn(FPRegisterID vd, FPRegisterID vj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::VFRINTRM_S, LOONGARCH64Instructions::VFRINTRM_D>(vj, vd);
    }

    template<unsigned fpSize>
    void vfrintrpInsn(FPRegisterID vd, FPRegisterID vj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::VFRINTRP_S, LOONGARCH64Instructions::VFRINTRP_D>(vj, vd);
    }

    template<unsigned fpSize>
    void vfrintrneInsn(FPRegisterID vd, FPRegisterID vj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::VFRINTRNE_S, LOONGARCH64Instructions::VFRINTRNE_D>(vj, vd);
    }

    template<unsigned fpSize>
    void vfrintrzInsn(FPRegisterID vd, FPRegisterID vj)
    {
        insnFP<fpSize, LOONGARCH64Instructions::VFRINTRZ_S, LOONGARCH64Instructions::VFRINTRZ_D>(vj, vd);
    }

    void nop()
    {
        andiInsn(LOONGARCH64Registers::zero, LOONGARCH64Registers::zero, I12Immediate::v<I12Immediate, 0>());
    }

    template<unsigned maskSize>
    void maskRegister(RegisterID rd, RegisterID rs)
    {
        static_assert(maskSize < 64);
        slli_dInsn(rd, rs, 64 - maskSize);
        srli_dInsn(rd, rd, 64 - maskSize);
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
            addi_wInsn(rd, rs, I12Immediate::v<I12Immediate, 0>());
            return;
        }

        slli_dInsn(rd, rs, 64 - bitSize);
        srai_dInsn(rd, rd, 64 - bitSize);
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

        slli_dInsn(rd, rs, 64 - bitSize);
        srli_dInsn(rd, rd, 64 - bitSize);
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

    template<unsigned nBitsize>
    static int SImm(int x)
    {
        return (x << (32 - nBitsize)) >> (32 - nBitsize);
    }

    template<unsigned fieldStart, unsigned fieldSize>
    static intptr_t bitField(intptr_t x)
    {
        return (x >> fieldStart) & ((fieldSize >= 64 ? 0 : (intptr_t(1) << fieldSize)) - 1);
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
            int64_t value = imm;
            int64_t hi12 = bitField<52, 12>(value);
            int64_t lo52 = bitField<0, 52>(value);

            if (hi12 != 0 && lo52 == 0) {
                m_ops[m_opCount++] = { Op::Type::LU52I_D, true /* rj is zero */, hi12 };
            } else {
                int64_t hi20 = bitField<32, 20>(value);
                int64_t lo20 = bitField<12, 20>(value);
                int64_t lo12 = bitField<0, 12>(value);

                if (lo20 == 0) {
                    m_ops[m_opCount++] = { Op::Type::ORI, true /* rj is zero */, lo12 };
                } else if (bitField<12, 20>(SImm<12>(lo12)) == lo20) {
                    m_ops[m_opCount++] = { Op::Type::ADDI_W, true /* rj is zero */, SImm<12>(lo12) };
                } else {
                    m_ops[m_opCount++] = { Op::Type::LU12I_W, true /* no rj */, lo20 };
                    if (lo12 != 0) {
                        m_ops[m_opCount++] = { Op::Type::ORI, false /* rj is not zero same as rd */, lo12 };
                    }
                }
                if (hi20 != bitField<20, 20>(SImm<20>(lo20))) {
                    m_ops[m_opCount++] = { Op::Type::LU32I_D, true /* no rj */, hi20 };
                }
                if (hi12 != bitField<20, 12>(SImm<20>(hi20))) {
                    m_ops[m_opCount++] = { Op::Type::LU52I_D, false /* rj is not zero same as rd */, hi12 };
                }
            }
        }

        ImmediateLoader(PlaceholderTag, int64_t imm)
            : ImmediateLoader(imm)
        {
            // The non-placeholder constructor already generated the necessary operations to load this immediate.
            // This constructor still fills out the remaining potential operations as nops. This enables future patching
            // of these instructions with other immediate-load sequences.

            for (unsigned i = m_opCount; i < m_ops.size(); ++i)
                m_ops[i] = { Op::Type::NOP, true, 0 };
            m_opCount = m_ops.size();
        }

        void moveInto(LOONGARCH64Assembler& assembler, RegisterID dest)
        {
            // This is a helper method that generates the necessary instructions through the LOONGARCH64Assembler infrastructure.

            for (unsigned i = 0; i < m_opCount; ++i) {
                auto& op = m_ops[i];
                switch (op.type) {
                case Op::Type::LU52I_D:
                    assembler.lu52i_dInsn(dest,
                                          op.isZero ? LOONGARCH64Registers::zero : dest,
                                          I12Immediate::v<I12Immediate>(SImm<12>(op.value)));
                    break;
                case Op::Type::LU32I_D:
                    assembler.lu32i_dInsn(dest, I20Immediate::v<I20Immediate>(SImm<20>(op.value)));
                    break;
                case Op::Type::LU12I_W:
                    assembler.lu12i_wInsn(dest, I20Immediate::v<I20Immediate>(SImm<20>(op.value)));
                    break;
                case Op::Type::ADDI_W:
                    assembler.addi_wInsn(dest,
                                         op.isZero ? LOONGARCH64Registers::zero : dest,
                                         I12Immediate::v<I12Immediate>(op.value));
                    break;
                case Op::Type::ORI:
                    assembler.oriInsn(dest,
                                      op.isZero ? LOONGARCH64Registers::zero : dest,
                                      I12Immediate(op.value));
                    break;
                case Op::Type::NOP:
                    assembler.andiInsn(LOONGARCH64Registers::zero,
                                       LOONGARCH64Registers::zero,
                                       I12Immediate::v<I12Immediate, 0>());
                    break;
                }
            }
        }

        struct Op {
            enum class Type {
                LU52I_D,
                LU32I_D,
                LU12I_W,
                ADDI_W,
                ORI,
                NOP,
            };

            Type type;
            bool isZero; // RJ register is zero
            int64_t value;
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

    struct LinkJumpOrCallImpl {
        static void apply(uint32_t* location, void* target)
        {
            LOONGARCH64Instructions::InstructionValue instruction(location[1]);
            auto destination = RegisterID(instruction.field<0, 5>());

            intptr_t offset = uintptr_t(target) - uintptr_t(&location[1]);
            if (I26Immediate::isSImm<26>(offset >> 2)) {
                location[0] = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                       LOONGARCH64Registers::zero,
                                                                       I12Immediate::v<I12Immediate, 0>());
                switch (LOONGARCH64Instructions::Opcode6(instruction.field<26, 6>())) {
                case LOONGARCH64Instructions::Opcode6::B_OP:
                    location[1] = LOONGARCH64Instructions::B::construct(I26Immediate::v<I26Immediate>(offset >> 2));
                    break;
                case LOONGARCH64Instructions::Opcode6::BL_OP:
                    location[1] = LOONGARCH64Instructions::BL::construct(I26Immediate::v<I26Immediate>(offset >> 2));
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    location[1] = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                           LOONGARCH64Registers::zero,
                                                                           I12Immediate::v<I12Immediate, 0>());
                    break;
                }
                return;
            }

            offset += sizeof(uint32_t);
            int32_t si18, si20;
            splitSimm38(offset, si18, si20);

            location[0] = LOONGARCH64Instructions::PCADDU18I::construct(LOONGARCH64Registers::r19,
                                                                        I20Immediate::v<I20Immediate>(si20 >> 18));
            location[1] = LOONGARCH64Instructions::JIRL::construct(LOONGARCH64Registers::r19,
                                                                   destination,
                                                                   I16Immediate(si18 >> 2));
        }
    };

    struct LinkJumpImpl : LinkJumpOrCallImpl {
        static constexpr unsigned PlaceholderValue = 1;
        static uint32_t placeholderInsn()
        {
            return LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                            LOONGARCH64Registers::zero,
                                                            I12Immediate::v<I12Immediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(LOONGARCH64Assembler& assembler, const F& functor)
        {
            assembler.insn(placeholderInsn());
            functor();
        }
    };

    struct LinkCallImpl : LinkJumpOrCallImpl {
        static constexpr unsigned PlaceholderValue = 2;
        static uint32_t placeholderInsn()
        {
            return LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                            LOONGARCH64Registers::zero,
                                                            I12Immediate::v<I12Immediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(LOONGARCH64Assembler& assembler, const F& functor)
        {
            assembler.insn(placeholderInsn());
            functor();
        }
    };

    struct LinkBranchImpl {
        static constexpr unsigned PlaceholderValue = 3;
        static uint32_t placeholderInsn()
        {
            return LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                            LOONGARCH64Registers::zero,
                                                            I12Immediate::v<I12Immediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(LOONGARCH64Assembler& assembler, const F& functor)
        {
            auto insnValue = placeholderInsn();
            for (unsigned i = 0; i < 2; ++i)
                assembler.insn(insnValue);
            functor();
        }

        static void apply(uint32_t* location, void* target)
        {
            LOONGARCH64Instructions::InstructionValue instruction(location[2]);

            auto branchInstructionForOpcode6 =
                [](LOONGARCH64Instructions::Opcode6 op, RegisterID rj, RegisterID rd, I16Immediate imm)
                {
                    switch (op) {
                    case LOONGARCH64Instructions::Opcode6::BEQ_OP:
                        return LOONGARCH64Instructions::BEQ::construct(rj, rd, imm);
                    case LOONGARCH64Instructions::Opcode6::BNE_OP:
                        return LOONGARCH64Instructions::BNE::construct(rj, rd, imm);
                    case LOONGARCH64Instructions::Opcode6::BLT_OP:
                        return LOONGARCH64Instructions::BLT::construct(rj, rd, imm);
                    case LOONGARCH64Instructions::Opcode6::BGE_OP:
                        return LOONGARCH64Instructions::BGE::construct(rj, rd, imm);
                    case LOONGARCH64Instructions::Opcode6::BLTU_OP:
                        return LOONGARCH64Instructions::BLTU::construct(rj, rd, imm);
                    case LOONGARCH64Instructions::Opcode6::BGEU_OP:
                        return LOONGARCH64Instructions::BGEU::construct(rj, rd, imm);
                    default:
                        break;
                    }

                    RELEASE_ASSERT_NOT_REACHED();
                    return LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                    LOONGARCH64Registers::zero,
                                                                    I12Immediate::v<I12Immediate, 0>());
                };
            auto rj = RegisterID(instruction.field<5, 5>());
            auto rd = RegisterID(instruction.field<0, 5>());

            intptr_t offset = uintptr_t(target) - uintptr_t(&location[2]);
            if (I16Immediate::isSImm<16>(offset >> 2)) {
                location[0] = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                       LOONGARCH64Registers::zero,
                                                                       I12Immediate::v<I12Immediate, 0>());
                location[1] = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                       LOONGARCH64Registers::zero,
                                                                       I12Immediate::v<I12Immediate, 0>());
                location[2] = branchInstructionForOpcode6(LOONGARCH64Instructions::Opcode6(instruction.field<26, 6>()),
                                                          rj,
                                                          rd,
                                                          I16Immediate::v<I16Immediate>(offset >> 2));
                return;
            }

            if (I26Immediate::isSImm<26>(offset >> 2)) {
                location[0] = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                       LOONGARCH64Registers::zero,
                                                                       I12Immediate::v<I12Immediate, 0>());
                location[1] = branchInstructionForOpcode6(LOONGARCH64Instructions::Opcode6(instruction.field<26, 6>() ^ 0b000001),
                                                          rj,
                                                          rd,
                                                          I16Immediate::v<I16Immediate>(8 >> 2));
                location[2] = LOONGARCH64Instructions::B::construct(I26Immediate::v<I26Immediate>(offset >> 2));
                return;
            }

            offset += sizeof(uint32_t);

            location[0] = branchInstructionForOpcode6(LOONGARCH64Instructions::Opcode6(instruction.field<26, 6>() ^ 0b000001),
                                                      rj,
                                                      rd,
                                                      I16Immediate::v<I16Immediate>(12 >> 2));

            int32_t si18, si20;
            splitSimm38(offset, si18, si20);

            location[1] = LOONGARCH64Instructions::PCADDU18I::construct(LOONGARCH64Registers::r19,
                                                                        I20Immediate::v<I20Immediate>(si20 >> 18));
            location[2] = LOONGARCH64Instructions::JIRL::construct(LOONGARCH64Registers::r19,
                                                                   LOONGARCH64Registers::zero,
                                                                   I16Immediate(si18 >> 2));
        }
    };

    struct PatchPointerImpl {
        static constexpr unsigned PlaceholderValue = 4;
        static uint32_t placeholderInsn()
        {
            return LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                            LOONGARCH64Registers::zero,
                                                            I12Immediate::v<I12Immediate, PlaceholderValue>());
        }

        template<typename F>
        static void generatePlaceholder(LOONGARCH64Assembler& assembler, const F& functor)
        {
            auto insnValue = placeholderInsn();
            for (unsigned i = 0; i < 7; ++i)
                assembler.insn(insnValue);
            functor();
        }

        static void apply(uint32_t* location, void* value)
        {
            LOONGARCH64Instructions::InstructionValue instruction(location[7]);
            RegisterID destination = RegisterID(instruction.field<0, 5>());
            if (destination == LOONGARCH64Registers::zero) {
                LOONGARCH64Instructions::InstructionValue instruction(location[0]);
                destination = RegisterID(instruction.field<0, 5>());
            }
            apply(location, destination, value);
        }

        static void apply(uint32_t* location, RegisterID destination, void* value)
        {
            using ImmediateLoader = LOONGARCH64Assembler::ImmediateLoader;
            ImmediateLoader imml(ImmediateLoader::Placeholder, reinterpret_cast<intptr_t>(value));
            RELEASE_ASSERT(imml.m_opCount == 8);

            for (unsigned i = 0; i < imml.m_opCount; ++i) {
                auto& op = imml.m_ops[i];
                switch (op.type) {
                case ImmediateLoader::Op::Type::LU52I_D:
                    location[i] = LOONGARCH64Instructions::LU52I_D::construct(op.isZero ? LOONGARCH64Registers::zero : destination,
                                                                              destination,
                                                                              I12Immediate::v<I12Immediate>(SImm<12>(op.value)));
                    break;
                case ImmediateLoader::Op::Type::LU32I_D:
                    location[i] = LOONGARCH64Instructions::LU32I_D::construct(destination,
                                                                              I20Immediate::v<I20Immediate>(SImm<20>(op.value)));
                    break;
                case ImmediateLoader::Op::Type::LU12I_W:
                    location[i] = LOONGARCH64Instructions::LU12I_W::construct(destination,
                                                                              I20Immediate::v<I20Immediate>(SImm<20>(op.value)));
                    break;
                case ImmediateLoader::Op::Type::ADDI_W:
                    location[i] = LOONGARCH64Instructions::ADDI_W::construct(op.isZero ? LOONGARCH64Registers::zero : destination,
                                                                             destination,
                                                                             I12Immediate::v<I12Immediate>(op.value));
                    break;
                case ImmediateLoader::Op::Type::ORI:
                    location[i] = LOONGARCH64Instructions::ORI::construct(op.isZero ? LOONGARCH64Registers::zero : destination,
                                                                          destination,
                                                                          I12Immediate(op.value));
                    break;
                case ImmediateLoader::Op::Type::NOP:
                    location[i] = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                           LOONGARCH64Registers::zero,
                                                                           I12Immediate::v<I12Immediate, 0>());
                    break;
                }
            }
        }

        static void* read(uint32_t* location)
        {
            unsigned i = 0;
            {
                // Iterate through all ImmediateLoader::Op::Type::NOP instructions generated for the purposes of the placeholder.
                uint32_t nopInsn = LOONGARCH64Instructions::ANDI::construct(LOONGARCH64Registers::zero,
                                                                            LOONGARCH64Registers::zero,
                                                                            I12Immediate::v<I12Immediate, 0>());
                for (; i < 8; ++i) {
                    if (location[i] != nopInsn)
                        break;
                }
            }

            intptr_t target = 0;
            for (; i < 8; ++i) {
                LOONGARCH64Instructions::InstructionValue insn(location[i]);

                // Counterpart to ImmediateLoader::Op::Type::LU52I_D.
                if (insn.field<22, 10>() == uint32_t(LOONGARCH64Instructions::Opcode10::LU52I_D_OP)) {
                    target += int64_t(insn.field<10, 12>()) << 52;
                    continue;
                }

                // Counterpart to ImmediateLoader::Op::Type::LU32I_D.
                if (insn.field<25, 7>()  == uint32_t(LOONGARCH64Instructions::Opcode7::LU32I_D_OP)) {
                    target += int64_t(insn.field<5, 20>()) << 32;
                    continue;
                }

                // Counterpart to ImmediateLoader::Op::Type::LU12I_W.
                if (insn.field<25, 7>()  == uint32_t(LOONGARCH64Instructions::Opcode7::LU12I_W_OP)) {
                    target += int64_t(insn.field<5, 20>()) << 12;
                    continue;
                }

                // Counterpart to ImmediateLoader::Op::Type::ORI.
                if (insn.field<22, 10>() == uint32_t(LOONGARCH64Instructions::Opcode10::ORI_OP)) {
                    target += int64_t(insn.field<10, 12>());
                    continue;
                }

                // Counterpart to ImmediateLoader::Op::Type::ADDI_W.
                if (insn.field<22, 10>() == uint32_t(LOONGARCH64Instructions::Opcode10::ADDI_W_OP)) {
                    target += int64_t(insn.field<10, 12>());
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

#endif // ENABLE(ASSEMBLER) && CPU(LOONGARCH64)
