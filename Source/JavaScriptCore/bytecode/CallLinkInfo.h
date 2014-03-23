/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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

#include "ClosureCallStubRoutine.h"
#include "CodeLocation.h"
#include "CodeSpecializationKind.h"
#include "JITWriteBarrier.h"
#include "JSFunction.h"
#include "Opcode.h"
#include "WriteBarrier.h"
#include <wtf/SentinelLinkedList.h>

namespace JSC {

#if ENABLE(JIT)

class RepatchBuffer;

struct CallLinkInfo : public BasicRawSentinelNode<CallLinkInfo> {
    enum CallType { None, Call, CallVarargs, Construct };
    static CallType callTypeFor(OpcodeID opcodeID)
    {
        if (opcodeID == op_call || opcodeID == op_call_eval)
            return Call;
        if (opcodeID == op_construct)
            return Construct;
        ASSERT(opcodeID == op_call_varargs);
        return CallVarargs;
    }
        
    CallLinkInfo()
        : isFTL(false)
        , hasSeenShouldRepatch(false)
        , hasSeenClosure(false)
        , callType(None)
        , slowPathCount(0)
    {
    }
        
    ~CallLinkInfo()
    {
        if (isOnList())
            remove();
    }
    
    CodeSpecializationKind specializationKind() const
    {
        return specializationFromIsConstruct(callType == Construct);
    }

    CodeLocationNearCall callReturnLocation;
    CodeLocationDataLabelPtr hotPathBegin;
    CodeLocationNearCall hotPathOther;
    JITWriteBarrier<JSFunction> callee;
    WriteBarrier<JSFunction> lastSeenCallee;
    RefPtr<ClosureCallStubRoutine> stub;
    bool isFTL : 1;
    bool hasSeenShouldRepatch : 1;
    bool hasSeenClosure : 1;
    unsigned callType : 5; // CallType
    unsigned calleeGPR : 8;
    unsigned slowPathCount;
    CodeOrigin codeOrigin;

    bool isLinked() { return stub || callee; }
    void unlink(VM&, RepatchBuffer&);

    bool seenOnce()
    {
        return hasSeenShouldRepatch;
    }

    void setSeen()
    {
        hasSeenShouldRepatch = true;
    }
    
    static CallLinkInfo& dummy();
};

inline CodeOrigin getCallLinkInfoCodeOrigin(CallLinkInfo& callLinkInfo)
{
    return callLinkInfo.codeOrigin;
}

typedef HashMap<CodeOrigin, CallLinkInfo*, CodeOriginApproximateHash> CallLinkInfoMap;

#else // ENABLE(JIT)

typedef HashMap<int, void*> CallLinkInfoMap;

#endif // ENABLE(JIT)

} // namespace JSC

#endif // CallLinkInfo_h
