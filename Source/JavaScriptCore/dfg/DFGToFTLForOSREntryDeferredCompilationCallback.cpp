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

#include "config.h"
#include "DFGToFTLForOSREntryDeferredCompilationCallback.h"

#if ENABLE(FTL_JIT)

#include "CodeBlock.h"
#include "DFGJITCode.h"
#include "Executable.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

ToFTLForOSREntryDeferredCompilationCallback::ToFTLForOSREntryDeferredCompilationCallback(
    PassRefPtr<CodeBlock> dfgCodeBlock)
    : m_dfgCodeBlock(dfgCodeBlock)
{
}

ToFTLForOSREntryDeferredCompilationCallback::~ToFTLForOSREntryDeferredCompilationCallback()
{
}

PassRefPtr<ToFTLForOSREntryDeferredCompilationCallback>
ToFTLForOSREntryDeferredCompilationCallback::create(
    PassRefPtr<CodeBlock> dfgCodeBlock)
{
    return adoptRef(new ToFTLForOSREntryDeferredCompilationCallback(dfgCodeBlock));
}

void ToFTLForOSREntryDeferredCompilationCallback::compilationDidBecomeReadyAsynchronously(
    CodeBlock* codeBlock)
{
    if (Options::verboseOSR()) {
        dataLog(
            "Optimizing compilation of ", *codeBlock, " (for ", *m_dfgCodeBlock,
            ") did become ready.\n");
    }
    
    m_dfgCodeBlock->jitCode()->dfg()->forceOptimizationSlowPathConcurrently(
        m_dfgCodeBlock.get());
}

void ToFTLForOSREntryDeferredCompilationCallback::compilationDidComplete(
    CodeBlock* codeBlock, CompilationResult result)
{
    if (Options::verboseOSR()) {
        dataLog(
            "Optimizing compilation of ", *codeBlock, " (for ", *m_dfgCodeBlock,
            ") result: ", result, "\n");
    }
    
    JITCode* jitCode = m_dfgCodeBlock->jitCode()->dfg();
        
    switch (result) {
    case CompilationSuccessful:
        jitCode->osrEntryBlock = codeBlock;
        return;
    case CompilationFailed:
        jitCode->osrEntryRetry = 0;
        jitCode->abandonOSREntry = true;
        return;
    case CompilationDeferred:
        return;
    case CompilationInvalidated:
        jitCode->osrEntryRetry = 0;
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} } // JSC::DFG

#endif // ENABLE(FTL_JIT)
