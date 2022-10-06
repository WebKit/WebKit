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

#include "BaselineJITRegisters.h"
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
namespace DFG {
struct UnlinkedCallLinkInfo;
}

class CCallHelpers;
class ExecutableBase;
class FunctionCodeBlock;
class JSFunction;
class OptimizingCallLinkInfo;
class PolymorphicCallStubRoutine;
enum OpcodeID : unsigned;

struct CallFrameShuffleData;
struct UnlinkedCallLinkInfo;
struct BaselineUnlinkedCallLinkInfo;

using CompileTimeCallLinkInfo = std::variant<OptimizingCallLinkInfo*, BaselineUnlinkedCallLinkInfo*, DFG::UnlinkedCallLinkInfo*>;

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
    static MacroAssembler::JumpList emitDataICFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR) WARN_UNUSED_RETURN;
    static void emitDataICSlowPath(VM&, CCallHelpers&, GPRReg callLinkInfoGPR);
    static MacroAssembler::JumpList emitTailCallDataICFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall) WARN_UNUSED_RETURN;

public:
    static MacroAssembler::JumpList emitFastPath(CCallHelpers&, CompileTimeCallLinkInfo, GPRReg calleeGPR, GPRReg callLinkInfoGPR) WARN_UNUSED_RETURN;
    static MacroAssembler::JumpList emitTailCallFastPath(CCallHelpers&, CompileTimeCallLinkInfo, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall) WARN_UNUSED_RETURN;
    static void emitSlowPath(VM&, CCallHelpers&, CompileTimeCallLinkInfo, GPRReg callLinkInfoGPR);
#endif

    void revertCallToStub();

    bool isDataIC() const { return useDataIC() == UseDataIC::Yes; }
    UseDataIC useDataIC() const { return static_cast<UseDataIC>(m_useDataIC); }

    bool allowStubs() const { return m_allowStubs; }

    void disallowStubs()
    {
        m_allowStubs = false;
    }

    CodeLocationLabel<JSInternalPtrTag> doneLocation();

    void setMonomorphicCallee(VM&, JSCell*, JSObject* callee, CodeBlock*, CodePtr<JSEntryPtrTag>);
    void setSlowPathCallDestination(CodePtr<JSEntryPtrTag>);
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

    void setClearedByVirtual()
    {
        m_clearedByVirtual = true;
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

    static ptrdiff_t offsetOfCodeBlock()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, u) + OBJECT_OFFSETOF(UnionType, dataIC.m_codeBlock);
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

    Type type() const { return static_cast<Type>(m_type); }

protected:
    CallLinkInfo(Type type, CodeOrigin codeOrigin, UseDataIC useDataIC)
        : m_codeOrigin(codeOrigin)
        , m_hasSeenShouldRepatch(false)
        , m_hasSeenClosure(false)
        , m_clearedByGC(false)
        , m_clearedByVirtual(false)
        , m_allowStubs(true)
        , m_callType(None)
        , m_useDataIC(static_cast<unsigned>(useDataIC))
        , m_type(static_cast<unsigned>(type))
    {
        ASSERT(type == this->type());
        ASSERT(useDataIC == this->useDataIC());
    }

#if ENABLE(JIT)
    void setCallLinkInfoGPR(GPRReg);
#endif

    uint32_t m_maxArgumentCountIncludingThis { 0 }; // For varargs: the profiled maximum number of arguments. For direct: the number of stack slots allocated for arguments.
    CodeLocationLabel<JSInternalPtrTag> m_doneLocation;
    CodePtr<JSEntryPtrTag> m_slowPathCallDestination;
    union UnionType {
        UnionType()
            : dataIC { nullptr, nullptr }
        { }

        struct DataIC {
            CodeBlock* m_codeBlock; // This is weekly held. And cleared whenever m_monomorphicCallDestination is changed.
            CodePtr<JSEntryPtrTag> m_monomorphicCallDestination;
        } dataIC;

        struct {
            CodeLocationDataLabelPtr<JSInternalPtrTag> m_codeBlockLocation;
            CodeLocationDataLabelPtr<JSInternalPtrTag> m_calleeLocation;
        } codeIC;
    } u;

    WriteBarrier<JSCell> m_calleeOrCodeBlock;
    WriteBarrier<JSCell> m_lastSeenCalleeOrExecutable;
#if ENABLE(JIT)
    RefPtr<PolymorphicCallStubRoutine> m_stub;
#endif
    CodeOrigin m_codeOrigin;
    bool m_hasSeenShouldRepatch : 1;
    bool m_hasSeenClosure : 1;
    bool m_clearedByGC : 1;
    bool m_clearedByVirtual : 1;
    bool m_allowStubs : 1;
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
        : CallLinkInfo(Type::Baseline, CodeOrigin { }, UseDataIC::Yes)
    {
    }

    void initialize(VM&, CallType, BytecodeIndex);

    void setCodeLocations(CodeLocationLabel<JSInternalPtrTag> doneLocation)
    {
        m_doneLocation = doneLocation;
    }

#if ENABLE(JIT)
    static constexpr GPRReg calleeGPR() { return BaselineJITRegisters::Call::calleeGPR; }
    static constexpr GPRReg callLinkInfoGPR() { return BaselineJITRegisters::Call::callLinkInfoGPR; }
    void setCallLinkInfoGPR(GPRReg callLinkInfoGPR) { RELEASE_ASSERT(callLinkInfoGPR == BaselineJITRegisters::Call::callLinkInfoGPR); }
#endif
};

inline CodeOrigin getCallLinkInfoCodeOrigin(CallLinkInfo& callLinkInfo)
{
    return callLinkInfo.codeOrigin();
}

struct UnlinkedCallLinkInfo {
    CodeLocationLabel<JSInternalPtrTag> doneLocation;

    void setCodeLocations(CodeLocationLabel<JSInternalPtrTag>, CodeLocationLabel<JSInternalPtrTag> doneLocation)
    {
        this->doneLocation = doneLocation;
    }
};

struct BaselineUnlinkedCallLinkInfo : public UnlinkedCallLinkInfo {
    BytecodeIndex bytecodeIndex; // Currently, only used by baseline, so this can trivially produce a CodeOrigin.

#if ENABLE(JIT)
    void setUpCall(CallLinkInfo::CallType, GPRReg) { }
#endif
    void setFrameShuffleData(const CallFrameShuffleData&) { }
};

#if ENABLE(JIT)

class OptimizingCallLinkInfo final : public CallLinkInfo {
public:
    friend class CallLinkInfo;

    OptimizingCallLinkInfo()
        : CallLinkInfo(Type::Optimizing, { }, UseDataIC::Yes)
    {
    }

    OptimizingCallLinkInfo(CodeOrigin codeOrigin, UseDataIC useDataIC)
        : CallLinkInfo(Type::Optimizing, codeOrigin, useDataIC)
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
    void setDirectCallTarget(CodeBlock*, CodeLocationLabel<JSEntryPtrTag>);
    void emitSlowPath(VM&, CCallHelpers&);

    void setFrameShuffleData(const CallFrameShuffleData&);

    const CallFrameShuffleData* frameShuffleData()
    {
        return m_frameShuffleData.get();
    }

    void initializeFromDFGUnlinkedCallLinkInfo(VM&, const DFG::UnlinkedCallLinkInfo&);

private:
    MacroAssembler::JumpList emitFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR) WARN_UNUSED_RETURN;
    MacroAssembler::JumpList emitTailCallFastPath(CCallHelpers&, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall) WARN_UNUSED_RETURN;

    CodeLocationNearCall<JSInternalPtrTag> m_callLocation;
    CodeLocationLabel<JSInternalPtrTag> m_slowPathStart;
    CodeLocationLabel<JSInternalPtrTag> m_fastPathStart;
    std::unique_ptr<CallFrameShuffleData> m_frameShuffleData;
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
