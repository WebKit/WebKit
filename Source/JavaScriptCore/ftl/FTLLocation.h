/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLLocation_h
#define FTLLocation_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "FPRInfo.h"
#include "FTLStackMaps.h"
#include "GPRInfo.h"
#include <wtf/HashMap.h>

namespace JSC { namespace FTL {

class Location {
public:
    enum Kind {
        Unprocessed,
        Register,
        Indirect,
        Constant
    };
    
    Location()
        : m_kind(Unprocessed)
    {
        u.constant = 0;
    }
    
    Location(WTF::HashTableDeletedValueType)
        : m_kind(Unprocessed)
    {
        u.constant = 1;
    }
    
    static Location forRegister(int16_t dwarfRegNum)
    {
        Location result;
        result.m_kind = Register;
        result.u.variable.dwarfRegNum = dwarfRegNum;
        return result;
    }
    
    static Location forIndirect(int16_t dwarfRegNum, int32_t offset)
    {
        Location result;
        result.m_kind = Indirect;
        result.u.variable.dwarfRegNum = dwarfRegNum;
        result.u.variable.offset = offset;
        return result;
    }
    
    static Location forConstant(int64_t constant)
    {
        Location result;
        result.m_kind = Constant;
        result.u.constant = constant;
        return result;
    }

    static Location forStackmaps(const StackMaps&, const StackMaps::Location&);
    
    Kind kind() const { return m_kind; }
    
    bool hasDwarfRegNum() const { return kind() == Register || kind() == Indirect; }
    int16_t dwarfRegNum() const
    {
        ASSERT(hasDwarfRegNum());
        return u.variable.dwarfRegNum;
    }
    
    bool hasOffset() const { return kind() == Indirect; }
    int32_t offset() const
    {
        ASSERT(hasOffset());
        return u.variable.offset;
    }
    
    bool hasConstant() const { return kind() == Constant; }
    int64_t constant() const
    {
        ASSERT(hasConstant());
        return u.constant;
    }
    
    bool operator!() const { return kind() == Unprocessed && !u.variable.offset; }
    
    bool isHashTableDeletedValue() const { return kind() == Unprocessed && u.variable.offset; }
    
    bool operator==(const Location& other) const
    {
        return m_kind == other.m_kind
            && u.constant == other.u.constant;
    }
    
    unsigned hash() const
    {
        unsigned result = m_kind;
        
        switch (kind()) {
        case Unprocessed:
            result ^= u.variable.offset;
            break;

        case Register:
            result ^= u.variable.dwarfRegNum;
            break;
            
        case Indirect:
            result ^= u.variable.dwarfRegNum;
            result ^= u.variable.offset;
            break;
            
        case Constant:
            result ^= WTF::IntHash<int64_t>::hash(u.constant);
            break;
        }
        
        return WTF::IntHash<unsigned>::hash(result);
    }
    
    void dump(PrintStream&) const;
    
    bool isGPR() const;
    bool involvesGPR() const;
    GPRReg gpr() const;
    
    bool isFPR() const;
    FPRReg fpr() const;
    
    // Assuming that all registers are saved to the savedRegisters buffer according
    // to FTLSaveRestore convention, this loads the value into the given register.
    // The code that this generates isn't exactly super fast. This assumes that FP
    // and SP contain the same values that they would have contained in the original
    // frame. If we did push things onto the stack then probably we'll have to change
    // the signature of this method to take a stack offset for stack-relative
    // indirects.
    void restoreInto(MacroAssembler&, char* savedRegisters, GPRReg result) const;
    
private:
    Kind m_kind;
    union {
        int64_t constant;
        struct {
            int16_t dwarfRegNum;
            int32_t offset;
        } variable;
    } u;
};

struct LocationHash {
    static unsigned hash(const Location& key) { return key.hash(); }
    static bool equal(const Location& a, const Location& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::FTL

namespace WTF {

void printInternal(PrintStream&, JSC::FTL::Location::Kind);

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::FTL::Location> {
    typedef JSC::FTL::LocationHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::FTL::Location> : SimpleClassHashTraits<JSC::FTL::Location> { };

} // namespace WTF

#endif // ENABLE(FTL_JIT)

#endif // FTLLocation_h

