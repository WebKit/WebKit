/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef GetByIdStatus_h
#define GetByIdStatus_h

#include "CodeOrigin.h"
#include "ConcurrentJITLock.h"
#include "ExitingJITType.h"
#include "GetByIdVariant.h"
#include "StructureStubInfo.h"

namespace JSC {

class CodeBlock;

class GetByIdStatus {
public:
    enum State {
        NoInformation,  // It's uncached so we have no information.
        Simple,         // It's cached for a simple access to a known object property with
                        // a possible structure chain and a possible specific value.
        TakesSlowPath,  // It's known to often take slow path.
        MakesCalls      // It's known to take paths that make calls.
    };

    GetByIdStatus()
        : m_state(NoInformation)
    {
    }
    
    explicit GetByIdStatus(State state)
        : m_state(state)
    {
        ASSERT(state == NoInformation || state == TakesSlowPath || state == MakesCalls);
    }
    
    GetByIdStatus(
        State state, bool wasSeenInJIT, const GetByIdVariant& variant = GetByIdVariant())
        : m_state(state)
        , m_wasSeenInJIT(wasSeenInJIT)
    {
        ASSERT((state == Simple) == variant.isSet());
        m_variants.append(variant);
    }
    
    static GetByIdStatus computeFor(CodeBlock*, StubInfoMap&, unsigned bytecodeIndex, StringImpl* uid);
    static GetByIdStatus computeFor(VM&, Structure*, StringImpl* uid);
    
    static GetByIdStatus computeFor(CodeBlock* baselineBlock, CodeBlock* dfgBlock, StubInfoMap& baselineMap, StubInfoMap& dfgMap, CodeOrigin, StringImpl* uid);
    
    State state() const { return m_state; }
    
    bool isSet() const { return m_state != NoInformation; }
    bool operator!() const { return !isSet(); }
    bool isSimple() const { return m_state == Simple; }

    size_t numVariants() const { return m_variants.size(); }
    const Vector<GetByIdVariant, 1>& variants() const { return m_variants; }
    const GetByIdVariant& at(size_t index) const { return m_variants[index]; }
    const GetByIdVariant& operator[](size_t index) const { return at(index); }

    bool takesSlowPath() const { return m_state == TakesSlowPath || m_state == MakesCalls; }
    bool makesCalls() const { return m_state == MakesCalls; }
    
    bool wasSeenInJIT() const { return m_wasSeenInJIT; }
    
    void dump(PrintStream&) const;
    
private:
#if ENABLE(DFG_JIT)
    static bool hasExitSite(const ConcurrentJITLocker&, CodeBlock*, unsigned bytecodeIndex, ExitingJITType = ExitFromAnything);
#endif
#if ENABLE(JIT)
    static GetByIdStatus computeForStubInfo(const ConcurrentJITLocker&, CodeBlock*, StructureStubInfo*, StringImpl* uid);
#endif
    bool computeForChain(CodeBlock*, StringImpl* uid, PassRefPtr<IntendedStructureChain>);
    static GetByIdStatus computeFromLLInt(CodeBlock*, unsigned bytecodeIndex, StringImpl* uid);
    
    bool appendVariant(const GetByIdVariant&);
    
    State m_state;
    Vector<GetByIdVariant, 1> m_variants;
    bool m_wasSeenInJIT;
};

} // namespace JSC

#endif // PropertyAccessStatus_h

