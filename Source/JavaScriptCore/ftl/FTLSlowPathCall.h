/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef FTLSlowPathCall_h
#define FTLSlowPathCall_h

#if ENABLE(FTL_JIT)

#include "CCallHelpers.h"
#include "FTLSlowPathCallKey.h"
#include "JITOperations.h"
#include "StructureStubInfo.h"

namespace JSC { namespace FTL {

class State;

class SlowPathCall {
public:
    SlowPathCall() { }
    
    SlowPathCall(MacroAssembler::Call call, const SlowPathCallKey& key)
        : m_call(call)
        , m_key(key)
    {
    }
    
    MacroAssembler::Call call() const { return m_call; }
    SlowPathCallKey key() const { return m_key; }
    
private:
    MacroAssembler::Call m_call;
    SlowPathCallKey m_key;
};

void storeCodeOrigin(State&, CCallHelpers&, CodeOrigin);

MacroAssembler::Call callOperation(
    State&, const RegisterSet&, CCallHelpers&, CodeOrigin, CCallHelpers::JumpList*,
    J_JITOperation_ESsiJI, GPRReg result, StructureStubInfo*, GPRReg object,
    StringImpl* uid);
MacroAssembler::Call callOperation(
    State&, const RegisterSet&, CCallHelpers&, CodeOrigin, CCallHelpers::JumpList*,
    V_JITOperation_ESsiJJI, StructureStubInfo*, GPRReg value, GPRReg object,
    StringImpl* uid);

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLSlowPathCall_h

