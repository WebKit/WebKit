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
#include "FTLOSRExit.h"

#if ENABLE(FTL_JIT)

#include "CodeBlock.h"
#include "DFGBasicBlock.h"
#include "DFGNode.h"
#include "FTLExitArgument.h"
#include "FTLExitArgumentList.h"
#include "FTLJITCode.h"
#include "Operations.h"

namespace JSC { namespace FTL {

using namespace DFG;

OSRExit::OSRExit(
    ExitKind exitKind, ValueFormat profileValueFormat,
    MethodOfGettingAValueProfile valueProfile, CodeOrigin codeOrigin,
    CodeOrigin originForProfile, int lastSetOperand, unsigned numberOfArguments,
    unsigned numberOfLocals)
    : OSRExitBase(exitKind, codeOrigin, originForProfile)
    , m_profileValueFormat(profileValueFormat)
    , m_valueProfile(valueProfile)
    , m_patchableCodeOffset(0)
    , m_lastSetOperand(lastSetOperand)
    , m_values(numberOfArguments, numberOfLocals)
{
}

CodeLocationJump OSRExit::codeLocationForRepatch(CodeBlock* ftlCodeBlock) const
{
    return CodeLocationJump(
        reinterpret_cast<char*>(
            ftlCodeBlock->jitCode()->ftl()->exitThunks().dataLocation()) +
        m_patchableCodeOffset);
}

void OSRExit::convertToForward(
    BasicBlock* block, Node* currentNode, unsigned nodeIndex,
    const FormattedValue &value, ExitArgumentList& arguments)
{
    Node* node;
    Node* lastMovHint;
    if (!doSearchForForwardConversion(block, currentNode, nodeIndex, !!value, node, lastMovHint))
        return;

    ASSERT(node->codeOrigin != currentNode->codeOrigin);
    
    m_codeOrigin = node->codeOrigin;
    
    if (!value)
        return;
    
    VirtualRegister overriddenOperand = lastMovHint->local();
    m_lastSetOperand = overriddenOperand;
    
    // Is the value for this operand being passed as an argument to the exit, or is
    // it something else? If it's an argument already, then replace that argument;
    // otherwise add another argument.
    if (m_values.operand(overriddenOperand).isArgument()) {
        ExitArgument exitArgument = m_values.operand(overriddenOperand).exitArgument();
        arguments[exitArgument.argument()] = value.value();
        m_values.operand(overriddenOperand) = ExitValue::exitArgument(
            exitArgument.withFormat(value.format()));
        return;
    }
    
    unsigned argument = arguments.size();
    arguments.append(value.value());
    m_values.operand(m_lastSetOperand) = ExitValue::exitArgument(
        ExitArgument(value.format(), argument));
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

