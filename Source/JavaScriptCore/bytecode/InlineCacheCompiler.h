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
#include "StructureStubClearingWatchpoint.h"
#include <wtf/FixedVector.h>
#include <wtf/Vector.h>

namespace JSC {
namespace DOMJIT {
class GetterSetter;
}

class CCallHelpers;
class CodeBlock;
class PolymorphicAccess;
class ProxyObjectAccessCase;
class StructureStubInfo;
class InlineCacheHandler;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PolymorphicAccess);

class AccessGenerationResult {
public:
    enum Kind {
        MadeNoChanges,
        GaveUp,
        Buffered,
        GeneratedNewCode,
        GeneratedFinalCode, // Generated so much code that we never want to generate code again.
        GeneratedMegamorphicCode, // Generated so much code that we never want to generate code again. And this is megamorphic code.
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
        RELEASE_ASSERT(kind != GeneratedMegamorphicCode);
    }

    AccessGenerationResult(Kind, Ref<InlineCacheHandler>&&);

    Kind kind() const { return m_kind; }

    bool madeNoChanges() const { return m_kind == MadeNoChanges; }
    bool gaveUp() const { return m_kind == GaveUp; }
    bool buffered() const { return m_kind == Buffered; }
    bool generatedNewCode() const { return m_kind == GeneratedNewCode; }
    bool generatedFinalCode() const { return m_kind == GeneratedFinalCode; }
    bool generatedMegamorphicCode() const { return m_kind == GeneratedMegamorphicCode; }
    bool shouldResetStubAndFireWatchpoints() const { return m_kind == ResetStubAndFireWatchpoints; }

    // If we gave up on this attempt to generate code, or if we generated the "final" code, then we
    // should give up after this.
    bool shouldGiveUpNow() const { return gaveUp() || generatedFinalCode(); }

    bool generatedSomeCode() const { return generatedNewCode() || generatedFinalCode() || generatedMegamorphicCode(); }

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

    InlineCacheHandler* handler() const { return m_handler.get(); }

private:
    Kind m_kind { MadeNoChanges };
    RefPtr<InlineCacheHandler> m_handler;
    Vector<std::pair<InlineWatchpointSet&, StringFireDetail>> m_watchpointsToFire;
};

class PolymorphicAccess {
    WTF_MAKE_NONCOPYABLE(PolymorphicAccess);
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(PolymorphicAccess);
public:
    friend class InlineCacheCompiler;

    PolymorphicAccess();
    ~PolymorphicAccess();

    // When this fails (returns GaveUp), this will leave the old stub intact but you should not try
    // to call this method again for that PolymorphicAccess instance.
    AccessGenerationResult addCases(
        const GCSafeConcurrentJSLocker&, VM&, CodeBlock*, StructureStubInfo&, Vector<RefPtr<AccessCase>, 2>);

    AccessGenerationResult addCase(
        const GCSafeConcurrentJSLocker&, VM&, CodeBlock*, StructureStubInfo&, Ref<AccessCase>);

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

    void dump(PrintStream& out) const;

private:
    friend class AccessCase;
    friend class CodeBlock;
    friend class InlineCacheCompiler;

    typedef Vector<RefPtr<AccessCase>, 2> ListType;

    ListType m_list;
    std::unique_ptr<WatchpointsOnStructureStubInfo> m_watchpoints;
};

class InlineCacheHandler final : public RefCounted<InlineCacheHandler> {
    WTF_MAKE_NONCOPYABLE(InlineCacheHandler);
    friend class InlineCacheCompiler;
public:
    static ptrdiff_t offsetOfCallTarget() { return OBJECT_OFFSETOF(InlineCacheHandler, m_callTarget); }
    static ptrdiff_t offsetOfJumpTarget() { return OBJECT_OFFSETOF(InlineCacheHandler, m_jumpTarget); }
    static ptrdiff_t offsetOfNext() { return OBJECT_OFFSETOF(InlineCacheHandler, m_next); }

    static Ref<InlineCacheHandler> create(Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<WatchpointsOnStructureStubInfo>&& watchpoints)
    {
        return adoptRef(*new InlineCacheHandler(WTFMove(stubRoutine), WTFMove(watchpoints)));
    }

    CodePtr<JITStubRoutinePtrTag> callTarget() const { return m_callTarget; }
    CodePtr<JITStubRoutinePtrTag> jumpTarget() const { return m_jumpTarget; }

    void aboutToDie();
    bool containsPC(void* pc) const
    {
        if (!m_stubRoutine)
            return false;

        uintptr_t pcAsInt = bitwise_cast<uintptr_t>(pc);
        return m_stubRoutine->startAddress() <= pcAsInt && pcAsInt <= m_stubRoutine->endAddress();
    }

    CallLinkInfo* callLinkInfoAt(const ConcurrentJSLocker&, unsigned index);

    // If this returns false then we are requesting a reset of the owning StructureStubInfo.
    bool visitWeak(VM&) const;

    void dump(PrintStream&) const;

    static Ref<InlineCacheHandler> createNonHandlerSlowPath(CodePtr<JITStubRoutinePtrTag>);

private:
    InlineCacheHandler() = default;
    InlineCacheHandler(Ref<PolymorphicAccessJITStubRoutine>&&, std::unique_ptr<WatchpointsOnStructureStubInfo>&&);

    static Ref<InlineCacheHandler> createSlowPath(VM&, AccessType);

    CodePtr<JITStubRoutinePtrTag> m_callTarget;
    CodePtr<JITStubRoutinePtrTag> m_jumpTarget;
    RefPtr<PolymorphicAccessJITStubRoutine> m_stubRoutine;
    std::unique_ptr<WatchpointsOnStructureStubInfo> m_watchpoints;
    RefPtr<InlineCacheHandler> m_next;
};

inline bool canUseMegamorphicGetById(VM& vm, UniquedStringImpl* uid)
{
    return !parseIndex(*uid) && uid != vm.propertyNames->length && uid != vm.propertyNames->name && uid != vm.propertyNames->prototype && uid != vm.propertyNames->underscoreProto;
}

inline bool canUseMegamorphicPutById(VM& vm, UniquedStringImpl* uid)
{
    return !parseIndex(*uid) && uid != vm.propertyNames->underscoreProto;
}

inline AccessGenerationResult::AccessGenerationResult(Kind kind, Ref<InlineCacheHandler>&& handler)
    : m_kind(kind)
    , m_handler(WTFMove(handler))
{
    RELEASE_ASSERT(kind == GeneratedNewCode || kind == GeneratedFinalCode || kind == GeneratedMegamorphicCode);
}

class InlineCacheCompiler {
public:
    InlineCacheCompiler(JITType jitType, VM& vm, JSGlobalObject* globalObject, ECMAMode ecmaMode, StructureStubInfo& stubInfo)
        : m_vm(vm)
        , m_globalObject(globalObject)
        , m_stubInfo(&stubInfo)
        , m_ecmaMode(ecmaMode)
        , m_jitType(jitType)
    {
    }

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

    DisposableCallSiteIndex callSiteIndexForExceptionHandling();

    const HandlerInfo& originalExceptionHandler();

    bool needsToRestoreRegistersIfException() const { return m_needsToRestoreRegistersIfException; }
    CallSiteIndex originalCallSiteIndex() const;

    void emitExplicitExceptionHandler();

    void setSpillStateForJSCall(SpillState& spillState)
    {
        if (!m_spillStateForJSCall.isEmpty()) {
            RELEASE_ASSERT(m_spillStateForJSCall.numberOfStackBytesUsedForRegisterPreservation == spillState.numberOfStackBytesUsedForRegisterPreservation,
                m_spillStateForJSCall.numberOfStackBytesUsedForRegisterPreservation,
                spillState.numberOfStackBytesUsedForRegisterPreservation);
            RELEASE_ASSERT(m_spillStateForJSCall.spilledRegisters == spillState.spilledRegisters,
                m_spillStateForJSCall.spilledRegisters.bitsForDebugging(),
                spillState.spilledRegisters.bitsForDebugging());
        }
        RELEASE_ASSERT(spillState.spilledRegisters.numberOfSetRegisters() || !spillState.numberOfStackBytesUsedForRegisterPreservation,
            spillState.spilledRegisters.numberOfSetRegisters(),
            spillState.numberOfStackBytesUsedForRegisterPreservation);
        m_spillStateForJSCall = spillState;
    }
    SpillState spillStateForJSCall() const
    {
        RELEASE_ASSERT(m_spillStateForJSCall.spilledRegisters.numberOfSetRegisters() || !m_spillStateForJSCall.numberOfStackBytesUsedForRegisterPreservation,
            m_spillStateForJSCall.spilledRegisters.numberOfSetRegisters(),
            m_spillStateForJSCall.numberOfStackBytesUsedForRegisterPreservation);
        return m_spillStateForJSCall;
    }

    ScratchRegisterAllocator makeDefaultScratchAllocator(GPRReg extraToLock = InvalidGPRReg);

    // Fall through on success. Two kinds of failures are supported: fall-through, which means that we
    // should try a different case; and failure, which means that this was the right case but it needs
    // help from the slow path.
    void generateWithGuard(unsigned index, AccessCase&, MacroAssembler::JumpList& fallThrough);

    // Fall through on success, add a jump to the failure list on failure.
    void generate(unsigned index, AccessCase&);

    void generateImpl(unsigned index, AccessCase&);

    static bool canEmitIntrinsicGetter(StructureStubInfo&, JSFunction*, Structure*);

    VM& vm() { return m_vm; }

    AccessGenerationResult regenerate(const GCSafeConcurrentJSLocker&, PolymorphicAccess&, CodeBlock*);

    static MacroAssemblerCodeRef<JITThunkPtrTag> generateSlowPathCode(VM&, AccessType);
    static Ref<InlineCacheHandler> generateSlowPathHandler(VM&, AccessType);

    static void emitDataICPrologue(CCallHelpers&);
    static void emitDataICEpilogue(CCallHelpers&);

    bool useHandlerIC() const;

private:
    CallSiteIndex callSiteIndexForExceptionHandlingOrOriginal();
    const ScalarRegisterSet& liveRegistersToPreserveAtExceptionHandlingCallSite();

    void emitDOMJITGetter(GetterSetterAccessCase&, const DOMJIT::GetterSetter*, GPRReg baseForGetGPR);
    void emitModuleNamespaceLoad(ModuleNamespaceAccessCase&, MacroAssembler::JumpList& fallThrough);
    void emitProxyObjectAccess(unsigned index, ProxyObjectAccessCase&, MacroAssembler::JumpList& fallThrough);
    void emitIntrinsicGetter(IntrinsicGetterAccessCase&);

    VM& m_vm;
    JSGlobalObject* const m_globalObject;
    StructureStubInfo* m_stubInfo { nullptr };
    const ECMAMode m_ecmaMode { ECMAMode::sloppy() };
    JITType m_jitType;
    CCallHelpers* m_jit { nullptr };
    ScratchRegisterAllocator* m_allocator { nullptr };
    MacroAssembler::JumpList m_success;
    MacroAssembler::JumpList m_failAndRepatch;
    MacroAssembler::JumpList m_failAndIgnore;
    ScratchRegisterAllocator::PreservedState m_preservedReusedRegisterState;
    std::unique_ptr<WatchpointsOnStructureStubInfo> m_watchpoints;
    GPRReg m_scratchGPR { InvalidGPRReg };
    FPRReg m_scratchFPR { InvalidFPRReg };
    Vector<StructureID> m_weakStructures;
    Vector<std::unique_ptr<OptimizingCallLinkInfo>> m_callLinkInfos;
    ScalarRegisterSet m_liveRegistersToPreserveAtExceptionHandlingCallSite;
    ScalarRegisterSet m_liveRegistersForCall;
    CallSiteIndex m_callSiteIndex;
    SpillState m_spillStateForJSCall;
    bool m_calculatedRegistersForCallAndExceptionHandling : 1 { false };
    bool m_needsToRestoreRegistersIfException : 1 { false };
    bool m_calculatedCallSiteIndex : 1 { false };
    bool m_doesJSCalls : 1 { false };
    bool m_doesCalls : 1 { false };
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::AccessGenerationResult::Kind);
void printInternal(PrintStream&, JSC::AccessCase::AccessType);
void printInternal(PrintStream&, JSC::AccessCase::State);

} // namespace WTF

#endif // ENABLE(JIT)
