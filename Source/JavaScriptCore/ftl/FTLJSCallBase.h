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

#ifndef FTLJSCallBase_h
#define FTLJSCallBase_h

#include "DFGCommon.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "CCallHelpers.h"
#include "CallLinkInfo.h"
#include "CodeOrigin.h"
#include "FTLOSRExit.h"

namespace JSC {

class LinkBuffer;

namespace DFG {
struct Node;
}

namespace FTL {

class State;

class JSCallBase {
public:
    JSCallBase();
    JSCallBase(CallLinkInfo::CallType, CodeOrigin semantic, CodeOrigin callSiteDescription);
    
    void emit(CCallHelpers&, State&, int32_t osrExitFromGenericUnwindSpillSlots);
    void link(VM&, LinkBuffer&);

    void setCallSiteIndex(CallSiteIndex callSiteIndex) { m_callSiteIndex = callSiteIndex; }
    CodeOrigin callSiteDescriptionOrigin() const { return m_callSiteDescriptionOrigin; }

    void setCorrespondingGenericUnwindOSRExit(OSRExit* exit) { m_correspondingGenericUnwindOSRExit = exit; }
    
protected:
    CallLinkInfo::CallType m_type;
    CodeOrigin m_semanticeOrigin;
    CodeOrigin m_callSiteDescriptionOrigin; // These two code origins may be different with tail calls under some circumstances of inlining. See relevant comment in LowerDFGToLLVM.
    CallSiteIndex m_callSiteIndex;
    CCallHelpers::DataLabelPtr m_targetToCheck;
    CCallHelpers::Call m_fastCall;
    CCallHelpers::Call m_slowCall;
    CallLinkInfo* m_callLinkInfo;
public:
    OSRExit* m_correspondingGenericUnwindOSRExit;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3

#endif // FTLJSCallBase_h

