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
#include <wtf/Bitmap.h>
#include <wtf/CommaPrinter.h>

namespace JSC {

typedef Bitmap<MacroAssembler::numGPRs + MacroAssembler::numFPRs> RegisterBitmap;
class RegisterAtOffsetList;

enum IncludedWidth : uint8_t {
    ExcludeRegister = 0,
    IncludeLower64Bits,
    IncludeUpper64Bits,
    IncludeRegister
};
class RegisterSet {
public:
    constexpr RegisterSet() { }

    template<typename... Regs>
    constexpr explicit RegisterSet(Regs... regs)
    {
        setMany(regs...);
    }
    
    JS_EXPORT_PRIVATE static RegisterSet stackRegisters();
    JS_EXPORT_PRIVATE static RegisterSet reservedHardwareRegisters();
    static RegisterSet runtimeTagRegisters();
    static RegisterSet specialRegisters(); // The union of stack, reserved hardware, and runtime registers.
    JS_EXPORT_PRIVATE static RegisterSet calleeSaveRegisters();
    static RegisterSet vmCalleeSaveRegisters(); // Callee save registers that might be saved and used by any tier.
    static RegisterAtOffsetList* vmCalleeSaveRegisterOffsets();
    static RegisterSet llintBaselineCalleeSaveRegisters(); // Registers saved and used by the LLInt.
    static RegisterSet dfgCalleeSaveRegisters(); // Registers saved and used by the DFG JIT.
    static RegisterSet ftlCalleeSaveRegisters(); // Registers that might be saved and used by the FTL JIT.
#if ENABLE(WEBASSEMBLY)
    static RegisterSet webAssemblyCalleeSaveRegisters(); // Registers saved and used by the WebAssembly JIT.
#endif
    JS_EXPORT_PRIVATE static RegisterSet volatileRegistersForJSCall();
    static RegisterSet stubUnavailableRegisters(); // The union of callee saves and special registers.
    JS_EXPORT_PRIVATE static RegisterSet macroScratchRegisters();
    JS_EXPORT_PRIVATE static RegisterSet allGPRs();
    JS_EXPORT_PRIVATE static RegisterSet allFPRs();
    static RegisterSet allRegisters();
    JS_EXPORT_PRIVATE static RegisterSet argumentGPRS();

template<typename Self>
class RegisterSetBase {
public:
    void set(Reg reg, IncludedWidth value = IncludeRegister)
    {
        ASSERT(!!reg);
        if (reg.isGPR()) {
            if (value == IncludeLower64Bits)
                value = IncludeRegister;
            ASSERT(value == ExcludeRegister || value == IncludeRegister);
        }
        static_cast<Self&>(*this).setImpl(reg.index(), value);
    }
    
    void set(JSValueRegs regs, IncludedWidth value = IncludeRegister)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            set(regs.tagGPR(), value);
        set(regs.payloadGPR(), value);
    }
    void clear(JSValueRegs regs) { set(regs, ExcludeRegister); }

    void set(const Self& other, IncludedWidth value = IncludeRegister) { value == ExcludeRegister ? static_cast<Self&>(*this).exclude(other) : static_cast<Self&>(*this).merge(other); }
    void clear(const Self& other) { set(other, ExcludeRegister); }

    void clear(Reg reg)
    {
        ASSERT(!!reg);
        set(reg, ExcludeRegister);
    }
    
    IncludedWidth get(Reg reg) const
    {
        ASSERT(!!reg);
        auto value = static_cast<const Self&>(*this).getImpl(reg.index());
        if (reg.isGPR())
            ASSERT(value == ExcludeRegister || value == IncludeRegister);
        return value;
    }

    template<typename Iterable>
    void setAll(const Iterable& iterable)
    {
        for (Reg reg : iterable)
            set(reg);
    }
    
    // Also allow add/remove/contains terminology, which means the same thing as set/clear/get.
    bool add(Reg reg)
    {
        ASSERT(!!reg);
        return !static_cast<Self&>(*this).testAndSet(reg);
    }
    bool remove(Reg reg)
    {
        ASSERT(!!reg);
        return static_cast<Self&>(*this).testAndClear(reg);
    }
    bool contains(Reg reg) const { return get(reg) != ExcludeRegister; }
    
    size_t numberOfSetGPRs() const
    {
        Self temp = static_cast<const Self&>(*this);
        temp.filter(Self::allGPRs());
        return temp.numberOfSetRegisters();
    }
    size_t numberOfSetFPRs() const
    {
        Self temp = static_cast<const Self&>(*this);
        temp.filter(Self::allFPRs());
        return temp.numberOfSetRegisters();
    }
    size_t numberOfSetRegisters() const { return static_cast<const Self&>(*this).allSet().count(); }
    bool isEmpty() const { return !numberOfSetRegisters(); }
    
    JS_EXPORT_PRIVATE void dump(PrintStream& out) const
    {
        CommaPrinter comma;
        out.print("[");
        for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
            if (get(reg))
                out.print(comma, reg, "(", static_cast<uint8_t>(get(reg)), ")");
        }
        out.print("]");
    }

    bool operator!=(const Self& other) const { return !(static_cast<const Self&>(*this) == other); }
    
    template<typename Func>
    void forEach(const Func& func) const
    {
        static_cast<const Self&>(*this).allSet().forEachSetBit(
            [&] (size_t index) {
                func(Reg::fromIndex(index));
            });
    }
    
    template<typename Func>
    void forEachWithWidth(const Func& func) const
    {
        for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
            auto value = get(reg);
            if (value != ExcludeRegister)
                func(reg, value);
        }
    }
    
    class iterator {
    public:
        iterator()
        {
        }
        
        iterator(const RegisterBitmap& bitmap)
            : m_bitmap(bitmap)
            , m_iterator(m_bitmap.begin())
        {
        }
        
        Reg operator*() const { return Reg::fromIndex(*m_iterator); }
        
        iterator& operator++()
        {
            ++m_iterator;
            return *this;
        }
        
        bool operator==(const iterator& other) const
        {
            return m_iterator == other.m_iterator;
        }
        
        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }
        
        iterator& end()
        {
            m_iterator = m_bitmap.end();
            return *this;
        }
        
    private:
        RegisterBitmap m_bitmap;
        RegisterBitmap::iterator m_iterator;
    };
    
    iterator begin() const { return iterator(static_cast<const Self&>(*this).allSet()); }
    iterator end() const { return iterator(static_cast<const Self&>(*this).allSet()).end(); }
    
protected:
    void setAny(Reg reg) { set(reg); }
    void setAny(JSValueRegs regs) { set(regs); }
    template<typename T>
    typename std::enable_if_t<std::is_base_of<RegisterSetBase<T>, T>::value, void>
    setAny(const T& set) { static_cast<Self&>(*this).merge(set); }
    template<typename T>
    typename std::enable_if_t<std::is_base_of<RegisterSetBase<T>, T>::value, void>
    setAny(T&& set) { static_cast<Self&>(*this).merge(std::forward<T>(set)); }
    void setMany() { }
    template<typename RegType, typename... Regs>
    void setMany(RegType&& reg, Regs... regs)
    {
        setAny(std::forward<RegType>(reg));
        setMany(regs...);
    }

    // These offsets mirror the logic in Reg.h.
    static constexpr unsigned gprOffset = 0;
    static constexpr unsigned fprOffset = gprOffset + MacroAssembler::numGPRs;
};

class RegisterSet : public RegisterSetBase<RegisterSet> {
public:

    JS_EXPORT_PRIVATE static RegisterSet stackRegisters();
    JS_EXPORT_PRIVATE static RegisterSet reservedHardwareRegisters();
    static RegisterSet runtimeTagRegisters();
    static RegisterSet specialRegisters(); // The union of stack, reserved hardware, and runtime registers.
    static RegisterAtOffsetList* vmCalleeSaveRegisterOffsets();
    static RegisterSet llintBaselineCalleeSaveRegisters(); // Registers saved and used by the LLInt.
    static RegisterSet dfgCalleeSaveRegisters(); // Registers saved and used by the DFG JIT.
    static RegisterSet stubUnavailableRegisters(); // The union of callee saves and special registers.
    JS_EXPORT_PRIVATE static RegisterSet macroScratchRegisters();
    JS_EXPORT_PRIVATE static RegisterSet allGPRs();
    JS_EXPORT_PRIVATE static RegisterSet allFPRs();
    static RegisterSet allRegisters();
    JS_EXPORT_PRIVATE static RegisterSet argumentGPRS();

    constexpr RegisterSet() { }

    template<typename... Regs>
    constexpr explicit RegisterSet(Regs&&... regs)
    {
        setMany(std::forward<Regs>(regs)...);
    }

    bool operator==(const RegisterSet& other) const { return m_includeLower64Bits == other.m_includeLower64Bits; }
    unsigned hash() const { return m_includeLower64Bits.hash(); }

    void merge(const RegisterSet& other)
    {
        m_includeLower64Bits.merge(other.m_includeLower64Bits);
    }

    void filter(const RegisterSet& other)
    {
        m_includeLower64Bits.filter(other.m_includeLower64Bits);
    }

    void exclude(const RegisterSet& other)
    {
        m_includeLower64Bits.exclude(other.m_includeLower64Bits);
    }

    bool subsumes(const RegisterSet& other) const
    {
        return m_includeLower64Bits.subsumes(other.m_includeLower64Bits);
    }

protected:
    RegisterBitmap allSet() const { return m_includeLower64Bits; }

    void setImpl(unsigned index, IncludedWidth value = IncludeRegister)
    {
        ASSERT(value == IncludeRegister || value == ExcludeRegister);
        m_includeLower64Bits.set(index, value & 0b01);
    }

    IncludedWidth getImpl(unsigned index) const { return static_cast<IncludedWidth>((m_includeLower64Bits.get(index) << 1) | m_includeLower64Bits.get(index)); }

    bool testAndSet(Reg reg) { return m_includeLower64Bits.testAndSet(reg.index()); }
    bool testAndClear(Reg reg) { return m_includeLower64Bits.testAndClear(reg.index()); }

    RegisterBitmap m_includeLower64Bits;

    friend RegisterSetBase<RegisterSet>;
};

static_assert(sizeof(RegisterSet) <= sizeof(uint64_t));

class RegisterSet128 : public RegisterSetBase<RegisterSet128> {
public:
    constexpr RegisterSet128() { }

    template<typename... Regs>
    constexpr explicit RegisterSet128(Regs&&... regs)
    {
        setMany(std::forward<Regs>(regs)...);
    }

    static RegisterSet128 use128Bits(RegisterSet registerSet)
    {
        RegisterSet128 result;
        registerSet.forEach([&] (Reg reg) {
            result.set(reg);
        });
        return result;
    }

    static RegisterSet128 use64Bits(RegisterSet registerSet)
    {
        RegisterSet128 result;
        registerSet.forEach([&] (Reg reg) {
            result.set(reg, reg.isFPR() ? IncludeLower64Bits : IncludeRegister);
        });
        return result;
    }

    static RegisterSet128 allGPRs() { return use128Bits(RegisterSet::allGPRs()); }
    static RegisterSet128 allFPRs() { return use128Bits(RegisterSet::allFPRs()); }
    JS_EXPORT_PRIVATE static RegisterSet128 calleeSaveRegisters();
    static RegisterSet128 vmCalleeSaveRegisters(); // Callee save registers that might be saved and used by any tier.
    static RegisterSet128 ftlCalleeSaveRegisters(); // Registers that might be saved and used by the FTL JIT.
    static RegisterSet128 volatileRegistersForJSCall();
    static RegisterSet128 registersToNotSaveForJSCall();
    static RegisterSet128 registersToNotSaveForCCall();
#if ENABLE(WEBASSEMBLY)
    static RegisterSet128 webAssemblyCalleeSaveRegisters(); // Registers saved and used by the WebAssembly JIT.
#endif

    bool operator==(const RegisterSet128& other) const { return m_includeLower64Bits == other.m_includeLower64Bits && m_includeUpper64Bits == other.m_includeUpper64Bits; }
    unsigned hash() const { return m_includeLower64Bits.hash(); }
    
    void merge(Reg reg, IncludedWidth value)
    {
        ASSERT(!!reg);
        set(reg, static_cast<IncludedWidth>(value | get(reg)));
    }

    void merge(const RegisterSet128& other)
    {
        m_includeLower64Bits.merge(other.m_includeLower64Bits);
        m_includeUpper64Bits.merge(other.m_includeUpper64Bits);
    }
    
    void merge(const RegisterSet& other)
    {
        merge(use128Bits(other));
    }

    void filter(const RegisterSet128& other)
    {
        m_includeLower64Bits.filter(other.m_includeLower64Bits);
        m_includeUpper64Bits.filter(other.m_includeUpper64Bits);
    }

    void exclude(const RegisterSet128& other)
    {
        m_includeLower64Bits.exclude(other.m_includeLower64Bits);
        m_includeUpper64Bits.exclude(other.m_includeUpper64Bits);
    }

    bool subsumes(const RegisterSet128& other) const
    {
        return m_includeLower64Bits.subsumes(other.m_includeLower64Bits) && m_includeUpper64Bits.subsumes(other.m_includeUpper64Bits);
    }
    
    RegisterSet allRegisters() const {
        RegisterSet result;
        forEach([&] (Reg reg) {
            result.set(reg);
        });
        return result;
    }
    
    RegisterSet allFullRegisters() const {
        RegisterSet result;
        forEach([&] (Reg reg) {
            if (get(reg) == IncludeRegister)
                result.set(reg);
        });
        return result;
    }

protected:
    RegisterBitmap allSet() const
    {
        RegisterBitmap all(m_includeLower64Bits);
        all.merge(m_includeUpper64Bits);
        return all;
    }

    void setImpl(unsigned index, IncludedWidth value = IncludeRegister)
    {
        m_includeLower64Bits.set(index, value & 0b01);
        m_includeUpper64Bits.set(index, value & 0b10);
    }

    IncludedWidth getImpl(unsigned index) const { return static_cast<IncludedWidth>((m_includeUpper64Bits.get(index) << 1) | m_includeLower64Bits.get(index)); }

    bool testAndSet(Reg reg) { return m_includeLower64Bits.testAndSet(reg.index()) | m_includeUpper64Bits.testAndSet(reg.index()); }
    bool testAndClear(Reg reg) { return m_includeLower64Bits.testAndClear(reg.index()) | m_includeUpper64Bits.testAndClear(reg.index()); }

    RegisterBitmap m_includeLower64Bits;
    RegisterBitmap m_includeUpper64Bits;

    friend RegisterSetBase<RegisterSet128>;
};

static_assert(sizeof(RegisterSet128) <= 2*sizeof(uint64_t));

struct RegisterSetHash {
    static unsigned hash(const RegisterSet& set) { return set.hash(); }
    static bool equal(const RegisterSet& a, const RegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::RegisterSet> : JSC::RegisterSetHash { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)
