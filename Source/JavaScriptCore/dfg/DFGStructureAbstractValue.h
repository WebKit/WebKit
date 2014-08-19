/*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGStructureAbstractValue_h
#define DFGStructureAbstractValue_h

#if ENABLE(DFG_JIT)

#include "DFGTransition.h"
#include "JSCell.h"
#include "SpeculatedType.h"
#include "DumpContext.h"
#include "StructureSet.h"

namespace JSC { namespace DFG {

class StructureAbstractValue {
public:
    StructureAbstractValue() { }
    StructureAbstractValue(Structure* structure)
        : m_set(structure)
    {
        setClobbered(false);
    }
    StructureAbstractValue(const StructureSet& other)
        : m_set(other)
    {
        setClobbered(false);
    }
    ALWAYS_INLINE StructureAbstractValue(const StructureAbstractValue& other)
        : m_set(other.m_set)
    {
        setClobbered(other.isClobbered());
    }
    
    ALWAYS_INLINE StructureAbstractValue& operator=(Structure* structure)
    {
        m_set = structure;
        setClobbered(false);
        return *this;
    }
    ALWAYS_INLINE StructureAbstractValue& operator=(const StructureSet& other)
    {
        m_set = other;
        setClobbered(false);
        return *this;
    }
    ALWAYS_INLINE StructureAbstractValue& operator=(const StructureAbstractValue& other)
    {
        m_set = other.m_set;
        setClobbered(other.isClobbered());
        return *this;
    }
    
    void clear()
    {
        m_set.clear();
        setClobbered(false);
    }
    
    void makeTop()
    {
        m_set.deleteStructureListIfNecessary();
        m_set.m_pointer = topValue;
    }
    
#if ASSERT_DISABLED
    void assertIsRegistered(Graph&) const { }
#else
    void assertIsRegistered(Graph&) const;
#endif
    
    void clobber();
    void observeInvalidationPoint() { setClobbered(false); }
    
    void observeTransition(Structure* from, Structure* to);
    void observeTransitions(const TransitionVector&);
    
    static StructureAbstractValue top()
    {
        StructureAbstractValue result;
        result.m_set.m_pointer = topValue;
        return result;
    }
    
    bool isClear() const { return m_set.isEmpty(); }
    bool isTop() const { return m_set.m_pointer == topValue; }
    bool isNeitherClearNorTop() const { return !isClear() && !isTop(); }
    
    // A clobbered abstract value means that the set currently contains the m_set set of
    // structures plus TOP, except that the "plus TOP" will go away at the next invalidation
    // point. Note that it's tempting to think of this as "the set of structures in m_set plus
    // the set of structures transition-reachable from m_set" - but this isn't really correct,
    // since if we add an unwatchable structure after clobbering, the two definitions are not
    // equivalent. If we do this, the new unwatchable structure will be added to m_set.
    // Invalidation points do not try to "clip" the set of transition-reachable structures from
    // m_set by looking at reachability as this would mean that the new set is TOP. Instead they
    // literally assume that the set is just m_set rather than m_set plus TOP.
    bool isClobbered() const { return m_set.getReservedFlag(); }
    
    bool add(Structure* structure);
    
    bool merge(const StructureSet& other);
    
    ALWAYS_INLINE bool merge(const StructureAbstractValue& other)
    {
        if (other.isClear())
            return false;
        
        if (isTop())
            return false;
        
        if (other.isTop()) {
            makeTop();
            return true;
        }
        
        return mergeSlow(other);
    }
    
    void filter(const StructureSet& other);
    void filter(const StructureAbstractValue& other);
    
    ALWAYS_INLINE void filter(SpeculatedType type)
    {
        if (!(type & SpecCell)) {
            clear();
            return;
        }
        if (isNeitherClearNorTop())
            filterSlow(type);
    }
    
    ALWAYS_INLINE bool operator==(const StructureAbstractValue& other) const
    {
        if ((m_set.isThin() && other.m_set.isThin()) || isTop() || other.isTop())
            return m_set.m_pointer == other.m_set.m_pointer;
        
        return equalsSlow(other);
    }
    
    const StructureSet& set() const
    {
        ASSERT(!isTop());
        return m_set;
    }
    
    size_t size() const
    {
        ASSERT(!isTop());
        return m_set.size();
    }
    
    Structure* at(size_t i) const
    {
        ASSERT(!isTop());
        return m_set.at(i);
    }
    
    Structure* operator[](size_t i) const { return at(i); }
    
    // In most cases, what you really want to do is verify whether the set is top or clobbered, and
    // if not, enumerate the set of structures. Use this only in cases where the singleton case is
    // meaningfully special, like for transitions.
    Structure* onlyStructure() const
    {
        if (isTop() || isClobbered())
            return nullptr;
        return m_set.onlyStructure();
    }
    
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
    
    // The methods below are all conservative and err on the side of making 'this' appear bigger
    // than it is. For example, contains() may return true if the set is clobbered or TOP.
    // isSubsetOf() may return false in case of ambiguities. Therefore you should only perform
    // optimizations as a consequence of the "this is smaller" return value - so false for
    // contains(), true for isSubsetOf(), false for isSupersetOf(), and false for overlaps().

    bool contains(Structure* structure) const;
    
    bool isSubsetOf(const StructureSet& other) const;
    bool isSubsetOf(const StructureAbstractValue& other) const;
    
    bool isSupersetOf(const StructureSet& other) const;
    bool isSupersetOf(const StructureAbstractValue& other) const
    {
        return other.isSubsetOf(*this);
    }
    
    bool overlaps(const StructureSet& other) const;
    bool overlaps(const StructureAbstractValue& other) const;
    
private:
    static const uintptr_t clobberedFlag = StructureSet::reservedFlag;
    static const uintptr_t topValue = StructureSet::reservedValue;
    static const unsigned polymorphismLimit = 10;
    static const unsigned clobberedSupremacyThreshold = 2;
    
    void filterSlow(SpeculatedType type);
    bool mergeSlow(const StructureAbstractValue& other);
    
    bool equalsSlow(const StructureAbstractValue& other) const;
    
    void makeTopWhenThin()
    {
        ASSERT(m_set.isThin());
        m_set.m_pointer = topValue;
    }
    
    bool mergeNotTop(const StructureSet& other);
    
    void setClobbered(bool clobbered)
    {
        ASSERT(!isTop() || !clobbered);
        m_set.setReservedFlag(clobbered);
    }
    
    StructureSet m_set;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGStructureAbstractValue_h


