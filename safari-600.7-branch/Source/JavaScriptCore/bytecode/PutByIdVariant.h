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

#ifndef PutByIdVariant_h
#define PutByIdVariant_h

#include "IntendedStructureChain.h"
#include "PropertyOffset.h"

namespace JSC {

class PutByIdVariant {
public:
    enum Kind {
        NotSet,
        Replace,
        Transition
    };
    
    PutByIdVariant()
        : m_kind(NotSet)
        , m_oldStructure(0)
        , m_newStructure(0)
        , m_offset(invalidOffset)
    {
    }
    
    static PutByIdVariant replace(Structure* structure, PropertyOffset offset)
    {
        PutByIdVariant result;
        result.m_kind = Replace;
        result.m_oldStructure = structure;
        result.m_offset = offset;
        return result;
    }
    
    static PutByIdVariant transition(
        Structure* oldStructure, Structure* newStructure,
        PassRefPtr<IntendedStructureChain> structureChain, PropertyOffset offset)
    {
        PutByIdVariant result;
        result.m_kind = Transition;
        result.m_oldStructure = oldStructure;
        result.m_newStructure = newStructure;
        result.m_structureChain = structureChain;
        result.m_offset = offset;
        return result;
    }
    
    Kind kind() const { return m_kind; }
    
    bool isSet() const { return kind() != NotSet; }
    bool operator!() const { return !isSet(); }
    
    Structure* structure() const
    {
        ASSERT(kind() == Replace);
        return m_oldStructure;
    }
    
    Structure* oldStructure() const
    {
        ASSERT(kind() == Transition || kind() == Replace);
        return m_oldStructure;
    }
    
    Structure* newStructure() const
    {
        ASSERT(kind() == Transition);
        return m_newStructure;
    }
    
    IntendedStructureChain* structureChain() const
    {
        ASSERT(kind() == Transition);
        return m_structureChain.get();
    }
    
    PropertyOffset offset() const
    {
        ASSERT(isSet());
        return m_offset;
    }
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

private:
    Kind m_kind;
    Structure* m_oldStructure;
    Structure* m_newStructure;
    RefPtr<IntendedStructureChain> m_structureChain;
    PropertyOffset m_offset;
};

} // namespace JSC

#endif // PutByIdVariant_h

