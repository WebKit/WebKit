/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Igalia S.A. All rights reserved.
 *
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

#include "CacheableIdentifier.h"
#include "CheckPrivateBrandVariant.h"
#include "CodeOrigin.h"
#include "ConcurrentJSLock.h"
#include "ExitFlag.h"
#include "ICStatusMap.h"
#include "StubInfoSummary.h"

namespace JSC {

class AccessCase;
class CodeBlock;
class StructureStubInfo;

class CheckPrivateBrandStatus final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum State : uint8_t {
        // It's uncached so we have no information.
        NoInformation,
        // It's cached for a simple access.
        Simple,
        // It will likely take the slow path.
        LikelyTakesSlowPath,
        // It has been seen to take the slow path.
        ObservedTakesSlowPath,
    };

    CheckPrivateBrandStatus()
        : m_state(NoInformation)
    {
    }

    explicit CheckPrivateBrandStatus(State state)
        : m_state(state)
    {
        ASSERT(state != Simple);
    }

    static CheckPrivateBrandStatus computeFor(CodeBlock* baselineBlock, ICStatusMap& baselineMap, ICStatusContextStack& dfgContextStack, CodeOrigin);

    State state() const { return m_state; }

    bool isSet() const { return m_state != NoInformation; }
    bool operator!() const { return !isSet(); }
    bool observedSlowPath() const { return m_state == ObservedTakesSlowPath; }
    bool isSimple() const { return m_state == Simple; }
    const Vector<CheckPrivateBrandVariant, 1>& variants() { return m_variants; }
    CacheableIdentifier singleIdentifier() const;

    CheckPrivateBrandStatus slowVersion() const;

    // Attempts to reduce the set of variants to fit the given structure set. This may be approximate.
    void filter(const StructureSet&);

    DECLARE_VISIT_AGGREGATE;
    template<typename Visitor> void markIfCheap(Visitor&);
    bool finalize(VM&);

    bool appendVariant(const CheckPrivateBrandVariant&);
    void shrinkToFit();

    void dump(PrintStream&) const;

private:
    explicit CheckPrivateBrandStatus(StubInfoSummary, StructureStubInfo&);
    void merge(const CheckPrivateBrandStatus&);

    static CheckPrivateBrandStatus computeForBaseline(CodeBlock*, ICStatusMap&, BytecodeIndex, ExitFlag);
#if ENABLE(JIT)
    static CheckPrivateBrandStatus computeForStubInfoWithoutExitSiteFeedback(
        const ConcurrentJSLocker&, CodeBlock* profiledBlock, StructureStubInfo*);
#endif

    Vector<CheckPrivateBrandVariant, 1> m_variants;
    State m_state;
};

} // namespace JSC
