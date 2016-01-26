/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

#ifndef FTLState_h
#define FTLState_h

#if ENABLE(FTL_JIT)

#include "B3Procedure.h"
#include "DFGCommon.h"
#include "DFGGraph.h"
#include "FTLAbbreviatedTypes.h"
#include "FTLGeneratedFunction.h"
#include "FTLInlineCacheDescriptor.h"
#include "FTLJITCode.h"
#include "FTLJITFinalizer.h"
#include "FTLJSCall.h"
#include "FTLJSCallVarargs.h"
#include "FTLJSTailCall.h"
#include "FTLStackMaps.h"
#include <wtf/Box.h>
#include <wtf/Noncopyable.h>

namespace JSC {

namespace B3 {
class PatchpointValue;
class StackSlot;
} // namespace B3

namespace FTL {

class PatchpointExceptionHandle;

inline bool verboseCompilationEnabled()
{
    return DFG::verboseCompilationEnabled(DFG::FTLMode);
}

inline bool shouldDumpDisassembly()
{
    return DFG::shouldDumpDisassembly(DFG::FTLMode);
}

class State {
    WTF_MAKE_NONCOPYABLE(State);
    
public:
    State(DFG::Graph& graph);
    ~State();
    
    // None of these things is owned by State. It is the responsibility of
    // FTL phases to properly manage the lifecycle of the module and function.
    DFG::Graph& graph;
#if FTL_USES_B3
    std::unique_ptr<B3::Procedure> proc;
#endif
    LContext context;
    LModule module;
    LValue function;
    bool allocationFailed { false }; // Throw out the compilation once LLVM returns.
    RefPtr<JITCode> jitCode;
    GeneratedFunction generatedFunction;
    JITFinalizer* finalizer;
#if FTL_USES_B3
    // Top-level exception handler. Jump here if you know that you have to genericUnwind() and there
    // are no applicable catch blocks anywhere in the Graph.
    RefPtr<PatchpointExceptionHandle> defaultExceptionHandle;
    Box<CCallHelpers::Label> exceptionHandler { Box<CCallHelpers::Label>::create() };
    B3::StackSlot* capturedValue { nullptr };
#else // FTL_USES_B3
    unsigned handleStackOverflowExceptionStackmapID { UINT_MAX };
    unsigned handleExceptionStackmapID { UINT_MAX };
    unsigned capturedStackmapID { UINT_MAX };
    unsigned varargsSpillSlotsStackmapID { UINT_MAX };
    unsigned exceptionHandlingSpillSlotStackmapID { UINT_MAX };
#endif // FTL_USE_B3
    SegmentedVector<GetByIdDescriptor> getByIds;
    SegmentedVector<PutByIdDescriptor> putByIds;
    SegmentedVector<CheckInDescriptor> checkIns;
    SegmentedVector<BinaryOpDescriptor> binaryOps;
    SegmentedVector<LazySlowPathDescriptor> lazySlowPaths;
#if ENABLE(MASM_PROBE)
    SegmentedVector<ProbeDescriptor> probes;
#endif
#if !FTL_USES_B3
    Vector<JSCall> jsCalls;
    Vector<JSCallVarargs> jsCallVarargses;
    Vector<JSTailCall> jsTailCalls;
    Vector<CString> codeSectionNames;
    Vector<CString> dataSectionNames;
    SegmentedVector<OSRExitDescriptorImpl> osrExitDescriptorImpls;
#endif // !FTL_USES_B3
    void* unwindDataSection;
    size_t unwindDataSectionSize;
    RefPtr<DataSection> stackmapsSection;
    
    void dumpState(const char* when);
    void dumpState(LModule, const char* when);

    HashSet<CString> nativeLoadedLibraries;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLState_h

