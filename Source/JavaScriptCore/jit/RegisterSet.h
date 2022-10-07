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

#include "wtf/Compiler.h"
#if !ENABLE(C_LOOP)

#include "FPRInfo.h"
#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "Reg.h"
#include "SIMDInfo.h"
#include <wtf/Bitmap.h>
#include <wtf/CommaPrinter.h>

namespace JSC {

class SmallRegisterSet;
typedef Bitmap<MacroAssembler::numGPRs + MacroAssembler::numFPRs> RegisterBitmap;
class RegisterAtOffsetList;
struct RegisterSetHash;
class WholeRegisterSet;

class RegisterSet final {
    friend SmallRegisterSet;
    friend WholeRegisterSet;

public:
    constexpr RegisterSet() { }

    inline constexpr RegisterSet(WholeRegisterSet);

    template<typename... Regs>
    inline constexpr explicit RegisterSet(Regs... regs)
    {
        setMany(regs...);
    }

    inline constexpr RegisterSet& add(Reg reg, Width width)
    {
        ASSERT(!!reg);
        m_bits.set(reg.index());

        if (UNLIKELY(width > Width64 && conservativeWidth(reg) > Width64))
            m_upperBits.set(reg.index());
        return *this;
    }

    inline constexpr RegisterSet& add(JSValueRegs regs, Width width)
    {
        ASSERT_UNUSED(width, width == Width64);
        if (regs.tagGPR() != InvalidGPRReg)
            add(regs.tagGPR(), Width64);
        add(regs.payloadGPR(), Width64);
        return *this;
    }

    inline constexpr RegisterSet& remove(Reg reg)
    {
        ASSERT(!!reg);
        m_bits.clear(reg.index());
        m_upperBits.clear(reg.index());
        return *this;
    }

    inline constexpr RegisterSet& remove(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            remove(regs.tagGPR());
        remove(regs.payloadGPR());
        return *this;
    }

    inline constexpr bool hasAnyWideRegisters() const { return m_upperBits.count(); }
    inline constexpr bool isEmpty() const { return m_bits.isEmpty() && m_upperBits.isEmpty(); }

    inline constexpr RegisterSet& merge(const RegisterSet& other)
    {
        m_bits.merge(other.m_bits);
        m_upperBits.merge(other.m_upperBits);
        return *this;
    }

    inline constexpr RegisterSet& filter(const RegisterSet& other)
    {
        m_bits.filter(other.m_bits);
        m_upperBits.filter(other.m_upperBits);
        return *this;
    }

    inline constexpr RegisterSet& exclude(const RegisterSet& other)
    {
        m_bits.exclude(other.m_bits);
        m_upperBits.exclude(other.m_upperBits);
        return *this;
    }

    inline constexpr WholeRegisterSet whole() const WARN_UNUSED_RETURN;
    inline constexpr size_t numberOfSetRegisters() const;
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
            if (m_bits.get(reg.index()) && (m_upperBits.get(reg.index()) || conservativeWidth(reg) <= Width64))
                continue;

            if (m_bits.get(reg.index()))
                out.print("↓");
            else
                out.print("↑");
        }
        out.print("]");
    }

    inline constexpr bool operator==(const RegisterSet& other) const { return m_bits == other.m_bits && m_upperBits == other.m_upperBits; }
    inline constexpr bool operator!=(const RegisterSet& other) const { return m_bits != other.m_bits || m_upperBits != other.m_upperBits; }

protected:
    inline constexpr void setAny(Reg reg) { ASSERT(!reg.isFPR()); add(reg, Width128); }
    inline constexpr void setAny(JSValueRegs regs) { add(regs, Width64); }
    inline constexpr void setAny(const RegisterSet& set) { merge(set); }
    inline constexpr void setMany() { }
    template<typename RegType, typename... Regs>
    inline constexpr void setMany(RegType reg, Regs... regs)
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
    JS_EXPORT_PRIVATE static WholeRegisterSet runtimeTagRegisters();
    JS_EXPORT_PRIVATE static WholeRegisterSet specialRegisters(); // The union of stack, reserved hardware, and runtime registers.
    JS_EXPORT_PRIVATE static WholeRegisterSet calleeSaveRegisters();
    JS_EXPORT_PRIVATE static WholeRegisterSet vmCalleeSaveRegisters(); // Callee save registers that might be saved and used by any tier.
    JS_EXPORT_PRIVATE static RegisterAtOffsetList* vmCalleeSaveRegisterOffsets();
    JS_EXPORT_PRIVATE static WholeRegisterSet llintBaselineCalleeSaveRegisters(); // Registers saved and used by the LLInt.
    JS_EXPORT_PRIVATE static WholeRegisterSet dfgCalleeSaveRegisters(); // Registers saved and used by the DFG JIT.
    JS_EXPORT_PRIVATE static WholeRegisterSet ftlCalleeSaveRegisters(); // Registers that might be saved and used by the FTL JIT.
    JS_EXPORT_PRIVATE static WholeRegisterSet stubUnavailableRegisters(); // The union of callee saves and special registers.
    JS_EXPORT_PRIVATE static WholeRegisterSet argumentGPRS();
    JS_EXPORT_PRIVATE static RegisterSet registersToSaveForJSCall(RegisterSet live);
    JS_EXPORT_PRIVATE static RegisterSet registersToSaveForCCall(RegisterSet live);
};

class WholeRegisterSet final {
    friend SmallRegisterSet;
    friend RegisterSet;
public:
    constexpr WholeRegisterSet() = default;

    constexpr explicit inline WholeRegisterSet(const RegisterSet& set)
        : m_bits(set.m_bits)
        , m_upperBits(set.m_upperBits)
    {
        m_bits.merge(m_upperBits);
    }

    inline constexpr bool contains(Reg reg, Width width) const
    {
        ASSERT(m_bits.count() >= m_upperBits.count());
        if (LIKELY(width <= Width64) || conservativeWidth(reg) <= Width64)
            return m_bits.get(reg.index());
        return m_bits.get(reg.index()) && m_upperBits.get(reg.index());
    }

    inline size_t numberOfSetGPRs() const
    {
        RegisterBitmap temp = m_bits;
        temp.filter(RegisterSet::allGPRs().m_bits);
        return temp.count();
    }

    inline size_t numberOfSetFPRs() const
    {
        RegisterBitmap temp = m_bits;
        temp.filter(RegisterSet::allFPRs().m_bits);
        return temp.count();
    }

    inline constexpr size_t numberOfSetRegisters() const
    {
        ASSERT(m_bits.count() >= m_upperBits.count());
        return m_bits.count();
    }

    inline constexpr size_t sizeOfSetRegisters() const
    {
#if USE(JSVALUE64)
        return (m_bits.count() + m_upperBits.count()) * sizeof(CPURegister);
#else
        return numberOfSetGPRs() * pointerWidth() + numberOfSetFPRs() * sizeof(double);
#endif
    }

    inline constexpr bool subsumes(const WholeRegisterSet& other) const
    {
        return m_bits.subsumes(other.m_bits) && m_upperBits.subsumes(other.m_upperBits);
    }

    inline constexpr bool isEmpty() const
    {
        ASSERT(m_bits.count() >= m_upperBits.count());
        return m_bits.isEmpty();
    }

    inline constexpr WholeRegisterSet& includeWholeRegisterWidth()
    {
        m_upperBits.merge(m_bits);
        return *this;
    }

    template<typename Func>
    inline constexpr void forEach(const Func& func) const
    {
        ASSERT(m_bits.count() >= m_upperBits.count());
        m_bits.forEachSetBit(
            [&] (size_t index) {
                ASSERT(m_bits.get(index) >= m_upperBits.get(index));
                func(Reg::fromIndex(index));
            });
    }

    template<typename Func>
    inline constexpr void forEachWithWidth(const Func& func) const
    {
        ASSERT(m_bits.count() >= m_upperBits.count());
        m_bits.forEachSetBit(
            [&] (size_t index) {
                ASSERT(m_bits.get(index) >= m_upperBits.get(index));
                Reg reg = Reg::fromIndex(index);
                Width includedWidth = (conservativeWidth(reg) <= Width64 || !m_upperBits.get(index)) ? Width64 : Width128;
                func(reg, includedWidth);
            });
    }

    class iterator {
    public:
        inline constexpr iterator() = default;

        inline constexpr iterator(const RegisterBitmap::iterator& iter)
            : m_iter(iter)
        {
        }

        inline constexpr Reg operator*() const { return Reg::fromIndex(*m_iter); }

        iterator& operator++()
        {
            ++m_iter;
            return *this;
        }

        inline constexpr bool operator==(const iterator& other) const
        {
            return m_iter == other.m_iter;
        }

        inline constexpr bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        RegisterBitmap::iterator m_iter;
    };

    inline constexpr iterator begin() const { return iterator(m_bits.begin()); }
    inline constexpr iterator end() const { return iterator(m_bits.end()); }

    inline constexpr WholeRegisterSet& add(Reg reg, Width width)
    {
        ASSERT(!!reg);
        m_bits.set(reg.index());

#if USE(JSVALUE64)
        if (UNLIKELY(width > Width64 && conservativeWidth(reg) > Width64))
            m_upperBits.set(reg.index());
#endif
        return *this;
    }

    inline constexpr WholeRegisterSet& add(JSValueRegs regs, Width width)
    {
        ASSERT_UNUSED(width, width == Width64);
        if (regs.tagGPR() != InvalidGPRReg)
            add(regs.tagGPR(), Width64);
        add(regs.payloadGPR(), Width64);
        return *this;
    }

    inline constexpr WholeRegisterSet& remove(Reg reg)
    {
        ASSERT(!!reg);
        m_bits.clear(reg.index());
        m_upperBits.clear(reg.index());
        return *this;
    }

    inline constexpr WholeRegisterSet& remove(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            remove(regs.tagGPR());
        remove(regs.payloadGPR());
        return *this;
    }

    inline constexpr bool hasAnyWideRegisters() const { return m_upperBits.count(); }

    inline constexpr WholeRegisterSet& merge(const WholeRegisterSet& other)
    {
        m_bits.merge(other.m_bits);
        m_upperBits.merge(other.m_upperBits);
        ASSERT(m_bits.count() >= m_upperBits.count());
        return *this;
    }

    void dump(PrintStream& out) const
    {
        CommaPrinter comma;
        out.print("[");
        for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
            if (!m_bits.get(reg.index()) && !m_upperBits.get(reg.index()))
                continue;
            out.print(comma, reg);
            if (m_bits.get(reg.index()) && (m_upperBits.get(reg.index()) || conservativeWidth(reg) <= Width64))
                continue;

            if (m_bits.get(reg.index()))
                out.print("↓");
            else
                out.print("↑");
        }
        out.print("]");
    }

    inline constexpr bool operator==(const WholeRegisterSet& other) const { return m_bits == other.m_bits && m_upperBits == other.m_upperBits; }
    inline constexpr bool operator!=(const WholeRegisterSet& other) const { return m_bits != other.m_bits || m_upperBits != other.m_upperBits; }

private:
    RegisterBitmap m_bits = { };
    RegisterBitmap m_upperBits = { };
};

constexpr RegisterSet::RegisterSet(WholeRegisterSet set)
    : m_bits(set.m_bits)
    , m_upperBits(set.m_upperBits)
{ }

constexpr WholeRegisterSet RegisterSet::whole() const
{
#if ASSERT_ENABLED
    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (!m_bits.get(reg.index()) && !m_upperBits.get(reg.index()))
            continue;

        ASSERT(!m_upperBits.get(reg.index()) || m_bits.get(reg.index()));
    }
#endif
    return WholeRegisterSet(*this);
}

constexpr size_t RegisterSet::numberOfSetRegisters() const { return whole().numberOfSetRegisters(); }
size_t RegisterSet::numberOfSetGPRs() const { return whole().numberOfSetGPRs(); }
size_t RegisterSet::numberOfSetFPRs() const { return whole().numberOfSetFPRs(); }

class SmallRegisterSet final {
public:
    constexpr SmallRegisterSet() { }

    inline constexpr SmallRegisterSet(const WholeRegisterSet& registers)
        : m_bits(registers.m_bits)
    {
    }

    inline SmallRegisterSet& operator=(const WholeRegisterSet& r)
    {
        m_bits = r.m_bits;
        return *this;
    }

    inline constexpr unsigned hash() const { return m_bits.hash(); }
    inline constexpr bool operator==(const SmallRegisterSet& other) const { return m_bits == other.m_bits; }
    inline constexpr bool operator!=(const SmallRegisterSet& other) const { return m_bits != other.m_bits; }

    inline constexpr WholeRegisterSet set() const WARN_UNUSED_RETURN
    {
        WholeRegisterSet result;
        m_bits.forEachSetBit(
            [&] (size_t index) {
                result.add(Reg::fromIndex(index), Width64);
            });
        return result;
    }

    inline constexpr void add(Reg reg, Width width)
    {
        ASSERT(!!reg);
        ASSERT_UNUSED(width, width == Width64);
        m_bits.set(reg.index());
    }

    inline constexpr void add(JSValueRegs regs, Width width)
    {
        ASSERT_UNUSED(width, width == Width64);
        if (regs.tagGPR() != InvalidGPRReg)
            add(regs.tagGPR(), Width64);
        add(regs.payloadGPR(), Width64);
    }

    inline constexpr void remove(Reg reg)
    {
        ASSERT(!!reg);
        m_bits.clear(reg.index());
    }

    inline constexpr bool contains(Reg reg, Width width) const
    {
        ASSERT_UNUSED(width, width == Width64);
        if (UNLIKELY(width > Width64))
            return false;
        return m_bits.get(reg.index());
    }

    inline size_t numberOfSetGPRs() const
    {
        RegisterBitmap temp = m_bits;
        temp.filter(RegisterSet::allGPRs().m_bits);
        return temp.count();
    }

    inline size_t numberOfSetFPRs() const
    {
        RegisterBitmap temp = m_bits;
        temp.filter(RegisterSet::allFPRs().m_bits);
        return temp.count();
    }

    inline constexpr size_t numberOfSetRegisters() const
    {
        return m_bits.count();
    }

private:
    RegisterBitmap m_bits;
};

struct RegisterSetHash {
    static constexpr unsigned hash(const SmallRegisterSet& set) { return set.hash(); }
    static constexpr bool equal(const SmallRegisterSet& a, const SmallRegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::SmallRegisterSet> : JSC::RegisterSetHash { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)
