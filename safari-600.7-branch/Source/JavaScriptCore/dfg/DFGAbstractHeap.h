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

#ifndef DFGAbstractHeap_h
#define DFGAbstractHeap_h

#if ENABLE(DFG_JIT)

#include "VirtualRegister.h"
#include <wtf/HashMap.h>
#include <wtf/PrintStream.h>

namespace JSC { namespace DFG {

// Implements a three-level type hierarchy:
// - World is the supertype of all of the things.
// - Kind with TOP payload is the direct subtype of World.
// - Kind with non-TOP payload is the direct subtype of its corresponding TOP Kind.

#define FOR_EACH_ABSTRACT_HEAP_KIND(macro) \
    macro(InvalidAbstractHeap) \
    macro(World) \
    macro(Arguments_numArguments) \
    macro(Arguments_overrideLength) \
    macro(Arguments_registers) \
    macro(Arguments_slowArguments) \
    macro(ArrayBuffer_data) \
    macro(Butterfly_arrayBuffer) \
    macro(Butterfly_publicLength) \
    macro(Butterfly_vectorLength) \
    macro(JSArrayBufferView_length) \
    macro(JSArrayBufferView_mode) \
    macro(JSArrayBufferView_vector) \
    macro(JSCell_structureID) \
    macro(JSCell_indexingType) \
    macro(JSCell_typeInfoFlags) \
    macro(JSCell_typeInfoType) \
    macro(JSFunction_executable) \
    macro(JSFunction_scopeChain) \
    macro(JSObject_butterfly) \
    macro(JSVariableObject_registers) \
    macro(NamedProperties) \
    macro(IndexedInt32Properties) \
    macro(IndexedDoubleProperties) \
    macro(IndexedContiguousProperties) \
    macro(ArrayStorageProperties) \
    macro(Variables) \
    macro(TypedArrayProperties) \
    macro(GCState) \
    macro(BarrierState) \
    macro(RegExpState) \
    macro(InternalState) \
    macro(Absolute) \
    /* Use this for writes only, to indicate that this may fire watchpoints. Usually this is never directly written but instead we test to see if a node clobbers this; it just so happens that you have to write world to clobber it. */\
    macro(Watchpoint_fire) \
    /* Use this for reads only, just to indicate that if the world got clobbered, then this operation will not work. */\
    macro(MiscFields) \
    /* Use this for writes only, just to indicate that hoisting the node is invalid. This works because we don't hoist anything that has any side effects at all. */\
    macro(SideState)

enum AbstractHeapKind {
#define ABSTRACT_HEAP_DECLARATION(name) name,
    FOR_EACH_ABSTRACT_HEAP_KIND(ABSTRACT_HEAP_DECLARATION)
#undef ABSTRACT_HEAP_DECLARATION
};

class AbstractHeap {
public:
    class Payload {
    public:
        Payload()
            : m_isTop(false)
            , m_value(0)
        {
        }
        
        Payload(bool isTop, int64_t value)
            : m_isTop(isTop)
            , m_value(value)
        {
            ASSERT(!(isTop && value));
        }
        
        Payload(int64_t value)
            : m_isTop(false)
            , m_value(value)
        {
        }
        
        Payload(const void* pointer)
            : m_isTop(false)
            , m_value(bitwise_cast<intptr_t>(pointer))
        {
        }
        
        Payload(VirtualRegister operand)
            : m_isTop(false)
            , m_value(operand.offset())
        {
        }
        
        static Payload top() { return Payload(true, 0); }
        
        bool isTop() const { return m_isTop; }
        int64_t value() const
        {
            ASSERT(!isTop());
            return valueImpl();
        }
        int64_t valueImpl() const
        {
            return m_value;
        }
        
        bool operator==(const Payload& other) const
        {
            return m_isTop == other.m_isTop
                && m_value == other.m_value;
        }
        
        bool operator!=(const Payload& other) const
        {
            return !(*this == other);
        }
        
        bool operator<(const Payload& other) const
        {
            if (isTop())
                return !other.isTop();
            if (other.isTop())
                return false;
            return value() < other.value();
        }
        
        bool isDisjoint(const Payload& other) const
        {
            if (isTop())
                return false;
            if (other.isTop())
                return false;
            return m_value != other.m_value;
        }
        
        bool overlaps(const Payload& other) const
        {
            return !isDisjoint(other);
        }
        
        void dump(PrintStream&) const;
        
    private:
        bool m_isTop;
        int64_t m_value;
    };
    
    AbstractHeap()
    {
        m_value = encode(InvalidAbstractHeap, Payload());
    }
    
    AbstractHeap(AbstractHeapKind kind)
    {
        ASSERT(kind != InvalidAbstractHeap);
        m_value = encode(kind, Payload::top());
    }
    
    AbstractHeap(AbstractHeapKind kind, Payload payload)
    {
        ASSERT(kind != InvalidAbstractHeap && kind != World);
        m_value = encode(kind, payload);
    }
    
    AbstractHeap(WTF::HashTableDeletedValueType)
    {
        m_value = encode(InvalidAbstractHeap, Payload::top());
    }
    
    bool operator!() const { return kind() == InvalidAbstractHeap && !payloadImpl().isTop(); }
    
    AbstractHeapKind kind() const { return static_cast<AbstractHeapKind>(m_value & ((1 << topShift) - 1)); }
    Payload payload() const
    {
        ASSERT(kind() != World && kind() != InvalidAbstractHeap);
        return payloadImpl();
    }
    
    bool isDisjoint(const AbstractHeap& other)
    {
        ASSERT(kind() != InvalidAbstractHeap);
        ASSERT(other.kind() != InvalidAbstractHeap);
        if (kind() == World)
            return false;
        if (other.kind() == World)
            return false;
        if (kind() != other.kind())
            return true;
        return payload().isDisjoint(other.payload());
    }
    
    bool overlaps(const AbstractHeap& other)
    {
        return !isDisjoint(other);
    }
    
    AbstractHeap supertype() const
    {
        ASSERT(kind() != InvalidAbstractHeap);
        if (kind() == World)
            return AbstractHeap();
        if (payload().isTop())
            return World;
        return AbstractHeap(kind());
    }
    
    unsigned hash() const
    {
        return WTF::IntHash<int64_t>::hash(m_value);
    }
    
    bool operator==(const AbstractHeap& other) const
    {
        return m_value == other.m_value;
    }
    
    bool operator!=(const AbstractHeap& other) const
    {
        return !(*this == other);
    }
    
    bool operator<(const AbstractHeap& other) const
    {
        if (kind() != other.kind())
            return kind() < other.kind();
        return payload() < other.payload();
    }
    
    bool isHashTableDeletedValue() const
    {
        return kind() == InvalidAbstractHeap && payloadImpl().isTop();
    }
    
    void dump(PrintStream& out) const;
    
private:
    static const unsigned valueShift = 15;
    static const unsigned topShift = 14;
    
    Payload payloadImpl() const
    {
        return Payload((m_value >> topShift) & 1, m_value >> valueShift);
    }
    
    static int64_t encode(AbstractHeapKind kind, Payload payload)
    {
        int64_t kindAsInt = static_cast<int64_t>(kind);
        ASSERT(kindAsInt < (1 << topShift));
        return kindAsInt | (payload.isTop() << topShift) | (payload.valueImpl() << valueShift);
    }
    
    // The layout of the value is:
    // Low 14 bits: the Kind
    // 15th bit: whether or not the payload is TOP.
    // The upper bits: the payload.value().
    int64_t m_value;
};

struct AbstractHeapHash {
    static unsigned hash(const AbstractHeap& key) { return key.hash(); }
    static bool equal(const AbstractHeap& a, const AbstractHeap& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::DFG

namespace WTF {

void printInternal(PrintStream&, JSC::DFG::AbstractHeapKind);

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::DFG::AbstractHeap> {
    typedef JSC::DFG::AbstractHeapHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::DFG::AbstractHeap> : SimpleClassHashTraits<JSC::DFG::AbstractHeap> { };

} // namespace WTF

#endif // ENABLE(DFG_JIT)

#endif // DFGAbstractHeap_h

