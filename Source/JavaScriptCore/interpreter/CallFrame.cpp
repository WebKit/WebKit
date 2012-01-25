/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "CodeBlock.h"
#include "Interpreter.h"

namespace JSC {

#ifndef NDEBUG
void CallFrame::dumpCaller()
{
    int signedLineNumber;
    intptr_t sourceID;
    UString urlString;
    JSValue function;
    
    interpreter()->retrieveLastCaller(this, signedLineNumber, sourceID, urlString, function);
    printf("Callpoint => %s:%d\n", urlString.utf8().data(), signedLineNumber);
}

RegisterFile* CallFrame::registerFile()
{
    return &interpreter()->registerFile();
}

#endif

#if ENABLE(DFG_JIT)
bool CallFrame::isInlineCallFrameSlow()
{
    if (!callee())
        return false;
    JSCell* calleeAsFunctionCell = getJSFunction(callee());
    if (!calleeAsFunctionCell)
        return false;
    JSFunction* calleeAsFunction = asFunction(calleeAsFunctionCell);
    return calleeAsFunction->executable() != codeBlock()->ownerExecutable();
}
        
CallFrame* CallFrame::trueCallerFrame()
{
    // this -> The callee; this is either an inlined callee in which case it already has
    //    a pointer to the true caller. Otherwise it contains current PC in the machine
    //    caller.
    //
    // machineCaller -> The caller according to the machine, which may be zero or
    //    more frames above the true caller due to inlining.
    //
    // trueCaller -> The real caller.
    
    // Am I an inline call frame? If so, we're done.
    if (isInlineCallFrame())
        return callerFrame();
    
    // I am a machine call frame, so the question is: is my caller a machine call frame
    // that has inlines or a machine call frame that doesn't?
    CallFrame* machineCaller = callerFrame()->removeHostCallFrameFlag();
    if (!machineCaller)
        return 0;
    ASSERT(!machineCaller->isInlineCallFrame());
    if (!machineCaller->codeBlock())
        return machineCaller;
    if (!machineCaller->codeBlock()->hasCodeOrigins())
        return machineCaller; // No inlining, so machineCaller == trueCaller
    
    // Figure out where the caller frame would have gone relative to the machine
    // caller, and rematerialize it. Do so for the entire inline stack.
    
    ReturnAddressPtr currentReturnPC = returnPC();
    CodeBlock* machineCodeBlock = machineCaller->codeBlock();
    
    CodeOrigin codeOrigin;
    if (!machineCodeBlock->codeOriginForReturn(currentReturnPC, codeOrigin))
        return machineCaller; // Not currently in inlined code, so machineCaller == trueCaller
    
    for (InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame; inlineCallFrame;) {
        InlineCallFrame* nextInlineCallFrame = inlineCallFrame->caller.inlineCallFrame;
        
        CallFrame* inlinedCaller = machineCaller + inlineCallFrame->stackOffset;
        
        JSFunction* calleeAsFunction = inlineCallFrame->callee.get();
        
        // Fill in the inlinedCaller
        inlinedCaller->setCodeBlock(machineCaller->codeBlock());
        
        inlinedCaller->setScopeChain(calleeAsFunction->scope());
        if (nextInlineCallFrame)
            inlinedCaller->setCallerFrame(machineCaller + nextInlineCallFrame->stackOffset);
        else
            inlinedCaller->setCallerFrame(machineCaller);
        
        inlinedCaller->setInlineCallFrame(inlineCallFrame);
        inlinedCaller->setArgumentCountIncludingThis(inlineCallFrame->arguments.size());
        inlinedCaller->setCallee(calleeAsFunction);
        
        inlineCallFrame = nextInlineCallFrame;
    }
    
    return machineCaller + codeOrigin.inlineCallFrame->stackOffset;
}
#endif

}
