/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef PolymorphicGetByIdList_h
#define PolymorphicGetByIdList_h

#if ENABLE(JIT)

#include "CodeOrigin.h"
#include "MacroAssembler.h"
#include "Opcode.h"
#include "Structure.h"
#include <wtf/Vector.h>

namespace JSC {

class CodeBlock;
struct StructureStubInfo;

class GetByIdAccess {
public:
    enum AccessType {
        Invalid,
        SimpleInline, // This is the patched inline access.
        SimpleStub, // This is a stub.
        WatchedStub,
        Getter,
        CustomGetter
    };
    
    GetByIdAccess()
        : m_type(Invalid)
        , m_chainCount(0)
    {
    }
    
    GetByIdAccess(
        VM&, JSCell* owner, AccessType, PassRefPtr<JITStubRoutine>, Structure*,
        StructureChain* = 0, unsigned chainCount = 0);
    
    ~GetByIdAccess();
    
    static GetByIdAccess fromStructureStubInfo(StructureStubInfo&);
    
    bool isSet() const { return m_type != Invalid; }
    bool operator!() const { return !isSet(); }
    
    AccessType type() const { return m_type; }
    
    Structure* structure() const { return m_structure.get(); }
    
    StructureChain* chain() const { return m_chain.get(); }
    unsigned chainCount() const { return m_chainCount; }
    
    JITStubRoutine* stubRoutine() const
    {
        ASSERT(isSet());
        return m_stubRoutine.get();
    }
    
    bool doesCalls() const { return type() == Getter || type() == CustomGetter; }
    bool isWatched() const { return type() == WatchedStub; }
    bool isSimple() const { return !doesCalls() && !isWatched(); }
    
    bool visitWeak(RepatchBuffer&) const;

private:
    friend class CodeBlock;
    
    AccessType m_type;
    unsigned m_chainCount;
    WriteBarrier<Structure> m_structure;
    WriteBarrier<StructureChain> m_chain;
    RefPtr<JITStubRoutine> m_stubRoutine;
};

class PolymorphicGetByIdList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Either creates a new polymorphic get list, or returns the one that is already in
    // place.
    static PolymorphicGetByIdList* from(StructureStubInfo&);
    
    ~PolymorphicGetByIdList();
    
    MacroAssemblerCodePtr currentSlowPathTarget(StructureStubInfo& stubInfo) const;
    
    void addAccess(const GetByIdAccess&);
    
    bool isEmpty() const { return m_list.isEmpty(); }
    unsigned size() const { return m_list.size(); }
    bool isFull() const;
    bool isAlmostFull() const; // True if adding an element would make isFull() true.
    const GetByIdAccess& at(unsigned i) const { return m_list[i]; }
    const GetByIdAccess& operator[](unsigned i) const { return m_list[i]; }
    
    bool didSelfPatching() const; // Are any of the accesses SimpleInline?
    
    bool visitWeak(RepatchBuffer&) const;

private:
    friend class CodeBlock;
    
    PolymorphicGetByIdList(StructureStubInfo&);
    
    Vector<GetByIdAccess, 2> m_list;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // PolymorphicGetByIdList_h

