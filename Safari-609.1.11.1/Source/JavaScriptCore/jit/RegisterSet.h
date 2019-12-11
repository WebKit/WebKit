/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "TempRegisterSet.h"
#include <wtf/Bitmap.h>

namespace JSC {

typedef Bitmap<MacroAssembler::numGPRs + MacroAssembler::numFPRs + 1> RegisterBitmap;
class RegisterAtOffsetList;

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
    static RegisterSet volatileRegistersForJSCall();
    static RegisterSet stubUnavailableRegisters(); // The union of callee saves and special registers.
    JS_EXPORT_PRIVATE static RegisterSet macroScratchRegisters();
    JS_EXPORT_PRIVATE static RegisterSet allGPRs();
    JS_EXPORT_PRIVATE static RegisterSet allFPRs();
    static RegisterSet allRegisters();
    JS_EXPORT_PRIVATE static RegisterSet argumentGPRS();

    static RegisterSet registersToNotSaveForJSCall();
    static RegisterSet registersToNotSaveForCCall();
    
    void set(Reg reg, bool value = true)
    {
        ASSERT(!!reg);
        m_bits.set(reg.index(), value);
    }
    
    void set(JSValueRegs regs, bool value = true)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            set(regs.tagGPR(), value);
        set(regs.payloadGPR(), value);
    }

    void set(const RegisterSet& other, bool value = true) { value ? merge(other) : exclude(other); }

    void clear(Reg reg)
    {
        ASSERT(!!reg);
        set(reg, false);
    }
    
    bool get(Reg reg) const
    {
        ASSERT(!!reg);
        return m_bits.get(reg.index());
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
        return !m_bits.testAndSet(reg.index());
    }
    bool remove(Reg reg)
    {
        ASSERT(!!reg);
        return m_bits.testAndClear(reg.index());
    }
    bool contains(Reg reg) const { return get(reg); }
    
    void merge(const RegisterSet& other) { m_bits.merge(other.m_bits); }
    void filter(const RegisterSet& other) { m_bits.filter(other.m_bits); }
    void exclude(const RegisterSet& other) { m_bits.exclude(other.m_bits); }
    
    bool subsumes(const RegisterSet& other) const { return m_bits.subsumes(other.m_bits); }
    
    size_t numberOfSetGPRs() const;
    size_t numberOfSetFPRs() const;
    size_t numberOfSetRegisters() const { return m_bits.count(); }
    
    bool isEmpty() const { return m_bits.isEmpty(); }
    
    JS_EXPORT_PRIVATE void dump(PrintStream&) const;
    
    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    
    RegisterSet(EmptyValueTag)
    {
        m_bits.set(hashSpecialBitIndex);
    }
    
    RegisterSet(DeletedValueTag)
    {
        m_bits.set(hashSpecialBitIndex);
        m_bits.set(deletedBitIndex);
    }
    
    bool isEmptyValue() const
    {
        return m_bits.get(hashSpecialBitIndex) && !m_bits.get(deletedBitIndex);
    }
    
    bool isDeletedValue() const
    {
        return m_bits.get(hashSpecialBitIndex) && m_bits.get(deletedBitIndex);
    }
    
    bool operator==(const RegisterSet& other) const { return m_bits == other.m_bits; }
    bool operator!=(const RegisterSet& other) const { return m_bits != other.m_bits; }
    
    unsigned hash() const { return m_bits.hash(); }
    
    template<typename Func>
    void forEach(const Func& func) const
    {
        m_bits.forEachSetBit(
            [&] (size_t index) {
                func(Reg::fromIndex(index));
            });
    }
    
    class iterator {
    public:
        iterator()
        {
        }
        
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
        
        bool operator==(const iterator& other)
        {
            return m_iter == other.m_iter;
        }
        
        bool operator!=(const iterator& other)
        {
            return !(*this == other);
        }
        
    private:
        RegisterBitmap::iterator m_iter;
    };
    
    iterator begin() const { return iterator(m_bits.begin()); }
    iterator end() const { return iterator(m_bits.end()); }
    
private:
    void setAny(Reg reg) { set(reg); }
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
    static constexpr unsigned hashSpecialBitIndex = fprOffset + MacroAssembler::numFPRs;
    static constexpr unsigned deletedBitIndex = 0;
    
    RegisterBitmap m_bits;
};

struct RegisterSetHash {
    static unsigned hash(const RegisterSet& set) { return set.hash(); }
    static bool equal(const RegisterSet& a, const RegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::RegisterSet> {
    typedef JSC::RegisterSetHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::RegisterSet> : public CustomHashTraits<JSC::RegisterSet> { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)
