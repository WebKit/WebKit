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
#include "DFGOSRExit.h"

#if ENABLE(DFG_JIT)

#include "DFGAssemblyHelpers.h"
#include "DFGSpeculativeJIT.h"

namespace JSC { namespace DFG {

static unsigned computeNumVariablesForCodeOrigin(
    CodeBlock* codeBlock, const CodeOrigin& codeOrigin)
{
    if (!codeOrigin.inlineCallFrame)
        return codeBlock->m_numCalleeRegisters;
    return
        codeOrigin.inlineCallFrame->stackOffset +
        baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame)->m_numCalleeRegisters;
}

OSRExit::OSRExit(ExitKind kind, JSValueSource jsValueSource, MethodOfGettingAValueProfile valueProfile, MacroAssembler::Jump check, SpeculativeJIT* jit, unsigned recoveryIndex)
    : m_jsValueSource(jsValueSource)
    , m_valueProfile(valueProfile)
    , m_check(check)
    , m_nodeIndex(jit->m_compileIndex)
    , m_codeOrigin(jit->m_codeOriginForOSR)
    , m_codeOriginForExitProfile(m_codeOrigin)
    , m_recoveryIndex(recoveryIndex)
    , m_kind(kind)
    , m_count(0)
    , m_arguments(jit->m_arguments.size())
    , m_variables(computeNumVariablesForCodeOrigin(jit->m_jit.graph().m_profiledBlock, jit->m_codeOriginForOSR))
    , m_lastSetOperand(jit->m_lastSetOperand)
{
    ASSERT(m_codeOrigin.isSet());
    for (unsigned argument = 0; argument < m_arguments.size(); ++argument)
        m_arguments[argument] = jit->computeValueRecoveryFor(jit->m_arguments[argument]);
    for (unsigned variable = 0; variable < m_variables.size(); ++variable)
        m_variables[variable] = jit->computeValueRecoveryFor(jit->m_variables[variable]);
}

void OSRExit::dump(FILE* out) const
{
    for (unsigned argument = 0; argument < m_arguments.size(); ++argument)
        m_arguments[argument].dump(out);
    fprintf(out, " : ");
    for (unsigned variable = 0; variable < m_variables.size(); ++variable)
        m_variables[variable].dump(out);
}

bool OSRExit::considerAddingAsFrequentExitSiteSlow(CodeBlock* dfgCodeBlock, CodeBlock* profiledCodeBlock)
{
    if (static_cast<double>(m_count) / dfgCodeBlock->speculativeFailCounter() <= Options::osrExitProminenceForFrequentExitSite)
        return false;
    
    return baselineCodeBlockForOriginAndBaselineCodeBlock(m_codeOriginForExitProfile, profiledCodeBlock)->addFrequentExitSite(FrequentExitSite(m_codeOriginForExitProfile.bytecodeIndex, m_kind));
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
