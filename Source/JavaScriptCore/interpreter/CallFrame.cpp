/*
 * Copyright (C) 2008, 2013, 2014 Apple Inc. All Rights Reserved.
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
#include "JSActivation.h"
#include "JSCInlines.h"
#include "VMEntryScope.h"

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

unsigned CallFrame::bytecodeOffset()
{
    if (!codeBlock())
        return 0;
#if ENABLE(DFG_JIT)
    if (hasLocationAsCodeOriginIndex())
        return bytecodeOffsetFromCodeOriginIndex();
#endif
    return locationAsBytecodeOffset();
}

CodeOrigin CallFrame::codeOrigin()
{
    if (!codeBlock())
        return CodeOrigin(0);
#if ENABLE(DFG_JIT)
    if (hasLocationAsCodeOriginIndex()) {
        unsigned index = locationAsCodeOriginIndex();
        ASSERT(codeBlock()->canGetCodeOrigin(index));
        return codeBlock()->codeOrigin(index);
    }
#endif
    return CodeOrigin(locationAsBytecodeOffset());
}

Register* CallFrame::topOfFrameInternal()
{
    CodeBlock* codeBlock = this->codeBlock();
    ASSERT(codeBlock);
    return registers() + codeBlock->stackPointerOffset();
}

JSGlobalObject* CallFrame::vmEntryGlobalObject()
{
    if (this == lexicalGlobalObject()->globalExec())
        return lexicalGlobalObject();

    // For any ExecState that's not a globalExec, the 
    // dynamic global object must be set since code is running
    ASSERT(vm().entryScope);
    return vm().entryScope->globalObject();
}

JSActivation* CallFrame::activation() const
{
    CodeBlock* codeBlock = this->codeBlock();
    RELEASE_ASSERT(codeBlock->needsActivation());
    VirtualRegister activationRegister = codeBlock->activationRegister();
    return registers()[activationRegister.offset()].Register::activation();
}

void CallFrame::setActivation(JSActivation* activation)
{
    CodeBlock* codeBlock = this->codeBlock();
    RELEASE_ASSERT(codeBlock->needsActivation());
    VirtualRegister activationRegister = codeBlock->activationRegister();
    registers()[activationRegister.offset()] = activation;
}

} // namespace JSC
