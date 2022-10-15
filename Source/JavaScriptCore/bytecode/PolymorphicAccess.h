/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "AccessCase.h"
#include "JITStubRoutine.h"
#include "JSFunctionInlines.h"
#include "MacroAssembler.h"
#include "ScratchRegisterAllocator.h"
#include <wtf/FixedVector.h>
#include <wtf/Vector.h>

namespace JSC {
namespace DOMJIT {
class GetterSetter;
}

class CCallHelpers;
class CodeBlock;
class PolymorphicAccess;
class StructureStubInfo;
class WatchpointsOnStructureStubInfo;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PolymorphicAccess);

class AccessGenerationResult {
public:
    enum Kind {
        MadeNoChanges,
        GaveUp,
        Buffered,
        GeneratedNewCode,
        GeneratedFinalCode, // Generated so much code that we never want to generate code again.
        ResetStubAndFireWatchpoints // We found out some data that makes us want to start over fresh with this stub. Currently, this happens when we detect poly proto.
    };
    

    AccessGenerationResult() = default;
    AccessGenerationResult(AccessGenerationResult&&) = default;
    AccessGenerationResult& operator=(AccessGenerationResult&&) = default;
    
    AccessGenerationResult(Kind kind)
        : m_kind(kind)
    {
        RELEASE_ASSERT(kind != GeneratedNewCode);
        RELEASE_ASSERT(kind != GeneratedFinalCode);
    }
    
    AccessGenerationResult(Kind kind, CodePtr<JITStubRoutinePtrTag> code)
        : m_kind(kind)
        , m_code(code)
    {
        RELEASE_ASSERT(kind == GeneratedNewCode || kind == GeneratedFinalCode);
        RELEASE_ASSERT(code);
    }
    
    bool operator==(const AccessGenerationResult& other) const
    {
        return m_kind == other.m_kind && m_code == other.m_code;
    }
    
    bool operator!=(const AccessGenerationResult& other) const
    {
        return !(*this == other);
    }
    
    explicit operator bool() const
    {
        return *this != AccessGenerationResult();
    }
    
    Kind kind() const { return m_kind; }
    
    const CodePtr<JITStubRoutinePtrTag>& code() const { return m_code; }
    
    bool madeNoChanges() const { return m_kind == MadeNoChanges; }
    bool gaveUp() const { return m_kind == GaveUp; }
    bool buffered() const { return m_kind == Buffered; }
    bool generatedNewCode() const { return m_kind == GeneratedNewCode; }
    bool generatedFinalCode() const { return m_kind == GeneratedFinalCode; }
    bool shouldResetStubAndFireWatchpoints() const { return m_kind == ResetStubAndFireWatchpoints; }
    
    // If we gave up on this attempt to generate code, or if we generated the "final" code, then we
    // should give up after this.
    bool shouldGiveUpNow() const { return gaveUp() || generatedFinalCode(); }
    
    bool generatedSomeCode() const { return generatedNewCode() || generatedFinalCode(); }
    
    void dump(PrintStream&) const;

    void addWatchpointToFire(InlineWatchpointSet& set, StringFireDetail detail)
    {
        m_watchpointsToFire.append(std::pair<InlineWatchpointSet&, StringFireDetail>(set, detail));
    }
    void fireWatchpoints(VM& vm)
    {
        ASSERT(m_kind == ResetStubAndFireWatchpoints);
        for (auto& pair : m_watchpointsToFire)
            pair.first.invalidate(vm, pair.second);
    }
    
private:
    Kind m_kind;
    CodePtr<JITStubRoutinePtrTag> m_code;
    Vector<std::pair<InlineWatchpointSet&, StringFireDetail>> m_watchpointsToFire;
};

class PolymorphicAccess {
    WTF_MAKE_NONCOPYABLE(PolymorphicAccess);
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(PolymorphicAccess);
public:
    PolymorphicAccess();
    ~PolymorphicAccess();

    // When this fails (returns GaveUp), this will leave the old stub intact but you should not try
    // to call this method again for that PolymorphicAccess instance.
    AccessGenerationResult addCases(
        const GCSafeConcurrentJSLocker&, VM&, CodeBlock*, StructureStubInfo&, Vector<RefPtr<AccessCase>, 2>);

    AccessGenerationResult addCase(
        const GCSafeConcurrentJSLocker&, VM&, CodeBlock*, StructureStubInfo&, Ref<AccessCase>);
    
    AccessGenerationResult regenerate(const GCSafeConcurrentJSLocker&, VM&, JSGlobalObject*, CodeBlock*, ECMAMode, StructureStubInfo&);
    
    bool isEmpty() const { return m_list.isEmpty(); }
    unsigned size() const { return m_list.size(); }
    const AccessCase& at(unsigned i) const { return *m_list[i]; }
    const AccessCase& operator[](unsigned i) const { return *m_list[i]; }

    DECLARE_VISIT_AGGREGATE;

    // If this returns false then we are requesting a reset of the owning StructureStubInfo.
    bool visitWeak(VM&) const;
    
    // This returns true if it has marked everything it will ever marked. This can be used as an
    // optimization to then avoid calling this method again during the fixpoint.
    template<typename Visitor> void propagateTransitions(Visitor&) const;

    void aboutToDie();

    void dump(PrintStream& out) const;
    bool containsPC(void* pc) const
    { 
        if (!m_stubRoutine)
            return false;

        uintptr_t pcAsInt = bitwise_cast<uintptr_t>(pc);
        return m_stubRoutine->startAddress() <= pcAsInt && pcAsInt <= m_stubRoutine->endAddress();
    }

private:
    friend class AccessCase;
    friend class CodeBlock;
    friend struct AccessGenerationState;
    
    typedef Vector<RefPtr<AccessCase>, 2> ListType;
    
    void commit(
        const GCSafeConcurrentJSLocker&, VM&, std::unique_ptr<WatchpointsOnStructureStubInfo>&, CodeBlock*, StructureStubInfo&,
        AccessCase&);

    ListType m_list;
    RefPtr<PolymorphicAccessJITStubRoutine> m_stubRoutine;
    std::unique_ptr<WatchpointsOnStructureStubInfo> m_watchpoints;
};

struct AccessGenerationState {
    AccessGenerationState(VM& vm, JSGlobalObject* globalObject, ECMAMode ecmaMode)
        : m_vm(vm) 
        , m_globalObject(globalObject)
        , m_ecmaMode(ecmaMode)
    {
    }
    VM& m_vm;
    JSGlobalObject* const m_globalObject;
    CCallHelpers* jit { nullptr };
    ScratchRegisterAllocator* allocator;
    ScratchRegisterAllocator::PreservedState preservedReusedRegisterState;
    PolymorphicAccess* access { nullptr };
    StructureStubInfo* stubInfo { nullptr };
    MacroAssembler::JumpList success;
    MacroAssembler::JumpList failAndRepatch;
    MacroAssembler::JumpList failAndIgnore;
    GPRReg scratchGPR { InvalidGPRReg };
    FPRReg scratchFPR { InvalidFPRReg };
    const ECMAMode m_ecmaMode { ECMAMode::sloppy() };
    std::unique_ptr<WatchpointsOnStructureStubInfo> watchpoints;
    Vector<StructureID> weakStructures;
    Bag<OptimizingCallLinkInfo> m_callLinkInfos;
    bool m_doesJSCalls : 1 { false };
    bool m_doesCalls : 1 { false };

    void installWatchpoint(CodeBlock*, const ObjectPropertyCondition&);

    void restoreScratch();
    void succeed();

    struct SpillState {
        SpillState() = default;
        SpillState(ScalarRegisterSet&& regs, unsigned usedStackBytes)
            : spilledRegisters(WTFMove(regs))
            , numberOfStackBytesUsedForRegisterPreservation(usedStackBytes)
        {
        }

        ScalarRegisterSet spilledRegisters { };
        unsigned numberOfStackBytesUsedForRegisterPreservation { std::numeric_limits<unsigned>::max() };

        bool isEmpty() const { return numberOfStackBytesUsedForRegisterPreservation == std::numeric_limits<unsigned>::max(); }
    };

    const ScalarRegisterSet& calculateLiveRegistersForCallAndExceptionHandling();

    SpillState preserveLiveRegistersToStackForCall(const RegisterSet& extra = { });
    SpillState preserveLiveRegistersToStackForCallWithoutExceptions();

    void restoreLiveRegistersFromStackForCallWithThrownException(const SpillState&);
    void restoreLiveRegistersFromStackForCall(const SpillState&, const RegisterSet& dontRestore = { });

    const ScalarRegisterSet& liveRegistersForCall();

    CallSiteIndex callSiteIndexForExceptionHandlingOrOriginal();
    DisposableCallSiteIndex callSiteIndexForExceptionHandling();

    const HandlerInfo& originalExceptionHandler();

    bool needsToRestoreRegistersIfException() const { return m_needsToRestoreRegistersIfException; }
    CallSiteIndex originalCallSiteIndex() const;
    
    void emitExplicitExceptionHandler();

    void setSpillStateForJSCall(SpillState& spillState)
    {
        if (!m_spillStateForJSCall.isEmpty()) {
            ASSERT(m_spillStateForJSCall.numberOfStackBytesUsedForRegisterPreservation == spillState.numberOfStackBytesUsedForRegisterPreservation);
            ASSERT(m_spillStateForJSCall.spilledRegisters == spillState.spilledRegisters);
        }
        m_spillStateForJSCall = spillState;
    }
    SpillState spillStateForJSCall() const { return m_spillStateForJSCall; }

    ScratchRegisterAllocator makeDefaultScratchAllocator(GPRReg extraToLock = InvalidGPRReg);

private:
    const ScalarRegisterSet& liveRegistersToPreserveAtExceptionHandlingCallSite();
    
    ScalarRegisterSet m_liveRegistersToPreserveAtExceptionHandlingCallSite;
    ScalarRegisterSet m_liveRegistersForCall;
    CallSiteIndex m_callSiteIndex;
    SpillState m_spillStateForJSCall;
    bool m_calculatedRegistersForCallAndExceptionHandling : 1 { false };
    bool m_needsToRestoreRegistersIfException : 1 { false };
    bool m_calculatedCallSiteIndex : 1 { false };
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::AccessGenerationResult::Kind);
void printInternal(PrintStream&, JSC::AccessCase::AccessType);
void printInternal(PrintStream&, JSC::AccessCase::State);

} // namespace WTF

#endif // ENABLE(JIT)
