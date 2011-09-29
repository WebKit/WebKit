/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGOSREntry.h"

#if ENABLE(DFG_JIT)

#include "CallFrame.h"
#include "CodeBlock.h"
#include "DFGNode.h"
#include "JIT.h"

namespace JSC { namespace DFG {

void* prepareOSREntry(ExecState* exec, CodeBlock* codeBlock, unsigned bytecodeIndex)
{
#if ENABLE(DFG_OSR_ENTRY)
    ASSERT(codeBlock->getJITType() == JITCode::DFGJIT);
    ASSERT(codeBlock->alternative());
    ASSERT(codeBlock->alternative()->getJITType() == JITCode::BaselineJIT);
    ASSERT(!codeBlock->jitCodeMap());
    ASSERT(codeBlock->numberOfDFGOSREntries());

#if ENABLE(JIT_VERBOSE_OSR)
    printf("OSR in %p(%p) from bc#%u\n", codeBlock, codeBlock->alternative(), bytecodeIndex);
#endif
    
    JSGlobalData* globalData = &exec->globalData();
    OSREntryData* entry = codeBlock->dfgOSREntryDataForBytecodeIndex(bytecodeIndex);
    
    ASSERT(entry->m_bytecodeIndex == bytecodeIndex);
    
    // The code below checks if it is safe to perform OSR entry. It may find
    // that it is unsafe to do so, for any number of reasons, which are documented
    // below. If the code decides not to OSR then it returns 0, and it's the caller's
    // responsibility to patch up the state in such a way as to ensure that it's
    // both safe and efficient to continue executing baseline code for now. This
    // should almost certainly include calling either codeBlock->optimizeAfterWarmUp()
    // or codeBlock->dontOptimizeAnytimeSoon().
    
    // 1) Verify predictions. If the predictions are inconsistent with the actual
    //    values, then OSR entry is not possible at this time. It's tempting to
    //    assume that we could somehow avoid this case. We can certainly avoid it
    //    for first-time loop OSR - that is, OSR into a CodeBlock that we have just
    //    compiled. Then we are almost guaranteed that all of the predictions will
    //    check out. It would be pretty easy to make that a hard guarantee. But
    //    then there would still be the case where two call frames with the same
    //    baseline CodeBlock are on the stack at the same time. The top one
    //    triggers compilation and OSR. In that case, we may no longer have
    //    accurate value profiles for the one deeper in the stack. Hence, when we
    //    pop into the CodeBlock that is deeper on the stack, we might OSR and
    //    realize that the predictions are wrong. Probably, in most cases, this is
    //    just an anomaly in the sense that the older CodeBlock simply went off
    //    into a less-likely path. So, the wisest course of action is to simply not
    //    OSR at this time.
    
    for (unsigned i = 1; i < entry->m_predictions.argumentUpperBound(); ++i) {
        ActionablePrediction prediction = entry->m_predictions.argument(i);
        if (prediction == NoActionablePrediction)
            continue;

        if (i >= exec->argumentCountIncludingThis()) {
#if ENABLE(JIT_VERBOSE_OSR)
            printf("    OSR failed because argument %u was not passed, expected %s.\n", i, actionablePredictionToString(prediction));
#endif
            return 0;
        }
        
        if (!valueObeysPrediction(globalData, exec->argument(i - 1), prediction)) {
#if ENABLE(JIT_VERBOSE_OSR)
            printf("    OSR failed because argument %u is %s, expected %s.\n", i, exec->argument(i - 1).description(), actionablePredictionToString(prediction));
#endif
            return 0;
        }
    }
    
    for (unsigned i = 0; i < entry->m_predictions.variableUpperBound(); ++i) {
        ActionablePrediction prediction = entry->m_predictions.variable(i);
        if (prediction == NoActionablePrediction)
            continue;
        
        if (!valueObeysPrediction(globalData, exec->registers()[i].jsValue(), prediction)) {
#if ENABLE(JIT_VERBOSE_OSR)
            printf("    OSR failed because variable %u is %s, expected %s.\n", i, exec->registers()[i].jsValue().description(), actionablePredictionToString(prediction));
#endif
            return 0;
        }
    }
    
    // 2) Check the stack height. The DFG JIT may require a taller stack than the
    //    baseline JIT, in some cases. If we can't grow the stack, then don't do
    //    OSR right now. That's the only option we have unless we want basic block
    //    boundaries to start throwing RangeErrors. Although that would be possible,
    //    it seems silly: you'd be diverting the program to error handling when it
    //    would have otherwise just kept running albeit less quickly.
    
    if (!globalData->interpreter->registerFile().grow(&exec->registers()[codeBlock->m_numCalleeRegisters])) {
#if ENABLE(JIT_VERBOSE_OSR)
        printf("    OSR failed because stack growth failed.\n");
#endif
        return 0;
    }
    
#if ENABLE(JIT_VERBOSE_OSR)
    printf("    OSR should succeed.\n");
#endif
    
    // 3) Fix the call frame.
    
    exec->setCodeBlock(codeBlock);
    
    // 4) Find and return the destination machine code address.
    
    void* result = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(codeBlock->getJITCode().start()) + entry->m_machineCodeOffset);
    
#if ENABLE(JIT_VERBOSE_OSR)
    printf("    OSR returning machine code address %p.\n", result);
#endif
    
    return result;
#else // ENABLE(DFG_OSR_ENTRY)
    UNUSED_PARAM(exec);
    UNUSED_PARAM(codeBlock);
    UNUSED_PARAM(bytecodeIndex);
    return 0;
#endif
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
