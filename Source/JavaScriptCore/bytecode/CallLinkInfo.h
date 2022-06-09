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

#include "CallFrameShuffleData.h"
#include "CallMode.h"
#include "CodeLocation.h"
#include "CodeOrigin.h"
#include "CodeSpecializationKind.h"
#include "PolymorphicCallStubRoutine.h"
#include "WriteBarrier.h"
#include <wtf/ScopedLambda.h>
#include <wtf/SentinelLinkedList.h>

namespace JSC {

class CCallHelpers;
class ExecutableBase;
class FunctionCodeBlock;
class JSFunction;
class PolymorphicCallStubRoutine;
enum OpcodeID : unsigned;

struct CallFrameShuffleData;
struct UnlinkedCallLinkInfo;

#if ENABLE(JIT)
namespace BaselineCallRegisters {
constexpr JSValueRegs calleeJSR { JSRInfo::jsRegT10 };
constexpr GPRReg calleeGPR { GPRInfo::regT0 };
constexpr GPRReg callLinkInfoGPR { GPRInfo::regT2 };
}
#endif

class CallLinkInfo : public PackedRawSentinelNode<CallLinkInfo> {
public:
    friend class LLIntOffsetsExtractor;

    enum class Type : uint8_t {
        Baseline,
        Optimizing,
    };

    enum CallType : uint8_t {
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

    static constexpr uintptr_t polymorphicCalleeMask = 1;
    
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

    bool isLinked() const { return stub() || m_calleeOrCodeBlock; }
    void unlink(VM&);

    enum class UseDataIC : uint8_t {
        Yes,
        No
    };

#if ENABLE(JIT)
protected:
    static MacroAssembler::JumpList emitFastPathImpl(CallLinkInfo*, CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC, bool isTailCall, ScopedLambda<void()>&& prepareForTailCall) WARN_UNUSED_RETURN;
public:
    static MacroAssembler::JumpList emitDataICFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR) WARN_UNUSED_RETURN;
    static MacroAssembler::JumpList emitTailCallDataICFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall) WARN_UNUSED_RETURN;
    static void emitDataICSlowPath(VM&, CCallHelpers&, GPRReg callLinkInfoGPR);
#endif

    void revertCallToStub();

    bool isDataIC() const { return static_cast<UseDataIC>(m_useDataIC) == UseDataIC::Yes; }
    void setUsesDataICs(UseDataIC useDataIC) { m_useDataIC = static_cast<unsigned>(useDataIC); }

    bool allowStubs() const { return m_allowStubs; }

    void disallowStubs()
    {
        m_allowStubs = false;
    }

    CodeLocationLabel<JSInternalPtrTag> doneLocation();

    void setMonomorphicCallee(VM&, JSCell*, JSObject* callee, MacroAssemblerCodePtr<JSEntryPtrTag>);
    void setSlowPathCallDestination(MacroAssemblerCodePtr<JSEntryPtrTag>);
    void clearCallee();
    JSObject* callee();

    void setCodeBlock(VM&, JSCell*, FunctionCodeBlock*);
    void clearCodeBlock();
    FunctionCodeBlock* codeBlock();

    void setLastSeenCallee(VM&, const JSCell* owner, JSObject* callee);
    void clearLastSeenCallee();
    JSObject* lastSeenCallee() const;
    bool haveLastSeenCallee() const;
    
    void setExecutableDuringCompilation(ExecutableBase*);
    ExecutableBase* executable();
    
#if ENABLE(JIT)
    void setStub(Ref<PolymorphicCallStubRoutine>&&);
#endif
    void clearStub();

    PolymorphicCallStubRoutine* stub() const
    {
#if ENABLE(JIT)
        return m_stub.get();
#else
        return nullptr;
#endif
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

    static ptrdiff_t offsetOfMaxArgumentCountIncludingThis()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, m_maxArgumentCountIncludingThis);
    }

    uint32_t maxArgumentCountIncludingThis()
    {
        return m_maxArgumentCountIncludingThis;
    }
    
    void setMaxArgumentCountIncludingThis(unsigned);
    void updateMaxArgumentCountIncludingThis(unsigned argumentCountIncludingThis)
    {
        if (m_maxArgumentCountIncludingThis < argumentCountIncludingThis)
            m_maxArgumentCountIncludingThis = argumentCountIncludingThis;
    }

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

#if ENABLE(JIT)
    GPRReg calleeGPR() const;
    GPRReg callLinkInfoGPR() const;
#endif

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
            if (stub()) {
#if ENABLE(JIT)
                stub()->forEachDependentCell(functor);
#else
                RELEASE_ASSERT_NOT_REACHED();
#endif
            } else {
                functor(m_calleeOrCodeBlock.get());
                if (isDirect())
                    functor(m_lastSeenCalleeOrExecutable.get());
            }
        }
        if (!isDirect() && haveLastSeenCallee())
            functor(lastSeenCallee());
    }

    void visitWeak(VM&);

#if ENABLE(JIT)
    void setFrameShuffleData(const CallFrameShuffleData&);

    const CallFrameShuffleData* frameShuffleData()
    {
        return m_frameShuffleData.get();
    }
#endif

    Type type() const { return static_cast<Type>(m_type); }

protected:
    CallLinkInfo(Type type, CodeOrigin codeOrigin)
        : m_codeOrigin(codeOrigin)
        , m_hasSeenShouldRepatch(false)
        , m_hasSeenClosure(false)
        , m_clearedByGC(false)
        , m_clearedByVirtual(false)
        , m_allowStubs(true)
        , m_clearedByJettison(false)
        , m_callType(None)
        , m_useDataIC(static_cast<unsigned>(UseDataIC::Yes))
        , m_type(static_cast<unsigned>(type))
    {
        ASSERT(type == this->type());
    }

    CallLinkInfo(Type type)
        : CallLinkInfo(type, CodeOrigin { })
    {
    }

#if ENABLE(JIT)
    void setCallLinkInfoGPR(GPRReg);
#endif

    uint32_t m_maxArgumentCountIncludingThis { 0 }; // For varargs: the profiled maximum number of arguments. For direct: the number of stack slots allocated for arguments.
    CodeLocationLabel<JSInternalPtrTag> m_doneLocation;
    MacroAssemblerCodePtr<JSEntryPtrTag> m_slowPathCallDestination;
    union UnionType {
        UnionType()
            : dataIC { nullptr }
        { }

        struct DataIC {
            MacroAssemblerCodePtr<JSEntryPtrTag> m_monomorphicCallDestination;
        } dataIC;

        struct {
            CodeLocationDataLabelPtr<JSInternalPtrTag> m_calleeLocation;
        } codeIC;
    } u;

    WriteBarrier<JSCell> m_calleeOrCodeBlock;
    WriteBarrier<JSCell> m_lastSeenCalleeOrExecutable;
#if ENABLE(JIT)
    RefPtr<PolymorphicCallStubRoutine> m_stub;
    std::unique_ptr<CallFrameShuffleData> m_frameShuffleData;
#endif
    CodeOrigin m_codeOrigin;
    bool m_hasSeenShouldRepatch : 1;
    bool m_hasSeenClosure : 1;
    bool m_clearedByGC : 1;
    bool m_clearedByVirtual : 1;
    bool m_allowStubs : 1;
    bool m_clearedByJettison : 1;
    unsigned m_callType : 4; // CallType
    unsigned m_useDataIC : 1; // UseDataIC
    unsigned m_type : 1; // Type
#if ENABLE(JIT)
    GPRReg m_calleeGPR { InvalidGPRReg };
    GPRReg m_callLinkInfoGPR { InvalidGPRReg };
#endif
    uint32_t m_slowPathCount { 0 };
};

class BaselineCallLinkInfo final : public CallLinkInfo {
public:
    BaselineCallLinkInfo()
        : CallLinkInfo(Type::Baseline)
    {
    }

    void initialize(VM&, CallType, BytecodeIndex, CallFrameShuffleData*);

    void setCodeLocations(CodeLocationLabel<JSInternalPtrTag> doneLocation)
    {
        m_doneLocation = doneLocation;
    }

#if ENABLE(JIT)
    static constexpr GPRReg calleeGPR() { return BaselineCallRegisters::calleeGPR; }
    static constexpr GPRReg callLinkInfoGPR() { return BaselineCallRegisters::callLinkInfoGPR; }
    void setCallLinkInfoGPR(GPRReg callLinkInfoGPR) { RELEASE_ASSERT(callLinkInfoGPR == BaselineCallRegisters::callLinkInfoGPR); }
#endif
};

inline CodeOrigin getCallLinkInfoCodeOrigin(CallLinkInfo& callLinkInfo)
{
    return callLinkInfo.codeOrigin();
}

struct UnlinkedCallLinkInfo {
    BytecodeIndex bytecodeIndex; // Currently, only used by baseline, so this can trivially produce a CodeOrigin.
    CodeLocationLabel<JSInternalPtrTag> doneLocation;
};

#if ENABLE(JIT)

class OptimizingCallLinkInfo final : public CallLinkInfo {
public:
    friend class CallLinkInfo;

    OptimizingCallLinkInfo(CodeOrigin codeOrigin)
        : CallLinkInfo(Type::Optimizing, codeOrigin)
    {
    }

    void setUpCall(CallType callType, GPRReg calleeGPR)
    {
        m_callType = callType;
        m_calleeGPR = calleeGPR;
    }

    void setCodeLocations(
        CodeLocationLabel<JSInternalPtrTag> slowPathStart,
        CodeLocationLabel<JSInternalPtrTag> doneLocation)
    {
        if (!isDataIC())
            m_slowPathStart = slowPathStart;
        m_doneLocation = doneLocation;
    }

    CodeLocationLabel<JSInternalPtrTag> fastPathStart();
    CodeLocationLabel<JSInternalPtrTag> slowPathStart();

    GPRReg calleeGPR() const { return m_calleeGPR; }
    GPRReg callLinkInfoGPR() const { return m_callLinkInfoGPR; }
    void setCallLinkInfoGPR(GPRReg callLinkInfoGPR) { m_callLinkInfoGPR = callLinkInfoGPR; }

    void emitDirectFastPath(CCallHelpers&);
    void emitDirectTailCallFastPath(CCallHelpers&, ScopedLambda<void()>&& prepareForTailCall);
    void initializeDirectCall();
    void setDirectCallTarget(CodeLocationLabel<JSEntryPtrTag>);
    void emitSlowPath(VM&, CCallHelpers&);

    MacroAssembler::JumpList emitFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC) WARN_UNUSED_RETURN;
    MacroAssembler::JumpList emitTailCallFastPath(CCallHelpers&, GPRReg calleeGPR, ScopedLambda<void()>&& prepareForTailCall) WARN_UNUSED_RETURN;

private:
    CodeLocationNearCall<JSInternalPtrTag> m_callLocation;
    CodeLocationLabel<JSInternalPtrTag> m_slowPathStart;
    CodeLocationLabel<JSInternalPtrTag> m_fastPathStart;
};

inline GPRReg CallLinkInfo::calleeGPR() const
{
    switch (type()) {
    case Type::Baseline:
        return static_cast<const BaselineCallLinkInfo*>(this)->calleeGPR();
    case Type::Optimizing:
        return static_cast<const OptimizingCallLinkInfo*>(this)->calleeGPR();
    }
    return InvalidGPRReg;
}

inline GPRReg CallLinkInfo::callLinkInfoGPR() const
{
    switch (type()) {
    case Type::Baseline:
        return static_cast<const BaselineCallLinkInfo*>(this)->callLinkInfoGPR();
    case Type::Optimizing:
        return static_cast<const OptimizingCallLinkInfo*>(this)->callLinkInfoGPR();
    }
    return InvalidGPRReg;
}

inline void CallLinkInfo::setCallLinkInfoGPR(GPRReg callLinkInfoGPR)
{
    switch (type()) {
    case Type::Baseline:
        return static_cast<BaselineCallLinkInfo*>(this)->setCallLinkInfoGPR(callLinkInfoGPR);
    case Type::Optimizing:
        return static_cast<OptimizingCallLinkInfo*>(this)->setCallLinkInfoGPR(callLinkInfoGPR);
    }
}

#endif

} // namespace JSC
