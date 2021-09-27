/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#include "CallMode.h"
#include "CodeLocation.h"
#include "CodeSpecializationKind.h"
#include "PolymorphicCallStubRoutine.h"
#include "WriteBarrier.h"
#include <wtf/Function.h>
#include <wtf/SentinelLinkedList.h>

namespace JSC {

#if ENABLE(JIT)

class CCallHelpers;
class FunctionCodeBlock;
class JSFunction;
enum OpcodeID : unsigned;
struct CallFrameShuffleData;

class CallLinkInfo : public PackedRawSentinelNode<CallLinkInfo> {
public:
    enum CallType {
        None,
        Call,
        CallVarargs,
        Construct,
        ConstructVarargs,
        TailCall,
        TailCallVarargs,
        DirectCall,
        DirectConstruct,
        DirectTailCall
    };
    
    static CallType callTypeFor(OpcodeID opcodeID);

    static bool isVarargsCallType(CallType callType)
    {
        switch (callType) {
        case CallVarargs:
        case ConstructVarargs:
        case TailCallVarargs:
            return true;

        default:
            return false;
        }
    }

    CallLinkInfo(CodeOrigin);
        
    ~CallLinkInfo();
    
    static CodeSpecializationKind specializationKindFor(CallType callType)
    {
        return specializationFromIsConstruct(callType == Construct || callType == ConstructVarargs || callType == DirectConstruct);
    }
    CodeSpecializationKind specializationKind() const
    {
        return specializationKindFor(static_cast<CallType>(m_callType));
    }
    
    static CallMode callModeFor(CallType callType)
    {
        switch (callType) {
        case Call:
        case CallVarargs:
        case DirectCall:
            return CallMode::Regular;
        case TailCall:
        case TailCallVarargs:
        case DirectTailCall:
            return CallMode::Tail;
        case Construct:
        case ConstructVarargs:
        case DirectConstruct:
            return CallMode::Construct;
        case None:
            RELEASE_ASSERT_NOT_REACHED();
        }

        RELEASE_ASSERT_NOT_REACHED();
    }
    
    static bool isDirect(CallType callType)
    {
        switch (callType) {
        case DirectCall:
        case DirectTailCall:
        case DirectConstruct:
            return true;
        case Call:
        case CallVarargs:
        case TailCall:
        case TailCallVarargs:
        case Construct:
        case ConstructVarargs:
            return false;
        case None:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
    
    CallMode callMode() const
    {
        return callModeFor(static_cast<CallType>(m_callType));
    }

    bool isDirect() const
    {
        return isDirect(static_cast<CallType>(m_callType));
    }

    bool isTailCall() const
    {
        return callMode() == CallMode::Tail;
    }
    
    NearCallMode nearCallMode() const
    {
        return isTailCall() ? NearCallMode::Tail : NearCallMode::Regular;
    }

    bool isVarargs() const
    {
        return isVarargsCallType(static_cast<CallType>(m_callType));
    }

    bool isLinked() const { return m_stub || m_calleeOrCodeBlock; }
    void unlink(VM&);

    void setUpCall(CallType callType, GPRReg calleeGPR)
    {
        m_callType = callType;
        m_calleeGPR = calleeGPR;
    }

    GPRReg calleeGPR() const { return m_calleeGPR; }
    
    enum class UseDataIC : uint8_t {
        Yes,
        No
    };

private:
    MacroAssembler::JumpList emitFastPathImpl(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC, WTF::Function<void()> prepareForTailCall) WARN_UNUSED_RETURN;
public:
    MacroAssembler::JumpList emitFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC) WARN_UNUSED_RETURN;
    MacroAssembler::JumpList emitTailCallFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC, WTF::Function<void()> prepareForTailCall) WARN_UNUSED_RETURN;
    void emitDirectFastPath(CCallHelpers&);
    void emitDirectTailCallFastPath(CCallHelpers&, WTF::Function<void()> prepareForTailCall);
    void emitSlowPath(VM&, CCallHelpers&);
    void revertCallToStub();

    bool isDataIC() const { return static_cast<UseDataIC>(m_useDataIC) == UseDataIC::Yes; }
    void setUsesDataICs(UseDataIC useDataIC) { m_useDataIC = static_cast<unsigned>(useDataIC); }

    void setCodeLocations(
        CodeLocationLabel<JSInternalPtrTag> slowPathStart,
        CodeLocationLabel<JSInternalPtrTag> doneLocation)
    {
        if (!isDataIC())
            u.codeIC.m_slowPathStart = slowPathStart;
        m_doneLocation = doneLocation;
    }

    void initializeDirectCall();

    bool allowStubs() const { return m_allowStubs; }

    void disallowStubs()
    {
        m_allowStubs = false;
    }

    CodeLocationLabel<JSInternalPtrTag> fastPathStart();
    CodeLocationLabel<JSInternalPtrTag> slowPathStart();
    CodeLocationLabel<JSInternalPtrTag> doneLocation();

    void setMonomorphicCallee(VM&, JSCell*, JSObject* callee, MacroAssemblerCodePtr<JSEntryPtrTag>);
    void setSlowPathCallDestination(MacroAssemblerCodePtr<JSEntryPtrTag>);
    void clearCallee();
    JSObject* callee();

    void setCodeBlock(VM&, JSCell*, FunctionCodeBlock*);
    void clearCodeBlock();
    FunctionCodeBlock* codeBlock();
    void setDirectCallTarget(CodeLocationLabel<JSEntryPtrTag>);

    void setLastSeenCallee(VM&, const JSCell* owner, JSObject* callee);
    void clearLastSeenCallee();
    JSObject* lastSeenCallee() const;
    bool haveLastSeenCallee() const;
    
    void setExecutableDuringCompilation(ExecutableBase*);
    ExecutableBase* executable();
    
    void setStub(Ref<PolymorphicCallStubRoutine>&&);
    void clearStub();

    PolymorphicCallStubRoutine* stub() const
    {
        return m_stub.get();
    }

    void setSlowStub(Ref<JITStubRoutine>&& newSlowStub)
    {
        m_slowStub = WTFMove(newSlowStub);
    }

    void clearSlowStub()
    {
        m_slowStub = nullptr;
    }

    JITStubRoutine* slowStub()
    {
        return m_slowStub.get();
    }

    bool seenOnce()
    {
        return m_hasSeenShouldRepatch;
    }

    void clearSeen()
    {
        m_hasSeenShouldRepatch = false;
    }

    void setSeen()
    {
        m_hasSeenShouldRepatch = true;
    }

    bool hasSeenClosure()
    {
        return m_hasSeenClosure;
    }

    void setHasSeenClosure()
    {
        m_hasSeenClosure = true;
    }

    bool clearedByGC()
    {
        return m_clearedByGC;
    }
    
    bool clearedByVirtual()
    {
        return m_clearedByVirtual;
    }

    bool clearedByJettison()
    {
        return m_clearedByJettison;
    }

    void setClearedByVirtual()
    {
        m_clearedByVirtual = true;
    }

    void setClearedByJettison()
    {
        m_clearedByJettison = true;
    }
    
    void setCallType(CallType callType)
    {
        m_callType = callType;
    }

    CallType callType()
    {
        return static_cast<CallType>(m_callType);
    }

    uint32_t* addressOfMaxArgumentCountIncludingThis()
    {
        return &m_maxArgumentCountIncludingThis;
    }

    uint32_t maxArgumentCountIncludingThis()
    {
        return m_maxArgumentCountIncludingThis;
    }
    
    void setMaxArgumentCountIncludingThis(unsigned);

    static ptrdiff_t offsetOfSlowPathCount()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, m_slowPathCount);
    }

    static ptrdiff_t offsetOfCallee()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, m_calleeOrCodeBlock);
    }

    static ptrdiff_t offsetOfMonomorphicCallDestination()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, u) + OBJECT_OFFSETOF(UnionType, dataIC.m_monomorphicCallDestination);
    }

    static ptrdiff_t offsetOfSlowPathCallDestination()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, m_slowPathCallDestination);
    }

    GPRReg calleeGPR()
    {
        return m_calleeGPR;
    }

    uint32_t slowPathCount()
    {
        return m_slowPathCount;
    }

    CodeOrigin codeOrigin()
    {
        return m_codeOrigin;
    }

    template<typename Functor>
    void forEachDependentCell(const Functor& functor) const
    {
        if (isLinked()) {
            if (stub())
                stub()->forEachDependentCell(functor);
            else {
                functor(m_calleeOrCodeBlock.get());
                if (isDirect())
                    functor(m_lastSeenCalleeOrExecutable.get());
            }
        }
        if (!isDirect() && haveLastSeenCallee())
            functor(lastSeenCallee());
    }

    void visitWeak(VM&);

    void setFrameShuffleData(const CallFrameShuffleData&);

    const CallFrameShuffleData* frameShuffleData()
    {
        return m_frameShuffleData.get();
    }

private:
    CodeLocationLabel<JSInternalPtrTag> m_fastPathStart;
    CodeLocationLabel<JSInternalPtrTag> m_doneLocation;
    MacroAssemblerCodePtr<JSEntryPtrTag> m_slowPathCallDestination;
    union UnionType {
        UnionType() 
            : dataIC { nullptr, InvalidGPRReg }
        { }
        struct DataIC {
            MacroAssemblerCodePtr<JSEntryPtrTag> m_monomorphicCallDestination;
            GPRReg m_callLinkInfoGPR;
        } dataIC;

        struct {
            CodeLocationNearCall<JSInternalPtrTag> m_callLocation;
            CodeLocationDataLabelPtr<JSInternalPtrTag> m_calleeLocation;
            CodeLocationLabel<JSInternalPtrTag> m_slowPathStart;
        } codeIC;
    } u;

    WriteBarrier<JSCell> m_calleeOrCodeBlock;
    WriteBarrier<JSCell> m_lastSeenCalleeOrExecutable;
    RefPtr<PolymorphicCallStubRoutine> m_stub;
    RefPtr<JITStubRoutine> m_slowStub;
    std::unique_ptr<CallFrameShuffleData> m_frameShuffleData;
    CodeOrigin m_codeOrigin;
    bool m_hasSeenShouldRepatch : 1;
    bool m_hasSeenClosure : 1;
    bool m_clearedByGC : 1;
    bool m_clearedByVirtual : 1;
    bool m_allowStubs : 1;
    bool m_clearedByJettison : 1;
    unsigned m_callType : 4; // CallType
    unsigned m_useDataIC : 1; // UseDataIC
    GPRReg m_calleeGPR { InvalidGPRReg };
    uint32_t m_slowPathCount { 0 };
    uint32_t m_maxArgumentCountIncludingThis { 0 }; // For varargs: the profiled maximum number of arguments. For direct: the number of stack slots allocated for arguments.
};

inline CodeOrigin getCallLinkInfoCodeOrigin(CallLinkInfo& callLinkInfo)
{
    return callLinkInfo.codeOrigin();
}

#endif // ENABLE(JIT)

} // namespace JSC
