/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef PolymorphicAccess_h
#define PolymorphicAccess_h

#if ENABLE(JIT)

#include "CodeOrigin.h"
#include "JSFunctionInlines.h"
#include "MacroAssembler.h"
#include "ObjectPropertyConditionSet.h"
#include "Opcode.h"
#include "ScratchRegisterAllocator.h"
#include "Structure.h"
#include <wtf/Vector.h>

namespace JSC {

class CodeBlock;
class PolymorphicAccess;
class StructureStubInfo;
class WatchpointsOnStructureStubInfo;
class ScratchRegisterAllocator;

struct AccessGenerationState;

// An AccessCase describes one of the cases of a PolymorphicAccess. A PolymorphicAccess represents a
// planned (to generate in future) or generated stub for some inline cache. That stub contains fast
// path code for some finite number of fast cases, each described by an AccessCase object.
//
// An AccessCase object has a lifecycle that proceeds through several states. Note that the states
// of AccessCase have a lot to do with the global effect epoch (we'll say epoch for short). This is
// a simple way of reasoning about the state of the system outside this AccessCase. Any observable
// effect - like storing to a property, changing an object's structure, etc. - increments the epoch.
// The states are:
//
// Primordial:   This is an AccessCase that was just allocated. It does not correspond to any actual
//               code and it is not owned by any PolymorphicAccess. In this state, the AccessCase
//               assumes that it is in the same epoch as when it was created. This is important
//               because it may make claims about itself ("I represent a valid case so long as you
//               register a watchpoint on this set") that could be contradicted by some outside
//               effects (like firing and deleting the watchpoint set in question). This is also the
//               state that an AccessCase is in when it is cloned (AccessCase::clone()).
//
// Committed:    This happens as soon as some PolymorphicAccess takes ownership of this AccessCase.
//               In this state, the AccessCase no longer assumes anything about the epoch. To
//               accomplish this, PolymorphicAccess calls AccessCase::commit(). This must be done
//               during the same epoch when the AccessCase was created, either by the client or by
//               clone(). When created by the client, committing during the same epoch works because
//               we can be sure that whatever watchpoint sets they spoke of are still valid. When
//               created by clone(), we can be sure that the set is still valid because the original
//               of the clone still has watchpoints on it.
//
// Generated:    This is the state when the PolymorphicAccess generates code for this case by
//               calling AccessCase::generate() or AccessCase::generateWithGuard(). At this point
//               the case object will have some extra stuff in it, like possibly the CallLinkInfo
//               object associated with the inline cache.
//               FIXME: Moving into the Generated state should not mutate the AccessCase object or
//               put more stuff into it. If we fix this, then we can get rid of AccessCase::clone().
//               https://bugs.webkit.org/show_bug.cgi?id=156456
//
// An AccessCase may be destroyed while in any of these states.
//
// Note that right now, an AccessCase goes from Primordial to Generated quite quickly.
// FIXME: Make it possible for PolymorphicAccess to hold onto AccessCases that haven't been
// generated. That would allow us to significantly reduce the number of regeneration events.
// https://bugs.webkit.org/show_bug.cgi?id=156457
class AccessCase {
    WTF_MAKE_NONCOPYABLE(AccessCase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum AccessType : uint8_t {
        Load,
        MegamorphicLoad,
        Transition,
        Replace,
        Miss,
        GetGetter,
        Getter,
        Setter,
        CustomValueGetter,
        CustomAccessorGetter,
        CustomValueSetter,
        CustomAccessorSetter,
        IntrinsicGetter,
        InHit,
        InMiss,
        ArrayLength,
        StringLength,
        DirectArgumentsLength,
        ScopedArgumentsLength
    };
    
    enum State : uint8_t {
        Primordial,
        Committed,
        Generated
    };

    static std::unique_ptr<AccessCase> tryGet(
        VM&, JSCell* owner, AccessType, PropertyOffset, Structure*,
        const ObjectPropertyConditionSet& = ObjectPropertyConditionSet(),
        bool viaProxy = false,
        WatchpointSet* additionalSet = nullptr);

    static std::unique_ptr<AccessCase> get(
        VM&, JSCell* owner, AccessType, PropertyOffset, Structure*,
        const ObjectPropertyConditionSet& = ObjectPropertyConditionSet(),
        bool viaProxy = false,
        WatchpointSet* additionalSet = nullptr,
        PropertySlot::GetValueFunc = nullptr,
        JSObject* customSlotBase = nullptr);
    
    static std::unique_ptr<AccessCase> megamorphicLoad(VM&, JSCell* owner);
    
    static std::unique_ptr<AccessCase> replace(VM&, JSCell* owner, Structure*, PropertyOffset);

    static std::unique_ptr<AccessCase> transition(
        VM&, JSCell* owner, Structure* oldStructure, Structure* newStructure, PropertyOffset,
        const ObjectPropertyConditionSet& = ObjectPropertyConditionSet());

    static std::unique_ptr<AccessCase> setter(
        VM&, JSCell* owner, AccessType, Structure*, PropertyOffset,
        const ObjectPropertyConditionSet&, PutPropertySlot::PutValueFunc = nullptr,
        JSObject* customSlotBase = nullptr);

    static std::unique_ptr<AccessCase> in(
        VM&, JSCell* owner, AccessType, Structure*,
        const ObjectPropertyConditionSet& = ObjectPropertyConditionSet());

    static std::unique_ptr<AccessCase> getLength(VM&, JSCell* owner, AccessType);
    static std::unique_ptr<AccessCase> getIntrinsic(VM&, JSCell* owner, JSFunction* intrinsic, PropertyOffset, Structure*, const ObjectPropertyConditionSet&);
    
    static std::unique_ptr<AccessCase> fromStructureStubInfo(VM&, JSCell* owner, StructureStubInfo&);

    ~AccessCase();
    
    AccessType type() const { return m_type; }
    State state() const { return m_state; }
    PropertyOffset offset() const { return m_offset; }
    bool viaProxy() const { return m_rareData ? m_rareData->viaProxy : false; }
    
    Structure* structure() const
    {
        if (m_type == Transition)
            return m_structure->previousID();
        return m_structure.get();
    }
    bool guardedByStructureCheck() const;

    Structure* newStructure() const
    {
        ASSERT(m_type == Transition);
        return m_structure.get();
    }
    
    ObjectPropertyConditionSet conditionSet() const { return m_conditionSet; }
    JSFunction* intrinsicFunction() const
    {
        ASSERT(type() == IntrinsicGetter && m_rareData);
        return m_rareData->intrinsicFunction.get();
    }
    Intrinsic intrinsic() const
    {
        return intrinsicFunction()->intrinsic();
    }

    WatchpointSet* additionalSet() const
    {
        return m_rareData ? m_rareData->additionalSet.get() : nullptr;
    }

    JSObject* customSlotBase() const
    {
        return m_rareData ? m_rareData->customSlotBase.get() : nullptr;
    }

    JSObject* alternateBase() const;

    // If you supply the optional vector, this will append the set of cells that this will need to keep alive
    // past the call.
    bool doesCalls(Vector<JSCell*>* cellsToMark = nullptr) const;

    bool isGetter() const
    {
        switch (type()) {
        case Getter:
        case CustomValueGetter:
        case CustomAccessorGetter:
            return true;
        default:
            return false;
        }
    }

    // This can return null even for a getter/setter, if it hasn't been generated yet. That's
    // actually somewhat likely because of how we do buffering of new cases.
    CallLinkInfo* callLinkInfo() const
    {
        if (!m_rareData)
            return nullptr;
        return m_rareData->callLinkInfo.get();
    }
    
    // Is it still possible for this case to ever be taken?  Must call this as a prerequisite for
    // calling generate() and friends.  If this returns true, then you can call generate().  If
    // this returns false, then generate() will crash.  You must call generate() in the same epoch
    // as when you called couldStillSucceed().
    bool couldStillSucceed() const;
    
    static bool canEmitIntrinsicGetter(JSFunction*, Structure*);

    bool canBeReplacedByMegamorphicLoad() const;

    // If this method returns true, then it's a good idea to remove 'other' from the access once 'this'
    // is added. This method assumes that in case of contradictions, 'this' represents a newer, and so
    // more useful, truth. This method can be conservative; it will return false when it doubt.
    bool canReplace(const AccessCase& other) const;

    void dump(PrintStream& out) const;
    
private:
    friend class CodeBlock;
    friend class PolymorphicAccess;

    AccessCase();

    bool visitWeak(VM&) const;
    
    // FIXME: This only exists because of how AccessCase puts post-generation things into itself.
    // https://bugs.webkit.org/show_bug.cgi?id=156456
    std::unique_ptr<AccessCase> clone() const;
    
    // Perform any action that must be performed before the end of the epoch in which the case
    // was created. Returns a set of watchpoint sets that will need to be watched.
    Vector<WatchpointSet*, 2> commit(VM&, const Identifier&);

    // Fall through on success. Two kinds of failures are supported: fall-through, which means that we
    // should try a different case; and failure, which means that this was the right case but it needs
    // help from the slow path.
    void generateWithGuard(AccessGenerationState&, MacroAssembler::JumpList& fallThrough);

    // Fall through on success, add a jump to the failure list on failure.
    void generate(AccessGenerationState&);
    
    void generateImpl(AccessGenerationState&);
    void emitIntrinsicGetter(AccessGenerationState&);
    
    AccessType m_type { Load };
    State m_state { Primordial };
    PropertyOffset m_offset { invalidOffset };

    // Usually this is the structure that we expect the base object to have. But, this is the *new*
    // structure for a transition and we rely on the fact that it has a strong reference to the old
    // structure. For proxies, this is the structure of the object behind the proxy.
    WriteBarrier<Structure> m_structure;

    ObjectPropertyConditionSet m_conditionSet;

    class RareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RareData()
            : viaProxy(false)
        {
            customAccessor.opaque = nullptr;
        }
        
        bool viaProxy;
        RefPtr<WatchpointSet> additionalSet;
        // FIXME: This should probably live in the stub routine object.
        // https://bugs.webkit.org/show_bug.cgi?id=156456
        std::unique_ptr<CallLinkInfo> callLinkInfo;
        union {
            PropertySlot::GetValueFunc getter;
            PutPropertySlot::PutValueFunc setter;
            void* opaque;
        } customAccessor;
        WriteBarrier<JSObject> customSlotBase;
        WriteBarrier<JSFunction> intrinsicFunction;
    };

    std::unique_ptr<RareData> m_rareData;
};

class AccessGenerationResult {
public:
    enum Kind {
        MadeNoChanges,
        GaveUp,
        Buffered,
        GeneratedNewCode,
        GeneratedFinalCode // Generated so much code that we never want to generate code again.
    };
    
    AccessGenerationResult()
    {
    }
    
    AccessGenerationResult(Kind kind)
        : m_kind(kind)
    {
        RELEASE_ASSERT(kind != GeneratedNewCode);
        RELEASE_ASSERT(kind != GeneratedFinalCode);
    }
    
    AccessGenerationResult(Kind kind, MacroAssemblerCodePtr code)
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
    
    const MacroAssemblerCodePtr& code() const { return m_code; }
    
    bool madeNoChanges() const { return m_kind == MadeNoChanges; }
    bool gaveUp() const { return m_kind == GaveUp; }
    bool buffered() const { return m_kind == Buffered; }
    bool generatedNewCode() const { return m_kind == GeneratedNewCode; }
    bool generatedFinalCode() const { return m_kind == GeneratedFinalCode; }
    
    // If we gave up on this attempt to generate code, or if we generated the "final" code, then we
    // should give up after this.
    bool shouldGiveUpNow() const { return gaveUp() || generatedFinalCode(); }
    
    bool generatedSomeCode() const { return generatedNewCode() || generatedFinalCode(); }
    
    void dump(PrintStream&) const;
    
private:
    Kind m_kind;
    MacroAssemblerCodePtr m_code;
};

class PolymorphicAccess {
    WTF_MAKE_NONCOPYABLE(PolymorphicAccess);
    WTF_MAKE_FAST_ALLOCATED;
public:
    PolymorphicAccess();
    ~PolymorphicAccess();

    // When this fails (returns GaveUp), this will leave the old stub intact but you should not try
    // to call this method again for that PolymorphicAccess instance.
    AccessGenerationResult addCases(
        VM&, CodeBlock*, StructureStubInfo&, const Identifier&, Vector<std::unique_ptr<AccessCase>>);

    AccessGenerationResult addCase(
        VM&, CodeBlock*, StructureStubInfo&, const Identifier&, std::unique_ptr<AccessCase>);
    
    AccessGenerationResult regenerate(VM&, CodeBlock*, StructureStubInfo&, const Identifier&);
    
    bool isEmpty() const { return m_list.isEmpty(); }
    unsigned size() const { return m_list.size(); }
    const AccessCase& at(unsigned i) const { return *m_list[i]; }
    const AccessCase& operator[](unsigned i) const { return *m_list[i]; }

    // If this returns false then we are requesting a reset of the owning StructureStubInfo.
    bool visitWeak(VM&) const;

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
    
    typedef Vector<std::unique_ptr<AccessCase>, 2> ListType;
    
    void commit(
        VM&, std::unique_ptr<WatchpointsOnStructureStubInfo>&, CodeBlock*, StructureStubInfo&,
        const Identifier&, AccessCase&);

    MacroAssemblerCodePtr regenerate(
        VM&, CodeBlock*, StructureStubInfo&, const Identifier&, ListType& cases);

    ListType m_list;
    RefPtr<JITStubRoutine> m_stubRoutine;
    std::unique_ptr<WatchpointsOnStructureStubInfo> m_watchpoints;
    std::unique_ptr<Vector<WriteBarrier<JSCell>>> m_weakReferences;
};

struct AccessGenerationState {
    AccessGenerationState()
        : m_calculatedRegistersForCallAndExceptionHandling(false)
        , m_needsToRestoreRegistersIfException(false)
        , m_calculatedCallSiteIndex(false)
    {
    }
    CCallHelpers* jit { nullptr };
    ScratchRegisterAllocator* allocator;
    ScratchRegisterAllocator::PreservedState preservedReusedRegisterState;
    PolymorphicAccess* access { nullptr };
    StructureStubInfo* stubInfo { nullptr };
    MacroAssembler::JumpList success;
    MacroAssembler::JumpList failAndRepatch;
    MacroAssembler::JumpList failAndIgnore;
    GPRReg baseGPR { InvalidGPRReg };
    JSValueRegs valueRegs;
    GPRReg scratchGPR { InvalidGPRReg };
    const Identifier* ident;
    std::unique_ptr<WatchpointsOnStructureStubInfo> watchpoints;
    Vector<WriteBarrier<JSCell>> weakReferences;

    Watchpoint* addWatchpoint(const ObjectPropertyCondition& = ObjectPropertyCondition());

    void restoreScratch();
    void succeed();

    void calculateLiveRegistersForCallAndExceptionHandling(const RegisterSet& extra = RegisterSet());

    void preserveLiveRegistersToStackForCall(const RegisterSet& extra = RegisterSet());

    void restoreLiveRegistersFromStackForCall(bool isGetter = false);
    void restoreLiveRegistersFromStackForCallWithThrownException();
    void restoreLiveRegistersFromStackForCall(const RegisterSet& dontRestore);

    const RegisterSet& liveRegistersForCall()
    {
        RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
        return m_liveRegistersForCall;
    }

    CallSiteIndex callSiteIndexForExceptionHandlingOrOriginal();
    CallSiteIndex callSiteIndexForExceptionHandling()
    {
        RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
        RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
        RELEASE_ASSERT(m_calculatedCallSiteIndex);
        return m_callSiteIndex;
    }

    const HandlerInfo& originalExceptionHandler() const;
    unsigned numberOfStackBytesUsedForRegisterPreservation() const
    {
        RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
        return m_numberOfStackBytesUsedForRegisterPreservation;
    }

    bool needsToRestoreRegistersIfException() const { return m_needsToRestoreRegistersIfException; }
    CallSiteIndex originalCallSiteIndex() const;
    
    void emitExplicitExceptionHandler();
    
private:
    const RegisterSet& liveRegistersToPreserveAtExceptionHandlingCallSite()
    {
        RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
        return m_liveRegistersToPreserveAtExceptionHandlingCallSite;
    }
    
    RegisterSet m_liveRegistersToPreserveAtExceptionHandlingCallSite;
    RegisterSet m_liveRegistersForCall;
    CallSiteIndex m_callSiteIndex { CallSiteIndex(std::numeric_limits<unsigned>::max()) };
    unsigned m_numberOfStackBytesUsedForRegisterPreservation { std::numeric_limits<unsigned>::max() };
    bool m_calculatedRegistersForCallAndExceptionHandling : 1;
    bool m_needsToRestoreRegistersIfException : 1;
    bool m_calculatedCallSiteIndex : 1;
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::AccessGenerationResult::Kind);
void printInternal(PrintStream&, JSC::AccessCase::AccessType);
void printInternal(PrintStream&, JSC::AccessCase::State);

} // namespace WTF

#endif // ENABLE(JIT)

#endif // PolymorphicAccess_h

