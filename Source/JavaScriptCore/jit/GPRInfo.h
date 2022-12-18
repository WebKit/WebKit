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
#include <array>
#include <wtf/FunctionTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>

namespace JSC {

enum NoResultTag { NoResult };

// We use the same conventions in the baseline JIT as in the LLint. If you
// change mappings in the GPRInfo, you should change them in the offlineasm
// compiler adequately. The register naming conventions are described at the
// top of the LowLevelInterpreter.asm file.

typedef MacroAssembler::RegisterID GPRReg;
static constexpr GPRReg InvalidGPRReg { GPRReg::InvalidGPRReg };

#if ENABLE(ASSEMBLER)

#if USE(JSVALUE64)
class JSValueRegs {
public:
    constexpr JSValueRegs()
        : m_gpr(InvalidGPRReg)
    {
    }
    
    constexpr explicit JSValueRegs(GPRReg gpr)
        : m_gpr(gpr)
    {
    }
    
    static JSValueRegs payloadOnly(GPRReg gpr)
    {
        return JSValueRegs(gpr);
    }
    
    static JSValueRegs withTwoAvailableRegs(GPRReg gpr, GPRReg)
    {
        return JSValueRegs(gpr);
    }
    
    bool operator!() const { return m_gpr == InvalidGPRReg; }
    explicit operator bool() const { return m_gpr != InvalidGPRReg; }

    constexpr bool operator==(JSValueRegs other) const { return m_gpr == other.m_gpr; }
    constexpr bool operator!=(JSValueRegs other) const { return !(*this == other); }
    
    constexpr GPRReg gpr() const { return m_gpr; }
    constexpr GPRReg tagGPR() const { return InvalidGPRReg; }
    constexpr GPRReg payloadGPR() const { return m_gpr; }
    
    constexpr bool uses(GPRReg gpr) const
    {
        if (gpr == InvalidGPRReg)
            return false;
        return m_gpr == gpr;
    }
    constexpr bool overlaps(JSValueRegs other) const { return uses(other.payloadGPR()); }

    void dump(PrintStream&) const;
    
private:
    GPRReg m_gpr;
};

class JSValueSource {
public:
    JSValueSource()
        : m_offset(notAddress())
        , m_base(InvalidGPRReg)
    {
    }
    
    JSValueSource(JSValueRegs regs)
        : m_offset(notAddress())
        , m_base(regs.gpr())
    {
    }
    
    explicit JSValueSource(GPRReg gpr)
        : m_offset(notAddress())
        , m_base(gpr)
    {
    }
    
    JSValueSource(MacroAssembler::Address address)
        : m_offset(address.offset)
        , m_base(address.base)
    {
        ASSERT(m_offset != notAddress());
        ASSERT(m_base != InvalidGPRReg);
    }
    
    static JSValueSource unboxedCell(GPRReg payloadGPR)
    {
        return JSValueSource(payloadGPR);
    }
    
    bool operator!() const { return m_base == InvalidGPRReg; }
    explicit operator bool() const { return m_base != InvalidGPRReg; }
    
    bool isAddress() const { return m_offset != notAddress(); }
    
    int32_t offset() const
    {
        ASSERT(isAddress());
        return m_offset;
    }
    
    GPRReg base() const
    {
        ASSERT(isAddress());
        return m_base;
    }
    
    GPRReg gpr() const
    {
        ASSERT(!isAddress());
        return m_base;
    }

    GPRReg payloadGPR() const { return gpr(); }

    JSValueRegs regs() const
    {
        return JSValueRegs(gpr());
    }
    
    MacroAssembler::Address asAddress() const { return MacroAssembler::Address(base(), offset()); }
    
private:
    static inline int32_t notAddress() { return 0x80000000; }     
          
    int32_t m_offset;
    GPRReg m_base;
};
#endif // USE(JSVALUE64)

#if USE(JSVALUE32_64)
class JSValueRegs {
public:
    constexpr JSValueRegs()
        : m_tagGPR(InvalidGPRReg)
        , m_payloadGPR(InvalidGPRReg)
    {
    }
    
    constexpr JSValueRegs(GPRReg tagGPR, GPRReg payloadGPR)
        : m_tagGPR(tagGPR)
        , m_payloadGPR(payloadGPR)
    {
    }
    
    static constexpr JSValueRegs withTwoAvailableRegs(GPRReg tagGPR, GPRReg payloadGPR)
    {
        return JSValueRegs(tagGPR, payloadGPR);
    }
    
    static constexpr JSValueRegs payloadOnly(GPRReg payloadGPR)
    {
        return JSValueRegs(InvalidGPRReg, payloadGPR);
    }
    
    bool operator!() const { return !static_cast<bool>(*this); }
    explicit operator bool() const
    {
        return static_cast<GPRReg>(m_tagGPR) != InvalidGPRReg
            || static_cast<GPRReg>(m_payloadGPR) != InvalidGPRReg;
    }

    constexpr bool operator==(JSValueRegs other) const
    {
        return m_tagGPR == other.m_tagGPR
            && m_payloadGPR == other.m_payloadGPR;
    }
    constexpr bool operator!=(JSValueRegs other) const { return !(*this == other); }
    
    constexpr GPRReg tagGPR() const { return m_tagGPR; }
    constexpr GPRReg payloadGPR() const { return m_payloadGPR; }
    GPRReg gpr(WhichValueWord which) const
    {
        switch (which) {
        case TagWord:
            return tagGPR();
        case PayloadWord:
            return payloadGPR();
        }
        ASSERT_NOT_REACHED();
        return tagGPR();
    }

    constexpr bool uses(GPRReg gpr) const
    {
        if (gpr == InvalidGPRReg)
            return false;
        return m_tagGPR == gpr || m_payloadGPR == gpr;
    }
    constexpr bool overlaps(JSValueRegs other) const
    {
        return uses(other.payloadGPR()) || uses(other.tagGPR());
    }

    void dump(PrintStream&) const;
    
private:
    GPRReg m_tagGPR;
    GPRReg m_payloadGPR;
};

class JSValueSource {
public:
    JSValueSource()
        : m_offset(notAddress())
        , m_baseOrTag(InvalidGPRReg)
        , m_payload(InvalidGPRReg)
        , m_tagType(0)
    {
    }
    
    JSValueSource(JSValueRegs regs)
        : m_offset(notAddress())
        , m_baseOrTag(regs.tagGPR())
        , m_payload(regs.payloadGPR())
        , m_tagType(0)
    {
    }
    
    JSValueSource(GPRReg tagGPR, GPRReg payloadGPR)
        : m_offset(notAddress())
        , m_baseOrTag(tagGPR)
        , m_payload(payloadGPR)
        , m_tagType(0)
    {
    }
    
    JSValueSource(MacroAssembler::Address address)
        : m_offset(address.offset)
        , m_baseOrTag(address.base)
        , m_payload(InvalidGPRReg)
        , m_tagType(0)
    {
        ASSERT(m_offset != notAddress());
        ASSERT(m_baseOrTag != InvalidGPRReg);
    }
    
    static JSValueSource unboxedCell(GPRReg payloadGPR)
    {
        JSValueSource result;
        result.m_offset = notAddress();
        result.m_baseOrTag = InvalidGPRReg;
        result.m_payload = payloadGPR;
        result.m_tagType = static_cast<int8_t>(JSValue::CellTag);
        return result;
    }

    bool operator!() const { return !static_cast<bool>(*this); }
    explicit operator bool() const
    {
        return m_baseOrTag != InvalidGPRReg || m_payload != InvalidGPRReg;
    }
    
    bool isAddress() const
    {
        ASSERT(!!*this);
        return m_offset != notAddress();
    }
    
    int32_t offset() const
    {
        ASSERT(isAddress());
        return m_offset;
    }
    
    GPRReg base() const
    {
        ASSERT(isAddress());
        return m_baseOrTag;
    }
    
    GPRReg tagGPR() const
    {
        ASSERT(!isAddress() && m_baseOrTag != InvalidGPRReg);
        return m_baseOrTag;
    }
    
    GPRReg payloadGPR() const
    {
        ASSERT(!isAddress());
        return m_payload;
    }
    
    bool hasKnownTag() const
    {
        ASSERT(!!*this);
        ASSERT(!isAddress());
        return m_baseOrTag == InvalidGPRReg;
    }
    
    uint32_t tag() const
    {
        return static_cast<int32_t>(m_tagType);
    }
    
    JSValueRegs regs() const
    {
        return JSValueRegs(tagGPR(), payloadGPR());
    }
    
    MacroAssembler::Address asAddress(unsigned additionalOffset = 0) const { return MacroAssembler::Address(base(), offset() + additionalOffset); }
    
private:
    static inline int32_t notAddress() { return 0x80000000; }     
          
    int32_t m_offset;
    GPRReg m_baseOrTag;
    GPRReg m_payload;
    int8_t m_tagType; // Contains the low bits of the tag.
};
#endif // USE(JSVALUE32_64)

#if CPU(X86)
#define NUMBER_OF_ARGUMENT_REGISTERS 0u
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 0u

class GPRInfo {
public:
    typedef GPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 6;
    static constexpr unsigned numberOfArgumentRegisters = NUMBER_OF_ARGUMENT_REGISTERS;

    // Temporary registers.
    static constexpr GPRReg regT0 = X86Registers::eax;
    static constexpr GPRReg regT1 = X86Registers::edx;
    static constexpr GPRReg regT2 = X86Registers::ecx;
    static constexpr GPRReg regT3 = X86Registers::ebx; // Callee-save
    static constexpr GPRReg regT4 = X86Registers::esi; // Callee-save
    static constexpr GPRReg regT5 = X86Registers::edi; // Callee-save
    static constexpr GPRReg callFrameRegister = X86Registers::ebp;
    // These constants provide the names for the general purpose argument & return value registers.
    static constexpr GPRReg argumentGPR0 = X86Registers::ecx; // regT2
    static constexpr GPRReg argumentGPR1 = X86Registers::edx; // regT1
    static constexpr GPRReg argumentGPR2 = X86Registers::eax; // regT0
    static constexpr GPRReg argumentGPR3 = X86Registers::ebx; // regT3
    static constexpr GPRReg nonArgGPR0 = X86Registers::esi; // regT4
    static constexpr GPRReg returnValueGPR = X86Registers::eax; // regT0
    static constexpr GPRReg returnValueGPR2 = X86Registers::edx; // regT1
    static constexpr GPRReg nonPreservedNonReturnGPR = X86Registers::ecx;

    static GPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
        static const GPRReg registerForIndex[numberOfRegisters] = { regT0, regT1, regT2, regT3, regT4, regT5 };
        return registerForIndex[index];
    }

    static GPRReg toArgumentRegister(unsigned)
    {
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }

    static unsigned toIndex(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        ASSERT(static_cast<int>(reg) < 8);
        static const unsigned indexForRegister[8] = { 0, 2, 1, 3, InvalidIndex, InvalidIndex, 4, 5 };
        return indexForRegister[reg];
    }

    static const char* debugName(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        return MacroAssembler::gprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(X86)

#if CPU(X86_64)
#if !OS(WINDOWS)
#define NUMBER_OF_ARGUMENT_REGISTERS 6u
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 5u
#else
#define NUMBER_OF_ARGUMENT_REGISTERS 4u
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 7u
#endif

class GPRInfo {
public:
    typedef GPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 10;
    static constexpr unsigned numberOfArgumentRegisters = NUMBER_OF_ARGUMENT_REGISTERS;

    // These registers match the baseline JIT.
    static constexpr GPRReg callFrameRegister = X86Registers::ebp;
    static constexpr GPRReg numberTagRegister = X86Registers::r14;
    static constexpr GPRReg notCellMaskRegister = X86Registers::r15;
    static constexpr GPRReg constantsRegister = X86Registers::r13;

    // Temporary registers.
    static constexpr GPRReg regT0 = X86Registers::eax;
#if !OS(WINDOWS)
    static constexpr GPRReg regT1 = X86Registers::esi;
    static constexpr GPRReg regT2 = X86Registers::edx;
    static constexpr GPRReg regT3 = X86Registers::ecx;
    static constexpr GPRReg regT4 = X86Registers::r8;
    static constexpr GPRReg regT5 = X86Registers::r10;
    static constexpr GPRReg regT6 = X86Registers::edi;
    static constexpr GPRReg regT7 = X86Registers::r9;
#else
    static constexpr GPRReg regT1 = X86Registers::edx;
    static constexpr GPRReg regT2 = X86Registers::r8;
    static constexpr GPRReg regT3 = X86Registers::r9;
    static constexpr GPRReg regT4 = X86Registers::r10;
    static constexpr GPRReg regT5 = X86Registers::ecx;
#endif

    static constexpr GPRReg regCS0 = X86Registers::ebx;

#if !OS(WINDOWS)
    static constexpr GPRReg regCS1 = X86Registers::r12;
    static constexpr GPRReg regCS2 = X86Registers::r13; // constantsRegister
    static constexpr GPRReg regCS3 = X86Registers::r14; // numberTagRegister
    static constexpr GPRReg regCS4 = X86Registers::r15; // notCellMaskRegister
#else
    static constexpr GPRReg regCS1 = X86Registers::esi;
    static constexpr GPRReg regCS2 = X86Registers::edi;
    static constexpr GPRReg regCS3 = X86Registers::r12;
    static constexpr GPRReg regCS4 = X86Registers::r13; // constantsRegister
    static constexpr GPRReg regCS5 = X86Registers::r14; // numberTagRegister
    static constexpr GPRReg regCS6 = X86Registers::r15; // notCellMaskRegister
#endif

    // These constants provide the names for the general purpose argument & return value registers.
#if !OS(WINDOWS)
    static constexpr GPRReg argumentGPR0 = X86Registers::edi; // regT6
    static constexpr GPRReg argumentGPR1 = X86Registers::esi; // regT1
    static constexpr GPRReg argumentGPR2 = X86Registers::edx; // regT2
    static constexpr GPRReg argumentGPR3 = X86Registers::ecx; // regT3
    static constexpr GPRReg argumentGPR4 = X86Registers::r8; // regT4
    static constexpr GPRReg argumentGPR5 = X86Registers::r9; // regT7
#else
    static constexpr GPRReg argumentGPR0 = X86Registers::ecx; // regT5
    static constexpr GPRReg argumentGPR1 = X86Registers::edx; // regT1
    static constexpr GPRReg argumentGPR2 = X86Registers::r8; // regT2
    static constexpr GPRReg argumentGPR3 = X86Registers::r9; // regT3
#endif
    static constexpr GPRReg nonArgGPR0 = X86Registers::r10; // regT5 (regT4 on Windows)
    static constexpr GPRReg nonArgGPR1 = X86Registers::eax; // regT0
    static constexpr GPRReg returnValueGPR = X86Registers::eax; // regT0
    static constexpr GPRReg returnValueGPR2 = X86Registers::edx; // regT1 or regT2
    static constexpr GPRReg nonPreservedNonReturnGPR = X86Registers::r10; // regT5 (regT4 on Windows)
    static constexpr GPRReg nonPreservedNonArgumentGPR0 = X86Registers::r10; // regT5 (regT4 on Windows)
    static constexpr GPRReg nonPreservedNonArgumentGPR1 = X86Registers::eax;
    static constexpr GPRReg wasmScratchGPR0 = X86Registers::eax;
#if !OS(WINDOWS)
    static constexpr GPRReg wasmScratchGPR1 = X86Registers::r10;
#endif

    // FIXME: I believe that all uses of this are dead in the sense that it just causes the scratch
    // register allocator to select a different register and potentially spill things. It would be better
    // if we instead had a more explicit way of saying that we don't have a scratch register.
    static constexpr GPRReg patchpointScratchRegister = MacroAssembler::s_scratchRegister;

    static GPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
#if !OS(WINDOWS)
        static const GPRReg registerForIndex[numberOfRegisters] = { regT0, regT1, regT2, regT3, regT4, regT5, regT6, regT7, regCS0, regCS1 };
#else
        static const GPRReg registerForIndex[numberOfRegisters] = { regT0, regT1, regT2, regT3, regT4, regT5, regCS0, regCS1, regCS2, regCS3 };
#endif
        return registerForIndex[index];
    }
    
    static GPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
#if !OS(WINDOWS)
        static const GPRReg registerForIndex[numberOfArgumentRegisters] = { argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3, argumentGPR4, argumentGPR5 };
#else
        static const GPRReg registerForIndex[numberOfArgumentRegisters] = { argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3 };
#endif
        return registerForIndex[index];
    }
    
    static unsigned toIndex(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        ASSERT(static_cast<int>(reg) < 16);
#if !OS(WINDOWS)
        static const unsigned indexForRegister[16] = { 0, 3, 2, 8, InvalidIndex, InvalidIndex, 1, 6, 4, 7, 5, InvalidIndex, 9, InvalidIndex, InvalidIndex, InvalidIndex };
#else
        static const unsigned indexForRegister[16] = { 0, 5, 1, 6, InvalidIndex, InvalidIndex, 7, 8, 2, 3, 4, InvalidIndex, 9, InvalidIndex, InvalidIndex, InvalidIndex };
#endif
        return indexForRegister[reg];
    }

    static const char* debugName(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        return MacroAssembler::gprName(reg);
    }

    static const std::array<GPRReg, 3>& reservedRegisters()
    {
        static const std::array<GPRReg, 3> reservedRegisters { {
            MacroAssembler::s_scratchRegister,
            numberTagRegister,
            notCellMaskRegister,
        } };
        return reservedRegisters;
    }
    
    static constexpr unsigned InvalidIndex = 0xffffffff;
};

static_assert(GPRInfo::regT0 == X86Registers::eax);
static_assert(GPRInfo::returnValueGPR2 == X86Registers::edx);

#endif // CPU(X86_64)

#if CPU(ARM_THUMB2)
#define NUMBER_OF_ARGUMENT_REGISTERS 4u
// Callee Saves includes r10, r11, and FP registers d8..d15, which are twice the size of a GPR
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 18u

class GPRInfo {
public:
    typedef GPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 10;
    static constexpr unsigned numberOfArgumentRegisters = NUMBER_OF_ARGUMENT_REGISTERS;

    // Temporary registers.
    static constexpr GPRReg regT0 = ARMRegisters::r0;
    static constexpr GPRReg regT1 = ARMRegisters::r1;
    static constexpr GPRReg regT2 = ARMRegisters::r2;
    static constexpr GPRReg regT3 = ARMRegisters::r3;
    static constexpr GPRReg regT4 = ARMRegisters::r4;
    static constexpr GPRReg regT5 = ARMRegisters::r5;
    static constexpr GPRReg regT6 = ARMRegisters::r8;
    static constexpr GPRReg regT7 = ARMRegisters::r9;
    static constexpr GPRReg regCS0 = ARMRegisters::r10;
    static constexpr GPRReg regCS1 = ARMRegisters::r11;
    // These registers match the baseline JIT.
    static constexpr GPRReg callFrameRegister = ARMRegisters::fp;
    // These constants provide the names for the general purpose argument & return value registers.
    static constexpr GPRReg argumentGPR0 = ARMRegisters::r0; // regT0
    static constexpr GPRReg argumentGPR1 = ARMRegisters::r1; // regT1
    static constexpr GPRReg argumentGPR2 = ARMRegisters::r2; // regT2
    static constexpr GPRReg argumentGPR3 = ARMRegisters::r3; // regT3
    static constexpr GPRReg nonArgGPR0 = ARMRegisters::r4; // regT4
    static constexpr GPRReg returnValueGPR = ARMRegisters::r0; // regT0
    static constexpr GPRReg returnValueGPR2 = ARMRegisters::r1; // regT1
    static constexpr GPRReg nonPreservedNonReturnGPR = ARMRegisters::r5;
    static constexpr GPRReg nonPreservedNonArgumentGPR0 = ARMRegisters::r5;

    static GPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
        static const GPRReg registerForIndex[numberOfRegisters] = { regT0, regT1, regT2, regT3, regT4, regT5, regT6, regT7, regCS0, regCS1 };
        return registerForIndex[index];
    }

    static GPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        static const GPRReg registerForIndex[numberOfArgumentRegisters] = { argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3 };
        return registerForIndex[index];
    }

    static unsigned toIndex(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        ASSERT(static_cast<int>(reg) < 16);
        static const unsigned indexForRegister[16] =
            { 0, 1, 2, 3, 4, 5, InvalidIndex, InvalidIndex, 6, 7, 8, 9, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex };
        unsigned result = indexForRegister[reg];
        return result;
    }

    static const char* debugName(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        return MacroAssembler::gprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(ARM)

#if CPU(ARM64)
#define NUMBER_OF_ARGUMENT_REGISTERS 8u
// Callee Saves includes x19..x28 and FP registers q8..q15
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 18u

class GPRInfo {
public:
    typedef GPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 16;
    static constexpr unsigned numberOfArgumentRegisters = NUMBER_OF_ARGUMENT_REGISTERS;

    // These registers match the baseline JIT.
    static constexpr GPRReg callFrameRegister = ARM64Registers::fp;
    static constexpr GPRReg numberTagRegister = ARM64Registers::x27;
    static constexpr GPRReg notCellMaskRegister = ARM64Registers::x28;
    static constexpr GPRReg constantsRegister = ARM64Registers::x26;
    static constexpr GPRReg dataTempRegister = MacroAssembler::dataTempRegister;
    static constexpr GPRReg memoryTempRegister = MacroAssembler::memoryTempRegister;
    // Temporary registers.
    static constexpr GPRReg regT0 = ARM64Registers::x0;
    static constexpr GPRReg regT1 = ARM64Registers::x1;
    static constexpr GPRReg regT2 = ARM64Registers::x2;
    static constexpr GPRReg regT3 = ARM64Registers::x3;
    static constexpr GPRReg regT4 = ARM64Registers::x4;
    static constexpr GPRReg regT5 = ARM64Registers::x5;
    static constexpr GPRReg regT6 = ARM64Registers::x6;
    static constexpr GPRReg regT7 = ARM64Registers::x7;
    static constexpr GPRReg regT8 = ARM64Registers::x8;
    static constexpr GPRReg regT9 = ARM64Registers::x9;
    static constexpr GPRReg regT10 = ARM64Registers::x10;
    static constexpr GPRReg regT11 = ARM64Registers::x11;
    static constexpr GPRReg regT12 = ARM64Registers::x12;
    static constexpr GPRReg regT13 = ARM64Registers::x13;
    static constexpr GPRReg regT14 = ARM64Registers::x14;
    static constexpr GPRReg regT15 = ARM64Registers::x15;
    static constexpr GPRReg regCS0 = ARM64Registers::x19; // Used by FTL only
    static constexpr GPRReg regCS1 = ARM64Registers::x20; // Used by FTL only
    static constexpr GPRReg regCS2 = ARM64Registers::x21; // Used by FTL only
    static constexpr GPRReg regCS3 = ARM64Registers::x22; // Used by FTL only
    static constexpr GPRReg regCS4 = ARM64Registers::x23; // Used by FTL only
    static constexpr GPRReg regCS5 = ARM64Registers::x24; // Used by FTL only
    static constexpr GPRReg regCS6 = ARM64Registers::x25;
    static constexpr GPRReg regCS7 = ARM64Registers::x26; // constants
    static constexpr GPRReg regCS8 = ARM64Registers::x27; // numberTag
    static constexpr GPRReg regCS9 = ARM64Registers::x28; // notCellMask
    // These constants provide the names for the general purpose argument & return value registers.
    static constexpr GPRReg argumentGPR0 = ARM64Registers::x0; // regT0
    static constexpr GPRReg argumentGPR1 = ARM64Registers::x1; // regT1
    static constexpr GPRReg argumentGPR2 = ARM64Registers::x2; // regT2
    static constexpr GPRReg argumentGPR3 = ARM64Registers::x3; // regT3
    static constexpr GPRReg argumentGPR4 = ARM64Registers::x4; // regT4
    static constexpr GPRReg argumentGPR5 = ARM64Registers::x5; // regT5
    static constexpr GPRReg argumentGPR6 = ARM64Registers::x6; // regT6
    static constexpr GPRReg argumentGPR7 = ARM64Registers::x7; // regT7
    static constexpr GPRReg nonArgGPR0 = ARM64Registers::x8; // regT8
    static constexpr GPRReg nonArgGPR1 = ARM64Registers::x9; // regT9
    static constexpr GPRReg returnValueGPR = ARM64Registers::x0; // regT0
    static constexpr GPRReg returnValueGPR2 = ARM64Registers::x1; // regT1
    static constexpr GPRReg nonPreservedNonReturnGPR = ARM64Registers::x2;
    static constexpr GPRReg nonPreservedNonArgumentGPR0 = ARM64Registers::x8;
    static constexpr GPRReg nonPreservedNonArgumentGPR1 = ARM64Registers::x9;
    static constexpr GPRReg patchpointScratchRegister = ARM64Registers::ip0;
    static constexpr GPRReg wasmScratchGPR0 = ARM64Registers::x9;
    static constexpr GPRReg wasmScratchGPR1 = ARM64Registers::x10;
    static constexpr GPRReg wasmScratchGPR2 = ARM64Registers::x11;
    static constexpr GPRReg wasmScratchGPR3 = ARM64Registers::x12;

    // GPRReg mapping is direct, the machine register numbers can
    // be used directly as indices into the GPR RegisterBank.
    static_assert(ARM64Registers::q0 == 0);
    static_assert(ARM64Registers::q1 == 1);
    static_assert(ARM64Registers::q2 == 2);
    static_assert(ARM64Registers::q3 == 3);
    static_assert(ARM64Registers::q4 == 4);
    static_assert(ARM64Registers::q5 == 5);
    static_assert(ARM64Registers::q6 == 6);
    static_assert(ARM64Registers::q7 == 7);
    static_assert(ARM64Registers::q8 == 8);
    static_assert(ARM64Registers::q9 == 9);
    static_assert(ARM64Registers::q10 == 10);
    static_assert(ARM64Registers::q11 == 11);
    static_assert(ARM64Registers::q12 == 12);
    static_assert(ARM64Registers::q13 == 13);
    static_assert(ARM64Registers::q14 == 14);
    static_assert(ARM64Registers::q15 == 15);
    static GPRReg toRegister(unsigned index)
    {
        return (GPRReg)index;
    }
    static unsigned toIndex(GPRReg reg)
    {
        if (reg > regT15)
            return InvalidIndex;
        return (unsigned)reg;
    }

    static GPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        return toRegister(index);
    }

    static const char* debugName(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        return MacroAssembler::gprName(reg);
    }

    static const std::array<GPRReg, 4>& reservedRegisters()
    {
        static const std::array<GPRReg, 4> reservedRegisters { {
            dataTempRegister,
            memoryTempRegister,
            numberTagRegister,
            notCellMaskRegister,
        } };
        return reservedRegisters;
    }
    
    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(ARM64)

#if CPU(MIPS)
#define NUMBER_OF_ARGUMENT_REGISTERS 4u
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 2u

class GPRInfo {
public:
    typedef GPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 11;
    static constexpr unsigned numberOfArgumentRegisters = NUMBER_OF_ARGUMENT_REGISTERS;

    // regT0 must be v0 for returning a 32-bit value.
    // regT1 must be v1 for returning a pair of 32-bit value.

    // Temporary registers.
    static constexpr GPRReg regT0 = MIPSRegisters::v0;
    static constexpr GPRReg regT1 = MIPSRegisters::v1;
    static constexpr GPRReg regT2 = MIPSRegisters::t2;
    static constexpr GPRReg regT3 = MIPSRegisters::t3;
    static constexpr GPRReg regT4 = MIPSRegisters::t4;
    static constexpr GPRReg regT5 = MIPSRegisters::t5;
    static constexpr GPRReg regT6 = MIPSRegisters::t6;
    static constexpr GPRReg regT7 = MIPSRegisters::a0;
    static constexpr GPRReg regT8 = MIPSRegisters::a1;
    static constexpr GPRReg regT9 = MIPSRegisters::a2;
    static constexpr GPRReg regT10 = MIPSRegisters::a3;
    // These registers match the baseline JIT.
    static constexpr GPRReg callFrameRegister = MIPSRegisters::fp;
    // These constants provide the names for the general purpose argument & return value registers.
    static constexpr GPRReg argumentGPR0 = MIPSRegisters::a0;
    static constexpr GPRReg argumentGPR1 = MIPSRegisters::a1;
    static constexpr GPRReg argumentGPR2 = MIPSRegisters::a2;
    static constexpr GPRReg argumentGPR3 = MIPSRegisters::a3;
    static constexpr GPRReg nonArgGPR0 = regT4;
    static constexpr GPRReg returnValueGPR = regT0;
    static constexpr GPRReg returnValueGPR2 = regT1;
    static constexpr GPRReg nonPreservedNonReturnGPR = regT2;
    static constexpr GPRReg regCS0 = MIPSRegisters::s0;
    static constexpr GPRReg regCS1 = MIPSRegisters::s1; // constants

    static GPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
        static const GPRReg registerForIndex[numberOfRegisters] = { regT0, regT1, regT2, regT3, regT4, regT5, regT6, regT7, regT8, regT9, regT10 };
        return registerForIndex[index];
    }

    static GPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        static const GPRReg registerForIndex[numberOfArgumentRegisters] = { argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3 };
        return registerForIndex[index];
    }

    static unsigned toIndex(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        ASSERT(reg < 32);
        static const unsigned indexForRegister[32] = {
            InvalidIndex, InvalidIndex, 0, 1, 7, 8, 9, 10,
            InvalidIndex, InvalidIndex, 2, 3, 4, 5, 6, InvalidIndex,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex
        };
        return indexForRegister[reg];
    }

    static const char* debugName(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        return MacroAssembler::gprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(MIPS)

#if CPU(RISCV64)

#define NUMBER_OF_ARGUMENT_REGISTERS 8u
#define NUMBER_OF_CALLEE_SAVES_REGISTERS 23u

class GPRInfo {
public:
    typedef GPRReg RegisterType;
    static constexpr unsigned numberOfRegisters = 13;
    static constexpr unsigned numberOfArgumentRegisters = NUMBER_OF_ARGUMENT_REGISTERS;

    static constexpr GPRReg callFrameRegister = RISCV64Registers::fp;
    static constexpr GPRReg numberTagRegister = RISCV64Registers::x25;
    static constexpr GPRReg notCellMaskRegister = RISCV64Registers::x26;
    static constexpr GPRReg constantsRegister = RISCV64Registers::x24;

    static constexpr GPRReg regT0 = RISCV64Registers::x10;
    static constexpr GPRReg regT1 = RISCV64Registers::x11;
    static constexpr GPRReg regT2 = RISCV64Registers::x12;
    static constexpr GPRReg regT3 = RISCV64Registers::x13;
    static constexpr GPRReg regT4 = RISCV64Registers::x14;
    static constexpr GPRReg regT5 = RISCV64Registers::x15;
    static constexpr GPRReg regT6 = RISCV64Registers::x16;
    static constexpr GPRReg regT7 = RISCV64Registers::x17;
    static constexpr GPRReg regT8 = RISCV64Registers::x5;
    static constexpr GPRReg regT9 = RISCV64Registers::x6;
    static constexpr GPRReg regT10 = RISCV64Registers::x7;
    static constexpr GPRReg regT11 = RISCV64Registers::x28;
    static constexpr GPRReg regT12 = RISCV64Registers::x29;

    static constexpr GPRReg regCS0 = RISCV64Registers::x9;
    static constexpr GPRReg regCS1 = RISCV64Registers::x18;
    static constexpr GPRReg regCS2 = RISCV64Registers::x19;
    static constexpr GPRReg regCS3 = RISCV64Registers::x20;
    static constexpr GPRReg regCS4 = RISCV64Registers::x21;
    static constexpr GPRReg regCS5 = RISCV64Registers::x22;
    static constexpr GPRReg regCS6 = RISCV64Registers::x23;
    static constexpr GPRReg regCS7 = RISCV64Registers::x24; // constants
    static constexpr GPRReg regCS8 = RISCV64Registers::x25; // numberTag
    static constexpr GPRReg regCS9 = RISCV64Registers::x26; // notCellMask
    static constexpr GPRReg regCS10 = RISCV64Registers::x27;

    static constexpr GPRReg argumentGPR0 = RISCV64Registers::x10; // regT0
    static constexpr GPRReg argumentGPR1 = RISCV64Registers::x11; // regT1
    static constexpr GPRReg argumentGPR2 = RISCV64Registers::x12; // regT2
    static constexpr GPRReg argumentGPR3 = RISCV64Registers::x13; // regT3
    static constexpr GPRReg argumentGPR4 = RISCV64Registers::x14; // regT4
    static constexpr GPRReg argumentGPR5 = RISCV64Registers::x15; // regT5
    static constexpr GPRReg argumentGPR6 = RISCV64Registers::x16; // regT6
    static constexpr GPRReg argumentGPR7 = RISCV64Registers::x17; // regT7

    static constexpr GPRReg nonArgGPR0 = RISCV64Registers::x5; // regT8
    static constexpr GPRReg nonArgGPR1 = RISCV64Registers::x6; // regT9

    static constexpr GPRReg returnValueGPR = RISCV64Registers::x10; // regT0
    static constexpr GPRReg returnValueGPR2 = RISCV64Registers::x11; // regT1

    static constexpr GPRReg nonPreservedNonReturnGPR = RISCV64Registers::x12; // regT2
    static constexpr GPRReg nonPreservedNonArgumentGPR0 = RISCV64Registers::x5; // regT8
    static constexpr GPRReg nonPreservedNonArgumentGPR1 = RISCV64Registers::x6; // regT9

    static constexpr GPRReg wasmScratchGPR0 = RISCV64Registers::x6; // regT9
    static constexpr GPRReg wasmScratchGPR1 = RISCV64Registers::x7; // regT10

    static constexpr GPRReg patchpointScratchRegister = RISCV64Registers::x30; // Should match dataTempRegister

    static GPRReg toRegister(unsigned index)
    {
        ASSERT(index < numberOfRegisters);
        static const GPRReg registerForIndex[numberOfRegisters] = {
            regT0, regT1, regT2, regT3, regT4, regT5, regT6, regT7,
            regT8, regT9, regT10, regT11, regT12,
        };
        return registerForIndex[index];
    }

    static GPRReg toArgumentRegister(unsigned index)
    {
        ASSERT(index < numberOfArgumentRegisters);
        static const GPRReg registerForIndex[numberOfArgumentRegisters] = {
            argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3,
            argumentGPR4, argumentGPR5, argumentGPR6, argumentGPR7,
        };
        return registerForIndex[index];
    }

    static unsigned toIndex(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        ASSERT(static_cast<int>(reg) < 32);
        static const unsigned indexForRegister[32] = {
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, 8, 9, 10,
            InvalidIndex, InvalidIndex, 0, 1, 2, 3, 4, 5,
            6, 7, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex, 11, 12, InvalidIndex, InvalidIndex,
        };
        return indexForRegister[reg];
    }

    static const char* debugName(GPRReg reg)
    {
        ASSERT(reg != InvalidGPRReg);
        return MacroAssembler::gprName(reg);
    }

    static constexpr unsigned InvalidIndex = 0xffffffff;
};

#endif // CPU(RISCV64)

// To make some code generic over both JSVALUE64 and JSVALUE32_64 platforms, we use standard names
// for certain JSValueRegs instances. On JSVALUE64, a JSValueRegs corresponds to a single 64-bit
// architectural GPR, while on JSVALUE32_64, a JSValueRegs corresponds to a pair of 32-bit
// architectural GPRs. Nevertheless, a lot of the difference between the targets can be abstracted
// over using the following aliases. See AssemblyHelper::noOverlap for catching conflicting register
// aliasing statically.
class JSRInfo {
public:
    // Temporary registers.
    // On JSVALUE64, jsRegT{2*n+1}{2*n} always maps one-to-one to GPR regT{2*n}
    // On JSVALUE32_64, jsRegT{2*n+1}{2*n} always maps to the GPR pair regT{2*n+1} / regT{2*n}
    // This mapping is deliberately simple to ease reasoning about aliasing. E.g.:
    // Seeing 'jsRegT10' indicates that in general both 'regT1' and 'regT0' may be used.
#if USE(JSVALUE64)
    static constexpr JSValueRegs jsRegT10 { GPRInfo::regT0 };
    static constexpr JSValueRegs jsRegT32 { GPRInfo::regT2 };
    static constexpr JSValueRegs jsRegT54 { GPRInfo::regT4 };
#elif USE(JSVALUE32_64)
    static constexpr JSValueRegs jsRegT10 { GPRInfo::regT1, GPRInfo::regT0 };
    static constexpr JSValueRegs jsRegT32 { GPRInfo::regT3, GPRInfo::regT2 };
    static constexpr JSValueRegs jsRegT54 { GPRInfo::regT5, GPRInfo::regT4 };
#endif

    // Return value register
#if USE(JSVALUE64)
    static constexpr JSValueRegs returnValueJSR { GPRInfo::returnValueGPR };
#elif USE(JSVALUE32_64)
    static constexpr JSValueRegs returnValueJSR { GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR };
#endif
};

// The baseline JIT uses "accumulator" style execution with regT0 (for 64-bit)
// and regT0 + regT1 (for 32-bit) serving as the accumulator register(s) for
// passing results of one opcode to the next. Hence:
static_assert(GPRInfo::regT0 == GPRInfo::returnValueGPR);
#if USE(JSVALUE32_64)
static_assert(GPRInfo::regT1 == GPRInfo::returnValueGPR2);
#endif
static_assert(JSRInfo::jsRegT10 == JSRInfo::returnValueJSR);

inline GPRReg extractResult(GPRReg result) { return result; }
#if USE(JSVALUE64)
inline GPRReg extractResult(JSValueRegs result) { return result.gpr(); }
#else
inline JSValueRegs extractResult(JSValueRegs result) { return result; }
#endif
inline NoResultTag extractResult(NoResultTag) { return NoResult; }

// We use this hack to get the GPRInfo from the GPRReg type in templates because our code is bad and we should feel bad..
constexpr GPRInfo toInfoFromReg(GPRReg) { return GPRInfo(); }

class NoOverlapImpl {
    static constexpr unsigned noOverlapImplRegMask(GPRReg gpr)
    {
        if (gpr == InvalidGPRReg)
            return 0ULL;
        unsigned bit = static_cast<unsigned>(gpr);
        RELEASE_ASSERT(bit < countOfBits<uint64_t>);
        return 1ULL << bit;
    }

    // Base case
    template<typename... Args>
    static constexpr bool noOverlapImpl(uint64_t) { return true; }

    // GPRReg case
    template<typename... Args>
    static constexpr bool noOverlapImpl(uint64_t used, GPRReg gpr, Args... args)
    {
        unsigned mask = noOverlapImplRegMask(gpr);
        if (used & mask)
            return false;
        return noOverlapImpl(used | mask, args...);
    }

    // JSValueRegs case
    template<typename... Args>
    static constexpr bool noOverlapImpl(uint64_t used, JSValueRegs jsr, Args... args)
    {
        unsigned mask = noOverlapImplRegMask(jsr.payloadGPR());
#if USE(JSVALUE32_64)
        mask |= noOverlapImplRegMask(jsr.tagGPR());
#endif
        if (used & mask)
            return false;
        return noOverlapImpl(used | mask, args...);
    }

public:
    // Entry point
    template <typename... Args>
    static constexpr bool entry(Args... args) { return noOverlapImpl(0, args...); }
};

// Checks that the given list of GPRRegs and JSValueRegs do not overlap. Use this in static
// assertions to ensure that register aliases live at the same point do not map to the same
// architectural register.
template <typename... Args>
constexpr bool noOverlap(Args... args) { return NoOverlapImpl::entry(args...); }

class PreferredArgumentImpl {
    private:
    template <typename OperationType, unsigned ArgNum>
    static constexpr std::enable_if_t<(FunctionTraits<OperationType>::arity > ArgNum), size_t> sizeOfArg()
    {
        return sizeof(typename FunctionTraits<OperationType>::template ArgumentType<ArgNum>);
    }

#if USE(JSVALUE64)
    template <typename OperationType, unsigned ArgNum, unsigned Index = ArgNum, typename... Args>
    static constexpr JSValueRegs pickJSR(GPRReg first, Args... rest)
    {
        static_assert(sizeOfArg<OperationType, ArgNum - Index>() <= 8, "Don't know how to handle large arguments");
        if constexpr (!Index)
            return JSValueRegs { first };
        else {
            UNUSED_PARAM(first); // Otherwise warning due to constexpr
            return pickJSR<OperationType, ArgNum, Index - 1>(rest...);
        }
    }
#elif USE(JSVALUE32_64)
    template <typename OperationType, unsigned ArgNum, unsigned Index = ArgNum, typename... Args>
    static constexpr JSValueRegs pickJSR(GPRReg first, GPRReg second, GPRReg third, Args... rest)
    {
        constexpr size_t sizeOfCurrentArg = sizeOfArg<OperationType, ArgNum - Index>();
        static_assert(sizeOfCurrentArg <= 8, "Don't know how to handle large arguments");
        if constexpr (!Index) {
            if constexpr (sizeOfCurrentArg <= 4) {
                // Fits in single GPR
                UNUSED_PARAM(second); // Otherwise warning due to constexpr
                UNUSED_PARAM(third); // Otherwise warning due to constexpr
                return JSValueRegs::payloadOnly(first);
            } else if (first == GPRInfo::argumentGPR1 && second == GPRInfo::argumentGPR2 && third == GPRInfo::argumentGPR3) {
                // Wide argument passed in GPRs needs to start with even register number, so skip argumentGPR1
                return JSValueRegs { third, second };
            } else {
                // First is either an even register, or this argument will be pushed to the stack, so it does not matter
                return JSValueRegs { second, first };
            }
        } else {
            if constexpr(sizeOfCurrentArg <= 4) {
                // Fits in single GPR
                UNUSED_PARAM(first); // Otherwise warning due to constexpr
                return pickJSR<OperationType, ArgNum, Index - 1>(second, third, rest...);
            } else if (first == GPRInfo::argumentGPR1 && second == GPRInfo::argumentGPR2 && third == GPRInfo::argumentGPR3) {
                // Wide argument passed in GPRs needs to start with even register number, so skip argumentGPR1, but reuse it later
                return pickJSR<OperationType, ArgNum, Index - 1>(first, rest...);
            } else {
                // First is either an even register, or this argument will be pushed to the stack, so it does not matter
                return pickJSR<OperationType, ArgNum, Index - 1>(third, rest...);
            }
        }
    }

    template <typename OperationType, unsigned ArgNum, unsigned Index = ArgNum, typename... Args>
    static constexpr JSValueRegs pickJSR(GPRReg first, GPRReg second)
    {
        constexpr size_t sizeOfCurrentArg = sizeOfArg<OperationType, ArgNum - Index>();
        static_assert(sizeOfCurrentArg <= 8, "Don't know how to handle large arguments");
        // Base case, 'first' and 'second' are never argument register, or will be pushed on the stack anyway
        if constexpr (!Index) {
            if constexpr (sizeOfCurrentArg <= 4) {
                UNUSED_PARAM(second); // Otherwise warning due to constexpr
                return JSValueRegs::payloadOnly(first);
            } else
                return JSValueRegs { second, first };
        } else {
            if constexpr(sizeOfCurrentArg <= 4) {
                UNUSED_PARAM(first); // Otherwise warning due to constexpr
                return pickJSR<OperationType, ArgNum, Index - 1>(second);
            } else
                RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Out of registers");
        }
    }

    template <typename OperationType, unsigned ArgNum, unsigned Index = ArgNum, typename... Args>
    static constexpr JSValueRegs pickJSR(GPRReg first)
    {
        constexpr size_t sizeOfCurrentArg = sizeOfArg<OperationType, ArgNum - Index>();
        static_assert(sizeOfCurrentArg <= 8, "Don't know how to handle large arguments");
        // Base case, 'first' is never an argument register, or will be pushed on the stack anyway
        if constexpr (sizeOfCurrentArg <= 4)
            return JSValueRegs::payloadOnly(first);
        else
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Out of registers");
    }
#endif

public:
    template <typename OperationType, unsigned ArgNum>
    static constexpr std::enable_if_t<(FunctionTraits<OperationType>::arity > ArgNum), JSValueRegs>
    preferredArgumentJSR()
    {
#if USE(JSVALUE64)
#if !OS(WINDOWS)
        return pickJSR<OperationType, ArgNum>(
            GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2,
            GPRInfo::argumentGPR3, GPRInfo::argumentGPR4, GPRInfo::argumentGPR5);
#else
        return pickJSR<OperationType, ArgNum>(
            GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2,
            GPRInfo::argumentGPR3, GPRInfo::nonArgGPR0,   GPRInfo::nonArgGPR1);
#endif
#elif USE(JSVALUE32_64)
#if CPU(ARM_THUMB2)
        // The last register is guaranteed to be pushed onto the stack for calls, so we can use
        // the link register as a temporary.
        return pickJSR<OperationType, ArgNum>(
            GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2,
            GPRInfo::argumentGPR3, GPRInfo::regT4,        GPRInfo::regT5,
            GPRInfo::regT6,        GPRInfo::regT7,        ARMRegisters::lr);
#elif CPU(MIPS)
        return pickJSR<OperationType, ArgNum>(
            GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2,
            GPRInfo::argumentGPR3, GPRInfo::regT2,        GPRInfo::regT3,
            GPRInfo::regT4,        GPRInfo::regT5,        GPRInfo::regT6);
#endif
#endif
    }

    template <typename OperationType, unsigned ArgNum>
    static constexpr std::enable_if_t<(FunctionTraits<OperationType>::arity > ArgNum), GPRReg>
    preferredArgumentGPR()
    {
#if USE(JSVALUE32_64)
        static_assert(sizeOfArg<OperationType, ArgNum>() <= 4, "Argument does not fit in GPR");
#endif
        return preferredArgumentJSR<OperationType, ArgNum>().payloadGPR();
    }
};

// Computes (statically, at compilation time), the ideal machine register an argument should be
// loaded into for a function call. This yields the ABI specific argument registers for the
// initial arguments as appropriate, then suitable temporary registers for the remaining
// arguments. The idea is that 'setupArguments' will have to do the minimal amount of work when
// using these registers to hold the arguments, so if you are loading most arguments from memory
// anyway, using these registers yields the smallest code required for a call.
template <typename OperationType, unsigned ArgNum>
constexpr std::enable_if_t<(FunctionTraits<OperationType>::arity > ArgNum), GPRReg>
preferredArgumentGPR()
{
    return PreferredArgumentImpl::preferredArgumentGPR<OperationType, ArgNum>();
}

// See preferredArgumentGPR for the purpose of this function. This version returns a JSValueRegs
// instead of a GPR, which on JSVALUE64 are equivalent, but on JSVALUE32_64 a JSValueRegs is
// required to hold a 64-bit wide function argument, so use this in particular when passing a
// JSValue/EncodedJSValue to be compatible with both JSVALUE64 an JSVALUE32_64 platforms, and use
// preferredArgumentGPR when passing host pointers.
template <typename OperationType, unsigned ArgNum>
constexpr std::enable_if_t<(FunctionTraits<OperationType>::arity > ArgNum), JSValueRegs>
preferredArgumentJSR()
{
    return PreferredArgumentImpl::preferredArgumentJSR<OperationType, ArgNum>();
}

#endif // ENABLE(ASSEMBLER)

} // namespace JSC

namespace WTF {

inline void printInternal(PrintStream& out, JSC::GPRReg reg)
{
#if ENABLE(ASSEMBLER)
    out.print("%", JSC::GPRInfo::debugName(reg));
#else
    out.printf("%%r%d", reg);
#endif
}

} // namespace WTF
