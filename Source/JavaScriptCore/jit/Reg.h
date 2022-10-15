/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER)

#include "MacroAssembler.h"

namespace JSC {

// Reg is a polymorphic register class. It can refer to either integer or float registers.
// Here are some use cases:
//
// GPRReg gpr;
// Reg reg = gpr;
// reg.isSet() == true
// reg.isGPR() == true
// reg.isFPR() == false
//
// for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
//     if (reg.isGPR()) {
//     } else /* reg.isFPR() */ {
//     }
// }
//
// The above loop could have also used !!reg or reg.isSet() as a condition.

class Reg {
public:
    constexpr Reg()
        : m_index(invalid())
    {
    }

    constexpr Reg(WTF::HashTableDeletedValueType)
        : m_index(deleted())
    {
    }
    
    constexpr Reg(MacroAssembler::RegisterID reg)
        : m_index(MacroAssembler::registerIndex(reg))
    {
    }
    
    constexpr Reg(MacroAssembler::FPRegisterID reg)
        : m_index(MacroAssembler::registerIndex(reg))
    {
    }
    
    static constexpr Reg fromIndex(unsigned index)
    {
        Reg result;
        result.m_index = index;
        return result;
    }
    
    static constexpr Reg first()
    {
        Reg result;
        result.m_index = 0;
        return result;
    }
    
    static constexpr Reg last()
    {
        Reg result;
        result.m_index = MacroAssembler::numberOfRegisters() + MacroAssembler::numberOfFPRegisters() - 1;
        return result;
    }
    
    constexpr Reg next() const
    {
        ASSERT(!!*this);
        if (*this == last())
            return Reg();
        Reg result;
        result.m_index = m_index + 1;
        return result;
    }
    
    constexpr unsigned index() const { return m_index; }

    static unsigned maxIndex()
    {
        return last().index();
    }
    
    constexpr bool isSet() const { return m_index != invalid(); }
    constexpr explicit operator bool() const { return isSet(); }

    constexpr bool isHashTableDeletedValue() const { return m_index == deleted(); }
    
    constexpr bool isGPR() const
    {
        return m_index < MacroAssembler::numberOfRegisters();
    }
    
    constexpr bool isFPR() const
    {
        return (m_index - MacroAssembler::numberOfRegisters()) < MacroAssembler::numberOfFPRegisters();
    }
    
    MacroAssembler::RegisterID gpr() const
    {
        ASSERT(isGPR());
        return static_cast<MacroAssembler::RegisterID>(MacroAssembler::firstRegister() + m_index);
    }
    
    MacroAssembler::FPRegisterID fpr() const
    {
        ASSERT(isFPR());
        return static_cast<MacroAssembler::FPRegisterID>(
            MacroAssembler::firstFPRegister() + (m_index - MacroAssembler::numberOfRegisters()));
    }
    
    constexpr bool operator==(const Reg& other) const
    {
        return m_index == other.m_index;
    }
    
    constexpr bool operator!=(const Reg& other) const
    {
        return m_index != other.m_index;
    }
    
    constexpr bool operator<(const Reg& other) const
    {
        return m_index < other.m_index;
    }
    
    constexpr bool operator>(const Reg& other) const
    {
        return m_index > other.m_index;
    }
    
    constexpr bool operator<=(const Reg& other) const
    {
        return m_index <= other.m_index;
    }
    
    constexpr bool operator>=(const Reg& other) const
    {
        return m_index >= other.m_index;
    }
    
    constexpr unsigned hash() const
    {
        return m_index;
    }
    
    const char* debugName() const;
    
    void dump(PrintStream&) const;

    class AllRegsIterable {
    public:

        class iterator {
        public:
            iterator() { }

            explicit iterator(Reg reg)
                : m_regIndex(reg.index())
            {
            }

            Reg operator*() const { return Reg::fromIndex(m_regIndex); }

            iterator& operator++()
            {
                m_regIndex = Reg::fromIndex(m_regIndex).next().index();
                return *this;
            }

            bool operator==(const iterator& other) const
            {
                return m_regIndex == other.m_regIndex;
            }

            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }

        private:
            unsigned m_regIndex;
        };

        iterator begin() const { return iterator(Reg::first()); }
        iterator end() const { return iterator(Reg()); }
    };

    static AllRegsIterable all() { return AllRegsIterable(); }

private:
    static constexpr uint8_t invalid() { return (1 << 7) - 1; }

    static constexpr uint8_t deleted() { return invalid() - 1; }
    
    unsigned m_index : 7;
};

struct RegHash {
    static unsigned hash(const Reg& key) { return key.hash(); }
    static bool equal(const Reg& a, const Reg& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

enum class Width : uint8_t {
    Width8,
    Width16,
    Width32,
    Width64,
    Width128,
};
static constexpr Width Width8 = Width::Width8;
static constexpr Width Width16 = Width::Width16;
static constexpr Width Width32 = Width::Width32;
static constexpr Width Width64 = Width::Width64;
static constexpr Width Width128 = Width::Width128;

ALWAYS_INLINE constexpr Width widthForBytes(unsigned bytes)
{
    switch (bytes) {
    case 0:
    case 1:
        return Width8;
    case 2:
        return Width16;
    case 3:
    case 4:
        return Width32;
    case 5:
    case 6:
    case 7:
    case 8:
        return Width64;
    default:
        return Width128;
    }
}

ALWAYS_INLINE constexpr unsigned bytesForWidth(Width width)
{
    switch (width) {
    case Width8:
        return 1;
    case Width16:
        return 2;
    case Width32:
        return 4;
    case Width64:
        return 8;
    case Width128:
        return 16;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

inline constexpr uint64_t mask(Width width)
{
    switch (width) {
    case Width8:
        return 0x00000000000000ffllu;
    case Width16:
        return 0x000000000000ffffllu;
    case Width32:
        return 0x00000000ffffffffllu;
    case Width64:
        return 0xffffffffffffffffllu;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

constexpr Width pointerWidth()
{
    if (sizeof(void*) == 8)
        return Width64;
    return Width32;
}

inline Width canonicalWidth(Width width)
{
    return std::max(Width32, width);
}

inline bool isCanonicalWidth(Width width)
{
    return width >= Width32;
}

ALWAYS_INLINE constexpr Width conservativeWidthWithoutVectors(const Reg reg)
{
    return reg.isFPR() ? Width64 : widthForBytes(sizeof(CPURegister));
}

ALWAYS_INLINE constexpr Width conservativeWidth(const Reg reg)
{
    return reg.isFPR() ? Width64 : widthForBytes(sizeof(CPURegister));
}

ALWAYS_INLINE constexpr unsigned conservativeRegisterBytes(const Reg reg)
{
    return bytesForWidth(conservativeWidth(reg));
}

ALWAYS_INLINE constexpr unsigned conservativeRegisterBytesWithoutVectors(const Reg reg)
{
    return bytesForWidth(conservativeWidthWithoutVectors(reg));
}

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::Reg> : JSC::RegHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::Reg> : SimpleClassHashTraits<JSC::Reg> {
    static constexpr bool emptyValueIsZero = false;
 };

inline void printInternal(PrintStream& out, JSC::Width width)
{
    switch (width) {
    case JSC::Width8:
        out.print("8");
        return;
    case JSC::Width16:
        out.print("16");
        return;
    case JSC::Width32:
        out.print("32");
        return;
    case JSC::Width64:
        out.print("64");
        return;
    case JSC::Width128:
        out.print("128");
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace WTF

#endif // ENABLE(ASSEMBLER)
