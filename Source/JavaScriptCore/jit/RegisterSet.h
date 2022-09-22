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

class RegisterSet {
    friend FrozenRegisterSet;
    friend WholeRegisterSet;

protected:    
    inline void set(Reg reg, Width width)
    {
        ASSERT(!!reg);
        m_bits.set(reg.index());
        
        if (width > Width64 && conservativeWidth(reg) > Width64)
            m_upperBits.set(reg.index());
    }
    
    inline void set(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            set(regs.tagGPR(), Width128);
        set(regs.payloadGPR(), Width128);
    }

    inline void clear(Reg reg)
    {
        ASSERT(!!reg);
        m_bits.clear(reg.index());
        m_upperBits.clear(reg.index());
    }

    inline void clear(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            clear(regs.tagGPR());
        clear(regs.payloadGPR());
    }

public:
    constexpr RegisterSet() { }

    inline RegisterSet(WholeRegisterSet);

    template<typename... Regs>
    constexpr explicit RegisterSet(Regs... regs)
    {
        setMany(regs...);
    }

    bool hasAnyWideRegisters() const { return m_upperBits.count(); }
    bool isEmpty() const { return m_bits.isEmpty(); }

    inline RegisterSet& includeRegister(Reg reg, Width w = Width128) { set(reg, w); return *this; }
    inline RegisterSet& includeRegister(JSValueRegs regs) { set(regs); return *this; }
    inline RegisterSet& excludeRegister(Reg reg) { clear(reg); return *this; }
    inline RegisterSet& excludeRegister(JSValueRegs regs) { clear(regs); return *this; }

    RegisterSet& merge(const RegisterSet& other)
    {
        m_bits.merge(other.m_bits);
        m_upperBits.merge(other.m_upperBits);
        return *this;
    }

    RegisterSet& filter(const RegisterSet& other)
    {
        m_bits.filter(other.m_bits);
        m_upperBits.filter(other.m_upperBits);
        return *this;
    }

    RegisterSet& exclude(const RegisterSet& other)
    {
        m_bits.exclude(other.m_bits);
        m_upperBits.exclude(other.m_upperBits);
        return *this;
    }

    inline WholeRegisterSet whole() const;
    inline size_t numberOfSetRegisters() const;
    inline size_t numberOfSetGPRs() const;
    inline size_t numberOfSetFPRs() const;

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
    
    bool operator==(const RegisterSet& other) const { return m_bits == other.m_bits && m_upperBits == other.m_upperBits; }
    bool operator!=(const RegisterSet& other) const { return m_bits != other.m_bits || m_upperBits != other.m_upperBits; }
    
protected:
    void setAny(Reg reg) { set(reg, Width128); }
    void setAny(JSValueRegs regs) { set(regs); }
    void setAny(const RegisterSet& set) { merge(set); }
    void setMany() { }
    template<typename RegType, typename... Regs>
    void setMany(RegType reg, Regs... regs)
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
    JS_EXPORT_PRIVATE static WholeRegisterSet allGPRs();
    JS_EXPORT_PRIVATE static WholeRegisterSet allFPRs();
    JS_EXPORT_PRIVATE static WholeRegisterSet allRegisters();
    JS_EXPORT_PRIVATE static WholeRegisterSet allScalarRegisters();

    JS_EXPORT_PRIVATE static WholeRegisterSet stackRegisters();
    JS_EXPORT_PRIVATE static WholeRegisterSet reservedHardwareRegisters();
    JS_EXPORT_PRIVATE static WholeRegisterSet macroClobberedRegisters();
    static WholeRegisterSet runtimeTagRegisters();
    static WholeRegisterSet specialRegisters(); // The union of stack, reserved hardware, and runtime registers.
    JS_EXPORT_PRIVATE static WholeRegisterSet calleeSaveRegisters();
    static WholeRegisterSet vmCalleeSaveRegisters(); // Callee save registers that might be saved and used by any tier.
    static RegisterAtOffsetList* vmCalleeSaveRegisterOffsets();
    static WholeRegisterSet llintBaselineCalleeSaveRegisters(); // Registers saved and used by the LLInt.
    static WholeRegisterSet dfgCalleeSaveRegisters(); // Registers saved and used by the DFG JIT.
    static WholeRegisterSet ftlCalleeSaveRegisters(); // Registers that might be saved and used by the FTL JIT.
    static WholeRegisterSet stubUnavailableRegisters(); // The union of callee saves and special registers.
    JS_EXPORT_PRIVATE static WholeRegisterSet argumentGPRS();
    JS_EXPORT_PRIVATE static RegisterSet registersToSaveForJSCall(RegisterSet live);
    JS_EXPORT_PRIVATE static RegisterSet registersToSaveForCCall(RegisterSet live);
};

class JS_EXPORT_PRIVATE WholeRegisterSet {
    friend FrozenRegisterSet;
    friend RegisterSet;
protected:
    inline size_t includes(Reg reg) const
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

    explicit WholeRegisterSet(const RegisterSet& set)
        : m_set(set)
    {
        m_set.m_bits.merge(m_set.m_upperBits);
    }

    inline WholeRegisterSet& includeRegister(Reg reg, Width w = Width128) { m_set.includeRegister(reg, w); m_set.whole(); return *this; }
    inline WholeRegisterSet& includeRegister(JSValueRegs regs) { m_set.includeRegister(regs); m_set.whole(); return *this; }
    inline WholeRegisterSet& excludeRegister(Reg reg) { m_set.excludeRegister(reg); m_set.whole(); return *this; }
    inline WholeRegisterSet& excludeRegister(JSValueRegs regs) { m_set.excludeRegister(regs); m_set.whole(); return *this; }
    void merge(const WholeRegisterSet& other) { m_set.merge(other.m_set); m_set.whole(); }
    void merge(const RegisterSet& other) { merge(other.whole()); }

    inline bool includesRegister(Reg reg, Width width = Width64) const
    {
        if (width > conservativeWidth(reg))
            width = conservativeWidth(reg);
        return includes(reg) >= bytesForWidth(width);
    }

    size_t numberOfSetGPRs() const
    {
        RegisterBitmap temp = m_set.m_bits;
        temp.filter(RegisterSet::allGPRs().m_set.m_bits);
        return temp.count();
    }
    size_t numberOfSetFPRs() const
    {
        RegisterBitmap temp = m_set.m_bits;
        temp.filter(RegisterSet::allFPRs().m_set.m_bits);
        return temp.count();
    }

    size_t numberOfSetRegisters() const
    {
        return m_set.m_bits.count();
    }

    size_t sizeOfSetRegisters() const
    {
        return (m_set.m_bits.count() + m_set.m_upperBits.count()) * 8;
    }

    bool subsumes(const WholeRegisterSet& other) const
    {
        return m_set.m_bits.subsumes(other.m_set.m_bits) && m_set.m_upperBits.subsumes(other.m_set.m_upperBits);
    }

    bool isEmpty() const
    {
        return m_set.isEmpty();
    }

    unsigned hash() const { return m_set.m_bits.hash(); }
    bool operator==(const WholeRegisterSet& other) const { return m_set == other.m_set; }
    bool operator!=(const WholeRegisterSet& other) const { return m_set != other.m_set; }
    void dump(PrintStream& out) const { m_set.dump(out); }
    inline WholeRegisterSet includeWholeRegisterWidth() const;
    
    template<typename Func>
    void forEach(const Func& func) const
    {
        ASSERT(m_set.m_bits.count() >= m_set.m_upperBits.count());
        m_set.m_bits.forEachSetBit(
            [&] (size_t index) {
                ASSERT(m_set.m_bits.get(index) >= m_set.m_upperBits.get(index));
                func(Reg::fromIndex(index));
            });
    }

    template<typename Func>
    void forEachWithWidth(const Func& func) const
    {
        forEach(
            [&] (Reg reg) {
                func(reg, widthForBytes(includes(reg)));
            });
    }
    
    class iterator {
    public:
        iterator() = default;
        
        iterator(const RegisterBitmap::iterator& iter)
            : m_iter(iter)
        {
        }
        
        Reg operator*() const { return Reg::fromIndex(*m_iter); }
        
        iterator& operator++()
        {
            ++m_iter;
            return *this;
        }
        
        bool operator==(const iterator& other) const
        {
            return m_iter == other.m_iter;
        }
        
        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }
        
    private:
        RegisterBitmap::iterator m_iter;
    };
    
    iterator begin() const { return iterator(m_set.m_bits.begin()); }
    iterator end() const { return iterator(m_set.m_bits.end()); }

    bool hasAnyWideRegisters() const { return m_set.hasAnyWideRegisters(); }

private:
    RegisterSet m_set = { };
    
};

RegisterSet::RegisterSet(WholeRegisterSet set)
    : RegisterSet(set.m_set)
{ }

WholeRegisterSet RegisterSet::whole() const
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

WholeRegisterSet WholeRegisterSet::includeWholeRegisterWidth() const
{
    WholeRegisterSet s;
    forEach([&] (Reg r) {
        s.includeRegister(r);
    });
    return s;
}

size_t RegisterSet::numberOfSetRegisters() const { return whole().numberOfSetRegisters(); }
size_t RegisterSet::numberOfSetGPRs() const { return whole().numberOfSetGPRs(); }
size_t RegisterSet::numberOfSetFPRs() const { return whole().numberOfSetFPRs(); }

struct RegisterSetHash {
    static unsigned hash(const WholeRegisterSet& set) { return set.hash(); }
    static bool equal(const WholeRegisterSet& a, const WholeRegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

class FrozenRegisterSet {
public:
    constexpr FrozenRegisterSet() { }

    FrozenRegisterSet(const WholeRegisterSet& registers)
        : m_bits(registers.m_set.m_bits)
    {
    }

    FrozenRegisterSet& operator=(const WholeRegisterSet& r)
    {
        m_bits = r.m_set.m_bits;
        return *this;
    }

    bool operator==(const FrozenRegisterSet& other) const { return m_bits == other.m_bits; }
    bool operator!=(const FrozenRegisterSet& other) const { return m_bits != other.m_bits; }

    WholeRegisterSet set() const { 
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

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::RegisterSet> : JSC::RegisterSetHash { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)
