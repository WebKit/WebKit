/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "CallLinkStatus.h"
#include "CodeOrigin.h"
#include "ConcurrentJSLock.h"
#include "ICStatusMap.h"
#include "InByIdVariant.h"
#include "StubInfoSummary.h"

namespace JSC {

class AccessCase;
class CodeBlock;
class StructureStubInfo;

class InByIdStatus final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum State {
        // It's uncached so we have no information.
        NoInformation,
        // It's cached for a simple access to a known object property with
        // a possible structure chain and a possible specific value.
        Simple,
        // It's known to often take slow path.
        TakesSlowPath,
    };

    InByIdStatus() = default;

    InByIdStatus(State state, const InByIdVariant& variant = InByIdVariant())
        : m_state(state)
    {
        ASSERT((state == Simple) == variant.isSet());
        if (variant.isSet())
            m_variants.append(variant);
    }

    explicit InByIdStatus(StubInfoSummary summary)
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
    
    static InByIdStatus computeFor(CodeBlock*, ICStatusMap&, BytecodeIndex, UniquedStringImpl* uid);
    static InByIdStatus computeFor(CodeBlock*, ICStatusMap&, BytecodeIndex, UniquedStringImpl* uid, ExitFlag);
    static InByIdStatus computeFor(CodeBlock* baselineBlock, ICStatusMap& baselineMap, ICStatusContextStack& contextStack, CodeOrigin, UniquedStringImpl* uid);

#if ENABLE(DFG_JIT)
    static InByIdStatus computeForStubInfo(const ConcurrentJSLocker&, CodeBlock* baselineBlock, StructureStubInfo*, CodeOrigin, UniquedStringImpl* uid);
#endif

    State state() const { return m_state; }

    bool isSet() const { return m_state != NoInformation; }
    explicit operator bool() const { return isSet(); }
    bool isSimple() const { return m_state == Simple; }

    size_t numVariants() const { return m_variants.size(); }
    const Vector<InByIdVariant, 1>& variants() const { return m_variants; }
    const InByIdVariant& at(size_t index) const { return m_variants[index]; }
    const InByIdVariant& operator[](size_t index) const { return at(index); }

    bool takesSlowPath() const { return m_state == TakesSlowPath; }
    
    void merge(const InByIdStatus&);

    // Attempts to reduce the set of variants to fit the given structure set. This may be approximate.
    void filter(const StructureSet&);
    
    void markIfCheap(SlotVisitor&);
    bool finalize(VM&);

    void dump(PrintStream&) const;

private:
#if ENABLE(DFG_JIT)
    static InByIdStatus computeForStubInfoWithoutExitSiteFeedback(const ConcurrentJSLocker&, VM&, StructureStubInfo*, UniquedStringImpl* uid);
#endif
    bool appendVariant(const InByIdVariant&);

    State m_state { NoInformation };
    Vector<InByIdVariant, 1> m_variants;
};

} // namespace JSC
