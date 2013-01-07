/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "CodeOrigin.h"

#include "CodeBlock.h"
#include "Executable.h"

namespace JSC {

unsigned CodeOrigin::inlineDepthForCallFrame(InlineCallFrame* inlineCallFrame)
{
    unsigned result = 1;
    for (InlineCallFrame* current = inlineCallFrame; current; current = current->caller.inlineCallFrame)
        result++;
    return result;
}

unsigned CodeOrigin::inlineDepth() const
{
    return inlineDepthForCallFrame(inlineCallFrame);
}
    
Vector<CodeOrigin> CodeOrigin::inlineStack() const
{
    Vector<CodeOrigin> result(inlineDepth());
    result.last() = *this;
    unsigned index = result.size() - 2;
    for (InlineCallFrame* current = inlineCallFrame; current; current = current->caller.inlineCallFrame)
        result[index--] = current->caller;
    ASSERT(!result[0].inlineCallFrame);
    return result;
}

void CodeOrigin::dump(PrintStream& out) const
{
    Vector<CodeOrigin> stack = inlineStack();
    for (unsigned i = 0; i < stack.size(); ++i) {
        if (i)
            out.print(" --> ");
        
        if (InlineCallFrame* frame = stack[i].inlineCallFrame)
            out.print("#", frame->hash(), ":<", RawPointer(frame->executable.get()), "> ");
        
        out.print("bc#", stack[i].bytecodeIndex);
    }
}

CodeBlockHash InlineCallFrame::hash() const
{
    return executable->hashFor(specializationKind());
}

CodeBlock* InlineCallFrame::baselineCodeBlock() const
{
    return jsCast<FunctionExecutable*>(executable.get())->baselineCodeBlockFor(specializationKind());
}

void InlineCallFrame::dump(PrintStream& out) const
{
    out.print("#", hash(), ":<", RawPointer(executable.get()), ", bc#", caller.bytecodeIndex, ", ", specializationKind());
    if (callee)
        out.print(", known callee: ", JSValue(callee.get()));
    else
        out.print(", closure call");
    out.print(", stack >= r", stackOffset);
    out.print(">");
}

} // namespace JSC

