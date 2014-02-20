/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLJITFinalizer_h
#define FTLJITFinalizer_h

#if ENABLE(FTL_JIT)

#include "DFGFinalizer.h"
#include "FTLGeneratedFunction.h"
#include "FTLJITCode.h"
#include "FTLOSRExitCompilationInfo.h"
#include "FTLSlowPathCall.h"
#include "LLVMAPI.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"

namespace JSC { namespace FTL {

class JITFinalizer : public DFG::Finalizer {
public:
    JITFinalizer(DFG::Plan&);
    virtual ~JITFinalizer();
    
    size_t codeSize() override;
    bool finalize() override;
    bool finalizeFunction() override;

    OwnPtr<LinkBuffer> exitThunksLinkBuffer;
    OwnPtr<LinkBuffer> entrypointLinkBuffer;
    OwnPtr<LinkBuffer> sideCodeLinkBuffer;
    OwnPtr<LinkBuffer> handleExceptionsLinkBuffer;
    Vector<SlowPathCall> slowPathCalls; // Calls inside the side code.
    Vector<OSRExitCompilationInfo> osrExit;
    MacroAssembler::Label arityCheck;
    GeneratedFunction function;
    RefPtr<JITCode> jitCode;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLJITFinalizer_h

