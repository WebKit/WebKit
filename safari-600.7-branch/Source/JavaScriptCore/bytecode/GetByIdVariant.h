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

#ifndef GetByIdVariant_h
#define GetByIdVariant_h

#include "IntendedStructureChain.h"
#include "JSCJSValue.h"
#include "PropertyOffset.h"
#include "StructureSet.h"

namespace JSC {

class GetByIdStatus;
struct DumpContext;

class GetByIdVariant {
public:
    GetByIdVariant(
        const StructureSet& structureSet = StructureSet(),
        PropertyOffset offset = invalidOffset, JSValue specificValue = JSValue(),
        PassRefPtr<IntendedStructureChain> chain = nullptr)
        : m_structureSet(structureSet)
        , m_chain(chain)
        , m_specificValue(specificValue)
        , m_offset(offset)
    {
        if (!structureSet.size()) {
            ASSERT(offset == invalidOffset);
            ASSERT(!specificValue);
            ASSERT(!chain);
        }
    }
    
    bool isSet() const { return !!m_structureSet.size(); }
    bool operator!() const { return !isSet(); }
    const StructureSet& structureSet() const { return m_structureSet; }
    IntendedStructureChain* chain() const { return const_cast<IntendedStructureChain*>(m_chain.get()); }
    JSValue specificValue() const { return m_specificValue; }
    PropertyOffset offset() const { return m_offset; }
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    
private:
    friend class GetByIdStatus;
    
    StructureSet m_structureSet;
    RefPtr<IntendedStructureChain> m_chain;
    JSValue m_specificValue;
    PropertyOffset m_offset;
};

} // namespace JSC

#endif // GetByIdVariant_h

