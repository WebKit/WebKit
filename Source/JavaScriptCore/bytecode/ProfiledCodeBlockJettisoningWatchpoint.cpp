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

#include "config.h"
#include "ProfiledCodeBlockJettisoningWatchpoint.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGCommon.h"
#include "DFGExitProfile.h"

namespace JSC {

void ProfiledCodeBlockJettisoningWatchpoint::fireInternal()
{
    if (DFG::shouldShowDisassembly()) {
        dataLog(
            "Firing profiled watchpoint ", RawPointer(this), " on ", *m_codeBlock, " due to ",
            m_exitKind, " at ", m_codeOrigin, "\n");
    }
    
    // FIXME: Maybe this should call alternative().
    // https://bugs.webkit.org/show_bug.cgi?id=123677
    CodeBlock* machineBaselineCodeBlock = m_codeBlock->baselineAlternative();
    CodeBlock* sourceBaselineCodeBlock =
        baselineCodeBlockForOriginAndBaselineCodeBlock(
            m_codeOrigin, machineBaselineCodeBlock);
    
    if (sourceBaselineCodeBlock) {
        sourceBaselineCodeBlock->addFrequentExitSite(
            DFG::FrequentExitSite(
                m_codeOrigin.bytecodeIndex, m_exitKind,
                exitingJITTypeFor(m_codeBlock->jitType())));
    }
    
    m_codeBlock->jettison(CountReoptimization);
    
    if (isOnList())
        remove();
}

} // namespace JSC

#endif // ENABLE(DFG_JIT)
