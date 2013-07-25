/*
 * Copyright (C) 2008, 2013 Apple Inc. All Rights Reserved.
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
#include "CallFrame.h"

#include "CallFrameInlines.h"
#include "CodeBlock.h"
#include "Interpreter.h"
#include "Operations.h"

namespace JSC {

#ifndef NDEBUG
JSStack* CallFrame::stack()
{
    return &interpreter()->stack();
}

#endif

#if USE(JSVALUE32_64)
unsigned CallFrame::locationAsBytecodeOffset() const
{
    ASSERT(codeBlock());
    ASSERT(hasLocationAsBytecodeOffset());
    return currentVPC() - codeBlock()->instructions().begin();
}

void CallFrame::setLocationAsBytecodeOffset(unsigned offset)
{
    ASSERT(codeBlock());
    ASSERT(Location::isBytecodeLocation(offset));
    setCurrentVPC(codeBlock()->instructions().begin() + offset);
    ASSERT(hasLocationAsBytecodeOffset());
}
#else
Instruction* CallFrame::currentVPC() const
{
    return codeBlock()->instructions().begin() + locationAsBytecodeOffset();
}
void CallFrame::setCurrentVPC(Instruction* vpc)
{
    setLocationAsBytecodeOffset(vpc - codeBlock()->instructions().begin());
}
#endif
    
#if ENABLE(DFG_JIT)
CallFrame* CallFrame::trueCallFrame()
{
    // Am I an inline call frame? If so, we're done.
    if (isInlinedFrame())
        return this;
    
    // If I don't have a code block, then I'm not DFG code, so I'm the true call frame.
    CodeBlock* machineCodeBlock = codeBlock();
    if (!machineCodeBlock)
        return this;
    
    // If the code block does not have any code origins, then there was no inlining, so
    // I'm done.
    if (!machineCodeBlock->hasCodeOrigins())
        return this;
    
    // Try to determine the CodeOrigin. If we don't have a pc set then the only way
    // that this makes sense is if the CodeOrigin index was set in the call frame.
    CodeOrigin codeOrigin;
    unsigned index = locationAsCodeOriginIndex();
    ASSERT(machineCodeBlock->canGetCodeOrigin(index));
    if (!machineCodeBlock->canGetCodeOrigin(index)) {
        // See above. In release builds, we try to protect ourselves from crashing even
        // though stack walking will be goofed up.
        return 0;
    }
    codeOrigin = machineCodeBlock->codeOrigin(index);

    if (!codeOrigin.inlineCallFrame)
        return this; // Not currently in inlined code.

    CodeOrigin innerMostCodeOrigin = codeOrigin;

    for (InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame; inlineCallFrame;) {
        InlineCallFrame* nextInlineCallFrame = inlineCallFrame->caller.inlineCallFrame;
        
        CallFrame* inlinedCaller = this + inlineCallFrame->stackOffset;
        
        JSFunction* calleeAsFunction = inlineCallFrame->callee.get();
        
        // Fill in the inlinedCaller
        inlinedCaller->setCodeBlock(inlineCallFrame->baselineCodeBlock());
        if (calleeAsFunction)
            inlinedCaller->setScope(calleeAsFunction->scope());
        if (nextInlineCallFrame)
            inlinedCaller->setCallerFrame(this + nextInlineCallFrame->stackOffset);
        else
            inlinedCaller->setCallerFrame(this);
        
        inlinedCaller->setInlineCallFrame(inlineCallFrame);
        inlinedCaller->setArgumentCountIncludingThis(inlineCallFrame->arguments.size());
        inlinedCaller->setLocationAsBytecodeOffset(codeOrigin.bytecodeIndex);
        inlinedCaller->setIsInlinedFrame();
        if (calleeAsFunction)
            inlinedCaller->setCallee(calleeAsFunction);
        
        codeOrigin = inlineCallFrame->caller;
        inlineCallFrame = nextInlineCallFrame;
    }
    
    return this + innerMostCodeOrigin.inlineCallFrame->stackOffset;
}
        
CallFrame* CallFrame::trueCallerFrame()
{
    CallFrame* callerFrame = this->callerFrame()->removeHostCallFrameFlag();
    if (!codeBlock())
        return callerFrame;

    // this -> The callee; this is either an inlined callee in which case it already has
    //    a pointer to the true caller. Otherwise it contains current PC in the machine
    //    caller.
    //
    // machineCaller -> The caller according to the machine, which may be zero or
    //    more frames above the true caller due to inlining.

    // Am I an inline call frame? If so, we're done.
    if (isInlinedFrame())
        return callerFrame;
    
    // I am a machine call frame, so the question is: is my caller a machine call frame
    // that has inlines or a machine call frame that doesn't?
    if (!callerFrame)
        return 0;

    if (!callerFrame->codeBlock())
        return callerFrame;
    ASSERT(!callerFrame->isInlinedFrame());
    
    return callerFrame->trueCallFrame()->removeHostCallFrameFlag();
}

unsigned CallFrame::bytecodeOffsetFromCodeOriginIndex()
{
    ASSERT(hasLocationAsCodeOriginIndex());
    CodeBlock* codeBlock = this->codeBlock();
    ASSERT(codeBlock);

    CodeOrigin codeOrigin;
    unsigned index = locationAsCodeOriginIndex();
    ASSERT(codeBlock->canGetCodeOrigin(index));
    codeOrigin = codeBlock->codeOrigin(index);

    for (InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame; inlineCallFrame;) {
        if (inlineCallFrame->baselineCodeBlock() == codeBlock)
            return codeOrigin.bytecodeIndex;

        codeOrigin = inlineCallFrame->caller;
        inlineCallFrame = codeOrigin.inlineCallFrame;
    }
    return codeOrigin.bytecodeIndex;
}

#endif // ENABLE(DFG_JIT)

Register* CallFrame::frameExtentInternal()
{
    CodeBlock* codeBlock = this->codeBlock();
    ASSERT(codeBlock);
    return registers() + codeBlock->m_numCalleeRegisters;
}

}
