/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#if !ENABLE(C_LOOP)

#include "FPRInfo.h"
#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "Reg.h"
#include "SimdInfo.h"
#include <wtf/Bitmap.h>
#include <wtf/CommaPrinter.h>

namespace JSC {

class FrozenRegisterSet;
typedef Bitmap<MacroAssembler::numGPRs + MacroAssembler::numFPRs> RegisterBitmap;
class RegisterAtOffsetList;
class WholeRegisterSet;
struct RegisterSetHash;

class RegisterSet {
    friend FrozenRegisterSet;
    friend WholeRegisterSet;
    friend RegisterSetHash;

protected:    
    ALWAYS_INLINE constexpr void set(Reg reg, Width width)
    {
        ASSERT(!!reg);
        m_bits.set(reg.index());
        
        if (width > Width64 && conservativeWidth(reg) > Width64)
            m_upperBits.set(reg.index());
    }
    
    ALWAYS_INLINE constexpr void set(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            set(regs.tagGPR(), Width128);
        set(regs.payloadGPR(), Width128);
    }

    ALWAYS_INLINE constexpr void clear(Reg reg)
    {
        ASSERT(!!reg);
        m_bits.clear(reg.index());
        m_upperBits.clear(reg.index());
    }

    ALWAYS_INLINE constexpr void clear(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            clear(regs.tagGPR());
        clear(regs.payloadGPR());
    }

public:
    constexpr RegisterSet() { }

    ALWAYS_INLINE constexpr RegisterSet(WholeRegisterSet);

    template<typename... Regs>
    ALWAYS_INLINE constexpr explicit RegisterSet(Regs... regs)
    {
        setMany(regs...);
    }

    ALWAYS_INLINE constexpr bool hasAnyWideRegisters() const { return m_upperBits.count(); }
    ALWAYS_INLINE constexpr bool isEmpty() const { return m_bits.isEmpty(); }

    ALWAYS_INLINE constexpr RegisterSet& includeRegister(Reg reg, Width w = Width128) { set(reg, w); return *this; }
    ALWAYS_INLINE constexpr RegisterSet& includeRegister(JSValueRegs regs) { set(regs); return *this; }
    ALWAYS_INLINE constexpr RegisterSet& excludeRegister(Reg reg) { clear(reg); return *this; }
    ALWAYS_INLINE constexpr RegisterSet& excludeRegister(JSValueRegs regs) { clear(regs); return *this; }

    ALWAYS_INLINE constexpr RegisterSet& merge(const RegisterSet& other)
    {
        m_bits.merge(other.m_bits);
        m_upperBits.merge(other.m_upperBits);
        return *this;
    }

    ALWAYS_INLINE constexpr RegisterSet& filter(const RegisterSet& other)
    {
        m_bits.filter(other.m_bits);
        m_upperBits.filter(other.m_upperBits);
        return *this;
    }

    ALWAYS_INLINE constexpr RegisterSet& exclude(const RegisterSet& other)
    {
        m_bits.exclude(other.m_bits);
        m_upperBits.exclude(other.m_upperBits);
        return *this;
    }

    ALWAYS_INLINE constexpr WholeRegisterSet whole() const;
    ALWAYS_INLINE constexpr size_t numberOfSetRegisters() const;
    ALWAYS_INLINE constexpr size_t numberOfSetGPRs() const;
    ALWAYS_INLINE constexpr size_t numberOfSetFPRs() const;

    void dump(PrintStream& out) const
    {
        CommaPrinter comma;
        out.print("[");
        for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
            if (!m_bits.get(reg.index()) && !m_upperBits.get(reg.index()))
                continue;
            out.print(comma, reg);
            if (m_bits.get(reg.index()) && (m_upperBits.get(reg.index()) || conservativeWidth(reg) == Width64))
                continue;
            
            if (m_bits.get(reg.index()))
                out.print("↓");
            else
                out.print("↑");
        }
        out.print("]");
    }
    
    ALWAYS_INLINE constexpr bool operator==(const RegisterSet& other) const { return m_bits == other.m_bits && m_upperBits == other.m_upperBits; }
    ALWAYS_INLINE constexpr bool operator!=(const RegisterSet& other) const { return m_bits != other.m_bits || m_upperBits != other.m_upperBits; }
    
protected:
    ALWAYS_INLINE constexpr void setAny(Reg reg) { set(reg, Width128); }
    ALWAYS_INLINE constexpr void setAny(JSValueRegs regs) { set(regs); }
    ALWAYS_INLINE constexpr void setAny(const RegisterSet& set) { merge(set); }
    ALWAYS_INLINE constexpr void setMany() { }
    template<typename RegType, typename... Regs>
    ALWAYS_INLINE constexpr void setMany(RegType reg, Regs... regs)
    {
        setAny(reg);
        setMany(regs...);
    }

    // These offsets mirror the logic in Reg.h.
    static constexpr unsigned gprOffset = 0;
    static constexpr unsigned fprOffset = gprOffset + MacroAssembler::numGPRs;
    
    RegisterBitmap m_bits = { };
    RegisterBitmap m_upperBits = { };

public:
    ALWAYS_INLINE constexpr static WholeRegisterSet allGPRs();
    ALWAYS_INLINE constexpr static WholeRegisterSet allFPRs();
    ALWAYS_INLINE constexpr static WholeRegisterSet allRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet allScalarRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet stackRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet reservedHardwareRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet macroClobberedRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet runtimeTagRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet specialRegisters(); // The union of stack, reserved hardware, and runtime registers.
    ALWAYS_INLINE constexpr static WholeRegisterSet calleeSaveRegisters();
    ALWAYS_INLINE constexpr static WholeRegisterSet vmCalleeSaveRegisters(); // Callee save registers that might be saved and used by any tier.
    JS_EXPORT_PRIVATE static RegisterAtOffsetList* vmCalleeSaveRegisterOffsets();
    ALWAYS_INLINE constexpr static WholeRegisterSet llintBaselineCalleeSaveRegisters(); // Registers saved and used by the LLInt.
    ALWAYS_INLINE constexpr static WholeRegisterSet dfgCalleeSaveRegisters(); // Registers saved and used by the DFG JIT.
    ALWAYS_INLINE constexpr static WholeRegisterSet ftlCalleeSaveRegisters(); // Registers that might be saved and used by the FTL JIT.
    ALWAYS_INLINE constexpr static WholeRegisterSet stubUnavailableRegisters(); // The union of callee saves and special registers.
    ALWAYS_INLINE constexpr static WholeRegisterSet argumentGPRS();
    ALWAYS_INLINE constexpr static RegisterSet registersToSaveForJSCall(RegisterSet live);
    ALWAYS_INLINE constexpr static RegisterSet registersToSaveForCCall(RegisterSet live);
};

class WholeRegisterSet {
    friend FrozenRegisterSet;
    friend RegisterSet;
    friend RegisterSetHash;
protected:
    ALWAYS_INLINE constexpr size_t includes(Reg reg) const
    {
        ASSERT(!!reg);
        if (!m_set.m_bits.get(reg.index()))
            return 0;
        Width width = Width64;
        if (m_set.m_upperBits.get(reg.index()) && width < conservativeWidth(reg))
            width = conservativeWidth(reg);
        return bytesForWidth(width);
    }

public:
    constexpr WholeRegisterSet() = default;

    constexpr explicit ALWAYS_INLINE WholeRegisterSet(const RegisterSet& set)
        : m_set(set)
    {
        m_set.m_bits.merge(m_set.m_upperBits);
    }

    ALWAYS_INLINE constexpr WholeRegisterSet& includeRegister(Reg reg, Width w = Width128) { m_set.includeRegister(reg, w); m_set.whole(); return *this; }
    ALWAYS_INLINE constexpr WholeRegisterSet& includeRegister(JSValueRegs regs) { m_set.includeRegister(regs); m_set.whole(); return *this; }
    ALWAYS_INLINE constexpr WholeRegisterSet& excludeRegister(Reg reg) { m_set.excludeRegister(reg); m_set.whole(); return *this; }
    ALWAYS_INLINE constexpr WholeRegisterSet& excludeRegister(JSValueRegs regs) { m_set.excludeRegister(regs); m_set.whole(); return *this; }
    ALWAYS_INLINE constexpr void merge(const WholeRegisterSet& other) { m_set.merge(other.m_set); m_set.whole(); }
    ALWAYS_INLINE constexpr void merge(const RegisterSet& other) { merge(other.whole()); }

    ALWAYS_INLINE constexpr bool includesRegister(Reg reg, Width width = Width64) const
    {
        if (width > conservativeWidth(reg))
            width = conservativeWidth(reg);
        return includes(reg) >= bytesForWidth(width);
    }

    ALWAYS_INLINE constexpr size_t numberOfSetGPRs() const
    {
        RegisterBitmap temp = m_set.m_bits;
        temp.filter(RegisterSet::allGPRs().m_set.m_bits);
        return temp.count();
    }

    ALWAYS_INLINE constexpr size_t numberOfSetFPRs() const
    {
        RegisterBitmap temp = m_set.m_bits;
        temp.filter(RegisterSet::allFPRs().m_set.m_bits);
        return temp.count();
    }

    ALWAYS_INLINE constexpr size_t numberOfSetRegisters() const
    {
        return m_set.m_bits.count();
    }

    ALWAYS_INLINE constexpr size_t sizeOfSetRegisters() const
    {
        return (m_set.m_bits.count() + m_set.m_upperBits.count()) * 8;
    }

    ALWAYS_INLINE constexpr bool subsumes(const WholeRegisterSet& other) const
    {
        return m_set.m_bits.subsumes(other.m_set.m_bits) && m_set.m_upperBits.subsumes(other.m_set.m_upperBits);
    }

    ALWAYS_INLINE constexpr bool isEmpty() const
    {
        return m_set.isEmpty();
    }

    ALWAYS_INLINE constexpr bool operator==(const WholeRegisterSet& other) const { return m_set == other.m_set; }
    ALWAYS_INLINE constexpr bool operator!=(const WholeRegisterSet& other) const { return m_set != other.m_set; }
    void dump(PrintStream& out) const { m_set.dump(out); }
    ALWAYS_INLINE constexpr WholeRegisterSet includeWholeRegisterWidth() const;
    
    template<typename Func>
    ALWAYS_INLINE constexpr void forEach(const Func& func) const
    {
        ASSERT(m_set.m_bits.count() >= m_set.m_upperBits.count());
        m_set.m_bits.forEachSetBit(
            [&] (size_t index) {
                ASSERT(m_set.m_bits.get(index) >= m_set.m_upperBits.get(index));
                func(Reg::fromIndex(index));
            });
    }

    template<typename Func>
    ALWAYS_INLINE constexpr void forEachWithWidth(const Func& func) const
    {
        forEach(
            [&] (Reg reg) {
                func(reg, widthForBytes(includes(reg)));
            });
    }
    
    class iterator {
    public:
        ALWAYS_INLINE constexpr iterator() = default;
        
        ALWAYS_INLINE constexpr iterator(const RegisterBitmap::iterator& iter)
            : m_iter(iter)
        {
        }
        
        ALWAYS_INLINE constexpr Reg operator*() const { return Reg::fromIndex(*m_iter); }
        
        iterator& operator++()
        {
            ++m_iter;
            return *this;
        }
        
        ALWAYS_INLINE constexpr bool operator==(const iterator& other) const
        {
            return m_iter == other.m_iter;
        }
        
        ALWAYS_INLINE constexpr bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }
        
    private:
        RegisterBitmap::iterator m_iter;
    };
    
    ALWAYS_INLINE constexpr iterator begin() const { return iterator(m_set.m_bits.begin()); }
    ALWAYS_INLINE constexpr iterator end() const { return iterator(m_set.m_bits.end()); }

    ALWAYS_INLINE constexpr bool hasAnyWideRegisters() const { return m_set.hasAnyWideRegisters(); }

private:
    RegisterSet m_set = { };
};

constexpr RegisterSet::RegisterSet(WholeRegisterSet set)
    : RegisterSet(set.m_set)
{ }

constexpr WholeRegisterSet RegisterSet::whole() const
{
    RegisterSet s;
    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (!m_bits.get(reg.index()) && !m_upperBits.get(reg.index()))
            continue;

        ASSERT(!m_upperBits.get(reg.index()) || m_bits.get(reg.index()));        
        s.includeRegister(reg, m_upperBits.get(reg.index()) ? Width128 : Width64);
    }
    return WholeRegisterSet(s);
}

constexpr WholeRegisterSet WholeRegisterSet::includeWholeRegisterWidth() const
{
    WholeRegisterSet s;
    forEach([&] (Reg r) {
        s.includeRegister(r);
    });
    return s;
}

constexpr size_t RegisterSet::numberOfSetRegisters() const { return whole().numberOfSetRegisters(); }
constexpr size_t RegisterSet::numberOfSetGPRs() const { return whole().numberOfSetGPRs(); }
constexpr size_t RegisterSet::numberOfSetFPRs() const { return whole().numberOfSetFPRs(); }

struct RegisterSetHash {
    static unsigned hash(const WholeRegisterSet& set) { return set.m_set.m_bits.hash(); }
    static bool equal(const WholeRegisterSet& a, const WholeRegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

class FrozenRegisterSet {
public:
    constexpr FrozenRegisterSet() { }

    ALWAYS_INLINE constexpr FrozenRegisterSet(const WholeRegisterSet& registers)
        : m_bits(registers.m_set.m_bits)
    {
    }

    ALWAYS_INLINE FrozenRegisterSet& operator=(const WholeRegisterSet& r)
    {
        m_bits = r.m_set.m_bits;
        return *this;
    }

    ALWAYS_INLINE unsigned hash() const { return m_bits.hash(); }
    ALWAYS_INLINE constexpr bool operator==(const FrozenRegisterSet& other) const { return m_bits == other.m_bits; }
    ALWAYS_INLINE constexpr bool operator!=(const FrozenRegisterSet& other) const { return m_bits != other.m_bits; }

    ALWAYS_INLINE constexpr WholeRegisterSet set() const
    { 
        WholeRegisterSet result;
        m_bits.forEachSetBit(
            [&] (size_t index) {
                result.includeRegister(Reg::fromIndex(index), Width64);
            });
        return result;
    }

private:
    RegisterBitmap m_bits;
};

constexpr WholeRegisterSet RegisterSet::stackRegisters()
{
    return RegisterSet(
        MacroAssembler::stackPointerRegister,
        MacroAssembler::framePointerRegister).whole();
}

constexpr WholeRegisterSet RegisterSet::reservedHardwareRegisters()
{
    WholeRegisterSet result;

#define SET_IF_RESERVED(id, name, isReserved, isCalleeSaved)    \
    if (isReserved)                                             \
        result.includeRegister(RegisterNames::id);
    FOR_EACH_GP_REGISTER(SET_IF_RESERVED)
    FOR_EACH_FP_REGISTER(SET_IF_RESERVED)
#undef SET_IF_RESERVED

    ASSERT(!result.numberOfSetFPRs());

    return result;
}

constexpr WholeRegisterSet RegisterSet::runtimeTagRegisters()
{
#if USE(JSVALUE64)
    return RegisterSet(GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister).whole();
#else
    return { };
#endif
}

constexpr WholeRegisterSet RegisterSet::specialRegisters()
{
    return RegisterSet(
        RegisterSet::stackRegisters(), 
        RegisterSet::reservedHardwareRegisters(), 
        runtimeTagRegisters()).whole();
}

constexpr WholeRegisterSet RegisterSet::stubUnavailableRegisters()
{
    // FIXME: This is overly conservative. We could subtract out those callee-saves that we
    // actually saved.
    // https://bugs.webkit.org/show_bug.cgi?id=185686
    return RegisterSet(specialRegisters(), vmCalleeSaveRegisters()).whole();
}

constexpr WholeRegisterSet RegisterSet::macroClobberedRegisters()
{
#if CPU(X86_64)
    return RegisterSet(MacroAssembler::s_scratchRegister).whole();
#elif CPU(ARM64) || CPU(RISCV64)
    return RegisterSet(MacroAssembler::dataTempRegister, MacroAssembler::memoryTempRegister).whole();
#elif CPU(ARM_THUMB2)
    WholeRegisterSet result;
    result.includeRegister(MacroAssembler::dataTempRegister);
    result.includeRegister(MacroAssembler::addressTempRegister);
    result.includeRegister(MacroAssembler::fpTempRegister);
    return result;
#elif CPU(MIPS)
    WholeRegisterSet result;
    result.includeRegister(MacroAssembler::immTempRegister);
    result.includeRegister(MacroAssembler::dataTempRegister);
    result.includeRegister(MacroAssembler::addrTempRegister);
    result.includeRegister(MacroAssembler::cmpTempRegister);
    return result;
#else
    return { };
#endif
}

constexpr WholeRegisterSet RegisterSet::calleeSaveRegisters()
{
    WholeRegisterSet result;

#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.includeRegister(RegisterNames::id);
    FOR_EACH_GP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED
#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.includeRegister(RegisterNames::id, Width64);
    FOR_EACH_FP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED
        
    return result;
}

constexpr WholeRegisterSet RegisterSet::vmCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if CPU(X86_64)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
#if OS(WINDOWS)
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
#endif
#elif CPU(ARM64)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
    result.includeRegister(GPRInfo::regCS7);
    result.includeRegister(GPRInfo::regCS8);
    result.includeRegister(GPRInfo::regCS9);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
    result.includeRegister(FPRInfo::fpRegCS7, Width64);
#elif CPU(ARM_THUMB2)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    result.includeRegister(FPRInfo::fpRegCS0);
    result.includeRegister(FPRInfo::fpRegCS1);
    result.includeRegister(FPRInfo::fpRegCS2);
    result.includeRegister(FPRInfo::fpRegCS3);
    result.includeRegister(FPRInfo::fpRegCS4);
    result.includeRegister(FPRInfo::fpRegCS5);
    result.includeRegister(FPRInfo::fpRegCS6);
#elif CPU(MIPS)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
#elif CPU(RISCV64)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
    result.includeRegister(GPRInfo::regCS7);
    result.includeRegister(GPRInfo::regCS8);
    result.includeRegister(GPRInfo::regCS9);
    result.includeRegister(GPRInfo::regCS10);
    result.includeRegister(FPRInfo::fpRegCS0);
    result.includeRegister(FPRInfo::fpRegCS1);
    result.includeRegister(FPRInfo::fpRegCS2);
    result.includeRegister(FPRInfo::fpRegCS3);
    result.includeRegister(FPRInfo::fpRegCS4);
    result.includeRegister(FPRInfo::fpRegCS5);
    result.includeRegister(FPRInfo::fpRegCS6);
    result.includeRegister(FPRInfo::fpRegCS7);
    result.includeRegister(FPRInfo::fpRegCS8);
    result.includeRegister(FPRInfo::fpRegCS9);
    result.includeRegister(FPRInfo::fpRegCS10);
    result.includeRegister(FPRInfo::fpRegCS11);
#endif
    return result;
}

constexpr WholeRegisterSet RegisterSet::llintBaselineCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if CPU(X86)
#elif CPU(X86_64)
#if !OS(WINDOWS)
    result.includeRegister(GPRInfo::regCS1);
    static_assert(GPRInfo::regCS2 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
#else
    result.includeRegister(GPRInfo::regCS3);
    static_assert(GPRInfo::regCS4 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS5 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS6 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS4);
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
#endif
#elif CPU(ARM_THUMB2) || CPU(MIPS)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
#elif CPU(ARM64) || CPU(RISCV64)
    result.includeRegister(GPRInfo::regCS6);
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7);
    result.includeRegister(GPRInfo::regCS8);
    result.includeRegister(GPRInfo::regCS9);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

constexpr WholeRegisterSet RegisterSet::dfgCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if CPU(X86)
#elif CPU(X86_64)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
#if !OS(WINDOWS)
    static_assert(GPRInfo::regCS2 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
#else
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    static_assert(GPRInfo::regCS4 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS5 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS6 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS4);
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
#endif
#elif CPU(ARM_THUMB2) || CPU(MIPS)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
#elif CPU(ARM64) || CPU(RISCV64)
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7);
    result.includeRegister(GPRInfo::regCS8);
    result.includeRegister(GPRInfo::regCS9);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

constexpr WholeRegisterSet RegisterSet::ftlCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if ENABLE(FTL_JIT)
#if CPU(X86_64) && !OS(WINDOWS)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    static_assert(GPRInfo::regCS2 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
#elif CPU(ARM64)
    // B3 might save and use all ARM64 callee saves specified in the ABI.
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7);
    result.includeRegister(GPRInfo::regCS8);
    result.includeRegister(GPRInfo::regCS9);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
    result.includeRegister(FPRInfo::fpRegCS7, Width64);
#elif CPU(RISCV64)
    result.includeRegister(GPRInfo::regCS0);
    result.includeRegister(GPRInfo::regCS1);
    result.includeRegister(GPRInfo::regCS2);
    result.includeRegister(GPRInfo::regCS3);
    result.includeRegister(GPRInfo::regCS4);
    result.includeRegister(GPRInfo::regCS5);
    result.includeRegister(GPRInfo::regCS6);
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7);
    result.includeRegister(GPRInfo::regCS8);
    result.includeRegister(GPRInfo::regCS9);
    result.includeRegister(GPRInfo::regCS10);
    result.includeRegister(FPRInfo::fpRegCS0);
    result.includeRegister(FPRInfo::fpRegCS1);
    result.includeRegister(FPRInfo::fpRegCS2);
    result.includeRegister(FPRInfo::fpRegCS3);
    result.includeRegister(FPRInfo::fpRegCS4);
    result.includeRegister(FPRInfo::fpRegCS5);
    result.includeRegister(FPRInfo::fpRegCS6);
    result.includeRegister(FPRInfo::fpRegCS7);
    result.includeRegister(FPRInfo::fpRegCS8);
    result.includeRegister(FPRInfo::fpRegCS9);
    result.includeRegister(FPRInfo::fpRegCS10);
    result.includeRegister(FPRInfo::fpRegCS11);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
#endif
    return result;
}

constexpr WholeRegisterSet RegisterSet::argumentGPRS()
{
    WholeRegisterSet result;
#if NUMBER_OF_ARGUMENT_REGISTERS
    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; i++)
        result.includeRegister(GPRInfo::toArgumentRegister(i));
#endif
    return result;
}

constexpr RegisterSet RegisterSet::registersToSaveForJSCall(RegisterSet liveRegisters)
{
    RegisterSet result = liveRegisters;
    result.exclude(RegisterSet::vmCalleeSaveRegisters());
    result.exclude(RegisterSet::stackRegisters());
    result.exclude(RegisterSet::reservedHardwareRegisters());
    return WholeRegisterSet(result);
}

constexpr RegisterSet RegisterSet::registersToSaveForCCall(RegisterSet liveRegisters)
{
    RegisterSet result = liveRegisters;
    result.exclude(RegisterSet::calleeSaveRegisters());
    result.exclude(RegisterSet::stackRegisters());
    result.exclude(RegisterSet::reservedHardwareRegisters());
    return WholeRegisterSet(result);
}

constexpr WholeRegisterSet RegisterSet::allGPRs()
{
    WholeRegisterSet result;
    for (MacroAssembler::RegisterID reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = static_cast<MacroAssembler::RegisterID>(reg + 1))
        result.includeRegister(reg);
    return result;
}

constexpr WholeRegisterSet RegisterSet::allFPRs()
{
    WholeRegisterSet result;
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = static_cast<MacroAssembler::FPRegisterID>(reg + 1))
        result.includeRegister(reg);
    return result;
}

constexpr WholeRegisterSet RegisterSet::allRegisters()
{
    WholeRegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    return result;
}

constexpr WholeRegisterSet RegisterSet::allScalarRegisters()
{
    WholeRegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    result.m_set.m_upperBits.clearAll();
    return result;
}

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::RegisterSet> : JSC::RegisterSetHash { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)
