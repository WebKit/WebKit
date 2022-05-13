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

#include "config.h"

#if ENABLE(DISASSEMBLER) && ENABLE(RISCV64_DISASSEMBLER)

#include "MacroAssemblerCodeRef.h"
#include "RISCV64Assembler.h"
#include <array>
#include <mutex>

namespace JSC {

namespace RISCV64Disassembler {

template<size_t BufferSize>
struct StringBufferBase {
    char* data() { return buffer.data(); }
    size_t size() { return sizeof(char) * buffer.size(); }

    CString createString()
    {
        buffer[BufferSize - 1] = '\0';
        return { buffer.data() };
    }

    std::array<char, BufferSize> buffer;
};

using StringBuffer = StringBufferBase<256>;
using SmallStringBuffer = StringBufferBase<32>;

template<typename RegisterType> const char* registerName(uint8_t) = delete;

template<>
const char* registerName<RISCV64Instructions::RegistersBase::GType>(uint8_t value)
{
    static const std::array<const char*, 32> s_gpRegNames {
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
        "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
        "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31",
    };

    if (value < 32)
        return s_gpRegNames[value];
    return "<unknown>";
}

template<>
const char* registerName<RISCV64Instructions::RegistersBase::FType>(uint8_t value)
{
    static const std::array<const char*, 32> s_fpRegNames {
        "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
        "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
        "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
        "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
    };

    if (value < 32)
        return s_fpRegNames[value];
    return "<unknown>";
}

const char* roundingMode(uint8_t value)
{
    using RISCV64Instructions::FPRoundingMode;

    switch (value) {
    case unsigned(FPRoundingMode::RNE):
        return "rne";
    case unsigned(FPRoundingMode::RTZ):
        return "rtz";
    case unsigned(FPRoundingMode::RDN):
        return "rdn";
    case unsigned(FPRoundingMode::RUP):
        return "rup";
    case unsigned(FPRoundingMode::RMM):
        return "rmm";
    case unsigned(FPRoundingMode::DYN):
        return "dyn";
    default:
        break;
    }

    return "<unknown>";
}

SmallStringBuffer memoryOperationFlags(uint8_t value)
{
    using RISCV64Instructions::MemoryOperation;

    SmallStringBuffer buffer;
    if (!!value) {
        snprintf(buffer.data(), buffer.size(), "%s%s%s%s",
            (value & unsigned(MemoryOperation::I)) ? "i" : "",
            (value & unsigned(MemoryOperation::O)) ? "o" : "",
            (value & unsigned(MemoryOperation::R)) ? "r" : "",
            (value & unsigned(MemoryOperation::W)) ? "w" : "");
    } else
        snprintf(buffer.data(), buffer.size(), "<none>");
    return buffer;
}

const char* aqrlFlags(uint8_t value)
{
    using RISCV64Instructions::MemoryAccess;

    switch (value) {
    case 0:
        return "";
    case unsigned(MemoryAccess::Acquire):
        return ".aq";
    case unsigned(MemoryAccess::Release):
        return ".rl";
    case unsigned(MemoryAccess::AcquireRelease):
        return ".aqrl";
    default:
        break;
    }

    return "<unknown>";
}

// A simple type that handles a parameter pack of instruction types, along with a contains<T>() helper that serves as a
// compile-time check whether a given type is included in that parameter pack.

template<typename... Args>
struct InstructionList {
    template<typename T>
    static constexpr bool contains()
    {
        return (std::is_same_v<T, Args> || ...);
    }
};

// To enable showing sensible disassembly, different instructions have to be formatted differently. Each such formatting
// class specifies the list of instructions it can format, but generally instructions under a given formatter fall into
// the same class of instructions. The disassemble() static function returns a CString holding the formatted data.

struct RTypeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::ADD, RISCV64Instructions::SUB,
        RISCV64Instructions::SLT, RISCV64Instructions::SLTU,
        RISCV64Instructions::SLL, RISCV64Instructions::SRL, RISCV64Instructions::SRA,
        RISCV64Instructions::XOR, RISCV64Instructions::OR, RISCV64Instructions::AND,
        RISCV64Instructions::ADDW, RISCV64Instructions::SUBW,
        RISCV64Instructions::SLLW, RISCV64Instructions::SRLW, RISCV64Instructions::SRAW,
        RISCV64Instructions::FSGNJ_S, RISCV64Instructions::FSGNJ_D,
        RISCV64Instructions::FSGNJN_S, RISCV64Instructions::FSGNJN_D,
        RISCV64Instructions::FSGNJX_S, RISCV64Instructions::FSGNJX_D,
        RISCV64Instructions::FMIN_S, RISCV64Instructions::FMIN_D,
        RISCV64Instructions::FMAX_S, RISCV64Instructions::FMAX_D,
        RISCV64Instructions::FEQ_S, RISCV64Instructions::FEQ_D,
        RISCV64Instructions::FLT_S, RISCV64Instructions::FLT_D,
        RISCV64Instructions::FLE_S, RISCV64Instructions::FLE_D,
        RISCV64Instructions::MUL, RISCV64Instructions::MULH, RISCV64Instructions::MULHSU, RISCV64Instructions::MULHU,
        RISCV64Instructions::DIV, RISCV64Instructions::DIVU, RISCV64Instructions::REM, RISCV64Instructions::REMU,
        RISCV64Instructions::MULW, RISCV64Instructions::DIVW, RISCV64Instructions::DIVUW, RISCV64Instructions::REMW, RISCV64Instructions::REMUW>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RS1>(T::rs1(insn)), registerName<typename T::Registers::RS2>(T::rs2(insn)));
        return buffer.createString();
    }
};

struct RTypeR2Formatting {
    using List = InstructionList<
        RISCV64Instructions::FMV_X_W, RISCV64Instructions::FMV_W_X,
        RISCV64Instructions::FMV_X_D, RISCV64Instructions::FMV_D_X,
        RISCV64Instructions::FCLASS_S, RISCV64Instructions::FCLASS_D>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s",
            T::name,
            registerName<typename T::Registers::RD>(T::rd(insn)), registerName<typename T::Registers::RS1>(T::rs1(insn)));
        return buffer.createString();
    }
};

struct RTypeWithRoundingModeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::FADD_S,
        RISCV64Instructions::FADD_D,
        RISCV64Instructions::FSUB_S,
        RISCV64Instructions::FSUB_D,
        RISCV64Instructions::FMUL_S,
        RISCV64Instructions::FMUL_D,
        RISCV64Instructions::FDIV_S,
        RISCV64Instructions::FDIV_D>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());
        uint8_t rm = T::rm(insn);

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %s%s%s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RS1>(T::rs1(insn)), registerName<typename T::Registers::RS2>(T::rs2(insn)),
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : ", rm:",
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : roundingMode(rm));
        return buffer.createString();
    }
};

struct RTypeWithRoundingModeFSQRTFormatting {
    using List = InstructionList<
        RISCV64Instructions::FSQRT_S, RISCV64Instructions::FSQRT_D>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());
        uint8_t rm = T::rm(insn);

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s%s%s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RS1>(T::rs1(insn)),
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : ", rm:",
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : roundingMode(rm));
        return buffer.createString();
    }
};

struct RTypeWithRoundingModeFCVTFormatting {
    using List = InstructionList<
        RISCV64Instructions::FCVT_W_S, RISCV64Instructions::FCVT_WU_S,
        RISCV64Instructions::FCVT_S_W, RISCV64Instructions::FCVT_S_WU,
        RISCV64Instructions::FCVT_W_D, RISCV64Instructions::FCVT_WU_D,
        RISCV64Instructions::FCVT_D_W, RISCV64Instructions::FCVT_D_WU,
        RISCV64Instructions::FCVT_L_S, RISCV64Instructions::FCVT_LU_S,
        RISCV64Instructions::FCVT_S_L, RISCV64Instructions::FCVT_S_LU,
        RISCV64Instructions::FCVT_L_D, RISCV64Instructions::FCVT_LU_D,
        RISCV64Instructions::FCVT_D_L, RISCV64Instructions::FCVT_D_LU,
        RISCV64Instructions::FCVT_S_D, RISCV64Instructions::FCVT_D_S>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());
        uint8_t rm = T::rm(insn);

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s%s%s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)), registerName<typename T::Registers::RS1>(T::rs1(insn)),
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : ", rm:",
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : roundingMode(rm));
        return buffer.createString();
    }
};

struct RTypeWithAqRlDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::SC_W, RISCV64Instructions::SC_D,
        RISCV64Instructions::AMOSWAP_W, RISCV64Instructions::AMOSWAP_D,
        RISCV64Instructions::AMOADD_W, RISCV64Instructions::AMOADD_D,
        RISCV64Instructions::AMOXOR_W, RISCV64Instructions::AMOXOR_D,
        RISCV64Instructions::AMOAND_W, RISCV64Instructions::AMOAND_D,
        RISCV64Instructions::AMOOR_W, RISCV64Instructions::AMOOR_D,
        RISCV64Instructions::AMOMIN_W, RISCV64Instructions::AMOMIN_D,
        RISCV64Instructions::AMOMAX_W, RISCV64Instructions::AMOMAX_D,
        RISCV64Instructions::AMOMINU_W, RISCV64Instructions::AMOMINU_D,
        RISCV64Instructions::AMOMAXU_W, RISCV64Instructions::AMOMAXU_D>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s%s %s, %s, %s",
            T::name, aqrlFlags(T::aqrl(insn)), registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RS1>(T::rs1(insn)), registerName<typename T::Registers::RS2>(T::rs2(insn)));
        return buffer.createString();
    }
};

struct RTypeWithAqRlLRFormatting {
    using List = InstructionList<
        RISCV64Instructions::LR_W, RISCV64Instructions::LR_D>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s%s %s, %s",
            T::name, aqrlFlags(T::aqrl(insn)),
            registerName<typename T::Registers::RD>(T::rd(insn)), registerName<typename T::Registers::RS1>(T::rs1(insn)));
        return buffer.createString();
    }
};

struct R4TypeWithRoundingModeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::FMADD_S, RISCV64Instructions::FMADD_D,
        RISCV64Instructions::FMSUB_S, RISCV64Instructions::FMSUB_D,
        RISCV64Instructions::FNMSUB_S, RISCV64Instructions::FNMSUB_D,
        RISCV64Instructions::FNMADD_S, RISCV64Instructions::FNMADD_D>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());
        uint8_t rm = T::rm(insn);

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %s, %s%s%s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)), registerName<typename T::Registers::RS1>(T::rs1(insn)),
            registerName<typename T::Registers::RS2>(T::rs2(insn)), registerName<typename T::Registers::RS3>(T::rs3(insn)),
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : ", rm:",
            (rm == unsigned(RISCV64Instructions::FPRoundingMode::DYN)) ? "" : roundingMode(rm));
        return buffer.createString();
    }
};

struct ITypeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::ADDI, RISCV64Instructions::SLTI, RISCV64Instructions::SLTIU,
        RISCV64Instructions::XORI, RISCV64Instructions::ORI, RISCV64Instructions::ANDI,
        RISCV64Instructions::SLLI, RISCV64Instructions::SRLI, RISCV64Instructions::SRAI,
        RISCV64Instructions::ADDIW, RISCV64Instructions::SLLIW, RISCV64Instructions::SRLIW, RISCV64Instructions::SRAIW>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RS1>(T::rs1(insn)), RISCV64Instructions::IImmediate::value(insn));
        return buffer.createString();
    }
};

struct ITypeImmediateAsOffsetFormatting {
    using List = InstructionList<
        RISCV64Instructions::JALR,
        RISCV64Instructions::LB, RISCV64Instructions::LBU,
        RISCV64Instructions::LH, RISCV64Instructions::LHU,
        RISCV64Instructions::LW, RISCV64Instructions::LWU,
        RISCV64Instructions::LD,
        RISCV64Instructions::FLW, RISCV64Instructions::FLD>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %d(%s)",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            RISCV64Instructions::IImmediate::value(insn), registerName<typename T::Registers::RS1>(T::rs1(insn)));
        return buffer.createString();
    }
};

struct STypeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::SB, RISCV64Instructions::SH, RISCV64Instructions::SW, RISCV64Instructions::SD,
        RISCV64Instructions::FSW, RISCV64Instructions::FSD>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %d(%s)",
            T::name, registerName<typename T::Registers::RS2>(T::rs2(insn)),
            RISCV64Instructions::SImmediate::value(insn), registerName<typename T::Registers::RS1>(T::rs1(insn)));
        return buffer.createString();
    }
};

struct BTypeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::BEQ,
        RISCV64Instructions::BNE,
        RISCV64Instructions::BLT,
        RISCV64Instructions::BGE,
        RISCV64Instructions::BLTU,
        RISCV64Instructions::BGEU>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %d",
            T::name, registerName<typename T::Registers::RS1>(T::rs1(insn)), registerName<typename T::Registers::RS2>(T::rs2(insn)),
            RISCV64Instructions::BImmediate::value(insn));
        return buffer.createString();
    }
};

struct UTypeDefaultFormatting {
    using List = InstructionList<
        RISCV64Instructions::LUI, RISCV64Instructions::AUIPC>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %u",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)), RISCV64Instructions::UImmediate::value(insn));
        return buffer.createString();
    }
};

struct JTypeDefaultFormatting {
    using List = InstructionList<RISCV64Instructions::JAL>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %d(%s)",
            T::name, RISCV64Instructions::JImmediate::value(insn), registerName<typename T::Registers::RD>(T::rd(insn)));
        return buffer.createString();
    }
};

struct FenceInstructionFormatting {
    using List = InstructionList<RISCV64Instructions::FENCE>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        auto immValue = RISCV64Instructions::IImmediate::value(insn);
        auto predecessorFlags = memoryOperationFlags((immValue >> 4) & ((1 << 4) - 1));
        auto successorFlags = memoryOperationFlags((immValue >> 0) & ((1 << 4) - 1));

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s,%s",
            T::name, predecessorFlags.data(), successorFlags.data());
        return buffer.createString();
    }
};

struct FenceIInstructionFormatting {
    using List = InstructionList<RISCV64Instructions::FENCE_I>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue)
    {
        static_assert(List::contains<T>());

        return { T::name };
    }
};

struct EnvironmentInstructionFormatting {
    using List = InstructionList<
        RISCV64Instructions::ECALL, RISCV64Instructions::EBREAK>;

    template<typename T>
    static CString disassemble(RISCV64Instructions::InstructionValue)
    {
        static_assert(List::contains<T>());

        return { T::name };
    }
};

// The Disassembler struct below has a template parameter pack, containing a list of instructions through which it
// should cascade and find a matching instruction type. When found, the DisassemblyFormatting class finds an
// appropriate formatter and uses it to return the disassembly string for the given instruction value.

template<typename T, typename FormattingType, typename... OtherFormattingTypes>
struct DisassemblyFormattingImpl {
    using Type = std::conditional_t<FormattingType::List::template contains<T>(),
        FormattingType,
        typename DisassemblyFormattingImpl<T, OtherFormattingTypes...>::Type>;
};

template<typename T, typename FormattingType>
struct DisassemblyFormattingImpl<T, FormattingType> {
    using Type = FormattingType;
};

template<typename T>
struct DisassemblyFormatting {
    using Type = typename DisassemblyFormattingImpl<T,
        RTypeDefaultFormatting,
        RTypeR2Formatting,
        RTypeWithRoundingModeDefaultFormatting,
        RTypeWithRoundingModeFSQRTFormatting,
        RTypeWithRoundingModeFCVTFormatting,
        RTypeWithAqRlDefaultFormatting,
        RTypeWithAqRlLRFormatting,
        R4TypeWithRoundingModeDefaultFormatting,
        ITypeDefaultFormatting,
        ITypeImmediateAsOffsetFormatting,
        STypeDefaultFormatting,
        BTypeDefaultFormatting,
        UTypeDefaultFormatting,
        JTypeDefaultFormatting,
        FenceInstructionFormatting,
        FenceIInstructionFormatting,
        EnvironmentInstructionFormatting>::Type;

    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        static_assert(Type::List::template contains<T>());
        return Type::template disassemble<T>(insn);
    }
};

template<typename InsnType, typename... OtherInsnTypes>
struct Disassembler {
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        if (InsnType::matches(insn))
            return DisassemblyFormatting<InsnType>::disassemble(insn);
        return Disassembler<OtherInsnTypes...>::disassemble(insn);
    }
};

template<typename InsnType>
struct Disassembler<InsnType> {
    static CString disassemble(RISCV64Instructions::InstructionValue insn)
    {
        if (InsnType::matches(insn))
            return DisassemblyFormatting<InsnType>::disassemble(insn);
        return { };
    }
};

CString disassembleOpcode(uint32_t *pc)
{
    using namespace RISCV64Instructions;
    using DisassemblerType = Disassembler<
        // RV32I Base Instruction Set
        LUI, AUIPC, JAL, JALR,
        BEQ, BNE, BLT, BGE, BLTU, BGEU,
        LB, LH, LW, LBU, LHU,
        SB, SH, SW,
        ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
        ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
        FENCE, ECALL, EBREAK,
        // RV64I Base Instruction Set (in addition to RV32I)
        LWU, LD, SD,
        ADDIW, SLLIW, SRLIW, SRAIW,
        ADDW, SUBW, SLLW, SRLW, SRAW,
        // RV32/RV64 Zifencei Standard Extension
        FENCE_I,
        // RV32M Standard Extension
        MUL, MULH, MULHSU, MULHU,
        DIV, DIVU, REM, REMU,
        // RV64M Standard Extension (in addition to RV32M)
        MULW, DIVW, DIVUW, REMW, REMUW,
        // RV32A Standard Extension
        LR_W, SC_W,
        AMOSWAP_W, AMOADD_W, AMOXOR_W, AMOAND_W, AMOOR_W, AMOMIN_W, AMOMAX_W, AMOMINU_W, AMOMAXU_W,
        // RV64A Standard Extension (in addition to RV32A)
        LR_D, SC_D,
        AMOSWAP_D, AMOADD_D, AMOXOR_D, AMOAND_D, AMOOR_D, AMOMIN_D, AMOMAX_D, AMOMINU_D, AMOMAXU_D,
        // RV32F Standard Extension
        FLW, FSW,
        FMADD_S, FMSUB_S, FNMSUB_S, FNMADD_S,
        FADD_S, FSUB_S, FMUL_S, FDIV_S, FSQRT_S,
        FSGNJ_S, FSGNJN_S, FSGNJX_S, FMIN_S, FMAX_S,
        FCVT_W_S, FCVT_WU_S, FMV_X_W,
        FEQ_S, FLT_S, FLE_S, FCLASS_S,
        FCVT_S_W, FCVT_S_WU, FMV_W_X,
        // RV64F Standard Extension (in addition to RV32F)
        FCVT_L_S, FCVT_LU_S, FCVT_S_L, FCVT_S_LU,
        // RV32D Standard Extension
        FLD, FSD,
        FMADD_D, FMSUB_D, FNMSUB_D, FNMADD_D,
        FADD_D, FSUB_D, FMUL_D, FDIV_D, FSQRT_D,
        FSGNJ_D, FSGNJN_D, FSGNJX_D, FMIN_D, FMAX_D,
        FCVT_S_D, FCVT_D_S,
        FEQ_D, FLT_D, FLE_D, FCLASS_D,
        FCVT_W_D, FCVT_WU_D, FCVT_D_W, FCVT_D_WU,
        // RV64D Standard Extension (in addition to RV32D)
        FCVT_L_D, FCVT_LU_D, FMV_X_D,
        FCVT_D_L, FCVT_D_LU, FMV_D_X>;

    auto disassembly = DisassemblerType::disassemble(InstructionValue { *pc });
    if (!disassembly.isNull())
        return disassembly;
    return CString { "<unrecognized opcode>" };
}

} // namespace RISCV64Disassembler

bool tryToDisassemble(const MacroAssemblerCodePtr<DisassemblyPtrTag>& codePtr, size_t size, void*, void*, const char* prefix, PrintStream& out)
{
    uint32_t* currentPC = codePtr.untaggedExecutableAddress<uint32_t*>();
    size_t byteCount = size;

    while (byteCount) {
        out.printf("%s%#16llx: <%08x> %s\n", prefix, static_cast<unsigned long long>(bitwise_cast<uintptr_t>(currentPC)), *currentPC,
            RISCV64Disassembler::disassembleOpcode(currentPC).data());

        ++currentPC;
        byteCount -= sizeof(uint32_t);
    }

    return true;
}

} // namespace JSC

#endif // ENABLE(DISASSEMBLER) && ENABLE(RISCV64_DISASSEMBLER)
