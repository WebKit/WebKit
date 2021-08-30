/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include "MacroAssembler.h"
#include <wtf/PrintStream.h>

namespace JSC {

typedef MacroAssembler::FPRegisterID FPRReg;
static constexpr FPRReg InvalidFPRReg { FPRReg::InvalidFPRReg };

#if ENABLE(ASSEMBLER)

#if CPU(X86) || CPU(X86_64)

class FPRInfo {
public:
    typedef FPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 6;
    static constexpr unsigned numberOfArgumentRegisters = is64Bit() ? 8 : 0;

    // Temporary registers.
    static constexpr FPRReg fpRegT0 = X86Registers::xmm0;
    static constexpr FPRReg fpRegT1 = X86Registers::xmm1;
    static constexpr FPRReg fpRegT2 = X86Registers::xmm2;
    static constexpr FPRReg fpRegT3 = X86Registers::xmm3;
    static constexpr FPRReg fpRegT4 = X86Registers::xmm4;
    static constexpr FPRReg fpRegT5 = X86Registers::xmm5;
#if CPU(X86_64)
    // Only X86_64 passes aguments in xmm registers
    static constexpr FPRReg argumentFPR0 = X86Registers::xmm0; // fpRegT0
    static constexpr FPRReg argumentFPR1 = X86Registers::xmm1; // fpRegT1
    static constexpr FPRReg argumentFPR2 = X86Registers::xmm2; // fpRegT2
    static constexpr FPRReg argumentFPR3 = X86Registers::xmm3; // fpRegT3
    static constexpr FPRReg argumentFPR4 = X86Registers::xmm4; // fpRegT4
    static constexpr FPRReg argumentFPR5 = X86Registers::xmm5; // fpRegT5
    static constexpr FPRReg argumentFPR6 = X86Registers::xmm6;
    static constexpr FPRReg argumentFPR7 = X86Registers::xmm7;
#endif
    // On X86 the return will actually be on the x87 stack,
    // so we'll copy to xmm0 for sanity!
    static constexpr FPRReg returnValueFPR = X86Registers::xmm0; // fpRegT0

    // FPRReg mapping is direct, the machine regsiter numbers can
    // be used directly as indices into the FPR RegisterBank.
    COMPILE_ASSERT(X86Registers::xmm0 == 0, xmm0_is_0);
    COMPILE_ASSERT(X86Registers::xmm1 == 1, xmm1_is_1);
    COMPILE_ASSERT(X86Registers::xmm2 == 2, xmm2_is_2);
    COMPILE_ASSERT(X86Registers::xmm3 == 3, xmm3_is_3);
    COMPILE_ASSERT(X86Registers::xmm4 == 4, xmm4_is_4);
    COMPILE_ASSERT(X86Registers::xmm5 == 5, xmm5_is_5);
    static FPRReg toRegister(unsigned index)
    {
        return (FPRReg)index;
    }
    static unsigned toIndex(FPRReg reg)
    {
        unsigned result = (unsigned)reg;
        if (result >= numberOfRegisters)
            return InvalidIndex;
        return result;
    }
    
    static FPRReg toArgumentRegister(unsigned index)
    {
        return (FPRReg)index;
    }

    static const char* debugName(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        return MacroAssembler::fprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(X86) || CPU(X86_64)

#if CPU(ARM)

class FPRInfo {
public:
    typedef FPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 6;

#if CPU(ARM_HARDFP)
    static constexpr unsigned numberOfArgumentRegisters = 8;
#else
    static constexpr unsigned numberOfArgumentRegisters = 0;
#endif

    // Temporary registers.
    // d7 is use by the MacroAssembler as fpTempRegister.
    static constexpr FPRReg fpRegT0 = ARMRegisters::d0;
    static constexpr FPRReg fpRegT1 = ARMRegisters::d1;
    static constexpr FPRReg fpRegT2 = ARMRegisters::d2;
    static constexpr FPRReg fpRegT3 = ARMRegisters::d3;
    static constexpr FPRReg fpRegT4 = ARMRegisters::d4;
    static constexpr FPRReg fpRegT5 = ARMRegisters::d5;
    // ARMv7 doesn't pass arguments in fp registers. The return
    // value is also actually in integer registers, for now
    // we'll return in d0 for simplicity.
    static constexpr FPRReg returnValueFPR = ARMRegisters::d0; // fpRegT0

#if CPU(ARM_HARDFP)
    static constexpr FPRReg argumentFPR0 = ARMRegisters::d0; // fpRegT0
    static constexpr FPRReg argumentFPR1 = ARMRegisters::d1; // fpRegT1
#endif

    // FPRReg mapping is direct, the machine regsiter numbers can
    // be used directly as indices into the FPR RegisterBank.
    COMPILE_ASSERT(ARMRegisters::d0 == 0, d0_is_0);
    COMPILE_ASSERT(ARMRegisters::d1 == 1, d1_is_1);
    COMPILE_ASSERT(ARMRegisters::d2 == 2, d2_is_2);
    COMPILE_ASSERT(ARMRegisters::d3 == 3, d3_is_3);
    COMPILE_ASSERT(ARMRegisters::d4 == 4, d4_is_4);
    COMPILE_ASSERT(ARMRegisters::d5 == 5, d5_is_5);
    static FPRReg toRegister(unsigned index)
    {
        return (FPRReg)index;
    }
    static unsigned toIndex(FPRReg reg)
    {
        return (unsigned)reg;
    }

#if CPU(ARM_HARDFP)
    static FPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        return static_cast<FPRReg>(index);
    }
#endif

    static const char* debugName(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        return MacroAssembler::fprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(ARM)

#if CPU(ARM64)

class FPRInfo {
public:
    typedef FPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 23;
    static constexpr unsigned numberOfArgumentRegisters = 8;

    // Temporary registers.
    // q8-q15 are callee saved, q31 is use by the MacroAssembler as fpTempRegister.
    static constexpr FPRReg fpRegT0 = ARM64Registers::q0;
    static constexpr FPRReg fpRegT1 = ARM64Registers::q1;
    static constexpr FPRReg fpRegT2 = ARM64Registers::q2;
    static constexpr FPRReg fpRegT3 = ARM64Registers::q3;
    static constexpr FPRReg fpRegT4 = ARM64Registers::q4;
    static constexpr FPRReg fpRegT5 = ARM64Registers::q5;
    static constexpr FPRReg fpRegT6 = ARM64Registers::q6;
    static constexpr FPRReg fpRegT7 = ARM64Registers::q7;
    static constexpr FPRReg fpRegT8 = ARM64Registers::q16;
    static constexpr FPRReg fpRegT9 = ARM64Registers::q17;
    static constexpr FPRReg fpRegT10 = ARM64Registers::q18;
    static constexpr FPRReg fpRegT11 = ARM64Registers::q19;
    static constexpr FPRReg fpRegT12 = ARM64Registers::q20;
    static constexpr FPRReg fpRegT13 = ARM64Registers::q21;
    static constexpr FPRReg fpRegT14 = ARM64Registers::q22;
    static constexpr FPRReg fpRegT15 = ARM64Registers::q23;
    static constexpr FPRReg fpRegT16 = ARM64Registers::q24;
    static constexpr FPRReg fpRegT17 = ARM64Registers::q25;
    static constexpr FPRReg fpRegT18 = ARM64Registers::q26;
    static constexpr FPRReg fpRegT19 = ARM64Registers::q27;
    static constexpr FPRReg fpRegT20 = ARM64Registers::q28;
    static constexpr FPRReg fpRegT21 = ARM64Registers::q29;
    static constexpr FPRReg fpRegT22 = ARM64Registers::q30;
    static constexpr FPRReg fpRegCS0 = ARM64Registers::q8;
    static constexpr FPRReg fpRegCS1 = ARM64Registers::q9;
    static constexpr FPRReg fpRegCS2 = ARM64Registers::q10;
    static constexpr FPRReg fpRegCS3 = ARM64Registers::q11;
    static constexpr FPRReg fpRegCS4 = ARM64Registers::q12;
    static constexpr FPRReg fpRegCS5 = ARM64Registers::q13;
    static constexpr FPRReg fpRegCS6 = ARM64Registers::q14;
    static constexpr FPRReg fpRegCS7 = ARM64Registers::q15;

    static constexpr FPRReg argumentFPR0 = ARM64Registers::q0; // fpRegT0
    static constexpr FPRReg argumentFPR1 = ARM64Registers::q1; // fpRegT1
    static constexpr FPRReg argumentFPR2 = ARM64Registers::q2; // fpRegT2
    static constexpr FPRReg argumentFPR3 = ARM64Registers::q3; // fpRegT3
    static constexpr FPRReg argumentFPR4 = ARM64Registers::q4; // fpRegT4
    static constexpr FPRReg argumentFPR5 = ARM64Registers::q5; // fpRegT5
    static constexpr FPRReg argumentFPR6 = ARM64Registers::q6; // fpRegT6
    static constexpr FPRReg argumentFPR7 = ARM64Registers::q7; // fpRegT7

    static constexpr FPRReg returnValueFPR = ARM64Registers::q0; // fpRegT0

    static FPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
        static const FPRReg registerForIndex[numberOfRegisters] = {
            fpRegT0, fpRegT1, fpRegT2, fpRegT3, fpRegT4, fpRegT5, fpRegT6, fpRegT7,
            fpRegT8, fpRegT9, fpRegT10, fpRegT11, fpRegT12, fpRegT13, fpRegT14, fpRegT15,
            fpRegT16, fpRegT17, fpRegT18, fpRegT19, fpRegT20, fpRegT21, fpRegT22
        };
        return registerForIndex[index];
    }

    static unsigned toIndex(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        ASSERT(static_cast<int>(reg) < 32);
        static const unsigned indexForRegister[32] = {
            0, 1, 2, 3, 4, 5, 6, 7,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex,
            8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, InvalidIndex
        };
        unsigned result = indexForRegister[reg];
        return result;
    }

    static FPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < 8);
        return static_cast<FPRReg>(index);
    }

    static const char* debugName(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        return MacroAssembler::fprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(ARM64)

#if CPU(MIPS)

class FPRInfo {
public:
    typedef FPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 7;
    static constexpr unsigned numberOfArgumentRegisters = 2;

    // Temporary registers.
    static constexpr FPRReg fpRegT0 = MIPSRegisters::f0;
    static constexpr FPRReg fpRegT1 = MIPSRegisters::f2;
    static constexpr FPRReg fpRegT2 = MIPSRegisters::f4;
    static constexpr FPRReg fpRegT3 = MIPSRegisters::f6;
    static constexpr FPRReg fpRegT4 = MIPSRegisters::f8;
    static constexpr FPRReg fpRegT5 = MIPSRegisters::f10;
    static constexpr FPRReg fpRegT6 = MIPSRegisters::f18;

    static constexpr FPRReg returnValueFPR = MIPSRegisters::f0;

    static constexpr FPRReg argumentFPR0 = MIPSRegisters::f12;
    static constexpr FPRReg argumentFPR1 = MIPSRegisters::f14;

    static FPRReg toRegister(unsigned index)
    {
        static const FPRReg registerForIndex[numberOfRegisters] = {
            fpRegT0, fpRegT1, fpRegT2, fpRegT3, fpRegT4, fpRegT5, fpRegT6 };

        ASSERT(index < numberOfRegisters);
        return registerForIndex[index];
    }

    static FPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        static const FPRReg indexForRegister[2] = {
            argumentFPR0, argumentFPR1
        };
        return indexForRegister[index];
    }

    static unsigned toIndex(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        ASSERT(reg < 20);
        static const unsigned indexForRegister[20] = {
            0, InvalidIndex, 1, InvalidIndex,
            2, InvalidIndex, 3, InvalidIndex,
            4, InvalidIndex, 5, InvalidIndex,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex,
            InvalidIndex, InvalidIndex, 6, InvalidIndex,
        };
        unsigned result = indexForRegister[reg];
        return result;
    }

    static const char* debugName(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        return MacroAssembler::fprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(MIPS)

#if CPU(RISCV64)

class FPRInfo {
public:
    typedef FPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 20;
    static constexpr unsigned numberOfArgumentRegisters = 8;

    static constexpr FPRReg fpRegT0 = RISCV64Registers::f0;
    static constexpr FPRReg fpRegT1 = RISCV64Registers::f1;
    static constexpr FPRReg fpRegT2 = RISCV64Registers::f2;
    static constexpr FPRReg fpRegT3 = RISCV64Registers::f3;
    static constexpr FPRReg fpRegT4 = RISCV64Registers::f4;
    static constexpr FPRReg fpRegT5 = RISCV64Registers::f5;
    static constexpr FPRReg fpRegT6 = RISCV64Registers::f6;
    static constexpr FPRReg fpRegT7 = RISCV64Registers::f7;
    static constexpr FPRReg fpRegT8 = RISCV64Registers::f10;
    static constexpr FPRReg fpRegT9 = RISCV64Registers::f11;
    static constexpr FPRReg fpRegT10 = RISCV64Registers::f12;
    static constexpr FPRReg fpRegT11 = RISCV64Registers::f13;
    static constexpr FPRReg fpRegT12 = RISCV64Registers::f14;
    static constexpr FPRReg fpRegT13 = RISCV64Registers::f15;
    static constexpr FPRReg fpRegT14 = RISCV64Registers::f16;
    static constexpr FPRReg fpRegT15 = RISCV64Registers::f17;
    static constexpr FPRReg fpRegT16 = RISCV64Registers::f28;
    static constexpr FPRReg fpRegT17 = RISCV64Registers::f29;
    static constexpr FPRReg fpRegT18 = RISCV64Registers::f30;
    static constexpr FPRReg fpRegT19 = RISCV64Registers::f31;

    static constexpr FPRReg fpRegCS0 = RISCV64Registers::f8;
    static constexpr FPRReg fpRegCS1 = RISCV64Registers::f9;
    static constexpr FPRReg fpRegCS2 = RISCV64Registers::f18;
    static constexpr FPRReg fpRegCS3 = RISCV64Registers::f19;
    static constexpr FPRReg fpRegCS4 = RISCV64Registers::f20;
    static constexpr FPRReg fpRegCS5 = RISCV64Registers::f21;
    static constexpr FPRReg fpRegCS6 = RISCV64Registers::f22;
    static constexpr FPRReg fpRegCS7 = RISCV64Registers::f23;
    static constexpr FPRReg fpRegCS8 = RISCV64Registers::f24;
    static constexpr FPRReg fpRegCS9 = RISCV64Registers::f25;
    static constexpr FPRReg fpRegCS10 = RISCV64Registers::f26;
    static constexpr FPRReg fpRegCS11 = RISCV64Registers::f27;

    static constexpr FPRReg argumentFPR0 = RISCV64Registers::f10; // fpRegT8
    static constexpr FPRReg argumentFPR1 = RISCV64Registers::f11; // fpRegT9
    static constexpr FPRReg argumentFPR2 = RISCV64Registers::f12; // fpRegT10
    static constexpr FPRReg argumentFPR3 = RISCV64Registers::f13; // fpRegT11
    static constexpr FPRReg argumentFPR4 = RISCV64Registers::f14; // fpRegT12
    static constexpr FPRReg argumentFPR5 = RISCV64Registers::f15; // fpRegT13
    static constexpr FPRReg argumentFPR6 = RISCV64Registers::f16; // fpRegT14
    static constexpr FPRReg argumentFPR7 = RISCV64Registers::f17; // fpRegT15

    static constexpr FPRReg returnValueFPR = RISCV64Registers::f10; // fpRegT8

    static FPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
        static const FPRReg registerForIndex[numberOfRegisters] = {
            fpRegT0, fpRegT1, fpRegT2, fpRegT3, fpRegT4, fpRegT5, fpRegT6, fpRegT7,
            fpRegT8, fpRegT9, fpRegT10, fpRegT11, fpRegT12, fpRegT13, fpRegT14, fpRegT15,
            fpRegT16, fpRegT17, fpRegT18, fpRegT19,
        };
        return registerForIndex[index];
    }

    static FPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        static const FPRReg registerForIndex[numberOfArgumentRegisters] = {
            argumentFPR0, argumentFPR1, argumentFPR2, argumentFPR3,
            argumentFPR4, argumentFPR5, argumentFPR6, argumentFPR7,
        };
        return registerForIndex[index];
    }

    static unsigned toIndex(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        ASSERT(static_cast<int>(reg) < 32);
        static const unsigned indexForRegister[32] = {
            0, 1, 2, 3, 4, 5, 6, 7,
            InvalidIndex, InvalidIndex, 8, 9, 10, 11, 12, 13,
            14, 15, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, 16, 17, 18, 19,
        };
        return indexForRegister[reg];
    }

    static const char* debugName(FPRReg reg)
    {
        ASSERT(reg != InvalidFPRReg);
        return MacroAssembler::fprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(RISCV64)

// We use this hack to get the FPRInfo from the FPRReg type in templates because our code is bad and we should feel bad..
constexpr FPRInfo toInfoFromReg(FPRReg) { return FPRInfo(); }

#endif // ENABLE(ASSEMBLER)

} // namespace JSC

namespace WTF {

inline void printInternal(PrintStream& out, JSC::FPRReg reg)
{
#if ENABLE(ASSEMBLER)
    out.print("%", JSC::FPRInfo::debugName(reg));
#else
    out.printf("%%fr%d", reg);
#endif
}

} // namespace WTF
