/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef B3Kind_h
#define B3Kind_h

#if ENABLE(B3_JIT)

#include "B3Opcode.h"
#include <wtf/HashTable.h>
#include <wtf/PrintStream.h>

namespace JSC { namespace B3 {

// A Kind is a terse summary of what a Value does. There is a fixed number of possible
// Kinds. Kind is a tuple of Opcode (see B3Opcode.h) and some extra bits. Most opcodes don't
// get any extra bits, and those bits must remain zero if the Kind's opcode field is set to
// one of those opcodes. The purpose of Kind is to be like an opcode in other IRs, but to
// be multidimensional. For example, a Load has many dimensions of customization that we may
// eventually implement. A Load can have different alignments, alignment failure modes,
// temporality modes, trapping modes, ordering modes, etc. It's fine to put such flags into
// subclasses of Value, but in some cases that would be overkill, particularly since if you
// did that for a pure value then you'd also have to thread it through ValueKey. It's much
// easier to put it in Kind, and then your extra bit will get carried around by everyone who
// knows how to carry around Kinds. Most importantly, putting flags into Kind allows you to
// use them as part of B3::Value's dynamic cast facility. For example we could have a
// trapping Load that uses a Value subclass that has a stackmap while non-trapping Loads
// continue to use the normal MemoryValue.
//
// Note that any code in the compiler that transcribes IR (like a strength reduction that
// replaces an Add with a different Add, or even with a different opcode entirely) will
// probably drop unknown bits by default. This is definitely not correct for many bits (like
// isChill for Div/Mod and all of the envisioned Load/Store flags), so if you add a new bit
// you will probably have to audit the compiler to make sure that phases that transcribe
// your opcode do the right thing with your bit.

class Kind {
public:
    Kind(Opcode opcode)
        : m_opcode(opcode)
    {
        u.bits = 0;
    }
    
    Kind()
        : Kind(Oops)
    {
    }
    
    Opcode opcode() const { return m_opcode; }
    void setOpcode(Opcode opcode) { m_opcode = opcode; }
    
    bool hasExtraBits() const { return !!u.bits; }
    
    // Chill bit. This applies to division-based arithmetic ops, which may trap on some
    // platforms or exhibit bizarre behavior when passed certain inputs. The non-chill
    // version will behave as unpredictably as it wants. For example, it's legal to
    // constant-fold Div(x, 0) to any value or to replace it with any effectful operation.
    // But when it's chill, that means that the semantics when it would have trapped are
    // the JS semantics. For example, Div<Chill>(@a, @b) means:
    //
    //     ((a | 0) / (b | 0)) | 0
    //
    // And Mod<Chill>(a, b) means:
    //
    //     ((a | 0) % (b | 0)) | 0
    //
    // Note that Div<Chill> matches exactly how ARM handles integer division.
    bool hasIsChill() const
    {
        switch (m_opcode) {
        case Div:
        case Mod:
            return true;
        default:
            return false;
        }
    }
    bool isChill() const
    {
        return hasIsChill() && u.isChill;
    }
    void setIsChill(bool isChill)
    {
        ASSERT(hasIsChill());
        u.isChill = isChill;
    }
    
    // Rules for adding new properties:
    // - Put the accessors here.
    // - hasBlah() should check if the opcode allows for your property.
    // - blah() returns a default value if !hasBlah()
    // - setBlah() asserts if !hasBlah()
    // - Try not to increase the size of Kind too much. But it wouldn't be the end of the
    //   world if it bloated to 64 bits.
    
    bool operator==(const Kind& other) const
    {
        return m_opcode == other.m_opcode
            && u.bits == other.u.bits;
    }
    
    bool operator!=(const Kind& other) const
    {
        return !(*this == other);
    }
    
    void dump(PrintStream&) const;
    
    unsigned hash() const
    {
        // It's almost certainly more important that this hash function is cheap to compute than
        // anything else. We can live with some kind hash collisions.
        return m_opcode + u.bits * 111;
    }
    
    Kind(WTF::HashTableDeletedValueType)
        : m_opcode(Oops)
    {
        u.bits = 1;
    }
    
    bool isHashTableDeletedValue() const
    {
        return *this == Kind(WTF::HashTableDeletedValue);
    }
    
private:
    Opcode m_opcode;
    union {
        bool isChill;
        uint16_t bits;
    } u;
};

// For every flag 'foo' you add, it's customary to create a Kind B3::foo(Kind) function that makes
// a kind with the flag set. For example, for chill, this lets us say:
//
//     block->appendNew<Value>(m_proc, chill(Mod), Origin(), a, b);
//
// That looks pretty slick. Let's keep it that way.

inline Kind chill(Kind kind)
{
    kind.setIsChill(true);
    return kind;
}

struct KindHash {
    static unsigned hash(const Kind& key) { return key.hash(); }
    static bool equal(const Kind& a, const Kind& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::B3

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::B3::Kind> {
    typedef JSC::B3::KindHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::B3::Kind> : public SimpleClassHashTraits<JSC::B3::Kind> {
    static const bool emptyValueIsZero = false;
};

} // namespace WTF

#endif // ENABLE(B3_JIT)

#endif // B3Kind_h

