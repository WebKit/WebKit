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

#include "config.h"

#if ENABLE(DISASSEMBLER) && ENABLE(LOONGARCH64_DISASSEMBLER)

#include "MacroAssemblerCodeRef.h"
#include "LOONGARCH64Assembler.h"
#include <array>
#include <mutex>

namespace JSC {

namespace LOONGARCH64Disassembler {

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
const char* registerName<LOONGARCH64Instructions::RegistersBase::GType>(uint8_t value)
{
    static const std::array<const char*, 32> s_gpRegNames {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
        "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
    };

    if (value < 32)
        return s_gpRegNames[value];
    return "<unknown>";
}

template<>
const char* registerName<LOONGARCH64Instructions::RegistersBase::FType>(uint8_t value)
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

template<>
const char* registerName<LOONGARCH64Instructions::RegistersBase::CType>(uint8_t value)
{
    static const std::array<const char*, 8> s_cfRegNames {
        "fcc0", "fcc1", "fcc2", "fcc3", "fcc4", "fcc5", "fcc6", "fcc7",
    };

    if (value < 8)
        return s_cfRegNames[value];
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
        LOONGARCH64Instructions::ADD_D, LOONGARCH64Instructions::SUB_D,
        LOONGARCH64Instructions::SLT, LOONGARCH64Instructions::SLTU,
        LOONGARCH64Instructions::SLL_D, LOONGARCH64Instructions::SRL_D, LOONGARCH64Instructions::SRA_D,
        LOONGARCH64Instructions::XOR, LOONGARCH64Instructions::OR, LOONGARCH64Instructions::AND,
        LOONGARCH64Instructions::ADD_W, LOONGARCH64Instructions::SUB_W,
        LOONGARCH64Instructions::SLL_W, LOONGARCH64Instructions::SRL_W, LOONGARCH64Instructions::SRA_W,
        LOONGARCH64Instructions::FCOPYSIGN_S, LOONGARCH64Instructions::FCOPYSIGN_D,
        LOONGARCH64Instructions::FMIN_S, LOONGARCH64Instructions::FMIN_D,
        LOONGARCH64Instructions::FMAX_S, LOONGARCH64Instructions::FMAX_D,
        LOONGARCH64Instructions::FCMP_CEQ_S, LOONGARCH64Instructions::FCMP_CEQ_D,
        LOONGARCH64Instructions::FCMP_CLT_S, LOONGARCH64Instructions::FCMP_CLT_D,
        LOONGARCH64Instructions::FCMP_CLE_S, LOONGARCH64Instructions::FCMP_CLE_D,
        LOONGARCH64Instructions::MUL_D, LOONGARCH64Instructions::MULH_D, LOONGARCH64Instructions::MULH_WU, LOONGARCH64Instructions::MULH_DU,
        LOONGARCH64Instructions::DIV_D, LOONGARCH64Instructions::DIV_DU, LOONGARCH64Instructions::MOD_D, LOONGARCH64Instructions::MOD_DU,
        LOONGARCH64Instructions::MUL_W, LOONGARCH64Instructions::DIV_W, LOONGARCH64Instructions::DIV_WU, LOONGARCH64Instructions::MOD_W, LOONGARCH64Instructions::MOD_WU>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RJ>(T::rj(insn)), registerName<typename T::Registers::RK>(T::rk(insn)));
        return buffer.createString();
    }
};

struct RRTypeDefaultFormatting {
    using List = InstructionList<
        LOONGARCH64Instructions::FNEG_S, LOONGARCH64Instructions::FNEG_D,
        LOONGARCH64Instructions::FABS_S, LOONGARCH64Instructions::FABS_D,
        LOONGARCH64Instructions::FCLASS_S, LOONGARCH64Instructions::FCLASS_D>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RJ>(T::rj(insn)));
        return buffer.createString();
    }
};

struct ITypeImmediateAsOffsetFormatting {
    using List = InstructionList<
        LOONGARCH64Instructions::LD_B, LOONGARCH64Instructions::LD_BU,
        LOONGARCH64Instructions::LD_H, LOONGARCH64Instructions::LD_HU,
        LOONGARCH64Instructions::LD_W, LOONGARCH64Instructions::LD_WU,
        LOONGARCH64Instructions::LD_D,
        LOONGARCH64Instructions::FLD_S, LOONGARCH64Instructions::FLD_D>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RJ>(T::rj(insn)), LOONGARCH64Instructions::I12Immediate::value(insn));
        return buffer.createString();
    }
};

struct I20RTypeDefaultFormatting {
    using List = InstructionList<
        LOONGARCH64Instructions::PCADDU18I>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            LOONGARCH64Instructions::I20Immediate::value(insn));
        return buffer.createString();
    }
};

struct I16RRTypeDefaultFormatting {
    using List = InstructionList<
        LOONGARCH64Instructions::BEQ,
        LOONGARCH64Instructions::BNE,
        LOONGARCH64Instructions::BLT,
        LOONGARCH64Instructions::BGE,
        LOONGARCH64Instructions::BLTU,
        LOONGARCH64Instructions::BGEU,
        LOONGARCH64Instructions::JIRL>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RJ>(T::rj(insn)), LOONGARCH64Instructions::I16Immediate::value(insn));
        return buffer.createString();
    }
};

struct I12RRTypeDefaultFormatting {
    using List = InstructionList<
        LOONGARCH64Instructions::ST_B, LOONGARCH64Instructions::ST_H, LOONGARCH64Instructions::ST_W, LOONGARCH64Instructions::ST_D,
        LOONGARCH64Instructions::FST_S, LOONGARCH64Instructions::FST_D,
        LOONGARCH64Instructions::ADDI_W, LOONGARCH64Instructions::ADDI_D,
        LOONGARCH64Instructions::XORI, LOONGARCH64Instructions::ORI,
        LOONGARCH64Instructions::ANDI,
        LOONGARCH64Instructions::SLTI, LOONGARCH64Instructions::SLTUI>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RJ>(T::rj(insn)), LOONGARCH64Instructions::I12Immediate::value(insn));
        return buffer.createString();
    }
};

struct I8RRTypeDefaultFormatting {
    using List = InstructionList<
        LOONGARCH64Instructions::SLLI_D, LOONGARCH64Instructions::SLLI_W,
        LOONGARCH64Instructions::SRLI_D, LOONGARCH64Instructions::SRLI_W,
        LOONGARCH64Instructions::SRAI_D, LOONGARCH64Instructions::SRAI_W>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)),
            registerName<typename T::Registers::RJ>(T::rj(insn)), LOONGARCH64Instructions::I8Immediate::value(insn));
        return buffer.createString();
    }
};

struct I20TypeDefaultFormatting {
    using List = InstructionList<LOONGARCH64Instructions::LU32I_D>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(List::contains<T>());

        StringBuffer buffer;
        snprintf(buffer.data(), buffer.size(), "%s %s, %d",
            T::name, registerName<typename T::Registers::RD>(T::rd(insn)), LOONGARCH64Instructions::I20Immediate::value(insn));
        return buffer.createString();
    }
};

struct EnvironmentInstructionFormatting {
    using List = InstructionList<LOONGARCH64Instructions::BREAK>;

    template<typename T>
    static CString disassemble(LOONGARCH64Instructions::InstructionValue)
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
        RRTypeDefaultFormatting,
        ITypeImmediateAsOffsetFormatting,
        I20RTypeDefaultFormatting,
        I16RRTypeDefaultFormatting,
        I12RRTypeDefaultFormatting,
        I8RRTypeDefaultFormatting,
        I20TypeDefaultFormatting,
        EnvironmentInstructionFormatting>::Type;

    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        static_assert(Type::List::template contains<T>());
        return Type::template disassemble<T>(insn);
    }
};

template<typename InsnType, typename... OtherInsnTypes>
struct Disassembler {
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        if (InsnType::matches(insn))
            return DisassemblyFormatting<InsnType>::disassemble(insn);
        return Disassembler<OtherInsnTypes...>::disassemble(insn);
    }
};

template<typename InsnType>
struct Disassembler<InsnType> {
    static CString disassemble(LOONGARCH64Instructions::InstructionValue insn)
    {
        if (InsnType::matches(insn))
            return DisassemblyFormatting<InsnType>::disassemble(insn);
        return { };
    }
};

CString disassembleOpcode(uint32_t *pc)
{
    using namespace LOONGARCH64Instructions;
    using DisassemblerType = Disassembler<
        LU32I_D, PCADDU18I, JIRL,
        BEQ, BNE, BLT, BGE, BLTU, BGEU,
        LD_B, LD_H, LD_W, LD_BU, LD_HU,
        ST_B, ST_H, ST_W,
        ADDI_D, SLTI, SLTUI, XORI, ORI, ANDI, SLLI_D, SRLI_D, SRAI_D,
        ADD_D, SUB_D, SLL_D, SLT, SLTU, XOR, SRL_D, SRA_D, OR, AND,
        BREAK,
        LD_WU, LD_D, ST_D,
        ADDI_W, SLLI_W, SRLI_W, SRAI_W,
        ADD_W, SUB_W, SLL_W, SRL_W, SRA_W,
        MUL_D, MULH_D, MULH_WU, MULH_DU,
        DIV_D, DIV_DU, MOD_D, MOD_DU,
        MUL_W, DIV_W, DIV_WU, MOD_W, MOD_WU,
        FLD_S, FST_S,
        FCOPYSIGN_S, FNEG_S, FABS_S, FMIN_S, FMAX_S,
        FCMP_CEQ_S, FCMP_CLT_S, FCMP_CLE_S, FCLASS_S,
        FLD_D, FST_D,
        FCOPYSIGN_D, FNEG_D, FABS_D, FMIN_D, FMAX_D,
        FCMP_CEQ_D, FCMP_CLT_D, FCMP_CLE_D, FCLASS_D>;

    auto disassembly = DisassemblerType::disassemble(InstructionValue { *pc });
    if (!disassembly.isNull())
        return disassembly;
    return CString { "<unrecognized opcode>" };
}

} // namespace LOONGARCH64Disassembler

bool tryToDisassemble(const CodePtr<DisassemblyPtrTag>& codePtr, size_t size, void*, void*, const char* prefix, PrintStream& out)
{
    uint32_t* currentPC = codePtr.untaggedPtr<uint32_t*>();
    size_t byteCount = size;

    while (byteCount) {
        out.printf("%s%#16llx: <%08x> %s\n", prefix, static_cast<unsigned long long>(std::bit_cast<uintptr_t>(currentPC)), *currentPC,
            LOONGARCH64Disassembler::disassembleOpcode(currentPC).data());

        ++currentPC;
        byteCount -= sizeof(uint32_t);
    }

    return true;
}

} // namespace JSC

#endif // ENABLE(DISASSEMBLER) && ENABLE(LOONGARCH64_DISASSEMBLER)
