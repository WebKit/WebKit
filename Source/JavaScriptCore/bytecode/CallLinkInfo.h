/*
 * Copyright (C) 2012, 2014, 2015 Apple Inc. All rights reserved.
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

#ifndef CallLinkInfo_h
#define CallLinkInfo_h

#include "CodeLocation.h"
#include "CodeSpecializationKind.h"
#include "JITWriteBarrier.h"
#include "JSFunction.h"
#include "Opcode.h"
#include "PolymorphicCallStubRoutine.h"
#include "WriteBarrier.h"
#include <wtf/SentinelLinkedList.h>

namespace JSC {

#if ENABLE(JIT)

class RepatchBuffer;

class CallLinkInfo : public BasicRawSentinelNode<CallLinkInfo> {
public:
    enum CallType { None, Call, CallVarargs, Construct, ConstructVarargs };
    static CallType callTypeFor(OpcodeID opcodeID)
    {
        if (opcodeID == op_call || opcodeID == op_call_eval)
            return Call;
        if (opcodeID == op_construct)
            return Construct;
        if (opcodeID == op_construct_varargs)
            return ConstructVarargs;
        ASSERT(opcodeID == op_call_varargs);
        return CallVarargs;
    }
    
    CallLinkInfo()
        : m_isFTL(false)
        , m_hasSeenShouldRepatch(false)
        , m_hasSeenClosure(false)
        , m_clearedByGC(false)
        , m_callType(None)
        , m_maxNumArguments(0)
        , m_slowPathCount(0)
    {
    }
        
    ~CallLinkInfo()
    {
        clearStub();

        if (isOnList())
            remove();
    }
    
    static CodeSpecializationKind specializationKindFor(CallType callType)
    {
        return specializationFromIsConstruct(callType == Construct || callType == ConstructVarargs);
    }
    CodeSpecializationKind specializationKind() const
    {
        return specializationKindFor(static_cast<CallType>(m_callType));
    }

    bool isLinked() { return m_stub || m_callee; }
    void unlink(RepatchBuffer&);

    void setUpCall(CallType callType, CodeOrigin codeOrigin, unsigned calleeGPR)
    {
        m_callType = callType;
        m_codeOrigin = codeOrigin;
        m_calleeGPR = calleeGPR;
    }

    void setCallLocations(CodeLocationNearCall callReturnLocation, CodeLocationDataLabelPtr hotPathBegin,
        CodeLocationNearCall hotPathOther)
    {
        m_callReturnLocation = callReturnLocation;
        m_hotPathBegin = hotPathBegin;
        m_hotPathOther = hotPathOther;
    }

    void setUpCallFromFTL(CallType callType, CodeOrigin codeOrigin,
        CodeLocationNearCall callReturnLocation, CodeLocationDataLabelPtr hotPathBegin,
        CodeLocationNearCall hotPathOther, unsigned calleeGPR)
    {
        m_isFTL = true;
        m_callType = callType;
        m_codeOrigin = codeOrigin;
        m_callReturnLocation = callReturnLocation;
        m_hotPathBegin = hotPathBegin;
        m_hotPathOther = hotPathOther;
        m_calleeGPR = calleeGPR;
    }

    CodeLocationNearCall callReturnLocation()
    {
        return m_callReturnLocation;
    }

    CodeLocationDataLabelPtr hotPathBegin()
    {
        return m_hotPathBegin;
    }

    CodeLocationNearCall hotPathOther()
    {
        return m_hotPathOther;
    }

    void setCallee(VM& vm, CodeLocationDataLabelPtr location, JSCell* owner, JSFunction* callee)
    {
        m_callee.set(vm, location, owner, callee);
    }

    void clearCallee()
    {
        m_callee.clear();
    }

    JSFunction* callee()
    {
        return m_callee.get();
    }

    void setLastSeenCallee(VM& vm, const JSCell* owner, JSFunction* callee)
    {
        m_lastSeenCallee.set(vm, owner, callee);
    }

    void clearLastSeenCallee()
    {
        m_lastSeenCallee.clear();
    }

    JSFunction* lastSeenCallee()
    {
        return m_lastSeenCallee.get();
    }

    bool haveLastSeenCallee()
    {
        return !!m_lastSeenCallee;
    }

    void setStub(PassRefPtr<PolymorphicCallStubRoutine> newStub)
    {
        clearStub();
        m_stub = newStub;
    }

    void clearStub();

    PolymorphicCallStubRoutine* stub()
    {
        return m_stub.get();
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

    void setCallType(CallType callType)
    {
        m_callType = callType;
    }

    CallType callType()
    {
        return static_cast<CallType>(m_callType);
    }

    uint8_t* addressOfMaxNumArguments()
    {
        return &m_maxNumArguments;
    }

    uint8_t maxNumArguments()
    {
        return m_maxNumArguments;
    }

    static ptrdiff_t offsetOfSlowPathCount()
    {
        return OBJECT_OFFSETOF(CallLinkInfo, m_slowPathCount);
    }

    void setCalleeGPR(unsigned calleeGPR)
    {
        m_calleeGPR = calleeGPR;
    }

    unsigned calleeGPR()
    {
        return m_calleeGPR;
    }

    uint32_t slowPathCount()
    {
        return m_slowPathCount;
    }

    void setCodeOrigin(CodeOrigin codeOrigin)
    {
        m_codeOrigin = codeOrigin;
    }

    CodeOrigin codeOrigin()
    {
        return m_codeOrigin;
    }

    void visitWeak(RepatchBuffer&);

    static CallLinkInfo& dummy();

private:
    CodeLocationNearCall m_callReturnLocation;
    CodeLocationDataLabelPtr m_hotPathBegin;
    CodeLocationNearCall m_hotPathOther;
    JITWriteBarrier<JSFunction> m_callee;
    WriteBarrier<JSFunction> m_lastSeenCallee;
    RefPtr<PolymorphicCallStubRoutine> m_stub;
    bool m_isFTL : 1;
    bool m_hasSeenShouldRepatch : 1;
    bool m_hasSeenClosure : 1;
    bool m_clearedByGC : 1;
    unsigned m_callType : 4; // CallType
    unsigned m_calleeGPR : 8;
    uint8_t m_maxNumArguments; // Only used for varargs calls.
    uint32_t m_slowPathCount;
    CodeOrigin m_codeOrigin;
};

inline CodeOrigin getCallLinkInfoCodeOrigin(CallLinkInfo& callLinkInfo)
{
    return callLinkInfo.codeOrigin();
}

typedef HashMap<CodeOrigin, CallLinkInfo*, CodeOriginApproximateHash> CallLinkInfoMap;

#else // ENABLE(JIT)

typedef HashMap<int, void*> CallLinkInfoMap;

#endif // ENABLE(JIT)

} // namespace JSC

#endif // CallLinkInfo_h
