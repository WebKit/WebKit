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

class AccessCase {
    WTF_MAKE_NONCOPYABLE(AccessCase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum AccessType {
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
    
    std::unique_ptr<AccessCase> clone() const;
    
    AccessType type() const { return m_type; }
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

    CallLinkInfo* callLinkInfo() const
    {
        if (!m_rareData)
            return nullptr;
        return m_rareData->callLinkInfo.get();
    }

    // Is it still possible for this case to ever be taken?
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

    // Fall through on success. Two kinds of failures are supported: fall-through, which means that we
    // should try a different case; and failure, which means that this was the right case but it needs
    // help from the slow path.
    void generateWithGuard(AccessGenerationState&, MacroAssembler::JumpList& fallThrough);

    // Fall through on success, add a jump to the failure list on failure.
    void generate(AccessGenerationState&);
    void emitIntrinsicGetter(AccessGenerationState&);
    
    AccessType m_type { Load };
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
        GeneratedNewCode
    };
    
    AccessGenerationResult()
    {
    }
    
    AccessGenerationResult(Kind kind)
        : m_kind(kind)
    {
        ASSERT(kind != GeneratedNewCode);
    }
    
    AccessGenerationResult(MacroAssemblerCodePtr code)
        : m_kind(GeneratedNewCode)
        , m_code(code)
    {
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
    bool generatedNewCode() const { return m_kind == GeneratedNewCode; }
    
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

    // This may return null, in which case the old stub routine is left intact. You are required to
    // pass a vector of non-null access cases. This will prune the access cases by rejecting any case
    // in the list that is subsumed by a later case in the list.
    AccessGenerationResult regenerateWithCases(
        VM&, CodeBlock*, StructureStubInfo&, const Identifier&, Vector<std::unique_ptr<AccessCase>>);

    AccessGenerationResult regenerateWithCase(
        VM&, CodeBlock*, StructureStubInfo&, const Identifier&, std::unique_ptr<AccessCase>);
    
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

} // namespace WTF

#endif // ENABLE(JIT)

#endif // PolymorphicAccess_h

