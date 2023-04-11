/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "ConcurrentJSLock.h"
#include "ICStatusMap.h"
#include "InstanceOfVariant.h"
#include "StubInfoSummary.h"

namespace JSC {

class AccessCase;
class CodeBlock;
class StructureStubInfo;

class InstanceOfStatus final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum State {
        // It's uncached so we have no information.
        NoInformation,
        
        // It's cached in a simple way.
        Simple,
        
        // It's known to often take slow path.
        TakesSlowPath
    };
    
    InstanceOfStatus()
        : m_state(NoInformation)
    {
    }
    
    InstanceOfStatus(State state)
        : m_state(state)
    {
        ASSERT(state == NoInformation || state == TakesSlowPath);
    }
    
    explicit InstanceOfStatus(StubInfoSummary summary)
    {
        switch (summary) {
        case StubInfoSummary::NoInformation:
            m_state = NoInformation;
            return;
        case StubInfoSummary::Simple:
        case StubInfoSummary::MakesCalls:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        case StubInfoSummary::TakesSlowPath:
        case StubInfoSummary::TakesSlowPathAndMakesCalls:
            m_state = TakesSlowPath;
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    static InstanceOfStatus computeFor(CodeBlock*, ICStatusMap&, BytecodeIndex);
    
#if ENABLE(DFG_JIT)
    static InstanceOfStatus computeForStubInfo(const ConcurrentJSLocker&, VM&, StructureStubInfo*);
#endif
    
    State state() const { return m_state; }
    
    bool isSet() const { return m_state != NoInformation; }
    explicit operator bool() const { return isSet(); }
    
    bool isSimple() const { return m_state == Simple; }
    bool takesSlowPath() const { return m_state == TakesSlowPath; }
    
    JSObject* commonPrototype() const;
    
    size_t numVariants() const { return m_variants.size(); }
    const Vector<InstanceOfVariant, 2>& variants() const { return m_variants; }
    const InstanceOfVariant& at(size_t index) const { return m_variants[index]; }
    const InstanceOfVariant& operator[](size_t index) const { return at(index); }

    void filter(const StructureSet&);
    
    void dump(PrintStream&) const;

private:
    void appendVariant(const InstanceOfVariant&);
    void shrinkToFit();
    
    State m_state;
    Vector<InstanceOfVariant, 2> m_variants;
};

} // namespace JSC

