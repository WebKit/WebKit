/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if !ENABLE(C_LOOP)

#include <JavaScriptCore/FPRInfo.h>
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/MacroAssembler.h>
#include <JavaScriptCore/MemoryMode.h>
#include <JavaScriptCore/Reg.h>
#include <JavaScriptCore/Width.h>
#include <wtf/BitSet.h>
#include <wtf/CommaPrinter.h>

#include <ranges>

namespace JSC {

class ScalarRegisterSet;
using RegisterBitSet = WTF::BitSet<MacroAssembler::numGPRs + MacroAssembler::numFPRs>;
class RegisterAtOffsetList;
struct ScalarRegisterSetHash;

enum IgnoreVectorsTag { IgnoreVectors };

class RegisterSet final {
    friend ScalarRegisterSet;

public:
    constexpr RegisterSet() = default;

    template<typename... Regs>
    inline constexpr explicit RegisterSet(Regs... regs)
    {
        setMany(regs...);
    }

    inline constexpr RegisterSet(ScalarRegisterSet scalarSet);

    static RegisterSet fromIterable(const std::ranges::range auto& regs)
    {
        RegisterSet result;
        for (auto reg : regs)
            result.add(reg, IgnoreVectors);
        return result;
    }

    inline constexpr bool contains(Reg reg, Width width) const
    {
        if (width < conservativeWidth(reg)) [[likely]]
            return m_bits.get(reg.index());
        if (conservativeWidth(reg) <= conservativeWidthWithoutVectors(reg))
            return m_bits.get(reg.index());
        return m_bits.get(reg.index()) && m_upperBits.get(reg.index());
    }

    inline constexpr bool contains(GPRReg reg) { return contains(Reg(reg), IgnoreVectors); }
    inline constexpr bool contains(Reg reg, IgnoreVectorsTag) const
    {
        return contains(reg, conservativeWidthWithoutVectors(reg));
    }

    inline size_t numberOfSetGPRs() const
    {
        RegisterBitSet temp = m_bits;
        temp.filter(RegisterSet::allGPRs().m_bits);
        return temp.count();
    }

    inline size_t numberOfSetFPRs() const
    {
        RegisterBitSet temp = m_bits;
        temp.filter(RegisterSet::allFPRs().m_bits);
        return temp.count();
    }

    inline constexpr size_t numberOfSetRegisters() const
    {
        return m_bits.count();
    }

    inline size_t byteSizeOfSetRegisters() const
    {
#if CPU(REGISTER64)
        return (m_bits.count() + m_upperBits.count()) * sizeof(CPURegister);
#else
        auto effectiveGPRCount = numberOfSetFPRs()
            ? WTF::roundUpToMultipleOf<2>(numberOfSetGPRs())
            : numberOfSetGPRs();
        return effectiveGPRCount * bytesForWidth(pointerWidth()) + numberOfSetFPRs() * sizeof(double);
#endif
    }

    inline constexpr bool isEmpty() const
    {
        return m_bits.isEmpty() && m_upperBits.isEmpty();
    }

    inline constexpr RegisterSet& includeWholeRegisterWidth()
    {
        m_upperBits.merge(m_bits);
        return *this;
    }

    [[nodiscard]] inline constexpr ScalarRegisterSet toScalarRegisterSet() const;
    [[nodiscard]] inline constexpr ScalarRegisterSet normalizeWidths() const;

    inline constexpr void forEach(const Invocable<void(Reg)> auto& func) const
    {
        m_bits.forEachSetBit(
            [&] (size_t index) {
                func(Reg::fromIndex(index));
            });
    }

    inline constexpr void forEachWithWidth(const Invocable<void(Reg, Width)> auto& func) const
    {
        m_bits.forEachSetBit(
            [&] (size_t index) {
                Reg reg = Reg::fromIndex(index);
                Width includedWidth = m_upperBits.get(index) ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg);
                func(reg, includedWidth);
            });
    }

    inline constexpr void forEachWithWidthAndPreserved(const Invocable<void(Reg, Width, PreservedWidth)> auto& func) const
    {
        auto allBits = m_bits;
        allBits.merge(m_upperBits);
        allBits.forEachSetBit(
            [&] (size_t index) {
                Reg reg = Reg::fromIndex(index);
                Width includedWidth = m_upperBits.get(index) ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg);
                PreservedWidth preservedWidth = PreservesNothing;
                if (!m_bits.get(index))
                    preservedWidth = Preserves64;
                func(reg, includedWidth, preservedWidth);
            });
    }

    class iterator : public RegisterBitSet::iterator {
        WTF_FORBID_HEAP_ALLOCATION;
        using Base = RegisterBitSet::iterator;
    public:
        // FIXME: It seems like these shouldn't be necessary but Clang complains about them missing for the static_casts in begin()/end() below.
        constexpr iterator(const Base& base) : Base(base) { }

        inline constexpr Reg reg() const { return Reg::fromIndex(Base::operator*()); }
        inline constexpr Reg operator*() const { return reg(); }

        inline constexpr bool isGPR() const { return reg().isGPR(); }
        inline constexpr bool isFPR() const { return reg().isFPR(); }

        inline constexpr GPRReg gpr() const { return reg().gpr(); }
        inline constexpr FPRReg fpr() const { return reg().fpr(); }
    };

    inline constexpr iterator begin() const LIFETIME_BOUND { return static_cast<iterator>(m_bits.begin()); }
    inline constexpr iterator end() const LIFETIME_BOUND { return static_cast<iterator>(m_bits.end()); }

    inline constexpr RegisterSet& add(Reg reg, Width width)
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(!!reg);
        m_bits.set(reg.index());

        if (width > conservativeWidthWithoutVectors(reg) && conservativeWidth(reg) > conservativeWidthWithoutVectors(reg)) [[unlikely]]
            m_upperBits.set(reg.index());
        return *this;
    }

    inline constexpr void add(GPRReg reg) { add(Reg(reg), IgnoreVectors); }
    inline constexpr void add(Reg reg, IgnoreVectorsTag)
    {
        add(reg, conservativeWidthWithoutVectors(reg));
    }

    inline constexpr RegisterSet& add(JSValueRegs regs, IgnoreVectorsTag = IgnoreVectors)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            add(regs.tagGPR());
        add(regs.payloadGPR());
        return *this;
    }

    inline constexpr RegisterSet& remove(Reg reg)
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(!!reg);
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

    inline constexpr bool subsumes(const RegisterSet& other) const
    {
        return m_bits.subsumes(other.m_bits) && m_upperBits.subsumes(other.m_upperBits);
    }

    void dump(PrintStream& out) const
    {
        CommaPrinter comma;
        out.print("["_s);
        for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
            if (!m_bits.get(reg.index()) && !m_upperBits.get(reg.index()))
                continue;
            out.print(comma, reg);
            if (m_bits.get(reg.index()) && (m_upperBits.get(reg.index()) || conservativeWidth(reg) == conservativeWidthWithoutVectors(reg)))
                continue;

            if (m_bits.get(reg.index()))
                out.print("↓");
            else
                out.print("↑");
        }
        out.print("]"_s);
    }

    friend constexpr bool operator==(const RegisterSet&, const RegisterSet&) = default;

    JS_EXPORT_PRIVATE static RegisterSet allGPRs();
    JS_EXPORT_PRIVATE static RegisterSet allFPRs();
    JS_EXPORT_PRIVATE static RegisterSet allRegisters();
    JS_EXPORT_PRIVATE static RegisterSet allScalarRegisters();
    JS_EXPORT_PRIVATE static RegisterSet stackRegisters();
    JS_EXPORT_PRIVATE static RegisterSet reservedHardwareRegisters();
    JS_EXPORT_PRIVATE static RegisterSet macroClobberedGPRs();
    JS_EXPORT_PRIVATE static RegisterSet macroClobberedFPRs();
    JS_EXPORT_PRIVATE static RegisterSet runtimeTagRegisters();
    JS_EXPORT_PRIVATE static RegisterSet specialRegisters();
    JS_EXPORT_PRIVATE static RegisterSet calleeSaveRegisters();
    JS_EXPORT_PRIVATE static RegisterSet vmCalleeSaveRegisters();
    JS_EXPORT_PRIVATE static RegisterAtOffsetList* vmCalleeSaveRegisterOffsets();
    JS_EXPORT_PRIVATE static RegisterSet llintBaselineCalleeSaveRegisters();
    JS_EXPORT_PRIVATE static RegisterSet dfgCalleeSaveRegisters();
    JS_EXPORT_PRIVATE static RegisterSet ftlCalleeSaveRegisters();
    JS_EXPORT_PRIVATE static RegisterSet stubUnavailableRegisters();
    JS_EXPORT_PRIVATE static RegisterSet argumentGPRs();
    JS_EXPORT_PRIVATE static RegisterSet argumentFPRs();
#if ENABLE(WEBASSEMBLY)
    JS_EXPORT_PRIVATE static RegisterSet wasmPinnedRegisters();
    JS_EXPORT_PRIVATE static RegisterSet ipintCalleeSaveRegisters();
    JS_EXPORT_PRIVATE static RegisterSet bbqCalleeSaveRegisters();
#endif
    JS_EXPORT_PRIVATE static RegisterSet registersToSaveForJSCall(RegisterSet live);
    JS_EXPORT_PRIVATE static RegisterSet registersToSaveForCCall(RegisterSet live);

private:
    inline constexpr void setAny(Reg reg) { ASSERT_UNDER_CONSTEXPR_CONTEXT(!reg.isFPR()); add(reg, IgnoreVectors); }
    inline constexpr void setAny(JSValueRegs regs) { add(regs, IgnoreVectors); }
    inline constexpr void setAny(const RegisterSet& set) { merge(set); }
    inline constexpr void setMany() { }
    template<typename RegType, typename... Regs>
    inline constexpr void setMany(RegType reg, Regs... regs)
    {
        setAny(reg);
        setMany(regs...);
    }

    RegisterBitSet m_bits = { };
    RegisterBitSet m_upperBits = { };
};

// FIXME: Investigate merging ScalarRegisterSet into RegisterSet as a single
// class template parameterized on whether upper bits are tracked.
class ScalarRegisterSet final {
    friend RegisterSet;

    inline constexpr ScalarRegisterSet(const RegisterSet& registers)
        : m_bits(registers.m_bits)
    { }

    inline constexpr ScalarRegisterSet(RegisterBitSet bits)
        : m_bits(bits)
    { }

public:
    constexpr ScalarRegisterSet() { }

    inline constexpr unsigned hash() const { return m_bits.hash(); }
    inline uint64_t bitsForDebugging() const { return m_bits.storage()[0]; }
    friend constexpr bool operator==(const ScalarRegisterSet&, const ScalarRegisterSet&) = default;

    [[nodiscard]] inline constexpr RegisterSet toRegisterSet() const
    {
        RegisterSet result;
        m_bits.forEachSetBit(
            [&] (size_t index) {
                result.add(Reg::fromIndex(index), conservativeWidthWithoutVectors(Reg::fromIndex(index)));
            });
        return result;
    }

    inline constexpr void add(Reg reg, IgnoreVectorsTag = IgnoreVectors)
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(!!reg);
        m_bits.set(reg.index());
    }

    inline constexpr void add(JSValueRegs regs, IgnoreVectorsTag = IgnoreVectors)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            add(regs.tagGPR());
        add(regs.payloadGPR());
    }

    inline constexpr void remove(Reg reg)
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(!!reg);
        m_bits.clear(reg.index());
    }

    inline constexpr bool contains(Reg reg, IgnoreVectorsTag = IgnoreVectors) const
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(!!reg);
        return m_bits.get(reg.index());
    }

    inline constexpr bool isEmpty() const
    {
        return m_bits.isEmpty();
    }

    inline size_t numberOfSetGPRs() const
    {
        RegisterBitSet temp = m_bits;
        temp.filter(RegisterSet::allGPRs().m_bits);
        return temp.count();
    }

    inline size_t numberOfSetFPRs() const
    {
        RegisterBitSet temp = m_bits;
        temp.filter(RegisterSet::allFPRs().m_bits);
        return temp.count();
    }

    inline constexpr size_t numberOfSetRegisters() const
    {
        return m_bits.count();
    }

    inline constexpr ScalarRegisterSet& merge(const ScalarRegisterSet& other)
    {
        m_bits.merge(other.m_bits);
        return *this;
    }

    inline constexpr ScalarRegisterSet& filter(const ScalarRegisterSet& other)
    {
        m_bits.filter(other.m_bits);
        return *this;
    }

    inline constexpr ScalarRegisterSet& exclude(const ScalarRegisterSet& other)
    {
        m_bits.exclude(other.m_bits);
        return *this;
    }

    inline constexpr bool subsumes(const ScalarRegisterSet& other) const
    {
        return m_bits.subsumes(other.m_bits);
    }

    inline constexpr void forEach(const Invocable<void(Reg)> auto& func) const
    {
        m_bits.forEachSetBit([&] (size_t index) {
            func(Reg::fromIndex(index));
        });
    }

    inline constexpr void forEachReg(const Invocable<void(Reg)> auto& func) const { forEach(func); }

    inline constexpr void forEachWithWidth(const Invocable<void(Reg, Width)> auto& func) const
    {
        m_bits.forEachSetBit(
            [&] (size_t index) {
                Reg reg = Reg::fromIndex(index);
                func(reg, conservativeWidthWithoutVectors(reg));
            });
    }

    using iterator = RegisterSet::iterator;
    inline constexpr iterator begin() const LIFETIME_BOUND { return static_cast<iterator>(m_bits.begin()); }
    inline constexpr iterator end() const LIFETIME_BOUND { return static_cast<iterator>(m_bits.end()); }

    void dump(PrintStream& out) const { toRegisterSet().dump(out); }

private:
    RegisterBitSet m_bits;
};

constexpr RegisterSet::RegisterSet(ScalarRegisterSet scalarSet)
    : m_bits(scalarSet.m_bits)
{ }

constexpr ScalarRegisterSet RegisterSet::toScalarRegisterSet() const
{
    return ScalarRegisterSet(*this);
}

constexpr ScalarRegisterSet RegisterSet::normalizeWidths() const
{
    auto bits = m_bits;
    bits.merge(m_upperBits);
    return ScalarRegisterSet { bits };
}

struct ScalarRegisterSetHash {
    static constexpr unsigned hash(const ScalarRegisterSet& set) { return set.hash(); }
    static constexpr bool equal(const ScalarRegisterSet& a, const ScalarRegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::ScalarRegisterSet> : JSC::ScalarRegisterSetHash { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)
