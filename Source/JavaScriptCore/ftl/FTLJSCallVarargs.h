/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef FTLJSCallVarargs_h
#define FTLJSCallVarargs_h

#include "DFGCommon.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "FTLJSCallBase.h"

namespace JSC {

class LinkBuffer;

namespace DFG {
struct Node;
}

namespace FTL {

class JSCallVarargs {
public:
    JSCallVarargs();
    JSCallVarargs(unsigned stackmapID, DFG::Node*, CodeOrigin callSiteDescriptionOrigin);
    
    DFG::Node* node() const { return m_node; }
    
    static unsigned numSpillSlotsNeeded();
    
    void emit(CCallHelpers&, State&, int32_t spillSlotsOffset, int32_t osrExitFromGenericUnwindSpillSlots);
    void link(VM&, LinkBuffer&, CodeLocationLabel exceptionHandler);
    
    unsigned stackmapID() const { return m_stackmapID; }
    
    bool operator<(const JSCallVarargs& other) const
    {
        return m_instructionOffset < other.m_instructionOffset;
    }
    
    void setCallSiteIndex(CallSiteIndex callSiteIndex) { m_callBase.setCallSiteIndex(callSiteIndex); }
    CodeOrigin callSiteDescriptionOrigin() const { return m_callBase.callSiteDescriptionOrigin(); }
    void setCorrespondingGenericUnwindOSRExit(OSRExit* exit) { m_callBase.setCorrespondingGenericUnwindOSRExit(exit); }
    
private:
    unsigned m_stackmapID;
    DFG::Node* m_node;
    JSCallBase m_callBase;
    CCallHelpers::JumpList m_exceptions;

public:
    uint32_t m_instructionOffset;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3

#endif // FTLJSCallVarargs_h

