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
