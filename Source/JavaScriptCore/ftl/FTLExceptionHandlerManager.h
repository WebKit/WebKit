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

#ifndef FTLExceptionHandlerManager_h
#define FTLExceptionHandlerManager_h

#include "DFGCommon.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "CallFrame.h"
#include "FTLJSCall.h"
#include "FTLJSCallVarargs.h"
#include "FTLJSTailCall.h"

namespace JSC { namespace FTL {

// This class is intented to be used during FTLCompile to manage some common tasks
// needed to query the correspondence between stackmap records and OSR exits that are
// exception handlers. The only kind of OSR exits we keep are those OSR exits that
// correspond to exceptions from patchpoints. That means exits from lazy slow paths,
// JS calls, Get/Put ById, etc. Note: this means we don't keep track of exception checks
// after C calls because those are modeled explictly in LLVM IR as a branch to a stackmap
// intrinsic. These patchpoint OSR exits that this class keeps track of are not branched to
// directly within LLVM IR.  We jump to these OSR exits from generated patchpoint code,
// from genericUnwind(), or from FTL::callOperation().
    
class State;

class ExceptionHandlerManager {
    WTF_MAKE_NONCOPYABLE(ExceptionHandlerManager);

public:
    ExceptionHandlerManager(State& state);

    void addNewExit(uint32_t stackmapRecordIndex, size_t osrExitIndex);
    void addNewCallOperationExit(uint32_t stackmapRecordIndex, size_t osrExitIndex);

    // These functions only make sense to be called after we've generated the OSR
    // exit thunks and allocated the OSR exit thunks' link buffer.
    CodeLocationLabel callOperationExceptionTarget(uint32_t stackmapRecordIndex);
    CodeLocationLabel lazySlowPathExceptionTarget(uint32_t stackmapRecordIndex);

    OSRExit* callOperationOSRExit(uint32_t stackmapRecordIndex);
    OSRExit* getCallOSRExit(uint32_t stackmapRecordIndex, const JSCall&);
    OSRExit* getCallOSRExit(uint32_t stackmapRecordIndex, const JSTailCall&);
    OSRExit* getCallOSRExit(uint32_t stackmapRecordIndex, const JSCallVarargs&);

    CallSiteIndex procureCallSiteIndex(uint32_t stackmapRecordIndex, CodeOrigin);
    CallSiteIndex procureCallSiteIndex(uint32_t stackmapRecordIndex, const JSCall&);
    CallSiteIndex procureCallSiteIndex(uint32_t stackmapRecordIndex, const JSTailCall&);
    CallSiteIndex procureCallSiteIndex(uint32_t stackmapRecordIndex, const JSCallVarargs&);
    OSRExit* jsCallOSRExit(uint32_t stackmapRecordIndex);

private:
    OSRExit* getCallOSRExitCommon(uint32_t stackmapRecordIndex);

    State& m_state;
    typedef HashMap<uint32_t, size_t, WTF::IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> RecordIndexToOSRExitIndexMap;
    RecordIndexToOSRExitIndexMap m_map;
    RecordIndexToOSRExitIndexMap m_callOperationMap;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3

#endif // FTLExceptionHandlerManager_h
